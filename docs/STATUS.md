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
the formal `hcomp`, a port of zkml-lang b57b121) is DONE. Slice P3b (the C4
eliminator, a port of zkml-lang 228455f, the law `soundness-pub` and the
effectiveness of the set quotient) is DONE. Slice P4a (C5 indexed sorts
and the motive fold, a port of zkml-lang f43a6bf) is DONE. Slice P4b (C5
mutual blocks and the fold over a block, a port of zkml-lang 7a74f7a) is
DONE. Part P7b-nat of slice P7b (`Nat` induction, the `natAdd` neutral
rules, `natMax`, `natIsSet` and record eta, a port of zkml-lang
af57c42) is DONE. Part P7b-list (`List` induction, native) is DONE. Part
P7b-a (the laws on `refSig` by induction) is DONE. Part P7b-b (the R2
rule and the quotient sig `refQSig` with the 7 laws) is DONE. Slice P8
(the Wasm edge entries for `Ref`) is DONE. Part P9a (the
`ZkmlCircuit` block and the `langc zkml` writer) is DONE. Part P9b s1
to s6c (the dense, residual, residual-loop, lookup, softmax and layer
normalization circuits, `NAMEWit` and `NAMESat` in every mode, at
zkml-lang 27aef04) is DONE. Its attention circuit is deferred.
Slice P11 (the governance instance, N7) is DONE.
Part P11b (block induction and `govModel`) is DONE. Slice P12 (the
final docs) is DONE 2026-10-20. Slice P10 is not started (it needs
zkml-lang Z9).

Gate coverage: `make check` has 26 examples, 13 parse rows, 81 check
rows, 150 evals, 26 IR files, 29 Wasm modules, 5380 differential cases
with 0 mismatches, 52 zkml rows and 36 C namespace regressions.
`make zk-check` gives 63 rows and 3 writer-zw rows. `docs/VALIDATION.md`
gives the commands and the tool versions.

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
  (`REFUSE_DATA`). `examples/hit.lang` gives the accept cases.
- C4 part P3b: the eliminator. `fold` over a higher inductive type takes
  one case for each constructor, the path constructors included, and
  reduces on a point constructor, on a path constructor at an interval
  and on the formal `hcomp`. `examples/hit.lang` gives `circleRec`,
  `truncRec` and `quotRec` with their laws by `refl`, set-quotient
  elimination into a set (`quotToTrunc`), truncation elimination into a
  proposition (`truncProp`) and the effectiveness of the set quotient
  (`effective`, `effectiveRefl`). `effective` takes `isSet HProp` as a
  hypothesis (`isSetHProp`).
- C5 part P4a: indexed sorts in `domain/domain.lang` (`Circuit (a b : Shape)`
  with the point constructors `circId` and `circSeq` and the path
  constructors `circIdl`, `circIdr`, `circAssoc` and `circSquash`), the
  `fold` eliminator with a motive over the indices, and cases over the
  fields only. `examples/qiit.lang` gives the accept cases and the laws
  of `circMap` and `circFlip` by `refl`.
- C5 part P4b: mutual blocks (`family Rose := ... and Kids := ...` in
  `domain/domain.lang`, the block `-- C5 (P4b)`) and the fold over a
  block, with one motive for each sort. `examples/rose.lang` gives the
  accept cases and the laws of `roseMap`, `kidsMap` and `kidsMap2` by
  `refl`. A fold over a block at run time fails with
  `REFUSE_RUNTIME_TREE`. P4 is done.
- The law `soundness-pub` (`SoundnessPub`, P3b) in `PolicyLaws`. Its
  conclusion is in the propositional truncation `Trunc`, so the C4
  families come before the laws in `domain/domain.lang` (rule R3).
  `examples/policy.lang` uses it (`extracted`).
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
- P7b-nat: the motive form of the `Nat` fold (induction), the `natAdd`
  neutral rules R1 to R3, `natMax`, `natIsSet` and record eta
  (`formers/FORMERS.md` F6 and the section `Nat conversion and record
  eta`). `examples/nat.lang` proves `natAddAssoc` by induction and
  gives a `Nat`-valued fold out of `Circuit` (`circCount`).
- P7b-list: the motive form of the `List` fold (induction) and the
  `natEq` rule (`formers/FORMERS.md` F6 and the section `Nat conversion
  and record eta`). `examples/list.lang` proves `appNatAssoc` and
  `sumFromApp` by induction.
