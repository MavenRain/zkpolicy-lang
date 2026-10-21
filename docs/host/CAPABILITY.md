# TinyCC Wasm host capability

This file holds host facts only. Put the facts of your domain in the
`probe/CAPABILITY.md` of your language. Paths are relative to the host
kit, or to the root of a generated language. The numbers were measured
on 2026-10-08 on AArch64 Darwin.

## Toolchain

- TinyCC 0.9.28rc (AArch64 Darwin) builds `build/langc` with
  `-std=c99 -Wall -Werror` (`Makefile:4`). `PIN` has the exact version.
- `cc -Wall -Wextra -Wswitch-enum -Werror -fsyntax-only` checks the same
  sources (`make check-clang`, `Makefile:5`). Each switch on an enum names
  every case and has no default arm.
- `tcc -run gen/embed.c` turns `domain/domain.lang` into `build/domain.c`
  at build time (`Makefile:20`). The domain is part of the executable.
- The kit uses the C99 `uint64_t` type for `Nat`. Unsigned arithmetic in
  C wraps modulo 2^64, so the evaluator and the IR use explicit carry
  tests to find an overflow.
- `src/wasm.c` writes the Wasm binary format directly: the type,
  function, memory, global, export and code sections, with LEB128
  numbers. It needs no library. The gate validates each module with
  `wasm-validate` and `wasm-objdump -x`, and runs it in node.

## Values

- `Nat` is an unsigned 64-bit integer. The lexer refuses a literal larger
  than 18446744073709551615 (`test/parse/nat-range.lang`).
- `natAdd` and `natMul` trap on overflow. `natDiv` and `natMod` trap on a divisor of 0. `natSub` stops at 0. A trap is
  lazy: it stops a computation only when the computation needs it.
  `README.md` gives the rule and the reason.
- There is no text type. The lexer refuses `"` (`test/parse/string.lang`).
- On Wasm, each value is a word and a trap flag. Only the entry result
  is strict.

## Overflow tests on Wasm

- `ADD_CARRY` is `a + b < a`, modulo 2^64 (`src/wasm.c:190`).
- `MUL_CARRY` is `b > (2^64 - 1) / max(a, 1)` with `i64.div_u`
  (`src/wasm.c:193`). The Wasm MVP has no 128-bit product. The test is
  exact: for a > 0, a * b <= 2^64 - 1 if and only if
  b <= floor((2^64 - 1) / a). For a = 0 the product is 0, and the test
  gives 0 for each b.
- `SUB` is `select(a - b, 0, a >= b)`.
- The differential grid holds the edge values (`test/grid.awk:8`). Node
  and `langc eval` agree: `scale 4294967296 4294967296` is `trap`,
  `scale 4294967295 4294967297` is 18446744073709551615, and
  `addPrice 18446744073709551615 1` is `trap`.

## Limits

- The checker and the evaluator stop at a depth of 2000
  (`src/front/check.c:10`, `src/front/core.h:16`). The parser stops at a
  nesting of 1000 (`src/front/parser.c:16`). These limits keep the C
  stack bounded. `test/gate.sh` checks a nesting of 1100 parentheses.
- The evaluator has a fuel of 20000000 steps (`src/front/core.h:17`).
  `langc eval examples/entries.lang square 4294967296` stops with
  `EVAL_FUEL` after 0.91 s user and 0.48 s system CPU time. For this
  reason the differential grid uses values of 2^32 and more only for
  entries with no loop.
- All memory of `langc` comes from one arena with a limit of 2 GiB
  (`src/main.c:12`). A run that reaches it stops with `OOM`.
  `langc eval test/ir/spine.lang foldOnes 4294967296` stops with `OOM`.
- The Wasm memory has a maximum of 32768 pages, which is 2 GiB
  (`src/wasm.c:27`). The allocator traps when `memory.grow` fails.
- The Wasm writer stops at a statement nesting of 2000 (`src/wasm.c:28`).
  The IR printer stops at 4000 (`src/front/lower.c:17`).
- A loop over a list spine has a fuel of 2^63 steps
  (`src/front/lower.c:765`). The heap limit stops the loop first.
