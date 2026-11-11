#pragma once

#include <cstdint>
#include <cstdio>

#include "bb/core/gfx/pixmap.hpp"
#include "bb/game/actor.hpp"
#include "bb/game/decor.hpp"
#include "bb/game/effects.hpp"
#include "bb/game/player.hpp"

namespace bb::game {

// The cheat keys of a tick (F1 to F12 with the cheat switch on).
struct CheatKeys {
    bool enabled = false;
    // Players 1 and 2 only (players 3 and 4 have no cheat keys).
    bool ammo[kMaxPlayers] = {};     // F1, F5
    bool cowBomb[kMaxPlayers] = {};  // F2, F6
    bool life[kMaxPlayers] = {};     // F3, F7
    bool win = false;                // F9
    bool listsDisplay = false;       // F11
    bool fpsDisplay = false;         // F12
};

// How a level is won: the camera at x or beyond, and two flags at their
// values.
struct VictoryCondition {
    int x = 0;
    int flag1 = 0;
    int value1 = 0;
    int flag2 = 0;
    int value2 = 0;
};

// The simulation of the running level (the per-tick part of the 2002
// Game): one tick updates every list of the World in the 2002 order, the
// camera, the locks, the flags, the timer, the target, the GO arrow and the
// collisions. One Level is reused from level to level, like the World; the
// state the 2002 code never reset between levels stays as it was.
class Level {
public:
    explicit Level(World& world);

    // Resets what a level start resets (start of Game::startLevel()).
    void reset();

    // One tick (Game::tickBegin() after the input update).
    void tick(const CheatKeys& cheats);
    // After each presented frame: ends the level when the players are all
    // dead or the victory conditions held long enough.
    void updateVictoryAndDefeat();

    // Draws the level into screen (the World's canvas draws on it too): the
    // decor, the entity lists in the 2002 order, then the distortion and
    // the screen shake (the first part of Game::drawAll()).
    void drawWorld(core::gfx::Pixmap& screen, Decor& decor);
    // What goes over the HUD: the GO arrow and the banner texts.
    void drawOverlays(core::gfx::Pixmap& screen);

    // The world-state fingerprint of deterministic runs; with a trace
    // stream, also prints the state (session::traceStream()).
    std::uint64_t fingerprint(std::FILE* trace, unsigned long long frame) const;

    // The players (null when absent); the campaign owns them.
    Player* players[kMaxPlayers] = {};

    VictoryCondition victory;
    bool bonusLevel = false;   // timed: ends when the timer runs out
    bool won = false;          // niveau_fini
    bool playersDead = false;  // joueurs_morts
    // Ticks the victory conditions held (-1: never yet; 200 ends the level).
    int victoryWait = -1;
    // Ticks without players (200 ends the game).
    int deathWait = 0;

    // The GO arrow, in screen coordinates.
    bool goArrow = false;
    int goArrowX = 0;
    int goArrowFrame = 0;
    // The distortion's phase (Weather::Distortion).
    int distortionPhase = 0;

    bool showFps = false;    // F12
    bool showLists = false;  // F11 (its display is commented out)

private:
    void updateCamera(bool scrollSpeedApplies);  // updateScrolling()
    void updateCheats(const CheatKeys& cheats);
    void updateShake();
    void updateBubbles();
    void updateLock();
    void updateHoldFire();
    void updateTarget();
    void updateFlags();
    void updateGoArrow();
    void removeFinished();  // cleanLists()

    World& world_;
    Precipitation precipitation_;
    Marker standIn_;  // the target when no player is left

    int timerStep_ = 0;
    int cheatWait_ = 0;
    int goDelay_ = 0;
    int goAngle_ = 0;
    int goBounces_ = 0;
    int goSubFrame_ = 0;
    int goLastCameraX_ = 0;
    bool goMustStop_ = false;
    // The shake's amplitude after the last tick's updateShake(), to rumble
    // when a shake starts (no part of the world state).
    int lastShake_ = 0;
};

}  // namespace bb::game
