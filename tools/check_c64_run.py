#!/usr/bin/env python3
"""Check a run of tests/c64/core_check.c in VICE (tests/vice_run.sh --check).

usage: check_c64_run.py [--screen FILE] [--text FILE] [--pbm FILE]
                        [--name NAME] [--tsv FILE] OUTDIR

OUTDIR is the vice_run work directory: screen.txt (the last screen,
decoded from screen RAM through the monitor) and, after a drawing,
snap1.bin (all 64 KB of RAM saved while the bitmap was shown).

  --screen FILE  every non-empty screen line, in order, must EQUAL the
                 lines of FILE (trailing blanks ignored)       (make c64-core)
  --text FILE    the program's output, the screen rows above the
                 `cycles C R` line, must match the golden FILE with
                 tools/compare_output.py (numbers within 1e-6 relative)
  --pbm FILE     the C64 bitmap in snap1.bin against the host rendering
                 (`pl0host --pbm`): set-pixel counts within 3% (5-byte
                 floats move a pixel here and there) and the bounding box
                 of the set pixels within 2 pixels on every side
  --name NAME    print a result line `NAME ok|FAIL <seconds> s ...`
  --tsv FILE     append `NAME result run_s compile_s run_cycles` to FILE
Always: no screen line may start with `FAIL` (core_check.c reports a
damaged source buffer that way).

Example mode reads `cycles C R` from the screen: C = CPU cycles of
pl0_compile, R = of pl0_run (CIA2 timers), seconds = cycles / 985248
(PAL). Writes OUTDIR/bitmap.png and OUTDIR/host.png for a drawing.
Exit 0 all checks passed, 1 otherwise.
"""
import os
import subprocess
import sys

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from pbm2png import read_p1, write_png  # noqa: E402

PAL_HZ = 985248
BITMAP = 0xE000
W, H = 320, 200
PIXEL_TOL = 0.03
BOX_TOL = 2


def bitmap(snap):
    px = []
    for y in range(H):
        row = []
        for x in range(W):
            b = snap[BITMAP + (y >> 3) * 320 + (x >> 3) * 8 + (y & 7)]
            row.append((b >> (7 - (x & 7))) & 1)
        px.append(row)
    return px


def box(px):
    pts = [(x, y) for y in range(H) for x in range(W) if px[y][x]]
    if not pts:
        return None
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    return min(xs), max(xs), min(ys), max(ys)


def main(argv):
    opts = {}
    i = 0
    rest = []
    while i < len(argv):
        if argv[i] in ("--screen", "--text", "--pbm", "--name", "--tsv") and i + 1 < len(argv):
            opts[argv[i]] = argv[i + 1]
            i += 2
        else:
            rest.append(argv[i])
            i += 1
    if len(rest) != 1:
        sys.exit(__doc__)
    outdir = rest[0]
    errors = []
    lines = [l.rstrip() for l in open(os.path.join(outdir, "screen.txt")).read().splitlines()]

    errors += ["screen shows %r" % l for l in lines if l.startswith("FAIL")]

    if "--screen" in opts:
        want = [l.rstrip() for l in open(opts["--screen"]).read().splitlines() if l.strip()]
        got = [l for l in lines if l]
        if got != want:
            errors.append("screen differs from %s" % opts["--screen"])
            for k in range(max(len(got), len(want))):
                g = got[k] if k < len(got) else "<none>"
                w = want[k] if k < len(want) else "<none>"
                if g != w:
                    errors.append("  line %d: got %r, want %r" % (k + 1, g, w))

    comp = run = None
    cyc = [l for l in lines if l.startswith("cycles ")]
    if cyc:
        parts = cyc[0].split()
        try:
            comp, run = int(parts[1]), int(parts[2])
        except (IndexError, ValueError):
            errors.append("bad cycles line %r" % cyc[0])

    if "--text" in opts:
        if not cyc:
            errors.append("no `cycles` line on the screen")
        else:
            out = lines[:lines.index(cyc[0])]
            path = os.path.join(outdir, "output.txt")
            open(path, "w").write("\n".join(out).rstrip("\n") + "\n")
            r = subprocess.run([sys.executable, os.path.join(HERE, "compare_output.py"),
                                opts["--text"], path], capture_output=True, text=True)
            if r.returncode != 0:
                errors.append("output differs from %s: %s" % (opts["--text"], r.stdout.strip()))

    if "--pbm" in opts:
        snap_path = os.path.join(outdir, "snap1.bin")
        if not os.path.exists(snap_path) or os.path.getsize(snap_path) != 65536:
            errors.append("no drawing snapshot %s" % snap_path)
        else:
            got = bitmap(open(snap_path, "rb").read())
            hw, hh, hbits = read_p1(opts["--pbm"])
            host = [[int(hbits[y * hw + x]) for x in range(hw)] for y in range(hh)]
            nc, nh = sum(map(sum, got)), sum(map(sum, host))
            bc, bh = box(got), box(host)
            write_png(os.path.join(outdir, "bitmap.png"), W, H,
                      "".join("1" if v else "0" for row in got for v in row), 2)
            write_png(os.path.join(outdir, "host.png"), hw, hh, hbits, 2)
            print("check_c64_run: %d set pixels on the C64, %d on the host (%+.2f%%); "
                  "box C64 %s host %s" % (nc, nh, 100.0 * (nc - nh) / max(nh, 1), bc, bh))
            if nh == 0 or abs(nc - nh) > PIXEL_TOL * nh:
                errors.append("set pixels %d vs host %d: more than 3%% apart" % (nc, nh))
            if bc is None or bh is None or any(abs(a - b) > BOX_TOL for a, b in zip(bc, bh)):
                errors.append("bounding box %s vs host %s: more than %d pixels apart"
                              % (bc, bh, BOX_TOL))

    for e in errors:
        print("check_c64_run: FAIL: " + e)
    secs = "%.2f" % (run / PAL_HZ) if run is not None else "-"
    csecs = "%.2f" % (comp / PAL_HZ) if comp is not None else "-"
    result = "FAIL" if errors else "ok"
    if "--name" in opts:
        print("%-12s %-4s run %7s s  compile %5s s  (%s run cycles)"
              % (opts["--name"], result, secs, csecs, run if run is not None else "-"))
    if "--tsv" in opts:
        with open(opts["--tsv"], "a") as f:
            f.write("%s\t%s\t%s\t%s\t%s\n" % (opts.get("--name", "-"), result, secs, csecs,
                                               run if run is not None else "-"))
    if errors:
        return 1
    print("check_c64_run: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
