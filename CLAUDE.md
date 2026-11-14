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
# Local (Mac/Linux), needs SDL2 and CMake 3.21+. Presets: default (Debug), release (RelWithDebInfo), asan (ASan + UBSan)
cmake --preset default && cmake --build --preset default
ctest --preset default -j 4 -E '^replay'     # everything but the replays: a few minutes
ctest --preset release -j 4 -L replay        # the 45 replay tests
# The presets set no -j: give ctest one job per CPU thread. Replays run about 7 times slower in Debug than in
# Release, with the same fingerprints, so play them in Release (a 6,000-frame replay takes about 5 s there).

# Subsets and single tests
ctest --preset default -R '^unit$'           # one ctest entry (names: unit, conformance, formats, content, editor,
ctest --preset default -R replay.smurf       #   editor.ui, naming.french, strings.fonts, replay.<name>[.pack|.wide],
ctest --preset default -L replay             #   replay.smurf.smooth, assets.verify, assets.reproducible, assets.ids)
ctest --preset default -L assets             # labels: replay, pack, wide, assets
build/default/tests/bb_unit_tests -tc='hero choice*'   # doctest cases by name (-sf=<file pattern> filters by source file)

# Linux check from a Mac (GCC, case-sensitive FS)
tools/docker/linux-check.sh

# Switch (devkitPro: switch-dev, switch-sdl2; $DEVKITPRO set, usually /opt/devkitpro)
cmake --preset switch && cmake --build --preset switch      # build/switch/blipblop.nro
tools/switch/make-nsp.sh                                     # .nsp (needs hacBrewPack and the maintainer's keys)
tools/switch/make-nsp.sh --upload v1.2.0     # at the tag, with a clean tree (untracked files count): attaches the .nsp
# hacBrewPack is not part of devkitPro: build a mirror (cp config.mk.template config.mk; make) and pass HACBREWPACK=<path>.
# A release: push a tag v*; release.yml builds the .nro with the tag's version, attaches it and publishes the
# release; then make-nsp.sh --upload <tag> from a checkout at the tag (docs/release-checklist.md).

# Run a local build (it reads ./data, which the build extracts from vc-projects/Blip_n_Blop_3/data.zip)
cd build/default && ./blipblop /cheat        # /quiet: no music; --pack blipblop.bbpack; --level data/snuf1.lvl 1: that
                                             # level alone (kind as in bb.lst); --all-missions; --help for the rest
