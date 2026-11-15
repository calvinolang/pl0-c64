#!/usr/bin/env python3
"""SPIKE helper: decode a NUL-terminated PETSCII buffer (cc65 c64 charmap)
to ASCII. Lowercase C literals are stored as $41-$5A; ROM output (FOUT)
is plain ASCII, so its 'E' shows up as 'e' here. CR ($0D) = newline."""
import sys
b = open(sys.argv[1], "rb").read().split(b"\0")[0]
out = []
for c in b:
    if c == 0x0D: out.append("\n")
    elif 0x41 <= c <= 0x5A: out.append(chr(c + 32))
    elif 0xC1 <= c <= 0xDA: out.append(chr(c - 0x80))
    elif 32 <= c < 127: out.append(chr(c))
    else: out.append("<%02x>" % c)
sys.stdout.write("".join(out))