- P7b-a: the laws `ComposeAssoc`, `EvalCompose`, `Completeness` and
  `SoundnessPub` on `refSig` in `domain/domain.lang`, by induction on
  `List` and `Nat`. Thus `refSig` has 6 of the 7 laws. `SipPolicy`
  needs the quotient sig of P7b-b. The domain prelude also gives
  `natEqRefl` (from `examples/nat.lang`), `natEqSound`, `boolCode`,
  `falseTrue` and `evalComposeL`. The mixed `natEq` rule (ruling R10)
  gives `flagNo` for `natEq` of 0 and a successor during the check
  (`formers/FORMERS.md`, the section `Nat conversion and record eta`).
  `examples/ref.lang` uses each new law at closed instances.
- P7b-b: the R2 rule (`fold M hM c_1 .. c_P x`, a fold over `Circle`,
  `Trunc` or `Quot` with a motive into props; `formers/FORMERS.md`, the
  section `Motive fold over a HIT`) and the quotient sig `refQSig` in
  `domain/domain.lang`. Its policies are `QPolicy`, a `Quot`. `refQSig`
  has the 7 laws `qComposeAssoc`, `qEvalCompose`, `qSoundness`,
  `qSoundnessPub`, `qPrivacy`, `qCompleteness` and `qSip`, so `refModel`
  is a `PolicyModel`. `refSig` stays (ruling R8). The tests are the
  `examples/hit.lang` tail, the eval `hit quotPropIota`, seven
  `test/check/propfold-*.lang` rejects and `test/ir/hit-proof.lang`.
  The port of R2 back to zkml-lang (ruling R9) is open.
- P8: the Wasm edge entries for `Ref` in `examples/edge.lang`. The
  policy `edgePolicy` is closed, so the compiler fixes it at compile
  time. Each sig has 4 entries (ruling R3): the transcript and the
  certificate (the two halves of `prove`), the verification and the
  round trip. The entries on `refQSig` use the policy at its `Quot` point
  (`qin`). The entries on `refSig` are a cross check (ruling R2).
  `edgeSameRun` shows that the two sigs give the same run.
  `edgeRoundYes` and `edgeRefRoundYes` give the round-trip law (ruling
  R6): an honest proof verifies at each `x` and `y`. The tests are 24
  eval rows (`edge` in `test/eval/expect.txt`) and the refusal row
  `test/ir/runtime-policy.lang`. Two limits apply. The lowering refuses
  a policy with a length that is known only at run time with
  `REFUSE_RUNTIME_CLOSURE`, because `refRun` folds the list into a
  function. An entry parameter or result is `Nat` or `Flag` only. Thus
  `prove` gives two entries, and the verification maps its `Bool` to a
  `Flag` (`boolFlag`). The gate counts are 24 examples, 95 evals, 27
  Wasm modules and 3908 differential cases with 0 mismatches.
- P9a (`SmallModel` and `ZkmlCircuit` at zkml-lang 7d0bb2d) is DONE. Part
  s1 adds the prims `natDiv` and `natMod` (a trap on a divisor of 0), the
  `ZkmlCircuit` block and the BabyBear operations at the end of the
  domain, and `examples/smallmodel.lang`. Part s2 moves `listUncons` and
  `listZipWith` into the domain and adds the writer `langc zkml PROG NAME
  -o OUT` (`src/zkml.c`, the contract in `docs/host/CAPABILITY.md`), the
  zkml-lang pin in `PIN` and `make zk-check`. The writer output for `smW`
  is the golden file `test/zk/smallmodel-smW.lang`. `make zk-check` builds
  zkml-lang at the `PIN` commit; zkml-lang checks the output and agrees
  with the eval of this kit on 13 rows. The circuit is the identity in
  P9a. The gate counts are 25 examples,
  117 evals, 28 Wasm modules, 4738 differential cases with 0 mismatches
  and 7 zkml rows. Part s3 adds 16 mutants on `src/zkml.c`. The golden
  output and the C regressions kill 12. The rows in `test/zk/writer.lang`
  kill 3: M5a and M5b (17 columns) and M8a (a blank line in a copy). M6
  (no `Quot` substring test) is equivalent: the walk follows the family
  `Quot`, and its constructor `quotGlue` names `quotIn`.
