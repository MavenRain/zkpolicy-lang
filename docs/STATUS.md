# zkpolicy-lang status

The build implements slice P0 (the tcc-wasm kit with the formers F1 to
F15) and slice P1a of `docs/PLAN.md`: the interval C1 without faces, and
PathP and Path (C2). P1a is a port of zkml-lang commit 6ed605a.
`docs/PORTS.md` records it. The build does not complete slice P1. Faces,
partial elements and systems come in slice P1b, a port of the zkml-lang
Z1b commits. Slices P2 to P12 are not started.

## Implemented

- F1 to F15 from the tcc-wasm kit. `formers/tcc-wasm.md` gives the status
  of each former.
- C1 interval: the pretype `I` (sort `SSet`), `i0`, `i1`, `~`, `/\` and
  `\/`. The checker compares interval terms by a De Morgan normal form.
- C2 PathP and Path: `PathP A a b`, `Path A a b` (PathP over a constant
  line), `<i> t`, `p @ r`, the endpoint rules and path eta.
- Path forms of `refl`, `symm` and `cong`, and `funExt`. The laws in
  `examples/paths.lang` hold by `refl`.
- The refusal `REFUSE_RUNTIME_PATH` in the lowering.

## Remaining work

1. Slice P1b: faces as interval terms, partial elements and systems. It
   needs the zkml-lang Z1b commits first.
2. Slices P2 to P4: C3 (`transp` and `hcomp`), C4 (HITs) and C5 (QIITs).
   They need zkml-lang Z2, Z3 and Z4 first. After P2, `trans` and
   `transport` take a path.
3. Slice P5: C6 Glue types and `ua`. It needs C3 (P2) first.
4. Slices P6 to P12 as `docs/PLAN.md` gives them. P6 needs C4 (P3) for
   the truncation in `soundness-pub`. P9 and P10 need zkml-lang Z5, Z6
   and Z9.

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
- An interval or path value cannot exist at run time
  (`REFUSE_RUNTIME_PATH`). Cause: the IR has no interval and no path.
- The kit limits stay. `docs/host/CAPABILITY.md` gives them.

## Internal boundaries

`src/front/lexer.c` and `src/front/parser.c` read the path syntax. `<i> t`,
`p @ r`, `~`, `/\` and `\/` become applications of builtin heads that no
identifier can spell, so the parser adds no new term kind.
`test/parse/expect.txt` gives the parse refusals.

`src/front/check.c` holds the builtin rows for C1 and C2, the pretype
mode, the endpoint checks and the path forms of `refl`, `symm`, `cong`
and `funExt`. `test/check/expect.txt` gives the refusal code of each
reject case (`TYPE_ENDPOINT`, `TYPE_PRETYPE`, `TYPE_MISMATCH`).

`src/front/eval.c` holds the interval normal form, the reduction of
`p @ r` (the value keeps the ends of the type) and path eta in the
conversion check. Neutral interval terms participate in the same algebra
as interval variables. The printer emits source syntax for interval and
path normal forms. `test/fronttool.c` checks these round trips, neutral
interval laws and type-directed checking of the argument to `funExt`.

`src/front/lower.c` refuses interval and path values at run time. A
program cannot use `I`, `PathP` or `Path` values in an entry. The gate
checks `test/ir/path.lang`.
