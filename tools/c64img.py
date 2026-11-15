#!/usr/bin/env python3
"""Post-processing of VICE screenshots for the documentation (make media).

VICE's monitor command `screenshot "FILE" 2` writes the whole emulated
screen as an RGBA PNG of 384x272 pixels (320x200 plus the visible border,
VICE's default "normal" border mode) in VICE's default palette. These
helpers check such a capture and turn it into the published form:

  - palette image (mode P) with the colours actually used, exact (a C64
    screen has at most 16 colours, so nothing is lost);
  - scaled 2x with nearest neighbour (768x544), so that it stays sharp in
    browsers and on GitHub, which smooth images scaled up by CSS.

usage: c64img.py IN.png OUT.png     (also importable)
"""
import sys

from PIL import Image

W, H = 384, 272          # VICE screenshot: 320x200 + border
BX, BY = 32, 35          # the 320x200 window in it (PAL: 35 rows above, 37 below)
SCALE = 2


def load(path):
    """The capture as RGB; fails unless it is 384x272."""
    im = Image.open(path).convert("RGB")
    if im.size != (W, H):
        raise ValueError(f"{path}: {im.size[0]}x{im.size[1]}, expected {W}x{H}")
    return im


def check(im, path="capture"):
    """Sanity checks of a capture: a uniform border (one colour all
    around the 320x200 window) and something on the screen (at least two
    colours inside it). Returns the number of colours."""
    px = im.load()
    bx, by = BX, BY
    border = set()
    for x in range(W):
        for y in list(range(by)) + list(range(by + 200, H)):
            border.add(px[x, y])
    for y in range(by, by + 200):
        for x in list(range(bx)) + list(range(bx + 320, W)):
            border.add(px[x, y])
    if len(border) != 1:
        raise ValueError(f"{path}: the border is not one colour ({len(border)} colours)")
    inner = im.crop((bx, by, bx + 320, by + 200)).getcolors(256)
    if inner is None or len(inner) < 2:
        raise ValueError(f"{path}: the screen is blank")
    return len(im.getcolors(256))


def to_palette(im, palette=None):
    """RGB -> P with an exact palette: the image's own colours, or the
    given list of RGB tuples (which must contain every colour used)."""
    cols = palette or sorted(c for _, c in im.getcolors(256))
    index = {c: i for i, c in enumerate(cols)}
    out = Image.new("P", im.size)
    flat = []
    for c in cols:
        flat.extend(c)
    out.putpalette(flat)
    out.putdata([index[c] for c in im.getdata()])
    return out


def publish(src, dst, scale=SCALE):
    """VICE capture src -> checked, palette, scaled PNG dst."""
    im = load(src)
    check(im, src)
    p = to_palette(im)
    if scale != 1:
        p = p.resize((W * scale, H * scale), Image.NEAREST)
    p.save(dst, optimize=True)
    return p


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    publish(sys.argv[1], sys.argv[2])
