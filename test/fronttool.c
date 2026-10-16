/* Domain declaration regressions use the same parser and checker as langc. */
#include <stdio.h>
#include <string.h>

#include "front/check.h"
#include "front/lexer.h"
#include "front/parser.h"

static int domain_test(const char *text, const char *want, int faces) {
  Arena arena;
  Diag diag;
  TokenList tokens = {0};
  DeclList decls = {0};
  Machine machine;
  arena_init(&arena, (size_t)1 << (faces ? 26 : 24));
  diag_init(&diag);
  int ok = lex_source(&arena, "test-domain", text, strlen(text), &tokens, &diag)
    && parse_source(&arena, "test-domain", ORIGIN_DOMAIN, &tokens, &decls, &diag)
    && check_program(&arena, &decls, &machine, &diag);
  int passed = want == NULL ? ok : !ok && diag.code != NULL && strcmp(diag.code, want) == 0;
  if (passed && faces) {
    const Value *p = val_var(&machine, 0);
    const Value *j = val_var(&machine, 1);
    const Value *a = apply_value(&machine, apply_value(&machine, def_value(&machine, machine.def_count - 2u), p), j);
    if (faces == 1) {
      const Value *b = apply_value(&machine, apply_value(&machine, def_value(&machine, machine.def_count - 1u), p), j);
      const Value *neg = val_op(&machine, VAL_OP, OP_INEG, 0, 0, &j, 1u);
      passed = a != NULL && b != NULL && neg != NULL
        && conv_under_face(&machine, 2u, j, a, b)
        && conv_under_face(&machine, 2u, neg, a, b);
    } else {
      const Core *normal = a == NULL ? NULL : quote_value(&machine, 2u, a);
      const Env *env = env_push(&machine, NULL, def_value(&machine, machine.def_count - 4u));
      env = env == NULL ? NULL : env_push(&machine, env, def_value(&machine, machine.def_count - 3u));
      const Value *specialized = normal == NULL || env == NULL ? NULL : eval_core(&machine, env, normal);
      passed = specialized != NULL && conv_values(&machine, 0u, specialized, def_value(&machine, machine.def_count - 1u));
    }
  }
  if (!passed) {
    fprintf(stderr, "FAIL domain: wanted %s, got %s: %s\n", want == NULL ? "ok" : want,
            ok ? "ok" : diag.code == NULL ? "no diagnostic" : diag.code, text);
    diag_print(&diag, stderr);
  }
  arena_release(&arena);
  return passed;
}

static int domain(const char *text, const char *want) {
  return domain_test(text, want, 0);
}

static int glue_test(const char *text, const char *want, int faces) {
  char source[16384];
  int n = snprintf(source, sizeof source,
    "def isContr : Type 0 -> Type 0 := fun A => Sigma (x : A) ((y : A) -> Path A x y)\n"
    "def fiber : (A B : Type 0) -> (A -> B) -> B -> Type 0 := fun A B f y => Sigma (x : A) (Path B (f x) y)\n"
    "def isEquiv : (A B : Type 0) -> (A -> B) -> Type 0 := fun A B f => (y : B) -> isContr (fiber A B f y)\n"
    "def Equiv : (A B : Type 0) -> Type 0 := fun A B => Sigma (f : A -> B) (isEquiv A B f)\n"
    "def idEquiv : (A : Type 0) -> Equiv A A := fun A => pack (fun x => x) "
      "(fun y => pack (pack y refl) (fun z => <i> pack (payload z @ ~ i) (<j> payload z @ (~ i \\/ j))))\n"
    "def G : Type 0 := Glue Nat []\n"
    "def x : G := glue [] 7\n%s", text);
  return n >= 0 && (size_t)n < sizeof source && domain_test(source, want, faces);
}

