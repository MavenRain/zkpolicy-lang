# Type formers

This file specifies the type formers of each language that starts from this
template. The specification does not depend on a host. A language has these
formers and no others. A language adds its core types and core operations
from `design/DESIGN.md`. It does not add a type former.

The forms below use one abstract syntax: the surface syntax of ledger-lang
`SPEC.md` sections 3 and 4 (commit 997fa7b). escrow-lang `SPEC.md` sections 2
and 3 (commit 279214c) write the same formers in assay forms. Each host file
(`hosts/mech/FORMERS.md`, `hosts/assay/FORMERS.md`, `hosts/tcc-json/FORMERS.md`,
`hosts/tcc-evm-contract/FORMERS.md`, `hosts/tcc-wasm/FORMERS.md`,
`hosts/tcc-evm/FORMERS.md`, `hosts/tcc-evm-dao/FORMERS.md`,
`hosts/tcc-evm-anchor/FORMERS.md`, `hosts/tcc-js/FORMERS.md`, `hosts/tcc-media/FORMERS.md`,
`hosts/tcc-webgpu/FORMERS.md`) gives the
host form of each former and its status. Section 5 of this file is the
realization matrix.

## 1. Notation

- `A`, `B`, `C` and `E` are data types. `S` is a seed type. `I` is an index
  type. `P` is a type function into `Type 0`.
- `a : A` means that the term `a` has the type `A`.
- `t[x := a]` is the term `t` with `a` in place of each free `x`.
- `=` in a law is equality of closed values. A host compares two closed
  values by its own method. mech compares the JSON encodings. assay uses
  kernel conversion.
- `Unit` is a type with one value, `unit`.
- A test is a function `p : A -> Option Unit`. The result `some unit` keeps
  the element. A host can use a type with two values in place of
  `Option Unit` (mech uses `Flag`, and `true` keeps).

## 2. Programs and refusal rules

A program is a sequence of definitions:

```
def NAME : TYPE := TERM
```

A definition can refer only to the definitions before it, to the core types
and operations, and to the forms of this file. The compiler applies these
refusal rules. They keep a program inside the former layer.

- **R1. No new data type.** The compiler refuses the host form that declares
  a data type (mech `mu`; assay `mu` and `nu`).
- **R2. No unproved fact.** The compiler refuses `axiom`. When the host kernel
  accepts an axiom, the compiler runs the host axiom report on its output. It
  refuses each axiom that the host file does not list as part of the target
  protocol.
- **R3. No general recursion.** The compiler refuses the host form for a
  recursive definition (`def rec`). Recursion comes only from F6 `fold` and
  F7 `unfold`. When F7 has the status HOST-LIMIT, recursion comes only from
  F6.
- **R4. No new core name.** The compiler refuses a definition whose name is
  a core type, a core operation, a prelude name or a reserved name.
- **R5. Host forms.** Each host adds the host forms that are not formers
  (mech: `poly`, `specialize`; assay: the contract forms). The host file
  gives the full list. `SPEC.md` section 2 copies it.

The compiler writes the target. A program cannot write a part of the target
directly.

## 3. Formers

Each former has a formation rule, introduction forms, elimination forms and
laws. A realization must satisfy each law that this section gives. A law
that a host cannot satisfy makes the status PARTIAL or HOST-LIMIT.

### F1 Product

- Formation: `Prod A B : Type 0` when `A : Type 0` and `B : Type 0`.
- Introduction: `pair a b : Prod A B` when `a : A` and `b : B`.
- Elimination: `first p : A` and `second p : B` when `p : Prod A B`.
- Laws: `first (pair a b) = a`. `second (pair a b) = b`.
  `pair (first p) (second p) = p`.

### F2 Coproduct

- Formation: `Sum A B : Type 0` when `A : Type 0` and `B : Type 0`.
- Introduction: `inl a : Sum A B` when `a : A`. `inr b : Sum A B` when
  `b : B`.
- Elimination: `either f g s : C` when `f : A -> C`, `g : B -> C` and
  `s : Sum A B`.
- Laws: `either f g (inl a) = f a`. `either f g (inr b) = g b`.

### F3 Option

- Formation: `Option A : Type 0` when `A : Type 0`.
- Introduction: `none : Option A`. `some a : Option A` when `a : A`.
- Elimination: `option z f o : C` when `z : C`, `f : A -> C` and
  `o : Option A`. The F5 and F8 forms also consume an option.
- Laws: `option z f none = z`. `option z f (some a) = f a`.
- A host can define `Option A` as `Sum Unit A`. Then `none` is `inl unit`,
  `some a` is `inr a`, and `option z f o = either (fun u => z) f o`.

### F4 List

- Formation: `List A : Type 0` when `A : Type 0`.
- Introduction: `nil : List A`. `cons a l : List A` when `a : A` and
  `l : List A`.
- Elimination: F6 `fold`. The F5 and F8 forms also consume a list.
- Laws: the F6 laws for the carrier `List A`.

### F5 Monad

- Instances: `Option`, `Sum E` and `List`. The declared type of the form
  selects the instance. Write `F` for the instance.
- Forms: `pure a : F A` when `a : A`. `map f t : F B` when `f : A -> B` and
  `t : F A`. `bind f t : F B` when `f : A -> F B` and `t : F A`.
- Laws:
  - Left identity: `bind f (pure a) = f a`.
  - Right identity: `bind pure t = t`.
  - Associativity: `bind g (bind f t) = bind (fun x => bind g (f x)) t`.
  - Map: `map f t = bind (fun x => pure (f x)) t`.
- For `Sum E`, `pure` is `inr`, and `bind f (inl e) = inl e`. For `List`,
  `bind` maps each element to a list and joins the lists in order.

### F6 Algebra fold

- Carriers: `Nat`, `List A`, and each recursive core type that the host
  column lists.
- Form over `Nat`: `fold f z n : C` when `f : C -> C`, `z : C` and
  `n : Nat`. Laws: `fold f z 0 = z`. `fold f z (n + 1) = f (fold f z n)`.
- Motive form over `Nat` (part P7b-nat): `fold M step base n : M n` when
  `M : Nat -> Type l`, `base : M 0`,
  `step : (k : Nat) -> M k -> M (natAdd k 1)` and `n : Nat`. Laws:
  `fold M step base 0 = base`.
  `fold M step base (k + 1) = step k (fold M step base k)`. The second
  law also applies to a neutral `natAdd x 1` (the succ iota). The check
  reads a call with at least four arguments as the motive form only when
  the first argument checks as a motive and the fourth is a Nat. Remaining
  arguments apply to its result. Partial applications check against an
  expected function type. Otherwise the check reads it as the form above
  and applies its result to the remaining arguments. When the motive
  form fails, the check reads the call as a fold over a family
  (`rule_fold_family`) before it refuses. Thus a family fold whose fourth
  argument is a Nat and whose motive fits `Nat -> Type` checks
  (fronttool row `indexedFn n x 7`). When both fail, the error of the
  motive form stays (`test/check/natfold-step-endpoint.lang`,
  `TYPE_ENDPOINT`). The section `Nat conversion and record eta` gives
  the conversion rules that proofs by induction use.
- Form over `List A`: `fold f z l : C` when `f : A -> C -> C`, `z : C` and
  `l : List A`. Laws: `fold f z nil = z`.
  `fold f z (cons a l) = f a (fold f z l)`. Thus `fold` consumes the
  elements from the right.
- Motive form over `List A` (part P7b-list): `fold M step base xs : M xs`
  when `M : List A -> Type l`, `base : M nil`,
  `step : (a : A) -> (l : List A) -> M l -> M (cons a l)` and
  `xs : List A`. Laws: `fold M step base nil = base`.
  `fold M step base (cons a l) = step a l (fold M step base l)`. The step
  gets the head and the tail of each cell. The check reads a call with at
  least four arguments as this form when the fourth argument has type
  `List A` and the first argument checks as a motive over `List A`. It
  checks the base against `M nil`, then the step. When the form fails, the
  check reads the call as a fold over a family, as for `Nat`. When both
  fail, the error of the motive form stays
  (`test/check/listfold-step-endpoint.lang`, `TYPE_ENDPOINT`).
  `examples/list.lang` proves `appNatAssoc` and `sumFromApp` by
  induction. The lowering erases the motive and gives the step the head
  and the tail (field 1 of the list node). `pairCount` folds a list that
  a `Nat` fold makes, so the Wasm differential runs that step.
- Form over a core type with several constructors: `fold` takes one function
  for each constructor that has fields and one value for each constant
  constructor. A function receives the folded results of the recursive
  fields (ledger-lang `SPEC.md` lines 141 to 151, the fold over `Value`).
