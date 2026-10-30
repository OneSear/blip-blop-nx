#!/usr/bin/env python3
"""Generates the scripted input files used as regression replays.

The scripts do not depend on exact timings: they repeat a short input cycle
(move and fire, jump, release everything, confirm, cheat keys) that makes
progress through every screen the game can be waiting on - menus, briefing
prompts, dialogues (which wait for all keys to be released), the pause menu,
name entry - and press F9 (win level, needs /cheat) at fixed intervals.

Jump is bound to the Menu key by the legacy defaults and to Left Alt once
D-37 is fixed; the scripts press both so they work before and after.

Usage: make_replays.py <output-directory>
"""

import os
import sys


class Script:
    def __init__(self, options=()):
        self.lines = ["bbreplay 1", "seed 1"] + [f"option {o}" for o in options]
        self.frame = 0
        self.held = None

    def hold(self, frames, *keys):
        if frames <= 0:
            raise ValueError(f"hold of {frames} frames at frame {self.frame}")
        state = " ".join(sorted(keys))
        if state != self.held:
            self.lines.append(f"@{self.frame} {state}".rstrip())
            self.held = state
        self.frame += frames

    def end(self, extra=0):
        self.frame += extra
        self.lines.append(f"end {self.frame}")
        return "\n".join(self.lines) + "\n"


def menu_to_first_level(s, second_player=False):
    s.hold(30)
    s.hold(10, "ESCAPE")          # skip the intro cutscene
    s.hold(60)
    s.hold(10, "RETURN")          # main menu: START
    s.hold(40)
    if second_player:
        s.hold(10, "DOWN")        # start menu: 2 PLAYERS
        s.hold(20)
    s.hold(10, "RETURN")
    s.hold(140)                   # character select: wait for Blip to slide in
    s.hold(10, "RETURN")          # pick Blip
    s.hold(60)


def play_cycle(s, cheats=True, pause_menu=False):
    s.hold(30, "RIGHT", "LCTRL")
    s.hold(8, "RIGHT", "LCTRL", "MENU", "LALT")  # jump while running
    s.hold(40, "RIGHT", "LCTRL")
    s.hold(12, "UP", "LCTRL")     # aim up
    s.hold(10, "LEFT", "LCTRL")
    s.hold(6)                     # release all: lets dialogues finish
    s.hold(4, "RETURN")           # briefing prompts, summaries, name entry
    s.hold(6)
    if cheats:
        s.hold(3, "F1", "F3")     # ammo and an extra life
        s.hold(3)
    if pause_menu:
        s.hold(4, "ESCAPE")       # pause menu...
        s.hold(20)
        s.hold(4, "RETURN")       # ...first entry resumes the game
        s.hold(6)


def campaign(frames_per_level=1500, levels=12, ending=True):
    s = Script(options=("/cheat", "/quiet"))
    menu_to_first_level(s)
    for level in range(levels):
        start = s.frame
        cycle = 0
        while s.frame - start < frames_per_level:
            play_cycle(s, pause_menu=(level == 0 and cycle == 3))
            cycle += 1
        # F9 is ignored for 20 ticks after F3 (Game::updateCheat cool-down).
        s.hold(30)
        s.hold(4, "F9")           # win the level
        s.hold(10)
        # Level summary, briefing, "press a key", cutscenes: keep confirming.
        for _ in range(12):
            s.hold(4, "RETURN")
            s.hold(30)
    if ending:
        # Ending: cutscene, "the end", credits, name entry, hi-scores.
        for _ in range(60):
            s.hold(4, "RETURN")
            s.hold(60)
    return s.end(600)


def smoke():
    """Menus, the first two levels with the pause menu, then quit."""
    return campaign(frames_per_level=1200, levels=2, ending=False)


def tap(s, *keys, frames=6, gap=6):
    """Presses keys briefly, then releases everything (menus act on a press
    and wait until every key is released)."""
    s.hold(frames, *keys)
    s.hold(gap)


def walk_right(s, until):
    """Walks right without firing until the given frame, releasing every key
    regularly so that dialogues can finish."""
    while s.frame < until:
        s.hold(60, "RIGHT", "F")      # player 1 and player 2 (F = P2 right)
        s.hold(10)


