# Validation

Date: 2026-10-22. Tools (`PIN` lines 1 to 6): tcc 0.9.28rc 2026-09-04
mob@0fb54300, node v23.10.0, wasm-validate 1.0.39 and wasm-objdump
1.0.39. Host executable: `build/langc` at 3ae366a (P9c s2 and the
review fix).

## Final gate

The recorded `make check` run is the P9c close gate at 3ae366a (P9c s2
and the review fix). It builds `build/langc`, `build/fronttool` and
`build/zkmltool` with tcc, compiles the same sources with
`cc -fsyntax-only`, and runs `test/gate.sh`, `test/review.sh`,
`build/fronttool` and `build/zkmltool`. The gate checks 26 examples, 13
parse refusals, 81 check refusals, 155 evals, 26 IR files, 29 Wasm
modules, 5380 differential cases with 0 mismatches and 70 zkml rows, and
prints `gate: 0 failures`. The review, domain, path and system
regressions passed, and the C runner passed 55 zkml namespace cases. The
run with `make -j 2` took 5181 seconds (P9c close, 2026-10-22, the PIN at
zkml-lang 677478b). It ran before the P9c close doc changes: `make
check` reads no doc.

The recorded `make zk-check` run passed at zkml-lang 677478b (`PIN`
line 8): 75 rows, 3
writer-zw rows, 55 zkml namespace cases and 0 failures. The run took
87 seconds.

The staged review found that the uniform 8-row attention table limit did
not cover larger model widths: a 3x3 model with an 8-row table passed the
writer but its generated `NAMESat` exhausted the PIN arena. The writer
now uses table caps of 8, 8, 5, 4, 3, 2, 1 and 1 rows at widths 1 to 8.
The C runner has 55 cases, including 16 boundary cases. All eight accepted
boundaries check at the PIN; one extra row gives `ZKML_MODEL` and leaves
the previous output intact. The refusal regressions failed before the
fix and pass after it.

Review validation passes `make build check-clang`, all 55 C cases, the
existing 70 writer rows, and `make zk-check` (75 rows plus 3 writer-zw
rows). The full gate above runs on the commit with the review fix. The
review changes affect writer validation, C regressions and
documentation.

Before the review fix, the P9c s2 run passed 39 namespace cases and took
2721 seconds (`make check`) and 83 seconds (`make zk-check`).

Before the s6c gamma-beta width fix, the earlier run passed 35 namespace
cases and took 5379 seconds (`make check`) and 49 seconds (`make
zk-check`).

## Slices

`docs/PLAN.md` section 5 gives the gate of each slice, and
`docs/STATUS.md` gives the tests of each slice.

P0 starts from the tcc-wasm kit. The kit gate passes: 13 Wasm modules
valid and 683 differential cases with 0 mismatches.

P1a ports C1 without faces and C2 from zkml-lang 6ed605a (Z1a). The
zkml-lang Z1a guest tests pass here, and the Z1a mutants are KILLED here.
`examples/paths.lang` gives the laws of `refl`, `symm`, `cong` and
`funExt` by `refl`.

P1b ports faces, partial elements and systems from zkml-lang 573c983
(Z1b). The Z1b guest tests pass here, and the Z1b mutants are KILLED here.
`examples/systems.lang` gives the accept cases.

P2a ports `transp` and `transport` from zkml-lang e069119 (Z2a). The Z2a
guest tests pass here, and the Z2a mutants are KILLED here.
`examples/transp.lang` derives `J` and its computation rule.

P2b-1 ports `hcomp` with checked sides from zkml-lang 0741c38 (Z2b-1).
The Z2b-1 guest tests pass here, and the Z2b-1 mutants are KILLED here.
`examples/hcomp.lang` gives the accept cases.

P2b-2 ports `comp`, `fill`, `hcomp` at a Sigma type, `transp` at `PathP`
and `trans` from zkml-lang 402c9fe (Z2b-2). The Z2b-2 guest tests pass
here, and the Z2b-2 mutants are KILLED here. `examples/kan.lang` gives the
laws `rUnit`, `lUnit` and `assoc`. The port removes the refusal case
`test/check/pequiv-trans.lang`, because that program checks after the
port.

P3a adds higher inductive types to the domain. `examples/hit.lang` gives
the accept cases. A program cannot declare one (`REFUSE_DATA`).

P3b adds the eliminator. `examples/hit.lang` gives `circleRec`,
`truncRec` and `quotRec` with their laws by `refl`, `quotToTrunc`,
`truncProp` and the effectiveness of the set quotient (`effective`, with
`isSet HProp` as a hypothesis). `examples/policy.lang` uses the law
`SoundnessPub` (`extracted`).

P4a adds indexed sorts and the motive fold. `examples/qiit.lang` gives the
accept cases and the laws of `circMap` and `circFlip` by `refl`.

P4b adds mutual blocks and the fold over a block. `examples/rose.lang`
gives the accept cases and the laws of `roseMap`, `kidsMap` and
`kidsMap2` by `refl`. A fold over a block at run time fails with
`REFUSE_RUNTIME_TREE`.

