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
| F1 | Product | DONE | `src/front/check.c:110`, `src/front/lower.c:577`, `src/front/lower.c:1215` | `examples/formers.lang:3`, `examples/laws.lang:12` |
| F2 | Coproduct | DONE | `src/front/check.c:111`, `src/front/check.c:123`, `src/front/lower.c:1223` | `examples/formers.lang:8`, `examples/laws.lang:13` |
| F3 | Option | DONE | `src/front/check.c:115`, `src/front/check.c:124`, `src/front/lower.c:1227` | `examples/formers.lang:11`, `examples/formers.lang:12` |
| F4 | List | DONE | `src/front/check.c:116`, `src/front/lower.c:836` | `examples/formers.lang:13`, `examples/inventory.lang:4`, `test/ir/spine.lang:8` |
| F5 | Monad pure/map/bind | DONE over Option, Sum E and List | `src/front/check.c:125`, `src/front/lower.c:1072`, `src/front/lower.c:1087`, `src/front/lower.c:851`, `src/front/lower.c:866` | `examples/monad.lang:2`, `examples/monad.lang:6`, `examples/monad.lang:8`, `examples/laws.lang:8`, `test/ir/spine.lang:10` |
| F6 | Algebra fold | DONE over Nat (with the motive form, P7b-nat), List and domain families. HOST-LIMIT: a run-time tree | `src/front/check.c:129`, `src/front/check.c:143`, `src/front/check.c:144`, `src/front/check.c:1111`, `src/front/check.c:1188`, `src/front/check.c:1211`, `src/front/check.c:1507`, `src/front/eval.c:530`, `src/front/eval.c:797`, `src/front/eval.c:847`, `src/front/eval.c:3013`, `src/front/eval.c:3023`, `src/front/lower.c:1122`, `src/front/lower.c:836`, `src/front/lower.c:1021`, `src/front/lower.c:1043`, `src/front/lower.c:676`, `src/front/lower.c:1225`, `src/front/lower.c:1277` | `examples/algebra.lang:4`, `examples/algebra.lang:3`, `examples/algebra.lang:6`, `test/ir/spine.lang:18`, `test/ir/tree.lang:3`, `examples/nat.lang:9`, `examples/nat.lang:10`, `examples/nat.lang:11`, `examples/nat.lang:12`, `examples/nat.lang:13`, `examples/nat.lang:15`, `examples/nat.lang:18`, `examples/nat.lang:19`, `examples/nat.lang:23`, `examples/nat.lang:24`, `examples/nat.lang:25`, `test/check/eta-hit.lang:4`, `test/check/eta-two-ctor.lang:3`, `test/check/natadd-assoc-refl.lang:3`, `test/check/natfold-step-endpoint.lang:4`, `test/check/natmax-neutral.lang:3`, `test/eval/expect.txt:51`, `test/eval/expect.txt:58`, `test/fronttool.c:458`, `test/ir/natisset-path.lang:4` |
| F7 | Algebra unfold | DONE into List with a Nat step limit | `src/front/check.c:130`, `src/front/lower.c:938` | `examples/algebra.lang:7`, `examples/algebra.lang:8`, `test/ir/spine.lang:5` |
| F8 | Filter | DONE over Option and List | `src/front/check.c:128`, `src/front/lower.c:1098`, `src/front/lower.c:888` | `examples/monad.lang:9`, `test/ir/spine.lang:11` |
| F9 | Non-dependent Pi | DONE. HOST-LIMIT: a function value at run time | `src/front/check.c:497`, `src/front/lower.c:1279`, `src/front/lower.c:503` | `examples/functions.lang:2`, `examples/functions.lang:4`, `examples/laws.lang:5`, `test/ir/closure.lang:3` |
| F10 | Dependent Pi | DONE over `Type 0` and over Nat | `src/front/check.c:497`, `src/front/lower.c:1274` | `examples/functions.lang:8`, `examples/equality.lang:7` |
| F11 | Sigma | DONE | `src/front/check.c:117`, `src/front/check.c:121`, `src/front/lower.c:1216`, `src/front/lower.c:1219` | `examples/sigma.lang:3`, `examples/sigma.lang:5`, `examples/sigma.lang:6` |
| F12 | Eq refl/symm/trans | DONE | `src/front/check.c:118`, `src/front/check.c:131`, `src/front/check.c:132`, `src/front/lower.c:1187` | `examples/equality.lang:4`, `examples/equality.lang:5`, `examples/equality.lang:6`, `test/check/bad-refl.lang:1` |
| F13 | Eq transport/cong | DONE | `src/front/check.c:133`, `src/front/check.c:134`, `src/front/lower.c:1247`, `src/front/lower.c:1249` | `examples/equality.lang:8`, `examples/equality.lang:9` |
| F14 | Universes | DONE for Type 0 and Type 1, without cumulativity | `src/front/lower.c:1273` | `examples/universes.lang:4`, `test/check/universe.lang:1`, `test/check/universe-range.lang:1` |
| F15 | Generic indexed family | PARTIAL: only the domain file declares an indexed family | `domain/domain.lang:10` | `examples/inventory.lang:9`, `examples/laws.lang:16` |
| C1 | Interval | DONE | `src/front/check.c:145`, `src/front/check.c:148`, `src/front/check.c:149`, `src/front/check.c:150`, `src/front/eval.c:1100`, `src/front/lower.c:509`, `src/front/check.c:158`, `src/front/check.c:159`, `src/front/check.c:161`, `src/front/check.c:1757`, `src/front/eval.c:1164`, `src/front/eval.c:1253` | `examples/paths.lang:6`, `examples/paths.lang:7`, `examples/paths.lang:8`, `test/check/pretype.lang:2`, `examples/systems.lang:5`, `examples/systems.lang:6`, `examples/systems.lang:8`, `examples/systems.lang:10`, `test/check/system-overlap.lang:1`, `test/check/system-face.lang:1`, `test/check/from-partial.lang:2` |
| C2 | PathP and Path | DONE | `src/front/check.c:151`, `src/front/check.c:152`, `src/front/check.c:153`, `src/front/check.c:154`, `src/front/check.c:155`, `src/front/eval.c:1112`, `src/front/eval.c:2156` | `examples/paths.lang:4`, `examples/paths.lang:9`, `examples/paths.lang:12`, `examples/paths.lang:13`, `test/check/path-endpoint.lang:2`, `test/check/papp-nat.lang:2`, `test/ir/path.lang:4` |
| C3 | Kan operations | DONE: `transp`, `hcomp`, `comp` and `fill`; `transport` and `trans` over a path; the C3 notes give the limits | `src/front/check.c:163`, `src/front/check.c:1416`, `src/front/check.c:1838`, `src/front/check.c:1864`, `src/front/eval.c:1754`, `src/front/lower.c:1210`, `src/front/check.c:164`, `src/front/check.c:1965`, `src/front/eval.c:2001`, `src/front/lower.c:1207`, `src/front/check.c:165`, `src/front/check.c:166`, `src/front/check.c:2018`, `src/front/check.c:1314`, `src/front/eval.c:1457`, `src/front/eval.c:1907`, `src/front/eval.c:1689` | `examples/transp.lang:5`, `examples/transp.lang:9`, `examples/transp.lang:11`, `examples/transp.lang:15`, `examples/transp.lang:20`, `test/check/transp-const.lang:2`, `test/check/transp-line.lang:2`, `test/check/transp-start.lang:2`, `test/check/transp-name.lang:2`, `examples/hcomp.lang:7`, `examples/hcomp.lang:8`, `examples/hcomp.lang:9`, `examples/hcomp.lang:10`, `examples/hcomp.lang:11`, `examples/hcomp.lang:13`, `test/check/hcomp-face.lang:2`, `test/check/hcomp-base.lang:2`, `test/check/hcomp-side.lang:2`, `test/check/hcomp-name.lang:2`, `examples/kan.lang:8`, `examples/kan.lang:9`, `examples/kan.lang:10`, `examples/kan.lang:11`, `examples/kan.lang:12`, `examples/kan.lang:14`, `examples/kan.lang:17`, `examples/kan.lang:18`, `examples/kan.lang:21`, `test/check/comp-base.lang:2`, `test/check/comp-name.lang:2`, `test/check/fill-name.lang:2`, `test/check/trans-ends.lang:2`, `test/check/trans-line.lang:2` |
| C4 | Higher inductive types | DONE: declarations, path constructor values, the formal `hcomp` (P3a) and the eliminator (P3b) | `src/front/check.c:3108`, `src/front/check.c:3043`, `src/front/check.c:2851`, `src/front/check.c:2843`, `src/front/check.c:2366`, `src/front/core.h:92`, `src/front/eval.c:2559`, `src/front/eval.c:2380`, `src/front/eval.c:2405`, `src/front/eval.c:3559`, `src/front/lower.c:651`, `src/front/lower.c:1224`, `src/front/check.c:1188`, `src/front/check.c:1008`, `src/front/check.c:910`, `src/front/check.c:833`, `src/front/eval.c:628`, `src/front/eval.c:2674` | `domain/domain.lang:119`, `domain/domain.lang:120`, `domain/domain.lang:122`, `examples/hit.lang:17`, `examples/hit.lang:18`, `examples/hit.lang:21`, `examples/hit.lang:22`, `examples/hit.lang:23`, `examples/hit.lang:24`, `examples/hit.lang:25`, `examples/hit.lang:28`, `examples/hit.lang:29`, `examples/hit.lang:30`, `examples/hit.lang:31`, `examples/hit.lang:47`, `examples/hit.lang:49`, `examples/hit.lang:54`, `test/check/hit-program.lang:2`, `test/ir/hit-path.lang:5`, `test/check/hit-fold-path.lang:2`, `test/check/hit-fold-prop.lang:3`, `test/check/hit-fold-arity.lang:2` |
| C5 | Quotient inductive-inductive types | DONE: indexed sorts, the motive fold and the case shape D6 (P4a); mutual blocks with one motive for each sort (P4b); `Nat`-valued folds with `natIsSet` (P7b-nat) | `src/front/check.c:860`, `src/front/check.c:880`, `src/front/check.c:910`, `src/front/check.c:938`, `src/front/check.c:994`, `src/front/check.c:1008`, `src/front/check.c:1188`, `src/front/check.c:2390`, `src/front/check.c:2432`, `src/front/check.c:3008`, `src/front/check.c:3223`, `src/front/check.c:3302`, `src/front/core.h:177`, `src/front/core.h:218`, `src/front/eval.c:581`, `src/front/eval.c:609`, `src/front/eval.c:650`, `src/front/eval.c:879`, `src/front/eval.c:2587`, `src/front/eval.c:2665`, `src/front/eval.c:2674`, `src/front/eval.c:2718`, `src/front/eval.c:3630`, `src/front/lower.c:706`, `src/front/lower.c:1000`, `src/front/lower.c:1020`, `src/front/lower.c:1038`, `src/front/lower.c:1050`, `src/front/parser.c:565` | `domain/domain.lang:130`, `domain/domain.lang:131`, `domain/domain.lang:132`, `domain/domain.lang:133`, `domain/domain.lang:139`, `domain/domain.lang:143`, `examples/nat.lang:26`, `examples/nat.lang:27`, `examples/qiit.lang:11`, `examples/qiit.lang:12`, `examples/qiit.lang:13`, `examples/qiit.lang:17`, `examples/qiit.lang:19`, `examples/qiit.lang:26`, `examples/rose.lang:7`, `examples/rose.lang:10`, `examples/rose.lang:14`, `examples/rose.lang:22`, `test/check/rose-count.lang:5`, `test/eval/expect.txt:40`, `test/eval/expect.txt:46`, `test/fronttool.c:393`, `test/fronttool.c:468`, `test/fronttool.c:472`, `test/ir/rose-tree.lang:5` |

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
proof, and `funExt` gives a path. `transport` takes a path too (C3).
`trans` takes a path too (C3). Interval and path
values have no runtime content: a definition of a `PathP` type is a proof
and erases. When such a value must exist at run time, the lowering
refuses the program with `REFUSE_RUNTIME_PATH` (`test/ir/path.lang`).
This is a HOST-LIMIT reason, not a defect. A definition of a `Partial`
type erases too. `fromPartial` of a total system reduces to its branch
before the lowering (`examples/systems.lang`). Elimination of a neutral
partial element stays unreduced and is subject to the same runtime
refusal.

