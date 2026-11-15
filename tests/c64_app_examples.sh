#!/bin/sh
# The final acceptance of the product (make c64-app-examples): every
# built-in example through the real program, the way a user runs it.
# Usage: tests/c64_app_examples.sh [--record] [name ...]
#
# For each example, a fresh VICE with build/pl0.prg and a fresh copy of
# build/pl0.d64 in a 1541 with true drive emulation (tests/vice_run.sh):
# F3, CRSR down to the example (its place in INDEX: tools/mkdisk.py
# --index), RETURN, F1, wait for the end-of-run prompt, then a key back to
# the editor. Checked (tools/check_app.py):
#   - text examples: the output rows above `-- PRESS ANY KEY --` against
#     examples/NAME.expected (tools/compare_output.py, numbers within 1e-6);
#   - drawings: the bitmap at `-- SPACE: DRAWING  ANY KEY: EDITOR --`
#     against the host rendering (pl0host --pbm) with the count, box and
#     overlap checks of tools/check_c64_run.py;
#   - back in the editor: status `NAME  L 1  C 1` and the editor's text
#     equal to examples/NAME.pl0 byte for byte.
# Time: the harness resets VICE's stopwatch (CPU cycles) when it types F1
# and sets a breakpoint on turtle_used(), which main.c calls once, right
# after pl0_run returns; tools/vice_screen.py reads the stopwatch where
# the CPU stopped (stops.txt). Seconds = cycles / 985248 (PAL): the time
# from F1 to the end of the program, compile included, through the real
# editor, as a user sees it. Fails if an example needs more than LIMIT s.
# --record writes the times to docs/architecture/c64-app-times.tsv, the
# "C64 app run (s)" column of examples-budget.md (then run
# tests/run_examples.sh --write-budget).
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root" || exit 2
PRG=build/pl0.prg
IMG=build/pl0.d64
LBL=build/c64/pl0.lbl
LIMIT=30
CYCLES=${APP_EX_CYCLES:-400000000}  # disk reads with TDE + a run measured past 30 s
OUT=$root/build/vice/pl0            # tests/vice_run.sh's work directory
W=$root/build/vice/app_examples
BAR=' F1 Run F2 New F3 Open F5 Save F7 Help'
PICK=' RETURN open  RUN/STOP back  CRSR move'
KEY='-- PRESS ANY KEY --'
DRAWKEY='-- SPACE: DRAWING  ANY KEY: EDITOR --'
DRIVE="--vice -drive8type --vice 1541 --vice -drive8truedrive --vice +virtualdev8"
record=0
[ "$1" = --record ] && { record=1; shift; }

status() { printf ' PL/0   %-16sL %s  C %s' "$1" "$2" "$3"; }
downs() { [ "$1" -gt 0 ] && printf '\\x11%.0s' $(seq "$1"); }
dump() { printf 'bsave "%s/%s" 0 %s' "$OUT" "$1" "$2"; }

