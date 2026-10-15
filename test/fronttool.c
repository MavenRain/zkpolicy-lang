/* Domain declaration regressions use the same parser and checker as langc. */
#include <stdio.h>
#include <string.h>

#include "front/check.h"
#include "front/lexer.h"
#include "front/parser.h"

static int domain(const char *text, const char *want) {
  Arena arena;
  Diag diag;
  TokenList tokens = {0};
  DeclList decls = {0};
  Machine machine;
  arena_init(&arena, (size_t)1 << 24);
  diag_init(&diag);
  int ok = lex_source(&arena, "test-domain", text, strlen(text), &tokens, &diag)
    && parse_source(&arena, "test-domain", ORIGIN_DOMAIN, &tokens, &decls, &diag)
    && check_program(&arena, &decls, &machine, &diag);
  int passed = want == NULL ? ok : !ok && diag.code != NULL && strcmp(diag.code, want) == 0;
  if (!passed) {
    fprintf(stderr, "FAIL domain: wanted %s, got %s: %s\n", want == NULL ? "ok" : want,
            ok ? "ok" : diag.code == NULL ? "no diagnostic" : diag.code, text);
  }
  arena_release(&arena);
  return passed;
}

/* Printed normal forms must check again and denote the original value. */
static int roundtrip(const char *type, const char *body) {
  Arena arena;
  Diag diag;
  TokenList tokens = {0};
  DeclList decls = {0};
  Machine machine;
  char source[16384];
  char normal[8192];
  int ok;
  arena_init(&arena, (size_t)1 << 24);
  diag_init(&diag);
  snprintf(source, sizeof source, "def original : %s := %s\n", type, body);
  ok = lex_source(&arena, "roundtrip", source, strlen(source), &tokens, &diag)
    && parse_source(&arena, "roundtrip", ORIGIN_PROGRAM, &tokens, &decls, &diag)
    && check_program(&arena, &decls, &machine, &diag)
    && value_print(&machine, NULL, 0, def_value(&machine, 0), normal, sizeof normal);
  if (ok) {
    snprintf(source, sizeof source, "def original : %s := %s\ndef copy : %s := %s\n", type, body, type, normal);
    tokens = (TokenList){0};
    decls = (DeclList){0};
    ok = lex_source(&arena, "roundtrip", source, strlen(source), &tokens, &diag)
      && parse_source(&arena, "roundtrip", ORIGIN_PROGRAM, &tokens, &decls, &diag)
      && check_program(&arena, &decls, &machine, &diag)
      && conv_values(&machine, 0, def_value(&machine, 0), def_value(&machine, 1));
  }
  if (!ok) {
    fprintf(stderr, "FAIL roundtrip: %s := %s\n", type, body);
    diag_print(&diag, stderr);
  }
  arena_release(&arena);
  return ok;
}

