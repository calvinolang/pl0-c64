#!/bin/sh
# On-target tests of the whole C64 program (Task 15: run flow, picker,
# disk save/load) in headless VICE. Usage: tests/vice_app.sh [case ...]
#
# Like tests/vice_smoke.sh: every case starts build/pl0.prg from scratch
# (tests/vice_run.sh), types keys through the monitor `keybuf` command when
# a screen line equals a trigger (--on; `pause` lets the program react
# before the next trigger), and waits for a final screen line, which
# counts only after every trigger fired. tools/check_app.py then checks
# rows of the final screen and dumps saved by the case with the monitor:
# the editor's text in a 64 KB RAM dump, a screen dump, a VIC register,
# the bitmap against the host rendering.
#
# The drive: a copy of build/pl0.d64 (or of an image the case builds with
# c1541) attached as device 8 of a 1541 with true drive emulation (the
# drive runs its own DOS: the real serial protocol, SEQ files and error
# channel; VICE's virtual device, the file system traps, is off). After a
# run the case reads files back from its image with c1541 to prove the
# bytes on the disk. no_disk runs with the drive but no disk, and with no
# drive at all. Never use the virtual device without an image: it then
# reads and writes the host's current directory.
#
# Key codes (PETSCII, \xHH for keybuf): F1 85, F2 89, F3 86, F5 87,
# CRSR down 11, up 91, right 1D, left 9D, RETURN 0D (\n), RUN/STOP 03,
# DEL 14. VICE types lower case letters as the unshifted keys.
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root" || exit 2
PRG=${APP_PRG:-build/pl0.prg}       # APP_PRG: another build (e.g. an older one, to see the cases fail)
IMG=build/pl0.d64
CYCLES=${APP_CYCLES:-200000000}
OUT=$root/build/vice/pl0           # tests/vice_run.sh's work directory
W=$root/build/vice/app             # the cases' images and files
BAR=' F1 Run F2 New F3 Open F5 Save F7 Help'
PICK=' RETURN open  RUN/STOP back  CRSR move'
KEY='-- PRESS ANY KEY --'
DRAWKEY='-- SPACE: DRAWING  ANY KEY: EDITOR --'
mkdir -p "$W"

status() { printf ' PL/0   %-16sL %s  C %s' "$1" "$2" "$3"; }
entry() { printf ' %-12s %s' "$1" "$2"; }             # a picker row
downs() { printf '\\x11%.0s' $(seq "$1"); }           # n x CRSR down
dump() { printf 'bsave "%s/%s" 0 %s' "$OUT" "$1" "$2"; }   # monitor save
boot=$(status SQUARE 1 1)

expect() { EXP=$W/expect_$CASE.txt; printf '%s\n' "$@" > "$EXP"; }

# drive options: true drive emulation, a disk image or none
DRIVE="--vice -drive8type --vice 1541 --vice -drive8truedrive --vice +virtualdev8"

# run IMAGE EXPECTED [--on TRIGGER CMD]...: one VICE run with the image
# (`none`: no disk in the drive; `nodrive`: no drive 8 at all)
run() {
  img=$1; want=$2; shift 2
  # the disk options go in front of the triggers as separate words (the
  # image path may contain spaces)
  case $img in
    none) ;;
    nodrive) set -- --vice -drive8type --vice 0 "$@" ;;
    *) set -- --vice -8 --vice "$img" "$@" ;;
  esac
  # shellcheck disable=SC2086
  sh tests/vice_run.sh --check "python3 tools/check_app.py '$EXP'" $DRIVE \
    "$@" "$PRG" "$want" "$CYCLES" > "$W/$CASE.log" 2>&1 ||
    { cat "$W/$CASE.log"; return 1; }
  cat "$W/$CASE.log" >> "$W/$CASE.all.log"
}

fresh() { cp "$IMG" "$W/$1.d64"; echo "$W/$1.d64"; }

# disk_file IMAGE NAME WANTFILE: the SEQ file NAME on the image holds the
# bytes of WANTFILE
disk_file() {
  rm -f "$W/got.bin"
  c1541 "$1" -read "$(echo "$2" | tr 'A-Z' 'a-z'),s" "$W/got.bin" > /dev/null 2>&1
  cmp -s "$3" "$W/got.bin" || { echo "vice_app: $2 on $1 differs from $3"; return 1; }
}

