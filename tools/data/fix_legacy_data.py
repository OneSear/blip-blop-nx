#!/usr/bin/env python3
"""Applies the Phase 0 data fixes to the legacy data archive, in place.

migration.md, Phase 0 "Data fixes" and Appendix D.C:
  D-10  9 music banks (.mbk) use '\\' path separators; fopen() fails on
        Linux and macOS and the campaign stops after world 1.
  D-46  snorkniv.lvl and snorkniv2.lvl reference "Snork.mbk" while the file
        is "snork.mbk"; case-sensitive file systems fail.
  D-47  data/theme.mp3 and data/snuf.mp3 are stale duplicates of the .zik
        files (nothing loads them).
  D-64  tambour.zik and gameover.zik are MP3 files starting with 417 zero
        bytes (an emptied tag); SDL_mixer does not recognise them, so the
        pre-level drum roll and the game-over music never play.

And the Phase 2 data change (migration.md Phase 2, "the two hard-coded
screens move to strings"):
  uk.dat gains the level summary labels (ids 840-849) and the lines of the
        credits (ids 1000-1098), which game.cpp printed from literals.

And the Phase 3 data change (no English literals in code that players see):
  uk.dat gains the loading, briefing, name entry and HUD texts (ids
        850-857), inserted before id 1000 to keep the ids in order.

And the Phase 5 content change (the Big Smurf, enemy 8, finished as the
boss of snuf2's ending, migration.md Phase 5a):
  snufenn.gfx gains his 34 images (356 -> 390), appended from
        big_smurf_images.bin (written by tools/art/big_smurf.py, then
        bbassets legacy; the 356 images before them are unchanged);
  snuf2.lvl gains his event (enemy 8 at x 5180, y 432, facing left,
        activation 4430), a copy of the strongman record at 4419 placed
        just before it to keep the descending activation order.

And the Phase 9 change (the new options and the French text, migration.md
Phase 9):
  uk.dat gains the labels of the scaling, filter, smooth motion and
        language options, in ids that were empty (802, 804, 811-815, 817,
        818);
  fr.dat is added, written from assets/source/strings/fr.json as bbassets
        writes it (the 2002 fr.dat was never in the repository, D-25); it
        is written again whenever fr.json changes.

And the four-player mode (players 3 and 4, Bleach and Blitz, 29 October
2026):
  bleach.gfx, blitz.gfx, scorep.lft and scoreg.lft are added, recoloured
        from blip.gfx, blop.gfx, scorei.lft and scoreo.lft by
        tools/art/new_heroes.py, and written again whenever it changes
        them;
  uk.dat gains the start and controls items of players 3 and 4 (ids
        858-861), before id 1000.

And the Switch port (switch-port.md, 30 October 2026):
  uk.dat gains the CRISP value of the filter option (id 862), before id
        1000, and the briefings ask for a button (id 852, "Press a button
        to start.").

The script is idempotent: running it on a fixed archive changes nothing.
Every other entry keeps its content, compression method and timestamp.

Usage: fix_legacy_data.py <data.zip>
"""

import os
import struct
import sys
import zipfile

BIG_SMURF_IMAGES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "big_smurf_images.bin")
SNUFENN_IMAGES_2002 = 356

MBK_WITH_BACKSLASHES = [
    "bisou", "credits", "dork", "end", "finalboss", "lem", "snork", "video", "videoboss",
]
LEVELS_WITH_SNORK_CASE = ["snorkniv", "snorkniv2"]
STALE_FILES = ["data/theme.mp3", "data/snuf.mp3"]
PADDED_MP3 = ["data/tambour.zik", "data/gameover.zik"]

