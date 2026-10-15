# Ports from zkml-lang

The cubical formers C1 to C5 come from zkml-lang as patches, one
zkml-lang commit at a time (`docs/PLAN.md`, item O1). Each row is one
ported commit. The patch is
`git -C zkml-lang diff <parent> <commit> -- src test examples formers docs/host`.
It does not include `docs/PLAN.md` and `docs/STATUS.md` of zkml-lang,
because they are specific to zkml-lang. The local gate is `make check`
and `make check-clang` in this repository.

| zkml-lang commit | Subject | Paths | Date | Local gate result |
|---|---|---|---|---|
| 6ed605a | Add interval and cubical path primitives | `src/front/` (check.c, core.h, eval.c, lexer.c, lexer.h, lower.c, parser.c), `test/` (fronttool.c, gate.sh, check, ir, parse), `examples/paths.lang`, `formers/`, `docs/host/` | 2026-10-15 | make check exit 0: 0 failures, 14 Wasm modules valid, 683 differential cases with 0 mismatches. 4 of 4 mutants KILLED |
