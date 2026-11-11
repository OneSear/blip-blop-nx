# Release checklist

What CI cannot see, checked by hand before a release is tagged, with the
`.nro` and the `.nsp` that will be released, on a Switch docked and
handheld (switch-port.md, section 8). The automated part comes first:

* CI is green: the `.nro` builds (it is the only thing CI builds).
* Locally, on a Mac or Linux (CI runs no tests): `ctest --preset release`
  (unit and conformance tests, the 14 replays, data and pack) and
  `ctest --preset asan`, or `tools/docker/linux-check.sh` for Linux.
* After the tag: the release has `blipblop-<version>-switch-nro.zip`;
  `tools/switch/make-nsp.sh --upload v<version>`, run at the tag, attaches
  `blipblop-<version>.nsp`.

Write down the console model, its system version, the custom firmware, the
file (`.nro` or `.nsp`) and the result of each line; a line that fails
blocks the release or becomes a known issue in the release notes.

## Install and start

- [ ] The `.nro` (the zip's `switch` folder at the SD card's root) starts
      from the Homebrew Menu in application mode (hold R on a game), and
      from the Album (applet mode, less memory): the keyboard and the
      controller applet work there too.
- [ ] The `.nsp` installs (an NSP installer such as Goldleaf), shows its
      icon and the name "Blip & Blop" on the HOME menu, and starts.
- [ ] The log, `sdmc:/switch/blipblop/BlipBlop.log`, says
      `Game data: romfs:/blipblop.bbpack`.
- [ ] The intro plays, then the attract mode and the main menu.

## Display

- [ ] Handheld: the log's `Display:` line says `picture 960x720 at 160,0`;
      the picture fills the screen's height between black bars.
- [ ] Docked on a 1080p TV: `picture 1440x1080 at 240,0`; with SCALING =
      INTEGER, `picture 1280x960 at 320,60`.
- [ ] Docking and undocking during a level and in a menu: the picture
      follows at once (a `Display:` line each time), the game goes on.
- [ ] FILTER = CRISP: square, sharp pixels that do not shimmer when the
      level scrolls; SHARP shows uneven pixels, SMOOTH blurs; the choice is
      back after a restart.
- [ ] SMOOTH MOTION = ON (the default): scrolling and walking look even;
      the game plays the same.
- [ ] WIDESCREEN = ON (the default): the levels fill the 16:9 screen
      (handheld and docked), the HUD's right corners at the right edge,
      the dialogues across, the pause menu and the level summary in the
      middle; no enemy appears out of nothing on the right; each level
      can be finished. The menus, the hero choice (across), the mission
      sheets (the story across), the cutscenes and the credits keep the
      levels' size: no jump from a menu to a level. OFF: from the next
      level, 4:3 with bars, as in 2002.
- [ ] LANGUAGE = FRANCAIS: the menus, the controls, the mission sheets,
      the dialogues, the banners, the level summary and the credits are in
      French; after a restart the game starts in French. A console set to
      French starts a first run in French.

## Controls

- [ ] In a level, with the console's Joy-Cons, a Pro Controller, a Joy-Con
      pair and a single Joy-Con held sideways: A (right button) and ZR (SR)
      fire, B (bottom) jumps, Y (left) and ZL (SL) drop the cow bomb, the
      D-pad or the stick moves, + or − pauses.
- [ ] In the menus: A confirms, B goes back one page and resumes from the
      pause menu, + or − pause and resume and skip a cutscene.
- [ ] The hero choice: + or − joins on each controller (and A); the stick
      and the D-pad move the tag, A takes a hero, B gives it back and
      leaves, B before anyone joined goes back to the main menu; a hero
      taken cannot be taken twice; the game starts once everyone who
      joined is READY, with each player's hero.
- [ ] X on the hero choice opens the controller applet; one controller
      closes it; four single Joy-Cons then join and play a four-player
      game, each controller's lights showing its number. Note whether SDL
      opens a second applet after it (it does when a controller's style
      changes outside handheld mode).
- [ ] The controls menu shows the Switch's names (`P1 A`, `P2 ZR`), and a
      binding survives a restart.
- [ ] How often a single Joy-Con's stick click pauses the game by
      accident (SDL makes it Start).

## The campaign

The replays win the levels with the F9 cheat. The definition of done
(migration.md §4.5) asks for the campaign completed end to end:

- [ ] A game, without cheats, is played through the 12 levels to the ending
      and the credits (one or two players; continues allowed).
- [ ] The replays of a Debug `.nro` on the console give the expected
      fingerprints (docs/replays.md); their benchmark line keeps a frame
      well under 16.7 ms.
- [ ] An hour of play: no slowdown, the memory in use (the benchmark line)
      steady, the console not hotter than with other games.

## Focus and quitting

- [ ] HOME during a level, then back: the pause menu is open.
- [ ] Sleep during a level, then wake: the pause menu is open; the sound
      comes back.
- [ ] Closing the game from the HOME menu saves the settings and the
      hi-scores, like EXIT in the main menu.

## User files

In `sdmc:/switch/blipblop/`, shared by the `.nro` and the `.nsp`:

- [ ] After a first run: `settings.cfg`, `hiscores.scr` and `BlipBlop.log`
      are there.
- [ ] A name typed on the console's keyboard at the end of a game is in the
      hi-score table after a restart, even when the console was switched
      off without leaving the game.
- [ ] Settings changed in the options are back after a restart, even when
      the console was switched off without leaving the game.
- [ ] Deleting the folder gives the default settings and hi-scores on the
      next start.
- [ ] A newer `.nsp` installs over the older one and keeps them; deleting
      the game (System Settings > Data Management) leaves them on the card.
