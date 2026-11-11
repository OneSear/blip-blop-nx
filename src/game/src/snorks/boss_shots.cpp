// The Snork in Black boss (ennemi_snork_in_black_boss.cpp), the Snork shots
// (tir_snorky_base.cpp, tir_snorky_base_2.cpp, tir_snorky_gouverneur.cpp,
// tir_snorky_mage.cpp, tir_epine.cpp) and the debris
// (moceaux_tete_snorky.cpp).

#include <cstdlib>

#include "bb/assets/music/snork.hpp"
#include "snorks/snorks_world.hpp"

namespace bb::game::snorks {

//-----------------------------------------------------------------------------
// The boss
//-----------------------------------------------------------------------------

namespace {

// His arena is in the level's markers: "snorks.bossPerches" (x, y of his
// four perches), "snorks.bossZones" (the target's zones that choose one:
// left of x, right of x, above y) and "snorks.sharkPath" (where the shark
// starts, sinks until, takes him, rises from, is gone).
constexpr int kTolerance = 5;  // the 2002 lowercase macro "tolerance"

constexpr Flag kBossState = Flag::User0;  // 1 intro, 2 fight, 3 shark, 4 gone

struct JumpVector {
    int speedX;
    int speedY;
};

// From perch [from] to perch [to] (unused on the diagonal).
const JumpVector kJumps[4][4] = {
    {{0, 0}, {4, -7}, {-1, -2}, {5, -3}},
    {{-6, -5}, {0, 0}, {-6, -3}, {-1, 3}},
    {{1, -7}, {5, -9}, {0, 0}, {7, -5}},
    {{-6, -7}, {1, -6}, {-5, -6}, {0, 0}},
};

}  // namespace

SnorkBoss::SnorkBoss(World& world) : Enemy(world) {
    health.points = 15000;
    dy = 0;
    direction = kFacingLeft;
    image = enemyImage(img::kBossStill);
    sharkX_ = world.marker("snorks.sharkPath", 0);
}

void SnorkBoss::update() {
    if (bleeding > 0) {
        bleeding -= 1;
    }
    switch (mode) {
        case kWalking:
        case kNormal:
            stand();
            break;
        case kFalling:
        case kJumping:
            jump();
            break;
        case kCarbonised:
        case kDying:
            die();
            break;
        case kShooting:
            fire();
            break;
        default:
            break;
    }
    // Only removed far to the right (not by the usual off-screen test).
    if (x > world().camera.right() + 360) {
        dead = true;
    }
}

void SnorkBoss::aimJump(int destination) {
    // Perches 0 and 2 are on the left: he faces right from them.
    direction = perch_ == 0 || perch_ == 2 ? kFacingRight : kFacingLeft;
    xSpeed_ = kJumps[perch_][destination].speedX;
    dy = kJumps[perch_][destination].speedY;
    perch_ = destination;
}

void SnorkBoss::stand() {
    World& w = world();

    if (w.flags[kBossState] == 1) {
        // The intro (18 images), then the fight music and a 50 s timer.
        if (animation.subFrame == 0 && animation.frame == 0) {
            w.media.levelSounds->play(snd::kBossIntro);
        }
        animation.tick(8);
        if (animation.frame >= 18) {
            w.flags[kBossState] = 2;
            animation.restart();
            w.media.levelMusic->play(bb::assets::music::snork::kBoss);
            w.flags[Flag::Timer] = 50;
        } else {
            image = enemyImage(img::kBossIntro + animation.frame);
        }
    } else if (w.flags[kBossState] == 2) {
        animation.frame += 1;
        if (animation.frame > 20) {
            if (attacking_) {
                // After an attack: jump to the perch facing the target.
                attacking_ = false;
                animation.subFrame = 0;
                int destination = w.random() % 4;
                const Entity* t = w.target;
                const int left = w.marker("snorks.bossZones", 0);
                const int right = w.marker("snorks.bossZones", 1);
                const int high = w.marker("snorks.bossZones", 2);
                if (t->x > right && t->y > high) {
                    destination = 2;
                } else if (t->x < left && t->y > high) {
                    destination = 3;
                } else if (t->x > right && t->y < high) {
                    destination = 0;
                } else if (t->x < left && t->y < high) {
                    destination = 1;
                }
                if (destination == perch_) {
                    animation.frame = 15;
                } else {
                    aimJump(destination);
                    animation.frame = 0;
                    mode = kFalling;
                    gravityDelay = 0;
                    jumping_ = true;
                    jump();
                    return;
                }
            } else {
                // Attack: a big spine at a target level with him and on his
                // side, else a fan of six spines.
                mode = kShooting;
                animation.restart();
                if (w.target != nullptr) {
                    const Entity* t = w.target;
                    const bool onHisSide = (perch_ == 0 || perch_ == 2) ? t->x > x : t->x < x;
                    attackType_ = onHisSide && y < t->y && y > t->y - 50 ? 1 : 0;
                }
                w.media.levelSounds->play(attackType_ == 0 ? snd::kBossSpines : snd::kBossBigSpine);
                attacking_ = true;
                fire();
                return;
            }
        }
        if (perch_ == 0 || perch_ == 2) {
            direction = kFacingRight;
            image = enemyImage(img::kBossStandRight);
        } else {
            direction = kFacingLeft;
            image = enemyImage(img::kBossStandLeft);
        }
    }
    hitboxFromImage();
}

// Moves along the jump; true when he reached the perch (snapped on it).
bool SnorkBoss::landJump() {
    gravityDelay += 1;
    gravityDelay %= kGravityDelay;
    if (gravityDelay == 0 && dy < kMaxFallSpeed) {
        dy += 1;
    }
    y += dy;

    const int px = world().marker("snorks.bossPerches", 2 * static_cast<std::size_t>(perch_));
    const int py = world().marker("snorks.bossPerches", 2 * static_cast<std::size_t>(perch_) + 1);
    if ((xSpeed_ > 0 && x + xSpeed_ >= px) || (xSpeed_ < 0 && x + xSpeed_ <= px)) {
        xSpeed_ = 0;
        x = px;
    }
    x += xSpeed_;

    if (x > px - kTolerance && x < px + kTolerance && y > py - kTolerance && y < py + kTolerance) {
        animation.restart();
        x = px;
        y = py;
        jumping_ = false;
        return true;
    }
    return false;
}

void SnorkBoss::jump() {
    static const int kJump[] = {0, 1, 2, 1};
    if (landJump()) {
        mode = kWalking;
        stand();
        return;
    }
    image = enemyImage((direction == kFacingRight ? img::kBossJumpRight : img::kBossJumpLeft) +
                       animation.loop(kJump, 4, 6));
    hitboxFromImage();
}

void SnorkBoss::die() {
    static const int kJump[] = {0, 1, 2, 1};
    World& w = world();

    hitbox.active = false;
    if (w.flags[kBossState] == 2 && !jumping_) {
        // Back to perch 0 first.
        if (perch_ == 0) {
            w.flags[kBossState] = 3;
            direction = kFacingRight;
            image = enemyImage(img::kBossStandRight);
        } else {
            aimJump(0);
            animation.frame = 0;
            jumping_ = true;
            gravityDelay = 0;
        }
    } else if (w.flags[kBossState] == 2 && jumping_) {
        if (landJump()) {
            return;
        }
        image = enemyImage((direction == kFacingRight ? img::kBossJumpRight : img::kBossJumpLeft) +
                           animation.loop(kJump, 4, 6));
    } else if (w.flags[kBossState] == 3) {
        // The shark swims past and takes him.
        shark_ = true;
        sharkX_ += 5;
        if (sharkX_ < w.marker("snorks.sharkPath", 1)) {
            sharkY_ += 1;
        } else if (sharkX_ > w.marker("snorks.sharkPath", 3)) {
            sharkY_ -= 1;
        }
        if (sharkX_ > w.marker("snorks.sharkPath", 4)) {
            w.flags[kBossState] = 4;
        }
    } else if (w.flags[kBossState] == 4) {
        sharkX_ += 5;
    }
}

void SnorkBoss::fire() {
    World& w = world();
    const bool right = direction == kFacingRight;

    if (attackType_ == 0) {
        // Six spines fanning from up to down, one per step.
        struct FanSpine {
            int image;
            int speedX;
            int speedY;
            int dx;
            int dy;
        };
        static const FanSpine kFan[6] = {
            {0, 4, -6, 36, -51}, {1, 5, -5, 36, -51}, {2, 6, -4, 52, -16},
            {3, 7, 0, 52, -16},  {4, 6, 4, 35, 25},   {5, 5, 5, 35, 25},
        };

        animation.tick(6);
        if (animation.frame > 5) {
            mode = kNormal;
            animation.restart();
            stand();
            return;
        }
        if (animation.subFrame == 1) {
            const FanSpine& f = kFan[animation.frame];
            // Right: images 425 to 430; left: 439 down to 434.
            auto* spine = right ? new Spine(w, img::kBossSpinesRight + f.image, f.speedX, f.speedY)
                                : new Spine(w, img::kBossSpinesLeft + 5 - f.image, -f.speedX, f.speedY);
            spine->direction = direction;
            spine->x = right ? x + f.dx : x - f.dx;
            spine->y = y + f.dy;
            w.enemyShots.add(spine);
        }
        image = enemyImage((right ? img::kBossFireRight : img::kBossFireLeft) + 2 * (animation.frame / 2) +
                           animation.subFrame / 3);
    } else {
        animation.tick(5);
        if (animation.frame >= 1) {
            auto* spine = right ? new Spine(w, img::kBossBigSpineRight, 14, 0)
                                : new Spine(w, img::kBossBigSpineLeft, -14, 0);
            spine->direction = direction;
            spine->x = right ? x + 77 : x - 77;
            spine->y = y - 2;
            w.enemyShots.add(spine);
            mode = kNormal;
            animation.restart();
            stand();
            return;
        }
        image = enemyImage(right ? img::kBossAimRight : img::kBossAimLeft);
    }
    hitboxFromImage();
}

void SnorkBoss::hitBy(Shot& shot) {
    static const int kBloodX[] = {0, 0, 4, 0, 0, 0, -4, 0};
    static const int kBloodY[] = {-15, -15, -15, -15, -25, -15, -15, -15};
    Enemy::hitBy(shot);
    sprayBlood(shot, kBloodX, kBloodY);
}

void SnorkBoss::draw() {
    const int takenX = world().marker("snorks.sharkPath", 2);
    if (sharkX_ < takenX) {
        Actor::draw();
    }
    if (shark_) {
        drawAt(sharkX_, sharkY_, enemyImage(sharkX_ < takenX ? img::kSharkLeft : img::kSharkRight));
    }
}

//-----------------------------------------------------------------------------
// Shots
//-----------------------------------------------------------------------------

namespace {

const int kBobShot[] = {0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 2, 2, 2, 1, 1, 1, 0, 0, 0};

// Outside the usual shot area (around the view).
bool outside(int x, int y, const Camera& camera) {
    return x < camera.x - 100 || x > camera.right() + 60 || y > 520 || y < -50;
}

}  // namespace

GruntShot::GruntShot(World& world, int speed) : Projectile(world), xSpeed_(speed) {
    dy = 0;
}

void GruntShot::update() {
    if (outside(x, y, world().camera)) {
        dead = true;
    }

    // Accelerates by one every 20 ticks.
    speedStep_ += 1;
    speedStep_ %= 20;
    if (speedStep_ == 0) {
        xSpeed_ += direction == kFacingRight ? 1 : -1;
    }
    x += xSpeed_;

    bobStep_ += 1;
    bobStep_ %= 43;
    if (bobStep_ >= 22) {
        y -= kBobShot[bobStep_ - 22];
    } else {
        y += kBobShot[bobStep_];
    }

    animation.cycle(4, 3);
    image = enemyImage((direction == kFacingRight ? img::kGruntShotRight : img::kGruntShotLeft) +
                       animation.frame);

    if (solidWall(x, y)) {
        dead = true;
    }
    hitboxFromImage();
}

GunnerShot::GunnerShot(World& world, int speedX, int speedY) : Projectile(world), xSpeed_(speedX) {
    dy = speedY;
    image = enemyImage(img::kGunnerShot);
}

void GunnerShot::update() {
    x += xSpeed_;
    y += dy;
    if (outside(x, y, world().camera)) {
        dead = true;
    }
    if (solidWall(x, y)) {
        dead = true;
    }
    hitboxFromImage();
}

GovernorShot::GovernorShot(World& world, Entity* target, int speedX, int speedY)
    : Projectile(world), target_(target), xSpeed_(speedX) {
    dy = speedY;
}

void GovernorShot::update() {
    static const int kInertia = 6;
    static const int kMaxSpeed = 6;
    static const int kSteerTime = 100;

    animation.cycle(4, 4);

    const int camera = world().camera.x;
    const int cameraRight = world().camera.right();
    if (x < camera - 100 || x > cameraRight + 100 || y > 580 || y < -100) {
        dead = true;
    }

    // Steers for 100 ticks, or while nearly still.
    age_ += 1;
    const bool slow = xSpeed_ < 2 && xSpeed_ > -2 && dy < 2 && dy > -2;
    if ((age_ < kSteerTime || slow) && target_ && !target_->dead) {
        if (x < target_->x && xSpeed_ < kMaxSpeed) {
            inertiaX_ += 1;
            if (inertiaX_ > kInertia) {
                xSpeed_ += 1;
                inertiaX_ = 0;
            }
        } else if (x > target_->x && xSpeed_ > -kMaxSpeed) {
            inertiaX_ -= 1;
            if (inertiaX_ < -kInertia) {
                xSpeed_ -= 1;
                inertiaX_ = 0;
            }
        }
        if (y < target_->y && dy < kMaxSpeed) {
            inertiaY_ += 1;
            if (inertiaY_ > kInertia) {
                dy += 1;
                inertiaY_ = 0;
            }
        } else if (y > target_->y && dy > -kMaxSpeed) {
            inertiaY_ -= 1;
            if (inertiaY_ < -kInertia) {
                dy -= 1;
                inertiaY_ = 0;
            }
        }
    }

    x += xSpeed_;
    y += dy;
    image = enemyImage(xSpeed_ <= 0 ? img::kGovernorShot + animation.frame
                                    : img::kGovernorShot + 3 - animation.frame);

    if (solidWall(x + xSpeed_, y + dy)) {
        dead = true;
    }
    hitboxFromImage();
}

void MageShot::update() {
    animation.tickUpTo(4, 3);
    if (animation.frame == 3) {
        animation.frame = 1;
    }
    if (animation.frame == 0 && animation.subFrame == 1) {
        xSpeed_ = direction == kFacingRight ? 6 : -6;
    }

    if (outside(x, y, world().camera)) {
        dead = true;
    }
    if (solidWall(x + xSpeed_, y + dy)) {
        dead = true;
    }
    // About to hit a wall: the impact image.
    if (solidWall(x + 3 * xSpeed_, y + 3 * dy)) {
        animation.frame = 3;
    }
    x += xSpeed_;
    y += dy;

    image =
        enemyImage((direction == kFacingRight ? img::kMageShotRight : img::kMageShotLeft) + animation.frame);
    hitboxFromImage();
}

Spine::Spine(World& world, int imageIndex, int speedX, int speedY) : Projectile(world), xSpeed_(speedX) {
    image = enemyImage(imageIndex);
    dy = speedY;
}

void Spine::update() {
    x += xSpeed_;
    y += dy;
    if (outside(x, y, world().camera) || solidWall(x, y)) {
        dead = true;
    }
    hitboxFromImage();
}

//-----------------------------------------------------------------------------
// Debris
//-----------------------------------------------------------------------------

void GruntHead::update() {
    World& w = world();

    animation.tick(5);

    const bool right = direction == kFacingRight;
    if (animation.frame == 2 && animation.subFrame == 0) {
        GruntTrunk* trunk = nullptr;
        if (right) {
            const int speedX = 1 + w.random() % 2;
            const int speedY = -1 + w.random() % 2;
            trunk = new GruntTrunk(w, speedX, speedY);
            trunk->x = x + 19;
        } else {
            const int speedX = -1 - w.random() % 2;
            const int speedY = -1 - w.random() % 2;
            trunk = new GruntTrunk(w, speedX, speedY);
            trunk->x = x - 19;
        }
        trunk->direction = direction;
        trunk->y = y - 26;
        w.blood.add(trunk);
    }
    if (animation.frame == 3 && animation.subFrame == 0) {
        auto* back = new GruntHeadBack(w, right ? 1 : -1, 0);
        back->x = x;
        back->direction = direction;
        back->y = y + 10;
        w.blood.add(back);
    }

    if (animation.frame == 6) {
        w.media.effects->engrave(x, y, image);
        dead = true;
        return;
    }

    fall();
    if (right) {
        x += 1;
        image = enemyImage(img::kGruntHeadRight + animation.frame);
        if (animation.frame >= 3) {
            fall();
            x += 1;
        }
        if (platformAt(x, y) != 0 && x > 0) {
            x -= 6 - animation.frame;
        }
    } else {
        x -= 1;
        image = enemyImage(img::kGruntHeadLeft + animation.frame);
        if (animation.frame >= 3) {
            fall();
            x -= 1;
        }
        // (2002: "x < 0", never true: no slowing down to the left.)
        if (platformAt(x, y) != 0 && x < 0) {
            x += 6 - animation.frame;
        }
    }
}

void GruntHeadBack::update() {
    fall();
    x += xSpeed_;

    animation.tickUpTo(6, 3);
    image = enemyImage((direction == kFacingRight ? img::kGruntHeadBackRight : img::kGruntHeadBackLeft) +
                       animation.frame);

    if (animation.frame == 3 && platformAt(x, y) != 0) {
        world().media.effects->engrave(x, y, image);
        dead = true;
    }
    dieWhenFallenOut();
}

void GruntTrunk::update() {
    fall();
    x += xSpeed_;

    animation.cycle(3, 8);
    image = enemyImage(img::kGruntTrunk + animation.frame);

    if (platformAt(x, y) != 0) {
        world().media.effects->engrave(x, y, image);
        dead = true;
    }
    dieWhenFallenOut();
}

void RiderBody::update() {
    fall();
    x += xSpeed_;

    animation.tickUpTo(6, 3);

    const int first = direction == kFacingRight ? img::kRiderBodyRight : img::kRiderBodyLeft;
    if (animation.frame < 3) {
        image = enemyImage(first + animation.frame);
    } else if (platformAt(x, y) != 0) {
        if (animation.subFrame == 0) {
            animation.frame += 1;
        }
        if (animation.frame < 5) {
            image = enemyImage(first + animation.frame);
        } else {
            world().media.effects->engrave(x, y, image);
            dead = true;
        }
    }
    dieWhenFallenOut();
}

}  // namespace bb::game::snorks