# ---- 1: open FACTORIAL with the picker (8th entry), run it, back
case_run_factorial() {
  img=$(fresh "$CASE")
  expect "row 0 $(status FACTORIAL 2 1)" "row 24 $BAR" "text mem.bin examples/factorial.pl0" \
    "screen out.bin" "row 9 10! = 3628800" "row 11 12! = 479001600" "row 12 $KEY"
  run "$img" "$(status FACTORIAL 2 1)" \
    --on "$boot" 'keybuf \x86' \
    --on "$(entry FACTORIAL 'factorials 1! to 12!')" "keybuf $(downs 7)\\n" \
    --on "$(status FACTORIAL 1 1)" 'keybuf \x85' \
    --on "$KEY" "$(dump out.bin '0400 07e7')" --on "$KEY" 'keybuf x' \
    --on "$(status FACTORIAL 1 1)" 'keybuf \x11' \
    --on "$(status FACTORIAL 2 1)" "$(dump mem.bin '0000 ffff')"
}

# ---- 2: a compile error stays in the editor; a runtime error shows on
# the text screen after the output, then in the editor on its line
case_run_error() {
  img=$(fresh "$CASE")
  printf '!x.' > "$W/err1.pl0"
  expect "row 24 ERROR line 1: undeclared name" "reverse 24" "cursor 1 0" \
    "row 0 $(status 'NEW*' 1 1)" "text mem.bin $W/err1.pl0"
  run "$img" 'ERROR line 1: undeclared name' \
    --on "$boot" 'keybuf \x89!x.\x85' \
    --on 'ERROR line 1: undeclared name' "$(dump mem.bin '0000 ffff')" || return 1
  printf 'begin !7; !1/0 end.' > "$W/err2.pl0"
  expect "row 24 ERROR line 1: division by zero" "reverse 24" "cursor 1 0" \
    "row 1 begin !7; !1/0 end." "text mem.bin $W/err2.pl0" \
    "screen out.bin" "row 0 7" "row 1 ERROR line 1: division by zero" "row 2 $KEY"
  run "$img" "$(status 'NEW*' 1 1)" \
    --on "$boot" 'keybuf \x89begin !7; !1/0 end.\x85' \
    --on "$KEY" "$(dump out.bin '0400 07e7')" --on "$KEY" 'keybuf x' \
    --on "$(status 'NEW*' 1 1)" "$(dump mem.bin '0000 ffff')"
}

# ---- 3: SQUARE from the picker draws like the host; SPACE shows the
# text and the drawing again ($D011 bit 5: bitmap mode); x: the editor
case_drawing_runs() {
  img=$(fresh "$CASE")
  build/pl0host --pbm "$W/square.pbm" examples/square.pl0 > /dev/null || return 1
  expect "pbm draw.bin $W/square.pbm" "byte d1.bin 20 20" "byte d2.bin 20 00" \
    "byte d3.bin 20 20" "byte d4.bin 20 00" "text mem.bin examples/square.pl0" \
    "row 0 $boot" "row 24 $BAR" "screen out.bin" "row 0 $DRAWKEY"
  run "$img" "$boot" \
    --on "$boot" 'keybuf \x86' \
    --on "$PICK" 'keybuf \n' \
    --on "$(status SQUARE 1 1)" 'keybuf \x85' \
    --on "$DRAWKEY" pause \
    --on "$DRAWKEY" "$(dump draw.bin '0000 ffff')" --on "$DRAWKEY" "$(dump out.bin '0400 07e7')" \
    --on "$DRAWKEY" 'bank io' --on "$DRAWKEY" "$(dump d1.bin 'd011 d011')" --on "$DRAWKEY" 'bank ram' \
    --on "$DRAWKEY" 'keybuf \x20' --on "$DRAWKEY" pause \
    --on "$DRAWKEY" 'bank io' --on "$DRAWKEY" "$(dump d2.bin 'd011 d011')" --on "$DRAWKEY" 'bank ram' \
    --on "$DRAWKEY" 'keybuf \x20' --on "$DRAWKEY" pause \
    --on "$DRAWKEY" 'bank io' --on "$DRAWKEY" "$(dump d3.bin 'd011 d011')" --on "$DRAWKEY" 'bank ram' \
    --on "$DRAWKEY" 'keybuf x' --on "$boot" pause \
    --on "$boot" 'bank io' --on "$boot" "$(dump d4.bin 'd011 d011')" --on "$boot" 'bank ram' \
    --on "$boot" "$(dump mem.bin '0000 ffff')"
}

