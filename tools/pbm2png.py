#!/usr/bin/env python3
"""Convert the ASCII PBM (P1) written by `pl0host --pbm` to a PNG.

Usage: pbm2png.py in.pbm out.png [scale]     (standard library only)
"""
import struct
import sys
import zlib


def read_p1(path):
    with open(path) as f:
        tok = f.read().split()
    if tok[0] != "P1":
        sys.exit("not an ASCII PBM (P1) file")
    w, h = int(tok[1]), int(tok[2])
    bits = "".join(tok[3:])
    if len(bits) != w * h:
        sys.exit("bad pixel count")
    return w, h, bits


def chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def write_png(path, w, h, bits, scale):
    raw = bytearray()
    for y in range(h):
        row = bytes(0 if bits[y * w + x] == "1" else 255 for x in range(w) for _ in range(scale))
        for _ in range(scale):
            raw += b"\x00" + row
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w * scale, h * scale, 8, 0, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def main():
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    w, h, bits = read_p1(sys.argv[1])
    write_png(sys.argv[2], w, h, bits, int(sys.argv[3]) if len(sys.argv) == 4 else 1)


if __name__ == "__main__":
    main()
