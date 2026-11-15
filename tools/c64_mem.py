#!/usr/bin/env python3
"""Memory check of the C64 program (make c64-mem).

usage: c64_mem.py MAPFILE [--doc DOC | --write-doc DOC]

Reads the segment list of the ld65 map (build/c64/pl0.map) and prints the
end of each area and the bytes left: low RAM (BSS must end at $97FF at the
latest; the C stack is $9800-$9FFF), HIBSS ($C000-$CBFF) and LOWBSS (the
cassette buffer, $033C-$03FB). Exit 1 if an area is over its limit.

The map's module list also gives the size of every object file in every
segment. --write-doc DOC writes the module and area tables generated from
the map between the two marker lines in DOC
(docs/architecture/memory-map.md); --doc DOC fails (exit 1) when those
tables differ from the map, so the document cannot drift from the build:
`make c64-mem` uses --doc, `make c64-mem-doc` rewrites it.
"""
import re
import sys

LIMITS = {"BSS": 0x97FF, "HIBSS": 0xCBFF, "LOWBSS": 0x03FB}
STARTS = {"HIBSS": 0xC000, "LOWBSS": 0x033C}
BEGIN = "<!-- begin c64_mem.py: generated from build/c64/pl0.map, checked by make c64-mem -->"
END = "<!-- end c64_mem.py -->"
# module table columns: (title, segments)
COLS = [("Code", ("CODE", "STARTUP", "ONCE", "EXEHDR")), ("Rodata + data", ("RODATA", "DATA", "INIT")),
        ("BSS", ("BSS",)), ("HIBSS", ("HIBSS",)), ("LOWBSS", ("LOWBSS",)), ("BASICBSS", ("BASICBSS",))]


def read_map(path):
    seg, mods, mod = {}, [], None
    part = None
    for line in open(path):
        if line.startswith("Modules list:"):
            part = "mod"
        elif line.startswith("Segment list:"):
            part = "seg"
        elif line.startswith("Exports list"):
            part = None
        if part == "seg":
            m = re.match(r"^(\w+)\s+([0-9A-F]{6})\s+([0-9A-F]{6})\s+([0-9A-F]{6})", line)
            if m:
                seg[m.group(1)] = (int(m.group(2), 16), int(m.group(3), 16), int(m.group(4), 16))
        elif part == "mod":
            m = re.match(r"^(\S.*):\s*$", line)
            if m and not line.startswith("---"):
                mod = [m.group(1), {}]
                mods.append(mod)
                continue
            m = re.match(r"^\s+(\w+)\s+Offs=\w+\s+Size=([0-9A-F]+)", line)
            if m and mod:
                mod[1][m.group(1)] = mod[1].get(m.group(1), 0) + int(m.group(2), 16)
    return seg, mods


def n(v):
    return "{:,}".format(v) if v else ""