- Source files are at most 1 MiB (`src/front/front.h:7`).

## Measured numbers

- `make check` runs 4838 differential cases on 28 modules, with
  0 mismatches.
- `langc build examples/entries.lang` writes a module of 492 bytes for six
  entries.

## HOST-LIMIT reasons

The IR is first-order (`src/ir.h`). It has words, heap blocks, branches,
counted loops and bounded loops. It has no calls, no function values and
no stack of its own. Two programs need more than that. The lowering
refuses them with a HOST-LIMIT code. These are not defects.

- `REFUSE_RUNTIME_CLOSURE` (`src/front/lower.c:507`): a function value
  must exist at run time, for example as the result of a `fold`. The IR
  has no closures. `test/ir/closure.lang` is the gate case.
- `REFUSE_RUNTIME_TREE` (`src/front/lower.c:1090`): a `fold` walks a value
  of a family with two or more recursive fields, and the value exists
  only at run time. A loop walks one spine. A tree needs recursion or an
  explicit stack. `test/ir/tree.lang` is the gate case.
- `REFUSE_RUNTIME_PATH` (`src/front/lower.c:514`): an interval or path
  value must exist at run time, for example in a `fold` accumulator. The
  IR has no interval and no path. A definition of a `PathP` type is a
  proof and erases, so the lowering skips it. `test/ir/path.lang` is the
  gate case. An `hcomp` that stays unreduced refuses
  (`src/front/lower.c:1249`), for example
  `fun x => hcomp (fun j => []) x` at `Nat`: an empty system at a
  neutral base. A `fold` that carries a stuck `natIsSet` at run time
  refuses too. `test/ir/natisset-path.lang` is its gate case.
- `REFUSE_RUNTIME_TRANSP` (`src/front/lower.c:531`): a `transp` that stays
  unreduced at run time. The core has Glue and `ua` (P5a), so `transp` can
  change the data. `test/ir/transp.lang` checks the refusal for transport
  through an indexed family. `test/ir/glue.lang` asserts that the lowering
  of `stuck` succeeds after P5b.
- `REFUSE_RUNTIME_GLUE` (`src/front/lower.c:525`): a Glue type, a `glue` or
  an `unglue` value must exist at run time. The IR has no Glue.
- `REFUSE_RUNTIME_PROOF` (`src/front/lower.c:520`): a fold with a
  prop-valued motive over a higher inductive type (part P7b-b, ruling
  R2) must exist at run time. The fold is a proof and has no run-time
  form (ruling R14). `test/ir/hit-proof.lang` is the gate case. A fold
  with value-dependent motives over a mutual block (P11b) is also a
  proof. It refuses the same way (`src/front/lower.c:1285`,
  `test/ir/block-ind-runtime.lang`, `runtimeCount`).
- A fold over a mutual block whose value exists only at run time (P11,
  ruling R5) has two refusal forms. A bare fold with a `Nat` result refuses
  with `REFUSE_RUNTIME_TREE` (`test/ir/governance-tree.lang`,
  `gCountStuck`). A fold with a function motive refuses with
  `REFUSE_RUNTIME_CLOSURE`, because the result is a function value and the
  IR has no closures (`test/ir/governance-closure.lang`, `gRunStuck`). A
  closed policy with an open private input reduces by iota, so
  `examples/governance.lang` (`gov4`) lowers and builds.

## zkml writer (P9b)

`langc zkml PROG NAME [--residual | --residual-loop=K | --lookup=TABLE |
--softmax=TABLE] -o OUT` writes a
zkml-lang source file for the model `NAME` of `PROG` (`src/zkml.c`). The
input contract:

- With `--residual` (P9b s2), `NAMECirc` is `circResidual` of the dense
  layer, so the model must have m = n. Else the writer refuses with
  `ZKML_MODEL`. Any other option gives `USAGE` (exit 2).
- With `--residual-loop=K` (P9b s3), `NAMECirc` is `circResidualLoop` of
  the dense layer with K steps: `circResidualLoop NAMES (circOp NAMES
  NAMES (opMatMul n n 0)) K`, the denotation (I + W)^K x. K is a decimal
  from 1 to 16, with no sign, no leading zero and nothing after the
  digits. Any other K gives `USAGE` (exit 2). The model must have m = n,
  with the same `ZKML_MODEL` refusal as `--residual`. The golden file is
  `test/zk/smallmodel-smW-loop2.lang` (K = 2).
