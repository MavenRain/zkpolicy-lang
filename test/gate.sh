#!/bin/sh
# The kit gate. `make check` runs it from the kit directory after the build.
set -eu
fail=0
count=0
# The zk mode (`make zk-check`, rule R6). Each line of test/zk/expect.txt is
# `PROG NAME ENTRY [ARGS] => WANT`. The writer output for PROG and NAME must
# pass the zkml-lang check at the PIN. WANT is a Nat, `zp` (the zp eval of
# ENTRY in examples/PROG.lang) or `zp:E` (the zp eval of the entry E in
# examples/PROG.lang at the same ARGS, P9b rule R3). NAME can be
# MODEL@OPTION: the writer then runs on MODEL with --OPTION, for example
# smW@residual (P9b s2), smW@residual-loop=2 (P9b s3) or smW@lookup=smSq
# (P9b s4: the table lookup after the dense layer). The check of each
# output at the PIN also type checks the generated NAMESat proof (P9b s5).
# Both sides reduce mod
# p = 2013265921: at the ARGS 2013265920 0 (p-1 0) the code of smDenseE is
# 2015279183920. Then P9b rule R9: the golden test/zk/writer-zw.lang passes
# the check at the PIN, and zwRunE and zwWitE give 8 at 2 3.
# ZKML_DIR is the zkml-lang checkout (default ../zkml-lang).
if [ "${1:-}" = zk ]; then
  rows=0
  zkml=${ZKML_DIR:-../zkml-lang}
  commit=$(rg -o -r '$1' '^zkml-lang commit: ([0-9a-f]{40})$' PIN || true)
  tree=$(rg -o -r '$1' '^zkml-lang tree: ([0-9a-f]{40})$' PIN || true)
  if [ -z "$commit" ] || [ -z "$tree" ]; then
    echo "FAIL zk: PIN has no zkml-lang commit and tree"
    exit 1
  fi
  # tree_is TREE: the PIN commit of ZKML_DIR has the tree TREE.
  tree_is() { [ "$(git -C "$zkml" rev-parse "$commit^{tree}" 2>/dev/null || true)" = "$1" ]; }
  if tree_is "$commit"; then
    echo "FAIL zk: the tree check accepts a wrong tree"
    exit 1
  fi
  if ! tree_is "$tree"; then
    echo "FAIL zk: $zkml has no commit $commit with the tree $tree"
    exit 1
  fi
  rm -rf build/zkml-pin
  mkdir -p build/zkml-pin
  git -C "$zkml" archive "$commit" | tar -x -C build/zkml-pin
  build/zkml-names domain/domain.lang build/zkml-pin/domain/domain.lang >build/zkml-names.h
  if ! cmp -s src/zkml-names.h build/zkml-names.h; then
    echo "FAIL zk: regenerate src/zkml-names.h against the PIN domain"
    exit 1
  fi
  if ! make -C build/zkml-pin build/langc >build/zkml-pin.log 2>&1; then
    echo "FAIL zk: the zkml-lang build at the PIN failed (build/zkml-pin.log)"
    exit 1
  fi
  zk=build/zkml-pin/build/langc
  build/zkmltool "$zk"
  tmp=${TMPDIR:-/tmp}/langc-zk.$$
  mkdir -p "$tmp"
  while read -r prog name entry rest; do
    args=${rest%%=>*}
    want=${rest##*=> }
    model=${name%@*}
    flag=
    [ "$model" = "$name" ] || flag=--${name#*@}
    out="$tmp/$prog-$name.lang"
    if [ ! -f "$out" ]; then
      status=0
      build/langc zkml "examples/$prog.lang" "$model" $flag -o "$out" 2>"$tmp/err" || status=$?
      got=$("$zk" check "$out" 2>&1) || true
      [ "$status:$got" = "0:ok" ] || { echo "FAIL zk $prog $name: writer exit $status, zkml check: $got"; fail=$((fail + 1)); }
    fi
    case "$want" in
      zp) want=$(build/langc eval "examples/$prog.lang" "$entry" $args 2>&1 || true) ;;
      zp:*) want=$(build/langc eval "examples/$prog.lang" "${want#zp:}" $args 2>&1 || true) ;;
    esac
    got=$("$zk" eval "$out" "$entry" $args 2>&1) || true
    rows=$((rows + 1))
    [ "$got" = "$want" ] || { echo "FAIL zk $prog $entry $args: want $want, got $got"; fail=$((fail + 1)); }
  done <test/zk/expect.txt
  rm -rf "$tmp"
  echo "zk: $rows rows checked"
  zwrows=1
  got=$("$zk" check test/zk/writer-zw.lang 2>&1) || true
  [ "$got" = ok ] || { echo "FAIL zk writer-zw.lang: zkml check: $got"; fail=$((fail + 1)); }
  for entry in zwRunE zwWitE; do
    got=$("$zk" eval test/zk/writer-zw.lang "$entry" 2 3 2>&1) || true
    zwrows=$((zwrows + 1))
    [ "$got" = 8 ] || { echo "FAIL zk writer-zw.lang $entry 2 3: want 8, got $got"; fail=$((fail + 1)); }
  done
  echo "zk: writer-zw.lang $zwrows rows checked"
  echo "gate: $fail failures"
  [ "$fail" -eq 0 ]
  exit
