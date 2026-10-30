# Migration progress

Status of the phases of [migration.md](../migration.md), with the facts
learnt while implementing them. Newest phase first.

## The Switch port (30 October 2026: implemented, not yet tried on a console)

[switch-port.md](../switch-port.md), steps S1 to S7, done in one go; what a
console must still show is the checklist (`docs/release-checklist.md`).

### What changed

* **The Switch is the only target.** CI builds the `.nro` and nothing else
  (`ci.yml`: one job in the `devkitpro/devkita64` container); `release.yml`
  attaches `blipblop-<version>-switch-nro.zip`; the maintainer packs the
  `.nsp` with `tools/switch/make-nsp.sh`. Gone: the desktop jobs and
  packages (CPack, the macOS bundle and its universal SDL, the Windows DLLs,
  the MinGW cross-check, the replays from the packages) and every Windows
  branch of the code and the build. Local builds on a Mac or Linux stay,
  for the tests, `bbassets` and `bbedit`.
* **Build.** The `switch` preset (devkitPro's `Switch.cmake`) builds
  `bbassets` and the runtime pack first for the machine it runs on (this
  tree again, `BB_TOOLS_ONLY`, no SDL), then the game, `blipblop.nro` with
  the pack in its RomFS (`cmake/BBSwitch.cmake`). The `.nsp`: `elf2nso` and
  `npdmtool` (`cmake/switch/npdm.json`) make the ExeFS, the NACP and the
  icon (`assets/switch/icon.jpg`, from the title picture, no Exif) the
  control data, hacBrewPack packs them with the maintainer's keys
  (`cmake/switch/MakeNsp.cmake`). Title ID `0x011142be60644000`, chosen at
  random with bits 0 to 12 clear.
* **Display.** `core::gfx::viewport()` places the 640x480 frame: fit
  (960x720 at 160,0 on the 1280x720 handheld screen, 1440x1080 at 240,0 on
  1920x1080) or by a whole factor of 2 or more (1280x960 at 320,60 docked).
  `Sdl2Display` draws that rectangle itself (no more SDL logical size) and
  implements the new FILTER value CRISP: the frame enlarged with sharp
  pixels by the next whole factor in a render-target texture, then filtered
  down. On the Switch the window is created at the screen's size (else the
  system stretched 640x480 to 16:9) and stays resizable, so that SDL
  follows docking; the display reads the output size at every present and
  logs a `Display:` line when it changes. VSYNC, F10 and the fullscreen
  setting are gone; the defaults are CRISP and SMOOTH MOTION on; the
  deterministic runs start from `core::Settings::reference()` (smooth motion
  off, as the replays were recorded).
* **Controls.** The default pad layout is the Switch's, by SDL's positional
  numbers: fire `kPadButtonB` (the right button, the Switch's A) or ZR, jump
  `kPadButtonA` (bottom, B), cow bomb `kPadButtonX` (left, Y) or ZL
  (`Bindings::defaultPadCodes()`: two inputs per action). The triggers come
  as buttons 32 and 33. A pad's right button acts as Return (confirm), its
  bottom button is the menus' new back (`Input::backHeld()`: up one page,
  resume from the pause menu). The controls menu names pad inputs the
  Switch's way (`P1 A`, `P2 ZR`, `P3 +`). SDL's Nintendo layout by label is
  turned off on the Mac too (`SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS`).
* **System.** `switch_main.cpp`: RomFS, nxlink's standard output when it
  started the game, the HOME menu's exit locked (closing reaches the game
  as a quit), `sdmc:/switch/blipblop/` as current and preferences
  directory. The applet's focus messages pause the game (HOME, sleep);
  `Sdl2Input` delivers a focus return seen in the same update as the loss
  one update later. Errors in the error applet; the controller applet
  before a game of 2 to 4 players; the hi-score name on the console's
  keyboard (`Input::beginTextEntry()`, never in deterministic runs); the
  console's language on the first run; the mixer at the device's rate
  (48 kHz); the settings saved when the options or controls pages are left
  and the hi-scores after a name entry.
* **Text.** CRISP (id 862, PRECIS in French) and "Press a button to start."
  (id 852) in `assets/source/strings` and, through
  `tools/data/fix_legacy_data.py`, in `data.zip` (`uk.dat`, `fr.dat`).

### Facts learnt

* devkitPro's SDL 2 for the Switch is 2.28.5; its pkg-config wrapper
  (`aarch64-none-elf-pkg-config`) links statically by itself. devkitA64's
  GCC is 15.2 and builds the tree with the strict warnings; its C++ library
  has `std::filesystem` (it links; still to see it list `romfs:/` on a
  console). The `.nro` is 102 MB, the ELF's code 7.8 MB.
* `npdmtool` refuses more than one debug flag; hacBrewPack's template sets
  two. hacBrewPack (built from the rlaphoenix mirror) packs the four
  folders with key generation 1; with random keys it made a 99 MB `.nsp`
  (not installable: the check of the chain only, deleted after).
* libnx's controller applet has no option for single Joy-Cons: the style
  set SDL configures allows them.

### Verification (30 October 2026)

* Before the changes, on the Mac (Debug): 40 of 40 tests passed.
* After: the 11 tests without replays pass (Release), with the new cases
  (viewport sizes, the Switch layout and its second inputs, the Switch's
  names, `filter crisp`, a desktop-era settings file, the CRISP display,
  the triggers as buttons).
* The replays: every tick fingerprint of the 13 replays that changed is the
  same as before (the simulation is untouched); only frames differ, from
  the first briefing ("Press a button to start.") and, in `menus`, from the
  options page (VSYNC gone, CRISP) — `menus` also presents 12 more frames,
  the two taps of the VSYNC item that were waits for a release. `attract`,
  which shows neither, is unchanged. `fourplayers`, generated again with the
  Switch layout's fire button (`JOY3_B1`), keeps every tick. The expected
  fingerprints were recorded again (`update_replay_hashes`); the 29 replay
  tests then pass, data and pack.
* The Switch build in the `devkitpro/devkita64` container: `cmake --preset
  switch && cmake --build --preset switch` from the tree makes
  `blipblop.nro` (102 MB); a Debug build carries the 14 replays in
  `romfs:/replays/`; `tools/switch/make-nsp.sh`, run on a copy of the tree
  with hacBrewPack and random keys, made `blipblop-0.1.0.nsp` (99 MB).
* Not seen yet, on a console: everything of `docs/release-checklist.md`.

## Phase 9 — Optional improvements (the chosen items done)

The maintainer chose (28 October 2026) the French text, the scaling
options, render interpolation and the in-house mixer; not the restored cut
content (achievements, also offered, were then removed from the plan).
Remappable controller buttons already existed: the 2002 controls menu
takes any input, pad buttons and directions included, for each player
(`DeviceInput::firstHeld()`).

### The options page

Eight items in both menus (`Menu::optionsItems()`, `switchOption()`,
`chooseOption()`, which replace the two copies of 2002): VSYNC, SCALING,
FILTER, SMOOTH MOTION, LANGUAGE, the two players' controls, RETURN. Right,
left and return switch an option. The settings file gains `scaling
fit|integer`, `filter sharp|smooth` and `smooth-motion on|off`; `language`
was read and written already, now it is used. The main menu's OPTIONS item
comes from the string table (it was a literal).

### French text (D-25)

* `assets/source/strings/fr.json`: a new translation of the 253 strings
  (the 2002 `fr.dat` was never in the repository). Names, places and the
  empty ids are kept. The app loads `data/fr.dat` or `data/uk.dat` for the
  language (`App::loadStrings()`), again when the menu switches it, and
  falls back to English when the data has no French.
* Fact: the fonts decide the characters. The menu, banner and HUD fonts
  (`menu`, `menus`, `cool`) have capitals, digits and
  ``!'*+,-./;=[\]` `` only; the dialogue font `rpg` adds lowercase and
  exactly à è é ê ô (no ç, â, î, û, ù, no capital accents, no colon); a
  missing character is skipped silently. The 2002 font was evidently made
  for the lost French text, so the translation keeps to it: capitals
  without accents, and wording without the other accented letters.
* `tools/data/check_strings.py` (ctest `strings.fonts`) checks every
  table but English: each character exists in the font the English
  string is drawn with, and each dialogue fits its box when word-wrapped
  as `printWordWrapped()` does (4 lines of 515 pixels, or as many as the
  English: string 606 takes 5 lines in English, its last one a few pixels
  below the box). Checked to catch a ç and a capital É.
* The pack holds `fr.dat` (every `strings/*.json` becomes a table). The
  2002 data must hold it too (`assets.verify`; the replays run on both):
  `tools/data/fix_legacy_data.py` adds `data/fr.dat` to `data.zip`, written
  from `fr.json` as `bbassets` writes it (verified byte for byte by
  `assets.verify`), and fills the new option labels of `uk.dat` (ids 802,
  804, 811–815, 817, 818, empty until now).
* D-96 (fixed): the French key names of 2002 named AZERTY key positions;
  letters now show the key's character (SDL key codes follow the layout).
* Not translated: words drawn in pictures (the character choice's
  "SELECT YOUR CHARACTER", the briefing pictures).
* Checked in frames: the options and key pages, the pause menu and a
  dialogue ("Oh non! Le pauvre vieux a été tué!") in French.
* To proofread: change `fr.json`, run `fix_legacy_data.py` on `data.zip`
  (it writes `fr.dat` again) and the tests.

### Scaling and filter

`Display::setScaling(integer, smooth)`: `SDL_RenderSetIntegerScale()` and
`SDL_SetTextureScaleMode()` (SDL 2.0.12). The defaults (fit, sharp) are
what the game did. The log's Display line names both, and is written again
when they change. Conformance test: in a 960×720 window, fit places the
frame at 0,0 scaled to 960×720, integer at 160,120 at scale 1. Fact:
`SDL_RenderReadPixels()` reads the viewport only, so a border cannot be
read back; the test reads the viewport and scale instead. Shaders: not
possible with SDL 2's renderer and software-drawn frames (DEC-01).

### Smooth motion (render interpolation)

