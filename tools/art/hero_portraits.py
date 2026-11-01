#!/usr/bin/env python3
"""The portraits of the hero choice (the screen where the players join and
each takes a hero): the four heroes side by side, so small ones.

  portrait_blip    select_blip of inter.gfx (397x300), scaled down to fit
                   136x104;
  portrait_blop    select_blop (548x285), likewise;
  portrait_bleach  select_blip recoloured as Bleach's sprites are
                   (new_heroes.bleach: the blue body pink, the red bandana
                   purple), then scaled down;
  portrait_blitz   select_blop recoloured as Blitz's sprites are
                   (new_heroes.blitz: the red body green, the green helmet
                   khaki; the cigar stays brown), then scaled down.

Scaling averages the source pixels each output pixel covers (an area
filter); an output pixel is transparent when less than half of what it
covers is opaque, and the colour of an opaque one is the average of the
opaque pixels it covers. The images are LGX version 1 (RGB555), their
hotspot at their centre. Deterministic: the same bytes give the same bytes.

Used by tools/data/fix_legacy_data.py, which appends them to inter.gfx.
Run alone, it writes a preview sheet:

  hero_portraits.py <data dir> <output dir>
"""

import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import new_heroes  # noqa: E402
from new_heroes import KEY555, KEY565, decode_lgx, encode_lgx, hsv, to8  # noqa: E402

INTER_IMAGES_2002 = 12  # inter.gfx before the portraits
SELECT_BLIP = 2
SELECT_BLOP = 3
BOX = (136, 104)  # the largest portrait: a margin in the 148-pixel cards


def blitz_portrait(r, g, b):
    """Blitz's colours, but the cigar's browns (dark, low saturation
    oranges) stay as they are: Blitz's sprites have no cigar."""
    h, s, v = hsv(r, g, b)
    deg = h * 360
    if 12 <= deg <= 40 and v < 0.75:
        return r, g, b
    return new_heroes.blitz(r, g, b)


def bank_images(gfx):
    """[(hotspot x, hotspot y, LGX blob)] of a .gfx bank."""
    count = struct.unpack_from("<i", gfx, 0)[0]
    images = []
    at = 4
    for _ in range(count):
        hx, hy, size = struct.unpack_from("<iii", gfx, at)
        images.append((hx, hy, gfx[at + 12:at + 12 + size]))
        at += 12 + size
    assert at == len(gfx), "trailing bytes in the bank"
    return images


def rgba(blob):
    """(width, height, [(r, g, b) or None]) of an LGX image (None: the
    colour key)."""
    version, w, h, pixels = decode_lgx(blob)
    out = []
    for p in pixels:
        if version == 0:
            out.append(None if p == KEY565 else (to8(p >> 11, 5), to8((p >> 5) & 63, 6), to8(p & 31, 5)))
        else:
            out.append(None if p == KEY555 else (to8((p >> 10) & 31, 5), to8((p >> 5) & 31, 5), to8(p & 31, 5)))
    return w, h, out


def recolour(image, colour):
    w, h, pixels = image
    cache = {}
    out = []
    for p in pixels:
        if p is not None and p not in cache:
            cache[p] = colour(*p)
        out.append(None if p is None else cache[p])
    return w, h, out


def scale_down(image, box):
    """The image scaled to fit the box, keeping its proportions."""
    w, h, pixels = image
    k = min(box[0] / w, box[1] / h)
    ow, oh = max(1, round(w * k)), max(1, round(h * k))
    sx, sy = w / ow, h / oh

    def spans(n, step, size):
        """For each output index, [(source index, weight)]."""
        out = []
        for o in range(n):
            a, b = o * step, (o + 1) * step
            cells = []
            i = int(a)
            while i < b and i < size:
                cells.append((i, min(b, i + 1) - max(a, i)))
                i += 1
            out.append(cells)
        return out

    xs, ys = spans(ow, sx, w), spans(oh, sy, h)
    out = []
    for oy in range(oh):
        for ox in range(ow):
            area = opaque = r = g = b = 0.0
            for y, wy in ys[oy]:
                row = y * w
                for x, wx in xs[ox]:
                    weight = wx * wy
                    area += weight
                    p = pixels[row + x]
                    if p is not None:
                        opaque += weight
                        r += p[0] * weight
                        g += p[1] * weight
                        b += p[2] * weight
            if opaque * 2 < area:
                out.append(None)
            else:
                out.append((round(r / opaque), round(g / opaque), round(b / opaque)))
    return ow, oh, out


def to_lgx(image):
    """LGX version 1 (RGB555): each colour to its nearest level, the colour
    key for transparent pixels, never for an opaque one."""
    w, h, pixels = image
    words = []
    for p in pixels:
        if p is None:
            words.append(KEY555)
            continue
        word = (round(p[0] * 31 / 255) << 10) | (round(p[1] * 31 / 255) << 5) | round(p[2] * 31 / 255)
        words.append(word ^ 1 if word == KEY555 else word)
    return encode_lgx(1, w, h, words)


def portraits(inter):
    """[(hotspot x, hotspot y, LGX blob)] of the four portraits, in the
    order of the heroes (Blip, Blop, Bleach, Blitz), from the 2002
    inter.gfx (its first 12 images)."""
    images = bank_images(inter)
    blip = rgba(images[SELECT_BLIP][2])
    blop = rgba(images[SELECT_BLOP][2])
    out = []
    for image in (blip, blop, recolour(blip, new_heroes.bleach), recolour(blop, blitz_portrait)):
        small = scale_down(image, BOX)
        out.append((small[0] // 2, small[1] // 2, to_lgx(small)))
    return out


def with_portraits(inter):
    """inter.gfx with the portraits after its 12 images of 2002 (written
    again if it has them already)."""
    images = bank_images(inter)[:INTER_IMAGES_2002]
    assert len(images) == INTER_IMAGES_2002, "inter.gfx has fewer images than in 2002"
    images += portraits(inter)
    out = bytearray(struct.pack("<i", len(images)))
    for hx, hy, blob in images:
        out += struct.pack("<iii", hx, hy, len(blob)) + blob
    return bytes(out)


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    data, out = sys.argv[1], sys.argv[2]
    with open(os.path.join(data, "inter.gfx"), "rb") as f:
        inter = f.read()
    images = portraits(inter)
    gfx = struct.pack("<i", len(images)) + b"".join(
        struct.pack("<iii", hx, hy, len(blob)) + blob for hx, hy, blob in images)
    path = os.path.join(out, "portraits.bmp")
    new_heroes.sheet([gfx], path, scale=2, width=1300)
    print("wrote", path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
