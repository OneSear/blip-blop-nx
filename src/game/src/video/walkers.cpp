// Toad, Yoshi, the Yoshi turret, Tails, the flying Tails and Knuckles
// (ennemi_toad.cpp, ennemi_yoshi.cpp, ennemi_yoshi_dca.cpp, ennemi_tails.cpp,
// ennemi_tailsvolant.cpp, ennemi_knukles.cpp) and Yoshi's shell
// (tir_carapace.cpp).

#include "video/video_world.hpp"

namespace bb::game::video {

namespace {

const int kBloodX[] = {0, 10, 15, 10, 0, -10, -15, -10};
const int kBloodY[] = {-15, -25, -25, -25, -35, -25, -25, -25};

// The corpse is engraved where it lies unless on platform layer 4.
bool onEngravingGround(World& w, int x, int y) {
    const int ground = w.collision.platformAt(x, y);
    return ground != 0 && ground != w.collision.platforms[4][x];
}

bool sameLayer(World& w, const Entity& a, const Entity& b) {
    return w.collision.platformLayerAt(a.x, a.y) == w.collision.platformLayerAt(b.x, b.y);
}

// Toad's "my brain hurts" gag: one counter for all the Toads (2002 file
// statics, kept in World::shared(): they decide how long the death
// convulsions last, which the fingerprints see). `wait` counts the ticks since the gag, advanced
// once per tick (on the blink phase) while a Toad lives; reset at each
// level start (D-50).
struct BrainHurts {
    int wait = 1000;
    bool phase = false;
    int count = 9;
};

}  // namespace

//-----------------------------------------------------------------------------
// Toad
//-----------------------------------------------------------------------------

namespace {

constexpr int kToadTopSpeed = 3;
constexpr int kToadChargeSpeed = 7;
const int kToadWalk[] = {0, 1, 2, 1};

}  // namespace

Toad::Toad(World& world) : Enemy(world) {
    chargeDelay_ = 50 + world.random() % 250;
    convulsions_ = world.random() % 5;
    treasure = 7;
    health.points = 300;
}

void Toad::update() {
    BrainHurts& brainHurts = world().shared<BrainHurts>();
    World& w = world();
    if (brainHurts.phase != w.blink) {
        brainHurts.wait += 1;
        brainHurts.phase = w.blink;
    }

    if (bleeding > 0) {
        bleeding -= 1;
    }

    switch (mode) {
        case kNormal:
        case kWalking:
            walkAbout();
            break;
        case kDying:
            die();
            break;
        case kCarbonised:
            fall();
            burn(img::kToadBurnLeft, img::kToadBurnRight, 13, 5);
            break;
        case kJumping:
        case kFalling:
            jump();
            break;
        case kShooting:  // the charge
            charge();
            break;
        default:
            break;
    }
    dieWhenOffscreen();
}

void Toad::walkAbout() {
    World& w = world();

    // No ground: falls, and no more this tick (D-54 fixed: 2002 also
    // walked on this tick, a double move where a platform ends).
    if (platformAt(x, y) == 0) {
        mode = kFalling;
        dy = 0;
        animation.restart();
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

    chargeWait_ += 1;
    if (chargeWait_ >= chargeDelay_) {
        const Entity& target = *w.target;
        if (sameLayer(w, target, *this) &&
            ((direction == kFacingRight && x < target.x) || (direction == kFacingLeft && x > target.x))) {
            chargeStep_ = 0;
            speed_ = 0;
            animation.restart();
            dy = 0;
            mode = kShooting;
            w.media.levelSounds->play(snd::kToadCharge);
            charge();
            return;
        }
        chargeWait_ = 0;
        chargeDelay_ = 50 + w.random() % 250;
        animation.restart();
        mode = kJumping;
        dy = -7;
        gravityDelay = 0;  // otherwise the jumps differ in height
        jump();
        return;
    }

    if (direction == kFacingRight) {
        walk(speed_);
        image = enemyImage(img::kToadWalkRight + animation.loop(kToadWalk, 4, 12 - 3 * speed_));
    } else {
        walk(-speed_);
        image = enemyImage(img::kToadWalkLeft + animation.loop(kToadWalk, 4, 12 - 3 * speed_));
    }
    if (speed_ < kToadTopSpeed) {
        speedStep_ += 1;
        if (speedStep_ >= 30) {
            speedStep_ = 0;
            speed_ += 1;
        }
    }
    hitboxFromImage();
}

void Toad::die() {
    World& w = world();

    fall();

    animation.tickUpTo(6, 10);
    // Convulsions: steps 1 to 4 again.
    if (animation.frame > 4 && convulsions_ > 0) {
        convulsions_ -= 1;
        animation.frame -= 4;
    }
    if (animation.frame == 5 && animation.subFrame == 0) {
        w.media.miscSounds->play(miscsnd::kDeathCry2 + w.random() % 2);  // one of 2
    }

    if (animation.frame >= 10) {
        if (onEngravingGround(w, x, y)) {
            w.media.effects->engrave(x, y, image);
            dead = true;
        }
    } else if (direction == kFacingLeft) {
        if (!solidWall(x - speed_, y) && platformAt(x, y) == 0) {
            x -= speed_;
        }
        image = enemyImage(img::kToadDieLeft + animation.frame);
    } else {
        if (!solidWall(x + speed_, y) && platformAt(x, y) == 0) {
            x += speed_;
        }
        image = enemyImage(img::kToadDieRight + animation.frame);
    }
}

void Toad::endCharge(int newDirection) {
    direction = newDirection;
    speed_ = 1;
    mode = kWalking;
    chargeWait_ = 0;
    chargeDelay_ = 50 + world().random() % 250;
    animation.restart();
    walkAbout();
}

void Toad::charge() {
    World& w = world();

    speed_ = kToadChargeSpeed;
    if (x - speed_ < minX || solidWall(x - speed_, y)) {
        endCharge(kFacingRight);
        return;
    }
    if (x + speed_ > w.camera.right() || solidWall(x + speed_, y)) {
        endCharge(kFacingLeft);
        return;
    }

    // Stamps for 80 ticks, then runs.
    chargeStep_ += 1;
    const int walkImage = direction == kFacingRight ? img::kToadWalkRight : img::kToadWalkLeft;
    if (chargeStep_ < 80) {
        image = enemyImage(walkImage + animation.loop(kToadWalk, 4, 3));
    } else {
        walk(direction == kFacingRight ? speed_ : -speed_);
        image = enemyImage(walkImage + animation.loop(kToadWalk, 4, 2));
    }
    hitboxFromImage();
}

void Toad::jump() {
    fall();

    const int ground = dy > 0 ? platformAt(x, y + dy) : 0;
    if (ground != 0) {
        mode = kNormal;
        dy = 0;
        y = ground;
        animation.restart();
        walkAbout();
        return;
    }

    if (direction == kFacingRight) {
        x += speed_;
        image = enemyImage(img::kToadJumpRight);
    } else {
        x -= speed_;
        image = enemyImage(img::kToadJumpLeft);
    }
    hitboxFromImage();
}

void Toad::hitBy(Shot& shot) {
    BrainHurts& brainHurts = world().shared<BrainHurts>();
    World& w = world();

    Enemy::hitBy(shot);
    sprayBlood(shot, kBloodX, kBloodY);

    if (mode == kDying) {
        if (brainHurts.wait >= 140 && brainHurts.wait <= 200) {
            // Killed again soon after the gag: more convulsions.
            brainHurts.wait = 250;
            w.media.levelSounds->play(snd::kToadBrainHurtsAgain);
            convulsions_ = 7;
        } else {
            brainHurts.count += 1;
            brainHurts.count %= 10;
            if (brainHurts.count == 0) {
                brainHurts.wait = 0;
                convulsions_ = 6;
                w.media.levelSounds->play(snd::kToadBrainHurts);
            } else {
                w.media.levelSounds->play(snd::kToadDeath + w.random() % 2);  // one of 2
            }
        }
    }
}

//-----------------------------------------------------------------------------
// Yoshi
//-----------------------------------------------------------------------------

namespace {

constexpr int kYoshiSpeed = 3;
const int kYoshiWalk[] = {0, 1, 2, 1};
const int kYoshiTongue[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};

// A shell thrown from (x, y) towards `direction`.
void throwShellFrom(World& w, int x, int y, int direction) {
    auto* shell = new Shell(w);
    shell->direction = direction;
    shell->x = direction == kFacingLeft ? x - 23 : x + 23;
    shell->y = y - 23;
    w.enemyShots.add(shell);
}

}  // namespace

Yoshi::Yoshi(World& world) : Enemy(world) {
    attackDelay_ = 50 + world.random() % 250;
    treasure = 10;
    health.points = 500;
}

void Yoshi::update() {
    if (bleeding > 0) {
        bleeding -= 1;
    }

    switch (mode) {
        case kNormal:
        case kWalking:
            walkAbout();
            break;
        case kDying:
            die();
            break;
        case kCarbonised:
            fall();
            burn(img::kYoshiBurnLeft, img::kYoshiBurnRight, 14, 5);
            break;
        case kFalling:
            fallDown();
            break;
        case kShooting:
            if (throwing_) {
                throwShell();
            } else {
                tongue();
            }
            break;
        default:
            break;
    }
    dieWhenOffscreen();
}

void Yoshi::walkAbout() {
    World& w = world();

    if (platformAt(x, y) == 0) {
        mode = kFalling;
        dy = 0;
        gravityDelay = 0;
        animation.restart();
        fallDown();
        return;
    }

    attackWait_ += 1;
    if (attackWait_ >= attackDelay_ && x <= w.camera.right()) {
        attackWait_ = 0;
        attackDelay_ = 50 + w.random() % 250;
        mode = kShooting;
        throwing_ = true;
        animation.restart();
        w.media.levelSounds->play(snd::kYoshiShell);
        throwShell();
        return;
    }

    // The tongue, at a player ahead on its platform layer.
    const Entity& target = *w.target;
    if (sameLayer(w, target, *this) && ((direction == kFacingRight && x > target.x - 150 && target.x > x) ||
                                        (direction == kFacingLeft && x < target.x + 150 && target.x < x))) {
        mode = kShooting;
        throwing_ = false;
        animation.restart();
        w.media.levelSounds->play(snd::kYoshiTongue);
        tongue();
        return;
    }

    if (x - kYoshiSpeed < minX || solidWall(x - kYoshiSpeed, y)) {
        direction = kFacingRight;
    } else if (x + kYoshiSpeed > w.camera.right() || solidWall(x + kYoshiSpeed, y)) {
        direction = kFacingLeft;
    }

    // (3 of the 4 walk steps, as in 2002.)
    if (direction == kFacingRight) {
        walk(kYoshiSpeed);
        image = enemyImage(img::kYoshiWalkRight + animation.loop(kYoshiWalk, 3, 3));
    } else {
        walk(-kYoshiSpeed);
        image = enemyImage(img::kYoshiWalkLeft + animation.loop(kYoshiWalk, 3, 3));
    }
    hitboxFromImage();
}

void Yoshi::fallDown() {
    fall();

    const int ground = dy > 0 ? platformAt(x, y + dy) : 0;
    if (ground != 0) {
        mode = kNormal;
        dy = 0;
        y = ground;
        animation.restart();
        walkAbout();
        return;
    }

    if (direction == kFacingRight) {
        x += 1;
        image = enemyImage(img::kYoshiWalkRight);
    } else {
        x -= 1;
        image = enemyImage(img::kYoshiWalkLeft);
    }
    hitboxFromImage();
}

void Yoshi::die() {
    World& w = world();

    fall();

    animation.tickUpTo(6, 9);

    if (animation.frame >= 9) {
        if (onEngravingGround(w, x, y)) {
            w.media.effects->engrave(x, y, image);
        }
        dead = true;
    } else {
        image = enemyImage((direction == kFacingLeft ? img::kYoshiDieLeft : img::kYoshiDieRight) +
                           animation.frame);
    }
}

void Yoshi::tongue() {
    // One step every 2 ticks both ways (D-81 fixed: 2002 ran the left one
    // every tick, so a Yoshi facing left attacked twice as fast).
    if (direction == kFacingRight) {
        image = enemyImage(img::kYoshiTongueRight + animation.loop(kYoshiTongue, 18, 2));
    } else {
        image = enemyImage(img::kYoshiTongueLeft + animation.loop(kYoshiTongue, 18, 2));
    }
    if (animation.frame >= 17) {
        mode = kWalking;
        animation.restart();
        walkAbout();
        return;
    }
    hitboxFromImage();
}

void Yoshi::throwShell() {
    animation.tick(15);

    if (animation.frame >= 2) {
        throwShellFrom(world(), x, y, direction);
        mode = kWalking;
        animation.restart();
        walkAbout();
        return;
    }

    image = enemyImage((direction == kFacingRight ? img::kYoshiThrowRight : img::kYoshiThrowLeft) +
                       animation.frame);
    hitboxFromImage();
}

void Yoshi::hitBy(Shot& shot) {
    Enemy::hitBy(shot);
    sprayBlood(shot, kBloodX, kBloodY);

    if (mode == kDying) {
        world().media.miscSounds->play(miscsnd::kGoreSplat + world().random() % 3);  // one of 3
    }
}

//-----------------------------------------------------------------------------
// The Yoshi turret
//-----------------------------------------------------------------------------

YoshiTurret::YoshiTurret(World& world) : Enemy(world) {
    attackDelay_ = 50 + world.random() % 150;
    treasure = 12;
    health.points = 400;
}

void YoshiTurret::update() {
    if (bleeding > 0) {
        bleeding -= 1;
    }

    switch (mode) {
        case kNormal:
        case kWalking:
            stand();
            break;
        case kDying:
            die();
            break;
        case kCarbonised:
            fall();
            burn(img::kYoshiBurnLeft, img::kYoshiBurnRight, 14, 5);
            break;
        case kShooting:
            throwShell();
            break;
        default:
            break;
    }
    dieWhenOffscreen();
    dieWhenPassed();
}

// D-98 fixed: a turret never moves along the level and the camera never goes
// back, so once its image is past the screen's left edge it can be neither
// seen nor shot again. In 2002 it stayed until 250 pixels past that edge
// (dieWhenOffscreen()), still counted as an enemy: in video, one left alive
// on the ledge at x 1285 or 1417 kept the scroll lock at 1534, which waits
// for no enemies, shut for good. A dying or burning turret ends by itself.
void YoshiTurret::dieWhenPassed() {
    if (mode != kDying && mode != kCarbonised && image != nullptr &&
        x - image->spotX() + image->width() < world().camera.x) {
        dead = true;
    }
}

void YoshiTurret::stand() {
    World& w = world();

    fall();

    attackWait_ += 1;
    if (attackWait_ >= attackDelay_ && x <= w.camera.right()) {
        attackWait_ = 0;
        attackDelay_ = 50 + w.random() % 150;
        direction = w.target->x > x ? kFacingRight : kFacingLeft;
        animation.frame = 0;
        mode = kShooting;
        w.media.levelSounds->play(snd::kYoshiShell);
        throwShell();
        return;
    }

    image = enemyImage(direction == kFacingRight ? img::kYoshiWalkRight : img::kYoshiWalkLeft);
    hitboxFromImage();
}

void YoshiTurret::die() {
    World& w = world();

    fall();

    animation.tickUpTo(6, 9);

    if (animation.frame >= 9) {
        if (onEngravingGround(w, x, y)) {
            w.media.effects->engrave(x, y, image);
        }
        dead = true;
    } else {
        image = enemyImage((direction == kFacingLeft ? img::kYoshiDieLeft : img::kYoshiDieRight) +
                           animation.frame);
    }
}

void YoshiTurret::throwShell() {
    animation.tick(15);

    if (animation.frame >= 2) {
        // (Back to standing from the next tick; subFrame is not reset.)
        throwShellFrom(world(), x, y, direction);
        animation.frame = 0;
        mode = kWalking;
        return;
    }

    image = enemyImage((direction == kFacingRight ? img::kYoshiThrowRight : img::kYoshiThrowLeft) +
                       animation.frame);
    hitboxFromImage();
}

void YoshiTurret::hitBy(Shot& shot) {
    Enemy::hitBy(shot);
    sprayBlood(shot, kBloodX, kBloodY);

    if (mode == kDying) {
        world().media.miscSounds->play(miscsnd::kGoreSplat + world().random() % 3);  // one of 3
    }
}

//-----------------------------------------------------------------------------
// Yoshi's shell
//-----------------------------------------------------------------------------

void Shell::update() {
    static const int kSpeed = 6;
    const int camera = world().camera.x;
    const int cameraRight = world().camera.right();

    fall();

    animation.cycle(3, 4);
    image = enemyImage(img::kShell + animation.frame);

    if (solidWall(x - kSpeed, y)) {
        if (bounced_) {
            dead = true;
        } else {
            direction = kFacingRight;
            bounced_ = true;
        }
    } else if (solidWall(x + kSpeed, y)) {
        if (bounced_) {
            dead = true;
        } else {
            direction = kFacingLeft;
            bounced_ = true;
        }
    }

    x += direction == kFacingLeft ? -kSpeed : kSpeed;
    if (x < camera - 100 || x > cameraRight + 60 || y > 520 || y < -50) {
        dead = true;
    }
    hitboxFromImage();
}

//-----------------------------------------------------------------------------
// Tails and the flying Tails
//-----------------------------------------------------------------------------

namespace {

constexpr int kTailsTopSpeed = 3;
constexpr int kTailsFlySpeed = 6;

}  // namespace

Tails::Tails(World& world, bool entersFlying) : Enemy(world), entersFlying_(entersFlying) {
    flyDelay_ = 100 + world.random() % 350;
    treasure = 8;
    health.points = 150;
}

void Tails::aimAt(int targetX) {
    if (targetX > x) {
        direction = kFacingRight;
        heading_ = 1;
    } else {
        direction = kFacingLeft;
        heading_ = -1;
    }
}

void Tails::update() {
    World& w = world();

    // The flying Tails enters from the top of the screen, flying.
    if (entersFlying_ && !placed_) {
        y = -20;
        mode = kShooting;
        swooping_ = false;
        speed_ = 0;
        animation.restart();
        placed_ = true;

        flying_ = true;
        if (w.target != nullptr) {
            goalX_ = w.target->x;
            goalY_ = w.target->y;
        } else {
            goalX_ = x - 10;
            goalY_ = 240;
        }
        aimAt(goalX_);
    }

    if (bleeding > 0) {
        bleeding -= 1;
    }

    switch (mode) {
        case kNormal:
        case kWalking:
            walkAbout();
            break;
        case kDying:
            die();
            break;
        case kCarbonised:
            fall();
            burn(img::kTailsBurnLeft, img::kTailsBurnRight, 11, 5);
            break;
        case kJumping:
        case kFalling:
            fallDown();
            break;
        case kShooting:  // the flight
            fly();
            break;
        default:
            break;
    }
    dieWhenOffscreen();
}

void Tails::walkAbout() {
    World& w = world();

    if (platformAt(x, y) == 0) {
        mode = kFalling;
        dy = 0;
        gravityDelay = 0;
        flying_ = true;
        animation.restart();
        fallDown();
        return;
    }

    if (x - speed_ < minX || solidWall(x - speed_, y)) {
        direction = kFacingRight;
        speed_ = 1;
    } else if (x + speed_ > w.camera.right() || solidWall(x + speed_, y)) {
        direction = kFacingLeft;
        speed_ = 1;
    }

    flyWait_ += 1;
    if (flyWait_ >= flyDelay_) {
        mode = kShooting;
        swooping_ = false;
        speed_ = 0;
        animation.restart();
        flying_ = true;
        goalX_ = w.target->x;
        goalY_ = w.target->y;
        aimAt(goalX_);
        fly();
        return;
    }

    animation.cycle(10 - 2 * speed_, 8);

    if (direction == kFacingRight) {
        walk(speed_);
        image = enemyImage(img::kTailsWalkRight + animation.frame);
    } else {
        walk(-speed_);
        image = enemyImage(img::kTailsWalkLeft + animation.frame);
    }
    if (speed_ < kTailsTopSpeed) {
        speedStep_ += 1;
        if (speedStep_ >= 30) {
            speedStep_ = 0;
            speed_ += 1;
        }
    }
    hitboxFromImage();
}

void Tails::die() {
    World& w = world();

    fall();

    if (flying_) {
        // Falls out of the sky first.
        animation.cycle(4, 2);
        if (direction == kFacingRight) {
            if (!solidWall(x + speed_, y)) {
                walk(speed_);
            }
            image = enemyImage(img::kTailsFallRight + animation.frame);
        } else {
            if (!solidWall(x - speed_, y)) {
                walk(-speed_);
            }
            image = enemyImage(img::kTailsFallLeft + animation.frame);
        }
        const int ground = dy > 0 ? platformAt(x, y + dy) : 0;
        if (ground != 0) {
            dy = 0;
            y = ground;
            flying_ = false;
            animation.frame = 7;
        }
        return;
    }

    animation.tickUpTo(6, 11);

    if (animation.frame >= 11) {
        if (onEngravingGround(w, x, y)) {
            w.media.effects->engrave(x, y, image);
            dead = true;
        } else if (direction == kFacingLeft) {
            // On to ground it can rest on, turning back at a wall (D-82
            // fixed: 2002 slid one facing left to the right while it kept
            // facing left, and stopped one facing right at the wall).
            if (solidWall(x - speed_, y)) {
                direction = kFacingRight;
            } else {
                walk(-speed_);
            }
        } else if (solidWall(x + speed_, y)) {
            direction = kFacingLeft;
        } else {
            walk(speed_);
        }
    } else if (direction == kFacingLeft) {
        if (!solidWall(x - speed_, y)) {
            walk(-speed_);
        }
        image = enemyImage(img::kTailsDieLeft + animation.frame);
    } else {
        if (!solidWall(x + speed_, y)) {
            walk(speed_);
        }
        image = enemyImage(img::kTailsDieRight + animation.frame);
    }
}

void Tails::land(int newDirection) {
    direction = newDirection;
    speed_ = 1;
    mode = kWalking;
    flyWait_ = 0;
    flyDelay_ = 100 + world().random() % 350;
    flying_ = false;
    animation.restart();
    walkAbout();
}

void Tails::fly() {
    World& w = world();

    // A swoop ends at a wall or at the screen edges. (The flying Tails
    // checks only the side it faces, rising or swooping.)
    const bool leftBlocked = x - speed_ < minX || solidWall(x - speed_, y);
    const bool rightBlocked = x + speed_ > w.camera.right() || solidWall(x + speed_, y);
    if (entersFlying_) {
        if (direction == kFacingLeft && leftBlocked) {
            land(kFacingRight);
            return;
        }
        if (direction == kFacingRight && rightBlocked) {
            land(kFacingLeft);
            return;
        }
    } else {
        if (swooping_ && leftBlocked) {
            land(kFacingRight);
            return;
        }
        if (swooping_ && rightBlocked) {
            land(kFacingLeft);
            return;
        }
    }

    animation.frame += 1;
    animation.frame %= 2;
    image =
        enemyImage((direction == kFacingRight ? img::kTailsFlyRight : img::kTailsFlyLeft) + animation.frame);

    if (swooping_) {
        // Swoops down on the goal, faster as it gets closer.
        if (goalY_ <= y || y >= w.collision.platforms[0][x]) {
            speed_ = kTailsFlySpeed;
        } else if (goalY_ - 80 > y && y > goalY_ - 110) {
            y += 4;
            speed_ = 1;
        } else if (goalY_ - 60 > y && y >= goalY_ - 80) {
            y += 3;
            speed_ = 2;
        } else if (goalY_ - 40 > y && y >= goalY_ - 60) {
            y += 2;
            speed_ = 3;
        } else if (goalY_ - 20 > y && y >= goalY_ - 40) {
            y += 1;
            speed_ = 4;
        } else {
            y += 1;
            speed_ = 5;
        }
        x += speed_ * heading_;
    } else {
        // Rises (or drops) to 100 to 110 pixels above the goal.
        if (y < goalY_ - 110) {
            y += 5;
        } else if (y > goalY_ - 100) {
            y -= 3;
        } else {
            swooping_ = true;
        }
    }
    hitboxFromImage();
}

void Tails::fallDown() {
    fall();

    animation.cycle(3, 2);

    const int ground = dy > 0 ? platformAt(x, y + dy) : 0;
    if (ground != 0) {
        mode = kNormal;
        dy = 0;
        y = ground;
        flying_ = false;
        animation.restart();
        walkAbout();
        return;
    }

    if (direction == kFacingRight) {
        x += speed_;
        image = enemyImage(img::kTailsFlyRight + animation.frame);
    } else {
        x -= speed_;
        image = enemyImage(img::kTailsFlyLeft + animation.frame);
    }
    hitboxFromImage();
}

void Tails::hitBy(Shot& shot) {
    Enemy::hitBy(shot);
    sprayBlood(shot, kBloodX, kBloodY);

    if (mode == kDying) {
        world().media.miscSounds->play(miscsnd::kDeathCry3);
    }
}

//-----------------------------------------------------------------------------
// Knuckles
//-----------------------------------------------------------------------------

namespace {

constexpr int kKnucklesTopSpeed = 3;
constexpr int kKnucklesChargeSpeed = 6;
constexpr int kKnucklesJumpSpeed = 12;
constexpr int kKnucklesMaxFallSpeed = 7;

}  // namespace

Knuckles::Knuckles(World& world) : Enemy(world) {
    chargeDelay_ = 100 + world.random() % 150;
    treasure = 7;
    health.points = 150;
}

void Knuckles::update() {
    if (bleeding > 0) {
        bleeding -= 1;
    }

    switch (mode) {
        case kNormal:
        case kWalking:
            walkAbout();
            break;
        case kDying:
            die();
            break;
        case kCarbonised:
            heavyFall();
            burn(img::kKnucklesBurnLeft, img::kKnucklesBurnRight, 10, 5);
            break;
        case kJumping:
            jump();
            break;
        case kFalling:
            fallDown();
            break;
        case kShooting:  // the charge
            charge();
            break;
        default:
            break;
    }
    dieWhenOffscreen();
}

void Knuckles::heavyFall() {
    gravityDelay += 1;
    gravityDelay %= kGravityDelay;

    if (gravityDelay == 0 && dy < kKnucklesMaxFallSpeed) {
        dy += 1;
    }
    if (dy < 0 && solidWall(x, y + dy)) {
        dy = kKnucklesMaxFallSpeed;
    }
    const int ground = platformAt(x, y + dy);
    if (ground != 0 && dy > 0) {
        y = ground;
    } else {
        y += dy;
    }
}

void Knuckles::walkAbout() {
    World& w = world();

    if (platformAt(x, y) == 0) {
        mode = kFalling;
        dy = 0;
        gravityDelay = 0;
        fallDown();
        return;
    }

    if (x - speed_ < minX || solidWall(x - speed_, y)) {
        direction = kFacingRight;
        speed_ = 1;
    } else if (x + speed_ > w.camera.right() || solidWall(x + speed_, y)) {
        direction = kFacingLeft;
        speed_ = 1;
    }

    chargeWait_ += 1;
    if (chargeWait_ >= chargeDelay_ && w.target != nullptr) {
        const Entity& target = *w.target;
        if (sameLayer(w, target, *this) &&
            ((direction == kFacingRight && x > target.x - 200 && target.x > x) ||
             (direction == kFacingLeft && x < target.x + 200 && target.x < x))) {
            mode = kShooting;
            w.media.levelSounds->play(snd::kKnucklesCharge);
            speed_ = kKnucklesChargeSpeed;
            animation.restart();
            charge();
            return;
        }
        if (target.y < y && target.x > x - 30 && target.x < x + 30) {
            mode = kJumping;
            speed_ = 0;
            animation.restart();
            gravityDelay = 0;  // otherwise the jumps differ in height
            dy = 0;
            w.media.levelSounds->play(snd::kKnucklesJump);
            jump();
            return;
        }
        chargeWait_ = 0;
        chargeDelay_ = 10 + w.random() % 10;
    }

    animation.cycle(10 - 2 * speed_, 8);

    if (direction == kFacingRight) {
        walk(speed_);
        image = enemyImage(img::kKnucklesWalkRight + animation.frame);
    } else {
        walk(-speed_);
        image = enemyImage(img::kKnucklesWalkLeft + animation.frame);
    }
    if (speed_ < kKnucklesTopSpeed) {
        speedStep_ += 1;
        if (speedStep_ >= 20) {
            speedStep_ = 0;
            speed_ += 1;
        }
    }
    hitboxFromImage();
}

void Knuckles::die() {
    heavyFall();

    animation.tickUpTo(6, 10);

    if (animation.frame >= 10) {
        // Engraved on the ground the other enemies use, not on layer 4 (D-83
        // fixed: the 2002 test was commented out).
        if (onEngravingGround(world(), x, y)) {
            world().media.effects->engrave(x, y, image);
            dead = true;
        } else if (direction == kFacingLeft && !solidWall(x - speed_, y)) {
            x -= speed_;
        } else if (!solidWall(x + speed_, y)) {
            x += speed_;
        }
    } else if (direction == kFacingLeft) {
        if (!solidWall(x - speed_, y)) {
            x -= speed_;
        }
        image = enemyImage(img::kKnucklesDieLeft + animation.frame);
    } else {
        if (!solidWall(x + speed_, y)) {
            x += speed_;
        }
        image = enemyImage(img::kKnucklesDieRight + animation.frame);
    }
}

void Knuckles::backToWalk() {
    mode = kWalking;
    chargeWait_ = 0;
    chargeDelay_ = 50 + world().random() % 150;
    animation.restart();
    walkAbout();
}

void Knuckles::jump() {
    // Crouches for 60 ticks, then jumps.
    animation.subFrame += 1;
    animation.subFrame %= 60;
    if (animation.subFrame == 0 && animation.frame == 0) {
        animation.frame += 1;
        dy = -kKnucklesJumpSpeed;
    }

    heavyFall();

    const int ground = dy > 0 ? platformAt(x, y + dy) : 0;
    if (ground != 0 && animation.frame > 0) {
        mode = kNormal;
        dy = 0;
        y = ground;
        speed_ = 1;
        chargeWait_ = 0;
        animation.restart();
        chargeDelay_ = 50 + world().random() % 150;
        walkAbout();
        return;
    }

    image = enemyImage((direction == kFacingLeft ? img::kKnucklesJumpLeft : img::kKnucklesJumpRight) +
                       animation.frame);
    hitboxFromImage();
}

void Knuckles::charge() {
    heavyFall();

    // Slows down by one every 10 ticks; two bursts of 40 ticks.
    animation.subFrame += 1;
    animation.subFrame %= 40;
    if (animation.subFrame % 10 == 0) {
        speed_ -= 1;
    }
    if (animation.frame == 2) {
        speed_ = 1;
        backToWalk();
        return;
    }
    if (animation.subFrame == 0) {
        animation.frame += 1;
        speed_ = kKnucklesChargeSpeed - 1;
    }

    if (direction == kFacingLeft) {
        if (solidWall(x - speed_, y)) {
            backToWalk();  // (keeping the charge speed, as in 2002)
            return;
        }
        walk(-speed_);
        image = enemyImage(img::kKnucklesChargeLeft + animation.frame);
    } else {
        if (solidWall(x + speed_, y)) {
            backToWalk();
            return;
        }
        walk(speed_);
        image = enemyImage(img::kKnucklesChargeRight + animation.frame);
    }
    hitboxFromImage();
}

void Knuckles::fallDown() {
    heavyFall();

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
        image = enemyImage(img::kKnucklesWalkRight);
    } else {
        x -= speed_;
        image = enemyImage(img::kKnucklesWalkLeft);
    }
    hitboxFromImage();
}

void Knuckles::hitBy(Shot& shot) {
    Enemy::hitBy(shot);
    sprayBlood(shot, kBloodX, kBloodY);

    if (mode == kDying) {
        world().media.miscSounds->play(miscsnd::kDeathCry3);
    }
}

}  // namespace bb::game::video
