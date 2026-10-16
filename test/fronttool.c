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

/* As roundtrip, with a domain text (family declarations and the defs they
   need) in front of both sources, so the normal form can use constructors
   that the kit domain is not linked in for. The last def is the one printed
   and compared. */
static int roundtrip_in(const char *domain, const char *type, const char *body) {
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
  snprintf(source, sizeof source, "%s\ndef original : %s := %s\n", domain, type, body);
  ok = lex_source(&arena, "roundtrip", source, strlen(source), &tokens, &diag)
    && parse_source(&arena, "roundtrip", ORIGIN_DOMAIN, &tokens, &decls, &diag)
    && check_program(&arena, &decls, &machine, &diag)
    && value_print(&machine, NULL, 0, def_value(&machine, machine.def_count - 1u), normal, sizeof normal);
  if (ok) {
    snprintf(source, sizeof source, "%s\ndef original : %s := %s\ndef copy : %s := %s\n", domain, type, body, type, normal);
    tokens = (TokenList){0};
    decls = (DeclList){0};
    ok = lex_source(&arena, "roundtrip", source, strlen(source), &tokens, &diag)
      && parse_source(&arena, "roundtrip", ORIGIN_DOMAIN, &tokens, &decls, &diag)
      && check_program(&arena, &decls, &machine, &diag)
      && conv_values(&machine, 0, def_value(&machine, machine.def_count - 2u), def_value(&machine, machine.def_count - 1u));
  }
  if (!ok) {
    fprintf(stderr, "FAIL roundtrip_in: %s := %s\n", type, body);
    diag_print(&diag, stderr);
  }
  arena_release(&arena);
  return ok;
}

/* P4a: the kit domain's indexed family, for the C5 round trips. */
static const char circuit_domain[] =
  "family Shape := shape (dims : List Nat)\n"
  "family Circuit (a b : Shape) :=\n"
  "    circId (idA : Shape) : Circuit idA idA\n"
  "  | circSeq (seqA seqB seqC : Shape) (seqF : Circuit seqA seqB) (seqG : Circuit seqB seqC) : Circuit seqA seqC\n"
  "  | circIdl (idlA idlB : Shape) (idlF : Circuit idlA idlB)\n"
  "      : Path (Circuit idlA idlB) (circSeq idlA idlA idlB (circId idlA) idlF) idlF\n"
  "  | circIdr (idrA idrB : Shape) (idrF : Circuit idrA idrB)\n"
  "      : Path (Circuit idrA idrB) (circSeq idrA idrB idrB idrF (circId idrB)) idrF\n"
  "  | circAssoc (asA asB asC asD : Shape) (asF : Circuit asA asB) (asG : Circuit asB asC) (asH : Circuit asC asD)\n"
  "      : Path (Circuit asA asD) (circSeq asA asC asD (circSeq asA asB asC asF asG) asH) (circSeq asA asB asD asF (circSeq asB asC asD asG asH))\n"
  "  | circSquash (sqA sqB : Shape) (sqF sqG : Circuit sqA sqB) (sqP sqQ : Path (Circuit sqA sqB) sqF sqG)\n"
  "      : Path (Path (Circuit sqA sqB) sqF sqG) sqP sqQ";