* `World::Motion`: each tick starts with `World::rememberPositions()` (the
  camera and every entity of the 16 lists); `Actor::drawAt()` draws an
  entity back towards its previous place by the fraction of a tick the
  clock owes (`FixedClock::owed()`, in 256ths), and `Level::drawWorld()`
  draws the camera so too when it moved 1 or 2 pixels (the decor ring
  holds the 2 columns before the camera), then puts it back. Moves of more
  than 16 pixels in a tick are jumps, drawn where they end. Entities that
  draw without `drawAt()` (Lara's spikes) are drawn where they are.
* Off by default: the replays keep their fingerprints (24 of the 26
  replay runs, data and pack, passed unchanged; only `menus` was
  re-recorded, for its new options page). `--smooth-motion` turns it on
  from the start; `replay.smurf.smooth` checks that the ticks are the
  same and the frames differ. Mutation check: without the camera put back
  after drawing, the test fails at line 10.
* Measured on 160 consecutive frames of the `smurf` replay (the decor's
  scroll between frames, from the dumped frames): off, steps of 2 and 4
  pixels (2,4,2,2,4...), mean 2.46, mean change between frames 1.37
  pixels; on, steps of 3 (3,3,3,2,3...), mean 2.50, mean change 0.48.

### The four-player mode (29 October 2026)

Asked by the maintainer: a third and a fourth player, Bleach ("a pink
girly player") and Blitz ("a green boy player").

* The art: the 2002 sprite banks are made per hero with one layout, and
  the hero classes differ (Blip: SMG and laser; Blop: shotgun and
  flamethrower), so Bleach is a recoloured Blip and Blitz a recoloured
  Blop (`blipKind()`), with their weapons. `tools/art/new_heroes.py`
  recolours the 2002 files themselves: blue body and hands to pink and
  the red bandana to purple (Bleach), red body and hands to green and the
  green helmet to khaki (Blitz), the score digits pink and green. In the
  muzzle-flash images (74–103), which hold the hand too, only the colours
  of the hero's other images change, never a pure red (the flames' reds,
  shared with Blip's bandana). Its LGX decoder and encoder give the 2002
  bytes back unchanged (checked on the four source files).
  `fix_legacy_data.py` adds `bleach.gfx`, `blitz.gfx`, `scorep.lft` and
  `scoreg.lft` to `data.zip` (written again when the script changes
  them); `bbassets extract` made their sources (`sprites/bleach`,
  `sprites/blitz`, `fonts/scorep`, `fonts/scoreg`, without sprite names:
  the code reaches every hero bank through Blip's names).
* Players: `kPlayerCount` (controls) and `kMaxPlayers` (game) are 4.
  Players 3 and 4 have no default keys and play with pads 3 and 4 (the
  default layout, `defaultPadCode()`); the settings file and the controls
  menu have `p3.`/`p4.` and PLAYER 3/4 CONTROL. The 2002 `bb.cfg` import
  still reads two players. START has 3 and 4 PLAYERS; player 1 chooses
  Blip or Blop, 2 is the other, 3 Bleach, 4 Blitz.
* Game: players 3 and 4 start on the starting points of 1 and 2 (the
  levels have two; another place could be in a wall or over a hole) and
  separate once they move. The camera, the targeting of enemies, the
  screen edges and the vehicles already worked on the player list. The
  scrolling stops for any of the four dying (`Camera::scrolls()`).
  Dialogues are written for one or two heroes: the first two players in
  the list speak, with Blip's faces for Blip and Bleach and Blop's for
  Blop and Blitz (the 2002 code wrote the faces of every listed player
  into an array of two). The hi-score entry asks each of the four.
* HUD: players 3 and 4 in a second row under 1 and 2 (88 pixels lower).
  The bottom corners were tried first: they hid the ground the heroes
  walk on (seen in the frames). The level summary is a table with three
  or four players: the labels once, a 120-pixel column per player still
  in the level, over the whole width.
* Not changed: the balance (bosses lose their life four times as fast
  with four players), the cheat keys (players 1 and 2 only).
* Checked: with one or two players nothing changed but the START page:
  every replay keeps its tick fingerprints, and frames dumped from the
  previous commit's build and this one are byte for byte the same (the
  `smurf` level at frames 1000–1004 and 15000–15002; one frame in 100 of
  the two-player `gameover` replay, 127 frames with its HUD and both
  summaries). The frame fingerprints changed from the START page on
  (they are chained) and were recorded again. New replay `fourplayers`:
  four players (two on the keyboard, two on pads 3 and 4) through the
  first level and its dialogue, the second level won (the table of
  four), the bonus level left from the pause menu (its summary), then a
  quit; looked at in frames (the four heroes, the second HUD row, the
  table in four colours).

### The in-house mixer (DEC-06)

* `bb::core::audio::Mixer` (`src/core/src/audio_mixer.cpp`): 32 voices
  (the 2002 FMOD channel count), sounds read from their WAV files
  (`decodeWav()`: PCM 8 or 16 bits, mono or stereo) and converted at load
  to mono 16-bit at 44.1 kHz, one music decoded as it plays and converted
  to 44.1 kHz stereo; everything summed at full volume and clipped, as
  SDL_mixer mixed it for the game (no volume was ever set). A voice is the
  first free one, as `Mix_PlayChannel(-1)` chose. Rate conversion is
  linear, in 16.16 fixed point.
* The decoders (`src/core/codecs/audio_codecs.cpp`, library
  `bb_core_codecs`, built without the strict warnings and left out of
  clang-tidy): stb_vorbis 1.22 for Ogg Vorbis and, by the maintainer's
  choice (29 October 2026, DEC-11), dr_mp3 0.7.4 for the two MP3 jingles
  (`tambour`, `gameover`). Both vendored in `third_party`, pinned.
* The SDL backend opens a plain audio device (44.1 kHz, stereo, 16-bit,
  1024 frames, as the 2014 port asked SDL_mixer for) whose callback calls
  `Mixer::mix()`; every other call locks the device. Sounds and musics are
  decoded outside the lock. SDL2_mixer is gone from the build, CI, the
  Docker images and the packages (with it libvorbis, libogg, mpg123 and
  wavpack).
* Checked: unit tests (WAV forms, rate conversion, voices, looping,
  clipping, a music played once, looped and at half the rate); the
  conformance test decodes every shipped sound (175) and music to its end.
  Every Ogg file decodes to exactly the sample count its last page
  declares (granule position; the 14 match), and the two MP3s to exactly
  their frames × 1152 (tambour 61,056 = 53 frames, gameover 268,416 = 233
  frames). The game opens the real device on macOS (no warning) and the
  replays are unchanged (they do not hear the sound). Not checked: how it
  sounds; that is on the release checklist ("Audio").

## Plan change (16 October 2026)

Supported targets are now Windows, macOS and Linux only; the consoles and
handhelds (Switch, original Xbox, Miyoo Mini, Anbernic-class) are dropped,
and portability stays a design goal (migration.md §1.1 G4, §1.2, §3.6).
Phase 7 becomes the desktop release (Windows verified, CI on three
platforms, presentation, CPack packages); DEC-02 (one 32-bit pixel
format), DEC-06 (SDL2_mixer; in-house mixer optional) and DEC-10
(exceptions only for programming errors) are revised, DEC-09 and R7
withdrawn. Nothing done so far depended on the dropped targets.

## Completing the plan, continued (25 October 2026)

The four items left open on 24 October that needed no hardware, then the
first CI results. The replays kept their fingerprints throughout (release
build, 13/13 identical; Linux in Docker, GCC, 34/34 tests).

* **DEC-08 helpers** (`src/game/include/bb/game/components.hpp`):
  `AnimationPlayer` (the step `frame` and its tick counter `subFrame`;
  `tick()`, `cycle()`, `tickUpTo()`, `loop()` over a table, `restart()`),
  `Hitbox` (corners, `active`, `overlaps()`, which is the 2002
  `Sprite::collision`) and `Health` (`points`, `take()`, `heal()`,
  `left()`). `Entity::hitbox`, `Actor::animation` and `Character::health`
  replace the loose fields, and the animation and damage code each class
  repeated calls the helpers. Unit tests: `tests/unit/components_test.cpp`
  (6 cases). docs/architecture/world.md and docs/legacy_map.md updated.
* **Platform `FileSystem`** (`bb/platform/file_system.hpp`, §3.5): read
  access by UTF-8 path is `core::FileReader` (open and read at an offset,
  a file's kind, a directory's entries), which `Vfs` and `PackFile` now
  read through; the platform's `FileSystem` adds whole-file writing and
  the game and preferences directories (moved out of `System`). The SDL
  backend uses `SDL_RWops`, so Windows paths are UTF-8; the tools and
  tests use `core::hostFiles()` (the C++ library). Settings, hi-scores,
  recordings, the data lookup and the pack go through it. Still written
  with `fopen`: the log and the `--hash` and `--trace-ticks` outputs.
  Conformance test `tests/conformance/sdl2_files_test.cpp` (writes, lists
  and reads back files with UTF-8 names). docs/architecture/platform.md
  updated.
* **D-55, the rest**: the seven debris classes that end only when they
  land (Rayman's limbs and head, the Care Bears' walking and dancing legs,
  the Snork boss world's grunt head and trunk and the rider's body) now
  also end once below the screen (`Actor::dieWhenFallenOut()`, y > 600:
  the screen is 480 pixels high and no platform is under it, so nothing
  that has fallen there would land). In 2002 a piece that fell into a
  hole fell for the rest of the level. No replay drops a piece into a
  hole (fingerprints unchanged); `tests/unit/actor_test.cpp` does, and
  fails with the check disabled. D-55 is closed; no defect of Appendix D
  is open.
* **Present paths on Linux (D-08)**: `tools/docker/linux-bench.sh` runs
  `bb_present_bench` in the Ubuntu 24.04 container on Xvfb: SDL 2.30.0,
  X11, OpenGL renderer over Mesa's software rasteriser (no GPU), window
  surface RGB888, 4 CPUs of this Mac (Intel). 600 frames per path, three
  runs in each measuring order; medians per frame:

  | Path | Medians (6 runs) |
  |------|-----------------:|
  | new texture every frame (2014 port) | 3.85–5.29 ms |
  | streaming texture (the game's) | 2.80–4.05 ms |
  | window surface | 3.66–5.83 ms |

  The streaming texture was the fastest in each of the six runs. These
  are software-rendering numbers; real Linux and Windows hardware is a
  line of docs/release-checklist.md. Under Wine the numbers would
  measure Wine, so none were taken there.
* **Present paths on Windows**: CI's Windows job runs the benchmark once
  (informative step) on the hosted VM, no GPU: SDL 2.32.10 (vcpkg),
  Direct3D renderer, window surface RGB888, 600 frames per path, one run
  (CI run 40212003833). Medians: new texture every frame 2.00 ms,
  streaming texture 1.10 ms, window surface 0.32 ms. The window surface
  is the fastest there but cannot scale (fullscreen, resized window,
  high DPI); the streaming texture stays, at about 7 % of a 60 Hz frame
  on this machine.
* **First CI run** (`c0dd29f`, run 40137765992, read through GitHub's
  public API; the job logs need a login): Linux (GCC), Linux ASan + UBSan
  and the static analysis passed. Windows failed installing its
  dependencies: vcpkg's `sdl2-mixer` has no `libvorbis` feature (Ogg
  Vorbis is a plain dependency of the port, checked in its
  `vcpkg.json`, version 2.8.2), so `sdl2-mixer[libvorbis,mpg123]` is
  refused; now `sdl2-mixer[mpg123]`. macOS (arm64 runner) passed the
  build, the tests and the replays and failed at `cpack`; the same
  command makes the disk image on this Intel Mac, so the cause is in the
  runner's log. A step now prints CPack's logs when packaging fails.
  Also for the first MSVC build: the build and test presets name their
  configuration (the Visual Studio generator is multi-config and would
  build Debug), and `cpack` gets `-C RelWithDebInfo`.
  `actions/checkout@v5` and `actions/upload-artifact@v6` run on Node 24
  (the v4 ones were flagged as deprecated Node 20).
* **The first MSVC build, fixed in three CI rounds** (logs read with
  `gh`):
  * `8ab0f9e`: `FireballUp::drop()` (`mario.cpp`) declared a local
    `kGravityDelay` hiding the one of `actor.hpp`: C4459, an error under
    `/WX`. GCC and Clang do not report it, even with `-Wshadow-all`; a
    search of the sources for other local constants named as a visible
    namespace constant found none.
  * `2fb4a4d`: the new `FileSystem` conformance test rewrote a file it
    still had open for reading. SDL 2 opens a file for reading with
    `FILE_SHARE_READ` only and for writing with no sharing, so Windows
    refuses the write (`SDL_rwops.c`). The game never does it (only the
    pack stays open, and it is never written); the test closes the file
    first and `FileSystem::writeAll` documents the rule.
  * `9932475`: every replay ended at once with `0xc0000409`.
    `std::setvbuf(stdout, nullptr, _IOLBF, 0)` (deterministic runs) has
    a size below Microsoft's minimum of 2, and their C library stops the
    process on an invalid parameter; `_IOLBF` is full buffering there
    anyway. Windows now uses `_IONBF`. MinGW's C library accepted it,
    which is why the Wine runs passed.
* **CI green on the three platforms** (run 40212003833, `9932475`):
  Linux (GCC), Linux (Clang, ASan + UBSan), macOS (arm64) and Windows
  (MSVC, SDL 2.32.10 and SDL2_mixer 2.8.2 from vcpkg): every test and the
  26 replay runs with the same fingerprints, and the packages (Linux
  .tar.gz, macOS disk image, Windows NSIS installer and zip) as
  artifacts. The Windows job is no longer allowed to fail. Risk R6 is
  closed. macOS packaging, which failed in the first run, has passed in
  every run since; its cause is unknown (no log), and CI prints CPack's
  logs if it fails again.

## Completing the plan (24 October 2026)

An audit of migration.md against the code (its definition of done, §4.5,
and the items of each phase) listed what was asked for and not done; the
user asked for all of it. This section records the work and its checks.
Done first, without changing a fingerprint (the 26 replay runs, data and
pack, passed after it), then the gameplay fixes, which change them.

### Without gameplay changes

* **D-43**: the distances Lara and the zombie shaman divide by are at
  least 1 (`lara.cpp`, `zombie_shaman.cpp`); 2002 divided by zero when the
  target stood on the gun.
* **Global state (§4.5, D-50)**: the 14 mutable globals of the game (the
  2002 file statics and static members: shotgun spread counters, zombie
  moans, the stork, the Lemmings counters, the wild Smurf's timer, Toad's
  counter, the video props' clocks) live in a per-world store,
  `World::shared<T>()`, cleared at each level start. The app's state
  (`session.cpp`, `files.cpp`) became the classes `Session` and
  `GameFiles`, owned by `main`.
* **French identifiers**: `tools/check_identifiers.py` checks the code
  against the 113 French words of the glossary (Appendix B), comments and
  strings excepted; test `naming.french`. Checked that it fails on two
  planted identifiers.
* **Reader fuzzing** (§6): `tests/formats/fuzz_test.cpp` (in `formats`)
  feeds every legacy reader seeded damage of the shipped files (flipped
  bytes, extreme size fields, truncation) and random bytes; they may
  accept or refuse but not crash or read out of bounds. Found at once:
  `readGfx` trusted a damaged image count and the test process grew to
  7.2 GB (stack sample: `assets_legacy_formats.cpp:130`). `readGfx`,
  `readSfx` and the LGX decoder now refuse counts the remaining bytes
  cannot hold, before allocating.
* **Benchmark** (§6): headless runs print `bench: N frames, X ms per frame
  on average, Y ms at worst, T ticks per second, peak memory M MB`
  (docs/replays.md).
* **Replay failures** dump the frames of the build under test around the
  first divergence, one in six, and print the folder (docs/replays.md).
* **Poisoned locals** (§6): option `BB_POISON_LOCALS`
  (`-ftrivial-auto-var-init=pattern`), on in the `asan` preset.
* **`InputMapper` removed**: nothing used it (Phase 1 interface); the
  menus and `AliasControls` read the bindings.
* **Symbolic entity ids (DEC-07)**: the source levels name their entities
  as the registry does (`"enemy": "smurf.farmer"`, 1,056 fields);
  `bbassets` writes names and reads names or numbers; the pack built from
  them is byte for byte the previous one.
* **Level markers (D-51)**: the level positions the 2002 classes kept in
  their code (the wild Smurf's limit, the shaman's spots, the Snork boss's
  perches, zones and shark path, the stork, the Lemmings cow bomb,
  Mario's floor, the Princess, Rayman, Lara's spikes and wall, the
  flamethrower ban under water) are named markers of the level
  (`World::marker()`): in the source levels' `"markers"`, in the pack
  (format version 2), and for the 2002 `.lvl` files from a table
  (`assets_level_markers.cpp`). The `content` test checks that every level
  placing a class has the markers it reads and that both sources agree
  (10,548 checks; checked that it fails without `snorks.bossZones`).
* **Registers (D-52)**: the 17 remaining uses of the generic registers by
  number are named per world.
* **No floating point in the game (D-56)**: `squareRoot()` (`trig.hpp`,
  bit by bit) replaces the shaman's and Lara's `int(sqrt())`; the
  plumbers' jump `int(-9 + speed * 0.25)` is `(speed - 36) / 4`; the
  cruisers' vertical shot speed `int(2 - (dx * 2 * 0.25) / dy)` is
  `(8 * dy - 2 * dx) / (4 * dy)`. A unit test compares each with the
  floating-point form: every square root up to 2^22 and one in 9,973 up
  to `INT_MAX`, every speed in ±1000, every dx and dy in ±1000. The 13
  replays kept their fingerprints (done after the re-recording below).
* **D-19 closed**: the cutscene fade was commented out in the original
  2002 player too (commit d48c5bd), and no shipped cutscene uses it.
* **D-95, found while checking D-19, fixed**: the cutscene colours were
  not "never used". The 2002 `drawScene()` cleared the screen with layer
  0's `SetBackColor` colour (white for a non-zero argument) and filled
  the bars with its `SetClipColor` colour; the 2014 SDL port always used
  black. `intro.cin` and `end.cin` each have two white flashes
  (`SetBackColor(0, 1)`, `RenderScene(2)`, commented "Flash blanc") that
  were lost. Restored: the attract replay now shows three white frames
  at frames 412 to 414, between the "BAD... NASTY" scene and the next
  (checked on the dumped frames). Changed fingerprints, re-recorded: the
  four replays that show a cutscene past a flash, `attract` (the intro),
  `menus` (the intro again from the main menu, 686 frames),
  `finalboss` and `campaign` (`end.cin`); the others skip the intro at
  frame 31.
* **Music tracks**: playlist tracks can be named; `bbassets ids` writes
  `bb/assets/music/<bank>.hpp`, and the 11 calls that played a track by
  number use the names (docs/formats/source-assets.md).

### Gameplay fixes (kept 2002 defects)

The defects kept so far so that the replays stayed identical are fixed;
each site has a "fixed" comment with the 2002 behaviour.

* D-40: a sprite index outside its bank draws a transparent 1×1 sprite
  (2002 read past the bank); unit test.
* D-54: an enemy walking off a platform falls without a second step on
  the same tick (basic Smurf, Mario, Luigi, small Mario, Rayman, the video
  walkers, Sonic), and Rayman's `fight()` returns after the fall.
* D-55, the kill percentage: the never-dying stork (D-77), the Snork in
  Black cameo of the Care Bear world (shots go through it) and small
  Mario no longer count as enemies; the
  Pac-Man car no longer counts, Sonic does. Not done: the debris that are
  only removed when they land (they are not removed off screen).
* D-76 (the strongman's corpse is engraved), D-78 (the wild Smurf's hide
  timer is its own and its limit is a marker; its first image, chosen
  before the level event sets its facing, has no effect: every update
  chooses the image before the first draw), D-79 (the lightsaber is caught within 10 pixels), D-80 (a star
  turns back at a wall both ways), D-81 (Yoshi's tongue at the same pace
  both ways), D-82 (a dead Tails facing left turns back at a wall), D-83
  (Knuckles' corpse is not engraved on layer 4), D-84 (Rayman's COM hero
  counts as an enemy created), D-85 (Sonic lands on the floor at his x),
  D-87 (the cruiser's laser flash moves), D-88 (Mario's bursts reach the
  screen edges), D-89 (placing the second player leaves the first one's
  animation), D-90 and D-93 (each player stops the scroll through its own
  flag; a player crushed on the last life leaves it free), D-91 (a player
  crushed by the scroll lands on the nearest column with a platform
  instead of falling out of the level), D-92 (a left beam off the screen
  edge is drawn at its visible width), D-94 (the wait after the last
  player's death is reset at each level). D-86 is dead 2002 code, not
  ported: nothing to fix.

Effect on the replays, from the milestone logs of the build before the
fixes (the ASan snapshot of 12:24) and after:

* `attract`, `menus`: identical fingerprints.
* `smoke`, `dork`, `campaign`, `finalboss`, `gameover`, `lemmings`,
  `smurf`: the same events in the same order; frames and fingerprints
  differ. First divergence, traced with `--trace-ticks`: tick 420 of
  snuf1, a basic Smurf one step behind as it starts falling (D-54).
  From there the scripted inputs meet a different game.
* `carebears` and `video` completed their last level earlier (frames
  50,158 and 36,722 instead of 51,051 and 38,316), then their scripts
  confirmed the summary and started the next level; `snorks` and
  `videoboss` ran out of script before the end (snorkniv took about 6,000
  frames more). The four scripts are retimed (`tools/replays/
  make_replays.py`: end frames 50,200, 36,650, 38,200 and 24,000): each
  completes its level at the same frame as with a long script (snorkniv2
  at 38,311, videoboss at 24,141) and ends in the level summary, as
  before.
* The fingerprints were then recorded again (`update_replay_hashes`).

### Game controllers (Phase 7)

* The SDL 2 backend opens a pad SDL recognises (its game controller
  database) as a game controller: its buttons have SDL's numbers on every
  system (`kPadButtonA`... in `codes.hpp`), its D-pad comes as a hat and
  its left stick as axes 0 and 1. Other pads stay joysticks with their own
  numbers. The log names each pad, at start and when plugged in: `Pad 1:
  <name> (game controller)` or `(joystick, own button numbers)`, and
  `Pad 1 removed`.
* Default layout: the first pad plays the first player, the second pad
  the second (`Bindings::defaultPadCode()`): D-pad or left stick to move,
  A jump, X fire, B cow bomb. `InputState::aliasHeld()` accepts it as well
  as the bound key, for the players' aliases, until the player binds one
  of its actions to a pad in the controls menu (then only the chosen
  bindings count). A on any pad counts as Return (menus, character
  select), Start or Back as Escape (pause, back, skip); Start ends the
  name entry.
* The D-pad (or hat) and the stick are kept apart: before, a stick
  resting near the centre sent axis events that released a direction the
  D-pad held.
* The replays contain no pad input, so the fingerprints are unaffected:
  the campaign's final hashes (27,035 frames, frame hash
  9e11dab3f11ad4ac, state hash 31898a8959e4a05b) are the ones measured
  before the change.
* Checked without a pad: a conformance test attaches an SDL virtual game
  controller and checks what the game receives (A and Start with their
  numbers, D-pad directions combined as a hat, the left stick as axis 1,
  removal). It fails when the backend opens pads as plain joysticks: the
  pad is then not reported as a controller and its D-pad comes as
  buttons, not a hat. Unit tests cover the default layout and when it
  applies.
* Not checked: a real controller on each system (the release checklist
  has the steps).

### Verification (24 October 2026, on the final tree)

* macOS, Release: the 13 replays give the recorded fingerprints. Debug:
  unit, conformance, formats, content, naming.french and assets.ids pass;
  clang-tidy reports nothing on the 70 source files changed today.
* ASan with poisoned locals (first run of `BB_POISON_LOCALS`): the 13
  replays give the recorded fingerprints, no sanitizer report.
* Linux (Ubuntu 24.04, GCC 13): 34/34. It first failed: GCC's `-Wshadow`
  rejected the D-90 `Player` constructor parameter `slot`, named like the
  member (Clang accepted it); renamed `index`.
* Windows (MinGW, Wine): 33/34 at first, all replays included. The new
  `naming.french` failed: the test named Python by its CMake target, so
  the cross build ran the host's Python through Wine and lost its result
  ("0 identifiers found", exit status lost). It now names the
  interpreter's path; checked in the cross build and on macOS.

## Phase 8 — Tools (done)

### The level editor `bbedit` (25 October 2026)

The plan's optional editor (§5 Phase 8): `tools/editor`, user guide
[editor.md](editor.md).

* **What the 2002 editor did** (`vc-projects/BB3_level_editor`, read for
  this; 5,757 lines in `BB3_level_editorDlg.cpp`): one 640×480 view without
  zoom; a tool list of 27 entries (platforms with a brush on 8 layers,
  solid and blood walls, every event type, starts, victory, the seven bank
  names); placing with the left button, deleting with the right. No
  selection, move, edit of a placed object, undo, test play, or change of
  the screens or the level's width ("Nouveau" had no handler). Defects
  found in it: a level longer than the editor's screen list overflowed the
  platform and wall arrays; loading kept the previous level's pickups;
  unused event fields were saved uninitialised (the `0xCD` fill and garbage
  the source levels keep as `null`).
* **The new editor**: SDL 2 and Dear ImGui 1.92.9b (vendored,
  `third_party/imgui`). Everything the 2002 editor edited, plus selection,
  moving by dragging, editing any field of a placed event by name (entity
  types by their registry names, flags by name, conditions by meaning),
  duplicate, delete, 200 steps of undo (a stroke or a drag is one step),
  zoom and pan, the screen sequence (the level's width), the markers, new
  levels, and sprite hotspots and names (`sprites.json`). Its document
  model (`level_document.hpp`) has no interface code and its own tests.
* **The 2002 activation rules, checked on the data**: x − 750 / x + 50 for
  enemies (facing), x − 800 for generators (x + 50 when temporary and
  facing right), x − 660 for midground, x − 740 for props, foreground,
  vehicles and pickups (`OnButtonSave`, lines 2564–2953). All 1,141
  positioned events of the 12 levels match; the editor recomputes the
  activation x of an event when it moves and keeps the descending order
  the game reads.
* **Files unchanged when nothing changed**: every source level loads and
  saves byte for byte, and every one of the 56 sprite manifests rewrites
  byte for byte (test `editor`). New events set every named field and
  leave the others at the 2002 fill, so the files keep their form (no
  `"_slot"` keys).
* **Entity images**: the 2002 icons are copies of bank images. The script
  `tools/editor/make_entity_previews.py` compares each icon BMP with the
  bank images of the same size: 64 of the 89 enemy and prop types match at
  0 % difference (0.1 % for `lemmings.water`) once 16-bit rounding (12
  levels per channel) is allowed; a contact sheet of the 64 pairs was
  checked by eye. With a tolerance of 8 only 49 matched, the 15 others at
  6–19 % (rounding). The rest have no icon of a bank image's size or
  differ by 9 % or more. Output: `assets/source/editor/entities.json`,
  which `bbassets` ignores (the pack built with it is byte for byte the
  previous one). Screenshots show the entities standing on the platform
  lines (snuf1 at x 1350).
* **Test play**: new game options `--level <file> <kind>` (one level, one
  player, through the campaign screen's `playOnly()`; quits after it,
  without ending, game over or hi-scores) and `--overlay <pack>` (assets
  of a pack replace the game's: `OverlayPackSource`). F5 writes the level
  alone into an overlay pack (in memory, milliseconds) and starts the game
  on the game's own pack; Shift+F5 packs the whole source tree first
  (40.7 s with a debug `bbassets`, 11.5 s with a release one, measured).
  Checked headless: the overlay with the unchanged snuf1 gives the
  fingerprint of the run without it (`faba298c9854a1c2` after 900 frames),
  an overlay with snuf1's events removed another (`b4e922122537f9b7`); a
  format test checks the overlay serves its level and only that.
* **Its own interface test**: `bbedit --self-test` (ctest `editor.ui`)
  runs with SDL's dummy video driver and drives the interface with SDL
  input events: place an enemy (at the clicked point, activation x − 750),
  undo; paint a platform stroke on layer 0 (columns 1997 to 2103 at 200,
  no other), undo it in one step; drag an enemy by 40 pixels and undo;
  delete one and undo; 16 checks. It found two faults of its own while
  being written (input sent before the layout settled; a Ctrl modifier
  left down, which ImGui turns into a right click on macOS).
  `--screenshot`, `--at` and `--show` save the window for a look.
* **Facts found**: the victory needs flag 1 == value 1 *and* flag 2 ==
  value 2 with the screen's left edge at victory x or further
  (`level.cpp:514`); the game ignores overkill and dynamic-load events and
  a pickup generator's type; a midground decal left of x 320 is dropped at
  once (2002 rule). The editor says so in its inspector.
* **Not done**: editing the decor images, fonts and sounds (their sources
  are PNG, WAV and JSON files for ordinary tools) and the campaign,
  strings and scripts (text files). The editor is not in the packages
  (a developer tool, built with the tools).

### Content validation in the tests (24 October 2026)

* `bbassets` is the one asset tool (extract, pack, verify, legacy, ids,
  ids-check); no other CLI was needed.
* New test `content` (`tests/content/levels_test.cpp`, label `assets`):
  every level of `bb.lst`, read from the data directory and from the
  runtime pack, places only registered entity ids (enemies, generators,
  props, pickups, vehicles), names only existing images (decals), sounds
  and musics of its banks, and keeps the events in descending activation
  order. 10,417 checks pass. Checked that it can fail: without the pickup
  registrations it reports 98 failures (49 pickup events, in both
  sources).
* New test `assets.reproducible`: the pack built again from the same
  sources equals the build's pack byte for byte (checked by hand first:
  two builds of 93,664,468 bytes, identical; the script fails on a
  different file).
* CI runs both (`ctest -LE replay`). Both pass on Linux (GCC 13), after
  a fix: GCC could not compile a `REQUIRE_MESSAGE` on a `shared_ptr`
  (doctest prints the operands; Apple Clang accepted it).
* The optional editor was done on 25 October (above).

## Phase 7 — Desktop release (in progress)

### User files and data lookup (24 October 2026)

* **Hi-scores** live in the user's preferences directory
  (`hiscores.scr`, next to `settings.cfg` and the log) instead of the data
  directory, which is read-only once installed. A first run imports the
  2002 `data/bb.scr` when there is one, as the settings import `data/bb.cfg`;
  without a preferences directory the scores are written to `data/bb.scr`
  as before. Deterministic runs neither read nor write them (unchanged).
  Checked: a first run logged `Imported data/bb.scr (2002 hi-scores)` and
  wrote `hiscores.scr` byte for byte equal to it; the second run logged
  `Using .../hiscores.scr`.
* Fact: the `bb.scr` in `data.zip` is not the default table: its first two
  entries are "KID" 173,194 and " DDD" 138,570 (decoded with the game's
  reader), scores played in 2014 and shipped with the data. A package
  carries the runtime pack, not `data/`, so a fresh install starts from
  the default table; a run from the build tree imports the 2014 scores.
* **Data lookup** (`bb::app::locateGameData()`): without `--data` or
  `--pack`, `./data` when it holds `uk.dat` (the build tree and the
  replays, unchanged), else `blipblop.bbpack` next to the executable, else
  `data/` next to the executable. Checked: the smoke replay run from an
  empty working directory read `build/release/blipblop.bbpack` and kept
  its fingerprints.

### Packages (CPack)

`cmake/BBPackaging.cmake` (the install rules moved there from the root and
`src/app` CMake files):

* macOS: `Blip and Blop.app` (`Info.plist` from `cmake/Info.plist.in`),
  the pack in `Contents/Resources`, the libraries copied and relinked by
  `fixup_bundle` into `Contents/Frameworks`, an ad hoc signature; a disk
  image with the app, `LICENSE` and `README.md`.
* Fact: Homebrew's `sdl2` is now sdl2-compat (2.32.72), which loads SDL 3
  at run time, first from `@loader_path/libSDL3.dylib`; `fixup_bundle`
  cannot see it, so the install step copies `libSDL3` there when the
  bundled `libSDL2` names that path.
* Checked on macOS (x86_64): the installed app and the one on the mounted
  disk image verify with `codesign --verify --deep --strict`; the smoke
  replay run from them read the pack in `Resources` and kept its
  fingerprints; `DYLD_PRINT_LIBRARIES` showed 19 libraries loaded from the
  bundle and none from Homebrew. The image is 56 MB.
* CPack turned `LICENSE` into a click-through agreement on the disk image
  (`hdiutil attach` cancelled); turned off
  (`CPACK_DMG_SLA_USE_RESOURCE_FILE_LICENSE`), the licence is beside the
  app.
* Windows: executable, SDL DLLs (`$<TARGET_RUNTIME_DLLS>`), pack, licence,
  README; NSIS installer and zip. Built by CI since run 40212003833
  (25 October 2026; no Windows machine here).
* Linux: `.tar.gz` of the same folder, SDL from the system. Checked in
  the Ubuntu 24.04 container (GCC 13): the package holds `blipblop`, the
  pack, `LICENSE` and `README.md`; unpacked and run from an empty
  directory, the smoke replay read the pack next to the executable and
  kept its fingerprints. The pack built there is byte for byte the one
  built on macOS.
* CI (`.github/workflows/ci.yml`) packages on each platform and uploads
  the packages.
* **Replays from the packages (28 October 2026).** The definition of done
  (§4.5) asks for the campaign completed from the release packages; only
  the smoke replay had been run from them, by hand.
  `tools/replays/package-replay.sh` installs a package (mounts the disk
  image, unpacks the `.tar.gz` or the `.zip`), runs replays with the
  installed game from an empty directory and compares the fingerprints;
  CI runs `campaign` and `smoke` after packaging on the three platforms
  (Windows: the zip; the NSIS installer needs a desktop session). Checked
  on macOS (x86_64) with the disk image: both match; the log read the
  pack in `Contents/Resources`. The `campaign` replay plays every level
  briefly and wins it with the F9 cheat, then the ending and the credits;
  a full playthrough by a player stays on the release checklist.
* **The macOS disk image did not open on an Intel Mac (29 October 2026).**
  The v1.0.0 release's disk image, built on GitHub's Apple silicon runner
  with Homebrew's SDL 2, held an arm64-only app; on the maintainer's Intel
  Mac (macOS 14.8) Finder said it is "not supported by this Mac". Homebrew's
  libraries also require the runner's macOS version. The workflows now
  build SDL 2.32.10 from its source release (checksum checked) for arm64
  and x86_64 and macOS 11 (`tools/macos/sdl2-universal.sh`) and the game
  likewise (`CMAKE_OSX_ARCHITECTURES`, `MACOSX_DEPLOYMENT_TARGET`). Checked
  on the Intel Mac: the app and its `libSDL2` are `x86_64 arm64`, `minos
  11.0`, the signature verifies, and the campaign and smoke replays played
  from the disk image keep their fingerprints. CI also plays the smoke
  replay with the Intel half under Rosetta when the runner has it
  (`BB_RUN_WITH` of `package-replay.sh`).
* **The Windows packages did not start (found 28 October 2026 by the step
  above, run 40784663938).** The installed game exited with 0xc0000135
  (a DLL not found): the packages held SDL2 and SDL2_mixer
  (`$<TARGET_RUNTIME_DLLS>` lists the direct DLLs only), not SDL2_mixer's
  codecs (`ogg`, `vorbis`, `vorbisfile`, `mpg123`, `wavpackdll` from
  vcpkg), which vcpkg copies next to the built game, so every test of the
  build tree passed. `cmake/BBInstallWindowsDlls.cmake` now copies every
  DLL the game loads, recursively, at install time
  (`file(GET_RUNTIME_DEPENDENCIES)`, system DLLs excluded; an unresolved
  one stops the packaging), and `InstallRequiredSystemLibraries` adds the
  Microsoft C++ runtime they are built against (the CI machine has it
  installed, so only a clean machine would show its absence).

### Windows: cross-build and Wine (24 October 2026)

No Windows machine is available; `tools/docker/windows-cross-check.sh`
builds the project for 64-bit Windows in a container (MinGW-w64 GCC 13,
the official SDL2 2.30.8 and SDL2_mixer 2.8.0 MinGW packages,
`tools/docker/mingw/`) and runs every test under Wine.

* Result: 33/33, the 26 replay runs (data and pack) with the fingerprints
  of macOS and Linux; `bbassets.exe` built the pack under Wine byte for
  byte equal to the macOS one. The zip package (`cpack -G ZIP`) holds
  `blipblop.exe`, `SDL2.dll`, `SDL2_mixer.dll`, the pack, `LICENSE` and
  `README.md`; unpacked, run from an empty directory, it found the pack
  and the smoke replay kept its fingerprints.
* Found and fixed: the executables did not link with GNU ld: SDL2main
  calls SDL2, which SDL's package does not declare, and CMake put SDL2
  first (it lists a shared library once). `BBFindSDL.cmake` now declares
  the dependency on `SDL2::SDL2main`. MSVC's linker does not depend on
  the order, so CI's MSVC build may not have shown it.
* Found and fixed: the fingerprint and trace files were opened in text
  mode, so the Windows build wrote CRLF line ends (the fingerprints were
  the same; the replay test compares lines, `cmp` saw the difference).
  They are binary now: the same bytes everywhere.
* The test scripts take CMake's `CMAKE_CROSSCOMPILING_EMULATOR` (Wine)
  to run the cross-compiled game and `bbassets`.
* Wine fact: a prefix created while `WINEPATH` is set fails ("could not
  load kernel32.dll"); the image creates the prefix first.
* The MSVC build and the NSIS installer passed in CI on 25 October
  ("Completing the plan, continued"). Not needed: the MinGW runtime DLLs
  a MinGW-built package would have to ship (the release builds with
  MSVC).

### Display (24 October 2026)

* The window is resizable (the picture keeps 4:3 with bars) and drawn at
  the screen's resolution (`SDL_WINDOW_ALLOW_HIGHDPI`; on Windows the DPI
  awareness and scaling hints, not yet checked there). Measured on the
  Retina Mac: before, "window 640x480, drawn at 640x480 pixels" (the
  system enlarged it); after, "drawn at 1280x960 pixels" (the log's new
  Display line), still 59 frames per second.
* Fullscreen: the 2002 game ran fullscreen at 640x480, the 2014 port and
  this one only in a window (nothing called `setFullscreen()`). Now F10
  switches (F11 is the /cheat key for the lists), the system's controls
  too, and the setting `fullscreen on|off` keeps the state. Measured:
  "window 1440x900, drawn at 2880x1800 pixels", 59 frames per second,
  the setting saved back. The picture is scaled by 3.75 (not an integer;
  an integer scale is a Phase 9 scaling option).
* "Do not present unchanged frames in menus" was for the CPU: measured now
  10–14 % of a core in the intro and menus (51–77 % in the 2014 port,
  §3.14), and skipping presents would change what the replays
  fingerprint. Not done.
* `docs/release-checklist.md`: the manual checks per platform before a
  release (install, display, audio, controls, focus, user files).
* Checked after these changes: ASan replays 13/13 identical without a
  report; Linux (GCC 13) 33/33; Windows (MinGW, Wine) 33/33.

### Frame pacing (D-75, continued)

A log line now gives the display's refresh rate and the average presented
frames per second. Measured with vsync on and the display reporting 60 Hz:
717 frames per second. Vsync does not hold the frame rate, so the pacing to
60 frames per second now applies with vsync too; measured after: the log
reads "439 frames presented, 59 per second" (rounded down). Replays are unaffected (virtual time).

## Phase 6 — Clean-up and consolidation (done)

### What the plan asked for, checked (24 October 2026)

* **2002 structures and names.** `SuperListe`, the `LGXpaker` singleton,
  the `debug` global, the duplicated animation tables and the clashing
  macros (`SPEED_MAX`, `DELAY_ACCELERATION`, `ETAT_ATTENDS`, lowercase
  `tolerance`) went with the legacy code in Phase 5. A search of `src`,
  `tests` and `tools` finds them only in comments that name the 2002 code
  a port comes from (`ETAT_ATTENDS` -> `kWaiting`, `tolerance` ->
  `kTolerance`). The French file names stay in such comments on purpose:
  they are the key to `docs/legacy_map.md`.
* **`-Werror` everywhere.** The strict warning set (`bb_set_warnings`)
  covers every target; the relaxed set for the legacy code
  (`bb_set_legacy_warnings`) is removed.
* **clang-tidy.** `.clang-tidy` applies to `src`, `tests` and `tools`:
  `bugprone-*`, `modernize-*`, `performance-*`, two cppcoreguidelines
  checks and `readability-identifier-naming` (camelBack functions and
  variables, `k` constants, `_` private and protected members). Left out,
  with the reasons in the file: `[[nodiscard]]` everywhere, C arrays
  (the 2002 constant tables, kept for comparison), smaller enums (the
  file formats and the hashed world state are 32-bit on purpose), `auto`
  for casts, braced returns, string concatenation in error messages;
  `tests/.clang-tidy` also leaves out the optional-access and
  exception-escape checks (doctest's `REQUIRE` and test `main`s). Run with
  LLVM 22, the whole repository gives no warning. Five `NOLINT`s remain,
  each with its reason on the line (a sort of pointers by the
  names they point to, a 2002 format string, the implicit conversion of
  raw pointers to handles, an array built from the services, a test of
  copies).
* **Unit tests** for formats (`tests/formats`), collision, Rng, the clock,
  hashing, the VFS, entity handles, lists and registry, input, the
  hi-scores, sprites and fonts, the screen stack (`tests/unit`).
* **Docs.** [architecture/overview.md](architecture/overview.md) (layers,
  the run, drawing, determinism, assets),
  [architecture/platform.md](architecture/platform.md) (adding a backend),
  [content.md](content.md) (adding an entity, a level, checking with the
  replays), with [cut_content.md](cut_content.md) and
  [architecture/world.md](architecture/world.md) from Phase 5.

### Fixes the checks found

* Names: the file-scope globals of `files.cpp`, the constants of the decor
  and the credits.
* `ground` was assigned inside 17 `if` conditions (2002 idiom); it is a
  `const` initialised before the test.
* Uninitialised locals initialised; `atoi` replaced by a checked
  `strtol` in the cutscene, dialogue and session parsers.
* Identical branches merged (effects, player, a Sonic enemy, the input
  timeline); `Entity` copy assignment guarded against itself;
  `bbassets` reports exceptions instead of terminating.
* Linux (GCC 13) failed on `hud.cpp`: the level timer was printed into a
  10-byte buffer, too small for a 10-digit `int` plus its terminator
  (`-Werror=format-truncation`). The buffer is 12 bytes; every value that
  fitted prints the same.
* Tests: integer products widened before use as sizes and offsets.

None of these changed a fingerprint: the thirteen replays are identical
after each step.

Checked at the end of the phase: Debug build, `ctest --preset default`
31/31 (the 26 replay runs, data and pack, among them); ASan replays 13/13
identical without a report; Linux (GCC 13) 31/31.

### D-75 fixed early (a Phase 7 item)

The VSYNC option works again (see migration.md D-75): the window is created
with it, the menu toggle applies at once, and without vsync the
application paces the presented frames to 60 per second (measured
16.6 ms per frame). With vsync on a 120 Hz screen the menus run at 120
frames per second, as they have since 2014; settled in Phase 7 (pacing
with vsync too).

### Left

* `.github/workflows/ci.yml` has never run (the directory has never been
  pushed); its static-analysis job now runs clang-tidy on `src`, `tests`
  and `tools` and cppcheck on `src`. cppcheck is not installed on the
  development Mac, so its findings are unknown until CI runs.
* Resolved: CI has run since 25 October 2026 (run 40137765992 first, all
  jobs green from run 40212003833).

## Phase 5 — Content migration (done)

### Deviations from the plan (recorded 24 October 2026)

* **DEC-08 helpers**: no `AnimationPlayer`, `Hitbox` or `Health` class was
  written. The classes keep the 2002 shape (an `update()` per mode over
  `Actor`'s `frame`/`subFrame`, `animate()`, `hitboxFromImage()` and
  `hitPoints`); only the Pokémon walkers and heroes became data
  (`WalkerSpec`, `HeroSpec`, 7 specs), far from the ~38 enemies the audit
  estimated could be.
* **Named sprites and sounds**: the code names every image and sound it
  uses, as the first of a run plus a step (`kSmurfWalkRight + step`);
  600 of the 3,789 images and 63 of the 175 sounds carry a name, the rest
  are reached through those offsets.
* **Known bugs**: kept so that the replays stayed identical through the
  batches, then fixed together with one re-recording ("Completing the
  plan", above), except the debris of D-55.
* **Level data**: the absolute positions (5d, 5f, 5g) became level markers
  and the registers got per-world names in the same round, not in their
  batches. The Lemmings 200-kill rule stays in code (`kNukeKills`), and
  the video bosses' register 0 is a named register, not the "named level
  script" the table asked for.

### Exit criteria (24 October 2026)

* All 12 levels replay identically on the new framework: the thirteen
  replays (attract, menus, smoke, campaign, game over and one per world)
  keep the fingerprints of their frames and of their world state, on
  macOS; Linux and ASan are checked by the snapshot checks below.
* The legacy game code is deleted: `vc-projects/Blip_n_Blop_3/` holds only
  `data.zip`. The 2002 code is in the git history (commit `44cab36`).

### Batch 5o, the application, done

* `src/app` is the application: `App` (`app.cpp`, the 2002 InitApp,
  PlayGame and ReleaseAll, and the glue of the legacy port) owns the
  window, the back buffer and the backdrop, the World, the decor, the
  registry, the banks and fonts, the string table, the options, the audio
  output and the hi-scores, fills `game::Services` and runs the screens;
  `session` (record, replay, fingerprints; it now owns the quit flag and
  remembers the /cheat and /quiet switches), `DeviceInput` (the keyboard
  and the pads, `game::Input`), `settings_file` (the settings and the
  one-time import of `bb.cfg`), `StringTable` (`game::TextSource`; 2000
  ids, the credits use 1000 and up) and `files` (the data root, the asset
  source).
* Gone with the legacy directory: the adapters (`Surface`, `Picture`,
  `PictureBank`, `Fonte`, `SoundBankBB`, `MusicBank`), the `debug` stream,
  the globals of `legacy_assets`, `legacy_world`, `legacy_media`,
  `legacy_registry`, `legacy_io`, `config`, `txt_data`, `video`, `input`,
  `session`, `blip_n_blop_3.cpp` - and `zero_new.cpp`: measured first, the
  thirteen replays keep their fingerprints with an allocator that fills
  every allocation with `0xA5`, so no code depends on zeroed memory any
  more (Phase 0 needed it for the 2002 classes, D-69).
* The data extraction (`legacy_data`) is in the root `CMakeLists.txt`.
* All new and changed sources of Phases 5j to 5o are formatted with the
  repository's `.clang-format`.


### Batch 5b, Pokémon bonus level (`dork`)

* **Golden master**: replay `dork` (ctest 19 tests) wins the level through
  its script without the win-level cheat: switch 1 starts lift 1, the
  first bomb is on the ledge it reaches; ledges 362, 296 and 226 lead to
  switch 2 and lift 2, then bomb 2; switch 3 (reached with a somersault)
  drops the crate, whose platforms lead to bomb 3; then every enemy is
  killed. All five enemy types (ids 100–104), the 12 prop types and the
  crate's platforms run in it. Facts measured on the way: a jump rises
  105 pixels when the key is held and 78 when it is released after 8
  frames (gravity doubles while rising without the key); jumping again
  in the air (somersault) reaches about 166; a dead player comes back at
  the same x about 80 frames later, falling through platforms to the
  first solid one; once the three bombs are defused the bomb resets the
  timer every tick (it stops at 39), and the level is won when no enemy
  is left.
* **Ported** (`src/game/src/pokemon/`, `registerPokemonWorld()`): the
  three walking Pokémon are one data-driven `Walker` (`WalkerSpec`: speeds,
  images, death and burn timings, blood, sound; kept quirks: Charmander
  looks 1 pixel ahead but walks 2, and its blood timer never counts down,
  so it bleeds once), `Com` and `FlyingCom`, and the 12 props: `Alarm`,
  `Arrow`, `SkyWindow`, `NeonSign` (both signs), `Lift` (both lifts),
  `Switch`, `LockedCrate` and the `CratePlatform`, `SwingingPlatform` and
  `FallingCrate` it creates, `Bomb` (ids 109 and 900). 34 files of the 2002
  classes are deleted. All seven replays keep their fingerprints, and the
  `dork` trace shows the same entity counts per class as the 2002 code
  (for example 30,194 COM ticks in both).
* **New framework pieces**: `MovingPlatform` (writes its platform heights,
  carries players like the 2002 lifts), `CollisionMap::setPlatform()`
  (clipped to the level), `Canvas::drawPart()` (opaque sub-rectangle, for
  the sky window), `LevelEffects::showBanner()` (banner texts from ported
  code), a tick delay for `Enemy::burn()`, `Actor::levelImage()`.
* **Found while porting**: `game.cpp`, `grave()` and the monolith cast the
  prop and moving-platform lists (and the enemies, for the monolith) to
  `Sprite*`; with ported props in them that is undefined behaviour (UBSan
  reported it, the Release build crashed on the first `dork` tick). Those
  loops go through `Entity` now. `grave()` iterates the prop list with its
  shared cursor, so a prop that engraves during the prop update ends that
  update loop early (2002 behaviour, kept: the falling crate does it).
* **Defects fixed without changing the fingerprints**: D-41 for the ported
  props (platform writes are clipped), D-42 for the bomb (a fourth bomb
  gives no pickup instead of using an uninitialised pointer), D-50 for
  `save_time` (the bomb defused last holds the timer), D-52 for these
  props (the registers have names).
* **Verified** (15 October 2026): Release unit tests (46 cases) and the
  seven replays; ASan + UBSan on the seven replays (no finding); Linux
  (Docker, GCC) `ctest` 19/19. GCC's `-Wshadow` flagged one more parameter
  named like a member (`MovingPlatform::carryPlayers`); renamed.

### Named images and sounds (Smurf and Pokémon worlds)

* `bbassets ids` now also names sounds (`sounds.json` `"name"` →
  `bb/assets/sounds/<bank>.hpp`), and `extract` keeps sound names like
  sprite names (checked by re-extracting over the named manifests).
* Named in the manifests (from how the code uses each index): 78 images
  of `snufenn` (Smurf enemies), 18 of `pokeenn` (Pokémon enemies), 28 of
  `dorkfonda` (dork props); 8 sounds of `smurf`, 3 of `dork`, 2 of `misc`.
  The ported classes use only these constants (animation tables became
  steps from a named first image); the one number left is the strongman's
  music track (music banks have no names yet). The fingerprints did not
  change; `assets.ids`, `assets.verify` and `formats` pass.
* `assets/source` is still not committed: the names live in its manifests
  (and in the committed headers, which `assets.ids` compares).

### Batch 5n, the screens and the campaign, done

* **Services** (`bb/game/services.hpp`): what a screen works with - the
  back buffer and the backdrop (the 2002 `systemSurface`, shared by the
  menus and the level summary), `Input` (update, keys, aliases, "any key",
  the first key held, the focus), `Session` (milestones, clock, quit,
  deterministic runs, the per-tick fingerprint), `Options` (vsync,
  cheats), the asset source, the audio output, the strings, the fonts,
  all the banks (`Banks`), the world, the decor and the hi-scores; and the
  loaders `loadSprites()`, `loadFont()`, `loadSounds()`, `loadMusic()`.
  The legacy code implements `Input` and `Session` and fills one
  `Services` (`connectLegacyWorld()`) until 5o moves that to the app.
* **Ported**: `HiScores` (the `strcpy` byte layout of `bb.scr` kept: its
  checksum counts the bytes after the names), `CutscenePlayer` and
  `DialoguePlayer` (scripts parsed exactly as in 2002), the waiting
  screens, `Menu` with `MainMenu` and `PauseMenu` (the two 2002 copies
  share their pages, rebinding and drawing; the differences kept: the
  main menu reads the input twice, takes left and right together, goes
  back to different items), the character choice, the credits, the title,
  the hi-score table and entry, game over, the level summary (one routine
  for both players), and `bb::game::Game` with the attract, campaign,
  level, pause and dialogue screens. The interface bank's images are
  named (`inter`).
* **Dropped, no visible effect**: the cutscenes' alpha fade, layer colours
  and volume (their drawing was commented out), the French title of the
  character choice and French key names (the language is forced, D-25).
* **Deleted**: `game.cpp/.h`, `cine_player.*`, `rpg_player.*`, both
  menus, `key_translator.*`, every screen but `runScreens()`, and the dead
  `event.h`, `fic_events.h`, `trace.h`, `ben_maths.*`, `lgx_packer.*`;
  `legacy_world` is down to `g_world`. Unit tests 58 (hi-scores, audio,
  sprites and fonts). All thirteen replays keep their fingerprints.

### Batch 5m, audio and the players' controls, done

* **Audio**: `bb::game::SoundBank` and `MusicBank` (`src/game/src/audio.cpp`)
  on the platform `Audio`, through `AudioOutput` (the device and the two
  switches; the legacy `sound_on` and `music_on` are references to them).
  The rules kept: a looping sound stops when all its looping plays are
  stopped; a stop without a voice does nothing; one music at a time.
  Unit tests (55) check them with a fake device (the replays do not hash
  sound). The legacy `SoundBankBB` and `MusicBank` are adapters that look
  the banks up in the asset source; `sound.cpp` keeps only the output,
  `sound_bank.cpp/.h` are gone. `bb_game` links the platform interfaces.
* **Controls**: `bb::game::InputState` (what the game reads of the input,
  by code or alias) and `AliasControls` (a player's seven aliases); the
  legacy `Input` implements `InputState` until 5o moves it to the app.
* All thirteen replays keep their fingerprints.

### Batch 5l, rendering, done

* **Core**: `bb::core::gfx::Sprite` (a pixmap and its hotspot) with
  `draw()`, the 2002 `Picture::BlitTo` clipping (nothing beyond the
  640x480 screen; cut at x 640 and y 480 even on the 840-pixel ring),
  `SpriteBank`, and `BitmapFont` (`Fonte`: print, right-aligned, centred,
  wrapped, word-wrapped, width); unit tests 52. `BlitTo` and `PasteTo`
  drew alike (the SDL port's `BltFast` ignores its flags; the colour key
  applies whenever the image has one).
* **Game**: `Image` and `ImageBank` are `Sprite` and `SpriteBank` (no
  virtual layer); `bb::game::Decor` (`src/game/src/decor.cpp`) is the
  840-pixel decor ring of `scroll.cpp` and the decal engraving of
  `grave()`, and implements `LevelEffects`; `Level::drawWorld()` draws the
  decor, the lists in the 2002 order, the distortion and the screen shake,
  `Level::drawOverlays()` the GO arrow (`misc` image named `go_arrow`) and
  the banners. Covered by the replays: rain (snuf1), snow (bisous3), the
  distortion (both Snork levels), the shake (cow bombs).
* **Legacy**: `Picture` is an alias of `Sprite`, `PictureBank` and `Fonte`
  are thin adapters until their screens are ported (5n); `picture.cpp`,
  `scroll.cpp/.h` gone, with 20 draw methods of `Game`, `videoA`,
  `num_decor` and `grave()`. Dead code dropped: the smaller scroll buffers
  (`createSurface()` never fails, so the ring is always 840 wide) and the
  dialogue's 640-pixel-buffer position.
* **HUD**: `bb::game::Hud` (`src/game/src/hud.cpp`), with the fonts of
  `Media::fonts` and the strings of `Media::texts` (`TextSource`);
  `Canvas` is a concrete class of the game on the back buffer (its
  "opaque" and "keyed" part copies were the same copy: one `drawPart()`).
  `bb` images named `life_segment`, `life_segment_lost`. The replays cover
  both HUD layouts (one two-player replay) and both heroes' fonts.
* Still on the legacy `Surface`: the screens (briefing, loading, menus,
  cutscenes, dialogues), ported in 5n.
* All thirteen replays keep their fingerprints (world and frames).

### Batch 5k, the level simulation, done

`game.cpp`, the menus and the screens count in Phase 5 (decision of
23 October 2026); 5k to 5o port them. 5k: the per-tick part of `Game`
(the update order of every list, the camera of `updateScrolling()`, the
scroll lock, hold fire, the flags and the level timer, the target, the
screen shake, the bubbles, the GO arrow, the distortion phase, the cheats,
the collisions, the clean-up, victory and defeat, the world-state
fingerprint) is `bb::game::Level` (`src/game/src/level.cpp`), reused from
level to level like the World; `Game::tickBegin()` reads the input and
the cheat keys and calls `Level::tick()`. Loading, drawing and the
campaign stay in `game.cpp` for 5l to 5n. 29 methods of `Game` and
`updateScrolling()` gone. Found and kept: D-94. All thirteen replays keep
their fingerprints.

### Batch 5j, players, their shots, pickups and effects, done

* **Ported** (no new replay: all thirteen keep their fingerprints):
  `bb::game::Player` (`src/game/src/player/player.cpp`) for Blip and Blop,
  with the weapon tuning in one `WeaponSpec` table (the 2002 code repeated
  it in five places) and `PlayerControls` (legacy `LegacyControls` on the
  key aliases); the shots `RifleBullet`, `SmgBullet`, `ShotgunPellet`,
  `LaserBeam`, `Flame` and the cow bomb's `FallingCow` (`shots.cpp`; the
  pools and the shared shotgun spread counters kept); the eight pickups
  (`pickups.cpp`, `registerPickups()`); `BloodSpray`, `Bubble`, `Raindrop`,
  `Snowflake` and the `Precipitation` pools (`effects.cpp`); `Banner`
  (`banner.cpp`, rows in `World::bannerRowFree`). `LevelEffects` keeps only
  `engrave()`; `Canvas::drawText()` draws the banner font. Named: 17 images
  of `bb`, 18 of `blip` and `blop`, 7 of `misc`, 6 sounds of `bb` and 6 of
  `misc`. 50 legacy files deleted (69 source files left in
  `vc-projects/Blip_n_Blop_3`), among them the 2002 bases `Sprite`,
  `Personnage`, `Tir` and `Ennemi` (nothing derived from them any more);
  `Marker` stands in for the target when no player is left and for the
  decal box of `grave()`.
* **Found**: `game.cpp` and `scroll.cpp` still cast list entries to
  `Sprite *` (players, cows, weather, impacts, bubbles): undefined once the
  classes are ported; UBSan reported it in `lemmings`, where the release
  build crashed. All go through `Entity` now. Kept: D-89 to D-93. Dropped,
  as dead code: the vehicle speed added to the shots (always 0: only set
  for a rider, who never fires), the rider's "fire locked" flag (never
  read), the controls' inertia switch (always on), the flamethrower's
  third flame type (`random() % 2` never chose it).
* **Checked**: per-class trace counts over the thirteen replays equal the
  2002 build's (players 328,244 = `Blip` + `Blop`; weapon pickups 418,325
  = the four 2002 classes; every shot class identical). The invincibility
  pickup appears in no replay: its port is checked by reading only.
* GCC `-Wshadow` (Linux leg) rejected three constructors of 5g and 5h
  (`Plunger`, `CruiserShot`, `CruiserLaser`) whose parameters were named
  like members; renamed (clang does not warn).

### Cut content removed (a Phase 6 item, done during Phase 5)

* The content no shipped level reaches — the heart bear (206) and the car
  bear with their shots, gore and explosion, Diabolo (306) and its
  tornado, Pac-Man, the star prop (705), the fish (302–305), the airlocks
  (300, 301) and three test headers — is deleted (25 files) and removed
  from `legacy_registry.cpp`; `docs/cut_content.md` describes each piece,
  why it was unreachable and what bringing it back would take (with the
  Mario hologram, the seahorse vehicle and the test turret of 5h and 5i).
  Checked: the thirteen replays keep their fingerprints (ctest 31/31,
  Release).
* Left in `vc-projects/Blip_n_Blop_3`: 118 source files (423 at commit
  `44cab36`): the players (`couille.*`, `blip.h`, `blop.h`), their shots
  (`tir_bb*`), the pickups (`bonus_*.h`), the cow bomb, `TexteCool`,
  `Giclure`, weather, `game.cpp`, the menus and screens, and the engine
  glue (input, sound, session, assets adapters).

### Batch 5i, the level machinery: generators, events, vehicles, done

* **Ported** (no new replay: the thirteen replays already run every one
  of these; they keep their fingerprints): the last five level event types
  in `makeLevelEvent()` — enemy generators, pickup generators, banner texts
  (with their flag condition), music, vehicles (a registry category of
  their own, placed in `World::vehicles`); `EnemyGenerator` (normal and
  temporary) and `PickupGenerator` with the nine pickup sets
  (`src/game/src/generators.cpp`; the sets' two counters and the pickup
  set, `MakeBonus`'s statics and `makeb_current_mode`, are `World` fields);
  `Generator::update()` (the game loops no longer cast);
  `bb::game::Vehicle` (`src/game/src/vehicle.cpp`) with the `Rider` role,
  which the 2002 player implements, and the stork (`finalboss/stork.cpp`;
  its shared "first drop" static kept, it decides where the rider lands).
  `current_zik` is `World::currentMusic`; `MusicPlayer::stop()`;
  `LevelEffects::dropWeaponPickup()` is gone (the pickups are ported
  code). The seahorse vehicle and the test turret, never reachable, are
  archived in `docs/cut_content.md` with Mario's hologram. 27 files
  deleted; unit tests 48 (banner condition, enemy generator).

### Batch 5h, final boss (`finalboss`), done

* **Golden master**: replay `finalboss` (ctest 31 tests): the first eleven
  levels won with F9, then the final boss level won by the generic driver
  alone (measured: Mario's 50,000 HP fall steadily, the attack sets change
  at 17,500, 11,500 and 8,500; the level is complete at frame 32,086).
  Ends as the ending starts (the campaign replay covers the ending).
* **Ported** (`src/game/src/finalboss/`, `registerFinalBossWorld()`):
  `Mario` with his 16 attacks, `Fireball`, `FireballUp` (`mario.cpp`);
  `BattleCruiser`, `CruiserShot`, `CruiserLaser`, `HyperspaceRock` and the
  three floating platforms as `Floater` (`decor.cpp`). Found and kept:
  D-87, D-88. Named: 22 images of `finalbossenn`, 18 of `finalbossfonda`
  (the level has no sound bank). The stork vehicle, the star prop (705,
  never placed) and the Mario hologram (never spawned) stay for later.
  All thirteen replays keep their fingerprints; the trace shows the 2002
  counts per class.

### Batch 5g, video-game boss level (`videoboss`), done

* **Golden master**: replay `videoboss` (ctest 29 tests). Measured: with
  the generic driver the Lara fight never ends (traced: stage register 4
  for 20,000 frames): Lara has no hitbox and dies only when the weight
  (hitbox y −79 to 101, at x 565) is pushed past x 950 by shots going
  right (2002 directions 2 to 6, 3 pixels per hit), within the 180-second
  timer set at stage 3. The ledges of the locked screen (camera 417) are
  at y 322, 218 and 105 (x up to 479–483); from the top one a shot going
  right is level with the weight. `lara_weight` walks to the left edge,
  climbs with three somersaults, faces right and fires: Lara is crushed at
  frame ~10,540 (from 9344, stage 4). The generic driver does the rest
  (small Mario, Luigi, Sonic, the car); level complete at frame 23,663.
* **Ported** (`src/game/src/videoboss/`, `registerVideoBossWorld()`):
  `Lara`, `Spikes`, `Weight`, `RaymanHead`, `LaraSpine` (`lara.cpp`);
  `SmallMario` and `Luigi` on `Plumber`, which holds their identical
  push-back code, and `Plunger` (`plumbers.cpp`); `Sonic`,
  `SonicLightning`, `SonicRing`, `PacManCar` (`sonic.cpp`); 12 props as 6
  classes (`decor.cpp`); `GoreDrop` (`src/game/src/gore.cpp`, shared with
  the 2002 cow bomb). Framework: `Shot::heading()` (the 2002 shot
  direction, which the weight and the plumbers read),
  `Canvas::drawPartKeyed()` (the spikes draw only their risen part). Found
  and kept: D-85; not ported, dead code: D-86. Named: 50 images of
  `videobossenn`, 15 of `videofondaboss`, 1 sound of `boss`, 2 more images
  of `misc`. 41 files deleted (Pac-Man himself stays for Phase 6: never
  spawned). All twelve replays keep their fingerprints; the trace shows the
  2002 counts per class.

### Batch 5f, video-game world (`video`), done

* **Golden master**: replay `video` (ctest 27 tests): the first nine
  levels won with F9, then video won with the generic driver. Measured:
  the driver walks under the Yoshi turret standing on the ledge at x 1417
  (y 147, 400 HP) without killing it, and the scroll lock at x 1536 waits
  for every enemy, so the camera stopped there for good (traced: the
  turret alone left from frame ~10,500). The script stops under it at
  frame 9635 (x ≈ 1414) and fires up for 600 frames, then lets the driver
  go on. The end arena (camera 10388) is reached at ~28,800; Rayman (15000
  HP, invulnerable while the four Pokémon heroes he summons live) falls at
  ~38,000; level complete at frame 38,316. Recorded with the 2002 classes.
* **Ported** (`src/game/src/video/`, `registerVideoWorld()`): `Toad`,
  `Yoshi`, `YoshiTurret`, `Tails` (ids 504 and 508, the flying one enters
  from the top), `Knuckles` (its own heavier gravity), `Shell`
  (`walkers.cpp`); `Princess`, `PrincessGuard` (`princess.cpp`; the guard
  holds a `Handle`, D-04; banners through `LevelEffects::showBanner()`,
  the cow bomb pickup through the registry); `Rayman`, `RaymanFist`,
  `RaymanLimb`, `RaymanHead`, `PokemonHero` with four `HeroSpec`s
  (`rayman.cpp`); `SyncedProp` (question block, coin, ring: the 2002 class
  statics became one clock per kind) and `LoopProp` (sunflowers, flag).
  Kept: Toad's "brain hurts" file statics (they set the convulsion count,
  which the fingerprints see; D-50), Rayman's arena positions (D-51). Found
  and kept: D-81 to D-84. Named: 83 images of `videoenn`, 8 of
  `videofonda`, 9 sounds of `video`, 2 more of `misc`. 45 files deleted
  (`EnnemiTeteRayman` stays: Lara throws it in videoboss). All eleven
  replays keep their fingerprints; the trace of `video` shows the 2002
  counts per class (summed where classes merged).

### Batch 5e, Lemmings bonus level (`lem`), done

* **Golden master**: replay `lemmings` (ctest 25 tests) wins the bonus
  level, which needs 200 kills within its 100-second timer: the lemmings
  come from generators that wake with the camera (slow ones at the start,
  one lemming per 40 ticks only beyond the pit), so the script fights at
  each scroll lock with cow bombs (F2 then Space), walks on, and crosses the
  pit (x 1443–1568, 125 pixels) with a somersault, since a plain jump
  covers about 140 pixels. Facts: on a bonus level the player has one life
  (`game.cpp:368`), so falling into the pit fails the level; after the
  200th kill every lemming blows up, the last one drops a cow bomb pickup
  and sets register 0, and dialogue 1 sets registers 3 and 1 (victory).
* **Ported** (`src/game/src/lemmings/lemmings.cpp`,
  `registerLemmingsWorld()`): `Lemming` (ids 400 and 401: the flying
  lemming's 2002 constructor only set `y`, which the spawner overwrites),
  `Fan`, `FlickeringLight` (props 401 and 403), `Water` and `WaterTile`.
  Kept: the file statics that count deaths (every 20th plays the rare
  cry, the others draw a random one, so they steer the random stream) and
  animate the water, shared and kept across levels as in 2002 (D-50); the
  water lays its tiles from x = 0 because its 2002 constructor ran before
  its position was set. Named images (`lemenn`, `lemfond`) and sounds. 9
  files deleted; all ten replays keep their fingerprints and the trace
  shows the 2002 counts.

### Batch 5d, Snorks (`snorkniv`, `snorkniv2`), done

* **Golden master**: replay `snorks` (ctest 23 tests): the first six levels
  won with F9, then both Snork levels won by the generic driver alone
  (snorkniv complete at frame ~19,300; snorkniv2, with the Snork in Black
  boss from ~28,000, at ~35,800).
* **Ported** (`src/game/src/snorks/`, `registerSnorkWorld()`): a `Trooper`
  base for the four walking Snorks that shared one loop by copy (`Grunt`,
  `Gunner`, `Governor`, `SnorkInBlack`), `Mage`, `SeahorseRider`,
  `SnorkBoss` (four perches and a jump table from the 2002 code, absolute
  positions kept); shots `GruntShot`, `GunnerShot`, `GovernorShot`,
  `MageShot`, `Spine`; debris `GruntHead`, `GruntHeadBack`, `GruntTrunk`,
  `RiderBody` (in the blood-spray list, as in 2002; `game.cpp`'s loops on
  that list go through `Entity` now). 83 images of `snorkenn2` and 15
  sounds of `snork` named. 25 files of the 2002 classes deleted
  (`ennemi_snork_base_1.h` was an orphan copy); `TirEpine` stays for Lara
  and Sonic (5g); Diabolo (306) and the airlocks and fish props stay 2002
  code until Phase 6 (never placed). All nine replays keep their
  fingerprints; the `snorks` trace shows the 2002 per-class counts.
  Kept 2002 quirks: the Snork in Black never counts down its blood timer;
  a grunt's head slows down when it lands only when rolling right (the
  left test is `x < 0`); the charging seahorse checks a 1-pixel step on the
  right and an 8-pixel step on the left.

### The Big Smurf (id 8), finished

Requested 15 October 2026; the user chose recoloured Smurf art, a boss at
the end of snuf2 and gifts for his ranged attack.

* **Art**: `tools/art/big_smurf.py` (with `tools/art/pngrgba.py`, no
  dependency) recolours the basic Smurf's walk, jump and death frames (red
  hat and trousers, white beard; small grey regions such as eyes and teeth
  kept); 34 images appended to `snufenn` (356–389, named `big_smurf_*`).
* **Data**: his event (enemy 8 at x 5180, y 432, facing left, activation
  4430, the editor's rule for enemies facing left) in `snuf2.json` and,
  through `tools/data/fix_legacy_data.py` (Phase 5 section, idempotent;
  image records in `tools/data/big_smurf_images.bin`), in `data.zip`.
  `bbassets verify`: source and data agree.
* **Code**: `BigSmurf` and `LobbedGift` (`src/game/src/smurf/big_smurf.cpp`)
  on the 2002 skeleton (target switching, approach-and-flurry, keeping his
  distance) with what was missing: the entrance (waits off screen until
  register 1 = 5, the five strongmen dead), the gift lob (three, then a
  flurry), the death (adds 1 to register 1), 5,000 HP, contact damage 2
  during the flurry. The stork now waits for register 1 = 6.
  `ennemi_grand_smurf.*` deleted.
* **Replays**: `smurf` now plays the fight (he enters at frame ~15,600 and
  dies at ~22,300; snuf2 complete at 22,875; the replay ends in its
  summary). `smoke`, `campaign`, `gameover` and `dork` were re-recorded
  too: they pass through snuf2, whose sleeping-event count (fingerprinted)
  is one higher; their screen milestones are identical frame for frame
  (first difference at tick 1320, the first checkpoint after snuf2 loads).

### Batch 5c, Care Bears (`bisous1`–`bisous3`), done

* **Golden master**: replay `carebears` (ctest 21 tests) wins the three
  levels without the win-level cheat (snuf1, snuf2 and dork are won with
  F9 first). A generic level driver replaces hand-measured scripts:
  `explore_cycle` (forward with fire, short jump, aim up, diagonally up
  and down, drop through the platform with down twice quickly, confirm,
  extra life; every fourth step a sweep back with full jumps) completes
  bisous1 (frame ~11,700) and bisous2 (~17,800); in bisous3 the shaman
  (11,000 HP, teleports between three spots depending on where the target
  player is) is fought with `boss_sweep` (short steps along the arena
  floor, firing up), and the level is complete at ~51,000. Facts found on
  the way: the player drops through a platform when down is pressed twice
  within 10 ticks (`couille.cpp:600`, `time_down`); holding down aims the
  gun down. The replay exercises the nine placed enemy types, the six
  shots and the six gore pieces.
* **Ported** (`src/game/src/carebears/`, `registerCareBearWorld()`): the
  sky and star bears share a `HalvedBear` base (their whole death: cut in
  two, the legs fall as gore, the top falls and is engraved), `BoomBear`,
  `CloudBear`, `ZombieBear` (its own hit pipeline, kept: kills skip wall
  blood and weapon pickups), `Shaman`, `Jedi` and `JediHero`, `SnorkCameo`;
  shots `Rainbow`, `BouncingStar`, `StormCloud`, `Lightsaber` (a looping
  sound stopped by its destructor), `ShamanBall`, `ShamanColumn`; gore
  `WalkingLegs`, `DancingLegs`, `BoomPiece`. Named images (94 of
  `bisouenn`) and sounds (`bisou`, `misc`). 30 files of the 2002 classes
  deleted; the heart bear (206) and the car bear, never placed, stay 2002
  code until Phase 6 decides on cut content. All eight replays keep their
  fingerprints; the `carebears` trace shows the same per-class counts as
  the 2002 code (365,606 zombie ticks in both). `game.cpp`'s gore loops
  go through `Entity` now (they cast to `Sprite*`).
  Kept 2002 quirks: the star bear advances its shot counter twice per tick
  and looks 40 pixels beyond the right edge of the screen; the zombies'
  moan counter is shared and survives levels (only the sound depends on
  it); the shaman picks his attack with `sqrt` (exact in IEEE arithmetic,
  so platform-independent); new defects D-79 and D-80.
* `tests/formats` counted 3,755 images in the shipped banks; it now expects
  3,755 + 34 (the Big Smurf). The failure showed only on the Linux run,
  because the macOS format tests had run before the data change.

## Phase 4 — World and entity framework (done)

`globals.h` and `SuperListe` are gone; a typed `World` owns the level;
the Smurf world runs on the new framework with identical fingerprints,
next to the 2002 classes, behind one entity registry (exit criteria of
migration.md). Architecture: [docs/architecture/world.md](architecture/world.md).

### Done

* **Golden master for the Smurf world**: replay `smurf` plays snuf1 and
  snuf2 to the end without the win-level cheat (extra lives and ammunition
  only) and stops on the next briefing. ctest has 17 tests.
* **Typed lists** (`bb/game/entity_list.hpp`, unit-tested):
  `EntityList<T>` replaces `SuperListe` with the same iteration rules (the
  newest entity first, one cursor per list, an entity added during an
  iteration is seen by the next one, removal moves to the next entity).
  Lists own their entities or borrow them (players, player shots, weather
  particles, moving platforms, events moving between lists). **D-01 is
  fixed**: removed entities are deleted (destructors run) instead of
  `free()`d, and the lists are emptied before the level banks close
  (`TirEpeeLaser` stops its sound in its destructor). The 2002 code also
  never released the entities left at the end of a level (`vide()` freed
  the cells only); they are deleted now. ASan and UBSan: no finding on the
  six replays.
* **World** (`bb/game/world.hpp`): one `bb::game::World` owns the level
  state of `globals.h`: camera, scroll lock, hold-fire, the 11 flags (named
  `Flag` enum), `CollisionMap` (built from `LevelData`; `plat`, `plat2`,
  `mur_opaque`, `mur_sanglant`), weather, screen shake, blink phases,
  counters, and the 21 lists. **`globals.h` is gone**: the legacy code
  reaches the World through `legacy_world.h` (references named after the
  2002 globals, removed as classes are ported) and the assets through
  `legacy_assets.h` (surfaces, banks, fonts, controls). `scr_offset` was
  never used and is dropped; `cowBombOn` is campaign state, not level
  state.
* **Entity** (`bb/game/entity.hpp`): the base of every entity; `Sprite`
  derives from it. It holds what the world reads on every entity: the
  position, the hitbox (`x1`..`y2`, `collides`, formerly `col_on`) and
  `dead` (formerly `a_detruire`, 249 uses), plus `update()`, `draw()` and
  `state()` (the fingerprinted animation and character state). The world
  fingerprint goes through it. Events, generators and banner texts derive
  from `LevelEvent`, `Generator` and `BannerText`. The 2002 helper
  `draw(x, y, pic)` is now `drawPicture` (148 calls), because the member
  `draw()` hid it.

* **Media and effects interfaces** (`bb/game/media.hpp`): `Image`,
  `ImageBank`, `SoundPlayer`, `Canvas`, `LevelEffects` (decal, blood spray,
  weapon pickup). `Picture`, `PictureBank` and `SoundBankBB` implement them;
  `legacy_media.cpp` adds the canvas and effects adapters and connects
  `g_world` at start-up (`connectLegacyWorld()`). The World also holds the
  session's random generator, the target player (`target`, kept in step
  with `tete_turc`) and three hidden 2002 counters that carry over from
  level to level: the target turn (a function static), the treasure points
  towards the next weapon pickup and the blood decal index (file globals of
  `enemy.cpp`). `Giclure`'s owner is any `Entity`.
* **Roles and collisions**: pure interfaces `Shot`, `ShotTarget`,
  `Hurtable`, `Collectable` (no data, so no diamond); `Tir`, `Ennemi`,
  `Couille` and `Bonus` implement them. `Entity::hits()` replaces the
  virtual `Sprite::collision()` (the laser overrides it).
  `World::resolveCollisions()` replaces `Game::manageCollisions()` pair for
  pair, in the same order.
* **Generic loops**: the update, draw and clean-up loops of the enemy and
  enemy-shot lists, the bubbles (`wait_bulle` is now `Entity::bubbleDelay`)
  and `cleanLists()` go through `Entity` only; generators and banner texts
  have `finished()`. Moving platforms are now owned by their list: the
  2002 code never freed them (they are only in that list).
* **Base classes for ported content** (`bb/game/actor.hpp`,
  `src/game/src/actor.cpp`): `Actor` (Sprite), `Character` (Personnage),
  `Enemy` (Ennemi, with the death pipeline and `lob()` = `tirEnCloche`),
  `Projectile` (Tir), translated line by line. Not used by any class yet.

* **EntityRegistry** (`bb/game/entity_registry.hpp`, unit-tested): entity
  types by category (enemy, prop, pickup; their legacy ids overlap) and
  legacy id, or by symbolic name (`smurf.greedy`, `pokemon.lift`,
  `pickup.cow_bomb`...). `spawn()` creates the entity at a `SpawnPoint`,
  adds it to its list (props and moving platforms are separate
  placements) and counts enemies for the kill statistics. The three
  `switch` factories are gone: `legacy_registry.cpp` registers the 2002
  classes (49 enemy ids, 49 prop ids, 8 pickups), and enemy 0 draws its
  random skin inside its factory from the World's generator (the same
  session stream). Prop 900 is a second id for the Pokémon bomb, as a plain
  prop instead of a moving platform; no shipped level uses it. Enemy id 502
  (princess guard) has no factory, as in 2002.

* **The Smurf world runs on the new framework** (exit criterion of the
  phase). `src/game/src/smurf/` ports the eight enemy types of snuf1 and
  snuf2 (the level files place ids 0 to 7; the Big Smurf, 8, never) and
  their five shots: `Smurf` (one class with a `Skin` for the 2002 basic
  Smurf and its five variants, which only changed speeds, hit points and
  an accessory), `GreedySmurf` and `Cake`, `PranksterSmurf` and `Gift`,
  `WildSmurf` and `WildSmurfShot`, `FarmerSmurf`, `StrongmanSmurf` and
  `Fireball`, `Smurfette` and `DeadlyKiss`, `Stork`.
  `registerSmurfWorld()` replaces their legacy registrations; the 29 files
  of the 2002 classes are deleted. The `smurf` replay's world trace shows
  every ported class and no 2002 Smurf class. Facts needed to keep the
  fingerprints:
  * every random draw happens in the 2002 order (the skin is drawn before
    the constructor's own draws);
  * the wild Smurf's shot declared its own `dx`/`dy`, hiding `Sprite::dy`:
    the fingerprinted `dy` of that shot is always 0, so the port keeps its
    speed in separate members (`xSpeed`, `ySpeed`);
  * the wild Smurf's constructor read `dir` before the level event set it
    (always the facing-left images), and its two timers were static,
    shared by all wild Smurfs (kept, D-78 and Phase 5a);
  * the stork's sine comes from `bSin`, computed at start-up with libm and
    the 2002 PI (3.141592654). `bb/game/trig.hpp` holds the same 360
    values as a table (`sine1024`, `cosine1024`), dumped from C++ on macOS
    and checked against libm by a unit test on every platform.
  The strongman starts his music through a new `MusicPlayer` interface
  (`MusicBank` implements it). New 2002 defects, kept: D-76 (the
  strongman's corpse is never engraved), D-77 (the stork counts in the
  kill percentage but cannot be killed), D-78.

* **Validated handles** (`Handle<T>` in `bb/game/entity.hpp`,
  unit-tested): a reference that becomes null when its entity is
  destroyed (the entity shares a life token, cleared by its destructor; a
  copied entity gets its own). Used for the D-04 cases: the Princess's
  guards (the genuine use-after-free: they read her after she was freed;
  a null handle now means she is gone, which they already treated as
  dead), the lightsaber's thrower and target, Rayman's fist, the blood
  spray's owner (`Giclure::pere`), and the targets of the deadly kiss and
  the wild Smurf. The cached copies of the target player in
  `EnnemiBisouAuto`, `EnnemiGrandSmurf` and `TirSnorkyGouverneur` go stale
  but never dangle (players are freed after the game); they change with
  their classes in Phase 5. No replay reaches the Princess's death, so
  that path is covered by the unit test only.
* **Level events** (`bb/game/level_event.hpp`, unit-tested):
  `LevelEvent` has the activation X, `wakes()`, `ready()` and `run(World&)`;
  `World::updateEvents()` replaces `Game::updateEvents()` (same list
  moves, same order). `makeLevelEvent()` builds the ported events from a
  level record: `SpawnEvent` (enemy, prop or pickup through
  `World::registry`), scroll lock, scroll speed, set flag, hold fire,
  weather, sound, dialogue, and midground and foreground decals (a
  `Decal` entity; the 2002 rule that drops a midground decal left of
  x = 320 is kept). The 2002 event classes for music, banner texts, the two
  generators and the vehicle remain behind the same interface (`Event`
  bridges `aActiver()`/`doEvent()`); 14 legacy event files are deleted.
  `EventEnnemi` and `EventBonus` stay as wrappers over the registry for
  the 2002 code that calls them directly (generators, `MakeBonus`, Luigi).

All six replays gave the expected fingerprints after every step (checked
again on 15 October 2026 after the base classes).

### Verification (15 October 2026)

* macOS Debug: `ctest` 17/17 (the 6 replays and their pack variants,
  unit, conformance, formats, assets).
* macOS ASan + UBSan: the 6 replays give the expected fingerprints, no
  finding (run on the Smurf port, and again with the handles and the
  level events).
* Linux (Docker, GCC, case-sensitive file system): `ctest` 17/17, so the
  replays give the macOS fingerprints and glibc gives the trigonometry
  table. GCC's `-Wshadow` also flags a parameter named like a member,
  which clang does not: two such names (in `Enemy::sprayBlood()` and in
  the `EntityList` unit test) were renamed.
* Release: unit tests (45 cases) and the 6 replays after every step.

### Left for Phase 5 (by design)

* The content still in 2002 classes: every world but the Smurfs, the
  generators, banner texts, music and vehicle events, players
  (`Couille`), pickups, props, gore, weather. `WeaponSpec`, `Pickup`,
  `Prop`, `MovingPlatform`, `Vehicle` and `Player` base classes come with
  the first content that needs them (plan section 5, Phase 5 order).
* The defects kept for identical replays: D-76, D-77, D-78 (and the wild
  Smurf's shared timer); cached copies of the target player in three 2002
  classes (D-04, stale but not dangling).
* The Big Smurf (id 8, never placed): finished in Phase 5 (see there).

## Phase 3 — Main loop and screens (done; see "Left")

The game has one loop: `runScreens()` steps a stack of screens and is the
only caller of `presentFrame()`. Every blocking routine of 2002 (intro and
attract cycle, menus, character select, briefing, level, dialogues, pause
menu, level summary, game over, name entry, hi-scores, credits, cutscenes)
is a screen that draws at most one frame per update. The level simulation
runs on a fixed 11.8 ms clock.

### Done

* **Screen stack** (`src/game`, library `bb_game`, standard C++ only):
  `bb::game::Screen`, `Frame {Present, Wait, Continue}` and `ScreenStack`
  (push, finish, replace, step; finished screens are destroyed in the order
  they finished). Unit tests in `tests/unit/screen_stack_test.cpp`.
* **Legacy screens** (`vc-projects/Blip_n_Blop_3/screens/`): the runner and
  its small helpers (wait for release, wait for a key, delay), the attract
  cycle (root screen, from `Game::go` and `MenuMain`), title, hi-score
  table, game over and name entry, credits, character select, cutscene,
  pause menu (`MenuGame` made resumable), dialogue (`RPGPlayer`), level
  summary (`showPE`), level and campaign. `Game` keeps the level world and
  a small API used by the screens; `game.cpp` went from 4,391 to 3,131
  lines.
* **Deleted**: `Game::go`, `jouePartie`, `joueNiveau`, `gameLoop`,
  `updateAll`, `updateMenu`, `updateRPG`, `getHiscore`, `getName`,
  `showGameOver`, `showHighScores`, `selectPlayer`, `showMainScreen`,
  `showCredits`, `CINEPlayer::playScene`, `Input::waitKey` and `waitClean`,
  `bbSleep`, `session::sleepMilliseconds`.
* **Milestones**: `session::note` prints each screen change with its input
  frame and tick (stdout only, not part of the fingerprints), so two builds
  can be compared screen by screen.
* **Menu replays** added to the golden set: `menus` (every menu path the
  other replays do not reach, key redefinition, Blop, dialogue skip, pause
  menu options and exit, attract cycle interrupted, normal quit) and
  `gameover` (two players, level summary with and without bonus, game over,
  both names typed, hi-score table). ctest has 15 tests.
* **FixedClock** (`bb/core/fixed_clock.hpp`, tested): 11.8 ms ticks, at
  most 3 per frame, time beyond that dropped. Used by the level loop
  (rebased after a pause or a dialogue, so their time is not simulated),
  the credits and the cutscenes. The 2002 frame-time average ("glorf") is
  gone.
* **Focus loss pauses the level.** The platform already reported focus
  events (now also suspend and resume, `SDL_APP_WILLENTERBACKGROUND` /
  `DIDENTERFOREGROUND`); `Input` keeps the focus state, and a loss opens
  the pause menu at the next tick of a level (`Input::takeFocusLost()`,
  `Game::pauseRequested()`). A loss that ends before a level asks for it
  (in a menu, the briefing) is forgotten. The focus state is part of the
  recorded input (`unfocused` in `.bbreplay`, like `quit`), so recordings
  stay exact; the `menus` replay now loses the focus once during the level.
* **No player-visible literal left in the screens and the HUD**: "PLEASE
  WAIT", "LOADING", "Press a key to start.", "Loading...", "PLAYER %d GOT
  A HIGH SCORE!", "PLEASE ENTER YOUR NAME :", HUD "Bonus"/"Stage" are
  `uk.dat` ids 850–857 (`tools/data/fix_legacy_data.py`, inserted before id
  1000 because `writeDat` writes ids in order and the format tests compare
  bytes). The fingerprints did not change.
* **HUD** (`screens/hud.*`, class `Hud`): `Game::drawHUB`, `drawHUBpv` and
  `drawTimer` moved out of `Game`; the two players' mirrored layouts are
  data instead of two copies of the code. The fingerprints did not change
  (`gameover` draws both players on a bonus level).
* **Level summary in English**: the members of `LevelSummaryScreen` (the
  2002 local names of `showPE`) and its comments.
* **Quit request handled by every screen.** A closed window (or the end of
  a replay) sets `app_killed`, which every screen checks; cutscenes now do
  too (they used to play to their end). Two milestones show it: `stop:
  quit requested` / `stop: end of the replay` and `exit`. Checked with a
  sweep that cut the four scripted replays with a quit every 150 to 700
  frames (169 runs, Release): every run exited within 1 to 5 frames, from
  the intro and the attract cycle, main menu, character select, briefing,
  level, dialogue, pause menu, level summary, game over, name entry,
  hi-score table, credits and the ending cutscene; two more runs quit
  inside the pause menu (exit 4 frames later).

The screen stack was checked to be exact: all five replays (and their pack
variants) gave the fingerprints recorded before the change, in Release and
Debug. What made it exact: one old loop iteration is one `update()`; a
blocking wait that returned at once uses no frame (`Continue`); code after
a `presentFrame()` that reads the time runs at the start of the next
update; the pause menu and dialogues run inside a tick, which resumes after
them.

### Behaviour changes

* **Game speed no longer depends on the display.** The 2002 loop averaged
  the last 50 frame durations: it ran one tick per frame for the first
  frames of the first level (and of each cutscene and the credits), and
  between about 78 and 93 Hz the average locked the game to the display
  rate. FixedClock gives 84.7 ticks per second at any rate.
  With the 60 Hz virtual clock of the replays, the first level played
  gets 3 more ticks (snuf1 in `gameover`: 338 ticks instead of 335 in 241
  frames); later levels get the same number of ticks as before (snuf2: 870
  both). Because the random generator is seeded once per session, those 3
  ticks change everything random afterwards: in `gameover` the two players
  die in bisous1 9,874 ticks after its start instead of 10,098.
  All five fingerprints were re-recorded on 14 October 2026, after checking
  that every replay still reaches the same screens in the same order (the
  milestone lists of the old and new builds differ only in frame and tick
  numbers). The `gameover` script was changed for this: it held a walk key
  until just before the old death time, and any key held skips the game
  over screen, so it now stops walking earlier and waits (the new script
  gives the same screens with the old build too). The names "BLIP 1" and
  "BLOP" were checked on the dumped hi-score table.
  `menus` was re-recorded once more for the added focus loss; the other
  four fingerprints were unchanged by the focus code and the strings.
* **Closing the window during a cutscene** quits at once instead of after
  the scene (the 2014 `playScene` loop, like the 2002 one, only checked
  Escape). No replay quits during a cutscene, so no fingerprint changed.

### Measurements

* **CPU in the main menu** (exit criterion "CPU idle in menus"), windowed,
  60 Hz vsync, Intel i5-7360U, a replay that skips the intro and waits in
  the main menu for 10 s, `ps` sampled every second: Release 21–24 % of one
  core, Debug 51–60 %. The plan's baseline for the 2014 code (Debug, menu
  screen) was 35–77 %. The macOS `sample` profiler (Release, 4 s in the
  menu): 1,231 of 1,616 main-thread samples sleep until the next frame (no
  busy loop); 110 are the frame fingerprint, which only replay runs compute;
  the rest draws the menu and uploads the frame. A normal run (no
  fingerprint) was not measured directly because it cannot be driven to the
  menu without input. The menu still redraws and presents every frame; not
  presenting unchanged frames is left for the renderer work (Phase 7).

### Facts found during Phase 3

* **The VSYNC option does nothing (D-75)**, since the 2014 port: the
  renderer is always created with vsync and nothing reads `vSyncOn` except
  the menus and the settings code.
* The legacy random generator is seeded once per session, so any change in
  the number of ticks of one level changes everything random in the levels
  after it.
* The Linux check had not been run since Phase 2 added `bbassets`: GCC
  rejected two `snprintf` calls into 16-byte buffers
  (`-Wformat-truncation`, an error with `-Werror`; Clang does not warn).
  The buffers are now 32 bytes; the tool's output is unchanged.

### Deviations from the plan

* **The quit request stays a flag** (`app_killed`) checked by each screen
  rather than an event delivered by the screen stack: the screens already
  check it at the points where the 2002 loops did, which also keeps the
  shutdown frames that the replays hash. The sweep above checks that it is
  handled everywhere.
* **UI timers stay on the session clock** (dialogue waits, attract 10 s,
  title 5 s, credits pages), not tick counters as §3.10 says: screens are
  outside the fixed tick, and a frame count would run faster on a
  high-refresh display. The session clock is virtual in deterministic runs,
  so replays stay exact.
* **Upstream's 2019 extractions were not used as references** (the plan's
  `hud.h`, `go_arrow.h`, `character_selection.*`, `menus/`): that history is
  not in this repository. `Hud` and the screens were written from the 2014
  code.
* `Game` is not fully dissolved: it keeps the level world (entities,
  scrolling, collisions, events), which Phase 4 replaces with `World`.

### Exit criteria

* **Campaign completable with no nested loops**: yes. `runScreens()` is the
  only caller of `presentFrame()` and `session::idle()`, and no loop waits
  on input; the `campaign` replay plays the 12 levels (won with the F9
  cheat), the ending and the credits.
* **Replays pass**: yes, 15 of 15 tests on the final code: macOS Clang
  Debug, macOS Clang with AddressSanitizer and UBSan (no finding), and Linux
  GCC Release (`tools/docker/linux-check.sh`, the same fingerprints as
  macOS). The ASan run predates only the `bbassets` buffer fix, which the
  Debug and Linux runs include.
* **CPU idle in menus**: no busy loop (the main thread sleeps until the
  next frame); 21–24 % of a core in Release because the menu is redrawn and
  presented every frame (see Measurements).

### Left

* D-75: the VSYNC option, which needs a frame limiter first.
* Not presenting unchanged frames in menus (renderer work, Phase 7).
* Windows build still not verified (since Phase 1).
* Resolved later: D-75 in Phase 6; the Windows build in Phase 7 (CI,
  25 October 2026); unchanged frames are still presented, by decision
  (Phase 7, "Display").

## Phase 2 — Assets and formats (done; see "Left")

Every data file is read by a bounds-checked reader in `bb::core::assets`
(`src/core/include/bb/core/assets/`); the game gets its assets through an
`AssetSource`, either the 2002 files of `data/` or the runtime pack
(`--pack <file>`). The source assets in `assets/source/` (PNG, JSON, WAV,
Ogg Vorbis/MP3, UTF-8 text) are converted from and to the 2002 files and
packed by the tool `bbassets`. All three regression replays give the same
fingerprints from `data/` and from a pack built from `assets/source/`.

### Done

* **Readers and writers in core** (`legacy_formats.hpp`): `.gfx` and LGX,
  `.lft`, `.sfx` (and WAV headers), `.lvl`, `.dat`, `.lst`, `.mbk`, `.cin`,
  `.rpg`, `.scr` (`.cfg` was imported in Phase 1). Format-independent
  structures in `asset_types.hpp` (`SpriteBank`, `Font`, `SoundBank`,
  `LevelData`, `StringTable`, `Campaign`, `MusicList`); 16-bit images stay
  in their authored layout (`Image16`, RGB565 or RGB555) and are expanded
  with the 2002 decoder's arithmetic. `tests/formats/` reads every shipped
  file, writes it back and compares bytes, and feeds truncated files to the
  readers.
* **The game reads through them**: `PictureBank`, `Fonte`, `SoundBank`,
  `MusicBank`, `Game::chargeNiveau`, `Game::loadList`, `loadTxtData`, the
  cutscene and dialogue players and the hi-scores. Every data access that
  opened a host path (`resolvePath()`, 7 call sites, plus `Precache()`) is
  gone; music is handed to the platform as bytes. The 2002 LGX decoder and
  its colour tables are deleted (`lgx_packer.*` keeps the half-tone only).
* **Runtime pack** (`pack.hpp`, [formats/runtime-pack.md](formats/runtime-pack.md)):
  one file, an index sorted by name hash, sprites as row spans
  (skip/copy/fill) of 16-bit pixels; any image can be read on its own
  (`PackSpriteBank`, for lazy decor screens).
* **Source assets and tool** ([formats/source-assets.md](formats/source-assets.md)):
  `bbassets extract | pack | pack-legacy | legacy | verify | ids`
  (the plan's bbpack, bbfont, bbsfx, bbtext and bblevel in one program).
  The build target `assets_pack` makes `<build>/blipblop.bbpack` from
  `assets/source/`. Third-party code for the tools only: stb_image 2.30,
  stb_image_write 1.16, nlohmann/json 3.11.3 (`third_party/README.md`).
* **Text**: fonts are indexed by Unicode code point and all game text is
  UTF-8 (strings, key names, credits). The credits (99 lines) and the level
  summary labels (10) moved from `game.cpp` literals to the string table
  (`uk.dat` ids 1000–1098 and 840–849, added to `data.zip` by
  `tools/data/fix_legacy_data.py`).
* **Symbolic sprite names**: the manifests can name images; `bbassets ids`
  generates `src/assets/include/bb/assets/sprites/<bank>.hpp`. The first
  names (10 images of `misc.gfx`: weapons, pickups, the HUD cow bomb, read
  from the code and checked on the images) replace their magic numbers in
  the pickups and the HUD. The test `assets.ids` fails when a manifest and
  its header disagree.
* **Tests** (ctest, 11): `unit`, `conformance`, `formats`, `assets.verify`,
  `assets.ids`, and the three replays twice, from `data/` and from the pack
  (`replay.*.pack`).

### Behaviour changes

* **Accented names in the credits (D-73).** The 2002 credit literals were
  Windows-1252 bytes that select the accented glyphs of `rpg.lft`; commit
  `fa5d0ca` (January 2014) re-encoded the sources as UTF-8 and the credits
  lost every accented letter ("Jrme Karaban"). The ten literals are now
  Windows-1252 escapes. The `attract` and `campaign` frame fingerprints were
  re-recorded on 13 October 2026: with only these ten literals reverted,
  all three replays give the Phase 0 fingerprints again, and the campaign
  world-state fingerprint did not change (credits are drawn, not
  simulated).
* **Score checksum on signed chars (D-74)** on every platform; the shipped
  `bb.scr` loads and is written back byte for byte (checked with a normal
  run on a copy of the data).
* Corrupt or truncated data files are now refused with a message instead of
  being read past their end (the 2002 loaders trusted every size).
* In French (unreachable: the game forces English, D-25) the `ù` key can no
  longer be typed into a hi-score name: its name is now two UTF-8 bytes and
  the name entry takes one-byte key names only.
* Music is streamed from memory instead of from a file.

### Measurements

* Sprite encodings over the 3,755 shipped images: LGX 91.3 MB, row spans
  (pack) 68.3 MB, raw 16-bit 118.6 MB. Spans compress the RGB565 decor
  screens, which LGX stores raw.
* Pack 93.6 MB (sprites, 20.8 MB of music, 4.0 MB of sounds) against 124 MB
  for the 2002 files; source tree 87 MB (61 MB of PNG).
* `bbassets extract` 25 s, `bbassets pack` about 50 s in a Debug build
  (macOS, Intel i5-7360U); stb is compiled with `-O2` in every build.

### Facts found during Phase 2

* All 3,564 LGX version 1 images re-encode to their shipped bytes with the
  editors' greedy run rule; no bank has data after its last image.
* The level files carry the MSVC debug fills of the 2002 editor:
  `0xCDCDCDCD` in every event field it never set, `0xCC` stack bytes after
  the asset names. The game's loader copies some fill values into its state
  (scroll locks and texts whose condition does not use the flag), so the
  converters keep them (`null` in the level JSON). Platforms were painted
  with a brush: 9 to 3,788 runs of equal height per level.
* Every dialogue scene a level triggers exists in its `.rpg` file, so the
  2002 player's stream state after a missing scene (the next scenes would
  fail too) never happens.
* `Precache()` (`precache.h`) opened each dialogue and cutscene file and read
  nothing: it used the return value of `fseek` (0) as the file size.
* The text files are LF-only; 3 of the 12 music lists and 2 of the 7
  dialogue scripts end with a newline, the others not. The only non-ASCII
  bytes are the three apostrophes of `uk.dat` and an `é` in a comment of
  `video.rpg`. 4 WAVs have an 18-byte `fmt ` chunk and a `fact` chunk; all
  175 are PCM mono.
* The baseline was red when this phase started: the credits fix (D-73) was
  in the working tree but the fingerprints had not been re-recorded.

### Left

* Symbolic names for the other 3,745 images (Phase 5, "while migrating").
* Other player-visible literals in code (HUD "Bonus"/"Stage", menu labels):
  they go to the string table with their screens in Phase 3.
* The legacy game still decodes whole banks at load; per-image loading is
  used once the scrolling and drawing are rewritten (Phases 4 and 7).
* `assets/source/` is not under version control yet (87 MB, mostly PNG):
  committing it, possibly with Git LFS, is the maintainer's decision.
  Until then the build extracts it from `data.zip` when it is missing, and
  `bbassets extract` keeps the names of existing manifests.
* Resolved later: the player-visible literals went to the string table in
  Phase 3; `assets/source/` is
  committed without Git LFS (the maintainer's choice, 25 October 2026).
  Images are named as the first of a run plus a step, not one by one
  (Phase 5, "Deviations from the plan"). Still true: a bank is decoded
  whole when it is loaded (`services.cpp`); peak memory is 85.6 MB in the
  `video` replay (docs/replays.md), so per-image loading was not needed
  on the desktop targets.

### Deviations from the plan

* **One pack for the game** rather than one container per bank or level:
  one file to open and ship, one index, and each image is still readable
  on its own.
* **Sounds stay WAV in the pack** (the plan said 16-bit PCM): the shipped
  bytes are kept exactly (the replays do not hear audio, so a conversion
  could not be checked); an in-house mixer (optional since 16 October 2026,
  plan §3.8) could convert at load.
* **One tool, `bbassets`**, instead of five programs.

## Phase 1 — Platform layer (done; Windows verified on 25 October 2026)

The legacy game now talks to `bb::platform` interfaces
(`src/platform/include/bb/platform/`) implemented on SDL 2
(`src/platform/sdl2/`). Every step below kept the three regression replays
unchanged (same expected fingerprints as at the end of Phase 0).

### Done

* **Files**: `bb::core::Vfs` (case- and separator-tolerant lookup under a
  data root, `--data <dir>`); the legacy `legacy_io.*` replaces
  `Engine/io.*` and `path_fix.*`.
* **Graphics**: the legacy `Surface` keeps the DirectDraw-style API on a
  `bb::core::gfx::Pixmap`; blits, fills and the half-tone darkening match
  SDL exactly (`tests/conformance/blit_vs_sdl_test.cpp`). The SDL display
  streams the frame into an ABGR8888 texture
  (`tests/conformance/sdl2_display_test.cpp` reads it back). Removed:
  `graphics.*`, `dd_gfx.*`, `sdl_*.h`, `restore.*`, `alpha_blend.*`.
* **Input**: platform `InputSource` (stable pad slots, hot-plug); core
  input codes, machine names (`key:left_ctrl`, `pad1:button3`), the 2002
  UK/FR display names, bindings and a text settings file
  (`blipblop-settings 1`) stored in SDL's preferences directory
  (`LOADED Studio/Blip and Blop/settings.cfg`) or at `--settings <file>`.
  The shipped `data/bb.cfg` is imported once and never written; its
  DirectInput codes for player 2 are rejected and player 2 gets the
  default keys (D-67).
* **Audio**: platform `Audio` on SDL2_mixer. The legacy `Sound` and
  `MusicBank` call it directly. Removed: `fmod.h`, `fmod__errors.h`, the
  FMOD emulation in `Engine/windows.cpp`, the never-shipped tracker-module
  path of `MusicBank`, and the unused Windows binaries `fmod.dll`,
  `fmodvc.lib`, `ddraw.lib`, `dinput.lib`.
  `tests/conformance/sdl2_audio_test.cpp` runs on SDL's dummy driver.
* **Win32 layer, timer, message pump**: `Engine/` (the `windows.h`
  replacement and its stubs), `l_timer.*` and `ben_divers.*` are deleted.
  The legacy code keeps `RECT`, `DWORD`, `COLORREF` and `RGB` from
  `legacy_types.h` (which also keeps the `using namespace std;` the 2002
  sources rely on), and reads time through `legacy_time.h`
  (`LGetTime`, `bbTickCount`, `bbSleep` on `session` time). The 17
  `manageMsg()` calls were no-ops (the port's `PeekMessage` always
  returned false) and are gone; so are the never-called window procedure,
  `Question()`, the `active`, `want_to_kill_app` and `winSet` flags, and
  the four GDI-based LGX encoders of `lgx_packer.cpp` (the editors keep
  theirs in `winLGX.cpp`). Error and warning boxes go through the
  platform `System`.
* **Application and entry point**: `main()` is in the SDL backend
  (`src/platform/sdl2/sdl2_main.cpp`, so that SDL2main can provide the
  Windows entry point) and calls `bbMain()` in `src/app/main.cpp`, which
  parses the options, opens the platform and calls `runLegacyGame()`. The
  legacy code is the object library `bb_legacy`: its compile commands have
  no SDL include path, and no file outside `src/platform` and
  `tests/conformance` includes an SDL header. `runLegacyGame()` drops
  every pointer to the platform before returning, so global destructors
  never call into a closed SDL.
* **Log**: `bb::core::Log` with file and console sinks. The legacy `Debug`
  object passes complete lines to it. Info lines are buffered (the 2014
  port flushed every message); warnings and errors are flushed at once
  and also printed on stderr. The file is `BlipBlop.log` in the
  preferences directory, in the working directory for deterministic runs,
  or the file given with `--log`. Unit tests in `tests/unit/log_test.cpp`.

### Behaviour changes

* **32 sound voices instead of 8.** The 2002 game asked FMOD for 32
  channels (`CHANNEL_NUMBER`). The 2014 port never called
  `Mix_AllocateChannels`, so it ran with SDL_mixer's default of 8
  (`MIX_CHANNELS`); a ninth simultaneous sound was dropped. The platform
  now allocates 32 voices.
* `--max-frames` also stops normal (non-deterministic) runs.
* **F12 frame counter works (D-45).** The port's `SetTimer` was a stub, so
  the counter always showed 0. It now publishes the frames drawn during
  each second of game time (virtual time in deterministic runs). No replay
  presses F12.
* **Error and warning boxes are shown again** in windowed runs, as in the
  2002 game; the port only printed `MessageBox(...)` to stdout. Headless
  runs log them.
* **Log location and buffering** (see above). The deterministic `attract`
  log is identical to the Phase 0 one except for the deleted
  "Performance counter available." line.
* **Score file arithmetic (D-70).** `HiScores::crypte()` shifted a signed
  constant into overflow; now unsigned, same bits. The shipped `bb.scr`
  still loads and is written back byte for byte.
* **Real-time timer precision (D-71).** The 2002 `LGetTime()` converted the
  performance counter with single-precision floats. Measured with the
  1 GHz counter of the macOS test machine, its step grew from 0.1 ms to
  0.2 ms after 30 minutes, 0.4 ms after 1 hour and 1.6 ms after 4 hours
  (one game tick is 11.8 ms). It now uses exact integer microseconds.
  Deterministic runs were already exact and are unchanged.

### Measurements

* All 175 sound effects of the 9 `.sfx` banks decode through SDL_mixer,
  and all 22 music entries of the 12 `.mbk` banks open (20 Ogg Vorbis
  streams and the 2 MP3 jingles `tambour` and `gameover`).
* Every `.mbk` entry has type 1 (streamed file). Type 0 (FMOD tracker
  module) appears in no shipped bank; the SDL port could not play it.
* **Present paths (D-08)**, `tests/bench/present_bench.cpp`, Release,
  sdl2-compat 2.32.72 over SDL 3 with the Metal renderer, Intel Iris Plus
  640, no vsync, 600 frames per path, median per frame over three runs on
  an idle machine, both measuring orders:

  | Path | Median |
  |------|-------:|
  | new texture every frame (2014 port) | 0.24–0.30 ms |
  | streaming texture (Phase 1) | 0.37–0.42 ms |
  | window surface, ABGR→ARGB conversion included | 0.44–0.48 ms |

  The window surface is ARGB8888 here, so it needs a conversion, and it
  cannot scale (fullscreen, resized window). The streaming texture stays:
  it is about 0.1 ms slower than creating a texture per frame, under 1 %
  of a 60 Hz frame. With a replay running at the same time, the ranking
  changed (medians 1.34, 1.37 and 0.74 ms), so each target is measured
  idle with this benchmark before its present path is chosen (Phase 7).

### Facts found during Phase 1

* The sanitizer runs of Phase 0 replayed deterministic runs only, which
  never load or save hi-scores. The first normal run under UBSan stopped
  at once in `HiScores::crypte()` (D-70).
* With clang's `-Wvla -Wgnu -Wmicrosoft`, the 177 legacy files show no
  compiler extension. This is a sign that MSVC can build them, not a
  proof: no Windows build was made.

* The regression replays do not exercise music: `campaign` and `smoke`
  run with `/quiet`, and `attract` never loads a level. Music is covered
  by the audio conformance test only.
* `Sound::loadFromMem` failures were invisible: the 2014 shim returned a
  sample object even when SDL_mixer rejected the WAV, and
  `SoundBank::loadSFX` ignores the result anyway.

### Exit criteria

* Replays unchanged: yes, with the Phase 0 expected fingerprints: macOS
  clang Debug, macOS clang with AddressSanitizer and UBSan (no finding),
  and Linux GCC Release (`tools/docker/linux-check.sh`).
* No SDL header outside `src/platform`: yes (and the conformance tests,
  which compare against SDL on purpose).
* Windows build without DirectX headers: no DirectX, Win32 or FMOD header
  is left in the code. Not verified by a Windows build (no Windows
  toolchain on the development machine; the CI workflow has never run).

### Left

* `bb::core::input::InputMapper` exists but the legacy game still reads
  its own key arrays; the screens of Phase 3 are its first users.
* Resolved: the legacy game is gone (Phase 5); every screen reads the
  input through the game's input services.

## Phase 0 — Baseline, hygiene and safety net

### Done

* **Build**: root `CMakeLists.txt` (CMake ≥ 3.16, C++17, out-of-source only),
  `CMakePresets.json` (`default`, `release`, `asan`), SDL2/SDL2_mixer found
  through their CMake packages with a pkg-config fallback, data extracted
  from `data.zip` into the build directory. The legacy game builds with no
  shim outside the repository.
* **Compile fixes**: `fmod.h` calling-convention guard, `<malloc.h>`, all 65
  missing returns (23 by deleting the never-used `Engine/ddraw.*` and
  `Engine/dinput.*`), the 3 mismatched header guards, writable string
  literals, nested comments.
* **Determinism**: virtual time, recorded/replayed input, `bbRand()`
  (Microsoft CRT generator, seed 1), frame and world-state fingerprints,
  headless mode (`docs/replays.md`). Uninitialised heap and stack variables
  of the legacy code now start at zero (`zero_new.cpp`,
  `-ftrivial-auto-var-init=zero`).
* **Unit tests** for `bb::core` (doctest), **regression replays** registered
  in CTest, GitHub Actions workflow (Linux, macOS, Windows, sanitizers,
  static analysis).
* **Hygiene**: `.gitignore`, `.gitattributes`, `.clang-format`,
  `.clang-tidy`, `.editorconfig`; vendored SDL 2.0.1, Visual Studio/VC6
  project files of the game and every VC6 cache file removed.
* **Docs**: `README.md`, MIT `LICENSE`, `docs/formats/`, `docs/replays.md`,
  `docs/legacy_map.md`.

### Measurements

| Check | Result |
|-------|--------|
| Two runs of the same replay | identical fingerprints (frames and world state) |
| Debug vs Release vs ASan+UBSan builds | identical fingerprints |
| macOS (Apple Clang 16) vs Linux (Ubuntu 24.04, GCC 13), every replay | identical fingerprints, after the fix below |
| Sanitizer findings over the campaign replay | 0, after the fixes below |
| Port fixes, blend mode, opaque fills, data fixes (G1, G2 and data) | no frame changed in any replay |
| Half-tone fix (D-32) | changes only the darkened areas: menus and dialogue boxes, as in the original |
| Campaign replay (12 levels, 26,990 frames, 25,827 ticks), Release, headless | about 35 s |
| Attract mode, 6,000 frames, Debug, CPU time | 78 s before, 39 s after D-59/D-62 |
| Release profile of attract mode | `SDL_Blit_Slow` gone; blits are 2.7 % of samples |

The world-state fingerprint of the campaign replay is the same before and
after all Phase 0 fixes: none of them changed gameplay.

### Facts found during Phase 0 (not in the original plan)

* **Function arguments made runs compiler-dependent.** 22 calls pass two
  random numbers as arguments, e.g. `new GoreGiclure(bbRand() % 6,
  -2 - bbRand() % 6)`. C++ leaves the order of argument evaluation
  unspecified; Clang evaluates left to right, GCC right to left, so the two
  values were swapped on Linux and the campaign replay diverged in
  `snorkniv.lvl` (a Snork trunk flew with a different speed). The numbers
  are now drawn into locals in source order. Found with `--trace-ticks`,
  which dumps the world state of a tick range for diffing.

* **The SDL port could not finish the campaign.** `Fonte::printC` logged
  its text before the original NULL check; `snuf2.lvl` shows banner text
  150, which `uk.dat` does not contain, so the game crashed in the second
  level. The Princess's texts 517 and 519 would crash `video.lvl` the same
  way. (D-21 is a crash, not only a slowdown.)
* `tambour.zik` (pre-level drum roll) and `gameover.zik` are 2002 MP3 files
  starting with 417 zero bytes; SDL_mixer rejects them, so these two jingles
  never played in the port (D-64).
* Joystick hats never worked: the handler tested the hat index instead of
  the hat value.
* The shipped `data/bb.cfg` binds player 2 to DirectInput scan codes
  (0x10, 0x1F, 0x20, …), not SDL key codes: player 2's keyboard controls
  were broken with the shipped configuration (one of them even became the
  Space key, player 1's cow bomb).
* The cheat keys share a 20-tick cool-down: F9 (win level) is ignored for
  20 ticks after F3 (extra life). The replay scripts wait before F9.
* SDL_BlitSurface semantics are reproduced exactly by `bb::core::gfx::blit`
  (3,000 random cases in `tests/conformance`), including overlapping
  self-copies, which SDL's copy blitter handles like `memmove`. Before
  D-59 those self-copies (screen shake and deformation, D-20) went through
  SDL's blend path, which has no overlap handling (SDL source); they now go
  through the overlap-safe copy. This is established from the SDL source;
  no replay was built to exercise a shake specifically.
* Uninitialised members change the picture, not only the logic: the wild
  Smurf draws its gun when an uninitialised `shooting` flag is non-zero.
* The trigonometric tables use PI = 3.141592654; the resulting values sit
  about 1e-7 away from integer boundaries, so `int(1024 * cos(…))` is the
  same with every libm (no cross-platform risk there).
* The Microsoft CRT generator reproduced by `bb::core::Rng` is verified
  against the documented first outputs (41, 18467, 6334, …).
