# Host capability probe: tcc-wasm

Date: 2026-10-20. Host tree: this repository (`src/`, the tcc-wasm kit) at
a778619, read only. Driver: `build/langc`. No separate probe ran for this
page (P12 ruling R6). Each answer comes from `docs/host/CAPABILITY.md`,
`formers/tcc-wasm.md`, `docs/STATUS.md` and the slice gates.

`probe/guard.py` is the guard for a later probe run. It stops a command
above 4096 MB resident memory or after 120 s (its defaults,
`probe/guard.py:92-93`). No probe process ran, so this page gives no peak
memory.

## Questions

Each host must answer these questions. Each answer is one numbered section
below. An answer sets the status of a former in `formers/tcc-wasm.md`.

| Question | Section | Formers |
|---|---|---|
| Which commands check a file, print a normal form, build and run? | 1 | all |
| Does the host take several files, or must the driver join them? | 2 | all |
| Which recursion does the host accept, and how does it refuse the rest? | 3 | F6, F7 |
| Which `Nat` primitives exist? Does `Nat` have an eliminator? | 4 | F6, F7 |
| How does a result leave the host? | 5 | target |
| What do check and build cost (time and resident memory) at N = 100 and N = 1000? | 6 | all |
| Can a family take a parameter? Can a constructor of such a family appear in a term? | 7 | F4, F12, F15 |
| Can a program project a Sigma? Does the kernel accept Sigma eta? | 8 | F11 |
| How does a program use equality: transport, cong, an index type? | 9 | F12, F13 |
| How does the host report an axiom? | 10 | rule R2 |

## 1. Commands

| Job | Command | Result |
| --- | --- | --- |
| Check | `build/langc check PROG` | `ok` and exit 0. Exit 1: the program is refused (a refusal code and a message). Exit 2: usage or IO (`src/main.c:2`). |
| Print normal form | `build/langc eval PROG NAME [ARGS...]` | The normal form of `NAME` on stdout (`src/main.c:195`). Example: `gqComp4` of `examples/governance.lang` gives 36. |
| Build | `build/langc build PROG -o OUT [--runtime]` | A Wasm module. `wasm-validate` accepts it (`test/gate.sh:349`). |
| Run | `node test/run-wasm.mjs MODULE - < CASES` | The result of each case (`test/gate.sh:363`). |
| Write a circuit | `build/langc zkml PROG NAME [--residual \| --residual-loop=K \| --lookup=TABLE] -o OUT` | A zkml-lang source file for the model `NAME` (`docs/host/CAPABILITY.md:128-129`). |

- `src/main.c:47-52` gives the usage text. `build/langc ir PROG` and
  `build/langc abi PROG` print the IR and the entry interface.
- Each command takes one program file (`src/main.c:113`). Any other
  `zkml` option gives `USAGE` (exit 2, `docs/host/CAPABILITY.md:134`).
- Not probed: no command ran apart from the gate. `make check` runs each
  command above.

## 2. Several files

The host takes one program file (`src/main.c:113`). It has no import form.
`gen/embed.c` copies `domain/domain.lang` into `langc` at build time
(`Makefile:20`), so each program can use the policy core
(`docs/STATUS.md:538`). A program cannot declare a family (rule R1,
`formers/tcc-wasm.md:50`).

## 3. Recursion and totality

There is no general recursion (rule R3, `formers/FORMERS.md:51-53`).
Recursion comes only from `fold` (F6) and `unfold` (F7). A definition can
use only the definitions before it. A self-reference or a `def rec` refuses
with `REFUSE_REC` (`src/front/check.c:2863`): "%s is this definition or a
later one; a definition can use only earlier ones (rule R3)". The test rows
are `rec.lang REFUSE_REC` and `self.lang REFUSE_REC`
(`test/check/expect.txt:3-4`).

## 4. Nat

- Primitives (`src/front/check.c:139-147`): `natAdd`, `natSub`, `natMul`,
  `natDiv`, `natMod` and `natMax` return `Nat`; `natEq` and `natLe`
  return `Flag`. `natIsSet` proves equality between paths in `Nat`.
- `Nat` is the C99 `uint64_t` type (`docs/host/CAPABILITY.md:17-19`). The
  largest value is 18446744073709551615, 2^64 - 1
  (`docs/host/CAPABILITY.md:27-28`). This value can leave the host
  (`docs/host/CAPABILITY.md:47`).
- `natAdd` and `natMul` trap on overflow. `natDiv` and `natMod` trap on a
  divisor of 0. `natSub` stops at 0 (`docs/host/CAPABILITY.md:29`). The
  addition layers of the domain saturate at the host bound
  (`docs/STATUS.md:116`).
- A definition recurses on a `Nat` only through `fold`, using either the
  iterator form (`examples/algebra.lang:4`) or the motive form (slice
  P7b-nat, `formers/tcc-wasm.md:19`).

