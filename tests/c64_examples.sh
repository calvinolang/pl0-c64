#!/bin/sh
# Run every built-in example on the C64 in headless VICE (make c64-example
# for each examples/*.pl0, one after the other) and print a table: result,
# emulated time of pl0_run and of pl0_compile (CIA2 cycle counts / 985248,
# PAL), their total, and the P-code instruction count measured on the host
# (pl0host --count-instr: the same compiler, so the same program).
# Usage: tests/c64_examples.sh [--record]
#   --record  also write the totals to docs/architecture/c64-times.tsv,
#             which tools/example_rules.py adds to examples-budget.md as
#             the "C64 s" column (then run tests/run_examples.sh
#             --write-budget).
# Exit 1 if any example failed or its total (compile + run: the time from
# F1 to the end of the program, which the spec limits to about 30 s) is
# over LIMIT seconds.
cd "$(dirname "$0")/.." || exit 2
LIMIT=30
tsv=build/c64/examples.tsv
mkdir -p build/c64
: >"$tsv"
fail=0
for src in examples/*.pl0; do
    name=$(basename "$src" .pl0)
    if ! make -s --no-print-directory c64-example EX="$name" TSV="$tsv" >"build/c64/ex_$name.log" 2>&1; then
        grep -q "^$name	" "$tsv" || printf '%s\tFAIL\t-\t-\t-\n' "$name" >>"$tsv"
        echo "FAIL $name (log: build/c64/ex_$name.log)"
        tail -5 "build/c64/ex_$name.log"
    fi
done
echo
printf '%-12s %-6s %8s %10s %8s %12s\n' example result "run s" "compile s" "total s" "P-code instr"
: >"$tsv.total"
while IFS='	' read -r name result run comp cycles; do
    instr=$(build/pl0host --count-instr "examples/$name.pl0" 2>&1 >/dev/null | sed -n 's/^instructions: //p')
    total=-
    if [ "$result" = ok ]; then total=$(awk -v r="$run" -v c="$comp" 'BEGIN { printf "%.2f", r + c }'); fi
    printf '%-12s %-6s %8s %10s %8s %12s\n' "$name" "$result" "$run" "$comp" "$total" "$instr"
    printf '%s\t%s\n' "$name" "$total" >>"$tsv.total"
    if [ "$result" != ok ]; then fail=$((fail+1)); continue; fi
    if awk -v t="$total" -v l=$LIMIT 'BEGIN { exit !(t > l) }'; then
        echo "  ^ over $LIMIT s"; fail=$((fail+1))
    fi
done <"$tsv"
n=$(wc -l <"$tsv" | tr -d ' ')
if [ "$1" = --record ]; then
    { echo "# example	C64 seconds, compile + run (tests/c64_examples.sh --record)"
      cat "$tsv.total"; } >docs/architecture/c64-times.tsv
    echo "wrote docs/architecture/c64-times.tsv"
fi
if [ $fail -ne 0 ]; then echo "$fail of $n example(s) failed on the C64"; exit 1; fi
echo "$n example(s) passed on the C64, each within $LIMIT s (compile + run)"
