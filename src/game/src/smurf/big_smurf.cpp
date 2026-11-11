// The Big Smurf (ennemi_grand_smurf.cpp, finished) and his gift.

#include <cstdlib>

#include "smurf/smurf_world.hpp"

namespace bb::game::smurf {

namespace {

constexpr int kSpeed = 3;           // walking (2002 dx)
constexpr int kFlurrySteps = 50;    // images of a flurry, 3 ticks each
constexpr int kThrowTicks = 30;     // arms up; the gift leaves at half
constexpr int kThrowPause = 60;     // between two gifts
constexpr int kGiftsPerVolley = 3;  // then he runs at the target
constexpr int kCloseRange = 100;    // a flurry starts closer than this
constexpr int kThrowRange = 200;    // throws from at least this far

}  // namespace

BigSmurf::BigSmurf(World& world) : Enemy(world) {
    health.points = 5000;
    mode = kWaitingOffscreen;
    image = enemyImage(img::kBigSmurfWalkLeft);
}

void BigSmurf::update() {
    World& w = world();

    if (bleeding != 0) {
        bleeding -= 1;
    }

    switch (mode) {
        case kWaitingOffscreen:
            // Until the strongmen are dead, then he walks in.
            if (w.flags[kBossKills] >= 5) {
                mode = kWalking;
                direction = kFacingLeft;
                xSpeed_ = -kSpeed;
            }
            return;

        case kWalking:
        case kNormal:
            updateTarget();
            choose();
            walkAbout();
            break;

        case kFlurry:
            updateTarget();
            flurry();
            break;

        case kThrowing:
            updateTarget();
            throwGift();
            break;

        case kDying:
            die();
            break;

        case kCarbonised:
            burn(img::kSmurfBurnLeft, img::kSmurfBurnRight, 6);
            if (dead) {
                scriptKill();
            }
            break;

        default:
            break;
    }
}

void BigSmurf::updateTarget() {
    if (targetWait_ > 0 && target_ && !target_->dead) {
        targetWait_ -= 1;
        return;
    }
    World& w = world();
    if (w.target != nullptr && !w.target->dead) {
        target_ = w.target;
        targetWait_ = 200 + w.random() % 100;
    } else {
        target_.reset();
    }
}

void BigSmurf::choose() {
    if (!target_) {
        return;
    }
    const int camera = world().camera.x;
    const int cameraRight = world().camera.right();
    const int dx = target_->x - x;
    direction = dx < 0 ? kFacingLeft : kFacingRight;

    // Still entering: walk in first.
    if (x > cameraRight - 60) {
        xSpeed_ = -kSpeed;
        return;
    }

    if (plan_ == Plan::Flurry) {
        if (dx > -kCloseRange && dx < kCloseRange) {
            animation.restart();
            mode = kFlurry;
        } else {
            xSpeed_ = dx < 0 ? -kSpeed : kSpeed;
        }
        return;
    }

    // Gifts: back off to throwing range, then throw; too far away or
    // cornered, he runs at the target instead.
    if (dx < -400 || dx > 400 || x <= camera + 60 || x >= cameraRight - 60) {
        plan_ = Plan::Flurry;
    } else if (std::abs(dx) < kThrowRange) {
        xSpeed_ = dx < 0 ? kSpeed : -kSpeed;
    } else if (throwWait_ > 0) {
        throwWait_ -= 1;
        xSpeed_ = 0;
    } else {
        animation.restart();
        xSpeed_ = 0;
        mode = kThrowing;
    }
}

void BigSmurf::moveBy(int step) {
    const int camera = world().camera.x;
    const int cameraRight = world().camera.right();
    const int next = x + step;
    // Walks freely while entering from the right edge.
    if (!solidWall(next, y) && next > camera + 50 && (next < cameraRight - 40 || step < 0)) {
        x = next;
    }
}

void BigSmurf::walkAbout() {
    fall();
    moveBy(xSpeed_);

    const bool right = direction == kFacingRight;
    if (xSpeed_ == 0) {
        image = enemyImage(right ? img::kBigSmurfWalkRight : img::kBigSmurfWalkLeft);
    } else {
        image = enemyImage((right ? img::kBigSmurfWalkRight : img::kBigSmurfWalkLeft) +
                           animation.loop(kWalkCycle, kWalkSteps, 4));
    }
    hitboxFromImage();
}

void BigSmurf::flurry() {
    fall();

    animation.tick(3);

    if (animation.frame > kFlurrySteps) {
        animation.restart();
        mode = kNormal;
        plan_ = world().random() % 2 == 0 ? Plan::Flurry : Plan::Gifts;
        giftsThrown_ = 0;
        return;
    }

    // A fast shuffle towards the target on the first three walking images.
    const bool right = direction == kFacingRight;
    image = enemyImage((right ? img::kBigSmurfWalkRight : img::kBigSmurfWalkLeft) + animation.frame % 3);
    moveBy(right ? 1 : -1);
    hitboxFromImage();
}

void BigSmurf::throwGift() {
    World& w = world();
    fall();

    animation.frame += 1;
    image = enemyImage(direction == kFacingRight ? img::kBigSmurfJumpRight : img::kBigSmurfJumpLeft);
    hitboxFromImage();

    if (animation.frame == kThrowTicks / 2 && target_) {
        auto* gift = new LobbedGift(w);
        gift->direction = direction;
        gift->x = x;
        gift->y = y - 45;
        lob(gift->x, gift->y, target_->x, target_->y, gift->xSpeed, gift->dy);
        w.enemyShots.add(gift);
        w.media.levelSounds->play(snd::kGreedyThrow + w.random() % 2);
    }

    if (animation.frame >= kThrowTicks) {
        animation.frame = 0;
        mode = kNormal;
        throwWait_ = kThrowPause;
        giftsThrown_ += 1;
        if (giftsThrown_ >= kGiftsPerVolley) {
            plan_ = Plan::Flurry;
            giftsThrown_ = 0;
        }
    }
}

void BigSmurf::die() {
    fall();

    animation.tick(4);
    if (animation.frame > 10) {
        dead = true;
        scriptKill();
        return;
    }
    image = enemyImage((direction == kFacingLeft ? img::kBigSmurfDieLeft : img::kBigSmurfDieRight) +
                       animation.frame);
}

void BigSmurf::scriptKill() {
    if (!counted_) {
        counted_ = true;
        world().flags[kBossKills] += 1;  // the stork can leave
    }
}

void BigSmurf::hitBy(Shot& shot) {
    static const int kBloodX[] = {0, 0, 4, 0, 0, 0, -4, 0};
    static const int kBloodY[] = {-15, -15, -15, -15, -15, -15, -15, -15};

    Enemy::hitBy(shot);
    sprayBlood(shot, kBloodX, kBloodY);

    if (mode == kDying) {
        world().media.levelSounds->play(snd::kSmurfDeath + world().random() % 10);
    }
}

//-----------------------------------------------------------------------------

LobbedGift::LobbedGift(World& world) : Projectile(world) {
    image = enemyImage(img::kGift);
}

void LobbedGift::update() {
    World& w = world();

    if (!exploding_) {
        // Flies like the greedy Smurf's cake.
        gravityDelay += 1;
        gravityDelay %= kGravityDelay;
        if (gravityDelay == 0 && dy < kMaxFallSpeed) {
            dy += 1;
        }
        const int ground = dy > 0 ? platformAt(x, y + dy) : 0;
        if (ground != 0) {
            y = ground;
            exploding_ = true;
            animation.frame = 20;
            animation.subFrame = 0;
            w.media.miscSounds->play(miscsnd::kExplosion);
        } else {
            y += dy;
            x += xSpeed;
            const int camera = w.camera.x;
            const int cameraRight = w.camera.right();
            if (solidWall(x, y) || x < camera - 100 || x > cameraRight + 60 || y > 520) {
                dead = true;
            }
            hitboxFromImage();
            return;
        }
    }

    // Explodes like the prankster's gift: only the first steps hurt.
    animation.tick(6);
    if (animation.frame > 35) {
        dead = true;
        return;
    }
    image = enemyImage(img::kGiftExplosion + animation.frame - 20);
    hitboxFromImage();
    if (animation.frame >= 30) {
        noHitbox();
    }
}

}  // namespace bb::game::smurf