# Text that game.cpp printed from literals (Phase 2), by string id.
NEW_STRINGS = [
    (840, 'BONUS STAGE FAILED!'),
    (841, 'LEVEL COMPLETE!'),
    (842, 'LIFE UP'),
    (843, 'PERFECT'),
    (844, 'Objective'),
    (845, 'Casualties'),
    (846, '%'),
    (847, 'Time left'),
    (848, 'BONUS'),
    (849, 'SCORE'),
    (1000, 'Credits'),
    (1001, 'CODE'),
    (1002, 'Benjamin Karaban'),
    (1003, 'Sylvain Bugat'),
    (1004, 'MUSIC AND SOUND'),
    (1005, 'DESIGN'),
    (1006, 'Gerard Kelly'),
    (1007, 'ADDITIONAL MUSIC'),
    (1008, 'El Mobo'),
    (1009, 'Rik Ede'),
    (1010, 'ARTWORK'),
    (1011, 'Laurent Schneider'),
    (1012, 'Jérémie Comarmond'),
    (1013, 'Jérôme Karaban'),
    (1014, 'Didier Colin'),
    (1015, 'Sylvain Bugat'),
    (1016, 'ADDITIONAL SOUND'),
    (1017, 'AND VOCAL EFFECTS'),
    (1018, 'Eric Khodja'),
    (1019, 'Jérôme Karaban'),
    (1020, 'Benjamin Karaban'),
    (1021, 'Vincent Lagarrigue'),
    (1022, 'STORYLINE'),
    (1023, 'Benjamin Karaban'),
    (1024, 'Eric Khodja'),
    (1025, 'ORIGINAL CONCEPT'),
    (1026, 'Laurent Schneider'),
    (1027, 'Jérémie Comarmond'),
    (1028, 'Didier Colin'),
    (1029, 'Sylvain Bugat'),
    (1030, 'Benjamin Karaban'),
    (1031, 'BETA TEST'),
    (1032, 'Jérémie Khodja'),
    (1033, 'Julien Areas'),
    (1034, 'Carlos Sédille'),
    (1035, 'Juliette Karaban'),
    (1036, 'THANKS TO'),
    (1037, 'Arnaud Dubois'),
    (1038, 'Carole Schmitt'),
    (1039, 'Aasterion staff'),
    (1040, 'Aurore Anger'),
    (1041, 'Cécile Baudoncourt'),
    (1042, 'Nicolas Clément'),
    (1043, 'Nathalie Morin'),
    (1044, 'Florent and Thomas'),
    (1045, 'Bruno Singer'),
    (1046, 'Francois Deschamps'),
    (1047, 'Colin Auger'),
    (1048, 'TP122 students'),
    (1049, 'Jeff'),
    (1050, 'Flavy'),
    (1051, 'SPECIAL THANKS TO'),
    (1052, 'La Guinguette Pirate'),
    (1053, 'TiPunch power!'),
    (1054, 'La Pirogue'),
    (1055, 'I love you Séverine!'),
    (1056, 'Le Montbauron'),
    (1057, 'mind the 8 ball!'),
    (1058, "La Fleche d'Or"),
    (1059, 'avoid the toilets!'),
    (1060, 'Hippochine Restaurant'),
    (1061, 'great music!'),
    (1062, 'Vitigno Pizzeria'),
    (1063, 'pizza! good!'),
    (1064, 'Max Linder Theatre'),
    (1065, 'THX rules!'),
    (1066, 'UNEF'),
    (1067, 'cheap beer 4 all!'),
    (1068, 'Hayao Myazaki'),
    (1069, 'Totoro! To-to-ro!'),
    (1070, 'Kevin Smith'),
    (1071, 'thirty seven?'),
    (1072, 'Peter Jackson'),
    (1073, 'Bad Taste rules!'),
    (1074, 'Monty Python'),
    (1075, 'behind the rabbit?'),
    (1076, 'John Boorman'),
    (1077, 'Zardoz has spoken!'),
    (1078, 'Luc Besson'),
    (1079, 'nah... just kidding!'),
    (1080, 'Christopher Lambert'),
    (1081, 'watch your back!'),
    (1082, 'Jeremy Irons'),
    (1083, 'lord Profion!'),
    (1084, 'Wayne Scott'),
    (1085, 'Rambo! Rambo!'),
    (1086, 'Les Joyeux Urbains'),
    (1087, 'va chez ta mère!'),
    (1088, 'Toss'),
    (1089, 'da! da! da!'),
    (1090, 'Le cassoulet'),
    (1091, 'best served at 4 AM'),
    (1092, 'Gamedev.net'),
    (1093, 'DirectX vs hamsters'),
    (1094, 'THANK'),
    (1095, 'YOU'),
    (1096, 'FOR PLAYING'),
    (1097, 'LOADED Studio'),
    (1098, 'May 2002'),
]

# Text that the screens and the HUD printed from literals (Phase 3).
PHASE3_STRINGS = [
    (850, 'PLEASE WAIT'),
    (851, 'LOADING'),
    (852, 'Press a key to start.'),
    (853, 'Loading...'),
    (854, 'PLAYER %d GOT A HIGH SCORE!'),
    (855, 'PLEASE ENTER YOUR NAME :'),
    (856, 'Bonus'),
    (857, 'Stage'),
]

