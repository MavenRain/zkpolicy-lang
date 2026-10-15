# zkpolicy-lang plan

## 1. Rulings

- The language uses only the base types and operations of the Cubical Agda
  fragment in `design/DESIGN.md`. The laws of the fragment are a minimal
  list. The language can add laws about the same types and operations. It
  does not add a base type or a base operation.
- Type formers: products, coproducts, monads (map, bind), algebras and
  coalgebras (fold, unfold), quotients, paths, loops, HITs and QIITs, as in
  cubical type theory.
- Proof irrelevance is prohibited (section 3, C7).
- Host (USER 2026-10-15): TinyCC, with the tcc-wasm kit.
- Targets (USER 2026-10-15): the zkml-lang Circuit IR, a Wasm edge runtime
  and an EVM verifier.
- Tests: unit and integration tests are in the guest language. A host
  runner that loads and runs guest test files is permitted. Do not add a
  node, Python or shell harness.

## 2. Design choices (not ruled)

- Name: zkpolicy-lang, from the module name `ZKPolicyDSL`. The compiler
  binary is `build/langc`.
- Route: `bin/new-lang.sh zkpolicy-lang tcc-wasm` in lang-template. At P0,
  `src/`, `test/` and `Makefile` are the same as zkml-lang 57d5c92,
  because the two languages come from the same kit. The cubical formers C1
  to C5 come from zkml-lang as patches, one zkml-lang commit at a time
  (O1). P1 adds `docs/PORTS.md`, which records each source commit.
  lang-template does not change.
- Signature: the design gives no definition for `Field`, `Decision`,
  `Policy`, `Transcript`, `ZKProof`, `eval`, `prove`, `compose`,
  `restrict` and `verify`. They are the fields of one guest record,
  `PolicyModel`, and the laws are more fields of it. An instance is a value
  of `PolicyModel`. It gives each carrier as a type made from the formers,
  each operation as a definition and each law as a proof. `Visibility`,
  `PolicyEquiv` and `Bool` are definitions of the design. `Context` is a
  definition over the `Field` field.
- Instances: `Ref`, a small finite instance for the guest tests and the
  Wasm gate, and `SmallModel`, the small-model case of the design: a
  `Decision` is a quantized tensor, and a `Policy` is an architecture with
  weights. In both, `Policy` is a set quotient (N5), and a proof is a
  certificate that holds the private part (N4). Neither instance adds a
  base type. Each type is made from the formers.
- Erasure: the checker checks all terms, paths and proofs included. An
  exported entry has a first-order type. A `ZKProof` value is data, and it
  stays at run time. REFUSE_RUNTIME_CLOSURE stays, and REFUSE_RUNTIME_PATH
  comes with C2 from zkml-lang. Thus a certificate holds no path (N4).
- zkml-lang target: `ZkmlCircuit` is a guest type, a model of the
  zkml-lang Circuit IR. A C writer prints a `SmallModel` policy as a
  zkml-lang source file: a `Circuit` term and its witness. zkml-lang then
  makes the LLZK, Halo2 and Plonky3 output. `PIN` records the zkml-lang
  commit. The writer is in the trusted base.
- Wasm target: the kit Wasm writer. An entry for `eval`, or for the
  witness half of `prove`, compiles to a Wasm module for the edge device.
- EVM target: a verifier contract for the Halo2 output of the zkml-lang
  route (O6). A DAO contract calls it with the public part, the transcript
  and the proof.
- Identity type: Path. The kit formers F12 and F13 (refl, symm, trans,
  transport, cong) become derived definitions over Path, as in zkml-lang.

## 3. Formers

The kit has F1 to F15 (`formers/FORMERS.md`). The design also needs:

- C1 Interval: `I`, `i0`, `i1`, `~`, `∧`, `∨`, face formulas, partial
  elements and systems. From zkml-lang Z1.
- C2 PathP and Path: path abstraction, path application and the endpoint
  rules. A loop is a `Path x x`. From zkml-lang Z1.
- C3 Kan operations: `transp` and `hcomp` (with `comp` and `fill`) for
  each former. From zkml-lang Z2.
- C4 HITs: point constructors, path constructors and eliminators with path
  cases. Examples: the circle, the set quotient `A / R`, the propositional
  truncation and the set truncation. From zkml-lang Z3.
- C5 QIITs: the finitary schema of zkml-lang O8. From zkml-lang Z4.
- C6 Glue types and univalence (`ua`). The design names univalence for
  `sip-policy`, and the effectiveness proof of the set quotient in N5
  needs `hPropExt`. zkml-lang does not have C6 (its O3), so this
  repository builds it (O5).
- C7 No proof irrelevance. The checker has no `Prop` or `SProp` universe,
  no irrelevant arguments, no rule that makes two proofs judgmentally
  equal, and no axiom K or UIP. The propositional truncation and the set
  truncation are HITs (C4). They give paths between proofs, not judgmental
  equality. A check test refuses `refl` between two different values of
  one `ZKProof` type.

