// Lara's room (ennemi_lara.cpp, ennemi_pic.cpp, ennemi_poid.cpp,
// ennemi_tete_rayman.cpp, tir_epine.cpp as Lara uses it).

#include <algorithm>

#include "bb/game/gore.hpp"
#include "bb/game/trig.hpp"
#include "videoboss/videoboss_world.hpp"

namespace bb::game::videoboss {

namespace {

constexpr int kAttackDelay = 10;
constexpr int kSpineSpeed = 10;

// Her bust: pressing the button (stage 2), then drawing (stage 3).
const int kButton[] = {0, 0, 1, 2, 3, 3, 3, 2, 1, 0};
const int kDraw[] = {0, 0, 0, 1, 1, 2, 3, 3, 4, 4, 4};

}  // namespace

//-----------------------------------------------------------------------------
// Lara
//-----------------------------------------------------------------------------

Lara::Lara(World& world) : Enemy(world) {
    health.points = 15000;
    image = enemyImage(img::kLaraBody);
}

void Lara::update() {
    if (bleeding > 0) {
        bleeding -= 1;
    }

    switch (mode) {
        case kNormal:
        case kWalking:
        case kJumping:
        case kFalling:
            stand();
            break;
        case kCarbonised:
        case kDying:
            // (She has no hitbox: never reached.)
            world().flags[kStage] = 5;
            dead = true;
            break;
        case kShooting:
            shoot();
            break;
        default:
            break;
    }
}

void Lara::stand() {
    World& w = world();

    if (!spikesLaid_) {
        // Three rows of six, from the level's "videoboss.laraSpikes": the x
        // of the first column (outer rows, middle row), then the rows' y.
        for (int row = 2; row >= 0; row--) {
            for (int column = 0; column < 6; column++) {
                auto* spike = new Spikes(w);
                spike->x = w.marker("videoboss.laraSpikes", row != 1 ? 0 : 1) + column * 65 - row * 20;
                spike->y = w.marker("videoboss.laraSpikes", 2 + static_cast<std::size_t>(row));
                spike->direction = kFacingLeft;
                w.enemies.add(spike);
            }
        }
        spikesLaid_ = true;
    }

    switch (w.flags[kStage]) {
        case 1:
            // Throws Rayman's head and waits.
            if (!headThrown_ && w.target != nullptr) {
                auto* head = new RaymanHead(w);
                head->x = w.target->x + 10;
                head->y = w.target->y - 30;
                head->dy = -8;
                head->direction = kFacingLeft;
                w.enemies.add(head);
                headThrown_ = true;
            }
            break;
        case 2:
            if (animation.tick(6)) {
                if (animation.frame > 9) {
                    w.flags[kStage] = 3;
                    animation.frame = 0;
                    w.flags[Flag::Timer] = 180;
                } else {
                    bust_ = img::kLaraBustButton + kButton[animation.frame];
                }
            }
            break;
        case 3:
            if (animation.tick(6)) {
                if (animation.frame > 10) {
                    w.flags[kStage] = 4;
                    animation.frame = 0;
                } else {
                    bust_ = img::kLaraBustDraw + kDraw[animation.frame];
                }
            }
            break;
        case 4:
            aim();
            break;
        case 7:
            dead = true;
            w.flags[Flag::Kills] += 1;
            break;
        default:
            break;
    }
}

void Lara::fire(int gunX, int gunY, int extraX) {
    World& w = world();
    int dx = w.target->x + extraX - x + gunX;
    int dyAim = w.target->y - y + gunY;
    // At least 1: the 2002 code divided by zero when the target stood on
    // the gun (D-43).
    const int distance = std::max(1, squareRoot(dx * dx + dyAim * dyAim));
    dx = (dx * kSpineSpeed) / distance;
    dyAim = (dyAim * kSpineSpeed) / distance;

    auto* spine = new LaraSpine(w, dx, dyAim + w.random() % 3 - 1);
    spine->x = x - gunX;
    spine->y = y - gunY;
    w.enemyShots.add(spine);
}

void Lara::aim() {
    World& w = world();

    attackWait_ += 1;
    if (attackWait_ <= kAttackDelay || w.target == nullptr) {
        return;
    }
    const int dyTarget = y - 102 - w.target->y;
    const int dxTarget = x - w.target->x;
    if (dyTarget == 0) {
        return;
    }

    // The slope (x10) picks the gun position and the bust image.
    const int slope = (dxTarget * 10) / dyTarget;
    if (slope > 40) {
        fire(60, 144, 20);
        bust_ = img::kLaraAim4;
    } else if (slope > 30) {
        bust_ = img::kLaraAim1;
        fire(58, 154, 20);
    } else if (slope > 20) {
        fire(57, 165, 20);
        bust_ = img::kLaraAim2;
    } else if (slope > 5) {
        bust_ = img::kLaraAim3;
        fire(53, 175, 20);
    } else if (slope > -15) {
        // Too close to shoot.
        bust_ = img::kLaraAim4;
        return;
    } else if (slope > -40) {
        fire(59, 129, 20);
        bust_ = img::kLaraAim5;
    } else {
        fire(60, 144, 0);
        bust_ = img::kLaraAim4;
    }
    animation.subFrame = 0;
    animation.frame = 1;
    mode = kShooting;
    shoot();
}

void Lara::shoot() {
    // Five more bust images, each held one tick longer.
    if (animation.tick(animation.frame)) {
        if (animation.frame >= 7) {
            mode = kWalking;
            animation.restart();
            bust_ -= 5;
            attackWait_ = 0;
            stand();
            return;
        }
        bust_ += 1;
    }
}

void Lara::draw() {
    if (world().flags[kStage] < 6) {
        drawAt(x, y - 102, enemyImage(bust_));
    }
    Actor::draw();
}

//-----------------------------------------------------------------------------
// The spikes
//-----------------------------------------------------------------------------

Spikes::Spikes(World& world) : Enemy(world) {
    health.points = 1;
    image = enemyImage(img::kSpikes);
}

void Spikes::spikeHitbox() {
    hitboxFromImage();
    hitbox.x2 += 25;
    hitbox.x1 += 25;
}

void Spikes::update() {
    World& w = world();
    const int stage = w.flags[kStage];

    if (stage == 3 || stage == 2) {
        // Rises 104 pixels, slower near the top.
        int step = 0;
        if (-104 > risen_) {
            step = 0;
        } else if (-96 > risen_) {
            step = 1;
        } else if (-86 > risen_) {
            step = 2;
        } else if (-66 > risen_) {
            step = 4;
        } else if (-46 > risen_) {
            step = 6;
        } else if (-26 > risen_) {
            step = 8;
        } else if (0 >= risen_) {
            step = 10;
        }
        y -= step;
        risen_ -= step;
        spikeHitbox();
    } else if (stage >= 5) {
        // Sinks, faster near the top.
        if (-104 > risen_) {
            y += 10;
            risen_ += 10;
        } else if (-96 > risen_) {
            y += 8;
            risen_ += 8;
        } else if (-86 > risen_) {
            y += 6;
            risen_ += 6;
        } else if (-66 > risen_) {
            y += 4;
            risen_ += 4;
        } else if (-46 > risen_) {
            y += 3;
            risen_ += 3;
        } else if (-26 > risen_) {
            y += 2;
            risen_ += 2;
        } else if (0 >= risen_) {
            y += 1;
            risen_ += 1;
        } else {
            // Gone: her wall opens ("videoboss.laraWall": the column, the
            // first and the last row).
            const int column = w.marker("videoboss.laraWall", 0);
            for (int row = w.marker("videoboss.laraWall", 1); row <= w.marker("videoboss.laraWall", 2);
                 row++) {
                w.collision.solidWalls[row][column] = false;
            }
            dead = true;
        }
        spikeHitbox();
    }
}

void Spikes::draw() {
    // Only the part that has risen.
    const Image* spikes = enemyImage(img::kSpikes);
    world().media.canvas->drawPart(spikes, 0, 0, 39, -risen_, x - world().camera.x, y);
}

//-----------------------------------------------------------------------------
// The weight
//-----------------------------------------------------------------------------

Weight::Weight(World& world) : Enemy(world) {
    health.points = 1000;
    image = enemyImage(img::kWeight);
}

void Weight::update() {
    World& w = world();

    if (mode == kDying) {
        crush();
        return;
    }

    image = enemyImage(img::kWeight);
    const int stage = w.flags[kStage];
    if (stage < 3) {
        y = -100;
        x = w.marker("videoboss.laraWeightX");
    }
    if (stage == 3 || stage == 4) {
        if (y < 100) {
            fallThrough();
        } else {
            dy = 0;
        }
        if (x > world().marker("videoboss.laraExitX")) {
            mode = kDying;
        }
    }

    if (pushes_ >= 1) {
        x += 3;
        pushes_ -= 1;
    }
    hitboxFromImage();
}

void Weight::splash(int count) {
    World& w = world();
    for (int i = 0; i < count; i++) {
        const int speedX = w.random() % 15 - 7;
        const int speedY = -1 - w.random() % 10;
        auto* drop = new GoreDrop(w, speedX, speedY);
        drop->x = x + w.random() % 80 - 40;
        drop->y = y - w.random() % 30 - 10;
        w.blood.add(drop);
    }
}

void Weight::crush() {
    World& w = world();

    if (solidWall(x, y + dy)) {
        // Landed on her.
        w.flags[kStage] = 7;
        animation.subFrame += 1;
        animation.subFrame %= 15;
        if (animation.frame == 0 && animation.subFrame == 1) {
            w.shake.start(8);
        }
        if (animation.subFrame == 0) {
            animation.frame += 1;
            if (animation.frame >= 3) {
                w.media.effects->engrave(x, y, image);
                dead = true;
            }
        }
        if (animation.frame < 3) {
            image = enemyImage(img::kWeightLanded + animation.frame);
        }
        return;
    }

    // Falling: the gore grows as the ground nears.
    fallThrough();
    if (solidWall(x, y + dy * 10)) {
        image = enemyImage(img::kWeightCrush3);
        splash(8);
    } else if (solidWall(x, y + dy * 15)) {
        image = enemyImage(img::kWeightCrush2);
        splash(6);
    } else if (solidWall(x, y + dy * 20)) {
        image = enemyImage(img::kWeightCrush2);
        splash(5);
    } else if (solidWall(x, y + dy * 25)) {
        w.flags[kStage] = 6;
        image = enemyImage(img::kWeightCrush1);
        splash(4);
    } else if (solidWall(x, y + dy * 30)) {
        image = enemyImage(img::kWeightCrush1);
        splash(3);
    } else {
        image = enemyImage(img::kWeightFalling);
    }
}

void Weight::hitBy(Shot& shot) {
    // Pushed by the shots going right; it takes no damage.
    if (shot.heading() >= 2 && shot.heading() <= 6) {
        pushes_ += 1;
    }
    shot.onHit(0);
}

//-----------------------------------------------------------------------------
// Rayman's head
//-----------------------------------------------------------------------------

RaymanHead::RaymanHead(World& world) : Enemy(world) {
    health.points = 1;
    image = enemyImage(img::kRaymanHeadRoll);
}

void RaymanHead::update() {
    if (bleeding > 0) {
        bleeding -= 1;
    }

    switch (mode) {
        case kNormal:
        case kWalking:
        case kJumping:
        case kFalling:
            roll();
            break;
        case kCarbonised:
        case kDying:
            die();
            break;
        default:
            break;
    }
}

void RaymanHead::roll() {
    World& w = world();

    x += 3;
    fall();
    hitboxFromImage();

    if (animation.tick(4)) {
        animation.frame %= 8;
        image = enemyImage(img::kRaymanHeadRoll + animation.frame);
    }

    if (dy > 0 && platformAt(x, y + dy) != 0) {
        w.media.effects->engrave(x, y, image);
        w.flags[kStage] = 2;
        dead = true;
    }
}

void RaymanHead::die() {
    if (animation.tick(6)) {
        if (animation.frame >= 6) {
            world().flags[kStage] = 2;
            dead = true;
        } else {
            image = enemyImage(img::kRaymanHeadDie + animation.frame);
        }
    }
}

void RaymanHead::hitBy(Shot& shot) {
    static const int kBloodX[] = {0, 10, 15, 10, 0, -10, -15, -10};
    static const int kBloodY[] = {-15, -25, -25, -25, -35, -25, -25, -25};

    Enemy::hitBy(shot);
    shot.onHit(9999);
    sprayBlood(shot, kBloodX, kBloodY);
}

//-----------------------------------------------------------------------------
// Lara's spine
//-----------------------------------------------------------------------------

void LaraSpine::update() {
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
