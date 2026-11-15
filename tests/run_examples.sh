#!/bin/sh
# Check every built-in example in examples/ against its golden output and
# the example rules (tools/example_rules.py):
#   - a text example has <name>.expected: stdout is compared with
#     tools/compare_output.py (numbers within 1e-6 relative);
#   - a drawing example has <name>.lines: the clipped line dump of
#     `pl0host --lines` must match exactly;
#   - each example runs without error in fewer than BUDGET P-code
#     instructions (`pl0host --count-instr`), follows the source rules and
#     the output rules (fits the screen), and the required examples exist;
#   - docs/architecture/examples-budget.md matches the measured numbers.
# Usage: tests/run_examples.sh [--write-budget]   (from `make examples` or
# `make test`; --write-budget regenerates the budget table instead of
# checking it).
cd "$(dirname "$0")/.." || exit 2
host=build/pl0host
doc=docs/architecture/examples-budget.md
BUDGET=40000
MAX_EXAMPLES=15
REQUIRED="factorial primes newton pi gcd square polygon spiral koch tree sine lissajous"
mode=check
[ "$1" = "--write-budget" ] && mode=write
[ -x "$host" ] || { echo "error: $host not built (run make host)"; exit 2; }

tmp=$(mktemp -d) || exit 2
trap 'rm -rf "$tmp"' EXIT
n=0; fail=0
: >"$tmp/rows"

bad() { echo "FAIL $1: $2"; fail=$((fail+1)); }

for name in $REQUIRED; do
    [ -f "examples/$name.pl0" ] || bad "$name" "required example examples/$name.pl0 missing"
done

for src in examples/*.pl0; do
    [ -e "$src" ] || continue
    n=$((n+1))
    name=$(basename "$src" .pl0)
    base=${src%.pl0}
    ok=1
    if [ -f "$base.expected" ] && [ -f "$base.lines" ]; then
        bad "$name" "has both .expected and .lines"; continue
    elif [ -f "$base.expected" ]; then kind=text
    elif [ -f "$base.lines" ]; then kind=drawing
    else bad "$name" "no golden ($base.expected or $base.lines)"; continue
    fi
    if ! python3 tools/example_rules.py source "$src"; then
        bad "$name" "source rules"; ok=0
    fi
    "$host" --count-instr --lines "$tmp/lines" "$src" >"$tmp/out" 2>"$tmp/err"
    rc=$?
    instr=$(sed -n 's/^instructions: //p' "$tmp/err")
    fns=$(sed -n 's/^functions: //p' "$tmp/err")
    trig=$(sed -n 's/^turtle-trig: //p' "$tmp/err")
    grep -v -e '^instructions: ' -e '^functions: ' -e '^turtle-trig: ' "$tmp/err" >"$tmp/err2"
    if [ $rc -ne 0 ] || [ -s "$tmp/err2" ] || [ -z "$instr" ]; then
        bad "$name" "pl0host exit $rc: $(cat "$tmp/err2")"; continue
    fi
    if [ "$instr" -ge $BUDGET ]; then
        bad "$name" "$instr P-code instructions, budget is $BUDGET"; ok=0
    fi
    if ! python3 tools/example_rules.py output $kind "$tmp/out" "$tmp/lines"; then
        bad "$name" "output rules"; ok=0
    fi
    if [ $kind = text ]; then
        if ! python3 tools/compare_output.py "$base.expected" "$tmp/out"; then
            bad "$name" "output differs from $base.expected"; ok=0
        fi
    elif ! cmp -s "$base.lines" "$tmp/lines"; then
        bad "$name" "drawing differs from $base.lines"; ok=0
    fi
    bytes=$(wc -c <"$src" | tr -d ' ')
    nlines=$(grep -c . "$tmp/lines")
    ext=$(awk 'NR==1{a=$1;b=$1;c=$2;d=$2}
        {for(i=1;i<=3;i+=2){if($i<a)a=$i;if($i>b)b=$i;j=i+1;if($j<c)c=$j;if($j>d)d=$j}}
        END{if(NR==0)print "0 0 0 0"; else print a,b,c,d}' "$tmp/lines")
    echo "$name $kind $bytes $instr $fns $trig $nlines $ext" >>"$tmp/rows"
    [ $ok -eq 1 ] && echo "ok   $name ($kind, $instr instructions)"
done

if [ $n -gt $MAX_EXAMPLES ]; then bad "examples" "$n examples, at most $MAX_EXAMPLES"; fi
if [ $fail -eq 0 ] || [ $mode = write ]; then
    python3 tools/example_rules.py budget $mode "$tmp/rows" "$doc" || fail=$((fail+1))
    [ $mode = write ] && echo "wrote $doc"
fi
if [ $fail -ne 0 ]; then echo "$fail example problem(s) in $n example(s)"; exit 1; fi
echo "$n example(s) passed"