## 4. Notes on the fragment (defaults, not rulings)

The design stays verbatim. These notes record where a part of the fragment
does not check as written, and the default for each.

- N1 Placeholders: `Field _ = Type`, `Decision = Type`, `Policy = Type`,
  `Transcript = Type` and `ZKProof _ _ _ = Type` make each carrier the
  universe itself. Then a `Context` holds two types, and `eval` returns a
  type. Default: each is an abstract field of `PolicyModel` (section 2) in
  `Type`. Thus `Context`, `PolicyEquiv` and the values of `ZKProof` go down
  from `Type₁` to `Type`.
- N2 `eval-compose`: the private part `_` is a hole, and the comment says
  that the full law needs an update operation on `Context`. The design does
  not give that operation, and no operation can be added. Default: the hole
  is a proof-relevant witness:

  ```agda
  eval-compose : ∀ p q c → Σ[ x ∈ Field private ]
    eval (compose p q) c ≡ eval q (record { pub = Context.pub c ; priv = x })
  ```

  The public part goes through unchanged. The first projection gives the
  private part that `q` gets, for example the output of `p` as a hidden
  value. Other options are in O3.
- N3 `verify`: the hole `priv = _` does not check, because the verifier
  has no private part. Default: `ZKProof` takes the public part, not the
  context: `ZKProof : Policy → Field public → Transcript → Type`. Then
  `prove p c : Σ[ t ∈ Transcript ] ZKProof p (Context.pub c) t`, and
  `verify` checks with no hole. This is the usual split into a statement
  (policy, public part, transcript) and a witness (the private part). It
  changes the type of a base type, so a USER ruling is necessary before P6
  (O2).
- N4 `soundness`: the stated path is `refl` on `eval p c`, so each
  instance satisfies it and it gives no information. The comment says that
  the real law is dependent, but the design has no operation from
  `Transcript` to `Decision`. Default, with N3: keep the stated law and add
  two laws:

  ```agda
  completeness : ∀ p c →
    verify p (Context.pub c) (fst (prove p c)) (snd (prove p c)) ≡ true
  soundness-pub : ∀ p x t (π : ZKProof p x t) → verify p x t π ≡ true →
    ∥ Σ[ y ∈ Field private ]
        fst (prove p (record { pub = x ; priv = y })) ≡ t ∥₁
  ```

  The truncation is necessary. Without it, the law is a function from an
  accepted proof to the private part, and an instance with zero knowledge
  cannot give that function. The truncation is a HIT, so this is not proof
  irrelevance (C7). In `Ref` and `SmallModel`, the certificate is the
  private part (data, no path), and `verify` compares transcripts with a
  decidable equality that each instance defines. These instances are not
  zero knowledge. Zero knowledge comes from the backend proof that
  realizes the certificate (N8).
- N5 `sip-policy`: with funext, `PolicyEquiv p q ≃ (p ≡ q)` says that
  `eval` is an embedding of `Policy` into `Context → Decision`. An
  architecture with weights is not such a type, because two sets of
  weights can give one function. Default: in each instance, `Policy` is a
  set quotient `Policy₀ / PolicyEquiv₀`, and `sip-policy` comes from the
  effectiveness of the quotient (C4, C6). When `Decision` is a set,
  `PolicyEquiv` is a proposition and `Policy` is a set. The writers print
  the representative, so a printed policy is one member of its class.
- N6 Higher structure: the design names 2-paths for the composition of
  proofs, and a HIT for the free algebra of policies. With N5, `Policy` is
  a set, so each 2-path between two proofs of `compose-assoc` is trivial,
  and the pentagon holds. Default: `Policy₀` is a list of layers, `compose`
  is append, and `compose-assoc` is a proof on lists. The other option is
  the free algebra HIT with `compose-assoc` as a path constructor (O9).
- N7 DAO self-reference: a governance context can hold a policy. In an
  instance, `Field public` holds a `Policy₀` term, so `Policy₀` and the
  governance type are one inductive-inductive definition (C5). `eval`
  recurses on the nesting of the context, not on the policy: the policy in
  a governance context is a strict part of that context. The quotient of
  N5 comes after this definition, so no inductive-recursive definition is
  necessary (O7).
- N8 Zero knowledge: the design has no simulator and no probability, so
  the language cannot state zero knowledge. It is a property of the
  backend (zkml-lang and its proof systems), outside the guest laws.
- N9 `Bool`: `verify` returns `Bool`, which the fragment takes from the
  Cubical prelude. Default: `Bool` is `Unit ⊎ Unit`, with the names `true`
  and `false`. It is not a new base type. `≃` and `Σ[ ]` are derived from
  Pi, Sigma and Path.