P5a to P5c add Glue types, `glue`, `unglue`, `ua`, the Kan rules for Glue
and the universe, and `uaβ`. P5c adds `glueEta`, `isProp`, `isSet`,
`propEquiv` and `hPropExt` to the domain, the singleton tests in
`examples/glue.lang` and the C7 check tests.

P6 adds the policy core. `examples/policy.lang` uses each accessor and
each lemma over an abstract model. Check cases refuse `verify` on a
context and `privacy` with two different public parts. The third case
(`trans` on two paths) is removed in P2b-2.

P7a adds the `Ref` signature. `examples/ref.lang` uses each proved law,
and one mutant for each law does not check.

P7b-nat adds `Nat` induction. `examples/nat.lang` proves `natAddAssoc` by
induction and gives `circCount`, a `Nat`-valued fold out of `Circuit`.

P7b-list adds `List` induction. `examples/list.lang` proves `appNatAssoc`
and `sumFromApp` by induction.

P7b-a proves `ComposeAssoc`, `EvalCompose`, `Completeness` and
`SoundnessPub` on `refSig`. `examples/ref.lang` uses each new law at
closed instances.

P7b-b adds the fold over a higher inductive type with a motive into props
and the quotient sig `refQSig`. The tests are the `examples/hit.lang`
tail, the eval row `hit quotPropIota`, seven `test/check/propfold-*.lang`
refusals and `test/ir/hit-proof.lang` (`REFUSE_RUNTIME_PROOF`).

P8 adds the Wasm edge entries in `examples/edge.lang`. The tests are 24
eval rows (`edge` in `test/eval/expect.txt`) and the refusal row
`test/ir/runtime-policy.lang`. The gate counts are 24 examples, 95 evals,
27 Wasm modules and 3908 differential cases with 0 mismatches.

P9a adds `SmallModel`, `ZkmlCircuit` and the writer `langc zkml`. The
golden file is `test/zk/smallmodel-smW.lang`. `make check` has 7 zkml
rows (2 golden outputs and 5 refusal rows) and 21 C namespace cases.
`make zk-check` has 13 rows. The gate counts are 25 examples, 117 evals,
28 Wasm modules and 4738 differential cases with 0 mismatches. Of 16
mutants on `src/zkml.c`, the golden output and the C regressions kill 12,
the rows in `test/zk/writer.lang` kill 3, and M6 is equivalent.

P9b s1 adds the dense circuit at zkml-lang 27aef04: 9 zkml rows and 15 zk
rows. P9b s2 adds `--residual`: 12 zkml rows and 23 zk rows, and mutants
M1 to M4 and M6 are KILLED. P9b s3 adds `--residual-loop=K`: 121 evals,
4838 differential cases, 18 zkml rows and 33 zk rows, and mutants M1 to
M8 are KILLED. P9b s4 adds `--lookup=TABLE`: 125 evals, 4938
differential cases, 26 zkml rows, 23 C namespace cases and 43 zk rows,
and mutants M1 to M8 are KILLED. P9b s5 adds `NAMEWit` and `NAMESat`: 28
zkml rows (5 golden outputs and 23 refusal rows), 31 C namespace cases
and 43 zk rows, and mutants M1 to M6 are KILLED. The PIN type check of
each writer output is the proof gate of `NAMESat`. P9b s6a adds
`NAMESat` in the residual modes. The gate on 2026-10-20 (HEAD 8e55974
and the s6a diff) passes in 6644 s with 28 zkml rows, 31 C namespace
cases and 43 zk rows. No count changes. Mutants M1 to M5 are KILLED.
P9b s6b adds `--softmax=TABLE` at the fixed scale K = 3: 140 evals,
5274 differential cases (100 for `smSoftE` and 216 for `smSoftScale`),
36 zkml rows, 33 C namespace cases and 53 zk rows. The gate on
2026-10-21 (HEAD 05f95b5 and the s6b diff) passes in 3246 s. It
ran before the s6b doc changes, because `make check` reads no doc.
Mutants M1 to M5 are KILLED, each by a golden, refusal or zk row.
A first gate run failed: the Wasm module of `examples/smallmodel.lang`
was not valid, because the `smSoftE` entry had 176974 locals (the old
`smSoftScale` was a fold of doubling steps). The first size fix used
one multiplication and division, and `smSoftE` had 2120 locals.
Staged review found that this shortcut traps on
`smSoftScale 3 18446744073709551615 18446744073709551615`, where the PIN
returns 8. The host now uses the PIN's quotient/remainder doubling
steps. The zero remainder `natMod v 1` keeps the count dynamic for an
open input, so lowering shares the state in a loop instead of expanding
it at every use. `smSoftE` has 9250 IR locals and the module validates.
Six boundary regressions run in both eval and Wasm, including a
representable result at K = 64 and a genuinely overflowing result.
The bounded loop keeps all 100 `smSoftE` grid cases. The coverage
after these fixes was 146 evals and 5280 differential cases.
Post-review targeted checks pass: all 146 eval rows, all 1132
smallmodel eval/Wasm cases with 0 mismatches, all 36 writer golden and
refusal rows, and `make zk-check` (53 rows, 3 writer-zw rows and 33
namespace cases). The C build and `check-clang` also pass. The full
`make check` suite was not rerun after these review fixes.
P9b s6c adds `--layernorm=GB,TABLE`: 150 evals, 5380 differential
cases (100 for `smLnE`), 52 zkml rows, 35 C namespace cases and 63 zk
rows. The gate on 2026-10-21 (HEAD c246390 and the s6c diff) passes in
5379 s. It is the first full run after the s6b review fixes. It ran
before the s6c doc changes, because `make check` reads no doc.
Mutants M1 to M6 are KILLED, each by a golden, refusal, eval or zk row.