# The labels of the Phase 9 options, in ids that were empty.
PHASE9_STRINGS = [
    (802, 'SMOOTH'),
    (804, 'SMOOTH MOTION = '),
    (811, 'ENGLISH'),
    (812, 'FRANCAIS'),
    (813, 'SCALING = '),
    (814, 'FIT'),
    (815, 'INTEGER'),
    (817, 'FILTER = '),
    (818, 'SHARP'),
]
# The menu items of the four-player mode.
FOUR_PLAYER_STRINGS = [
    (858, 'START 3 PLAYERS GAME'),
    (859, 'START 4 PLAYERS GAME'),
    (860, 'PLAYER 3 CONTROL'),
    (861, 'PLAYER 4 CONTROL'),
]
# The Switch port: the third filter value, and a button rather than a key.
SWITCH_STRINGS = [
    (862, 'CRISP'),
]
SWITCH_WORDING = (b"\n852^Press a key to start.", b"\n852^Press a button to start.")
FRENCH_SOURCE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "assets", "source",
                             "strings", "fr.json")


def french_dat():
    """fr.dat as bbassets writes fr.json (writeDat): id^text lines in id
    order, Windows-1252, no final newline; None without fr.json."""
    import json
    if not os.path.exists(FRENCH_SOURCE):
        return None
    with open(FRENCH_SOURCE, encoding="utf-8") as f:
        strings = sorted((s["id"], s["text"]) for s in json.load(f)["strings"])
    return "\n".join("%d^%s" % (i, text) for i, text in strings).encode("cp1252")


def add_big_smurf_images(data):
    count = struct.unpack_from("<i", data, 0)[0]
    if count != SNUFENN_IMAGES_2002:
        return data, None
    extra = open(BIG_SMURF_IMAGES, "rb").read()
    added = 0
    pos = 0
    while pos < len(extra):          # count the appended records
        size = struct.unpack_from("<i", extra, pos + 8)[0]
        pos += 12 + size
        added += 1
    assert pos == len(extra), "big_smurf_images.bin is not a list of gfx records"
    out = struct.pack("<i", count + added) + data[4:] + extra
    return out, f"{added} Big Smurf images appended"


def add_big_smurf_event(data):
    # The event table is at the end: eventCount, then 60-byte records.
    for count_at in range(len(data) - 4, 0, -4):
        count = struct.unpack_from("<i", data, count_at)[0]
        if 0 < count < 10000 and count_at + 4 + count * 60 == len(data):
            break
    else:
        raise ValueError("snuf2.lvl: no event table")
    records = [data[count_at + 4 + i * 60:count_at + 4 + (i + 1) * 60] for i in range(count)]
    fields = [struct.unpack_from("<9i", r) for r in records]
    if any(f[0] == 0 and f[2] == 8 for f in fields):
        return data, None
    template = next(i for i, f in enumerate(fields) if f[:4] == (0, 4419, 5, 5169))
    record = bytearray(records[template])
    struct.pack_into("<4i", record, 0, 0, 4430, 8, 5180)   # enemy 8, activation, x
    records.insert(template, bytes(record))
    out = data[:count_at] + struct.pack("<i", count + 1) + b"".join(records)
    return out, "Big Smurf event added"