int main(void) {
  int ok = 1;
  ok &= domain("family F := makeF (field : Nat) (field : Flag)", "REFUSE_NAME");
  ok &= domain("family F := makeF (makeF : Nat)", "REFUSE_NAME");
  ok &= domain("family F := makeF (firstField : Nat) (secondField : Flag)", NULL);
  ok &= domain("family F := a (field : Nat) | b (field : Nat)", "REFUSE_NAME");
  ok &= domain("def s : Sum Nat Nat := inl 7\ndef bad : Sum Flag Nat := bind s (fun x => inr x)", "TYPE_MISMATCH");
  ok &= domain("family Lot (n : Nat) := makeLot (amount : Nat)\n"
               "def s : Sum Nat Nat := inl 0\n"
               "def f : (n : Nat) -> Nat -> Lot n := fun n x => makeLot n x\n"
               "def bad : (n : Nat) -> Lot n := first (pair (either f f s) 0)", "TYPE_MISMATCH");
  ok &= domain("def bad : Type 1 := Nat", "TYPE_MISMATCH");
  ok &= domain("def bad : Type 2 := Type 1", "TYPE_UNIVERSE");
  ok &= domain("family U := makeU (decode : Type 0)\ndef bad : Type 0 := U", "TYPE_MISMATCH");
  ok &= domain("family U := makeU (decode : Type 0)\nfamily W := makeW (wrapped : U)\ndef bad : Type 0 := W", "TYPE_MISMATCH");
  ok &= domain("family Box (A : Type 1) := box (unbox : A)\ndef bad : Type 0 := Box (Type 0)", "TYPE_MISMATCH");
  ok &= domain("family U := makeU (decode : Type 0)\ndef good : Type 1 := U", NULL);
  ok &= domain("def eta : (p : Prod Nat Nat) -> Eq (Prod Nat Nat) (pair (first p) (second p)) p := fun p => refl", NULL);
  ok &= domain("def eta : (p : Sigma (n : Nat) (Eq Nat n n)) -> Eq (Sigma (n : Nat) (Eq Nat n n)) (pack (witness p) (payload p)) p := fun p => refl", NULL);
  ok &= roundtrip("Path Nat 0 0", "<i> 0");
  ok &= roundtrip("I -> I -> I", "fun i j => i /\\ j");
  ok &= roundtrip("I -> I -> I", "fun i j => i \\/ j");
  ok &= roundtrip("I -> I", "fun i => ~ i");
  ok &= roundtrip("(p : Path Nat 0 1) -> I -> Nat", "fun p i => p @ i");
  ok &= roundtrip("(p : Path Nat 0 1) -> Path Nat 1 0", "fun p => symm p");
  ok &= roundtrip("Type 0", "Path Nat 0 0");
  ok &= roundtrip("(A : Type 0) -> (a : A) -> Path A a a", "fun A a => refl");
  ok &= domain("def law : (g : I -> I) -> (i : I) -> (p : Path Nat 0 1) -> "
               "Eq Nat (p @ (i0 /\\ g i)) 0 := fun g i p => refl", NULL);
  ok &= domain("def law : (g : I -> I) -> (i : I) -> (p : Path Nat 0 1) -> "
               "Eq Nat (p @ (~ ~ (g i))) (p @ (g i)) := fun g i p => refl", NULL);
  ok &= domain("def law : (g : I -> I) -> (h : I -> I) -> (i : I) -> (p : Path Nat 0 1) -> "
               "Eq Nat (p @ (g i /\\ h i)) (p @ (h i /\\ g i)) := fun g h i p => refl", NULL);
  ok &= domain("def law : (g : (I -> I) -> I) -> (p : Path Nat 0 1) -> "
               "Eq Nat (p @ (g (fun i => i) /\\ g (fun i => ~ ~ i))) "
               "(p @ (g (fun i => i))) := fun g p => refl", NULL);
  ok &= domain("def bad : (g : I -> I) -> (i : I) -> (p : Path Nat 0 1) -> "
               "Eq Nat (p @ (g i /\\ ~ (g i))) 0 := fun g i p => refl", "TYPE_REFL");
  ok &= domain("def bad : (g : I -> I) -> (h : I -> I) -> (i : I) -> (p : Path Nat 0 1) -> "
               "Eq Nat (p @ (g i /\\ h i)) (p @ (g i)) := fun g h i p => refl", "TYPE_REFL");
  ok &= domain("def bad : (g : I -> I) -> (i : I) -> (j : I) -> (p : Path Nat 0 1) -> "
               "Eq Nat (p @ (g i /\\ g j)) (p @ (g i)) := fun g i j p => refl", "TYPE_REFL");
  ok &= domain("def f : Nat -> Nat := fun x => x\n"
               "def law : Path (Nat -> Nat) f f := funExt (fun x => refl)", NULL);
  ok &= domain("def law : (B : Nat -> Type 0) -> (f : (x : Nat) -> B x) -> "
               "Path ((x : Nat) -> B x) f f := fun B f => funExt (fun x => refl)", NULL);
  ok &= domain("def f : Nat -> Nat := fun x => x\n"
               "def g : Nat -> Nat := fun x => natAdd x 1\n"
               "def bad : Path (Nat -> Nat) f g := funExt (fun x => refl)", "TYPE_ENDPOINT");
  ok &= roundtrip("Partial i1 Nat", "[ i1 |-> 7 ]");
  ok &= roundtrip("Nat -> Partial i1 Nat", "fun x => [ i1 |-> x ]");
  ok &= roundtrip("(i : I) -> Partial (i \\/ ~ i) Nat",
                  "fun i => [ i |-> 0 , ~ i |-> 1 ]");
  ok &= roundtrip("(p : Partial i1 Nat) -> Nat", "fun p => fromPartial p");
  ok &= roundtrip("(i : I) -> Partial (i /\\ ~ i) Nat", "fun i => []");
  ok &= roundtrip("(A : Type 0) -> (u : A) -> A", "fun A => fun u => transp (fun _ => A) i0 u");
  ok &= roundtrip("(q : Path (Type 0) Nat Nat) -> (i : I) -> Partial (~ i) Nat",
                  "fun q => fun i => [ ~ i |-> transp (fun j => q @ j) i 3 ]");
  ok &= roundtrip("(q : Path (Type 0) Nat Nat) -> (i : I) -> Partial (~ i) Nat",
                  "fun q => fun i => [ ~ i |-> transp (fun j => q @ (i /\\ j)) i1 3 ]");
  ok &= roundtrip("(A : Type 0) -> (B : Type 0) -> (q : Path (Type 0) A B) -> (f : A -> Nat) -> B -> Nat",
                  "fun A => fun B => fun q => fun f => transp (fun i => q @ i -> Nat) i0 f");
  ok &= roundtrip("(A : Type 0) -> (a : A) -> (b : A) -> Path A a b -> Eq A a b",
                  "fun A a b p => transp (fun i => Eq A a (p @ i)) i0 refl");
  ok &= roundtrip("(A : Type 0) -> (P : A -> Type 0) -> (a : A) -> (b : A) -> (p : Path A a b) -> P a -> P b",
                  "fun A P a b p u => transport P (transp (fun i => Eq A a (p @ i)) i0 refl) u");
  ok &= domain("def wrap : Nat -> Partial i1 Nat := fun x => [ i1 |-> x ]\n"
               "def law : (x : Nat) -> Eq Nat (fromPartial (wrap x)) x := fun x => refl", NULL);
  ok &= domain("def wrap : Nat -> Partial i1 Nat := fun x => [ i1 |-> natAdd x 1 ]\n"
               "def law : (x : Nat) -> Eq Nat (fromPartial (wrap x)) (natAdd x 1) := fun x => refl", NULL);
  ok &= domain("def wrap : (Nat -> Nat) -> Partial i1 (Nat -> Nat) := fun f => [ i1 |-> f ]\n"
               "def law : (f : Nat -> Nat) -> Eq (Nat -> Nat) (fromPartial (wrap f)) f := fun f => refl", NULL);
  ok &= domain("def wrap : (i : I) -> Nat -> Partial i1 Nat := fun i x => [ i |-> x , i1 |-> x ]\n"
               "def law : (i : I) -> (x : Nat) -> Eq Nat (fromPartial (wrap i x)) x := fun i x => refl", NULL);
  ok &= domain("def empty : (i : I) -> Partial (i /\\ ~ i) Nat -> Partial i0 Nat := fun i p => p", NULL);
  ok &= domain("def empty : (g : I -> I) -> (i : I) -> Partial (g i /\\ ~ (g i)) Nat -> "
               "Partial i0 Nat := fun g i p => p", NULL);
  ok &= domain("def bad : (i : I) -> Partial (i \\/ ~ i) Nat -> Partial i1 Nat := fun i p => p", "TYPE_MISMATCH");
  ok &= domain("def same : (a : I) -> (b : I) -> (c : I) -> (d : I) -> (e : I) -> "
               "(f : I) -> (g : I) -> (h : I) -> (i : I) -> "
               "Partial (a /\\ b /\\ c /\\ d /\\ e /\\ f /\\ g /\\ h /\\ i) Nat -> "
               "Partial (a /\\ b /\\ c /\\ d /\\ e /\\ f /\\ g /\\ h /\\ i) Nat := fun a b c d e f g h i p => p", NULL);
  /* Cell order and subdivision do not change the partial element. */
  ok &= domain("def reorder : (i : I) -> (j : I) -> (F : Partial (i \\/ j) Nat -> Type 0) -> "
               "F [ i |-> 0 , j |-> 0 ] -> F [ j |-> 0 , i |-> 0 ] := fun i j F x => x", NULL);
  ok &= domain("def split : (i : I) -> (j : I) -> (F : Partial (i \\/ j) Nat -> Type 0) -> "
               "F [ i |-> 0 , j |-> 0 ] -> F [ i \\/ j |-> 0 ] := fun i j F x => x", NULL);
  ok &= domain("def wrong : (i : I) -> (F : Partial (i \\/ ~ i) Nat -> Type 0) -> "
               "F [ i |-> 0 , ~ i |-> 1 ] -> F [ ~ i |-> 0 , i |-> 1 ] := fun i F x => x", "TYPE_MISMATCH");
  /* A branch can eliminate its partial inputs, only on their faces. */
  ok &= roundtrip("(i : I) -> Partial i Nat -> Partial i Nat", "fun i p => [ i |-> fromPartial p ]");
  ok &= roundtrip("(i : I) -> (j : I) -> Partial (i \\/ j) Nat -> Partial (i \\/ j) Nat",
                  "fun i j p => [ i |-> fromPartial p , j |-> fromPartial p ]");
  ok &= roundtrip("(i : I) -> (j : I) -> Partial (i \\/ j) Nat -> Partial (i \\/ j) Nat",
                  "fun i j p => [ i \\/ j |-> fromPartial p ]");
  ok &= domain("def endpoint : (p : Path Nat 0 1) -> (i : I) -> Partial i (Eq Nat (p @ i) 1) := "
               "fun p i => [ i |-> refl ]", NULL);
  ok &= domain("def endpoint : (p : Path Nat 0 1) -> (i : I) -> Partial (~ i) (Eq Nat (p @ i) 0) := "
               "fun p i => [ ~ i |-> refl ]", NULL);
  ok &= domain("def bad : (p : Path Nat 0 1) -> (i : I) -> Partial i (Eq Nat (p @ i) 0) := "
               "fun p i => [ i |-> refl ]", "TYPE_REFL");
  ok &= roundtrip("(p : Path (Type 0) Nat (Sum Nat Nat)) -> (i : I) -> Partial i (p @ i)",
                  "fun p i => [ i |-> inl 0 ]");
  ok &= domain("def bad : (i : I) -> Partial i Nat -> Partial (i \\/ ~ i) Nat := "
               "fun i p => [ i |-> fromPartial p , ~ i |-> fromPartial p ]", "TYPE_SYSTEM");
  ok &= domain("def bad : (i : I) -> Partial i Nat -> Partial (i \\/ ~ i) Nat := "
               "fun i p => [ i \\/ ~ i |-> fromPartial p ]", "TYPE_SYSTEM");
  ok &= domain("def proofs : (p : Path Nat 0 0) -> (q : Path Nat 0 0) -> (i : I) -> "
               "Partial i (Path Nat 0 0) := fun p q i => [ i |-> p , i |-> q ]", "TYPE_SYSTEM");
  ok &= domain("def opaque : (g : I -> I) -> (i : I) -> Partial (g i) Nat -> Partial (g i) Nat := "
               "fun g i p => [ g i |-> fromPartial p ]", NULL);
  ok &= roundtrip("(p : Path Nat 0 0) -> (i : I) -> Partial (i \\/ ~ i) (Eq Nat (p @ i) 0)",
                  "fun p i => [ i \\/ ~ i |-> refl ]");
  ok &= domain("def nested : (i : I) -> (f : Partial i1 Nat -> Nat) -> Partial i Nat := "
               "fun i f => [ i |-> f [ i |-> 0 ] ]", NULL);
  ok &= domain("def bad : (i : I) -> (f : Partial i1 Nat -> Nat) -> Partial (i \\/ ~ i) Nat := "
               "fun i f => [ i |-> f [ i |-> 0 ] , ~ i |-> f [ i |-> 0 ] ]", "TYPE_SYSTEM");
  ok &= domain("def boundary : (p : Path Nat 0 1) -> (i : I) -> (F : Partial i Nat -> Type 0) -> "
               "F [ i |-> p @ i ] -> F [ i |-> 1 ] := fun p i F x => x", NULL);
  if (ok) puts("domain, path and system regressions: passed");
  return ok ? 0 : 1;
}
