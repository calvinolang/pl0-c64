#!/usr/bin/env python3
"""SPIKE helper: C64 screen RAM dump (1000 bytes) -> 25 lines of ASCII."""
import sys
b = open(sys.argv[1], "rb").read()[:1000]
def ch(c):
    c &= 0x7F                       # drop reverse video
    if c == 0: return "@"
    if 1 <= c <= 26: return chr(c + 96)    # lowercase charset: 1..26 = a..z
    if 65 <= c <= 90: return chr(c)        # 65..90 = A..Z
    if 32 <= c <= 63: return chr(c)
    return "."
for r in range(25):
    print("".join(ch(c) for c in b[r * 40:(r + 1) * 40]).rstrip())
