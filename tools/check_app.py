#!/usr/bin/env python3
"""Check a run of the C64 program (tests/vice_app.sh) after the harness
found its final screen line.

usage: check_app.py EXPECTFILE OUTDIR

EXPECTFILE holds one check per line. The screen checks of
tools/check_editor.py (row, prefix, cursor, reverse) apply to
OUTDIR/screen.bin, the screen that matched, or to the dump named by the
last `screen` line; dumps are files the case saved in OUTDIR with the
monitor (`bsave "OUTDIR/NAME" 0 START END` at a trigger):
    screen NAME          the following screen checks use OUTDIR/NAME
                         (a dump of $0400-$07E7); `screen` alone: the
                         final screen again
    text NAME FILE       the editor's text in the 64 KB RAM dump NAME
                         (the TextBuf ed_tb, found through
                         build/c64/pl0.lbl, read around its gap) equals
                         the bytes of FILE
    byte NAME MASK VALUE the first byte of dump NAME, AND MASK (hex),
                         equals VALUE (hex): e.g. $D011 saved with
                         `bank io`, bit $20 = bitmap mode
    pbm NAME FILE        the bitmap ($E000) in the 64 KB dump NAME against
                         the host rendering FILE (pl0host --pbm), with the
                         checks of tools/check_c64_run.py
    output NAME FILE     the program output in the screen dump NAME (the
                         rows above the last `-- PRESS ANY KEY --` row)
                         matches the golden FILE with
                         tools/compare_output.py (numbers within 1e-6
                         relative); if the output fills the screen and
                         FILE has more lines, only its last lines are
                         compared, and the check says so
Exit 0 if all hold, else 1 with one message per failure.
"""
import os
import struct
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from check_editor import check  # noqa: E402
from check_c64_run import bitmap, compare_drawing  # noqa: E402
from pbm2png import read_p1  # noqa: E402
from screen2txt import decode  # noqa: E402

LBL = os.path.join(HERE, "..", "build", "c64", "pl0.lbl")
KEY = "-- PRESS ANY KEY --"


def check_output(scr, golden):
    """The output rows of a run screen against a golden file: a list of
    error messages (empty if it matches)."""
    rows = [r.rstrip() for r in decode(scr).splitlines()]
    if KEY not in rows:
        return ["no %r row on the screen" % KEY]
    out = rows[:len(rows) - 1 - rows[::-1].index(KEY)]
    while out and not out[-1]:
        out.pop()
    want = open(golden).read().rstrip("\n").split("\n")
    if len(want) > len(out) and len(out) == len(rows) - 1:
        # the output filled the screen above the key row, so it may have
        # scrolled: the screen holds its tail (the example rules keep
        # every example within 22 rows, so this is for other programs)
        print("check_app: output longer than the screen: compared the last %d of %d lines"
              % (len(out), len(want)))
        want = want[len(want) - len(out):]
    with tempfile.TemporaryDirectory() as tmp:
        a, b = os.path.join(tmp, "want"), os.path.join(tmp, "got")
        open(a, "w").write("\n".join(want) + "\n")
        open(b, "w").write("\n".join(out) + "\n")
        r = subprocess.run([sys.executable, os.path.join(HERE, "compare_output.py"), a, b],
                           capture_output=True, text=True)
    if r.returncode != 0:
        return ["output differs from %s: %s" % (golden, r.stdout.strip()),
                "output was:\n" + "\n".join(out)]
    return []


def label(name):
    for line in open(LBL):
        parts = line.split()
        if len(parts) == 3 and parts[2] == "." + name:
            return int(parts[1], 16)
    raise SystemExit("check_app: label %s not in %s" % (name, LBL))


def tb_text(mem):
    """The logical text of ed_tb (textbuf.h: buf, cap, len, cur, gs)."""
    a = label("_ed_tb")
    buf, cap, ln, _cur, gs = struct.unpack("<HHHHH", mem[a:a + 10])
    gap = cap - ln
    return mem[buf:buf + gs] + mem[buf + gs + gap:buf + cap]


def read(outdir, name):
    p = os.path.join(outdir, name)
    if not os.path.exists(p):
        return None
    return open(p, "rb").read()


def main(argv):
    if len(argv) != 3:
        sys.exit(__doc__)
    outdir = argv[2]
    errors = []
    screen_name = "screen.bin"
    group = []

    def flush():
        scr = read(outdir, screen_name)
        if not group:
            return
        if scr is None or len(scr) != 1000:
            errors.append("no screen dump %s" % screen_name)
            return
        errs = check(group, scr)
        if errs:
            errors.extend("%s: %s" % (screen_name, e) for e in errs)
            errors.append("%s was:\n%s" % (screen_name, decode(scr)))
        del group[:]

    for line in open(argv[1]).read().splitlines():
        if not line.strip():
            continue
        kind, _, rest = line.partition(" ")
        if kind in ("row", "prefix", "cursor", "reverse"):
            group.append(line)
            continue
        flush()
        args = rest.split()
        if kind == "screen":
            screen_name = args[0] if args else "screen.bin"
        elif kind == "text":
            mem = read(outdir, args[0])
            if mem is None or len(mem) != 65536:
                errors.append("no RAM dump %s" % args[0])
                continue
            got, want = tb_text(mem), open(args[1], "rb").read()
            if got != want:
                k = next((i for i in range(min(len(got), len(want))) if got[i] != want[i]),
                         min(len(got), len(want)))
                errors.append("text differs from %s: %d bytes vs %d, first difference at %d: "
                              "%r vs %r" % (args[1], len(got), len(want), k,
                                            got[k:k + 20], want[k:k + 20]))
        elif kind == "byte":
            d = read(outdir, args[0])
            if not d:
                errors.append("no dump %s" % args[0])
            elif d[0] & int(args[1], 16) != int(args[2], 16):
                errors.append("%s: byte $%02X, mask $%s, expected $%s"
                              % (args[0], d[0], args[1], args[2]))
        elif kind == "pbm":
            mem = read(outdir, args[0])
            if mem is None or len(mem) != 65536:
                errors.append("no RAM dump %s" % args[0])
                continue
            hw, hh, hbits = read_p1(args[1])
            host = [[int(hbits[y * hw + x]) for x in range(hw)] for y in range(hh)]
            errs, summary = compare_drawing(bitmap(mem), host)
            print("check_app: " + summary)
            errors += errs
        elif kind == "output":
            scr = read(outdir, args[0])
            if scr is None or len(scr) != 1000:
                errors.append("no screen dump %s" % args[0])
            else:
                errors += check_output(scr, args[1])
        else:
            errors.append("bad expectation line: %r" % line)
    flush()
    for e in errors:
        print("check_app: " + e)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
