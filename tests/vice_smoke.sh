#!/bin/sh
# On-target tests of the C64 editor (build/pl0.prg) in headless VICE.
# Usage: tests/vice_smoke.sh [case ...]     (no case: all of them)
#
# Every case is independent: it starts the program from scratch with
# tests/vice_run.sh, types keys through the monitor `keybuf` command when a
# screen line equals a trigger (--on), waits for a final screen line, then
# tools/check_editor.py checks more rows of that same screen (whole rows,
# row prefixes, the reverse-video cursor cell, the reverse bars) from an
# expectation file written by the case. A trigger that never shows is a
# failure too (the run hits the cycle limit), so triggers double as
# intermediate assertions.
# Key codes (PETSCII, written \xHH for keybuf): F1 85, F3 86, F5 87, F7 88,
# F2 89, CRSR down 11, up 91, right 1D, left 9D, HOME 13, CLR 93, DEL 14,
# INST 94, RETURN 0D (\n).
# buffer_full uses build/c64/pl0_tiny.prg: the same program built with a
# 400-byte source buffer (-DPL0_SOURCE_CAP=400; make c64-editor builds it),
# so 21 typed characters fill it instead of 7,800.
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root" || exit 2
PRG=build/pl0.prg
TINY=build/c64/pl0_tiny.prg
CYCLES=${SMOKE_CYCLES:-60000000}
BAR=' F1 Run F2 New F3 Open F5 Save F7 Help'
mkdir -p build/vice

# status NAME LINE COL: the exact status row (name field 16 wide from col 8)
status() { printf ' PL/0   %-16sL %s  C %s' "$1" "$2" "$3"; }

# run CASE PRG EXPECTED [--on TRIGGER CMD]...; the expectations are in $EXP
run() {
  c=$1; prg=$2; want=$3; shift 3
  sh tests/vice_run.sh --check "python3 tools/check_editor.py '$EXP'" \
    "$@" "$prg" "$want" "$CYCLES" > "build/vice/smoke_$c.log" 2>&1
}

expect() { EXP=$root/build/vice/expect_$CASE.txt; printf '%s\n' "$@" > "$EXP"; }

boot=$(status SQUARE 1 1)

case_editor_boots() {
  expect "row 0 $boot" "reverse 0" "row 24 $BAR" "reverse 24" \
    "row 1 [ a square ]" "row 2 [ The turtle starts in the middle" \
    "row 9 var i;" "row 19 end." "row 20" "cursor 1 0"
  run "$CASE" $PRG "$boot"
}

case_typing() {
  expect "row 1 hello[ a square ]" "cursor 1 5" "row 24 $BAR"
  run "$CASE" $PRG "$(status 'SQUARE*' 1 6)" --on "$boot" 'keybuf hello'
}

# right x20 on line 2 (column 21), down x7 to `var i;` (column 7, the end),
# down to `begin` (6), down to line 11 (back to the sticky 21); up x2
# (`var i;`, 7), left (6, forgets the sticky column), down x2 (line 11, 6)
case_cursor_keys_and_sticky_column() {
  r20=$(printf '\\x1d%.0s' $(seq 20)); d7=$(printf '\\x11%.0s' $(seq 7))
  expect "cursor 11 5" "row 11   penup; moveto(-80, -80); pendown;" "row 0 $(status SQUARE 11 6)"
  run "$CASE" $PRG "$(status SQUARE 11 6)" \
    --on "$boot" "keybuf \\x11$r20$d7" \
    --on "$(status SQUARE 9 7)" 'keybuf \x11\x11' \
    --on "$(status SQUARE 11 21)" 'keybuf \x91\x91\x9d\x11\x11'
}

# CLR, DEL, HOME, INST on line 1; DEL at the start of line 2 joins it to
# line 1 (43 characters: `>` in column 39); INST at the end of line 2 joins
# line 3 to it
case_home_clr_del_inst() {
  expect "row 1  a square [ The turtle starts in the mi>" \
    "row 2   of the screen, facing up. Four  times>" \
    "row 3   penup/moveto/pendown first move" "cursor 2 32"
  run "$CASE" $PRG "$(status 'SQUARE*' 2 33)" \
    --on "$boot" 'keybuf \x93\x14\x13\x94' \
    --on ' a square' 'keybuf \x11\x14' \
    --on "$(status 'SQUARE*' 1 11)" 'keybuf \x11\x93\x94'
}

# F2 (the starter is unmodified: no question), then 40 lines `1`..`40`
lines40=$(seq 40 | tr '\n' '|' | sed 's/|$//; s/|/\\n/g')

# the view follows the cursor down: lines 18..40 on rows 1..23
case_newline_and_scroll() {
  expect "row 1 18" "row 22 39" "row 23 40" "cursor 23 2" "row 24 $BAR"
  run "$CASE" $PRG "$(status 'NEW*' 40 3)" --on "$boot" "keybuf \\x89$lines40"
}

# then up x25 to line 15: the view scrolls back to lines 15..37
case_scroll_up() {
  up=$(printf '\\x91%.0s' $(seq 25))
  expect "row 1 15" "row 23 37" "cursor 1 2" "row 24 $BAR"
  run "$CASE" $PRG "$(status 'NEW*' 15 3)" \
    --on "$boot" "keybuf \\x89$lines40" \
    --on "$(status 'NEW*' 40 3)" "keybuf $up"
}

