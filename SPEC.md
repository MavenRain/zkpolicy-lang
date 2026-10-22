# zkpolicy-lang specification

Status (2026-10-22): slices P0 to P9c, P11, P11b and P12 are DONE. P9b
closes at s6c. P9c (the PIN move to zkml-lang 677478b and the attention
writer mode `--attention`) closes at s2. P9d s1 (the lowering shares a
lowered value in a block) is DONE. Slice P10 is open.
`docs/PLAN.md` section 5 gives each slice.

## 1. Purpose

zkpolicy-lang is a language for agent policies and small-model inference,
where a zero-knowledge proof shows that a decision obeys its policy. A
program is a sequence of definitions over the policy core. The compiler
checks each definition and refuses the host forms of section 2. It writes
a Wasm edge module (`langc build`) and a zkml-lang Circuit IR source file
(`langc zkml`). An EVM verifier is slice P10, which is open. The host is
TinyCC 0.9.28rc with the tcc-wasm kit. The type formers are F1 to F15 of
`formers/FORMERS.md`. The core data types and core operations are only the
types and operations of `design/DESIGN.md` (the design).

Source: `README.md`, `docs/PLAN.md` section 5, `PIN`.

## 2. Programs

A program is a sequence of definitions:

```
def NAME : TYPE := TERM
```

The compiler refuses these host forms in a program
(`formers/tcc-wasm.md:110`, `test/check/expect.txt`):

- a family declaration (rule R1, `REFUSE_DATA`, `test/check/data.lang`);
- an `axiom` (rule R2, `REFUSE_AXIOM`, `test/check/axiom.lang`);
- a recursive definition or a self-reference (rule R3, `REFUSE_REC`,
  `test/check/rec.lang`);
- a definition that uses a core name or a prelude name (rule R4,
  `REFUSE_NAME`, `test/check/core-name.lang`).

Thus a program cannot add a data type, an unproved fact or general
recursion. Recursion comes only from `fold` and `unfold` (F6, F7). The
compiler writes the target; a program cannot.

The lowering to a target also refuses the run-time forms that the host
cannot give (HOST-LIMIT in `formers/tcc-wasm.md`):

- a fold over a tree that exists only at run time: `REFUSE_RUNTIME_TREE`
  (`formers/tcc-wasm.md:37-38`);
- a function value: `REFUSE_RUNTIME_CLOSURE` (`formers/tcc-wasm.md:43-44`);
- an interval, path or PathP value: `REFUSE_RUNTIME_PATH`
  (`formers/tcc-wasm.md:59`);
- an unreduced `transp`: `REFUSE_RUNTIME_TRANSP` (`formers/tcc-wasm.md:70`);
- a Glue type, `glue` or `unglue`: `REFUSE_RUNTIME_GLUE`
  (`formers/tcc-wasm.md:101-102`);
- a fold into a proposition: `REFUSE_RUNTIME_PROOF`
  (`formers/tcc-wasm.md:107`).

Source: `formers/tcc-wasm.md`, `test/check/expect.txt`.

## 3. Type formers

The type formers are F1 to F15 of `formers/FORMERS.md`. This language uses
the tcc-wasm column of the realization matrix. `formers/tcc-wasm.md` gives
the host form of each former. The table gives each former that is not
plain DONE.

| ID | Status on tcc-wasm | Effect on this language | Open item |
|---|---|---|---|
| F5 | DONE over Option, Sum E and List (`formers/tcc-wasm.md:18`) | `pure`, `map` and `bind` work on these three carriers only | none |
| F6 | DONE over Nat, List and domain families; HOST-LIMIT: a run-time tree (`formers/tcc-wasm.md:19`) | A fold over a tree that exists only at run time refuses with `REFUSE_RUNTIME_TREE` | none (host limit) |
| F7 | DONE into List with a Nat step limit (`formers/tcc-wasm.md:20`) | `unfold` has a step limit and writes only a List | none |
| F8 | DONE over Option and List (`formers/tcc-wasm.md:21`) | `filter` has two carriers | none |
| F9 | DONE; HOST-LIMIT: a function value at run time (`formers/tcc-wasm.md:22`) | A function value at run time refuses with `REFUSE_RUNTIME_CLOSURE` | none (host limit) |
| F10 | DONE (`formers/tcc-wasm.md:23`) | A dependent Pi checks its domain and codomain as types; domains include `Flag` as well as `Type 0` and `Nat` (`src/front/check.c:3070-3081`) | none |
| F14 | DONE for Type 0 and Type 1, without cumulativity (`formers/tcc-wasm.md:27`) | Two universes, and no cumulativity | none |
| F15 | PARTIAL: only the domain file declares an indexed family (`formers/tcc-wasm.md:28`) | A program cannot declare a family (rule R1) | none (rule R1) |