- N10 Names: `public` and `private` are keywords in Agda, so the fragment
  does not parse in Agda as written. In the guest, they are the
  constructors of `Visibility`.
- N11 Laws not in the design. Candidates, not defaults: `restrict` is
  idempotent, `restrict` and `compose` commute, and a law for the parallel
  case of `compose`. Each needs a USER ruling (O8).

## 5. Slices and gates

- P0: `bin/new-lang.sh zkpolicy-lang tcc-wasm`, `design/DESIGN.md` (the
  design without change), this plan and the README introduction. Gate: kit
  `make check` (13 Wasm modules valid, 683 differential cases, 0
  mismatches).
- P1: C1 and C2 from zkml-lang (6ed605a and the Z1b commits). F12 and F13
  over Path. Gate: the zkml-lang Z1 guest tests pass here.
- P2: C3 from zkml-lang Z2.
- P3: C4 from zkml-lang Z3. Guest tests: set quotient elimination into a
  set, truncation elimination into a proposition.
- P4: C5 from zkml-lang Z4.
- P5: C6 Glue types and `ua`. Guest tests: `uaβ`, `hPropExt` and the
  effectiveness of the set quotient. The C7 check tests.
- P6: Domain core: `Visibility`, `Bool` (N9), `PolicyModel` with N1 to
  N4, `Context`, `PolicyEquiv` and `≃`. Lemmas: `PolicyEquiv` is an
  equivalence relation, and `privacy` gives equal decisions of
  `restrict p` on two contexts with one public part. Needs O2 and O3
  first.
- P7: The `Ref` instance (N4, N5, N6), with a proof of each law. Guest
  tests for each law, and one mutant for each law that must not check.
- P8: Wasm edge runtime: `eval` and the witness half of `prove` for `Ref`
  as Wasm entries. Gate: `langc eval` against Wasm, differential.
- P9: `SmallModel` and `ZkmlCircuit`. Needs zkml-lang Z5 and Z6 (the
  Circuit type and its operators) at a pinned commit. The writer prints
  zkml-lang source. Gate: zkml-lang `langc check` accepts each file, and
  zkml-lang `langc eval` of each circuit on its witness agrees with
  `langc eval` here.
- P10: EVM verifier (O6). Needs zkml-lang Z9 (Halo2). Gate: geth `evm`
  runs the verifier, accepts a proof from the P9 output and refuses a
  changed proof.
- P11: DAO self-reference (N7): the governance instance, and a guest test
  where a governance policy decides on a policy that holds a governance
  context.
- P12: `SPEC.md`, `probe/CAPABILITY.md`, `docs/STATUS.md`,
  `docs/VALIDATION.md` and `README.md`. Final gate.
- Gate: `make check`. The target gates need zkml-lang, Rust and EVM tools,
  so they are `make zk-check`, outside the sandbox when necessary. Each
  long run stays under 4 GB. Rust builds use `-j 2`.

## 6. Open items

- O1: Source of the cubical core. Default: patches from zkml-lang, as each
  zkml-lang slice lands. Other option: zkml-lang O1 moves C1 to C5 into
  lang-template first, and this repository takes them from there.
- O2: The `verify` hole (N3). Default: `ZKProof` takes `Field public`.
  Other options: keep `Context` and add an implicit private argument,
  which an on-chain verifier cannot supply; or a truncated Σ over the
  private part, which the verifier still receives as data. A USER ruling
  is necessary before P6.
- O3: The `eval-compose` hole (N2). Default: the Σ witness. Other options:
  `priv = Context.priv c`, which makes `compose p q` agree with `q` on all
  contexts (so `sip-policy` makes it equal to `q`); or the congruence law
  `PolicyEquiv p p' → PolicyEquiv q q' → PolicyEquiv (compose p q) (compose p' q')`,
  which has no hole. A USER ruling is necessary before P6.
- O4: `soundness` (N4). Default: keep the stated law, and add
  `completeness` and the truncated `soundness-pub`.
- O5: Glue types and univalence (C6). Default: in, because the design
  names univalence. If out, `sip-policy` and the effectiveness of the set
  quotient need a different proof.
- O6: EVM verifier route. Default: the Halo2 output of the zkml-lang
  route, with KZG on BN254, and a verifier contract made from it with the
  snark-verifier crate. Other option: an EVM writer in this repository,
  from the tcc-evm kit (it shares the front end with tcc-wasm), that calls
  the BN254 pairing precompile.
- O7: DAO recursion (N7). Default: one inductive-inductive definition, and
  `eval` by recursion on the context. Other option: a coalgebra (unfold)
  for a governance process that does not stop.
- O8: Laws not in the design (N11). Default: none before P7.
- O9: `Policy₀` (N6). Default: a list of layers. Other option: the free
  algebra HIT.
