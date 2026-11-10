#!/usr/bin/env python3
"""The mission sheets in a language other than English. The seven sheets
of the campaign (the briefings of bb.lst with a text: brief1.gfx to
brief7.gfx) are pictures with their English text typed in: a title in
yellow, the story on the right, the objectives at the bottom left. To type
a translation instead (src/game/src/briefing.cpp), the game needs:

  brief1.gfx ... brief7.gfx  a second image: the sheet without its text,
                the art and the BONUS STAGE stamp of brief2 and brief5
                kept (the stamp filled in where letters covered it);
  brief.lft     the typewriter face of the story and the objectives, its
                glyphs cut from the sheets, plus what French needs: the
                accented lowercase letters (an accent drawn over the
                letter), q (a mirrored p), 0 and 5 (cut apart from the
                "50" where they touch) and a no-break space;
  mission.lft   the face of the titles (MISSION 1 to 4, FINAL MISSION),
                plus an E made of the F and the L.

How the text is found: the art of a sheet is its largest region of
non-black pixels (8-connected); the stamp is its letters' regions, mostly
pure red (no green, no blue). The text is every other non-black pixel of
the sheet's text areas (AREAS), the stamp's pure reds excepted: on the
stamp, the letters are the pixels with some green. The lines of an area
are its runs of rows with text, matched with the English of TEXT; a line's
glyphs are its regions, merged when they overlap horizontally (the dot of
an i). Where serifs touch, one region holds two or three letters: the
letters go to the regions by their widths (the median widths of the lines
where every region is one letter). Each glyph is then the most typical of
its copies (the one nearest to all the others), its rows counted from the
line's baseline (the bottom of its letters without descenders).

A glyph is its ink plus one blank column on the right: the 2002 letters
are one pixel apart, nearly always; the space is 8 pixels (9 between the
inks), the line height 22 (the sheets alternate 21 and 22). The titles'
letters were 3 to 6 pixels apart; they get 2, and a 14-pixel space, so
that MISSION FINALE fits right of the art of brief7 (303 pixels).

The fonts are LGX version 1 (RGB555, black as the colour key: the game
draws them on black); the sheets without text, like the sheets, LGX
version 0 (RGB565), the 16-bit words of the sheet kept as they are.
Deterministic: the same bytes give the same bytes.

Used by tools/data/fix_legacy_data.py. Run alone, it writes preview
images of the sheets without text and of the fonts:

  briefings.py <data dir> <output dir>
"""

import math
import os
import statistics
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from new_heroes import KEY555, decode_lgx, encode_lgx, to8  # noqa: E402

SHEETS = ["brief1", "brief2", "brief3", "brief4", "brief5", "brief6", "brief7"]

# The text areas of each sheet (left, top, right, bottom, inclusive).
AREAS = {
    "brief1": {"title": (390, 15, 600, 55), "story": (360, 98, 615, 360), "objectives": (35, 314, 300, 384)},
    "brief2": {"story": (330, 84, 639, 348), "objectives": (15, 312, 290, 400)},
    "brief3": {"title": (390, 15, 600, 55), "story": (352, 60, 625, 432), "objectives": (35, 330, 346, 398)},
    "brief4": {"title": (365, 15, 575, 55), "story": (340, 88, 636, 397), "objectives": (8, 311, 305, 402)},
    "brief5": {"story": (350, 110, 635, 350), "objectives": (35, 314, 250, 384)},
    "brief6": {"title": (400, 38, 610, 75), "story": (370, 115, 635, 356), "objectives": (35, 314, 290, 384)},
    "brief7": {"title": (330, 18, 636, 57), "story": (346, 68, 636, 437), "objectives": (35, 314, 170, 382)},
}

