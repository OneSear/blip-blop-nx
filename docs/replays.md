# Deterministic runs, replays and regression tests

The migration is guarded by regression replays (migration.md, sections 4.2
and 6): a recorded or scripted input is played back and the frames it
produces are compared with fingerprints recorded before the change. A replay
is only useful if the game is deterministic, so a run started with
`--record`, `--replay` or `--deterministic` changes three things:

* **Time is virtual.** Every presented frame advances the clock by exactly
  1/60 s, and so does every update of a screen that waits without drawing
  (a screen returning `Frame::Wait`: waiting for a key or for all keys to
  be released, a delay). The legacy timers (`LGetTime`, `bbTickCount`) and
  the `FixedClock`s of the level, the cutscenes and the credits read this
  clock, so the 84.7 Hz simulation runs the same ticks on every machine.
  Without `--headless`, frames are still paced to real time.
* **Input comes from the recording.** The input in effect at a frame is the
  same for every read during that frame. When recording, the live input is
  sampled at the first read of each frame.
* **Randomness is seeded.** `rand()` was replaced by `bbRand()`, a copy of the
  Microsoft C runtime generator the 2002 game was built with (seed 1 by
  default, `--seed` or the `seed` statement to change it).

Deterministic runs also start from the default settings and hi-scores and do
not save them, so saved state cannot change their outcome.

## Input files (`.bbreplay`, text, version 1)

```
bbreplay 1                 # header, mandatory
seed 1                     # random seed (default 1)
option /cheat              # legacy switch applied before the start (repeatable)
@120 RETURN                # from frame 120 on, Return is held
@125                       # from frame 125 on, nothing is held
@300 RIGHT LCTRL MENU      # several inputs at once
@4000 unfocused            # the window has lost the focus (or the game is
                           # suspended): a level opens its pause menu
@4100                      # focus back
@9000 quit                 # the window is closed
end 9600                   # the run stops after 9600 frames
```

The focus state is recorded like a held input, so a recording of a session
where the window lost the focus replays the same pause menu.

Frames count every frame of virtual time (presented or idle). Input names:
`RIGHT LEFT UP DOWN RETURN ESCAPE SPACE TAB BACKSPACE LCTRL RCTRL LALT RALT
LSHIFT RSHIFT MENU F1`…`F12`, letters `A`…`Z`, digits `K0`…`K9`, joysticks
`JOY<slot>_B<button>` and `JOY<slot>_UP/DOWN/LEFT/RIGHT`, any other SDL key
name with `_` for spaces, or a raw legacy input code (`0x4000004F`).

`tools/replays/make_replays.py` generates the scripted replays in
`tests/replays/`; edit the generator, not the generated files.

## Fingerprint files (`.bbhash`, text, version 1)

```
bbhash 1
frame 60 6467bfc3f443d985    # chained hash of presented frames 1..60
tick 60 1a2b...              # chained hash of the world state after ticks 1..60
...
final frames 2000 365b... ticks 2810 cbf2...
```

A frame hash covers the colour that reaches the screen (the back buffer
blended over black, as the presentation does); a tick hash covers the
camera, flags, random state and the position, animation, collision box,
state and health of every entity. Checkpoints are written every 60 frames or
ticks (`--hash-every`), so a mismatch locates the first divergence within one
second of play.

## Regression tests

`ctest` runs every `tests/replays/*.bbreplay` headless and compares the
fingerprints with `tests/replays/expected/<name>.bbhash`. After a deliberate
behaviour change, regenerate the expected files with

```sh
cmake --build --preset default --target update_replay_hashes
```

and commit them in a dedicated, reviewed commit that says why the frames
changed.

The same replays play on a Switch: a Debug build of the `.nro` carries
them in its RomFS, and nxlink passes the arguments
(`nxlink -s build/switch/blipblop.nro -- --replay romfs:/replays/campaign.bbreplay
--hash sdmc:/switch/blipblop/campaign.bbhash`); compare the hashes with
`tests/replays/expected/` on the PC. The same fingerprints show that the
game plays the same on the Switch's processor.

