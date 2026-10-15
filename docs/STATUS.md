# zkpolicy-lang status

The build implements slice P0 (the tcc-wasm kit with the formers F1 to
F15) and slice P1 of `docs/PLAN.md`: the interval C1 with faces, partial
elements and systems, and PathP and Path (C2). P1a is a port of zkml-lang
commit 6ed605a, and P1b is a port of zkml-lang commit 573c983.
`docs/PORTS.md` records them. The build also implements slice P6, the
policy domain core, before P1b (USER ruling 2026-10-15, option (b)).
The build also implements slice P7a, the `Ref` signature and two of its
laws. Slices P2 to P5 and P7b to P12 are not started.

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
- The refusal `REFUSE_RUNTIME_PATH` in the lowering.
- P6 policy core in `domain/domain.lang`: `Visibility`, `Bool`
  (`Sum Unit Unit`), `Ctx`, `PolicySig`, `PolicyEquiv`, `isContr`,
  `fiber`, `isEquiv`, `Equiv`, one statement for each law, `PolicyLaws`,
  `PolicyModel`, one accessor for each field, and the lemmas `pequivRefl`,
  `pequivSymm` and `privacySamePub`. `examples/policy.lang` uses each of
  them over an abstract model.
- P7a `Ref` signature in `domain/domain.lang`: the family `Layer`,
  `refSig`, and the proofs `refSoundness` and `refPrivacy`.
  `examples/ref.lang` computes `refSig` on closed contexts and uses each
  proved law.
  `Decision` retains the resulting `Nat` state so that composition and
  proving can respect `PolicyEquiv` when P7b takes the quotient.
  Addition layers saturate at the host Nat bound; guest proofs check
  that their generated certificates still verify at that bound.

## Remaining work

1. Slices P2 to P4: C3 (`transp` and `hcomp`), C4 (HITs) and C5 (QIITs).
   They need zkml-lang Z2, Z3 and Z4 first. After P2, `trans` and
   `transport` take a path.
2. Slice P5: C6 Glue types and `ua`. It needs C3 (P2) first.
3. Slices P7b to P12 as `docs/PLAN.md` gives them. P7b needs P3 (the C4
   eliminators for induction on `List` and `Nat`, and the set quotient for
   `sipPolicy`) and P5 (`hPropExt`). P9 and P10 need
   zkml-lang Z5, Z6 and Z9.
4. Moved from P6: the law `soundness-pub` to P3, and the lemma
   `pequivTrans` to P2.

## Known limits

- The interval normal form holds at most 16 meets of at most 8 literals.
  A larger term stays unreduced, so the checker can fail to equate two
  large terms that are equal. Cause: fixed-size arrays in
  `src/front/eval.c`.
- `<i> t` and `funExt h` do not infer a type. They need a declared
  `PathP` type. Cause: the checker cannot find the line from the body.
- `refl` and `cong` on a path need a constant line. Cause: a dependent
  line needs `transp` (slice P2).
- `trans` and `transport` take an `Eq` proof only. Cause: path
  composition needs `hcomp` (slice P2).
- A system holds at most 16 cells. Cause: fixed-size arrays in
  `src/front/check.c`.
- `PolicyEquiv` has no transitivity lemma. Cause: path composition needs
  `hcomp` (slice P2).
- `PolicyLaws` has no `soundness-pub`. Cause: the propositional
  truncation is a HIT (slice P3).
- A name cannot contain `-`. The laws and lemmas have camelCase names.
- An interval, path or system value cannot exist at run time
  (`REFUSE_RUNTIME_PATH`). Cause: the IR has no interval, no path and no
  system. `fromPartial` of a total system reduces before the lowering.
  Elimination of a neutral partial element stays unreduced.
- The kit limits stay. `docs/host/CAPABILITY.md` gives them.

## Internal boundaries

`src/front/lexer.c` and `src/front/parser.c` read the path syntax. `<i> t`,
`p @ r`, `~`, `/\` and `\/` become applications of builtin heads that no
identifier can spell, so the parser adds no new term kind. A system
`[ f |-> u , ... ]` becomes nested cells over the heads `|->` and `[]`,
and `(r = 0)` and `(r = 1)` become `~ r` and `r`.
`test/parse/expect.txt` gives the parse refusals.

`src/front/check.c` holds the builtin rows for C1 and C2, the pretype
mode, the endpoint checks, the path forms of `refl`, `symm`, `cong`
and `funExt`, and the cover and overlap checks of a system.
`test/check/expect.txt` gives the refusal code of each reject case
(`TYPE_ENDPOINT`, `TYPE_PRETYPE`, `TYPE_MISMATCH`, `TYPE_SYSTEM`).

`src/front/eval.c` holds the interval normal form, the face normal form,
the reduction of systems and `fromPartial`, the reduction of
`p @ r` (the value keeps the ends of the type) and path eta in the
conversion check. Neutral interval terms participate in the same algebra
as interval variables. The printer emits source syntax for interval and
path normal forms. `test/fronttool.c` checks these round trips, neutral
interval laws and type-directed checking of the argument to `funExt`.
It also checks system normal forms, elimination with neutral branches,
conversion of the face indices of `Partial` types, branch restrictions,
and conversion after reordering or subdividing system cells.

`src/front/lower.c` refuses interval, path and system values at run time. A
program cannot use `I`, `PathP` or `Path` values in an entry. The gate
checks `test/ir/path.lang`.

`domain/domain.lang` holds the sample domain and the P6 policy core.
`gen/embed.c` copies the file into `langc`, so each guest file can use the
policy core. The core adds no Wasm entry. `src/front/check.c` no longer
reserves `Bool`, because the domain defines it.
