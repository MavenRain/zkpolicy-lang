# TinyCC Wasm realization

Paths below are relative to this host kit, or to the root of a generated
language. The template's `formers/FORMERS.md` gives the host-neutral laws.
`src/front/check.c` lines are rows of the builtin table. `src/front/lower.c`
lines are the lowering to the IR.

DONE means: `langc check` accepts the former, `langc eval` computes it,
and `langc build` lowers it to Wasm when its value is first-order at run
time. Types and universes erase on Wasm. Proof contents erase, while their trap flags remain.

| ID | Former | Status | Where | Evidence |
|---|---|---|---|---|
| F1 | Product | DONE | `src/front/check.c:102`, `src/front/lower.c:577`, `src/front/lower.c:1162` | `examples/formers.lang:3`, `examples/laws.lang:12` |
| F2 | Coproduct | DONE | `src/front/check.c:103`, `src/front/check.c:115`, `src/front/lower.c:1170` | `examples/formers.lang:8`, `examples/laws.lang:13` |
| F3 | Option | DONE | `src/front/check.c:107`, `src/front/check.c:116`, `src/front/lower.c:1172` | `examples/formers.lang:11`, `examples/formers.lang:12` |
| F4 | List | DONE | `src/front/check.c:108`, `src/front/lower.c:818` | `examples/formers.lang:13`, `examples/inventory.lang:4`, `test/ir/spine.lang:8` |
| F5 | Monad pure/map/bind | DONE over Option, Sum E and List | `src/front/check.c:117`, `src/front/lower.c:1036`, `src/front/lower.c:1051`, `src/front/lower.c:833`, `src/front/lower.c:848` | `examples/monad.lang:2`, `examples/monad.lang:6`, `examples/monad.lang:8`, `examples/laws.lang:8`, `test/ir/spine.lang:10` |
| F6 | Algebra fold | DONE over Nat, List and domain families. HOST-LIMIT: a run-time tree | `src/front/check.c:121`, `src/front/lower.c:1086`, `src/front/lower.c:818`, `src/front/lower.c:992`, `src/front/lower.c:1012` | `examples/algebra.lang:4`, `examples/algebra.lang:3`, `examples/algebra.lang:6`, `test/ir/spine.lang:18`, `test/ir/tree.lang:3` |
| F7 | Algebra unfold | DONE into List with a Nat step limit | `src/front/check.c:122`, `src/front/lower.c:920` | `examples/algebra.lang:7`, `examples/algebra.lang:8`, `test/ir/spine.lang:5` |
| F8 | Filter | DONE over Option and List | `src/front/check.c:120`, `src/front/lower.c:1062`, `src/front/lower.c:870` | `examples/monad.lang:9`, `test/ir/spine.lang:11` |
| F9 | Non-dependent Pi | DONE. HOST-LIMIT: a function value at run time | `src/front/check.c:456`, `src/front/lower.c:1222`, `src/front/lower.c:503` | `examples/functions.lang:2`, `examples/functions.lang:4`, `examples/laws.lang:5`, `test/ir/closure.lang:3` |
| F10 | Dependent Pi | DONE over `Type 0` and over Nat | `src/front/check.c:456`, `src/front/lower.c:1219` | `examples/functions.lang:8`, `examples/equality.lang:7` |
| F11 | Sigma | DONE | `src/front/check.c:109`, `src/front/check.c:113`, `src/front/lower.c:1163`, `src/front/lower.c:1166` | `examples/sigma.lang:3`, `examples/sigma.lang:5`, `examples/sigma.lang:6` |
| F12 | Eq refl/symm/trans | DONE | `src/front/check.c:110`, `src/front/check.c:123`, `src/front/check.c:124`, `src/front/lower.c:1143` | `examples/equality.lang:4`, `examples/equality.lang:5`, `examples/equality.lang:6`, `test/check/bad-refl.lang:1` |
| F13 | Eq transport/cong | DONE | `src/front/check.c:125`, `src/front/check.c:126`, `src/front/lower.c:1192`, `src/front/lower.c:1194` | `examples/equality.lang:8`, `examples/equality.lang:9` |
| F14 | Universes | DONE for Type 0 and Type 1, without cumulativity | `src/front/lower.c:1218` | `examples/universes.lang:4`, `test/check/universe.lang:1`, `test/check/universe-range.lang:1` |
| F15 | Generic indexed family | PARTIAL: only the domain file declares an indexed family | `domain/domain.lang:10` | `examples/inventory.lang:9`, `examples/laws.lang:16` |
| C1 | Interval | DONE | `src/front/check.c:135`, `src/front/check.c:138`, `src/front/check.c:139`, `src/front/check.c:140`, `src/front/eval.c:993`, `src/front/lower.c:509`, `src/front/check.c:148`, `src/front/check.c:149`, `src/front/check.c:151`, `src/front/check.c:1191`, `src/front/eval.c:1057`, `src/front/eval.c:1146` | `examples/paths.lang:6`, `examples/paths.lang:7`, `examples/paths.lang:8`, `test/check/pretype.lang:2`, `examples/systems.lang:5`, `examples/systems.lang:6`, `examples/systems.lang:8`, `examples/systems.lang:10`, `test/check/system-overlap.lang:1`, `test/check/system-face.lang:1`, `test/check/from-partial.lang:2` |
| C2 | PathP and Path | DONE | `src/front/check.c:141`, `src/front/check.c:142`, `src/front/check.c:143`, `src/front/check.c:144`, `src/front/check.c:145`, `src/front/eval.c:1005`, `src/front/eval.c:1287` | `examples/paths.lang:4`, `examples/paths.lang:9`, `examples/paths.lang:12`, `examples/paths.lang:13`, `test/check/path-endpoint.lang:2`, `test/check/papp-nat.lang:2`, `test/ir/path.lang:4` |

