#!/usr/bin/env python3
"""Check the C64 bitmap drawn by tests/c64/gfx_check.c (run with
tests/vice_run.sh --snap, which leaves snap1.bin..snap3.bin, 64 KB of RAM
each, in OUTDIR).

usage: check_bitmap.py --ref PREFIX --lbl LABELS OUTDIR

  PREFIX.pbm / PREFIX.lines  host reference from build/gfx_ref (the same
                             lines through gfx_host and the shared clipper)
  LABELS                     ld65 label file of gfx_check.prg (__BSS_RUN__)

Checks:
  1. snap1 -> snap2 (gfx_open): only the bitmap $E000-$FF3F, the matrix
     $CC00-$CFE7, the RAM NMI vector $FFFA/$FFFB and volatile RAM changed;
     snap2 has an all-zero bitmap, a matrix of $01 (black ink on white
     paper) and an NMI vector pointing at an RTI ($40).
  2. The VIC registers the program copied to $02A9-$02AD while the bitmap
     was shown: bank 3, $D011 = $3B, hi-res, $D018 = $38, white border.
  3. snap2 -> snap3 (drawing): only the bitmap and volatile RAM changed.
     Volatile: $0000-$03FF (zero page, stack, KERNAL work area) and the
     program's BSS plus C stack, __BSS_RUN__..$9FFF. Everything else (screen
     RAM, the program code, $A000-$DFFF RAM, $FF40-$FFFF) must be unchanged.
  4. The bitmap equals, pixel for pixel, a Python model of the C64 routine
     (octant Bresenham, see c64_line) run on the host-clipped endpoints; its
     set-pixel count is within 1% of the host renderer's PBM; every pixel of
     each axis-parallel line is set (exact, algorithm independent).
Writes OUTDIR/bitmap.pbm, OUTDIR/bitmap.png and OUTDIR/host.png.
"""
import os
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pbm2png import read_p1, write_png  # noqa: E402

BITMAP, BITMAP_END = 0xE000, 0xE000 + 8000      # exclusive end ($FF40)
MATRIX, MATRIX_END = 0xCC00, 0xCC00 + 1000      # exclusive end ($CFE8)
W, H = 320, 200


def bss_start(lbl):
    for line in open(lbl):
        parts = line.split()
        if len(parts) == 3 and parts[2] == ".__BSS_RUN__":
            return int(parts[1], 16)
    sys.exit("check_bitmap: __BSS_RUN__ not in " + lbl)


