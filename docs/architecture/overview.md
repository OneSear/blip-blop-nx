# Architecture

How the game is built since the end of the migration (migration.md,
Phase 5, October 2026). The world and its entities are described in
[world.md](world.md); the file formats in [../formats/](../formats/README.md);
the replays in [../replays.md](../replays.md).

## Layers

```
src/app        the executable: bbMain, App, session, device input, settings, strings, files
src/game       bb::game: the world, every entity, the screens, the campaign
src/assets     bb::assets: the names of the images and sounds (generated headers)
src/core       bb::core: pixmaps, sprites, fonts, asset readers and packs, input codes,
               settings, Rng, FixedClock, hashing, Log, Vfs
src/platform   bb::platform: the interfaces (Display, InputSource, Audio, Clock, System,
               Platform) and the SDL 2 backend (sdl2/)
tools          bbassets (extract, pack, ids), the level editor bbedit (Dear ImGui on SDL 2,
               docs/editor.md), replay scripts
```

Dependencies point down only: `app` uses `game`, `core` and the platform
interfaces, and is the only user of a backend (`bb::platform::sdl2`);
`game` uses `core` and the platform interfaces (the audio banks hold a
`platform::Audio`); `core` uses the C++17 standard library only. No file of
the game outside `src/platform/sdl2` includes an SDL header (the editor,
a tool, uses SDL directly).

## The run

`main()` (in the backend, `sdl2_main.cpp`) calls `bbMain()`
(`src/app/main.cpp`), which parses the command line (`session::init()`),
creates the platform, opens the log and runs a `bb::app::App`:

1. `App::init()` checks the audio device, loads the settings (or the
   defaults in deterministic runs), the hi-scores, the string table, opens
   the window, loads the four interface fonts, the interface pictures and
   their musics.
2. `App::run()` creates a `bb::game::Game` and pushes an `AttractScreen`
   on a `ScreenStack`, then runs the stack until it is empty: each step
   asks the top screen for a frame (`Frame::Present`: the back buffer is
   presented; `Frame::Wait`: an idle frame; `Frame::Continue`: the stack
   changed). Nothing ever blocks inside a screen.
3. At the end the settings and the hi-scores are saved (not in
   deterministic runs) and `session::finish()` writes the fingerprints.

The screens (`src/game/src/screens/`) reach everything through
`bb::game::Services`: the back buffer and a backdrop pixmap, `Input`,
`Session` (milestones, game clock, quit, fingerprints), `Options`, the
asset source, the audio output, the strings, the fonts, the banks, the
`World`, the decor and the hi-scores. `App` owns all of them.

The campaign (`CampaignScreen`) plays the parts of `bb.lst`: cutscenes
(`CutscenePlayer`), briefings and levels (`LevelScreen`). A level loads
through `Game::startLevel()`, then runs frame by frame: `Game::beginFrame()`
asks the `FixedClock` how many 11.8 ms ticks the frame needs, each tick is
`Game::tickBegin()` (the input, the cheat keys, `Level::tick()`), the pause
menu or a dialogue when asked for, and `Game::tickEnd()` (the world-state
fingerprint); then `Game::drawAll()` draws the frame.

## Drawing

One 640x480 `core::gfx::Pixmap` is the back buffer. Sprites
(`core::gfx::Sprite`: a pixmap with a hotspot and a colour key) are drawn
with `core::gfx::draw()`, which keeps the 2002 clipping; fonts are
`core::gfx::BitmapFont`. The level's background is `bb::game::Decor`: an
840-pixel ring filled two columns at a time ahead of the camera, which
also takes the decals (corpses, blood). Entities draw through
`bb::game::Canvas`, which knows the back buffer, the fonts and the
strings. The platform's `Display` only presents finished frames, scaled
to the screen (the Switch's 1280x720 or 1920x1080: switch-port.md).

## Determinism

With `--record`, `--replay` or `--deterministic` the session gives a
virtual clock (60 frames per second), a fixed seed for the one random
generator, and the input of each frame from the recording. Every
presented frame and every tick is fingerprinted; the thirteen replays of
`tests/replays` compare those fingerprints with the expected ones (see
[../replays.md](../replays.md)). This is what kept the migration honest:
every step of Phases 1 to 5 left the fingerprints unchanged unless a
change was meant (and then the replays were recorded again).

## Assets

The game reads its assets through `core::assets::AssetSource`: the 2002
files of the data directory (`data/`, extracted from
`vc-projects/Blip_n_Blop_3/data.zip` by the build) or a runtime pack
(`blipblop.bbpack`, built by `bbassets` from `assets/source`, `--pack`).
Without `--data` or `--pack`, `bb::app::locateGameData()` takes `./data`
when it holds the game's files, else the pack next to the executable (on
the Switch, the pack at the root of the RomFS, `romfs:/`), else a `data`
directory next to the executable.
Images and sounds are named in the source manifests; `bbassets ids`
generates the constants of `src/assets` (see
[../formats/source-assets.md](../formats/source-assets.md)).
