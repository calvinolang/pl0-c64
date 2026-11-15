#!/usr/bin/env python3
"""make media, part 1: still captures of the real program in VICE.

usage: media_stills.py [--out DIR] [name ...]
  names: editor editor-error picker help save, and the examples (square,
  koch, ...); no name: all of them. DIR defaults to docs/img/c64.

Every capture is a fresh, headless x64sc (tests/vice_run.sh, which runs
tools/vice_screen.py): build/pl0.prg injected into RAM (-autostartprgmode
1, as the test harness does, instead of the 80 s autostart of the disk),
a copy of build/pl0.d64 in drive 8, a 1541 with true drive emulation,
warp. Keys go in through the monitor's `keybuf` when a screen line shows
(--on TRIGGER CMD), exactly as in tests/c64_app_examples.sh. Once the
final screen shows, the harness lets the emulation run on until its next
screen poll (`pause`: about half a second of wall time in warp, many
frames), so that the whole frame has been redrawn, then VICE's monitor
writes the screenshot (`screenshot "FILE" 2`: PNG, 384x272 with the
border, VICE's default palette). tools/c64img.py checks it and writes
DIR/NAME.png (palette PNG, 2x nearest neighbour, 768x544).

Examples: F3, CRSR down to the example's INDEX entry, RETURN, F1, then the
end-of-run prompt: for a drawing the bitmap (the prompt is on the hidden
text screen), for a text example the output screen ending with
`-- PRESS ANY KEY --`.
"""
import os
import shutil
import subprocess
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import c64img  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PRG, IMG = "build/pl0.prg", "build/pl0.d64"
BAR = " F1 Run F2 New F3 Open F5 Save F7 Help"
PICK = " RETURN open  RUN/STOP back  CRSR move"
KEY = "-- PRESS ANY KEY --"
DRAWKEY = "-- SPACE: DRAWING  ANY KEY: EDITOR --"
DRIVE = ["--vice", "-drive8type", "--vice", "1541", "--vice", "-drive8truedrive",
         "--vice", "+virtualdev8"]
CYCLES = "400000000"
UI = ["editor", "editor-error", "picker", "help", "save"]


def status(name, line, col):
    return " PL/0   %-16sL %d  C %d" % (name, line, col)


def keys(*codes):
    """keybuf argument: PETSCII codes as \\xHH (the monitor types them)."""
    return "".join("\\x%02x" % c for c in codes)


def index():
    out = subprocess.run([sys.executable, "tools/mkdisk.py", "--index", "examples"],
                         check=True, capture_output=True, text=True).stdout
    return [l.split(None, 1)[0].lower() for l in out.splitlines() if l.strip()]


def scenario(name, order):
    """(list of --on pairs, final line) leading to the screen to capture."""
    boot = status("SQUARE", 1, 1)
    F1, F3, F5, F7, DOWN, RIGHT, DEL = 0x85, 0x86, 0x87, 0x88, 0x11, 0x1D, 0x14
    if name == "editor":
        return [], boot
    if name == "editor-error":
        # line 15 `    forward(160);` -> `    forwrd(160);`, F1
        err = "ERROR line 15: undeclared name"
        return [(boot, "keybuf " + keys(*[DOWN] * 14, *[RIGHT] * 9, DEL, F1))], err
    if name == "picker":
        # the list, KOCH selected (a drawing from the second page of INDEX)
        pos = order.index("koch")
        return [(boot, "keybuf " + keys(F3)),
                (PICK, "keybuf " + keys(*[DOWN] * pos))], PICK
    if name == "help":
        return [(boot, "keybuf " + keys(F7))], " PL/0 HELP"
    if name == "save":
        return [(boot, "keybuf " + keys(F5)),
                ("SAVE AS: SQUARE", "keybuf " + keys(*[DEL] * 6) + "mysquare")], \
            "SAVE AS: MYSQUARE"
    pos = order.index(name)
    NAME = name.upper()
    on = status(NAME, 1, 1)
    end = DRAWKEY if os.path.exists(f"examples/{name}.lines") else KEY
    return [(boot, "keybuf " + keys(F3)),
            (PICK, "keybuf " + keys(*[DOWN] * pos) + "\\n"),
            (on, "keybuf " + keys(F1))], end


def capture(name, order, outdir):
    work = "media_" + name
    vdir = os.path.join(ROOT, "build", "vice", work)
    disk = os.path.join(ROOT, "build", "vice", work + ".d64")
    shutil.copyfile(IMG, disk)
    shot = os.path.join(vdir, "shot.png")
    ons, final = scenario(name, order)
    args = ["sh", "tests/vice_run.sh", "--work", work] + DRIVE + ["--vice", "-8", "--vice", disk]
    for trig, cmd in ons:
        args += ["--on", trig, cmd]
    args += ["--on", final, "pause", "--on", final, f'screenshot "{shot}" 2']
    args += [PRG, final, CYCLES, "ERROR" if name != "editor-error" else "FAIL"]
    log = os.path.join(ROOT, "build", "vice", work + ".log")
    with open(log, "w") as f:
        rc = subprocess.run(args, stdout=f, stderr=subprocess.STDOUT).returncode
    if rc != 0 or not os.path.exists(shot):
        print(f"  {name}: FAILED (log: {os.path.relpath(log, ROOT)})")
        print(open(log).read()[-2000:])
        return False
    dst = os.path.join(outdir, name + ".png")
    c64img.publish(shot, dst)
    print(f"  {name}: {os.path.relpath(dst, ROOT)} ({os.path.getsize(dst)} bytes)")
    return True


def main(argv):
    os.chdir(ROOT)
    outdir = "docs/img/c64"
    if argv[:1] == ["--out"]:
        outdir, argv = argv[1], argv[2:]
    for f in (PRG, IMG):
        if not os.path.exists(f):
            sys.exit(f"media_stills: {f} missing (make c64 disk)")
    os.makedirs(outdir, exist_ok=True)
    order = index()
    names = argv or UI + order
    bad = [n for n in names if n not in UI and n not in order]
    if bad:
        sys.exit(f"media_stills: unknown capture(s): {' '.join(bad)}")
    fail = 0
    for i, n in enumerate(names, 1):
        print(f"[{i}/{len(names)}] {n}", flush=True)
        fail += not capture(n, order, outdir)
    if fail:
        sys.exit(f"media_stills: {fail} capture(s) failed")


if __name__ == "__main__":
    main(sys.argv[1:])