F1 to F4 and F11 to F13 are plain DONE. The cubical parts C1 and C2 are
DONE, and C3 to C5 are DONE with the limits in `formers/tcc-wasm.md:31`
to `formers/tcc-wasm.md:33`.

Source: `formers/tcc-wasm.md` (former and carrier tables).

## 4. Structures

- **Monad**: `pure`, `map`, `bind` on Option, Sum E and List (F5,
  `formers/tcc-wasm.md:18`).
- **Algebra**: `fold` on Nat and List (each with a motive form, slices
  P7b-nat and P7b-list), on the domain families, on the HITs (the
  eliminator, slice P3b, C4), and on indexed sorts and mutual blocks with
  one motive for each sort (slice P4b, C5; block induction at P11b)
  (`formers/tcc-wasm.md:19`, `formers/tcc-wasm.md:32`,
  `formers/tcc-wasm.md:33`). A fold over Circle, Trunc or Quot into a
  proposition follows P7b-b ruling R2 (`formers/tcc-wasm.md:106`). At run
  time it refuses with `REFUSE_RUNTIME_PROOF` (`formers/tcc-wasm.md:107`).
  `unfold` writes into List with a Nat step limit (F7,
  `formers/tcc-wasm.md:20`).
- **Filterable**: `filter` on Option and List (F8,
  `formers/tcc-wasm.md:21`).

Source: `formers/tcc-wasm.md` rows F5 to F8, C4 and C5.

## 5. Core types

The core types are in `domain/domain.lang` (slice P6). The N numbers are
the notes on the fragment in `docs/PLAN.md`.

| Type | Meaning (note) | Definition |
|---|---|---|
| `Visibility` | The two visibilities of a field (N10) | `family Visibility := public \| private` (`domain/domain.lang:27`) |
| `Bool` | The coproduct Unit + Unit. It is not a new base type (N9) | `def Bool : Type 0 := Sum Unit Unit` (`domain/domain.lang:30`) |
| `Ctx` | The context over the Field component: a public part and a private part | `fun F => Prod (F public) (F private)` (`domain/domain.lang:35`) |
| `PolicySig` | The signature (N1, N3) | A `Type 1` record of the fields in section 6 (`domain/domain.lang:43`) |
| `PolicyEquiv` | Two policies are equivalent when they give the same decision on each context | `(c : Ctx (Field S)) -> Path (Decision S) (eval S p c) (eval S q c)` (`domain/domain.lang:76`) |
| `Equiv` | Equivalence of types from Pi, Sigma and Path only (N9) | `Sigma (f : A -> B) (isEquiv A B f)` (`domain/domain.lang:86`) |
| `PolicyLaws` | The laws of a signature | `composeAssoc`, `evalCompose`, `soundness`, `soundnessPub`, `privacy`, `completeness`, `SipPolicy` (`domain/domain.lang:173`) |
| `PolicyModel` | A model: a signature and a proof of each law | `Sigma (S : PolicySig) (PolicyLaws S)` (`domain/domain.lang:183`) |

Source: `domain/domain.lang` and its comments.

## 6. Core operations

The operations are the fields of `PolicySig` (`domain/domain.lang:44` to
`domain/domain.lang:53`). Its carrier fields are `Field`, `Decision`,
`Policy`, `Transcript` and `ZKProof`.

