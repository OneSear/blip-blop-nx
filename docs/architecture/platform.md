# The platform layer

The game runs on the Nintendo Switch, and on a Mac or Linux for local tests,
through one backend, SDL 2 (`src/platform/sdl2`): devkitPro's SDL 2 on the
Switch, with the few libnx calls SDL does not make in `sdl2/switch/`
(switch-port.md), and Homebrew's or the distribution's SDL 2 elsewhere.
Another backend (SDL 3, a console SDK) would implement the same interfaces;
nothing outside `src/platform` and `src/app/main.cpp` would change.

## What a backend provides

All in `src/platform/include/bb/platform/`:

| Interface | Responsibility |
|-----------|----------------|
| `Platform` | Creates the `Display` and gives the other services. |
| `Display` | Shows a finished 640x480 frame (`present(const core::gfx::Pixmap&)`), in step with the screen, scaled to it with its shape kept (`core::gfx::viewport()`: fit, or by a whole factor of 2 or more) and filtered (SHARP, CRISP or SMOOTH); the refresh rate. On the Switch the window is the screen: 1280x720 in handheld mode, 1920x1080 docked, followed live. A headless display (tests) presents nothing. |
| `InputSource` | The keyboard, pad and window events (`poll()`), with key codes of `bb::core::input` (`codes.hpp`); the pads' slots and names; the system's controller applet before a game of several players (`requestPlayers()`); its on-screen keyboard (`beginTextEntry()`, the text as a `Text` event). A focus loss and its return seen in the same update are delivered in two (a suspended Switch game reads both when it comes back). |
| `Audio` | Sounds from complete WAV files (32 voices, looping or not) and one streamed music at a time (Ogg Vorbis or MP3); `available()` false when there is no device. The SDL backend opens a plain SDL audio device and fills it from the platform-free `bb::core::audio::Mixer` (Phase 9), at the device's rate (48 kHz on the Switch), locking the device around every other call. |
| `Clock` | Microseconds since start, and sleeping. |
| `FileSystem` | Files by UTF-8 path: opening for reading at any offset, a file's kind, a directory's entries (`core::FileReader`, what `Vfs` and the pack read through), writing whole files (settings, hi-scores, recordings); the game's directory (`romfs:/` on the Switch) and the user's preferences directory (`sdmc:/switch/blipblop/` on the Switch). The SDL backend uses `SDL_RWops`. The log and the debugging outputs (`--hash`, `--trace-ticks`) are still written with `fopen`. |
| `System` | Message boxes for errors and warnings (the Switch's error applet); the process's peak memory (the headless benchmark line; on the Switch, the memory in use); the system's language (the game's on its first run). |

And the process entry (`entry.hpp`): the backend owns `main()` (or what the
system calls instead) and calls `bbMain(argc, argv)`. On the Switch,
`sdl2/switch/switch_main.cpp` first mounts the RomFS, sends the standard
output to nxlink when it started the game, locks the HOME menu's exit (so
that closing the game reaches it as a quit) and makes `sdmc:/switch/blipblop/`
the current directory.

## Steps

1. Create `src/platform/<name>/` with an implementation of the interfaces
   and a `create<Name>Platform()` factory, as `sdl2_platform.cpp` does.
2. Add a library target in `src/platform/CMakeLists.txt` linking
   `bb::platform` (the interfaces) and the system's libraries; the only
   files that may include them are in that directory.
3. Give the backend its entry point (`sdl2_main.cpp` is the model) and let
   `src/app/CMakeLists.txt` link it instead of SDL 2's.
4. In `src/app/main.cpp`, create the new platform instead of
   `bb::platform::sdl2::createPlatform()`.
5. Check it: the conformance tests (`tests/conformance`) compare a
   backend's display and audio with the SDL behaviour the replays were
   recorded with; the replays must keep their fingerprints (they do not
   depend on the backend: frames are fingerprinted before they are
   presented).

## Notes

* The pixel layout is the one of `core::gfx::Pixel` (bytes R, G, B, A).
* Input codes: keys below 255, `0x40000000 | (key & 0xFFF)` for the other
  keys, `((pad + 1) << 10) | button` for pads (100 to 103 for the
  directions); `bb::app::DeviceInput` turns the events into these. A pad
  the backend recognises as a game controller (`padIsController()`)
  reports the button numbers of `codes.hpp` (`kPadButtonA`...), by
  position, its D-pad as a hat, its left stick as axes 0 and 1 and its
  triggers as the buttons `kPadButtonLeftTrigger` and
  `kPadButtonRightTrigger`, whatever the device; the SDL 2 backend uses
  SDL's game controller database for this, with Nintendo's controllers by
  position too (`SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS` off). The
  default pad layout of each player (`Bindings::defaultPadCodes()`) is the
  Switch's: the right button (the Switch's A) fires, the bottom one (B)
  jumps, the left one (Y) drops the cow bomb, ZR and ZL too.
* On the Switch, SDL always reports 8 pads, the system's players 1 to 8;
  its first reads the console's Joy-Cons and the system's player 1 together.
* A deterministic run never reads the clock or the input of the backend
  directly: the session gives virtual time and the recorded input.
