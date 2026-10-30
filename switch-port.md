# Blip & Blop — Nintendo Switch port plan

**Status (30 October 2026, evening).** Implemented, built and tested on a Mac and in devkitPro's container; not yet tried on a console. What a console must still show is the checklist of section 8 (`docs/release-checklist.md`). The facts learnt and the departures from this plan are in [docs/progress.md](docs/progress.md), "The Switch port"; section 11 below lists the departures.

Plan of 30 October 2026. It builds on the platform layer of the migration ([migration.md](migration.md), [docs/architecture/platform.md](docs/architecture/platform.md)): the game draws 640x480 frames in software and only `src/platform` talks to the system, so the port is a build, a few platform services, the size of the picture in handheld and docked mode (section 3), the Switch's controllers (section 4), an installable `.nsp` (section 6.4) and the end of the desktop releases (section 6.6). Section 2.2 was read from the source of devkitPro's SDL, not yet tried on a console.

Decided by the maintainer on 30 October 2026:

- The Switch is the game's only target. CI builds the `.nro` and nothing else; the maintainer packs the `.nsp` with a command line; both are released. The game keeps building on a Mac or a Linux PC, by hand, to try things and run the tests; nothing builds it for Windows any more.
- On the Switch, A fires, B jumps, Y drops the cow bomb, ZR fires, ZL drops the cow bomb, + or − pauses.

## 1. Goals and non-goals

**Goals**