- P9b s1 (the dense circuit at zkml-lang 27aef04) is DONE. `PIN` and
  `src/zkml-names.h` move to 27aef04, with 63 new domain names. The writer
  generates `NAMES` and `NAMET`, the input shape [n] and output shape [m].
  `NAMERd` gives the model as matrix 0 in the weights. `NAMECirc` is the
  dense layer `circOp NAMES NAMET (opMatMul n m 0)` for m rows and n
  columns. `NAMERunE` runs the circuit on `NAMERd NAME`. `NAMEWitE` uses
  `instEval` of `bbInst` to evaluate a trace of two stages: the input and
  the `bbRun` output. `make zk-check` has 15 rows. The `smW` RunE and WitE
  rows must give the zp eval of `smDenseE` (WANT form `zp:smDenseE`). Two
  rows use the arguments `2013265920 0`. It also checks
  `test/zk/writer-zw.lang` at the PIN (R9). The rows in
  `test/zk/writer-names.lang` refuse models `zt` and `zr` because generated
  names `ztT` and `zrRd` collide. Thus s1 has 9 zkml rows.
  P9b s2 adds `langc zkml PROG NAME --residual -o OUT`: `NAMECirc` is
  `circResidual` of the dense layer, and the writer refuses a model
  with m != n (`ZKML_MODEL`). The golden file is
  `test/zk/smallmodel-smW-residual.lang`. 8 zk rows (`smW@residual`)
  must give the zp eval of `smResE`. Thus `make check` has 12 zkml rows
  and `make zk-check` has 23 rows. For the residual circuit, the open
  Sat proof by refl does not check, but the closed proofs at `2 3` and
  `2013265920 0` check. Thus the `NAMESat` proof stays open.
  Mutants M1, M2, M3, M4, M6 are KILLED.
  The gate counts are 25 examples, 13 parse rows, 78 check rows, 117 evals,
  25 IR files, 28 Wasm modules and 4738 differential cases with 0
  mismatches. The writer has 12 rows and 21 C namespace regressions.
  P9b s3 adds `langc zkml PROG NAME --residual-loop=K -o OUT`: `NAMECirc`
  is `circResidualLoop` of the dense layer with K steps, K from 1 to 16,
  and the writer refuses m != n (`ZKML_MODEL`) and a bad K (`USAGE`, exit
  2). The host gets `smResLoop` (n residual layers by Nat induction) and
  `smRes2E`, with 4 eval rows. The golden file is
  `test/zk/smallmodel-smW-loop2.lang`. 8 zk rows (`smW@residual-loop=2`)
  must give the zp eval of `smRes2E`, and 2 rows evaluate the copied
  `smRes2E` at the PIN. Thus `make check` has 18 zkml rows and `make
  zk-check` has 33 rows. The closed Sat proofs at `2 3` and `2013265920 0`
  check for the loop circuit; the open Sat proof by refl does not, so the
  `NAMESat` proof stays open. Mutants M1 to M8 are KILLED.
  The gate counts are 25 examples, 13 parse rows, 78 check rows, 121 evals,
  25 IR files, 28 Wasm modules and 4838 differential cases with 0
  mismatches. The writer has 18 rows and 21 C namespace regressions.
  P9b s4 adds `langc zkml PROG NAME --lookup=TABLE -o OUT`: `NAMECirc`
  is `circSeq` of the dense layer and `opLookup NAMET 1`, the table
  lookup on each entry, and `NAMERd` puts `TABLE` at index 1 of the
  weights. The writer resolves `TABLE` as it resolves `NAME`, refuses a
  table whose rows do not have two entries (`ZKML_MODEL`), and refuses
  `--lookup` with no name (`USAGE`, exit 2). The host gets `smSq`,
  `smTableRow`, `smLookup`, `smAct` and `smActE`, with 4 eval rows. The
  golden file is `test/zk/smallmodel-smW-lookup.lang`. 8 zk rows
  (`smW@lookup=smSq`) must give the zp eval of `smActE`, and 2 rows
  evaluate the copied `smActE` at the PIN. Thus `make check` has 26 zkml
  rows and `make zk-check` has 43 rows. The closed Sat proofs at `2 3`
  and `2013265920 0` check for the lookup circuit. The open Sat proof by
  refl also checks for this circuit (it did not for the residual loop).
  The generated `NAMESat` proof stays deferred. Mutants M1 to M4 and M6
  to M8 are KILLED by the first gate run. M5 (the table type check)
  survived that run, because the row read also refuses `smDot` with the
  `ZKML_MODEL` code. The `--lookup=smDot` row now compares the full
  message, and the re-run KILLS M5.
  The gate counts are 25 examples, 13 parse rows, 78 check rows, 125 evals,
  25 IR files, 28 Wasm modules and 4938 differential cases with 0
  mismatches. The writer has 26 rows and 23 C namespace regressions.
  The PIN gate checks 43 eval rows and 3 writer-zw rows with 0 failures.
  The `smW` golden row kills M1 (matrix index 1).
  The `zw` golden row kills M2 (the swapped operator dimensions).
  The `zw` golden row kills M3 (the output shape [n]).
  The `smW` golden row kills M4 (the witness with only the input stage).
  The `smW` golden row kills M5 (the RunE call with nil weights).
  The `zt` refusal row kills M6 (the missing T suffix).
  The `zr` refusal row kills M7 (the missing Rd suffix).
  P9b s5 adds `NAMEWit` and `NAMESat`. `NAMEWit` is the witness of two
  stages for an open input list, in every mode. `NAMEWitE` evaluates
  `instEval` of `bbInst` on `NAMEWit`, with the same values as before. In
  the dense and lookup modes the writer generates `NAMESat`, the open
  proof that `NAMEWit x` satisfies `NAMECirc` at every input `x`, by
  `refl` at the PIN. In the residual modes it does not, and a comment
  line says why: at the PIN, `refl` does not prove the open path for
  `circResidual` (the `zipWithT bbAdd` fold is stuck on an open input).
  The suffixes `Wit` and `Sat` join the generated-name guard. The rows in
  `test/zk/writer-names.lang` refuse `zs` and `zv` because the generated
  names `zsSat` and `zvWit` collide. All 5 golden files change. Thus
  `make check` has 28 zkml rows. The PIN type check of each writer output
  in `make zk-check` (43 rows) is the proof gate of `NAMESat`: no new zk
  row is needed. Mutants M1 to M6 are KILLED.
  The residual golden row and the `smW@residual` zk row kill M1
  (`NAMESat` also in the residual mode: the PIN refuses the open refl).
  The `smW` golden row kills M2 (`NAMEWit` with the input stage only).
  The `zs` refusal row kills M3 (the missing Sat suffix).
  The `zv` refusal row kills M4 (the missing Wit suffix).
  The `smW` golden row kills M5 (`NAMESat` with the output shape `NAMES`).
  The `smW` golden row kills M6 (the old inline trace in `NAMEWitE`).
  The gate counts are 25 examples, 13 parse rows, 78 check rows, 125 evals,
  25 IR files, 28 Wasm modules and 4938 differential cases with 0
  mismatches. The writer has 28 rows and 31 C namespace regressions.
  The witness input binder avoids capturing a model named `x`. The C
  runner checks models named `x` and `x_` in all four writer modes,
  including the PIN type check of each output in `make zk-check`.
  The PIN gate checks 43 eval rows and 3 writer-zw rows with 0 failures.
  P9b s6a generates `NAMESat` in the residual and residual-loop modes
  too. There the proof is the inverse of the domain lemma `stageDecEnc`
  at `NAMET`:
  `<i> stageDecEnc Nat 0 NAMET (bbRun NAMES NAMET NAMECirc (NAMERd NAME) x) @ ~ i`.
  The dense and lookup modes keep the proof by `refl`. The two residual
  golden outputs change from 31 to 32 lines. There is no PIN move. The
  PIN type check of each writer output in `make zk-check` is the proof
  gate of `NAMESat` in all four modes, so s6a adds no zk row. The input
  binder is `x_` for a model named `x`, and the dimension binder is `i_`
  for a model named `i`. Without the `i_` binder, a probe model `i`
  fails with `TYPE_MISMATCH` at the PIN. Mutants M1 to M5 are KILLED.
  The two residual golden rows kill M1 (`NAMESat` by `refl` in the
  residual modes). The C runner also refuses M1: the models `x` and
  `x_` fail with `TYPE_ENDPOINT` at the PIN.
  The two residual golden rows kill M2 (no `NAMESat` in the residual
  modes).
  The two residual golden rows and the C runner kill M3 (the path at
  `i` and not at `~ i`).
  The loop2 golden row kills M4 (no `NAMESat` in the residual-loop mode
  only).
  The two residual golden rows kill M5 (`NAMES` for `NAMET`). The PIN
  type check accepts M5, because `NAMES` and `NAMET` are equal for
  `smW`.
  The PIN gate checks 43 eval rows and 3 writer-zw rows with 0 failures.
  P9b s6b adds `langc zkml PROG NAME --softmax=TABLE -o OUT`: `NAMECirc`
  is the dense layer, then the softmax with the exp table `TABLE`:
  `circSeq NAMES NAMET NAMET (circOp NAMES NAMET (opMatMul n m 0)) (circOp NAMET NAMET (opSoftmax NAMET 1 3))`.
  The scale K is fixed at 3 (`ZKML_SOFTMAX_K`). At the PIN, the check of
  the `refl` proof `NAMESat` stops at the arena limit for K from 4 to
  10, and the `stageDecEnc` form stops at every K. The writer reads the
  table with the same check as `--lookup`. The two table messages now
  say `the table` and not `the lookup table`. The host gets the exp
  table `smExp` (2^8 exp(-d) for d from 0 to 6), `smSoftRow`, `smSoftE`
  and `smSoftScale`. The 4 old golden outputs get the 11 new host
  definitions, because `smSoftE` is a model entry. The new golden file
  is `test/zk/smallmodel-smW-softmax.lang`. Thus `make check` has 36
  zkml rows (6 golden outputs and 30 refusal rows), 146 evals, 5280
  differential cases (`smSoftE` adds 100 cases and `smSoftScale` adds
  222 cases, including 6 arithmetic boundary cases) and 33 C namespace
  regressions (the models `x` and `x_` in the softmax mode).
  `make zk-check` has 53 rows: 8 rows
  `smW@softmax=smExp` and 2 rows on the copied `smSoftE`. There is no
  PIN move. Mutants M1 to M5 are KILLED.
  The `smW@softmax=smExp` zk value rows and the softmax golden row kill
  M1 (`opSoftmax` reads slot 0, not the table at slot 1).
  The zk rows kill M2 (`ZKML_SOFTMAX_K` 4): the PIN check of
  `smWSat` stops at the arena limit. The softmax golden row also kills
  M2.
  The zk value rows kill M3 (the rows read the table `smSq`, not
  `smExp`). The golden rows do not change.
  The `--softmax` refusal rows and the C namespace cases kill M4 (no
  table check in the softmax mode).
  The zk value rows and the golden rows kill M5 (the host scale K 4
  in `smSoft`).
  The PIN gate checks 53 eval rows and 3 writer-zw rows with 0 failures.
  P9b s6c adds `langc zkml PROG NAME --layernorm=GB,TABLE -o OUT`:
  `NAMECirc` is the dense layer, then the layer normalization with the
  gamma-beta matrix `GB` and the inverse square root table `TABLE`:
  `circSeq NAMES NAMET NAMET (circOp NAMES NAMET (opMatMul n m 0)) (circOp NAMET NAMET (opLayerNorm NAMET 1 2))`.
  `NAMERd` is `cons w (cons GB (cons TABLE nil))`: `GB` is at slot 1
  and `TABLE` is at slot 2. `GB` and `TABLE` must have different
  names. `NAMESat` is `refl`. The writer checks `GB` first: the type
  `List (List Nat)` and two rows (gamma, beta) of m entries. Then it
  checks `TABLE` with the same check as `--lookup`. In the lookup,
  softmax and layer normalization modes, a table or gamma-beta entry at
  or above bbP = 2013265921 gives `ZKML_MODEL`, because the entry can
  overflow `natMul` in the host Wasm. The host gets 13 definitions,
  with the gamma-beta matrix `smGB`, the inverse square root table
  `smIsq` (26 rows), `smLn` and `smLnE`. `smLnE` is in the bounded-loop
  exception of `test/grid.awk` (100 grid cases). Its Wasm function has
  1241 locals. The 5 old golden outputs get the 13 new host
  definitions. The new golden file is `test/zk/smallmodel-smW-layernorm.lang`.
  Thus `make check` has 52 zkml rows (7 golden outputs and 45 refusal
  rows), 150 evals, 5380 differential cases and 36 C namespace
  regressions. `make zk-check` has 63 rows: 8 rows
  `smW@layernorm=smGB,smIsq` and 2 rows on the copied `smLnE`. There
  is no PIN move. Mutants M1 to M6 are KILLED.
  The layernorm golden row and the zk rows kill M1 (`opLayerNorm
  NAMET 2 1`: the circuit reads `TABLE` as `GB`).
  The layernorm golden row and the zk rows kill M2 (`NAMERd` puts
  `TABLE` at slot 1 and `GB` at slot 2).
  The `--layernorm` refusal rows kill M3 (no gamma-beta shape check).
  The bbP refusal rows (`zgp` and `ztp`) kill M4 (no bbP check).
  The zk value rows kill M5 (the rows read the table `smSq`, not
  `smIsq`).
  The `smLnE` eval rows and the 6 smallmodel golden rows kill M6 (a
  host `smIsq` of 2 rows: each golden output copies `smIsq`). The
  `zp:smLnE` rows do not kill M6, because both sides read `smIsq`.
  The PIN gate checks 63 eval rows and 3 writer-zw rows with 0 failures.
