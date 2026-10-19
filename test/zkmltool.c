/* Writer namespace regressions use checked programs, including shadowing. */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

#include "front/parser.h"
#include "zkml.h"

static const char *target_compiler;

static int target_check(const char *path) {
  pid_t child = fork();
  if (child < 0) return 0;
  if (child == 0) {
    if (freopen("/dev/null", "wb", stdout) == NULL) _exit(127);
    execl(target_compiler, target_compiler, "check", path, (char *)NULL);
    _exit(127);
  }
  int status;
  pid_t done;
  do { done = waitpid(child, &status, 0); } while (done < 0 && errno == EINTR);
  return done == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static int target_eval(const char *path, const char *entry, const char *want) {
  int output[2];
  if (pipe(output) != 0) return 0;
  pid_t child = fork();
  if (child < 0) {
    close(output[0]);
    close(output[1]);
    return 0;
  }
  if (child == 0) {
    close(output[0]);
    if (dup2(output[1], STDOUT_FILENO) < 0) _exit(127);
    close(output[1]);
    execl(target_compiler, target_compiler, "eval", path, entry, "1", (char *)NULL);
    _exit(127);
  }
  close(output[1]);
  char value[64];
  size_t used = 0;
  ssize_t n;
  while ((n = read(output[0], value + used, sizeof value - 1 - used)) != 0) {
    if (n < 0) {
      if (errno == EINTR) continue;
      break;
    }
    used += (size_t)n;
    if (used == sizeof value - 1) break;
  }
  close(output[0]);
  value[used] = '\0';
  int status;
  pid_t done;
  do { done = waitpid(child, &status, 0); } while (done < 0 && errno == EINTR);
  int ok = n == 0 && done == child && WIFEXITED(status) && WEXITSTATUS(status) == 0 && strcmp(value, want) == 0;
  if (!ok) fprintf(stderr, "FAIL zkml eval %s: want %s, got %s", entry, want, value);
  return ok;
}

static int parse(Arena *arena, const char *text, Origin origin, DeclList *decls, Diag *diag) {
  TokenList tokens = {0};
  return lex_source(arena, "zkml-test", text, strlen(text), &tokens, diag)
    && parse_source(arena, "zkml-test", origin, &tokens, decls, diag);
}

static int writer_op(const char *domain, const char *program, const char *model, const char *want, const char *needle,
                     ZkmlMode mode, const char *value) {
  Arena arena;
  Diag diag;
  DeclList decls = {0};
  Machine machine;
  const char *path = "build/zkml-test.lang";
  const char sentinel[] = "previous output\n";
  char output[8192];
  arena_init(&arena, (size_t)1 << 25);
  diag_init(&diag);
  int ok = parse(&arena, domain, ORIGIN_DOMAIN, &decls, &diag)
    && parse(&arena, program, ORIGIN_PROGRAM, &decls, &diag)
    && check_program(&arena, &decls, &machine, &diag);
  FILE *out = fopen(path, "wb");
  if (out != NULL) {
    ok = fwrite(sentinel, 1, sizeof sentinel - 1, out) == sizeof sentinel - 1 && ok;
    ok = fclose(out) == 0 && ok;
  } else ok = 0;
  if (ok) {
    int status = zkml_command_op(&machine, &decls, program, strlen(program), model, mode, path, &diag);
    ok = want == NULL ? status == 0 : status == 1 && diag.code != NULL && strcmp(diag.code, want) == 0;
    FILE *in = fopen(path, "rb");
    if (in == NULL) ok = 0;
    else {
      size_t n = fread(output, 1, sizeof output - 1, in);
      output[n] = '\0';
      ok = !ferror(in) && feof(in) && ok;
      fclose(in);
      ok = (want == NULL ? strstr(output, needle) != NULL : strcmp(output, sentinel) == 0) && ok;
    }
    if (ok && want == NULL && target_compiler != NULL) ok = target_check(path);
    if (ok && value != NULL && target_compiler != NULL) {
      ok = target_eval(path, "modelRunE", value) && target_eval(path, "modelWitE", value);
    }
  }
  if (!ok) {
    fprintf(stderr, "FAIL zkml namespace: want %s for %s\n", want == NULL ? "success" : want, program);
    if (diag.set) diag_print(&diag, stderr);
  }
  remove(path);
  arena_release(&arena);
  return ok;
}

static int writer(const char *domain, const char *program, const char *model, const char *want, const char *needle) {
  ZkmlMode dense = {ZKML_OP_DENSE, 0, NULL};
  return writer_op(domain, program, model, want, needle, dense, NULL);
}

int main(int argc, char **argv) {
  if (argc > 2) return 2;
  target_compiler = argc == 2 ? argv[1] : NULL;
  const char weights[] = "def w : List (List Nat) := cons (cons 1 nil) nil\n";
  const char circle[] = "family Circle := circleBase | circleLoop : Path Circle circleBase circleBase";
  int ok = 1;
  ok &= writer("", "def bbRun : List (List Nat) := cons (cons 1 nil) nil", "bbRun", "ZKML_MODEL", "");
  ok &= writer("", "def stage : List (List Nat) := cons (cons 1 nil) nil", "stage", "ZKML_MODEL", "");
  ok &= writer("", "def w : List (List Nat) := cons (cons 1 nil) nil\n"
                    "def bbRun : Nat := (fun (xs : List (List Nat)) => 2) w", "w", "ZKML_MODEL", "");
  ok &= writer("def refAdd : Nat -> Nat -> Nat := fun a b => natAdd a b", weights, "w", NULL, "def wRunE");
  ok &= writer("def refAdd : Nat -> Nat -> Nat := fun a b => natAdd a b",
               "def w : List (List Nat) := cons (cons 1 nil) nil\n"
               "def run : Nat := (fun (xs : List (List Nat)) => refAdd 1 2) w", "w", "ZKML_MODEL", "");
  ok &= writer("def refAdd : Nat -> Nat -> Nat := fun a b => natAdd a b",
               "def w : List (List Nat) := cons (cons (refAdd 1 2) nil) nil", "w", "ZKML_MODEL", "");
  ok &= writer("def refAdd : Nat -> Nat -> Nat := fun a b => natAdd a b",
               "def w : List (List Nat) := cons (cons 1 nil) nil\n"
               "def run : Nat -> Nat := fun refAdd => (fun (xs : List (List Nat)) => refAdd) w",
               "w", NULL, "def run");
  ok &= writer("", "def w : List (List Nat) := cons (cons 1 nil) nil\n"
                    "def bbRun : Nat := 2", "w", NULL, "def wRunE");
  ok &= writer("family Color := red | green | blue",
               "def w : List (List Nat) := cons (cons 1 nil) nil\n"
               "def run : Nat := (fun (xs : List (List Nat)) => fold 1 2 3 red) w", "w", NULL, "def run");
  ok &= writer("family Layer := sealPrivate | addPublic | subPublic | addConst (constAmount : Nat)",
               "def w : List (List Nat) := cons (cons 1 nil) nil\n"
               "def run : Nat := (fun (xs : List (List Nat)) => (fun (f : Layer -> Nat) => 0) (fun l => 1)) w",
               "w", "ZKML_MODEL", "");
  ok &= writer("", "def w : List (List Nat) := cons (cons 1 nil) nil\n"
                    "def wRunE : Nat := 2", "w", "ZKML_MODEL", "");
  ok &= writer("", "def w : List (List Nat) := nil", "w", "ZKML_MODEL", "");
  ok &= writer("", "def w : List (List Nat) := cons nil nil", "w", "ZKML_MODEL", "");
  ok &= writer("", "def w : List (List Nat) := cons (cons 1 nil) (cons (cons 2 (cons 3 nil)) nil)", "w", "ZKML_MODEL", "");
  ok &= writer("", "def x0 : List (List Nat) := cons (cons 1 nil) nil", "x0", NULL,
               "(pair x0 (cons (stageEnc Nat x0S (cons x0_ nil)) "
               "(cons (stageEnc Nat x0T (bbRun x0S x0T x0Circ (x0Rd x0) (cons x0_ nil))) nil))))");
  ok &= writer("", "def x1 : List (List Nat) := cons (cons 1 (cons 2 nil)) nil", "x1", NULL,
               "(pair x1 (cons (stageEnc Nat x1S (cons x0 (cons x1_ nil))) "
               "(cons (stageEnc Nat x1T (bbRun x1S x1T x1Circ (x1Rd x1) (cons x0 (cons x1_ nil)))) nil))))");
  ok &= writer(circle,
               "def w : List (List Nat) := cons (cons 1 nil) nil\n"
               "def proof : Path Nat 0 0 := fold (fun (z : Circle) => Path Nat 0 0) (fun (z : Circle) => natIsSet 0 0) refl circleBase\n"
               "def run : Nat := (fun (xs : List (List Nat)) => (fun (p : Path Nat 0 0) => 0) proof) w",
               "w", "ZKML_MODEL", "");
  ok &= writer("def isContr : Type 0 -> Type 0 := fun A => Sigma (x : A) ((y : A) -> Path A x y)\n"
               "def fiber : (A B : Type 0) -> (A -> B) -> B -> Type 0 := fun A B f y => Sigma (x : A) (Path B (f x) y)\n"
               "def isEquiv : (A B : Type 0) -> (A -> B) -> Type 0 := fun A B f => (y : B) -> isContr (fiber A B f y)\n"
               "def Equiv : (A B : Type 0) -> Type 0 := fun A B => Sigma (f : A -> B) (isEquiv A B f)\n",
                    "def G : Type 0 := Glue (List (List Nat)) []\n"
                    "def q : G := glue [] (cons (cons 1 nil) nil)\n"
                    "def w : List (List Nat) := unglue q", "w", "ZKML_MODEL", "");
  ok &= writer(circle,
               "def w : List (List Nat) := cons (cons 1 nil) nil\n"
               "def run : Nat := (fun (xs : List (List Nat)) => (fun (p : (fun (q : Path Nat 0 0) => Nat) "
               "(fold (fun (z : Circle) => Path Nat 0 0) (fun (z : Circle) => natIsSet 0 0) refl circleBase)) => p) 0) w",
               "w", "ZKML_MODEL", "");
  ok &= writer(circle,
               "def w : List (List Nat) := cons (cons 1 nil) nil\n"
               "def run : (fun (q : Path Nat 0 0) => Nat) "
               "(fold (fun (z : Circle) => Path Nat 0 0) (fun (z : Circle) => natIsSet 0 0) refl circleBase) := "
               "(fun (xs : List (List Nat)) => 0) w", "w", "ZKML_MODEL", "");
  ok &= writer(circle,
               "def proof : Path Nat 0 0 := fold (fun (z : Circle) => Path Nat 0 0) (fun (z : Circle) => natIsSet 0 0) refl circleBase\n"
               "def w : List (List Nat) := cons (cons 1 nil) nil\n"
               "def run : Nat := (fun (xs : List (List Nat)) => 2) w", "w", NULL, "def run");
  ZkmlMode lookup_w = {ZKML_OP_LOOKUP, 0, "w"};
  ok &= writer_op("", "def model : List (List Nat) := cons (cons 1 nil) nil\n"
                     "def w : List (List Nat) := cons (cons 1 (cons 7 nil)) nil",
                  "model", NULL, "def modelRunE", lookup_w, "7\n");
  ZkmlMode lookup_w_ = {ZKML_OP_LOOKUP, 0, "w_"};
  ok &= writer_op("", "def model : List (List Nat) := cons (cons 1 nil) nil\n"
                     "def w_ : List (List Nat) := cons (cons 1 (cons 7 nil)) nil",
                  "model", NULL, "def modelRunE", lookup_w_, "7\n");
  if (ok) puts("zkml namespace regressions: 23 passed");
  return ok ? 0 : 1;
}