| Operation | Type | Meaning |
|---|---|---|
| `eval` | `Policy -> Ctx Field -> Decision` | The decision of a policy on a context |
| `prove` | `(p : Policy) -> (c : Ctx Field) -> Sigma (t : Transcript) (ZKProof p (pub Field c) t)` | A transcript and a proof over the public part |
| `compose` | `Policy -> Policy -> Policy` | The composition of two policies |
| `restrict` | `Policy -> Policy` | The restriction of a policy |
| `verify` | `(p : Policy) -> (x : Field public) -> (t : Transcript) -> ZKProof p x t -> Bool` | The verification from the public part (accessor at `domain/domain.lang:72`) |

Source: `domain/domain.lang`.

## 7. Target and instance encoding

The compiler writes three outputs (`src/main.c:47`).

| Output | Encoding |
|---|---|
| Wasm module: `langc build PROG -o OUT [--runtime]` | Each entry parameter and result is `Nat` or `Flag` (`docs/STATUS.md:159`). The verification maps its `Bool` to a `Flag` (`boolFlag`). Example: `examples/edge.lang`. |
| IR and interface: `langc ir PROG`, `langc abi PROG` | The IR is first-order: no calls, no function values and no stack of its own (`docs/host/CAPABILITY.md:81`). |
| zkml-lang source: `langc zkml PROG NAME [--residual \| --residual-loop=K \| --lookup=TABLE \| --softmax=TABLE \| --layernorm=GB,TABLE \| --attention=WK,WV,TABLE] -o OUT` | Definitions named NAME with the suffixes S, T, Rd, Circ, Enc, Wit, RunE and WitE (`src/zkml.c:16`). Every mode also generates NAMESat (`src/zkml.c:569-582`). Dense, lookup, softmax and layer normalization modes prove it by `refl`. Residual, residual-loop and attention modes prove it by the inverse of the domain lemma `stageDecEnc` at NAMET (`src/zkml.c:437-461`). The attention mode limits TABLE by model width as below, because the PIN check of its NAMESat stops at the arena limit. |

Attention requires square query, key and value matrices of width n, with
2 n <= 16. Its supported exp table sizes at the PIN are:

| Model width n | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| Maximum TABLE rows | 8 | 8 | 5 | 4 | 3 | 2 | 1 | 1 |

A larger table gives `ZKML_MODEL` before the output file is opened.

A proof erases. A definition of a `PathP` type is a proof
(`formers/tcc-wasm.md:57`). A fold with a value-dependent motive is a
proof with no run-time value, so it refuses with `REFUSE_RUNTIME_PROOF`
(`src/front/lower.c:593`). The P11b block fold refuses the same way
(`src/front/lower.c:1358`).

The lowering keeps each lowered value in its block. A later use of the
same value in that block, or in a block inside it, reads the same local.
The two arms of a branch do not share values (flags 51 to 55 of
zkml-lang 4fadeb6, the test `test/ir/share.lang`).

Source: `src/main.c`, `src/zkml.c`, `docs/STATUS.md`,
`docs/host/CAPABILITY.md`, `formers/tcc-wasm.md`.

## 8. Host and target

- Host: TinyCC 0.9.28rc (`PIN` line 1) with the tcc-wasm kit. It builds
  `build/langc` with `-std=c99 -Wall -Werror`
  (`docs/host/CAPABILITY.md:10`). The gate also uses node and wabt 1.0.39
  (`wasm-validate`, `wasm-objdump`), with the versions in `PIN` lines 3 to
  5.
- Target pin: zkml-lang commit 677478b (tree 19607ac1), `PIN` lines 8 and
  9.
- The host facts that set the design (`probe/CAPABILITY.md`,
  `docs/host/CAPABILITY.md`): `Nat` is an unsigned 64-bit word, and
  `natAdd` and `natMul` trap on overflow (line 29). Each Wasm value is a
  word and a trap flag (line 33). The checker and the evaluator stop at
  depth 2000 (line 52), and the evaluator fuel is 20000000 steps (line 56).
  The arena limit is 2 GiB (line 61). The IR is first-order (line 81).
