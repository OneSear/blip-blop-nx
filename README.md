# Blip & Blop

*Blip'n Blop* ("Balls of Steel") is a 2002 run-and-gun game by LOADED Studio:
two ball-shaped heroes fight their way through parodies of the Smurfs, the
Snorks, Pokémon, the Care Bears, Lemmings and a gallery of video-game icons.
The game was released as open source in 2013 and ported to SDL by volunteers.

This repository brings that code to the **Nintendo Switch** as homebrew, in
C++17 with SDL 2 (devkitPro's) and CMake, with a portable core (no platform
code outside a thin SDL 2 layer). The migration to modern C++ is described
in [migration.md](migration.md), the Switch port in
[switch-port.md](switch-port.md); progress is tracked in
[docs/progress.md](docs/progress.md). The game still builds on a Mac or
Linux, by hand, to try things and run the tests; it is no longer released
for Windows, macOS or Linux.

## Playing on a Switch

The game is homebrew: it runs on consoles that can start homebrew (custom
firmware such as Atmosphère). Each release has two files:

* `blipblop-<version>-switch-nro.zip`: copy its `switch` folder to the SD
  card's root (`sdmc:/switch/blipblop/blipblop.nro`) and start the game from
  the Homebrew Menu. Start the Homebrew Menu in application mode (hold R
  while starting any game) for the console's full memory.
* `blipblop-<version>.nsp`: install it with an NSP installer (for example
  Goldleaf); the game then has its own icon on the HOME menu. It was not
  signed by Nintendo, so the console's custom firmware must accept such
  titles. A newer version installs over the older one.

Both keep the settings (`settings.cfg`), the hi-scores (`hiscores.scr`) and
the log (`BlipBlop.log`) in `sdmc:/switch/blipblop/`: the same ones whichever
starts. On the first run the game speaks French if the console does.

### Controls

In a level, for each player:

| Action | Console's Joy-Cons, Pro Controller, Joy-Con pair | One Joy-Con held sideways |
|--------|--------------------------------------------------|---------------------------|
| Move, aim | D-pad or left stick | stick |
| Fire | A, or ZR | right button, or SR |
| Jump | B | bottom button |
| Cow bomb | Y, or ZL | left button, or SL |
| Pause | + or − | its + or − |

In the menus: A confirms, B goes back one page (and resumes the game from
the pause menu), + or − pauses and resumes and skips cutscenes; any button
shows the next box of a dialogue. The hi-score name is typed on the
console's keyboard.

START offers games of 1 to 4 players. Player 1 chooses Blip or Blop and
player 2 is the other; players 3 and 4 are Bleach (a pink Blip: SMG and
laser) and Blitz (a green Blop: shotgun and flamethrower). Player 1 plays
with the console's Joy-Cons in handheld mode; a game of 2 to 4 players
opens the console's controller screen, where each player takes a
controller (two Joy-Cons held sideways are two players) and its lights
show the player's number: controller *n* plays player *n*. The controls
menu changes any player's bindings, shown with the Switch's names (`P1 A`,
`P2 ZR`); a player who binds an action to a controller keeps only the
bindings chosen there. A USB keyboard on the dock plays too (below).

### Display

