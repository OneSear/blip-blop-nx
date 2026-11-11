// The props of finalboss (fond_battle_cruiser.cpp, fond_tircruiser.cpp,
// tir_cruiser_laser.cpp, fond_rocher_hyperspace.cpp, fond_monolite.cpp,
// fond_rocher_1.cpp, fond_rocher_2.cpp).

#include "bb/game/trig.hpp"
#include "finalboss/finalboss_world.hpp"

namespace bb::game::finalboss {

//-----------------------------------------------------------------------------
// The battle cruisers
//-----------------------------------------------------------------------------

namespace {

constexpr int kShotSpeed = 2;  // TIR_SPEED
const int kCruiserSway[] = {0, 1, 2, 1};

}  // namespace

void BattleCruiser::update() {
    World& w = world();

    flameSub_ += 1;
    flameSub_ %= 40;
    if (flameSub_ == 0) {
        flameStep_ += 1;
        flameStep_ %= 6;
    }

    // The two cruisers pass each other; while close, they exchange fire.
    x += 1;
    enemyX_ -= 1;
    image = levelImage(prop::kCruiserRight + animation.loop(kCruiserSway, 4, 12));

    const int dyEnemy = enemyY_ - y;
    const int dxEnemy = enemyX_ - x;
    if (dyEnemy != 0 && dxEnemy / dyEnemy < 2 && dxEnemy > 0) {
        attackStep_ += 1;
        if (attackStep_ == 2) {
            attackStep_ = -2;
        }

        image = levelImage(prop::kCruiserFiring + 2 + attackStep_);
        // The vertical speeds: int(speed - (dx * speed * 0.25) / dy) in 2002
        // (D-56), the same in integers.
        const int vertical = (4 * kShotSpeed * dyEnemy - dxEnemy * kShotSpeed) / (4 * dyEnemy);
        auto* shot = new CruiserShot(w, prop::kCruiserShot + 2 + attackStep_,
                                     (dxEnemy * kShotSpeed) / dyEnemy, vertical);
        shot->x = x - 8 * attackStep_;
        shot->y = y + 6;
        w.props.add(shot);

        shot = new CruiserShot(w, prop::kCruiserShot + 1 - attackStep_, -(dxEnemy * kShotSpeed) / dyEnemy,
                               (-4 * kShotSpeed * dyEnemy + dxEnemy * kShotSpeed) / (4 * dyEnemy));
        shot->x = enemyX_ - 8 * attackStep_;
        shot->y = enemyY_ - 6;
        w.props.add(shot);

        if (attackStep_ == -2) {
            auto* laser = new CruiserLaser(w, prop::kCruiserLaserRight - dxEnemy / dyEnemy, 1);
            laser->x = x + 16;
            laser->y = y;
            w.props.add(laser);
        }
        if (attackStep_ == 0) {
            auto* laser = new CruiserLaser(w, prop::kCruiserLaserLeft - dxEnemy / dyEnemy, -1);
            laser->x = enemyX_ - 16;
            laser->y = enemyY_;
            w.props.add(laser);
        }
    } else {
        attackStep_ = -3;
    }

    // Round again (level positions, D-51; further right in a wider view).
    const int extra = w.camera.extra();
    if (x > 720 + extra) {
        x = -20;
        attackStep_ = 1;
        enemyX_ = 700 + extra;
    }
}

void BattleCruiser::draw() {
    Actor::draw();
    if (attackStep_ == -3) {
        drawAt(enemyX_, enemyY_, levelImage(prop::kCruiserLeft + kCruiserSway[animation.frame]));
    } else {
        drawAt(enemyX_, enemyY_, levelImage(prop::kEnemyCruiserFiring + 1 - attackStep_));
    }

    // The engine flames flicker at ticks 0 to 2 and 10 to 12 of 40.
    if ((flameSub_ >= 0 && flameSub_ <= 2) || (flameSub_ >= 10 && flameSub_ <= 12)) {
        const int flicker = flameSub_ == 1 || flameSub_ == 11 ? 1 : 0;
        // Per step: the flame's offset from each cruiser (x, y, enemy x).
        static const int kOffsets[6][3] = {{21, -2, -20}, {-10, -7, 11}, {8, -6, -7},
                                           {-8, 6, 7},    {-10, -7, 11}, {21, 2, -20}};
        const int* offset = kOffsets[flameStep_];
        drawAt(x + offset[0], y + offset[1], levelImage(prop::kCruiserFlameRight + flicker));
        drawAt(enemyX_ + offset[2], enemyY_ + offset[1], levelImage(prop::kCruiserFlameLeft + flicker));
    }
}

void CruiserShot::update() {
    if (dy >= 0 && y + dy > 43) {
        // Bursts at the lower cruiser's height.
        y = 43;
        if (animation.tick(4)) {
            if (animation.frame == 3) {
                dead = true;
            } else {
                image = levelImage(prop::kCruiserSparkRight + animation.frame);
            }
        }
    } else if (dy <= 0 && y - dy < 20) {
        y = 18;
        if (animation.tick(4)) {
            if (animation.frame == 3) {
                dead = true;
            } else {
                image = levelImage(prop::kCruiserSparkLeft + animation.frame);
            }
        }
    } else {
        x += xSpeed_;
        y += dy;
    }
    dieWhenOffscreen();
}

void CruiserLaser::update() {
    animation.tick(4);
    if (animation.frame == 4) {
        dead = true;
        return;
    }
    if (animation.frame == 1 && animation.subFrame == 0) {
        // The beam images 10 to 17: down-right steep to flat, then up-left.
        static const int kJumpX[] = {36, 19, 10, 0, -36, -19, -10, 0};
        static const int kJumpY[] = {19, 19, 19, 19, -19, -19, -19, -19};
        const int beam = picture_ - 10;
        if (beam >= 0 && beam < 8) {
            x += kJumpX[beam];
            y += kJumpY[beam];
        }
    }
    if (animation.frame > 0) {
        image = levelImage((toRight_ ? prop::kCruiserSparkRight - 1 : prop::kCruiserSparkLeft - 1) +
                           animation.frame);
    }
}

//-----------------------------------------------------------------------------
// The hyperspace rocks
//-----------------------------------------------------------------------------

HyperspaceRock::HyperspaceRock(World& world) : Actor(world) {
    angle_ = world.random() % 360;
}

void HyperspaceRock::pickLook() {
    World& w = world();
    switch (plane_) {
        case 0:
            xSpeed_ = 8 + w.random() % 4;
            image = levelImage(prop::kHyperspaceRockNear + w.random() % 5);
            bobSpeed_ = 4 + w.random() % 2;
            break;
        case 1:
            xSpeed_ = 4 + w.random() % 5;
            image = levelImage(prop::kHyperspaceRockMid + w.random() % 5);
            bobSpeed_ = 3 + w.random() % 2;
            break;
        case 2:
            xSpeed_ = 2 + w.random() % 3;
            image = levelImage(prop::kHyperspaceRockFar + w.random() % 7);
            bobSpeed_ = 2 + w.random() % 2;
            break;
        default:
            break;
    }
}

void HyperspaceRock::update() {
    World& w = world();

    angle_ += bobSpeed_;
    angle_ %= 360;

    if (placing_) {
        // The plane by the starting x (all the level's rocks start on the
        // first screen), then a random place.
        if (x >= 0 && x < 640) {
            plane_ = x < 200 ? 2 : x < 420 ? 1 : 0;
            pickLook();
        }
        x = w.random() % w.camera.width;  // across the view (the 2002 screen: 640)
        baseY_ = w.random() % 380;
        y = w.random() % 380;
        placing_ = false;
    }
    animation.subFrame += 1;
    animation.subFrame %= xSpeed_;
    x += xSpeed_;

    static const int kBobDivisor[] = {92, 128, 192};
    y = baseY_ + cosine1024(angle_) / kBobDivisor[plane_];

    // Off the right of the screen: back from the left.
    if (x > w.camera.right() + 60) {
        x = w.camera.x - 80;
        baseY_ = w.random() % 380;
        y = w.random() % 380;
        pickLook();
    }
}

void HyperspaceRock::draw() {
    if (!placing_) {
        Actor::draw();
    }
}

//-----------------------------------------------------------------------------
// The monolith and the floating rocks
//-----------------------------------------------------------------------------

namespace {

const int kMonolithAnimation[] = {0, 1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 4, 3, 3, 3, 2, 2, 1,
                                  0, 5, 6, 6, 7, 7, 7, 8, 8, 8, 8, 8, 7, 7, 7, 6, 6, 5};
const int kMonolithBob[] = {0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 1, 0, 0, 0};
const int kRockAnimation[] = {0, 1, 1, 2, 2, 2, 3, 3, 3, 3, 3, 2, 2, 2, 1, 1,
                              0, 4, 4, 5, 5, 5, 6, 6, 6, 6, 6, 5, 5, 5, 4, 4};
const int kRock1Bob[] = {0, 0, 1, 1, 1, 1, 2, 2, 1, 1, 1, 0, 0, 0};
const int kRock2Bob[] = {0, 0, 0, 0, 1, 0, 0, 0, 0, 0};

}  // namespace

// (The images before the first update are unrelated ones of the bank, as
// in 2002; the first update replaces them.)
const FloaterSpec Floater::kMonolith = {prop::kCruiserSparkRight + 1,
                                        prop::kMonolith,
                                        kMonolithAnimation,
                                        36,
                                        3,
                                        kMonolithBob,
                                        22,
                                        44,
                                        4,
                                        20,
                                        138,
                                        118,
                                        118,
                                        138,
                                        true};
const FloaterSpec Floater::kRock1 = {prop::kEnemyCruiserFiring + 3,
                                     prop::kRock1,
                                     kRockAnimation,
                                     32,
                                     3,
                                     kRock1Bob,
                                     13,
                                     26,
                                     4,
                                     30,
                                     45,
                                     45,
                                     45,
                                     45,
                                     false};
const FloaterSpec Floater::kRock2 = {
    prop::kRock2Still, prop::kRock2, kRockAnimation, 32, 4, kRock2Bob, 9, 18, 6, 25, 45, 45, 45, 45, false};

void Floater::carry(EntityList<Entity>& list, int width, int step) {
    // The list's own cursor, as in 2002.
    for (list.rewind(); !list.atEnd(); list.next()) {
        Entity* rider = list.current();
        const int riderX = rider->x;
        if (riderX >= x - spec_.carryLeft && riderX < x + width - spec_.carryRight &&
            platformAt(riderX, rider->y) == y - spec_.top) {
            rider->y += step;
        }
    }
}

void Floater::update() {
    World& w = world();

    image =
        levelImage(spec_.first + animation.loop(spec_.animation, spec_.animationSteps, spec_.animationDelay));

    bobSub_ += 1;
    bobSub_ %= spec_.bobDelay;
    if (bobSub_ == 0) {
        bobFrame_ += 1;
        if (bobFrame_ == spec_.bobSteps) {
            bobFrame_ = 0;
        }
    }
    const int step =
        bobFrame_ <= spec_.bobHalf ? spec_.bob[bobFrame_] : -spec_.bob[bobFrame_ - spec_.bobHalf];

    const int width = image->width();
    carry(w.players, width, step);
    if (spec_.carriesEnemies) {
        carry(w.enemies, width, step);
    }

    y += step;
    setPlatform(4, x - spec_.platformLeft, x + width - spec_.platformRight, y - spec_.top);
    hitboxFromImage();
}

}  // namespace bb::game::finalboss