def menus():
    """The menus the other replays do not reach: main menu wrap-around, exit
    confirmation, options, key redefinition, Blop selection, pause menu
    options and exit, the pause menu opened by a focus loss, dialogue skip,
    the attract cycle interrupted by keys, and a normal quit. The Phase 9
    options are all switched on, so everything from the key pages on is in
    French and the level is drawn with smooth motion."""
    s = Script()
    s.hold(30)
    tap(s, "ESCAPE", frames=10)       # skip the intro
    s.hold(60)
    # Main menu (START, OPTIONS, EXIT): up wraps to EXIT; cancel the exit.
    tap(s, "UP")
    tap(s, "RETURN")                  # exit confirmation (YES, NO)
    tap(s, "DOWN")
    tap(s, "RETURN")                  # NO: back to the main menu on EXIT
    tap(s, "DOWN")                    # wraps to START
    tap(s, "DOWN")                    # OPTIONS
    tap(s, "RETURN")                  # options (SCALING, FILTER, SMOOTH MOTION,
                                      # LANGUAGE, P1 to P4 KEYS, RETURN)
    tap(s, "DOWN")                    # wraps to SCALING
    tap(s, "RIGHT")
    tap(s, "LEFT")
    tap(s, "RETURN")                  # integer scaling: switched three times, on
                                      # (headless: not shown)
    s.hold(24)                        # the two taps of the VSYNC item, gone with
                                      # the desktop versions: the frames after
                                      # the options page are the same as before
    tap(s, "DOWN")
    tap(s, "LEFT")                    # crisp to smooth filter (headless: not shown)
    tap(s, "DOWN")
    tap(s, "RETURN")                  # smooth motion on
    tap(s, "DOWN")
    tap(s, "RIGHT")                   # French
    tap(s, "DOWN")
    tap(s, "RETURN")                  # player 1 keys, focus on RETURN
    tap(s, "UP")                      # special
    tap(s, "RETURN")                  # redefine: waits for a key
    s.hold(20)
    tap(s, "B", frames=8)             # player 1 special = B
    tap(s, "DOWN")
    tap(s, "RETURN")                  # back to the options
    tap(s, "DOWN")
    tap(s, "RETURN")                  # player 2 keys
    for _ in range(6):
        tap(s, "UP")                  # down
    tap(s, "RETURN")
    s.hold(20)
    tap(s, "K", frames=8)             # player 2 down = K
    for _ in range(6):
        tap(s, "DOWN")
    tap(s, "RETURN")                  # back to the options
    for _ in range(3):
        tap(s, "DOWN")
    tap(s, "RETURN")                  # RETURN: main menu on OPTIONS
    tap(s, "UP")
    tap(s, "RETURN")                  # START (1 to 4 PLAYERS, RETURN)
    for _ in range(4):
        tap(s, "DOWN")
    tap(s, "RETURN")                  # back to the main menu
    tap(s, "RETURN")                  # START
    tap(s, "RETURN")                  # 1 PLAYER: character select
    s.hold(60)                        # Blip slides in
    tap(s, "RIGHT")                   # Blip leaves, Blop slides in
    s.hold(90)
    tap(s, "LEFT")                    # back to Blip
    s.hold(90)
    tap(s, "RIGHT")
    s.hold(90)
    tap(s, "RETURN", frames=10)       # Blop
    s.hold(260)                       # loading and the 2-second wait
    tap(s, "RETURN", frames=10)       # briefing: start
    level = s.frame
    # First level as Blop: fire and walk until the first dialogue, which
    # Escape skips; then the pause menu.
    while s.frame < level + 420:      # the first dialogue starts 420 frames in
        s.hold(40, "RIGHT", "LCTRL")
        s.hold(6)
    s.hold(20)
    tap(s, "ESCAPE", frames=10, gap=30)
    s.hold(200)
    tap(s, "ESCAPE", frames=10)       # pause menu (RESUME, OPTIONS, EXIT)
    tap(s, "DOWN")
    tap(s, "RETURN")                  # options, focus on RETURN
    for _ in range(4):
        tap(s, "UP")
    tap(s, "RETURN")                  # player 1 keys
    tap(s, "RETURN")                  # RETURN: back to the options
    tap(s, "RETURN")                  # RETURN: back to the pause menu
    tap(s, "ESCAPE")                  # Escape resumes
    s.hold(120, "RIGHT", "LCTRL")
    s.hold(60, "unfocused")           # the window loses the focus: pause menu
    s.hold(30)                        # focus back, the menu stays
    tap(s, "RETURN")                  # RESUME
    s.hold(10)
    tap(s, "ESCAPE", frames=10)       # pause menu again
    tap(s, "DOWN")
    tap(s, "DOWN")
    tap(s, "RETURN")                  # EXIT: back to the main menu
    # Attract cycle, interrupted by a key on each screen.
    s.hold(660)                       # 10 s without input: title screen
    tap(s, "SPACE")
    s.hold(660)                       # hi-score table
    tap(s, "SPACE")
    s.hold(660)                       # title screen
    s.hold(320)                       # intro cutscene
    tap(s, "ESCAPE")
    s.hold(660)                       # title screen
    s.hold(400)                       # credits
    tap(s, "SPACE")
    s.hold(60)
    # Quit: EXIT, YES.
    tap(s, "UP")
    tap(s, "RETURN")
    tap(s, "RETURN")
    return s.end(300)