- P11 (the governance instance, N7) is one mutual block (C5) in
  `domain/domain.lang` after `refModel`: `GPolicy` (a list of layers),
  `GLayer` (a constant layer, a seal or a nested governance context) and
  `Gov` (a governance context, a public `GPolicy` and a private `Nat`).
  The indexed form `Gov (p : GPolicy)` refuses with `DOMAIN_QIIT` (K1), so
  the instance is a mutual block and not an inductive-inductive
  definition. `eval` is one block fold (O7) with the motives `Nat -> Nat`,
  `Nat -> Nat` and `Nat`. It descends through `gNest` and `gov`, so the
  recursion is on the nesting of the context. `gSig : PolicySig` packs
  `gField`, `gEval`, `gProve`, `gCompose` (append), `gRestrict` (a seal in
  front) and `gVerify`. Evaluation runs the policy before the public
  context, leaving an intermediate private witness for `EvalCompose`.
  Append preserves policies inside nested contexts by rebuilding them
  with `gpNil` instead of the second policy. `GQ` is the quotient of
  `GPolicy` by `PolicyEquiv gSig`, and `gqEval` is `quotRec` at `gEval`. Five laws
  check without induction over the block (ruling R8): `gSoundness` by
  `refl`, `gPrivacy` by congruence along the public-policy path,
  `gCompleteness` by `natEqRefl`, `gSoundnessPub` by
  `truncIn` and `natEqSound`, and `gqSip` on `GQ` through `GQEquiv` (the
  body of `PolicyEquiv` at `gqEval`), because the quotient sig needs
  `gqCompose` (finding F1, ruling R9). `examples/governance.lang` is the
  N7 test: the governance policy `gp4` decides on the policy `p4` that
  holds the context `gov [gConst 10] 4`. Its 5 eval rows are `q3e` 7,
  `q4e` 20, `gov4 3` 20, `q5e` 20 and `q5c` 21, and the example gets ir,
  Wasm and differential rows. Three review eval rows check nested append
  (`nestedCompose` 1), a public seal (`sealedEval` 0) and its verifier
  (`sealedVerify` 1). Two checked proofs preserve the nested policy shape
  and give a private witness for the public-seal composition case. Both
  proofs fail on the original P11 implementation. Two refusal rows:
  `test/ir/governance-tree.lang` refuses a bare run-time block fold with
  `REFUSE_RUNTIME_TREE`, and `test/ir/governance-closure.lang` refuses a
  run-time block fold with a function motive with
  `REFUSE_RUNTIME_CLOSURE`.
  The gate counts are 26 examples, 13 parse rows, 78 check rows, 133 evals,
  26 IR files, 29 Wasm modules and 4955 differential cases with 0
  mismatches. The writer has 28 rows and 31 C namespace regressions.
  Mutants M1 to M6 are KILLED.
  The `q4e` eval row kills M1 (the `gNest` case drops the nested decision).
  The domain load kills M2 (the `gSeal` case keeps the value): `gPrivacy`
  fails to type check.
  The `q3e` eval row kills M3 (the `gov` case ignores the private part).
  The `q5c` eval row kills M4 (the `gCompose` base case drops the second
  policy).
  The domain load kills M5 (`gRestrict` is the identity): `gPrivacy` fails
  to type check.
  The domain load kills M6 (`gVerify` compares `t` with `t`):
  `gSoundnessPub` fails with `TYPE_MISMATCH`.
