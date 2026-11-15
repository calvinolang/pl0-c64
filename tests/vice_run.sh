#!/bin/sh
# Run a C64 program in headless VICE and check its text screen.
# Usage: tests/vice_run.sh [options] <prg> <expected-line> <cycle-limit> [fail-text]
#   Starts x64sc headless with warp (tools/vice_screen.py), dumps the screen
#   RAM $0400-$07E7 through the VICE remote monitor about twice a second,
#   converts screen codes to ASCII and looks for a screen line EQUAL to
#   expected-line (trailing spaces ignored). A line STARTING with fail-text
#   stops the run as a failure (checked first). The whole run is wrapped in
#   `timeout` so it can never hang.
# Options:
#   --on TRIGGER CMD  send monitor command CMD once a screen line equals
#                     TRIGGER (key injection, no quotes around the keys:
#                     VICE would type them: --on feed1 'keybuf 12.5\n');
#                     repeatable, fired in order (see tools/vice_screen.py)
#   --vice ARG        pass ARG to x64sc (repeatable: --vice -8 --vice d.d64)
#   --as-user         VICE's default autostart (a .prg through a disk image
#                     and the drive) instead of injecting the .prg into RAM
#   --snap            RAM snapshot handshake ($02A7/$02A8): OUTDIR/snapN.bin
#                     holds all 64 KB of RAM at step N (tests/c64/gfx_check.c)
#   --check CMD       after the screen check passed, run `CMD OUTDIR` (word
#                     split) and fail if it fails (e.g. tools/check_bitmap.py)
#   --work NAME       work directory build/vice/NAME instead of the prg name
# Exit: 0 expected line found (and the check passed); 1 otherwise (fail
# text, cycle limit, VICE error, timeout or failed check); the screen is
# printed on failure. Work files go to build/vice/<prg name>/.
check=; work=
n=$#; i=0
while [ $i -lt $n ]; do            # pull --check/--work out, keep everything else
  a=$1; shift; i=$((i + 1))
  if [ "$a" = --check ]; then check=$1; shift; i=$((i + 1)); continue; fi
  if [ "$a" = --work ]; then work=$1; shift; i=$((i + 1)); continue; fi
  set -- "$@" "$a"
done
prg=; expected=; skip=0; npos=0
for a in "$@"; do                  # the first positional argument is the prg
  if [ $skip -gt 0 ]; then skip=$((skip - 1)); continue; fi
  case $a in
    --on) skip=2 ;;
    --vice) skip=1 ;;
    --snap|--as-user) ;;
    *) npos=$((npos + 1))
       [ $npos -eq 1 ] && prg=$a
       [ $npos -eq 2 ] && expected=$a ;;
  esac
done
[ $npos -ge 3 ] || { echo "usage: $0 [--on T C]... [--snap] [--as-user] [--check CMD] [--work NAME] <prg> <expected-line> <cycle-limit> [fail-text]"; exit 2; }
[ -f "$prg" ] || { echo "vice_run: $prg not found"; exit 1; }
root=$(cd "$(dirname "$0")/.." && pwd)
out=$root/build/vice/${work:-$(basename "$prg" .prg)}
rm -rf "$out"; mkdir -p "$out"
timeout 360 python3 "$root/tools/vice_screen.py" --out "$out" "$@"
rc=$?
pkill -f -- "-config $out/vicerc" 2>/dev/null
if [ $rc -eq 0 ]; then
  grep -v '^ *$' "$out/screen.txt"
  if [ -n "$check" ]; then
    $check "$out" || { echo "vice_run: FAILED check: $check $out"; exit 1; }
  fi
  echo "vice_run: OK, screen shows the line '$expected'"
  exit 0
fi
[ $rc -eq 124 ] && echo "vice_run: TIMEOUT after 360 s"
echo "vice_run: FAILED (rc=$rc), last screen:"
[ -f "$out/screen.txt" ] && cat "$out/screen.txt"
exit 1