def four_players():
    """A four-player game: Blip and Blop on the keyboard, Bleach and Blitz on
    pads 3 and 4 (the Switch's layout: d-pad, A - SDL's B1, the right
    button - fires, B - B0, the bottom one - jumps). All four
    run, jump and fire through the first level and its dialogue; F9 wins it
    and the second one (whose level summary is the table of four); then part
    of the third level, the pause menu's EXIT and a quit."""
    s = Script(options=("/cheat", "/quiet"))
    s.hold(30)
    s.hold(10, "ESCAPE")              # skip the intro cutscene
    s.hold(60)
    s.hold(10, "RETURN")              # main menu: START
    s.hold(40)
    for _ in range(3):
        tap(s, "DOWN")                # start menu: 4 PLAYERS
    s.hold(10, "RETURN")
    s.hold(140)                       # character select: Blip slides in
    s.hold(10, "RETURN")              # player 1 is Blip
    s.hold(60)
    run = ("RIGHT", "LCTRL", "F", "TAB", "JOY3_RIGHT", "JOY3_B1", "JOY4_RIGHT", "JOY4_B1")
    jump = ("LALT", "G", "JOY3_B0", "JOY4_B0")
    back = ("LEFT", "D", "JOY3_LEFT", "JOY4_LEFT")

    def cycle():
        s.hold(15, *run[:4])          # 1 and 2 ahead: the four spread out
        s.hold(30, *run)
        s.hold(8, *run, *jump)        # all jump while running
        s.hold(40, *run)
        s.hold(10, *back)
        s.hold(6)                     # release all: lets dialogues finish
        s.hold(4, "RETURN")           # briefing prompts, summaries
        s.hold(6)
        s.hold(3, "F1", "F3")         # ammo and a life for player 1
        s.hold(3)

    for level, frames in enumerate((2000, 900, 600)):
        start = s.frame
        while s.frame - start < frames:
            cycle()
        if level < 2:
            s.hold(30)
            s.hold(4, "F9")           # win the level
            s.hold(10)
            for _ in range(12):
                s.hold(4, "RETURN")
                s.hold(30)
    tap(s, "ESCAPE", frames=10)       # pause menu (RESUME, OPTIONS, EXIT)
    tap(s, "DOWN")
    tap(s, "DOWN")
    tap(s, "RETURN")                  # EXIT: back to the main menu
    s.hold(60)
    tap(s, "UP")                      # EXIT, YES: quit
    tap(s, "RETURN")
    tap(s, "RETURN")
    return s.end(300)