- P11b (block induction, N7) adds a fold over a mutual block with
  value-dependent motives (`src/front/check.c`: `fold_block`,
  `check_motive_dep`, `case_type`). The case of a constructor takes its
  fields, then one induction hypothesis for each field whose type is a
  sort of the block. Its type is the motive at the constructor value.
  Iota (`src/front/eval.c`: `fold_family`) gives the fields, then the
  folds at the block-sort fields. A block with a path constructor
  refuses with `TYPE_MISMATCH`, and so does a fold that mixes an
  index-only motive with a value-dependent motive or has an indexed sort.
  The fold is a proof, so the lowering refuses it with
  `REFUSE_RUNTIME_PROOF`. The domain adds
  `gComposeAssoc`, `gEvalCompose`, `gqCompose`, the quotient sig
  `gQSig`, the six quotient laws, `gqSip : SipPolicy gQSig` and
  `govModel : PolicyModel`. Rulings R1 to R8 are the defaults. New rows:
  three check refusals (`test/check/block-ind-hit.lang` and
  `test/check/block-ind-mixed.lang` with `TYPE_MISMATCH`,
  `test/check/block-ind-noih.lang` with `TYPE_INFER`), the IR file
  `test/ir/block-ind-runtime.lang` (it checks, `runtimeCount` refuses
  with `REFUSE_RUNTIME_PROOF`, `gCount2` gives 2 and `gCountNest` gives
  10), and the eval rows `q7n` 20, `q7m` 20 and `gqComp4` 36. The gate
  counts are 26 examples, 13 parse rows, 81 check rows, 136 evals, 26 IR
  files, 29 Wasm modules and 4958 differential cases with 0 mismatches.
  Mutants K1 to K3 (kernel) and D1 to D3 (domain) are KILLED.