- With `--lookup=TABLE` (P9b s4), `NAMECirc` is the dense layer, then the
  table lookup on each entry: `circSeq NAMES NAMET NAMET (circOp NAMES
  NAMET (opMatMul n m 0)) (circOp NAMET NAMET (opLookup NAMET 1))`.
  `NAMERd` is `cons w (cons TABLE nil)`: the matrix at index 0, the table
  at index 1. `TABLE` is a program definition of type `List (List Nat)`
  with rows of two entries `[key, value]`. The writer resolves `TABLE` on
  the path of `NAME` (`ZKML_NAME`, then `ZKML_MODEL` for the type and the
  rows), then refuses a row that does not have two entries (`ZKML_MODEL`,
  `the table needs rows of two entries`). `--lookup` alone and
  `--lookup=` with no name give `USAGE` (exit 2). The m = n rule of the
  residual modes does not apply. `TABLE` = `NAME` is allowed. A duplicate
  key reads the first row and a missing key reads 0, as `bbLookup` at the
  PIN. The writer copies the table and its definition closure. `NAMERunE`
  and `NAMEWitE` keep the two-stage trace of the dense mode. The golden
  file is `test/zk/smallmodel-smW-lookup.lang` (table `smSq`).
- With `--softmax=TABLE` (P9b s6b), `NAMECirc` is the dense layer, then
  the softmax with the exp table: `circSeq NAMES NAMET NAMET (circOp
  NAMES NAMET (opMatMul n m 0)) (circOp NAMET NAMET (opSoftmax NAMET 1
  3))`. The scale K is fixed at 3 (`ZKML_SOFTMAX_K`): the output entries
  are floor(e 2^K / sum e). At the PIN, the check of the `refl` proof
  `NAMESat` stops at the arena limit for K above 3. `NAMERd` and the
  table rules are the same as for `--lookup`: `TABLE` is at index 1,
  with the same `ZKML_NAME` and `ZKML_MODEL` refusals. `--softmax`
  alone and `--softmax=` with no name give `USAGE` (exit 2). The golden
  file is `test/zk/smallmodel-smW-softmax.lang` (table `smExp`).
- In every mode (P9b s5, s6a and s6b), the writer also generates
  `NAMESat`, the open satisfaction proof: for every input `x`, the
  two-stage witness `NAMEWit x` satisfies `NAMECirc` (`bbInstSat`). In
  the dense, lookup and softmax modes the proof is `refl` at the PIN.
  In the residual modes
  `refl` does not prove the open path for `circResidual`, because the
  `zipWithT bbAdd` fold is stuck on an open input. There the proof is the
  inverse of the domain lemma `stageDecEnc` at `NAMET`:
  `<i> stageDecEnc Nat 0 NAMET (bbRun NAMES NAMET NAMECirc (NAMERd NAME) x) @ ~ i`.
  One comment line names the proof form. The input binder is `x_` for a
  model named `x`, and the dimension binder is `i_` for a model named
  `i`. The closed proofs at `2 3` and `2013265920 0` check in every mode.
- The writer checks `PROG` first, with the same refusals as `check`.
- `NAME` must be a program definition. A domain name, for example `bbP`,
  does not count. Else the writer refuses with `ZKML_NAME`.
- The type of `NAME` must be `List (List Nat)` (the weights). The machine
  evaluates the value. The value must be a closed list of m >= 1 rows of
  equal length n, with 1 <= n <= `ENTRY_PARAMS_MAX`, and each entry must
  be a `Nat` literal. Else the writer refuses with `ZKML_MODEL`.
- The model entries are the program definitions with an entry type and a
  closure that contains `NAME`. The writer skips an entry when its closure
  refers to a quotient name (a free name that starts with `quot` or
  contains `Quot`). The quotient view stays on the zkpolicy-lang side in
  P9a. One comment line names each skipped entry.