def gameover():
    """Two players: a level summary with the objective bonus, a bonus level
    lost on time, game over, both names entered (letters, space, backspace),
    the hi-score table, then the attract title screen."""
    s = Script(options=("/cheat", "/quiet"))
    menu_to_first_level(s, second_player=True)
    for _ in range(2):                # snuf1 and snuf2, won with F9
        s.hold(200)
        tap(s, "RETURN", frames=10, gap=200)   # briefing
        s.hold(30)
        tap(s, "F9", frames=4, gap=60)
        for _ in range(6):            # level summary
            tap(s, "RETURN", frames=4, gap=40)
    s.hold(200)
    tap(s, "RETURN", frames=10, gap=60)        # briefing of the bonus level
    s.hold(2700)                      # the players die: bonus failed
    for _ in range(6):                # level summary, briefing
        tap(s, "RETURN", frames=4, gap=60)
    # Both players die without firing, about 6800 frames after the walk
    # starts; stop walking before that (any key held skips the game over
    # screen) and wait long enough for the death to happen.
    walk_right(s, s.frame + 6500)
    s.hold(800)                       # death, then the game over screen
    tap(s, "RETURN")
    s.hold(80)                        # player 1 name entry
    for key in ("B", "L", "I", "X", "BACKSPACE", "P", "SPACE", "K1"):
        tap(s, key)
    tap(s, "RETURN")
    s.hold(80)                        # player 2 name entry
    for key in ("B", "L", "O", "P"):
        tap(s, key)
    tap(s, "RETURN")
    s.hold(900)                       # hi-score table times out
    return s.end(1200)


def smurf():
    """The Smurf world played to the end without the win-level cheat:
    snuf1 complete at about frame 6900; in snuf2 the five strongmen, then
    the Big Smurf (enters at about 15600, dies at about 22300), the stork,
    the level complete at about 22900; ends in the level summary. Extra
    lives and ammunition (F1, F3) keep the player alive."""
    s = Script(options=("/cheat", "/quiet"))
    menu_to_first_level(s)
    while s.frame < 22820:
        play_cycle(s)
    return s.end(200)                 # in the snuf2 level summary


JUMP = ("MENU", "LALT")
FIRE = "LCTRL"


def until(s, frame, *keys):
    """Holds keys until the given absolute frame."""
    s.hold(frame - s.frame, *keys)


def cycle_until(s, frame, steps):
    """Repeats (frames, keys) steps, stopping exactly at the given frame."""
    while s.frame < frame:
        for n, keys in steps:
            n = min(n, frame - s.frame)
            if n > 0:
                s.hold(n, *keys)


def wait_fire(s, frame):
    """Stands firing, turning left and right, with an extra life (F3) now
    and then."""
    cycle_until(s, frame, [(20, (FIRE,)), (4, ("LEFT", FIRE)), (20, (FIRE,)),
                           (4, ("RIGHT", FIRE)), (3, ("F3", FIRE))])


def somersault(s, *keys):
    """A jump, then jump again in the air (the somersault rises about
    166 pixels instead of 105)."""
    s.hold(18, FIRE, *keys, *JUMP)
    s.hold(3, FIRE, *keys)
    s.hold(30, FIRE, *keys, *JUMP)