- Output checks: `make check` runs each Wasm module against `langc eval`
  (the differential grid). `make zk-check` builds zkml-lang at the PIN and
  checks each `langc zkml` output with zkml-lang `langc check` and
  `langc eval` (43 rows at a778619). There is no axiom report: the
  compiler refuses `axiom` (rule R2, `probe/CAPABILITY.md` section 10).

Source: `PIN`, `docs/host/CAPABILITY.md`, `probe/CAPABILITY.md`,
`docs/STATUS.md`.

## 9. Open items

- O1. The source of the cubical core: zkml-lang patches, or lang-template
  first. Default (not ruled).
- O2. The `verify` hole (N3): `ZKProof` takes `Field public`. RULED
  2026-10-15 (USER): the default.
- O3. The `eval-compose` hole (N2): a Sigma witness, a `priv` choice or a
  congruence law. RULED 2026-10-15 (USER): the default.
- O4. `soundness` (N4): keep the law, and add `completeness` and
  `soundness-pub`. RULED 2026-10-15 (USER): the default.
- O5. Glue types and univalence (C6): in or out. RULED 2026-10-15 (USER):
  in.
- O6. The EVM verifier route: the zkml Halo2/KZG snark-verifier, or a
  tcc-evm writer. RULED 2026-10-15 (USER): the default.
- O7. DAO recursion (N7): one mutual block with eval by recursion, or a
  coalgebra. RULED 2026-10-19 (USER): the default (P11 rulings R1 and R2).
- O8. Laws that are not in the design (N11). Default: none before P7 (not
  ruled).
- O9. `Policy₀` (N6): a list of layers, or the free algebra HIT. Default
  (not ruled).
- Probe-forced: K1, one mutual block for N7. The indexed form refuses with
  `DOMAIN_QIIT` (`probe/CAPABILITY.md`, Consequences item 1).

Source: `docs/PLAN.md` section 6.

## 10. Milestones

The milestones are the slices of `docs/PLAN.md` section 5.

| Slice | Content | Status |
|---|---|---|
| P0 | The tcc-wasm kit with the formers F1 to F15, the design, the plan | DONE (the scaffold) |
| P1a | C1 without faces, C2 | DONE |
| P1b | Faces, partial elements, systems | DONE |
| P2a | `transp`, and `transport` over a path | DONE |
| P2b-1 | `hcomp` with checked sides | DONE |
| P2b-2 | The rest of C3: `comp`, `fill` | DONE |
| P3a | C4 HIT declarations | DONE 2026-10-16 |
| P3b | C4 eliminator, `soundness-pub` | DONE 2026-10-16 |
| P4a | C5 indexed sorts, motive fold | DONE 2026-10-16 |
| P4b | C5 mutual blocks, block fold | DONE 2026-10-16 |
| P5a | C6 Glue, `glue`, `unglue`, `ua` | DONE 2026-10-15 |
| P5b | Kan rules for Glue, uaβ | DONE 2026-10-15 |
| P5c | hPropExt, set-quotient effectiveness, C7 tests | DONE 2026-10-16 |
| P6 | The domain core: `Visibility`, `Bool`, `PolicyModel` | DONE (before P1b, USER ruling 2026-10-15) |
| P7a | The reference signature `refSig`, soundness and privacy proofs | DONE |
| P7b-nat | Nat induction (the motive `Nat` fold) | DONE 2026-10-16 |
| P7b-list | List induction | DONE 2026-10-16 |
| P7b-a | `composeAssoc`, `evalCompose` and completeness proofs | DONE 2026-10-16 |
| P7b-b | The R2 rule, the quotient signature `refQSig` | DONE 2026-10-17; the port of the rule to zkml-lang is open |
| P8 | The Wasm edge runtime for the reference `eval` and `prove` | DONE 2026-10-17 |
| P9a | `SmallModel` and `ZkmlCircuit` at the zkml-lang PIN | DONE 2026-10-18 |
| P9b | Operator circuits, the `langc zkml` writer | s1 to s5 DONE 2026-10-18 to 2026-10-19, s6a DONE 2026-10-20, s6b (the softmax circuit) DONE 2026-10-21, s6c (the layer normalization circuit) DONE 2026-10-21; CLOSED 2026-10-21 at s6c |
| P9c | The attention circuit, the PIN move to zkml-lang 677478b | s1 (the PIN move) DONE 2026-10-21; s2 (`--attention`) DONE 2026-10-22; CLOSED 2026-10-22 at s2 (ruling R28) |
| P10 | The EVM verifier | Not started (it needs zkml-lang Z9) |
| P11 | The governance instance (N7) | DONE 2026-10-19 |
| P11b | Block induction, the governance laws, `govModel` | DONE 2026-10-20 |
| P12 | The final docs and the final gate | DONE 2026-10-20 |

