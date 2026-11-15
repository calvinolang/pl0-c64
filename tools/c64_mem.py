#!/usr/bin/env python3
"""Memory check of the C64 program (make c64-mem).

usage: c64_mem.py MAPFILE

Reads the segment list of the ld65 map (build/c64/pl0.map) and prints the
end of each area and the bytes left: low RAM (BSS must end at $97FF at the
latest; the C stack is $9800-$9FFF), HIBSS ($C000-$CBFF) and LOWBSS (the
cassette buffer, $033C-$03FB). Exit 1 if an area is over its limit.
"""
import re
import sys

LIMITS = {"BSS": 0x97FF, "HIBSS": 0xCBFF, "LOWBSS": 0x03FB}
STARTS = {"HIBSS": 0xC000, "LOWBSS": 0x033C}


def main(argv):
    if len(argv) != 2:
        sys.exit(__doc__)
    seg = {}
    for line in open(argv[1]):
        m = re.match(r"^(\w+)\s+([0-9A-F]{6})\s+([0-9A-F]{6})\s+([0-9A-F]{6})", line)
        if m:
            seg[m.group(1)] = (int(m.group(2), 16), int(m.group(3), 16))
    ok = True
    for name, limit in LIMITS.items():
        if name not in seg:
            continue
        start, end = seg[name]
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
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
