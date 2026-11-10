# Authoring content

How to add or change what the game shows: an enemy or a prop, images and
sounds, a level. The game's code is in `src/game` (see
[architecture/overview.md](architecture/overview.md) and
[architecture/world.md](architecture/world.md)); the source assets are in
`assets/source` (see [formats/source-assets.md](formats/source-assets.md)).

## A new entity type

1. **Images and sounds.** Add the PNG files to the bank's folder
   (`assets/source/sprites/<bank>/`) and their lines to its
   `sprites.json`, with a `"name"` for each image the code uses (the first
   of an animation: the code adds the step). Sounds likewise in
   `assets/source/sounds/<bank>/sounds.json`. Then regenerate the names:

   ```
   build/default/tools/bbassets ids assets/source src/assets/include
   ```

   The test `assets.ids` fails while the headers are out of date.

2. **The class.** Derive from the base that fits (`src/game/include/bb/game/actor.hpp`):
   `Enemy` (hit by the players' shots, with the shared death pipeline:
   blood, weapon pickups, the kill counter), `Projectile` (a shot of an
   enemy), `MovingPlatform` (a prop players stand on), `Actor` (anything
   else), or `Vehicle`. Initialise every member. Use the world's random
   generator (`world().random()`) and nothing else, so that replays stay
   deterministic, and the named images (`enemyImage(img::kSomething)`).
   Put it with its world (`src/game/src/<world>/`).

3. **The registration.** Add it to its world's `register...World()`:

   ```cpp
   r.add({EntityCategory::Enemy, 402, "lemmings.digger", Placement::Enemies, makeLemming});
   ```

   The number is the one level files use (unique within the category); the
   name is `<world>.<entity>`, unique across categories
   (`EntityRegistry::add()` refuses duplicates).

4. **A level that places it**, below.

5. **A replay** that meets it, below; and a line in `docs/progress.md` (what
   was added) and `docs/legacy_map.md` if it replaces 2002 code.

## A level

Levels are `assets/source/levels/<name>.json` (the format is in
[formats/source-assets.md](formats/source-assets.md#levels)); the campaign,
`assets/source/campaign/bb.json`, lists the levels, cutscenes and
briefings in order. A briefing with text (a mission sheet) is typed in the
languages other than English: its strings, its place on the picture and
the sheet without its text are listed in `src/game/src/briefing.cpp` and
made by `tools/art/briefings.py`. An enemy is placed by an event:

```json
{"type": "enemy", "activationX": 1102, "enemy": 400, "x": 1052, "y": 428, "facing": 1}
```

It wakes when the camera reaches `activationX`; the events must stay in
descending `activationX` order. The other event types (generators, props,
scroll locks, flags, banners, music, weather, dialogues, vehicles) are
listed in the same document. The 2002 MFC editors in `vc-projects/` still
edit the 2002 files: `bbassets legacy` writes them from `assets/source`,
`bbassets extract` reads them back.

## Checking

* `ctest --preset default` builds and runs everything: unit tests, the
  asset checks and the thirteen replays.
* `ctest --preset default -L assets` checks the content alone: the test
  `content` reads every level of the campaign from the data directory and
  from the runtime pack, and fails when an event places an entity no world
  registers, or names an image, a sound or a music its level's banks do not
  have, or when the events are not in descending `activationX` order;
  `assets.reproducible` builds the pack again and compares the bytes.
* A change that is meant to change the game changes the fingerprints of
  the replays that see it: record them again
  (`cmake --build --preset default --target update_replay_hashes`) and say
  why in `docs/progress.md`. A change that is not meant to change the game
  must keep every fingerprint.
* To script a replay for new content, `tools/replays/make_replays.py` has
  helpers (walking, jumping, firing, the cheat keys); `--trace-ticks`
  prints the world tick by tick to find the frames where things happen
  (see [replays.md](replays.md)).

## Cut content

Content of 2002 that no level used is described in
[cut_content.md](cut_content.md), with what bringing it back would take.
