# zkpolicy-lang

zkpolicy-lang is a language for agent policies and small-model inference.
A zero-knowledge proof shows that a decision obeys its policy. The use
cases are DAOs and edge devices. The core types and operations come from
the Cubical Agda fragment in `design/DESIGN.md`: a split into public and
private parts, contexts, decisions, policies, transcripts and proofs, with
evaluation, proof, composition, restriction and verification. The type
formers are those of cubical type theory, with paths, HITs and QIITs, and
without proof irrelevance. The host is TinyCC. The targets are the
zkml-lang Circuit IR, a Wasm edge runtime and an EVM verifier (slice P10,
not started). It comes from lang-template with the tcc-wasm host.

## Status

Status 2026-10-20: slices P0 to P9b (s1 to s5), P11, P11b and P12 are
DONE. The deferred part of P9b and slice P10 are open (P12 ruling R1).
`docs/STATUS.md` gives each slice and the gate counts at HEAD.

The slices (`docs/PLAN.md` section 5):

- P0: the tcc-wasm kit with the formers F1 to F15.
- P1 and P2: the interval, faces, systems, PathP and Path (C1, C2), and
  `transp`, `hcomp`, `comp` and `fill` (C3).
- P3 and P4: HITs and their eliminator (C4), indexed sorts and mutual
  blocks (C5).
- P5: Glue types and univalence (C6), hPropExt and set quotients (C7).
- P6: the policy domain core.
- P7: the reference signature and its laws (P7a, P7b).
- P8: the Wasm edge runtime.
- P9a and P9b: the small model, its circuits and the `langc zkml` writer.
- P11 and P11b: the governance instance, block induction and `govModel`.
- P12: the final docs and the final gate.

## Known limits

`docs/STATUS.md` section `## Known limits` gives each limit and its cause.

## Quick start

Run each command from the repository root. TinyCC (`tcc`), node,
`wasm-validate` and `wasm-objdump` must be on the path. `PIN` gives the
versions.

```
make build
make check
make zk-check
build/langc check examples/governance.lang
build/langc eval examples/governance.lang gqComp4
build/langc build examples/policy.lang -o build/policy.wasm
```

- `make build` builds `build/langc`.
- `make check` runs the full gate. It takes about 60 minutes.
- `make check-clang` checks the C sources with clang.
- `make zk-check` checks the `langc zkml` outputs with zkml-lang at the
  commit in `PIN`. Set `ZKML_DIR` to the zkml-lang checkout (the default
  is `../zkml-lang`).
- `build/langc check` prints `ok`. `build/langc eval` prints `36`.
  `build/langc build` writes a Wasm module.

## Files

- `docs/PLAN.md`: the rulings, the design choices, the notes on the
  fragment and the slices P0 to P12.
- `SPEC.md`: the specification.
- `design/DESIGN.md`: the domain design. It is the only source of the core
  types and the core operations.
- `formers/FORMERS.md`: the type formers. `formers/tcc-wasm.md` gives their
  status on the tcc-wasm host.
- `probe/CAPABILITY.md`: the host probe.
- `docs/STATUS.md` and `docs/VALIDATION.md`: the status and the gate results.
- `docs/host/README.md`: how to build and test the host kit, and how to
  replace the sample domain in `domain/`.
