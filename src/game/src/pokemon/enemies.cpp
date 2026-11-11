// The Pokémon enemies: the walkers (ennemi_pikachu.cpp,
// ennemi_bulbizarre.cpp, ennemi_flameche.cpp) and the COMs (ennemi_com.cpp,
// ennemi_com_volant.cpp).

#include "pokemon/pokemon_world.hpp"

namespace bb::game::pokemon {

namespace {

const int kBloodX[] = {0, 10, 15, 10, 0, -10, -15, -10};
const int kBloodXCharmander[] = {0, 10, 10, 10, 0, -10, -10, -10};
const int kBloodY[] = {-15, -25, -25, -25, -35, -25, -25, -25};

}  // namespace

// turn, walk, walk R/L, die delay/steps/R/L, engraves, burns, burn R/L,
// bleeds again, cries, blood.
const WalkerSpec kPikachu = {3,
                             3,
                             img::kPikachuWalkRight,
                             img::kPikachuWalkLeft,
                             4,
                             10,
                             img::kPikachuDieRight,
                             img::kPikachuDieLeft,
                             true,
                             true,
                             img::kPikachuBurnRight,
                             img::kPikachuBurnLeft,
                             true,
                             true,
                             kBloodX,
                             kBloodY};
const WalkerSpec kBulbasaur = {1,
                               1,
                               img::kBulbasaurWalkRight,
                               img::kBulbasaurWalkLeft,
                               5,
                               8,
                               img::kBulbasaurDieRight,
                               img::kBulbasaurDieLeft,
                               true,
                               true,
                               img::kBulbasaurBurnRight,
                               img::kBulbasaurBurnLeft,
                               true,
                               true,
                               kBloodX,
                               kBloodY};
const WalkerSpec kCharmander = {1,
                                2,
                                img::kCharmanderWalkRight,
                                img::kCharmanderWalkLeft,
                                5,
                                14,
                                img::kCharmanderDieRight,
                                img::kCharmanderDieLeft,
                                false,
                                false,
                                0,
                                0,
                                false,
                                false,
                                kBloodXCharmander,
                                kBloodY};

//-----------------------------------------------------------------------------
// Walkers
//-----------------------------------------------------------------------------

void Walker::update() {
    if (spec_.bleedsAgain && bleeding > 0) {
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
            if (spec_.burns) {
                burn(spec_.burnLeft, spec_.burnRight, 14, 5);
            } else {
                die();
            }
            break;
        default:
            break;
    }
    dieWhenOffscreen();
}

void Walker::walkAbout() {
    fall();

    animation.cycle(8, 2);

    // Charmander looks one pixel ahead but walks two.
    const int ahead = spec_.turnSpeed;
    if (x - ahead < minX || solidWall(x - ahead, y)) {
        direction = kFacingRight;
    } else if (x + ahead > world().camera.right() || solidWall(x + ahead, y)) {
        direction = kFacingLeft;
    }

    if (direction == kFacingRight) {
        walk(spec_.walkSpeed);
        image = enemyImage(spec_.walkRight + animation.frame);
    } else {
        walk(-spec_.walkSpeed);
        image = enemyImage(spec_.walkLeft + animation.frame);
    }
    hitboxFromImage();
}

void Walker::die() {
    fall();

    animation.tickUpTo(spec_.dieDelay, spec_.dieSteps);

    if (animation.frame >= spec_.dieSteps) {
        // Engraved where it lies, unless that is a moving platform (whose
        // heights are on layer 4).
        if (spec_.engraves) {
            const int ground = platformAt(x, y);
            if (ground != 0 && ground != world().collision.platforms[4][x]) {
                world().media.effects->engrave(x, y, image);
            }
        }
        dead = true;
    } else {
        image = enemyImage((direction == kFacingLeft ? spec_.dieLeft : spec_.dieRight) + animation.frame);
    }
}

void Walker::hitBy(Shot& shot) {
    Enemy::hitBy(shot);
    sprayBlood(shot, spec_.bloodX, spec_.bloodY);

    if (spec_.cries && mode == kDying) {
        world().media.miscSounds->play(miscsnd::kDeathCry + world().random() % 2);  // one of 2
    }
}

//-----------------------------------------------------------------------------
// COMs
//-----------------------------------------------------------------------------

void Com::update() {
    switch (mode) {
        case kNormal:
        case kWalking:
            animation.cycle(8, 2);

            if (x - 1 < world().camera.x || solidWall(x - 1, y)) {
                direction = kFacingRight;
            } else if (x + 1 > world().camera.right() || solidWall(x + 1, y)) {
                direction = kFacingLeft;
            }
            x += direction == kFacingRight ? 1 : -1;

            image = enemyImage(img::kComFly + animation.frame);
            hitboxFromImage();
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

void Com::die() {
    if (animation.subFrame == 0 && animation.frame == 0) {
        world().media.levelSounds->play(snd::kComDeath);
    }

    animation.tickUpTo(3, 22);

    if (animation.frame >= 22) {
        dead = true;
    } else {
        image = enemyImage(img::kComDie + animation.frame);
    }
}

FlyingCom::FlyingCom(World& world) : Com(world) {
    aimAtTarget();
}

void FlyingCom::aimAtTarget() {
    World& w = world();
    if (w.target != nullptr) {
        targetY_ = w.target->y;
    } else {
        targetY_ = 100 + w.random() % 200;
    }
}

void FlyingCom::update() {
    switch (mode) {
        case kNormal:
        case kWalking:
            fly();
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

void FlyingCom::fly() {
    const int camera = world().camera.x;
    const int cameraRight = world().camera.right();

    animation.cycle(8, 2);

    // At a wall or a screen edge: turn, climb back and aim again.
    if (direction == kFacingLeft && (x - speed_ < camera || solidWall(x - speed_, y))) {
        direction = kFacingRight;
        attacking_ = false;
        aimAtTarget();
    } else if (direction == kFacingRight && (x + speed_ > cameraRight || solidWall(x + speed_, y))) {
        direction = kFacingLeft;
        attacking_ = false;
        aimAtTarget();
    }

    if (attacking_) {
        // Down to the target height, faster sideways as it gets closer.
        if (targetY_ <= y) {
            speed_ = 6;
        } else if (targetY_ - 80 > y && y > targetY_ - 110) {
            y += 4;
            speed_ = 1;
        } else if (targetY_ - 60 > y && y >= targetY_ - 80) {
            y += 3;
            speed_ = 2;
        } else if (targetY_ - 40 > y && y >= targetY_ - 60) {
            y += 2;
            speed_ = 3;
        } else if (targetY_ - 20 > y && y >= targetY_ - 40) {
            y += 1;
            speed_ = 4;
        } else {
            y += 1;
            speed_ = 5;
        }
    } else {
        // Back up to about 100 pixels above the target height.
        if (y < targetY_ - 110) {
            y += 5;
        } else if (y > targetY_ - 100) {
            y -= 3;
        } else {
            attacking_ = true;
        }
    }

    x += direction == kFacingRight ? speed_ : -speed_;

    image = enemyImage(img::kComFly + animation.frame);
    hitboxFromImage();
}

}  // namespace bb::game::pokemon