def dork():
    """The Pokemon bonus level played to its end without the win-level
    cheat (snuf1 and snuf2 are won with F9 first). The level is a script:
    each switch starts a lift or the falling crate, standing next to a bomb
    defuses it (flag 0), and once the three bombs are defused the level is
    won when no enemy is left. The inputs follow positions measured with
    --trace-ticks, so they rely on exact timings: re-measure them if the
    game's behaviour changes. Ends while bisous1 waits for a key."""
    s = Script(options=("/cheat", "/quiet"))
    menu_to_first_level(s)
    for _ in range(2):                    # snuf1, snuf2: won with F9
        for _ in range(3):
            play_cycle(s)
        s.hold(30)
        s.hold(4, "F9")
        s.hold(10)
        for _ in range(12):
            s.hold(4, "RETURN")
            s.hold(30)
    # dork starts at frame 1938. Switch 1 (jump at x 485), then lift 1.
    s.hold(120)
    s.hold(150, "RIGHT")
    s.hold(20)
    s.hold(8, *JUMP)                      # short jump into switch 1
    s.hold(40)
    s.hold(60, "LEFT")                    # under lift 1
    until(s, 2504)
    s.hold(8, *JUMP)                      # onto lift 1 at its lowest
    until(s, 2702)                        # ride up
    s.hold(80, "RIGHT")                   # bomb 1 (frame 2746)
    # Up the ledges 362, 296 and 226 into switch 2.
    until(s, 2940, "RIGHT", FIRE)
    for _ in range(3):
        s.hold(35, "RIGHT", FIRE, *JUMP)
        s.hold(6, "RIGHT", FIRE)
    s.hold(4, "LEFT", FIRE)               # stay on ledge 226
    wait_fire(s, 3425)
    until(s, 3453, "RIGHT", FIRE)         # to the end of the ledge
    s.hold(35, "RIGHT", FIRE, *JUMP)      # onto lift 2, rising
    s.hold(12, "RIGHT", FIRE)
    until(s, 3560, FIRE)                  # ride up
    s.hold(70, "RIGHT", FIRE)             # bomb 2 (frame 3621)
    until(s, 3912, "RIGHT", FIRE)         # down, right to switch 3
    somersault(s)                         # switch 3: the crate falls
    wait_fire(s, 4210)                    # until the crate has landed
    s.hold(60, "LEFT", FIRE)
    somersault(s)                         # onto the crate's step (dies there
    wait_fire(s, 4500)                    # and comes back on it)
    s.hold(40, "LEFT", FIRE)
    somersault(s, "LEFT")                 # onto a crate platform
    wait_fire(s, 4675)
    until(s, 4715, "RIGHT", FIRE)
    s.hold(35, "RIGHT", FIRE, *JUMP)      # onto the swinging platform
    s.hold(25, "RIGHT", FIRE)
    until(s, 4815, FIRE)                  # ride up
    s.hold(80, "RIGHT", FIRE)             # bomb 3 (frame 4875)
    s.hold(40, "LEFT", FIRE)              # down: kill every enemy left
    cycle_until(s, 6060, [(60, ("LEFT", FIRE)), (16, ("UP", "LEFT", FIRE)),
                          (16, ("UP", FIRE)), (120, ("RIGHT", FIRE)),
                          (16, ("UP", "RIGHT", FIRE)), (16, ("UP", FIRE)),
                          (35, ("RIGHT", FIRE) + JUMP), (60, ("LEFT", FIRE)),
                          (35, ("LEFT", FIRE) + JUMP), (3, ("F3", FIRE)), (6, ())])
    s.hold(30)                            # won at frame 6048
    for _ in range(12):                   # level summary, briefing
        s.hold(4, "RETURN")
        s.hold(40)
    return s.end(200)


def skip_levels(s, count):
    """From the main menu, wins the first `count` levels with F9."""
    menu_to_first_level(s)
    for _ in range(count):
        for _ in range(3):
            play_cycle(s)
        s.hold(30)
        s.hold(4, "F9")
        s.hold(10)
        for _ in range(12):
            s.hold(4, "RETURN")
            s.hold(30)


def explore_cycle(s, n):
    """One step of the generic level driver used since Phase 5c: forward
    with fire, a short jump, aim up, diagonally up and down, drop through
    the platform (down twice quickly), confirm dialogues and summaries, an
    extra life; every fourth step a sweep back with full jumps (to reach
    enemies left on ledges)."""
    s.hold(40, "RIGHT", FIRE)
    s.hold(8, "RIGHT", FIRE, *JUMP)
    s.hold(30, "RIGHT", FIRE)
    s.hold(12, "UP", FIRE)
    s.hold(12, "UP", "RIGHT", FIRE)
    s.hold(12, "UP", "LEFT", FIRE)
    s.hold(12, "DOWN", FIRE)
    s.hold(3, "DOWN")
    s.hold(3)
    s.hold(3, "DOWN")
    s.hold(6)
    s.hold(4, "RETURN")
    s.hold(6)
    s.hold(3, "F1", "F3")
    s.hold(3)
    if n % 4 == 3:
        s.hold(70, "LEFT", FIRE)
        s.hold(35, "LEFT", FIRE, *JUMP)
        s.hold(12, "LEFT", "DOWN", FIRE)
        s.hold(35, "RIGHT", FIRE, *JUMP)
        s.hold(6)