static int glue_domain(const char *text, const char *want) {
  return glue_test(text, want, 0);
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
  /* Glue eta must compose with eta in its base type, in both directions. */
  ok &= glue_domain(
    "def GP : Type 0 := Glue (Prod Nat Nat) []\n"
    "def expand : GP -> GP := fun g => glue [] (pair (first (unglue g)) (second (unglue g)))\n"
    "def baseEta : (g : GP) -> Path (Prod Nat Nat) (pair (first (unglue g)) (second (unglue g))) (unglue g) := fun g => refl\n"
    "def rebuildEta : (g : GP) -> Path GP (expand g) (glue [] (unglue g)) := fun g => refl\n"
    "def eta : (g : GP) -> Path GP (expand g) g := fun g => refl\n"
    "def etaRev : (g : GP) -> Path GP g (expand g) := fun g => refl", NULL);
  ok &= glue_domain(
    "def GS : Type 0 := Glue (Sigma (x : Nat) (Path Nat 7 x)) []\n"
    "def eta : (g : GS) -> Path GS (glue [] (pack (witness (unglue g)) (payload (unglue g)))) g := fun g => refl", NULL);
  ok &= glue_domain(
    "def GF : Type 0 := Glue (Nat -> Nat) []\n"
    "def eta : (g : GF) -> Path GF (glue [] (fun x => unglue g x)) g := fun g => refl", NULL);
  ok &= glue_domain(
    "def GP : Type 0 := Glue (Path Nat 7 7) []\n"
    "def eta : (g : GP) -> Path GP (glue [] (<i> unglue g @ i)) g := fun g => refl", NULL);
  ok &= glue_domain(
    "def GG : Type 0 := Glue G []\n"
    "def eta : (g : GG) -> Path GG (glue [] (glue [] (unglue (unglue g)))) g := fun g => refl", NULL);
  ok &= glue_domain(
    "def eta : (T : Type 0) -> (e : Equiv T (Prod Nat Nat)) -> (j : I) -> "
      "(g : Glue (Prod Nat Nat) [ j |-> pack T e ]) -> "
      "Path (Glue (Prod Nat Nat) [ j |-> pack T e ]) "
      "(glue [ j |-> g ] (pair (first (unglue g)) (second (unglue g)))) g := fun T e j g => refl", NULL);
  ok &= glue_domain(
    "def bad : (g : G) -> Path G (glue [] 7) g := fun g => refl", "TYPE_ENDPOINT");
  /* Equal unglued bases still require equality on every glued face. */
  ok &= glue_domain(
    "def bad : (q : isEquiv Nat Nat (fun x => 0)) -> (j : I) -> "
      "(g : Glue Nat [ j |-> pack Nat (pack (fun x => 0) q) ]) -> "
      "Path (Glue Nat [ j |-> pack Nat (pack (fun x => 0) q) ]) "
      "(glue [ j |-> 0 ] (unglue g)) g := fun q j g => refl", "TYPE_ENDPOINT");
  /* Normalize before restricting: Kan reduction must commute with faces. */
  ok &= glue_test(
    "def GT : (p j : I) -> Type 0 := fun p j => Glue Nat [ j |-> pack Nat (idEquiv Nat) , "
      "~ j |-> pack Nat (idEquiv Nat) , p |-> pack Nat (idEquiv Nat) ]\n"
    "def actual : (p j : I) -> GT p j := fun p j => "
      "transp (fun i => Glue Nat [ j |-> pack Nat (idEquiv Nat) , ~ j |-> pack Nat (idEquiv Nat) , "
      "(i /\\ p) |-> pack Nat (idEquiv Nat) ]) i0 (glue [ (j \\/ ~ j) |-> 3 ] 3)\n"
    "def expected : (p j : I) -> GT p j := fun p j => glue [ (j \\/ ~ j \\/ p) |-> 3 ] 3", NULL, 1);
  ok &= glue_test(
    "def GT : (p j : I) -> Type 0 := fun p j => Glue Nat [ (j \\/ ~ j) |-> pack Nat (idEquiv Nat) , "
      "p |-> pack Nat (idEquiv Nat) ]\n"
    "def actual : (p j : I) -> GT p j := fun p j => "
      "transp (fun i => Glue Nat [ (j \\/ ~ j) |-> pack Nat (idEquiv Nat) , "
      "(i /\\ p) |-> pack Nat (idEquiv Nat) ]) i0 (glue [ j |-> 3 , ~ j |-> 3 ] 3)\n"
    "def expected : (p j : I) -> GT p j := fun p j => glue [ (j \\/ ~ j \\/ p) |-> 3 ] 3", NULL, 1);
  /* A neutral face application must not capture the transport dimension. */
  ok &= glue_test(
    "def arg0 : I -> I := fun i => i0\n"
    "def arg1 : Glue Nat [] := glue [] 3\n"
    "def actual : (f : I -> I) -> Glue Nat [ f i0 |-> pack Nat (idEquiv Nat) ] -> "
      "Glue Nat [ f i1 |-> pack Nat (idEquiv Nat) ] := fun f u => "
      "transp (fun i => Glue Nat [ f i |-> pack Nat (idEquiv Nat) ]) i0 u\n"
    "def expected : Glue Nat [] := actual arg0 arg1", NULL, 2);
  ok &= glue_domain(
    "def P : Type 0 := Sigma (x : Nat) Nat\n"
    "def swapNN : P -> P := fun p => pack (payload p) (witness p)\n"
    "def sw : Equiv P P := pack swapNN (fun y => pack (pack (swapNN y) refl) "
      "(fun z => <i> pack (swapNN (payload z @ ~ i)) (<j> payload z @ (~ i \\/ j))))\n"
    "def GT : I -> Type 0 := fun j => Glue P [ j |-> pack P sw , ~ j |-> pack P (idEquiv P) ]\n"
    "def gv : (j : I) -> GT j := fun j => glue [ ~ j |-> pack 3 5 , j |-> pack 5 3 ] (pack 3 5)\n"
    "def good : (j : I) -> Path (GT j) (hcomp (fun l => []) (gv j)) (gv j) := fun j => refl\n"
    "def goodSide : (j k : I) -> Path (GT j) (hcomp (fun l => [ k |-> gv j ]) (gv j)) (gv j) := fun j k => refl\n"
    "def HT : (j k : I) -> Type 0 := fun j k => Glue P [ (j \\/ k) |-> pack P sw , "
      "(~ j /\\ ~ k) |-> pack P (idEquiv P) ]\n"
    "def hv : (j k : I) -> HT j k := fun j k => glue [ j |-> pack 5 3 , k |-> pack 5 3 , "
      "(~ j /\\ ~ k) |-> pack 3 5 ] (pack 3 5)\n"
    "def goodUnion : (j k : I) -> Path (HT j k) (hcomp (fun l => []) (hv j k)) (hv j k) := fun j k => refl", NULL);
  /* A total outer Glue face applies its equivalence to an inner glue. */
  ok &= glue_domain("def good : (j : I) -> (e : Equiv G Nat) -> Glue Nat [ j |-> pack G e ] := "
                    "fun j e => glue [ j |-> x ] (witness e x)", NULL);
  ok &= glue_domain("def bad : (j : I) -> (e : Equiv G Nat) -> Glue Nat [ j |-> pack G e ] := "
                    "fun j e => glue [ j |-> x ] 7", "GLUE_BOUNDARY");
  ok &= glue_domain("def out : (j : I) -> Glue G [ j |-> pack G (idEquiv G) ] -> G := fun j v => unglue v\n"
                    "def law : Path G (out i1 x) x := refl", NULL);
  /* Boundary and coverage comparisons include the enclosing system face. */
  ok &= glue_domain("def good : (i j : I) -> (p : Path Nat 5 6) -> Partial i (Glue Nat [ j |-> pack Nat (idEquiv Nat) ]) := "
                    "fun i j p => [ i |-> glue [ j |-> 6 ] (p @ i) ]", NULL);
  ok &= glue_domain("def bad : (i j : I) -> (p : Path Nat 5 6) -> Partial i (Glue Nat [ j |-> pack Nat (idEquiv Nat) ]) := "
                    "fun i j p => [ i |-> glue [ j |-> 5 ] (p @ i) ]", "GLUE_BOUNDARY");
  ok &= glue_domain("def good : (i j : I) -> Partial i (Glue Nat [ j |-> pack Nat (idEquiv Nat) ]) := "
                    "fun i j => [ i |-> glue [ i /\\ j |-> 7 ] 7 ]", NULL);
  ok &= glue_domain("def good : (i j k : I) -> (p : Path Nat 5 6) -> Partial (i \\/ k) (Glue Nat [ j |-> pack Nat (idEquiv Nat) ]) := "
                    "fun i j k p => [ i \\/ k |-> glue [ (i \\/ k) /\\ j |-> 6 ] (p @ (i \\/ k)) ]", NULL);
  ok &= glue_domain("def bad : (i j k : I) -> (p : Path Nat 5 6) -> Partial (i \\/ k) (Glue Nat [ j |-> pack Nat (idEquiv Nat) ]) := "
                    "fun i j k p => [ i \\/ k |-> glue [ (i \\/ k) /\\ j |-> 5 ] (p @ (i \\/ k)) ]", "GLUE_BOUNDARY");
  ok &= domain("def side : (i : I) -> Partial i Nat := fun i => [ i |-> 3 ]\n"
               "def good : (i : I) -> Nat := fun i => hcomp (fun j => [ i |-> fromPartial (side i) ]) 3", NULL);
  ok &= domain("def side : (i : I) -> (k : I) -> Partial (i /\\ k) Nat := fun i k => [ i /\\ k |-> 3 ]\n"
               "def good : (i : I) -> (k : I) -> Partial k Nat := fun i k => [ k |-> hcomp (fun j => [ i |-> fromPartial (side i k) ]) 3 ]", NULL);
  ok &= domain("def side : (i : I) -> (k : I) -> Partial (i /\\ k) Nat := fun i k => [ i /\\ k |-> 3 ]\n"
               "def bad : (i : I) -> (k : I) -> Nat := fun i k => hcomp (fun j => [ i |-> fromPartial (side i k) ]) 3", "TYPE_SYSTEM");
  ok &= domain("def side : (i : I) -> Partial i Nat := fun i => [ i |-> 3 ]\n"
               "def bad : (i : I) -> Nat := fun i => hcomp (fun j => [ i |-> fromPartial (side i) , i1 |-> fromPartial (side i) ]) 3", "TYPE_SYSTEM");
  ok &= domain("def bad : I := hcomp (fun j => [ i1 |-> j ]) i0", "TYPE_PRETYPE");
  ok &= domain("def bad : I := hcomp (fun j => []) i0", "TYPE_PRETYPE");
  ok &= domain("def bad : I -> Nat := hcomp (fun j => []) (fun i => 3)", "TYPE_PRETYPE");
  ok &= domain("def bad : Nat -> I := hcomp (fun j => []) (fun x => i0)", "TYPE_PRETYPE");
  ok &= domain("def bad : Partial i1 Nat := hcomp (fun j => []) [ i1 |-> 3 ]", "TYPE_PRETYPE");
  ok &= domain("def s : Partial i1 Nat := [ i1 |-> 3 ]\ndef bad : Nat := fromPartial (hcomp (fun j => []) s)", "TYPE_PRETYPE");
  ok &= domain("def good : (i : I) -> Nat := fun i => hcomp (fun i => [ i |-> 3 ]) 3", NULL);
  ok &= domain("def bad : (i : I) -> Nat := fun i => hcomp (fun i => [ i |-> 3 ]) 4", "TYPE_SYSTEM");
  ok &= domain("def good : (i : I) -> Path Nat 3 4 -> Nat := fun i p => hcomp (fun i => [ i |-> p @ i ]) 3", NULL);
  ok &= domain("def bad : (i : I) -> Path Nat 3 4 -> Nat := fun i p => hcomp (fun i => [ i |-> p @ i , i1 |-> 3 ]) 3", "TYPE_SYSTEM");
  ok &= roundtrip("(i : I) -> Nat", "fun i => hcomp (fun i => [ i |-> 3 ]) 3");
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
  ok &= roundtrip("(A : Type 0) -> (a : A) -> (b : A) -> Path A a b -> Path A a b",
                  "fun A a b p => <i> hcomp (fun j => [ ~ i |-> a , i |-> b ]) (p @ i)");
  ok &= roundtrip("(f : Nat -> Nat) -> Path (Nat -> Nat) f f", "fun f => <i> hcomp (fun j => [ ~ i |-> f , i |-> f ]) f");
  ok &= roundtrip("(s : Sigma (x : Nat) Nat) -> Sigma (x : Nat) Nat", "fun s => hcomp (fun j => []) s");
  ok &= roundtrip("(q : Path Nat 3 3) -> Path Nat 3 3", "fun q => transp (fun i => Path Nat 3 3) i0 q");
  ok &= roundtrip("(p : Path Nat 3 4) -> (q : Path Nat 4 5) -> Path Nat 3 5", "fun p q => trans p q");
  ok &= roundtrip("(A : I -> Type 0) -> (u : A i0) -> (r : I) -> A r",
                  "fun A u r => fill A (fun j => []) u r");
  ok &= roundtrip("(P : I -> I -> Type 0) -> (a : (i : I) -> P i i0) -> (b : (i : I) -> P i i1) -> (p : PathP (P i0) (a i0) (b i0)) -> PathP (P i1) (a i1) (b i1)",
                  "fun P a b p => transp (fun i => PathP (P i) (a i) (b i)) i0 p");
  ok &= roundtrip("(A : Type 0) -> (B : A -> Type 0) -> (s : Sigma (x : A) (B x)) -> Sigma (x : A) (B x)",
                  "fun A B s => hcomp (fun j => []) s");
  ok &= domain("def good : (A : I -> Type 0) -> (u : A i0) -> (r : I) -> (s : I) -> Partial (~ r \\/ ~ s) (A (r /\\ s)) := fun A u r s => [ ~ r \\/ ~ s |-> u ]", NULL);
  ok &= domain("def bad : (A : I -> Type 0) -> (u : A i0) -> (r : I) -> (s : I) -> Partial (~ r \\/ s) (A (r /\\ s)) := fun A u r s => [ ~ r \\/ s |-> u ]", "TYPE_MISMATCH");
  ok &= domain("def good : (A : I -> Type 0) -> (u : A i0) -> (r : I) -> Partial (~ r) (A r) := fun A u r => [ ~ r |-> hcomp (fun j => [ i1 |-> u ]) u ]", NULL);
  ok &= domain("def good : (A : I -> Type 0) -> (u : A i0) -> (r : I) -> Partial (~ r) (A r) := fun A u r => [ ~ r |-> transp (fun j => A (r /\\ j)) i1 u ]", NULL);
  ok &= domain("def bad : (A : I -> Type 0) -> (u : A i0) -> (r : I) -> A r := fun A u r => (fun (s : Partial (~ r) (A r)) => u) [ ~ r |-> u ]", "TYPE_MISMATCH");
  ok &= domain("def bad : (A : I -> Type 0) -> (u : A i0) -> (r : I) -> Partial (~ r) (A r) := fun A u r => [ ~ r |-> missing ]", "NAME_UNKNOWN");
  ok &= domain("def bad : Partial i0 Nat := [ i0 |-> flagYes ]", "TYPE_MISMATCH");
  ok &= domain("def good : (p : Path Nat 3 4) -> (r : I) -> Partial (~ r) Nat := fun p r => [ ~ r |-> hcomp (fun j => [ i1 |-> p @ r , i1 |-> 3 ]) 3 ]", NULL);
  ok &= domain("def bad : (p : Path Nat 3 4) -> (r : I) -> Partial (~ r) Nat := fun p r => [ ~ r |-> hcomp (fun j => [ i1 |-> p @ r , i1 |-> 4 ]) 3 ]", "TYPE_SYSTEM");
  ok &= domain("def good : (p : Path Nat 3 4) -> (r : I) -> Partial (~ r) (Path Nat (p @ r) 3) := fun p r => [ ~ r |-> refl ]", NULL);
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