F6: a fold over a family with one recursive field (for example `Stack`)
lowers to a loop over the spine. A constructor with two or more recursive
fields (for example `Tree`) needs a tree at run time. The lowering refuses
it with `REFUSE_RUNTIME_TREE` when the tree exists only at run time.
A fold over a closed tree works, because normalization folds it at
compile time.

F9: a lambda stays only as the direct function argument of an eliminator.
When a function value must exist at run time, the lowering refuses it
with `REFUSE_RUNTIME_CLOSURE`. Both refusals are HOST-LIMIT reasons, not
defects. `docs/CAPABILITY.md` gives the reasons.

F15: a domain family can take index parameters, for example
`family Lot (size : Nat)`. A constructor takes the indices first
(`makeLot 3 250 : Lot 3`). The checker keeps `Lot 3` and `Lot 4` apart.
A program cannot declare a family (rule R1). No test has a field type
that refers to an index. The index erases on Wasm.

C1, C2: `Path A a b` is `PathP` over a constant line, so the core has no
`Path` operation. `refl`, `symm` and `cong` take a path as well as an `Eq`
proof, and `funExt` gives a path. `trans` and `transport` take an `Eq`
proof only until slice P2 (zkml-lang Z2) adds `transp` and `hcomp`.
Interval and path values have no runtime content: a definition of a
`PathP` type is a proof and erases. When such a value must exist at run time, the lowering
refuses the program with `REFUSE_RUNTIME_PATH` (`test/ir/path.lang`).
This is a HOST-LIMIT reason, not a defect. A definition of a `Partial`
type erases too. `fromPartial` of a total system reduces to its branch
before the lowering (`examples/systems.lang`). Elimination of a neutral
partial element stays unreduced and is subject to the same runtime
refusal.

The refusal mutants are in `test/check/`: R1 `data.lang`, R2 `axiom.lang`,
R3 `rec.lang`, R4 `core-name.lang`. `test/check/expect.txt` gives the code
for each mutant.
