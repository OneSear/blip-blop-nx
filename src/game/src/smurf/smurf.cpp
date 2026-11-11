// The basic Smurf and its skins (ennemi_smurf.cpp, ennemi_smurf_a_lunettes.h,
// ennemi_smurf_coquet.h, ennemi_smurf_frileux.h, ennemi_smurf_paresseux.h,
// ennemi_smurf_peon.h).

#include "smurf/smurf_world.hpp"

namespace bb::game::smurf {

Smurf::Smurf(World& world, Skin skin) : Enemy(world), skin_(skin) {
    health.points = 100;
    jumpDelay_ = 50 + world.random() % 250;

    switch (skin_) {
        case Skin::Vain:
            jumpSpeed_ = -6;
            speed_ = 1 + world.random() % 2;
            break;
        case Skin::Chilly:
            health.points = 75;
            break;
        case Skin::Lazy:
            jumpSpeed_ = -4;
            break;
        case Skin::Peon:
            health.points = 125;
            break;
        case Skin::Plain:
        case Skin::Brainy:
            break;
    }
}

void Smurf::update() {
    if (skin_ == Skin::Brainy) {
        speed_ = mode == kJumping ? 2 : 1;
    }
    if (bleeding != 0) {
        bleeding -= 1;
    }

    switch (mode) {
        case kWalking:
        case kNormal:
            walkAbout();
            break;
        case kFalling:
        case kJumping:
            jump();
            break;
        case kDying:
            die();
            break;
        case kCarbonised:
            burn(img::kSmurfBurnLeft, img::kSmurfBurnRight, 6);
            break;
        default:
            break;
    }
    dieWhenOffscreen();
}

void Smurf::walkAbout() {
    if (skin_ == Skin::Chilly) {
        speed_ = 1 + world().random() % 3;
    }

    // No ground: fall, and no more this tick (D-54 fixed: 2002 also walked
    // on this tick, a double move where a platform ends).
    if (platformAt(x, y) == 0) {
        mode = kFalling;
        dy = 0;
        gravityDelay = 0;
        jump();
        return;
    }

    // Jump now and then.
    jumpWait_ += 1;
    if (jumpWait_ > jumpDelay_) {
        mode = kJumping;
        dy = jumpSpeed_;
        gravityDelay = 0;  // or the jumps would differ in height
        jumpWait_ = 0;
        jumpDelay_ = 50 + world().random() % 250;
        jump();
        return;  // (the same double move in 2002, D-54)
    }

    // Walk.
    if (x - speed_ < minX || solidWall(x - speed_, y)) {
        direction = kFacingRight;
    } else if (x + speed_ > world().camera.right() || solidWall(x + speed_, y)) {
        direction = kFacingLeft;
    }
    if (direction == kFacingRight) {
        walk(speed_);
        image = enemyImage(img::kSmurfWalkRight + animation.loop(kWalkCycle, kWalkSteps, 5));
    } else {
        walk(-speed_);
        image = enemyImage(img::kSmurfWalkLeft + animation.loop(kWalkCycle, kWalkSteps, 5));
    }
    hitboxFromImage();
}

void Smurf::jump() {
    fall();

    // Landed?
    const int ground = dy > 0 ? platformAt(x, y + dy) : 0;
    if (ground != 0) {
        mode = kNormal;
        dy = 0;
        y = ground;
        walkAbout();
        return;
    }

    if (direction == kFacingRight) {
        x += speed_;
        image = enemyImage(img::kSmurfJumpRight);
    } else {
        x -= speed_;
        image = enemyImage(img::kSmurfJumpLeft);
    }
    hitboxFromImage();
}

void Smurf::die() {
    fall();

    animation.tick(4);
    if (animation.frame > 10) {
        dead = true;
        return;
    }

    if (direction == kFacingLeft) {
        if (!solidWall(x - speed_, y) && platformAt(x, y) == 0) {
            x -= speed_;
        }
        image = enemyImage(img::kSmurfDieLeft + animation.frame);
    } else {
        if (!solidWall(x + speed_, y) && platformAt(x, y) == 0) {
            x += speed_;
        }
        image = enemyImage(img::kSmurfDieRight + animation.frame);
    }
}

void Smurf::hitBy(Shot& shot) {
    static const int kBloodX[] = {0, 0, 4, 0, 0, 0, -4, 0};
    static const int kBloodY[] = {-15, -15, -15, -15, -15, -15, -15, -15};

    Enemy::hitBy(shot);
    sprayBlood(shot, kBloodX, kBloodY);

    if (mode == kDying) {
        world().media.levelSounds->play(snd::kSmurfDeath + world().random() % 10);  // one of 10
    }
}

void Smurf::draw() {
    const bool alive = mode != kDying && mode != kCarbonised;
    const bool left = direction == kFacingLeft;

    switch (skin_) {
        case Skin::Plain:
        case Skin::Lazy:
            Actor::draw();
            break;

        case Skin::Brainy:
            Actor::draw();
            if (alive) {
                if (left) {
                    drawAt(x - 4, y - 27, enemyImage(img::kBrainyGlassesLeft));
                } else {
                    drawAt(x + 4, y - 27, enemyImage(img::kBrainyGlassesRight));
                }
            }
            break;

        case Skin::Vain:
            Actor::draw();
            if (alive) {
                drawAt(left ? x + 3 : x - 3, y - 31, enemyImage(img::kVainFlower));
            }
            break;

        case Skin::Chilly:
            // The scarf behind, the hat in front.
            if (alive) {
                if (left) {
                    drawAt(x + 7, y - 18, enemyImage(img::kChillyScarfLeft));
                    Actor::draw();
                    drawAt(x, y - 20, enemyImage(img::kChillyHatLeft));
                } else {
                    drawAt(x - 7, y - 18, enemyImage(img::kChillyScarfRight));
                    Actor::draw();
                    drawAt(x, y - 20, enemyImage(img::kChillyHatRight));
                }
            } else {
                Actor::draw();
            }
            break;

        case Skin::Peon:
            Actor::draw();
            // The helmet stays on for the first steps of the death.
            if (alive || (mode == kDying && animation.frame <= 2)) {
                drawAt(x, y - 32, enemyImage(left ? img::kPeonHelmetLeft : img::kPeonHelmetRight));
            }
            break;
    }
}

}  // namespace bb::game::smurf
