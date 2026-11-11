// Sonic and the Pac-Man car (ennemi_sonic.cpp, tir_sonic_eclair.cpp,
// morceau_sonic_ring.cpp, ennemi_voiture_pacman.cpp).

#include "videoboss/videoboss_world.hpp"

namespace bb::game::videoboss {

namespace {

constexpr Flag kSonicState = Flag::User2;  // 1 super saiyan, 2 dead, 3 the car landed

constexpr int kRunSpeed = 7;
// Positions are offsets from the arena's left edge, the level's
// "videoboss.arenaX" (1280 in videoboss).
constexpr int kRollLeft = 20, kRollRight = 620;     // rolls up to, then jumps
constexpr int kSuperLeft = 320, kSuperRight = 350;  // where he turns super
constexpr int kCarLanding = 320;                    // the car slows down until
constexpr int kOffstageX = 120;                     // before the car comes
// The lightning strikes more often below these HP (vie_sous_critique...).
constexpr int kLow = 3750;
constexpr int kCritical = 2500;
constexpr int kVeryLow = 1250;

// The ball (the same steps both ways).
const int kBall[] = {0, 1, 4, 2, 3, 4};
// Super saiyan: from the call image, growing.
const int kSuperSaiyan[] = {0, 1, 2, 1, 0, 1, 2, 3, 2, 1, 2, 3, 4, 3, 2, 3, 4, 5, 4, 3, 4, 5,
                            6, 5, 4, 5, 6, 7, 6, 5, 6, 7, 8, 7, 6, 7, 8, 9, 8, 7, 8, 9, 10};

}  // namespace

//-----------------------------------------------------------------------------
// Sonic
//-----------------------------------------------------------------------------

Sonic::Sonic(World& world) : Enemy(world) {
    attackDelay_ = 50 + world.random() % 250;
    health.points = 10000;
    minX = world.marker("videoboss.arenaX");
}

void Sonic::update() {
    World& w = world();

    if (w.flags[kStage] == 7) {
        dead = true;
    }

    // Waits while Luigi's blood falls, then stage 4.
    if (wait_ < 400) {
        wait_ += 1;
        return;
    }
    if (w.flags[kStage] < 4) {
        w.flags[kStage] = 4;
    }

    if (bleeding > 0) {
        bleeding -= 1;
    }

    switch (mode) {
        case kNormal:
        case kWalking:
            run();
            break;
        case kCarbonised:
        case kDying:
            die();
            break;
        case kJumping:
        case kFalling:
            jump();
            break;
        case kShooting:
            // (Type 2, the spine attack, can never be chosen: its choice
            // is commented out in 2002.)
            if (attackType_ == 0) {
                horizontal();
            } else if (attackType_ == 1) {
                vertical();
            } else if (attackType_ == 3) {
                lightning();
            }
            break;
        default:
            break;
    }
}

void Sonic::runStep() {
    animation.cycle(10 - speed_, 8);
    if (direction == kFacingRight) {
        walk(speed_);
        image = enemyImage(img::kSonicRunRight + animation.frame);
    } else {
        walk(-speed_);
        image = enemyImage(img::kSonicRunLeft + animation.frame);
    }
}

void Sonic::run() {
    World& w = world();

    // No ground: falls, and no more this tick (D-54 fixed: 2002 also
    // walked on this tick, a double move where a platform ends).
    if (platformAt(x, y) == 0) {
        mode = kFalling;
        dy = 0;
        gravityDelay = 0;
        jump();
        return;
    }

    if (x - speed_ < minX || solidWall(x - speed_, y)) {
        direction = kFacingRight;
        speed_ = 1;
    } else if (x + speed_ > w.camera.right() || solidWall(x + speed_, y)) {
        direction = kFacingLeft;
        speed_ = 1;
    }

    attackWait_ += 1;
    if (attackWait_ >= attackDelay_ && rings_ == 0 && w.target != nullptr) {
        const Entity& target = *w.target;
        if (target.y > 0 && ((direction == kFacingRight && target.x - x > 0) ||
                             (direction == kFacingLeft && target.x - x < 0))) {
            switch (w.random() % 3) {
                case 0:
                    attackType_ = 0;
                    speed_ = 12;
                    mode = kShooting;
                    goalX_ = target.x;
                    goalY_ = target.y;
                    shots_ = 0;
                    attackPhase_ = -1;
                    animation.restart();
                    horizontal();
                    return;
                case 1:
                    attackPhase_ = -1;
                    attackType_ = 1;
                    speed_ = 12;
                    mode = kShooting;
                    goalX_ = target.x;
                    goalY_ = target.y;
                    shots_ = 0;
                    animation.restart();
                    vertical();
                    return;
                default:
                    attackType_ = 3;
                    animation.restart();
                    shots_ = 0;
                    mode = kShooting;
                    lightning();
                    return;
            }
        }
    }

    // Below 9000 HP, loses his rings: three throws, 30 ticks apart.
    if (rings_ > 0 && health.points < 9000) {
        if (rings_ % 30 == 0) {
            throwRings();
        }
        rings_ -= 1;
    }

    runStep();
    if (speed_ < kRunSpeed) {
        speedStep_ += 1;
        if (speedStep_ >= 15) {
            speedStep_ = 0;
            speed_ += 1;
        }
    }
    hitboxFromImage();
}

void Sonic::die() {
    World& w = world();

    if (w.flags[kSonicState] == 0) {
        // Runs on until he turns super.
        fall();
        if (x - speed_ < minX || solidWall(x - speed_, y)) {
            direction = kFacingRight;
            speed_ = 1;
        } else if (x + speed_ > w.camera.right() || solidWall(x + speed_, y)) {
            direction = kFacingLeft;
            speed_ = 1;
        }
        runStep();
        if (speed_ < kRunSpeed) {
            speedStep_ += 1;
            if (speedStep_ >= 15) {
                speedStep_ = 0;
                speed_ += 1;
            }
        }
        const int arena = w.marker("videoboss.arenaX");
        if (w.collision.platformLayerAt(x, y) == 0 && x > arena + kSuperLeft && x < arena + kSuperRight) {
            w.flags[kSonicState] = 1;
            animation.restart();
            direction = kFacingLeft;
        }
    } else if (w.flags[kSonicState] == 1) {
        // Super saiyan: rises, the screen shakes harder and harder.
        y -= 1;
        animation.subFrame += 1;
        animation.subFrame %= 5;
        if (animation.subFrame == 0) {
            w.shake.start(animation.frame / 5 + 2);
            animation.frame += 1;
            if (animation.frame >= 43) {
                w.flags[kSonicState] = 2;
                dead = true;
            } else {
                image = enemyImage(img::kSonicCallLeft + kSuperSaiyan[animation.frame]);
            }
        }
    }
}

void Sonic::jump() {
    fall();

    const int ground = dy > 0 ? platformAt(x, y + dy) : 0;
    if (ground != 0) {
        mode = kNormal;
        dy = 0;
        y = ground;
        animation.restart();
        run();
        return;
    }

    // Not out of the screen.
    if (direction == kFacingLeft && (x - speed_ < minX || solidWall(x - speed_, y))) {
        direction = kFacingRight;
        speed_ = 1;
    } else if (direction == kFacingRight &&
               (x + speed_ > world().camera.right() || solidWall(x + speed_, y))) {
        direction = kFacingLeft;
        speed_ = 1;
    }

    runStep();
    hitboxFromImage();
}

void Sonic::tickSpines(int belowHp) {
    if (health.points < belowHp) {
        spineStep_ += 1;
        spineStep_ %= 2;
        if (spineStep_ == 0) {
            spineFrame_ += 1;
            spineFrame_ %= 16;
        }
    }
}

void Sonic::backToRun() {
    mode = kWalking;
    attackWait_ = 0;
    attackDelay_ = 50 + world().random() % 250;
    animation.restart();
    run();
}

void Sonic::horizontal() {
    World& w = world();

    if (attackPhase_ == -1) {
        // Crouches for 50 ticks.
        tickSpines(kLow);
        shots_ += 1;
        if (shots_ == 50) {
            attackPhase_ = 0;
        } else {
            animation.cycle(2, 5);
            image = enemyImage((direction == kFacingRight ? img::kSonicCrouchRight : img::kSonicCrouchLeft) +
                               animation.frame);
        }
        hitboxFromImage();
        return;
    }

    if (attackPhase_ == 0) {
        // Rolls to an end of the arena, then jumps.
        tickSpines(kCritical);
        fall();
        const int arena = w.marker("videoboss.arenaX");
        if (direction == kFacingRight && x + speed_ > arena + kRollRight) {
            speed_ = 0;
            dy = -12;
            attackPhase_ = 1;
        }
        if (direction == kFacingLeft && x - speed_ < arena + kRollLeft) {
            speed_ = 0;
            dy = -12;
            attackPhase_ = 1;
        }
    } else if (attackPhase_ == 1) {
        // Up to the target's height, then rolls back across.
        fall();
        tickSpines(kVeryLow);
        if (y + dy < goalY_) {
            attackPhase_ = 2;
            direction = direction == kFacingRight ? kFacingLeft : kFacingRight;
            dy = 0;
            speed_ = 12;
        }
    } else if (attackPhase_ == 2) {
        if (x - speed_ < minX || solidWall(x - speed_, y)) {
            direction = kFacingRight;
            speed_ = 1;
            backToRun();
            return;
        }
        if (x + speed_ > w.camera.right() || solidWall(x + speed_, y)) {
            direction = kFacingLeft;
            speed_ = 1;
            backToRun();
            return;
        }
    }

    if (direction == kFacingRight) {
        x += speed_;
        image = enemyImage(img::kSonicBallRight + animation.loop(kBall, 6, 3));
    } else {
        x -= speed_;
        image = enemyImage(img::kSonicBallLeft + animation.loop(kBall, 6, 3));
    }
    hitboxFromImage();
}

void Sonic::vertical() {
    World& w = world();

    if (attackPhase_ == -1) {
        tickSpines(kLow);
        shots_ += 1;
        if (shots_ == 50) {
            attackPhase_ = 0;
        } else {
            animation.cycle(2, 5);
            image = enemyImage((direction == kFacingRight ? img::kSonicCrouchRight : img::kSonicCrouchLeft) +
                               animation.frame);
        }
        hitboxFromImage();
        return;
    }

    if (attackPhase_ == 0) {
        fall();
        tickSpines(kCritical);
        const int arena = w.marker("videoboss.arenaX");
        if ((direction == kFacingRight && x + speed_ > arena + kRollRight) ||
            (direction == kFacingLeft && x - speed_ < arena + kRollLeft)) {
            speed_ = 0;
            dy = -12;
            attackPhase_ = 1;
        }
    } else if (attackPhase_ == 1) {
        // Up to y 40, then across as fast as he rose.
        fall();
        tickSpines(kVeryLow);
        if (y + dy < 40) {
            attackPhase_ = 2;
            direction = direction == kFacingRight ? kFacingLeft : kFacingRight;
            speed_ = -dy;
            dy = 0;
        }
    } else if (attackPhase_ == 2) {
        // Over the target's old position: drops.
        if ((direction == kFacingRight && x > goalX_) || (direction == kFacingLeft && x < goalX_)) {
            attackPhase_ = 3;
            dy = 12;
            speed_ = 0;
        }
    } else if (attackPhase_ == 3) {
        // The floor under him (D-85 fixed: 2002 read it at column y).
        const int floor = w.collision.platforms[0][x];
        if (y + dy > floor) {
            fall();
            y = floor;
            dy = 0;
            animation.restart();
            speed_ = 1;
            backToRun();
            return;
        }
        y += dy;
    }

    if (direction == kFacingRight) {
        x += speed_;
        image = enemyImage(img::kSonicBallRight + animation.loop(kBall, 6, 3));
    } else {
        x -= speed_;
        image = enemyImage(img::kSonicBallLeft + animation.loop(kBall, 6, 3));
    }
    hitboxFromImage();
}

void Sonic::lightning() {
    World& w = world();

    animation.tick(5);
    if (animation.frame == 23) {
        speed_ = 1;
        backToRun();
        return;
    }

    // More bolts as his HP drop: the call starts again.
    if (animation.frame == 5 && animation.subFrame == 0) {
        if ((shots_ <= 1 && health.points < kLow) || (shots_ <= 2 && health.points < kCritical) ||
            (shots_ <= 3 && health.points < kVeryLow)) {
            animation.frame = 0;
            animation.subFrame = 1;
        }
    }

    if (animation.frame == 0 && animation.subFrame == 1 && w.target != nullptr) {
        auto* bolt = new SonicLightning(w);
        bolt->direction = direction;
        bolt->x = w.target->x;
        bolt->y = 0;
        w.enemyShots.add(bolt);
        shots_ += 1;
    }

    y -= 1;
    image = enemyImage((direction == kFacingRight ? img::kSonicCallRight : img::kSonicCallLeft) +
                       animation.subFrame);
    hitboxFromImage();
}

void Sonic::throwRings() {
    // 8 rings into each quarter.
    static const int kSpeeds[8][2] = {{1, 10}, {3, 9}, {4, 7}, {5, 6}, {6, 5}, {7, 4}, {9, 3}, {10, 1}};
    static const int kQuarters[4][2] = {{1, 1}, {-1, 1}, {-1, -1}, {1, -1}};
    World& w = world();

    for (const auto& quarter : kQuarters) {
        for (const auto& speed : kSpeeds) {
            auto* ring = new SonicRing(w, speed[0] * quarter[0], speed[1] * quarter[1]);
            ring->x = x;
            ring->y = y - 40;
            w.blood.add(ring);
        }
    }
}

void Sonic::hitBy(Shot& shot) {
    static const int kBloodX[] = {0, 10, 15, 10, 0, -10, -15, -10};
    static const int kBloodY[] = {-15, -25, -25, -25, -35, -25, -25, -25};

    Enemy::hitBy(shot);
    sprayBlood(shot, kBloodX, kBloodY);
}

//-----------------------------------------------------------------------------
// The lightning and the rings
//-----------------------------------------------------------------------------

void SonicLightning::update() {
    animation.tick(3);
    image = enemyImage(img::kSonicLightning + animation.subFrame);

    if (animation.frame > 30) {
        hitboxFromImage();
        if (animation.frame > 36) {
            dead = true;
        }
    }
    if (animation.frame == 0 || animation.frame > 30) {
        world().shake.start(7);
    }
}

void SonicLightning::draw() {
    if (animation.frame == 0 || animation.frame > 30) {
        Actor::draw();
    }
}

void SonicRing::update() {
    x += xSpeed_;
    fallThrough();

    if (platformAt(x + xSpeed_, y + dy) != 0) {
        if (dy > 2) {
            dy = -dy + 1;
        } else if (dy == 2 || dy == 1) {
            dy = -2;
        }
    }

    animation.cycle(4, 4);
    image = enemyImage(img::kSonicRing + animation.frame);
    dieWhenOffscreen();
}

//-----------------------------------------------------------------------------
// The Pac-Man car
//-----------------------------------------------------------------------------

namespace {

constexpr int kCarGravityDelay = 5;
constexpr int kCarMaxFallSpeed = 8;

}  // namespace

PacManCar::PacManCar(World& world) : Enemy(world) {
    health.points = 1000;
    image = enemyImage(img::kPacmanCar);
}

void PacManCar::drop() {
    gravityDelay += 1;
    gravityDelay %= kCarGravityDelay;

    if (gravityDelay == 0 && dy < kCarMaxFallSpeed) {
        dy += 1;
    }
    if (dy < 0 && solidWall(x, y + dy)) {
        dy = kMaxFallSpeed;
    }
    const int ground = platformAt(x, y + dy);
    if (ground != 0 && dy > 0 && platformAt(x, y) == 0) {
        y = ground;
    } else {
        y += dy;
    }
}

void PacManCar::update() {
    World& w = world();
    const int state = w.flags[kSonicState];

    if (state == 1 || state == 2) {
        // After 160 ticks, drives in (slowing down) and lands on layer 1
        animation.frame += 1;
        if (animation.frame > 160) {
            drop();
            const int landing = w.marker("videoboss.arenaX") + kCarLanding;
            if (x < landing - 250) {
                x += 5;
            } else if (x < landing - 150) {
                x += 4;
            } else if (x < landing - 50) {
                x += 3;
            } else if (x < landing) {
                x += 2;
            } else if (w.collision.platformLayerAt(x, y + dy) == 1) {
                y = platformAt(x, y + dy);
                w.flags[kSonicState] = 3;
                animation.restart();
            }
        }
    } else if (state == 3) {
        endWait_ += 1;
        if (endWait_ >= 250 && w.flags[kStage] < 6) {
            w.flags[kStage] = 6;
        }
    } else {
        x = w.marker("videoboss.arenaX") + kOffstageX;
        y = -10;
    }
}

void PacManCar::draw() {
    // Pac-Man getting out (register 2 >= 4: the steps that set it are
    // commented out in 2002, so never drawn).
    if (world().flags[kSonicState] >= 4) {
        drawAt(x + 48, y - 82, enemyImage(img::kPacmanCar + 1 + animation.frame));
    }
    Actor::draw();
}

}  // namespace bb::game::videoboss