- Uniqueness: `fold f z` is the only function `h` with `h nil = z` and
  `h (cons a l) = f a (h l)`. The same holds for each carrier: `fold` is the
  unique algebra map out of the carrier. Each fold terminates, because it
  consumes one constructor at each step.

### F7 Algebra unfold

- Form into `List E`: `unfold g n s : List E` when
  `g : S -> Option (Prod E S)`, `n : Nat` and `s : S`. `n` is the step
  limit.
- Laws: `unfold g 0 s = nil`. When `g s = none`,
  `unfold g (n + 1) s = nil`. When `g s = some (pair e r)`,
  `unfold g (n + 1) s = cons e (unfold g n r)`.
- Form into `Nat`: `g : S -> Option S`, and the result is the number of
  steps.
- Form into a core type with several constructors: `g` gives one leg of a
  sum for each constructor. A seed that is left after `n` steps becomes the
  constant constructor that the host column names.
- The step limit makes each unfold terminate. A host that has no measure for
  the step limit cannot realize F7. Its status is HOST-LIMIT.

### F8 Filterable filter

- Carriers: `Option`, `List`, and each sequence core type that the host
  column lists. `Sum E` is not a carrier.
- Form: `filter p t : F A` when `p : A -> Option Unit` (a test, section 1)
  and `t : F A`.
- Laws: `filter p none = none`. `filter p (some a) = some a` when `p a`
  keeps, and `none` when it does not. `filter p nil = nil`.
  `filter p (cons a l) = cons a (filter p l)` when `p a` keeps, and
  `filter p l` when it does not.
- Thus `filter` keeps exactly the elements whose test keeps them, in their
  order.

### F9 Pi, not dependent

- Formation: `(x : A) -> B : Type 0` when `A` and `B` are data types and `B`
  does not refer to `x`. A type parameter comes first:
  `(T : Type 0) -> (x : T) -> B` is in `Type 1`.
- Introduction: `fun (x : A) => t` when `t : B` with `x : A` in scope.
- Elimination: the application `f a` when `f : (x : A) -> B` and `a : A`.
  An application gives one data type for each type parameter first, then
  the values.
- Laws: `(fun (x : A) => t) a = t[x := a]`. A function is not an instance
  and has no encoding.
- Rule: `B` cannot refer to a value parameter. That form is F10.

### F10 Pi, dependent

- Formation: `(x : A) -> B` when `B` can refer to `x`.
- Introduction: `fun (x : A) => t` when `t : B` with `x : A` in scope.
- Elimination: `f a : B[x := a]` when `f : (x : A) -> B` and `a : A`.
- Laws: `(fun (x : A) => t) a = t[x := a]`.
- Example: escrow-lang `settle` takes `L : Aggregation F`, so the type of a
  later argument depends on the value `F`.

### F11 Sigma

- Formation: `Sigma A B` when `B` can refer to a value of `A`. In binder
  form: `(x : A) * B`.
- Introduction: `pack a b : Sigma A B` when `a : A` and `b : B[x := a]`.
- Elimination: `witness s : A` and `payload s : B[x := witness s]` when
  `s : Sigma A B`.
- Laws: `witness (pack a b) = a`. `payload (pack a b) = b`.
  `pack (witness s) (payload s) = s`.
- A host that can build a Sigma value but cannot project it has the status
  PARTIAL. A record family that a match reads can replace a projected Sigma.
  The host file names each replacement.

### F12 Equality: refl, symm, trans

- Formation: `Eq A x y` when `A : Type 0`, `x : A` and `y : A`.
- Introduction: `refl x : Eq A x x`.
- Elimination: `symm e : Eq A y x` when `e : Eq A x y`.
  `trans d e : Eq A x z` when `d : Eq A x y` and `e : Eq A y z`.
- Rules: `Eq A x y` has an inhabitant only when `x = y`. A proof has no
  runtime content. It erases from the target, and a definition of an `Eq`
  type is not an instance.

### F13 Equality: transport and cong

- Elimination: `transport P e u : P y` when `e : Eq A x y` and `u : P x`.
  `cong f e : Eq B (f x) (f y)` when `f : A -> B` and `e : Eq A x y`.
- Laws: `transport P (refl x) u = u`. `cong f (refl x) = refl (f x)`.

### F14 Universes

- `Type 0` classifies the data types and the function types of data types.
- `Type 1` classifies `Type 0`, the function types with a type parameter,
  the type functions (for example `Option`), and each core family whose
  index is a type.
- There is no cumulativity. A type in `Type 0` is not a type in `Type 1`.
  There is no `Type 2` in a program.
- An instance is a top-level definition whose type is a data type. A
  definition of a type, of a function or of a proof is not an instance.

### F15 Indexed family

- Formation: `D i : Type 0` when `i : I` and `I` is a core data type. The
  index is a value, not a type, so F15 uses F10.
- Introduction: each constructor fixes the index or takes it as an argument:
  `mkD i x : D i`.
- Elimination: a match on `d : D i` gives the fields. Each field type uses
  the same `i`.
- Laws: a match on `mkD i x` gives `x`.
- Examples: ledger-lang `Ref k` (indexed by `k : Kind`) and escrow-lang
  `Aggregation F` (indexed by `F : ChoiceRule`).

### C1 Interval

zkpolicy-lang adds the formers C1 to C7 of `docs/PLAN.md`. The template
does not have them. C1 to C5 come from zkml-lang as patches (PLAN item
O1), and `docs/PORTS.md` records each source commit. Slice P1 adds C1 and
C2: P1a ports zkml-lang 6ed605a, and P1b ports zkml-lang 573c983.

- Formation: `I` is a pretype. Its sort is `SSet`, not a universe, so `I`
  is not a type in `Type 0` or `Type 1`. `I` can be the domain of a Pi
  type: `(j : I) -> B`. The checker refuses `def X : Type 0 := I` with
  `TYPE_PRETYPE`.
- Introduction: `i0 : I` and `i1 : I`. `~ r : I`, `r /\ s : I` and
  `r \/ s : I` when `r : I` and `s : I`.
