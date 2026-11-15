#!/bin/sh
# Run every built-in example on the C64 in headless VICE (make c64-example
# for each examples/*.pl0, one after the other) and print a table: result,
# emulated run time of pl0_run (CIA2 cycle count / 985248, PAL), compile
# time, and the P-code instruction count measured on the host
# (pl0host --count-instr: the same compiler, so the same program).
# Usage: tests/c64_examples.sh [--record]
#   --record  also write the run times to docs/architecture/c64-times.tsv,
#             which tools/example_rules.py adds to examples-budget.md as
#             the "C64 s" column (then run tests/run_examples.sh
#             --write-budget).
# Exit 1 if any example failed or took longer than LIMIT seconds.
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
printf '%-12s %-6s %8s %10s %12s\n' example result "run s" "compile s" "P-code instr"
while IFS='	' read -r name result run comp cycles; do
    instr=$(build/pl0host --count-instr "examples/$name.pl0" 2>&1 >/dev/null | sed -n 's/^instructions: //p')
    printf '%-12s %-6s %8s %10s %12s\n' "$name" "$result" "$run" "$comp" "$instr"
    if [ "$result" != ok ]; then fail=$((fail+1)); continue; fi
    if awk -v t="$run" -v l=$LIMIT 'BEGIN { exit !(t > l) }'; then
        echo "  ^ over $LIMIT s"; fail=$((fail+1))
    fi
done <"$tsv"
n=$(wc -l <"$tsv" | tr -d ' ')
if [ "$1" = --record ]; then
    { echo "# example	C64 run seconds (tests/c64_examples.sh --record)"
      cut -f1,3 "$tsv"; } >docs/architecture/c64-times.tsv
    echo "wrote docs/architecture/c64-times.tsv"
fi
if [ $fail -ne 0 ]; then echo "$fail of $n example(s) failed on the C64"; exit 1; fi
echo "$n example(s) passed on the C64, each within $LIMIT s"
