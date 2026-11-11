// The running level (the per-tick part of game.cpp and scroll.cpp:
// tickBegin(), updateScrolling(), updateLock(), updateHoldFire(),
// updateFlags(), updateTeteTurc(), updateVictoryAndDefeat(),
// updateCheat(), updateTremblements(), updateBulles(), updateFlecheGo(),
// updateDeformation(), cleanLists(), worldStateHash()).

#include "bb/game/level.hpp"

#include <typeinfo>

#include "bb/assets/sprites/misc.hpp"
#include "bb/core/gfx/sprite.hpp"
#include "bb/core/hash.hpp"
#include "bb/game/trig.hpp"

namespace bb::game {

namespace {

constexpr int kEndDelay = 200;             // ticks between victory or defeat and the end
constexpr int kTimerTicks = 80;            // ticks per second of the level timer
constexpr Flag kEndNowFlag = Flag::User1;  // the level ends at once at...
constexpr int kEndNowValue = 999;
constexpr int kGoDelay = 300;    // ticks without scrolling before the GO arrow
constexpr int kMaxBubbles = 15;  // enemies give off bubbles below this count
constexpr int kCheatDelay = 20;  // ticks between two one-shot cheats

void drawAll(EntityList<Entity>& list) {
    for (list.rewind(); !list.atEnd(); list.next()) {
        list.current()->draw();
    }
}

template <class T>
void updateAll(EntityList<T>& list) {
    for (list.rewind(); !list.atEnd(); list.next()) {
        list.current()->update();
    }
}

bool finished(const Entity* e) {
    return e->dead;
}
bool finished(Generator* g) {
    return g->finished();
}
bool finished(const Banner* b) {
    return b->finished();
}

template <class T>
void dropFinished(EntityList<T>& list) {
    list.rewind();
    while (!list.atEnd()) {
        if (finished(list.current())) {
            list.removeCurrent();
        } else {
            list.next();
        }
    }
}

void hashList(core::Fnv1a64& h, const EntityList<Entity>& list, bool characters, const char* name,
              std::FILE* trace) {
    h.add(list.count());

    if (trace != nullptr && list.count() > 0) {
        std::fprintf(trace, "  %s (%d)\n", name, list.count());
    }

    list.forEach([&](const Entity* e) {
        const EntityState s = e->state();

        if (trace != nullptr) {
            std::fprintf(trace,
                         "    %-28s x=%d y=%d dy=%d etape=%d ss=%d dir=%d del=%d col=%d box=%d,%d,%d,%d",
                         typeid(*e).name(), e->x, e->y, s.dy, s.frame, s.subFrame, s.direction,
                         static_cast<int>(e->dead), static_cast<int>(e->hitbox.active), e->hitbox.x1,
                         e->hitbox.y1, e->hitbox.x2, e->hitbox.y2);
            if (characters) {
                std::fprintf(trace, " pv=%d etat=%d", s.hitPoints, s.state);
            }
            std::fprintf(trace, "\n");
        }

        h.add(e->x);
        h.add(e->y);
        h.add(s.dy);
        h.add(s.frame);
        h.add(s.subFrame);
        h.add(s.direction);
        h.add(static_cast<int>(e->dead));
        h.add(static_cast<int>(e->hitbox.active));
        h.add(e->hitbox.x1);
        h.add(e->hitbox.y1);
        h.add(e->hitbox.x2);
        h.add(e->hitbox.y2);

        if (characters) {
            h.add(s.hitPoints);
            h.add(s.state);
        }
    });
}

}  // namespace

Level::Level(World& world) : world_(world), precipitation_(world) {}

void Level::reset() {
    World& w = world_;

    playersDead = false;
    won = false;
    victoryWait = -1;
    // Per level (D-94 fixed: 2002 reset it only when a campaign started, so
    // after a bonus level lost by both players the next death ended the
    // game at once).
    deathWait = 0;
    w.clearShared();
    timerStep_ = 0;
    goAngle_ = 0;
    goArrow = false;
    goDelay_ = 0;
    lastShake_ = 0;

    w.scrollLock.on = false;
    w.camera.speed = 0;
    for (bool& stopped : w.camera.stopped) {
        stopped = false;
    }
    w.holdFire.on = false;
    w.weather.intensity = 0;
    w.shake.dy = 0;
    w.shake.step = 0;
    w.shake.amplitude = 0;

    for (int& flag : w.flags.values) {
        flag = 0;
    }
    clearBanners(w);
}

//-----------------------------------------------------------------------------
// The tick
//-----------------------------------------------------------------------------

void Level::tick(const CheatKeys& cheats) {
    World& w = world_;
    w.rememberPositions();

    w.blink = !w.blink;
    if (w.blink) {
        w.slowBlink = !w.slowBlink;
    }

    // Weapon pickups drop while a player could use one.
    w.weaponPickupWanted = false;
    for (Player* player : players) {
        if (player != nullptr) {
            w.weaponPickupWanted = w.weaponPickupWanted || player->wantsWeaponPickup();
        }
    }

    // The camera follows the players, then again after the events, which
    // may lock it.
    const bool scrolling = w.camera.scrolls();
    if (scrolling) {
        updateCamera(false);
    }
    w.updateEvents();
    if (w.camera.scrolls()) {
        updateCamera(true);
    }

    updateCheats(cheats);
    updateShake();

    if (w.flags[Flag::Bubbles] != 0) {
        updateBubbles();
    }
    if (w.weather.type == Weather::Rain || w.weather.type == Weather::Snow) {
        precipitation_.update();
    }

    updateAll(w.midground);
    updateAll(w.props);
    updateAll(w.movingPlatforms);
    updateAll(w.vehicles);
    updateAll(w.players);
    updateAll(w.playerShots);
    updateAll(w.cowBombs);
    updateAll(w.impacts);
    updateAll(w.enemies);
    updateAll(w.gore);
    updateAll(w.enemyShots);
    updateAll(w.blood);
    updateAll(w.pickups);
    updateAll(w.foreground);
    updateAll(w.bannerTexts);
    updateAll(w.enemyGenerators);
    updateAll(w.pickupGenerators);

    updateLock();
    updateHoldFire();
    updateTarget();
    updateFlags();
    updateGoArrow();

    if (w.weather.type == Weather::Distortion && w.weather.intensity != 0) {
        distortionPhase += 2;
        distortionPhase %= 360;
    }

    // Players cannot be hurt while the victory conditions hold.
    w.resolveCollisions(victoryWait <= 0);

    removeFinished();
}

void Level::updateCamera(bool scrollSpeedApplies) {
    World& w = world_;
    Camera& camera = w.camera;

    if (w.scrollLock.on) {
        if (camera.x < w.scrollLock.x) {
            camera.x = w.scrollLock.x;
        }
    } else if (camera.speed != 0 && scrollSpeedApplies) {
        camera.x += camera.speed;
    } else if (w.players.count() > 0) {
        // Towards the players' mean position, 2 pixels per tick, never back.
        int meanX = 0;
        for (w.players.rewind(); !w.players.atEnd(); w.players.next()) {
            meanX += w.players.current()->x;
        }
        meanX /= w.players.count();
        meanX -= camera.width / 2;

        if (meanX > camera.x) {
            if (meanX - camera.x >= 2) {
                camera.x += 2;
            } else {
                camera.x = meanX;
            }
        }
    }

    if (camera.x < 0) {
        camera.x = 0;
    } else if (camera.x > camera.limit - camera.width) {
        camera.x = camera.limit - camera.width;
    }
}

void Level::updateCheats(const CheatKeys& cheats) {
    if (cheatWait_ < kCheatDelay) {
        cheatWait_ += 1;
    }
    if (!cheats.enabled) {
        return;
    }

    // The per-player cheats, then the global ones; a one-shot cheat waits
    // 20 ticks after the previous one (and the waiting stops the checks of
    // what follows, as in 2002).
    for (int i = 0; i < kMaxPlayers; ++i) {
        Player* player = players[i];
        if (player == nullptr) {
            continue;
        }
        if (cheats.ammo[i]) {
            player->ammo += 10;
        }
        if (cheatWait_ < kCheatDelay) {
            return;
        }
        if (cheats.life[i]) {
            player->lives++;
            cheatWait_ = 0;
        }
        if (cheats.cowBomb[i]) {
            player->cowBombs += 1;
            cheatWait_ = 0;
        }
    }

    if (cheats.win) {
        won = true;
    }
    if (cheatWait_ < kCheatDelay) {
        return;
    }
    if (cheats.fpsDisplay) {
        showFps = !showFps;
        cheatWait_ = 0;
    }
    if (cheats.listsDisplay) {
        showLists = !showLists;
        cheatWait_ = 0;
    }
}

void Level::updateShake() {
    ScreenShake& shake = world_.shake;

    // A shake that started, or grew, since the last tick rumbles the
    // players' controllers.
    if (shake.amplitude > lastShake_) {
        for (const Player* player : players) {
            if (player != nullptr && player->lives > 0) {
                player->rumble(Rumble::Quake);
            }
        }
    }
    lastShake_ = shake.amplitude;

    if (shake.amplitude == 0) {
        return;
    }

    // `step` is never changed: the shake moves every tick.
    if (shake.step == 0) {
        shake.dy += shake.ddy;

        if (shake.dy <= -shake.amplitude) {
            shake.amplitude -= 1;
            shake.ddy = (shake.amplitude >> 1) + 1;
            shake.dy += 1;
        } else if (shake.dy >= shake.amplitude) {
            shake.amplitude -= 1;
            shake.ddy = -(shake.amplitude >> 1) - 1;
            shake.dy -= 1;
        }
    }
}

void Level::updateBubbles() {
    World& w = world_;

    for (w.players.rewind(); !w.players.atEnd(); w.players.next()) {
        emitBubble(w, *w.players.current());
    }
    for (w.enemies.rewind(); !w.enemies.atEnd() && w.bubbles.count() < kMaxBubbles; w.enemies.next()) {
        emitBubble(w, *w.enemies.current());
    }
    updateAll(w.bubbles);
}

void Level::updateLock() {
    World& w = world_;
    ScrollLock& lock = w.scrollLock;

    if (!lock.on) {
        return;
    }

    if ((lock.condition == ScrollLock::NoEnemies && w.enemies.empty()) ||
        (lock.condition == ScrollLock::NoGenerators && w.enemyGenerators.empty()) ||
        (lock.condition == ScrollLock::FlagEquals && w.flags.values[lock.flag] == lock.value) ||
        (lock.condition == ScrollLock::FlagAtLeast && w.flags.values[lock.flag] >= lock.value)) {
        lock.on = false;
        goArrow = true;
        goArrowX = -10;
        goBounces_ = 0;
    }
}

void Level::updateHoldFire() {
    World& w = world_;

    if (w.holdFire.on && w.flags.values[w.holdFire.flag] == w.holdFire.value) {
        w.holdFire.on = false;
    }
}

void Level::updateTarget() {
    World& w = world_;

    // With no player left, enemies aim at the middle of the screen.
    if (w.players.count() == 0) {
        standIn_.x = w.camera.centre();
        standIn_.y = w.collision.platforms[0][w.camera.centre()];
        w.target = &standIn_;
        return;
    }

    // The players in turn, one per tick (the turn is never reset).
    w.targetTurn += 1;
    w.targetTurn %= w.players.count();

    w.players.rewind();
    for (int i = 0; i < w.targetTurn && !w.players.atEnd(); ++i) {
        w.players.next();
    }
    w.target = w.players.current();
}

void Level::updateFlags() {
    World& w = world_;
    GameFlags& flags = w.flags;

    flags[Flag::Generators] = w.enemyGenerators.count();
    flags[Flag::Enemies] = w.enemies.count();
    w.pickupMode = flags[Flag::Bonus];

    // The timer counts down once per second, until the victory conditions
    // first hold.
    if (flags[Flag::Timer] > 0 && victoryWait == -1) {
        timerStep_ += 1;
        timerStep_ %= kTimerTicks;

        if (timerStep_ == 0) {
            flags[Flag::Timer] -= 1;
            if (flags[Flag::Timer] == 0 && bonusLevel) {
                won = true;
            }
        }
    }
}

void Level::updateGoArrow() {
    const World& w = world_;

    // The arrow comes when the camera has not moved for a while, or when a
    // scroll lock opens.
    if (goLastCameraX_ != w.camera.x) {
        goLastCameraX_ = w.camera.x;
        goDelay_ = 0;
        goMustStop_ = true;
    } else if (!goArrow && !w.scrollLock.on && w.camera.x < w.camera.limit - w.camera.width) {
        goDelay_ += 1;

        if (goDelay_ >= kGoDelay) {
            goArrow = true;
            goArrowX = -10;
            goBounces_ = 0;
            goMustStop_ = false;
        }
    }

    if (!goArrow) {
        return;
    }

    goSubFrame_ += 1;
    goSubFrame_ %= 4;
    if (goSubFrame_ == 0) {
        goArrowFrame += 1;
        goArrowFrame %= 5;
    }

    goAngle_ += 3;
    goAngle_ %= 360;

    if (goBounces_ == 0) {
        // Slides in from the left to the right edge (the view's width
        // is not always a multiple of the step).
        goArrowX += 10;
        if (goArrowX >= w.camera.width) {
            goArrowX = w.camera.width;
            goBounces_ += 1;
            goAngle_ = 0;
        }
    } else if (goBounces_ != -1) {
        // Bounces on the right edge; once the camera moves, goes after 3
        // bounces.
        int bounce = (100 * sine1024(goAngle_)) >> kTrigShift;
        if (bounce < 0) {
            bounce = -bounce;
        }
        goArrowX = w.camera.width - bounce;

        if (bounce == 0) {
            goBounces_ += 1;
            if (goBounces_ >= 3 && goMustStop_) {
                goBounces_ = -1;
            }
        }
    } else {
        // Leaves to the right.
        goArrowX += 10;
        if (goArrowX > w.camera.width + 160) {
            goArrow = false;
            goDelay_ = 0;
        }
    }
}

void Level::removeFinished() {
    World& w = world_;

    // List by list in the 2002 order (each list deletes its entities or,
    // when it borrows them, only forgets them).
    dropFinished(w.players);
    dropFinished(w.midground);
    dropFinished(w.playerShots);
    dropFinished(w.cowBombs);
    dropFinished(w.bubbles);
    dropFinished(w.impacts);
    dropFinished(w.enemies);
    dropFinished(w.pickups);
    dropFinished(w.enemyGenerators);
    dropFinished(w.pickupGenerators);
    dropFinished(w.bannerTexts);
    dropFinished(w.props);
    dropFinished(w.foreground);
    dropFinished(w.blood);
    dropFinished(w.enemyShots);
    dropFinished(w.weatherParticles);
    dropFinished(w.gore);
    dropFinished(w.movingPlatforms);
}

//-----------------------------------------------------------------------------
// End of the level
//-----------------------------------------------------------------------------

void Level::updateVictoryAndDefeat() {
    World& w = world_;

    if (w.players.empty()) {
        deathWait += 1;
        if (deathWait >= kEndDelay) {
            playersDead = true;
        }
        return;
    }

    if (w.camera.x >= victory.x && w.flags.values[victory.flag1] == victory.value1 &&
        w.flags.values[victory.flag2] == victory.value2) {
        w.holdFire.on = true;
        victoryWait += 1;

        // Register 1 at 999: the level ends at once (finalboss's script).
        if (w.flags[kEndNowFlag] == kEndNowValue) {
            victoryWait = kEndDelay;
        }
        if (victoryWait >= kEndDelay) {
            won = true;
        }
    } else if (victoryWait > 0) {
        victoryWait = 0;
    }
}

//-----------------------------------------------------------------------------
// Drawing
//-----------------------------------------------------------------------------

void Level::drawWorld(core::gfx::Pixmap& screen, Decor& decor) {
    namespace gfx = core::gfx;
    World& w = world_;

    // Smooth motion (World::Motion): the camera too is drawn between the
    // last two ticks, when it moved forward by at most 2 pixels (the ring of
    // the decor holds the 2 columns before the camera, Decor::draw). Its
    // place is put back after drawing.
    decor.clampCamera();
    const int cameraX = w.camera.x;
    const int cameraMoved = cameraX - w.motion.previousCameraX;
    if (w.motion.on && cameraMoved > 0 && cameraMoved <= 2) {
        w.camera.x += w.motion.offset(w.motion.previousCameraX, cameraX);
    }

    decor.draw(screen);

    drawAll(w.midground);
    drawAll(w.props);
    if (w.flags[Flag::Bubbles] != 0) {
        drawAll(w.bubbles);
    }
    drawAll(w.movingPlatforms);
    drawAll(w.blood);
    drawAll(w.gore);
    drawAll(w.enemies);
    drawAll(w.enemyShots);
    drawAll(w.pickups);
    drawAll(w.playerShots);
    drawAll(w.players);
    drawAll(w.vehicles);
    drawAll(w.impacts);
    drawAll(w.cowBombs);
    drawAll(w.weatherParticles);
    drawAll(w.foreground);

    const gfx::Pixel black = gfx::kAlphaMask;
    const int width = w.camera.width;

    // The distortion: rows of 2 pixels shifted by a sine, black behind.
    if (w.weather.type == Weather::Distortion && w.weather.intensity != 0) {
        int phase = distortionPhase;
        for (int y = 0; y < gfx::kScreenHeight; y += 2) {
            phase += 1;
            phase %= 360;

            const int shift = (5 * sine1024(phase)) >> kTrigShift;
            const gfx::Rect row =
                shift < 0 ? gfx::Rect{-shift, y, width + shift, 2} : gfx::Rect{0, y, width - shift, 2};
            gfx::blit(screen, shift < 0 ? 0 : shift, y, screen, &row);

            const gfx::Rect gap =
                shift < 0 ? gfx::Rect{width + shift, y, -shift, 2} : gfx::Rect{0, y, shift, 2};
            gfx::fill(screen, &gap, black);
        }
    }

    // The screen shake: the picture moved up or down, black in the gap.
    const ScreenShake& shake = w.shake;
    if (shake.amplitude != 0 && shake.dy != 0) {
        const int h = gfx::kScreenHeight;
        if (shake.dy < 0) {
            const gfx::Rect part{0, -shake.dy, width, h + shake.dy};
            gfx::blit(screen, 0, 0, screen, &part);
            const gfx::Rect gap{0, h + shake.dy, width, -shake.dy};
            gfx::fill(screen, &gap, black);
        } else {
            const gfx::Rect part{0, 0, width, h - shake.dy};
            gfx::blit(screen, 0, shake.dy, screen, &part);
            const gfx::Rect gap{0, 0, width, shake.dy};
            gfx::fill(screen, &gap, black);
        }
    }

    w.camera.x = cameraX;
}

void Level::drawOverlays(core::gfx::Pixmap& screen) {
    World& w = world_;

    if (goArrow) {
        core::gfx::draw(screen, *w.media.misc->image(bb::assets::sprites::misc::kGoArrow + goArrowFrame),
                        goArrowX, 150);
    }
    w.bannerTexts.forEach([&](const Banner* b) { b->draw(*w.media.canvas); });
}

//-----------------------------------------------------------------------------
// Fingerprint
//-----------------------------------------------------------------------------

std::uint64_t Level::fingerprint(std::FILE* trace, unsigned long long frame) const {
    const World& w = world_;
    core::Fnv1a64 h;

    h.add(w.camera.x);
    h.add(w.camera.speed);
    h.add(static_cast<int>(w.scrollLock.on));
    h.add(w.scrollLock.x);
    h.add(static_cast<int>(w.holdFire.on));
    h.add(w.enemiesCreated);
    h.add(w.weather.intensity);
    h.add(w.weather.type);
    h.add(w.shake.dy);
    h.add(w.dialogueToPlay);
    h.add(w.rng->state());

    for (int flag : w.flags.values) {
        h.add(flag);
    }

    if (trace != nullptr) {
        std::fprintf(trace, "tick (frame %llu) offset=%d scroll=%d lock=%d rng=%u flags", frame, w.camera.x,
                     w.camera.speed, static_cast<int>(w.scrollLock.on), w.rng->state());
        for (int flag : w.flags.values) {
            std::fprintf(trace, " %d", flag);
        }
        std::fprintf(trace, "\n");
    }

    hashList(h, w.players, true, "players", trace);
    hashList(h, w.enemies, true, "enemies", trace);
    hashList(h, w.playerShots, false, "player shots", trace);
    hashList(h, w.cowBombs, false, "cow bombs", trace);
    hashList(h, w.impacts, false, "impacts", trace);
    hashList(h, w.vehicles, false, "vehicles", trace);
    hashList(h, w.enemyShots, false, "enemy shots", trace);
    hashList(h, w.pickups, false, "pickups", trace);
    hashList(h, w.props, false, "props", trace);
    hashList(h, w.midground, false, "midground", trace);
    hashList(h, w.foreground, false, "foreground", trace);
    hashList(h, w.movingPlatforms, false, "platforms", trace);
    hashList(h, w.blood, false, "blood", trace);
    hashList(h, w.gore, false, "gore", trace);
    hashList(h, w.weatherParticles, false, "weather", trace);
    hashList(h, w.bubbles, false, "bubbles", trace);

    h.add(w.sleepingEvents.count());
    h.add(w.events.count());
    h.add(w.enemyGenerators.count());
    h.add(w.pickupGenerators.count());
    h.add(w.bannerTexts.count());

    return h.value();
}

}  // namespace bb::game