def fix_entry(name, data):
    """Returns (new_data, description) or (data, None) when unchanged."""
    base = name[len("data/"):].rsplit(".", 1)[0] if name.startswith("data/") else None
    if name.endswith(".mbk") and base in MBK_WITH_BACKSLASHES and b"\\" in data:
        return data.replace(b"\\", b"/"), "path separators"
    if name.endswith(".lvl") and base in LEVELS_WITH_SNORK_CASE:
        # The music bank is the 5th 20-byte name field of the header.
        field = data[80:100]
        if field.startswith(b"Snork.mbk\0"):
            return data[:80] + b"s" + data[81:], "music bank name case"
    if name == "data/uk.dat" and b"\n840^" not in data:
        # Ids are appended in ascending order after the last one (834).
        added = "".join("\n%d^%s" % (i, text) for i, text in sorted(NEW_STRINGS + PHASE3_STRINGS))
        return data + added.encode("cp1252"), "added %d strings" % (len(NEW_STRINGS) + len(PHASE3_STRINGS))
    if name == "data/uk.dat" and b"\n850^" not in data:
        # Phase 2 was applied: the new ids go before id 1000.
        added = "".join("\n%d^%s" % (i, text) for i, text in PHASE3_STRINGS)
        at = data.index(b"\n1000^")
        return data[:at] + added.encode("cp1252") + data[at:], "added %d strings" % len(PHASE3_STRINGS)
    if name == "data/uk.dat" and b"\n858^" not in data and b"\n857^" in data:
        added = "".join("\n%d^%s" % (i, text) for i, text in FOUR_PLAYER_STRINGS)
        at = data.index(b"\n1000^")
        data = data[:at] + added.encode("cp1252") + data[at:]
        more, _ = fix_entry(name, data)
        return more, "added %d four-player strings" % len(FOUR_PLAYER_STRINGS)
    if name == "data/uk.dat" and b"\n862^" not in data and b"\n861^" in data:
        added = "".join("\n%d^%s" % (i, text) for i, text in SWITCH_STRINGS)
        at = data.index(b"\n1000^")
        data = data[:at] + added.encode("cp1252") + data[at:]
        more, _ = fix_entry(name, data)
        return more, "added %d Switch strings" % len(SWITCH_STRINGS)
    if name == "data/uk.dat" and SWITCH_WORDING[0] in data:
        return data.replace(SWITCH_WORDING[0], SWITCH_WORDING[1]), "the Switch's wording of id 852"
    if name == "data/uk.dat" and b"\n811^\n" in data:
        for i, text in PHASE9_STRINGS:
            empty = b"\n%d^\n" % i
            assert data.count(empty) == 1, i
            data = data.replace(empty, b"\n%d^%s\n" % (i, text.encode("cp1252")))
        return data, "filled %d option labels" % len(PHASE9_STRINGS)
    if name in PADDED_MP3 and data[:1] == b"\0":
        start = next(i for i, c in enumerate(data) if c != 0)
        if data[start:start + 2] in (b"\xff\xfb", b"\xff\xfa", b"\xff\xf3", b"\xff\xf2"):
            return data[start:], f"removed {start} leading zero bytes"
    if name == "data/snufenn.gfx":
        return add_big_smurf_images(data)
    if name == "data/snuf2.lvl":
        return add_big_smurf_event(data)
    return data, None


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    path = sys.argv[1]
    with zipfile.ZipFile(path) as src:
        entries = [(info, src.read(info)) for info in src.infolist()]

    changes = []
    out_entries = []
    for info, data in entries:
        if info.filename in STALE_FILES:
            changes.append(f"{info.filename}: deleted")
            continue
        new_data, what = fix_entry(info.filename, data)
        if what:
            changes.append(f"{info.filename}: {what}")
        out_entries.append((info, new_data, what is not None))

    # The art of players 3 and 4, from the (fixed) 2002 files.
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "art"))
    import new_heroes  # noqa: E402
    names = [info.filename for info, _, _ in out_entries]
    by_name = {info.filename: data for info, data, _ in out_entries}
    template = out_entries[names.index("data/blip.gfx")][0]
    for new_name, data in sorted(new_heroes.new_files(lambda n: by_name[n]).items()):
        if new_name not in names:
            info = zipfile.ZipInfo(new_name, date_time=(2026, 10, 29, 0, 0, 0))
            info.external_attr = template.external_attr
            out_entries.append((info, data, True))
            changes.append(f"{new_name}: added")
        elif by_name[new_name] != data:
            k = names.index(new_name)
            out_entries[k] = (out_entries[k][0], data, True)
            changes.append(f"{new_name}: written again by new_heroes.py")

    french = french_dat()
    if french is not None:
        names = [info.filename for info, _, _ in out_entries]
        if "data/fr.dat" not in names:
            info = zipfile.ZipInfo("data/fr.dat", date_time=(2026, 10, 29, 0, 0, 0))
            info.external_attr = out_entries[names.index("data/uk.dat")][0].external_attr
            out_entries.insert(names.index("data/uk.dat") + 1, (info, french, True))
            changes.append("data/fr.dat: added")
        else:
            k = names.index("data/fr.dat")
            info, data, _ = out_entries[k]
            if data != french:
                out_entries[k] = (info, french, True)
                changes.append("data/fr.dat: written again from fr.json")

    if not changes:
        print("nothing to fix")
        return 0

    tmp = path + ".tmp"
    with zipfile.ZipFile(path) as src, zipfile.ZipFile(tmp, "w") as dst:
        for info, data, changed in out_entries:
            if changed:
                new_info = zipfile.ZipInfo(info.filename, date_time=info.date_time)
                new_info.compress_type = zipfile.ZIP_DEFLATED
                new_info.external_attr = info.external_attr
                dst.writestr(new_info, data)
            else:
                # Same content, compression method and timestamp.
                with src.open(info) as f:
                    dst.writestr(info, f.read())
    os.replace(tmp, path)
    for c in changes:
        print(c)
    return 0


if __name__ == "__main__":
    sys.exit(main())
