# Domain slot

Replace the sample domain with the design of your language.
`domain/domain.lang` holds the domain. At build time, `tcc -run
gen/embed.c` copies it into `build/domain.c`, so the domain is part of
`build/langc`. `langc` loads the domain first, then the program.

## What the file holds

- `family` declarations. Only this file can declare a family. In a
  program, `family` is refused with `REFUSE_DATA` (rule R1).
- `def` operations, written in the language.
- Line comments that start with `--`.

Each family, constructor, field and definition name in this file becomes
a core name. A program cannot use one of them as a new definition name
(rule R4, `REFUSE_NAME`).

## Family syntax

```
family NAME (param : T)* := ctor (field : T)* | ctor (field : T)* ...
```

- A family with only nullary constructors is an enum.
- A family with one constructor is a record. It gets one projection for
  each field name.
- A field can have the type of its own family. Such a field is recursive.
- A parameter is an index (former F15). A constructor takes the indices
  first: `makeLot 3 250 : Lot 3`.
- A `fold` over a family takes one argument for each constructor, in
  declaration order. A constant constructor takes a value. A constructor
  with fields takes a function. A recursive field arrives folded.

In the IR, a constructor value is a heap block. Word 0 is the constructor
index, in declaration order. Each field has a word and a trap flag.
A `fold` over a family with one recursive field lowers to a loop.
A family with two or more recursive fields cannot be folded at run time
(`REFUSE_RUNTIME_TREE`, see `docs/CAPABILITY.md`).

## The sample

| Family | Kind | Use |
|---|---|---|
| `Color` | enum: `red`, `green`, `blue` | `isRed`, `colorCode` in `examples/algebra.lang` |
| `Item` | record: `itemColor`, `itemPrice`, `itemCount` | `lineTotal`, `total`, `redTotal` |
| `Stack` | one recursive field, `rest` | `depth`, and the spine loop in `test/ir/spine.lang` |
| `Tree` | two recursive fields | the tree mutant `test/ir/tree.lang` |
| `Lot (size : Nat)` | indexed record | `examples/inventory.lang`, `examples/laws.lang` |

The definitions are `lineTotal`, `isRed`, `total`, `redTotal` and `depth`.

`family Tree` exists only for the tree mutant. The mutant checks that the
lowering refuses a fold over a run-time tree. Rule R1 lets only this file
declare a family, so the family must be here.

## The policy core (P6)

The policy core comes after the sample, in the same file. The sample
stays until P8 or P12, because the gate eval and the differential cases
use it.

| Name | Kind | Use |
|---|---|---|
| `Visibility` | enum: `public`, `private` (N10) | the index of `Field` |
| `Bool`, `true`, `false` | `Sum Unit Unit` (N9) | the result of `verify` |
| `Ctx`, `pub`, `priv` | `Prod (F public) (F private)` | the context over a `Field` |
| `PolicySig` | nested Sigma in `Type 1` | `Field`, `Decision`, `Policy`, `Transcript`, `ZKProof`, `eval`, `prove`, `compose`, `restrict`, `verify` |
| `PolicyEquiv` | Pi of paths | two policies with the same decision on each context |
| `isContr`, `fiber`, `isEquiv`, `Equiv` | Pi, Sigma and Path only | the type of `sipPolicy` |
| `ComposeAssoc`, `EvalCompose`, `Soundness`, `SoundnessPub`, `Privacy`, `Completeness`, `SipPolicy` | one statement for each law; the C4 families `Circle`, `Trunc` and `Quot` come before the laws, because `SoundnessPub` uses `Trunc` (rule R3) | the fields of `PolicyLaws` |
| `PolicyModel` | `Sigma (S : PolicySig) (PolicyLaws S)` | an instance (P7 and later) |

Each field has one accessor: `sig` and `laws`, the ten signature
accessors (they take a `PolicySig`), and `composeAssoc`, `evalCompose`,
`soundness`, `soundnessPub`, `privacy`, `completeness` and `sipPolicy` (they take a
`PolicyModel`). The lemmas are `pequivRefl`, `pequivSymm`,
`pequivTrans` and `privacySamePub`. `pequivTrans` uses `trans` over a
path (C3). `examples/policy.lang` uses each of them. The law
`soundness-pub` is `SoundnessPub` (P3b): its conclusion is in the
propositional truncation `Trunc`.

## The Ref instance (P7a)

