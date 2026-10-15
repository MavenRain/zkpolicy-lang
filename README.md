# zkpolicy-lang

zkpolicy-lang is a language for agent policies and small-model inference.
A zero-knowledge proof shows that a decision obeys its policy. The use
cases are DAOs and edge devices. The core types and operations come from
the Cubical Agda fragment in `design/DESIGN.md`: a split into public and
private parts, contexts, decisions, policies, transcripts and proofs, with
evaluation, proof, composition, restriction and verification. The type
formers are those of cubical type theory, with paths, HITs and QIITs, and
without proof irrelevance. The host is TinyCC. The targets are the
zkml-lang Circuit IR, a Wasm edge runtime and an EVM verifier. It comes
from lang-template with the tcc-wasm host.

- `docs/PLAN.md`: the rulings, the design choices, the notes on the
  fragment and the slices P0 to P12.
- `SPEC.md`: the specification. Fill each section.
- `design/DESIGN.md`: the domain design. It is the only source of the core
  types and the core operations.
- `formers/FORMERS.md`: the type formers. `formers/tcc-wasm.md` gives their
  status on the tcc-wasm host.
- `probe/CAPABILITY.md`: the host probe.
- `docs/STATUS.md` and `docs/VALIDATION.md`: the status and the gate results.
- `docs/host/README.md`: how to build and test the host kit, and how to
  replace the sample domain in `domain/`.