## Remaining work

1. Slice P5: C6. P5a (Glue types, `glue`, `unglue` and `ua`) and P5b (the
   Kan rules for Glue and the universe, `uaβ`) are DONE 2026-10-15. P5c
   (Glue eta, `hPropExt`, the C7 check tests) is DONE 2026-10-16. P3b
   proves the effectiveness of the set quotient with `isSet HProp` as a
   hypothesis. The gap is univalence as an equivalence: the domain has
   `ua` only.
2. The rest of P9b, and slice P10, as `docs/PLAN.md` gives them. P9b
   still needs the
   attention circuit (`opAttention`, zkml-lang 677478b, past the PIN).
   P10 needs zkml-lang Z9.
3. The port of the P7b-b ruling R2 back to zkml-lang (P7b-b ruling R9)
   is open. Induction on `Stack` is a separate slice.

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
  with varying parameters. This includes a constructor and the formal
  `hcomp` of a higher inductive type along `Trunc (L @ i)`.
- `transp` of `refl` along a varying `Eq` line stays unreduced. Cause:
  the endpoint indices need not remain definitionally equal at `i1`.
- A system holds at most 16 cells. Cause: fixed-size arrays in
  `src/front/check.c`.
- `fold` over a higher inductive type takes a motive only into props
  (part P7b-b, ruling R2), and only over `Circle`, `Trunc` and `Quot`.
  Over an indexed family, a mutual block or a family with a recursive
  field, `fold M hM ...` fails with `TYPE_MISMATCH` (ruling R12). A fold
  into a type that is not a prop has no motive. Cause: the C4
  eliminator of zkml-lang Z3b is not dependent.