`Ref` comes after the policy core, in the same file. It uses the formers
only. Its one new family is `Layer`.

| Field | Value in `refSig` |
|---|---|
| `Field` | `refField`: a fold over `Visibility` into `Type 0`. Both parts are `Nat`. |
| `Decision` | `Nat`: the private part at the end of the run. `isZero` tests acceptance. |
| `Policy` | `List Layer`. A layer changes the private part. `refRun` applies the layers from the head to the end. |
| `Transcript` | `Nat`: the private part at the end of the run |
| `ZKProof` | `refProof`: `Nat`, the private part (N4). It holds no path. |
| `eval` | `refEval`: the resulting private part |
| `prove` | `refProve`: the transcript and the private part |
| `compose` | `refCompose`: append (N6) |
| `restrict` | `refRestrict`: puts `sealPrivate` first, so the policy runs on a private part of 0 |
| `verify` | `refVerify`: runs the policy on the certificate and compares the result with the transcript by `natEq` |

The decision retains the full state so that `compose` and `prove` can
respect `PolicyEquiv` when P7b takes the quotient. A Boolean decision
would equate the policies `[sealPrivate, addConst 1]` and
`[sealPrivate, addConst 2]`, since both reject every context. Appending
`subPublic` distinguishes them at public part 1, and their transcripts
are already different. The `Nat` decisions distinguish these policies.

The host represents `Nat` up to 18446744073709551615. `addPublic` and
`addConst` use `refAdd`, which saturates at this bound using truncated
subtraction. This keeps the layers total: an honest proof at the bound
still verifies, rather than comparing an overflow trap with a transcript.
`examples/ref.lang` checks both addition layers at the bound.

P7a proves `Soundness` (`refSoundness`) and `Privacy` (`refPrivacy`).
`ComposeAssoc`, `EvalCompose` and `Completeness` need induction on `List`
and `Nat`, and the kernel has no dependent eliminator before C4. Thus
these proofs, the quotient `Policy` and `SipPolicy` come at P7b, and
there is no `PolicyModel` value for `Ref` yet. `examples/ref.lang`
computes `refSig` on closed contexts, uses each proved law, and checks
state equality through composition and proving. The check refusal
`ref-state-equivalence.lang` rejects equating the two distinct states.

## The ZkmlCircuit block (P9a)

The end of `domain/domain.lang` holds the zkml-lang circuit interface at
zkml-lang 7d0bb2d (rule R2), the BabyBear field operations and two list
helpers. The `langc zkml` writer imports matching declarations from the
pinned zkml domain and never copies a domain definition. A dependency
on a domain name without a matching declaration refuses with
`ZKML_MODEL`. When changing either domain or the PIN, regenerate
`src/zkml-names.h` with `build/zkml-names`; `make zk-check` verifies it.

- `Resource`, `zeroR`, `resAdd`, `Tensor`, `Inst`, `layerWit`, `ZLayer`
  and the identity layer `zIdL`: the zkml-lang circuit interface over the
  `Shape` and `Circuit` families. `ZLayer` is the zkml-lang family
  `Layer`, because the Ref layer has the name `Layer` (rule R4).
- `bbP`, `bbAdd`, `bbSub`, `bbMul` and `bbNeg`: BabyBear (zkml-lang Z6d),
  p = 2^31 - 2^27 + 1. A field value is a `Nat` below p. A product of two
  values is below 2^62, so `natMul` does not trap. `bbSub` reduces b
  first, so a + p - b does not stop at 0. The operations use the prim
  `natMod`.
- `listUncons` and `listZipWith`: the zkml-lang text, verbatim.
  `listUncons` gives the head and the tail of a nonempty list.
  `listZipWith` unfolds the two tails, bounded by the length of the first
  list, and stops at the end of the shorter list.

## How to replace the domain

1. Write your families and definitions in `domain/domain.lang`. Keep the
   names apart from the core names in `README.md`.
2. Change the examples and the tests that use the sample names:
   `examples/algebra.lang`, `examples/inventory.lang`,
   `examples/laws.lang`, `test/eval/expect.txt`,
   `test/check/domain-name.lang`, `test/ir/spine.lang` and
   `test/ir/tree.lang`.
3. Keep a family with two recursive fields for `test/ir/tree.lang`, or
   remove that file and its block in `test/gate.sh`.
4. Run `make check` in the kit directory. The build makes `build/domain.c`
   again when `domain/domain.lang` changes.