Handheld, the picture fills the height of the 1280x720 screen: 960x720,
with black bars at the sides. Docked, the console draws 1920x1080 and the
picture is 1440x1080, or 1280x960 (exactly twice the game's 640x480) with
SCALING = INTEGER. Docking and undocking change it at once. The options
menu (in the main menu and the pause menu) also sets the FILTER: CRISP
(square, sharp pixels, the default), SHARP (nearest pixels, uneven at these
sizes) or SMOOTH (blurred); SMOOTH MOTION (on by default: it draws between
the game's 84.7 simulation ticks a second, so that movement is even on the
60 Hz screen, one tick late); and the language (English or French).

## Building

### The .nro

Requirements: devkitPro's Switch packages, CMake 3.21 or later (presets) and
a C++17 compiler for the machine you build on: the build first compiles the
asset tool `bbassets` for it and makes the runtime pack (94 MB) from
`assets/source`, which goes into the `.nro`'s RomFS.

```sh
sudo dkp-pacman -S switch-dev switch-sdl2      # once; devkitPro's pacman
cmake --preset switch && cmake --build --preset switch   # build/switch/blipblop.nro
nxlink -s build/switch/blipblop.nro           # to a Homebrew Menu waiting on Y (NetLoader)
```

nxlink passes the arguments after the `.nro` to the game (`--replay`,
`/cheat`; put `--` before options nxlink would take for its own), and shows
the game's warnings on the PC. Debug builds (`-DCMAKE_BUILD_TYPE=Debug`) also
carry the replays in the RomFS (`romfs:/replays/`).

### The .nsp

```sh
tools/switch/make-nsp.sh                   # build/switch/blipblop-<version>.nsp
tools/switch/make-nsp.sh --upload v1.2.0   # from the tag, attached to its release
```

It needs hacBrewPack (not part of devkitPro: build it with `make` from a
mirror of the deleted The-4n/hacBrewPack, for example
<https://github.com/rlaphoenix/hacBrewPack>) and the keys it encrypts with,
dumped from your own console (Lockpick_RCM) into `~/.switch/prod.keys`.
The keys are Nintendo's: they never go into the repository or CI
(`.gitignore` lists the usual key files). The title ID, `0x011142be60644000`
(`cmake/BBSwitch.cmake`), is the installed game's identity: never change it.
The `.nsp`'s permissions are `cmake/switch/npdm.json`.

### CI and releases

CI (`.github/workflows/ci.yml`) builds the `.nro` on every push and pull
request, in the `devkitpro/devkita64` container, and nothing else. Pushing a
tag `v*` (for example `git tag v1.0.0 && git push origin v1.0.0`), or
creating a release with a new tag in GitHub, runs
`.github/workflows/release.yml`: it builds the `.nro` with the tag's version
and attaches `blipblop-<version>-switch-nro.zip` to the tag's release (a
release it has to create starts as a draft and is published once the zip
is attached). For an existing tag: Actions > Release > Run workflow. Then
attach the `.nsp` with `tools/switch/make-nsp.sh --upload <tag>`. Before a
release, the checks of [docs/release-checklist.md](docs/release-checklist.md),
the local tests included.

### Local builds on a Mac or Linux

The game, its tests and its tools build on a Mac or Linux, to try things and
to test; CI does not build them. Requirements: CMake 3.21 or later, a C++17
compiler and SDL2.

| System | Dependencies |
|--------|--------------|
| macOS (Homebrew) | `brew install cmake sdl2` |
| Debian / Ubuntu | `sudo apt install cmake g++ libsdl2-dev` |

```sh
cmake --preset default          # Debug build in build/default
cmake --build --preset default
ctest --preset default          # unit tests and regression replays: before each merge and release
```

Other presets: `release` (optimised, much faster replays) and `asan`
(AddressSanitizer and UndefinedBehaviorSanitizer). In-source builds are
refused. `tools/docker/linux-check.sh` builds and runs every test in a Linux
container (GCC, a case-sensitive file system).

The build extracts the game data (`vc-projects/Blip_n_Blop_3/data.zip`) into
the build directory, next to the executable, and builds the runtime pack
`blipblop.bbpack` from the source assets in `assets/source/` (PNG, JSON, WAV,
Ogg Vorbis and UTF-8 text; see [docs/formats/source-assets.md](docs/formats/source-assets.md)).
When `assets/source/` is absent, the build extracts it from the game data
first.

### Assets

The asset tool `bbassets` (built in `build/<preset>/tools/`) converts between
the 2002 files, the source assets and the runtime pack:

```sh
bbassets extract data ../../assets/source     # 2002 files -> source assets
bbassets pack ../../assets/source game.bbpack  # source assets -> runtime pack
bbassets verify ../../assets/source data       # do the sources rebuild the 2002 files?
bbassets legacy ../../assets/source out        # source assets -> 2002 files (for the editors)
bbassets ids ../../assets/source ../../src/assets/include   # sprite-name constants
```

The level editor `bbedit` (same directory) edits the levels and the sprite
hotspots and names of `assets/source`, and test-plays a level with F5:
see [docs/editor.md](docs/editor.md). A change of the game's text goes into
`assets/source/strings/` and, through `tools/data/fix_legacy_data.py`, into
`data.zip`.

## Running a local build

```sh
cd build/default
./blipblop            # add /quiet to disable music, /cheat for cheat keys
./blipblop --pack blipblop.bbpack   # read the assets from the runtime pack
```

Without `--data` or `--pack`, the game reads the `data` directory of the
working directory; when there is none, the runtime pack next to the
executable (`romfs:/` on the Switch), then a `data` directory next to the
executable.

Keyboard defaults: arrows to move and aim, Left Ctrl to fire, Left Alt to
jump, Space for the cow bomb, Escape for the pause menu. Cheat keys (`/cheat`):
F1 ammo, F2 cow bomb, F3 extra life, F9 win the level, F12 FPS counter.
Players 3 and 4 have no keys by default and play with game controllers 3 and
4. Controllers have the Switch's layout everywhere, by position (A is the
right button, B the bottom one; SDL's A is the Switch's B); other pads keep
their own button numbers.