The staged s6c review separates the gamma-beta column limit from the
16-parameter input limit: gamma-beta rows can have m entries even when
m is above 16. A C regression for a 17-by-1 model fails before the fix
and passes after it, including the generated proof and both generated
evaluation entries at the PIN. Post-review checks pass: `check-clang`,
52 writer golden and refusal rows, and `make zk-check` (63 rows,
3 writer-zw rows and 36 C namespace cases). The full `make check`
on 2026-10-21 at 2b37870 (the commit with this fix) passes in 2807
seconds with 0 failures. Its counts equal the s6c counts, except 36 C
namespace cases. `make zk-check` alone passes in 51 seconds.

P11 adds the governance instance. `examples/governance.lang` has 5 eval
rows, and the review adds 3 eval rows. The refusal rows are
`test/ir/governance-tree.lang` (`REFUSE_RUNTIME_TREE`) and
`test/ir/governance-closure.lang` (`REFUSE_RUNTIME_CLOSURE`). The gate
counts are 26 examples, 133 evals, 29 Wasm modules and 4955 differential
cases with 0 mismatches. Mutants M1 to M6 are KILLED.

P11b adds block induction. The new rows are three check refusals
(`test/check/block-ind-hit.lang` and `test/check/block-ind-mixed.lang`
with `TYPE_MISMATCH`, `test/check/block-ind-noih.lang` with
`TYPE_INFER`), the IR file `test/ir/block-ind-runtime.lang` and the eval
rows `q7n`, `q7m` and `gqComp4`. The gate counts are 81 check refusals,
136 evals and 4958 differential cases with 0 mismatches. Mutants K1 to K3
(kernel) and D1 to D3 (domain) are KILLED.

P12 writes the final docs and runs the final gate above.

P9b is closed at s6c.

P9c s1 moves the PIN to zkml-lang 677478b (tree 19607ac1) and
regenerates `src/zkml-names.h` (12 new names). The full `make -j 2
check` on 2026-10-21 at f842b04 with this change passes in 5946 seconds
with 0 failures. Its counts equal the P9b close counts: 150 evals, 5380
differential cases with 0 mismatches, 52 zkml rows and 36 C namespace
cases. `make zk-check` alone passes in 141 seconds: 63 rows, 3
writer-zw rows and 0 failures.

P9c s2 adds `--attention=WK,WV,TABLE`: 155 evals, 5380 differential
cases, 70 zkml rows, 39 C namespace cases and 75 zk rows. The full
`make -j 2 check` on 2026-10-22 at 4eb49b7 with the s2 change passes in
2721 seconds with 0 failures. It ran before the s2 doc changes,
because `make check` reads no doc. `make zk-check` alone passes in
83 seconds: 75 rows, 3 writer-zw rows and 0 failures. Mutants M1
to M6 are KILLED, each by a golden or refusal row. The 8 attention
host definitions were in `test/host/smallmodel-attention.lang` until
P9d s1, outside `examples/`, so the IR and Wasm loops did not read
them. `smAttnE` has
no Wasm differential: its Wasm function has 168216 locals, and
`wasm-validate` (wabt 1.0.39) and node refuse more than 50000. A first
full run with the definitions in `examples/smallmodel.lang` failed at
`wasm-validate` for this reason. Now the narrow gate of each writer
change also runs the Wasm and differential step for each changed
example file (ruling R27). Slice P10 (the EVM verifier) has no gate
run.

P9d s1 ports the IR sharing of zkml-lang 4fadeb6 to
`src/front/lower.c` and moves the 8 attention host definitions to
`examples/smallmodel.lang` (`smAttnE` has 823 locals). The full
`make -j 2 check` on 2026-10-22 at 72290e7 with the s1 change passes
in 3047 seconds with 0 failures: 161 evals, 26 IR files, 30 Wasm
modules, 6877 differential cases with 0 mismatches (5380, 1297 more on
`examples/smallmodel.lang` and 200 on `test/ir/share.lang`), 70 zkml
rows and 55 C namespace cases. It ran before the s1 doc changes,
because `make check` reads no doc. `make zk-check` alone passes in 96
seconds: 75 rows, 3 writer-zw rows and 0 failures. Mutants M1 (no
sharing) and M2 (each value kept in the outermost block) are KILLED.