def doc_block(seg, mods):
    """The generated Markdown: module table, then the areas."""
    rows, lib, tot = [], {}, {}
    for name, sizes in mods:
        for s, v in sizes.items():
            tot[s] = tot.get(s, 0) + v
        if name.endswith(".o") and "(" not in name:
            rows.append((name, sizes))
        else:
            for s, v in sizes.items():
                lib[s] = lib.get(s, 0) + v
    rows.append(("cc65 runtime + libc (%d modules)" % sum(1 for m in mods if "(" in m[0]), lib))
    rows.append(("**total**", tot))
    for s, (start, end, size) in seg.items():
        if s != "ZEROPAGE" and s != "LOADADDR" and tot.get(s, 0) != size:
            raise SystemExit("c64_mem: modules add up to %d bytes of %s, the segment has %d"
                             % (tot.get(s, 0), s, size))
    out = ["| Module | " + " | ".join(c[0] for c in COLS) + " |",
           "|---|" + "---:|" * len(COLS)]
    for name, sizes in rows:
        label = name if name.startswith("**") or name.startswith("cc65") else "`%s`" % name
        out.append("| %s | %s |" % (label, " | ".join(n(sum(sizes.get(s, 0) for s in segs))
                                                     for _, segs in COLS)))
    out.append("")
    hdr_start = seg["EXEHDR"][0]
    ro0, ro1 = seg["RODATA"][0], seg["INIT"][1]
    b0, b1 = seg["BSS"][0], seg["BSS"][1]
    hb0, hb1 = seg["HIBSS"][0], seg["HIBSS"][1]
    lb0, lb1 = seg["LOWBSS"][0], seg["LOWBSS"][1]
    a = [("BASIC stub + startup", hdr_start, seg["STARTUP"][1], ""),
         ("CODE", seg["CODE"][0], seg["CODE"][1], ""),
         ("RODATA + DATA + INIT", ro0, ro1, ""),
         ("BSS", b0, b1, "ONCE (run-once startup code) lies under its start"),
         ("**free below the C stack**", b1 + 1, LIMITS["BSS"], "BSS may grow up to $97FF"),
         ("C stack", 0x9800, 0x9FFF, "cc65 parameter stack"),
         ("BASICBSS", seg["BASICBSS"][0], seg["BASICBSS"][1], "the source buffer, under the BASIC ROM"),
         ("HIBSS", hb0, hb1, "{:,} of 3,072 used, {} free".format(hb1 - hb0 + 1, LIMITS["HIBSS"] - hb1)),
         ("LOWBSS", lb0, lb1, "%d of 192 used, %d free (the cassette buffer)"
          % (lb1 - lb0 + 1, LIMITS["LOWBSS"] - lb1))]
    out += ["| Area | Range | Bytes | Notes |", "|---|---|---:|---|"]
    for name, s, e, note in a:
        out.append("| %s | `$%04X-$%04X` | %s | %s |" % (name, s, e, "{:,}".format(e - s + 1), note))
    out.append("")
    img1 = max(ro1, seg["ONCE"][1])
    out.append("Image `$%04X-$%04X` (to the end of ONCE): %s bytes in `pl0.prg` with the "
               "2-byte load address." % (hdr_start, img1, "{:,}".format(img1 - hdr_start + 1 + 2)))
    return "\n".join(out) + "\n"


def doc(mode, path, block):
    text = open(path).read()
    if BEGIN not in text or END not in text:
        print("c64_mem: %s has no generated section (the lines %s and %s)" % (path, BEGIN, END))
        return False
    head, rest = text.split(BEGIN, 1)
    old, tail = rest.split(END, 1)
    new = "\n" + block
    if mode == "write":
        open(path, "w").write(head + BEGIN + new + END + tail)
        print("c64_mem: wrote the tables in %s" % path)
        return True
    if old != new:
        print("c64_mem: FAIL: the tables in %s differ from the map; "
              "run make c64-mem-doc (python3 tools/c64_mem.py MAP --write-doc %s)" % (path, path))
        o, w = old.splitlines(), new.splitlines()
        for k in range(max(len(o), len(w))):
            if k >= len(o) or k >= len(w) or o[k] != w[k]:
                print("  doc: %s\n  map: %s" % (o[k] if k < len(o) else "<none>",
                                                w[k] if k < len(w) else "<none>"))
        return False
    print("c64_mem: %s matches the map" % path)
    return True


def main(argv):
    if len(argv) not in (2, 4) or (len(argv) == 4 and argv[2] not in ("--doc", "--write-doc")):
        sys.exit(__doc__)
    seg, mods = read_map(argv[1])
    ok = True
    for name, limit in LIMITS.items():
        if name not in seg:
            # a renamed or removed segment must not pass the gate unchecked
            print("c64_mem: FAIL: segment %s is not in the map %s (renamed? update LIMITS)"
                  % (name, argv[1]))
            ok = False
            continue
        start, end, _ = seg[name]
        free = limit - end
        if name == "BSS":
            print("c64_mem: BSS $%04X-$%04X, CODE ends $%04X; free below $9800: %d bytes"
                  % (start, end, seg["CODE"][1], free))
        else:
            print("c64_mem: %s $%04X-$%04X: %d of %d bytes used, %d free"
                  % (name, start, end, end - STARTS[name] + 1, limit - STARTS[name] + 1, free))
        if free < 0:
            print("c64_mem: FAIL: %s ends past $%04X" % (name, limit))
            ok = False
    if len(argv) == 4:
        ok = doc("write" if argv[2] == "--write-doc" else "check", argv[3], doc_block(seg, mods)) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