fi

for prog in examples/*.lang; do
  out=$(build/langc check "$prog" 2>&1) || true
  count=$((count + 1))
  if [ "$out" != "ok" ]; then
    echo "FAIL $prog: $out"
    fail=$((fail + 1))
  fi
done
echo "examples: $count checked, $fail failed"

tmp=${TMPDIR:-/tmp}/langc-gate.$$
mkdir -p "$tmp"
awk 'BEGIN { printf "def deep : Nat := " } { } END {
  for (i = 0; i < 1100; i++) printf "("; printf "1"; for (i = 0; i < 1100; i++) printf ")"; print "" }' </dev/null >"$tmp/deep.lang"

# refusals DIR LABEL: each line of DIR/expect.txt is `FILE CODE`. `langc check`
# must exit 1 and print `langc: CODE: ...` on stderr.
refusals() {
  checked=0
  while read -r file code; do
    path="$1/$file"
    [ "$file" = "deep.lang" ] && path="$tmp/deep.lang"
    status=0
    build/langc check "$path" 2>"$tmp/err" >/dev/null || status=$?
    first=$(head -n 1 "$tmp/err")
    checked=$((checked + 1))
    case "$status:$first" in
      "1:langc: $code: "*) ;;
      *) echo "FAIL $file: want exit 1 and $code, got exit $status: $first"; fail=$((fail + 1)) ;;
    esac
  done <"$1/expect.txt"
  echo "$2 refusals: $checked checked"
}
refusals test/parse parse
refusals test/check check

# A refl refusal shows both normal forms.
build/langc check test/check/bad-refl.lang 2>"$tmp/err" >/dev/null || true
if ! rg -q -F 'the left side normalizes to 1201, the right side normalizes to 1200' "$tmp/err"; then
  echo "FAIL bad-refl.lang: the message does not show both normal forms"
  fail=$((fail + 1))
fi

# Each line of test/eval/expect.txt is `FILE NAME [ARGS] => OUTPUT`. `langc eval`
# must exit 0 and print OUTPUT. A trap also prints `langc: EVAL_OVERFLOW: NAME: ...`
# on stderr.
evals=0
while IFS= read -r line; do
  want=${line#* => }
  set -- ${line%% => *}
  file=$1
  name=$2
  shift 2
  status=0
  got=$(build/langc eval "examples/$file.lang" "$name" "$@" 2>"$tmp/err") || status=$?
  first=$(head -n 1 "$tmp/err")
  want_err=""
  [ "$want" = "trap" ] && want_err="langc: EVAL_OVERFLOW: $name: "
  evals=$((evals + 1))
  case "$status|$got|$first" in
    "0|$want|$want_err"*) ;;
    *) echo "FAIL eval $file $name: want $want, got exit $status: $got $first"; fail=$((fail + 1)) ;;
  esac
done <test/eval/expect.txt
echo "evals: $evals checked"

# eval_refused STATUS CODE NAME [ARGS]: `langc eval examples/entries.lang NAME
# ARGS` must exit STATUS and print `langc: CODE: NAME: ...` on stderr.
eval_refused() {
  want_status=$1
  code=$2
  shift 2
  status=0
  build/langc eval examples/entries.lang "$@" 2>"$tmp/err" >/dev/null || status=$?
  case "$status:$(head -n 1 "$tmp/err")" in
    "$want_status:langc: $code: $1: "*) ;;
    *) echo "FAIL eval $*: want exit $want_status and $code, got exit $status"; fail=$((fail + 1)) ;;
  esac
}
eval_refused 2 EVAL_ARGS addPrice 1
eval_refused 2 EVAL_ARGS addPrice x 1
eval_refused 2 EVAL_ARGS addPrice 18446744073709551616 1
eval_refused 2 EVAL_ARGS pick 2 4 9
eval_refused 1 EVAL_ENTRY nope

status=0
build/langc 2>/dev/null || status=$?
[ "$status" -eq 2 ] || { echo "FAIL usage: want exit 2, got $status"; fail=$((fail + 1)); }
status=0
build/langc check "$tmp/missing.lang" 2>/dev/null || status=$?
[ "$status" -eq 2 ] || { echo "FAIL missing file: want exit 2, got $status"; fail=$((fail + 1)); }

# The IR: each example lowers, with one func line for each entry.
irs=0
for f in examples/*.lang; do
  irs=$((irs + 1))
  status=0
  build/langc ir "$f" > "$tmp/ir.txt" 2> "$tmp/ir.err" || status=$?
  funcs=$(rg -c '^func ' "$tmp/ir.txt" || echo 0)
  entries=$(build/langc abi "$f" | rg -c '.' || echo 0)
  [ "$status" -eq 0 ] && [ "$funcs" -eq "$entries" ] || { echo "FAIL ir $f: exit $status, $funcs funcs for $entries entries: $(head -n 1 "$tmp/ir.err")"; fail=$((fail + 1)); }
done
echo "ir: $irs lowered"
status=0
build/langc check test/ir/closure.lang > /dev/null || status=$?
[ "$status" -eq 0 ] || { echo "FAIL closure.lang: check exit $status"; fail=$((fail + 1)); }
status=0
first=$(build/langc ir test/ir/closure.lang 2>&1 > /dev/null) || status=$?
case "$status:$first" in
  "1:langc: REFUSE_RUNTIME_CLOSURE: "*) ;;
  *) echo "FAIL closure.lang: want exit 1 and REFUSE_RUNTIME_CLOSURE, got exit $status: $first"; fail=$((fail + 1)) ;;
esac
status=0
build/langc check test/ir/runtime-policy.lang > /dev/null || status=$?
[ "$status" -eq 0 ] || { echo "FAIL runtime-policy.lang: check exit $status"; fail=$((fail + 1)); }
status=0
first=$(build/langc ir test/ir/runtime-policy.lang 2>&1 > /dev/null) || status=$?
case "$status:$first" in
  "1:langc: REFUSE_RUNTIME_CLOSURE: "*) ;;
  *) echo "FAIL runtime-policy.lang: want exit 1 and REFUSE_RUNTIME_CLOSURE, got exit $status: $first"; fail=$((fail + 1)) ;;
esac
status=0
build/langc check test/ir/path.lang > /dev/null || status=$?
[ "$status" -eq 0 ] || { echo "FAIL path.lang: check exit $status"; fail=$((fail + 1)); }
status=0
first=$(build/langc ir test/ir/path.lang 2>&1 > /dev/null) || status=$?
case "$status:$first" in
  "1:langc: REFUSE_RUNTIME_PATH: "*) ;;
  *) echo "FAIL path.lang: want exit 1 and REFUSE_RUNTIME_PATH, got exit $status: $first"; fail=$((fail + 1)) ;;
esac
status=0
build/langc check test/ir/hit-path.lang > /dev/null || status=$?
[ "$status" -eq 0 ] || { echo "FAIL hit-path.lang: check exit $status"; fail=$((fail + 1)); }
status=0
first=$(build/langc ir test/ir/hit-path.lang 2>&1 > /dev/null) || status=$?
case "$status:$first" in
  "1:langc: REFUSE_RUNTIME_PATH: "*) ;;
  *) echo "FAIL hit-path.lang: want exit 1 and REFUSE_RUNTIME_PATH, got exit $status: $first"; fail=$((fail + 1)) ;;
esac
status=0
build/langc check test/ir/hit-proof.lang > /dev/null || status=$?
[ "$status" -eq 0 ] || { echo "FAIL hit-proof.lang: check exit $status"; fail=$((fail + 1)); }
status=0
first=$(build/langc ir test/ir/hit-proof.lang 2>&1 > /dev/null) || status=$?
case "$status:$first" in
  "1:langc: REFUSE_RUNTIME_PROOF: "*) ;;
  *) echo "FAIL hit-proof.lang: want exit 1 and REFUSE_RUNTIME_PROOF, got exit $status: $first"; fail=$((fail + 1)) ;;
esac
status=0
build/langc check test/ir/natisset-path.lang > /dev/null || status=$?
[ "$status" -eq 0 ] || { echo "FAIL natisset-path.lang: check exit $status"; fail=$((fail + 1)); }
status=0
first=$(build/langc ir test/ir/natisset-path.lang 2>&1 > /dev/null) || status=$?
case "$status:$first" in
  "1:langc: REFUSE_RUNTIME_PATH: "*) ;;
  *) echo "FAIL natisset-path.lang: want exit 1 and REFUSE_RUNTIME_PATH, got exit $status: $first"; fail=$((fail + 1)) ;;
esac
status=0
build/langc check test/ir/qiit-tree.lang > /dev/null || status=$?
[ "$status" -eq 0 ] || { echo "FAIL qiit-tree.lang: check exit $status"; fail=$((fail + 1)); }
status=0
first=$(build/langc ir test/ir/qiit-tree.lang 2>&1 > /dev/null) || status=$?
case "$status:$first" in
  "1:langc: REFUSE_RUNTIME_TREE: "*) ;;
  *) echo "FAIL qiit-tree.lang: want exit 1 and REFUSE_RUNTIME_TREE, got exit $status: $first"; fail=$((fail + 1)) ;;
esac
status=0
build/langc check test/ir/rose-tree.lang > /dev/null || status=$?
[ "$status" -eq 0 ] || { echo "FAIL rose-tree.lang: check exit $status"; fail=$((fail + 1)); }
status=0
first=$(build/langc ir test/ir/rose-tree.lang 2>&1 > /dev/null) || status=$?
case "$status:$first" in
  "1:langc: REFUSE_RUNTIME_TREE: bad: a fold over the mutual block of Rose "*) ;;
  *) echo "FAIL rose-tree.lang: want exit 1 and the mutual block refusal, got exit $status: $first"; fail=$((fail + 1)) ;;
esac
status=0
build/langc check test/ir/governance-tree.lang > /dev/null || status=$?
[ "$status" -eq 0 ] || { echo "FAIL governance-tree.lang: check exit $status"; fail=$((fail + 1)); }
status=0
first=$(build/langc ir test/ir/governance-tree.lang 2>&1 > /dev/null) || status=$?
case "$status:$first" in
  "1:langc: REFUSE_RUNTIME_TREE: gCountStuck: "*) ;;
  *) echo "FAIL governance-tree.lang: want exit 1 and REFUSE_RUNTIME_TREE at gCountStuck, got exit $status: $first"; fail=$((fail + 1)) ;;
esac
status=0
build/langc check test/ir/governance-closure.lang > /dev/null || status=$?
[ "$status" -eq 0 ] || { echo "FAIL governance-closure.lang: check exit $status"; fail=$((fail + 1)); }
status=0
first=$(build/langc ir test/ir/governance-closure.lang 2>&1 > /dev/null) || status=$?
case "$status:$first" in
  "1:langc: REFUSE_RUNTIME_CLOSURE: gRunStuck: "*) ;;
  *) echo "FAIL governance-closure.lang: want exit 1 and REFUSE_RUNTIME_CLOSURE at gRunStuck, got exit $status: $first"; fail=$((fail + 1)) ;;
esac
status=0
build/langc check test/ir/block-ind-runtime.lang > /dev/null || status=$?
[ "$status" -eq 0 ] || { echo "FAIL block-ind-runtime.lang: check exit $status"; fail=$((fail + 1)); }
status=0
first=$(build/langc ir test/ir/block-ind-runtime.lang 2>&1 > /dev/null) || status=$?
case "$status:$first" in
  "1:langc: REFUSE_RUNTIME_PROOF: runtimeCount: "*) ;;
  *) echo "FAIL block-ind-runtime.lang: want exit 1 and REFUSE_RUNTIME_PROOF at runtimeCount, got exit $status: $first"; fail=$((fail + 1)) ;;
esac
status=0
got=$(build/langc eval test/ir/block-ind-runtime.lang gCount2 2>&1) || status=$?
[ "$status:$got" = "0:2" ] || { echo "FAIL block-ind-runtime.lang: want eval gCount2 => 2, got exit $status: $got"; fail=$((fail + 1)); }
status=0
got=$(build/langc eval test/ir/block-ind-runtime.lang gCountNest 2>&1) || status=$?
[ "$status:$got" = "0:10" ] || { echo "FAIL block-ind-runtime.lang: want eval gCountNest => 10, got exit $status: $got"; fail=$((fail + 1)); }
status=0
build/langc check test/ir/glue.lang > /dev/null || status=$?
[ "$status" -eq 0 ] || { echo "FAIL glue.lang: check exit $status"; fail=$((fail + 1)); }
status=0
first=$(build/langc ir test/ir/glue.lang 2>&1 > /dev/null) || status=$?
[ "$status" -eq 0 ] || { echo "FAIL glue.lang: want exit 0 (the transp along ua computes, P5b), got exit $status: $first"; fail=$((fail + 1)); }
status=0
build/langc check test/ir/transp.lang > /dev/null || status=$?
[ "$status" -eq 0 ] || { echo "FAIL transp.lang: check exit $status"; fail=$((fail + 1)); }
status=0
first=$(build/langc ir test/ir/transp.lang 2>&1 > /dev/null) || status=$?
case "$status:$first" in
  "1:langc: REFUSE_RUNTIME_TRANSP: "*) ;;
  *) echo "FAIL transp.lang: want exit 1 and REFUSE_RUNTIME_TRANSP, got exit $status: $first"; fail=$((fail + 1)) ;;
esac
got=$(build/langc eval test/ir/lazy.lang lazy 3 2>&1) || true
[ "$got" = "3" ] || { echo "FAIL lazy.lang: want 3, got $got"; fail=$((fail + 1)); }
build/langc ir test/ir/lazy.lang > /dev/null || { echo "FAIL lazy.lang: ir"; fail=$((fail + 1)); }
for want in "foldOnes 3:6" "mapUp 3:18" "bindUp 3:36" "filterUp 5:3" "trapHead 5:1" "trapSum 5:trap" "trapSum 2:3" \
  "filterTrap 2:3" "filterTrap 5:trap" "bindTrap 5:1" "stackSum 3:15" "stackTrap 0:7" "stackTrap 2:5"; do
  call=${want%%:*}
  got=$(build/langc eval test/ir/spine.lang ${call% *} ${call#* } 2>"$tmp/err") || true
  want_err=""
  [ "${want#*:}" = "trap" ] && want_err="langc: EVAL_OVERFLOW: ${call% *}: "
  case "$got|$(head -n 1 "$tmp/err")" in
    "${want#*:}|$want_err"*) ;;
    *) echo "FAIL spine.lang $call: want ${want#*:}, got $got"; fail=$((fail + 1)) ;;
  esac
done
build/langc ir test/ir/spine.lang > /dev/null || { echo "FAIL spine.lang: ir"; fail=$((fail + 1)); }
status=0
build/langc check test/ir/tree.lang > /dev/null || status=$?
[ "$status" -eq 0 ] || { echo "FAIL tree.lang: check exit $status"; fail=$((fail + 1)); }
status=0
first=$(build/langc ir test/ir/tree.lang 2>&1 > /dev/null) || status=$?
case "$status:$first" in
  "1:langc: REFUSE_RUNTIME_TREE: "*) ;;
  *) echo "FAIL tree.lang: want exit 1 and REFUSE_RUNTIME_TREE, got exit $status: $first"; fail=$((fail + 1)) ;;
esac

# The Wasm target: each module builds and is valid. Then the differential: on
# the grid of test/grid.awk, node and `langc eval` give the same result for
# each entry (trap == trap). The stderr of eval is not merged into stdout.
wasms=0
cases=0
mismatches=0
for f in examples/*.lang test/ir/spine.lang test/ir/lazy.lang test/ir/proof-trap.lang; do
  w="$tmp/$(basename "$f" .lang).wasm"
  wasms=$((wasms + 1))
  if ! build/langc build "$f" -o "$w" 2>"$tmp/w.err" || ! wasm-validate "$w" 2>>"$tmp/w.err" ||
    ! wasm-objdump -x "$w" >/dev/null 2>>"$tmp/w.err"; then
    echo "FAIL wasm $f: $(head -n 1 "$tmp/w.err")"
    fail=$((fail + 1))
    continue
  fi
  build/langc ir "$f" | awk -f test/grid.awk >"$tmp/cases.txt"
  : >"$tmp/want.txt"
  while read -r call; do
    status=0
    want=$(build/langc eval "$f" $call 2>/dev/null) || status=$?
    [ "$status" -eq 0 ] || want="eval-exit-$status"
    echo "$want" >>"$tmp/want.txt"
  done <"$tmp/cases.txt"
  if ! node test/run-wasm.mjs "$w" - <"$tmp/cases.txt" >"$tmp/got.txt" 2>"$tmp/w.err"; then
    echo "FAIL node $f: $(head -n 1 "$tmp/w.err")"
    fail=$((fail + 1))
    continue
  fi
  cases=$((cases + $(rg -c '.' "$tmp/cases.txt" || echo 0)))
  paste -d '|' "$tmp/cases.txt" "$tmp/want.txt" "$tmp/got.txt" | awk -F '|' '$2 != $3' >"$tmp/bad.txt"
  bad=$(rg -c '.' "$tmp/bad.txt" || echo 0)
  mismatches=$((mismatches + bad))
  [ "$bad" -eq 0 ] || { echo "FAIL differential $f: $bad mismatches, first $(head -n 1 "$tmp/bad.txt")"; fail=$((fail + 1)); }
done
echo "wasm: $wasms modules valid"
echo "differential: $cases cases, $mismatches mismatches"
# A Flag argument larger than 1 traps (langc eval refuses it with EVAL_ARGS).
got=$(node test/run-wasm.mjs "$tmp/entries.wasm" pick 2 4 9 2>&1) || true
[ "$got" = "trap" ] || { echo "FAIL wasm pick 2 4 9: want trap, got $got"; fail=$((fail + 1)); }
# The zkml writer (rule R4): the output for smallmodel smW is the golden
# file test/zk/smallmodel-smW.lang, and the output for zw in
# test/zk/writer.lang is test/zk/writer-zw.lang. The output for smW with
# --residual is test/zk/smallmodel-smW-residual.lang (P9b s2), and with
# --residual-loop=2 it is test/zk/smallmodel-smW-loop2.lang (P9b s3), and
# with --lookup=smSq it is test/zk/smallmodel-smW-lookup.lang (P9b s4). Then
# the refusals ZKML_NAME, ZKML_MODEL and USAGE (exit 2). A refusal row can
# have a writer option in the column 4: zw with --residual or
# --residual-loop=2 is ZKML_MODEL, because the residual circuits need m = n;
# --lookup=TABLE is ZKML_NAME when no def has the name, and ZKML_MODEL when
# the table is not List (List Nat) or a row does not have two entries. The
# smDot row after the loop also checks the full message: the type check
# comes before the row read (P9b s4 mutant M5).
# A bad writer option is USAGE: K must be 1 to 16, with no leading zero and
# nothing after the digits. --lookup alone and --lookup= with no name are
# USAGE. In test/zk/writer-names.lang
# the defs ztT and zrRd are generated names of the models zt and zr (P9b
# rule R7), so the writer refuses zt and zr. The defs zsSat and zvWit are
# generated names of the models zs and zv (P9b s5), so the writer refuses
# zs and zv.
zkrows=1
status=0
build/langc zkml examples/smallmodel.lang smW -o "$tmp/smW.lang" 2>"$tmp/err" || status=$?
if [ "$status" -ne 0 ] || ! cmp -s "$tmp/smW.lang" test/zk/smallmodel-smW.lang; then
  echo "FAIL zkml smW: exit $status, or the output is not test/zk/smallmodel-smW.lang"
  fail=$((fail + 1))
fi
status=0
build/langc zkml test/zk/writer.lang zw -o "$tmp/zw.lang" 2>"$tmp/err" || status=$?
zkrows=$((zkrows + 1))
if [ "$status" -ne 0 ] || ! cmp -s "$tmp/zw.lang" test/zk/writer-zw.lang; then
  echo "FAIL zkml zw: exit $status, or the output is not test/zk/writer-zw.lang"
  fail=$((fail + 1))
fi
status=0
build/langc zkml examples/smallmodel.lang smW --residual -o "$tmp/smWres.lang" 2>"$tmp/err" || status=$?
zkrows=$((zkrows + 1))
if [ "$status" -ne 0 ] || ! cmp -s "$tmp/smWres.lang" test/zk/smallmodel-smW-residual.lang; then
  echo "FAIL zkml smW --residual: exit $status, or the output is not test/zk/smallmodel-smW-residual.lang"
  fail=$((fail + 1))
fi
status=0
build/langc zkml examples/smallmodel.lang smW --residual-loop=2 -o "$tmp/smWloop2.lang" 2>"$tmp/err" || status=$?
zkrows=$((zkrows + 1))
if [ "$status" -ne 0 ] || ! cmp -s "$tmp/smWloop2.lang" test/zk/smallmodel-smW-loop2.lang; then
  echo "FAIL zkml smW --residual-loop=2: exit $status, or the output is not test/zk/smallmodel-smW-loop2.lang"
  fail=$((fail + 1))
fi
status=0
build/langc zkml examples/smallmodel.lang smW --lookup=smSq -o "$tmp/smWlookup.lang" 2>"$tmp/err" || status=$?
zkrows=$((zkrows + 1))
if [ "$status" -ne 0 ] || ! cmp -s "$tmp/smWlookup.lang" test/zk/smallmodel-smW-lookup.lang; then
  echo "FAIL zkml smW --lookup=smSq: exit $status, or the output is not test/zk/smallmodel-smW-lookup.lang"
  fail=$((fail + 1))
fi
while read -r prog name code flag; do
  status=0
  build/langc zkml "$prog" "$name" $flag -o "$tmp/z.lang" 2>"$tmp/err" >/dev/null || status=$?
  first=$(head -n 1 "$tmp/err")
  zkrows=$((zkrows + 1))
  case "$status:$first" in
    "1:langc: $code: "*) ;;
    *) echo "FAIL zkml $name: want exit 1 and $code, got exit $status: $first"; fail=$((fail + 1)) ;;
  esac
done <<EOF
examples/smallmodel.lang nope ZKML_NAME
examples/smallmodel.lang bbP ZKML_NAME
examples/smallmodel.lang smDot ZKML_MODEL
test/zk/writer.lang zwide ZKML_MODEL
test/zk/writer-names.lang zt ZKML_MODEL
test/zk/writer-names.lang zr ZKML_MODEL
test/zk/writer-names.lang zs ZKML_MODEL
test/zk/writer-names.lang zv ZKML_MODEL
test/zk/writer.lang zw ZKML_MODEL --residual
test/zk/writer.lang zw ZKML_MODEL --residual-loop=2
test/zk/writer.lang zw ZKML_NAME --lookup=nope
examples/smallmodel.lang smW ZKML_MODEL --lookup=smDot
test/zk/writer-table.lang zm ZKML_MODEL --lookup=zw3
test/zk/writer-table.lang zm ZKML_MODEL --lookup=zw1
EOF
status=0
build/langc zkml examples/smallmodel.lang smW --lookup=smDot -o "$tmp/z.lang" 2>"$tmp/err" >/dev/null || status=$?
first=$(head -n 1 "$tmp/err")
zkrows=$((zkrows + 1))
want='langc: ZKML_MODEL: smDot: the type of the lookup table is not List (List Nat)'
if [ "$status" -ne 1 ] || [ "$first" != "$want" ]; then
  echo "FAIL zkml smW --lookup=smDot: want exit 1 and the table type message, got exit $status: $first"
  fail=$((fail + 1))
fi
status=0
build/langc zkml examples/smallmodel.lang smW "$tmp/z.lang" >/dev/null 2>&1 || status=$?
zkrows=$((zkrows + 1))
[ "$status" -eq 2 ] || { echo "FAIL zkml smW without -o: want exit 2, got exit $status"; fail=$((fail + 1)); }
for bad in --bogus --residual-loop=0 --residual-loop=17 --residual-loop=02 --residual-loop=2x --lookup --lookup=; do
  status=0
  build/langc zkml examples/smallmodel.lang smW "$bad" -o "$tmp/z.lang" >/dev/null 2>&1 || status=$?
  zkrows=$((zkrows + 1))
  [ "$status" -eq 2 ] || { echo "FAIL zkml smW $bad: want exit 2, got exit $status"; fail=$((fail + 1)); }
done
echo "zkml: $zkrows rows checked"
rm -rf "$tmp"

if rg -n '\x{2013}|\x{2014}' . >/dev/null; then
  echo "FAIL: an em-dash or en-dash is in the kit"
  fail=$((fail + 1))
fi
echo "gate: $fail failures"
[ "$fail" -eq 0 ]
