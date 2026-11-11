// Small Mario and Luigi (ennemi_mariotapette.cpp, ennemi_luigi.cpp) and the
// plunger (tir_debouche_chiote.cpp).

#include "bb/game/gore.hpp"
#include "bb/game/level_event.hpp"
#include "videoboss/videoboss_world.hpp"

namespace bb::game::videoboss {

namespace {

constexpr int kRecoilGround = 20;  // MULTIPLICATEUR_RECUL_SOL
constexpr int kSpeedGround = 100;  // MULTIPLACATEUR_VITESSE_AU_SOL
constexpr int kRecoilAir = 15;     // MULTIPLICATEUR_RECUL_AERIEN
constexpr int kSpeedAir = 80;      // MULTIPLICATEUR_VITESSE_AERIEN
constexpr int kJumpGravityDelay = 12;

constexpr Flag kLuigiState = Flag::User1;  // 1 at the pit, 2 leaving, 3 bleeding
constexpr Flag kLuigiHere = Flag::User3;

bool sameLayer(World& w, const Entity& a, const Entity& b) {
    return w.collision.platformLayerAt(a.x, a.y) == w.collision.platformLayerAt(b.x, b.y);
}

}  // namespace

//-----------------------------------------------------------------------------
// Plumber
//-----------------------------------------------------------------------------

void Plumber::hitBy(Shot& shot) {
    static const int kBloodX[] = {0, 10, 15, 10, 0, -10, -15, -10};
    static const int kBloodY[] = {-15, -25, -25, -25, -35, -25, -25, -25};
    World& w = world();
    const int heading = shot.heading();
    const bool fromLeft = heading >= 2 && heading <= 6;
    const bool fromRight = heading >= 10 && heading <= 14;

    for (int i = 0; i < 4; i++) {
        int speedX = 0;
        if (fromLeft) {
            speedX = w.random() % 6;
        } else if (fromRight) {
            speedX = (-w.random()) % 5 - 1;
        } else {
            speedX = w.random() % 11 - 5;
        }
        const int speedY = -2 - w.random() % 6;
        auto* drop = new GoreDrop(w, speedX, speedY);
        drop->x = x + w.random() % 11 - 5;
        drop->y = y - w.random() % 30 - 10;
        w.blood.add(drop);
    }

    if (fromLeft) {
        push_ += shot.damage();
    } else if (fromRight) {
        push_ -= shot.damage();
    }

    Enemy::hitBy(shot);
    sprayBlood(shot, kBloodX, kBloodY);
}

void Plumber::groundPush(int topSpeed) {
    if (direction == kFacingRight) {
        if (push_ < 0) {
            if (speed_ > -push_ / kSpeedGround) {
                speed_ += push_ / kSpeedGround;
            } else {
                speed_ = 0;
            }
        } else if (speed_ + push_ / kSpeedGround < topSpeed) {
            speed_ += push_ / kSpeedGround;
        } else {
            speed_ = topSpeed;
        }
    } else {
        if (push_ > 0) {
            if (speed_ > push_ / kSpeedGround) {
                speed_ -= push_ / kSpeedGround;
            } else {
                speed_ = 0;
            }
        } else if (speed_ - push_ / kSpeedGround < topSpeed) {
            speed_ -= push_ / kSpeedGround;
        } else {
            speed_ = topSpeed;
        }
    }
}

void Plumber::groundRecoil(int rightLimit) {
    if (push_ >= kRecoilGround) {
        if (x + push_ / kRecoilGround < rightLimit) {
            x += push_ / kRecoilGround;
        }
    } else if (push_ <= -kRecoilGround) {
        if (x + push_ / kRecoilGround > minX) {
            x += push_ / kRecoilGround;
        }
    }
}

void Plumber::airPush(int topSpeed) {
    if (push_ != 0) {
        x += push_ / kRecoilAir;
    }
    if (direction == kFacingRight) {
        if (push_ < 0) {
            if (speed_ + push_ / kSpeedAir < 0 && speed_ + push_ / kSpeedAir < -2 * topSpeed) {
                speed_ = -2 * topSpeed;
            } else {
                speed_ += push_ / kSpeedAir;
            }
        } else if (speed_ + push_ / kSpeedAir < 2 * topSpeed) {
            speed_ += push_ / kSpeedAir;
        } else {
            speed_ = 2 * topSpeed;
        }
    } else {
        if (push_ > 0) {
            if (speed_ - push_ / kSpeedAir < 0 && speed_ - push_ / kSpeedAir < -2 * topSpeed) {
                speed_ = -2 * topSpeed;
            } else {
                speed_ -= push_ / kSpeedAir;
            }
        } else if (speed_ - push_ / kSpeedAir < 2 * topSpeed) {
            speed_ -= push_ / kSpeedAir;
        } else {
            speed_ = 2 * topSpeed;
        }
    }
}

void Plumber::floatUp() {
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

bool Plumber::jumpAt(int targetX, int targetY, int delayBase, int delayRange) {
    World& w = world();
    const int dyTarget = targetY - y;
    const int dxTarget = targetX - x;

    // A target above and ahead, closer than 45 degrees to the vertical.
    int speed = 0;
    if (direction == kFacingRight && dyTarget < 0 && dxTarget > 0 && dyTarget + dxTarget < 0) {
        speed = (dxTarget * -9) / dyTarget;
    } else if (direction == kFacingLeft && dyTarget < 0 && dxTarget < 0 && dyTarget - dxTarget < 0) {
        speed = (dxTarget * 9) / dyTarget;
    } else {
        return false;
    }
    animation.restart();
    attackWait_ = 0;
    attackDelay_ = delayBase + w.random() % delayRange;
    speed_ = speed;
    dy = (speed_ - 36) / 4;  // int(-9 + speed_ * 0.25) in 2002 (D-56)
    mode = kJumping;
    gravityDelay = 0;  // otherwise the jumps differ in height
    return true;
}

//-----------------------------------------------------------------------------
// Small Mario
//-----------------------------------------------------------------------------

namespace {

constexpr int kMarioTopSpeed = 5;
constexpr int kMarioChargeSpeed = 7;
constexpr int kShrinkTicks = 300;  // NABO_TRANSFORMATION
// Positions in the arena of Mario, Luigi and Sonic are offsets from its
// left edge, the level's "videoboss.arenaX" (1280 in videoboss).
constexpr int kEnterX = 680;                    // where Luigi, the car and Sonic come in
constexpr int kPitLeft = 220, kPitRight = 270;  // over the pit
const int kMarioWalk[] = {0, 1, 2, 1};
const int kSmallWalk[] = {0, 1};
const int kShrinkRight[] = {img::kMarioWalkRight, img::kSmallMarioRight};
const int kShrinkLeft[] = {img::kMarioWalkLeft, img::kSmallMarioLeft};

}  // namespace

SmallMario::SmallMario(World& world) : Plumber(world) {
    attackDelay_ = 50 + world.random() % 250;
    health.points = 10000;
    minX = world.marker("videoboss.arenaX");
    image = enemyImage(img::kMarioWalkLeft);
}

void SmallMario::update() {
    if (world().flags[kStage] < 1) {
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
            die();
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
}

void SmallMario::walkAbout() {
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
    if (attackWait_ >= attackDelay_ && w.target != nullptr) {
        const Entity& target = *w.target;
        if (sameLayer(w, target, *this) &&
            ((direction == kFacingRight && x < target.x) || (direction == kFacingLeft && x > target.x))) {
            animation.restart();
            chargeStep_ = 0;
            speed_ = 0;
            dy = 0;
            mode = kShooting;
            charge();
            return;
        }
        if (jumpAt(target.x, target.y, 50, 250)) {
            jump();
            return;
        }
    }

    if (direction == kFacingRight) {
        walk(speed_);
        image = enemyImage(img::kMarioWalkRight + animation.loop(kMarioWalk, 4, 16 - 3 * speed_));
    } else {
        walk(-speed_);
        image = enemyImage(img::kMarioWalkLeft + animation.loop(kMarioWalk, 4, 16 - 3 * speed_));
    }
    groundPush(kMarioTopSpeed);
    groundRecoil(w.camera.right());
    push_ = 0;

    if (speed_ < kMarioTopSpeed) {
        speedStep_ += 1;
        if (speedStep_ >= 20) {
            speedStep_ = 0;
            speed_ += 1;
        }
    }
    hitboxFromImage();
}

void SmallMario::die() {
    World& w = world();

    fall();
    if (!small_) {
        // Shrinks.
        shrinkWait_ += 1;
        if (shrinkWait_ >= kShrinkTicks) {
            speed_ = 1;
            small_ = true;
        } else if (direction == kFacingRight) {
            image = enemyImage(kShrinkRight[animation.loop(kSmallWalk, 2, 6)]);
        } else {
            image = enemyImage(kShrinkLeft[animation.loop(kSmallWalk, 2, 6)]);
        }
    } else if (w.flags[kStage] >= 3) {
        // Walks about, small and harmless.
        if (!luigiCalled_) {
            if (x - speed_ < minX || solidWall(x - speed_, y)) {
                direction = kFacingRight;
                speed_ = 2;
            } else if (x + speed_ > w.camera.right() || solidWall(x + speed_, y)) {
                direction = kFacingLeft;
                speed_ = 2;
            }
        }
        if (direction == kFacingRight) {
            walk(speed_);
            image = enemyImage(img::kSmallMarioRight + animation.loop(kSmallWalk, 2, 16 - 3 * speed_));
        } else {
            walk(-speed_);
            image = enemyImage(img::kSmallMarioLeft + animation.loop(kSmallWalk, 2, 16 - 3 * speed_));
        }
    }

    if (!luigiCalled_ && small_) {
        // Luigi comes; the players hold fire until stage 3.
        w.flags[kLuigiHere] = 1;
        luigiCalled_ = true;
        direction = kFacingRight;
        auto* luigi = new Luigi(w);
        w.enemiesCreated += 1;
        luigi->x = w.marker("videoboss.arenaX") + kEnterX;
        luigi->y = y;
        luigi->direction = kFacingLeft;
        w.enemies.add(luigi);
        HoldFireEvent(static_cast<int>(kStage), 3).run(w);
    }
}

void SmallMario::endCharge() {
    speed_ = 1;
    animation.restart();
    mode = kWalking;
    attackWait_ = 0;
    attackDelay_ = 50 + world().random() % 250;
    push_ = 0;
    walkAbout();
}

void SmallMario::charge() {
    World& w = world();

    speed_ = kMarioChargeSpeed;
    if (x - speed_ < minX || solidWall(x - speed_, y)) {
        direction = kFacingRight;
        endCharge();
        return;
    }
    if (x + speed_ > w.camera.right() || solidWall(x + speed_, y)) {
        direction = kFacingLeft;
        endCharge();
        return;
    }

    // Stamps for 40 ticks (enough shots in the face stop him), then runs.
    chargeStep_ += 1;
    const int walkImage = direction == kFacingRight ? img::kMarioWalkRight : img::kMarioWalkLeft;
    if (chargeStep_ < 40) {
        image = enemyImage(walkImage + animation.loop(kMarioWalk, 4, 3));
        if (push_ > 1000) {
            endCharge();
            return;
        }
    } else {
        walk(direction == kFacingRight ? speed_ : -speed_);
        image = enemyImage(walkImage + animation.loop(kMarioWalk, 4, 2));
    }
    hitboxFromImage();
}

void SmallMario::jump() {
    World& w = world();

    if (dy < 0) {
        floatUp();
    } else {
        fall();
    }

    // Not out of the screen.
    if (x - speed_ < minX || solidWall(x - speed_, y)) {
        direction = kFacingRight;
        speed_ = 1;
    } else if (x + speed_ > w.camera.right() || solidWall(x + speed_, y)) {
        direction = kFacingLeft;
        speed_ = 1;
    }

    const int ground = dy > 0 ? platformAt(x, y + dy) : 0;
    if (ground != 0) {
        mode = kNormal;
        dy = 0;
        y = ground;
        if (speed_ > kMarioTopSpeed) {
            speed_ = kMarioTopSpeed;
        }
        walkAbout();
        return;
    }

    if (!small_) {
        if (direction == kFacingRight) {
            x += speed_;
            image = enemyImage(dy < 0 ? img::kMarioJumpUpRight : img::kMarioJumpDownRight);
        } else {
            x -= speed_;
            image = enemyImage(dy < 0 ? img::kMarioJumpUpLeft : img::kMarioJumpDownLeft);
        }
        airPush(kMarioTopSpeed);
    } else if (direction == kFacingRight) {
        x += speed_;
        image = enemyImage(img::kSmallMarioRight + animation.loop(kSmallWalk, 2, 16 - 3 * speed_));
    } else {
        x -= speed_;
        image = enemyImage(img::kSmallMarioLeft + animation.loop(kSmallWalk, 2, 16 - 3 * speed_));
    }

    push_ = 0;
    hitboxFromImage();
}

//-----------------------------------------------------------------------------
// Luigi
//-----------------------------------------------------------------------------

namespace {

constexpr int kLuigiTopSpeed = 6;
constexpr int kShakeTicks = 180;  // EXPLOSE_DELAY
constexpr int kBleedTicks = 90;   // NOMBRE_GICLURES
const int kLuigiWalk[] = {0, 1, 2, 1};
const int kLuigiDying[] = {0, 1, 2, 3, 4, 3, 2, 1};
// The plunger pair of each volley step: they spread (0 to 3) and close.
const int kSpread[] = {0, 1, 2, 3, 2, 1};

}  // namespace

Luigi::Luigi(World& world) : Plumber(world) {
    attackDelay_ = 50 + world.random() % 250;
    health.points = 10000;
    minX = world.camera.x + 20;
}

void Luigi::update() {
    World& w = world();

    if (w.flags[kStage] == 7) {
        dead = true;
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
            die();
            break;
        case kJumping:
        case kFalling:
            jump();
            break;
        case kShooting:
            volley();
            break;
        default:
            break;
    }
}

void Luigi::walkAbout() {
    World& w = world();

    if (platformAt(x, y) == 0) {
        mode = kFalling;
        dy = 0;
        gravityDelay = 0;
        jump();
    }

    if (x - speed_ < minX + 20 || solidWall(x - speed_, y)) {
        direction = kFacingRight;
        speed_ = 1;
    } else if (x + speed_ > w.camera.right() - 40 || solidWall(x + speed_, y)) {
        direction = kFacingLeft;
        speed_ = 1;
    }

    // He attacks from stage 3; on screen, he sets stage 2.
    if (w.flags[kStage] >= 3) {
        attackWait_ += 1;
    } else if (x < w.camera.right() - 40) {
        w.flags[kStage] = 2;
    }

    if (attackWait_ >= attackDelay_ && w.target != nullptr) {
        const Entity& target = *w.target;
        if (jumpAt(target.x, target.y, 10, 50)) {
            jump();
            return;
        }
        int dyTarget = target.y - y;
        const int dxTarget = target.x - x;
        if (dyTarget == 0) {
            dyTarget = 1;
        }
        const int steepness = dxTarget / dyTarget;
        // A volley at a target ahead, at most about 27 degrees off the
        // horizontal (while he has 1000 HP).
        if (health.points >= 1000 && (steepness <= -2 || steepness >= 2) &&
            ((direction == kFacingRight && dxTarget > 0) || (direction == kFacingLeft && dxTarget < 0))) {
            attackWait_ = 0;
            attackDelay_ = 50 + w.random() % 250;
            animation.restart();
            speed_ = 0;
            volleyStep_ = 0;
            volleyFrame_ = 0;
            mode = kShooting;
            volleyTicks_ = 0;
            w.media.levelSounds->play(snd::kLuigiPlunger);
            volley();
            return;
        }
    }

    if (speed_ > 6) {
        speed_ = 6;
    } else if (speed_ < -6) {
        speed_ = -6;
    }
    if (direction == kFacingRight) {
        walk(speed_);
        image = enemyImage(img::kLuigiWalkRight + animation.loop(kLuigiWalk, 4, 14 - 2 * speed_));
    } else {
        walk(-speed_);
        image = enemyImage(img::kLuigiWalkLeft + animation.loop(kLuigiWalk, 4, 14 - 2 * speed_));
    }
    groundPush(kLuigiTopSpeed);
    groundRecoil(w.camera.right() - 20);
    push_ = 0;

    if (speed_ < kLuigiTopSpeed) {
        speedStep_ += 1;
        if (speedStep_ >= 20) {
            speedStep_ = 0;
            speed_ += 1;
        }
    }
    hitboxFromImage();
}

void Luigi::die() {
    World& w = world();

    switch (w.flags[kLuigiState]) {
        case 0:
            // Walks dying, until over the pit.
            fall();
            if (x - 3 < minX || solidWall(x - 3, y)) {
                direction = kFacingRight;
            } else if (x + 3 > w.camera.right() || solidWall(x + 3, y)) {
                direction = kFacingLeft;
            }
            if (direction == kFacingRight) {
                walk(3);
                image = enemyImage(img::kLuigiDyingRight + animation.loop(kLuigiWalk, 4, 8));
            } else {
                walk(-3);
                image = enemyImage(img::kLuigiDyingLeft + animation.loop(kLuigiWalk, 4, 8));
            }
            if (w.collision.platformLayerAt(x, y) == 0 && x > w.marker("videoboss.arenaX") + kPitLeft &&
                x < w.marker("videoboss.arenaX") + kPitRight) {
                direction = kFacingLeft;
                w.flags[kLuigiState] = 1;
                attackDelay_ = 0;  // (the 2002 code counts in it)
            }
            break;
        case 1:
            attackDelay_ += 1;
            if (attackDelay_ >= kShakeTicks) {
                w.flags[kLuigiState] = 2;
                direction = kFacingRight;
            } else {
                image = enemyImage(img::kLuigiDying + animation.loop(kLuigiDying, 8, 6));
            }
            break;
        case 2:
            walk(3);
            image = enemyImage(img::kLuigiDyingRight + animation.loop(kLuigiWalk, 4, 8));
            if (x > w.camera.right() + 40) {
                w.flags[kLuigiState] = 3;
                attackDelay_ = 0;
            }
            break;
        case 3:
            attackDelay_ += 1;
            if (attackDelay_ >= kBleedTicks) {
                // The car and Sonic come. Sonic counts in the enemies
                // created, the car, which never dies, does not (D-55 fixed:
                // 2002 counted the car instead of Sonic).
                auto* car = new PacManCar(w);
                car->x = w.marker("videoboss.arenaX") + kEnterX;
                car->y = y;
                car->direction = kFacingLeft;
                w.enemies.add(car);

                auto* sonic = new Sonic(w);
                sonic->x = w.marker("videoboss.arenaX") + kEnterX;
                sonic->y = y;
                sonic->direction = kFacingLeft;
                w.enemies.add(sonic);
                w.enemiesCreated += 1;
                dead = true;
            } else {
                for (int i = 0; i < 16; i++) {
                    const int speedX = (-w.random()) % 10 - 2;
                    const int speedY = -1 - w.random() % 10;
                    auto* drop = new GoreDrop(w, speedX, speedY);
                    drop->x = x + w.random() % 20 - 10;
                    drop->y = y - w.random() % 60 - 10;
                    w.blood.add(drop);
                }
            }
            break;
        default:
            break;
    }
}

void Luigi::volley() {
    World& w = world();

    volleyTicks_ += 1;
    if (volleyTicks_ > 275) {
        animation.restart();
        mode = kWalking;
        walkAbout();
        return;
    }

    const bool right = direction == kFacingRight;
    // Aims for 50 ticks.
    if (volleyTicks_ < 50) {
        image = enemyImage(right ? img::kLuigiShootRight : img::kLuigiShootLeft);
        hitboxFromImage();
        return;
    }

    image =
        enemyImage((right ? img::kLuigiShootRight : img::kLuigiShootLeft) + animation.loop(kLuigiWalk, 4, 2));
    volleyStep_ += 1;
    volleyStep_ %= 4;
    if (volleyStep_ == 0) {
        volleyFrame_ += 1;
        volleyFrame_ %= 6;

        // Two plungers, one from his belt and one from his shoulder.
        const int spread = kSpread[volleyFrame_];
        const int first = right ? img::kPlungerRight : img::kPlungerLeft;
        const int speed = right ? 12 - spread : -(12 - spread);
        auto* low = new Plunger(w, first + 3 - spread, speed, spread);
        auto* high = new Plunger(w, first + 3 + spread, speed, -spread);
        low->direction = direction;
        low->x = right ? x + 23 : x - 23;
        low->y = y - 13;
        high->direction = direction;
        high->x = low->x;
        high->y = y - 33;
        w.enemyShots.add(low);
        w.enemyShots.add(high);

        // Recoil.
        if (right) {
            if (x > w.camera.x + 30) {
                x -= 1;
            }
        } else if (x < w.camera.right() - 30) {
            x += 1;
        }
    }
    hitboxFromImage();
}

void Luigi::jump() {
    World& w = world();

    if (dy < 0) {
        floatUp();
    } else {
        fall();
    }

    if (x - speed_ < minX || solidWall(x - speed_, y)) {
        direction = kFacingRight;
        speed_ = 1;
    } else if (x + speed_ > w.camera.right() - 40 || solidWall(x + speed_, y)) {
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
        if (x + speed_ < w.camera.right() - 20) {
            x += speed_;
        }
        image = enemyImage(dy < 0 ? img::kLuigiJumpUpRight : img::kLuigiJumpDownRight);
    } else {
        if (x - speed_ > minX) {
            x -= speed_;
        }
        image = enemyImage(dy < 0 ? img::kLuigiJumpUpLeft : img::kLuigiJumpDownLeft);
    }
    airPush(kLuigiTopSpeed);

    push_ = 0;
    hitboxFromImage();
}

void Luigi::hitBy(Shot& shot) {
    Plumber::hitBy(shot);
    if (health.points <= 0) {
        // Killed: the players hold fire until stage 5.
        HoldFireEvent(static_cast<int>(kStage), 5).run(world());
    }
}

//-----------------------------------------------------------------------------
// The plunger
//-----------------------------------------------------------------------------

void Plunger::update() {
    const int camera = world().camera.x;
    const int cameraRight = world().camera.right();

    x += xSpeed_;
    y += dy;
    if (x < camera - 100 || x > cameraRight + 60 || y > 520 || y < -50) {
        dead = true;
    }
    hitboxFromImage();
}

}  // namespace bb::game::videoboss