def boss_sweep(s, until_frame):
    """Walks a locked arena's floor in short steps, firing straight up and
    diagonally at each stop, left to right then back."""
    while s.frame < until_frame:
        for direction in ("RIGHT", "LEFT"):
            for _ in range(12):
                s.hold(14, direction, FIRE)
                s.hold(20, "UP", FIRE)
                s.hold(8, "UP", direction, FIRE)
            s.hold(3, "F1", "F3")
            s.hold(4, "RETURN")
            s.hold(6)


def carebears():
    """The Care Bear world played to the end without the win-level cheat
    (snuf1, snuf2 and dork are won with F9 first): bisous1 complete at
    about frame 11800, bisous2 at 16900; in bisous3 the arena is reached at
    about 25500 and the shaman (11000 HP, teleports) is fought with a floor
    sweep firing up; the level is complete at frame 50158. Ends in the
    bisous3 level summary. (Timings measured on 24 October 2026, after
    the D-54 and D-76..D-94 fixes; before them bisous3 ended at 51051.)"""
    s = Script(options=("/cheat", "/quiet"))
    skip_levels(s, 3)
    n = 0
    while s.frame < 25600:
        explore_cycle(s, n)
        n += 1
    boss_sweep(s, CAREBEARS_END)
    return s.end(200)


CAREBEARS_END = 50200


def explore_levels(skipped, until_frame):
    """Wins the first `skipped` levels with F9, then plays on with the
    generic driver until the given frame."""
    s = Script(options=("/cheat", "/quiet"))
    skip_levels(s, skipped)
    n = 0
    while s.frame < until_frame:
        explore_cycle(s, n)
        n += 1
    return s


def snorks():
    """The Snork world played to the end without the win-level cheat (the
    first six levels are won with F9): snorkniv complete at about frame
    25300, snorkniv2 (its boss from about 33000, dialogue 1 when he falls
    at 37871) at frame 38311. Ends in the snorkniv2 level summary. (Before
    the fixes of 24 October 2026: 19300, 28000 and 35800.)"""
    return explore_levels(6, SNORKS_END).end(200)


SNORKS_END = 38200


def video():
    """The video-game world's first level (video) played to the end
    without the win-level cheat (the first nine levels are won with F9):
    the generic driver, except that the Yoshi turret on the ledge at x 1417
    is shot from under it at about frame 9640 (the driver walks past it,
    and the scroll lock at x 1536 waits for it); the end arena (camera
    10388) is reached, then Rayman (15000 HP, with the Pokemon hero
    summons) is fought; the level is complete at frame 36722 (38316 before
    the fixes of 24 October 2026). Ends in the level summary."""
    s = Script(options=("/cheat", "/quiet"))
    skip_levels(s, 9)
    n = 0
    while s.frame < 9618:
        explore_cycle(s, n)
        n += 1
    s.hold(17, "RIGHT", FIRE)
    cycle_until(s, s.frame + 600, [(40, ("UP", FIRE)), (3, ("F1", "F3", "UP", FIRE))])
    while s.frame < VIDEO_END:
        explore_cycle(s, n)
        n += 1
    return s.end(100)


VIDEO_END = 36650


def lara_weight(s):
    """videoboss, from the start of the Lara fight (register 0 = 4): Lara
    dies only when the weight hanging at x 565 (hitbox y -79 to 101) is
    pushed past x 950, 3 pixels per hit by shots going right. From the
    left edge of the locked screen (camera 417), three somersaults climb
    the ledges at y 322, 218 and 105 (x 383 to 479); from the top one,
    facing right, the shots meet the weight."""
    until(s, s.frame + 200, "LEFT")
    s.hold(8, "RIGHT")
    s.hold(10)
    for _ in range(3):
        s.hold(18, *JUMP)
        s.hold(3)
        s.hold(30, *JUMP)
        s.hold(40)
    s.hold(1, "RIGHT")
    s.hold(5)