- The writer copies `NAME` and the closure of the model entries (types and
  bodies, program definitions only) in source order. It removes blank
  lines and full-line comments. Domain dependencies must have matching
  declarations at the zkml PIN, including their dependencies. Otherwise
  the writer refuses with `ZKML_MODEL` before opening `OUT`.
- The copied source must use kernel features supported at the PIN.
  Glue operations and proposition-valued HIT folds (including uses in
  erased types and binder annotations) refuse with `ZKML_MODEL`.
- Then it writes nine definitions in every mode. `NAMESat` is the
  ninth. `NAMES` and `NAMET` are the input
  shape [n] and the output shape [m]. `NAMERd` gives `NAME` as matrix 0
  in the weights. `NAMECirc` is the dense layer
  `circOp NAMES NAMET (opMatMul n m 0)`, an operator circuit `Op [n] [m]`.
  The 0 is the matrix index in the weights, not a dimension. `NAMEEnc` is
  a base-1000 code of a list for eval only. It is not injective above 999.
  `NAMEWit` is the witness of two stages for an open input list `x`: the
  pair of `NAME` and the stages `stageEnc Nat NAMES x` and `stageEnc Nat
  NAMET` of the `bbRun` output. `NAMERunE` gives the code of `bbRun` on
  `NAMERd NAME`. `NAMEWitE` gives the code of `instEval` of `bbInst` on
  `NAMEWit` at the input. The P9a witness had the input stage only, and
  its eval gave the input back.
- `make zk-check` checks the output at the PIN. A row in
  `test/zk/expect.txt` can have the WANT form `zp:ENTRY`: the zp eval of
  `ENTRY` in `PROG` at the same arguments. The `smW` RunE and WitE rows
  use `zp:smDenseE`, `zp:smResE` (`smW@residual`), `zp:smRes2E`
  (`smW@residual-loop=2`) and `zp:smActE` (`smW@lookup=smSq`).
- The generated names use suffixes `S`, `T`, `Rd`, `Circ`, `Enc`, `Wit`,
  `RunE`, `WitE` and `Sat`. Copied program names and generated names must be free in
  the pinned zkml domain. A generated name must also be free in
  `PROG`. Collisions refuse with `ZKML_MODEL` before opening `OUT`.

Trusted base: the writer is in the trusted base for the text that it
copies and generates. The guard is the zkml-lang check at the PIN. `make
zk-check` refuses a zkml-lang checkout when the tree of the `PIN` commit
is not the `PIN` tree, builds zkml-lang at that commit, runs its `langc
check` on each writer output and compares its `langc eval` with the `langc
eval` of this kit (53 rows in `test/zk/expect.txt`; 8 rows have the NAME
`smW@residual`, 8 rows `smW@residual-loop=2`, 8 rows
`smW@lookup=smSq` and 8 rows `smW@softmax=smExp`, so the writer runs
with that option, and 2 rows evaluate the copied `smRes2E`, 2 rows the
copied `smActE` and 2 rows the copied `smSoftE` at the PIN). It also checks
`test/zk/writer-zw.lang` at the PIN, then checks `zwRunE` and `zwWitE`
at `2 3`. These are 3 checks: one type check and two evals. `make check`
compares the writer output with the golden files
`test/zk/smallmodel-smW.lang`, `test/zk/writer-zw.lang` and the four
option goldens, and checks the refusals, including the models `zt`, `zr`,
`zs` and `zv` of `test/zk/writer-names.lang`, whose generated names
`ztT`, `zrRd`, `zsSat` and `zvWit` collide. The `langc check` at the PIN
of each writer output is also the proof gate of the generated `NAMESat`:
no extra row is needed. `test/zk/writer-table.lang`
holds a table with rows of three entries and one with rows of one entry,
for the two-entry guard. `test/zk/writer.lang`
has a blank line in a copy, a binder that shadows a program definition, an
entry that uses `Quot` only through a definition, and a model with 17
columns. The C runner `build/zkmltool` adds 23 namespace
regressions, including shadowing and preserving an existing output on
refusal. `src/zkml-names.h` records pinned domain names and matching
imports. `gen/zkml-names.c` regenerates it from both parsed domains;
`make zk-check` verifies that it matches before testing the output.