## 5. How a result leaves the host

- `langc eval` prints the normal form on stdout (`src/main.c:195`).
- A Wasm entry takes and gives `Nat` or `Flag` values only
  (`docs/STATUS.md:159`). Each value is a word and a trap flag. Only the
  entry result is strict (`docs/host/CAPABILITY.md:33-34`).
- `langc zkml` writes a zkml-lang source file to `OUT`
  (`docs/host/CAPABILITY.md:128-129`). `make zk-check` checks the files
  with zkml-lang at `PIN`.

## 6. Cost

Not measured (P12 ruling R6). No probe ran at N = 100 or N = 1000.

| Step | N = 100 | N = 1000 |
| --- | --- | --- |
| Check | not measured | not measured |
| Build | not measured | not measured |
| Run | not measured | not measured |

The kit limits are in `docs/host/CAPABILITY.md` (measured 2026-10-08 on
AArch64 Darwin): the arena limit is 2 GiB (line 61), and the Wasm memory is
32768 pages, which is 2 GiB (line 64).

## 7. Parametric families

Yes, in the domain file only. `domain/domain.lang:136` declares
`family Circuit (a b : Shape)` (slice P4a, `docs/STATUS.md:86`).
`examples/qiit.lang` is the probe file: it uses the constructors of
`Circuit` in terms. A program cannot declare a family (rule R1,
`formers/tcc-wasm.md:50`), so F15 is PARTIAL (`formers/tcc-wasm.md:28`). F4
and F12 are DONE (`formers/tcc-wasm.md:17`, `formers/tcc-wasm.md:25`).

## 8. Sigma projection and eta

Yes. `pack` makes a Sigma value, and `witness` and `payload` project it
(F11 DONE, `formers/tcc-wasm.md:24`; `src/front/check.c:121`,
`src/front/check.c:125`). There is no Sigma pattern: a program uses the
projections. The type forms are `Sigma (n : Nat) (Eq Nat n price)` and
`(n : Nat) * Eq Nat n price` (`examples/sigma.lang:3`,
`examples/sigma.lang:6`). The eta law is `pack (witness s) (payload s) = s`
(`formers/FORMERS.md:231`). Slice P7b-nat adds record eta to the
conversion (`docs/STATUS.md:118-119`).

## 9. Equality

- `Eq A a b` with `refl`, `symm` and `trans` (F12 DONE,
  `formers/tcc-wasm.md:25`). `transport P p u` and `cong f p` (F13 DONE,
  `formers/tcc-wasm.md:26`). `examples/equality.lang:4-9` uses each form.
- `Eq` takes the type as its first argument (arity 3,
  `src/front/check.c:110`), so one family serves each index type.
- `Path A a b` is `PathP` over a constant line (C2 DONE,
  `formers/tcc-wasm.md:30`). `refl`, `symm` and `cong` take a path or an
  `Eq` proof (`formers/tcc-wasm.md:53-55`). `transport P p u` over a path is
  `transp (fun i => P (p @ i)) i0 u` (slice P2a,
  `formers/tcc-wasm.md:68-69`).

## 10. Axiom detection

The host has no axiom form and no axiom report. The compiler refuses
`axiom` (rule R2, `formers/FORMERS.md:47-50`): `test/check/axiom.lang`
refuses with `REFUSE_AXIOM` (`test/check/expect.txt:2`). Thus a checked
program has no axiom, and no report is necessary.

## Consequences for the slices

1. K1: one mutual block for N7. The indexed form `Gov (p : GPolicy)`
   refuses with `DOMAIN_QIIT`, so the governance block is mutual
   (`docs/PLAN.md:388-391`; section 7).
2. An entry takes and gives `Nat` or `Flag` only. Thus `prove` gives two
   entries, and the verification maps its `Bool` to a `Flag` (`boolFlag`)
   (`docs/STATUS.md:159-161`; section 5).
3. The IR has no closures. A run-time closure refuses with
   `REFUSE_RUNTIME_CLOSURE` (`docs/host/CAPABILITY.md:86-88`).
4. A tree walk at run time refuses with `REFUSE_RUNTIME_TREE`. A closed
   policy with an open private input reduces by iota first, so
   `examples/governance.lang` lowers and builds
   (`docs/host/CAPABILITY.md:89-92`, `docs/host/CAPABILITY.md:116-123`).
5. Paths and proofs erase, or the program refuses with
   `REFUSE_RUNTIME_PATH` or `REFUSE_RUNTIME_PROOF`
   (`formers/tcc-wasm.md:57-59`, `docs/host/CAPABILITY.md:109-115`).
6. `Nat` is a 64-bit word with explicit carry tests
   (`docs/host/CAPABILITY.md:17-19`; section 4).