# ---- 4: RUN/STOP ends an endless loop: `stopped`, the text intact
case_stop_with_runstop() {
  img=$(fresh "$CASE")
  p='var i; begin i := 0; while 1 = 1 do i := i + 1 end.'
  printf '%s' "$p" > "$W/loop.pl0"
  expect "row 24 ERROR line 1: stopped" "cursor 1 0" "text mem.bin $W/loop.pl0" \
    "screen out.bin" "row 0 ERROR line 1: stopped" "row 1 $KEY"
  run "$img" "$(status 'NEW*' 1 1)" \
    --on "$boot" "keybuf \\x89$p" \
    --on "$(status 'NEW*' 1 $((${#p} + 1)))" 'keybuf \x85' --on "$(status 'NEW*' 1 $((${#p} + 1)))" pause \
    --on '' 'keybuf \x03' \
    --on "$KEY" "$(dump out.bin '0400 07e7')" --on "$KEY" 'keybuf x' \
    --on "$(status 'NEW*' 1 1)" "$(dump mem.bin '0000 ffff')"
}

# ---- 5: save TEST, read it back from the image, F2, open TEST again
case_save_roundtrip() {
  img=$(fresh "$CASE")
  printf 'var a;\nbegin a := 6*7; !a end.' > "$W/test.pl0"
  expect "row 0 $(status TEST 1 1)" "row 1 var a;" "row 24 $BAR" "text mem.bin $W/test.pl0" \
    "screen saved.bin" "row 24 SAVED TEST" "row 0 $(status TEST 2 24)" \
    "screen pick.bin" "row 0  OPEN PROGRAM" "row 17  TEST" "reverse 17" "row 24 $PICK"
  run "$img" "$(status TEST 1 1)" \
    --on "$boot" 'keybuf \x89var a;\nbegin a := 6*7; !a end.\x87' \
    --on 'SAVE AS:' 'keybuf test\n' \
    --on 'SAVED TEST' "$(dump saved.bin '0400 07e7')" --on 'SAVED TEST' 'keybuf \x89' \
    --on "$(status NEW 1 1)" 'keybuf \x86' \
    --on "$PICK" "keybuf $(downs 15)" --on ' TEST' pause \
    --on ' TEST' "$(dump pick.bin '0400 07e7')" --on ' TEST' 'keybuf \n' \
    --on "$(status TEST 1 1)" "$(dump mem.bin '0000 ffff')" || return 1
  disk_file "$img" TEST "$W/test.pl0"
}

# ---- 6: bad names (empty, *, /, 13 characters): BAD NAME and nothing
# written; saving over SQUARE asks: N keeps the old bytes, Y replaces them
case_save_bad_name() {
  img=$(fresh "$CASE")
  c1541 "$img" -list > "$W/before.txt"
  expect "row 24 $BAR" "row 1 !1."
  run "$img" "$(status 'NEW*' 1 3)" \
    --on "$boot" 'keybuf \x89!1.\x87' \
    --on 'SAVE AS:' 'keybuf \n' --on 'BAD NAME' 'keybuf \x87' \
    --on 'SAVE AS:' 'keybuf a*b\n' --on 'BAD NAME' 'keybuf \x87' \
    --on 'SAVE AS:' 'keybuf a/b\n' --on 'BAD NAME' 'keybuf \x87' \
    --on 'SAVE AS:' 'keybuf abcdefghijklm\n' --on 'BAD NAME' 'keybuf \x9d' || return 1
  c1541 "$img" -list > "$W/after.txt"
  cmp -s "$W/before.txt" "$W/after.txt" || { echo "vice_app: the directory changed"; return 1; }
  printf '!1.' > "$W/one.pl0"
  expect "row 24 NOT SAVED" "row 0 $(status 'NEW*' 1 4)"
  run "$img" 'NOT SAVED' \
    --on "$boot" 'keybuf \x89!1.\x87' \
    --on 'SAVE AS:' 'keybuf square\n' --on 'REPLACE? (Y/N)' 'keybuf n' || return 1
  disk_file "$img" SQUARE examples/square.pl0 || return 1
  expect "row 24 SAVED SQUARE" "row 0 $(status SQUARE 1 4)"
  run "$img" 'SAVED SQUARE' \
    --on "$boot" 'keybuf \x89!1.\x87' \
    --on 'SAVE AS:' 'keybuf square\n' --on 'REPLACE? (Y/N)' 'keybuf y' || return 1
  disk_file "$img" SQUARE "$W/one.pl0"
}