int main(void) {
  int ok = 1;
  ok &= domain("family F := base | loop (x : F) : Path F x x\n"
               "def foldF : F -> Nat := fun x => fold 0 (fun y => refl) x\n"
               "def law : (i : I) -> Path Nat (foldF (loop base @ i)) 0 := fun i => refl", NULL);
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
  /* P3a (zkml-lang Z3a): higher inductive types (C4). */
  ok &= domain("family Circle := circleBase | circleLoop : Path Circle circleBase circleBase\n"
               "family Trunc (A : Type 0) := truncIn (truncVal : A)\n"
               "  | truncSquash (truncX truncY : Trunc A) : Path (Trunc A) truncX truncY\n"
               "family Quot (A : Type 0) (R : A -> A -> Type 0) := quotIn (quotRep : A)\n"
               "  | quotGlue (quotA quotB : A) (quotR : R quotA quotB) : Path (Quot A R) (quotIn A R quotA) (quotIn A R quotB)\n"
               "  | quotSquash (quotX quotY : Quot A R) (quotP quotQ : Path (Quot A R) quotX quotY) : Path (Path (Quot A R) quotX quotY) quotP quotQ", NULL);
  ok &= domain("family L (A : Type 0) := lnil | lcons (h : A) (t : L A)", NULL);
  ok &= domain("family F := base | loop (x : F) (y : Eq Nat (fold 0 0 x) 0) : Path F x x", "DOMAIN_FAMILY");
  ok &= domain("family F := base | extra (y : Eq Nat (fold 0 0 0 base) 0) | loop : Path F base base", "DOMAIN_FAMILY");
  ok &= domain("family C := c | l : Nat", "DOMAIN_HIT");
  ok &= domain("family C := c | l : Path Nat 0 0", "DOMAIN_HIT");
  ok &= domain("family C := c | l : Path C (transp (fun i => C) i0 c) c", "DOMAIN_HIT");
  ok &= domain("family C := c | l (p : Path C c c) : Path (Path (Path C c c) p p) refl refl", "DOMAIN_HIT");
  ok &= domain("family C := c | d (p : Path C c c)", "DOMAIN_HIT");
  ok &= domain("family C := c | l : Path C c c | d", "DOMAIN_HIT");
  ok &= domain("family C := l (x : C) : Path C x x", "DOMAIN_HIT");
  ok &= domain("family C := c | l : Path C c 0", "TYPE_MISMATCH");
  ok &= domain("family L (A : Type 0) := lnil | lcons (h : A) (t : L Nat)", "DOMAIN_FAMILY");
  ok &= domain("family Trunc (A : Type 0) := truncIn (truncVal : A)\n"
               "  | truncSquash (truncX truncY : Trunc A) : Path (Trunc A) truncX truncY\n"
               "def bad : Trunc Nat -> Nat := fun t => truncVal t", "NAME_UNKNOWN");
  ok &= domain("family Circle := circleBase | circleLoop : Path Circle circleBase circleBase\n"
               "def loop : Path Circle circleBase circleBase := circleLoop", NULL);
  /* A HIT argument applied to a function returned by fold is not its scrutinee. */
  ok &= domain("family C := c | loop : Path C c c\n"
               "def byNat : Nat := fold (fun (g : C -> Nat) => g) (fun (v : C) => 7) 2 c\n"
               "def natResult : Path Nat byNat 7 := refl", NULL);
  ok &= domain("family C := c | loop : Path C c c\n"
               "def byList : Nat := fold (fun (x : Nat) (g : C -> Nat) => g) (fun (v : C) => 9) (cons 1 nil) c\n"
               "def listResult : Path Nat byList 9 := refl", NULL);
  ok &= domain("family C := c | d | loop : Path C c c\n"
               "def byNat : Nat := fold (fun (g : C -> Nat) => g) (fun (v : C) => 7) 2 c\n"
               "def natResult : Path Nat byNat 7 := refl\n"
               "def byList : Nat := fold (fun (x : Nat) (g : C -> Nat) => g) (fun (v : C) => 9) (cons 1 nil) c\n"
               "def listResult : Path Nat byList 9 := refl", NULL);
  /* P3b: when a fold over a HIT and a simple fold both take the arguments,
     the fold over the HIT comes first. */
  ok &= domain("family C := cs (p : C) | cz | ct | cl : Path C cz cz\n"
               "def byFamily : Nat := fold (fun (n : Nat) => natAdd n 1) 0 5 refl (cs cz)\n"
               "def familyResult : Path Nat byFamily 1 := refl", NULL);
  /* P3b: a fold over the formal hcomp is an hcomp at B (D8); at Nat it is its base. */
  ok &= domain("family Circle := circleBase | circleLoop : Path Circle circleBase circleBase\n"
               "def sq : (i : I) -> Circle := fun i => hcomp (fun j => [ i |-> circleLoop @ j ]) circleBase\n"
               "def f : (i : I) -> Nat := fun i => fold 0 refl (sq i)\n"
               "def end1 : Path Nat (f i1) 0 := refl\n"
               "def any : (i : I) -> Path Nat (f i) 0 := fun i => refl", NULL);
  ok &= roundtrip_in("family Circle := circleBase | circleLoop : Path Circle circleBase circleBase",
                     "Circle -> Nat", "fun x => fold 0 refl x");
  ok &= roundtrip_in("family Trunc (A : Type 0) := truncIn (truncVal : A)\n"
                     "  | truncSquash (truncX truncY : Trunc A) : Path (Trunc A) truncX truncY",
                     "Trunc Nat -> Trunc Nat", "fun x => fold (fun a => truncIn Nat a) (truncSquash Nat) x");
  /* Indexed folds recover printed motive domains from the index telescope,
     including an index whose type depends on an earlier index. */
  ok &= roundtrip_in("family F (n : Nat) := makeF (value : Nat) : F value",
                     "(n : Nat) -> F n -> Nat",
                     "fun n x => fold (fun (i : Nat) => Nat) (fun k => k) x");
  ok &= roundtrip_in("family Box (A : Type 0) (x : A) := box (T : Type 0) (v : T) : Box T v",
                     "(A : Type 0) -> (x : A) -> Box A x -> A",
                     "fun A x b => fold (fun (T : Type 0) => fun (v : T) => T) (fun T v => v) b");
  ok &= roundtrip_in("family F (n : Nat) := makeF (value : Nat) : F value",
                     "(n : Nat) -> F n -> Type 0",
                     "fun n x => fold (fun (i : Nat) => Type 0) (fun k => Nat) x");
  ok &= roundtrip_in("family F (n : Nat) := makeF (value : Nat) : F value\n"
                     "def indexedFn : (n : Nat) -> F n -> Nat -> Nat := fun n x => fold (fun i => Nat -> Nat) (fun k y => natAdd k y) x",
                     "(n : Nat) -> F n -> Nat",
                     "fun n x => indexedFn n x 7");
  ok &= domain("family F (n : Nat) := makeF (value : Nat) : F value\n"
               "def answer : Nat := fold (fun (f : F 0 -> Nat) => f) (fun (x : F 0) => 7) 1 (makeF 0)\n"
               "def law : Path Nat answer 7 := refl", NULL);
  /* The final indexed argument can also have exactly the number of
     constructors that would make the whole application another fold. */
  ok &= domain("family F (n : Nat) := makeF (value : Nat) : F value | otherF (otherValue : Nat) : F otherValue\n"
               "def answer : Nat := fold (fun (f : F 0 -> Nat) => f) (fun (x : F 0) => 7) 1 (makeF 0)\n"
               "def law : Path Nat answer 7 := refl", NULL);
  ok &= roundtrip_in("family F (n : Nat) := makeF (value : Nat) : F value\n"
                     "family G (n : Nat) := makeG (gv : Nat) : G gv | otherG (ogv : Nat) : G ogv\n"
                     "def indexedFn : (n : Nat) -> F n -> G 0 -> Nat := fun n x => fold (fun i => G 0 -> Nat) (fun k y => k) x",
                     "(n : Nat) -> F n -> G 0 -> Nat",
                     "fun n x y => indexedFn n x y");
  /* A case can itself look like the scrutinee of a shorter fold. The
     remaining applications must check before that interpretation wins. */
  ok &= roundtrip_in("family F (n : Nat) := makeF : F 1\n"
                     "family G (n : Nat) := firstG : G 1 | secondG : G 0\n"
                     "def motive : Nat -> Type 0 := fun n => fold (fun (T : Type 0) => Nat -> Nat) (F 1) n\n"
                     "def indexedFn : G 1 -> Nat -> Nat := fun x => fold motive (fun y => natAdd y 1) makeF x",
                     "G 1 -> Nat", "fun x => indexedFn x 7");
  ok &= roundtrip_in("family F (n : Nat) := makeF : F 1\n"
                     "family G (n : Nat) := firstG : G 1 | secondG : G 0\n"
                     "def motive : Nat -> Type 0 := fun n => fold (fun (T : Type 0) => G 0 -> Nat) (F 1) n\n"
                     "def indexedFn : F 1 -> G 0 -> Nat := fun x => fold motive (fun y => 7) x",
                     "F 1 -> G 0 -> Nat", "fun x y => indexedFn x y");
  /* P4a: indexed point and path constructors, and both a stuck fold and
     a fold at a path constructor, print and read back. */
  ok &= roundtrip_in(circuit_domain, "Circuit (shape nil) (shape nil)",
                     "circSeq (shape nil) (shape nil) (shape nil) (circId (shape nil)) (circId (shape nil))");
  ok &= roundtrip_in(circuit_domain, "(i : I) -> Circuit (shape nil) (shape nil)",
                     "fun i => circIdl (shape nil) (shape nil) (circId (shape nil)) @ i");
  ok &= roundtrip_in(circuit_domain, "(a : Shape) -> (b : Shape) -> Circuit a b -> Circuit b a",
                     "fun a b x => fold (fun (a : Shape) => fun (b : Shape) => Circuit b a) (fun idA => circId idA)"
                     " (fun seqA seqB seqC seqF seqG => circSeq seqC seqB seqA seqG seqF)"
                     " (fun idlA idlB idlF => circIdr idlB idlA idlF) (fun idrA idrB idrF => circIdl idrB idrA idrF)"
                     " (fun asA asB asC asD asF asG asH => symm (circAssoc asD asC asB asA asH asG asF))"
                     " (fun sqA sqB sqF sqG sqP sqQ => circSquash sqB sqA sqF sqG sqP sqQ) x");
  ok &= roundtrip_in(circuit_domain, "(i : I) -> Circuit (shape nil) (shape nil)",
                     "fun i => fold (fun (a : Shape) => fun (b : Shape) => Circuit b a) (fun idA => circId idA)"
                     " (fun seqA seqB seqC seqF seqG => circSeq seqC seqB seqA seqG seqF)"
                     " (fun idlA idlB idlF => circIdr idlB idlA idlF) (fun idrA idrB idrF => circIdl idrB idrA idrF)"
                     " (fun asA asB asC asD asF asG asH => symm (circAssoc asD asC asB asA asH asG asF))"
                     " (fun sqA sqB sqF sqG sqP sqQ => circSquash sqB sqA sqF sqG sqP sqQ)"
                     " (circIdl (shape nil) (shape nil) (circId (shape nil)) @ i)");
  /* D5: hcomp at a HIT is a formal constructor (canonical, distinct from the
     base) when the faces do not hold, and the side at i1 when they do. */
  ok &= domain("family Circle := circleBase | circleLoop : Path Circle circleBase circleBase\n"
               "def sq : (i : I) -> Circle := fun i => hcomp (fun j => [ i |-> circleLoop @ j ]) circleBase\n"
               "def top : Path Circle (sq i1) circleBase := refl\n"
               "def same : (i : I) -> Path Circle (sq i) (hcomp (fun j => [ i |-> circleLoop @ j ]) circleBase) := fun i => refl\n"
               "def tr : Path Circle circleBase circleBase := trans circleLoop circleLoop\n"
               "def e0 : Path Circle (tr @ i0) circleBase := refl", NULL);
  ok &= domain("family Circle := circleBase | circleLoop : Path Circle circleBase circleBase\n"
               "def sq : (i : I) -> Circle := fun i => hcomp (fun j => [ i |-> circleLoop @ j ]) circleBase\n"
               "def bad : (i : I) -> Path Circle (sq i) circleBase := fun i => refl", "TYPE_ENDPOINT");
  /* Round trips through the printer with the family in scope: a path
     constructor at an interval (print_pctor), a square at two intervals, and
     a formal hcomp at a HIT (print_path_syntax). */
  ok &= roundtrip_in("family Circle := circleBase | circleLoop : Path Circle circleBase circleBase",
                     "(i : I) -> Circle", "fun i => circleLoop @ i");
  ok &= roundtrip_in("family Quot (A : Type 0) (R : A -> A -> Type 0) := quotIn (quotRep : A)\n"
                     "  | quotGlue (quotA quotB : A) (quotR : R quotA quotB) : Path (Quot A R) (quotIn A R quotA) (quotIn A R quotB)\n"
                     "  | quotSquash (quotX quotY : Quot A R) (quotP quotQ : Path (Quot A R) quotX quotY) : Path (Path (Quot A R) quotX quotY) quotP quotQ\n"
                     "def Q : Nat -> Nat -> Type 0 := fun x => fun y => Nat",
                     "(i : I) -> (j : I) -> Quot Nat Q",
                     "fun i j => (quotSquash Nat Q (quotIn Nat Q 1) (quotIn Nat Q 2) (quotGlue Nat Q 1 2 0) (quotGlue Nat Q 1 2 1) @ i) @ j");
  ok &= roundtrip_in("family Circle := circleBase | circleLoop : Path Circle circleBase circleBase",
                     "(i : I) -> Circle", "fun i => hcomp (fun j => [ i |-> circleLoop @ j ]) circleBase");
  /* D4: each end rule of a path constructor. refl quotes its end under the
     binder, so t i0 and u i0 s reach the PCTOR value at an end. */
  ok &= domain("family Quot (A : Type 0) (R : A -> A -> Type 0) := quotIn (quotRep : A)\n"
               "  | quotGlue (quotA quotB : A) (quotR : R quotA quotB) : Path (Quot A R) (quotIn A R quotA) (quotIn A R quotB)\n"
               "  | quotSquash (quotX quotY : Quot A R) (quotP quotQ : Path (Quot A R) quotX quotY) : Path (Path (Quot A R) quotX quotY) quotP quotQ\n"
               "def Q : Nat -> Nat -> Type 0 := fun x => fun y => Nat\n"
               "def g0 : Path (Quot Nat Q) (quotIn Nat Q 1) (quotIn Nat Q 2) := quotGlue Nat Q 1 2 0\n"
               "def g1 : Path (Quot Nat Q) (quotIn Nat Q 1) (quotIn Nat Q 2) := quotGlue Nat Q 1 2 1\n"
               "def sq : Path (Path (Quot Nat Q) (quotIn Nat Q 1) (quotIn Nat Q 2)) g0 g1 := quotSquash Nat Q (quotIn Nat Q 1) (quotIn Nat Q 2) g0 g1\n"
               "def t : (r : I) -> Path (Quot Nat Q) (g0 @ r) (g0 @ r) := fun r => refl\n"
               "def e0 : Path (Path (Quot Nat Q) (quotIn Nat Q 1) (quotIn Nat Q 1)) (t i0) refl := refl\n"
               "def e1 : Path (Path (Quot Nat Q) (quotIn Nat Q 2) (quotIn Nat Q 2)) (t i1) refl := refl\n"
               "def u : (r : I) -> (s : I) -> Path (Quot Nat Q) ((sq @ r) @ s) ((sq @ r) @ s) := fun r => fun s => refl\n"
               "def f0 : (s : I) -> Path (Path (Quot Nat Q) (g0 @ s) (g0 @ s)) (u i0 s) refl := fun s => refl\n"
               "def f1 : (s : I) -> Path (Path (Quot Nat Q) (g1 @ s) (g1 @ s)) (u i1 s) refl := fun s => refl\n"
               "def h0 : (r : I) -> Path (Path (Quot Nat Q) (quotIn Nat Q 1) (quotIn Nat Q 1)) (u r i0) refl := fun r => refl\n"
               "def h1 : (r : I) -> Path (Path (Quot Nat Q) (quotIn Nat Q 2) (quotIn Nat Q 2)) (u r i1) refl := fun r => refl", NULL);
  ok &= domain("def s : Sum Nat Nat := inl 7\ndef bad : Sum Flag Nat := bind s (fun x => inr x)", "TYPE_MISMATCH");
  ok &= domain("family Lot (n : Nat) := makeLot (amount : Nat)\n"
               "def s : Sum Nat Nat := inl 0\n"
               "def f : (n : Nat) -> Nat -> Lot n := fun n x => makeLot n x\n"
               "def bad : (n : Nat) -> Lot n := first (pair (either f f s) 0)", "TYPE_MISMATCH");
  /* P4a: indexed sorts with path constructors (C5, D1 to D4). Field names
     are domain-wide (rule R4), so each constructor has its own. */
  ok &= domain("family Sh := sh (n : Nat)\n"
               "family C (a b : Sh) :=\n"
               "    cId (ia : Sh) : C ia ia\n"
               "  | cSeq (sa sb sc : Sh) (sf : C sa sb) (sg : C sb sc) : C sa sc\n"
               "  | cIdl (la lb : Sh) (lf : C la lb) : Path (C la lb) (cSeq la la lb (cId la) lf) lf\n"
               "  | cSquash (qa qb : Sh) (qf qg : C qa qb) (qp qq : Path (C qa qb) qf qg) : Path (Path (C qa qb) qf qg) qp qq", NULL);
  ok &= domain("family Sh := sh (n : Nat)\nfamily C (a b : Sh) := cId (ia : Sh) : C ia ia | cBad (ba : Sh)", "DOMAIN_INDEX");
  ok &= domain("family Sh := sh (n : Nat)\nfamily C (a b : Sh) := cId (ia : Sh) : C ia ia | cBad (ba : Sh) (bf : C ba ba) : C ba ((fun (z : C ba ba) => ba) bf)", "DOMAIN_INDEX");
  ok &= domain("family Sh := sh (n : Nat)\nfamily C (a b : Sh) := cId (ia : Sh) : C ia ia | cBad (ba : Sh) (bf : C ba ba) (bg : C ba ((fun (z : C ba ba) => ba) bf)) : C ba ba", "DOMAIN_INDEX");
  ok &= domain("family Sh := sh (n : Nat)\nfamily C (a b : Sh) := cId (ia : Sh) : C ia ia | cBad (ba : Sh) (bf : C ba ba) (be : Eq Sh ((fun (z : C ba ba) => ba) bf) ba) : C ba ba", "DOMAIN_INDEX");
  ok &= domain("family Sh := sh (n : Nat)\nfamily C (a b : Sh) := cId (ia : Sh) : C ia ia | cBad (ba : Sh) : Sh", "DOMAIN_HIT");
  ok &= domain("family Sh := sh (n : Nat)\nfamily C (a b : Sh) := cId (ia : Sh) : C ia ia | cBad (ba : Sh) (bf : C ba ba) : Path (C ba ba) ((fun (z : C ba ba) => z) bf) bf", "DOMAIN_HIT");
  ok &= domain("family Sh := sh (n : Nat)\nfamily C (a b : Sh) := cId (ia : Sh) : C ia ia\ndef bad : C (sh 0) (sh 0) -> Nat := fun x => fold (fun a => 0) x", "TYPE_MISMATCH");
  ok &= domain("family F (n : Nat) := makeF (value : Nat) : F value\n"
               "def bad : (n : Nat) -> F n -> Nat := fun n x => fold (fun (i : Flag) => Nat) (fun k => k) x", "TYPE_MISMATCH");
  ok &= domain("family F (n : Nat) := makeF (value : Nat) : F value\n"
               "def bad : (n : Nat) -> F n -> Nat := fun n x => fold (fun i => 0) (fun k => k) x", "TYPE_MISMATCH");
  ok &= domain("family F := base | loop (x : F) (p : Path F ((fun (z : F) => z) x) x) : Path F x x", "DOMAIN_HIT");
  ok &= domain("family F := base | sq (x : F) : Path (Path F x x) refl refl", "DOMAIN_HIT");
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
