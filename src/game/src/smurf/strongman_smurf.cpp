// The strongman Smurf (ennemi_smurf_costaud.cpp) and his fireball
// (tir_hadoken.h).

#include "bb/assets/music/snuf.hpp"
#include "smurf/smurf_world.hpp"

namespace bb::game::smurf {

StrongmanSmurf::StrongmanSmurf(World& world) : Enemy(world) {
    attackDelay_ = 50 + world.random() % 50;
    health.points = 2000;
}

void StrongmanSmurf::update() {
    World& w = world();

    if (bleeding > 0) {
        bleeding -= 1;
    }

    // The fight starts when the level script sets it.
    if (w.flags[kFightStage] != 0) {
        attackWait_ += 1;

        if (w.flags[kFightMusic] == 0) {
            w.media.levelMusic->play(bb::assets::music::snuf::kBoss);
            w.flags[kFightMusic] = 1;
            w.flags[Flag::Timer] = 40;
        }
    }

    switch (mode) {
        case kNormal:
        case kWalking:
            walkAbout();
            break;
        case kDying:
            die();
            break;
        case kFireball:
            fireball();
            break;
        case kUppercut:
            uppercut();
            break;
        case kSpinKick:
            spinKick();
            break;
        case kCarbonised:
            burn(img::kStrongmanBurnLeft, img::kStrongmanBurnRight, 6);
            break;
        default:
            break;
    }
    dieWhenOffscreen();
}

void StrongmanSmurf::walkAbout() {
    static const int kWalk[] = {0, 1, 2, 3, 2, 1};
    static const int kSpeed = 1;

    fall();
    xSpeed_ = 0;

    if (x - kSpeed < minX || solidWall(x - kSpeed, y)) {
        direction = kFacingRight;
    } else if (x + kSpeed > world().camera.right() || solidWall(x + kSpeed, y)) {
        direction = kFacingLeft;
    }

    if (direction == kFacingRight) {
        walk(kSpeed);
        image = enemyImage(img::kStrongmanWalkRight + animation.loop(kWalk, 6, 6));
    } else {
        walk(-kSpeed);
        image = enemyImage(img::kStrongmanWalkLeft + animation.loop(kWalk, 6, 6));
    }

    if (attackWait_ >= attackDelay_ && platformAt(x, y) != 0) {
        attack();
        return;
    }
    hitboxFromImage();
}

void StrongmanSmurf::hitBy(Shot& shot) {
    static const int kBloodX[] = {0, 0, 4, 0, 0, 0, -4, 0};
    static const int kBloodY[] = {-15, -15, -15, -15, -15, -15, -15, -15};

    Enemy::hitBy(shot);
    sprayBlood(shot, kBloodX, kBloodY);

    if (health.points <= 0) {
        world().flags[kBossKills] += 1;
    }
}

void StrongmanSmurf::die() {
    fall();

    // Thrown on by a spin kick while in the air.
    const int nextX = platformAt(x, y) == 0 ? x + xSpeed_ : x;
    if (!solidWall(nextX, y)) {
        x = nextX;
    }

    if (animation.frame < 13) {
        animation.tick(3);
    }

    image = enemyImage((direction == kFacingRight ? img::kStrongmanDieRight : img::kStrongmanDieLeft) +
                       animation.frame);
    // At the last step, on the ground, the corpse becomes part of the decor
    // (D-76 fixed: the 2002 test waited for a step 14 that never came).
    if (animation.frame >= 13 && platformAt(x, y) != 0) {
        dead = true;
        world().media.effects->engrave(x, y, image);
    }
}

void StrongmanSmurf::fireball() {
    if (animation.tick(6)) {
        if (animation.frame == 4) {
            auto* ball = new Fireball(world());
            ball->x = x;
            ball->y = y - 30;
            ball->direction = direction;
            world().enemyShots.add(ball);
        }

        if (animation.frame > 7) {
            animation.restart();
            mode = kWalking;
            attackDelay_ = 100 + world().random() % 50;
            attackWait_ = 0;
            walkAbout();
            return;
        }
    }

    if (animation.frame <= 4) {
        image = enemyImage(
            (direction == kFacingRight ? img::kStrongmanFireballRight : img::kStrongmanFireballLeft) +
            animation.frame);
    }
}

void StrongmanSmurf::uppercut() {
    const int cameraRight = world().camera.right();
    fall();

    if (animation.frame <= 2) {
        // Take-off.
        if (animation.tick(4)) {
            if (animation.frame > 2) {
                dy = uppercutSpeed_;
            }
        }
    } else if (dy < 0) {
        // Going up.
        if (direction == kFacingRight && x < cameraRight && !solidWall(x + 1, y)) {
            x += 1;
        } else if (direction == kFacingLeft && x > minX && !solidWall(x - 1, y)) {
            x -= 1;
        }
    } else {
        // Coming down.
        if (animation.frame < 6) {
            animation.tick(12);
        }
        if (platformAt(x, y) != 0) {
            animation.restart();
            attackDelay_ = 100 + world().random() % 50;
            attackWait_ = 0;
            mode = kNormal;
            walkAbout();
            return;
        }
    }

    image =
        enemyImage((direction == kFacingRight ? img::kStrongmanUppercutRight : img::kStrongmanUppercutLeft) +
                   animation.frame);
    hitboxFromImage();
}

void StrongmanSmurf::spinKick() {
    fall();

    animation.cycle(4, 6);

    const int nextX = x + xSpeed_;
    if (!solidWall(nextX, y) && nextX > minX && nextX < world().camera.right() + 100) {
        x = nextX;
    }

    if (dy > 0 && platformAt(x, y) != 0) {
        animation.restart();
        attackDelay_ = 100 + world().random() % 50;
        attackWait_ = 0;
        mode = kNormal;
        walkAbout();
        return;
    }

    image = enemyImage(img::kStrongmanSpin + animation.frame);
    hitboxFromImage();
}

void StrongmanSmurf::attack() {
    World& w = world();
    if (w.target == nullptr) {
        return;
    }

    animation.restart();

    const int targetX = w.target->x;
    const int targetY = w.target->y;
    const int dx = targetX - x;
    const int dyTarget = targetY - y;

    direction = dx > 0 ? kFacingRight : kFacingLeft;

    if (dyTarget > -50 && dyTarget < 50 && x > w.camera.x && x < w.camera.right()) {
        // Level with the target, on screen: fireball.
        mode = kFireball;
        w.media.levelSounds->play(snd::kHadoken);
        fireball();
    } else if (dx < 150 && dx > -150 && dyTarget < 0) {
        // Close below the target: uppercut.
        mode = kUppercut;
        uppercutSpeed_ = dyTarget / 10;
        if (uppercutSpeed_ < -10) {
            uppercutSpeed_ = -10;
        }
        w.media.levelSounds->play(snd::kUppercut);
        uppercut();
    } else {
        // Otherwise a spinning jump that lands on the target.
        lob(x, y - 5, targetX, targetY, xSpeed_, dy);
        mode = kSpinKick;
        spinKick();
    }
}

//-----------------------------------------------------------------------------

void Fireball::update() {
    static const int kSpeed = 8;

    animation.cycle(2, 3);

    // Blinks: no image every third step.
    if (animation.frame == 2) {
        image = nullptr;
    } else if (direction == kFacingRight) {
        x += kSpeed;
        image = enemyImage(img::kFireballRight + animation.frame);
    } else {
        x -= kSpeed;
        image = enemyImage(img::kFireballLeft + animation.frame);
    }

    if (solidWall(x, y)) {
        dead = true;
    }
    dieWhenOffscreen();
    hitboxFromImage();
}

}  // namespace bb::game::smurf
