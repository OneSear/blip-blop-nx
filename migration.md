# Blip & Blop — Migration Plan: C++17 / SDL2 / CMake

*Status: plan only, no code changed. Written against commit `44cab36` (master), October 2026. Fact-checked against the code on 11 October 2026 (baseline build reproduced, every data file parsed, both forks cloned, benchmark re-run); the corrections from that review are applied in this revision.*
*Scope of the study: every source file of the game (`vc-projects/Blip_n_Blop_3`, ~51.6k lines), the four editor tools, the shipped `data.zip`, the level-editor source folders, and the git history.*

---

## Table of contents

1. Goals and non-goals
2. What we have today (as-is assessment)
3. Target architecture
4. Migration strategy
5. Phased plan
6. Testing and verification
7. Risks and open questions
8. Decision log
- Appendix A — File formats (reverse-engineered)
- Appendix B — Glossary French → English
- Appendix C — Inventory tables (IDs, worlds, levels, banks)
- Appendix D — Defects and hazards found during the read
- Appendix E — Content catalogue (enemies, projectiles, props)
- Appendix F — Related forks and what to take from them
- Appendix G — Measurement log (environment, method, raw results)

---

## 1. Goals and non-goals

### 1.1 Goals

| # | Goal | What it means concretely |
|---|------|--------------------------|
| G1 | **C++17, SDL2, CMake** | One CMake build for Windows, macOS, Linux; no Visual Studio project files; no vendored SDL binaries; C++17 everywhere. |
| G2 | **One language in the code: English** | Identifiers, file names, comments, log messages and commit messages in English. French stays only where it belongs: in *game data* (dialogue, menu strings) as a selectable language. |
| G3 | **Real object-oriented design** | Encapsulated classes with invariants, RAII ownership, interfaces for platform services, no global mutable state, no `void*` lists, no C-style casts, `enum class` instead of `#define` families, composition for shared behaviour. |
| G4 | **Portable core with minimal dependencies** | A `core` library that depends on the C++17 standard library only. SDL2 lives in a thin platform layer. **Supported targets: Windows, macOS and Linux** (decision of 16 October 2026). Portability stays a design goal — platform-free `core` and `game`, one thin backend per platform, no OS-specific code outside `platform/` — so that another platform would need only a backend, but no other port is planned. |
| G5 | **Modern, documented data pipeline** | Source assets in open formats (PNG, WAV/OGG, JSON/UTF-8 text) with a build step producing compact runtime packs. Every runtime format versioned and documented. |
| G6 | **Keep the game playable at every step** | No big-bang rewrite. Each phase ends with a game that runs the full campaign and passes regression replays. |

### 1.2 Non-goals (for this migration)

- Changing the gameplay, the feel, the 640×480 pixel art, or the 85 Hz simulation rate (see §3.10 — any retuning is a separate, later decision).
- Rewriting the MFC level editor. It stays the authoring tool until a converter and, optionally, a new editor exist (Phase 8).
- Online features, achievements, new content, HD art.
- Console, handheld and retro-gaming ports (Nintendo Switch, original Xbox, Miyoo Mini, Anbernic-class handhelds, Android). They were targets in the first version of this plan and were dropped on 16 October 2026; the portable architecture keeps them possible, but nothing in the migration is tuned for them (no 16-bit pixel path, no console toolchains, no in-house mixer requirement).
- Store distribution. The game parodies copyrighted characters (Smurfs, Snorks, Pokémon, Care Bears, Lemmings, Mario, Sonic, Pac-Man, Lara Croft, Rayman…); it can only be distributed for free. See §7.

---

## 2. What we have today (as-is assessment)

### 2.1 Repository inventory

```
blip-blop/
  assets-gfx/smurf-sprites/      425 source BMPs for the Smurf world + a .pbk list with absolute D:\ paths
  level-editor/                  13 level source folders: 640×480 BMP screens, sprite BMPs, bbe_*.ini descriptors,
                                 and a prebuilt BB3_level_editor.exe copied into each folder
  vc-projects/
    Blip_n_Blop_3/               THE GAME: 182 .cpp + ~250 headers, Engine/ shim, CMakeLists.txt, data.zip (53 MB),
                                 VC6 (.dsp) + VS2013 (.vcxproj) projects, vendored SDL2 2.0.1 Windows binaries
    BB3_level_editor/            MFC level editor, ~7.0k lines (Latin-1 encoded sources)
    GFX Editor/                  MFC sprite-bank compiler, ~2.6k lines (BMP → LGX → .gfx; also .gfx → BMP import)
    FNT Editor/                  MFC font compiler, ~1.1k lines (BMP glyphs → .lft)
    SFX Editor/                  MFC sound-bank compiler, ~1.2k lines (WAV → .sfx)
```

Repository hygiene facts: 785 BMP files (787 on disk), 13 copies of the level-editor `.exe`, VC6 cache files (`.ncb`, `.opt`, `.plg`, `.aps`, `.clw`) and a 53 MB `data.zip` are tracked in git; there is no `LICENSE`, no top-level `README` and no `.gitignore`.

**Lines of code by layer** (game directory, headers included):

| Layer | Files (examples) | Lines | Share |
|-------|------------------|------:|------:|
| Platform shim (2013–2014 Linux port) | `Engine/*`, `sdl_*.h`, `graphics.*`, `dd_gfx.*`, `alpha_blend.*`, `fmod.h`, `l_timer.*`, `ben_divers.*`, `restore.*` | 4,170 | 8% |
| Asset / IO core | `picture*`, `lgx_packer*`, `fonte*`, `sound*`, `music_bank*`, `txt_data*`, `hi_scores*`, `config*`, `ben_debug*`, `ben_maths*`, `super_liste*`, `input*`, `key_translator*`, `scroll*` | 5,693 | 11% |
| Game orchestration & screens | `game.cpp` (4,391 lines), `globals*`, `blip_n_blop_3.cpp`, `menu_*`, `cine_player*`, `rpg_player*` | 8,392 | 16% |
| Entity framework | `sprite*`, `personnage*`, `joueur.h`, `enemy*`, `tir.h`, `tir_bb*`, `bonus.h`, `event*`, `gen_*`, `make_bonus*`, `control*`, `couille*` (player, 1,697 lines), `blip/blop`, `vehicule.*`, effects, weather | 5,506 | 11% |
| **Content: enemies** | `ennemi_*` — 64 classes (57 direct `Ennemi` subclasses + 7 derived from those) | 18,573 | 36% |
| Content: projectiles | `tir_*` — 31 classes | 3,897 | 8% |
| Content: background objects | `fond_*` — ~50 classes (props, moving platforms, set pieces) | 3,640 | 7% |
| Content: pickups, gore, debris, vehicles | `bonus_*`, `gore_*`, `morceau*`, `vehicule_*` | 1,753 | 3% |
| **Total** | 240 classes | **51,624** | |

Class hierarchy counts: 84 classes derive directly from `Sprite`, 57 from `Ennemi` (plus 7 from enemy subclasses: the 5 Smurf skins, flying Lemmings, Jedi hero — a naive grep says 58 because the stale `ennemi_snork_base_1.h` re-declares `EnnemiLemmings`), 25 from `Tir` + 6 from `TirBB`, 8 from `Bonus`, 17 from `Event`, 3 from `Vehicule`.

**Shipped data (`data.zip`, 124 MB uncompressed):**

| Type | Count | Size | Content |
|------|------:|-----:|---------|
| `.gfx` sprite banks | 56 | 91.3 MB | LGX-compressed 16-bit images with hotspots (decor screens, enemies, props, UI, briefings) |
| `.zik` music | 16 | 20.8 MB | Ogg Vorbis (converted from MP3 in 2014; two stale `.mp3` duplicates remain, 4.6 MB) |
| `.sfx` sound banks | 9 | 4.0 MB | Concatenated WAV files (PCM mono; 142 of the 175 sounds are 22,050 Hz 16-bit, the rest 8-bit and/or 44,100 Hz) |
| `.lvl` levels | 12 | 3.6 MB | Binary: asset names, screen sequence, per-pixel platform heights, 8×8 wall grids, event list sorted by activation X (descending in the file, see A.6) |
| `.lft` fonts | 8 | 85 KB | 255 LGX glyphs + line height + space width |
| `.mbk` music banks, `.rpg` dialogue scripts, `.cin` cutscene scripts, `.lst` campaign list, `.dat` strings | 12/7/2/1/1 | small | Plain text (single-byte Windows-1252) |
| `.cfg`, `.scr` | 1/1 | tiny | Binary config and obfuscated hi-scores |

Only `uk.dat` exists; `fr.dat` was never in the repository (checked both `data.zip` revisions). The campaign (`bb.lst`) is 12 levels in 7 worlds; see Appendix C.

### 2.2 Three generations of code in one tree

1. **2000–2002 original (LOADED Studio)** — Win32 + DirectDraw 7 (640×480×16, exclusive mode, 85 Hz CRT), DirectInput 8, FMOD 3.4. French identifiers and comments, English log strings. Author nicknames in headers (Prosper, Mephisto, Mortimus).
2. **2013–2014 Linux port (shim approach; every commit in the repository dates from November 2013 to March 2014)** — rather than re-architecting, `Engine/windows.h`, `ddraw.h`, `dinput.h`, `io.h` *fake the Win32/DirectX API* with stubs, and `sdl_surface.h` wraps `SDL_Surface` behind the old `IDirectDrawSurface7` method names (`BltFast`, `Lock`, `Release`…). FMOD calls are mapped onto SDL2_mixer in `Engine/windows.cpp`. Keyboard constants `DIK_*` are `#define`d to `SDLK_*`. This made the game run on Linux but froze the 2001 design (lost-surface handling, Windows message pump, VRAM ring buffer) in place and introduced its own bugs (Appendix D). Our starting point is the upstream state of 13 March 2014; the upstream repository (`benkaraban/blip-blop`) received 117 further commits in 2019 that this tree does not have — see Appendix F.
3. **VS2013 hybrid project** — links SDL2 *and* `dinput8.lib`/`winmm.lib` (Debug) or the full DirectX set (Release); it is unclear which configurations still build on Windows.

### 2.3 Architecture as-is

```
main() ─► InitApp()            : FMOD→SDL_mixer init, timer, config, hi-scores, strings, "DirectDraw" (SDL window),
                                  input, surfaces, LGX decoder, fonts, UI bank, UI music
       ─► Game::go()           : intro cutscene, attract-mode loop (menu / hi-scores / credits / intro cycling)
             └► jouePartie()   : load global banks, create players, iterate bb.lst
                   ├► briefing / cutscene screens (blocking loops)
                   └► joueNiveau() ─► chargeNiveau() (parse .lvl) ─► gameLoop() until win/death/skip
                                                                      ├► updateAll(): events, 21 lists, collisions
                                                                      └► drawAll():  scroll, lists, HUD, flip
```

Everything communicates through **global state** declared in `globals.h` (`vc-projects/Blip_n_Blop_3/globals.h`):

- 21 `SuperListe` lists (players, player shots, cow bombs, impacts, vehicles, sleeping/pending events, enemies, enemy shots, generators, pickups, pickup generators, animated/static/foreground props, moving platforms, banner texts, blood sprays, gore, weather, bubbles).
- 10 `PictureBank`, 4 `SoundBankBB`, 3 `MusicBank`, 8 `Fonte` instances.
- Scroll/camera (`offset`), level collision arrays (`y_plat`, `murs_opaques`, `murs_sanglants`), `game_flag[11]`, weather, screen shake, scroll locks, hold-fire.
- Plus singletons elsewhere: `in` (input), `debug` (log), `LGXpaker`, `graphicInstance`, `tete_turc` (the player enemies aim at), `hi_scores`, config booleans, `backSurface`/`videoA`/`systemSurface`.

Entities are a classic hierarchy (`Sprite` → `Personnage` → `Ennemi` / `Couille`; `Sprite` → `Tir` → `TirBB`; `Sprite` → `Bonus`, props, gore…) with `virtual update()` / `affiche()`, but: all members public, identity by `#define` integer IDs, three giant `switch` factories (`event_ennemi.cpp`, `event_fond_anime.cpp`, `event_bonus.cpp`), hard-coded sprite indices (`pbk_ennemis[168]` — 676 such sites for enemies alone) and sound indices, `rand()` at 375 sites (365 outside comments), and lifetime owned by `void*` lists that `free()` objects created with `new`.

### 2.4 Runtime model worth preserving (the good ideas)

- **Fixed-step integer simulation at ≈85 Hz.** `LGetTime()` returns 0.1 ms units; one logic tick is 118 units = 11.8 ms (`game.cpp:1254`). A 50-sample moving average ("glorf") decides between one update per frame and catch-up loops. All physics constants are integers per tick (gravity latency 7, max fall speed 4, walk 2 px…).
- **Level scripting by scroll position.** Each level is a list of events sorted by activation X (the order is load-bearing, see A.6); events spawn enemies, generators, props, platforms, pickups, vehicles, trigger dialogue, music, weather, scroll locks, flags, banner texts and sounds. Scroll locks release on conditions (no enemies left, no generators, flag equals/≥ value). Victory = camera reached X *and* two flag conditions.
- **Sprite banks with hotspots** (`.gfx`), bitmap fonts, sound banks, and text-scripted cutscenes (`.cin`) and dialogues (`.rpg`) — simple, data-driven, worth keeping.
- **Collision**: per-pixel platform heights on 8 layers (layers ≥ 6 are slippery), 8×8-cell solid-wall and blood-wall grids, AABB hitboxes derived from the current image.
- **Decals baked into the background** (`grave()`): corpses, blood, cow-bomb craters are blitted into the scrolling background cache and persist naturally.
- **Player model**: 18 aim directions with weapon inertia (`poid_arme`), 5 weapons with pre-allocated projectile pools, cow-bomb screen attack, double jump ("saleto", presumably salto/somersault), slippery platforms, respawn drop-in, invincibility blink.

### 2.5 Runtime model that must change

- **Blocking screens.** Menus, cutscenes, dialogues, level summary, hi-score entry, credits and attract mode each run their own nested render loop and busy-wait on input (`in.waitKey()`, `in.waitClean()` spin at 100% CPU; `Sleep(2000)` before each level). Console OS events (sleep, focus loss, controller changes) cannot be handled this way.
- **Windows-era plumbing**: `manageMsg()` message pump, `checkRestore()` for lost DirectDraw surfaces, `ChangeDisplaySettings` (the 85 Hz refresh-rate negotiation itself is already commented out, `blip_n_blop_3.cpp:363–389`; only `BEST_RATE` and the restore calls remain), the 840-pixel VRAM ring buffer (`scroll.cpp`) built around a 2001 GeForce bug.
- **Rendering**: 32-bit `SDL_Surface`s everywhere (the source art is 16-bit — memory doubles); every surface is created with an alpha mask and left in SDL's default *blend* mode, so all blits — even opaque full-screen copies — take SDL's generic per-pixel path (measured: 49–68 % of the main thread across two runs, §3.14); a texture created and destroyed every frame in `Graphics::Flip()`; the half-tone darkening ignoring its rectangle; overlapping self-blits for shake/deformation.
- **Audio**: FMOD vocabulary mapped onto SDL2_mixer with broken loop semantics (Appendix D), MOD/tracker code paths that no data uses, a 32-channel assumption.
- **Input**: raw `SDLK_*` codes stored in the config file, joystick state indexed by SDL instance IDs, a precedence bug that disables even-numbered joysticks.

### 2.6 Build baseline (measured on this machine: macOS 14, Apple clang 16, CMake 4.4, Homebrew `sdl2-compat` 2.32.72 providing the SDL2 API over SDL3)

| Step | Result |
|------|--------|
| `cmake` on the existing `CMakeLists.txt` | Fails: `cmake_minimum_required(VERSION 2.8)` is rejected by CMake ≥ 4 (needs the `CMAKE_POLICY_VERSION_MINIMUM=3.5` override). |
| Build, unmodified | **≈3,240 errors** (3,242 in the original run, 3,239 when reproduced with `-j4 -k`; the count depends on build order), all from two causes: `fmod.h` only neutralises `_stdcall`/`_cdecl` for Linux/Win32 (not `__APPLE__`), and `game.cpp` / `sound_bank.cpp` include the non-standard `<malloc.h>`. |
| Build with `-DPLATFORM_LINUX` and a two-line `malloc.h` shim (outside the repo) | **0 errors, 168 warnings**, links, **runs**: window opens, UI bank loads, intro cutscene plays. |
| Warning profile | 65 × `-Wreturn-type` (non-void functions falling off the end — undefined behaviour, can trap in optimised builds), 69 × deprecated declarations, 5 mismatched-header-guard warnings (3 files), 3 precedence, 1 tautological compare. |

Conclusion: the code is *already* valid C++17 for modern compilers; the migration cost is architecture and data, not language syntax.

### 2.7 Language mix assessment

| Where | Situation | Plan |
|-------|-----------|------|
| Class / member / function names | ~90% French (`Couille`, `Ennemi`, `Tir`, `estTouche`, `a_detruire`, `etape`, `ss_etape`, `tete_turc`…), 10% English (`Picture`, `PictureBank`, `Input`, `Game`, `LoadBMP`) | Rename to English following the glossary in Appendix B, class by class, file by file, together with the structural refactor of each class. |
| File names | Mixed, partially translated in 2013–2014 (`enemy.h` but `ennemi_*.cpp`; `fonte.cpp`; `super_liste.cpp`), typos (`vehivule_cigogne.h`, `moceaux_tete_snorky.cpp`, `ennemi_sorky_base_1.cpp`) | New directory layout (§3.3) with English file names; old files deleted as classes migrate. |
| Comments | French, often informal, sometimes containing design knowledge (GeForce bug, why two scroll updates per tick) | Translate when the file is touched; keep the design knowledge. |
| Log / UI strings in code | English (log), English hard-coded UI text (credits, "LEVEL COMPLETE!", "PLEASE WAIT") | Move hard-coded UI text to the string table; keep logs English. |
| Data | `uk.dat` English (single-byte Windows-1252: three curly-apostrophe bytes `0x92` in strings 207, 504 and 523); French key-name table in `key_translator.cpp`; `lang_type` forced to UK in `config.cpp:107` | UTF-8 string tables per language; French becomes possible again once a `fr` table is written (the original French text is lost — see §7). |
| Editor tools | French UI strings, Latin-1 sources, MFC | Out of scope except documentation; replaced by converters/CLI tools over time. |

---

## 3. Target architecture

### 3.1 Principles

1. **Core has zero platform dependencies.** `bb::core` and `bb::game` compile with the C++17 standard library only. No `SDL.h` outside `platform/`. This keeps the code portable: a new platform is a matter of writing (or configuring) one backend.
2. **One main loop, no blocking.** A `Screen` state machine drives everything (intro, menus, briefing, level, pause, dialogue overlay, level summary, hi-scores, credits). Nothing ever spins waiting for a key.
3. **Explicit context instead of globals.** Entities receive a `World&` (camera, collision maps, flags, spawner, audio, RNG, sprite banks) in `update()`/`draw()`. No singletons; `main()` owns the object graph.
4. **Ownership is visible.** `std::unique_ptr` for list-owned entities, `std::array` for the existing fixed pools, non-owning references for cross-object links that are *validated each tick* (the current raw pointers produce use-after-free, see Appendix D).
5. **Deterministic simulation.** Fixed 85 Hz tick, own seeded PRNG, no wall-clock reads in game logic. This is both a design property and the regression-testing strategy (§6).
6. **Data-driven where it is cheap, code where it is bespoke.** Simple walkers, props and projectiles become tables; the ~22 scripted bosses and set pieces stay as classes. The user asked for real OOP — the design keeps a class hierarchy with composition, not an ECS.
7. **Frugal by design, measured on desktop.** The reference machine is the study machine (a 2017 dual-core Intel laptop, Appendix G): no per-frame heap allocation, no busy loops (CPU near idle in menus), software rendering, decor decoded lazily. These keep the game light on every desktop and keep other platforms possible; no device budget is imposed.
8. **Formats are versioned, little-endian, field-by-field.** No `fread(&struct)`. Every runtime format has a magic, a version, and a spec in `docs/formats/`.

### 3.2 Layers and modules

```
┌────────────────────────────────────────────────────────────────────────────┐
│ app/            main(), argument parsing, bootstrap, platform selection    │
├────────────────────────────────────────────────────────────────────────────┤
│ game/   (bb::game)  screens · world · entities · hud · level loading ·     │
│                     cutscene & dialogue players · campaign flow            │
├────────────────────────────────────────────────────────────────────────────┤
│ core/   (bb::core)  Pixmap & software Blitter · SpriteBank · BitmapFont ·  │
│                     asset pack readers · legacy format readers · Mixer ·   │
│                     InputMapper (actions) · Rng · FixedClock · Vfs ·       │
│                     Config · StringTable · Log                              │
├────────────────────────────────────────────────────────────────────────────┤
│ platform/ (bb::platform)  interfaces: Display, InputSource, AudioDevice,   │
│                           Clock, FileSystem, LogSink, System (sleep/focus) │
│   sdl2/     the implementation, used by Windows, macOS and Linux        │
│   <other>/  none planned (an SDL3 backend would be the likely next one)    │
├────────────────────────────────────────────────────────────────────────────┤
│ tools/   bbpack (sprite banks), bblevel (levels), bbfont, bbsfx, bbtext    │
│          — reuse core readers; may use any third-party library             │
└────────────────────────────────────────────────────────────────────────────┘
Dependency rule: arrows point down only. game → core → platform *interfaces*. Only app/ links a platform implementation.
```