def videoboss():
    """The video-game boss level played to the end without the win-level
    cheat (the first ten levels are won with F9): the generic driver until
    the Lara fight starts (frame 9344), then `lara_weight` and fire until
    the weight falls (Lara dies at about frame 10540); then the generic
    driver again through small Mario, Luigi, Sonic and the Pac-Man car;
    the level is complete at frame 24141 (23663 before the fixes of 24
    October 2026). Ends in the level summary."""
    s = Script(options=("/cheat", "/quiet"))
    skip_levels(s, 10)
    n = 0
    while s.frame < 9344:
        explore_cycle(s, n)
        n += 1
    lara_weight(s)
    cycle_until(s, 10600, [(60, (FIRE,)), (3, ("F1", "F3", FIRE))])
    while s.frame < VIDEOBOSS_END:
        explore_cycle(s, n)
        n += 1
    s.hold(250)
    return s.end()


VIDEOBOSS_END = 24000


def finalboss():
    """The final boss level played to the end without the win-level cheat
    (the first eleven levels are won with F9): the generic driver alone.
    After the stork flight, Mario (50,000 HP, attacks chosen by his HP)
    falls at about frame 32,000; the level is complete at frame 32,086.
    Ends as the ending starts."""
    return explore_levels(11, FINALBOSS_END).end(60)


FINALBOSS_END = 32000


def stand_and_bomb(s, until_frame):
    """Stands firing both ways, with a cow bomb (F2, then Space) and extra
    lives now and then (the Lemmings level needs 200 kills in 100 s)."""
    while s.frame < until_frame:
        for _ in range(3):
            s.hold(20, FIRE)
            s.hold(4, "LEFT", FIRE)
            s.hold(20, FIRE)
            s.hold(4, "RIGHT", FIRE)
        s.hold(3, "F2", FIRE)
        s.hold(30, FIRE)
        s.hold(4, "SPACE")
        s.hold(10, FIRE)
        s.hold(3, "F1", "F3")
        s.hold(6)


def lemmings():
    """The Lemmings bonus level won (the first eight levels are won with F9;
    lem starts at frame 6974). The lemmings come from generators that wake
    with the camera, and the level is won after 200 kills (they all blow
    up, then dialogue 1): 40 kills at the start (scroll lock), walk to the
    second lock and 100 kills there (frame ~9250), then a somersault over
    the pit (x 1443-1568, frame 9441) and the last kills beyond it; won at
    frame 10917. Ends in the level summary."""
    s = Script(options=("/cheat", "/quiet"))
    skip_levels(s, 8)
    until(s, 6974)
    stand_and_bomb(s, 7800)
    s.hold(243, "RIGHT", FIRE)            # x 90 -> 760: the second lock
    stand_and_bomb(s, 9200)
    until(s, 9441, "RIGHT", FIRE)         # to x ~1415
    somersault(s, "RIGHT")                # over the pit
    s.hold(30, "RIGHT", FIRE)
    stand_and_bomb(s, LEMMINGS_END)
    return s.end(150)


LEMMINGS_END = 10930


def attract(frames=6000):
    s = Script()
    s.hold(1)
    return s.end(frames - 1)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(out, exist_ok=True)
    files = {
        "attract.bbreplay": attract(),
        "campaign.bbreplay": campaign(),
        "smoke.bbreplay": smoke(),
        "menus.bbreplay": menus(),
        "gameover.bbreplay": gameover(),
        "fourplayers.bbreplay": four_players(),
        "smurf.bbreplay": smurf(),
        "dork.bbreplay": dork(),
        "carebears.bbreplay": carebears(),
        "snorks.bbreplay": snorks(),
        "lemmings.bbreplay": lemmings(),
        "video.bbreplay": video(),
        "videoboss.bbreplay": videoboss(),
        "finalboss.bbreplay": finalboss(),
    }
    for name, text in files.items():
        with open(os.path.join(out, name), "w", newline="\n") as f:
            f.write("# Generated by tools/replays/make_replays.py - do not edit by hand.\n")
            f.write(text)
        print(f"wrote {name}: {text.count(chr(10))} lines")


if __name__ == "__main__":
    main()
