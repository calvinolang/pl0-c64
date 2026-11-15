#!/usr/bin/env python3
"""C64 screen RAM dump (1000 bytes, $0400-$07E7) -> 25 lines of ASCII.

Assumes the lower/upper case character set that cc65 programs use
($D018 = $17): screen codes 1..26 are a..z, 65..90 are A..Z. Reverse video
is ignored. Screen code 100 is the cc65 '_' (PETSCII $A4).
Usage: screen2txt.py DUMP   (also importable: decode(bytes) -> str)
"""
import sys


def ch(c):
    c &= 0x7F                               # drop reverse video
    if c == 0: return "@"
    if 1 <= c <= 26: return chr(c + 96)     # lowercase charset: 1..26 = a..z
    if 65 <= c <= 90: return chr(c)         # 65..90 = A..Z
    if 32 <= c <= 63: return chr(c)
    if c == 100: return "_"
    return "."


def decode(b):
    b = b[:1000]
    return "\n".join("".join(ch(c) for c in b[r * 40:(r + 1) * 40]).rstrip()
                     for r in range(25)) + "\n"


if __name__ == "__main__":
    sys.stdout.write(decode(open(sys.argv[1], "rb").read()))
