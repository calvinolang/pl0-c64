#!/bin/sh
# The on-target suite (make smoke) or any list of make targets, run one
# after the other with a time limit each, then a summary table.
# Usage: tests/smoke.sh [target ...]   (default: the smoke suite below)
# Every target's output goes to build/smoke/TARGET.log; a failed target
# prints its last lines and the run goes on with the next one. Exit 1 if
# any target failed. VICE, its ROMs and cc65 are checked first
# (tests/need_vice.sh): missing, the run fails with install hints, or with
# ALLOW_NO_VICE=1 prints `SKIPPED: VICE not installed` and exits 0.
cd "$(dirname "$0")/.." || exit 2
sh tests/need_vice.sh
case $? in
  0) ;;
  3) echo "SKIPPED: VICE not installed (ALLOW_NO_VICE=1)"; exit 0 ;;
  *) exit 1 ;;
esac
# target:limit in seconds (about 3x the measured time)
SUITE="c64-num:300 c64-gfx:300 c64-io:300 c64-core:300 c64-editor:900
c64-disk:300 c64-mem:120 c64-app:1800 c64-autostart:600"
[ $# -gt 0 ] && SUITE=$*
mkdir -p build/smoke
tab=build/smoke/summary.txt
: > "$tab"
n=0; fail=0; t0=$(date +%s)
total=$(echo $SUITE | wc -w)
for item in $SUITE; do
  t=${item%%:*}; lim=${item#*:}; [ "$lim" = "$item" ] && lim=1800
  n=$((n + 1))
  printf '[%d/%d] make %s ... ' $n "$total" "$t"
  start=$(date +%s)
  timeout "$lim" make --no-print-directory "$t" > "build/smoke/$t.log" 2>&1
  rc=$?
  secs=$(( $(date +%s) - start ))
  if [ $rc -eq 0 ]; then
    res=pass
  else
    res=FAIL; fail=$((fail + 1))
    [ $rc -eq 124 ] && res=TIMEOUT && echo "killed after $lim s" >> "build/smoke/$t.log"
  fi
  echo "$res ($secs s)"
  printf '%-16s %-8s %6d\n' "$t" $res $secs >> "$tab"
  if [ $res != pass ]; then
    tail -n 30 "build/smoke/$t.log"; echo "(log: build/smoke/$t.log)"
  fi
done
echo
printf '%-16s %-8s %6s\n' case result seconds
cat "$tab"
printf '%-16s %-8s %6d\n' total "$((n - fail))/$n" $(( $(date +%s) - t0 ))
[ $fail -eq 0 ]
