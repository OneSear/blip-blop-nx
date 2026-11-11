// Rayman and what he throws or summons (ennemi_rayman.cpp,
// tir_rayman_poing.cpp, morceau_rayman.cpp, morceau_tete_rayman.cpp,
// ennemi_pikashu_heros.cpp, ennemi_bulbizarre_heros.cpp,
// ennemi_flameche_heros.cpp, ennemi_com_heros.cpp).

#include <cstdlib>

#include "video/video_world.hpp"

namespace bb::game::video {

namespace {

// The level script's registers.
constexpr Flag kStage = Flag::User0;   // 1 a limb is flying, 2 fighting
constexpr Flag kHeroes = Flag::User1;  // the hero summons (4 down to 0)
constexpr Flag kLimb = Flag::User2;    // the last limb landed: 1, 2 fists, 3 feet
constexpr Flag kArena = Flag::User3;   // 1 in the arena, 2 dead

// His arena's left edge and entry are the level's "video.raymanArena".

constexpr int kSpeed = 3;
constexpr int kCriticalSpeed = 6;
constexpr int kFist1Speed = 7;
constexpr int kFist2Speed = 10;
constexpr int kBelowFistsHp = 10000;  // PV_SOUS_CRITIQUE
constexpr int kCriticalHp = 5000;
constexpr int kAboveDeathHp = 1000;  // PV_SUR_CRITIQUE

constexpr int kHeroMusic = 1;

bool sameLayer(World& w, const Entity& a, const Entity& b) {
    return w.collision.platformLayerAt(a.x, a.y) == w.collision.platformLayerAt(b.x, b.y);
}

bool onEngravingGround(World& w, int x, int y) {
    const int ground = w.collision.platformAt(x, y);
    return ground != 0 && ground != w.collision.platforms[4][x];
}

template <class T>
T* addHero(World& w, const HeroSpec& spec, int x, int y) {
    auto* hero = new T(w, spec);
    hero->x = x - 10;
    hero->y = y - 25;
    hero->direction = kFacingLeft;
    w.enemies.add(hero);
    return hero;
}

}  // namespace

//-----------------------------------------------------------------------------
// Rayman
//-----------------------------------------------------------------------------

Rayman::Rayman(World& world) : Enemy(world) {
    attackDelay_ = 100 + world.random() % 250;
    health.points = 15000;
    minX = world.marker("video.raymanArena", 0);
    treasure = 0;
}

void Rayman::update() {
    World& w = world();

    if (bleeding > 0) {
        bleeding -= 1;
    }
    if (x < w.marker("video.raymanArena", 1) && w.flags[kArena] == 0) {
        w.flags[kArena] = 1;
    }

    switch (mode) {
        case kNormal:
        case kWalking:
            if (w.flags[kStage] > 0) {
                walkAbout();
            }
            break;
        case kCarbonised:
        case kDying:
            die();
            break;
        case kJumping:
        case kFalling:
            fallDown();
            break;
        case kShooting:
            punch();
            break;
        default:
            break;
    }
    dieWhenOffscreen();
}

void Rayman::walkAbout() {
    World& w = world();
    const int stage = w.flags[kStage];
    const int heroes = w.flags[kHeroes];

    if (heroes <= heroes_ && stage == 2 && heroes > 0) {
        summon();
    } else if (stage <= 2 && heroes == 0) {
        if (stage == 2 && fight()) {
            return;  // the punch has started (and set its image)
        }
        hitboxFromImage();
    } else if (x - w.camera.x > w.camera.width - 195) {
        // Walks into the arena (to 195 pixels from the right edge).
        animation.cycle(3, 21);
        if (direction == kFacingRight) {
            walk(kSpeed);
            image = enemyImage(img::kRaymanWalkRight + animation.frame);
        } else {
            walk(-kSpeed);
            image = enemyImage(img::kRaymanWalkLeft + animation.frame);
        }
        if (w.flags[kArena] == 1) {
            hitbox.active = false;
        }
    }
}

void Rayman::summon() {
    World& w = world();

    switch (w.flags[kHeroes]) {
        case 4:
            addHero<PokemonHero>(w, PokemonHero::kBulbasaur, x, y);
            w.enemiesCreated += 1;
            w.media.levelMusic->play(kHeroMusic);
            w.flags[Flag::Timer] = 70;
            heroes_ -= 1;
            break;
        case 3:
            // (The COM counts too: its death is a kill, D-84 fixed.)
            addHero<PokemonHero>(w, PokemonHero::kCharmander, x, y);
            w.enemiesCreated += 1;
            addHero<PokemonHero>(w, PokemonHero::kCom, x, y);
            w.enemiesCreated += 1;
            heroes_ -= 2;
            break;
        case 1:
            w.enemiesCreated += 1;
            addHero<PokemonHero>(w, PokemonHero::kPikachu, x, y);
            heroes_ -= 1;
            break;
        default:
            break;
    }
}

void Rayman::startPunch() {
    goalX_ = world().target->x;
    dy = 0;
    mode = kShooting;
    reach_ = 0;
    animation.restart();
    attackStep_ = 0;
    punch();
}

void Rayman::throwLimb(int limb) {
    World& w = world();
    int speedX = 0;
    if (x - w.camera.x > w.camera.width / 2) {
        speedX = -1 - w.random() % 3;
    } else {
        speedX = 1 + w.random() % 3;
    }
    const int speedY = -3 - w.random() % 3;
    auto* piece = new RaymanLimb(w, limb, speedX, speedY);
    piece->direction = direction;
    piece->y = y - 22;
    piece->x = x;
    w.blood.add(piece);
}

bool Rayman::fight() {
    World& w = world();
    const int onScreen = x - w.camera.x;

    if (health.points > kBelowFistsHp ||
        (health.points > kCriticalHp && fist1_ && (onScreen < 80 || onScreen > w.camera.width - 40))) {
        // Walks and punches. No ground: falls, and no more this tick (D-54
        // fixed: 2002 also walked on this tick).
        if (platformAt(x, y) == 0) {
            mode = kFalling;
            dy = 0;
            gravityDelay = 0;
            fallDown();
            return false;
        }

        if (x - kSpeed < minX || solidWall(x - kSpeed, y)) {
            direction = kFacingRight;
        } else if (x + kSpeed > w.camera.right() - 40 || solidWall(x + kSpeed, y)) {
            direction = kFacingLeft;
        }
        attackWait_ += 1;
        if (attackWait_ >= attackDelay_ && w.target != nullptr) {
            const Entity& target = *w.target;
            if (sameLayer(w, target, *this) &&
                ((direction == kFacingRight && x < target.x) || (direction == kFacingLeft && x > target.x))) {
                startPunch();
                return true;
            }
        }
        animation.cycle(3, 21);
        if (direction == kFacingRight) {
            walk(kSpeed);
            image = enemyImage(img::kRaymanWalkRight + animation.frame);
        } else {
            walk(-kSpeed);
            image = enemyImage(img::kRaymanWalkLeft + animation.frame);
        }
    } else if (health.points > kCriticalHp && onScreen >= 80 && onScreen <= w.camera.width - 40) {
        // Loses the first fist; stands and punches with the second.
        if (fist1_) {
            throwLimb(img::kRaymanFistRight);
            w.flags[kStage] = 1;
            fist1_ = false;
        }
        attackWait_ += 1;
        if (attackWait_ >= attackDelay_ && w.target != nullptr) {
            const Entity& target = *w.target;
            if (sameLayer(w, target, *this)) {
                direction = x < target.x ? kFacingRight : kFacingLeft;
                startPunch();
                return true;
            }
        }
        image = enemyImage(direction == kFacingRight ? img::kRaymanFistlessRight : img::kRaymanFistlessLeft);
    } else if (health.points > kAboveDeathHp) {
        // Loses the second fist and runs.
        if (fist2_) {
            throwLimb(img::kRaymanFistLeft);
            w.flags[kStage] = 1;
            fist2_ = false;
        }
        if (platformAt(x, y) == 0) {
            mode = kFalling;
            dy = 0;
            gravityDelay = 0;
            fallDown();
        }

        if (x - kSpeed < minX || solidWall(x - kSpeed, y)) {
            direction = kFacingRight;
        } else if (x + kSpeed > w.camera.right() - 40 || solidWall(x + kSpeed, y)) {
            direction = kFacingLeft;
        }

        animation.cycle(3, 8);
        if (direction == kFacingRight) {
            walk(kCriticalSpeed);
            image = enemyImage(img::kRaymanRunRight + animation.frame);
        } else {
            walk(-kCriticalSpeed);
            image = enemyImage(img::kRaymanRunLeft + animation.frame);
        }
    } else if (onScreen >= 80 && onScreen <= w.camera.width - 40) {
        // Loses his feet and stands.
        if (feet_) {
            const int speedY = -3 - w.random() % 3;
            auto* foot = new RaymanLimb(w, img::kRaymanFootRight, 1 + w.random() % 3, speedY);
            foot->direction = direction;
            foot->y = y - 22;
            foot->x = x + 10;
            w.blood.add(foot);

            foot = new RaymanLimb(w, img::kRaymanFootLeft, -1 - w.random() % 3, speedY);
            foot->direction = direction;
            foot->y = y - 22;
            foot->x = x - 10;
            w.blood.add(foot);

            w.flags[kStage] = 1;
            feet_ = false;
        }
        image = enemyImage(direction == kFacingRight ? img::kRaymanFootlessRight : img::kRaymanFootlessLeft);
    }
    return false;
}

void Rayman::die() {
    World& w = world();

    animation.subFrame += 1;
    animation.subFrame %= 6;

    if (animation.frame == 0 && animation.subFrame == 1) {
        // The head flies off.
        int speedX = 0;
        if (x - w.camera.x > w.camera.width / 2) {
            speedX = -1 - w.random() % 3;
        } else {
            speedX = 1 + w.random() % 3;
        }
        const int speedY = -3 - w.random() % 3;
        auto* head = new RaymanHead(w, speedX, speedY);
        head->x = x;
        head->y = y - 60;
        w.blood.add(head);
        w.flags[kArena] = 2;
    }

    if (animation.subFrame == 0) {
        animation.frame += 1;
        if (animation.frame >= 6) {
            dead = true;
        } else if (animation.frame < 2 && direction == kFacingRight) {
            image = enemyImage(img::kRaymanDieRight + animation.frame);
        } else {
            image = enemyImage(img::kRaymanDie + animation.frame);
        }
    }
}

void Rayman::endPunch(int delayBase, int delayRange) {
    World& w = world();
    mode = kWalking;
    animation.restart();
    attackWait_ = 0;
    fist_->dead = true;
    attackDelay_ = delayBase + w.random() % delayRange;
    walkAbout();
}

void Rayman::punch() {
    World& w = world();
    // Above 10000 HP the fist is slower and the animation shorter.
    const bool strong = health.points > kBelowFistsHp;
    const int steps = strong ? 6 : 7;
    const int fistSpeed = strong ? kFist1Speed : kFist2Speed;

    if (attackStep_ == 0) {
        // Winds up: the reach grows by 30 every 3 ticks until it reaches
        // the goal, then the fist flies.
        if (animation.tick(3)) {
            animation.frame %= steps;
            reach_ += 30;
            const bool right = direction == kFacingRight;
            if ((right && goalX_ - x < reach_) || (!right && x - goalX_ < reach_)) {
                auto* fist = new RaymanFist(w, right ? fistSpeed : -fistSpeed, goalX_);
                fist->direction = direction;
                fist->x = right ? x + 13 : x - 13;
                fist->y = y - 13;
                w.enemyShots.add(fist);
                fist_ = fist;

                attackStep_ = 1;
                animation.subFrame = 0;
                animation.frame = steps;
            }
        }
    } else if (attackStep_ == 1) {
        // Waits for the fist to come back. (Nothing destroys the fist
        // meanwhile; the 2002 code did not check, D-04.)
        animation.subFrame += 1;
        if (animation.subFrame == 20) {
            animation.frame = steps + 1;
        }
        const RaymanFist* fist = fist_.get();
        if (fist != nullptr &&
            ((direction == kFacingRight && fist->x < x) || (direction == kFacingLeft && fist->x > x))) {
            if (strong) {
                endPunch(50, 150);
            } else {
                endPunch(10, 50);
            }
            return;
        }
    }

    if (strong) {
        image = enemyImage((direction == kFacingRight ? img::kRaymanPunchRight : img::kRaymanPunchLeft) +
                           animation.frame);
    } else {
        image = enemyImage((direction == kFacingRight ? img::kRaymanPunch2Right : img::kRaymanPunch2Left) +
                           animation.frame);
    }
    hitboxFromImage();
}

void Rayman::fallDown() {
    fall();

    const int ground = dy > 0 ? platformAt(x, y + dy) : 0;
    if (ground != 0) {
        mode = kNormal;
        dy = 0;
        y = ground;
        walkAbout();
        return;
    }
    hitboxFromImage();
}

void Rayman::hitBy(Shot& shot) {
    static const int kBloodX[] = {0, 0, 4, 0, 0, 0, -4, 0};
    static const int kBloodY[] = {-15, -15, -15, -15, -15, -15, -15, -15};

    // Invulnerable while heroes are out.
    if (world().flags[kHeroes] == 0) {
        Enemy::hitBy(shot);
        sprayBlood(shot, kBloodX, kBloodY);
    }
}

//-----------------------------------------------------------------------------
// The fist, the limbs and the head
//-----------------------------------------------------------------------------

void RaymanFist::update() {
    const bool right = direction == kFacingRight;
    image = enemyImage(right ? img::kRaymanFistRight : img::kRaymanFistLeft);

    // Past the goal: slows down by 2 every |speed| ticks, then comes back.
    if (xSpeed_ != 0 && ((right && x > goalX_) || (!right && x < goalX_))) {
        animation.subFrame += 1;
        animation.subFrame %= std::abs(xSpeed_);
    }
    if (animation.subFrame == 0) {
        xSpeed_ += right ? -2 : 2;
    }
    x += xSpeed_;
    hitboxFromImage();
}

RaymanLimb::RaymanLimb(World& world, int limb, int speedX, int speedY)
    : Actor(world), limb_(limb), xSpeed_(speedX) {
    image = enemyImage(limb);
    dy = speedY;
}

void RaymanLimb::update() {
    World& w = world();

    x += xSpeed_;
    fall();

    if (platformAt(x, y) != 0) {
        int limb = 0;
        switch (limb_) {
            case img::kRaymanFistRight:
                limb = 1;
                break;
            case img::kRaymanFistLeft:
                limb = 2;
                break;
            case img::kRaymanFootRight:
            case img::kRaymanFootLeft:
                limb = 3;
                break;
            default:
                break;
        }
        if (limb != 0) {
            w.flags[kStage] = 2;
            w.flags[kLimb] = limb;
            w.media.effects->engrave(x, y, image);
            dead = true;
        }
    }
    dieWhenFallenOut();
}

void RaymanHead::update() {
    x += xSpeed_;
    fall();

    if (animation.tick(4)) {
        animation.frame %= 8;
        image = enemyImage(img::kRaymanHead + animation.frame);
    }

    if (dy > 0 && platformAt(x, y + dy) != 0) {
        world().media.effects->engrave(x, y, image);
        dead = true;
    }
    dieWhenFallenOut();
}

//-----------------------------------------------------------------------------
// The Pokémon heroes
//-----------------------------------------------------------------------------

// hp, treasure, start dy/dx, out, walk speed, walk R/L, die steps, die R/L,
// engraves, landing sound, bleeds.
const HeroSpec PokemonHero::kPikachu = {1500,
                                        100,
                                        -8,
                                        -2,
                                        img::kPikachuHeroOut,
                                        3,
                                        img::kPikachuHeroWalkRight,
                                        img::kPikachuHeroWalkLeft,
                                        10,
                                        img::kPikachuHeroDieRight,
                                        img::kPikachuHeroDieLeft,
                                        true,
                                        true,
                                        true};
const HeroSpec PokemonHero::kBulbasaur = {1500,
                                          5,
                                          -8,
                                          -2,
                                          img::kBulbasaurHeroOut,
                                          2,
                                          img::kBulbasaurHeroWalkRight,
                                          img::kBulbasaurHeroWalkLeft,
                                          8,
                                          img::kBulbasaurHeroDieRight,
                                          img::kBulbasaurHeroDieLeft,
                                          true,
                                          false,
                                          true};
const HeroSpec PokemonHero::kCharmander = {1000,
                                           5,
                                           -7,
                                           -1,
                                           img::kCharmanderHeroOut,
                                           2,
                                           img::kCharmanderHeroWalkRight,
                                           img::kCharmanderHeroWalkLeft,
                                           14,
                                           img::kCharmanderHeroDieRight,
                                           img::kCharmanderHeroDieLeft,
                                           false,
                                           false,
                                           true};
const HeroSpec PokemonHero::kCom = {
    1000, 5,     -8,    -2,   img::kComHeroOut, 2, img::kComHeroFloat, -1, 22, img::kComHeroDie,
    -1,   false, false, false};

PokemonHero::PokemonHero(World& world, const HeroSpec& spec)
    : Enemy(world), spec_(spec), xSpeed_(spec.startDx) {
    health.points = spec.hitPoints;
    dy = spec.startDy;
    treasure = spec.treasure;
}

void PokemonHero::update() {
    World& w = world();

    if (ball_ == 2) {
        // The ball flies and lands.
        x += xSpeed_;
        fall();
        const int ground = dy > 0 ? platformAt(x, y + dy) : 0;
        if (ground != 0) {
            if (spec_.landingSound) {
                w.media.levelSounds->play(snd::kPikachuHero);
            }
            mode = kNormal;
            dy = 0;
            y = ground;
            animation.restart();
            ball_ = 1;
            return;
        }
        animation.cycle(5, 4);
        image = enemyImage(img::kPikachuHeroBall + animation.frame);
    } else if (ball_ == 1) {
        // The ball opens.
        if (animation.tick(8)) {
            if (animation.frame > 3) {
                ball_ = 0;
                animation.restart();
            } else {
                image = enemyImage(spec_.outImage + animation.frame);
            }
        }
    } else {
        if (bleeding > 0) {
            bleeding -= 1;
        }
        switch (mode) {
            case kNormal:
            case kWalking:
                walkAbout();
                break;
            case kDying:
            case kCarbonised:
                die();
                break;
            default:
                break;
        }
        dieWhenOffscreen();
    }
}

void PokemonHero::walkAbout() {
    World& w = world();

    fall();

    animation.cycle(8, 2);

    if (x - 1 < minX || solidWall(x - 1, y)) {
        direction = kFacingRight;
    } else if (x + 1 > w.camera.right() || solidWall(x + 1, y)) {
        direction = kFacingLeft;
    }

    walk(direction == kFacingRight ? spec_.walkSpeed : -spec_.walkSpeed);
    if (spec_.walkLeft < 0 || direction == kFacingRight) {
        image = enemyImage(spec_.walkRight + animation.frame);
    } else {
        image = enemyImage(spec_.walkLeft + animation.frame);
    }
    hitboxFromImage();
}

void PokemonHero::die() {
    World& w = world();

    fall();

    animation.tickUpTo(4, spec_.dieSteps);

    if (animation.frame >= spec_.dieSteps) {
        if (spec_.engraves && onEngravingGround(w, x, y)) {
            w.media.effects->engrave(x, y, image);
        }
        dead = true;
        w.flags[kHeroes] -= 1;
    } else if (spec_.dieLeft < 0 || direction == kFacingRight) {
        image = enemyImage(spec_.dieRight + animation.frame);
    } else {
        image = enemyImage(spec_.dieLeft + animation.frame);
    }
}

void PokemonHero::hitBy(Shot& shot) {
    static const int kBloodX[] = {0, 10, 15, 10, 0, -10, -15, -10};
    static const int kBloodY[] = {-15, -25, -25, -25, -35, -25, -25, -25};

    if (!spec_.bleeds) {
        Enemy::hitBy(shot);
    } else if (ball_ == 0) {
        Enemy::hitBy(shot);
        sprayBlood(shot, kBloodX, kBloodY);
    }
}

}  // namespace bb::game::video