Deterministic runs start from the reference settings (`core::Settings::
reference()`: the defaults with smooth motion off, as the replays were
recorded), so the smooth motion option (Phase 9) is off in every replay but
`menus`, which switches it on.
`--smooth-motion` turns it on from the start; `replay.smurf.smooth` plays
the `smurf` replay so and compares only its `tick` lines: drawing between
ticks must show other frames (the test fails if none differs) and change
nothing of the simulation.

A failing replay test does the first step itself: it dumps the frames of
the build under test, one in six, from the frame checkpoint before the
divergence to the one after, into `build/<preset>/tests/replays/out/<replay>-frames/`
and prints the path. To compare, dump the same frames with a build of the
previous version:

```sh
./blipblop --headless --replay ../../tests/replays/campaign.bbreplay \
           --dump-frames /tmp/frames 3000 3100
```

Headless runs also print a benchmark line, against the desktop budget of
migration.md §3.14 (4 ms of drawing and 2 ms of simulation per frame):

```
bench: 36779 frames, 1.17 ms per frame on average, 155.26 ms at worst, 1085 ticks per second, peak memory 85.6 MB
```

(the `video` replay, Release build, on the development Mac; the worst frame
is a level load). The time is the work between presented frames, the
fingerprints included.

## The replays

`tools/replays/make_replays.py tests/replays` writes them all.

| Replay | What it plays |
|--------|---------------|
| `attract` | The intro and the attract cycle, no input. |
| `smoke` | Menus, the first two levels briefly (pause menu once), quit. |
| `campaign` | All 12 levels played briefly then won with F9, the ending and the credits. |
| `menus` | The menu paths the others do not reach, key redefinition, Blop, dialogue skip, a focus loss. |
| `gameover` | Two players, level summaries, game over, both names, the hi-score table. |
| `smurf` | snuf1 and snuf2 played to the end without F9, including the Big Smurf fight. |
| `dork` | snuf1 and snuf2 won with F9, then the Pokémon bonus level won through its script: the three switches, both lifts, the falling crate and its platforms, the three bombs, then every enemy killed. |
| `lemmings` | The first eight levels won with F9, then the Lemmings bonus level won: fights with cow bombs at each scroll lock, a somersault over the pit, the 200-kill nuke. |
| `snorks` | The first six levels won with F9, then snorkniv and snorkniv2 won with the generic driver. |
| `carebears` | snuf1, snuf2 and dork won with F9, then bisous1, bisous2 and bisous3 won with the generic driver (`explore_cycle`) and a floor sweep against the shaman (`boss_sweep`). |
| `video` | The first nine levels won with F9, then video won with the generic driver; the Yoshi turret on the ledge at x 1417 is shot from under it (the driver walks past it and the next scroll lock waits for it); Rayman with the Pokémon hero summons. |
| `videoboss` | The first ten levels won with F9, then videoboss won: the generic driver, except the Lara fight, where the player climbs to the top ledge and shoots the weight until it falls on her (`lara_weight`); then small Mario, Luigi, Sonic and the Pac-Man car with the generic driver. |
| `finalboss` | The first eleven levels won with F9, then the final boss level won with the generic driver alone (the stork flight, the hyperspace rocks, Mario); ends as the ending starts. |

Scripts that play a level for real (`smurf`, `dork`) can only push keys at
fixed frames, so `dork` was written by measurement: run a draft with
`--trace-ticks`, read the player's position and the props' positions per
frame, adjust the frames. Its comments give the frames where each step
happens (bomb 1 at frame 2746, bomb 2 at 3621, bomb 3 at 4875, the level
won at 6048). A change of the game's behaviour on that level (a deliberate
fix) can make the script fail: check those frames again and re-measure.