A local build keeps its settings, hi-scores and log in the user's
preferences directory, for example
`~/Library/Application Support/LOADED Studio/Blip and Blop/` on macOS or
`~/.local/share/LOADED Studio/Blip and Blop/` on Linux; `--settings <file>` and
`--log <file>` choose other files. The first run imports the 2002 files of
the data directory when there are any: `data/bb.cfg` and `data/bb.scr` (the
one in `data.zip` holds two scores played in 2014, "KID" and " DDD", above
the default table).

### Deterministic runs, replays and regression tests

```sh
./blipblop --record session.bbreplay        # play and record the input
./blipblop --replay session.bbreplay --hash out.bbhash
./blipblop --headless --replay ../../tests/replays/campaign.bbreplay
./blipblop --help
```

Recording and replaying use virtual time, a fixed random seed and the
reference settings, so a replay reproduces the same frames on every run and
every platform, the Switch included. `--hash` writes fingerprints of the
presented frames and of the world state; the regression tests compare them
with the expected values in `tests/replays/`. See [docs/replays.md](docs/replays.md).

## Repository layout

```
CMakeLists.txt, CMakePresets.json, cmake/   the build (cmake/BBSwitch.cmake: the .nro and the .nsp)
src/core/                 platform-independent code (C++17 standard library only)
src/platform/             platform interfaces, and their SDL 2 backend (the only SDL user;
                          sdl2/switch/: the few libnx calls of the Switch)
src/game/                 the game: world, entities of every world, screens, campaign
src/app/                  the game executable: entry point, session (record/replay), input, settings
tests/                    unit, conformance and regression tests; benchmarks in tests/bench
tools/                    developer tools: bbassets, the level editor bbedit, replay scripts,
                          tools/switch/make-nsp.sh
assets/source/            the source assets the runtime pack is built from; assets/switch/: the icon
vc-projects/Blip_n_Blop_3 data.zip, the 2002 game data (the 2002 code is in the git history)
vc-projects/*Editor*      the original MFC level, sprite, font and sound editors (Windows; replaced by bbedit)
level-editor/, assets-gfx/ source art for the editors
docs/                     file formats, replay format, progress notes
```

## Licence and credits

The code is distributed under the MIT licence (see [LICENSE](LICENSE)): the
Google Code project through which LOADED Studio published the game in 2013
recorded the licence as MIT.

Original game (2002): Benjamin Karaban and Sylvain Bugat (code); Gerard Kelly
(music and sound design); Rik Ede and El Mobo (additional music); Laurent
Schneider, Jérémie Comarmond, Didier Colin, Jérôme Karaban and Sylvain Bugat
(artwork); Eric Khodja and Benjamin Karaban (storyline). SDL port (2013-2014):
Roberto Domenella, kidanger and Benjamin Karaban. Modernisation work in the
upstream repository (2019): Guillaume "Vermeille" Sanchez —
<https://github.com/benkaraban/blip-blop>.

The game parodies characters that belong to their respective owners (the
Smurfs, the Snorks, Pokémon, the Care Bears, Lemmings, Mario, Sonic, Pac-Man,
Lara Croft, Rayman and others). Those characters are not covered by the
licence above, and builds of this game must not be distributed commercially.
Nintendo Switch is a trademark of Nintendo; this homebrew is not affiliated
with or endorsed by Nintendo.
