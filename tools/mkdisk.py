#!/usr/bin/env python3
"""Build (and verify) the PL/0 disk image with c1541.

usage: mkdisk.py PRG EXAMPLES_DIR OUT.d64            build, then verify
       mkdisk.py --verify EXAMPLES_DIR IMAGE.d64 [PRG]
                                                     verify only (PL0 against
                                                     PRG too, if given)
       mkdisk.py --index EXAMPLES_DIR                print the INDEX file

The disk (title `PL0 TURTLE`, id `25`) holds:
  - PL0    PRG  the program (build/pl0.prg), LOAD"PL0",8,1 / autostart;
  - INDEX  SEQ  one line per example, `NAME description` + '\\n', in the
                demo order ORDER below (the picker lists them so);
  - one SEQ file per example: the name is the file name without `.pl0`,
    in upper case (FACTORIAL, not FACTORIAL.PL0); the bytes are the
    source as is (ASCII, '\\n' line ends), so a PC reads them back with
    `c1541 IMAGE -read name,s`.
The description is the text of the first { ... } comment, white space
collapsed; it must fit the picker's 24 columns.

c1541 converts file names from ASCII to PETSCII: lower case ASCII becomes
PETSCII $41-$5A, the bytes the C64 program sends for A-Z. So every name
is passed to c1541 in lower case (upper case ASCII would become the
shifted PETSCII letters $C1-$DA). File contents are never converted.
Verification reads every file back with `c1541 -read` and compares bytes,
and checks the directory listing (names and types).
"""
import os
import re
import subprocess
import sys
import tempfile

TITLE = "pl0 turtle,25"
# simple drawings first, then the text programs, then recursion
ORDER = ["square", "polygon", "star", "spiral", "rose", "sine", "lissajous",
         "factorial", "gcd", "primes", "newton", "pi",
         "koch", "sierpinski", "tree"]
DESC_MAX = 24
NAME_RE = re.compile(r"^[A-Z0-9._-]{1,12}$")


def die(msg):
    sys.exit("mkdisk: " + msg)


def examples(exdir):
    found = sorted(f[:-4] for f in os.listdir(exdir) if f.endswith(".pl0"))
    if sorted(ORDER) != found:
        die("examples/*.pl0 %s do not match ORDER %s" % (found, sorted(ORDER)))
    return ORDER


def description(src):
    m = re.search(r"\{([^}]*)\}", src)
    if not m:
        return ""
    d = " ".join(m.group(1).split())
    if len(d) > DESC_MAX:
        die("description %r longer than %d" % (d, DESC_MAX))
    return d


def index_text(exdir):
    lines = []
    for n in examples(exdir):
        src = open(os.path.join(exdir, n + ".pl0"), encoding="ascii").read()
        name = n.upper()
        if not NAME_RE.match(name):
            die("bad file name %r" % name)
        lines.append("%s %s\n" % (name, description(src)))
    return "".join(lines)


def c1541(*args):
    r = subprocess.run(["c1541"] + list(args), stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, text=True, errors="replace")
    if r.returncode != 0:
        die("c1541 %s failed:\n%s" % (" ".join(args), r.stdout))
    return r.stdout


def build(prg, exdir, out):
    idx = index_text(exdir)
    with tempfile.TemporaryDirectory() as tmp:
        ip = os.path.join(tmp, "index")
        open(ip, "w", encoding="ascii", newline="").write(idx)
        tmpimg = out + ".tmp"
        if os.path.exists(tmpimg):
            os.remove(tmpimg)
        c1541("-format", TITLE, "d64", tmpimg)
        args = [tmpimg, "-write", prg, "pl0", "-write", ip, "index,s"]
        for n in examples(exdir):
            args += ["-write", os.path.join(exdir, n + ".pl0"), n + ",s"]
        c1541(*args)
    os.replace(out + ".tmp", out)


def read_back(image, name):
    with tempfile.TemporaryDirectory() as tmp:
        p = os.path.join(tmp, "f")
        c1541(image, "-read", name, p)
        return open(p, "rb").read()


def verify(exdir, image, prg=None):
    listing = c1541(image, "-list").lower()
    want = [("pl0", "prg"), ("index", "seq")] + [(n, "seq") for n in examples(exdir)]
    got = re.findall(r'^\d+\s+"([^"]*)"\s+(\w+)', listing, re.M)
    got = got[1:]                       # the header line: title, id
    if got != want:
        die("directory %s, want %s" % (got, want))
    if '"pl0 turtle' not in listing:
        die("disk title is not PL0 TURTLE:\n" + listing)
    if read_back(image, "index,s") != index_text(exdir).encode("ascii"):
        die("INDEX differs")
    for n in examples(exdir):
        if read_back(image, n + ",s") != open(os.path.join(exdir, n + ".pl0"), "rb").read():
            die("%s differs" % n.upper())
    if prg and read_back(image, "pl0") != open(prg, "rb").read():
        die("PL0 differs from %s" % prg)
    print("mkdisk: %s verified (PL0, INDEX, %d examples)" % (image, len(examples(exdir))))


def main(argv):
    if len(argv) in (3, 4) and argv[0] == "--verify":
        verify(argv[1], argv[2], argv[3] if len(argv) == 4 else None)
    elif len(argv) == 2 and argv[0] == "--index":
        sys.stdout.write(index_text(argv[1]))
    elif len(argv) == 3:
        build(argv[0], argv[1], argv[2])
        verify(argv[1], argv[2], argv[0])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main(sys.argv[1:])