Status 2026-10-22: P9b is closed at s6c and P9c is closed at s2. The
repository closes at P11b. P9d s1 (the lowering shares a lowered value
in a block) is DONE. Slice P10 stays open.

Source: `docs/PLAN.md` section 5, `docs/STATUS.md`.

## 11. The governance instance (N7)

Slice P11 adds the governance instance to `domain/domain.lang`, after
`refModel`. It is one mutual block (C5) of three sorts:

```
family GPolicy := gpNil | gpCons (gpHead : GLayer) (gpTail : GPolicy)
  and GLayer := gConst (gAmount : Nat) | gSeal | gNest (gCtx : Gov)
  and Gov := gov (govPub : GPolicy) (govPriv : Nat)
```

`GPolicy` is the policy, a list of layers (N6, O9). `GLayer` is a constant
layer, a seal or a nested governance context. `Gov` is the governance
context. Its public part is a `GPolicy` and its private part is a `Nat`.
The policy in a context is a strict part of that context, as N7 asks. The
block is strictly positive by the schema: the kernel accepts a field only
as an exact sort of the block or as a plain type with no block sort in it.
Thus `Gov -> Gov` and `List GLayer` refuse with `DOMAIN_FAMILY`
(`src/front/check.c:3391`, `src/front/check.c:3351`). A P12 probe on a
copy of the domain confirmed both refusals (2026-10-20).

The carrier is `gField v = fold GPolicy Nat v`. Thus `Ctx gField` is
`Prod GPolicy Nat`, and `gov (pub gField c) (priv gField c)` packs a
context into the sort.

`eval` is one block fold (O7) with the motives `Nat -> Nat` for `GPolicy`,
`Nat -> Nat` for `GLayer` and `Nat` for `Gov`. The cases, in constructor
order:

| Constructor | Case | Meaning |
|---|---|---|
| `gpNil` | `fun y => y` | the empty policy keeps the running value |
| `gpCons h t` | `fun y => t (h y)` | the head runs first, then the tail |
| `gConst n` | `fun y => refAdd y n` | add a constant (saturating, as `Ref`) |
| `gSeal` | `fun y => 0` | the private value is sealed |
| `gNest d` | `fun y => refAdd y d` | add the decision of the nested context |
| `gov f y` | `f y` | the decision of a context is its policy on its private part |

`gRun p` and `govDec g` are this fold at a policy and at a context.
`gEval p c = govDec (gov (pub gField c) (gRun p (priv gField c)))`. The
fold descends through `gNest` into the nested context and through `gov`
into its policy. Thus the recursion is on the nesting of the context and
not on the policy alone.

The signature `gSig : PolicySig` packs `gField`, `Nat` (the transcript),
`GPolicy`, `Nat` (the proof is the private part), `gProof`, `gEval`,
`gProve`, `gCompose`, `gRestrict` and `gVerify`. `gCompose` is append: a
block fold with motive `GPolicy -> GPolicy` at the policy sort. Its
`gpNil` case is the identity function. The outer tail receives the second
policy; a nested `gov` rebuilds its policy at `gpNil`, preserving the
nested context instead of appending into it.
`gRestrict p = gpCons gSeal p`. `gVerify p x t y` runs `p` on
`y`, then runs the public policy `x` on that result and compares with `t`.
The public policy runs last so that the intermediate private value after
`p` can witness `EvalCompose`. If the public policy ran first, a public
seal with `p = gpCons (gConst 1) gpNil` and `q = gpNil` would give 1 on
the composed side and 0 for every witness on the other side.