# The English of the sheets, line by line as typed (blank lines left out).
TEXT = {
    "brief1": {
        "title": ["MISSION 1"],
        "story": ["Dr Gorgomel, the famous", "scientist, has recently", "disappeared.",
                  "We suspect he has been", "kidnapped.",
                  "You have to go to his", "laboratory near the", "forest and search the",
                  "area in order to find", "him."],
        "objectives": ["Objective :", "-Bring back Dr Gorgomel."],
    },
    "brief2": {
        "story": ["Your plane has been hijacked", "by dorkemon terrorists!",
                  "Some of the creatures", "are hidden in the luggage", "room.",
                  "They are threatening to", "blow up the plane.",
                  "You have to stop them by", "any means!"],
        "objectives": ["Objectives:", "-Defuse the 3 time bombs", "-Get rid of all terrorists"],
    },
    "brief3": {
        "title": ["MISSION 2"],
        "story": ["We have captured the", "leader of the Snuffs.",
                  "After a little friendly", "chitchat, we learnt that", "the Snuffs were buying",
                  "weapons from Snotty", "gunrunners.",
                  "He told us about the", "rendezvous point for the", "next weapon delivery :",
                  "the notorious Honey Night", "Club.",
                  "You must go there and", "arrest the suspect."],
        "objectives": ["Objective :", "-Capture the 'Snott in black'."],
    },
    "brief4": {
        "title": ["MISSION 3"],
        "story": ["Our military satellite", "tracked down the mysterious", "snott in black.",
                  "We lost his position in the", "middle of the Atlantic", "Ocean at latitude 41 46'",
                  "north, longitude 50 14'", "west.",
                  "We suspect the snotty secret", "base to be somewhere nearby.",
                  "Go there and investigate."],
        "objectives": ["Objectives :", "-Find the secret base.", "-Capture the snott in black."],
    },
    "brief5": {
        "story": ["An underground passage", "has been discovered after", "exploring the remains",
                  "of the Snotty base.",
                  "There is a huge number of", "vicious creatures swarming", "in there.",
                  "So, we need your help to", "secure the area."],
        "objectives": ["Objective :", "-Clean up the place"],
    },
    "brief6": {
        "title": ["MISSION 4"],
        "story": ["You have discovered a", "way to another base.",
                  "This one seems to be very", "well guarded and we", "suspect those responsible",
                  "for all this mess to be", "inside.",
                  "Expect a very strong", "resistance."],
        "objectives": ["Objective :", "-Find the high council."],
    },
    "brief7": {
        "title": ["FINAL MISSION"],
        "story": ["The high council base has", "been destroyed.",
                  "As a reward we give you a", "brand new car : the", "Ballsmobile.",
                  "However, Dario is still", "alive and has been spotted", "on Big Mama - a gigantic",
                  "asteroid.",
                  "Use the flying ability of", "the Ballsmobile to get", "there and do a favour to",
                  "all mankind by eliminating", "this sick man."],
        "objectives": ["Objective :", "-Kill Dario."],
    },
}

WIDTH, HEIGHT = 640, 454
NO_DESCENDER = set("acemnorsuvwxz")

# The fonts: rows from the baseline (0) of the glyphs' cells.
TEXT_TOP, TEXT_BOTTOM = -15, 3
TEXT_LINE_HEIGHT, TEXT_SPACE = 22, 8
TITLE_TOP, TITLE_BOTTOM = -27, 0
TITLE_LINE_HEIGHT, TITLE_SPACE, TITLE_GAP = 30, 14, 2
NO_BREAK_SPACE = " "

# The accents, drawn over or under the letter's centre (x) in rows
# counted from the baseline: polylines (x, y) and their width.
# Over a lowercase letter: rows -15 to -12 (the letters start at -10).
ACUTE = ([(1.5, -13.6), (-0.8, -11.9)], 1.7)
GRAVE = ([(-1.5, -13.6), (0.8, -11.9)], 1.7)
CIRCUMFLEX = ([(-2.6, -11.9), (0.0, -14.2), (2.6, -11.9)], 1.5)
DIAERESIS = ([(-2.3, -12.6)], 2.3), ([(2.3, -12.6)], 2.3)
# Under the c: rows 0 to 3.
CEDILLA = ([(0.2, -0.2), (0.4, 1.0), (1.7, 1.7), (1.1, 2.8), (-1.3, 3.1)], 1.4)
ACCENTED = {
    "à": ("a", [GRAVE]), "â": ("a", [CIRCUMFLEX]), "ç": ("c", [CEDILLA]),
    "é": ("e", [ACUTE]), "è": ("e", [GRAVE]), "ê": ("e", [CIRCUMFLEX]), "ë": ("e", list(DIAERESIS)),
    "î": ("i", [CIRCUMFLEX]), "ï": ("i", list(DIAERESIS)), "ô": ("o", [CIRCUMFLEX]),
    "ù": ("u", [GRAVE]), "û": ("u", [CIRCUMFLEX]), "ü": ("u", list(DIAERESIS)),
}