# an image with user files: U01..U09 (written in a mixed order), BIG (9000
# bytes), CRLF (CR LF line ends, a tab, a CR alone)
picker_image() {
  img=$W/$CASE.d64
  cp "$IMG" "$img"
  for u in U05 U02 U09 U01 U07 U03 U08 U04 U06; do
    printf '!%s.\n' "$u" | tr 'U' '1' > "$W/$u.pl0"
    c1541 "$img" -write "$W/$u.pl0" "$(echo $u | tr 'U' 'u'),s" > /dev/null || return 1
  done
  python3 -c "import sys; sys.stdout.write(('{ ' + 'x' * 86 + ' }\n') * 100)" > "$W/big.pl0"
  c1541 "$img" -write "$W/big.pl0" "big,s" > /dev/null || return 1
  printf 'var a;\r\nbegin\ta := 1;\r!a\nend.\r\n' > "$W/crlf.raw"
  printf 'var a;\nbegin a := 1;\n!a\nend.\n' > "$W/crlf.pl0"
  c1541 "$img" -write "$W/crlf.raw" "crlf,s" > /dev/null || return 1
  # a name with a space: the program cannot open it, so it is not listed
  # (it would sort between CRLF and U01 and move every row below)
  c1541 "$img" -write "$W/crlf.pl0" "my prog,s" > /dev/null || return 1
  echo "$img"
}

# ---- 7: the list: examples, then BIG CRLF U01..U09 sorted (26 entries,
# 21 rows): CRSR down to the end scrolls and stops; RUN/STOP and the
# discard question; FILE TOO BIG; CR LF line ends
case_picker() {
  img=$(picker_image) || return 1
  expect "row 0 $(status U09 1 1)" "text mem.bin $W/U09.pl0" \
    "screen first.bin" "row 2 $(entry SQUARE 'a square')" "reverse 2" \
    "row 9 $(entry FACTORIAL 'factorials 1! to 12!')" "row 16 $(entry TREE 'fractal tree')" \
    "row 17  BIG" "row 18  CRLF" "row 19  U01" "row 22  U04" \
    "screen last.bin" "row 2 $(entry SINE 'plot of sin(x)')" "row 22  U09" "reverse 22" \
    "row 21  U08"
  run "$img" "$(status U09 1 1)" \
    --on "$boot" 'keybuf \x86' \
    --on "$PICK" "$(dump first.bin '0400 07e7')" --on "$PICK" "keybuf $(downs 27)" \
    --on ' U09' pause --on ' U09' "$(dump last.bin '0400 07e7')" --on ' U09' 'keybuf \x91\x11\n' \
    --on "$(status U09 1 1)" "$(dump mem.bin '0000 ffff')" || return 1
  # z modifies; F3 asks: N stays, Y opens the list; RUN/STOP: back, unchanged
  { printf 'z'; cat examples/square.pl0; } > "$W/zsquare.pl0"
  expect "row 0 $(status 'SQUARE*' 1 3)" "row 24 $BAR" "text mem.bin $W/zsquare.pl0"
  run "$img" "$(status 'SQUARE*' 1 3)" \
    --on "$boot" 'keybuf z\x86' \
    --on 'DISCARD PROGRAM? (Y/N)' 'keybuf n' --on "$(status 'SQUARE*' 1 2)" pause \
    --on "$(status 'SQUARE*' 1 2)" 'keybuf \x86' --on 'DISCARD PROGRAM? (Y/N)' 'keybuf y' \
    --on "$PICK" 'keybuf \x03' --on "$(status 'SQUARE*' 1 2)" 'keybuf \x1d' \
    --on "$(status 'SQUARE*' 1 3)" "$(dump mem.bin '0000 ffff')" || return 1
  expect "row 0 $boot" "row 24 FILE TOO BIG" "text mem.bin examples/square.pl0"
  run "$img" 'FILE TOO BIG' \
    --on "$boot" 'keybuf \x86' --on "$PICK" "keybuf $(downs 15)\\n" \
    --on 'FILE TOO BIG' "$(dump mem.bin '0000 ffff')" || return 1
  expect "row 0 $(status CRLF 1 1)" "row 2 begin a := 1;" "text mem.bin $W/crlf.pl0"
  run "$img" "$(status CRLF 1 1)" \
    --on "$boot" 'keybuf \x86' --on "$PICK" "keybuf $(downs 16)\\n" \
    --on "$(status CRLF 1 1)" "$(dump mem.bin '0000 ffff')"
}

