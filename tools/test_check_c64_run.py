#!/usr/bin/env python3
"""Self-test of the drawing comparison in tools/check_c64_run.py (host
only, run by `make test`).

For real example renders (examples/NAME.lines drawn with the model of the
C64 line routine from tools/check_bitmap.py) the comparison must accept
the drawing itself and a copy shifted by 1 pixel (what 5-byte floats can
do), and reject a copy mirrored in x, mirrored in y, or rotated 90 degrees
about the screen centre, unless that copy coincides with the drawing
within the tolerances because the figure is symmetric under it (then no
check on the bitmap could tell, and no sign error would be visible).
EXPECT lists the outcome per example and transformation; the test also
prints the numbers.
"""
import os
import sys

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from check_bitmap import c64_line  # noqa: E402
from check_c64_run import compare_drawing, W, H  # noqa: E402

ROOT = os.path.dirname(HERE)


def render(name):
    px = [[0] * W for _ in range(H)]
    for l in open(os.path.join(ROOT, "examples", name + ".lines")):
        if l.strip():
            c64_line(px, *(int(v) for v in l.split()))
    return px


def transform(px, f):
    out = [[0] * W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            if px[y][x]:
                nx, ny = f(x, y)
                if 0 <= nx < W and 0 <= ny < H:
                    out[ny][nx] = 1
    return out


TRANSFORMS = [
    ("identical", lambda x, y: (x, y)),
    ("shift 1 px", lambda x, y: (x + 1, y)),
    ("mirror x", lambda x, y: (W - 1 - x, y)),
    ("mirror y", lambda x, y: (x, H - 1 - y)),
    ("rotate 90", lambda x, y: (259 - y, x - 60)),     # about (159.5, 99.5)
]

# True = must pass, False = must fail
EXPECT = {
    "square": [True, True, True, True, True],          # symmetric under all three
    "koch":   [True, True, True, False, False],       # symmetric about x = 160
    "tree":   [True, True, True, False, False],       # symmetric about x = 160
    "spiral": [True, True, False, False, False],
}


def main():
    bad = 0
    for name, want in EXPECT.items():
        host = render(name)
        for (label, f), ok in zip(TRANSFORMS, want):
            errs, summary = compare_drawing(transform(host, f), host)
            got = not errs
            # what the count and box checks alone (before the overlap
            # check) would have said
            old = not [e for e in errs if not e.startswith("overlap")]
            mark = "ok  " if got == ok else "BAD "
            if got != ok:
                bad += 1
            print("%s%-7s %-10s %s (expected %s; count+box alone: %s): %s"
                  % (mark, name, label, "pass" if got else "fail", "pass" if ok else "fail",
                     "pass" if old else "fail", summary))
    if bad:
        print("test_check_c64_run: %d unexpected result(s)" % bad)
        return 1
    print("test_check_c64_run: all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