- `fold` takes a motive only over `Nat`, `List A`, an indexed family or
  a mutual block. Over a single family that is not indexed (`Stack`),
  `fold M ...` fails with `TYPE_MISMATCH`. Cause: `rule_fold` reads
  motives only for `Nat`, for `List A`, for a family with indices or for
  a block. Induction over `Stack` is a separate slice.
  `formers/FORMERS.md` C5 gives the probe.
- A partial `List` motive fold (fewer than four arguments) does not get
  the motive form. Cause: the check selects the `List` form from the
  type of the fourth argument (P7b-list ruling R9).
- A motive with a wrong binder type (`fun (l : List Flag) => ...` over a
  `List Nat`, or `fun (k : Flag) => ...` over a `Nat`) does not select
  the motive form. The error comes from the fold over a family:
  `TYPE_INFER` for a step without binder types, `TYPE_MISMATCH` for a
  step with binder types (`test/check/listfold-motive-elem.lang`).
  Cause: a motive that does not check is not a motive, so the check
  tries the next form.
- `natMax n 0` does not convert with `n`, so `natMaxAssoc` cannot be
  proved. Cause: `natMax` has no neutral rule (zkml-lang Z5 flag 12).
- The succ iota of the `Nat` motive fold applies only to `natAdd x 1`.
  `natAdd x L` with a number `L` greater than 1 does not unfold. Cause:
  the iota matches only the form `natAdd x 1`.
- The effectiveness of the set quotient takes `isSet HProp` as a
  hypothesis. Cause: the domain has `ua`, not univalence as an
  equivalence, so a path type of `Type 0` between two propositions is
  not shown to be a proposition.
