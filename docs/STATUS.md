# zkpolicy-lang status

The build implements slice P0 (the tcc-wasm kit with the formers F1 to
F15) and slice P1 of `docs/PLAN.md`: the interval C1 with faces, partial
elements and systems, and PathP and Path (C2). P1a is a port of zkml-lang
commit 6ed605a, and P1b is a port of zkml-lang commit 573c983. It also
implements slice P2 (C3): `transp`, `hcomp`, `comp` and `fill`, and
transport and `trans` over a path. P2a is a port of zkml-lang commit
e069119, P2b-1 is a port of zkml-lang commit 0741c38, and P2b-2 is a port
of zkml-lang commit 402c9fe. `docs/PORTS.md` records the
ports. The build also implements slice P6, the
policy domain core, before P1b (USER ruling 2026-10-15, option (b)).
The build also implements slice P7a, the `Ref` signature and two of its
laws. Slices P5a (C6 Glue types and `ua`) and P5b (the Kan rules for Glue
and the universe, `uaβ`) and P5c (Glue eta, `hPropExt`, the C7 check
tests) are DONE. Slice P3a (C4 declarations, path constructor values and
the formal `hcomp`, a port of zkml-lang b57b121) is DONE. Slices P3b, P4
and P7b to P12 are not started.

## Implemented

- F1 to F15 from the tcc-wasm kit. `formers/tcc-wasm.md` gives the status
  of each former.
