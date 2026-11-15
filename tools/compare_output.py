#!/usr/bin/env python3
"""Compare program output with a golden file, tolerating float differences.

Usage: compare_output.py EXPECTED ACTUAL

Both files are split into whitespace-separated tokens. Two tokens are equal
if they are identical, or if both parse as numbers and agree within a
relative tolerance of 1e-6 (plus an absolute 1e-9 so that values that should
be 0, like sin(pi), compare equal on host doubles and C64 floats).
Exit 0 on match, 1 on mismatch (the first difference is printed), 2 on usage.
"""
import math
import sys

REL_TOL = 1e-6
ABS_TOL = 1e-9


def as_number(tok):
    try:
        v = float(tok)
    except ValueError:
        return None
    return v if math.isfinite(v) else None


def tokens_equal(a, b):
    if a == b:
        return True
    x, y = as_number(a), as_number(b)
    if x is None or y is None:
        return False
    return math.isclose(x, y, rel_tol=REL_TOL, abs_tol=ABS_TOL)


def main(argv):
    if len(argv) != 3:
        print(__doc__.strip().splitlines()[2], file=sys.stderr)
        return 2
    with open(argv[1]) as f:
        want = f.read().split()
    with open(argv[2]) as f:
        got = f.read().split()
    for i, (a, b) in enumerate(zip(want, got)):
        if not tokens_equal(a, b):
            print("token %d differs: expected %r, got %r" % (i + 1, a, b))
            return 1
    if len(want) != len(got):
        print("token count differs: expected %d, got %d" % (len(want), len(got)))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
