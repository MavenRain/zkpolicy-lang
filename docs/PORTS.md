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
| 573c983 | Add cubical faces, partial elements and systems | `src/front/` (check.c, core.h, eval.c, lexer.c, lexer.h, lower.c, parser.c), `test/` (fronttool.c, check, parse), `examples/systems.lang`, `formers/`, `docs/host/` | 2026-10-15 | make check exit 0: 0 failures, 16 Wasm modules valid, 694 differential cases with 0 mismatches. 2 of 2 mutants KILLED |
| e069119 | Add cubical transport over type lines and paths | `src/front/` (check.c, core.h, eval.c, lower.c), `test/` (fronttool.c, check, eval), `examples/transp.lang`, `formers/`, `docs/host/` | 2026-10-15 | make check exit 0: 0 failures, 18 Wasm modules valid, 696 differential cases with 0 mismatches. 3 of 3 mutants KILLED. Local change: the helper `face_env` of zkml-lang 573c983 is added again to `src/front/check.c`, because the P1b review fixes removed it and `transp` uses it |
| 0741c38 | Add cubical homogeneous composition with checked sides | `src/front/` (check.c, core.h, eval.c, lower.c), `test/` (fronttool.c, check, eval), `examples/hcomp.lang`, `formers/`, `docs/host/` | 2026-10-15 | make check exit 0: 0 failures, 19 Wasm modules valid, 697 differential cases with 0 mismatches. 3 of 3 mutants KILLED. Local changes: 37 line references in `formers/tcc-wasm.md` point to the local `src/front/check.c`; side branches are checked under their own face and the ambient face, with regression tests |
| 402c9fe | Complete cubical Kan composition and path laws | `src/front/` (check.c, eval.c), `test/` (fronttool.c, check, eval), `examples/kan.lang`, `formers/` | 2026-10-15 | make check exit 0: 0 failures, 20 Wasm modules valid, 698 differential cases with 0 mismatches. 4 of 4 mutants KILLED. Local changes: at the 17 conflict hunks in `src/front/check.c`, the zkml-lang side; the local face checks (`same_face`, `value_under_face`, and the ambient face in `check_on`) stay; 11 line references in `formers/tcc-wasm.md` point to the local `src/front/check.c`; the refusal case `test/check/pequiv-trans.lang` is removed, because it checks after the port |
