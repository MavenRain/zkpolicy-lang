# Validation

Date: 2026-10-20. Tools (`PIN` lines 1 to 6): tcc 0.9.28rc 2026-09-04
mob@0fb54300, node v23.10.0, wasm-validate 1.0.39 and wasm-objdump
1.0.39. Host executable: `build/langc` at a778619.

## Final gate

`make check` passes. It builds `build/langc`, `build/fronttool` and
`build/zkmltool` with tcc, compiles the same sources with
`cc -fsyntax-only`, and runs `test/gate.sh`, `test/review.sh`,
`build/fronttool` and `build/zkmltool`. The gate checks 26 examples, 13
parse refusals, 81 check refusals, 136 evals, 26 IR files, 29 Wasm
modules, 4958 differential cases with 0 mismatches and 28 zkml rows, and
prints `gate: 0 failures`. The review, domain, path and system
regressions pass, and the C runner passes 31 zkml namespace cases. The
run with `make -j 2` took 4720 seconds.

`make zk-check` passes at zkml-lang 27aef04 (`PIN` line 8): 43 rows, 3
writer-zw rows, 31 zkml namespace cases and 0 failures. The run took 58
seconds.

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
each writer output is the proof gate of `NAMESat`.

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

The rest of P9b (the softmax, layer normalization and attention circuits,
and a `NAMESat` proof for the residual modes) and P10 (the EVM verifier)
have no gate run.
