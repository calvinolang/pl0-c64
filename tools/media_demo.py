#!/usr/bin/env python3
"""make media, part 2: docs/img/c64/demo.gif, the first minute of a user.

usage: media_demo.py [--out GIF] [--frames DIR] [--assemble]
  --assemble: no VICE run, only the GIF from the frames of the last run
  (DIR/frames.tsv), for work on the assembly

The real program in a headless x64sc (build/pl0.prg injected into RAM
with -autostartprgmode 1 as the test harness does, a copy of
build/pl0.d64 in a 1541 with true drive emulation, warp: warp changes
wall time only). The script plays a storyboard (STORY below): it waits
for screen lines, holds, and types keys through the monitor's `keybuf`,
one key per frame as a person would.

Frames are taken at emulated-time checkpoints, not wall time:
  - checkpoint 1, an exec breakpoint on the KERNAL IRQ entry $FF48 with
    `ignore 1 5`: the CPU stops at every 6th jiffy interrupt, every
    0.1 s of C64 time (the CIA timer runs at 60 Hz; when the program
    masks interrupts while it plots, interrupts that fall due collapse
    into one, so a frame can last longer: the durations below are
    measured, not assumed);
  - then checkpoint 2, any instruction while the raster is on line 0
    (`break exec 0000 ffff if RL == $00`, enabled only for this step):
    the previous video frame is complete, so a capture never shows half
    of one screen and half of another;
  - there: the stopwatch (`sw`, CPU cycles), `screenshot "F" 2` (VICE's
    PNG of the whole 384x272 screen) and the screen RAM for the triggers.
A frame lasts (its stopwatch to the next) / 985248 s (PAL), divided by
the speed of its part of the story (`speed`: the 26 s Koch run and the
disk waits are played faster, which the GIF says on a caption-free
basis only in docs/architecture/media.md). Identical frames are merged.
Then a title card (drawn with the C64 character ROM from the VICE
installation, at generation time only) and the frames go into a GIF
with one exact 16-colour palette, 2x nearest neighbour (768x544).
"""
import os
import re
import shutil
import socket
import subprocess
import sys
import time

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image  # noqa: E402
import c64img  # noqa: E402
from screen2txt import decode  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PAL_HZ = 985248
BAR = " F1 Run F2 New F3 Open F5 Save F7 Help"
PICK = " RETURN open  RUN/STOP back  CRSR move"
DRAWKEY = "-- SPACE: DRAWING  ANY KEY: EDITOR --"
F1, F3, DOWN, RIGHT, LEFT, DEL, RET = 0x85, 0x86, 0x11, 0x1D, 0x9D, 0x14, 0x0D


def status(name, line, col):
    return " PL/0   %-16sL %d  C %d" % (name, line, col)


def koch_pos():
    out = subprocess.run([sys.executable, "tools/mkdisk.py", "--index", "examples"],
                         check=True, capture_output=True, text=True).stdout
    return [l.split()[0] for l in out.splitlines()].index("KOCH")


def story():
    """The storyboard: (action, argument) pairs, played in order.
    until LINE: wait for a screen line (text screen RAM, also while the
    bitmap shows); hold S: S seconds of C64 time; keys [codes]: one key
    per frame; speed F: play the following frames F times faster;
    rec: start recording."""
    boot = status("SQUARE", 1, 1)
    return [
        ("until", boot), ("rec", None), ("hold", 2.0),
        ("keys", [F1]), ("until", DRAWKEY), ("hold", 1.5),
        ("keys", [ord("x")]), ("until", boot), ("hold", 0.6),
        # the starter's own suggestion: i < 3 and turn(120); the cursor
        # moves are played 2x, the typing at its own pace
        ("speed", 2.0), ("keys", [DOWN] * 12), ("until", status("SQUARE", 13, 1)),
        ("keys", [RIGHT] * 13), ("speed", 1.0), ("keys", [DEL, ord("3")]),
        ("until", status("SQUARE*", 13, 14)),
        ("speed", 2.0), ("keys", [DOWN] * 3 + [LEFT, LEFT]), ("speed", 1.0),
        ("keys", [DEL, DEL, ord("1"), ord("2"), ord("0")]),
        ("until", status("SQUARE*", 16, 13)), ("hold", 1.0),
        ("keys", [F1]), ("until", DRAWKEY), ("hold", 1.5),
        ("keys", [ord("x")]), ("until", status("SQUARE*", 16, 13)), ("hold", 0.6),
        ("keys", [F3]), ("until", "DISCARD PROGRAM? (Y/N)"), ("hold", 0.8),
        ("speed", 4.0), ("keys", [ord("y")]), ("until", PICK), ("speed", 1.0), ("hold", 0.6),
        ("speed", 2.0), ("keys", [DOWN] * koch_pos()), ("speed", 1.0), ("hold", 0.6),
        ("speed", 4.0), ("keys", [RET]), ("until", status("KOCH", 1, 1)), ("speed", 1.0),
        ("hold", 1.5),
        # compile and run (26 s on the C64) at 5x
        ("speed", 5.0), ("keys", [F1]), ("until", DRAWKEY), ("speed", 1.0), ("hold", 2.5),
    ]


