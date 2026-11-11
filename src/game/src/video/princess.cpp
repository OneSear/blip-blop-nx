// The Princess and her guards (ennemi_princesse.cpp,
// ennemi_princesseguard.cpp).

#include "video/video_world.hpp"

namespace bb::game::video {

namespace {

const int kBloodX[] = {0, 10, 15, 10, 0, -10, -15, -10};
const int kBloodY[] = {-15, -25, -25, -25, -35, -25, -25, -25};

// The level script's registers.
constexpr Flag kEscort = Flag::User1;       // set: the guards come, she walks off
constexpr Flag kGuardsFall = Flag::User2;   // set: every guard dies
constexpr Flag kGuardsAlive = Flag::User3;  // counted down by the guards

constexpr int kEscapedText = 517;  // banner texts, missing from uk.dat (D-63)
constexpr int kKilledText = 519;
constexpr int kCowBombPickup = 6;  // EventBonus BCOW
constexpr int kPrincessSpeed = 1;

bool onEngravingGround(World& w, int x, int y) {
    const int ground = w.collision.platformAt(x, y);
    return ground != 0 && ground != w.collision.platforms[4][x];
}

}  // namespace

//-----------------------------------------------------------------------------
// The Princess
//-----------------------------------------------------------------------------

Princess::Princess(World& world) : Enemy(world) {
    health.points = 3700;
    treasure = 0;
    image = enemyImage(img::kPrincessStand);
}

void Princess::update() {
    if (bleeding > 0) {
        bleeding -= 1;
    }

    switch (mode) {
        case kNormal:
        case kWalking:
            walkAbout();
            break;
        case kDying:
        case kCarbonised:
            die();
            break;
        default:
            break;
    }

    if (world().flags[kEscort] != 0) {
        dieWhenOffscreen();
    }
}

void Princess::walkAbout() {
    World& w = world();
    const int camera = w.camera.x;
    const int cameraRight = w.camera.right();

    if (w.flags[kEscort] != 0) {
        if (x > cameraRight + 180) {
            showBanner(w, kEscapedText);
            dead = true;
        }
        if (!guarded_) {
            guarded_ = true;
            for (int i = 1; i < 4; i++) {
                auto* guard = new PrincessGuard(w, this, 40 + i * 40);
                guard->x = cameraRight + 80 - i * 20;
                guard->y = y - 20;
                guard->direction = kFacingLeft;
                w.enemies.add(guard);
            }
            w.enemiesCreated += 3;
        }
        if (direction == kFacingRight) {
            // Faster in the left part of the screen.
            animation.cycle(6, 2);
            walk(x - camera > 400 ? kPrincessSpeed : 2);
            image = enemyImage(img::kPrincessWalk + animation.frame);
        } else if (direction == kFacingLeft) {
            direction = kFacingRight;
        }
    } else if (x < world().marker("video.princessWalk", 0)) {
        animation.cycle(6, 2);
        walk(kPrincessSpeed);
        image = enemyImage(img::kPrincessWalk + animation.frame);
    } else if (x > world().marker("video.princessWalk", 1)) {
        image = enemyImage(img::kPrincessWalk);
    } else {
        image = enemyImage(img::kPrincessStand);
    }
    hitboxFromImage();
}

void Princess::die() {
    World& w = world();

    fall();

    animation.tickUpTo(6, 6);

    if (animation.frame >= 5) {
        if (onEngravingGround(w, x, y)) {
            w.media.effects->engrave(x, y, image);
            showBanner(w, kKilledText);
            dead = true;
            if (w.flags[kGuardsAlive] > 0) {
                w.flags[kGuardsFall] = 1;
            }
            w.registry->spawn(w, EntityCategory::Pickup, kCowBombPickup, SpawnPoint{x, y, 0});
        }
    } else {
        image = enemyImage(img::kPrincessDie + animation.frame);
    }
}

void Princess::hitBy(Shot& shot) {
    // Bleeds, but is hurt only once the escort has started.
    if (world().flags[kEscort] != 0) {
        Enemy::hitBy(shot);
    }
    sprayBlood(shot, kBloodX, kBloodY);
}

//-----------------------------------------------------------------------------
// The guards
//-----------------------------------------------------------------------------

namespace {

constexpr int kGuardSpeed = 1;
const int kGuardWalk[] = {0, 1, 2, 1};

}  // namespace

PrincessGuard::PrincessGuard(World& world, Princess* princess, int distance)
    : Enemy(world), princess_(princess), distance_(distance) {
    health.points = 2500;
    treasure = 100;
    direction = kFacingLeft;
}

void PrincessGuard::update() {
    if (bleeding > 0) {
        bleeding -= 1;
    }

    switch (mode) {
        case kNormal:
        case kWalking:
            walkAbout();
            break;
        case kCarbonised:
        case kDying:
            die();
            break;
        default:
            break;
    }
    dieWhenOffscreen();
}

void PrincessGuard::walkAbout() {
    World& w = world();

    // A null handle: she was destroyed (the 2002 code read freed memory,
    // D-04).
    Princess* princess = princess_.get();
    if (princess == nullptr || princess->dead) {
        dead = true;
    }

    if (w.flags[kGuardsFall] != 0) {
        mode = kDying;
        die();
        return;
    }

    fall();
    if (princess != nullptr) {
        // Keeps `distance_` pixels to her left; catches up three times
        // faster than it falls back.
        if (x + distance_ + kGuardSpeed < princess->x) {
            walk(kGuardSpeed);
            image = enemyImage(img::kGuardWalkLeft + animation.loop(kGuardWalk, 4, 6));
        } else if (x + distance_ - kGuardSpeed > princess->x) {
            walk(-kGuardSpeed * 3);
            image = enemyImage(img::kGuardWalkLeft + animation.loop(kGuardWalk, 4, 6));
        } else {
            image = enemyImage(img::kGuardWalkLeft);
        }
    } else {
        dead = true;
    }
    hitboxFromImage();
}

void PrincessGuard::die() {
    World& w = world();

    fall();

    animation.tickUpTo(6, 10);

    if (animation.frame >= 10) {
        if (onEngravingGround(w, x, y)) {
            w.media.effects->engrave(x, y, image);
            dead = true;
            w.flags[kGuardsAlive] -= 1;
        }
    } else {
        image = enemyImage(img::kGuardDie + animation.frame);
    }
}

void PrincessGuard::hitBy(Shot& shot) {
    Enemy::hitBy(shot);
    sprayBlood(shot, kBloodX, kBloodY);
}

}  // namespace bb::game::video