[ -f $PRG ] && [ -f $IMG ] || { echo "c64_app_examples: $PRG or $IMG missing (make c64 disk)"; exit 1; }
[ -x build/pl0host ] || { echo "c64_app_examples: build/pl0host missing (make host)"; exit 1; }
brk=$(awk '$3 == "._turtle_used" { print substr($2, 3) }' $LBL)
[ -n "$brk" ] || { echo "c64_app_examples: _turtle_used not in $LBL"; exit 1; }
mkdir -p "$W"
python3 tools/mkdisk.py --index examples | awk '{ print $1 }' > "$W/order.txt" || exit 1
names=$*
[ -n "$names" ] || names=$(tr 'A-Z' 'a-z' < "$W/order.txt")
total=$(echo $names | wc -w)
tsv=$W/times.tsv
: > "$tsv"
n=0; fail=0
for name in $names; do
  n=$((n + 1))
  NAME=$(echo "$name" | tr 'a-z' 'A-Z')
  pos=$(grep -nx "$NAME" "$W/order.txt" | cut -d: -f1)
  [ -n "$pos" ] || { echo "c64_app_examples: $NAME is not in INDEX"; fail=$((fail + 1)); continue; }
  img=$W/$name.d64
  cp $IMG "$img"
  EXP=$W/expect_$name.txt
  on=$(status "$NAME" 1 1)
  if [ -f "examples/$name.lines" ]; then
    kind=drawing
    build/pl0host --pbm "$W/$name.pbm" "examples/$name.pl0" > /dev/null || exit 1
    endp=$DRAWKEY
    printf '%s\n' "pbm draw.bin $W/$name.pbm" "screen out.bin" "row 0 $DRAWKEY" > "$EXP"
  else
    kind=text
    endp=$KEY
    printf '%s\n' "output out.bin examples/$name.expected" > "$EXP"
  fi
  printf '%s\n' "screen" "row 0 $on" "row 24 $BAR" "text mem.bin examples/$name.pl0" >> "$EXP"
  printf '[%d/%d] %-10s (%s, INDEX entry %d) ... ' $n "$total" "$NAME" $kind "$pos"
  start=$(date +%s)
  sh tests/vice_run.sh --check "python3 tools/check_app.py $EXP" $DRIVE --vice -8 --vice "$img" \
    --on "$(status SQUARE 1 1)" 'keybuf \x86' \
    --on "$PICK" "keybuf $(downs $((pos - 1)))\\n" \
    --on "$on" "break $brk" --on "$on" 'sw reset' --on "$on" 'keybuf \x85' \
    --on "$endp" pause \
    --on "$endp" "$(dump out.bin '0400 07e7')" --on "$endp" "$(dump draw.bin '0000 ffff')" \
    --on "$endp" 'keybuf x' \
    --on "$on" "$(dump mem.bin '0000 ffff')" \
    $PRG "$on" "$CYCLES" > "$W/$name.log" 2>&1
  rc=$?
  wall=$(( $(date +%s) - start ))
  cyc=$(head -1 "$OUT/stops.txt" 2>/dev/null)
  secs=-
  case $cyc in
    ''|*[!0-9]*) [ $rc -eq 0 ] && { echo "no stopwatch reading (breakpoint at \$$brk never hit)" >> "$W/$name.log"; rc=1; } ;;
    *) secs=$(awk -v c="$cyc" 'BEGIN { printf "%.2f", c / 985248 }') ;;
  esac
  result=ok
  if [ $rc -ne 0 ]; then
    result=FAIL
  elif awk -v t="$secs" -v l=$LIMIT 'BEGIN { exit !(t > l) }'; then
    result=SLOW
  fi
  printf '%s\t%s\t%s\t%s\t%s\n' "$name" $kind $result "$secs" $wall >> "$tsv"
  echo "$result, $secs s on the C64 ($wall s wall)"
  if [ $result != ok ]; then
    fail=$((fail + 1))
    [ $result = FAIL ] && { tail -n 40 "$W/$name.log"; echo "(log: build/vice/app_examples/$name.log)"; }
  fi
done
echo
printf '%-12s %-8s %-6s %14s %12s %8s\n' example kind result "C64 app run s" "core run s" "wall s"
while IFS='	' read -r name kind result secs wall; do
  core=$(awk -v n="$name" '$1 == n { print $2 }' docs/architecture/c64-times.tsv)
  printf '%-12s %-8s %-6s %14s %12s %8s\n' "$name" $kind $result "$secs" "${core:--}" $wall
done < "$tsv"
if [ $record -eq 1 ]; then
  { echo "# example	C64 seconds from F1 to the end of the run, through the program (tests/c64_app_examples.sh --record)"
    sort "$tsv" | awk -F'\t' '{ print $1 "\t" $4 }'; } > docs/architecture/c64-app-times.tsv
  echo "wrote docs/architecture/c64-app-times.tsv (now run tests/run_examples.sh --write-budget)"
fi
if [ $fail -ne 0 ]; then echo "c64_app_examples: $fail of $n example(s) failed or took more than $LIMIT s"; exit 1; fi
echo "c64_app_examples: $n example(s) passed through the program, each within $LIMIT s from F1"