# --- The sheets ----------------------------------------------------------

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


def bank(images):
    out = bytearray(struct.pack("<i", len(images)))
    for hx, hy, blob in images:
        out += struct.pack("<iii", hx, hy, len(blob)) + blob
    return bytes(out)


def rgb(word):
    return to8(word >> 11, 5), to8((word >> 5) & 63, 6), to8(word & 31, 5)


def word565(c):
    return (round(c[0] * 31 / 255) << 11) | (round(c[1] * 63 / 255) << 5) | round(c[2] * 31 / 255)


def is_red(word):
    """A pure red: the stamp's colours (the letters typed over it add green
    and blue)."""
    return word != 0 and word & 0x07FF == 0


def regions(points):
    """The 8-connected regions of a set of (x, y), each a sorted list."""
    seen = set()
    out = []
    for p in sorted(points):
        if p in seen:
            continue
        seen.add(p)
        stack, region = [p], []
        while stack:
            x, y = stack.pop()
            region.append((x, y))
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    q = (x + dx, y + dy)
                    if q in points and q not in seen:
                        seen.add(q)
                        stack.append(q)
        out.append(sorted(region))
    return out


class Sheet:
    """A sheet: its 16-bit words, its art, stamp and text."""

    def __init__(self, name, blob):
        version, w, h, words = decode_lgx(blob)
        assert (version, w, h) == (0, WIDTH, HEIGHT), name
        self.name = name
        self.words = words
        ink = {(i % w, i // w) for i, v in enumerate(words) if v != 0}
        found = regions(ink)
        art = max(found, key=len)
        self.art = set(art)
        self.stamp = set()
        for r in found:
            xs = [x for x, _ in r]
            ys = [y for _, y in r]
            big = max(xs) - min(xs) > 40 or max(ys) - min(ys) > 40
            if r is not art and big and sum(is_red(self.at(p)) for p in r) * 2 > len(r):
                self.stamp.update(r)
        self.text = {}
        for area, (x0, y0, x1, y1) in AREAS[name].items():
            self.text[area] = {(x, y) for y in range(y0, y1 + 1) for x in range(x0, x1 + 1)
                               if (x, y) in ink and (x, y) not in self.art
                               and not ((x, y) in self.stamp and is_red(self.at((x, y))))}
        # What is not red on the stamp was typed over it.
        typed = set().union(*self.text.values())
        assert all(is_red(self.at(p)) or p in typed for p in self.stamp), (name, "letters outside AREAS")

    def at(self, p):
        return self.words[p[1] * WIDTH + p[0]]

    def without_text(self):
        """The 16-bit words of the sheet without its text: black, but on
        the stamp, filled in from the pixels around (the average of the
        known 8 neighbours, ring after ring)."""
        words = list(self.words)
        text = set().union(*self.text.values())
        hidden = set()
        for x, y in text:
            words[y * WIDTH + x] = 0
            if (x, y) in self.stamp:
                hidden.add((x, y))
        while hidden:
            filled = {}
            for x, y in sorted(hidden):
                known = [rgb(words[(y + dy) * WIDTH + x + dx])
                         for dy in (-1, 0, 1) for dx in (-1, 0, 1)
                         if (dx or dy) and 0 <= x + dx < WIDTH and 0 <= y + dy < HEIGHT
                         and (x + dx, y + dy) not in hidden]
                if known:
                    filled[(x, y)] = word565(tuple(round(sum(c[i] for c in known) / len(known))
                                                   for i in range(3)))
            assert filled, "nothing around the stamp's letters"
            for (x, y), v in filled.items():
                words[y * WIDTH + x] = v
                hidden.discard((x, y))
        return words

    def lines(self, area):
        """[(English line, its points)] of an area."""
        points = self.text[area]
        rows = sorted({y for _, y in points})
        runs = []
        for y in rows:
            if runs and y - runs[-1][1] <= 2:
                runs[-1][1] = y
            else:
                runs.append([y, y])
        english = TEXT[self.name][area]
        assert len(runs) == len(english), (self.name, area, len(runs), len(english))
        return [(s, {p for p in points if y0 <= p[1] <= y1}) for s, (y0, y1) in zip(english, runs)]


# --- Glyphs ----------------------------------------------------------------

def glyph_regions(points):
    """The regions of a line, merged when they overlap horizontally by
    half the narrower one, left to right: [(left, right, points)]."""
    found = [(min(x for x, _ in r), max(x for x, _ in r), r) for r in regions(points)]
    found.sort()
    merged = []
    for left, right, r in found:
        if merged:
            m_left, m_right, m = merged[-1]
            overlap = min(m_right, right) - max(m_left, left) + 1
            if overlap * 2 >= min(right - left + 1, m_right - m_left + 1):
                merged[-1] = (min(m_left, left), max(m_right, right), m + r)
                continue
        merged.append((left, right, r))
    return merged


def labelled(lines):
    """[(letters, region, line number)] of lines [(string, points)]: the
    letters each region holds."""
    widths = {}
    for s, points in lines:
        found = glyph_regions(points)
        letters = s.replace(" ", "")
        if len(found) == len(letters):
            for c, (left, right, _) in zip(letters, found):
                widths.setdefault(c, []).append(right - left + 1)
    median = {c: statistics.median(v) for c, v in widths.items()}
    out = []
    for n, (s, points) in enumerate(lines):
        found = glyph_regions(points)
        letters = s.replace(" ", "")
        count, m = len(letters), len(found)
        # cost[i][j]: the first i regions holding the first j letters.
        cost = [[math.inf] * (count + 1) for _ in range(m + 1)]
        back = {}
        cost[0][0] = 0
        for i, (left, right, _) in enumerate(found):
            width = right - left + 1
            for j in range(count):
                if cost[i][j] == math.inf:
                    continue
                for k in (1, 2, 3):
                    if j + k > count:
                        break
                    expected = sum(median.get(c, 9) for c in letters[j:j + k]) + (k - 1) * 0.5
                    c = cost[i][j] + abs(width - expected) + (k - 1) * 2
                    if c < cost[i + 1][j + k]:
                        cost[i + 1][j + k] = c
                        back[(i + 1, j + k)] = k
        assert cost[m][count] < math.inf, s
        j, held = count, []
        for i in range(m, 0, -1):
            k = back[(i, j)]
            held.append(letters[j - k:j])
            j -= k
        held.reverse()
        out += [(h, region, n) for h, region in zip(held, found)]
    return out


def copies(sheets, areas):
    """{letters: [{(dx, dy): (r, g, b)}]}: every copy of every glyph of
    those areas, dx from its ink's left, dy from its line's baseline."""
    out = {}
    for sheet in sheets:
        for area in areas:
            if area not in sheet.text:
                continue
            lines = sheet.lines(area)
            found = labelled(lines)
            bottoms = {}
            for letters, (_, _, points), n in found:
                if all(c in NO_DESCENDER for c in letters):
                    bottoms.setdefault(n, []).append(max(y for _, y in points))
            for letters, (left, _, points), n in found:
                if n in bottoms:
                    base = statistics.median_low(bottoms[n])
                else:  # a title: its capitals sit on the baseline
                    base = statistics.mode(max(y for _, y in r[2]) for r in glyph_regions(lines[n][1]))
                out.setdefault(letters, []).append(
                    {(x - left, y - base): rgb(sheet.at((x, y))) for x, y in points})
    return out


def typical(glyphs):
    """The copy nearest to all the others (copies typed over the stamp,
    pink, left out)."""
    clean = [g for g in glyphs if all(abs(c[0] - c[2]) <= 24 for c in g.values())] or glyphs
    clean = clean[:40]

    def distance(a, b):
        return sum(abs(a.get(k, (0, 0, 0))[1] - b.get(k, (0, 0, 0))[1]) for k in set(a) | set(b))

    return min(clean, key=lambda g: sum(distance(g, o) for o in clean))


def width_of(glyph):
    return max(dx for dx, _ in glyph) + 1


def cropped(glyph):
    left = min(dx for dx, _ in glyph)
    return {(dx - left, dy): c for (dx, dy), c in glyph.items()}


def coverage(polyline, width, cx):
    """{(dx, dy): coverage} of a stroke along polyline (offset by cx) with
    round ends, sampled 8 x 8 times per pixel."""
    half = width / 2
    points = [(cx + x, y) for x, y in polyline]
    segments = list(zip(points, points[1:])) or [(points[0], points[0])]
    xs = [x for x, _ in points]
    ys = [y for _, y in points]
    out = {}
    for py in range(math.floor(min(ys) - half), math.ceil(max(ys) + half)):
        for px in range(math.floor(min(xs) - half), math.ceil(max(xs) + half)):
            hits = 0
            for sy in range(8):
                for sx in range(8):
                    x, y = px + (sx + 0.5) / 8, py + (sy + 0.5) / 8
                    for (ax, ay), (bx, by) in segments:
                        lx, ly = bx - ax, by - ay
                        t = 0.0 if lx == ly == 0 else max(0.0, min(1.0, ((x - ax) * lx + (y - ay) * ly) /
                                                                   (lx * lx + ly * ly)))
                        if (x - ax - t * lx) ** 2 + (y - ay - t * ly) ** 2 <= half * half:
                            hits += 1
                            break
            if hits:
                out[(px, py)] = hits / 64
    return out


def accented(base, accents):
    """The letter with its accents; an i loses its dot, the accents go over
    where the dot was."""
    glyph = dict(base)
    dot = [(dx, dy) for dx, dy in glyph if dy <= -11]
    if dot:  # the dotless i
        weights = [(dx, glyph[(dx, dy)][1]) for dx, dy in dot]
        cx = sum((dx + 0.5) * w for dx, w in weights) / sum(w for _, w in weights)
        for p in dot:
            del glyph[p]
    else:
        cx = width_of(glyph) / 2
    for polyline, width in accents:
        for (dx, dy), cover in coverage(polyline, width, cx).items():
            v = round(255 * cover)
            old = glyph.get((dx, dy), (0, 0, 0))
            if v > old[1]:
                glyph[(dx, dy)] = (v, v, v)
    return glyph


def split(pair, column):
    """The two glyphs of a region holding two letters, cut before column."""
    left = {(dx, dy): c for (dx, dy), c in pair.items() if dx < column}
    right = {(dx, dy): c for (dx, dy), c in pair.items() if dx >= column}
    return cropped(left), cropped(right)


def stem(glyph):
    """The column with the most ink in the middle rows (a capital's stem)."""
    ink = {}
    for (dx, dy), c in glyph.items():
        if -20 <= dy <= -5:
            ink[dx] = ink.get(dx, 0) + c[1]
    return max(sorted(ink), key=lambda dx: ink[dx])


def union(a, b, shift):
    """The glyph a with b drawn over it, b moved right by shift."""
    out = dict(a)
    for (dx, dy), c in b.items():
        p = (dx + shift, dy)
        if c[1] > out.get(p, (0, 0, 0))[1]:
            out[p] = c
    return cropped(out)


def text_glyphs(sheets):
    found = copies(sheets, ["story", "objectives"])
    glyphs = {c: cropped(typical(v)) for c, v in found.items() if len(c) == 1}
    five, zero = split(found["50"][0], 10)  # "50" of brief4: they touch on columns 9 and 10
    glyphs.setdefault("5", five)
    glyphs.setdefault("0", zero)
    p = glyphs["p"]
    glyphs["q"] = {(width_of(p) - 1 - dx, dy): c for (dx, dy), c in p.items()}
    for c, (letter, accents) in ACCENTED.items():
        glyphs[c] = accented(glyphs[letter], accents)
    return glyphs


def title_glyphs(sheets):
    found = copies(sheets, ["title"])
    glyphs = {c: cropped(typical(v)) for c, v in found.items() if len(c) == 1}
    f, l = glyphs["F"], glyphs["L"]
    glyphs["E"] = union(f, l, stem(f) - stem(l))
    return glyphs


# --- Fonts -----------------------------------------------------------------

def glyph_lgx(glyph, top, bottom, gap):
    """LGX version 1 of a glyph: its ink and gap blank columns on its right,
    rows top to bottom; black is the colour key."""
    w = (width_of(glyph) if glyph else 0) + gap
    words = []
    for dy in range(top, bottom + 1):
        for dx in range(w):
            c = glyph.get((dx, dy), (0, 0, 0))
            word = (round(c[0] * 31 / 255) << 10) | (round(c[1] * 31 / 255) << 5) | round(c[2] * 31 / 255)
            # Black is the key (what rounds to black too: the game draws
            # the fonts on black, and on the stamp, where black would show).
            words.append(KEY555 if word == 0 else word ^ 1 if word == KEY555 else word)
    for (dx, dy) in glyph:
        assert top <= dy <= bottom, ("glyph outside its cell", dy)
    return encode_lgx(1, w, bottom - top + 1, words)


def font(glyphs, top, bottom, gap, line_height, space, blanks=()):
    """A .lft font: glyphs by character, blanks (characters drawn as
    nothing, space wide)."""
    slots = {}
    for c, glyph in glyphs.items():
        slots[c.encode("cp1252")[0]] = glyph_lgx(glyph, top, bottom, gap)
    for c in blanks:
        slots[c.encode("cp1252")[0]] = glyph_lgx({}, top, bottom, space)
    out = bytearray(struct.pack("<ii", line_height, space))
    for code in range(1, 256):
        blob = slots.get(code, b"")
        out += struct.pack("<i", len(blob)) + blob
    return bytes(out)


def new_files(read):
    """{name: bytes} of the files this script writes; read(name) gives a
    2002 file (a sheet's bank may have its second image already)."""
    banks = {name: bank_images(read("data/%s.gfx" % name)) for name in SHEETS}
    sheets = [Sheet(name, banks[name][0][2]) for name in SHEETS]
    out = {}
    for sheet in sheets:
        plain = encode_lgx(0, WIDTH, HEIGHT, sheet.without_text())
        out["data/%s.gfx" % sheet.name] = bank([banks[sheet.name][0], (0, 0, plain)])
    out["data/brief.lft"] = font(text_glyphs(sheets), TEXT_TOP, TEXT_BOTTOM, 1, TEXT_LINE_HEIGHT, TEXT_SPACE,
                                 blanks=[NO_BREAK_SPACE])
    out["data/mission.lft"] = font(title_glyphs(sheets), TITLE_TOP, TITLE_BOTTOM, TITLE_GAP, TITLE_LINE_HEIGHT,
                                   TITLE_SPACE)
    return out


# --- Previews --------------------------------------------------------------

def write_bmp(path, w, h, pixel):
    """A 24-bit BMP; pixel(x, y) gives (r, g, b)."""
    stride = (w * 3 + 3) & ~3
    rows = bytearray()
    for y in range(h - 1, -1, -1):
        row = bytearray()
        for x in range(w):
            r, g, b = pixel(x, y)
            row += bytes((b, g, r))
        rows += row + b"\0" * (stride - len(row))
    header = struct.pack("<2sIHHI", b"BM", 54 + len(rows), 0, 0, 54)
    info = struct.pack("<IiiHHIIiiII", 40, w, h, 1, 24, 0, len(rows), 2835, 2835, 0, 0)
    with open(path, "wb") as f:
        f.write(header + info + rows)


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    data, out_dir = sys.argv[1], sys.argv[2]

    def read(name):
        with open(os.path.join(data, os.path.basename(name)), "rb") as f:
            return f.read()

    files = new_files(read)
    for name in SHEETS:
        _, w, h, words = decode_lgx(bank_images(files["data/%s.gfx" % name])[1][2])
        path = os.path.join(out_dir, name + "-without-text.bmp")
        write_bmp(path, w, h, lambda x, y: rgb(words[y * w + x]))
        print("wrote", path)
    for name in ("brief", "mission"):
        lft = files["data/%s.lft" % name]
        glyphs = []
        at = 8
        for code in range(1, 256):
            size = struct.unpack_from("<i", lft, at)[0]
            if size:
                glyphs.append(decode_lgx(lft[at + 4:at + 4 + size]))
            at += 4 + size
        scale, w = 3, sum(g[1] + 2 for g in glyphs)
        h = max(g[2] for g in glyphs)

        def pixel(x, y, glyphs=glyphs):
            x //= scale
            y //= scale
            for _, gw, gh, words in glyphs:
                if x < gw:
                    if y >= gh:
                        return 0, 0, 60
                    v = words[y * gw + x]
                    return (0, 0, 0) if v == KEY555 else (to8(v >> 10, 5), to8((v >> 5) & 31, 5), to8(v & 31, 5))
                x -= gw
                if x < 2:
                    return 60, 0, 0
                x -= 2
            return 0, 0, 60

        path = os.path.join(out_dir, name + ".bmp")
        write_bmp(path, w * scale, h * scale, pixel)
        print("wrote", path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