# ---- 7b: an INDEX entry whose file is gone: a plain message, the text
# and the name unchanged, the editor still works
case_missing_file() {
  img=$(fresh "$CASE")
  c1541 "$img" -delete polygon > /dev/null || return 1
  { printf 'k'; cat examples/square.pl0; } > "$W/ksquare.pl0"
  expect "row 0 $(status 'SQUARE*' 1 2)" "row 24 $BAR" "text mem.bin $W/ksquare.pl0" \
    "screen msg.bin" "row 24 DISK ERROR: FILE NOT FOUND" "reverse 24" "row 0 $boot"
  run "$img" "$(status 'SQUARE*' 1 2)" \
    --on "$boot" 'keybuf \x86' --on "$PICK" 'keybuf \x11\n' \
    --on 'DISK ERROR: FILE NOT FOUND' "$(dump msg.bin '0400 07e7')" \
    --on 'DISK ERROR: FILE NOT FOUND' 'keybuf k' \
    --on "$(status 'SQUARE*' 1 2)" "$(dump mem.bin '0000 ffff')"
}

# ---- 7c: the disk goes away in the middle of a load (the monitor
# detaches the image while the 1541 reads): the drive's error (shown as NO
# DISK), not a load: text and name unchanged; the editor still works.
# (Turning drive 8 off instead, `resourceset "Drive8Type" "0"`, hangs the
# KERNAL's serial receive, which waits for the clock line without a
# timeout: as a real drive switched off mid-transfer can. A READST error
# with a fine drive status, DK_READ, could not be injected.)
case_read_error() {
  img=$W/$CASE.d64
  cp "$IMG" "$img"
  python3 -c "
import sys
for i in range(205): sys.stdout.write('{ line %03d abcdefghijklmnopqrstuvwxy }\n' % i)" > "$W/full.pl0"
  c1541 "$img" -write "$W/full.pl0" "full,s" > /dev/null || return 1
  expect "screen msg.bin" "row 24 NO DISK" "row 0 $boot" "row 1 [ a square ]" \
    "screen" "row 0 $(status SQUARE 1 2)" "row 24 $BAR"
  run "$img" "$(status SQUARE 1 2)" \
    --on "$boot" 'keybuf \x86' --on "$PICK" "keybuf $(downs 15)\\n" \
    --on 'LOADING FULL' 'detach 8' \
    --on "$boot" "$(dump msg.bin '0400 07e7')" --on "$boot" 'keybuf \x1d'
}

