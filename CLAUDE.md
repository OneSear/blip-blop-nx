# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Blip & Blop (LOADED Studio, 2002) migrated to C++17 / SDL 2 / CMake and released only as **Nintendo Switch homebrew** (`.nro`, `.nsp`). Mac and Linux builds exist only to develop, run the tests and use the tools. CI (`.github/workflows/ci.yml`) builds the `.nro` and **runs no tests**, so run `ctest` locally before you call a change done.

The plans and history are in long documents. Read only the sections you need:
- `migration.md`: the modernisation plan, about 160 KB. Appendix B is the French→English glossary and Appendix D lists the 2002 defects.
- `switch-port.md`: the Switch port.
- `docs/progress.md`: what was done, newest first, with the facts learnt along the way.
- `docs/architecture/`: `overview.md` is current. `world.md` was written at Phase 4, when 2002 classes still ran beside the ported ones; that 2002 code is gone now.
- `docs/replays.md`, `docs/content.md` (adding entities and levels), `docs/formats/`, `docs/editor.md`, `docs/legacy_map.md`, `docs/release-checklist.md`.

## Commands

```sh
# Local (Mac/Linux), needs SDL2. Presets: default (Debug), release (RelWithDebInfo, much faster replays), asan (ASan + UBSan)
cmake --preset default && cmake --build --preset default
ctest --preset default                       # everything: unit, conformance, formats, content, editor, naming, replays, assets

# Subsets and single tests
ctest --preset default -R '^unit$'           # one ctest entry (names: unit, conformance, formats, content, editor,
ctest --preset default -R replay.smurf       #   editor.ui, naming.french, strings.fonts, replay.<name>[.pack],
ctest --preset default -L replay             #   replay.smurf.smooth, assets.verify, assets.reproducible, assets.ids)
ctest --preset default -L assets             # labels: replay, pack, assets
build/default/tests/bb_unit_tests -tc='hero choice*'   # doctest cases by name (-sf=<file pattern> filters by source file)

# Linux check from a Mac (GCC, case-sensitive FS)
tools/docker/linux-check.sh

# Switch (devkitPro: switch-dev, switch-sdl2; $DEVKITPRO set)
cmake --preset switch && cmake --build --preset switch      # build/switch/blipblop.nro
tools/switch/make-nsp.sh                                     # .nsp (needs hacBrewPack and the maintainer's keys)

# Run a local build (it reads ./data, which the build extracts from vc-projects/Blip_n_Blop_3/data.zip)
cd build/default && ./blipblop /cheat        # /quiet: no music; --pack blipblop.bbpack; --help
```

Formatting and linting use `.clang-format` and `.clang-tidy`. The build exports `compile_commands.json` for clang-tidy, and there is no lint target. Python 3 is needed for the `naming.french` and `strings.fonts` tests, which are skipped without it.

## Architecture

The layers are in `src/` and dependencies only point down (`docs/architecture/overview.md`):

- `src/core` (`bb::core`, **C++17 standard library only**): pixmaps, sprites, fonts, asset readers and the runtime pack, `Rng`, `FixedClock`, hashing, `Vfs`, settings, `audio::Mixer`.
- `src/platform` (`bb::platform`): interfaces only (`Platform`, `Display`, `InputSource`, `Audio`, `Clock`, `FileSystem`, `System`). `src/platform/sdl2` is the only SDL user, and `sdl2/switch/` holds the few libnx calls. The backend owns `main()` and calls `bbMain()`.
- `src/assets` (`bb::assets`): **generated** headers of the image and sound names. Regenerate them with `build/default/tools/bbassets ids assets/source src/assets/include`; the `assets.ids` test fails when they are stale.
- `src/game` (`bb::game`): the world, every entity (one folder per game world: `smurf/`, `snorks/`, `pokemon/`, `carebears/`, `lemmings/`, `video/`, `videoboss/`, `finalboss/`, `player/`), the screens and the campaign.
- `src/app`: the executable. Command line and record/replay in `session.cpp`; `App` owns everything and hands it to the screens through `bb::game::Services`.
- `tools/`: `bbassets` (2002 files ↔ `assets/source` ↔ `.bbpack`), `bbedit` (the level editor, Dear ImGui on SDL, allowed to use SDL directly), the replay generator, and Python art and data scripts.

