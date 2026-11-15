#!/bin/sh
# Run every examples/*.pl0 that has a matching .expected file through
# build/pl0host and compare stdout+stderr with tools/compare_output.py.
# Usage: tests/run_examples.sh   (run from the repository root; `make examples`)
cd "$(dirname "$0")/.." || exit 2
host=build/pl0host
[ -x "$host" ] || { echo "error: $host not built (run make host)"; exit 2; }

n=0; fail=0
tmp=$(mktemp) || exit 2
trap 'rm -f "$tmp"' EXIT
for src in examples/*.pl0; do
    [ -e "$src" ] || continue
    exp="${src%.pl0}.expected"
    [ -f "$exp" ] || continue
    n=$((n+1))
    "$host" "$src" >"$tmp" 2>&1
    if python3 tools/compare_output.py "$exp" "$tmp"; then
        echo "ok   $src"
    else
        echo "FAIL $src"
        fail=$((fail+1))
    fi
done
if [ $n -eq 0 ]; then
    echo "no examples to check: examples/ has no .pl0 file with a matching .expected"
    exit 0
fi
if [ $fail -ne 0 ]; then echo "$fail of $n example(s) FAILED"; exit 1; fi
echo "$n example(s) passed"