- A name cannot contain `-`. The laws and lemmas have camelCase names.
- An interval, path or system value cannot exist at run time
  (`REFUSE_RUNTIME_PATH`). Cause: the IR has no interval, no path and no
  system. `fromPartial` of a total system reduces before the lowering.
  Elimination of a neutral partial element stays unreduced.
- A `transp` that stays unreduced at run time is refused with
  `REFUSE_RUNTIME_TRANSP` (P5a). The arm stays; after P5b no entry of
  `test/ir/glue.lang` reaches it. `test/ir/transp.lang` still exercises
  the refusal through an indexed family. A Glue type, a `glue` or an `unglue`
  value at run time is refused with `REFUSE_RUNTIME_GLUE`. A fold with a
  prop-valued motive (R2) at run time is refused with
  `REFUSE_RUNTIME_PROOF` (ruling R14, `test/ir/hit-proof.lang`).
- `transp` and `hcomp` at a Glue type reduce (P5b, CCHM 6.2), and `transp`
  along `ua` computes: `uaβ` is a law (`examples/glue.lang`). Two
  `transpEquiv` proof terms convert through the aligned-system fast path of
  `conv_system`; the pairwise restricted comparison costs exponential work
  in the `hcomp` nesting depth. The arena is not reset between definitions,
  so a file with many such conversions can reach the 2 GiB limit.
- The arena limit of `langc` and the maximum of the Wasm memory are
  2 GiB (ruling R16, `src/main.c:12`, `src/wasm.c:27`). Each `langc`
  run has one arena, and each run checks the domain when it loads it.
  At P7b-b, the laws on `refQSig` (from `qComposeAssoc` on) made the
  domain load cost 96 to 160 MiB (no new measurement at HEAD), and `examples/glue.lang` alone needs more than
  768 MiB. With a limit of 1 GiB, `glue.lang` stopped with `OOM`.
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
uniform recursion and the field telescope of a path constructor), and the case types of a
`fold` over a higher inductive type. It also holds the folds over a
family and a mutual block: `fold_family` (`src/front/check.c:1090`),
`fold_block` (`src/front/check.c:1158`), `check_motive_dep`
(`src/front/check.c:1070`, a motive that depends on the value),
`case_type` (`src/front/check.c:950`) and `rule_fold`
(`src/front/check.c:1513`), and the Nat and List motive folds
(`src/front/check.c:1222`, `src/front/check.c:1250`).
`test/check/expect.txt` gives the refusal code of each reject case
(`TYPE_ENDPOINT`, `TYPE_PRETYPE`, `TYPE_MISMATCH`, `TYPE_SYSTEM`,
`TYPE_LINE`).

`src/front/eval.c` holds the interval normal form, the face normal form,
the reduction of systems and `fromPartial`, the reduction of
`p @ r` (the value keeps the ends of the type) and path eta in the
conversion check, the reductions of `transp` and `hcomp`, the end
rules of a path constructor at an interval, the formal `hcomp` at a
higher inductive type, the reductions of `fold` over one, the `fold`
iota over a family and a mutual block (`fold_family`,
`src/front/eval.c:640`) and the Nat and List folds
(`src/front/eval.c:934`). Neutral interval terms participate in the same algebra
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
The run-time refusal codes are `REFUSE_RUNTIME_CLOSURE`
(`src/front/lower.c:507`), `REFUSE_RUNTIME_PROOF`
(`src/front/lower.c:520`, from the fold dispatch at
`src/front/lower.c:1285` when the motive depends on the value),
`REFUSE_RUNTIME_GLUE` (`src/front/lower.c:525`),
`REFUSE_RUNTIME_TRANSP` (`src/front/lower.c:531`) and
`REFUSE_RUNTIME_TREE` (`src/front/lower.c:1078`,
`src/front/lower.c:1090`).

`src/zkml.c` writes the zkml-lang circuit file (`langc zkml`).
`src/zkml-names.h` holds the zkml-lang names at the PIN 27aef04.
`test/zkmltool.c` checks 31 namespace cases. `test/zk` holds the inputs
and `test/zk/expect.txt` of `make zk-check`.

`domain/domain.lang` holds the sample domain, the P6 policy core,
`refSig` and `refQSig` with `refModel` (P7), the `ZkmlCircuit` block
(P9), and the governance block with `govModel`
(P11, P11b).
`gen/embed.c` copies the file into `langc`, so each guest file can use the
policy core. The core adds no Wasm entry. `src/front/check.c` no longer
reserves `Bool`, because the domain defines it.
