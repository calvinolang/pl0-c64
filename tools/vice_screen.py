#!/usr/bin/env python3
"""Run a .prg in headless VICE and watch its text screen.

usage: vice_screen.py [options] --out OUTDIR PRG EXPECTED CYCLES [FAILTEXT]

Starts x64sc headless (the options proven by the Task 1 spike: -console
-default -warp, dummy sound, -limitcycles, text remote monitor,
-autostartprgmode 1), then about every half second of wall time breaks
into the remote monitor, saves the screen RAM $0400-$07E7 with `bsave`,
continues the emulation (`x`) and decodes the dump with screen2txt.

Matching works on whole screen lines (trailing spaces stripped):
  - FAILTEXT is checked first: any line that STARTS with it fails the run
    (exit 1), so `FAIL` also catches `FAIL 3`;
  - EXPECTED must equal a whole line (exit 0); `PASS` does not match
    `PASSED` or `no PASS here`.
If VICE exits first because the cycle limit was reached, or the wall-time
guard expires, the last screen is kept and the exit code is 2. Exit 3: the
remote monitor never answered.

Options (all may be repeated or combined):
  --on TRIGGER CMD   once a screen line equals TRIGGER, send the monitor
                     command CMD (e.g. 'keybuf "12.5\\n"'). Each --on fires
                     once, in the given order: one fires only after all
                     earlier ones have fired.
  --snap             memory handshake for tests/c64/gfx_check.c: the program
                     writes a step number to $02A7 and waits until $02A8
                     equals it. For each new step n the harness saves all
                     64 KB of RAM (`bank ram`) to OUTDIR/snapN.bin, then
                     writes n to $02A8 and resumes.
The last decoded screen is printed and written to OUTDIR/screen.txt, the
monitor session to OUTDIR/monitor.log, VICE's own output to OUTDIR/vice.log.
"""
import os
import socket
import subprocess
import sys
import time

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from screen2txt import decode  # noqa: E402

WALL_LIMIT = float(os.environ.get("VICE_WALL_LIMIT", "300"))
DEBUG = os.environ.get("VICE_DEBUG")
STEP, ACK = 0x02A7, 0x02A8        # --snap handshake bytes (free KERNAL page 2 RAM)


def parse_args(argv):
    on, snap, out, pos = [], False, None, []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--on" and i + 2 < len(argv):
            on.append((argv[i + 1], argv[i + 2])); i += 3
        elif a == "--out" and i + 1 < len(argv):
            out = argv[i + 1]; i += 2
        elif a == "--snap":
            snap = True; i += 1
        else:
            pos.append(a); i += 1
    if out is None or len(pos) not in (3, 4):
        sys.exit(__doc__)
    return on, snap, out, pos


def main():
    on, snap, outdir, pos = parse_args(sys.argv[1:])
    prg, expected, cycles = pos[:3]
    failtext = pos[3] if len(pos) == 4 else None
    os.makedirs(outdir, exist_ok=True)
    outdir = os.path.abspath(outdir)
    dump = os.path.join(outdir, "screen.bin")
    stepf = os.path.join(outdir, "step.bin")
    s = socket.socket(); s.bind(("127.0.0.1", 0)); port = s.getsockname()[1]; s.close()
    args = ["x64sc", "-console", "-default", "-config", os.path.join(outdir, "vicerc"),
            "-warp", "-sounddev", "dummy", "-limitcycles", str(cycles),
            "-remotemonitor", "-remotemonitoraddress", f"ip4://127.0.0.1:{port}",
            "-autostartprgmode", "1", "-autostart", os.path.abspath(prg)]
    vlog = open(os.path.join(outdir, "vice.log"), "w")
    p = subprocess.Popen(args, stdout=vlog, stderr=subprocess.STDOUT)
    log = open(os.path.join(outdir, "monitor.log"), "w")
    c = None
    for _ in range(200):
        try:
            c = socket.create_connection(("127.0.0.1", port)); break
        except OSError:
            if p.poll() is not None: break
            time.sleep(0.1)
    if c is None:
        p.kill()
        print("vice_screen: no remote monitor (VICE log in %s/vice.log)" % outdir)
        return 3
    c.settimeout(0.5)

    def command(cmd, wait_prompt=True, marker=b""):
        """Send one monitor command; with wait_prompt, read until the reply
        contains marker and ends with the monitor prompt."""
        c.sendall((cmd + "\n").encode())
        out = b""; t = time.time()
        while wait_prompt and time.time() - t < 30:
            try:
                d = c.recv(65536)
            except socket.timeout:
                continue
            if not d: raise OSError("monitor closed")
            out += d
            lines = out.splitlines()
            if marker in out and lines and b"(C:$" in lines[-1]:
                break
        log.write(f">>> {cmd}\n{out.decode('latin-1')}\n")
        return out

    def save(path, start, end):
        if os.path.exists(path): os.remove(path)
        command(f'bsave "{path}" 0 {start:04x} {end:04x}', marker=b"Saving file")
        return open(path, "rb").read() if os.path.exists(path) else b""

    screen = ""; rc = 2; t0 = time.time(); fired = 0; acked = 0
    try:
        command("bank ram")
        while p.poll() is None and time.time() - t0 < WALL_LIMIT:
            time.sleep(0.5)
            data = save(dump, 0x0400, 0x07E7)
            if snap:
                st = save(stepf, STEP, STEP)
                if len(st) == 1 and st[0] > acked:
                    n = st[0]
                    save(os.path.join(outdir, f"snap{n}.bin"), 0x0000, 0xFFFF)
                    command(f"> {ACK:04x} {n:02x}")
                    acked = n
            if len(data) == 1000:
                screen = decode(data)
                lines = [l.rstrip() for l in screen.splitlines()]
                if DEBUG: print(round(time.time() - t0, 1), [l for l in lines if l][-1:])
                while fired < len(on) and on[fired][0] in lines:
                    command(on[fired][1]); fired += 1
            command("x", wait_prompt=False)
            if len(data) == 1000:
                if failtext and any(l.startswith(failtext) for l in lines): rc = 1; break
                if expected in lines: rc = 0; break
        else:
            reason = "cycle limit reached" if p.poll() is not None else "wall-time guard expired"
            print(f"vice_screen: {reason} before a screen line equal to {expected!r}")
    except OSError as e:
        print(f"vice_screen: VICE stopped ({e}); cycle limit reached?")
    try:
        c.sendall(b"quit\n")
    except OSError:
        pass
    c.close(); log.close()
    try:
        p.wait(timeout=20)
    except subprocess.TimeoutExpired:
        p.kill()
    open(os.path.join(outdir, "screen.txt"), "w").write(screen)
    return rc


if __name__ == "__main__":
    sys.exit(main())