The quotient of N5 comes after the definition. `GQ` is
`Quot GPolicy (PolicyEquiv gSig)`, `gqin` is `quotIn`, and
`gqEval : GQ -> Ctx gField -> Nat` is `quotRec` at `gEval` with
`isSetCtxG`. No inductive-recursive definition is necessary (O7).

Five laws check without induction over the block (P11 ruling R8, 2026-10-19):

- `gSoundness : Soundness gSig` by `refl`, and `gPrivacy : Privacy gSig`
  by congruence along the path between the public policies. The seal
  makes `eval (restrict p) c` reduce to
  `govDec (gov (pub gField c) (gRun p 0))` for an open `p`.
- `gCompleteness : Completeness gSig` by `natEqRefl`.
- `gSoundnessPub : SoundnessPub gSig` by `truncIn` and `natEqSound`.
- `gqSip : SipPolicy gQSig` by `propEquiv` and `gqTo`, two nested folds
  over the quotient. `GQEquiv z w` is
  `(c : Ctx gField) -> Path Nat (gqEval z c) (gqEval w c)`, the body of
  `PolicyEquiv gQSig`. P11 stated the law on `GQ`, because the quotient
  sig needs `gqCompose` (P11 finding F1, P11 ruling R9). P11b adds `gqCompose`
  and `gQSig`, and states the law as `SipPolicy gQSig`.

Limit K1: the indexed form `Gov (p : GPolicy)` refuses with `DOMAIN_QIIT`
(a sort indexed by a sort of its block). Thus the instance is one mutual
block and not an inductive-inductive definition. That is the N7 shape
that the kernel accepts.

Block induction (P11b). A fold over a mutual block can have
value-dependent motives: the motive of the sort `j` takes a value of `j`.
The case of a constructor takes its fields, then one induction
hypothesis for each field whose type is a sort of the block. The type of
the case is the motive at the constructor value. Iota gives the fields,
then the folds at the block-sort fields. Every motive of the fold must
be value-dependent, every sort must be unindexed, and the block must not
have a path constructor. Otherwise, the fold refuses with `TYPE_MISMATCH`.
The fold is a proof. When it
must exist at run time, the lowering refuses with `REFUSE_RUNTIME_PROOF`.

`gCompose` is a block fold, so the head of a cons is `gCompL h`, the same
fold at the layer sort. `gCompG` is the fold at the context sort.
`gCompIdP` and `gCompIdL` are one dependent block fold with three
motives. They show that `gCompose z gpNil` is `z` and that `gCompL` and
`gCompG` are identities. The cons case of each law uses them:

- `gComposeAssoc : ComposeAssoc gSig` by a fold over the first policy.
- `gEvalCompose : EvalCompose gSig` through `gRunCompose`, a path from
  `gRun (gCompose p q) y` to `gRun q (gRun p y)`. The witness is
  `gRun p (priv gField c)`, the private value after `p`.

`gqCompose : GQ -> GQ -> GQ` is `quotRec` into `GQ -> GQ`, with
`isSetGQFun`. `gqRespR` and `gqRespL` show that `gCompose` respects
`PolicyEquiv gSig` on each side. `gqComposeR p` is `gqin (gCompose p q)`
on the second policy, and `gqComposeGlue` is its path on the first.
`gQSig : PolicySig` packs `gField`, `Nat`, `GQ`, `Nat`, `gqProof`,
`gqEval`, `gqProve`, `gqCompose`, `gqRestrict` and `gqVerify`. The six
quotient laws are `gqComposeAssoc`, `gqEvalCompose`, `gqSoundness`,
`gqSoundnessPub`, `gqPrivacy` and `gqCompleteness`. The witness of
`gqEvalCompose` is `gqEval x (pair gpNil (priv gField c))`, because
`govDec (gov gpNil w)` is `w`. With `gqSip`, `gQSig` has the seven laws,
and `govModel : PolicyModel` packs them.

`examples/governance.lang` checks the closed instances `q7assoc`,
`q7evalc` and `q7evalcSig` by `refl`. Its new eval rows are `q7n` 20,
`q7m` 20 and `gqComp4` 36.
