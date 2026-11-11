// The Jedi bear and the Jedi hero (ennemi_bisou_jedi.cpp, .h) and the
// Snork in Black cameo (ennemi_bisou_sib.cpp).

#include "carebears/carebears_world.hpp"

namespace bb::game::carebears {

namespace {

const int kBloodX[] = {0, 7, 10, 7, 0, -7, -10, -7};
const int kBloodY[] = {-15, -25, -25, -25, -35, -25, -25, -25};

constexpr Flag kJediHeroState = Flag::User1;  // 1 arrived (dialogue), 2 dead

}  // namespace

//-----------------------------------------------------------------------------
// The Jedi bear
//-----------------------------------------------------------------------------

Jedi::Jedi(World& world) : Enemy(world) {
    health.points = 250;
    shotDelay_ = 200 + world.random() % 250;
}

void Jedi::update() {
    if (bleeding > 0) {
        bleeding -= 1;
    }
    if (holdsSword) {
        shotWait_ += 1;
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
            burnUp();
            break;
        default:
            break;
    }
    dieWhenOffscreen();
}

void Jedi::walkAbout() {
    static const int kSpeed = 1;
    World& w = world();
    const int camera = w.camera.x;
    const int cameraRight = w.camera.right();

    fall();

    animation.cycle(6, 8);

    if (x - kSpeed < camera + 20 || solidWall(x - kSpeed, y)) {
        direction = kFacingRight;
    } else if (x + kSpeed > cameraRight - 40 || solidWall(x + kSpeed, y)) {
        direction = kFacingLeft;
    }
    if (direction == kFacingRight) {
        walk(kSpeed);
        image = enemyImage(img::kJediWalkRight + animation.frame);
    } else {
        walk(-kSpeed);
        image = enemyImage(img::kJediWalkLeft + animation.frame);
    }

    if (shotWait_ > shotDelay_ && w.target != nullptr) {
        throwSword(true);
    }
    hitboxFromImage();
}

// Throws the lightsaber at the target player; a Jedi that dies with his
// sword throws it too (lost at once when there is no target).
void Jedi::throwSword(bool targetKnown) {
    World& w = world();
    auto* sword = new Lightsaber(w);
    sword->x = x;
    sword->y = y - 40;
    sword->dy = 0;
    sword->xSpeed = -20;
    sword->thrower = this;
    if (targetKnown) {
        sword->target = w.target;
    } else {
        sword->lost = true;
    }
    w.enemyShots.add(sword);
    shotWait_ = 0;
    shotDelay_ = 200 + w.random() % 250;
    holdsSword = false;
}

void Jedi::die() {
    if (holdsSword) {
        throwSword(world().target != nullptr);
    }

    if (animation.tick(4)) {
        if (animation.frame > 20) {
            if (platformAt(x, y) != 0) {
                world().media.effects->engrave(x, y, image);
            }
            dead = true;
            return;
        }
    }
    image =
        enemyImage((direction == kFacingRight ? img::kJediDieRight : img::kJediDieLeft) + animation.frame);
}

void Jedi::burnUp() {
    if (holdsSword) {
        throwSword(world().target != nullptr);
    }

    if (animation.tick(4)) {
        if (animation.frame > 7) {
            dead = true;
            return;
        }
    }
    image =
        enemyImage((direction == kFacingRight ? img::kJediBurnRight : img::kJediBurnLeft) + animation.frame);
}

void Jedi::hitBy(Shot& shot) {
    Enemy::hitBy(shot);
    sprayBlood(shot, kBloodX, kBloodY);
}

void Jedi::draw() {
    // The sword swings with the walk.
    static const int kSwordX[] = {8, 4, -1, -6, -6, -6, -1, 3};
    static const int kSwordY[] = {-19, -19, -19, -19, -18, -18, -19, -19};
    static const int kSwordStep[] = {0, 1, 2, 3, 4, 3, 2, 1};

    Actor::draw();
    if (holdsSword) {
        if (direction == kFacingRight) {
            drawAt(x + kSwordX[animation.frame], y + kSwordY[animation.frame],
                   enemyImage(img::kJediSwordRight + kSwordStep[animation.frame]));
        } else {
            drawAt(x - kSwordX[animation.frame], y + kSwordY[animation.frame],
                   enemyImage(img::kJediSwordLeft + kSwordStep[animation.frame]));
        }
    }
}

//-----------------------------------------------------------------------------
// The Jedi hero
//-----------------------------------------------------------------------------

void JediHero::update() {
    World& w = world();
    if (entering_) {
        if (x > w.camera.right() - 190) {  // walks in to 190 pixels from the right edge
            walkAbout();
            hitbox.active = false;
        } else {
            entering_ = false;
            w.flags[kJediHeroState] = 1;
            w.flags[Flag::GeneratorsOff] = 0;
        }
        return;
    }
    Jedi::update();
}

void JediHero::hitBy(Shot& shot) {
    Jedi::hitBy(shot);
    if (health.points <= 0) {
        world().flags[kJediHeroState] = 2;
        world().flags[Flag::GeneratorsOff] = 1;
    }
}

//-----------------------------------------------------------------------------
// The Snork in Black cameo
//-----------------------------------------------------------------------------

SnorkCameo::SnorkCameo(World& world) : Enemy(world) {
    // Hurt if the Jedi hero has arrived (bisous1's script); clears it.
    if (world.flags[kJediHeroState] != 0) {
        mode = kWaitingHurt;
        world.flags[kJediHeroState] = 0;
    } else {
        mode = kWaiting;
    }
}

void SnorkCameo::update() {
    static const int kRun[] = {0, 1, 2, 1};
    World& w = world();

    fall();

    switch (mode) {
        case kWaitingHurt:
            image = enemyImage(img::kSnorkHurt);
            hitboxFromImage();
            if (x < w.camera.right() - 200 || w.flags[kJediHeroState] == 0) {
                mode = kGoingIn;
            }
            break;
        case kWaiting:
            image = enemyImage(img::kSnorkWaiting);
            hitboxFromImage();
            if (x - w.target->x < 200) {
                mode = kRunning;
            }
            break;
        case kGoingIn:
            if (animation.tick(9)) {
                if (animation.frame == 5) {
                    animation.restart();
                    mode = kRunning;
                } else {
                    image = enemyImage(img::kSnorkHurt + animation.frame);
                }
            }
            noHitbox();
            break;
        case kRunning:
            image = enemyImage(img::kSnorkRun + animation.loop(kRun, 4, 5));
            walk(3);
            noHitbox();
            break;
        default:
            break;
    }
    dieWhenOffscreen();
}

void SnorkCameo::hitBy(Shot&) {
    // Shots go through; after 300 hits it runs away.
    hits_ += 1;
    if (hits_ > 300) {
        if (mode == kWaitingHurt) {
            mode = kGoingIn;
        } else if (mode == kWaiting) {
            mode = kRunning;
        }
        noHitbox();
    }
}

}  // namespace bb::game::carebears