# ---- 3b: input after a drawing is read on the text screen; the next
# line shows the drawing again; a runtime error after a drawing shows on
# the text screen, then in the editor
case_input_after_drawing() {
  img=$(fresh "$CASE")
  p='var a; begin forward(50); ?a; forward(a) end.'
  printf 'begin forward(50); forward(30) end.' > "$W/fw.pl0"
  build/pl0host --pbm "$W/fw.pbm" "$W/fw.pl0" > /dev/null || return 1
  expect "byte d1.bin 20 00" "pbm draw.bin $W/fw.pbm" "byte d2.bin 20 20" \
    "screen ask.bin" "row 0 ?"
  run "$img" "$(status 'NEW*' 1 $((${#p} + 1)))" \
    --on "$boot" "keybuf \\x89$p\\x85" \
    --on '?' "$(dump ask.bin '0400 07e7')" \
    --on '?' 'bank io' --on '?' "$(dump d1.bin 'd011 d011')" --on '?' 'bank ram' \
    --on '?' 'keybuf 30\n' \
    --on "$DRAWKEY" pause \
    --on "$DRAWKEY" "$(dump draw.bin '0000 ffff')" \
    --on "$DRAWKEY" 'bank io' --on "$DRAWKEY" "$(dump d2.bin 'd011 d011')" --on "$DRAWKEY" 'bank ram' \
    --on "$DRAWKEY" 'keybuf x' || return 1
  printf 'begin forward(50); !1/0 end.' > "$W/err3.pl0"
  expect "row 24 ERROR line 1: division by zero" "cursor 1 0" "text mem.bin $W/err3.pl0" \
    "screen out.bin" "row 0 ERROR line 1: division by zero" "row 1 $KEY" "byte d1.bin 20 00"
  run "$img" "$(status 'NEW*' 1 1)" \
    --on "$boot" 'keybuf \x89begin forward(50); !1/0 end.\x85' \
    --on "$KEY" "$(dump out.bin '0400 07e7')" \
    --on "$KEY" 'bank io' --on "$KEY" "$(dump d1.bin 'd011 d011')" --on "$KEY" 'bank ram' \
    --on "$KEY" 'keybuf x' \
    --on "$(status 'NEW*' 1 1)" "$(dump mem.bin '0000 ffff')"
}

# ---- 8: no disk in the drive, then no drive at all: plain messages, the
# editor still works
case_no_disk() {
  for img in none nodrive; do
    expect "row 0 $(status 'SQUARE*' 1 2)" "row 1 k[ a square ]" "row 24 $BAR"
    run $img "$(status 'SQUARE*' 1 2)" \
      --on "$boot" 'keybuf \x86' --on 'NO DISK' 'keybuf \x87' \
      --on 'SAVE AS: SQUARE' 'keybuf \n' --on 'DISK ERROR: NO DISK' 'keybuf k' || return 1
  done
}

# ---- 9: a near-full buffer (7,995 bytes) through the KERNAL both ways:
# load it into the buffer under the BASIC ROM, save it as COPY, compare
case_source_at_a000() {
  img=$W/$CASE.d64
  cp "$IMG" "$img"
  python3 -c "
import sys
for i in range(205): sys.stdout.write('{ line %03d abcdefghijklmnopqrstuvwxy }\n' % i)" > "$W/full.pl0"
  c1541 "$img" -write "$W/full.pl0" "full,s" > /dev/null || return 1
  expect "row 0 $(status COPY 1 1)" "row 1 [ line 000 abcdefghijklmnopqrstuvwxy ]" \
    "text mem.bin $W/full.pl0"
  run "$img" 'SAVED COPY' \
    --on "$boot" 'keybuf \x86' --on "$PICK" "keybuf $(downs 15)\\n" \
    --on "$(status FULL 1 1)" 'keybuf \x87' \
    --on 'SAVE AS: FULL' 'keybuf \x14\x14\x14\x14copy\n' \
    --on 'SAVED COPY' "$(dump mem.bin '0000 ffff')" || return 1
  disk_file "$img" COPY "$W/full.pl0"
}

ALL="run_factorial run_error drawing_runs input_after_drawing stop_with_runstop
save_roundtrip save_bad_name picker missing_file read_error no_disk source_at_a000"
[ $# -gt 0 ] && ALL=$*
[ -f "$PRG" ] && [ -f "$IMG" ] || { echo "vice_app: $PRG or $IMG missing (make c64 disk)"; exit 1; }
[ -x build/pl0host ] || { echo "vice_app: build/pl0host missing (make host)"; exit 1; }
pass=0; fail=0; failed=
for CASE in $ALL; do
  start=$(date +%s)
  rm -f "$W/$CASE.all.log"
  if "case_$CASE"; then
    pass=$((pass + 1)); echo "PASS $CASE ($(( $(date +%s) - start )) s)"
  else
    fail=$((fail + 1)); failed="$failed $CASE"
    echo "FAIL $CASE (log: build/vice/app/$CASE.log)"
  fi
done
echo "vice_app: $pass passed, $fail failed${failed:+:$failed}"
[ $fail -eq 0 ]