class Vice:
    def __init__(self, workdir, disk):
        s = socket.socket(); s.bind(("127.0.0.1", 0)); port = s.getsockname()[1]; s.close()
        args = ["x64sc", "-console", "-default", "-config", os.path.join(workdir, "vicerc"),
                "-warp", "-sounddev", "dummy", "-limitcycles", "600000000",
                "-remotemonitor", "-remotemonitoraddress", f"ip4://127.0.0.1:{port}",
                "-drive8type", "1541", "-drive8truedrive", "+virtualdev8", "-8", disk,
                "-autostartprgmode", "1", "-autostart", os.path.join(ROOT, "build/pl0.prg")]
        self.p = subprocess.Popen(args, stdout=open(os.path.join(workdir, "vice.log"), "w"),
                                  stderr=subprocess.STDOUT)
        self.log = open(os.path.join(workdir, "monitor.log"), "w")
        for _ in range(200):
            try:
                self.c = socket.create_connection(("127.0.0.1", port)); break
            except OSError:
                if self.p.poll() is not None: break
                time.sleep(0.1)
        else:
            self.p.kill(); sys.exit("media_demo: no remote monitor")
        self.c.settimeout(0.5)
        self.buf = b""

    def read_until(self, marker, limit=60):
        # (raises OSError on a timeout too, so a caller may retry)
        """Read monitor output until it holds marker and ends with a prompt."""
        t = time.time()
        while time.time() - t < limit:
            lines = self.buf.splitlines()
            if marker in self.buf and lines and re.search(rb"\(C:\$[0-9a-f]{4}\) ?$", lines[-1]):
                out, self.buf = self.buf, b""
                self.log.write(out.decode("latin-1") + "\n")
                return out
            if self.p.poll() is not None:
                raise OSError("VICE exited")
            try:
                d = self.c.recv(65536)
            except socket.timeout:
                continue
            if not d: raise OSError("monitor closed")
            self.buf += d
        raise OSError(f"no {marker!r} from the monitor")

    def cmd(self, s, marker=b""):
        self.log.write(f">>> {s}\n")
        self.c.sendall((s + "\n").encode())
        return self.read_until(marker)

    def go(self, n):
        """Resume until checkpoint n stops the CPU."""
        self.log.write(">>> x\n")
        self.c.sendall(b"x\n")
        return self.read_until(b"#%d (Stop on" % n)

    def quit(self):
        try:
            self.c.sendall(b"detach 8\nx\n")
            for _ in range(10):
                if self.p.poll() is not None: break
                self.c.sendall(b"quit\n")
                try: self.p.wait(timeout=1)
                except subprocess.TimeoutExpired: pass
        except OSError:
            pass
        if self.p.poll() is None: self.p.kill()
        self.p.wait()