- The game runs on a Switch (original model, OLED, Lite): installed from `blipblop-<version>.nsp`, with its own icon on the HOME menu; and as `blipblop.nro` from the Homebrew Menu, for development and for consoles where the `.nsp` cannot be installed. The whole campaign, 1 to 4 players, music and sounds, settings and hi-scores kept on the SD card.
- Handheld (and tabletop) mode: the picture takes the whole height of the 1280x720 screen, at 960x720. Docked: the Switch's largest output, 1920x1080, with the picture at 1440x1080, or 1280x960 (exactly ×2) as an option. Docking and undocking switch live.
- The Switch's controls: A fires, B jumps, Y drops the cow bomb, ZR and ZL fire and drop the cow bomb too, + or − pauses; in the menus A confirms and B goes back; the buttons' Switch names on screen; every controller (the console's Joy-Cons, a Pro Controller, a Joy-Con pair, one Joy-Con held sideways); four players with four Joy-Cons.
- Builds: one command makes the `.nro` from a fresh clone, on the maintainer's Mac and in CI; one command makes the `.nsp`. Mac and Linux builds stay for local tests: the unit and conformance tests, the 14 replays (which keep their fingerprints), `bbassets`, the level editor `bbedit`.
- Switch code stays in `src/platform` and the CMake files; no `#ifdef __SWITCH__` in `src/app` or `src/game`, so that the local Mac and Linux builds keep running the same game.

**Non-goals**

- Windows, Linux and macOS releases: dropped on 30 October 2026, with their packages, CI jobs and Windows support (section 6.6).
- An official release (Nintendo's SDK, the eShop). The SDK is under NDA, which a public MIT repository cannot hold; the game parodies Nintendo's and others' characters; and the README rules out commercial distribution. The `.nsp` is a homebrew package: built with the free devkitPro tools and hacBrewPack, encrypted with keys from the maintainer's own console, not signed by Nintendo, so it installs only on consoles whose custom firmware (such as Atmosphère) accepts titles Nintendo did not sign.
- Widescreen gameplay. Showing more than 640 pixels of a level would change the game: the decor ring is 840 pixels wide, enemies and level events start from the camera's position just outside the 640-pixel view, and the menus, briefings, cutscenes and HUD are 640x480 pictures. The picture stays 4:3, with bars at the sides.
- The Switch 2, touch and motion controls, HD rumble (section 10 lists the small ones as later options).

## 2. Starting point

### 2.1 The game

- Every frame is a 640x480 `core::gfx::Pixmap` drawn in software; `platform::Display` only uploads and scales it. `Sdl2Display` scales with `SDL_RenderSetLogicalSize` (`src/platform/sdl2/sdl2_platform.cpp:708`) and, for SCALING = INTEGER, `SDL_RenderSetIntegerScale`.
- Simulation ticks of 11.8 ms (84.7 a second); frames paced at 60 a second (`App::present`); SMOOTH MOTION draws between ticks for 60 Hz screens.
- Pads are game controllers with SDL 2's button numbers, by position (`src/core/include/bb/core/input/codes.hpp`). The default pad layout (`Bindings::defaultPadCode`, `src/core/src/input_bindings.cpp:44`): D-pad or left stick to move, the bottom button (`kPadButtonA`) to jump, the left one (`kPadButtonX`) to fire, the right one (`kPadButtonB`) for the cow bomb.
- `DeviceInput::held()` (`src/app/device_input.cpp:247`) lets any pad's bottom button act as Return and Start or Back as Escape. The menus confirm with Return or player 1's fire (`src/game/src/screens/menus.cpp:357`); the pause menu resumes with Escape; the main menu has no "back".
- Escape pauses a level (`src/game/src/game.cpp:567`), skips a cutscene or a dialogue; a lost focus also pauses.
- Hi-score names are typed key by key (`HiScoreEntryScreen`, `src/game/src/screens/attract.cpp:189`); a pad can only end the entry, with Start.
- Settings and hi-scores are written once, when the game exits (`src/app/app.cpp:467`).
- Data: the runtime pack `blipblop.bbpack` (94 MB), built by `bbassets` from `assets/source` (in git), is found next to the executable (`GameFiles::locateGameData`). Peak memory measured on the desktop: 85.6 MB.
- The 14 replays in `tests/replays` play with the keyboard, except `fourplayers`, where players 3 and 4 use pads 3 and 4 with the default layout (`JOY3_B0` jump, `JOY3_B2` fire, left and right). Section 4 generates it again with the Switch layout's codes.
- CI (`.github/workflows/ci.yml`) builds, tests and packages the game on Linux, macOS and Windows (64 and 32 bits), with a sanitizer job and a static-analysis job; `release.yml` attaches the desktop packages (CPack, `cmake/BBPackaging.cmake`) to a tag's release.

### 2.2 devkitPro's SDL 2 for the Switch

Read from devkitPro's SDL fork (github.com/devkitPro/SDL, branch `switch-sdl-2.28`: `src/video/switch`, `src/joystick/switch`, `src/filesystem/switch`, the Switch line of `src/joystick/SDL_gamecontrollerdb.h`):

| Area | What it does | Consequence for the game |
|------|--------------|--------------------------|
| Video | One window; its size becomes the size of the system's frame buffer (`nwindowSetDimensions`). The display mode is reported as 1920x1080 in both modes. With `SDL_WINDOW_RESIZABLE`, the event pump resizes the window to 1280x720 when the console goes handheld and to 1920x1080 when it is docked, on a change only, not when the window is created. | Today's 640x480 window would be stretched to 16:9 by the system until the first change of mode. The window must be created at the size of the current mode, stay resizable, and never go fullscreen. |
| Pads | Always 8 pads, all named "Switch Controller"; pad 1 reads the console's Joy-Cons and the system's player 1 together; pad *n* is the system's player *n*. A built-in game-controller mapping, by position: SDL's A is the Switch's B (bottom), SDL's B is A (right), SDL's X is Y (left), SDL's Y is X (top); Back is −, Start is +; L and R are the shoulders; ZL and ZR are trigger axes. No hot-plug events; when a controller's style changes outside handheld mode, SDL opens the system's controller applet. | By position, today's default layout would give B jump, Y fire, A cow bomb: the Switch layout (A fire, Y cow bomb) replaces it. The menus would confirm with B (SDL's A), and ZL and ZR do nothing (the backend drops trigger axes, `sdl2_platform.cpp:317`). |
| One Joy-Con | Held sideways: the face buttons are rotated so that SDL's A, B, X, Y are bottom, right, left, top again; the stick comes as the D-pad (no axes); SL and SR as ZL and ZR; a click of the stick as Start; − or + as Back. | The Switch layout works the same by position: the right button fires, the bottom one jumps, the left one drops the cow bomb, SR fires, SL drops the cow bomb; directions come through the D-pad. A click of the stick pauses. |
| Files | Base path `romfs:/`; preferences path: the current directory. | The pack at the root of the RomFS, the `.nro`'s or the installed title's, is found without changes once the RomFS is mounted; the preferences need a fixed folder. |
| Text | `SDL_StartTextInput()` shows the system keyboard; the text arrives as text-input events; the pads are frozen meanwhile. | Usable for the hi-score name. |
| System | The event pump turns the system's exit request into `SDL_QUIT`; it sends no focus events (HOME) and has no message boxes. | HOME does not pause the game; errors need the system's error applet. |

## 3. Display: handheld and docked

### 3.1 Sizes

| Mode | Frame buffer | Picture (SCALING = FIT) | Scale | Black bars | SCALING = INTEGER |
|------|--------------|-------------------------|-------|------------|-------------------|
| Handheld, tabletop (every model, Lite included) | 1280x720 | 960x720 | 1.5 | 160 px left and right | would be 640x480 (whole factor 1, a third of the screen): FIT is drawn instead, and the setting is kept for docked play |
| Docked, TV at 1080p | 1920x1080 | 1440x1080 | 2.25 | 240 px left and right | 1280x960 (×2), bars of 320 px at the sides and 60 px at the top and bottom |
| Docked, TV set to 720p or 480p in System Settings | 1920x1080 | 1440x1080 | 2.25 | the system scales the frame buffer down to the TV | the same, scaled down |

The picture keeps its 4:3 shape and the whole height of the screen; 1080p is the Switch's largest output; drawing at the output's size leaves the system nothing to rescale (except for a TV set below 1080p: reading `appletGetDefaultDisplayResolution()` would let the game draw at the TV's size, an optional refinement). Stretching to 16:9 is not offered: it would distort every picture by a third.

### 3.2 The CRISP filter

At 1.5 and 2.25 neither filter of today fits: SHARP (nearest) makes some columns and rows of pixels wider than the others (every other one at 1.5, every fourth at 2.25), which shimmers when the level scrolls; SMOOTH (bilinear) blurs the pixel art. CRISP, a third value of FILTER and the default: the frame is first enlarged with sharp pixels by the next whole factor, into a render-target texture (×2, 1280x960, in handheld mode; ×3, 1920x1440, docked), which is then drawn at the final size with bilinear filtering (×0.75 in both modes). Pixels stay square and sharp, with at most one blended pixel at their edges. It costs one more textured quad per frame and a texture of 4.9 MB (handheld) or 11 MB (docked).

### 3.3 Docking and undocking

SDL resizes the window when the mode changes. The display reads the output's size at every present (`SDL_GetRendererOutputSize`), recomputes the picture's rectangle, recreates the CRISP texture when the whole factor changes (2 or 3), and writes a `Display:` line in the log. No restart; the game's state is untouched.

### 3.4 Frame rate and options

- The Switch shows 60 frames a second: vsync always on; the 60-per-second pacing of `App::present` unchanged.
- SMOOTH MOTION on by default: 84.7 ticks a second do not divide into 60 frames.
- The VSYNC option, the F10 key and the `fullscreen` setting go: the Switch has no window to manage and always syncs. Local Mac and Linux builds open a plain window with vsync on.
- The options: SCALING (FIT, INTEGER), FILTER (SHARP, CRISP, SMOOTH), SMOOTH MOTION, LANGUAGE and the four players' controls.

### 3.5 Work

- New `src/core/include/bb/core/gfx/viewport.hpp`: a pure function from the frame's size, the output's size and the scaling to the picture's rectangle and the CRISP factor. Unit tests with the sizes of 3.1.
- `Sdl2Display` (the Switch's, and the local builds'): draws into that rectangle instead of using SDL's logical size and integer scale, clears the bars to black, implements CRISP with a render-target texture. The conformance tests (`tests/conformance/sdl2_display_test.cpp`, run locally with SDL's dummy driver) must pass unchanged; a new one checks CRISP (square pixels, edges blended over one pixel at most).
- `Sdl2Platform::createDisplay` on the Switch: the window at 1280x720 or 1920x1080 from `appletGetOperationMode()`, `SDL_WINDOW_RESIZABLE`, never fullscreen.
- Settings: `filter crisp` (`core::Settings`, `src/app/settings_file.cpp`); the defaults become the Switch's (CRISP, SMOOTH MOTION on); `vsync` and `fullscreen` lines in an old file are skipped as unknown statements. Deterministic runs keep today's reference settings (SMOOTH MOTION off), so that the replays' expected fingerprints hold, on a Mac or on a console.
- `menus.cpp`: three values for FILTER, no VSYNC item; the word CRISP in `assets/source/strings/uk.json` and its French in `fr.json`.
- The options page changes (one item less, a new value): if the `menus` replay goes through it, its expected fingerprints are recorded again, in a commit of their own (a meant change).

## 4. Controls

### 4.1 Principles

- The maintainer's layout: A fires, B jumps, Y drops the cow bomb; ZR fires and ZL drops the cow bomb too; + or − pauses.
- SDL numbers buttons by position, so the Switch layout is: right button fire, bottom jump, left cow bomb. It replaces the desktop's default layout (left fire, right cow bomb) for every pad; local Mac and Linux builds use it too.
- The menus follow the Switch: A confirms, B goes back, + and − pause. A is also the fire button, which confirms as in 2002: one button for both.
- The screens show the Switch's names: A, B, X, Y, L, R, ZL, ZR, +, −.
- The settings keep SDL's positional numbers (`pad1:button1` is the Switch's A), so a settings file or a replay means the same on the console and on a Mac.

