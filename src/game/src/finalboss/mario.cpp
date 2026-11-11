// Mario (ennemi_mario.cpp) and his fireballs (tir_mario_fireball.cpp,
// tir_mario_fireball_vertical.cpp).

#include "finalboss/finalboss_world.hpp"

namespace bb::game::finalboss {

namespace {

constexpr Flag kFightState = Flag::User0;  // 0 the dialogue, then fighting, 7 won

constexpr int kTopSpeed = 5;
constexpr int kJumpGravityDelay = 12;
// Below these HP he picks from the next set of attacks.
constexpr int kBelowFirstHp = 17500;  // MARIO_PV_SOUS_CRITIQUE
constexpr int kCriticalHp = 11500;
constexpr int kLastHp = 8500;  // MARIO_PV_SUR_CRITIQUE

const int kWalk[] = {0, 1, 2, 1};
// The push back of a throw.
const int kRecoil[] = {4, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};

}  // namespace

Mario::Mario(World& world) : Enemy(world) {
    attackDelay_ = 50 + world.random() % 50;
    health.points = 50000;
    image = enemyImage(img::kMarioWalkLeft);
}

void Mario::update() {
    World& w = world();
    minX = w.camera.x + 40;

    // Waits for the dialogue.
    if (w.flags[kFightState] == 0) {
        fall();
        return;
    }

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
            w.flags[kFightState] = 7;  // the victory; he stays
            break;
        case kJumping:
        case kFalling:
            jump();
            break;
        case kShooting:
            switch (attackType_) {
                case 0:
                    throwAcross();
                    break;
                case 1:
                    throwUp();
                    break;
                case 2:
                    burstAcross();
                    break;
                case 3:
                    burstUp();
                    break;
                case 4:
                    burstUpBack();
                    break;
                case 5:
                    burstAcrossTwice();
                    break;
                case 6:
                    burstUpTwice();
                    break;
                case 7:
                    cross();
                    break;
                case 8:
                    throwsAcross();
                    break;
                case 9:
                    throwsUp();
                    break;
                case 10:
                    crosses();
                    break;
                case 11:
                    fireRain();
                    break;
                case 12:
                    throwsUpAimed();
                    break;
                case 13:
                    throwsAcrossAimed();
                    break;
                case 14:
                    crossesAimed();
                    break;
                case 15:
                    berserk();
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

void Mario::walkAbout() {
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
    } else if (x + speed_ > w.camera.right() - 40 || solidWall(x + speed_, y)) {
        direction = kFacingLeft;
        speed_ = 1;
    }

    attackWait_ += 1;
    if (attackWait_ >= attackDelay_ && w.target != nullptr &&
        ((direction == kFacingRight && w.target->x > x) || (direction == kFacingLeft && w.target->x < x))) {
        startAttack();
        return;
    }

    if (direction == kFacingRight) {
        walk(speed_);
        image = enemyImage(img::kMarioWalkRight + animation.loop(kWalk, 4, 16 - 3 * speed_));
    } else {
        walk(-speed_);
        image = enemyImage(img::kMarioWalkLeft + animation.loop(kWalk, 4, 16 - 3 * speed_));
    }
    if (speed_ < kTopSpeed) {
        speedStep_ += 1;
        if (speedStep_ >= 20) {
            speedStep_ = 0;
            speed_ += 1;
        }
    }
    hitboxFromImage();
}

void Mario::startAttack() {
    World& w = world();

    animation.restart();
    speed_ = 0;
    dy = -6;
    attackWait_ = 0;
    mode = kShooting;
    goalY_ = w.target->y;
    goalX_ = w.target->x;
    attackStep_ = 0;
    attackSub_ = 0;
    attackPhase_ = 0;

    // Four sets of attacks, more and faster as his HP drop.
    if (health.points < kLastHp) {
        attackType_ = w.random() % 5 + 11;
        attackDelay_ = 30 + w.random() % 30;
    } else if (health.points < kCriticalHp) {
        attackType_ = w.random() % 6 + 5;
        attackDelay_ = 50 + w.random() % 50;
    } else if (health.points < kBelowFirstHp) {
        attackType_ = w.random() % 5 + 2;
        attackDelay_ = 100 + w.random() % 50;
    } else {
        attackType_ = w.random() % 5;
        attackDelay_ = 150 + w.random() % 50;
    }

    switch (attackType_) {
        case 0:
            fireball_ = true;
            throwAcross();
            break;
        case 1:
            fireball_ = true;
            throwUp();
            break;
        case 2:
            fireball_ = true;
            burstAcross();
            break;
        case 3:
            fireball_ = true;
            burstUp();
            break;
        case 4:
            // From the far edge of the screen back to him.
            if (direction == kFacingRight) {
                attackStep_ = 1 + (w.camera.right() - x) / 100;
            } else {
                attackStep_ = 1 + (x - w.camera.x) / 100;
            }
            fireball_ = true;
            burstUpBack();
            break;
        case 5:
            fireball_ = true;
            burstAcrossTwice();
            break;
        case 6:
            fireball_ = true;
            burstUpTwice();
            break;
        case 7:
            fireball_ = true;
            cross();
            break;
        case 8:
            volleys_ = 4 + w.random() % 7;
            fireball_ = true;
            throwsAcross();
            break;
        case 9:
            fireball_ = true;
            volleys_ = 3 + w.random() % 5;
            throwsUp();
            break;
        case 10:
            fireball_ = true;
            volleys_ = 4 + w.random() % 5;
            crosses();
            break;
        case 11:
            speed_ = 5;
            volleys_ = 3 + w.random() % 4;
            fireRain();
            break;
        case 12:
            fireball_ = true;
            volleys_ = 3 + w.random() % 3;
            throwsUpAimed();
            break;
        case 13:
            fireball_ = true;
            volleys_ = 3 + w.random() % 3;
            throwsAcrossAimed();
            break;
        case 14:
            fireball_ = true;
            volleys_ = 3 + w.random() % 4;
            crossesAimed();
            break;
        default:  // 15
            speed_ = 5;
            fireball_ = false;
            berserk();
            break;
    }
}

void Mario::endAttack() {
    mode = kWalking;
    animation.restart();
    speed_ = 1;
    walkAbout();
}

void Mario::jump() {
    World& w = world();

    if (dy < 0) {
        floatUp();
    } else {
        fall();
    }

    // Not out of the screen.
    if (direction == kFacingLeft && (x - speed_ < minX || solidWall(x - speed_, y))) {
        direction = kFacingRight;
        speed_ = 1;
    } else if (direction == kFacingRight && (x + speed_ > w.camera.right() || solidWall(x + speed_, y))) {
        direction = kFacingLeft;
        speed_ = 1;
    }

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
        image = enemyImage(dy < 0 ? img::kMarioJumpUpRight : img::kMarioJumpDownRight);
    } else {
        x -= speed_;
        image = enemyImage(dy < 0 ? img::kMarioJumpUpLeft : img::kMarioJumpDownLeft);
    }
    hitboxFromImage();
}

void Mario::floatUp() {
    gravityDelay += 1;
    gravityDelay %= kJumpGravityDelay;

    if (gravityDelay == 0 && dy < kMaxFallSpeed) {
        dy += 1;
    }
    if (dy < 0 && solidWall(x, y + dy)) {
        dy = kMaxFallSpeed;
    }
    const int ground = platformAt(x, y + dy);
    if (ground != 0 && dy > 0) {
        y = ground;
    } else {
        y += dy;
    }
}

void Mario::draw() {
    Actor::draw();
    if (fireball_ && animation.frame < 3) {
        if (direction == kFacingRight) {
            drawAt(x + 29, y - 33, enemyImage(img::kMarioHandFireballRight + animation.frame));
        } else {
            drawAt(x - 29, y - 33, enemyImage(img::kMarioHandFireballLeft + animation.frame));
        }
    }
}

void Mario::fireballAcross(int speed) {
    World& w = world();
    auto* ball = new Fireball(w, direction == kFacingRight ? speed : -speed);
    ball->direction = direction;
    ball->x = direction == kFacingRight ? x + 38 : x - 38;
    ball->y = y - 31;
    w.enemyShots.add(ball);
}

void Mario::fireballUp(int xSpeed, int goalX) {
    World& w = world();
    auto* ball = new FireballUp(w, xSpeed, goalX);
    ball->direction = direction;
    ball->x = direction == kFacingRight ? x + 38 : x - 38;
    ball->y = y - 38;
    w.enemyShots.add(ball);
}

//-----------------------------------------------------------------------------
// Single throws
//-----------------------------------------------------------------------------

void Mario::throwAcross() {
    World& w = world();

    image = enemyImage(direction == kFacingRight ? img::kMarioThrowRight : img::kMarioThrowLeft);

    const int floor = w.marker("finalboss.marioFloorY");  // the arena's floor
    if (y - 25 < goalY_ && y < floor) {
        // Down to the goal's height, slower as he nears it.
        if (y > goalY_ - 60) {
            dy = 2;
        } else if (y > goalY_ - 120) {
            dy = 3;
        } else if (y > goalY_ - 180) {
            dy = 4;
        } else if (y > goalY_ - 240) {
            dy = 5;
        } else {
            dy = 6;
        }
        if (y + dy > floor) {
            y = floor;
            dy = 0;
        } else {
            y += dy;
        }
    } else if (y - 30 > goalY_ && y <= floor) {
        // Up to it.
        if (y < goalY_ + 60) {
            dy = -2;
        } else if (y < goalY_ + 120) {
            dy = -3;
        } else if (y < goalY_ + 180) {
            dy = -4;
        } else if (y < goalY_ + 240) {
            dy = -5;
        } else {
            dy = -6;
        }
        y += dy;
    } else {
        // There: throws at step 40, pushed back, and walks at 60.
        animation.cycle(4, 3);

        attackStep_ += 1;
        if (attackStep_ == 60) {
            endAttack();
            return;
        }
        if (attackStep_ >= 40) {
            if (attackStep_ == 40) {
                fireball_ = false;
                fireballAcross(4);
            }
            if (direction == kFacingRight && x > w.camera.x + 10) {
                x -= kRecoil[attackStep_ - 40];
            } else if (direction == kFacingLeft && x < w.camera.right() - 10) {
                x += kRecoil[attackStep_ - 40];
            }
        }
    }
    hitboxFromImage();
}

void Mario::throwUp() {
    image = enemyImage(direction == kFacingRight ? img::kMarioThrowRight : img::kMarioThrowLeft);

    animation.cycle(4, 3);

    attackStep_ += 1;
    if (attackStep_ == 40) {
        fireball_ = false;
        fireballUp(0, goalX_);
    } else if (attackStep_ == 60) {
        endAttack();
        return;
    }
    hitboxFromImage();
}

void Mario::cross() {
    switch (attackPhase_) {
        case 0:
            throwAcross();
            if (attackStep_ > 0) {
                attackPhase_ = 1;
                attackStep_ = 20;
                attackSub_ = 0;
                fireball_ = true;
            }
            break;
        case 1:
            throwUp();
            if (attackStep_ > 40) {
                attackPhase_ = 2;
                attackStep_ = 0;
                attackSub_ = 0;
                fireball_ = true;
            }
            break;
        case 2:
            throwAcross();
            break;
        default:
            break;
    }
}

//-----------------------------------------------------------------------------
// Bursts across
//-----------------------------------------------------------------------------

void Mario::burstAcross() {
    image = enemyImage(direction == kFacingRight ? img::kMarioThrowRight : img::kMarioThrowLeft);

    animation.cycle(4, 3);

    // A fireball at each of four heights as he rises or falls.
    if ((y >= 140 && y <= 145) || (y >= 240 && y <= 245) || (y >= 340 && y <= 345) ||
        (y >= 440 && y <= 455)) {
        fireballAcross(1);
    }

    if ((dy < 0 && y < 100) || (dy > 0 && y > 435)) {
        animation.restart();
        mode = kFalling;
        fireball_ = false;
        jump();
        if (dy > 3) {
            dy = 3;
        }
        return;
    }
    y += dy;
    hitboxFromImage();
}

void Mario::burstAcrossTwice() {
    switch (attackPhase_) {
        case 0:
            if (y + dy < 100) {
                attackPhase_ = 1;
                fireball_ = false;
            } else {
                burstAcross();
            }
            break;
        case 1:
            // Up off the screen, then down again, bursting.
            fall();
            if (dy > 0 && y < 0) {
                attackPhase_ = 2;
                fireball_ = true;
                dy = 6;
            }
            hitboxFromImage();
            break;
        case 2:
            burstAcross();
            break;
        default:
            break;
    }
}

void Mario::throwsAcross() {
    throwAcross();
    if (volleys_ > attackPhase_ && attackStep_ > 50) {
        attackPhase_ += 1;
        attackStep_ = 30;
        attackSub_ = 0;
    } else if (volleys_ > attackPhase_ && attackStep_ == 40) {
        fireball_ = true;
    }
}

void Mario::throwsAcrossAimed() {
    World& w = world();

    throwAcross();
    if (volleys_ > attackPhase_ && attackStep_ > 50) {
        if (w.target != nullptr && ((w.target->x > x && direction == kFacingRight) ||
                                    (w.target->x < x && direction == kFacingLeft))) {
            attackPhase_ += 1;
            attackStep_ = 30;
            attackSub_ = 0;
            goalY_ = w.target->y;
        } else {
            fireball_ = false;
        }
    } else if (volleys_ > attackPhase_ && attackStep_ == 40) {
        fireball_ = true;
    }
}

//-----------------------------------------------------------------------------
// Bursts up
//-----------------------------------------------------------------------------

void Mario::burstUp() {
    World& w = world();

    image = enemyImage(direction == kFacingRight ? img::kMarioThrowRight : img::kMarioThrowLeft);

    animation.cycle(4, 3);

    // Every 6 ticks a fireball falling 100 pixels further away, until off
    // the screen.
    attackSub_ += 1;
    attackSub_ %= 6;
    if (attackSub_ == 0) {
        attackStep_ += 1;
        if (direction == kFacingRight) {
            goalX_ = x + attackStep_ * 100 + w.random() % 20;
            if (goalX_ > w.camera.right() + 80) {
                animation.restart();
                speed_ = 0;
                mode = kWalking;
                fireball_ = false;
                walkAbout();
                return;
            }
            fireballUp(attackStep_, goalX_);
        } else {
            // (2002: a local copy; the goal member is left alone.)
            const int goalX = x - attackStep_ * 100 - w.random() % 20;
            if (goalX < w.camera.x - 100) {
                animation.restart();
                speed_ = 0;
                mode = kWalking;
                fireball_ = false;
                walkAbout();
                return;
            }
            fireballUp(-attackStep_, goalX);
        }
    }
    hitboxFromImage();
}

void Mario::burstUpBack() {
    World& w = world();

    image = enemyImage(direction == kFacingRight ? img::kMarioThrowRight : img::kMarioThrowLeft);

    animation.cycle(4, 3);

    // From the far side of the screen back towards him.
    attackSub_ += 1;
    attackSub_ %= 6;
    if (attackSub_ == 0) {
        attackStep_ -= 1;
        if (direction == kFacingRight) {
            goalX_ = w.camera.x + attackStep_ * 100 + w.random() % 20;
            if (goalX_ < x + 20) {
                animation.restart();
                speed_ = 0;
                mode = kWalking;
                fireball_ = false;
                walkAbout();
                return;
            }
            fireballUp(attackStep_, goalX_);
        } else {
            const int goalX = w.camera.right() - attackStep_ * 100 - w.random() % 20;
            if (goalX > x - 20) {
                animation.restart();
                speed_ = 0;
                mode = kWalking;
                fireball_ = false;
                walkAbout();
                return;
            }
            fireballUp(-attackStep_, goalX);
        }
    }
    hitboxFromImage();
}

void Mario::burstUpTwice() {
    World& w = world();

    switch (attackPhase_) {
        case 0:
            if (direction == kFacingRight && x + (attackStep_ + 1) * 100 + 20 > w.camera.right() - 20) {
                attackPhase_ = 1;
                attackSub_ = 0;
                attackStep_ = 1 + (w.camera.right() - x) / 100;
            } else if (direction == kFacingLeft && x - (attackStep_ + 1) * 100 - 20 < w.camera.x + 20) {
                attackPhase_ = 1;
                attackSub_ = 0;
                attackStep_ = 1 + (x - w.camera.x) / 100;
            } else {
                burstUp();
            }
            break;
        case 1:
            burstUpBack();
            break;
        default:
            break;
    }
}

void Mario::throwsUp() {
    throwUp();
    if (volleys_ > attackPhase_ && attackStep_ == 40) {
        attackStep_ = 25;
        attackSub_ = 0;
        fireball_ = true;
        attackPhase_ += 1;
    }
}

void Mario::throwsUpAimed() {
    World& w = world();

    throwUp();
    if (volleys_ > attackPhase_ && attackStep_ == 40 && w.target != nullptr) {
        attackStep_ = 10;
        attackSub_ = 0;
        fireball_ = true;
        attackPhase_ += 1;
        goalX_ = w.target->x;
    }
}

//-----------------------------------------------------------------------------
// Both
//-----------------------------------------------------------------------------

void Mario::crosses() {
    if (attackPhase_ % 2 == 0) {
        throwUp();
        if (volleys_ > attackPhase_ && attackStep_ == 40) {
            attackStep_ = 35;
            attackSub_ = 0;
            fireball_ = true;
            attackPhase_ += 1;
        }
    } else {
        throwAcross();
        if (volleys_ > attackPhase_ && attackStep_ > 50) {
            attackPhase_ += 1;
            attackStep_ = 35;
            attackSub_ = 0;
        } else if (volleys_ > attackPhase_ && attackStep_ == 40) {
            fireball_ = true;
        }
    }
}

void Mario::crossesAimed() {
    World& w = world();

    if (attackPhase_ % 2 == 0) {
        throwUp();
        if (volleys_ > attackPhase_ && attackStep_ == 40 && w.target != nullptr) {
            attackStep_ = 30;
            attackSub_ = 0;
            fireball_ = true;
            attackPhase_ += 1;
            goalY_ = w.target->y;
        }
    } else {
        throwAcross();
        if (volleys_ > attackPhase_ && attackStep_ > 50) {
            if (w.target != nullptr && ((w.target->x > x && direction == kFacingRight) ||
                                        (w.target->x < x && direction == kFacingLeft))) {
                attackPhase_ += 1;
                attackStep_ = 30;
                attackSub_ = 0;
                goalX_ = w.target->x;
            } else {
                fireball_ = false;
            }
        } else if (volleys_ > attackPhase_ && attackStep_ == 40) {
            fireball_ = true;
        }
    }
}

void Mario::runToEdge() {
    World& w = world();

    if (direction == kFacingRight) {
        walk(speed_);
        image = enemyImage(img::kMarioWalkRight + animation.loop(kWalk, 4, 16 - 3 * speed_));
        if (x > w.camera.right() - 40) {
            fireball_ = true;
            attackPhase_ = 1;
            speed_ = 1;
            direction = kFacingLeft;
        }
    } else {
        walk(-speed_);
        image = enemyImage(img::kMarioWalkLeft + animation.loop(kWalk, 4, 16 - 3 * speed_));
        if (x < w.camera.x + 40) {
            fireball_ = true;
            attackPhase_ = 1;
            speed_ = 1;
            direction = kFacingRight;
        }
    }
    hitboxFromImage();
}

void Mario::berserk() {
    World& w = world();

    switch (attackPhase_) {
        case 0:
            runToEdge();
            break;
        case 1:
            // A burst up across the screen.
            if ((direction == kFacingRight && x + (attackStep_ + 1) * 100 + 20 > w.camera.right() - 20) ||
                (direction == kFacingLeft && x - (attackStep_ + 1) * 100 - 20 < w.camera.x + 20)) {
                attackPhase_ = 2;
            } else {
                burstUp();
            }
            break;
        case 2:
            // Up, bursting across.
            if (y + dy < 100) {
                attackPhase_ = 3;
                fireball_ = false;
            } else {
                burstAcross();
            }
            break;
        case 3:
            // Off the top; comes down at the other edge.
            fall();
            if (y < 0) {
                if (direction == kFacingRight) {
                    direction = kFacingLeft;
                    x = w.camera.right() - 40;
                } else {
                    direction = kFacingRight;
                    x = w.camera.x + 40;
                }
                dy = 6;
                attackPhase_ = 4;
                fireball_ = true;
            }
            hitboxFromImage();
            break;
        case 4:
            // Down, bursting across.
            if (y + dy > 435) {
                attackPhase_ = 5;
                // Bursts to the edge of the screen (D-88 fixed: 2002 used
                // screen positions as level ones).
                if (direction == kFacingRight) {
                    attackStep_ = 1 + (world().camera.right() - x) / 100;
                } else {
                    attackStep_ = 1 + (x - world().camera.x) / 100;
                }
                attackSub_ = 0;
                animation.restart();
                dy = 0;
            } else {
                burstAcross();
            }
            break;
        case 5:
            // A burst up coming back.
            if ((direction == kFacingRight && x + (attackStep_ - 1) * 100 - 20 < x) ||
                (direction == kFacingLeft && x - (attackStep_ - 1) * 100 + 20 > x)) {
                attackPhase_ = 6;
                attackStep_ = 0;
                fireball_ = false;
            } else {
                burstUpBack();
            }
            break;
        case 6:
            attackStep_ += 1;
            if (attackStep_ >= 160) {
                endAttack();
                return;
            }
            break;
        default:
            break;
    }
}

void Mario::fireRain() {
    World& w = world();

    if (attackPhase_ == 0) {
        runToEdge();
        return;
    }

    if (attackPhase_ < volleys_) {
        if (attackPhase_ % 2 == 0) {
            // A burst coming back.
            if ((direction == kFacingRight && x + (attackStep_ - 1) * 100 - 20 < x) ||
                (direction == kFacingLeft && x - (attackStep_ - 1) * 100 + 20 > x)) {
                attackPhase_ += 1;
                attackSub_ = 0;
                attackStep_ = 0;
            } else {
                burstUpBack();
            }
        } else {
            // A burst going out, to 20 pixels from the screen's edges (D-88
            // fixed: 2002 compared them as level positions).
            if (direction == kFacingRight && x + (attackStep_ + 1) * 100 + 20 > w.camera.right() - 20) {
                attackPhase_ += 1;
                attackSub_ = 0;
                attackStep_ = 1 + (w.camera.right() - x) / 100;
            } else if (direction == kFacingLeft && x - (attackStep_ + 1) * 100 - 20 < w.camera.x + 20) {
                attackPhase_ += 1;
                attackSub_ = 0;
                attackStep_ = 1 + (x - w.camera.x) / 100;
            } else {
                burstUp();
            }
        }
        return;
    }

    // Waits for the rain to fall.
    animation.frame += 1;
    fireball_ = false;
    if (animation.frame >= 60 + volleys_ * 30) {
        animation.restart();
        speed_ = 0;
        mode = kWalking;
        walkAbout();
    }
}

void Mario::hitBy(Shot& shot) {
    static const int kBloodX[] = {0, 10, 15, 10, 0, -10, -15, -10};
    static const int kBloodY[] = {-15, -25, -25, -25, -35, -25, -25, -25};

    Enemy::hitBy(shot);
    sprayBlood(shot, kBloodX, kBloodY);
}

//-----------------------------------------------------------------------------
// The fireballs
//-----------------------------------------------------------------------------

void Fireball::update() {
    const int camera = world().camera.x;
    const int cameraRight = world().camera.right();

    if (x < camera - 100 || x > cameraRight + 60 || y > 520 || y < -50) {
        dead = true;
    }

    speedStep_ += 1;
    speedStep_ %= 8;
    if (speedStep_ == 0) {
        xSpeed_ += direction == kFacingRight ? 1 : -1;
    }
    x += xSpeed_;

    animation.cycle(4, 3);

    // Longer flames as it speeds up (unchanged at speed 0).
    if (xSpeed_ > 12) {
        image = enemyImage(img::kFireballRightFast + animation.frame);
    } else if (xSpeed_ > 6) {
        image = enemyImage(img::kFireballRightMedium + animation.frame);
    } else if (xSpeed_ > 0) {
        image = enemyImage(img::kFireballRightSlow + animation.frame);
    }
    if (xSpeed_ < -12) {
        image = enemyImage(img::kFireballLeftFast + animation.frame);
    } else if (xSpeed_ < -6) {
        image = enemyImage(img::kFireballLeftMedium + animation.frame);
    } else if (xSpeed_ < 0) {
        image = enemyImage(img::kFireballLeftSlow + animation.frame);
    }
    hitboxFromImage();
}

void FireballUp::drop() {
    static const int kFireballGravityDelay = 10;  // LATENCE_FIREBALL
    static const int kMaxSpeed = 15;              // GRAVITE_FIREBALL

    gravityDelay += 1;
    gravityDelay %= kFireballGravityDelay;
    if (gravityDelay == 0 && dy < kMaxSpeed) {
        dy += 1;
    }
    if (dy < 0 && solidWall(x, y + dy)) {
        dy = kMaxSpeed;
    }
    y += dy;
}

void FireballUp::update() {
    World& w = world();
    const int camera = w.camera.x;
    const int cameraRight = w.camera.right();

    if (x < camera - 100 || x > cameraRight + 60 || y > 520 || y < -250) {
        dead = true;
    }
    if (dy > 0) {
        drop();
    } else {
        y += dy;
    }

    // Off the top: comes back down over its goal.
    if (y < -50) {
        x = goalX_;
        dy = 1;
    }
    if (dy < 0) {
        x += xSpeed_;
    }

    animation.cycle(4, 3);

    if (dy > 12) {
        image = enemyImage(img::kFireballDownFast + animation.frame);
    } else if (dy > 6) {
        image = enemyImage(img::kFireballDownMedium + animation.frame);
    } else if (dy > 0) {
        image = enemyImage(img::kFireballDownSlow + animation.frame);
    }
    if (dy < -12) {
        image = enemyImage(img::kFireballUpFast + animation.frame);
    } else if (dy < -6) {
        image = enemyImage(img::kFireballUpMedium + animation.frame);
    } else if (dy < 0) {
        image = enemyImage(img::kFireballUpSlow + animation.frame);
    }
    hitboxFromImage();

    const int floor = w.collision.platforms[0][0];
    if (floor < y + dy && floor > y) {
        w.shake.start(7);
    }
}

}  // namespace bb::game::finalboss
