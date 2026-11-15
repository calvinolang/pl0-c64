#!/bin/sh
# Run a C64 program in headless VICE and check its text screen.
# Usage: tests/vice_run.sh <prg> <expected-screen-text> <cycle-limit> [fail-text]
#   Starts x64sc headless with warp (tools/vice_screen.py), dumps the screen
#   RAM $0400-$07E7 through the VICE remote monitor about twice a second,
#   converts screen codes to ASCII and greps for the expected text.
#   Stops early when fail-text appears. The whole run is wrapped in
#   `timeout` so it can never hang.
# Exit: 0 expected text found; 1 otherwise (fail text, cycle limit, VICE
# error or timeout); the screen is printed on failure. Work files go to
# build/vice/<prg name>/.
[ $# -ge 3 ] || { echo "usage: $0 <prg> <expected-screen-text> <cycle-limit> [fail-text]"; exit 2; }
prg=$1; expected=$2; cycles=$3; failtext=${4:-}
[ -f "$prg" ] || { echo "vice_run: $prg not found"; exit 1; }
root=$(cd "$(dirname "$0")/.." && pwd)
out=$root/build/vice/$(basename "$prg" .prg)
rm -rf "$out"; mkdir -p "$out"
if [ -n "$failtext" ]; then
  timeout 360 python3 "$root/tools/vice_screen.py" "$prg" "$expected" "$cycles" "$out" "$failtext"
else
  timeout 360 python3 "$root/tools/vice_screen.py" "$prg" "$expected" "$cycles" "$out"
fi
rc=$?
pkill -f -- "-config $out/vicerc" 2>/dev/null
if [ $rc -eq 0 ]; then
  grep -v '^ *$' "$out/screen.txt"
  echo "vice_run: OK, screen shows '$expected'"
  exit 0
fi
[ $rc -eq 124 ] && echo "vice_run: TIMEOUT after 360 s"
echo "vice_run: FAILED (rc=$rc), last screen:"
[ -f "$out/screen.txt" ] && cat "$out/screen.txt"
exit 1