### 4.2 In a level (each player)

| Action | Console's Joy-Cons, Pro Controller, Joy-Con pair | One Joy-Con held sideways | Keyboard (unchanged: a USB keyboard on the dock, local builds, the replays) |
|--------|--------------------------------------------------|---------------------------|----------------------------------------------------------------------------|
| Move, aim | D-pad or left stick | stick | arrows (player 1) |
| Fire | A, or ZR | right button, or SR | Left Ctrl |
| Jump | B | bottom button | Left Alt |
| Cow bomb | Y, or ZL | left button, or SL | Space |
| Pause | + or − | its + or −, or a click of the stick | Escape |

X, L and R stay free for the controls menu.

### 4.3 Menus and other screens

| Action | Switch |
|--------|--------|
| Move the focus, change an option | D-pad or left stick (player 1) |
| Confirm | A (and ZR: player 1's fire confirms, as in 2002) |
| Back one page; on the pause menu's first page, resume | B |
| Pause, resume | + or − |
| Skip a cutscene | + or − |
| Dialogue | any button shows the next box; + or − skips the dialogue |
| Briefing, level summary, title, game over | any button |
| Character choice | left, right, then A |
| Hi-score name | the system keyboard (buttons or touch) |

Back is a pad button only: keyboard Escape keeps its 2002 meaning (the main menu ignores it), so the replays' menus do not change.

### 4.4 Players and controllers

- Handheld: player 1 plays with the console's Joy-Cons. Players 2 to 4 need other controllers, always on a Switch Lite.
- Starting a game of 2 to 4 players opens the system's controller applet for that many players, single Joy-Cons allowed: each player presses L and R (or SL and SR) on their controller, whose lights then show their number. Pad *n* is the system's player *n*, so player 3 plays with pad 3.
- Two Joy-Cons held sideways are two players; two pairs, four players.
- Pitfall: SDL's pad 1 merges the console's Joy-Cons and the system's player 1, so in handheld mode a wireless controller that becomes player 1 also moves Blip. The applet sorts the controllers out at the start of a game of several players; check on a console what it does with the attached Joy-Cons.

### 4.5 Controls menu

Used as today: choose an action, press the input. The names: `P1 A`, `P2 ZR`, `P3 UP` (the pad's number is the player's, shown by the controller's lights) instead of `JOY1 BUTTON1`; `PLUS` and `MINUS` if the menu font has no + or −.

### 4.6 Work

- `codes.hpp`: two codes for ZL and ZR (for example 32 and 33, past SDL 2's 21 buttons, which a later SDL could extend).
- `Sdl2Input`: the trigger axes become those buttons, with hysteresis (pressed beyond half their travel, released under a quarter). A case in `tests/conformance/sdl2_input_test.cpp`.
- `Bindings`: the Switch layout as the default (`defaultPadCodes(player, action)`): fire `kPadButtonB` (right: A) and ZR, jump `kPadButtonA` (bottom: B), cow bomb `kPadButtonX` (left: Y) and ZL. `InputState::aliasHeld()` checks both inputs of an action. Cases in `tests/unit/input_test.cpp`.
- `DeviceInput`: a pad's right button (A) acts as Return; a new `Input::backHeld()` is its bottom button (B).
- `menus.cpp`: back goes up one page (a controls page to the options page, the options, start and exit pages to the first page); on the pause menu's first page it resumes.
- `input_names.cpp`: `displayName()` with the Switch's names for pad buttons.
- The `fourplayers` replay: `tools/replays/make_replays.py` writes the Switch layout's codes for players 3 and 4 (fire `JOY3_B1` instead of `JOY3_B2`) and the replay is generated again. Its expected fingerprints should not change, the players doing the same things: that checks the new layout. If they do change, the cause is the confirm button, which moves from jump to fire, pressed in a menu; the expected fingerprints are then recorded again (a meant change).
- Controller applet: `InputSource::requestPlayers(count)`. On the Switch, `hidLaShowControllerSupport()` with `count` as minimum and maximum and single Joy-Cons allowed; on a Mac or Linux and in deterministic runs, nothing. The campaign calls it before a game of 2 to 4 players.
- Check on a console how often a single Joy-Con's stick click pauses by accident; if it does, the Switch code can drop Start for single Joy-Cons (`hidGetNpadStyleSet()`).

## 5. System integration

- **Entry point** (`src/platform/sdl2/switch/switch_main.cpp`, in place of `sdl2_main.cpp` on the Switch): `romfsInit()`, which mounts the RomFS of the `.nro` and of the installed title alike; the current directory set to `sdmc:/switch/blipblop/` (an installed title has none of its own, a `.nro` has its folder); in debug builds `socketInitializeDefault()` and `nxlinkStdio()`, so that the log's warnings reach the PC's terminal; `appletLockExit()`, so that closing the game from the HOME menu reaches it as `SDL_QUIT` and the settings are saved; the reverse at exit. An installed title starts without arguments (`argc` is 0; `Session::init()` reads from `argv[1]` on, so nothing to change). Every file that includes `<switch.h>` lives in `src/platform/sdl2/switch/`.
- **Files**: the preferences directory is `sdmc:/switch/blipblop/` (created when missing): `settings.cfg`, `hiscores.scr`, `BlipBlop.log`. The `.nro` and the `.nsp` share it: the same settings and hi-scores whichever starts. The pack is read from `romfs:/blipblop.bbpack`; a `data` folder in that directory still comes first (the 2002 data, mods).
- **Saving**: the settings when the options or controls pages are left, the hi-scores after a name entry, and both at exit (deterministic runs still never write). A console that sleeps for days or runs out of battery keeps them.
- **HOME and sleep**: the Switch code hooks the applet's focus messages (`appletHook()`, `AppletHookType_OnFocusState`) and sends `FocusLost` and `FocusGained`, so the game pauses as when a window loses the focus. The system suspends the game while it is in the background: check that the loss reaches `DeviceInput` in an update of its own, before the gain (a loss and a gain in the same update cancel out).
- **Errors**: `System::showError()` and `showWarning()` show the system's error applet (`errorApplicationCreate()`, `errorApplicationShow()`).
- **Hi-score name**: the screen asks the input for a text (`Input::beginTextEntry(19)`). Where the platform has an on-screen keyboard (`SDL_HasScreenKeyboardSupport()`: the Switch), the system keyboard appears and the screen waits for the text; letters go to capitals, characters the menu font lacks are dropped. In local Mac and Linux builds, and in deterministic runs (recordings included), the key-by-key entry stays: a recording made on the Switch enters an empty name with +.
- **Language**: on the first run (no settings file), French when the console's language is French or Canadian French (`setGetSystemLanguage()`, `setMakeLanguage()`), English otherwise.
- **"Press a key to start."** (string 852, under the briefings) becomes "Press a button to start." and its French. The briefings' frames change: the replays' expected fingerprints are recorded again, in a commit with that change only.
- **Audio**: the mixer runs at the device's rate (48 kHz on the Switch) instead of 44.1 kHz, so SDL converts nothing; check that the sound comes back after sleep.
- **Memory**: `System::peakMemoryBytes()` uses `getrusage()`, which the Switch's C library may lack: there, the memory in use from `svcGetInfo()` (`InfoType_UsedMemorySize`).
- **`std::filesystem`** (`Sdl2FileSystem::kind()` and `list()`): check it with devkitA64's library on `romfs:/` and `sdmc:/` in step S1; otherwise `stat()` and `opendir()` in the Switch build.

## 6. Build and packaging

### 6.1 Tools

devkitPro's pacman. This Mac has `/opt/devkitpro` (devkitARM, devkitPPC) but not the Switch packages yet:

```sh
sudo dkp-pacman -S switch-dev switch-sdl2   # devkitA64, libnx, switch-tools (elf2nso, npdmtool, nacptool, nxlink), the CMake files; SDL 2 with Mesa and nouveau
```

The Switch build also compiles `bbassets` for the machine it runs on (section 6.2): a C++17 compiler and CMake, already there on this Mac (and in the CI container). Local test builds need SDL 2 as today (Homebrew, or the distribution's package).

For the `.nsp` only:

- **hacBrewPack**, which makes the NCAs and the NSP from a homebrew's parts. It is not part of devkitPro; its original repository was deleted and mirrors keep it (for example github.com/rlaphoenix/hacBrewPack). GPL 2, but a build tool: nothing of it goes into the game. Built from source with `make`.
- **The keys** it encrypts with, `header_key` and `key_area_key_application_00` (key generation 1, hacBrewPack's default, which every firmware runs), dumped from the maintainer's own console (Lockpick_RCM) into `~/.switch/prod.keys`, where hacBrewPack looks by default. They are Nintendo's: never in the repository, an artifact, a log or CI. `.gitignore` gets `*.keys`, `keys.dat`, `keys.txt` and `keys.ini` against accidents.

### 6.2 CMake

- Presets: a new `switch` (below) makes the `.nro` and the `.nsp`; `default`, `release` and `asan` stay for local Mac and Linux builds.
- `BB_TOOLS_ONLY`, a new option: `bb::core`, `bb::game`, the platform's interfaces, `bbassets` and the runtime pack, without SDL 2, the game executable, the tests or the editor.
- The `switch` preset builds `bbassets` and the pack first, for the machine it runs on (`ExternalProject_Add` of this same source tree with `BB_TOOLS_ONLY=ON`, in `build/switch/host-tools`), then the game for the Switch. One configure and one build from a fresh clone, with no other build before. `BB_SWITCH_PACK` can name another pack.

```json
{
  "name": "switch",
  "displayName": "Nintendo Switch (homebrew .nro and .nsp)",
  "binaryDir": "${sourceDir}/build/switch",
  "toolchainFile": "$env{DEVKITPRO}/cmake/Switch.cmake",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "RelWithDebInfo",
    "BB_BUILD_TESTS": "OFF",
    "BB_BUILD_TOOLS": "OFF"
  }
}
```

Under `if(NINTENDO_SWITCH)` (set by the toolchain):

- `cmake/BBFindSDL.cmake`: SDL 2 through pkg-config, linked statically (EGL, OpenGL ES, Mesa, libnx come with it).
- No extraction of `data.zip`. Tests and tools stay off: they run on a Mac or Linux.
- `src/platform/CMakeLists.txt`: the files of `sdl2/switch/` in `bb_platform_sdl2`; `switch_main.cpp` as the entry point.
- Debug builds also put `tests/replays/` in the RomFS.
- `BB_SWITCH_TITLE_ID` (section 6.4) and `BB_SWITCH_KEYS`, by default `~/.switch/prod.keys`.
- Warnings stay errors: fix what devkitA64's GCC reports.

### 6.3 The .nro: development and the Homebrew Menu

The `.nro`, in the way of ftpd's `CMakeLists.txt` (a sketch):

```cmake
add_custom_target(blipblop_pack DEPENDS "${BB_SWITCH_PACK}")
dkp_set_target_file(blipblop_pack "${BB_SWITCH_PACK}")
dkp_add_asset_target(blipblop_romfs "${CMAKE_BINARY_DIR}/romfs")
dkp_install_assets(blipblop_romfs TARGETS blipblop_pack)

nx_generate_nacp(blipblop.nacp NAME "Blip & Blop" AUTHOR "LOADED Studio" VERSION "${BB_PACKAGE_VERSION}")
nx_create_nro(blipblop NACP blipblop.nacp ICON "${CMAKE_SOURCE_DIR}/assets/switch/icon.jpg" ROMFS blipblop_romfs)
```

`assets/switch/icon.jpg`: 256x256, made from the title picture, without EXIF data (section 6.4).

```sh
cmake --preset switch && cmake --build --preset switch   # build/switch/blipblop.nro, about 100 MB (bbassets and the pack first)
nxlink -s build/switch/blipblop.nro                      # to a Homebrew Menu waiting on Y (NetLoader)
```

nxlink passes the arguments after the `.nro` to the game (`--replay`, `/cheat`; a USB keyboard on the dock gives the cheat keys); if nxlink takes the game's `--` options for its own, put `--` before them. On the SD card: `sdmc:/switch/blipblop/blipblop.nro`. The Homebrew Menu started in application mode (hold R while starting any game) has the console's full memory; started from the Album (applet mode) it has less: check that the game, the keyboard and the controller applet work there too.

### 6.4 The .nsp: the release

hacBrewPack packs four folders:

| Folder | Content | Made from |
|--------|---------|-----------|
| `exefs/` | `main`, the program as an NSO; `main.npdm`, its permissions | the ELF through `elf2nso`; `cmake/switch/npdm.json` through `npdmtool` |
| `control/` | `control.nacp`: "Blip & Blop", "LOADED Studio", the version; `icon_AmericanEnglish.dat`, the icon | `nx_generate_nacp()`; `assets/switch/icon.jpg` (256x256 JPEG without EXIF data, which can leave the title stuck "installing") |
| `romfs/` | `blipblop.bbpack` | the `.nro`'s RomFS folder |
| `logo/` | none: `--nologo`, a black screen while the game loads | — |

- `cmake/switch/npdm.json`: hacBrewPack's `template/npdm.json` (an application: the application memory pool, every service, the whole file system), named `BlipBlop`.
- The title ID, `BB_SWITCH_TITLE_ID`, a default of the CMake files (committed: it is not a secret): chosen once, at random, in lower case, in the range hacBrewPack recommends (`0x0100000000000000` to `0x01ffffffffffffff`), ending in `000` (an application's own ID; updates take +0x800); checked against public lists of games' title IDs; then never changed: it is the identity of the installed game. hacBrewPack writes it into the NCAs, the NPDM and the NACP.
- A `blipblop_nsp` target, outside `ALL` (it needs the keys): it fills `build/switch/nsp/exefs` and `nsp/control`, runs hacBrewPack and names the result after the version:

```sh
hacbrewpack -k ~/.switch/prod.keys --titleid <BB_SWITCH_TITLE_ID> \
    --exefsdir nsp/exefs --controldir nsp/control --romfsdir romfs \
    --nologo --nspdir nsp     # nsp/<title id>.nsp, renamed blipblop-<version>.nsp
```

- The command line, `tools/switch/make-nsp.sh`, runs the whole chain, as the scripts of `tools/docker` do for theirs:

```sh
tools/switch/make-nsp.sh                   # build/switch/blipblop-<version>.nsp (about 100 MB), from the working tree
tools/switch/make-nsp.sh --upload v1.2.0   # the same for a release, then attached to it
```

  It checks its tools (`DEVKITPRO`, hacBrewPack, the keys), configures and builds the `switch` preset (which makes `bbassets` and the pack itself), builds `blipblop_nsp`, and prints the file's path and SHA-256. With `--upload <tag>` it first checks that the working tree is clean and at that tag, takes the version from the tag (`v1.2.0` gives 1.2.0, as `release.yml` does with `${TAG#v}`), and ends with `gh release upload <tag> build/switch/blipblop-<version>.nsp`.
- Installing: with an NSP installer homebrew (for example Goldleaf), from the SD card or over USB. The game then appears on the HOME menu with its icon and name, and is deleted in System Settings > Data Management. A newer version installs over the older one (same title ID); no update packages (patch NSPs).
- The console must accept titles that Nintendo did not sign: an NCA's first header signature needs Nintendo's private key (hacBrewPack signs the second with its own). That depends on the console's custom firmware set-up, outside this project; the `.nro` has no such need, and each release carries it (section 6.5).

What changes for the game when it is installed:

| | `.nro` (Homebrew Menu) | `.nsp` (installed) |
|---|---|---|
| Started from | the Homebrew Menu | the HOME menu, like any game |
| Process | the Homebrew loader's, with its permissions | its own, with `main.npdm`'s |
| Memory | full in application mode, less from the Album | full |
| RomFS | appended to the `.nro` | the title's |
| Arguments | from nxlink, if any | none |
| Current directory | the `.nro`'s folder, then `sdmc:/switch/blipblop/` | `sdmc:/switch/blipblop/`, set by the entry point |
| Settings, hi-scores, log | `sdmc:/switch/blipblop/` | the same folder |

GPU, audio, controllers, keyboard and applets must work in a process of the game's own: step S1 packs a first `.nsp` early for that. If something fails there and not in the `.nro`, the NPDM is the first suspect (permissions, system resource size).

### 6.5 CI and releases

CI builds the `.nro` and nothing else; the maintainer packs the `.nsp` with a command line (decided on 30 October 2026).

- `ci.yml`, on every push and pull request: one job, `switch`, in the `devkitpro/devkita64` container (Debian with a C++ compiler and CMake, the Switch packages and SDL 2): `cmake --preset switch && cmake --build --preset switch`, which also builds `bbassets` and the pack in the container; `blipblop.nro` kept as an artifact.
- `release.yml`, on a tag `v*` (or by hand for an existing tag): the same build with the tag's version (`-DBB_PACKAGE_VERSION="${TAG#v}"`), zipped as `blipblop-<version>-switch-nro.zip` (the folder `switch/blipblop/`, for the SD card's root); a step on the runner itself attaches it (the container has no `gh`), and the draft release is published. The maintainer then runs `tools/switch/make-nsp.sh --upload v<version>` on their machine, at the tag. A line of `docs/release-checklist.md` says so.
- No tests run in CI: they run on a Mac or Linux before each merge and each release (section 8).

### 6.6 Retiring the desktop targets

Step S2, once CI builds the `.nro` (S1), so that CI is never left empty.

**Removed**

- `ci.yml`: the Linux, macOS and Windows jobs (builds, tests, replays, packages, replays from the packages, the Windows benchmark), the sanitizer job and the static-analysis job.
- `release.yml`: the desktop package matrix.
- `cmake/BBPackaging.cmake` (CPack: NSIS installer, zip, disk image, `.tar.gz`) and its `include` in `CMakeLists.txt`; `cmake/BBInstallWindowsDlls.cmake`; `cmake/Info.plist.in`.
- `tools/macos/sdl2-universal.sh`, `tools/replays/package-replay.sh`, `tools/docker/windows-cross-check.sh`, `tools/docker/mingw/`.
- Windows: the MSVC branches of `cmake/BBWarnings.cmake` and `tools/CMakeLists.txt`, `SDL2main` in `cmake/BBFindSDL.cmake`, `psapi` in `src/platform/CMakeLists.txt`, the `_WIN32` branches of `sdl2_platform.cpp` (with the DPI hints), `src/app/session.cpp` and `tools/editor/editor_app.cpp`.
- The VSYNC option, F10 and the `fullscreen` setting (section 3.4).
- The desktop parts of `README.md` (Windows, packages, desktop releases) and of `docs/release-checklist.md`.

**Kept, for local builds on a Mac or Linux**

- The presets `default`, `release` and `asan`: the game (to try things, and for `bbedit`'s test play), `bbassets`, `bbedit`, the unit and conformance tests, the 14 replays, the benchmarks of `tests/bench`, `tools/docker/linux-check.sh` and `linux-bench.sh` (every test in a Linux container, from a Mac).
- The SDL 2 backend: devkitPro's SDL 2 on the Switch, Homebrew's or the distribution's on a Mac or Linux.

**Rewritten**

- `README.md`: the Switch (installing the `.nsp` or the `.nro`, the controls, where the settings are), building the `.nro` and the `.nsp`, the local builds for tests.
- `docs/architecture/platform.md`: the SDL 2 backend on the Switch and on a Mac or Linux.

## 7. Steps

Rough sizes, for one developer who knows the code. Each step keeps the local tests and the replays green on a Mac (or Linux), and notes its results in `docs/progress.md`.

### S1 — Toolchain and a first start (3–4 days)

The preset, `BB_TOOLS_ONLY` and the host-tools build, the CMake branch, the entry point, the RomFS, the preferences directory, the `std::filesystem` and `getrusage()` checks, the `.nro` with its icon and metadata, the `switch` job of `ci.yml`; hacBrewPack, the keys, the title ID and a first `.nsp`, packed by hand.

**Done when:** `cmake --preset switch && cmake --build --preset switch` makes the `.nro` from a fresh clone, on the Mac and in CI; the `.nro` starts from the Homebrew Menu and the first `.nsp`, installed, from the HOME menu; both play the intro and the attract mode with sound; the log on the SD card says `Game data: romfs:/blipblop.bbpack`.

### S2 — Switch only (1–2 days)

Section 6.6.

**Done when:** CI runs the `switch` job only; `release.yml` attaches the `.nro` zip and nothing else; a fresh clone still builds, tests and replays on the Mac (`cmake --preset default`, `ctest --preset default`); the README describes the Switch.

### S3 — Display (3–4 days)

Section 3.

**Done when:** the sizes of 3.1 are measured on a console (handheld 960x720, docked 1440x1080, INTEGER docked 1280x960), docking and undocking during a level and in a menu each write a `Display:` line, CRISP shows square pixels that do not shimmer when the level scrolls, and the local tests pass.

### S4 — Controls (3–4 days)

Section 4.

**Done when:** the tables of 4.2 and 4.3 hold with the console's Joy-Cons, a Pro Controller, a Joy-Con pair and single Joy-Cons; a four-player game is played with four Joy-Cons; the controls menu shows the Switch's names and a binding survives a restart; the 14 replays keep their fingerprints, `fourplayers` generated again.

### S5 — System integration (3–4 days)

Section 5.

**Done when:** HOME and sleep pause the game; closing it from the HOME menu keeps the settings; a hi-score name is typed with the system keyboard; a console in French starts the game in French; an error shows in the error applet.

### S6 — The .nsp (1–2 days)

Section 6.4: the `blipblop_nsp` target, the NPDM and control folders, the logo choice, `tools/switch/make-nsp.sh`.

**Done when:** `tools/switch/make-nsp.sh` makes `blipblop-<version>.nsp` from a fresh clone (with the tools and keys in place); it installs, shows its icon and name on the HOME menu, plays a level with sound and every controller, shares its settings and hi-scores with the `.nro`, closes from HOME, installs over itself with a newer version and is deleted from Data Management.

### S7 — Verification and release (2–3 days)

The release job of section 6.5; the replays on a console (section 8); the Switch's `docs/release-checklist.md`.

**Done when:** a tag produces the `.nro` zip, `tools/switch/make-nsp.sh --upload` attaches the `.nsp` of the same tag, and the checklist passes on a console, docked and handheld.

## 8. Tests

- **Locally, on a Mac or Linux** (no tests in CI): `ctest --preset default` (unit and conformance tests, the 14 replays), and `ctest --preset asan` for the sanitizers, before each merge to `master` and each release; `tools/docker/linux-check.sh` runs them all in a Linux container from the Mac. New cases: the viewport function (the sizes of 3.1), the Switch layout and its second inputs, the Switch's names, `filter crisp` in the settings.
- **On a console**: each replay through nxlink, `--replay romfs:/replays/<name>.bbreplay --hash sdmc:/switch/blipblop/<name>.bbhash`, the hashes compared on the Mac with `tests/replays/expected/`. The same fingerprints show that the game plays the same on the Switch's ARM processor. The same runs print the benchmark line (`bench: ... ms per frame ... peak memory`): a frame, drawing and ticks, should stay well under the 16.7 ms of a 60 Hz frame (aim for half). If SDL's dummy video driver is not in the Switch build, the replays run with the display.
- **By hand** (`docs/release-checklist.md`): the sizes of 3.1 on the console's screen and on a TV; docking and undocking during a level and in a menu; every controller style; 1 to 4 players; HOME, sleep, closing from HOME; a name entry; French; an hour of play (heat, memory); the `.nsp`: install, icon and name on the HOME menu, settings shared with the `.nro`, install of a newer version over it, deletion; the `.nro` from the Album.

## 9. Risks

| Risk | Answer |
|------|--------|
| The installed `switch-sdl2` differs from the source read for 2.2 | S1 shows the real behaviour early; the plan leans on few of its details (the window's size, the mapping, text input). |
| Software drawing too slow for the Switch's processor (Cortex-A57 at about 1 GHz) | Measured in S1 with the replays' benchmark line. The 2002 game drew the same frames on Pentium-class PCs. |
| A regression reaches `master` unseen: CI runs no tests | The local tests before each merge and each release (section 8); the release checklist asks for them. |
| The local Mac or Linux build breaks unseen: CI no longer builds it | It is the maintainer's daily build; `tools/docker/linux-check.sh` covers Linux from the Mac. |
| The `.nsp` fails where the `.nro` works (a process of its own, the NPDM's permissions) | A first `.nsp` in S1; the NPDM is the first suspect. |
| The keys leak (they are Nintendo's) | Only on the maintainer's machine; `.gitignore`; never in CI. |
| A title ID that a real game uses | Random, checked against public lists, chosen once (6.4). |
| Consoles that cannot install the `.nsp` | The `.nro` in each release (section 6.5). |
| The loss and return of the focus arrive in the same update after a suspension | The Switch code holds the return back by one frame. |
| Closing from HOME ends the process without a quit | Saving on change (section 5) keeps the settings anyway. |
| Less memory when the `.nro` starts from the Album | The `.nsp` always has the full memory; 85.6 MB at the peak on the desktop. |
| A single Joy-Con's stick click pauses the game | Section 4.6. |
| The console's Joy-Cons and a wireless player 1 merged in pad 1 | The controller applet (section 4.4). |

## 10. Decisions

**Decided by the maintainer (30 October 2026)**

- A homebrew release, as an `.nsp`, with the `.nro` next to it.
- The Switch is the only target. CI builds the `.nro` and nothing else; the `.nsp` is packed with a command line, `tools/switch/make-nsp.sh`. Builds on a Mac or Linux stay for local tests, outside CI and releases; nothing builds for Windows any more.
- On the Switch: A fires, B jumps, Y drops the cow bomb; ZR fires, ZL drops the cow bomb; + or − pauses.

**To confirm** (the recommended choice; any of them can change without touching the rest of the plan)

1. CRISP as the default filter; SHARP and SMOOTH remain.
2. Docked: FIT at 1440x1080 by default; INTEGER at 1280x960 as the option.
3. The system keyboard for hi-score names.
4. Later, not planned: a picture in the bars instead of black (from the game's art), the game's own logo and animation on the `.nsp`'s loading screen (hacBrewPack's `logo/` folder) instead of black, rumble when a player is hit or drops a cow bomb, touch to confirm in the menus, a widescreen mode.

## 11. Departures from the plan (implementation, 30 October 2026)

- The conformance test of the display's scaling (`tests/conformance/sdl2_display_test.cpp`) could not pass unchanged: it read SDL's logical-size viewport, which the display no longer uses. It now checks the pixels shown: the bars, the whole factors, CRISP's edges.
- The game's text lives twice: in `assets/source/strings` (the pack) and in `data.zip`'s `uk.dat` and `fr.dat` (the 2002 data, which the local replays play too). `tools/data/fix_legacy_data.py` now also writes the Switch's two text changes (CRISP, id 862; "Press a button to start.", id 852) into `data.zip`.
- The `menus` replay lost its VSYNC taps (the item is gone); idle frames of the same length take their place, so that everything after the options page happens on the same frames as before.
- libnx's controller applet has no switch for single Joy-Cons: it offers them when the controllers' style set allows them, which SDL configures (with the horizontal hold); `enable_permit_joy_dual` allows the pairs.
- The NPDM may set one debug flag only (devkitPro's `npdmtool` refuses `allow_debug` with `force_debug`; hacBrewPack's template has both): `allow_debug`.
- `release.yml` installs Debian's GitHub CLI in the devkitPro container to attach the `.nro`, rather than handing it to a step on the runner.
- CI builds the `.nro` only; the `.nsp`'s parts are built by `tools/switch/make-nsp.sh` (target `blipblop_nsp_parts`).
- To check on a console, beyond section 8: after the game's controller applet, SDL may open its own when a controller's style changed (it does so outside handheld mode).
