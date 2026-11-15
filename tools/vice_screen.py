#!/usr/bin/env python3
"""Run a .prg in headless VICE and watch its text screen.

usage: vice_screen.py PRG EXPECTED CYCLES OUTDIR [FAILTEXT]

Starts x64sc headless (the options proven by the Task 1 spike: -console
-default -warp, dummy sound, -limitcycles, text remote monitor,
-autostartprgmode 1), then about every half second of wall time breaks
into the remote monitor, saves the screen RAM $0400-$07E7 with `bsave`,
continues the emulation (`x`) and decodes the dump with screen2txt.
Stops as soon as the screen contains EXPECTED (exit 0) or FAILTEXT
(exit 1). If VICE exits first because the cycle limit was reached, or
the wall-time guard expires, the last screen is kept and the exit code
is 2. Exit 3: the remote monitor never answered. The last decoded screen
is printed and written to OUTDIR/screen.txt, the monitor session to
OUTDIR/monitor.log, VICE's own output to OUTDIR/vice.log.
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


def main():
    if len(sys.argv) not in (5, 6):
        sys.exit(__doc__)
    prg, expected, cycles, outdir = sys.argv[1:5]
    failtext = sys.argv[5] if len(sys.argv) == 6 else None
    os.makedirs(outdir, exist_ok=True)
    outdir = os.path.abspath(outdir)
    dump = os.path.join(outdir, "screen.bin")
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

    screen = ""; rc = 2; t0 = time.time()
    try:
        while p.poll() is None and time.time() - t0 < WALL_LIMIT:
            time.sleep(0.5)
            if os.path.exists(dump): os.remove(dump)
            command(f'bsave "{dump}" 0 0400 07e7', marker=b"Saving file")
            command("x", wait_prompt=False)
            if os.path.exists(dump) and os.path.getsize(dump) == 1000:
                screen = decode(open(dump, "rb").read())
                if DEBUG: print(round(time.time() - t0, 1), [l for l in screen.splitlines() if l.strip()][-1:])
                if expected in screen: rc = 0; break
                if failtext and failtext in screen: rc = 1; break
        else:
            reason = "cycle limit reached" if p.poll() is not None else "wall-time guard expired"
            print(f"vice_screen: {reason} before the screen showed {expected!r}")
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