The pieces that take several files to understand:
- **Main loop.** `App::run()` drives a `ScreenStack`. Each step asks the top screen for a `Frame` (`Present`, `Wait` or `Continue`), and nothing blocks inside a screen. `CampaignScreen` plays `bb.lst`: cutscenes, briefings and `LevelScreen`. In a level, `Game::beginFrame()` asks the `FixedClock` how many 11.8 ms ticks this frame needs (about 84.7 Hz logic on a 60 Hz display). Each tick runs `Game::tickBegin()` (input, then `Level::tick()`) and then `Game::tickEnd()` (the world fingerprint), and `Game::drawAll()` draws the frame.
- **World and entities.** One `World` is reused from level to level. `EntityList<T>` keeps the 2002 iteration order (newest first; an entity added during an iteration is visited on the next one; `dead` entities are removed at the end of the tick), and replays depend on that order. Entities interact through data-less role interfaces (`Shot`, `ShotTarget`, `Hurtable`, `Rider`, `Collectable`, reached through `asShot()` and similar). Cross-references are `Handle<T>`, never raw pointers. Entities are built from `Hitbox`, `AnimationPlayer` and `Health` (`components.hpp`) on top of `Actor`, `Character`, `Enemy`, `Projectile` or `Vehicle` (`actor.hpp`).
- **Content.** `EntityRegistry` maps a legacy id per category, plus a `<world>.<entity>` name, to a factory. Each world registers its entities in `register<World>World()`. Level events (`LevelEvent`) wake when the camera reaches their `activationX`; the events of a level file stay in descending `activationX` order.
- **Drawing.** Everything draws into one 640x480 `core::gfx::Pixmap` back buffer, through `bb::game::Canvas` and `core::gfx::draw()`, which keeps the 2002 clipping. `Display` only presents finished frames, scaled with `core::gfx::viewport()` and filtered (CRISP, SHARP or SMOOTH).
- **Assets.** The game reads them through `core::assets::AssetSource`, from either the 2002 `data/` directory or the runtime pack `blipblop.bbpack`, which is built from `assets/source/` (PNG, JSON, WAV, Ogg; see `docs/formats/source-assets.md`). On the Switch the pack is at `romfs:/`.

## Determinism: the main invariant

The regression replays guard every change. Each `tests/replays/*.bbreplay` plays headless with virtual time (1/60 s per frame), seed 1 and the reference settings. Its fingerprints are compared with `tests/replays/expected/<name>.bbhash`: `frame` lines hash the presented pixels and `tick` lines hash the world state (camera, flags, RNG, and every entity's position, animation, hitbox, state and health). Each replay also runs `.pack`, from the runtime pack, which must give identical hashes.

Rules for game code:
- Use only the world's generator (`world().random()`), and keep every draw and its order, constructors included. Never pass two random draws as arguments of one call: argument evaluation order differs between GCC and Clang, and that once broke the Linux replays. Draw into locals in source order.
- Initialise every member, because uninitialised members have changed what gets drawn. Keep the 2002 state numbers and animation counters, which the fingerprint records.
- A change meant to alter behaviour or pictures: re-record with `cmake --build --preset default --target update_replay_hashes` and **commit the new `.bbhash` files in a separate commit** whose message says why they changed. Say whether the tick hashes held: a UI-only change should change `frame` lines only.
- A fix of a 2002 defect goes in its own change, which re-records the hashes and adds the defect to `migration.md` Appendix D.
- Replays are generated: edit `tools/replays/make_replays.py` and run `tools/replays/make_replays.py tests/replays`. Do not hand-edit the `.bbreplay` files. The `dork` and `smurf` scripts push keys at measured frames, so a behaviour change on those levels can mean re-measuring them (`docs/replays.md`).
- Debugging a divergence: a failing replay test dumps frames to `build/<preset>/tests/replays/out/<replay>-frames/`. Use `./blipblop --headless --replay <f> --trace-ticks <out> <from> <to>` for the world state and `--dump-frames <dir> <from> <to>` to compare builds.

## Conventions

- Style: Google-based clang-format with a 4-space indent and a 110-column limit. Types are `CamelCase`; functions, methods, variables and members are `camelBack`; private and protected members end in `_`; constexpr names are `kName`. Every target uses `bb_set_strict_warnings` (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast`) with **`-Werror` by default**, and the Switch build uses GCC, so code must build cleanly under both GCC and Clang.
- No SDL header outside `src/platform/sdl2`, except in the editor. No platform code in `core` or `game`.
- **No French identifiers.** The `naming.french` test (`tools/check_identifiers.py`) checks this against the glossary in `migration.md` Appendix B. Comments may name the 2002 files that code was ported from.
- Game text lives in `assets/source/strings/uk.json` and `fr.json`. It reaches the 2002 `data.zip` through `tools/data/fix_legacy_data.py`. `strings.fonts` checks that every character exists in the fonts and that dialogues fit their box.
- With a code change, update `docs/progress.md` (what changed and what was learnt) and, when 2002 code moves, `docs/legacy_map.md`.
- Commit messages follow the pattern `Area: what changed` (for example `Hero choice: each player joins and takes a hero`), with a body in prose that explains why.
- Never read, print or commit Switch keys (`tools/switch/prod.keys`, `~/.switch/prod.keys`, `BB_SWITCH_KEYS`). They are gitignored, and CI never has them. Never change the title ID `0x011142be60644000` in `cmake/BBSwitch.cmake`.
- `vc-projects/` holds the 2002 game data (`data.zip`) and the original MFC editors (tabs, and the editors are Latin-1). It is not built.
