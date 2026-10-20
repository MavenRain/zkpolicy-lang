# zkpolicy-lang specification (draft)

Status: draft before milestone M0. `zkpolicy-lang` is a working name.

Fill each section. The quoted line under each heading says what to write
there. Delete the quoted line when the section is complete.

## 1. Purpose

> Write here: what one program describes, what the compiler checks, and what
> it writes. Name the host and the target. Say that the core types and core
> operations are only those of `design/DESIGN.md`.

zkpolicy-lang is a language for <the domain>. Its type formers are F1 to F15 of
`formers/FORMERS.md`. Its core data types and core operations are only the
types and operations of `design/DESIGN.md` (the design).

## 2. Programs

> Write here: the form of a program and the refusal list. Copy the host
> forms from `formers/tcc-wasm.md`. Add each fixed first definition that the
> domain needs (for example a size parameter).

A program is a sequence of definitions:

```
def NAME : TYPE := TERM
```

The compiler refuses these host forms in a program: <the list from
`formers/tcc-wasm.md`>. It also refuses a definition that uses a core name or
a prelude name. Thus a program cannot add a data type, an unproved fact or
general recursion. Recursion comes only from `fold` and `unfold` (F6, F7).
The compiler writes the target; a program cannot.

## 3. Type formers

> Write here: only the deviations from the formers. Do not copy the forms.
> Give each former whose status in `formers/tcc-wasm.md` is not DONE, and the
> open item (section 9) that records it.

The type formers are F1 to F15 of `formers/FORMERS.md`. This language uses
the tcc-wasm column of the realization matrix. `formers/tcc-wasm.md` gives
the host form of each former.

| ID | Status on tcc-wasm | Effect on this language | Open item |
|---|---|---|---|
| | | | |

## 4. Structures

> Write here: the carriers of Monad (F5), fold and unfold (F6, F7) and
> filter (F8) that the design uses. Name each carrier that the host does not
> give.

- **Monad**: `pure`, `map`, `bind` on <carriers>.
- **Algebra**: `fold` on <carriers>. `unfold` on <carriers>, or the reason
  that there is no `unfold`.
- **Filterable**: `filter` on <carriers>.

## 5. Core types

> Write here (DOMAIN SLOT): only the types of `design/DESIGN.md`. Give each
> type its meaning in the design and its definition in host forms. Mark each
> definition that a probe fact forces with "probe-forced".

| Type | Meaning (design section) | Definition |
|---|---|---|
| | | |

## 6. Core operations

> Write here (DOMAIN SLOT): only the operations of `design/DESIGN.md`. Give
> each operation its type and its meaning in the design.

| Operation | Type | Meaning (design section) |
|---|---|---|
| | | |

## 7. Target and instance encoding

> Write here: what the compiler writes. For a JSON target, give the instance
> rule and the encoding table. For a contract target, give the storage, the
> entries and the guards. Say how each proof erases.

An instance is a top-level definition whose type is a data type (F14).

| Type | Encoding |
|---|---|
| | |

## 8. Host and target

> Write here: the host, its pinned commit (`PIN`), the facts from
> `probe/CAPABILITY.md` that set the design, and the check that the compiler
> runs on its output (for example the axiom report).

## 9. Open items

> Write here: one item for each open question. Number the items O1, O2 and
> so on. Mark each decision as "RULED YYYY-MM-DD (USER): <the ruling>". Mark
> each fact that a probe found as "Probe-forced".

- O1. <question>.

## 10. Milestones

> Write here: the content of each milestone, and a dated status line under
> the current milestone.

| Milestone | Content |
|---|---|
| M0 | Host probe (`probe/CAPABILITY.md`); the core types construct and check; refusal list; examples and tests |
| M1 | Core operations |
| M2 | Queries and reads, or the contract |
| M3 | Hardening: speed, limits, a checked certificate for the output |

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
Thus `Gov -> Gov` and `List GLayer` refuse with `DOMAIN_FAMILY`.

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

Five laws check without induction over the block (ruling R8, 2026-10-19):

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
  sig needs `gqCompose` (finding F1, ruling R9). P11b adds `gqCompose`
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
