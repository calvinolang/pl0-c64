#!/bin/sh
# Autostart acceptance (make c64-autostart): the first minute a user has.
# Usage: tests/vice_autostart.sh [d64|prg ...]
#
# x64sc is started the way a user starts it, `x64sc -autostart
# build/pl0.d64` (case d64) or `x64sc -autostart build/pl0.prg` (case
# prg): VICE's default settings (`-default`: the stock KERNAL, BASIC and
# character ROMs, drive 8 a 1541-II) with true drive emulation on, and
# VICE's own autostart (tests/vice_run.sh --as-user): for the disk it
# types LOAD"*",8,1 and RUN, and the first file is PL0; a .prg goes
# through a temporary disk image and the drive (AutostartPrgMode 2). Only
# warp is added (it changes wall time, not emulated cycles).
# Checked:
#   - the editor boots within BOOT_LIMIT cycles of power-on: a breakpoint
#     on main() (from -moncommands, so it is set before anything runs)
#     gives the exact cycle; then the boot screen: status bar
#     ` PL/0   SQUARE  ...  L 1  C 1`, the key bar
#     ` F1 Run F2 New F3 Open F5 Save F7 Help`, both reverse, row 1 the
#     starter's first line, cursor at line 1 column 1;
#   - F1 draws the SQUARE starter: the bitmap against the host rendering
#     (pl0host --pbm) with the pixel count, box and overlap checks of
#     tools/check_c64_run.py, at `-- SPACE: DRAWING  ANY KEY: EDITOR --`.
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root" || exit 2
PRG=build/pl0.prg
IMG=build/pl0.d64
LBL=build/c64/pl0.lbl
# measured: main() at about 79 M cycles from the disk (a 28 KB load at
# 1541 speed); the limit leaves room, and the whole run must end by CYCLES
BOOT_LIMIT=${AUTOSTART_BOOT_LIMIT:-120000000}
CYCLES=${AUTOSTART_CYCLES:-200000000}
W=$root/build/vice/autostart
BAR=' F1 Run F2 New F3 Open F5 Save F7 Help'
DRAWKEY='-- SPACE: DRAWING  ANY KEY: EDITOR --'
boot=$(printf ' PL/0   %-16sL %s  C %s' SQUARE 1 1)

[ -f $PRG ] && [ -f $IMG ] || { echo "vice_autostart: $PRG or $IMG missing (make c64 disk)"; exit 1; }
[ -x build/pl0host ] || { echo "vice_autostart: build/pl0host missing (make host)"; exit 1; }
main=$(awk '$3 == "._main" { print substr($2, 3) }' $LBL)
[ -n "$main" ] || { echo "vice_autostart: _main not in $LBL"; exit 1; }
mkdir -p "$W"
echo "break $main" > "$W/moncommands.txt"
build/pl0host --pbm "$W/square.pbm" examples/square.pl0 > /dev/null || exit 1
EXP=$W/expect.txt
printf '%s\n' "screen boot.bin" "row 0 $boot" "reverse 0" "row 24 $BAR" "reverse 24" \
  "row 1 [ a square ]" "cursor 1 0" "pbm draw.bin $W/square.pbm" > "$EXP"

ALL="d64 prg"
[ $# -gt 0 ] && ALL=$*
pass=0; fail=0; failed=
for CASE in $ALL; do
  case $CASE in
    d64) file=$W/pl0.d64; cp $IMG "$file" ;;   # a copy: nothing may write to build/pl0.d64
    prg) file=$PRG ;;
    *) echo "vice_autostart: unknown case $CASE (d64 or prg)"; exit 2 ;;
  esac
  out=$root/build/vice/autostart_$CASE
  start=$(date +%s)
  sh tests/vice_run.sh --as-user --work "autostart_$CASE" --check "python3 tools/check_app.py $EXP" \
    --vice -drive8truedrive --vice -moncommands --vice "$W/moncommands.txt" \
    --on "$boot" "bsave \"$out/boot.bin\" 0 0400 07e7" --on "$boot" 'keybuf \x85' \
    --on "$DRAWKEY" pause --on "$DRAWKEY" "bsave \"$out/draw.bin\" 0 0000 ffff" \
    "$file" "$DRAWKEY" "$CYCLES" > "$W/$CASE.log" 2>&1
  rc=$?
  cyc=$(head -1 "$out/stops.txt" 2>/dev/null)
  case $cyc in
    ''|*[!0-9]*) echo "vice_autostart: main() never reached (no breakpoint stop)" >> "$W/$CASE.log"; rc=1; secs=- ;;
    *) secs=$(awk -v c="$cyc" 'BEGIN { printf "%.1f", c / 985248 }')
       if [ "$cyc" -gt "$BOOT_LIMIT" ]; then
         echo "vice_autostart: main() at cycle $cyc, over the limit $BOOT_LIMIT" >> "$W/$CASE.log"; rc=1
       fi ;;
  esac
  if [ $rc -eq 0 ]; then
    pass=$((pass + 1))
    echo "PASS autostart_$CASE (main() at cycle $cyc = $secs s after power-on; $(( $(date +%s) - start )) s wall)"
  else
    fail=$((fail + 1)); failed="$failed $CASE"
    echo "FAIL autostart_$CASE (log: build/vice/autostart/$CASE.log)"
    tail -n 40 "$W/$CASE.log"
  fi
done
echo "vice_autostart: $pass passed, $fail failed${failed:+:$failed}"
[ $fail -eq 0 ]
