#!/usr/bin/env python3
"""Checks that the translated string tables can be drawn by the game's fonts.

The fonts of the game (assets/source/fonts) hold few characters: the menu
and banner fonts have capitals, digits and a few signs, no accent; the
dialogue font (rpg) adds lowercase letters and only à è é ê ô. A character
a font lacks is skipped when drawn, without a warning. For each table other
than uk.json, every string must:

  * use only the characters of its font: the rpg font when the English
    string has lowercase letters, else what the menu font and the rpg font
    both have (the capitals of the credits are drawn with rpg);
  * for the dialogues (ids below 800 with lowercase letters), fit the
    dialogue box once word-wrapped as the game does
    (BitmapFont::printWordWrapped, dialogue.cpp): 4 lines of 515 pixels,
    or as many as the English string (606 takes 5 in English: its last
    line runs a few pixels below the box).

The text of the mission sheets (ids 870 to 939, ten per sheet: the title,
then the objectives and the story; src/game/src/briefing.cpp) is typed in
the faces cut from the sheets, mission (the titles) and brief: each
string, English included, uses only their characters. Where it lands on
the sheet is checked by the content tests (tests/content/briefings_test.cpp).

Usage: check_strings.py <assets/source>
"""

import json
import os
import struct
import sys

DIALOGUE_WIDTH = 515  # the bottom box: x 20 to 535 (the top one is 520)
DIALOGUE_LINES = 4    # 18-pixel lines from 15 pixels into a 100-pixel box
BRIEFINGS = range(870, 940)  # the mission sheets, a title every ten ids


def load_font(root, name):
    """Returns ({character: width}, space width) of font `name`."""
    folder = os.path.join(root, "fonts", name)
    with open(os.path.join(folder, "font.json"), encoding="utf-8") as f:
        manifest = json.load(f)
    widths = {}
    for glyph in manifest["glyphs"]:
        with open(os.path.join(folder, glyph["file"]), "rb") as f:
            header = f.read(24)
        widths[glyph["char"]] = struct.unpack(">I", header[16:20])[0]  # PNG IHDR width
    return widths, manifest["spaceWidth"]


def load_table(path):
    with open(path, encoding="utf-8") as f:
        return {s["id"]: s["text"] for s in json.load(f)["strings"]}


def wrapped_lines(text, widths, space):
    """The number of lines printWordWrapped() draws text on."""
    lines = 1
    x = 0
    for word in text.split():
        w = sum(widths.get(c, 0) for c in word)
        if x + w > DIALOGUE_WIDTH and x > 0:
            lines += 1
            x = 0
        x += w + space
    return lines


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    root = sys.argv[1]
    rpg, rpg_space = load_font(root, "rpg")
    menu, _ = load_font(root, "menu")
    capitals = set(menu) & set(rpg)
    sheet_faces = {"title": set(load_font(root, "mission")[0]), "text": set(load_font(root, "brief")[0])}

    english = load_table(os.path.join(root, "strings", "uk.json"))
    problems = []
    folder = os.path.join(root, "strings")
    for name in sorted(os.listdir(folder)):
        if not name.endswith(".json"):
            continue
        table = load_table(os.path.join(folder, name))
        for sid, text in sorted(table.items()):
            if sid in BRIEFINGS:
                face = "title" if sid % 10 == 0 else "text"
                missing = sorted({c for c in text if c != " " and c not in sheet_faces[face]})
                if missing:
                    problems.append(f"{name} {sid}: no glyph in the sheets' {face} face for "
                                    f"{' '.join(missing)}: {text!r}")
                continue
            if name == "uk.json":
                continue
            reference = english.get(sid, text).replace("%d", "")
            lower = any(c.islower() for c in reference)
            allowed = set(rpg) if lower else capitals
            drawn = text.replace("%d", "")
            missing = sorted({c for c in drawn if c != " " and c not in allowed})
            # The English text has the same signs the fonts lack (the colon
            # of "PLEASE ENTER YOUR NAME :"); they are skipped there too.
            missing = [c for c in missing if c not in reference]
            if missing:
                problems.append(f"{name} {sid}: no glyph for {' '.join(missing)}: {text!r}")
            if lower and sid < 800:
                lines = wrapped_lines(text, rpg, rpg_space)
                limit = max(DIALOGUE_LINES, wrapped_lines(english.get(sid, ""), rpg, rpg_space))
                if lines > limit:
                    problems.append(f"{name} {sid}: {lines} lines in the dialogue box "
                                    f"(at most {limit}): {text!r}")
    for p in problems:
        print(p)
    print(f"{len(problems)} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