- Laws: the laws of a De Morgan algebra. `~ i0 = i1`. `~ ~ r = r`.
  `~ (r /\ s) = ~ r \/ ~ s`. `~ (r \/ s) = ~ r /\ ~ s`. `r /\ i1 = r`.
  `r \/ i1 = i1`. `/\` and `\/` are associative, commutative and
  idempotent. They absorb, and each distributes over the other. The law
  `r /\ ~ r = i0` does not hold: the interval is not a Boolean algebra.
- Rules: the checker compares two interval terms by a normal form. The
  normal form is a join of meets of literals. A literal is a variable,
  a neutral interval term (such as an application), or its negation.
  The normal form holds at most 16 meets of at most 8
  literals. A larger term stays unreduced.
- Faces: a face is an interval term in the face position of a system.
  `(r = 0)` is `~ r` and `(r = 1)` is `r`. The checker compares two faces
  by the normal form of the interval, with one more law: a meet that
  holds a literal and its negation is empty. Thus `(i = 0) /\ (i = 1)`
  is the empty face, but `i /\ ~ i` is not `i0` as an interval term. A
  face holds when its normal form is `i1`.
- Partial elements: `Partial phi A : SSet` when `phi : I` and `A` is a
  type. `Partial phi A` is a pretype, as `I` is. Conversion of two
  `Partial` types compares their face indices as faces.
- Systems: `[ f1 |-> u1 , ... , fn |-> un ]` is in `Partial phi A` when
  each face `fk` is in `I`, each branch `uk` is in `A` on its face, the join
  `f1 \/ ... \/ fn` equals `phi` as a face, and two branches are equal
  on each meet of their two faces. `[]` is in `Partial i0 A`. A system
  holds at most 16 cells. Else the checker fails with `TYPE_SYSTEM`.
  Branch type comparisons assume every enclosing face. Empty cells still
  undergo the ordinary checks, and names must resolve in every branch.
  Conversion compares systems on their shared faces, so cell order and
  subdivision of a cover do not change the partial element. Neutral face
  atoms remain unconstrained during conversion; these comparisons are
  conservative.
- Elimination: `fromPartial p : A` when `p : Partial phi A` and `phi`
  holds in the current face context. Else the checker fails with
  `TYPE_SYSTEM`. `fromPartial` of a
  system gives the branch of the first face that holds. A total system
  keeps a cell around its branch until elimination, including when the
  branch is neutral. Its printed normal form remains a system.
- Names: `I`, `i0`, `i1`, `PathP`, `Path`, `funExt`, `Partial` and
  `fromPartial` are core names. A declaration with one of these names
  fails with `REFUSE_NAME` (rule R4). `~`, `/\`, `\/`, `@`, `<i> t`,
  `(r = 0)`, `(r = 1)` and `[ f |-> u , ... ]` are syntax.

### C2 PathP and Path

- Formation: `PathP A a b : Type n` when `A : I -> Type n`, `a : A i0` and
  `b : A i1`. `Path A a b` is `PathP (fun (i : I) => A) a b`, a path over
  a constant line. The core has no separate `Path` operation. A loop is a
  `Path A x x`.
- Introduction: `<i> t : PathP A a b` when `t : A i` with `i : I` in
  scope, `t[i := i0] = a` and `t[i := i1] = b`. Wrong ends fail with
  `TYPE_ENDPOINT`. `<i> t` needs a declared `PathP` type. It does not
  infer a type.
- Elimination: `p @ r : A r` when `p : PathP A a b` and `r : I`. An
  argument that is not an interval term fails with `TYPE_MISMATCH`.
- Laws: `(<i> t) @ r = t[i := r]`. `p @ i0 = a` and `p @ i1 = b`, also
  when `p` is a variable: the value of `p @ r` keeps the ends of the type.
  Eta: `<i> p @ i = p`.
- Overloads of F12 and F13 for a path:
  - `refl : Path A a a` is `<i> a`.
  - `symm p : PathP (fun (i : I) => A (~ i)) b a` is `<i> p @ ~ i` when
    `p : PathP A a b`.
  - `cong f p : Path B (f a) (f b)` is `<i> f (p @ i)` when `f : A -> B`
    and `p : Path A a b`.
  - `funExt h : Path ((x : A) -> B) f g` is `<i> fun x => h x @ i` when
    `h : (x : A) -> Path B (f x) (g x)`. `funExt h` needs a declared type.
    Its pointwise argument is checked against that type, so an unannotated
    lambda such as `fun x => refl` is allowed.
  - `refl` and `cong` need a constant line. A line that depends on `i`
    fails with `TYPE_MISMATCH`. This is not a gap of `transp`: `<i> a`
    over a line that varies is ill typed, because a fixed `a` is not in
    `A i`. A dependent `cong` needs the checker to infer the line. That is
    a later item.
- Laws that hold by `refl` (`examples/paths.lang`): `symm (symm p) = p`.
  `cong (fun x => x) p = p`. `cong (fun x => g (f x)) p = cong g (cong f p)`.
  `cong (fun k => k x) (funExt h) = h x`.
- `transport P p u` and `trans p q` take a path as well as an `Eq` proof
  (C3).
- Rules: a definition of a `PathP` type is a proof, not an instance. It
  erases from the target. When an interval or path value must exist at
  run time (for example in a fold accumulator), the lowering refuses the
  program with `REFUSE_RUNTIME_PATH`.

### C3 Kan operations

- Formation: `transp A r u : A i1` when `A : I -> Type n` is a line,
  `r : I` and `u : A i0`. The line can be `fun i => T` without an
  annotation. A term that is not a line fails with `TYPE_MISMATCH`.
- Constancy: the line is constant where `r` holds in the ambient face
  context. The checker evaluates the line under each face of that
  intersection and compares `A i` with `A i0`. A line
  that varies there fails with `TYPE_LINE`. `r = i0` adds no condition.
- Laws: `transp A i1 u = u`. Along a constant line of `Nat`, `Flag`,
  `Unit` or a universe, and along a constant line of a data type,
  `transp A r u = u`. Along a line of types:
  - Pi: `transp (fun i => A i -> B) i0 f` is
    `fun x => f (transp (fun i => A (~ i)) i0 x)` (`piRule`).
  - Sigma and Prod: a pair moves field by field (`sigmaRule`).
  - List, Option and Sum: a constructor moves field by field
    (`listRule`). `nil` and `none` give `u`.
  - PathP: along the line `fun i => PathP (P i) (a i) (b i)`, `transp`
    gives `<j> comp (fun i => P i j) side (p @ j)` with the side
    `fun k => [ ~ j |-> a k , j |-> b k , r |-> p @ j ]`
    (`transpPath` in `examples/kan.lang`).
- Transport over a path: `transport P p u` with `p : Path A a b` is
  `transp (fun i => P (p @ i)) i0 u`. `examples/transp.lang` derives `J`
  and its computation rule from `transp`.
- No regularity at a neutral type: `transp (fun _ => A) i0 u` stays
  unreduced when `A` is a type variable. A constructor of a family whose
  parameters vary along the line stays unreduced too.
  A `refl` transported along a varying `Eq` line also stays unreduced:
  its endpoints need not remain definitionally equal at `i1`.
- Rules: `transp` is a builtin, so a definition cannot use the name
  (`REFUSE_NAME`, rule R4). At run time, a `transp` that stays unreduced
  is refused with `REFUSE_RUNTIME_TRANSP` (P5a). After P5b, `transp`
  along `ua` computes, so no entry of `test/ir/glue.lang` reaches the arm.
- `hcomp` formation: `hcomp side u0 : A` when `u0 : A` and `side` is
  `fun j => [ f1 |-> u1 , ... ]`, a system in `j`. `phi` is implicit: it
  is the join of the faces. The faces are in the outer context, so a face
  cannot name `j` (`NAME_UNKNOWN`). The checker examines the system as a
  `Partial phi A` under `j : I` (the cover and the overlaps). A side of
  another form fails with `TYPE_SYSTEM`.
- The carrier `A` must be in a universe. `I`, `Partial` and Pi pretypes
  fail with `TYPE_PRETYPE`. A side binder may shadow an outer name:
  faces still refer to the outer context, while branches use the binder.
- `hcomp` base condition: on each face, `side i0` must agree with `u0`.
  A cell that does not agree fails with `TYPE_SYSTEM`.
- `hcomp` laws (`examples/hcomp.lang`): where `phi` holds, `hcomp side u0`
  is `side i1` (`hcompTotal`). Else the type gives the rule:
  - Pi: under `fun x`, each cell `u` becomes `u x` and the base becomes
    `u0 x` (`hcompPi`).
  - Prod: a pair of the compositions of the `first` and the `second`
    parts (`hcompProd`).
  - Sigma: the first part moves by `hcomp`. The second part moves by
    `comp` along `B` over the filler of the first part (`sigmaHcomp` in
    `examples/kan.lang`).
  - PathP: `<i> hcomp` at `P i`, with the cells `~ i |-> a` and
    `i |-> b` added. Each cell `u` becomes `u @ i`, and the base becomes
    `u0 @ i` (`hcompPath`).
  - The kit data types: when `u0` is a constant with no fields (`3`,
    `flagYes`, `unit`, `nil`, `none`) and each cell is `u0`, the result
    is `u0` (`hcompConst`).
- `hcomp` limits: a constructor with fields (`cons`, `some`, `inl`,
  `inr`, a family constructor) stays unreduced. So does `refl` at an `Eq`
  type, thus at a Sigma type with an `Eq` second part only the first part
  moves. `hcomp` at `Type 0` is a Glue type (P5b, the Univalence bullet).
  `hcomp` at `Type n` with `n > 0` stays unreduced. A
  neutral type stays unreduced, and so does an empty `hcomp` at a neutral
  base. At run time, an `hcomp` that stays
  unreduced is refused with `REFUSE_RUNTIME_PATH`. `hcomp` is a builtin,
  so a definition cannot use the name (`REFUSE_NAME`, rule R4).
- `comp` formation: `comp A side u0 : A i1` when `A : I -> Type n` is a
  line, `u0 : A i0` and `side` is `fun j => [ f1 |-> u1 , ... ]`. Each
  branch has the type `A j`. The faces, `phi` and the base condition are
  as for `hcomp` (`TYPE_SYSTEM`).
- `fill` formation: `fill A side u0 r : A r` under the same conditions,
  with `r : I`. At `r = i0` it is `u0`, and at `r = i1` it is
  `comp A side u0`.
- A constant line gives the homogeneous forms. The checker makes this
  choice once, when it elaborates the term, so no reduction depends on a
  comparison of types:
  - `comp (fun _ => A) side u0` is `hcomp side u0` at `A`.
  - `fill (fun _ => A) side u0 r` is `hcomp` at `A` with the side
    `fun j => [ ~ r |-> u0 , f1 |-> u1 (r /\ j) , ... ]` and the base
    `u0` (the Cubical Agda `hfill`).
  - A varying line gives the CCHM forms: `transp` moves the base and each
    branch to `A i1`, then `hcomp` composes them.
- Reason for the homogeneous forms: the CCHM form has
  `transp (fun _ => A) i0 u0` in its base. This term stays unreduced at a
  type variable `A` (no regularity). The laws below are over a type
  variable, so they need the homogeneous forms.
- `trans p q : Path A a c` when `p : Path A a b` and `q : Path A b c`
  over a constant line. It is
  `<i> hcomp (fun j => [ ~ i |-> a , i |-> q @ j ]) (p @ i)`
  (`transThree`). The checker infers both paths. A second path that does
  not start at the end of the first, or that has another line, fails
  with `TYPE_MISMATCH`.
- Laws (`examples/kan.lang`), as paths over a type variable `A`: `rUnit`
  (`p` equals `trans p (rfl A b)`), `lUnit` (`p` equals
  `trans (rfl A a) p`) and `assoc` (`trans p (trans q r)` equals
  `trans (trans p q) r`, through `compFill` and `compFill2`). `rfl A b`
  is `<i> b` with a declared type.
- `comp`, `fill` and `trans` limits: `trans` infers its second argument,
  and `refl` does not infer a type, so `trans p refl` fails with
  `TYPE_INFER`. Use `rfl A b`. `trans` over a path on a varying line
  fails with `TYPE_MISMATCH`. A `comp` or a `fill` that stays unreduced
  is an `hcomp` that stays unreduced, so at run time it is refused with
  `REFUSE_RUNTIME_PATH`. `comp` and `fill` are builtins, so a definition
  cannot use the names (`REFUSE_NAME`, rule R4).

### C4 Higher inductive types

Slice P3a ports zkml-lang b57b121 (Z3a): the declarations, the path
constructor values and the formal `hcomp`. Slice P3b ports zkml-lang
228455f (Z3b): the eliminator.

- Declaration: only `domain/domain.lang` declares a higher inductive
  type (rule R1). A program that declares one fails with `REFUSE_DATA`.
  A path constructor has the form `ctor (fields) : Path X a b`
  (dimension 1) or `ctor (fields) : Path (Path X a b) p q` (dimension
  2), where `X` is the family at its own parameters. The ends `a` and
  `b` are point terms: a field of the family type, or an earlier point
  constructor applied to the parameters, with a point term or a field
  at each of its fields. The ends `p` and `q` of a square are fields of
  the constructor. The point constructors come first, and the first
  constructor is a point constructor. The fields of a path constructor
  form a telescope. A field that names the family names it at its own
  parameters (uniform recursion): `lcons (h : A) (t : L Nat)` in
  `L (A : Type 0)` fails with `DOMAIN_FAMILY`. A constructor type that
  is not such a `Path`, a dimension of 3 or more, an end that is not a
  point term, a path field in a point constructor or a point
  constructor after a path constructor fails with `DOMAIN_HIT`.
  `domain/domain.lang` declares `Circle` (`circleBase`, `circleLoop`),
  `Trunc A` (`truncIn`, `truncSquash`) and `Quot A R` (`quotIn`,
  `quotGlue`, `quotSquash`).
- Values: a path constructor at its parameters and fields is a path
  value of its declared type, and `p @ r` is a canonical value at the
  interval `r`. At an end it reduces by the declared type. Dimension 1:
  `i0` gives `a` and `i1` gives `b`. Dimension 2, at `r1` then `r2`:
  `r1 = i0` gives `p @ r2`, `r1 = i1` gives `q @ r2`, `r2 = i0` gives
  `a` and `r2 = i1` gives `b`. `examples/hit.lang` gives the accept
  cases: the ends of `circleLoop` by `refl`, the type of `quotGlue`
  (`quotPath`), `isPropTrunc` from `truncSquash` and `isSetQuot` from
  `quotSquash`.
- Formal `hcomp`: `hcomp side u0` at a higher inductive type is a
  canonical value (a formal constructor) when no face of the side
  holds. When a face holds it is that branch at `i1`, as at every type.
  The formal `hcomp` is distinct from its base: a path between them by
  `refl` fails with `TYPE_ENDPOINT`. `trans p q` at a higher inductive
  type is such an `hcomp` under a binder, so
  `trans circleLoop circleLoop` is a path at `Circle` whose `i0` end is
  `circleBase` (`loopTwice`).
- `transp` at a higher inductive type: along a constant line it gives
  `u` (`transpBase`). Along a line whose parameters vary it stays
  unreduced, also on a constructor and on the formal `hcomp`.
- Run time: a point constructor lowers as a constructor of a data type
  (a tagged block). A path constructor at an interval and the formal
  `hcomp` are refused with `REFUSE_RUNTIME_PATH`. An entry has `Nat`
  and `Flag` parameters and result (the kit), so a higher inductive
  value reaches run time inside an entry only. `pointCount` in
  `examples/hit.lang` carries a `Circle` point through a fold, and the
  gate checks `test/ir/hit-path.lang`, whose fold step is the formal
  `hcomp`.
- Eliminator: `fold` takes one case for each constructor, the path
  constructors included. The case of a path constructor has the type
  `Path B a* b*` (dimension 1) or `Path (Path B a* b*) p* q*`
  (dimension 2) over the fields of the constructor, where a recursive
  field has the type `B` and a path field has the type `Path B u* v*`.
  The star `t*` of a point term `t` keeps the fields as variables and
  replaces each earlier point constructor by its case. The result type
  `B` is needed (a hint or a declared type) and rides as a hidden
  argument of the fold. Reductions: on a point constructor the fold is
  a data fold; on a path constructor at `r` it is the case at the
  folded fields, at `r`; on the formal `hcomp` it is an `hcomp` at `B`
  whose side is the fold of the side. `isProp B` is the case type of
  `truncSquash` and `isSet B` is the case type of `quotSquash`; the
  checker does not check the h-level of `B`. At run time only the
  point constructors have an arm: a path value gives
  `REFUSE_RUNTIME_PATH`. `examples/hit.lang` gives `circleRec`,
  `truncRec`, `quotRec` and their laws by `refl`, and the P3b guest
  tests: `quotToTrunc` (set-quotient elimination into a set),
  `truncProp` (truncation elimination into a proposition) and
  `effective` (the effectiveness of the set quotient, with
  `isSet HProp` as a hypothesis). The rejects
  `test/check/hit-fold-path.lang`, `test/check/hit-fold-prop.lang` and
  `test/check/hit-fold-arity.lang` fail with `TYPE_MISMATCH`.
- Limits: `PathP` constructors, a dimension of 3 or more, an end
  through a path constructor and `transp` along a line whose
  parameters vary are not available. There is no dependent
  elimination: a `fold` has no motive, and its result type is a type,
  not a family over the higher inductive type.

### C5 Quotient inductive-inductive types

Slice P4a ports zkml-lang f43a6bf (Z4a): indexed sorts and the motive
fold. Slice P4b ports zkml-lang 7a74f7a (Z4b): mutual blocks and the
fold over a block.

- Declaration (part P4a, indexed sorts): a family whose header binders
  are its result indices, `family Circuit (a b : Shape)` in
  `domain/domain.lang` (the block `-- C5 (P4a)`, after the C4 block).
  Every point constructor states its result,
  `circId (idA : Shape) : Circuit idA idA`. The indices come from the
  fields (D3), so a constructor application takes the fields only. A
  field that names the family names it at index terms over the earlier
  fields, `(seqF : Circuit seqA seqB)`. The ends of a path constructor
  are point terms under the indices. A point constructor without a
  result, an index that mentions a recursive field and a plain field
  that depends on a recursive field fail with `DOMAIN_INDEX`. A result
  whose head is another family and an end that is not a point term fail
  with `DOMAIN_HIT`. The kit domain declares `Circuit` with the point
  constructors `circId` and `circSeq` and the path constructors
  `circIdl`, `circIdr`, `circAssoc` and `circSquash`.
- Declaration (part P4b, mutual blocks): `family Rose := ... and Kids :=
  ...` declares the sorts of a block together, in order. The headers of
  all the sorts bind before the check reads a constructor, so a
  constructor of one sort can use each sort of the block as a type
  (D9). A field at another sort of the block is a sibling field. The
  kit domain declares `Rose` with the point constructor `rose`, and
  `Kids` with `kidsNil`, `kidsCons` and the path constructors `kidsSwap`
  and `kidsSquash` (the block `-- C5 (P4b)` of `domain/domain.lang`,
  after the block `-- C5 (P4a)`). A duplicate sort name fails with
  `REFUSE_NAME`. A header type whose head is a sort of the block, a
  uniform sort with parameters in a block and a path over a sibling sort
  fail with `DOMAIN_QIIT`. A sort of the block at another place in a
  header type (`List Kids`) fails with `REFUSE_REC`. An end that names a
  constructor of a later sort fails with `NAME_UNKNOWN`.
- Universes: a sort's universe includes the universe of each sibling
  field. These levels are inferred across the block before final
  constructor checking, including chains and cycles. An independent sort
  keeps its own level. A later sort in `Type 1` cannot serve as an index
  that requires `Type 0`.
- The motive fold: `fold M case_1 .. case_n x` over an indexed family
  takes a motive `M` first, then one case for each constructor, then
  the value (the layout `[M, cases, x]`). `M` takes the indices and
  gives a type: `fun (a : Shape) => fun (b : Shape) => Circuit b a` is
  the motive of `circFlip`. A fold without a motive, or with a motive of
  another arity, fails with `TYPE_MISMATCH`. The type of `fold M cases x`
  is `M` at the indices of the type of `x`.
- The case shape (D6): a case takes the fields of its constructor only,
  not the indices. A recursive field arrives at `M` at the field's
  indices, and a path field `p : Path (Circuit a b) u v` arrives as a
  path at `M a b` whose ends go through the cases. The case of a point
  constructor gives `M` at the result indices. The case of a path
  constructor is a path at `M` at the indices of the base of its line,
  with the ends through the cases of the earlier point constructors. A
  case at the wrong indices fails with `TYPE_MISMATCH`.
- Reductions: on a point constructor the fold is the case at the folded
  fields. On a path constructor at `r` it is the case at the folded
  fields, at `r`, with the ends folded (D8). On the formal `hcomp` it is
  an `hcomp` at `M` at the indices of the base. `examples/qiit.lang`
  gives `circMap` (the identity algebra), `circFlip` (the reverse, its
  `circAssoc` case under `symm`), `squashIsSet` from `circSquash` and
  six laws by `refl` under `(d : List Nat)`: `circMapId`, `circMapSeq`,
  `circMapIdl`, `circFlipSeq`, `circFlipIdl` and `circFlipIdr`.
- The fold over a mutual block (D10): `fold M_1 .. M_n case_1 .. case_k
  x` takes one motive for each sort, in the order of the sorts, then one
  case for each constructor of the block, in the order of declaration,
  then the value. Its type is the motive of the sort of `x`, at the
  indices of `x`. A sibling field arrives at the motive of its sort. On
  a constructor the fold uses the case of that constructor and folds
  each sibling field at its own sort, with the same motives and cases.
  A fold with a wrong number of arguments fails with `TYPE_MISMATCH`.
  `examples/rose.lang` gives `roseMap` and `kidsMap` (the identity
  algebra), `kidsMap2` (its `kidsSwap` case under `symm`), `roseGrow`
  (a new leaf in front of the children of each node), `kidsIsSet` from
  `kidsSquash` and three laws by `refl`.
- Printing: a stuck fold over an indexed family prints its motive and
  its value. Nothing is hidden, unlike the result type `B` of a fold
  over a higher inductive type. The checker supplies omitted motive binder
  types from the index telescope, so stuck folds round trip, including
  motives with dependent indices or a result in `Type 1`.
  A motive may return a function. Its fold can take further arguments,
  and printed applications round trip. A simple fold returning a function
  can likewise take an indexed value as a further argument.
- Limits: a `Nat`-valued fold out of `Circuit` checks (part P7b-nat).
  `circCount` in `examples/nat.lang` gives `natAddAssoc` for the
  `circAssoc` case and `natIsSet` for the `circSquash` case (the section
  `Nat conversion and record eta`). A fold out of `Circuit` lands in
  `Nat`, in `Circuit` or in a set that the program proves. A fold over a
  tree of `Circuit` at run time fails with
  `REFUSE_RUNTIME_TREE` (`test/ir/qiit-tree.lang`). The entries `rank`,
  `rankPlus` and `seqCount` in `examples/qiit.lang` lower, because the
  circuit stays in the accumulator and not in the result.
  A `Nat`-valued fold out of the block of `Rose` and `Kids` also checks,
  with `natIsSet` for the `kidsSquash` case.
  `test/check/rose-count.lang` still fails with `TYPE_ENDPOINT`, because
  `refl` is not a proof of that case. A fold over a mutual block at run
  time fails with `REFUSE_RUNTIME_TREE` (`test/ir/rose-tree.lang`),
  because a sibling field is a recursive field (D11).
- Limits of the motive (the P4a probe, 2026-10-16): a fold takes a
  motive over an indexed family, over a mutual block (P4b), over `Nat`
  (P7b-nat: `fold M step base n`) and over `List A` (P7b-list:
  `fold M step base xs`). The last two are the motive forms of F6.
  `fold M z s l` over a single domain family that is not indexed
  (`Stack`) fails with `TYPE_MISMATCH` (`fold needs a last argument of
  type Nat, List A or a domain family ...`). Induction over `Stack` is a
  separate slice.

### C6 Glue types and univalence

- Formation: `Glue A [ f |-> pack T e , ... ] : Type 0` when `A : Type 0`
  and each branch is `pack T e` with `T : Type 0` and `e : Equiv T A`. The
  faces are in the outer context, and their join is `phi`. A branch of
  another shape fails with `GLUE_EQUIV`. Where `phi` holds,
  `Glue A sys = T`.
- Introduction: `glue [ f |-> t , ... ] a : Glue A sys` when `a : A`, the
  faces of `glue` join to `phi` (`GLUE_COVER` for another join), and on
  each face `t : T` and `witness e t = a` (`GLUE_BOUNDARY`). `glue` needs
  its type as a hint. Where `phi` holds, `glue sys a = t`.
- Elimination: `unglue g : A` when `g : Glue A sys`. A term of another
  type fails with `UNGLUE_TYPE`. Laws: `unglue (glue sys a) = a`, and
  where `phi` holds, `unglue g = witness e g`.
- Univalence: `ua A B e : Path (Type 0) A B` is in the domain prelude as
  the Glue line `<i> Glue B [ ~ i |-> pack A e , i |-> pack B (idEquiv B) ]`,
  with `idEquiv` the identity equivalence.
- Kan rules (P5b, CCHM 6.2): `transp` along a Glue line takes the base
  through `A`, extends the fiber of each branch whose face holds along
  the whole line (`face_forall`), and glues the result. A constant Glue
  line is the identity. `hcomp` at a Glue type composes each branch at
  `T` and the base at `A`, then glues. `hcomp` at `Type 0` with sides
  `[ φ |-> T ]` is `Glue A [ φ |-> pack (T i1) (transpEquiv T) ]`, with
  `transpEquiv` the transport equivalence of `domain/domain.lang`.
  `hcomp` at `Type n` with `n > 0` stays unreduced. So `uaβ` is a law:
  `transp` along `ua A B e` at `a` is `witness e a` (`uaBeta`,
  `uaBetaSwap` in `examples/glue.lang`). Glue eta is a rule (P5c): a
  neutral `g` of a Glue type converts with `glue [ f |-> g ] (unglue g)`
  (`glueEta` in `examples/glue.lang`). `OP_GLUE_ELEM` carries its Glue
  type, so conversion builds `unglue g` and compares bases using all
  conversion rules, including eta for products, Sigma types and functions.
- Glue Kan reduction restricts each face meet, independently of the
  order and subdivision of the value's system. A Glue transport stays
  unreduced if an opaque face application depends on its interval:
  `face_forall` must wait until that dependency reduces.
- Rules: `Glue`, `glue` and `unglue` are builtins, so a definition cannot
  use the names (`REFUSE_NAME`, rule R4). A system with a constant false
  face `i0` is checked as a plain conversion, as an `hcomp` system is. At
  run time, a Glue type, a `glue` or an `unglue` value is refused with
  `REFUSE_RUNTIME_GLUE`, and a `transp` that stays unreduced with
  `REFUSE_RUNTIME_TRANSP`. After P5b no entry of `test/ir/glue.lang` reaches
  the `transp` arm: the lowering of `stuck` succeeds. `test/ir/transp.lang`
  retains the refusal check through an indexed family.
- Tests: `examples/glue.lang`, `test/check/glue-boundary.lang`,
  `glue-cover.lang`, `glue-equiv.lang`, `glue-name.lang`,
  `unglue-type.lang` and `test/ir/glue.lang`. There is no check test for a
  branch whose function is not an equivalence: `isEquiv` is a proposition
  the checker cannot refute, so `glue-equiv.lang` uses a branch of the
  wrong shape.

### Nat conversion and record eta

Slice P7b-nat ports this section from zkml-lang af57c42 (Z5a), where it
is C6. Here C6 is Glue types and C7 is the rule of no proof irrelevance,
so this section has no C number.

- Neutral rules (part P7b-nat): when an argument of `natAdd` is not a
  number, the conversion uses three rules.
  R1: `natAdd a 0 => a`. R2: `natAdd 0 b => b`.
  R3: `natAdd a (natAdd b 1) => natAdd (natAdd a b) 1`.
  Two numbers compute to a number as before, so the lowering does not
  change. `examples/nat.lang` proves `natAddZeroR` (R1), `natAddZeroL`
  (R2) and `natAddSucc` (R3) by `refl`.
- The `natEq` rules (parts P7b-list and P7b-a): only during the check and
  the conversion, `natEq (natAdd x 1) (natAdd y 1)` gives `natEq x y`.
  Part P7b-a adds the mixed rule (ruling R10): `natEq 0 (natAdd y 1)`
  and `natEq (natAdd x 1) 0` give `flagNo`. The rules match only
  `natAdd` with the number 1, as the succ iota does. The domain prelude
  (`domain/domain.lang`) proves `natEqRefl` by induction with the first
  rule. It proves `natEqSound` by double induction with both rules: a
  path from `flagBool (natEq a b)` to `true` gives a path from `a` to
  `b`. `natEq (natAdd x 2) (natAdd x 1)` does not convert with
  `natEq x x` (`test/check/nateq-succ-only.lang`, `TYPE_ENDPOINT`). The
  mixed rule does not give `flagYes` (`test/check/nateq-mixed-yes.lang`,
  `TYPE_ENDPOINT`). The run-time evaluation and the lowering do not use
  the rules. Thus `natEq 0 (natAdd n 1)` traps when `n` is `2^64-1`
  (`examples/nat.lang`, `natEqZeroSucc` and `natEqSuccZero`).
- Induction: `natAddAssoc` holds only by induction, with the motive form
  of F6. The base case is `refl`. The step case is
  `cong (fun (z : Nat) => natAdd z 1) ih`. A proof by `refl` fails with
  `TYPE_ENDPOINT` (`test/check/natadd-assoc-refl.lang`).
  Only the check and the conversion use the succ iota on a symbolic
  value. The run-time evaluation and the printed functions keep the
  checked `natAdd x 1` scrutinee. Thus a successor of `2^64-1` traps,
  also when the step does not use its accumulator (`examples/nat.lang`,
  `natSuccForget`).
- `natMax a b : Nat` gives the larger number. The lowering writes it as
  `add(a, sub(b, a))`, where `sub(b, a)` is 0 when `b` is less than `a`.
  It adds no IR operation. `natMax` has no neutral rule.
- `natIsSet x y p q : Path (Path Nat x y) p q` when `x y : Nat` and
  `p q : Path Nat x y`. It is a trusted proof that `Nat` is a set. It
  has no reduction rule, so it is always stuck. It has no run-time
  form: a definition with a `Path` result is not an entry, and in an
  entry body `@ i0` and `@ i1` compute it away. A fold that carries a
  stuck `natIsSet` at run time fails with `REFUSE_RUNTIME_PATH`
  (`test/ir/natisset-path.lang`).
- Record eta (part P7b-nat): the conversion reads a value `x` as
  `c (p_1 x) .. (p_n x)` when its family has one point constructor `c`,
  no index and no path constructor. `examples/nat.lang` proves
  `itemEta`, `shapeEta` and `lotEta` by `refl`. A family with two point
  constructors (`test/check/eta-two-ctor.lang`) or with a path
  constructor (`test/check/eta-hit.lang`) fails with `TYPE_ENDPOINT`.
  A family with an index fails with `TYPE_ENDPOINT` (fronttool row
  `makeF (fval x)`, `test/fronttool.c:458`).
- Limits: `natMax n 0` does not convert with `n`
  (`test/check/natmax-neutral.lang`, `TYPE_ENDPOINT`). Thus
  `natMaxAssoc` cannot be proved (zkml-lang Z5 flag 12). The succ iota of
  the motive form applies only to `natAdd x 1`. `natAdd x L` with a
  number `L` greater than 1 does not unfold in the iota, and it does not
  convert with `natAdd (natAdd x (L - 1)) 1`.

## 4. Status words

| Word | Meaning |
|---|---|
| DONE | The host realizes the former and its laws. Tests or host checks cover it. |
| PARTIAL | The host realizes a restricted form. The cell names the restriction. |
| PLANNED | The host can realize the former, but the build does not have it yet. |
| HOST-LIMIT | The host cannot realize the former. The cell cites the probe fact. |

## 5. Realization matrix

Each cell gives the status and the source of the fact at the origin commit.
mech facts come from ledger-lang 997fa7b. assay facts come from escrow-lang
279214c. tcc-json facts come from `hosts/tcc-json` in this repository.
tcc-evm-contract facts come from `hosts/tcc-evm-contract` in this repository
at commit eaa4c74. The tcc-evm-contract front end is a copy of the tcc-wasm
front end. Its cells give the status in `langc check` and `langc eval`. The
EVM output of each former is PLANNED until slice K4
(`hosts/tcc-evm-contract/FORMERS.md:3-8`).
tcc-wasm and tcc-evm facts come from `hosts/tcc-wasm` and `hosts/tcc-evm` in
this repository. The two kits share the front end, so each former has the
same status in both kits.
tcc-evm-dao facts come from `hosts/tcc-evm-dao` in this repository at commit
eaa4c74. Its cells give the status in `langc check` and `langc eval`
(`hosts/tcc-evm-dao/FORMERS.md:5-6`).
tcc-evm-anchor facts come from `hosts/tcc-evm-anchor` in this repository at
commit 5bec924. Its cells give the status in the checker. No former runs on
the EVM: the runtime reads the outcomes from the table
(`hosts/tcc-evm-anchor/FORMERS.md:5-9`).
tcc-js facts come from `hosts/tcc-js` in this repository. Its front end is
the tcc-json front end with the `Str` primitive, so each former has the same
status as in tcc-json. The status also covers the JS module that
`langc build --js` writes (`hosts/tcc-js/FORMERS.md`).
tcc-media facts come from `hosts/tcc-media` in this repository. Its front
end adds media primitives to tcc-json and keeps the same type formers.
tcc-webgpu facts come from `hosts/tcc-webgpu` in this repository. Its front
end is the tcc-js front end with the `U32` and `Tile R C A` primitives, so
each former has the same status as in tcc-js. `U32` and `Tile R C A` are
host primitives, not formers (`hosts/tcc-webgpu/FORMERS.md`).
The host files cite the host kits in this repository. When a status
changes, update the host file and this matrix in the same change.

| ID | Former | mech (ledger-lang 997fa7b) | assay (escrow-lang 279214c) | tcc-json (this repository) | tcc-evm-contract (this repository) | tcc-wasm (this repository) | tcc-evm (this repository) | tcc-evm-dao (this repository) | tcc-evm-anchor (this repository) | tcc-js (this repository) | tcc-media (this repository) | tcc-webgpu (this repository) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| F1 | Product | DONE: `compiler/runtime.mech:125`, `compiler/checker.mech:1-44` | DONE: built-in `prod`, `tuple`, `t.0`, `t.1` (`SPEC.md:53`) | DONE: `hosts/tcc-json/examples/formers.lang` | DONE in `check` and `eval`: `hosts/tcc-evm-contract/examples/formers.lang:3`. PLANNED: EVM output (`hosts/tcc-evm-contract/src/main.c:155`) | DONE: `hosts/tcc-wasm/examples/formers.lang:3` | DONE: `hosts/tcc-evm/examples/formers.lang:3` | DONE: built-in product, tuples, `.0` and `.1`: `hosts/tcc-evm-dao/domain/domain.lang:167,175,177` | DONE: built-in product, tuples, `.0` and `.1`; `Flag` and `AnchorPair`: `hosts/tcc-evm-anchor/domain/domain.lang:14,213` | DONE: `hosts/tcc-js/examples/formers.lang` | DONE: `hosts/tcc-media/examples/formers.lang` | DONE: `hosts/tcc-webgpu/examples/formers.lang` |
| F2 | Coproduct | DONE: `compiler/types.mech:111`, `compiler/checker.mech:57,1634` | DONE: `Sum A B` over built-in `sum` (`prelude/Prelude.asy:108-123`) | DONE: `hosts/tcc-json/examples/formers.lang` | DONE in `check` and `eval`: `hosts/tcc-evm-contract/examples/formers.lang:8`. PLANNED: EVM output (`hosts/tcc-evm-contract/src/main.c:155`) | DONE: `hosts/tcc-wasm/examples/formers.lang:8` | DONE: `hosts/tcc-evm/examples/formers.lang:8` | DONE: built-in `sum`; `Sum`, `pureSum`, `mapSum`, `bindSum`: `hosts/tcc-evm-dao/domain/domain.lang:108-119` | DONE: built-in `sum`; `Flag`, `flagNo`, `flagYes`, `flagAnd`: `hosts/tcc-evm-anchor/domain/domain.lang:14-20` | DONE: `hosts/tcc-js/examples/formers.lang` | DONE: `hosts/tcc-media/examples/formers.lang` | DONE: `hosts/tcc-webgpu/examples/formers.lang` |
| F3 | Option | DONE: `compiler/types.mech:108` | DONE: `Option A := sum (prod (), A)` (`prelude/Prelude.asy:79-106`) | DONE: `hosts/tcc-json/examples/formers.lang` | DONE in `check` and `eval`: `hosts/tcc-evm-contract/examples/formers.lang:11`. PLANNED: EVM output (`hosts/tcc-evm-contract/src/main.c:155`) | DONE: `hosts/tcc-wasm/examples/formers.lang:11` | DONE: `hosts/tcc-evm/examples/formers.lang:11` | DONE: `Option A` is a definition over `sum`; `none`, `pureOption`, `mapOption`, `bindOption`: `hosts/tcc-evm-dao/domain/domain.lang:79-93` | DONE: `Option A` is a definition over `sum`; `optionNone`, `optionPure`: `hosts/tcc-evm-anchor/domain/domain.lang:27-32` | DONE: `hosts/tcc-js/examples/formers.lang` | DONE: `hosts/tcc-media/examples/formers.lang` | DONE: `hosts/tcc-webgpu/examples/formers.lang` |
| F4 | List | DONE: `compiler/types.mech:109` | PARTIAL: one `mu` family for each element type; probe P2: a constructor of a family with a parameter cannot appear in a term (O8, `SPEC.md:267-275`) | DONE: `hosts/tcc-json/examples/formers.lang` | DONE in `check` and `eval`: `hosts/tcc-evm-contract/examples/formers.lang:13`. PLANNED: EVM output (`hosts/tcc-evm-contract/src/main.c:155`) | DONE: `hosts/tcc-wasm/examples/formers.lang:13` | DONE: `hosts/tcc-evm/examples/formers.lang:13` | PARTIAL: no generic `List`; one `mu` for each element type, `Ballots` and `Claims` (`hosts/tcc-evm-dao/domain/domain.lang:129,303`); probe P2: a constructor of a family with a parameter cannot appear in a term (`hosts/tcc-evm-dao/docs/CAPABILITY.md:26-30`) | PARTIAL: no generic `List`; one `mu` for each element type, `Candidates` and `Profile` (`hosts/tcc-evm-anchor/domain/domain.lang:101,130`) | DONE: `hosts/tcc-js/examples/formers.lang` | DONE: `hosts/tcc-media/examples/formers.lang` | DONE: `hosts/tcc-webgpu/examples/formers.lang` |
| F5 | Monad | DONE over `Option`, `List`, `Sum E`: `compiler/checker.mech:60-65,591-745` | DONE for each carrier: `Option`, `Sum E`, each list family (`SPEC.md:67-70`) | DONE over `Option`, `Sum E`, `List`: `hosts/tcc-json/examples/monad.lang` | DONE in `check` and `eval` over `Option`, `Sum E`, `List`: `hosts/tcc-evm-contract/examples/monad.lang:2`. PLANNED: EVM output (`hosts/tcc-evm-contract/src/main.c:155`) | DONE over `Option`, `Sum E`, `List`: `hosts/tcc-wasm/examples/monad.lang:2` | DONE over `Option`, `Sum E`, `List`: `hosts/tcc-evm/examples/monad.lang:2` | DONE over `Option` and `Sum` (`hosts/tcc-evm-dao/domain/domain.lang:84-93,110-119`). PARTIAL over lists: one set for `Ballots` and one for `Claims` (`hosts/tcc-evm-dao/domain/domain.lang:143-149,317-323`) | PARTIAL: over `Option` only: `optionPure`, `optionBind`, `optionMap` (`hosts/tcc-evm-anchor/domain/domain.lang:32-41`); the list families have no bind | DONE over `Option`, `Sum E`, `List`: `hosts/tcc-js/examples/monad.lang` | DONE over `Option`, `Sum E`, `List`: `hosts/tcc-media/examples/monad.lang` | DONE over `Option`, `Sum E`, `List`: `hosts/tcc-webgpu/examples/monad.lang` |
| F6 | Algebra fold | DONE over `Nat`, `List A`, `Text`, `Values`, `Attrs` and `Value` (five functions): `compiler/checker.mech:802-1088,1661-1957` | PARTIAL: each list family has a structural `def rec`; no fold over `Nat`, because built-in `Nat` has no eliminator (`SPEC.md:71-73`) | DONE over `Nat`, `List` and each family: `hosts/tcc-json/examples/algebra.lang` | DONE in `check` and `eval` over `Nat`, `List` and each domain family: `hosts/tcc-evm-contract/examples/algebra.lang:4`. PLANNED: EVM output (`hosts/tcc-evm-contract/src/main.c:155`) | DONE over `Nat`, `List` and each domain family: `hosts/tcc-wasm/examples/algebra.lang:4`. HOST-LIMIT: a tree at run time (`hosts/tcc-wasm/test/ir/tree.lang:3`) | DONE over `Nat`, `List` and each domain family: `hosts/tcc-evm/examples/algebra.lang:4`. HOST-LIMIT: a tree at run time (`hosts/tcc-evm/test/ir/tree.lang:3`) | PARTIAL: `foldBallots` and `foldClaims` (structural `def rec`), generic in the result type only (`hosts/tcc-evm-dao/domain/domain.lang:133,307`); no fold over `Nat` (`hosts/tcc-evm-dao/docs/CAPABILITY.md:33-34`) | PARTIAL: `candidatesFold` and `profileFold` (structural `def rec`), in the prelude only (`hosts/tcc-evm-anchor/domain/domain.lang:105,134`); a `def rec` in a program is `REFUSE_REC`; no fold over `Nat` | DONE over `Nat`, `List` and each family: `hosts/tcc-js/examples/algebra.lang` | DONE over `Nat`, `List` and each family: `hosts/tcc-media/examples/algebra.lang` | DONE over `Nat`, `List` and each family: `hosts/tcc-webgpu/examples/algebra.lang` |
| F7 | Algebra unfold | DONE into the same carriers, with a `Nat` step limit: `compiler/checker.mech:802-1088,1661-1957` | HOST-LIMIT: no structural measure and no `Nat` fuel (O9 RULED, `SPEC.md:276-279`) | DONE into `List`, with a `Nat` step limit: `hosts/tcc-json/examples/algebra.lang` | DONE in `check` and `eval` into `List`, with a `Nat` step limit: `hosts/tcc-evm-contract/examples/algebra.lang:7`. PLANNED: EVM output (`hosts/tcc-evm-contract/src/main.c:155`) | DONE into `List`, with a `Nat` step limit: `hosts/tcc-wasm/examples/algebra.lang:7` | DONE into `List`, with a `Nat` step limit: `hosts/tcc-evm/examples/algebra.lang:7` | HOST-LIMIT: no `unfold`; built-in `Nat` has no eliminator, so there is no `Nat` fuel; `nu` is a refused form (`hosts/tcc-evm-dao/docs/CAPABILITY.md:33-35`, `hosts/tcc-evm-dao/test/refusal.sh:40`) | HOST-LIMIT: the checker accepts only structural recursion, and built-in `Nat` has no eliminator, so an unfold has no measure and no fuel (`hosts/tcc-evm-anchor/docs/CAPABILITY.md:47`) | DONE into `List`, with a `Nat` step limit: `hosts/tcc-js/examples/algebra.lang` | DONE into `List`, with a `Nat` step limit: `hosts/tcc-media/examples/algebra.lang` | DONE into `List`, with a `Nat` step limit: `hosts/tcc-webgpu/examples/algebra.lang` |
| F8 | Filterable filter | DONE over `Option`, `List`, `Text`, `Values`, `Attrs`; no `Sum E` instance: `compiler/checker.mech:622-631` | DONE over `Option` and each list family; the test is `A -> Option (prod ())` (`SPEC.md:74-76`) | DONE over `Option` and `List`: `hosts/tcc-json/examples/monad.lang` | DONE in `check` and `eval` over `Option` and `List`: `hosts/tcc-evm-contract/examples/monad.lang:9`. PLANNED: EVM output (`hosts/tcc-evm-contract/src/main.c:155`) | DONE over `Option` and `List`: `hosts/tcc-wasm/examples/monad.lang:9` | DONE over `Option` and `List`: `hosts/tcc-evm/examples/monad.lang:9` | DONE over `Option`: `filterOption` (`hosts/tcc-evm-dao/domain/domain.lang:99`). PARTIAL over lists: `filterBallots`, `filterClaims` (`hosts/tcc-evm-dao/domain/domain.lang:153,327`) | PARTIAL: over `Profile` only: `profileFilter` (`hosts/tcc-evm-anchor/domain/domain.lang:140`) | DONE over `Option` and `List`: `hosts/tcc-js/examples/monad.lang` | DONE over `Option` and `List`: `hosts/tcc-media/examples/monad.lang` | DONE over `Option` and `List`: `hosts/tcc-webgpu/examples/monad.lang` |
| F9 | Pi, not dependent | DONE: `compiler/program.mech:190` ("B cannot refer to a value parameter"), `SPEC.md:61-68` | DONE: built-in (`SPEC.md:55`) | DONE: `hosts/tcc-json/examples/functions.lang` | DONE in `check` and `eval`: `hosts/tcc-evm-contract/examples/functions.lang:2`. PLANNED: EVM output (`hosts/tcc-evm-contract/src/main.c:155`) | DONE: `hosts/tcc-wasm/examples/functions.lang:2`. HOST-LIMIT: a function value at run time (`hosts/tcc-wasm/test/ir/closure.lang:3`) | DONE: `hosts/tcc-evm/examples/functions.lang:2`. HOST-LIMIT: a function value at run time (`hosts/tcc-evm/test/ir/closure.lang:3`) | DONE: `A -> B`: `hosts/tcc-evm-dao/domain/domain.lang:279` | DONE: `A -> B`; `rule : Tally -> Outcome` in each program: `hosts/tcc-evm-anchor/examples/programs/arrow-debreu.lang:14` | DONE: `hosts/tcc-js/examples/functions.lang` | DONE: `hosts/tcc-media/examples/functions.lang` | DONE: `hosts/tcc-webgpu/examples/functions.lang` |
| F10 | Pi, dependent | PLANNED: `docs/STATUS.md:133-137` | DONE in the kernel: `Aggregation F` (`SPEC.md:83-86,103`) | DONE: `hosts/tcc-json/examples/functions.lang` | DONE in `check` and `eval` over `Type 0` and over `Nat`: `hosts/tcc-evm-contract/examples/functions.lang:8`. PLANNED: EVM output (`hosts/tcc-evm-contract/src/main.c:155`) | DONE over `Type 0` and over `Nat`: `hosts/tcc-wasm/examples/functions.lang:8` | DONE over `Type 0` and over `Nat`: `hosts/tcc-evm/examples/functions.lang:8` | DONE: `(x : A) -> B` and the erased binder `(0 x : A)`; `cast`: `hosts/tcc-evm-dao/domain/domain.lang:246` | DONE: `(x : A) -> B` and the erased binder `(0 x : A)`: `hosts/tcc-evm-anchor/domain/domain.lang:105,248` | DONE: `hosts/tcc-js/examples/functions.lang` | DONE: `hosts/tcc-media/examples/functions.lang` | DONE: `hosts/tcc-webgpu/examples/functions.lang` |
| F11 | Sigma | PLANNED: reserved names only (`compiler/types.mech:214`, `SPEC.md:39,183`) | PARTIAL: built, never projected; the kernel refuses Sigma eta and has no Sigma pattern, so records are `mu` families read by `match` (O10, `SPEC.md:280-288`) | DONE: `hosts/tcc-json/examples/sigma.lang` | DONE in `check` and `eval`: `hosts/tcc-evm-contract/examples/sigma.lang:3`. PLANNED: EVM output (`hosts/tcc-evm-contract/src/main.c:155`) | DONE: `hosts/tcc-wasm/examples/sigma.lang:3` | DONE: `hosts/tcc-evm/examples/sigma.lang:3` | PARTIAL: built, never projected; no Sigma pattern and no Sigma eta, so `Config` and `Tally` are `mu` records (`hosts/tcc-evm-dao/docs/CAPABILITY.md:36-40`, `hosts/tcc-evm-dao/domain/domain.lang:183,186,251`) | DONE: `AnchorDAO F := (L : Aggregation F) * AnchorLog`, read by `.0` and `.1`, built by a pair in `anchor` and `amendDAO`: `hosts/tcc-evm-anchor/domain/domain.lang:215-216,221,251` | DONE: `hosts/tcc-js/examples/sigma.lang` | DONE: `hosts/tcc-media/examples/sigma.lang` | DONE: `hosts/tcc-webgpu/examples/sigma.lang` |
| F12 | Eq refl, symm, trans | DONE: `compiler/checker.mech:67-122` | PARTIAL: one `Eq` family for each index type in `Type 0` (`EqNat`, `EqDec`, `EqTally`; `SPEC.md:57`) | DONE: `hosts/tcc-json/examples/equality.lang` | DONE in `check` and `eval`: `hosts/tcc-evm-contract/examples/equality.lang:4`. PLANNED: EVM output (`hosts/tcc-evm-contract/src/main.c:155`) | DONE: `hosts/tcc-wasm/examples/equality.lang:4` | DONE: `hosts/tcc-evm/examples/equality.lang:4` | PARTIAL: one family for each index type (`EqNat`, `EqDec`, `EqTally`), each with symm and trans; no generic `Eq` (`hosts/tcc-evm-dao/domain/domain.lang:26,48,201`) | PARTIAL: one family, `EqOutcome`, with refl, symm and trans; no generic `Eq` (`hosts/tcc-evm-anchor/domain/domain.lang:174,285,290`) | DONE: `hosts/tcc-js/examples/equality.lang` | DONE: `hosts/tcc-media/examples/equality.lang` | DONE: `hosts/tcc-webgpu/examples/equality.lang` |
| F13 | Eq transport, cong | PLANNED: reserved names (`compiler/types.mech:214`, `docs/STATUS.md:133-137`) | PARTIAL: one `transport` and one `cong` for each `Eq` family (`SPEC.md:57`) | DONE: `hosts/tcc-json/examples/equality.lang` | DONE in `check` and `eval`: `hosts/tcc-evm-contract/examples/equality.lang:8`. PLANNED: EVM output (`hosts/tcc-evm-contract/src/main.c:155`) | DONE: `hosts/tcc-wasm/examples/equality.lang:8` | DONE: `hosts/tcc-evm/examples/equality.lang:8` | PARTIAL: one `transport` and one `cong` for each `Eq` family (`hosts/tcc-evm-dao/domain/domain.lang:29,43,51,65,204,218`) | PARTIAL: one `transport` and one `cong`, for `EqOutcome` only (`hosts/tcc-evm-anchor/domain/domain.lang:280,294`) | DONE: `hosts/tcc-js/examples/equality.lang` | DONE: `hosts/tcc-media/examples/equality.lang` | DONE: `hosts/tcc-webgpu/examples/equality.lang` |
| F14 | Universes | DONE: `compiler/parser.mech:69-83` | PARTIAL: `Type 0`; `Type 1` only as the type of a type function (`SPEC.md:58`) | DONE: `hosts/tcc-json/examples/universes.lang` | DONE in `check` and `eval` for `Type 0` and `Type 1`, without cumulativity: `hosts/tcc-evm-contract/examples/universes.lang:4`. PLANNED: EVM output (`hosts/tcc-evm-contract/src/main.c:155`) | DONE for `Type 0` and `Type 1`, without cumulativity: `hosts/tcc-wasm/examples/universes.lang:4` | DONE for `Type 0` and `Type 1`, without cumulativity: `hosts/tcc-evm/examples/universes.lang:4` | PARTIAL: `Type 0` and `Type 1` only; no example program uses `Type 1` (`hosts/tcc-evm-dao/src/parser.c:304`, `hosts/tcc-evm-dao/test/refusal.sh:51`) | PARTIAL: `Type 0 : Type 1`; each formation takes the maximum universe; an explicit `Type 1` annotation has no type, `TYPE_UNIVERSE` (`hosts/tcc-evm-anchor/src/check.c:908`) | DONE: `hosts/tcc-js/examples/universes.lang` | DONE: `hosts/tcc-media/examples/universes.lang` | DONE: `hosts/tcc-webgpu/examples/universes.lang` |
| F15 | Indexed family | PARTIAL: domain only (`Ref k`, `compiler/parser.mech:186-190`); the generic former is PLANNED | DONE for a family with a fixed index type; no parameter (probe P2, `SPEC.md:60-62,103`) | DONE: a family with parameters in `domain/` (R1): `hosts/tcc-json/domain/domain.lang` | PARTIAL in `check` and `eval`: only the domain file declares an indexed family: `hosts/tcc-evm-contract/domain/domain.lang:8`. PLANNED: EVM output (`hosts/tcc-evm-contract/src/main.c:155`) | PARTIAL: only the domain file declares an indexed family: `hosts/tcc-wasm/domain/domain.lang:10` | PARTIAL: only the domain file declares an indexed family: `hosts/tcc-evm/domain/domain.lang:10` | PARTIAL, in `domain/` only: `EqNat` and `Aggregation` (indexed by `ChoiceRule`); a family with a parameter cannot appear in a term (`hosts/tcc-evm-dao/domain/domain.lang:26,235`, `hosts/tcc-evm-dao/docs/CAPABILITY.md:26`) | DONE: `EqOutcome` and `Aggregation` (indexed by `Constitution`): `hosts/tcc-evm-anchor/domain/domain.lang:174,182` | DONE: a family with parameters in `domain/` (R1): `hosts/tcc-js/domain/domain.lang` | DONE: a family with parameters in `domain/` (R1): `hosts/tcc-media/domain/domain.lang` | DONE: a family with parameters in `domain/` (R1): `hosts/tcc-webgpu/domain/domain.lang` |