def decode(snap):
    """bitmap bytes -> list of rows of 0/1"""
    px = [[0] * W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            b = snap[BITMAP + (y >> 3) * 320 + (x >> 3) * 8 + (y & 7)]
            px[y][x] = (b >> (7 - (x & 7))) & 1
    return px


def c64_line(px, x0, y0, x1, y1):
    """The algorithm of src/c64/gfx_c64.s, step for step."""
    if x1 < x0:
        x0, y0, x1, y1 = x1, y1, x0, y0
    dx, dy = x1 - x0, abs(y1 - y0)
    sy = 1 if y1 >= y0 else -1
    x, y = x0, y0
    if dx >= dy:
        err = dx >> 1
        for _ in range(dx + 1):
            px[y][x] = 1
            err -= dy
            if err < 0:
                err += dx; y += sy
            x += 1
    else:
        err = dy >> 1
        for _ in range(dy + 1):
            px[y][x] = 1
            err -= dx
            if err < 0:
                err += dy; x += 1
            y += sy


def diff(a, b, allowed):
    bad = [i for i in range(65536) if a[i] != b[i] and not allowed(i)]
    return bad


def write_pbm(path, px):
    with open(path, "w") as f:
        f.write("P1\n%d %d\n" % (W, H))
        for row in px:
            f.write("".join("1" if v else "0" for v in row) + "\n")


def main():
    args = sys.argv[1:]
    try:
        ref = args[args.index("--ref") + 1]
        lbl = args[args.index("--lbl") + 1]
        outdir = args[-1]
    except (ValueError, IndexError):
        sys.exit(__doc__)
    errors = []
    snaps = []
    for n in (1, 2, 3):
        p = os.path.join(outdir, "snap%d.bin" % n)
        if not os.path.exists(p) or os.path.getsize(p) != 65536:
            sys.exit("check_bitmap: %s missing or not 64 KB" % p)
        snaps.append(open(p, "rb").read())
    s1, s2, s3 = snaps
    bss = bss_start(lbl)

    def volatile(i):
        return i < 0x0400 or bss <= i < 0xA000

    def in_bitmap(i):
        return BITMAP <= i < BITMAP_END

    def in_matrix(i):
        return MATRIX <= i < MATRIX_END

    bad = diff(s1, s2, lambda i: volatile(i) or in_bitmap(i) or in_matrix(i) or i in (0xFFFA, 0xFFFB))
    if bad:
        errors.append("gfx_open changed %d byte(s) outside bitmap/matrix/NMI vector, first at %s"
                      % (len(bad), ", ".join("$%04X" % i for i in bad[:8])))
    if any(s2[BITMAP:BITMAP_END]):
        errors.append("bitmap not cleared by gfx_open")
    if any(v != 0x01 for v in s2[MATRIX:MATRIX_END]):
        errors.append("matrix is not all $01 after gfx_open")
    nmi = s2[0xFFFA] | s2[0xFFFB] << 8
    if s2[nmi] != 0x40:
        errors.append("RAM NMI vector $%04X does not point at an RTI" % nmi)
    dd00, d011, d016, d018, d020 = s3[0x02A9:0x02AE]
    if (dd00 & 3, d011 & 0x7F, d016 & 0x10, d018 & 0xFE, d020 & 15) != (0, 0x3B, 0, 0x38, 1):
        errors.append("VIC registers while open: $DD00=%02X $D011=%02X $D016=%02X $D018=%02X $D020=%02X"
                      % (dd00, d011, d016, d018, d020))
    bad = diff(s2, s3, lambda i: volatile(i) or in_bitmap(i))
    if bad:
        errors.append("drawing changed %d byte(s) outside the bitmap, first at %s"
                      % (len(bad), ", ".join("$%04X" % i for i in bad[:8])))

    got = decode(s3)
    model = [[0] * W for _ in range(H)]
    lines = [tuple(int(v) for v in l.split()) for l in open(ref + ".lines") if l.strip()]
    for (x0, y0, x1, y1) in lines:
        c64_line(model, x0, y0, x1, y1)
    wrong = [(x, y) for y in range(H) for x in range(W) if got[y][x] != model[y][x]]
    if wrong:
        errors.append("%d pixel(s) differ from the model of the C64 routine, first %s"
                      % (len(wrong), wrong[:5]))
    hw, hh, hbits = read_p1(ref + ".pbm")
    host = hbits.count("1")
    count = sum(map(sum, got))
    if abs(count - host) > host / 100:
        errors.append("set pixels %d vs host %d: more than 1%% apart" % (count, host))
    axis = 0
    for (x0, y0, x1, y1) in lines:
        if x0 == x1 or y0 == y1:
            axis += 1
            unset = [(x, y) for y in range(min(y0, y1), max(y0, y1) + 1)
                     for x in range(min(x0, x1), max(x0, x1) + 1) if not got[y][x]]
            if unset:
                errors.append("axis-parallel line %s: %d pixel(s) not set, first %s"
                              % ((x0, y0, x1, y1), len(unset), unset[0]))

    write_pbm(os.path.join(outdir, "bitmap.pbm"), got)
    write_png(os.path.join(outdir, "bitmap.png"), W, H,
              "".join("1" if v else "0" for row in got for v in row), 2)
    write_png(os.path.join(outdir, "host.png"), hw, hh, hbits, 2)
    print("check_bitmap: %d visible lines (%d axis-parallel), %d set pixels on the C64, "
          "%d on the host (%+.2f%%), model mismatches %d"
          % (len(lines), axis, count, host, 100.0 * (count - host) / max(host, 1), len(wrong)))
    print("check_bitmap: images in %s/bitmap.png and host.png" % outdir)
    for e in errors:
        print("check_bitmap: FAIL: " + e)
    if errors:
        return 1
    print("check_bitmap: OK (no stray writes; bitmap matches)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
