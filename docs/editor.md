# The level editor (`bbedit`)

`bbedit` edits the game's levels and the hotspots and names of its sprite
images, in the source asset tree (`assets/source`,
[formats/source-assets.md](formats/source-assets.md)). It replaces the 2002
MFC editors (`vc-projects/BB3_level_editor`, `GFX Editor`), which only ran
on Windows and worked on the 2002 files. It runs on a Mac or Linux (SDL 2
and Dear ImGui) and is built with the tools of the local builds
(`build/<preset>/tools/bbedit`); its test play (F5) runs the local build of
the game.

```
bbedit [source-dir] [level]
```

`source-dir` defaults to `assets/source` in the current directory or one
of its parents; `level` is a level's file name without `.json` (`snuf1`).
Without a level, open one with File > Open level.

## The window

* **Tools** (left): the editing tool and its settings.
* **The level** (middle): the decor screens, the platforms (one colour per
  layer), the walls (solid red, blood yellow), the entities with their game
  images, the midground and foreground images, the events without a point
  (vertical lines with a summary), the players' starts (P1, P2) and the
  victory line. The screen numbers and decor images are under the level.
  Wheel: scroll; Ctrl+wheel (Cmd on macOS): zoom at the cursor; middle
  button: pan. View shows or hides each kind of object.
* **Inspector** (right): the selected object's fields, by name (an
  entity's type, its facing, a lock's condition, flags by name...). With
  nothing selected: the level's event counts.
* **Status line**: the last message, and the level position under the
  cursor.

## Tools

| Key | Tool | Use |
|-----|------|-----|
| 1 | Select and move | Click an object to select it, drag it to move it. Del deletes it, Ctrl+D duplicates it (32 pixels to the right). |
| 2 | Place | Choose the event type, then the entity (enemies, props, vehicles, pickups: a list with their images, filtered by the level's world) or the image (midground and foreground: the level bank's images); click to place. |
| 3 | Platforms | Left button paints the height under the cursor on the chosen layer (0 to 7; 6 and 7 are slippery) over the brush width; right button erases. |
| 4 | Walls | 8x8 cells, solid or blood: left button sets, right button clears. |
| | Player 1 start, Player 2 start, Victory | Click to set. |

Esc returns to the select tool. Ctrl+Z undoes, Ctrl+Shift+Z or Ctrl+Y
redoes (200 steps; a brush stroke or a drag is one step). Ctrl+S saves.

Placed events get the 2002 editor's defaults (a generator: capacity 10,
period 100; a lock, a hold-fire and a sound: value 1; an overkill: 200; a
pickup generator: period 500; a music: play).

## What the editor keeps right for the game

* **Activation x.** An event wakes when the screen's left edge reaches its
  activation x; the editor computes it from the event's position as the
  2002 editor did (enemies x − 750 facing left, x + 50 facing right;
  generators x − 800, x + 50 for a temporary one facing right; midground
  x − 660; props, foreground, vehicles and pickups x − 740). Every event of
  the 12 shipped levels has exactly this value (test `editor`). An event
  without a point is placed at its activation x.
* **Event order.** The game reads the events in descending activation
  order (docs/formats/lvl.md); the editor keeps them so after every edit.
* **Files.** Saving writes the level as `bbassets` does: a level opened
  and saved without a change is the same file byte for byte (test
  `editor`, on all 12 levels). New events write only their named fields,
  like the shipped ones. The 2002 fill value of fields the 2002 editor
  never set (`null` in the files) is kept and shown as "not set".

## Level properties (Level > Properties)

* **Assets**: the level's seven banks (decor, level, enemies, sounds,
  music, dialogue, portraits), chosen from the source tree.
* **Screens**: the decor image of each 640-pixel screen, left to right.
  Changing the count changes the level's width: platforms and walls past
  the new end are dropped, new screens have none.
* **Start and victory**: the players' starts; the victory x and its two
  flag conditions (the level is won when the screen's left edge is at
  victory x or further and flag 1 == value 1 and flag 2 == value 2).
* **Markers**: the named positions the level's entities read (a boss's
  arena, where the stork drops its rider...). The `content` test checks
  that a level has the markers of the entities it places.

File > New level makes a level with one screen and the current level's
banks; add it to `campaign/bb.json` to play it in the campaign.

## Test play (F5)

Level > Play (F5) saves the level and starts the game on it alone
(`blipblop --overlay <pack> --level data/<name>.lvl <kind>`): the level is
written into a small overlay pack, and every other asset comes from the
game's own pack or data. The level kind is the one `campaign/bb.json` gives
it (1, the first of a world, when it is not in the campaign). The game
quits at the end of the level.

The game's assets are those of its last build. After changing sprites (or
any other source asset), use Level > Play with rebuilt assets (Shift+F5),
which packs the whole source tree first (about 10 seconds with a release
build of `bbassets`, 40 with a debug one).

Both need `blipblop` next to `bbedit` or one directory up (the build tree's
layout), and the second also `bbassets` next to `bbedit`.

## Sprite hotspots and names (Sprites > Hotspots and names)

A bank's images with their names; the selected one zoomed, its hotspot (the
point drawn at an entity's x, y) marked with a red cross. Click the image
to move the hotspot, or type it; type a name (lower_case letters, digits
and single underscores) and press Enter. Save writes the bank's
`sprites.json`; after changing names, regenerate the C++ constants:
`bbassets ids assets/source src/assets/include` (the `assets.ids` test
fails until then). The images themselves are PNG files: edit them with any
image editor (docs/formats/source-assets.md gives the colour rules).

## Entity images

The editor draws an enemy or a prop with the game image the 2002 editor's
icon showed, listed in `assets/source/editor/entities.json`
(`{"entity": "smurf.greedy", "sprites": "snufenn.gfx", "image": 188}`).
`tools/editor/make_entity_previews.py` made it from the 2002 icons
(`level-editor/*/bbe_ennemis.ini`, `bbe_fondsanimes.ini`): each icon is a
copy of a bank image, found by comparing pixels. 64 of the 89 enemy and
prop types matched, each exactly (to the 16-bit rounding); the others are
drawn as labelled boxes, and can be added to the file by hand. Vehicles
and pickups are labelled boxes too (in 2002, pickups were a cross and a
name; turrets had icons).

## What the 2002 editors did that this one does not

* Fonts and sound banks: their sources are PNG and WAV files with a JSON
  list (docs/formats/source-assets.md), edited with ordinary tools; the
  2002 font and sound editors are not needed.
* The campaign (`campaign/bb.json`), the strings (`strings/uk.json`) and
  the scripts (`scripts/*.cin`, `*.rpg`) are text files.
* The decor screens are images of the decor bank: edit the PNG files.

The 2002 editor had no selection, move, undo, zoom or test play; it could
not change the screens, the markers or a level's width (docs/progress.md,
Phase 8, lists what it did and its defects).

## Checks

* `ctest -R editor`: the document tests (`tests/editor`) and `editor.ui`,
  which runs `bbedit --self-test`: with SDL's dummy video driver (no
  screen), it places an enemy, paints platforms, drags and deletes an enemy
  through the interface with SDL input events, checks the level after each
  gesture and undoes them (nothing is saved).
* `bbedit [source-dir] <level> --at <x> <zoom> --show <part> --screenshot
  <file.bmp>` draws the window and saves it (parts: `place`, `platforms`,
  `level`, `sprites`, `select`).
