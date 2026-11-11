// The zombie bear (ennemi_bisou_zombi.cpp) and the shaman
// (ennemi_bisou_chaman.cpp).

#include <algorithm>

#include "bb/assets/music/bisou.hpp"
#include "bb/game/trig.hpp"
#include "carebears/carebears_world.hpp"

namespace bb::game::carebears {

namespace {

const int kBloodX[] = {0, 7, 10, 7, 0, -7, -10, -7};
const int kBloodY[] = {-15, -25, -25, -25, -35, -25, -25, -25};
const int kBloodYCrawling[] = {0, -10, -10, -10, -20, -10, -10, -10};

constexpr Flag kShamanState = Flag::User0;  // 1 fight starts, 2 fighting, 7 dead
constexpr Flag kShamanHere = Flag::User2;   // set by the shaman every tick

// The zombies' moans: one counter for all of them, advanced once per tick
// (on the blink phase) until the shaman appears. 2002 file statics, kept
// in World::shared(): they only choose when a moan sound plays, which is
// not fingerprinted; reset at each level start (D-50).
struct Moans {
    int step = 199;
    int sound = 0;
    bool phase = false;
};

}  // namespace

//-----------------------------------------------------------------------------
// The zombie bear
//-----------------------------------------------------------------------------

void ZombieBear::update() {
    World& w = world();

    if (bleeding > 0) {
        bleeding -= 1;
    }

    // The shaman is dead: every zombie burns.
    if (w.flags[kShamanState] == 7 && mode != kCarbonised && mode != kTopBurning) {
        animation.restart();
        mode = mode == kCut ? kTopBurning : kCarbonised;
    }

    Moans& moans = w.shared<Moans>();
    if (moans.phase != w.blink && w.flags[kShamanHere] == 0) {
        moans.step += 1;
        moans.step %= 200;
        if (moans.step == 0) {
            moans.sound += 1;
            moans.sound %= 3;
            w.media.levelSounds->play(snd::kZombieMoan + moans.sound);
        }
        moans.phase = w.blink;
    }

    switch (mode) {
        case kNormal:
        case kWalking:
            walkAbout();
            break;
        case kCut:
            cut();
            break;
        case kFalling:
            topFalls();
            break;
        case kCrawling:
            crawl();
            break;
        case kDying:
            die();
            break;
        case kCarbonised:
            burnUp(img::kZombieBurnRight, img::kZombieBurnLeft, 6);
            break;
        case kTopBurning:
            burnUp(img::kZombieTopBurnRight, img::kZombieTopBurnLeft, 7);
            break;
        default:
            break;
    }
    dieWhenOffscreen();
}

void ZombieBear::walkAbout() {
    static const int kSpeed = 1;

    fall();

    animation.cycle(15, 8);

    // Walks every other tick.
    walkPhase_ = !walkPhase_;
    if (walkPhase_) {
        if (x - kSpeed < minX || solidWall(x - kSpeed, y)) {
            direction = kFacingRight;
        } else if (x + kSpeed > world().camera.right() || solidWall(x + kSpeed, y)) {
            direction = kFacingLeft;
        }
        if (direction == kFacingRight) {
            walk(kSpeed);
            image = enemyImage(img::kZombieWalkRight + animation.frame);
        } else {
            walk(-kSpeed);
            image = enemyImage(img::kZombieWalkLeft + animation.frame);
        }
    }
    hitboxFromImage();
}

void ZombieBear::hitBy(Shot& shot) {
    World& w = world();
    const int wound = shot.damage();

    shot.onHit(health.take(wound));
    if (health.left()) {
        // Below 500 HP a non-burning shot cuts it in two.
        if (health.points < 500 && (mode == kNormal || mode == kWalking) && shot.burning() == 0) {
            animation.restart();
            hitbox.active = false;
            mode = kCut;
        }
    } else {
        shot.onKill();
        hitbox.active = false;
        w.flags[Flag::Kills] += 1;  // its own pipeline: no wall blood, no pickup

        animation.restart();
        if (mode == kCrawling) {
            mode = shot.burning() == 2 ? kTopBurning : kDying;
        } else if (shot.burning() != 0) {
            mode = kCarbonised;
        } else if (mode != kCut) {
            mode = kCut;
        }
    }

    sprayBlood(shot, kBloodX, mode == kCrawling ? kBloodYCrawling : kBloodY);
}

void ZombieBear::topFalls() {
    animation.tickUpTo(6, 3);

    if (direction == kFacingRight) {
        image = enemyImage(img::kZombieFallRight + animation.frame);
        x += 1;
    } else {
        image = enemyImage(img::kZombieFallLeft + animation.frame);
        x -= 1;
    }

    fall();
    if (platformAt(x, y) != 0 && animation.frame >= 3) {
        animation.restart();
        mode = kCrawling;
        hitbox.active = true;
        crawl();
    }
}

void ZombieBear::die() {
    fall();

    animation.tickUpTo(6, 3);

    image = enemyImage((direction == kFacingRight ? img::kZombieDieRight : img::kZombieDieLeft) +
                       animation.frame);

    if (animation.frame >= 3) {
        world().media.effects->engrave(x, y, image);
        dead = true;
    }
}

void ZombieBear::cut() {
    fall();

    animation.tick(5);

    if (animation.frame == 4) {
        auto* legs = new WalkingLegs(world(), img::kZombieLegsRight, img::kZombieLegsLeft);
        legs->x = x;
        legs->direction = direction;
        legs->y = y;
        world().gore.add(legs);
        y -= 19;
        dy = 0;
        animation.restart();
        mode = kFalling;
        topFalls();
    } else {
        image = enemyImage((direction == kFacingRight ? img::kZombieCutRight : img::kZombieCutLeft) +
                           animation.frame);
    }
}

void ZombieBear::crawl() {
    static const int kCrawl[] = {0, 1, 2, 1};
    static const int kSpeed = 1;

    if (health.points <= 0) {
        animation.restart();
        mode = kDying;
        hitbox.active = false;
        die();
        return;
    }

    fall();

    // Crawls every fourth tick.
    crawlStep_ = (crawlStep_ + 1) % 4;
    if (crawlStep_ == 0) {
        if (x - kSpeed < minX || solidWall(x - kSpeed, y)) {
            direction = kFacingRight;
        } else if (x + kSpeed > world().camera.right() || solidWall(x + kSpeed, y)) {
            direction = kFacingLeft;
        }
        walk(direction == kFacingRight ? kSpeed : -kSpeed);
        hitboxFromImage();
    }

    image = enemyImage((direction == kFacingRight ? img::kZombieCrawlRight : img::kZombieCrawlLeft) +
                       animation.loop(kCrawl, 4, 16));
}

void ZombieBear::burnUp(int firstRight, int firstLeft, int lastStep) {
    if (animation.tick(4)) {
        if (animation.frame > lastStep) {
            dead = true;
            return;
        }
    }
    image = enemyImage((direction == kFacingRight ? firstRight : firstLeft) + animation.frame);
}

//-----------------------------------------------------------------------------
// The shaman
//-----------------------------------------------------------------------------

Shaman::Shaman(World& world) : Enemy(world) {
    health.points = 11000;
    shotDelay_ = 250 + world.random() % 50;
    treasure = 0;
}

void Shaman::update() {
    World& w = world();

    w.flags[kShamanHere] = 1;
    if (w.flags[kShamanState] == 1) {
        w.media.levelMusic->play(bb::assets::music::bisou::kBoss);
        w.flags[kShamanState] = 2;
        w.flags[Flag::Timer] = 50;
    }

    if (bleeding > 0) {
        bleeding -= 1;
    }
    shotWait_ += 1;

    switch (mode) {
        case kNormal:
        case kWalking:
            stand();
            break;
        case kRaisingStaff:
            raiseStaff();
            break;
        case kTeleporting:
            teleport();
            break;
        case kLoweringStaff:
            lowerStaff();
            break;
        case kShooting:
            cast();
            break;
        case kDying:
        case kCarbonised:
            burnUp();
            break;
        default:
            break;
    }
    dieWhenOffscreen();
}

void Shaman::stand() {
    World& w = world();
    fall();

    animation.cycle(6, 8);

    // Faces the middle of the screen.
    if (x < w.camera.centre()) {
        image = enemyImage(img::kShamanTeleportRight);
        direction = kFacingRight;
    } else {
        image = enemyImage(img::kShamanTeleportLeft);
        direction = kFacingLeft;
    }

    if (shotWait_ > shotDelay_ && animation.frame == 0) {
        mode = kRaisingStaff;
        animation.subFrame = 0;
        raiseStaff();
    }

    if (hits_ >= 10) {
        animation.restart();
        mode = kTeleporting;
        w.media.levelSounds->play(snd::kShamanTeleport);
    }
    hitboxFromImage();
}

void Shaman::teleport() {
    World& w = world();
    const int camera = w.camera.x;

    if (animation.tick(4)) {
        if (animation.frame == 8) {
            // Away from the target player: left spot when the target is on
            // the right half, else the high spot unless the target is high.
            int spot = 0;
            if (w.target->x > camera + w.camera.width / 2) {
                spot = 2;
            } else if (w.target->y < 280) {
                spot = 0;
            } else {
                spot = 1;
            }
            if (spot != spot_) {
                spot_ = spot;
            } else {
                spot_ += w.random() % 2 + 1;
                spot_ %= 3;
            }
            // His three spots, (x, y) in the level's marker: high right,
            // right, left.
            x = w.marker("carebears.shamanSpots", 2 * static_cast<std::size_t>(spot_));
            y = w.marker("carebears.shamanSpots", 2 * static_cast<std::size_t>(spot_) + 1);
        } else if (animation.frame == 19) {
            mode = kNormal;
            hits_ = 0;
            return;
        }
    }

    // Vanishes (8 images), blinks (3 steps), comes back (8 images).
    const bool onLeft = x < camera + w.camera.width / 2;
    direction = onLeft ? kFacingRight : kFacingLeft;
    const int first = onLeft ? img::kShamanTeleportRight : img::kShamanTeleportLeft;
    if (animation.frame <= 7) {
        image = enemyImage(first + animation.frame);
    } else if (animation.frame <= 10) {
        image = animation.subFrame % 2 == 0 ? enemyImage(first + 7) : nullptr;
    } else {
        image = enemyImage(first + 7 - (animation.frame - 11));
    }
    noHitbox();
}

void Shaman::raiseStaff() {
    if (animation.tick(5)) {
        if (animation.frame > 2) {
            animation.restart();
            mode = kShooting;
            glows_ = 0;
            cast();
            return;
        }
    }
    image = enemyImage((direction == kFacingRight ? img::kShamanStaffRight : img::kShamanStaffLeft) +
                       animation.frame);
    hitboxFromImage();
}

void Shaman::lowerStaff() {
    if (animation.tick(5)) {
        if (animation.frame > 2) {
            animation.restart();
            mode = kWalking;
            shotDelay_ = 100 + world().random() % 50;
            shotWait_ = 0;
            return;
        }
    }
    image = enemyImage((direction == kFacingRight ? img::kShamanStaffRight : img::kShamanStaffLeft) + 2 -
                       animation.frame);
    hitboxFromImage();
}

void Shaman::cast() {
    World& w = world();

    if (animation.tick(4)) {
        animation.frame %= 11;
        if (animation.frame == 0) {
            glows_ += 1;
            if (glows_ >= 3) {
                // A fireball when the target is more than 45 degrees off
                // the vertical, else a column along the ground.
                bool fireball = false;
                int dx = 0;
                int dyTarget = 0;
                if (w.target != nullptr) {
                    dx = w.target->x - x;
                    dyTarget = w.target->y - (y - 72);
                    // At least 1 (D-43: 2002 divided by zero at distance 0).
                    const int distance = std::max(1, squareRoot(dx * dx + dyTarget * dyTarget));
                    dx = (dx * 6) / distance;
                    dyTarget = (dyTarget * 6) / distance;
                    const int slope = dyTarget == 0 ? 10 : dx / dyTarget;
                    if (slope < -1 || slope > 1) {
                        fireball = true;
                    }
                }
                const int startX = direction == kFacingRight ? x + 18 : x - 18;
                if (fireball) {
                    auto* ball = new ShamanBall(w);
                    ball->y = y - 72;
                    ball->x = startX;
                    ball->xSpeed = (dx * 4) / 3;
                    ball->dy = (dyTarget * 4) / 3;
                    w.enemyShots.add(ball);
                } else {
                    auto* column = new ShamanColumn(w);
                    column->y = y - 72;
                    column->x = startX;
                    column->direction = direction;
                    w.enemyShots.add(column);
                }
                animation.restart();
                mode = kLoweringStaff;
            }
        }
    }
    hitboxFromImage();
}

void Shaman::draw() {
    Actor::draw();
    if (mode == kShooting) {
        if (direction == kFacingRight) {
            drawAt(x + 18, y - 70, enemyImage(img::kShamanGlowRight + animation.frame));
        } else {
            drawAt(x - 18, y - 70, enemyImage(img::kShamanGlowLeft + animation.frame));
        }
    }
}

void Shaman::burnUp() {
    if (animation.tick(4)) {
        if (animation.frame > 7) {
            dead = true;
            return;
        }
    }
    image = enemyImage((direction == kFacingRight ? img::kShamanBurnRight : img::kShamanBurnLeft) +
                       animation.frame);
}

void Shaman::hitBy(Shot& shot) {
    static const int kShamanBloodX[] = {0, 7, 10, 7, 0, -7, -10, -7};

    World& w = world();
    Enemy::hitBy(shot);
    sprayBlood(shot, kShamanBloodX, kBloodY);

    hits_ += 1;

    if (mode == kDying || mode == kCarbonised) {
        w.flags[kShamanState] = 7;
        w.flags[Flag::GeneratorsOff] = 1;
        w.media.levelSounds->play(snd::kShamanDeath);
    }
}

}  // namespace bb::game::carebears