### 3.3 Proposed directory layout

```
blip-blop/
  CMakeLists.txt, CMakePresets.json
  cmake/                 warnings.cmake, sanitizers.cmake, packaging (CPack) helpers
  src/
    app/                 main.cpp, cli options (--data, --windowed, --scale, --record, --replay, --quiet, --cheat)
    platform/            interfaces (headers only) + sdl2/ backend
    core/                gfx/ (Pixmap, Blitter, Palette/ColorKey, LgxCodec, SpriteBank, BitmapFont)
                         assets/ (Pack reader/writer, LegacyGfx, LegacyLft, LegacySfx, LegacyLvl, Vfs)
                         audio/ (Mixer, SoundBank, MusicStream, WavDecoder, [OggDecoder])
                         input/ (Actions, InputMapper, Bindings)
                         util/ (Rng, FixedClock, Log, Config, StringTable, fixed-point tables)
    game/
      screens/           IntroScreen, MainMenu, PauseMenu, PlayerSelect, Briefing, LevelScreen, LevelSummary,
                         GameOver, HiScoreEntry, HiScoreTable, Credits, Cutscene, DialogueOverlay
      world/             World, Camera, CollisionMap, GameFlags, EventSystem, Spawner, EntityRegistry, DecalLayer, Weather, ScreenShake
      entities/          Entity, Character, Enemy, Player (+ Blip/Blop), Projectile, PlayerProjectile, Pickup, Prop,
                         MovingPlatform, Vehicle, effects (BloodSpray, Debris, Bubble, BannerText)
        enemies/         smurf/ pokemon/ carebears/ lemmings/ snork/ videogame/ boss/
        projectiles/     props/   pickups/   gore/
      campaign/          Campaign (bb.lst), LevelData, LevelLoader, Hud
  tools/                 bbpack/ bblevel/ bbfont/ bbsfx/ bbtext/
  assets/
    source/              png + json sprite manifests, wav, ogg, strings/{en,fr}.json, levels/*.json, cutscenes, dialogues
    legacy/              data.zip (or Git LFS pointer) kept for converters and golden masters
  tests/                 unit/ (formats, maths, collision, rng) · golden/ (input recordings + expected hashes)
  docs/                  architecture.md, formats/*.md, glossary.md, release/*.md (per-platform build and packaging notes)
  legacy/                vc-projects/ + level-editor/ + assets-gfx/ moved here untouched, deleted at the end
```

### 3.4 Key abstractions (responsibilities, not code)

| Abstraction | Replaces | Responsibility |
|-------------|----------|----------------|
| `Pixmap` | `SDL::Surface`, `IDirectDrawSurface7`, `Picture::surf` | Owning 2-D pixel buffer in the canonical 32-bit format, with optional transparent-key/alpha mask; RAII; copy/move explicit. |
| `Blitter` | `BltFast`, `Blt`, `FillRect`, `halfTone`, overlapping self-blits | Clipped copy with transparency, opaque copy, fill, 50 % darken of a rectangle, row-shift (shake), horizontal sine shift (deformation) — all software, all on `Pixmap`. |
| `SpriteImage` / `SpriteBank` | `Picture` / `PictureBank` | Image + hotspot; banks indexed by **named constants generated from the sprite manifest** instead of magic numbers; optional lazy loading per entry for decor banks. |
| `BitmapFont` | `Fonte` | Glyphs by Unicode code point (table), measure/print left/right/centred/wrapped. |
| `SoundBank`, `Mixer`, `MusicStream` | `Sound*`, `SoundBankBB`, `MusicBank`, FMOD shim | Channels, loop/stop by handle, volume; music streaming with loop; mute switches; back-end agnostic. |
| `Actions`, `InputMapper`, `Bindings` | `Input`, `ControlP1/P2`, `Controlor`, alias table, `DIK_*` | Per-player action state (left/right/up/down/fire/jump/special + menu actions) with pressed/just-pressed edges; bindings to keys/pad buttons/axes/hats; rebinding UI; serialisation by *name*, not key code. |
| `FixedClock` | `l_timer`, `glorf` loops | Accumulator producing N fixed 11.8 ms ticks per rendered frame, with a cap to avoid spirals of death. |
| `Rng` | `rand()` | Small seedable generator owned by `World` (one stream for gameplay, one for cosmetics). |
| `Vfs` / `FileSystem` | `_open/_read`, `CreateFile`, `ifstream`, `fopen` with `data/` literals | Resolves asset paths from a data root (directory now, pack archive later); read-only; normalises `\`→`/` and case for legacy data *at the open boundary* — both forks fixed the backslash problem exactly there (F.1, F.2), and the Android port shows the same boundary can read from an APK/zip source. |
| `Screen` + `ScreenStack` | nested loops in `Game`, `MenuMain`, `MenuGame`, `CINEPlayer`, `RPGPlayer` | `update(ticks)` / `draw()` / `onEnter/onExit`; push/pop; overlays (pause, dialogue) draw over the level. |
| `World` | `globals.h` | Camera, `CollisionMap`, `GameFlags`, entity lists, `Spawner`, `DecalLayer`, weather, shake, target-player selection, references to banks/mixer/rng; cleared per level. |
| `Entity` | `Sprite` | Position, velocity, facing, animation state, hitbox, alive flag; `update(World&)`, `draw(Renderer&, const Camera&)`; protected helpers `fall()`, `walk()`, `hitboxFromImage()`. |
| `Character` / `Enemy` / `Player` | `Personnage` / `Ennemi` / `Couille` | HP and state; `Enemy::onHit(Projectile&)` with the shared death pipeline (blood spray, decal, pickup drop, kill counter) as overridable steps. |
| `EntityRegistry` | the three `switch` factories | Maps a stable string ID (and the legacy numeric ID) to a factory; content registers itself per world. |
| `LevelEvent` + subclasses | `Event*` | Same semantics (activation X, condition, fire once), English names, created by the level loader from `LevelData`. |
| `LevelData` / `LevelLoader` | `Game::chargeNiveau` | Validated parse of legacy `.lvl` *and* of the new level pack into one in-memory structure. |
| `Campaign` | `loadList`, `jouePartie` | Sequence of briefing / cutscene / level entries with level kinds (first, intermediate, last, bonus, complete, end). |
| `CutscenePlayer`, `DialoguePlayer` | `CINEPlayer`, `RPGPlayer` | Same script languages, non-blocking, driven by the screen stack. |

### 3.5 Platform abstraction (HAL) — what each interface must provide

- **Display**: create a 640×480 logical surface; present a `Pixmap` (one upload per frame into a streaming texture or a framebuffer); fullscreen/windowed; integer or aspect-correct scaling policy; vsync hint; query refresh rate.
- **InputSource**: pump OS events; keyboard state by scan code; controllers by *stable slot* (not SDL instance ID) with buttons/axes/hats, hot-plug notifications; text input is not needed (hi-score names are entered with the game font).
- **AudioDevice**: open a 44.1 kHz stereo 16-bit output; implemented over SDL2_mixer. The interface would also allow an in-house mixer behind a plain callback (§3.8), which is optional.
- **Clock**: monotonic microseconds; sleep.
- **FileSystem**: open/read/size/close, directory root for data and for writable config/scores (platform-specific: `%APPDATA%` on Windows, `~/Library/Application Support` on macOS, `~/.local/share` on Linux — SDL's preferences path).
- **System**: focus/visibility and suspend events (auto-pause — the Android port's `nativeStop → Game::Pause()` hook is the model, F.2), quit request, message box for fatal errors where available.

### 3.6 Platform matrix and consequences

| Target | Toolchain / SDL | Screen | Consequence for the design |
|--------|-----------------|--------|----------------------------|
| macOS | CMake + Apple clang + SDL2 (on the study machine the `sdl2` pkg-config package resolves to Homebrew's `sdl2-compat` 2.32.72, i.e. the SDL2 API over SDL3 — the thin HAL keeps a native SDL3 backend cheap) | any, 60–144 Hz, Retina scaling | Reference development machine; app bundle for release. |
| Linux | CMake + GCC or clang + distribution SDL2 (checked in a Docker image with GCC, case-sensitive file system) | any | Case-sensitive paths (the `Vfs` normalises them); a portable archive or AppImage for release. |
| Windows | CMake + MSVC (or clang-cl) + SDL2 development packages | any, high-DPI | `%APPDATA%` for user files; installer or zip for release. The Windows build has not been verified yet (Phase 7). |

85 Hz logic on 60–144 Hz displays is handled by `FixedClock` (§3.10). The game's native 640×480 is scaled to the window (§3.7).

Dropped on 16 October 2026: Nintendo Switch, original Xbox, Miyoo Mini, Anbernic-class handhelds (the first version of this plan studied their toolchains, memory and screens; that analysis is not maintained any more), and Android (a 2014 port exists, Appendix F.2). The portability rules of §3.1 keep any of them possible as a separate project.

### 3.7 Rendering design

- **Software back buffer, 640×480**, one present per frame. Rationale: pixel-perfect parity with the original, deterministic (frame hashes for tests), no GPU API in the game code (portable).
- **Pixel format**: one canonical 32-bit format (as implemented in Phase 1). The source art is 16-bit (LGX = RGB565/RGB555) and is expanded at load; a 16-bit build option is not needed for the desktop targets.
- **Transparency**: colour-key → 1-bit alpha at pack time; sprites stored as **span-RLE** (skip N / copy N per row) so blits do not test every pixel and the format naturally carries the mask. The legacy LGX codec remains available for reading old banks.
- **Scrolling**: the 840-pixel VRAM ring buffer disappears. The camera X selects the two visible decor screens; each is a `Pixmap` from a small LRU cache (lazy-loaded from the pack index). It also keeps memory modest: `videofond.gfx` is 11 MB of LGX holding 18 distinct 640×480 screens; keeping 2–3 decoded screens costs 2.5–3.7 MB in 32-bit, against 22 MB for the whole bank as surfaces today (per-level totals in C.8).
- **Decals** (`grave()`): baked into the cached decor screen copies, exactly as today (they vanish when a screen leaves the cache — the camera never moves left in code and no shipped level uses a negative forced scroll speed, so behaviour is identical). Platforms that move also bake through the same `DecalLayer` API.
- **Effects**: half-tone darkening (fix the ignored rectangle), vertical shake (row shift with black fill), sine deformation, invincibility blink, foreground layer — all `Blitter` operations on the back buffer.
- **Presentation**: integer scale when the window is a multiple of 640×480, otherwise aspect-correct scale; optional CRT-less nearest filter; no changes to the 4:3 composition.
- **Passes per frame**: today the frame is copied three times (decor ring buffer → back buffer, back buffer → new texture, texture → display); the target is to compose once into the back buffer and present once (§3.14).
- **Frame pacing**: render every display frame; interpolation between the last two simulation states is an *optional later feature* (needs previous positions in `Entity`, cheap to add once entities are rewritten; see decision DEC-05).

### 3.8 Audio design

- `AudioDevice` implemented over SDL2_mixer (done in Phase 1), fixing loop semantics, music looping and volume. SDL2_mixer is available on the three targets.
- Optional (Phase 9): an in-house `Mixer` in core — 32 voices, 16-bit PCM sounds resampled at load, one streaming music voice decoded with stb_vorbis — fed to a plain SDL2 audio callback. It would remove SDL2_mixer, libvorbis and libogg; it is no longer required by any target. (Done 29 October 2026, with dr_mp3 for the two MP3 jingles.)
- Drop FMOD vocabulary and the tracker-module code path (no `.mbk` entry uses type 0). The "n simultaneous buffers" field of `.sfx` is 1 for all 175 shipped sounds; drop it and make polyphony a mixer policy.

### 3.9 Input design

- Actions per player slot: `Left, Right, Up, Down, Fire, Jump, Special`; global: `MenuUp/Down/Left/Right, Accept, Back, Pause, Screenshot/Debug`.
- Bindings reference physical inputs by **name** (`keyboard:left_ctrl`, `pad0:button_a`, `pad1:axis0-`), stored in a versioned text config; the legacy `bb.cfg` (raw SDL key codes) is imported once and never written again.
- Default pad layout for desktop game controllers (fire/jump/special on face buttons, D-pad or stick for aim), keyboard defaults as today (arrows + L-Ctrl/L-Alt/Space; the current default jump key is wrongly mapped to the context-menu key, see D-37).
- Edge detection (`justPressed`) replaces `waitKey/waitClean` busy loops; hi-score name entry uses a virtual keyboard driven by the game font (works on pads).
- Controllers identified by slot with hot-plug handling; both players can use pads; player 2 may also use the keyboard.
- Touch input is out of scope (no touch target). Should a touch platform come back, the Android port's approach (F.2) — a virtual stick and buttons that set the *same* actions — fits this design without changes to the game.

### 3.10 Timing and determinism

- The simulation tick stays **11.8 ms (≈84.7 Hz)** throughout the migration; every speed and latency constant in the content depends on it. Changing the tick is a *separate project* (decision DEC-05).
- `FixedClock` accumulates real time and runs 1–3 ticks per frame (cap 3, then drop time); on a 60 Hz display this yields the same 1.41 ticks/frame average as the current "glorf" logic, deterministically.
- All randomness through `World::rng()` (gameplay) and `World::fxRng()` (cosmetic). The two wall-clock reads inside logic (`rpg_player.cpp` dialogue timing via `timeGetTime`, menu/attract timers via `GetTickCount`) become tick counters.
- With recorded inputs (per tick: 7 actions × 2 players + menu actions) the whole game replays bit-exactly → golden-master tests (§6).
- Caution from upstream: the 2019 rewrite's `UpdateRegulator` defaults to an 11 ms step (≈90.9 Hz), so the game runs about 7 % too fast there (F.1). `FixedClock` keeps 11.8 ms and all golden masters are recorded at 11.8 ms.

### 3.11 Data strategy

**Source assets (human-editable, in git):**

| Asset | Source format | Notes |
|-------|---------------|-------|
| Sprites | PNG (RGBA, transparency as alpha) + JSON manifest per bank: image list, hotspots, **symbolic names**, animation sequences | Recovered from the 56 `.gfx` with the converter (the GFX Editor already contains a GFX→BMP importer: `GFX_EditorDlg.cpp:1635`). Colour key (RGB555 (30,25,18) / RGB565 (30,52,18)) becomes alpha 0. |
| Fonts | PNG strip or per-glyph PNG + JSON (code point → glyph, line height, space width) | Current fonts are indexed by single-byte code (Windows-1252/Latin-1); the manifest maps code points so UTF-8 text works. |
| Sounds | WAV as today (mono; mostly 22,050 Hz 16-bit) + JSON bank list with names and polyphony | Extracted from `.sfx`. |
| Music | Ogg Vorbis as today | Delete the two stale `.mp3`. |
| Strings | `strings/en.json`, `strings/fr.json` (UTF-8, keyed by the existing numeric IDs *and* a symbolic name) | `uk.dat` converted from Windows-1252; French to be rewritten (§7). |
| Levels | `levels/<name>.json`: asset bank names, screen sequence, start/victory, platforms as **segments** per layer, walls as RLE or a 1-bit PNG mask, events with symbolic entity IDs | Produced by `bblevel` from `.lvl`; must round-trip back to `.lvl` so the MFC editor stays usable until replaced. |
| Cutscenes / dialogues | Keep `.cin` / `.rpg` text scripts (UTF-8), documented in Appendix A | Already data-driven and readable. |
| Campaign | `campaign.json` from `bb.lst` | |

**Runtime pack (what the engine loads):** one versioned little-endian container per bank/level with a magic, a version, an index table (name hash → offset, size) and payloads (sprites as decoded in the 16-bit source layout plus mask, expanded to 32-bit at load; PCM sounds; level binary). Built by the tools at build time; the engine never parses PNG or JSON in release builds (a PNG loader can be enabled for development hot-reload). Legacy `.gfx/.lft/.sfx/.lvl` readers stay in core until the end of Phase 5 so `data.zip` remains usable for A/B comparison.

**Why not just "BMP → PNG"?** The game never loads BMP at runtime; BMPs are editor inputs that were compiled to LGX. The real modernisation is: open source formats + manifests with names (so code stops saying `pbk_ennemis[168]`) + a compact runtime pack that is 16-bit-native and random-access (streaming). LGX itself is a reasonable codec; it just lacks an index, alpha, and names.

### 3.12 Naming and language conventions

- Namespaces `bb::platform`, `bb::core`, `bb::game`. Types `PascalCase`, functions/members `camelCase`, constants `kPascalCase`, files `snake_case.hpp/.cpp` named after the main type (`enemy_smurf_sauvage.cpp` → `enemies/smurf/wild_smurf.cpp`).
- Every French identifier is renamed per the glossary (Appendix B); the glossary is the review checklist and a `tools/check_naming` script greps for banned stems (`ennemi`, `tir_`, `fond_`, `etape`, `affiche`, `detruire`, …).
- Comments and docs in English; original design notes are kept in translation ("GeForce VRAM workaround" etc.).
- Game-visible text only through the string table; no English literals in code paths that players see (credits included).
- `enum class` for states, directions, weapons, worlds, event kinds, flags; `constexpr` tables for animations; no `#define` constants.
- Keep a `docs/legacy_map.md` table "old file / old class → new file / new class" for traceability during the migration (also helps future readers of the 2002 editors).

### 3.13 Errors, logging, configuration

- Expected failures (missing or corrupt files, bad settings) are values, not exceptions: loaders return results/optionals with error strings; fatal errors go to `Log` and a platform message box where available. Exceptions are used only for programming errors (for example a type registered twice in the `EntityRegistry`). The first version of this plan required `-fno-exceptions -fno-rtti` for console toolchains; that constraint left with them (DEC-10).
- `Log` writes to stderr and a file in the writable directory, buffered (today `Debug` reopens the log file for every message and `Fonte::printC` logs every call — a port-era performance bug).
- Config: versioned text file in the writable directory (bindings by name, display mode, scale filter, volumes, language, cheat flag); hi-scores: versioned, plain, with a checksum; legacy `bb.cfg`/`bb.scr` importers.

### 3.14 Performance: measured hot spots and the desktop budget

Measured on the study machine (Intel Mac, sdl2-compat 2.32.72 over SDL3, display refresh assumed 60 Hz — Appendix G) with the baseline build of §2.6, using the OS sampling profiler and a scratch micro-benchmark. No code was changed.

**Where the time goes today**