# /cheat keys: F1 ammo, F2 cow bomb, F3 extra life, F9 win the level, F12 FPS counter. Every build (the Switch
# too) also has the pause menu's CHEATS page (World::cheats): switches for all players, WIN LEVEL, no hi-score
```

Formatting and linting use `.editorconfig` (UTF-8, LF), `.clang-format` and `.clang-tidy`. The build exports `compile_commands.json` for clang-tidy, and there is no lint target. Python 3 is needed for the `naming.french` and `strings.fonts` tests, which are skipped without it.

## Architecture

The layers are in `src/` and dependencies only point down (`docs/architecture/overview.md`):

- `src/core` (`bb::core`, **C++17 standard library only**): pixmaps, sprites, fonts, asset readers and the runtime pack, `Rng`, `FixedClock`, hashing, `Vfs`, the settings and progress files, `audio::Mixer`.
- `src/platform` (`bb::platform`): interfaces only (`Platform`, `Display`, `InputSource`, `Audio`, `Clock`, `FileSystem`, `System`). `src/platform/sdl2` is the only SDL user, and `sdl2/switch/` holds the few libnx calls. The backend owns `main()` and calls `bbMain()`.
- `src/assets` (`bb::assets`): **generated** headers of the image and sound names. Regenerate them with `build/default/tools/bbassets ids assets/source src/assets/include`; the `assets.ids` test fails when they are stale.
- `src/game` (`bb::game`): the world, every entity (one folder per game world: `smurf/`, `snorks/`, `pokemon/`, `carebears/`, `lemmings/`, `video/`, `videoboss/`, `finalboss/`, `player/`), the screens and the campaign.
- `src/app`: the executable. Command line and record/replay in `session.cpp`; `App` owns everything and hands it to the screens through `bb::game::Services`.
- `tools/`: `bbassets` (2002 files ↔ `assets/source` ↔ `.bbpack`), `bbedit` (the level editor, Dear ImGui on SDL, allowed to use SDL directly), the replay generator, and Python art and data scripts.

The pieces that take several files to understand:
- **Main loop.** `App::run()` drives a `ScreenStack`. Each step asks the top screen for a `Frame` (`Present`, `Wait` or `Continue`), and nothing blocks inside a screen. `CampaignScreen` plays `bb.lst`: cutscenes, briefings and `LevelScreen`. In a level, `Game::beginFrame()` asks the `FixedClock` how many 11.8 ms ticks this frame needs (about 84.7 Hz logic on a 60 Hz display). Each tick runs `Game::tickBegin()` (input, players joining, then `Level::tick()`) and then `Game::tickEnd()` (the world fingerprint), and `Game::drawAll()` draws the frame.
- **World and entities.** One `World` is reused from level to level. `EntityList<T>` keeps the 2002 iteration order (newest first; an entity added during an iteration is visited on the next one; `dead` entities are removed at the end of the tick), and replays depend on that order. Entities interact through data-less role interfaces (`Shot`, `ShotTarget`, `Hurtable`, `Rider`, `Collectable`, reached through `asShot()` and similar). Cross-references are `Handle<T>`, never raw pointers. Entities are built from `Hitbox`, `AnimationPlayer` and `Health` (`components.hpp`) on top of `Actor`, `Character`, `Enemy`, `Projectile` or `Vehicle` (`actor.hpp`).
- **Content.** `EntityRegistry` maps a legacy id per category, plus a `<world>.<entity>` name, to a factory. Each world registers its entities in `register<World>World()`. Level events (`LevelEvent`) wake when the camera reaches their `activationX`; the events of a level file stay in descending `activationX` order.
- **Level flow.** A scroll lock holds the camera at its x until there are no enemies, no generators, or a flag equals or reaches a value (`Level::updateLock()`). The last lock of most levels never opens: the level is won when the camera is at its victory x and two flag conditions hold (`Level`'s victory check, then 200 ticks). Flags 0 to 3 are per-level script registers; the game keeps 6 (timer), 7 (generators off), 8 (kills), 9 (enemies alive) and 10 (generators alive). Bonus levels (kind 0) also end when their timer runs out. An enemy leaves only when it dies or is 250 pixels left or 260 pixels right of the view (`Actor::dieWhenOffscreen()`), and the walkers patrol between `minX` (the camera at their birth) and the view's right edge. So an enemy that stays alive out of the players' reach keeps a no-enemies lock or victory shut for good (D-98, the static Yoshi turret).
- **Campaign, missions and players.** `CampaignScreen` plays `bb.lst` from a part: 0, or the briefing of the mission chosen in the main menu (`MainMenu::firstPart()`). The missions are the briefings that are mission sheets (`findMissions()`), and `core::Progress` counts those reached. The hero choice gives a `Lineup`: a hero and an input player for each player. Input player p reads the aliases from `AliasControls::firstOf(p)` (p × 10), plus pad p's default layout while none of its actions is bound to a pad (`InputState::aliasHeld()`). `Game::updateJoins()` lets an unused input player join a running level. Rumble is output only: `Player::rumble()` goes through `PlayerControls`, `RumbleOutput` (`DeviceInput`) and `InputSource::rumble()`.
- **Options and saved files.** `core::Settings` is the `settings.cfg` text format, and `game::Options` is what the menus change. `src/app/settings_file.cpp` copies each into the other, and `App::present()` applies the display and audio changes. A new option touches all three, plus `Menu::OptionItem` and its strings (`menus.cpp`), the round-trip test (`tests/unit/input_test.cpp`) and the `menus` replay script. The preferences directory (`sdmc:/switch/blipblop/` on the Switch) holds `settings.cfg`, `hiscores.scr` and `progress.cfg`.
- **Drawing.** Everything draws into one `core::gfx::Pixmap` back buffer, through `bb::game::Canvas` and `core::gfx::draw()`, which keeps the 2002 clipping. The buffer is 640x480. With WIDESCREEN it is 854x480 for a level, a mission sheet or the hero choice (`setScreenWidth()`), and the 640-pixel screens are shown centred in a 16:9 frame. `Display` only presents finished frames. The OpenGL display (`gl_display.cpp`) scales them with the filter's shader (SHARP, CRISP, SMOOTH, XBR, CRT). Without OpenGL, or with `BB_DISPLAY=renderer`, SDL's renderer does it, and XBR and CRT look like CRISP.
- **Assets.** The game reads them through `core::assets::AssetSource`, from either the 2002 `data/` directory or the runtime pack `blipblop.bbpack`, which is built from `assets/source/` (PNG, JSON, WAV, Ogg; see `docs/formats/source-assets.md`). On the Switch the pack is at `romfs:/`.

## Determinism: the main invariant

The regression replays guard every change. Each `tests/replays/*.bbreplay` plays headless with virtual time (1/60 s per frame), seed 1 and the reference settings (`Settings::reference()`: smooth motion and widescreen off). A deterministic run never reads or writes the saved settings, hi-scores or progress, never rumbles, and its main menu offers only the first mission (`--all-missions`, replay `option all-missions`, offers them all). Its fingerprints are compared with `tests/replays/expected/<name>.bbhash`: `frame` lines hash the presented pixels and `tick` lines hash the world state (camera, flags, RNG, and every entity's position, animation, hitbox, state and health).

Each replay also runs as:
- `.pack`: from the runtime pack, which must give identical hashes.
- `.wide`: with `--widescreen`, against `<name>.wide.bbhash`, for every replay but `attract`. WIDESCREEN changes the simulation: the 854-pixel view wakes events earlier (`wakePoints()`) and stops at `Camera::limit`. Where a 4:3 script no longer wins its levels in 16:9, the generator writes one measured in 16:9, `tests/replays/wide/<name>.bbreplay`, which the `.wide` test plays instead.

A replay's `# expect` lines (`Script.expect_win()`) list milestones the run must print in order, a level won without the cheat; the test fails before comparing fingerprints when one is missing. Every level is won so in both views: keep it so when re-measuring a script.

`replay.smurf.smooth` checks that smooth motion changes no tick.

Rules for game code:
- Use only the world's generator (`world().random()`), and keep every draw and its order, constructors included. Never pass two random draws as arguments of one call: argument evaluation order differs between GCC and Clang, and that once broke the Linux replays. Draw into locals in source order.
- Initialise every member, because uninitialised members have changed what gets drawn. Keep the 2002 state numbers and animation counters, which the fingerprint records.
- Outputs such as sounds and rumble must never feed back into the world.
- A change meant to alter behaviour or pictures: re-record the expected hashes and **commit the new `.bbhash` files in a separate commit** whose message says why they changed. Say whether the tick hashes held: a UI-only change should change `frame` lines only. The `update_replay_hashes` target plays the replays one after another and copies each output over its expected file. It's faster to run `ctest --preset release -j 4 -L replay`, then copy each `build/release/tests/replays/out/*.bbhash` except `*.pack` and `*.smooth` into `tests/replays/expected/`; each `.pack` output must be identical to its plain run's.
- A fix of a 2002 defect goes in its own change, which re-records the hashes and adds the defect to `migration.md` Appendix D.
- Replays are generated: edit `tools/replays/make_replays.py` and run `tools/replays/make_replays.py tests/replays`. Do not hand-edit the `.bbreplay` files. The `dork` and `smurf` scripts push keys at measured frames, so a behaviour change on those levels can mean re-measuring them (`docs/replays.md`). The `menus`, `fourplayers` and `missions` scripts move through the menus by counting taps, so a new menu item means changing them. A headless replay has no pad connected: a pad's B (`Input::backHeld()`) and its A acting as Return never trigger, though pad inputs bound to a player's actions (`JOY3_B1`) do. To check a script, play it and read its milestones: `./blipblop --headless --replay <f>` prints `[frame N, tick T] ...` lines. To measure one, import `make_replays` from a scratch script, write a variant that plays on past the expected end (`explore_levels()`, `wide_explore()`), and read where its milestones land.
- Debugging a divergence: a failing replay test dumps frames to `build/<preset>/tests/replays/out/<replay>-frames/`. Use `./blipblop --headless --replay <f> --trace-ticks <out> <from> <to>` (ticks) for the world state and `--dump-frames <dir> <from> <to>` to compare builds. Each traced tick gives the camera (`offset=`), `lock=`, the RNG and the flags, then every entity by list with its class, position, health (`pv=`) and 2002 state (`etat=`): the way to see why a level is stuck.

## Conventions

- Style: Google-based clang-format with a 4-space indent and a 110-column limit. Types are `CamelCase`; functions, methods, variables and members are `camelBack`; private and protected members end in `_`; constexpr names are `kName`. Every target uses `bb_set_strict_warnings` (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast`) with **`-Werror` by default**, and the Switch build uses GCC, so code must build cleanly under both GCC and Clang.
- No SDL header outside `src/platform/sdl2`, except in the editor. No platform code in `core` or `game`.
- **No French identifiers.** The `naming.french` test (`tools/check_identifiers.py`) checks this against the glossary in `migration.md` Appendix B. Comments may name the 2002 files that code was ported from.
- Game text lives in `assets/source/strings/uk.json` and `fr.json`, which the runtime pack is built from; the plain replays read the 2002 `data.zip` instead. New text needs:
  - a free id in both JSON files;
  - a block in `tools/data/fix_legacy_data.py` that inserts the English into `uk.dat` in id order. Run the script on `vc-projects/Blip_n_Blop_3/data.zip`: it also rewrites `fr.dat` from `fr.json`, and a second run must change nothing;
  - the new `uk.dat` string count in `tests/formats/legacy_formats_test.cpp`.

  The menu and banner fonts have only capitals, digits and a few signs; the dialogue font (`rpg`) adds lowercase and à è é ê ô. `strings.fonts` checks that every character exists in its font and that dialogues fit their box.
- Files are `snake_case.cpp`/`.hpp`. Tests use doctest; a new `tests/<suite>/<subject>_test.cpp` must be added to its executable's source list in `tests/CMakeLists.txt`.
- With a code change, update `docs/progress.md` (what changed and what was learnt) and, when 2002 code moves, `docs/legacy_map.md`.
- Commit messages follow the pattern `Area: what changed` (for example `Hero choice: each player joins and takes a hero`), with a body in prose that explains why. A pull request reports the local test results and whether the tick hashes held, and shows pictures of visual changes (`--dump-frames`; `--display-shots <dir> <frame>` for the filters).
- Never read, print or commit Switch keys (`tools/switch/prod.keys`, `~/.switch/prod.keys`, `BB_SWITCH_KEYS`). They are gitignored, and CI never has them. Never change the title ID `0x011142be60644000` in `cmake/BBSwitch.cmake`.
- `vc-projects/` holds the 2002 game data (`data.zip`) and the original MFC editors (tabs, and the editors are Latin-1). It is not built.