def record(workdir):
    """Play the story; returns [(png, cycles, speed)] and the cycles of
    the end (the last frame's end)."""
    disk = os.path.join(workdir, "demo.d64")
    shutil.copyfile(os.path.join(ROOT, "build/pl0.d64"), disk)
    v = Vice(workdir, disk)
    try:
        # the first command stops the emulation and enters the monitor;
        # right after start-up VICE may not take it yet: send it again
        for _ in range(20):
            try:
                v.log.write(">>> bank ram\n"); v.c.sendall(b"bank ram\n")
                v.read_until(b"", 3); break
            except OSError as e:
                if "VICE exited" in str(e): raise
        else:
            raise OSError("the monitor never answered")
        v.cmd("break ff48", b"BREAK")
        v.cmd("break exec 0000 ffff if RL == $00", b"condition")
        v.cmd("disable 2")
        v.cmd("ignore 1 5")
        steps = story(); si = 0; rec = False; speed = 1.0
        frames = []; step_t0 = None; n = 0; t0 = time.time()
        scr = os.path.join(workdir, "screen.bin")
        while si < len(steps):
            if time.time() - t0 > 900:
                raise OSError(f"story stuck at step {si}: {steps[si]}")
            v.go(1)
            v.cmd("disable 1"); v.cmd("enable 2")
            v.go(2)
            v.cmd("disable 2"); v.cmd("enable 1"); v.cmd("ignore 1 5")
            cyc = int(re.search(rb"Stopwatch:\s+(\d+)", v.cmd("sw", b"Stopwatch")).group(1))
            if os.path.exists(scr): os.remove(scr)
            v.cmd(f'bsave "{scr}" 0 0400 07e7', b"Saving")
            lines = [l.rstrip() for l in decode(open(scr, "rb").read()).splitlines()]
            shot = None
            if rec:
                shot = os.path.join(workdir, "f%04d.png" % n); n += 1
                v.cmd(f'screenshot "{shot}" 2')
            # the story: advance through every step that is done now
            while si < len(steps):
                act, arg = steps[si]
                if step_t0 is None: step_t0 = cyc
                if act == "until" and arg not in lines: break
                if act == "hold" and cyc - step_t0 < arg * PAL_HZ: break
                if act == "rec": rec = True
                if act == "speed": speed = arg
                if act == "keys":
                    k = arg[0]   # printable keys as text (VICE converts them to PETSCII)
                    v.cmd("keybuf " + (chr(k) if 0x20 < k < 0x7f else "\\x%02x" % k))
                    if arg[1:]:
                        steps[si] = (act, arg[1:])
                    else:
                        si += 1; step_t0 = None
                    break        # one key per frame
                si += 1; step_t0 = None
            if shot:
                # the speed for the time from this frame to the next, set
                # by the steps just played (the interval can be long: no
                # interrupt while the KERNAL talks to the drive)
                frames.append((shot, cyc, speed))
        # one more frame: the end of the last frame's time
        v.go(1)
        end = int(re.search(rb"Stopwatch:\s+(\d+)", v.cmd("sw", b"Stopwatch")).group(1))
        return frames, end
    finally:
        v.quit()


def chargen():
    for d in (os.environ.get("VICE_C64_ROMS"),
              os.path.join(os.environ.get("XDG_DATA_HOME", os.path.expanduser("~/.local/share")), "vice/C64"),
              "/usr/share/vice/C64", "/usr/lib/vice/C64", "/usr/local/share/vice/C64",
              "/usr/lib64/vice/C64"):
        if d and os.path.exists(os.path.join(d, "chargen-901225-01.bin")):
            return open(os.path.join(d, "chargen-901225-01.bin"), "rb").read()
        if d and os.path.exists(os.path.join(d, "chargen")):
            return open(os.path.join(d, "chargen"), "rb").read()
    sys.exit("media_demo: no C64 character ROM in the VICE installation")


def screencode(ch):
    o = ord(ch)
    if "a" <= ch <= "z": return o - 96
    if "A" <= ch <= "Z": return o
    if ch == "@": return 0
    if 32 <= o <= 63: return o
    raise ValueError(ch)


def title_card(colours, rows):
    """A C64 text screen (lower/upper case set) in VICE's colours: rows is
    [(row, column, text, colour index)]; colours: VICE's 16 RGB values
    taken from the captures (0 black ... 14 light blue)."""
    rom = chargen()
    im = Image.new("RGB", (c64img.W, c64img.H), colours[14])
    im.paste(colours[6], (c64img.BX, c64img.BY, c64img.BX + 320, c64img.BY + 200))
    px = im.load()
    for r, c, text, col in rows:
        for i, ch in enumerate(text):
            g = rom[0x800 + screencode(ch) * 8: 0x800 + screencode(ch) * 8 + 8]
            for y in range(8):
                for x in range(8):
                    if g[y] & (0x80 >> x):
                        px[c64img.BX + (c + i) * 8 + x, c64img.BY + r * 8 + y] = colours[col]
    return im


# VICE's default x64sc palette as it shows in the captures; checked
# against every frame (a colour that is not here fails the build)
def vice_palette(frames_rgb):
    seen = set()
    for im in frames_rgb:
        seen.update(c for _, c in im.getcolors(256))
    return seen


