// The greedy Smurf (ennemi_smurf_gourmand.cpp) and his cake (tir_gateau.cpp).

#include "smurf/smurf_world.hpp"

namespace bb::game::smurf {

GreedySmurf::GreedySmurf(World& world) : Enemy(world) {
    health.points = 150;
    shotDelay_ = 50 + world.random() % 100;
}

void GreedySmurf::update() {
    if (bleeding != 0) {
        bleeding -= 1;
    }

    switch (mode) {
        case kNormal:
            stand();
            break;
        case kShooting:
            throwCake();
            break;
        case kHolding:
            hold();
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

    if (x < world().camera.x - 20) {
        dead = true;
    }
    dieWhenOffscreen();
}

void GreedySmurf::stand() {
    World& w = world();
    fall();

    // Throw now and then.
    shotWait_ += 1;
    if (shotWait_ > shotDelay_ && x > w.camera.x && x < w.camera.right() && w.target != nullptr) {
        w.media.levelSounds->play(snd::kGreedyThrow + w.random() % 2);  // one of 2
        mode = kShooting;
        shotWait_ = 0;
        shotDelay_ = 100 + w.random() % 250;
        animation.restart();
        aimX_ = w.target->x;
        aimY_ = w.target->y;
        direction = aimX_ > x ? kFacingRight : kFacingLeft;
        throwCake();
    }

    // As in 2002, this image replaces the first throwing one.
    image = enemyImage(direction == kFacingRight ? img::kGreedyThrowRight : img::kGreedyThrowLeft);
    hitboxFromImage();
}

void GreedySmurf::throwCake() {
    animation.tick(5);

    if (animation.frame >= 5) {
        auto* cake = new Cake(world());
        cake->direction = direction;
        cake->x = direction == kFacingLeft ? x - 13 : x + 13;
        cake->y = y - 28;
        lob(cake->x, cake->y, aimX_, aimY_, cake->xSpeed, cake->dy);
        world().enemyShots.add(cake);

        animation.frame = 0;
        mode = kHolding;
        return;
    }

    image = enemyImage((direction == kFacingRight ? img::kGreedyThrowRight : img::kGreedyThrowLeft) +
                       animation.frame);
    hitboxFromImage();
}

void GreedySmurf::hold() {
    animation.frame += 1;
    if (animation.frame >= 50) {
        animation.frame = 0;
        mode = kNormal;
        return;
    }
    image = enemyImage(direction == kFacingRight ? img::kGreedyHoldRight : img::kGreedyHoldLeft);
    hitboxFromImage();
}

void GreedySmurf::die() {
    fall();

    animation.tickUpTo(6, 11);

    // Thrown back by the shot.
    if (direction == kFacingRight) {
        if (!solidWall(x - 2, y)) {
            x -= 2;
        }
        image = enemyImage(img::kGreedyDieRight + animation.frame);
    } else {
        if (!solidWall(x + 2, y)) {
            x += 2;
        }
        image = enemyImage(img::kGreedyDieLeft + animation.frame);
    }

    if (animation.frame >= 11 && platformAt(x, y) != 0) {
        dead = true;
        world().media.effects->engrave(x, y, image);
    }
}

void GreedySmurf::hitBy(Shot& shot) {
    static const int kBloodX[] = {0, 0, 4, 0, 0, 0, -4, 0};
    static const int kBloodY[] = {-15, -15, -15, -15, -15, -15, -15, -15};

    Enemy::hitBy(shot);
    sprayBlood(shot, kBloodX, kBloodY);

    // Faces the shot.
    direction = shot.bloodDirection() < 4 ? kFacingLeft : kFacingRight;

    if (mode == kDying) {
        world().media.levelSounds->play(snd::kGreedyDeath + world().random() % 4);  // one of 4
    }
}

//-----------------------------------------------------------------------------

Cake::Cake(World& world) : Projectile(world) {
    image = enemyImage(img::kCake);
}

void Cake::update() {
    gravityDelay += 1;
    gravityDelay %= kGravityDelay;
    if (gravityDelay == 0 && dy < kMaxFallSpeed) {
        dy += 1;
    }
    y += dy;
    x += xSpeed;

    const int camera = world().camera.x;
    const int cameraRight = world().camera.right();
    if (solidWall(x, y) || x < camera - 100 || x > cameraRight + 60 || y > 520 || y < -50) {
        dead = true;
    }
    hitboxFromImage();
}

}  // namespace bb::game::smurf