- C1 interval: the pretype `I` (sort `SSet`), `i0`, `i1`, `~`, `/\` and
  `\/`. The checker compares interval terms by a De Morgan normal form.
- C2 PathP and Path: `PathP A a b`, `Path A a b` (PathP over a constant
  line), `<i> t`, `p @ r`, the endpoint rules and path eta.
- C1 faces, partial elements and systems: `(r = 0)`, `(r = 1)`,
  `Partial phi A`, `[ f |-> u , ... ]` and `fromPartial`. The checker
  checks each branch under its face, and examines the cover and the
  overlaps of each system. Conversion compares systems on shared faces.
  `examples/systems.lang` gives the accept cases.
- Path forms of `refl`, `symm` and `cong`, and `funExt`. The laws in
  `examples/paths.lang` hold by `refl`.
- C3 part P2a: `transp A r u` (the line, its constancy where `r` holds,
  and the reductions along Pi, Sigma, Prod and the kit data types), and
  `transport P p u` over a path. `examples/transp.lang` derives `J` and
  its computation rule.
- C3 part P2b-1: `hcomp side u0` (`phi` from the faces, the faces in the
  outer context, the base condition, and the reductions where `phi` holds
  and at Pi, Prod, PathP and the constants of the kit data types).
  `examples/hcomp.lang` gives the accept cases.
- C3 part P2b-2: `comp A side u0` and `fill A side u0 r` (the `hcomp`
  forms over a constant line, the CCHM forms over a varying line),
  `hcomp` at a Sigma type, `transp` along a line of `PathP` types, and
  `trans` over a path. `examples/kan.lang` gives the accept cases and the
  laws `rUnit`, `lUnit` and `assoc`.
- C4 part P3a: higher inductive types in `domain/domain.lang` (`Circle`,
  `Trunc A` and `Quot A R`; path constructors of dimension 1 and 2 with
  point-term ends and uniform recursion), path constructor values with
  their end rules, the formal `hcomp` at a higher inductive type, and
  `transp` along a constant line. A program cannot declare one
  (`REFUSE_DATA`), and `fold` over one fails with `TYPE_FOLD_HIT` until
  slice P3b. `examples/hit.lang` gives the accept cases.
- The refusal `REFUSE_RUNTIME_PATH` in the lowering.
- P6 policy core in `domain/domain.lang`: `Visibility`, `Bool`
  (`Sum Unit Unit`), `Ctx`, `PolicySig`, `PolicyEquiv`, `isContr`,
  `fiber`, `isEquiv`, `Equiv`, one statement for each law, `PolicyLaws`,
  `PolicyModel`, one accessor for each field, and the lemmas `pequivRefl`,
  `pequivSymm`, `pequivTrans` and `privacySamePub`. `pequivTrans` uses
  `trans` over a path (C3). `examples/policy.lang` uses each of them over
  an abstract model.
- P7a `Ref` signature in `domain/domain.lang`: the family `Layer`,
  `refSig`, and the proofs `refSoundness` and `refPrivacy`.
  `examples/ref.lang` computes `refSig` on closed contexts and uses each
  proved law.
  `Decision` retains the resulting `Nat` state so that composition and
  proving can respect `PolicyEquiv` when P7b takes the quotient.
  Addition layers saturate at the host Nat bound; guest proofs check
  that their generated certificates still verify at that bound.

## Remaining work

1. Slice P3b: the C4 eliminator (zkml-lang Z3b, commit 228455f), the law
   `soundness-pub` and the effectiveness of the set quotient. Slice P4:
   C5 (QIITs). It needs zkml-lang Z4 first.
2. Slice P5: C6. P5a (Glue types, `glue`, `unglue` and `ua`) and P5b (the
   Kan rules for Glue and the universe, `uaβ`) are DONE 2026-10-15. P5c
   (Glue eta, `hPropExt`, the C7 check tests) is DONE 2026-10-16. The
   effectiveness of the set quotient waits on P3b (the set quotient is a
   HIT; P3a gives its declaration, P3b its eliminator).
3. Slices P7b to P12 as `docs/PLAN.md` gives them. P7b needs P3b (the C4
   eliminators for induction on `List` and `Nat`, and the set quotient for
   `sipPolicy`) and P5 (`hPropExt`). P9 and P10 need
   zkml-lang Z5, Z6 and Z9.
4. Moved from P6: the law `soundness-pub` to P3b.

## Known limits

- The interval normal form holds at most 16 meets of at most 8 literals.
  A larger term stays unreduced, so the checker can fail to equate two
  large terms that are equal. Cause: fixed-size arrays in
  `src/front/eval.c`.
- `<i> t` and `funExt h` do not infer a type. They need a declared
  `PathP` type. Cause: the checker cannot find the line from the body.
- `refl` and `cong` on a path need a constant line. Cause: `<i> a` over
  a line that varies is ill typed, because a fixed `a` is not in `A i`.
  A dependent `cong` needs the checker to infer the line (a later item).
- `trans` over a path needs a constant line. A path over a varying line
  fails with `TYPE_MISMATCH`. Cause: the path form of `trans` is an
  `hcomp`, and `hcomp` composes in one type.
- `trans p refl` fails with `TYPE_INFER`. Cause: `trans` infers its
  second argument, and `refl` does not infer a type. Use `<i> b` with a
  declared type (`rfl` in `examples/kan.lang`).
- `transp` stays unreduced along a family whose parameters vary on the
  line, and along a constant line of a neutral type. Cause: the core has
  no regularity at a neutral type, and no rule for a family constructor
  with varying parameters.
- `transp` of `refl` along a varying `Eq` line stays unreduced. Cause:
  the endpoint indices need not remain definitionally equal at `i1`.
- A system holds at most 16 cells. Cause: fixed-size arrays in
  `src/front/check.c`.
- `PolicyLaws` has no `soundness-pub`. Cause: the propositional
  truncation is a HIT. P3a declares `Trunc`; the eliminator is slice P3b.
- `fold` over a higher inductive type is refused with `TYPE_FOLD_HIT`.
  Cause: the eliminator is slice P3b. `test/check/hit-fold.lang` gives
  the reject case.
- A name cannot contain `-`. The laws and lemmas have camelCase names.
- An interval, path or system value cannot exist at run time
  (`REFUSE_RUNTIME_PATH`). Cause: the IR has no interval, no path and no
  system. `fromPartial` of a total system reduces before the lowering.
  Elimination of a neutral partial element stays unreduced.
- A `transp` that stays unreduced at run time is refused with
  `REFUSE_RUNTIME_TRANSP` (P5a). The arm stays; after P5b no entry of
  `test/ir/glue.lang` reaches it. `test/ir/transp.lang` still exercises
  the refusal through an indexed family. A Glue type, a `glue` or an `unglue`
  value at run time is refused with `REFUSE_RUNTIME_GLUE`.
- `transp` and `hcomp` at a Glue type reduce (P5b, CCHM 6.2), and `transp`
  along `ua` computes: `uaβ` is a law (`examples/glue.lang`). Two
  `transpEquiv` proof terms convert through the aligned-system fast path of
  `conv_system`; the pairwise restricted comparison costs exponential work
  in the `hcomp` nesting depth. The arena is not reset between definitions,
  so a file with many such conversions can reach the 1 GiB limit.
- Glue transport stays unreduced when a face contains an opaque
  application that depends on the transport interval. Its universal face
  cannot be determined until the application reduces.
- A Glue or `glue` system with a constant false face `i0` is checked as a
  plain conversion, as an `hcomp` system is. Use an interval parameter.
- No check test shows `GLUE_EQUIV` for a function that is not an
  equivalence: `isEquiv` is a proposition the checker cannot refute.
  `test/check/glue-equiv.lang` uses a branch of the wrong shape.
- `hPropExt` (P5c) holds at `Type 0`, as `ua` does.
- `hcomp` needs a side of the form `fun j => [ ... ]`. A neutral side
  fails with `TYPE_SYSTEM`. Cause: the checker reads `phi` from the faces
  of the system.
- `hcomp` at a constructor with fields (`cons`, `some`, `inl`, `inr` and
  a family constructor) or at `refl` stays unreduced. So at a Sigma type
  with an `Eq` second part, only the first part moves. Cause: no rule
  moves `hcomp` into the fields or along the `Eq` indices yet.
- `hcomp` at `Type 0` is the Glue type with `transpEquiv` branches (P5b).
  `hcomp` at `Type n` with `n > 0` stays unreduced.
- An empty `hcomp` at a neutral base stays unreduced, as in CCHM.
- An `hcomp`, `comp` or `fill` that stays unreduced at run time gives
  `REFUSE_RUNTIME_PATH`. Cause: the IR has no system.
- The kit limits stay. `docs/host/CAPABILITY.md` gives them.

## Internal boundaries

`src/front/lexer.c` and `src/front/parser.c` read the path syntax. `<i> t`,
`p @ r`, `~`, `/\` and `\/` become applications of builtin heads that no
identifier can spell, so the parser adds no new term kind. A system
`[ f |-> u , ... ]` becomes nested cells over the heads `|->` and `[]`,
and `(r = 0)` and `(r = 1)` become `~ r` and `r`.
`test/parse/expect.txt` gives the parse refusals.

`src/front/check.c` holds the builtin rows for C1, C2 and C3, the pretype
mode, the endpoint checks, the path forms of `refl`, `symm`, `cong`
and `funExt`, the cover and overlap checks of a system, and the line and
constancy checks of `transp`, the side and base checks of
`hcomp`, and the constructor checks of a higher inductive type (the
shape `Path X a b` or `Path (Path X a b) p q`, the point-term ends,
uniform recursion and the field telescope of a path constructor).
`test/check/expect.txt` gives the refusal code of each reject case
(`TYPE_ENDPOINT`, `TYPE_PRETYPE`, `TYPE_MISMATCH`, `TYPE_SYSTEM`,
`TYPE_LINE`, `TYPE_FOLD_HIT`).

`src/front/eval.c` holds the interval normal form, the face normal form,
the reduction of systems and `fromPartial`, the reduction of
`p @ r` (the value keeps the ends of the type) and path eta in the
conversion check, the reductions of `transp` and `hcomp`, the end
rules of a path constructor at an interval and the formal `hcomp` at a
higher inductive type. Neutral interval terms participate in the same algebra
as interval variables. The printer emits source syntax for interval and
path normal forms, and for a path constructor at an interval.
`test/fronttool.c` checks these round trips, neutral
interval laws and type-directed checking of the argument to `funExt`.
It also checks system normal forms, elimination with neutral branches,
conversion of the face indices of `Partial` types, branch restrictions,
and conversion after reordering or subdividing system cells.

`src/front/lower.c` refuses interval, path and system values at run time,
and with them a path constructor at an interval and the formal `hcomp`
at a higher inductive type. A point constructor of a higher inductive
type lowers as a data constructor.
A `transp`, an `hcomp` or a Glue value that stays unreduced is refused.
A program cannot use `I`, `PathP` or `Path` values in an entry. The gate
checks `test/ir/path.lang`, `test/ir/glue.lang` and
`test/ir/hit-path.lang`.

`domain/domain.lang` holds the sample domain and the P6 policy core.
`gen/embed.c` copies the file into `langc`, so each guest file can use the
policy core. The core adds no Wasm entry. `src/front/check.c` no longer
reserves `Bool`, because the domain defines it.