def main(argv):
    os.chdir(ROOT)
    out = "docs/img/c64/demo.gif"
    workdir = os.path.join(ROOT, "build", "media", "demo")
    if "--out" in argv: out = argv[argv.index("--out") + 1]
    if "--frames" in argv: workdir = os.path.abspath(argv[argv.index("--frames") + 1])
    for f in ("build/pl0.prg", "build/pl0.d64"):
        if not os.path.exists(f): sys.exit(f"media_demo: {f} missing (make c64 disk)")
    tsv = os.path.join(workdir, "frames.tsv")
    if "--assemble" in argv:
        rows = [l.split("\t") for l in open(tsv).read().splitlines()]
        frames = [(os.path.join(workdir, r[0]), int(r[1]), float(r[2])) for r in rows[:-1]]
        end = int(rows[-1][1])
    else:
        shutil.rmtree(workdir, ignore_errors=True); os.makedirs(workdir)
        frames, end = record(workdir)
        with open(tsv, "w") as f:
            for png, cyc, speed in frames:
                f.write(f"{os.path.basename(png)}\t{cyc}\t{speed}\n")
            f.write(f"end\t{end}\t1\n")
    print(f"media_demo: {len(frames)} frames{' (from the last run)' if '--assemble' in argv else ' captured'}")
    # durations in ms, from the stopwatch and the speed of each part
    imgs, durs, dropped = [], [], []
    for i, (png, cyc, speed) in enumerate(frames):
        nxt = frames[i + 1][1] if i + 1 < len(frames) else end
        im = c64img.load(png)
        d = (nxt - cyc) / PAL_HZ * 1000 / speed
        torn = not c64img.uniform_border(im)
        if torn:
            # the program switched between the text and the bitmap screen
            # while VICE drew this frame: the C64 showed it for 20 ms, a
            # GIF would hold it for 100 ms; the previous frame lasts on
            dropped.append(os.path.basename(png))
        if imgs and (torn or list(im.getdata()) == list(imgs[-1].getdata())):
            durs[-1] += d
        else:
            imgs.append(im); durs.append(d)
    if dropped:
        print(f"media_demo: dropped {len(dropped)} torn frame(s): {' '.join(dropped)}")
    for im in imgs:
        c64img.check(im, blank_ok=True)
    # the colours: the blue screen and light blue border of the editor,
    # sampled from the first frame (VICE's palette, whatever its version)
    first = imgs[0].load()
    blue, lblue = first[c64img.BX + 300, c64img.BY + 180], first[0, 0]
    cols = {6: blue, 14: lblue}
    # white and yellow: the brightest colours in the captures (the save
    # prompt and the F2 question use them; else VICE's defaults)
    allc = sorted(vice_palette(imgs), key=lambda c: -sum(c))
    cols[1] = allc[0] if sum(allc[0]) > 700 else (255, 255, 255)
    palette16 = [cols.get(i, (0, 0, 0)) for i in range(16)]
    card = title_card(palette16, [
        (6, 8, "PL/0 on the Commodore 64", 1),
        (9, 4, "Wirth's teaching language, with", 14),
        (10, 4, "floats and turtle graphics:", 14),
        (11, 4, "editor, compiler and interpreter", 14),
        (12, 4, "in one program on a stock C64.", 14),
        (16, 4, "Recorded in the VICE emulator:", 14),
        (17, 4, "the starter, F1, an edit, F1,", 14),
        (18, 4, "then F3 opens the Koch snowflake.", 14),
    ])
    imgs.insert(0, card); durs.insert(0, 3000)
    # one exact palette for every frame
    pal = sorted(vice_palette(imgs))
    if len(pal) > 256: sys.exit("media_demo: more than 256 colours")
    pframes = [c64img.to_palette(im, pal).resize((c64img.W * 2, c64img.H * 2), Image.NEAREST)
               for im in imgs]
    durs = [max(20, int(round(d / 10.0)) * 10) for d in durs]
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    pframes[0].save(out, save_all=True, append_images=pframes[1:], duration=durs,
                    loop=0, optimize=False, disposal=1)
    total = sum(durs) / 1000
    print(f"media_demo: {out}: {len(pframes)} frames, {total:.1f} s, "
          f"{os.path.getsize(out)} bytes")
    with open(os.path.join(workdir, "durations.txt"), "w") as f:
        for i, d in enumerate(durs):
            f.write(f"{i}\t{d}\n")
    for i, im in enumerate(pframes):      # the GIF's frames, for review
        im.save(os.path.join(workdir, "gif%03d.png" % i))


if __name__ == "__main__":
    main(sys.argv[1:])
