// The wild Smurf (ennemi_smurf_sauvage.cpp) and his shot
// (tir_smurf_sauvage.cpp).

#include <cstdlib>
#include <limits>

#include "smurf/smurf_world.hpp"

namespace bb::game::smurf {

namespace {

// Where the shots start and how fast they go, by aim direction.
const int kShotStartX[] = {-15, -8, 6, 1, 8, 17};
const int kShotStartY[] = {55, 62, 65, 65, 62, 55};
const int kShotSpeedX[] = {-4, -1, 0, 0, 1, 4};
const int kShotSpeedY[] = {4, 5, 6, 6, 5, 4};

}  // namespace

WildSmurf::WildSmurf(World& world) : Enemy(world) {
    health.points = 150;
    animation.frame = 1;
    target_ = world.target;

    hideDelay_ = 100 + world.random() % 100;
    hideTicks_ = 0;
    mode = kHidden;

    // The 2002 constructor tested the direction before the level event set
    // it: it always took the facing-left branch. No effect: events run
    // before the enemies update (Level::tick), and every update chooses the
    // image from the target before the first draw.
    firstImage_ = img::kWildAimLeft;
    image = enemyImage(img::kWildAimLeft + 1);
    aim_ = DownLeft;
}

void WildSmurf::update() {
    World& w = world();

    // Not beyond the level's limit (snuf1's marker; the 2002 code held
    // 5850 in every level, D-78).
    const int maxX = w.marker("smurf.wildMaxX", 0, std::numeric_limits<int>::max());
    if (x > maxX) {
        x = maxX;
    }

    shooting_ = false;

    if (firstUpdate_) {
        y = -100;
        firstUpdate_ = false;
    }

    if (!target_ || target_->dead) {
        target_ = w.target;
    }

    if (mode != kDying) {
        if (target_ && x != target_->x) {
            slope_ = (std::abs(target_->y - y) * 10) / (x - target_->x);
        } else {
            slope_ = 3;
        }
    }

    if (slope_ > 0) {
        direction = kFacingLeft;
        firstImage_ = img::kWildAimLeft;
        if (slope_ <= 15) {
            aim_ = DiagonalLeft;
        } else if (slope_ <= 25) {
            aim_ = SteepLeft;
        } else {
            aim_ = DownLeft;
        }
    } else if (slope_ < 0) {
        direction = kFacingRight;
        firstImage_ = img::kWildAimRight;
        if (slope_ >= -15) {
            aim_ = DiagonalRight;
        } else if (slope_ >= -25) {
            aim_ = SteepRight;
        } else {
            aim_ = DownRight;
        }
    }

    if (mode != kDying) {
        image = enemyImage(firstImage_ + animation.frame);
    }

    if (bleeding != 0) {
        bleeding -= 1;
    }

    // (The 2002 code also had a branch for negative hit points, which
    // Enemy::hitBy never leaves.)

    switch (mode) {
        case kHidden:
            hideTicks_ += 1;
            if (hideTicks_ >= hideDelay_) {
                mode = kFalling;
                w.media.levelSounds->play(snd::kWildComesDown);

                // A random place around the screen centre, below a ceiling.
                if (w.random() % 2 == 0) {
                    x = w.camera.centre() - w.random() % 150;
                } else {
                    x = w.camera.centre() + w.random() % 150;
                }
                int end = 100 + w.random() % 140;
                while (solidWall(x, end)) {
                    end = 100 + w.random() % 140;
                }
                shots_ = 0;
                ropeEnd_ = end;
            }
            break;

        case kFalling:
            comeDown();
            break;

        case kShooting:
            shoot();
            break;

        case kClimbing:
            hideTicks_ = 0;
            if (y <= -100) {
                hideDelay_ = 100 + w.random() % 100;
                mode = kHidden;
                dead = true;
                target_.reset();
            } else {
                y -= 2;
            }
            hitboxFromImage();
            break;

        case kDying:
            die();
            break;

        case kCarbonised:
            burn();
            break;

        default:
            break;
    }
    dieWhenOffscreen();
}

void WildSmurf::comeDown() {
    if (y < ropeEnd_) {
        y += 3;
    } else {
        mode = kShooting;
    }
    hideTicks_ = 0;
    hitboxFromImage();
}

void WildSmurf::shoot() {
    shooting_ = true;

    animation.tick(4);
    if (animation.frame > 2) {
        animation.frame = 1;
    }

    shotStep_ += 1;
    shotStep_ %= 10;
    if (shotStep_ == 0) {
        auto* shot = new WildSmurfShot(world());
        shot->x = x + kShotStartX[aim_];
        shot->y = y + kShotStartY[aim_];
        shot->xSpeed = kShotSpeedX[aim_];
        shot->ySpeed = kShotSpeedY[aim_];
        world().enemyShots.add(shot);

        if (shots_ == 0) {
            world().media.levelSounds->play(snd::kWildShoots);
        }
        shots_ += 1;
    }

    if (shots_ >= 5) {
        mode = kClimbing;
    }
    hitboxFromImage();
}

void WildSmurf::hitBy(Shot& shot) {
    // No blood spray (commented out in 2002).
    Enemy::hitBy(shot);

    if (mode == kDying) {
        world().media.levelSounds->play(snd::kSmurfDeath + world().random() % 10);
    }
}

void WildSmurf::die() {
    if (animation.frame < 5) {
        if (animation.tick(4)) {
            // (frame is 1 to 5 here)
            image = enemyImage((direction == kFacingRight ? img::kWildDieRight : img::kWildDieLeft) +
                               animation.frame - 1);
        }
        return;
    }

    // Engraved in the decor with his rope, unless he hangs over a
    // midground decal (hitbox stretched to the top of the screen).
    hitboxFromImage();
    hitbox.y1 = 0;

    bool clear = true;
    world().midground.forEach([&](const Entity* decal) { clear = clear && !hits(*decal); });

    hitbox.active = false;

    if (clear) {
        LevelEffects& effects = *world().media.effects;
        for (int rope = 1; rope <= 5; ++rope) {
            effects.engrave(x, y - 50 * rope, enemyImage(img::kWildRope));
        }
        effects.engrave(x, y, image);
        dead = true;
    }
}

void WildSmurf::burn() {
    y = ropeEnd_ + 50 - animation.frame;
    Enemy::burn(img::kSmurfBurnLeft, img::kSmurfBurnRight, 6);
}

void WildSmurf::draw() {
    for (int rope = 1; rope <= 5; ++rope) {
        drawAt(x, y - 50 * rope, enemyImage(img::kWildRope));
    }

    Actor::draw();

    if (health.points <= 0) {
        return;
    }

    // The gun: animated while shooting.
    static const int kShootingGun[] = {img::kWildGunFireDiagonalLeft, img::kWildGunFireSteepLeft,
                                       img::kWildGunFireDownLeft,     img::kWildGunFireDownRight,
                                       img::kWildGunFireSteepRight,   img::kWildGunFireDiagonalRight};
    static const int kIdleGun[] = {img::kWildGunDiagonalLeft, img::kWildGunSteepLeft,
                                   img::kWildGunDownLeft,     img::kWildGunDownRight,
                                   img::kWildGunSteepRight,   img::kWildGunDiagonalRight};
    const int gunX = aim_ <= DownLeft ? x + 8 : x - 5;
    if (shooting_) {
        drawAt(gunX, y + 38, enemyImage(kShootingGun[aim_] + animation.frame));
    } else {
        drawAt(gunX, y + 38, enemyImage(kIdleGun[aim_]));
    }
}

//-----------------------------------------------------------------------------

WildSmurfShot::WildSmurfShot(World& world) : Projectile(world) {
    image = enemyImage(img::kWildShot);
}

void WildSmurfShot::update() {
    x += xSpeed;
    y += ySpeed;

    const int camera = world().camera.x;
    const int cameraRight = world().camera.right();
    if (solidWall(x, y) || x < camera - 20 || x > cameraRight + 20 || y < -20 || y > 500) {
        image = nullptr;
        noHitbox();
        dead = true;
    } else {
        hitboxFromImage();
    }
}

}  // namespace bb::game::smurf
