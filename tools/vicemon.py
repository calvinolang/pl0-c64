#!/usr/bin/env python3
"""Run a .prg in headless VICE, wait for a breakpoint, then
run monitor commands over VICE's text remote monitor and quit.

usage: vicemon.py PRG LBL BREAKLABEL OUTDIR CMD [CMD...]
  - starts x64sc -console -warp with the remote monitor on a free port,
    loads the ld65 label file and sets a breakpoint at BREAKLABEL,
  - when the breakpoint is hit, sends each CMD (VICE monitor syntax; may use
    labels such as ._logbuf) and stores the whole session in OUTDIR/monitor.log,
  - saves OUTDIR/screen.png via -exitscreenshot.
Exit code 0 if the breakpoint was hit, 2 if not (cycle limit / timeout).
"""
import os, socket, subprocess, sys, time

prg, lbl, brk, outdir = sys.argv[1:5]
cmds = sys.argv[5:]
os.makedirs(outdir, exist_ok=True)
s = socket.socket(); s.bind(("127.0.0.1", 0)); port = s.getsockname()[1]; s.close()
mon = os.path.join(outdir, "mon.txt")
with open(mon, "w") as f:
    bcmd = brk if brk.split()[0] in ("break", "watch", "trace") else "break " + brk
    f.write(f'll "{lbl}"\n{bcmd}\n')
cycles = os.environ.get("CYCLES", "200000000")
args = ["x64sc", "-console", "-default", "-config", os.path.join(outdir, "vicerc"),
        "-warp", "-sounddev", "dummy", "-limitcycles", cycles,
        "-remotemonitor", "-remotemonitoraddress", f"ip4://127.0.0.1:{port}",
        "-moncommands", mon, "-autostartprgmode", "1",
        "-exitscreenshot", os.path.join(outdir, "screen.png"), "-autostart", prg]
vlog = open(os.path.join(outdir, "vice.log"), "w")
p = subprocess.Popen(args, stdout=vlog, stderr=subprocess.STDOUT)
for _ in range(100):
    try:
        c = socket.create_connection(("127.0.0.1", port)); break
    except OSError:
        time.sleep(0.1)
else:
    p.kill(); sys.exit("no remote monitor")
c.settimeout(1.0)
log = open(os.path.join(outdir, "monitor.log"), "w")
buf = b""; hit = False; t0 = time.time()
while time.time() - t0 < 120 and p.poll() is None:
    try:
        d = c.recv(4096)
        if not d: break
        buf += d
    except socket.timeout:
        pass
    if b"(C:$" in buf:
        hit = True; break
log.write(buf.decode("latin-1"))
def send(cmd):
    c.sendall((cmd + "\n").encode())
    out = b""; t = time.time()
    while time.time() - t < (120 if cmd in ("x", "g") else 30):
        try:
            d = c.recv(65536)
            if not d: break
            out += d
            if out.rstrip().endswith(b")") and b"(C:$" in out.splitlines()[-1]:
                break
        except socket.timeout:
            if out: break
    log.write(f"\n>>> {cmd}\n" + out.decode("latin-1"))
if hit:
    for cmd in cmds:
        send(cmd)
    try:
        c.sendall(b"quit\n")
    except OSError:
        pass
c.close(); log.close()
try:
    p.wait(timeout=30)
except subprocess.TimeoutExpired:
    p.kill()
sys.exit(0 if hit else 2)
