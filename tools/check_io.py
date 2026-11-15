#!/usr/bin/env python3
"""Check the screens tests/c64/io_check.c leaves at its --snap steps.

usage: check_io.py OUTDIR      (OUTDIR/snap1.bin, snap2.bin: 64 KB of RAM)

  step 1, after printing `line 00`..`line 59` (each + io_put_nl): the screen
          scrolled, rows 0..23 are `line 36`..`line 59`, row 24 is empty;
          colour RAM is not in the RAM dump: the program prints odd lines
          in another colour and checks that the colours scrolled too.
  step 2, after a 100-character string a..z a..z ... + io_put_nl: rows of
          40, 40 and 20 characters, row 3 empty.
"""
import os
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from screen2txt import decode  # noqa: E402


def screen(outdir, n):
    p = os.path.join(outdir, "snap%d.bin" % n)
    if not os.path.exists(p) or os.path.getsize(p) != 65536:
        sys.exit("check_io: %s missing or not 64 KB" % p)
    return decode(open(p, "rb").read()[0x0400:0x07E8]).splitlines()


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    outdir = sys.argv[1]
    errors = []
    s1 = screen(outdir, 1)
    want = ["line %02d" % n for n in range(36, 60)] + [""]
    if s1 != want:
        errors.append("scroll screen:\n" + "\n".join(s1))
    s2 = screen(outdir, 2)
    a = "".join(chr(ord("a") + i % 26) for i in range(100))
    want = [a[:40], a[40:80], a[80:]] + [""] * 22
    if s2 != want:
        errors.append("wrap screen:\n" + "\n".join(s2))
    for e in errors:
        print("check_io: FAIL: " + e)
    if errors:
        return 1
    print("check_io: OK (scrolled screen shows line 36..59, 100 characters wrap 40/40/20)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
