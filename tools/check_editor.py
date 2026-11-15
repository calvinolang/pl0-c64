#!/usr/bin/env python3
"""Check the C64 editor screen saved by tests/vice_run.sh (tests/vice_smoke.sh).

usage: check_editor.py EXPECTFILE OUTDIR

Reads OUTDIR/screen.bin (screen RAM $0400-$07E7, the screen that matched)
and checks every line of EXPECTFILE:
    row N TEXT      screen row N equals TEXT (trailing spaces ignored;
                    no TEXT: the row is empty)
    prefix N TEXT   screen row N starts with TEXT
    cursor R C      the only reverse-video cell of the text rows 1-23 is
                    at row R, column C (0-based columns)
    reverse N       every cell of row N is in reverse video (the bars)
Rows are decoded like the harness does (tools/screen2txt.py). Exit 0 if
all hold, else 1 with one message per failure and the screen.
"""
import os
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from screen2txt import decode  # noqa: E402


def check(expect_lines, scr):
    rows = decode(scr).splitlines()
    errors = []
    for e in expect_lines:
        if not e.strip():
            continue
        kind, _, rest = e.partition(" ")
        if kind in ("row", "prefix"):
            n, _, text = rest.partition(" ")
            got = rows[int(n)]
            if kind == "row" and got != text.rstrip():
                errors.append(f"row {n}: expected {text.rstrip()!r}, got {got!r}")
            if kind == "prefix" and not got.startswith(text):
                errors.append(f"row {n}: expected to start with {text!r}, got {got!r}")
        elif kind == "cursor":
            r, c = (int(x) for x in rest.split())
            cells = [(i // 40, i % 40) for i in range(40, 960) if scr[i] & 0x80]
            if cells != [(r, c)]:
                errors.append(f"cursor: expected [({r}, {c})], reverse cells {cells}")
        elif kind == "reverse":
            n = int(rest)
            plain = [i % 40 for i in range(n * 40, n * 40 + 40) if not scr[i] & 0x80]
            if plain:
                errors.append(f"row {n}: not reverse at columns {plain}")
        else:
            errors.append(f"bad expectation line: {e!r}")
    return errors


def main(argv):
    if len(argv) != 3:
        sys.exit(__doc__)
    scr = open(os.path.join(argv[2], "screen.bin"), "rb").read()
    if len(scr) != 1000:
        print("check_editor: no screen dump")
        return 1
    errors = check(open(argv[1]).read().splitlines(), scr)
    for e in errors:
        print("check_editor: " + e)
    if errors:
        sys.stdout.write(decode(scr))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
