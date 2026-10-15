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
| `ComposeAssoc`, `EvalCompose`, `Soundness`, `Privacy`, `Completeness`, `SipPolicy` | one statement for each law | the fields of `PolicyLaws` |
| `PolicyModel` | `Sigma (S : PolicySig) (PolicyLaws S)` | an instance (P7 and later) |

Each field has one accessor: `sig` and `laws`, the ten signature
accessors (they take a `PolicySig`), and `composeAssoc`, `evalCompose`,
`soundness`, `privacy`, `completeness` and `sipPolicy` (they take a
`PolicyModel`). The lemmas are `pequivRefl`, `pequivSymm` and
`privacySamePub`. `examples/policy.lang` uses each of them. The law
`soundness-pub` comes at P3, and the lemma `pequivTrans` comes at P2.

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