# 60 characters: the window follows the cursor (`<` hides columns on the
# left, the cursor in column 38); HOME shows columns 1-39 and `>`; CLR goes
# back to the end, and five CRSR left keep the window where it is (the keys
# go one trigger at a time: keys that arrive together are handled before
# the next redraw, and the window follows from what was shown last)
case_long_line_horizontal_scroll() {
  digits=$(python3 -c "print(''.join(str(i % 10) for i in range(60)))")
  right='<3456789012345678901234567890123456789'
  left='012345678901234567890123456789012345678>'
  expect "row 1 $right" "cursor 1 33"
  run "$CASE" $PRG "$(status 'NEW*' 1 56)" \
    --on "$boot" "keybuf \\x89$digits" \
    --on "$right" 'keybuf \x13' \
    --on "$left" 'keybuf \x93' \
    --on "$right" 'keybuf \x9d\x9d\x9d\x9d\x9d'
}

case_error_display() {
  err='ERROR line 1: undeclared name'
  expect "row 24 $BAR" "row 1 !x." "cursor 1 1"
  run "$CASE" $PRG "$(status 'NEW*' 1 2)" \
    --on "$boot" 'keybuf \x89!x.\x85' \
    --on "$err" 'bank ram' \
    --on "$(status 'NEW*' 1 1)" 'keybuf \x1d'
}

case_error_on_line_3() {
  err='ERROR line 3: undeclared name'
  expect "row 0 $(status 'NEW*' 3 1)" "cursor 3 0" "row 3   y := 1" "reverse 24"
  run "$CASE" $PRG "$err" \
    --on "$boot" 'keybuf \x89var x;\nbegin\n  y := 1\nend.\x85'
}

# F1 compiles and runs the starter (a drawing; more in tests/vice_app.sh);
# a key returns to the editor as it was
case_compile_ok() {
  expect "row 0 $boot" "cursor 1 0" "row 1 [ a square ]" "row 24 $BAR"
  run "$CASE" $PRG "$boot" --on "$boot" 'keybuf \x85' \
    --on '-- SPACE: DRAWING  ANY KEY: EDITOR --' 'keybuf x'
}

# F7 at line 3 column 3; `x` returns (and is not typed); then CRSR right
case_help_screen() {
  expect "row 3   of the screen, facing up. Four" "cursor 3 3" "row 24 $BAR"
  run "$CASE" $PRG "$(status SQUARE 3 4)" \
    --on "$boot" 'keybuf \x11\x11\x1d\x1d\x88' \
    --on ' PL/0 HELP' 'keybuf x' \
    --on "$(status SQUARE 3 3)" 'keybuf \x1d'
}

# z (modified), F2 asks, N keeps (q is typed after it), F2 asks, Y clears
case_new_program_confirm() {
  ask='DISCARD PROGRAM? (Y/N)'
  expect "row 1" "row 2" "cursor 1 0" "row 24 $BAR"
  run "$CASE" $PRG "$(status NEW 1 1)" \
    --on "$boot" 'keybuf z\x89' \
    --on "$ask" 'keybuf nq' \
    --on "$(status 'SQUARE*' 1 3)" 'keybuf \x89' \
    --on "$ask" 'keybuf y'
}

# 400-byte buffer holding the 379-byte starter: 20 more characters fit
case_buffer_full() {
  a30=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
  expect "row 1 aaaaaaaaaaaaaaaaaaaa[ a square ]" "row 19 end." "cursor 1 21" "row 24 $BAR"
  run "$CASE" $TINY "$(status 'SQUARE*' 1 22)" \
    --on "$boot" "keybuf $a30" \
    --on 'BUFFER FULL' 'keybuf \x1d'
}

# keys that arrive together are all handled before the next redraw: edits
# on two different lines (x on line 1, y on line 3), a new line on line 9
# and a cursor move up must all show
case_batched_edits() {
  expect "row 1 x[ a square ]" "row 2 [ The turtle starts in the middle" \
    "row 3 y  of the screen, facing up. Four" "row 9 v" "row 10 ar i;" "row 11 begin" \
    "cursor 9 1"
  run "$CASE" $PRG "$(status 'SQUARE*' 9 2)" \
    --on "$boot" 'keybuf x\x11\x11\x9dy\x11\x11\x11\x11\x11\x11\n\x91\x1d'
}

# colours (CTRL/C= 1-8), RVS ON/OFF, graphics (C= and SHIFT letters and
# symbols), RUN/STOP, F4/F6/F8: nothing typed, nothing moved; then k
case_ignored_keys() {
  expect "row 1 k[ a square ]" "cursor 1 1" "row 24 $BAR"
  run "$CASE" $PRG "$(status 'SQUARE*' 1 2)" \
    --on "$boot" 'keybuf \x05\x1c\x1e\x1f\x90\x9c\x9e\x81\x95\x96\x97\x98\x99\x9a\x9b\x12\x92\xa6\xb0\xc0\xdb\xdd\xde\xa9\x03\x8a\x8b\x8c\x0e\x08k'
}

ALL="editor_boots typing cursor_keys_and_sticky_column home_clr_del_inst
newline_and_scroll scroll_up long_line_horizontal_scroll error_display error_on_line_3
compile_ok help_screen new_program_confirm buffer_full batched_edits ignored_keys"
[ $# -gt 0 ] && ALL=$*
[ -f $PRG ] || { echo "vice_smoke: $PRG missing (make c64)"; exit 1; }
pass=0; fail=0; failed=
for CASE in $ALL; do
  start=$(date +%s)
  if "case_$CASE"; then
    pass=$((pass + 1)); echo "PASS $CASE ($(( $(date +%s) - start )) s)"
  else
    fail=$((fail + 1)); failed="$failed $CASE"
    echo "FAIL $CASE (log: build/vice/smoke_$CASE.log)"
    tail -n 32 "build/vice/smoke_$CASE.log"
  fi
done
echo "vice_smoke: $pass passed, $fail failed${failed:+:$failed}"
[ $fail -eq 0 ]