C3: `transp A r u` checks the line, its constancy where `r` holds
(`TYPE_LINE`) and `u : A i0`. The evaluator moves `u` along Pi, Sigma,
Prod and the kit data types. `transport P p u` over a path is
`transp (fun i => P (p @ i)) i0 u`. A `transp` that stays unreduced at
run time is refused with `REFUSE_RUNTIME_TRANSP` (P5a). After P5b, `transp`
along `ua` computes, so no entry of `test/ir/glue.lang` reaches the arm: the
gate case lowers a `transp` along `ua` at `Nat` to its argument.

C3: `hcomp side u0` takes a side of the form `fun j => [ ... ]`, a system
in `j` (`TYPE_SYSTEM` for another side). The faces are in the outer
context, and their join is `phi`. On each face, `side i0` must agree with
`u0` (`TYPE_SYSTEM`). The evaluator gives `side i1` where `phi` holds, and
moves the composition inside Pi, Prod and PathP types. An `hcomp` that
stays unreduced at run time is refused with `REFUSE_RUNTIME_PATH`
(`src/front/lower.c:1207`), for example `fun x => hcomp (fun j => []) x`
at `Nat`.

C3: `comp A side u0` and `fill A side u0 r` check the line, the system in
`j` at `A j` and the base condition (`TYPE_SYSTEM`). Over a constant line
the checker gives the `hcomp` forms (`src/front/check.c:2018`), so the
laws hold at a type variable. Over a varying line it gives the CCHM forms.
`hcomp` at a Sigma type (`src/front/eval.c:1907`) and `transp`
along a line of `PathP` types (`src/front/eval.c:1689`) use the CCHM
`comp` (`src/front/eval.c:1457`). `trans p q` over a path is an
`hcomp` (`src/front/check.c:1314`). A path over a varying line
fails with `TYPE_MISMATCH`, and `trans p refl` fails with `TYPE_INFER`:
use `<i> b` with a declared type. `examples/kan.lang` checks `rUnit`,
`lUnit` and `assoc`.

C6: `Glue A sys`, `glue sys a` and `unglue g` check as `formers/FORMERS.md`
gives (`GLUE_EQUIV`, `GLUE_COVER`, `GLUE_BOUNDARY`, `UNGLUE_TYPE`). The
domain prelude defines `idEquiv` and `ua`. The evaluator reduces a Glue
type to `T`, a `glue` to its branch and an `unglue` to `witness e g` where
a face holds, and `unglue (glue sys a)` to `a`. The Kan rules of P5b move `transp`
and `hcomp` through a Glue type, and `hcomp` through `Type 0`. At run time, a
Glue type, a `glue` or an `unglue` value is refused with
`REFUSE_RUNTIME_GLUE`. `examples/glue.lang` checks `ua`, `swapEquiv` and
`unglue` of `glue`.

The refusal mutants are in `test/check/`: R1 `data.lang`, R2 `axiom.lang`,
R3 `rec.lang`, R4 `core-name.lang`. `test/check/expect.txt` gives the code
for each mutant.