| Phase | Main-thread samples in `SDL_Blit_Slow` (SDL's generic per-pixel blitter) | Rest | CPU of one core | RSS |
|-------|------:|------|----:|----:|
| Intro cutscene | 1,827 / 3,252 = **56 %** (all under `Picture::BlitTo`) | waiting for vsync in `SDL_RenderPresent` | 51 % | 64 MB |
| Hi-score / menu screen | 3,796 / 5,601 = **68 %** (a single opaque full-screen `PasteTo`) | vsync wait | 64–77 % | 58 MB |

A second run on 11 October 2026 (same Debug build, machine otherwise idle) gave 1,711 / 3,487 = 49 % for the intro and 3,817 / 5,818 = 66 % for the hi-score screen, with 35–66 % of a core and 62–68 MB RSS (raw figures in G.3).

**Root cause.** `Graphics::CreateSurface` builds every surface with an alpha mask (`graphics.cpp:114`); SDL2 and SDL3 both set such surfaces to `SDL_BLENDMODE_BLEND` by default (`SDL_surface.c`: "By default surfaces with an alpha mask are set up for blending"), and the only line that would reset it is commented out (`sdl_surface.h:28,35`). Every blit — colour-keyed sprites *and* opaque full-screen copies — therefore takes the slow alpha-blend path although the art has no alpha at all.

**Micro-benchmark (same machine, same SDL):**

| Operation | As the game is | `BLENDMODE_NONE` | RGB565 (no alpha mask) |
|-----------|---------------:|-----------------:|-----------------------:|
| 640×480 opaque copy | 128.6 µs | 48.0 µs (2.7×) | 27.1 µs (4.7×) |
| 64×64 colour-keyed sprite, 50 % transparent | 102.0 µs | 5.2 µs (**20×**) | 6.9 µs (15×) |
| same, `SDL_SetSurfaceRLE` on | — | 34.5 µs (slower here) | 22.5 µs (slower here) |
| present: `CreateTextureFromSurface`+destroy per frame | 0.68 ms | — | — |
| present: streaming texture + `SDL_UpdateTexture` | 1.76 ms (ARGB) / 1.02 ms (RGB565) — *slower* on this Metal backend | | |

Extrapolation (not measured on target): a gameplay frame today is one full-screen copy plus roughly 100–200 sprite blits, i.e. ≈ 10–20 ms of `SDL_Blit_Slow` on this machine — consistent with the profile. With blending off the same frame costs well under 1 ms here.

**Passes over the frame today:** (1) decor ring buffer → back buffer (full 640×480 copy, after per-frame 2-pixel-column updates of the ring buffer), (2) back buffer → new GPU texture (upload), (3) texture → display (GPU). Plus the half-tone routine touching the whole surface whenever a menu or dialogue is open, and the `printC` log write per HUD text each frame.

**Ordered plan (expected effect, then what to verify on target):**

1. `SDL_BLENDMODE_NONE` on every surface and opaque fills (D-59, D-62): one line, ~20× on sprites, ~2.7× on copies. Output is pixel-identical because decoded LGX pixels are opaque. *Phase 0.*
2. No busy-waiting; sleep to the next frame; the menu burns 50–77 % of a core today, depending on the run (D-57). *Phase 0/3.*
3. One frame-buffer pass instead of three: compose decor tiles and sprites directly into the back buffer (no ring buffer, no column copies), then present once. *Phase 1.*
4. (Dropped with the handheld targets: a 16-bit RGB565 path. The measurements above stay as a reference.)
5. Own span-RLE sprites (DEC-02): skip transparent spans without per-pixel tests; SDL's own RLE acceleration measured *slower* in this environment, so it is not assumed — measure on each target. *Phase 2.*
6. Half-tone and shake/deformation as `Blitter` routines on the touched rectangle only (D-32, D-20); cache the darkened frame while a menu is open (the game already does this with `systemSurface`). *Phase 1.*
7. Audio (today `Mix_OpenAudio(44100 Hz, stereo, 1024 samples)` ≈ 23 ms of buffer, 32 channels): SDL_mixer already converts WAVs to the device format at load (no per-frame resampling); the remaining cost, Ogg Vorbis decoding in the callback, is negligible on desktop. *Nothing planned.*
8. Loading: decode LGX straight into the target pixel format, one read buffer per bank instead of a `malloc` per image, lazy decor screens (§3.7). *Phase 2.*
9. (Dropped with the handheld targets: content-level knobs for low-power builds.)

**Desktop budget** (the study machine, 640×480 back buffer, 60 Hz display): CPU close to idle in menus (no busy waiting, Phase 3 measured it), a gameplay frame well inside the display period with headroom (drawing ≤ 4 ms, simulation ≤ 2 ms for 1–2 ticks), no per-frame allocation, memory per level bounded by lazy decor decoding. Memory today: 58–64 MB RSS before any level is loaded (mostly SDL/driver overhead on macOS, plus the 32-bit decoded global banks — 9.3 MB — and the intro bank); decoded images per level are tabulated in C.8 — the worst case, `video`, needs 36.3 MB of 32-bit images on top of the 9.3 MB of global banks (45.6 MB), against 22.7 MB in 16-bit and about 13.5 MB with lazily decoded decor screens (≈ 9 MB of level banks + 4.6 MB of global banks).

**How to keep measuring:** the scratch benchmark becomes `tests/bench` (blend vs none, 16 vs 32 bit, RLE on/off, texture vs window-surface present) and is run on the three targets before the present path is chosen (Phase 7); headless replays report ticks/s and peak RSS (§6).

---

## 4. Migration strategy

### 4.1 Strangler fig, not big bang

The legacy tree keeps building and running until parity is proven. New modules are introduced *behind* the old call sites, the old implementation is deleted only when the replacement passes the regression replays for all 12 levels. At every merge to `main` the full campaign is playable.

### 4.2 The safety net comes first

Before touching architecture, Phase 0 makes the game deterministic and records **golden masters**: input recordings for every level, menus and cutscenes, with per-tick state hashes and per-frame back-buffer hashes. From then on, every refactor is checked by replaying. Deliberate behaviour changes (bug fixes listed in Appendix D) are made *before* recording, or re-recorded in a dedicated, reviewed commit.

### 4.3 Order of work

Platform edge first (it is small, isolated and unblocks CI on three OSes), then assets, then the main loop, then the entity framework, then the content in world order, then clean-up, then the desktop release (Windows, macOS, Linux), then tools. Content migration is the bulk (≈28k lines, two thirds of it enemies) and is embarrassingly parallel once the framework exists — it can be split by world across several people or several weeks.

### 4.4 Working rules

- Small PRs, one concern each; file renames in the same commit as the class rename.
- Translate identifiers *when* restructuring a class, never as a separate mechanical pass (two passes over 240 classes is the expensive path).
- `legacy/` is read-only reference; fixes needed for the baseline go in before it is frozen.
- Warnings-as-errors on new code from day one (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion` progressively); sanitizers in CI.
- Every format, every subsystem gets a page in `docs/` when it lands.
- Treat upstream `benkaraban/blip-blop` (2019) as a reference, not a merge source: cherry-pick the items listed in Appendix F.1 with attribution; its repository-wide clang-format pass makes wholesale merges noisy.

### 4.5 Definition of done

- No French identifier remains (checked by script); comments English.
- `src/core` and `src/game` include no SDL header; the only global object is in `main`.
- CMake builds and tests pass on Windows, macOS and Linux CI.
- Golden-master replays pass for the 12 levels, the menu flows and the two cutscenes, with the same fingerprints on the three platforms.
- The campaign is completed end to end on Windows, macOS and Linux, from the release packages.
- `legacy/` removed; `docs/` complete; data pipeline reproducible from `assets/source`.

---

## 5. Phased plan

Effort figures are rough orders of magnitude for one experienced developer working focused days; they are there to compare phases, not to schedule.

### Phase 0 — Baseline, hygiene and safety net (≈ 1–2 weeks)

**Goal:** a reproducible, deterministic legacy build on three desktop OSes, with regression recordings.

- Modern `CMakeLists.txt` for the legacy tree: `cmake_minimum_required(3.16)`, C++17, `find_package(SDL2 / SDL2_mixer)` with pkg-config fallback, presets, out-of-source only; delete `deps/SDL2-2.0.1`, `.dsp/.dsw/.vcxproj`, VC6 cache files; add `.gitignore`, `.gitattributes`, `.clang-format`, `.editorconfig`.
- Compile fixes with no behaviour change: `fmod.h` calling-convention guard (or delete `fmod.h` and call SDL_mixer directly), `<malloc.h>` → `<cstdlib>`, the 65 missing returns, the 3 mismatched header guards, precedence warnings.
- **Port-regression fixes (restore the original game's behaviour):** looping SFX never loop (D-06), level music plays once (D-23), even-numbered joysticks ignored (D-05), joystick state indexed by instance ID (D-22), half-tone darkens the whole screen (D-32), default jump key mapped to the Menu key (D-37), per-message log file reopen and `printC` logging (D-21). Upstream already fixed D-32 and D-21 in 2019 — take its versions (F.1).
- **Performance quick wins (measured, §3.14):** set `SDL_BLENDMODE_NONE` on every surface (D-59 — about 20× faster colour-keyed sprites and 2.7× faster full-screen copies on the study machine, pixel-identical output) and make fills opaque (D-62); replace the busy-wait loops with a frame sleep (D-57 — the menu burns 50–77 % of a core today).
- **Data fixes:** normalise `\` → `/` in 9 `.mbk` files, lower-case `Snork.mbk` in two `.lvl` headers (upstream's `data/` has the `Snork.mbk` fix, but its `.mbk` files still contain backslashes — it handles those in code, `music_bank.cpp:44`, F.1), delete the two stale `.mp3`, remove the `data\\` literals in `menu_main.cpp:325` and `menu_game.cpp`; *also* normalise separators in the loaders themselves, as both forks did, so stray data never breaks a build again. Consider Git LFS for `data.zip` and the BMP trees.
- **Determinism:** replace `rand()` with a seeded generator; replace `timeGetTime`/`GetTickCount` in logic with tick counters; add `--record`/`--replay` of per-tick inputs and `--hash-frames`. Record golden masters for the 12 levels, menus, cutscenes.
- CI: GitHub Actions matrix (ubuntu, macos, windows), ASan/UBSan job (UBSan will flag the current undefined behaviour), `cppcheck`/`clang-tidy` report (non-blocking). For the Windows leg reuse upstream's recipe — vcpkg `sdl2` + `sdl2-mixer[libvorbis]`, CPack/NSIS installer with `data/` installed beside the binary (F.1).
- Add `benkaraban/blip-blop` as a git remote for cherry-picks and credit the 2013–2014 and 2019 contributors in the `README`.
- Docs: this plan, Appendix A as `docs/formats/`, `docs/legacy_map.md` skeleton, a `README` (how to build, where to put `data/`).
- Legal: add an MIT `LICENSE` — the licence recorded for the original project in the Google Code Archive (Appendix F.3) — crediting LOADED Studio and the later contributors.

**Exit criteria:** `cmake --preset default && cmake --build` works on the three OSes; replays reproduce identical hashes across runs and across OSes.

### Phase 1 — Platform layer (≈ 2 weeks)

**Goal:** delete the Win32/DirectX/FMOD shim; the game talks to `bb::platform` interfaces through `bb::core` services.

- Define `Display`, `InputSource`, `AudioDevice`, `Clock`, `FileSystem`, `System`, `LogSink`; implement `platform/sdl2`.
- Introduce `Pixmap` + `Blitter` (software) and route `Picture`, `Fonte`, scroll, HUD, effects through them; the blitter has only opaque, colour-key/span and darken paths — never SDL's generic alpha path (§3.14); presentation is one upload per frame (D-08 — the present path is chosen by measurement on the three targets, the streaming texture was *slower* on the study machine); remove `SDL::Surface`, `graphics.*`, `dd_gfx.*`, `alpha_blend.*` (dead code), `restore.*`, `ben_divers.*`, `l_timer.*`, `Engine/`.
- `InputMapper` with named bindings and `justPressed`; legacy `bb.cfg` import; `key_translator` tables become binding display names — upstream's X-macro tables `keys_us.xmacro` / `keys_fr.xmacro` are a ready-made form (F.1).
- `Mixer` façade over SDL2_mixer (interface designed so an in-house mixer could replace it, §3.8).
- `Vfs` with `--data` root; all `"data/..."` literals go through it; separators and case are normalised at open time, and the interface is designed so a pack/zip/APK source can be plugged in later (F.2).
- `Log` buffered.

**Exit criteria:** golden masters unchanged (frame hashes identical); no `SDL.h` outside `platform/`; Windows build without any DirectX header.

### Phase 2 — Assets and formats (≈ 2–3 weeks)

**Goal:** every data file is read by a validated, documented reader into plain structures; converters exist; the runtime pack is designed and generated.

- Legacy readers in core: GFX/LGX, LFT, SFX, MBK, LVL (field by field, 60-byte event records, bounds checks), LST, DAT (Windows-1252 → UTF-8), CIN, RPG, CFG, SCR.
- `SpriteBank` with symbolic names: generate per-bank constant headers from manifests (first version of the manifests is produced *from the code*: scrape every `pbk_x[N]` and name the indices while migrating; a grep counts 676 enemy-bank sites alone).
- Tools: `bbpack` (gfx → png+json, png+json → pack, with colour key → alpha), `bbfont`, `bbsfx`, `bbtext`, `bblevel` (lvl ↔ json, json → level pack). Converters must round-trip `data.zip` bit-exactly where the format allows (LGX re-encoding is lossless for RGB555/565).
- Runtime pack format v1 (Appendix A, "proposed"); engine loads either legacy files or packs behind the same `AssetSource` interface; decor banks support per-entry lazy loading.
- Fonts by code point; string tables UTF-8; the two hard-coded screens (`credits`, level summary labels) move to strings.

**Exit criteria:** `tools/build_assets` regenerates all packs from `assets/source`; the game runs identically from legacy data and from packs (same hashes); format docs complete.

### Phase 3 — Main loop and screens (≈ 2 weeks)

**Goal:** one non-blocking loop; `Game` (4,391 lines) dissolves into `Campaign`, `LevelScreen`, `Hud` and a dozen small screens.

- `ScreenStack`; `FixedClock`; app loop = pump events → N ticks → draw → present → sleep until next frame (no 100 % CPU).
- Convert each blocking routine into a `Screen` or overlay: intro & attract cycle (`Game::go`), `MenuMain`, `MenuGame` (pause), `selectPlayer`, `showBriefing`, `showPE` (level summary with its counting animation), `getName` (virtual keyboard), `showHighScores`, `showGameOver`, `showCredits`, `CINEPlayer::playScene`, `RPGPlayer` (dialogue overlay pausing the world), the "LOADING" screen and the 2-second pre-level wait.
- Focus loss / suspend → push pause; quit request handled everywhere (today only via `app_killed` polling).
- Menu input recordings added to the golden set.
- Use upstream's 2019 extractions (`hud.h`, `go_arrow.h`, `character_selection.*`, `menus/`) as references for how `Game` splits — re-implemented on the screen stack rather than merged (F.1).

**Exit criteria:** campaign completable with no nested loops; replays pass; CPU idle in menus.

### Phase 4 — World and entity framework (≈ 2–3 weeks)

**Goal:** `globals.h` is gone; a typed, owning `World` exists; content classes can be ported one by one against the new base classes while the old ones still run.

- `World` with `Camera`, `CollisionMap` (platform segments → per-pixel arrays rebuilt at load; solid/blood grids), `GameFlags` (named enum + 4 script registers), `EntityLists` (typed `std::vector<std::unique_ptr<T>>` with a spawn queue applied after each iteration and deferred removal — upstream's 2019 attempt with plain vectors crashed precisely because content appends during iteration and keeps raw pointers, F.1), `Spawner`, `DecalLayer`, `Weather`, `ScreenShake`, target-player selection (replaces `tete_turc` — note it is never null today), per-level `Rng`.
- `Entity`, `Character`, `Enemy`, `Projectile`, `PlayerProjectile`, `Pickup`, `Prop`, `MovingPlatform`, `Vehicle`, effects; `Player` (from `Couille`, 1,697 lines) with weapons as data (`WeaponSpec` table replaces the five copies of tuning constants spread across `couille.cpp`, `blip.h`, `blop.h`, `bonus_*.h`, `game.cpp`).
- `EntityRegistry` + `LevelEvent` hierarchy; legacy numeric IDs map to symbolic IDs; the three `switch` factories disappear.
- Cross-object references become validated handles (fixes the `PrincesseGuard`/`TirEpeeLaser`/`Rayman` use-after-free class of bugs).
- Collision loops (`manageCollisions`) become `World::resolveCollisions()` with the same rules; projectile pools preserved.

**Exit criteria:** one world (Smurf) runs on the new framework end to end with identical hashes; old and new frameworks coexist behind the registry.

### Phase 5 — Content migration, world by world (≈ 5–8 weeks, parallelisable)

**Goal:** all 64 enemy classes, 32 projectile classes, ~50 props, 8 pickups, gore/debris/vehicles/weather rewritten in English on the new base classes, hard-coded indices replaced by named sprites and sounds, hidden state removed, known bugs fixed.

Order = campaign order, each batch ending with replays of its levels:

| Batch | Levels | Content | Notes |
|-------|--------|---------|-------|
| 5a Smurf | snuf1, snuf2 | 14 enemy classes (basic Smurf + 5 skins, greedy, prankster, wild/abseiling, farmer, strongman mini-boss, Smurfette, stork, Big Smurf — finished as snuf2's final boss, 16 October 2026), cake/gift/wild-Smurf shots, homing kisses; these levels use midground decals and foregrounds but no animated props | Fixes the shared static shooting timer of the wild Smurf. |
| 5b Pokémon bonus | dork | Pikachu/Bulbasaur/flame walkers, flying COM, lift/crate/switch platforms, bomb defusal script | Moving platforms writing `y_plat` become `MovingPlatform` with proper carry logic. |
| 5c Care Bears | bisous1–3 | 9 placed enemy classes incl. shaman boss, zombie, Jedi and Jedi hero (heart bear and car are unused); rainbow/star/cloud shots, lightsaber; gore parts | Zombie and Jedi-hero replace the hit pipeline entirely → base class hooks. |
| 5d Snork | snorkniv, snorkniv2 | 7 enemies incl. Snork-in-Black boss, seahorse rider; head/debris pieces; airlock set piece (currently dead) | Boss jump tables and absolute perch coordinates become level data. |
| 5e Lemmings bonus | lem | Lemmings, fan/lamp/water props | The 200-kill nuke stays, as a scripted rule in data. |
| 5f Video-game world | video | Toad, Yoshi, Yoshi turret, Tails ×2, Knuckles, Princess escort + guards, Rayman + 4 "hero" summons, rings/shells/tongue shots, Mario-style props | Largest batch; many absolute X coordinates → level-relative markers. |
| 5g Video boss | videoboss | Lara + spikes + weight, small Mario, Luigi, Sonic, Pac-Man car; cutscene-like death sequences driven by flags | Four bosses share script register 0 — formalise as a named level script. |
| 5h Final boss | finalboss | Mario (16 attack patterns), vertical/horizontal fireballs (the hologram summon is commented out — dead content); the stork vehicle ride (the only vehicle use), hyperspace-rock parallax (39 instances), monolith/rock platforms, battle-cruiser backdrop | Mario stays a bespoke class. |

Per class: rename; `enum class` states; animation tables as `constexpr`; named sprites/sounds; statics → members or `World` script state; validated references; fix Appendix D items for that class; data-drive the simple ones (the audit estimates ~38 enemies, ~30 props and ~8 projectiles fit generic parametrised classes). Dead content (Diabolo, Care Bear car/heart, fish, airlocks, star prop; the Big Smurf was finished in 5a) is either finished or archived in `docs/cut_content.md` — not ported blindly.

**Exit criteria:** all 12 levels replay identically on the new framework; `legacy/` game code deleted.

### Phase 6 — Clean-up and consolidation (≈ 1–2 weeks)

- Remove `SuperListe`, `LGXpaker` singleton, `debug` global, remaining French names, duplicated animation tables and clashing macros (`SPEED_MAX`, `DELAY_ACCELERATION`, `ETAT_ATTENDS`, lowercase `tolerance`).
- Enable `-Werror` and `clang-tidy` naming checks repository-wide; unit tests for formats, collision, RNG, clock.
- Final docs: architecture, platform/backend guide (how a new platform would be added), content authoring guide, cut-content notes.

### Phase 7 — Desktop release: Windows, macOS, Linux (≈ 1–2 weeks)

**Goal:** the game ships on the three supported platforms from one CMake build. (Until 16 October 2026 this phase was the console and handheld ports; they are out of scope, §1.2.)

- **Windows verified**: build with MSVC (and clang-cl if cheap), CI job, replays give the macOS/Linux fingerprints; `%APPDATA%` user files; high-DPI awareness.
- **CI on the three platforms**: build, unit tests, replays (Release), sanitizers on one of them (as today on macOS).
- **Presentation**: choose the present path per platform by measurement (§3.14, `tests/bench`), do not present unchanged frames in menus, honour the VSYNC option with a frame limiter (D-75), integer / aspect-correct scaling and fullscreen on high-DPI displays.
- **Packaging (CPack)**: Windows installer (NSIS, upstream's recipe, Appendix F.1) or zip; macOS app bundle and disk image (SDL2 frameworks bundled, data in `Resources`); Linux archive and/or AppImage. The packages carry the runtime pack (`blipblop.bbpack`), the licence and the credits.
- **Controllers**: default bindings for common desktop game controllers; hot-plug checked on each platform.
- **Manual smoke checklist** per platform before a release tag (audio, controllers, display modes, focus loss, user files).

### Phase 8 — Tools (≈ 2–4 weeks, optional)

- Make the Phase 2 CLIs first-class (`bbpack`, `bblevel`, `bbfont`, `bbsfx`, `bbtext`) with a single `build_assets` driver and CI validation (every pack rebuilds reproducibly, every level validates: referenced IDs exist, platforms inside bounds, events in descending activation order as the loader expects).
- Optional: a cross-platform level/sprite editor (Dear ImGui + the engine's own renderer) replacing the MFC tools; until then the MFC editor + `bblevel` round trip is the authoring path.

**Status (25 October 2026): done.** `bbassets` is the single asset tool (24 October: the `content` and `assets.reproducible` tests). The editor is `bbedit` ([docs/editor.md](docs/editor.md)): levels (every event type, platforms, walls, starts, victory, screens, markers, undo, test play of the level in the game) and sprite hotspots and names, on the three platforms; fonts, sounds, strings, scripts and the campaign are plain files of the source tree. One deviation: it draws the game's images (decoded by `bb::core` from the source assets) through SDL's renderer, not the game's software blitter, which draws at 640×480 only. Details in docs/progress.md, Phase 8.

### Phase 9 — Optional improvements (not part of the migration)

Render interpolation for 60 Hz displays, French text restoration, configurable scaling filters/shaders, remappable controller buttons for both players, restored cut content. Listed so they do not creep into Phases 0–8. (Achievements were listed until 29 October 2026; the maintainer removed them. A third player, out of scope until then, was added by the maintainer's request on 29 October 2026 as a four-player mode: Bleach, a pink Blip, and Blitz, a green Blop.)

**Status (29 October 2026).** Chosen by the maintainer: French text, scaling options, render interpolation and the in-house mixer (DEC-06). Done: the in-house mixer (`bb::core::audio::Mixer`; SDL2_mixer, libvorbis, libogg and mpg123 are gone), French text (a new translation in the characters of the 2002 fonts, language in the options; pictures with English words, such as the character choice, stay English), integer or fit scaling and a sharp or smooth filter (shaders are not possible: SDL 2's renderer has none, and the frames are drawn in software, DEC-01), smooth motion (drawing between the last two ticks; it changes no tick, a replay test checks it). Remappable controller buttons already existed (the 2002 controls menu takes pad buttons). Not chosen: restored cut content. Added at the maintainer's request: the four-player mode (players 3 and 4, Bleach and Blitz, recoloured from Blip and Blop; pads 3 and 4 by default). Details in docs/progress.md, Phase 9.

---

## 6. Testing and verification

| Level | What | How |
|-------|------|-----|
| Golden-master replays | Whole-game regression | Recorded inputs per tick; hash of world state every tick (entity positions/states/flags/camera/RNG) and hash of the back buffer every frame; run headless (no display, no audio device) in CI for all 12 levels on the three platforms. A failing replay prints the first divergent tick and dumps both frames as PNG. |
| Format unit tests | Readers/writers | Round-trip every shipped `.gfx/.lft/.sfx/.lvl` through reader → writer → reader; fuzz the readers with truncated/garbage input (they must fail cleanly, never read out of bounds). |
| Core unit tests | Collision, `FixedClock`, `Rng`, `Blitter` ops, string tables, bindings serialisation | Small deterministic tests (Catch2 or doctest, tests only). |
| Sanitizers & static analysis | UB, leaks, naming | ASan/UBSan/LSan job; `clang-tidy` with `readability-identifier-naming`; `cppcheck`. |
| Performance budget | Frame time, memory | Headless benchmark replays print ticks/s and peak RSS against the desktop budget (§3.14). |
| Micro-benchmarks | Blit and present paths | The Phase 0 scratch benchmark (blend vs none, RLE on/off, texture vs window-surface present) becomes `tests/bench` and runs on the three platforms before the present path is chosen (Phase 7). |
| Manual smoke | Audio, controllers, display modes, suspend | Checklist per platform before a release tag. |

Determinism prerequisites (Phase 0): seeded RNG, no wall-clock in logic, no uninitialised members (UBSan/MSan or `-ftrivial-auto-var-init` during tests), no iteration-order dependence on allocation addresses.

---

## 7. Risks and open questions

| # | Risk / question | Impact | Mitigation / decision needed |
|---|-----------------|--------|------------------------------|
| R1 | **No `LICENSE` file in the tree** (headers say "CONFIDENTIAL DOCUMENT"), but the Google Code Archive metadata for the original project records the licence as **MIT** (Appendix F.3). | Without the file in the repository, redistribution rights are unclear to users. | Add an MIT `LICENSE` in Phase 0 crediting LOADED Studio (Benjamin Karaban, Sylvain Bugat) and the 2013–2014 and 2019 contributors; confirm with the authors if possible. |
| R2 | **Assets parody copyrighted characters** (Smurfs, Snorks, Pokémon, Care Bears, Lemmings, Mario, Sonic, Pac-Man, Lara Croft, Rayman…). | No store distribution ever; takedown risk even for free downloads. | State clearly in `README`; free distribution only; keep the engine asset-agnostic so a clean-room asset set is possible later. |
| R3 | **Original French text is lost** (`fr.dat` never in the repo; `lang_type` forced to UK). | "French as a data language" requires rewriting 136 strings. | Low effort; list as Phase 9; the string-table design supports it from Phase 2. |
| R4 | **85 Hz logic on 60 Hz displays** produces uneven stepping (1 or 2 ticks per frame). | Visible judder; changing the tick alters every constant and the recorded masters. | Keep 85 Hz for the migration (parity); add interpolation later (DEC-05). |
| R5 | **Hidden behaviour depends on bugs** (uninitialised members, use-after-free reads, static state surviving across levels, raw-index flags). | Fixing them changes replays; some "bugs" may be relied upon by level scripts. | Fix per class in Phase 5 with a documented re-recording; keep a list of intentional behaviour changes. |
| R6 | **The Windows build is unverified** (never built since the CMake migration; CI never ran on Windows). | Surprises late (MSVC warnings, paths, SDL packaging). | Phase 7 verifies it first; keep MSVC-hostile constructs out (the code already builds with clang and GCC under strict warnings). **Closed 25 October 2026:** CI builds with MSVC (`/W4 /WX`), passes every test and the 26 replay runs with the macOS and Linux fingerprints, and makes the NSIS installer and the zip (docs/progress.md). |
| R7 | *(Withdrawn 16 October 2026: memory on the original Xbox; the console targets are out of scope.)* | | |
| R8 | **SDL2 → SDL3 transition** (Homebrew already ships `sdl2-compat`). | Backend bit-rot. | Thin HAL; an SDL3 backend is a bounded task. |
| R9 | **Level editor stays Windows/MFC** for a long time. | Authoring friction; converter must round-trip perfectly. | `bblevel` lvl↔json round-trip tests on the 12 shipped levels; editor rewrite deferred. **Closed 25 October 2026:** `bbedit` (Phase 8). |
| R10 | **Scope creep from "while we are here" improvements.** | Migration never finishes. | Phase 9 list; definition of done; replays as the gate. |
| R11 | Team size / time: this is ~50k lines of 2001 hobby code; the content phase alone is weeks. | Fatigue, half-migrated tree. | Strangler strategy keeps a working game at all times; world-sized batches give visible milestones. |
| R12 | Upstream (`benkaraban/blip-blop`) has been inactive since 2019 and the Android port is GPL-3.0: cherry-picking from either needs correct attribution and licence hygiene. | Attribution or licence mistakes. | Reference upstream commit hashes in cherry-picks; credit contributors in `README`; take ideas, not code, from the GPL port (Appendix F). |

Decisions the maintainer should confirm before Phase 1 starts: DEC-01 (software rendering), DEC-02 (pixel formats), DEC-03 (runtime pack vs. keeping GFX), DEC-05 (85 Hz), DEC-07 (JSON sources, binary runtime), and the licence question R1. (DEC-09, the first console target, was withdrawn on 16 October 2026 with the console targets.)

---

## 8. Decision log (proposed)

| ID | Decision | Alternatives considered | Rationale |
|----|----------|-------------------------|-----------|
| DEC-01 | Software renderer into a 640×480 back buffer, presented once per frame. | GPU sprite batching via `SDL_Renderer`/OpenGL. | Pixel parity, determinism (frame hashes), portability (no GPU API in game code), ~200 blits/frame is trivial; the original was software too. |
| DEC-02 | One canonical 32-bit pixel format; transparency as a 1-bit mask via span-RLE. (Revised 16 October 2026: the RGB565 build option was for the consoles and handhelds.) | RGB565 option for low-memory devices. | The three desktop targets have the memory; one format means one set of fingerprints and one code path. |
| DEC-03 | New versioned runtime pack with index + names; legacy GFX/LGX kept as an import codec only. | Keep `.gfx` as the runtime format. | GFX has no index (no streaming), no names (magic numbers), no alpha; LGX RLE is kept conceptually (span-RLE). |
| DEC-04 | Source assets: PNG + JSON manifests, WAV, Ogg, UTF-8 JSON strings and levels. | Keep BMP + `.pbk`/INI. | Open, diffable, editable with any tool; converters recover them from `data.zip`. |
| DEC-05 | Keep the 11.8 ms tick (≈85 Hz) unchanged; interpolation is an optional later feature. | Retune to 60 Hz. | All gameplay constants are per-tick integers; retuning is a design project, not a port task. |
| DEC-06 | Audio: SDL2_mixer on the three targets; an in-house mixer (stb_vorbis + PCM) is optional (Phase 9). (Revised 16 October 2026: it was required for the consoles. Revised 29 October 2026: the maintainer chose the in-house mixer, `bb::core::audio::Mixer`, with dr_mp3 for the two MP3 jingles; SDL2_mixer is no longer used.) | In-house mixer everywhere. | SDL2_mixer is available and proven on desktop; FMOD/MOD paths removed. |
| DEC-07 | Levels authored as JSON (segments, masks, symbolic IDs), compiled to a binary pack; engine parses no JSON in release. | Engine reads JSON directly. | Keeps core dependency-free and loads fast on slow storage. |
| DEC-08 | Class hierarchy with composition (Entity/Character/Enemy/… + `AnimationPlayer`, `Hitbox`, `Health` helpers); generic parametrised classes for simple content; bespoke classes for bosses. | ECS. | Matches the existing mental model and the user's "real OOP" goal; content is bespoke behaviour, not data crunching. |
| DEC-09 | *(Withdrawn 16 October 2026: the console order. Targets are Windows, macOS and Linux; the code stays portable, §1.1 G4.)* | | |
| DEC-10 | Expected failures as results/optionals; exceptions only for programming errors. (Revised 16 October 2026: the code was required to build with `-fno-exceptions -fno-rtti` for console toolchains.) | Exception-free, RTTI-free core. | No console toolchain any more; loaders still return values, so the core stays exception-free in practice. |
| DEC-11 | Third-party allow-list: SDL2 (platform), SDL2_mixer (audio; unused since 29 October 2026), stb_image/stb_image_write (tools/dev), stb_vorbis (only for the optional in-house mixer), dr_mp3 (added 29 October 2026 by the maintainer, for the mixer's two MP3 jingles), Dear ImGui (the level editor), nlohmann/json + CLI11 (tools), Catch2/doctest (tests). Core itself: none, but the mixer's decoders (stb_vorbis, dr_mp3) in their own library, `bb_core_codecs`. | Boost, fmt, etc. | The user's explicit minimal-dependency goal. |
| DEC-12 | Legacy `bb.cfg`/`bb.scr` imported once into versioned text files; no further compatibility. | Keep binary formats. | Raw SDL key codes and `sizeof(bool)` layouts are not portable. |
| DEC-13 | Determinism as a first-class requirement (seeded RNG, tick-based timers, recordings). | Manual QA only. | 50k lines of behaviour nobody remembers; replays are the only affordable regression test. |
| DEC-14 | Upstream's 2019 work is mined, not merged: cherry-pick its D-32/D-21 fixes, data corrections, X-macro key tables, the HUD/GoArrow/CharacterSelection splits and the CI/CPack recipe; keep the 11.8 ms tick. | Merge `benkaraban/blip-blop` master and continue from there. | Its repository-wide reformat makes merges noisy, its 11 ms tick changes gameplay, and its `std::list` containers conflict with the DEC-08 entity design (Appendix F.1). |
| DEC-15 | No per-pixel alpha blending anywhere in the renderer: transparency by colour key / span mask, darkening by a dedicated routine, opaque fills; surfaces never left in SDL's default blend mode. | Keep SDL surface blending (status quo). | The art has no alpha; SDL's blend path costs 20× on sprites and 2.7× on full-screen copies (§3.14). |

---

## Appendix A — File formats (reverse-engineered from the readers and the editors)

All integers are 32-bit little-endian unless stated; strings are single-byte Windows-1252 (Latin-1 plus the `0x92` apostrophe); "the editors" are the MFC tools in `vc-projects/`. Reader locations are given so the specs can be checked.

### A.1 `.gfx` — sprite bank (`picture_bank.cpp:61`, written by `GFX_EditorDlg.cpp:1094`)

| Field | Type | Meaning |
|-------|------|---------|
| `count` | int32 | number of images |
| repeated `count` × | | |
| `hotspotX`, `hotspotY` | int32 ×2 | drawing origin relative to the image's top-left |
| `size` | int32 | byte size of the following LGX blob |
| `lgx` | bytes | one LGX image (A.2) |

Images are addressed by index only; the bank has no names and no offset table (sequential load, no streaming). Transparency is a colour key applied after decoding: for LGX v1 banks RGB (246,205,148), for v0 banks RGB (246,210,148) — these are the *decoded* values of the authoring key (250,206,152)/(250,214,152) after RGB555/RGB565 quantisation, i.e. the key is really "RGB555 (30,25,18)" / "RGB565 (30,52,18)". Banks loaded with `trans = false` (briefings, credits, "the end") have no key.

### A.2 LGX image (`lgx_packer.cpp:473`, encoders in `winLGX.cpp` and `lgx_packer.cpp:303/372`)

Header, 10 bytes: `'L' 'G' 'X'`, `version` (uint8: 0 or 1), `width` (uint16), `height` (uint16), `depth` (uint16, always 16).

- **v0**: `width × height` uint16 pixels in RGB565 (5-6-5), row-major, no padding.
- **v1**: RLE stream of uint16 words over the pixel sequence (runs may cross row boundaries): a word with bit 15 set is a run marker — `count = word & 0x7FFF` (2..32767) followed by one colour word; a word with bit 15 clear is a literal colour. Colours are RGB555 (5-5-5, bit 15 unused).

The encoders sample pixels through GDI `GetPixel`, hence the "colours are BGR" remark in the comments. All shipped fonts are v1. Of the 3,755 images in the 56 banks, 3,564 are v1 and 191 are v0; v0 is used for the 640×480 decor screens, the briefing images and a few large props, where run-length coding would not pay off.

### A.3 `.lft` — bitmap font (`fonte.cpp:52`, written by `FNT EditorDlg.cpp:527`)

| Field | Type | Meaning |
|-------|------|---------|
| `lineHeight` | int32 | vertical advance for multi-line text |
| `spaceWidth` | int32 | advance for `' '` |
| for `code = 1 … 255` | | glyph by single-byte code (Windows-1252/Latin-1) |
| `size` | int32 | 0 = no glyph |
| `lgx` | bytes | LGX image if `size > 0` |

Glyph advance = glyph width; hotspot (0,0); colour key (246,205,148). Code 0 has no slot.

### A.4 `.sfx` — sound bank (`sound_bank.cpp:84`, written by `SFX EditorDlg.cpp:688`)

| Field | Type | Meaning |
|-------|------|---------|
| `count` | int32 | |
| repeated: `voices` | int32 | intended simultaneous instances (always 1 in shipped data; ignored by the SDL port) |
| `size` | int32 | |
| `wav` | bytes | a complete RIFF/WAVE file; shipped data is PCM mono: 142 × 22,050 Hz 16-bit, 18 × 22,050 Hz 8-bit, 12 × 44,100 Hz 16-bit, 3 odd ones (8,050 / 10,989 / 44,100 Hz 8-bit) |

### A.5 `.mbk` — music bank (text, `music_bank.cpp:30`)

Line 1: `count`. Then `count` lines `type path`, `type` 0 = tracker module (unused by any shipped bank), 1 = streamed music. Paths are relative to the working directory and **must use `/`** (9 shipped banks still use `\`). `.zik` files are Ogg Vorbis (formerly renamed MP3).

### A.6 `.lvl` — level (`game.cpp:567`, written by `BB3_level_editorDlg.cpp:2435`)

| Field | Type | Meaning |
|-------|------|---------|
| 7 × `name[20]` | char[20] each | asset file names, NUL-terminated, **garbage after the NUL** (editor writes an uninitialised 100-byte stack buffer; bytes `0xCC` observed): decor bank, level/prop bank, enemy bank, sound bank, music bank, dialogue script, dialogue portrait bank. Empty string = none. Game prefixes `data/`. |
| `screenCount` | int32 | level width in 640-px screens |
| `screenIndex[screenCount]` | int32 | which decor image to show for each screen (tiles reused) |
| `start1X, start1Y, start2X, start2Y` | int32 ×4 | player spawn points |
| `victoryX, flag1, value1, flag2, value2` | int32 ×5 | victory when camera X ≥ `victoryX` and `flag[flag1] == value1` and `flag[flag2] == value2` |
| `platformY[8][screenCount × 640]` | int32 | per-pixel platform height per layer; `0xFFFF` (65535) = none; layers 6–7 are slippery (`couille.cpp:642`) |
| `solidWall[60][width / 8]` | uint8 (bool) | 8×8-pixel cells blocking movement |
| `bloodWall[60][width / 8]` | uint8 (bool) | cells that accept blood decals |
| `eventCount` | int32 | |
| `events[eventCount]` | 60 bytes each | `FICEVENT` records sorted by `activationX` in **descending** order (verified: no ascending adjacent pair in any of the 12 levels) |

`FICEVENT` (`fic_events.h`), 60 bytes as laid out by MSVC x86: `eventId`, `activationX`, `id`, `x`, union{`pictureBank`,`soundBank`,`textId`}, union{`y`,`condition`,`speed`,`intensity`,`play`}, union{`capacity`,`flag`,`duration`}, union{`value`,`facing`,`direction`}, `period` (9 × int32), `temporary` (bool, 1 byte + 3 padding), `file[20]`. Events wake when `activationX <= cameraX`. The loader prepends each record to the sleeping list (`SuperListe::ajoute`), so the descending file order becomes ascending in memory, and `Game::updateEvents()` (called from `updateAll`) wakes events from the head only until the first one that is not ready — the order is load-bearing and any converter must preserve it. The editor derives `activationX` from the object's X with per-type offsets (`BB3_level_editorDlg.cpp:2573–2946`): enemies −750 when facing left, +50 when facing right; generators −800 / +50 likewise; props, foregrounds, vehicles and pickups −740; midground decals −660; locks, hold-fire, scroll speed, dialogue, flags, texts, sounds, weather, music and pickup generators use X unchanged.

Event IDs: 0 enemy, 1 enemy generator, 2 animated prop, 3 midground decal, 4 foreground, 5 scroll lock, 6 hold fire, 7 force scroll speed, 8 dialogue, 9 set flag, 10 banner text, 11 sound, 12 weather, 13 overkill (unused), 14 dynamic load (unused), 15 music, 16 pickup generator, 17 vehicle, 18 pickup. Scroll-lock release conditions: 0 no enemies, 1 no generators, 2 flag == value, 3 flag ≥ value.

Flags: 0–3 user script registers (meaning differs per level), 4 bubbles (underwater), 5 pickup mode (selects a drop table in `make_bonus.cpp`), 6 timer, 7 generators off, 8 kill count, 9 enemy count, 10 generator count.

### A.7 `bb.lst` — campaign (text, `game.cpp:3449`)

Each line is `type [levelKind] *path`: type 0 cutscene (`.cin`), 1 level (`.lvl`), 2 briefing (`.gfx` with one image). Level kinds: 0 bonus (timer, lives preserved, cannot lose), 1 first of a world (reset weapons/stats), 2 intermediate, 3 last (summary), 4 complete (reset + summary), 5 end (final boss).

### A.8 `uk.dat` — strings (text, `txt_data.cpp:29`)

Lines `id^text`, Windows-1252, up to 1000 ids. 136 strings; used ids: 1–4, 10–19, 200–213, 300–308, 400–414, 500–507, 520–526, 600–615, 700–708 (dialogue and banner texts per world), 100–108 (bonus-level banners), 800–834 (menus; 828 = "TIME"). Ids 508–519 do not exist although `ennemi_princesse.cpp` requests banners 517 and 519 (D-63).

### A.9 `.cin` — cutscene script (text, `cine_player.cpp`)

Case-insensitive `command(args)` lines, `;` comments, 20 objects. Commands: `endscene`, `loadgfx(file)`, `loadmbk(file)`, `playmusic(n)`, `stopmusic`, `setpos(obj,x,y)`, `setpic(obj,image)`, `setmove(obj,dx,dy,ticks)`, `settext(obj,stringId)`, `reset(obj)`, `resetall`, `setclip(x1,y1,x2,y2)`, `noclip`, `setanim(obj,speed,loop,images…)`, `setxwarp(obj,from,to)`, `setywarp(obj,from,to)`, `stopanim(obj)`, `renderscene(frames)`; `setalpha`, `setdeltaalpha`, `setbackcolor`, `setclipcolor`, `setvolume`, `setdeltavolume` are parsed but inert in the SDL port (the alpha-fade implementation was never ported). Esc skips.

### A.10 `.rpg` — dialogue script (text, `rpg_player.cpp`)

Sections start at `rpg=N` and end at `stop`. Commands: `focus=haut|bas` (top/bottom box), `id=joueur|ennemi` (portrait bank: players or level), `img=N` (player portraits are offset +6 for Blop), `txt=N`, `affiche=ms` (show for N ms, skippable after 750 ms), `flag=F.V` (set flag F to V), `ifnbj=N … endif` (only if N players), `bbswap` (switch to the other player), `;` comments. The world is frozen while a dialogue plays.

### A.11 `bb.cfg` (binary, 61 bytes, `config.cpp`)

`vsync` (bool, 1 byte), `language` (int32), then 14 × int32 bindings: P1 up/down/left/right/fire/jump/special, P2 same. Values are raw SDL2 key codes (`0x4000xxxx` for non-printable keys) or joystick codes `((slot+1) << 10) | button`, with 100–103 for hat directions. Not portable; import once.

### A.12 `bb.scr` (binary, 244 bytes, `hi_scores.cpp`)

10 × (int32 score, char name[20]) XOR-obfuscated with `0x35674A1F << rank`, followed by an int32 checksum (sum of plain scores and name bytes).

### A.13 Editor-side formats

- `.pbk` (GFX Editor source list): `count`, then per image a line `hotspotX hotspotY compression` and a line with the BMP path (absolute Windows paths in the shipped `smurf-sprites.pbk`).
- Level editor `bbe_*.ini`: `bbe_decors.ini` (count + screen BMP names), `bbe_ndecors.ini` (count + screen index sequence), `bbe_ennemis.ini` / `bbe_fondsanimes.ini` / `bbe_mifonds.ini` / `bbe_1plan.ini` (`count`, then `id Name*bitmap.bmp`).

### A.14 Proposed runtime pack (to be specified in Phase 2)

*Specified and implemented in Phase 2: [docs/formats/runtime-pack.md](docs/formats/runtime-pack.md); the source assets it is built from: [docs/formats/source-assets.md](docs/formats/source-assets.md).*

Magic + version + flags (pixel format, endianness marker) → table of entries (FNV-1a name hash, type, offset, size, width/height/hotspot for images) → payloads. Sprite payload: per row, spans of `skip n` / `copy n` with RGB565 or RGBA8 pixels; sounds: PCM16 mono at source rate; fonts: glyph table (code point → entry) + spans; levels: the A.6 content with platforms as segments and symbolic IDs resolved to registry indices. Everything little-endian, 4-byte aligned, readable with a single `fread` of the directory followed by seeks (streaming).

---

## Appendix B — Glossary French → English

Used as the renaming checklist. Where the original word is slang or a joke, the note explains it. Upstream's README lexicon (couille, schnuff = "thing", snuf, tête de turc) agrees with it.

### B.1 Core types

| Current | Proposed | Note |
|---------|----------|------|
| `Sprite` | `Entity` | base of everything that updates and draws |
| `Personnage` | `Character` | has HP and a state |
| `Ennemi`, `ennemi_*` | `Enemy`, `enemies/` | |
| `Couille` | `Player` (`PlayerBall`) | slang for "ball/testicle" — the heroes are balls ("Balls of Steel") |
| `Joueur` | `ScoreKeeper` / `PlayerStats` | score + kill count mix-in |
| `Blip` / `Blop` | `Blip` / `Blop` | character names stay |
| `Tir`, `tir_*` | `Projectile`, `projectiles/` | |
| `TirBB` | `PlayerProjectile` | BB = Blip & Blop |
| `Bonus`, `bonus_*` | `Pickup`, `pickups/` | |
| `fond_*` classes (the "fonds animés" list) | `Prop` (`AnimatedProp`), `props/` | "animated background"; there is no `FondAnime` base class today — every prop derives from `Sprite` directly |
| `FondStatique` / mi-fond | `MidgroundDecal` | static image between decor and sprites |
| `PremierPlan` / `PPlan` | `ForegroundProp` | |
| plateforme mobile | `MovingPlatform` | |
| `Giclure` | `BloodSpray` | |
| `gore_*`, `Morceau*` | `gore/`, `Debris` / `BodyPart` | |
| `Bulle` | `Bubble` | underwater bubbles |
| `MeteoPluie`, `MeteoNeige`, météo | `RainDrop`, `SnowFlake`, `Weather` | |
| `TexteCool` | `BannerText` | the scrolling "GOTTA KILL'EM ALL!" banners |
| `Vehicule`, `VehiculeCigogne`, `VehiculeHypo` | `Vehicle`, `StorkVehicle`, `SeahorseVehicle` | "hypo" presumably = hippocampe = seahorse; "Hyporider" = seahorse rider |
| `Event`, `Event*` | `LevelEvent`, see B.4 | |
| `GenEnnemi`, `GenEnnemiTMP`, `GenBonus` | `EnemyGenerator`, `TemporaryEnemyGenerator`, `PickupGenerator` | |
| `SuperListe` | *(removed)* typed `EntityList<T>` | |
| `Picture`, `PictureBank`, `pbk_*` | `SpriteImage`, `SpriteBank`, `sprites.*` | |
| `LGXpacker` / `LGXpaker` | `LgxCodec` | |
| `Fonte`, `fnt_*` | `BitmapFont`, `fonts.*` | |
| `Sound`, `SoundBank`, `SoundBankBB`, `sbk_*` | `SoundSample`, `SoundBank`, `sounds.*` | the "BB" variant only adds the mute switch |
| `MusicBank`, `mbk_*`, zik | `MusicBank`/`Playlist`, `music.*`, music | |
| `Controlor`, `ControlP1/P2` | `PlayerController`, bound to a player slot | |
| `Input`, `in`, alias | `InputMapper`, action bindings | |
| `CINEPlayer` | `CutscenePlayer` | |
| `RPGPlayer` | `DialoguePlayer` | "RPG" meant dialogue boxes with portraits |
| `MenuMain`, `MenuGame` | `MainMenu`, `PauseMenu` | |
| `Game` | `Campaign` + `LevelScreen` + `Hud` + screens | |
| `Debug`, `debug` | `Log` | |
| `ben_*` (author prefix) | `util/` | |

### B.2 Members and functions

| Current | Proposed | Note |
|---------|----------|------|
| `x`, `y`, `dy` | `x`, `y`, `velocityY` | |
| `etape`, `ss_etape` | `animFrame`, `animTick` | "step", "sub-step" |
| `pic` | `image` | |
| `a_detruire`, `aDetruire()` | `destroyed`, `isDestroyed()` | |
| `dir`, `sens` | `direction` / `facing` | |
| `lat_grav` | `gravityTick` | gravity latency counter |
| `x1,y1,x2,y2`, `col_on` | `hitbox`, `collidable` | |
| `pv` | `hp` | points de vie |
| `etat` | `state` | |
| `affiche()` | `draw()` | |
| `anime()` | `animate()` | |
| `tombe()`, `tombe2()` | `fall()`, `fallThrough()` | second variant does not snap when already on a platform |
| `marche()` | `walk()` | |
| `colFromPic()`, `noCol()` | `hitboxFromImage()`, `disableCollision()` | |
| `updateADetruire()` | `destroyWhenOffscreen()` | |
| `estTouche()` | `onHit()` | |
| `degats()` | `damage()` | |
| `enflame()` | `fireKind()` | 0 none, 2 burns (carbonises) |
| `aTue()`, `aTouche()` | `onKill()`, `onHitConfirmed()` | |
| `gicle()` | `spawnBloodSpray()` | |
| `tirEnCloche()` | `computeLobbedShot()` | "bell-curve shot" |
| `xmin`, `blood`, `tresor`, `count()` | `minX`, `bloodCooldown`, `dropWeight`, `countsAsKill()` | |
| `tete_turc` | `World::target()` | "whipping boy" = the player enemies aim at |
| `pbk_own` | `ownSprites` | per-character bank |
| `id_couille` | `character` | enum Blip/Blop |
| `id_arme`, `ammo`, `cadence_arme`, `latence_arme`, `nb_etape_arme`, `poid_arme` | `weapon`, `ammo`, `fireInterval`, `muzzleAnimLatency`, `muzzleAnimFrames`, `aimInertia` | "poids" = weight = how slowly the aim turns |
| `dir_arme`, `etape_bouge_arme`, `etape_recul`, `tire`, `pruno` | `aimDirection`, `aimTick`, `recoilFrame`, `firing`, `fireTick` | |
| `dx_saut`, `dx_glisse`, `latence_glisse` | `jumpVelocityX`, `slideVelocityX`, `slideMomentum` | |
| `invincible`, `inv_cow`, `a_mal`, `etape_cli` | `invincibleTicks`, `cowBombShield`, `hurtTicks`, `blinkTick` | |
| `y_to_go`, `nb_life`, `nb_cow_bomb`, `wait_cow_bomb`, `mod_life` | `respawnY`, `lives`, `cowBombs`, `cowBombCooldown`, `extraLivesAwarded` | |
| `sauti`, `anime_sautille` | `bobTick`, `bobTable` | the permanent little hop of the balls |
| `perfect` | `flawless` | no damage taken this level |
| `rearme()` | `resetWeapon()` | |
| `onNormal/onAvance/onSaute/onMeure/onComeBack/onSaleto/onVehicule` | `onIdle/onWalk/onJump/onDie/onRespawn/onSomersault/onVehicle` | "saleto" presumably = salto = somersault (the double-jump roll) |
| `lockVehicule`, `unlockVehicule` | `enterVehicle`, `leaveVehicle` | |
| `okBonus()` | `wantsWeaponPickup()` | |
| `gauche/droite/haut/bas/saut/fire/super` | `left/right/up/down/jump/fire/special` | |
| `jouePartie`, `joueNiveau`, `chargeNiveau`, `releaseNiveau`, `chargePartie`, `releasePartie` | `runCampaign`, `playLevel`, `loadLevel`, `unloadLevel`, `loadGlobalAssets`, `unloadGlobalAssets` | |
| `drawHUB`, `drawHUBpv` | `drawHud`, `drawHealth` | |
| `drawFlecheGo`, `fleche_go` | `drawGoArrow`, `goArrow` | |
| `showPE` | `LevelSummaryScreen` | |
| `getName`, `getHiscore` | `HiScoreEntryScreen` | |
| `updateTeteTurc` | `selectTarget` | |
| `creeBulle` | `spawnBubble` | |
| `tremblement`, `*_tremblement` | `screenShake`, `ScreenShake` | |
| `grave()` | `bakeDecal()` | "engrave" into the background |
| `offset`, `scr_offset` | `cameraX`, `cameraX % 640` | |
| `scroll_locked`, `x_lock`, `cond_end_lock`, `flag_end_lock`, `val_end_lock` | `scrollLocked`, `lockX`, `unlockCondition`, `unlockFlag`, `unlockValue` | |
| `hold_fire`, `flag_hold_fire`, `val_hold_fire` | `fireHeld`, `fireReleaseFlag`, `fireReleaseValue` | |
| `no_scroll1/2` | `scrollPausedByPlayer[2]` | |
| `scr_level_size`, `level_size`, `num_decor` | `screenCount`, `levelWidth`, `screenIndices` | |
| `y_plat`, `plat()`, `plat2()` | `platformHeights`, `platformAt()`, `platformLayerAt()` | |
| `murs_opaques`, `mur_opaque()` | `solidWalls`, `isSolid()` | |
| `murs_sanglants`, `mur_sanglant()` | `bloodWalls`, `acceptsBlood()` | |
| `game_flag[]`, `FLAG_*` | `flags`, `enum class Flag` | |
| `intensite_meteo`, `type_meteo` | `weatherIntensity`, `weatherKind` | |
| `phase`, `slow_phase` | `blinkPhase`, `slowBlinkPhase` | |
| `okLanceFlame`, `cowBombOn`, `okBonus` | `flamethrowerAllowed`, `cowBombUnlocked`, `weaponDropAllowed` | |
| `nb_ennemis_created` | `enemiesSpawned` | |
| `rpg_to_play` | `pendingDialogue` | |
| `vbuffer_wide`, `videoA`, `systemSurface`, `primSurface`, `backSurface` | *(removed)* `backBuffer`, decor cache | |
| `LATENCE_GRAVITE`, `GRAVITE_MAX`, `NB_MAX_PLAT` | `kGravityLatency`, `kMaxFallSpeed`, `kPlatformLayers` | |
| `ETAT_*` | `enum class CharacterState { Walking, Jumping, Falling, Shooting, Dying, Waiting, Idle, Burnt, Burning, Respawning, Somersault, Dying2…4, InVehicle }` | |
| `BBDIR_*` (18 values) | `enum class AimDirection` | |
| `SENS_GAUCHE/DROITE` | `Facing::Left/Right` | |
| `ID_M16/PM/FUSIL/LF/LASER` | `Weapon::Rifle/Smg/Shotgun/Flamethrower/Laser` | PM = pistolet-mitrailleur; LF = lance-flammes |
| `BPM/BFUSIL/BLASER/BBEER/BINV/BTONNO/BCOW/BLF` | `PickupKind::Smg/Shotgun/Laser/Beer/Invincibility/Barrel/CowBomb/Flamethrower` | tonneau = barrel (full heal) |

### B.3 Content names

| Current | Proposed | Note |
|---------|----------|------|
| snuf / Smurf world | `smurf/` | "Snuff Elite Squad" parody |
| dork | `pokemon/` | "Dorkemon", "dorkeball" |
| Bisou / Bisounours | `carebears/` | Care Bears → "Scare Bears" in the English text |
| Snork / Snorky | `snork/` | |
| lem / Lemmings | `lemmings/` | |
| video / vidéo | `videogame/` | Mario, Luigi, Toad, Yoshi, Princess, Sonic, Tails, Knuckles, Rayman, Lara, Pac-Man |
| `EnnemiSmurfSauvage` | `WildSmurf` | "brujah" vampire gag |
| `EnnemiSmurfGourmand` / `Farceur` / `Paysan` / `Costaud` / `ALunettes` / `Coquet` / `Frileux` / `Paresseux` / `Peon` | `GreedySmurf` / `PranksterSmurf` / `FarmerSmurf` / `StrongSmurf` / `BrainySmurf` / `VainSmurf` / `ChillySmurf` / `LazySmurf` / `PeonSmurf` | |
| `EnnemiGrandSmurf`, `EnnemiSmurfCigogne` | `BigSmurf` (finished in Phase 5a), `Stork` | |
| `Bulbizarre`, `Flameche`, `COM`, `*Heros` | `Bulbasaur`, `Charmander`, `Com`, `*Hero` | keep parody names consistent with the English text |
| `BisouCiel/Etoile/Boom/Nuage/Zombi/Chaman/Coeur/Jedi/SIB/Auto` | `SkyBear/StarBear/BoomBear/CloudBear/ZombieBear/ShamanBear/HeartBear/JediBear/SnorkInBlackCameo/BearCar` | |
| `SnorkyBase1/2`, `Gouverneur`, `Mage`, `Hyporider`, `SnorkInBlack(Boss)`, `Diabolo` | `SnorkGrunt/SnorkGunner`, `SnorkGovernor`, `SnorkMage`, `SeahorseRider`, `SnorkInBlack(Boss)`, `Diabolo` | |
| `Princesse`, `Princesseguard`, `Knukles`, `Tailsvolant`, `YoshiDCA`, `Poid`, `Pic`, `Mariotapette`, `VoiturePacman`, `TeteRayman`, `MarioHologramme` | `Princess`, `PrincessGuard`, `Knuckles`, `FlyingTails`, `YoshiTurret`, `Weight`, `Spikes`, `SmallMario`, `PacManCar`, `RaymanHead`, `MarioHologram` | "tapette" is a slur in French — must be renamed |
| `TirGateau/Cadeau/Carapace/Etoile/Epine/Debouchechiote/ArcCiel/BouleChaman/ColonneChaman/Nuage/CoeurBisou/EpeeLaser/BisouMortel/Hadoken/Tornade/Soniceclair/Raymanpoing` | `CakeShot/GiftBomb/Shell/Star/Spine/Plunger/RainbowShot/ShamanBall/ShamanPillar/StormCloud/HeartShot/Lightsaber/HomingKiss/Hadoken/Tornado/SonicLightning/RaymanFist` | "débouche-chiotte" = toilet plunger |
| `FondVentilo/Lumiere/Eau/Cube/Piece/Ring/Tournesol/Drapeau/Monolite/Rocher/Assembleur/Ecran*/Electronic1-4/Dos/Windows/Worldmap/LCD/Bombe/Interupteur/Ascenceur/Caisse*/Ciel/Fleche/Alarme/Sas/Poisson/Etoile/Battlecruiser` | `Fan/Light/Water/QuestionBlock/Coin/Ring/Sunflower/Flag/Monolith/Rock/AssemblerScreen/Screen*/Electronics/DosScreen/WindowsScreen/WorldMap/Lcd/Bomb/Switch/Lift/Crate*/Sky/Arrow/Alarm/Airlock/Fish/Star/BattleCruiser` | |

### B.4 Events

`EventEnnemi` → `SpawnEnemyEvent`; `EventGenEnnemi` → `EnemyGeneratorEvent`; `EventFondAnime` → `SpawnPropEvent`; `EventMiFond` → `MidgroundDecalEvent`; `EventPremierPlan` → `ForegroundEvent`; `EventLock` → `ScrollLockEvent`; `EventHoldFire` → `HoldFireEvent`; `EventScrollSpeed` → `ForceScrollEvent`; `EventRPG` → `DialogueEvent`; `EventSetFlag` → `SetFlagEvent`; `EventTexte` → `BannerTextEvent`; `EventSon` → `PlaySoundEvent`; `EventMeteo` → `WeatherEvent`; `EventMusic` → `MusicEvent`; `EventGenBonus` → `PickupGeneratorEvent`; `EventVehicule` → `SpawnVehicleEvent`; `EventBonus` → `SpawnPickupEvent`; `FICEVENT` → `LevelEventRecord`.

### B.5 File extensions (unchanged on disk until Phase 2 packs exist)

`.gfx` sprite bank · `.lft` font · `.sfx` sound bank · `.mbk` music bank · `.zik` music · `.lvl` level · `.lst` campaign · `.dat` strings · `.cin` cutscene · `.rpg` dialogue · `.cfg` config · `.scr` scores · `.pbk` editor image list.

---

## Appendix C — Inventory tables

### C.1 Campaign (`data/bb.lst`)

| # | Entry | Kind | World | Screens | Events | Decor bank |
|---|-------|------|-------|--------:|-------:|------------|
| 1 | brief1.gfx | briefing | Smurf | | | |
| 2 | snuf1.lvl | level, first | Smurf | 13 | 161 | snufdec1.gfx (6.1 MB) |
| 3 | brief1_1.gfx | briefing | | | | |
| 4 | snuf2.lvl | level, last | Smurf | 13 | 175 | snufdec2.gfx (6.8 MB) |
| 5 | brief2.gfx | briefing | | | | |
| 6 | dork.lvl | level, **bonus** | Pokémon parody | 5 | 94 | pokefond.gfx |
| 7 | brief3.gfx | briefing | | | | |
| 8 | bisous1.lvl | level, first | Care Bears | 9 | 112 | bisoudec1.gfx |
| 9 | brief3_1.gfx | briefing | | | | |
| 10 | bisous2.lvl | level, intermediate | Care Bears | 9 | 69 | bisoudec2.gfx |
| 11 | bisous3.lvl | level, last | Care Bears | 13 | 86 | bisoudec3.gfx (7.4 MB) |
| 12 | brief4.gfx | briefing | | | | |
| 13 | snorkniv.lvl | level, first | Snorks | 14 | 119 | snorkfond1.gfx (8.6 MB) |
| 14 | snorkniv2.lvl | level, last | Snorks | 10 | 109 | snorkfond2.gfx |
| 15 | brief5.gfx | briefing | | | | |
| 16 | lem.lvl | level, **bonus** | Lemmings | 4 | 55 | lemdec.gfx |
| 17 | brief6.gfx | briefing | | | | |
| 18 | video.lvl | level, complete | Video-game crossover | 18 | 274 | videofond.gfx (11.1 MB) |
| 19 | brief6_1.gfx | briefing | | | | |
| 20 | videoboss.lvl | level, complete | Video-game bosses | 4 | 33 | videofondboss.gfx |
| 21 | brief7.gfx | briefing | | | | |
| 22 | finalboss.lvl | level, end | Mario | 4 | 56 | finalbossfond.gfx |

Intro (`intro.cin` + `intro.gfx`) and ending (`end.cin` + `end.gfx`, `theend.gfx`, `credits.gfx`) are played from code, not from the list.

### C.2 Global banks loaded for every game

`blip.gfx`, `blop.gfx` (player bodies, weapons, deaths), `bb.gfx` (eyes, muzzle flashes, player shots, explosions, HUD health), `misc.gfx` (100 images: pickups, blood, rain/snow, go-arrow, bubbles, gore drops), `rpg_bb.gfx` (player portraits), `inter.gfx` (UI backgrounds, player select), fonts `scorei/scoreo/ammo1/ammo2/cool/rpg/menu/menus.lft`, sounds `misc.sfx`, `bb.sfx`, music `inter.mbk`, `interl.mbk`.

### C.3 Enemy IDs (`event_ennemi.cpp`) — 50 numeric IDs, 64 classes

| Range | IDs → class | World |
|-------|-------------|-------|
| 0–8 | 0 basic Smurf (random skin among 6), 1 greedy, 2 prankster, 3 wild, 4 farmer, 5 strongman, 6 Smurfette, 7 stork, 8 Big Smurf (unfinished in 2002; finished in Phase 5a and placed at the end of snuf2) | Smurf |
| 100–104 | Pikachu, Bulbasaur, Charmander, COM (hovering flyer), flying COM (swooper) | Pokémon |
| 200–209 | sky, star, boom, cloud, zombie, shaman (boss), heart (unused), Jedi, Jedi hero, Snork-in-Black cameo | Care Bears |
| 300–307 | grunt, gunner, governor, mage, seahorse rider, Snork-in-Black, Diabolo (unused boss), Snork-in-Black boss | Snorks |
| 400–401 | Lemmings, flying Lemmings (behaves identically) | Lemmings |
| 500–508 | Toad, Yoshi, princess guard (no factory case), Princess, Tails, Yoshi turret, Knuckles, Rayman, flying Tails | Video world |
| 600–605 | Luigi, Sonic, small Mario ("Mariotapette"), Lara, spikes, weight | Video boss |
| 700 | Mario (final boss, 16 attack patterns) | Final |

Summoned, not placed: the four Pokémon "heroes" (by Rayman), princess guards (by the Princess), spikes (by Lara), Luigi, Sonic and the Pac-Man car (by the boss scripts). Never placed or spawned in shipped levels: 8 Big Smurf (placed in snuf2 since Phase 5a), 206 heart bear, 306 Diabolo, the Mario hologram (its only spawn site, `ennemi_mario.cpp:141`, is commented out), Pac-Man (its only spawn site, `ennemi_voiture_pacman.cpp:69`, is inside a commented-out block) and the car bear (no ID).

### C.4 Prop IDs (`event_fond_anime.cpp`) — moving platforms marked ★

100–111 Pokémon: alarms ×3, arrow, sky, lift ★, switch ★, falling crate ★, lift 2 ★, bomb ★, neon level 2/3 · 300–305 Snork: airlock ×2 ★ (unused), fish ×4 (unused) · 400–403 Lemmings: fan, light, water, light 2 · 500–507 video: question block, coin, ring, sunflower ×4, flag · 600–611 video: LCD, world map, electronics ×4, snow screen, camera screen, assembler, Windows, DOS, boss fan · 700–705 final boss: battle cruiser, hyperspace rock, monolith ★, rock ×2 ★, star (unused) · 900 bomb (duplicate). Shipped levels place props only in dork (100–111), lem (400–403), video (500–507), videoboss (600–611) and finalboss (700–704); the Smurf, Care Bear and Snork levels use none.

### C.5 Pickups (`event_bonus.cpp`), vehicles, weapons

Pickups 0–7: SMG, shotgun, laser, beer (+2 HP), invincibility (550 ticks), barrel (full heal), cow bomb, flamethrower. Drop tables: 9 lists of 20 in `make_bonus.cpp`, selected by flag 5. Vehicles: 0 stork (the seahorse vehicle exists but is not placed). Weapons: rifle (infinite); respawn ammo SMG 100 (Blip) / shotgun 10 (Blop); pickups give SMG 200 (cap 400), shotgun 20 shells (cap 40; 17 pellets per shell), flamethrower 1000 (cap 2000), laser 750 (cap 1500); "super weapon" after a flawless level = laser (Blip) or flamethrower (Blop, except in Snork levels).

### C.6 Hard-coded index usage (migration load)

| Bank / sound bank | Sites | Bank / sound bank | Sites |
|-------------------|------:|-------------------|------:|
| `pbk_ennemis[n]` | 676 | `sbk_niveau.play/stop` | 62 |
| `pbk_niveau[n]` | 153 | `sbk_misc` | 26 |
| `pbk_misc[n]` | 53 | `sbk_bb` | 15 |
| `pbk_bb[n]` | 33 | `pbk_own[n]` (player) | 32 |
| `pbk_inter[n]` | 23 | `rand()` calls | 375 (365 outside comments) |

Other coupling counts: 90 manual list iterations (`SuperListe::start()`), 220 `ajoute()` call sites (110 enemy projectiles, 27 blood sprays, 17 level-event loads, 14 enemies, 8 gore, 8 banners, 7 props, 6 player-projectile pool inserts…), `tete_turc` used in 39 files, `game_flag` written in 31 files, `y_plat` written by 8 platform classes, `murs_opaques` written by 2 classes, direct `backSurface->BltFast` in 4 gameplay files (`ennemi_pic.cpp`, `fond_poke_ciel.cpp`, `fond_snork_sas.cpp`, `fond_assembleur.cpp`) plus `tir_bb_laser.cpp`; `texte_cool.cpp` prints straight to `backSurface`.

### C.7 Default bindings (`config.cpp:173`)

P1: arrows, fire L-Ctrl, jump "L-Alt" (actually bound to the Menu key, D-37), special Space. P2: up Q, down S, left D, right F, fire Tab, jump G, special H. Cheats (`/cheat`): F1/F5 ammo, F2/F6 cow bomb, F3/F7 life, F9 win level, F11 lists, F12 FPS. Command line: `/safe`, `/cheat`, `/quiet`.

### C.8 Decoded image memory per level (computed from the shipped banks)

Sum of width × height of every image in the banks a level loads (decor, props, enemies, dialogue portraits), at 2 or 4 bytes per pixel. The global banks (`blip`, `blop`, `bb`, `misc`, `rpg_bb`, `inter`) add **4.6 MB (16-bit) / 9.3 MB (32-bit)** on top. "Distinct screens" is how many different decor images the level's screen sequence actually uses.

| Level | Screens | Distinct screens | Level banks | 16-bit | 32-bit (today) |
|-------|--------:|-----------------:|-------------|-------:|---------------:|
| snuf1 | 13 | 10 | snufdec1, snufpplan1, snufenn | 8.0 MB | 16.0 MB |
| snuf2 | 13 | 11 | snufdec2, snufenn, snufrpg | 8.4 MB | 16.8 MB |
| dork | 5 | 4 | pokefond, dorkfonda, pokeenn | 6.4 MB | 12.8 MB |
| bisous1 | 9 | 8 | bisoudec1, bisoufond, bisouenn, bisourpg | 8.0 MB | 16.0 MB |
| bisous2 | 9 | 7 | bisoudec2, bisouenn, bisourpg | 7.2 MB | 14.4 MB |
| bisous3 | 13 | 12 | bisoudec3, bisoufond3, bisouenn, bisourpg | 10.6 MB | 21.3 MB |
| snorkniv | 14 | 14 | snorkfond1, snorkfonda, snorkenn2, snorkrpg | 11.9 MB | 23.8 MB |
| snorkniv2 | 10 | 6 | snorkfond2, snorkfonda, snorkenn2, snorkrpg | 7.0 MB | 13.9 MB |
| lem | 4 | 3 | lemdec, lemfond, lemenn | 3.4 MB | 6.8 MB |
| video | 18 | 18 | videofond, videofonda, videoenn, videorpg | 18.1 MB | 36.3 MB |
| videoboss | 4 | 4 | videofondboss, videofondaboss, videobossenn, bossrpg | 6.9 MB | 13.9 MB |
| finalboss | 4 | 3 | finalbossfond, finalbossfonda, finalbossenn, finalbossrpg | 3.0 MB | 6.0 MB |

Worst case today (`video`): 36.3 + 9.3 = **45.6 MB** of decoded images at 32-bit, before the 840×480 ring buffer and the three 640×480 working surfaces (1.6 + 3.7 MB) and before any SDL/driver overhead. With decor screens decoded lazily (2–3 of 18 resident) the level's decor drops from 22.1 MB to 2.5–3.7 MB in 32-bit. (The 16-bit figures of the first version of this plan were for the dropped handheld targets.)


---

## Appendix D — Defects and hazards found during the read

Numbered for reference from the phases. "Port" = introduced by the 2013–2014 SDL shim; "Orig" = present in the 2002 code; "Data" = in `data.zip`. Paths relative to `vc-projects/Blip_n_Blop_3/`.

### D.A Memory safety and undefined behaviour

| ID | Where | Problem | Phase |
|----|-------|---------|-------|
| D-01 | `super_liste.cpp:154,175` (Orig) | `SuperListe::supprime()` calls `free()` on objects created with `new` (220 `ajoute()` sites, most of them `new`); destructors never run. Switching to `delete` will start running `TirEpeeLaser`'s destructor (`sbk_niveau.stop(13)`), possibly after the bank is closed. | 4 |
| D-02 | 65 sites, all in `Engine/windows.cpp` (42), `Engine/ddraw.cpp` (17), `Engine/dinput.cpp` (6) (Port) | Non-void functions without `return` (UB; clang may emit a trap in optimised builds). | 0 |
| D-03 | `fond_eau.h:8` (Orig) | Non-`const` globals defined in a header (`water_seed`, `water_seed2`). | 5e |
| D-04 | `ennemi_princesseguard.cpp`, `tir_epee_laser.cpp`, `ennemi_rayman.cpp`, cached target pointers in 6 classes (Orig) | Raw cross-object pointers. Genuine use-after-free: the guards read `princesse->a_detruire` after the Princess was freed (`SuperListe` iterates newest-first, so the guards update before her, and `cleanLists()` frees her at the end of that tick). Stale-state writes rather than freed memory: the lightsaber writes `lanceur->a_epee` into a carbonised Jedi (`ETAT_CARBONISE` is not checked at `tir_epee_laser.cpp:37`; the tick order enemies → shots → purge means it never touches freed memory), and Rayman dereferences `tir_poing` without a null check although nothing else can destroy the fist today. The six cached-target classes (`EnnemiBisouAuto`, `EnnemiSmurfSauvage`, `EnnemiGrandSmurf`, `TirBisouMortel`, `TirEpeeLaser`, `TirSnorkyGouverneur`) hold copies of `tete_turc`; players leave the list through `supprimePorc()` and are freed only after the game, so those copies go stale rather than dangling. | 4–5 |
| D-05 | `input.cpp:593` (Port) | `(k >> 10) & 0xFF > 0` parses as `(k >> 10) & 1`: only odd-numbered joysticks work; joystick 2 is routed into the keyboard buffer. | 0 |
| D-22 | `input.cpp:217–285` (Port) | `js[e.jbutton.which]` indexes by SDL *instance id*, not slot (out of bounds after hot-plug); axis handler reads `e.jhat.which`. | 0 |
| D-26 | `game.cpp:780` (Orig) | `_read(fic, &ficevent, sizeof(ficevent))` — layout depends on compiler padding/`sizeof(bool)`; no bounds check on `nb_events`, `scr_level_size`, names. | 2 |
| D-39 | `couille.cpp:554` (Orig) | Respawn after being crushed by the scroll: loop over 8 platform layers can run off the end (`y_plat[8][x]`). | 4 |
| D-40 | `picture_bank.h:41` (Orig) | `operator[]` has no bounds check; 676+153+… hard-coded indices. | 2 |
| D-41 | several `fond_*` (Orig) | `y_plat[layer][i]` writes with unbounded `i` loops (lift, crate platforms); `murs_opaques[(y+222)/8]` row may exceed 59. | 5 |
| D-42 | `tir_cruiser_laser.*`, `tir_bb_fusil.*`, `fond_rocher_hyperspace.*`, `bonus.h`, `ennemi_bisou_zombi.*`, `fond_bombe.*`, `ennemi_diabolo.*` (Orig) | Uninitialised members used (`npic`, `etape_dx/dy`, `x_delay`, `local_phase`, `my_phase`, bonus pointer, `dx`); the `x_delay` and bonus-pointer reads are latent with the shipped data (rock events have 0 ≤ x < 640, `dork.lvl` places exactly three bombs). | 5 |
| D-43 | `ennemi_toad.cpp`, `ennemi_knukles.cpp`, `ennemi_sonic.cpp`, `ennemi_bisou_chaman.cpp`, `ennemi_lara.cpp` (Orig) | Latent division by zero (`12-3*speed`, `10-2*speed`, `10-speed` — zero only for speeds 4 / 5 / 10, which no current constant produces; the `sqrt` distance in the shaman and Lara has no zero guard at all). | 5 |
| D-44 | `ennemi_sonic.cpp:579` (Orig) | `y_plat[0][y]` — x and y swapped. | 5g |
| D-20 | `game.cpp:3291,3435` (Orig/Port) | Shake and deformation blit `backSurface` onto itself (overlapping source/destination). | 1 |
| D-13 | `ben_debug.cpp:105,121–123` (Port) | `_itoa(int(long(ptr)))` truncates 64-bit pointers; `char r[10]` too small for negative 32-bit ints. | 1 |
| D-17 | `hi_scores.cpp:42,47` (Orig) | `0x35674a1f << i` overflows `int` for i ≥ 2 (signed shift UB). | 2 |

### D.B Behaviour regressions of the SDL port (fix before recording golden masters)

| ID | Where | Problem |
|----|-------|---------|
| D-06 | `Engine/windows.cpp:155` | `if (loopmode = FSOUND_LOOP_NORMAL)` assignment; both branches set `loop = 0` → flamethrower/laser loops play once. |
| D-23 | `Engine/windows.cpp:180`, `music_bank.cpp:103,110` | `Mix_PlayMusic(…, 0)` → level music never loops; `setVol` only affects the unused MOD path. |
| D-32 | `lgx_packer.cpp:713` | `halfTone()` ignores its rectangle and darkens the whole surface (menus, dialogue boxes). |
| D-37 | `input.h:47` | `DIK_LMENU` mapped to `SDLK_MENU` (context-menu key) instead of Left Alt → default jump key is wrong; name tables say "LEFT ALT". |
| D-21 | `fonte.cpp:235`, `ben_debug.cpp:74,79` | `printC` logs every call; `Debug::Msg` reopens the log file per message → heavy I/O every frame in menus. The log line also runs before the original NULL check, so a missing string crashes the game: `snuf2.lvl` shows text 150, absent from `uk.dat` — **the SDL port cannot finish the second level** (found and fixed in Phase 0). |
| D-08 | `graphics.cpp:198` | A texture is created and destroyed every frame (measured 0.68 ms per frame on the study machine; a streaming texture measured slower there — a cleanliness issue, decide the present path per target, §3.14). Phase 1: the per-frame texture is gone; measured again with `tests/bench/present_bench.cpp` (medians: per-frame texture 0.24–0.30 ms, streaming 0.37–0.42 ms, window surface with conversion 0.44–0.48 ms), the streaming texture is kept on desktop (docs/progress.md). |
| D-19 | `cine_player.cpp` | Cutscene alpha fades (`setalpha`/`setdeltaalpha`) and clip colours are parsed but inert (the DirectDraw blend routine was never ported). |
| D-45 | `blip_n_blop_3.cpp:570`, `game.cpp:2208` | `SetTimer` is a stub → `fps_count` never updates (FPS display shows 0). Fixed in Phase 1: the counter now uses game time. |
| D-33 | `graphics.cpp:44` | `SDL_CreateWindow` receives the size as the position (cosmetic). |
| D-24 | `sdl_surface.h:98` | `new`/`delete` of `SDL_Rect` per blit. |
| D-59 | `graphics.cpp:114`, `sdl_surface.h:28,35` | Surfaces are created with an alpha mask and SDL's default `SDL_BLENDMODE_BLEND` is never reset (the reset is commented out), so every blit uses `SDL_Blit_Slow`: 49–68 % of the main thread in two profiles; ~20× slower colour-keyed sprites and ~2.7× slower full-screen copies than `BLENDMODE_NONE` (§3.14). |
| D-60 | `scroll.cpp`, `graphics.cpp:198` | Three full-screen passes per frame (ring buffer → back buffer, back buffer → texture, texture → display) plus per-frame 2-pixel-column copies into the ring buffer. |
| D-62 | `game.cpp` (shake/deformation fills), `cine_player.cpp` (clip fills) | `FillRect(…, 0)` writes *transparent* black (alpha 0) into the back buffer; harmless only while every later copy ignores alpha — must become opaque fills once blending is disabled consistently. |

### D.C Data defects

| ID | Where | Problem |
|----|-------|---------|
| D-10 | `data/bisou.mbk`, `credits.mbk`, `dork.mbk`, `end.mbk`, `finalboss.mbk`, `lem.mbk`, `snork.mbk`, `video.mbk`, `videoboss.mbk` | Backslash paths → `fopen` fails on Linux/macOS → `chargeNiveau` fails → the campaign ends after world 1 unless `/quiet`. Only the three banks needed to reach the first world were fixed in 2014. |
| D-46 | `snorkniv.lvl`, `snorkniv2.lvl` | Reference `Snork.mbk` (capital S) → case-sensitive filesystems fail. |
| D-11 | all `.lvl` | Uninitialised bytes after the NUL in the 7 name fields; `lem.lvl`'s 7th field starts with NUL followed by garbage. |
| D-47 | `data/theme.mp3`, `data/snuf.mp3` | 4.6 MB of stale duplicates of the `.zik` files. |
| D-25 | `config.cpp:107`, data | `lang_type` forced to UK; `fr.dat` never existed in the repo. |
| D-48 | `uk.dat` | Windows-1252 (`0x92` curly apostrophes in strings 207, 504, 523); fonts are byte-indexed. |
| D-49 | `menu_main.cpp:325`, `menu_game.cpp:237-270` | Remaining `data\\...` literals in code. |
| D-63 | `uk.dat`, `ennemi_princesse.cpp:89,185`, `snuf2.lvl` | The Princess requests banner texts 517 (escape) and 519 (killed), but `uk.dat` has no ids 508–519; `snuf2.lvl` requests text 150, absent too. `txt_data[n]` stays NULL; the 2002 `printC` returned on NULL (blank banner), the port crashed (D-21). |
| D-64 | `tambour.zik`, `gameover.zik` | Not Ogg like the other `.zik` files but 2002 MP3s starting with 417 zero bytes; SDL_mixer rejects them, so the pre-level drum roll and the game-over music never played in the port. Fixed in Phase 0 by removing the padding (`tools/data/fix_legacy_data.py`). |

### D.D Design hazards (not bugs today, blockers for the target architecture)

| ID | Problem | Examples |
|----|---------|----------|
| D-50 | Hidden static state surviving across levels/replays | file statics in `ennemi_bisou_zombi.cpp`, `ennemi_lemmings.cpp` (`wait_no`), `ennemi_toad.cpp`, `vehicule_cigogne.cpp` (`first`); static members `EnnemiSmurfSauvage::wait_shoot/etape_wait` (shared by all instances, reset by every constructor), `FondCube/Piece/Ring` sync counters, `FondSnorkSas1::etat` (never reset); non-static file-scope globals in `tir_bb_fusil.cpp:34-38` (shotgun spread counters) and `fond_bombe.cpp:23` (`save_time`). |
| D-51 | Level-specific absolute coordinates and flag meanings inside enemy classes | wild Smurf clamp 5850; shaman teleport 8144/8082/7653; Snork boss perches 5831–6351; Rayman 10428/10930; Lara's 18 spikes; Luigi pit 1500–1550; `murs_opaques[28..55][108]`; Lemmings cow bomb at x=1500; Diabolo `xmin = 80` and knee-drop targets x = 590 / 50; `game.cpp:289` disabling the flamethrower by level file name. |
| D-52 | Script registers by raw index with per-level meanings | `game_flag[0..3]` mean different things in every level; `game_flag[6]/[7]` written by raw index in `fond_poke_interupteur.cpp`. |
| D-53 | Macro and table clashes across headers | `SPEED_MAX` 6 vs 7, `DELAY_ACCELERATION` 8 vs 20, `ETAT_ATTENDS` 100 vs 101, lowercase `#define tolerance 5`, duplicated `anim_*` tables with different values in 3 files, mismatched include guards (`tir_arc_ciel.h`, `tir_etoile.h`, `tir_mario_fireball_vertical.h`), `ennemi_snork_base_1.h` is a broken stale copy of the Lemmings header (re-declares `EnnemiLemmings`, mismatched guard, no `#endif`, included nowhere), headers without includes (order-dependent). |
| D-54 | Copy-paste slips in content | `game.cpp:321` resets `player1` while handling `player2`; `TirEtoile` sets the same facing in both wall branches; Gouverneur shot sound never plays; `SnorkSas1` closes 12 columns, `Sas2` reopens 6; the common `onAvance()` pattern (basic Smurf, Mario, Luigi, small Mario, hologram…) switches to `ETAT_TOMBE`, calls `onSaute()` and then falls through to the walking code in the same tick — a double move on the frame a platform ends. |
| D-55 | Objects that never die | 7 of the 11 gore/debris files — 10 of 16 classes, `gore_bisou_boom.h` holds 3 and `morceaux_tete_snorky.h` 4 — (`gore_pied_bisou_zombi`, `gore_pieds_bisou_ciel`, `gore_pieds_bisou_etoile`, `gore_tete_bisou_coeur`, `morceau_rayman`, `morceau_tete_rayman`, `morceaux_tete_snorky`) and several bosses never call `updateADetruire()`; permanent "enemies" (small Mario, stork, cameo, car) keep the enemy count above zero and cap the kill percentage. |
| D-56 | Floating point in a fixed-point game | `-8 + dx*0.25` (Diabolo), `-9 + speed*0.25` (Luigi, Mariotapette), `int(sqrt(...))` (shaman; Lara converts the `sqrt` result implicitly); non-deterministic across platforms only if `-ffast-math`, but inconsistent with the integer design. |
| D-57 | Blocking loops and busy waits | `Input::waitKey/waitClean/anyKeyPressed`, `wait()` spinning on `GetTickCount`, `Sleep(2000)` per level, nested render loops in every screen; measured 64–77 % of a core while the main menu is idle (52–66 % in a second run). |
| D-58 | Dead code and stale files | `alpha_blend.cpp` (793 lines; every blending routine is commented out, only `IsMMX()` and two globals remain live), `sdl_texture.h` (a class declared, never defined or used, yet included by `graphics.h`), `impactlaser.h` (0 bytes), `vehicule_test_tourelle.h`, `p_plan_test.h`, `trace.h`, `precache.h` (a no-op: `fseek` returns 0 so `len` is 0), the FMOD 3.4 headers `fmod.h`/`fmod__errors.h` kept only for the shim, `MyDX.*`, VC6 cache files, 13 copies of the editor `.exe`. |


### D.E Found while implementing Phase 0 (all fixed in Phase 0)

| ID | Where | Problem |
|----|-------|---------|
| D-65 | `input.cpp`, hat handler | Tests `e.jhat.hat` (the hat's index) instead of `e.jhat.value`: joystick hats (D-pads) never registered. |
| D-66 | 22 calls in `ennemi_luigi.cpp`, `ennemi_mariotapette.cpp`, `ennemi_poid.cpp`, `ennemi_rayman.cpp`, `ennemi_snorky_gouverneur.cpp`, `moceaux_tete_snorky.cpp`, `tir_bb_vache.cpp` | Two `rand()` calls as arguments of the same call: the evaluation order is unspecified, so Clang and GCC (and MSVC) drew the numbers in different orders and runs differed between compilers. Now drawn in source order. |
| D-67 | `data/bb.cfg` (shipped) | Player 2's bindings are DirectInput scan codes (0x10, 0x1F, 0x20, …), not SDL key codes: player 2's keyboard controls were broken with the shipped configuration. Deterministic runs use the built-in defaults; the Phase 1 config importer must not trust these values. |
| D-68 | `ben_maths.cpp:120` | `char(sqrt(i))` overflows `char` for i ≥ 16384 (undefined behaviour, flagged by UBSan); the table is unused. |
| D-69 | several classes (D-42 and others) | Uninitialised members read their value from the heap (the wild Smurf's `shooting` flag decides whether its gun is drawn). Phase 0 allocates legacy objects zeroed (`zero_new.cpp`) and compiles the legacy code with `-ftrivial-auto-var-init=zero`; the classes themselves are fixed in Phase 5. |

### D.F Found while implementing Phase 1 (all fixed in Phase 1)

| ID | Where | Problem |
|----|-------|---------|
| D-70 | `hi_scores.cpp`, `HiScores::crypte()`, `save()`, `load()` | The score-file key `0x35674a1f << i` overflows a signed `int` for every entry (undefined behaviour, flagged by UBSan in the first non-deterministic sanitizer run; replays never load or save scores). The checksum sums can overflow too on a corrupt file. Fixed with unsigned arithmetic producing the same 32 bits: the shipped `bb.scr` still loads and is written back byte for byte. |
| D-71 | `l_timer.cpp`, `LGetTime()` | The real-time clock converted the performance counter with single-precision floats. With a 1 GHz counter (macOS) its step grows to 0.2 ms after 30 minutes, 0.4 ms after 1 hour and 1.6 ms after 4 hours; one tick is 11.8 ms. Deterministic runs were not affected. Replaced by exact integer microseconds. |
| D-72 | `Engine/windows.cpp` (`FSOUND_Init` shim) | The SDL port never called `Mix_AllocateChannels`, so SDL_mixer mixed 8 voices instead of the 32 the game requested from FMOD (`CHANNEL_NUMBER`): a ninth simultaneous sound was dropped. The platform audio allocates 32 voices. |

### D.G Found while implementing Phase 2

| ID | Where | Problem |
|----|-------|---------|
| D-73 | `game.cpp`, `Game::showCredits()` (2014 port regression) | The 2002 sources were Windows-1252, so the credit literals (`"Jérôme Karaban"`, …) were single bytes that index the accented glyphs of `rpg.lft` (0xE0, 0xE8, 0xE9, 0xEA, 0xF4). Commit `fa5d0ca` ("Convert files to utf8", 3 January 2014) re-encoded the sources as UTF-8, turning each accent into two bytes with no glyph: the credits showed "Jrme Karaban". Fixed by writing the ten literals as Windows-1252 escapes (`"J\351r\364me"`); re-recorded the `attract` and `campaign` frame fingerprints (only credit frames changed; the campaign world-state fingerprint is unchanged). |
| D-74 | `hi_scores.cpp`, `HiScores::save()` / `load()` | The score checksum adds the name bytes as plain `char`, which is signed on x86 (MSVC, and Clang/GCC on x86) but unsigned on ARM Linux (Linux on ARM machines). A name byte of 0x80 or more would make a score file written on one platform fail its checksum on the other, and the game would silently reset the table. The shipped `bb.scr` has no such byte, and the game types ASCII names, so no run is affected today. The checksum is now computed on signed chars everywhere (`bb::core::assets::readScr` / `writeScr`). |

### D.H Found while implementing Phase 3

| ID | Where | Problem |
|----|-------|---------|
| D-75 | `config.cpp` `vSyncOn`, `menu_main.cpp` / `menu_game.cpp` (VSYNC option), 2014 `graphics.cpp:51` | The VSYNC menu option has done nothing since the 2014 SDL port: the renderer is always created with `SDL_RENDERER_PRESENTVSYNC`, and nothing but the menus and the settings code reads `vSyncOn` (the 2002 DirectDraw uses are commented out). The rewrite kept the behaviour: `videoOpen()` leaves `DisplayOptions::vsync` at `true`. Open: honouring the option needs a frame limiter for the non-vsync case (otherwise the screens would run unpaced). |

### D.J Status changes in Phase 5

| ID | Change |
|----|--------|
| D-41 | Fixed for the Pokémon props (5b): `CollisionMap::setPlatform()` clips; the other moving platforms are still 2002 code. |
| D-42 | Fixed for the bomb (5b): no pickup for a fourth bomb. |
| D-50 | `fond_bombe.cpp` `save_time` removed (5b): the bomb defused last holds the timer. |
| D-52 | Pokémon props (5b) use named registers (bombs defused = 0, switches on = 2). |
| D-69 | Closed (5o, 24 October 2026): every class is rewritten with initialised members; the zeroing allocator (`zero_new.cpp`) and the zero-initialised locals of the legacy target are gone. Checked: the thirteen replays keep their fingerprints with an allocator that fills each allocation with `0xA5`. |
| D-75 | Fixed (24 October 2026): the window is created with the VSYNC option, the menu's toggle applies at once (`Display::setVsync()`, `SDL_RenderSetVSync`), and without vsync the application paces the presented frames to 60 per second (measured: 16.6 ms per frame). Deterministic and headless runs are unchanged. Then measured: vsync does not hold the frame rate at 60 (120 frames per second on a 120 Hz screen; once 717 per second with vsync on, the screen reporting 60 Hz), and the character choice moves by frame; so the pacing to 60 frames per second applies with vsync too (measured after, vsync on: "439 frames presented, 59 per second", rounded down). |

### D.K Found while implementing Phase 5

Kept as they are in the ports (identical replays); to fix in a later, deliberate change.

| ID | Where | Problem |
|----|-------|---------|
| D-79 | `tir_epee_laser.cpp` (now `Lightsaber`) | On its way back, the lightsaber checks `dy < 10` (its vertical speed) where the distance `ddy < 10` was meant, so it can fly past the Jedi before he catches it. |
| D-80 | `tir_etoile.h` (now `BouncingStar`) | A star going right that meets a wall stays where it is ("turns" to the right it already faces), while one going left turns back. |
| D-81 | `ennemi_yoshi.cpp` `onAttaque()` (now `Yoshi::tongue()`) | The tongue animation runs at one step every 2 ticks to the right and every tick to the left, so a Yoshi facing left attacks twice as fast. |
| D-82 | `ennemi_tails.cpp`, `ennemi_tailsvolant.cpp` `onMeure()` (now `Tails::die()`) | Once the death animation is over, a Tails facing left with a wall on its left slides right (the `else` of a combined test) until it reaches engraving ground. |
| D-83 | `ennemi_knukles.cpp` `onMeure()` (now `Knuckles::die()`) | Knuckles' corpse is engraved on platform layer 4 too (the test is commented out); the other enemies of the world are not engraved there (Yoshi, the turret and the heroes vanish, Toad, Tails, the Princess and her guards wait for other ground). |
| D-84 | `ennemi_rayman.cpp` (now `Rayman::summon()`) | The COM hero is not counted in `nb_ennemis_created` but its death counts as a kill, so the kill percentage of video (kills over enemies created) counts one kill too many. |
| D-85 | `ennemi_sonic.cpp` `onAttackvertical()` (now `Sonic::vertical()`) | The landing of the vertical attack reads the floor height at column `y` (`y_plat[0][y]`) instead of `x`: in videoboss that reads 445 (the floor of x 0–479) where his arena's floor is at 469, so he stops 24 pixels up in the air and falls the rest of the way. |
| D-86 | `ennemi_sonic.cpp` | Dead code, not ported: the spine attack (`onAttackepine()`, attack type 2, whose choice is commented out) and `balance_epines()` (all its calls are commented out). Sonic still advances the counter that would have chosen the spine, as in 2002. |
| D-87 | `tir_cruiser_laser.cpp` (now `CruiserLaser`) | The laser flash moves by its image number (`npic`), a member the constructor never sets: with the zeroing allocator of the build it is 0 and the flash never moves (under the 2002 MSVC build it was garbage). |
| D-88 | `ennemi_mario.cpp` `onRafaleberserker()`, `onPluiedefeu()` (now `Mario::berserk()`, `Mario::fireRain()`) | Screen positions (640, 620, 20) compared with Mario's level x (about 1400 to 2000): the berserk's return burst is skipped (its step count comes out negative, so its end test holds at once); in the fire rain an outgoing burst to the right ends at once, and one to the left runs until a fireball would land off the screen, which ends the whole attack. |
| D-89 | `game.cpp` `Game::startLevel()` | Placing the second player resets the *first* player's animation (`player1->etape = 0; player1->ss_etape = 0;` in the `player2` block). |
| D-90 | `couille.cpp` (now `bb::game::Player`) | Both players stop the scroll through the first player's flag (`no_scroll1`, `camera.stopped[0]`); `no_scroll2` is never set. When one player's return ends, the scroll restarts even if the other is still dying. |
| D-91 | `couille.cpp` `update()`, crushed by the scroll (now `Player::crushedByScroll()`) | The player comes back on the first layer with a platform at the screen centre; with no platform there the 2002 loop read `y_plat[8]`, past the last layer. The port stops at layer 7 (65535: no platform), so the player falls out of the level and loses all lives — defined, but not what was meant. |
| D-92 | `tir_bb_laser.cpp` `affiche()` (now `LaserBeam::draw()`) | A left beam that starts off the left edge copies `largeur - xx` pixels (xx < 0) where `largeur + xx` was meant: wider than the visible beam. |
| D-93 | `couille.cpp` `update()`, crushed by the scroll | Crushed on the last life, the player is gone with the scroll still stopped (`no_scroll1 = true` before the early return): in a two-player game the scroll stays frozen until the other player dies and comes back. |
| D-94 | `game.cpp` `wait_for_death` (now `Level::deathWait`) | The 200-tick wait after the last player dies is reset only when a campaign starts, not per level: after a bonus level where both players died (their lives are restored), the next time all players are dead the game is over at once. |

### D.I Found while implementing Phase 4

Kept as they are, so that the replays stay identical; Phase 5 fixes them (and re-records the fingerprints).

| ID | Where | Problem |
|----|-------|---------|
| D-76 | `ennemi_smurf_costaud.cpp` `onMeure()` (now `StrongmanSmurf::die()`) | The death animation stops at step 13 but the corpse is engraved only from step 14, so it never is: the strongman's body stays an entity until it leaves the screen, and never becomes part of the decor like the other Smurfs'. |
| D-77 | `ennemi_smurf_cigogne.cpp` (now `Stork`) | The stork is placed as an enemy, so it counts in `nb_ennemis_created`, the divisor of the kill percentage of the level summary, but it cannot be hit (no hitbox) and never dies: 100 % kills is out of reach in snuf2. |
| D-78 | `ennemi_smurf_sauvage.cpp` (now `WildSmurf`) | Besides the shared static timer (Phase 5a): the constructor chooses its image and aim from `dir` before the level event sets it, so it always starts with the facing-left ones (corrected at the first update); and `x` is clamped to 5850, a position of snuf1, in every level. |


### D.L Status changes on 24 October 2026

Fixed deliberately; the replays were re-recorded (docs/progress.md, "Completing the plan"). Each site has a comment giving the 2002 behaviour.

| ID | Status |
|----|--------|
| D-40 | Fixed: an index outside the bank draws a transparent 1×1 sprite. |
| D-43 | Fixed: the shaman's and Lara's distances are at least 1. The walk animations' `12-3*speed` (Toad) and `10-2*speed` (Tails, Knuckles) stay: checked in `walkers.cpp`, their walking speeds are 0 to 3 (Toad) and 0 to 1 (the charges and flights set it back to 1 before walking). |
| D-50 | Fixed: the 14 game globals live in `World::shared<T>()`, cleared at each level start; the app's state is owned by `main`. |
| D-51 | Fixed: level positions are named level markers (`World::marker()`), in the source levels, the pack (version 2) and, for the 2002 files, `assets_level_markers.cpp`. |
| D-52 | Fixed: every register is used by a per-world name. |
| D-54 | Fixed: the `onAvance()` double move (fall, then a step on the same tick) in every class that had it. The other slips are D-80, D-89 and D-92. |
| D-55 | Fixed: the never-dying "enemies" (stork, Snork in Black cameo, small Mario, Pac-Man car) are not counted in the kill percentage, Sonic is; and (25 October) the debris that end only when they land (Rayman's limbs and head, the Care Bears' walking and dancing legs, the Snork boss world's grunt head and trunk and the rider's body) also end once fallen below the screen (`Actor::dieWhenFallenOut()`, y > 600), where nothing lands. No replay drops a piece into a hole, so the fingerprints did not change; a unit test does. |
| D-76 | Fixed. |
| D-77 | Fixed: the stork does not count. |
| D-78 | Fixed: own timer per wild Smurf; the 5850 limit is a snuf1 marker. The constructor's facing test is kept: it has no visible effect (the first update sets the image). |
| D-79 … D-85 | Fixed. |
| D-86 | Nothing to fix (dead 2002 code, not ported). |
| D-87 … D-94 | Fixed. |
| D-56 | Fixed: no floating point left in the game. `squareRoot()` (`trig.hpp`) for the shaman's and Lara's `int(sqrt())`; the plumbers' jump and the cruisers' shots in integers. Each form gives the 2002 values (unit test: every square root up to 2^22 and a spread to `INT_MAX`, every speed and distance in ±1000); the replays kept their fingerprints. |
| D-19 | Closed: the alpha fade was commented out in the original `cine_player.cpp` (commit d48c5bd) too, and no shipped cutscene uses `setalpha` or `setdeltaalpha`. The colours are D-95. |
| D-95 (new) | Found and fixed: the 2002 player cleared the cutscene screen with layer 0's `setbackcolor` colour and filled the bars with its `setclipcolor` colour; the 2014 SDL port always used black, so the two white flashes ("Flash blanc", 2 renders each) of `intro.cin` and of `end.cin` were lost. Restored; the `attract`, `menus`, `finalboss` and `campaign` replays (the ones showing a cutscene past a flash) were re-recorded for it. |
| Still open | None of the defects. |

### D.M Found and fixed in Phase 9 (29 October 2026)

| ID | Status |
|----|--------|
| D-25 | Fixed: French text exists again (`assets/source/strings/fr.json`, a new translation: the 2002 `fr.dat` was never in the repository) and the options choose the language. |
| D-96 (new) | Found and fixed: the French key names of `key_translator.cpp` named the places of DirectInput scan codes on an AZERTY keyboard ("Q" for the key at A's place). The game reads SDL key codes, which follow the keyboard's layout, so letters and signs now show the character of the key in both languages; the French words (RETOUR, CONTROLE GAUCHE...) stay. |
| D-97 (new) | Found by CI (UBSan, Linux) and fixed: before its first mouse event Dear ImGui puts the mouse at -FLT_MAX, and the level editor converted that to an `int` (`editor_app.cpp`, undefined behaviour); the level coordinates are clamped first. |
---

## Appendix E — Content catalogue

Compiled from a full read of every `ennemi_*`, `tir_*`, `fond_*`, `bonus_*`, `gore_*`, `morceau*` file, cross-checked against the event tables of the 12 shipped `.lvl` files. During review, every HP value, coordinate, count, flag write and behavioural statement below was re-checked against the source. Tags: **[tt]** targets a player · **[gf]** writes flags · **[gr]** bakes decals · **[sh]** screen shake · **[yp]** writes platform heights · **[mw]** writes walls · **[BS]** draws directly to the back buffer · **[abs]** hard-coded level coordinates · **[dead]** never placed or spawned in shipped levels.

### E.1 Enemies by world

**Smurf (IDs 0–8).** *Basic Smurf* (hp 100): walker that jumps now and then; ID 0 picks one of 6 skins at random — brainy, vain, chilly, lazy, peon (header-only variants that tweak one or two stats and draw an accessory — except the lazy one, whose accessory draw is commented out). *Greedy* (150): stationary, lobs cakes [tt][gr]. *Prankster* (150): plants timed gift bombs; a fatal hit while carrying one → explodes and burns. *Wild* (150, "brujah"): abseils from above near screen centre, aims in 6 directions, fires 5 shots, climbs away (not a kill); death bakes rope and body with 6 decals; shared static timers; clamp x ≤ 5850 [abs]. *Farmer* (200): fast charger; on death a severed-head animation plays at the death point while the headless body keeps running (fall-through from `ETAT_MEURE` into the walking code). *Strongman* (2000): Street-Fighter mini-boss (hadoken / dragon punch / hurricane kick), starts boss music and the timer (`flag[2] = 1`), `flag[1]++` on death [gf][tt][gr]. *Smurfette* (200): ledge-aware walker firing homing kisses. *Stork*: scripted flyer waiting for `flag[1] ≥ 5`, sets `flag[0] = 3` when off-screen; never collidable, never deleted, counts as an enemy. *Big Smurf* (5000) [dead in 2002; finished in Phase 5a as snuf2's final boss, with art recoloured from the basic Smurf]: unfinished boss, cannot be hit.

**Pokémon parody (100–104 + summoned heroes).** *Pikachu, Bulbasaur, Charmander* (100): walkers; Pikachu and Bulbasaur bake a decal on death unless on platform layer 4, Charmander never does. *COM* (100): hovers without gravity, drifting sideways between walls and screen edges. *Flying COM* (100): swooper re-aiming at the target's Y on each bounce [tt]. *Pikachu/Bulbasaur/Charmander/Com Hero* (1500/1500/1000/1000; drop weight 100 for the Pikachu hero): four near-identical copies summoned by Rayman, arrive in a two-stage "dorkeball" intro; `flag[1]--` on death [gf].

**Care Bears (200–209).** *Sky* (175): rainbow shot at < 90 px; split death (feet gore + crawling torso, then decal). *Star* (200): bouncing stars; same split death. *Boom* (200): kamikaze, explodes into three gore parts within 60 px. *Cloud* (250): stationary, spawns storm clouds; no blood/sound on death. *Zombie* (1000): cut in half at hp < 500 and keeps crawling; replaces the hit pipeline (no drop, bumps the kill counter itself); all zombies burn when `flag[0] == 7`; shared static moan timer. *Shaman* (11000, boss): after 10 hits teleports between three absolute X positions [abs]; chooses ball or pillar attacks from a sqrt slope; writes `flag[2]` every tick, `flag[0]` drives music and death → `flag[0] = 7` + generators off [gf]. *Jedi* (250): homing boomerang lightsaber holding a back-pointer to its thrower. *Jedi hero* (6000): walks in uncollidable, sets flags on arrival and death [gf]. *Snork-in-Black cameo*: invulnerable NPC that flees on the 301st hit ("Run Forrest" state) [tt][gf]. *Heart* (300) [dead]: head flies off, body sprints; its burn state is an empty `case ETAT_CARBONISE: break;`, so a burnt heart bear freezes until it scrolls off. *Car* (800) [dead]: homing flying car.

**Lemmings (400–401).** *Lemmings* (100): walker with an absolute wall at x < 5; explodes on death; kill #200 makes every lemming suicide, turns generators off and drops a cow bomb at x = 1500 [gf][abs]. *Flying Lemmings*: identical in practice (its constructor's Y is overwritten by the spawner).

**Snorks (300–307).** *Grunt* (200, file misnamed `ennemi_sorky_base_1.cpp`): wavy shot; head pops off and spawns further pieces. *Gunner* (150): aimed shot. *Governor* (150): burst of 5 homing shots (its sound never plays, D-54). *Mage* (200): floats, charges three times, diagonal bolt. *Seahorse rider* (350): 8 px/tick charge; on death the rider's corpse is ejected as debris. *Snork-in-Black* (250): spine bursts. *Snork-in-Black boss* (15000): hops between 4 perches [abs] with a hand-tuned jump table (its perch-position logic is repeated in the death sequence); 6-shot fans; eaten by a shark on death (`flag[0]` 3 → 4); removed only once x > camera + 1000 [gf][tt]. *Diabolo* (30000) [dead]: unfinished kick / tornado / knee-drop boss with shake, float maths, `debug <<` prints and large commented-out sections.

**Video-game world (500–508).** *Toad* (300): accelerating walker, charges on the same platform, otherwise jumps; "brain hurts" gag and a death-convulsion loop on file statics. *Yoshi* (500): shell throw or tongue. *Yoshi turret* (400): stationary shells. *Tails* (150) and *Flying Tails* (≈90 % copy): walk then swoop. *Knuckles* (150): charge or jump; overrides gravity. *Princess* (3700): escort; invulnerable but bleeds until `flag[1]`; spawns 3 guards; escaping requests banner text 517 and killing her requests text 519 and awards a cow bomb — neither string exists in `uk.dat`, so the banners are blank (D-63); `flag[2] = 1` only while guards are still alive (`flag[3] > 0`) [gf][abs]. *Princess guard* (2500): raw back-pointer to the princess (use-after-free, D-04); `flag[3]--` on death; ID 502 has no factory case. *Rayman* (15000) [abs]: summons the four heroes by `flag[1]`, invulnerable while any live; extending fist with a raw pointer; loses fists then feet at hp 10000/5000/1000, the landing limbs write flags that drive the script [gf].

**Video boss (600–605).** *Lara* (15000): never collidable (hp unused); spawns 18 spikes at absolute coordinates; phases on `flag[0]` values 1–4 and 7; aims spines in 6 sectors via sqrt. *Spikes* (`countsAsKill = false`, damage 5): rise/fall by flag, drawn with [BS]; when retracted clear `murs_opaques[28..55][108]` [mw]. *Weight*: right-going hits (shot directions 2–6) accumulate and push it 3 px per batch, dealing no damage; past x > 950 it falls and crushes Lara (`flag[0]` 6/7) with a blood fountain [sh][gr]. *Rayman's head*: decal + `flag[0] = 2` on landing. *Small Mario* (10000) [abs]: knocked back by shots; on death becomes a permanent small Mario, spawns Luigi at x = 1960 and triggers hold-fire. *Luigi* (10000): plunger volleys; death cutscene walks to the pit (x 1500–1550), sprays 16 gore drops per tick for a fixed duration, then spawns the Pac-Man car and Sonic (both at x = 1960); hold-fire. *Sonic* (10000) [abs]: rolling horizontal charge, vertical attack, lightning, ring scatter (`onCharge()` is an all-commented stub; a spike attack `onAttackepine()` exists and is dispatched, but the code that would select it is commented out); "super saiyan" death with growing shake → `flag[2] = 2`. *Pac-Man car* (1000): invulnerable, harmless; drives in, sets `flag[2] = 3` on landing and `flag[0] = 6` after 250 ticks to end the level; the `flag[2]` steps 4 and 5 are inside a commented-out block (`ennemi_voiture_pacman.cpp:41–77`). *Pac-Man* (1000) [dead]: its only spawn site is inside that same commented-out block, so it is dead content like the hologram.

**Final boss (700).** *Mario* (50000): 16 attack patterns chosen by HP thresholds (horizontal/vertical fireballs, bursts, double bursts, homing bursts, fire rain, berserk), [tt]; the hologram summon (`ennemi_mario.cpp:141`) is commented out. *Mario hologram* (hp 1) [dead]: invulnerable blinking fireball thrower, clamped to absolute screen X values (e.g. `x < 630`) [abs].

### E.2 Projectiles (32)

31 classes live in `tir_*` files (24 derive from `Tir`, 6 from `TirBB`, the cruiser laser from `Sprite`); the cruiser's plain shot lives in `fond_tir_cruiser.h`, making 32. Plain linear: spine (`traverse` flag), plunger, Snork gunner shot, wild-Smurf shot, player SMG, player shotgun (spread from global counters). Lobbed/bouncing: cake, shell, star, gift bomb (timed). Homing: kiss, lightsaber (loop sound 13 is stopped explicitly in `update()`; the destructor that would also stop it never runs, see D-01), governor shot. Not classified above: the hadoken, the mage's diagonal bolt and the horizontal Mario fireball. Wave: heart, Snork grunt shot. Special: rainbow, shaman ball, shaman pillar (walks), storm cloud (lightning column to y 480), Sonic lightning [sh], vertical Mario fireball [sh], tornado, Rayman fist (never destroys itself), battle-cruiser shots/lasers (derive from `Sprite` and live in the prop list). Player: rifle, SMG, shotgun, laser (beam with diagonal collision), flamethrower (300-slot pool), cow bomb (decal + 15 gore drops + shake).

### E.3 Props (≈ 50)

*Pure frame cycling (~30):* fan, boss fan, DOS/Windows/LCD/world-map/snow/camera screens, electronics ×4, sunflowers ×4, alarms ×3 (each set near-identical, differing only in image indices), arrow, neon lights, neon level signs, question block / coin / ring (synchronised through static members), hyperspace rock (parallax from initial x), star [dead], fish ×4 [dead]. *Moving platforms [yp]:* monolith (layer 4, carries players and enemies), rocks ×2 (layer 4, players only), lift 2 (layer 4, shown only when `flag[2] > 1`), crate (layer 4), crate platforms 1 and 2 (layers 4 and 3, spawned by the falling crate), lift (layer 4, driven by `flag[2]`). *Scripted:* bomb (defusing gives shotgun → SMG → cow bomb; timer from flag 6), switch (`flag[2]++`, writes `flag[7]` by raw index), falling crate (spawns 4 crates), sky (parallax strip, [BS]), airlocks ×2 [dead][BS][mw] (static state, trigger at x > 1400), assembler screen ([BS] vertical scroll), battle cruiser (two ships duelling with shots and lasers), water (spawns one surface strip per 376 px using header globals).

### E.4 Pickups, gore, debris, vehicles

Pickups: shotgun / SMG / flamethrower / laser each hard-code their weapon tuning; only the shotgun and SMG pickups refuse to replace a flamethrower/laser with ≥ 100 ammo (`bonus_fusil.h:33`, `bonus_pm.h:33`), the flamethrower and laser pickups replace each other unconditionally; beer +2 HP; barrel full heal; invincibility 550 ticks; cow bomb +1. Gore: generic blood drop (velocity-indexed images), Boom-bear ×3 parts, sky/zombie feet (identical logic, different image indices), star feet, heart head [dead], car [dead]; most bake a decal on landing (the generic blood drop simply disappears) and 7 of 11 files — 10 of 16 classes — never time out off-screen (D-55). Debris: Rayman limbs (write `flag[0]`/`flag[2]` on landing), Rayman head, Sonic ring, 3 Snork head pieces plus the seahorse rider's corpse (4 classes in `morceaux_tete_snorky.h`). Vehicles: stork (used twice, in the final-boss level: scripted flight to x ≥ 1480, then drops the player), seahorse (free flight, unused). Weather: 500 pooled rain drops / snow flakes; bubbles when flag 4 is set.

### E.5 What can be data-driven (estimate)

- Enemies: 64 classes, of which 6 were dead content (Big Smurf — finished in 5a —, Diabolo, heart bear, car bear, Mario hologram, Pac-Man). Of the 58 live ones, ~38 are one or two generic behaviours (walker, ledge-aware walker, turret, swooper, charger/jumper, dorkeball intro) plus tables (hp, drop weight, speed, per-facing animation bases, shot type and timing, blood offsets, death style, death sounds); the remaining ~20 are bespoke bosses/scripted actors.
- Props: ~30 of ~45 are animation tables; 7 are one `MovingPlatform` type with parameters (counting the spawned crate platform 1); ~8 are scripts.
- Projectiles: ~8 collapse into one parametric linear/lobbed class; the rest stay small classes.

### E.6 Cut content and curiosities

Cut or unused: Care Bear car and heart, Big Smurf (finished in 5a), Diabolo, Mario's hologram, Pac-Man (spawn commented out in `ennemi_voiture_pacman.cpp:41–77`), fish and airlocks, star prop, guard ID 502, duplicate `case 900` bomb; Luigi's charge attack (`ennemi_luigi.cpp:625`) and Lara spawning the weight herself (`ennemi_lara.cpp:97`) are commented out, and Sonic's spike attack can never be selected (`ennemi_sonic.cpp:136–146` is commented out). Three authors with distinct styles, by the nickname in each file header (Prosper: the core — 94 non-content files, 8 enemies, 19 projectiles — table-driven; Mortimus: 11 Smurf-variant files; Mephisto: Snorks, the video-game world and most props — 48 enemy, 51 prop and 16 projectile files — with heavy per-direction copy-paste and absolute coordinates). Pop-culture gags everywhere (Street Fighter and vampire Smurfs, Jedi bear, Monty Python "brain hurts", Fist of the North Star, Dragon Ball, the 2001 monolith, Lemmings nuke at 200 kills, Lara defeated by pushing a weight with bullets). Typos preserved in identifiers and file names: `sorky`, `pikashu`, `tete___rayman`, `moceaux`, `vehivule`, `Ascenceur`, `Interupteur`, `Knukles`, `HurricanKick`, `MULTIPLACATEUR`, editor labels "Windwos" and "vloant". One class name (`Mariotapette`) uses a French slur and must be renamed in a public port.

---

## Appendix F — Related forks and what to take from them

Checked in October 2026 by cloning both repositories and comparing them file by file with this tree. Statements below were verified in their sources.

### F.1 `benkaraban/blip-blop` — the upstream repository (174 commits, 2013-11-12 → 2019-09-21)

This tree is the upstream state of 13 March 2014 (commit `44cab36` exists in upstream; upstream master is 117 commits ahead). Apart from two README commits by Benjamin Karaban in March 2015, all later commits are from June–September 2019 by Guillaume "Vermeille" Sanchez, merged through two pull requests (the two merge commits are by the 2014 Linux porter, roby65). The README calls the work "mainly trying to modernize the code" and documents a lexicon of the French slang in the sources.

**What the 2019 work did (verified):**

- Build/CI: `cmake_minimum_required(3.1)` with `find_package(SDL2)`, vendored SDL2 removed, data folder unzipped into `data/` and installed next to the binary, CPack NSIS installer; Travis (Linux, clang) and AppVeyor (VS2015, vcpkg `sdl2`, `sdl2-mixer[mpg123,libvorbis]`). Note: the `add_executable` branches use the `WIN32` keyword on the non-Windows side (inverted condition).
- Shim removal: `Engine/` deleted; `windows.h`, `ddraw.h`, `dinput.h`, `io.h` gone; the FMOD vocabulary kept as `fake_fmod.cpp`; a plain `Rect` replaces the Win32 `RECT`; window and renderer are RAII (`unique_ptr` with SDL deleters); SDL failures throw (`errors.h`).
- Containers: every `SuperListe` became `std::list<std::unique_ptr<T>>` (`list_joueurs` stays `std::vector<Couille*>`; `list_tirs_bb` and `list_meteo` are lists of raw pointers to the pre-allocated pools); removal through `RemoveDestroyed` (`erase`/`remove_if`); per-list update/draw loops folded into `UpdateCollection`/`DrawCollection` templates. Their `globals.h:127` comment explains why they moved from `std::vector` to `std::list` after segfaults: entities are appended during iteration and other objects hold raw pointers, so relocation is forbidden. The event lists are iterated with reverse iterators to preserve the prepend order of the old list (their FIXME at `game.cpp:1334`).
- Screens: menus rewritten under `menus/` (`AbstractMenu`, `MenuList`, main/start/options/keys/pause menus; pause and title menus are separate classes sharing `MenuList`/`OptionsPanel` — the README calls them unified) with a new "TOGGLE FULLSCREEN" option (`uk.dat` string 802 added); `HUD` extracted to `hud.h`; the "GO" arrow to a `GoArrow` state machine (`go_arrow.h`); character selection to `CharacterSelection`.
- Assets: `MusicBank` rewritten around a `Music` interface (`Mp3Music`, `ModMusic`) with `std::vector<std::unique_ptr<Music>>` and backslashes replaced at load time (fixes D-10 generically); `PictureBank` holds `std::vector<std::unique_ptr<Picture>>`; `Fonte` uses vectors; `Debug` writes through an `fstream`; key display names are X-macro tables (`keys_us.xmacro`, `keys_fr.xmacro`).
- Timing: `Chrono` (SDL ticks, ms) + `UpdateRegulator` + `MovingAverage` replace `LGetTime`/"glorf". The default `delay_goal` is **11 ms**, so the simulation runs at ≈90.9 Hz instead of the original 11.8 ms / ≈84.7 Hz — a ~7 % speed-up of the whole game (`update_regulator.h`, default-constructed in `game.h:65`).
- Bugs from Appendix D fixed there: D-32 (half-tone respects its rectangle), D-21 (no logging in `printC`), D-46 (`Snork.mbk` lower-cased in both Snork level headers), D-10 (backslash paths handled in code). Still present: D-05, D-22 (`input.cpp:595`), D-06 and D-23 (`fake_fmod.cpp:38,67`), D-08 (texture per frame), 32-bit surfaces, all globals, and all content classes are essentially untouched (a handful of changed lines each, from the reformat and the `std::string` text table).
- Problems they recorded that match ours: screen shake disabled ("works unproperly at least on Linux", `game.cpp:1148`) and the deformation self-blit called "absolutely wrong … overlapping" (`game.cpp:2247`) — our D-20; valgrind-reported uninitialised values — our D-42; `sdl_surface.h:179` "delete this — OH MY THAT'S DANGEROUS".

**To take:** the vector-versus-list lesson (our Phase 4 design of deferred removal plus validated handles must forbid appending during iteration and raw cross-pointers, or it will hit the same crashes); the X-macro key-name tables; the HUD / GoArrow / CharacterSelection extractions as templates for Phase 3; the CI/CPack recipe for Phase 0. **Not to take:** the 11 ms tick (parity and golden masters need 11.8 ms), the partial `MenuType` enum menus (superseded by the screen stack), exceptions in core (DEC-10). Merging the 2019 branch wholesale is not recommended: clang-format touched every file, so its diffs against the 2014 tree are mostly noise; cherry-pick the ideas and record golden masters on the 11.8 ms tick.

### F.2 `smartties/Blip-Blop-for-Android` — Android port (Martin Jules, November 2014; 14 commits, last 2021)

Based on the 2014 upstream state (it keeps the `Engine/` shim). Was distributed on Google Play; the README now points to a listing on Amazon; the repository declares GPL-3.0.

**What it did (verified):**

- Ant + NDK project; SDL 2.0.2 and SDL2_mixer vendored under `jni/`; game sources in `jni/src`; ABIs armeabi, armeabi-v7a, x86; `minSdkVersion 12`; landscape; FMOD Ex headers present but unused (SDL2_mixer is linked).
- Data: not in the repository — `data/` must be copied into `assets/` and is read from the APK through `AAssetManager` with a custom `SDL_RWops` (`asset.cpp`) and a `Parseur` class replacing `_open/_read` for binary files (text files go through `istringstream`). Every path is normalised at open time (`\` → `/`, control characters stripped). Config and hi-scores are written to `SDL_AndroidGetInternalStoragePath()` (`create_pathSDL`, `log.cpp:23`).
- Touch: SDL finger events (`SDL_FINGERDOWN/MOTION/UP`, `jni/src/input.cpp:278–287`) feed a `Touch_manager` that tracks at most two fingers (an earlier custom JNI bridge, `nativeTouch`, is left commented out in `SDLActivity.java`); `Box_manager` holds normalised hit-boxes (`cv(x) = x / 700`) for menu entries and the in-game buttons (shoot, jump, cow bomb, pause); a virtual stick on the left half of the screen takes its origin from the first touch and maps the angle into eight direction sectors with hand-tuned boundaries (22.5°, 68°, 113°, 158°, 220°, 248°, 293°, 322°) onto the left/right/up/down flags (`Game::updateTouch`). `ControlP1` reads `in.gauche/haut/droit/bas/tirer/sauter` directly, so player 2 is unsupported. Overlay art is PNG decoded with `stb_image` (`data/pad.png`, `stick.png`, `pad0–7.png`). A later pull request fixed "sticky buttons" by removing the shoot/jump cross-activation that fired when a finger slid from one button onto the other (`jni/src/game.cpp`, commit `6347132`).
- Lifecycle: JNI `nativeStop`/`nativeRestart` call `Game::Pause()` (sets `go_to_menu`, stops music) and `Game::Resume()`.
- Audio mixed at 22,050 Hz instead of 44,100 (`FSOUND_Init(22050, …)`), presumably for 2014 phones.
- Rendering unchanged: 640×480 32-bit surface, texture created every frame, `SDL_RenderCopy(…, NULL, NULL)` stretches the 4:3 image to the phone's aspect ratio; an FPS counter is logged every second.

**To take:** a VFS that normalises paths and reads from a packed source at the file-open boundary (our `Vfs`, §3.4); touch input expressed as the same actions the keyboard produces, with the overlay owned by the platform/app layer; the lifecycle → auto-pause hook; 22 kHz mixing as an option for the weakest handhelds. **Not to take:** the stretched presentation (keep aspect-correct scaling), the two-finger limit, the single-player-only control binding.

### F.3 Licence

The Google Code Archive metadata for `code.google.com/p/blip-blop` (`project.json`) records `"license": "mit"`, with the summary "Blip'n Blop video game, now open source!" and the credits list from the game. Neither GitHub repository carries a licence file for the original code (GitHub detects none on `benkaraban/blip-blop`); the Android port's repository is marked GPL-3.0, which covers its own additions. Phase 0 should therefore add an MIT `LICENSE` naming LOADED Studio and the credited authors, and keep GPL-licensed Android code out of this tree unless it is deliberately reused under those terms.
---

## Appendix G — Measurement log (environment, method, raw results)

Kept so the numbers quoted in §2.6, §3.14 and C.8 can be reproduced or challenged.

### G.1 Environment

Intel Core i5-7360U @ 2.30 GHz (2 cores), 16 GB RAM, macOS 14.8.9, Apple clang 16.0.0, CMake 4.4.3, Homebrew `sdl2-compat` 2.32.72 (SDL2 API implemented over SDL3), SDL2_mixer 2.8.2, built-in 2560×1600 Retina display (refresh rate not read from the system; the vsync wait pattern is consistent with 60 Hz). The game was built from this tree, out of source, as **Debug** with `-DPLATFORM_LINUX` and a two-line `malloc.h` shim (§2.6); the hot spots found are inside the optimised SDL library, so the conclusions do not depend on the game's optimisation level, but game-side timings should be re-measured with a Release build.

### G.2 Method

- Profiles: the macOS `sample` tool on the running game (`/quiet` flag, no music): one 6-second window starting ≈1.5 s after launch (initialisation end + intro cutscene), one 8-second window starting ≈18 s after launch (attract mode: hi-score screen). CPU and RSS from `ps -o %cpu,rss` at ≈1.5 s, ≈8 s, ≈18 s and ≈26 s. Keystroke automation was not permitted by the OS, so gameplay itself was not profiled; the gameplay figures in §3.14 are extrapolated from the measured blit costs and the per-frame work read in the code.
- Micro-benchmark: a throwaway C program against the same SDL build, `-O2`. Surfaces created exactly as `Graphics::CreateSurface` does (ARGB8888 with alpha mask, `SDL_SetSurfaceAlphaMod(…, 255)`); the 64×64 sprite is filled with an 8×8 checkerboard of the key colour (50 % transparent) and `SDL_SetColorKey`. 300 iterations for full-screen copies, 20,000 for sprites, 200 for the present path (hidden window, `SDL_RENDERER_ACCELERATED`, no `SDL_RenderPresent`, so no vsync wait). Timing with `SDL_GetPerformanceCounter`.
- Source check: `SDL_surface.c` in the SDL2 branch (line ≈179) and SDL3 main (line ≈182) both contain "By default surface(s) with an alpha mask are set up for blending" followed by `SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND)`.
- Data figures (C.8, A.2, A.4, A.8, C.1): computed by parsing every `.gfx`, `.lft`, `.sfx`, `.lvl` and `uk.dat` extracted from `data.zip` with small scripts (image headers, WAV `fmt` chunks, level headers and event tables).

### G.3 Raw results

Profile, main thread, "top of stack" (samples):

| Window | `SDL_Blit_Slow` | Total main-thread samples | Where the blits came from | Other main-thread time |
|--------|----------------:|--------------------------:|----------------------------|------------------------|
| Intro (6 s) | 1,827 | 3,252 | `CINEPlayer::drawScene → drawSprite → Picture::BlitTo → SDL_UpperBlit → SDL_SoftBlit → SDL_Blit_Slow` | 1,377 in `DDFlip → Graphics::Flip → SDL_RenderPresent → … nextDrawable → semaphore wait` (vsync) |
| Hi-score screen (8 s) | 3,796 | 5,601 | `Game::showHighScores → Picture::PasteTo (opaque full screen) → … → SDL_Blit_Slow` | 1,203 in `DDFlipV → Flip → RenderPresent` (vsync) |

Process-wide, `SDL_FillSurfaceRect4SSE` had 15 samples and `CpuSwizzleBlt` (driver) 32–92; everything else was kernel waits of audio/compositor threads.

CPU and memory (`ps`): ≈1.5 s: 24.6 % of one core, 63.9 MB RSS (initialisation) · ≈8 s: 51.5 %, 64.2 MB (intro) · ≈18 s: 63.9 %, 58.1 MB (hi-score screen) · ≈26 s: 77.4 %, 59.5 MB (attract mode).

Second run, 11 October 2026 (same binary and method, 6 s intro window from ≈1.5 s, 8 s attract window from ≈17 s): `SDL_Blit_Slow` 1,711 of 3,487 main-thread samples (49 %) during the intro, all under `CINEPlayer::drawScene → Picture::BlitTo`; 3,817 of 5,818 (66 %) on the hi-score screen under `Game::showHighScores → Picture::PasteTo`; `SDL_RenderPresent` 1,556 and 1,175 samples respectively. `ps`: 34.8 % / 62.8 MB at 1.5 s, 44.3 % / 67.7 MB at 8 s, 65.8 % / 61.6 MB at 17 s, 56.2 % / 61.9 MB at 28 s; `top` instantaneous 52.6 % in attract mode.

Micro-benchmark (µs per operation; Mpix/s in parentheses):

| Case | Result |
|------|--------|
| 640×480 copy, ARGB8888 + alpha mask, default blend (as the game) | 128.6 µs (2,389 Mpix/s) |
| 640×480 copy, same surface, `SDL_BLENDMODE_NONE` | 48.0 µs (6,401) |
| 640×480 copy, XRGB8888 (no alpha mask) | 52.1 µs (5,901) |
| 640×480 copy, RGB565 | 27.1 µs (11,356) |
| 64×64 colour-keyed sprite, alpha mask, default blend (as the game) | 102.0 µs (40) |
| 64×64 colour-keyed sprite, alpha mask, `BLENDMODE_NONE` | 5.2 µs (785) |
| same + `SDL_SetSurfaceRLE(1)` | 34.5 µs (119) |
| 64×64 colour-keyed sprite, XRGB8888 | 8.3 µs (496) |
| 64×64 colour-keyed sprite, RGB565 | 6.9 µs (591) |
| same + `SDL_SetSurfaceRLE(1)` | 22.5 µs (182) |
| present: `SDL_CreateTextureFromSurface` + `RenderCopy` + destroy, per frame | 681 µs |
| present: streaming ARGB8888 texture, `SDL_UpdateTexture` + `RenderCopy` | 1,761 µs |
| present: streaming RGB565 texture, `SDL_UpdateTexture` + `RenderCopy` | 1,020 µs |

### G.4 Caveats

The profiled binary ran on `sdl2-compat`, not on a native SDL2 — the default blend rule is identical in both (G.2), but blitter speeds differ between libraries and CPUs, so every figure above is a ratio to carry over, not an absolute for any target. The RLE and streaming-texture results in particular contradict common expectations and must be re-measured on each device (Metal on an Intel laptop is not a GLES or framebuffer path on a handheld). The 10–20× slowdown assumed for a Cortex-A7 class CPU in §3.14 is an estimate to be replaced by the first on-device run of `tests/bench`.

