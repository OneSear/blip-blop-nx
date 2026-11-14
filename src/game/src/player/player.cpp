// Blip and Blop (couille.cpp, blip.h, blop.h, joueur.h).

#include "bb/game/player.hpp"

#include "bb/assets/sounds/bb.hpp"
#include "bb/assets/sprites/bb.hpp"
#include "bb/assets/sprites/blip.hpp"
#include "bb/assets/sprites/blop.hpp"

namespace bb::game {

namespace {

namespace img = bb::assets::sprites::blip;  // Blop's bank has the same layout
namespace common = bb::assets::sprites::bb;
namespace snd = bb::assets::sounds::bb;

static_assert(img::kSomersault == bb::assets::sprites::blop::kSomersault &&
                  img::kShotgunHand == bb::assets::sprites::blop::kShotgunHand &&
                  img::kRifle == bb::assets::sprites::blop::kRifle &&
                  img::kWalkLeft == bb::assets::sprites::blop::kWalkLeft &&
                  img::kCorpseLeft == bb::assets::sprites::blop::kCorpseLeft,
              "Blip and Blop share one image layout");

constexpr int kRifleBullets = 16;
constexpr int kSmgBullets = 20;
constexpr int kPellets = 40;
constexpr int kBeams = 2;
constexpr int kFlames = 300;

constexpr int kFullHealth = 5;
constexpr int kCowBombDelay = 100;

constexpr WeaponSpec kWeapons[] = {
    {3, 5, 10, 1},   // Rifle (M16)
    {3, 15, 45, 1},  // Shotgun
    {3, 5, 5, 5},    // Smg
    {3, 5, 1, 5},    // Flamethrower
    {3, 4, 1, 1},    // Laser
};

// Animations.
const int kWalkSteps[] = {0, 1, 2, 1};
// By vertical speed + 5 (rising fast .. falling fast).
const int kJumpRightImages[] = {img::kJumpRight + 2, img::kJumpRight + 2, img::kJumpRight + 2,
                                img::kJumpRight + 1, img::kJumpRight + 1, img::kJumpRight,
                                img::kJumpRight,     img::kWalkRight,     img::kWalkRight,
                                img::kWalkRight,     img::kWalkRight};
const int kJumpLeftImages[] = {img::kJumpLeft + 2, img::kJumpLeft + 2, img::kJumpLeft + 2, img::kJumpLeft + 1,
                               img::kJumpLeft + 1, img::kJumpLeft,     img::kJumpLeft,     img::kWalkLeft,
                               img::kWalkLeft,     img::kWalkLeft,     img::kWalkLeft};
const int kHop[] = {0, 2, 4, 2};  // anime_sautille
const int kFlashFrames[] = {0, 1, 2, 1, 0};

// Gun positions by the 10 gun aims (dir_arme / 2, plus one facing left).
const int kRifleDx[] = {3, 6, 0, 2, 7, -7, -2, 0, -6, -3};
const int kRifleDy[] = {-15, -9, -12, -10, -10, -10, -10, -12, -9, -15};
const int kSmgDx[] = {2, 6, 0, 0, 7, -7, 0, 0, -6, -2};
const int kSmgDy[] = {-12, -6, -10, -15, -9, -9, -15, -10, -6, -12};
const int kFlamethrowerDx[] = {-10, -10, -10, -10, -10, 10, 10, 10, 10, 10};
const int kFlamethrowerDy[] = {-10, -10, -10, -10, -10, -10, -10, -10, -10, -10};
const int kShotgunDx[] = {1, -4, -6, -4, -4, 4, 4, 6, 4, -1};
const int kShotgunDy[] = {-14, -12, -11, -10, -9, -9, -10, -11, -12, -14};
const int kShotgunHandDx[] = {0, 17, 24, 18, -3, 3, -18, -24, -17, 0};
const int kShotgunHandDy[] = {16, 10, -10, -31, -40, -38, -31, -10, 10, 16};
const int kLaserDx[] = {1, -4, -6, -4, -4, 4, 4, 6, 4, -1};
const int kLaserDy[] = {-12, -12, -11, -10, -9, -9, -10, -11, -12, -11};
const int kLaserGlowDx[] = {-2, -1, 0, 3, -1, 1, -3, 0, 0, 2};
const int kLaserGlowDy[] = {-6, -5, -8, -13, -15, -15, -13, -8, -5, -5};

// Muzzle flashes (Blop's rifle flash sits closer).
const int kRifleFlashDx[] = {-2, 20, 30, 22, -3, 3, -21, -30, -20, 2};
const int kRifleFlashDy[] = {24, 20, -9, -31, -49, -49, -31, -9, 20, 24};
const int kBlopRifleFlashDx[] = {1, 17, 22, 15, -4, 4, -15, -23, -20, -1};
const int kBlopRifleFlashDy[] = {16, 9, -10, -25, -33, -33, -25, -10, 8, 16};
const int kSmgFlashDx[] = {-6, 12, 12, 11, -6, 6, -11, -12, -13, -1};
const int kSmgFlashDy[] = {6, 5, -11, -22, -28, -30, -22, -11, 5, 6};
const int kShotgunFlashDx[] = {2, 29, 41, 29, -5, 5, -29, -41, -30, -2};
const int kShotgunFlashDy[] = {33, 21, -12, -44, -57, -57, -44, -12, 21, 33};
const int kLaserFlashDx[] = {-3, 24, 37, 29, -1, 0, -29, -37, -24, 2};
const int kLaserFlashDy[] = {32, 21, -8, -39, -53, -53, -39, -8, 21, 32};

// Where the shots leave.
const int kRifleShotDx[] = {-2, 25, 35, 27, -3, 3, -26, -30, -25, 2};
const int kRifleShotDy[] = {29, 25, -9, -36, -54, -54, -36, -9, 25, 29};
const int kSmgShotDx[] = {-2, 25, 35, 27, -3, 3, -26, -30, -25, 2};
const int kSmgShotDy[] = {29, 25, -9, -36, -54, -54, -36, -9, 25, 29};
const int kShotgunShotDx[] = {2, 29, 41, 29, -5, 5, -29, -41, -30, -2};
const int kShotgunShotDy[] = {33, 21, -12, -44, -57, -57, -44, -12, 21, 33};
const int kLaserShotDx[] = {-5, 42, 57, 56, -3, -2, -56, -57, -42, 0};
const int kLaserShotDy[] = {52, 33, -10, -71, -73, -73, -71, -10, 33, 52};
const int kFlameShotDx[] = {-2, 32, 44, 31, 0, 1, -31, -44, -32, 2};
const int kFlameShotDy[] = {46, 33, -7, -46, -63, -63, -46, -7, 33, 46};

// Recoil by gun aim and recoil step.
const int kRecoilX[][5] = {
    {0, 0, 0, 0, 0}, {0, -1, -2, -1, 0}, {0, -2, -3, -2, 0}, {0, -1, -2, -1, 0}, {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0}, {0, 1, 2, 1, 0},    {0, 2, 3, 2, 0},    {0, 1, 2, 1, 0},    {0, 0, 0, 0, 0},
};
const int kRecoilY[][5] = {
    {0, -2, -3, -2, 0}, {0, -1, -2, -1, 0}, {0, 0, 0, 0, 0}, {0, 1, 2, 1, 0},    {0, 2, 3, 2, 0},
    {0, 2, 3, 2, 0},    {0, 1, 2, 1, 0},    {0, 0, 0, 0, 0}, {0, -1, -2, -1, 0}, {0, -2, -3, -2, 0},
};
const int kShotgunRecoilX[][5] = {
    {0, 0, 0, 0, 0}, {0, -2, -4, -2, 0}, {0, -4, -6, -4, 0}, {0, -2, -4, -2, 0}, {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0}, {0, 2, 4, 2, 0},    {0, 4, 6, 4, 0},    {0, 2, 4, 2, 0},    {0, 0, 0, 0, 0},
};
const int kShotgunRecoilY[][5] = {
    {0, -4, -6, -4, 0}, {0, -2, -4, -2, 0}, {0, 0, 0, 0, 0}, {0, 2, 4, 2, 0},    {0, 4, 6, 4, 0},
    {0, 4, 6, 4, 0},    {0, 2, 4, 2, 0},    {0, 0, 0, 0, 0}, {0, -2, -4, -2, 0}, {0, -4, -6, -4, 0},
};

bool facesRight(int direction) {
    return direction <= aim::kUpRight;
}

// The 10 gun aims from the 18 directions.
int gunAim(int weaponAim) {
    return weaponAim / 2 + (weaponAim > aim::kUpRight ? 1 : 0);
}

}  // namespace

const WeaponSpec& weaponSpec(Weapon weapon) {
    return kWeapons[static_cast<int>(weapon)];
}

Player::Player(World& world, Hero which, const PlayerControls& controls, int index)
    : Character(world),
      hero(which),
      slot(index),
      controls_(controls),
      own_(which == Hero::Blip     ? world.media.blip
           : which == Hero::Blop   ? world.media.blop
           : which == Hero::Bleach ? world.media.bleach
                                   : world.media.blitz) {
    direction = aim::kRight;
    hitbox.active = true;
    health.points = kFullHealth;

    rifleBullets_.reserve(kRifleBullets);
    smgBullets_.reserve(kSmgBullets);
    pellets_.reserve(kPellets);
    beams_.reserve(kBeams);
    flames_.reserve(kFlames);
    for (int i = 0; i < kRifleBullets; ++i) {
        rifleBullets_.emplace_back(world);
    }
    for (int i = 0; i < kSmgBullets; ++i) {
        smgBullets_.emplace_back(world);
    }
    for (int i = 0; i < kPellets; ++i) {
        pellets_.emplace_back(world);
    }
    for (int i = 0; i < kBeams; ++i) {
        beams_.emplace_back(world);
    }
    for (int i = 0; i < kFlames; ++i) {
        flames_.emplace_back(world);
    }
}

//-----------------------------------------------------------------------------
// Weapons
//-----------------------------------------------------------------------------

void Player::setWeapon(Weapon w) {
    weapon = w;
}

void Player::equip(Weapon w, int rounds) {
    weapon = w;
    ammo = rounds;
    flashStep_ = 0;
    flashSubStep_ = 0;
    recoilStep_ = 0;
    firing = false;
}

void Player::rearm() {
    if (blipKind(hero)) {
        equip(Weapon::Smg, 100);
    } else {
        equip(Weapon::Shotgun, 10);
    }
}

void Player::equipSuperWeapon() {
    if (blipKind(hero) || !world().flamethrowerAllowed) {
        equip(Weapon::Laser, 750);
    } else {
        equip(Weapon::Flamethrower, 1000);
    }
}

bool Player::wantsWeaponPickup() const {
    switch (weapon) {
        case Weapon::Rifle:
            return true;
        case Weapon::Smg:
            return ammo <= 30;
        case Weapon::Shotgun:
            return ammo <= 4;
        case Weapon::Flamethrower:
        case Weapon::Laser:
            return ammo <= 100;
    }
    return false;
}

void Player::endLevel() {
    SoundPlayer& sounds = *world().media.commonSounds;
    sounds.stop(snd::kFlamethrowerLoop);
    sounds.stop(snd::kLaserLoop);
}

void Player::updateWeaponSounds() {
    SoundPlayer& sounds = *world().media.commonSounds;

    if (weapon == Weapon::Flamethrower) {
        if (firing && !flameSound_) {
            sounds.play(snd::kFlamethrowerLoop, SoundPlayer::kLoop);
            flameSound_ = true;
        } else if (!firing && flameSound_) {
            sounds.stop(snd::kFlamethrowerLoop);
            flameSound_ = false;
        }
    } else if (flameSound_) {
        sounds.stop(snd::kFlamethrowerLoop);
        flameSound_ = false;
    }

    if (weapon == Weapon::Laser) {
        if (firing && !laserSound_) {
            sounds.play(snd::kLaserLoop, SoundPlayer::kLoop);
            laserSound_ = true;
        } else if (!firing && laserSound_) {
            sounds.stop(snd::kLaserLoop);
            laserSound_ = false;
        }
    } else if (laserSound_) {
        sounds.stop(snd::kLaserLoop);
        laserSound_ = false;
    }
}

//-----------------------------------------------------------------------------
// Update
//-----------------------------------------------------------------------------

void Player::update() {
    World& w = world();

    if (w.blink) {
        blinkStep_ += 1;
        blinkStep_ %= 4;
    }

    updateWeaponSounds();

    if (lives <= 0) {
        return;
    }

    // Fallen out of the level: every life lost at once.
    if (y > 540) {
        lives = 0;
        perfect = false;
        dead = true;
        rumble(Rumble::Death);
    }

    if (mode == kDying) {
        dying();
        return;
    }
    if (mode == kComingBack) {
        comingBack();
        return;
    }

    if (cowBombWait_ < kCowBombDelay) {
        cowBombWait_ += 1;
    }
    if (controls_.special() && cowBombWait_ == kCowBombDelay && cowBombs >= 1) {
        dropCowBomb();
    }

    // Pushed by the scroll.
    if (x < w.camera.x + 20 && !controls_.right() && !solidWall(x + 2, y)) {
        x += 2;
        if (slideMomentum_ < 0) {
            slideDx_ = slideMomentum_ = 0;
        }
    }

    const int ground = platformAt(x, y);
    if (ground != 0) {
        y = ground;
    }

    // Left behind by the scroll: a life lost. The tick goes on (D-93).
    if (x < w.camera.x - 30) {
        crushedByScroll();
        if (dead) {
            return;
        }
    }

    if (invincible > 0) {
        invincible -= 1;
    } else {
        cowInvincible_ = false;
    }

    if (hurtEye_ > 0) {
        hurtEye_ -= 1;
    }

    if (animation.subFrame == 0) {
        bounce_ += 1;
        bounce_ %= 4;
    }

    // Down pressed twice quickly drops through the platform.
    if (controls_.down()) {
        if (downTime_ > 0 && downTime_ < 10 && mode == kNormal) {
            downTime_ = -1;
        } else {
            downTime_ = 0;
        }
    } else {
        downTime_ += 1;
    }

    if (mode != kRiding || !aimLocked_) {
        turn();
    }

    switch (mode) {
        case kNormal:
            standing();
            break;
        case kWalking:
            walking();
            break;
        case kSomersault:
            somersault();
            break;
        case kJumping:
        case kFalling:
            jumping();
            break;
        default:  // riding: the vehicle moves the player
            break;
    }

    slide();
    hitboxFromBody();

    if (mode != kSomersault) {
        updateWeapon();
    }
}

void Player::dropCowBomb() {
    World& w = world();

    // A cow every 64 pixels across the view (11 on the 2002 screen).
    const int cows = w.camera.width / 64 + 1;
    for (int i = 0; i < cows; ++i) {
        const int cowX = w.camera.x + i * 64 + w.random() % 20;
        if (!solidWall(cowX, 0)) {
            auto* cow = new FallingCow(w);
            cow->x = cowX;
            cow->y = w.random() % 180 - 200;
            cow->dy = 5 + w.random() % 3;
            cow->animation.frame = w.random() % 8;
            cow->owner = this;
            w.cowBombs.add(cow);
        }
    }

    cowBombWait_ = 0;
    cowBombs -= 1;
    invincible = 200;
    cowInvincible_ = true;
    w.media.commonSounds->play(snd::kCowBombDrop);
    rumble(Rumble::CowBomb);
}

void Player::dropIn(int atX, int atLandingY) {
    x = atX;
    landingY = atLandingY;
    respawn();
    rearm();
    rumble(Rumble::Join);
}

void Player::respawn() {
    mode = kComingBack;
    y = -50;
    hitbox.active = true;
    health.points = kFullHealth;
    weaponAim = direction = aim::kRight;
    invincible = 400;
    image = own(img::kJumpRight + 2);
    dy = -5;  // so that the eye and the gun are drawn in place
    jumpDx_ = 0;
    firing = false;
}

void Player::crushedByScroll() {
    World& w = world();

    // Each player stops the scroll through its own flag (D-90 fixed: both
    // used the first player's); a player crushed on its last life leaves
    // the scroll free (D-93 fixed: it stayed stopped).
    rumble(Rumble::Death);
    lives -= 1;
    if (lives == 0) {
        dead = true;
        w.camera.stopped[slot] = false;
        return;
    }
    w.camera.stopped[slot] = true;

    // Lands in the middle of the screen on the first layer with a platform
    // there, or else at the nearest column of the screen that has one (D-91
    // fixed: with none in the middle, the 2002 loop read past the last
    // layer and the player fell out of the level, losing every life).
    const int centre = w.camera.centre();
    x = centre;
    landingY = 0xFFFF;
    for (int offset = 0; offset < w.camera.width / 2 && landingY == 0xFFFF; ++offset) {
        for (const int column : {centre + offset, centre - offset}) {
            if (column < 0 || column >= w.collision.width || landingY != 0xFFFF) {
                continue;
            }
            for (int layer = 0; layer < CollisionMap::kLayers; ++layer) {
                if (w.collision.platforms[layer][column] != 0xFFFF) {
                    x = column;
                    landingY = w.collision.platforms[layer][column];
                    break;
                }
            }
        }
    }

    respawn();
    setWeapon(Weapon::Rifle);
    ammo = 0;
}

void Player::slide() {
    World& w = world();
    const int cameraX = w.camera.x;
    const int cameraRight = w.camera.right();

    if (mode != kNormal && mode != kWalking) {
        slideDx_ = slideMomentum_ = 0;
        return;
    }

    // Layers 6 and 7 are slippery: the slope and the controls build up a
    // momentum.
    const int layer = w.collision.platformLayerAt(x, y);
    if (layer < 6 || x <= 10 || x <= cameraX + 30 || x >= cameraRight - 30) {
        slideDx_ = slideMomentum_ = 0;
        return;
    }

    const int ahead = w.collision.platforms[layer][x + 10] - y;
    const int behind = w.collision.platforms[layer][x - 10] - y;

    if (ahead < 10 && ahead > -10) {
        slideMomentum_ += ahead;
    }
    if (behind < 10 && behind > -10) {
        slideMomentum_ -= behind;
    }

    if (slideMomentum_ > 0) {
        if (controls_.right()) {
            slideMomentum_ += 25;
        } else if (controls_.left()) {
            slideMomentum_ -= 10;
        } else {
            slideMomentum_ -= 1;
        }
    } else if (slideMomentum_ < 0) {
        if (controls_.left()) {
            slideMomentum_ -= 25;
        } else if (controls_.right()) {
            slideMomentum_ += 10;
        } else {
            slideMomentum_ += 1;
        }
    } else {
        if (controls_.right()) {
            slideMomentum_ += 25;
        } else if (controls_.left()) {
            slideMomentum_ -= 25;
        }
    }

    if (slideMomentum_ > 600) {
        slideMomentum_ = 600;
    } else if (slideMomentum_ < -600) {
        slideMomentum_ = -600;
    }

    if (slideMomentum_ > 0) {
        slideDx_ = slideMomentum_ / 200 + 1;
    } else if (slideMomentum_ < 0) {
        slideDx_ = slideMomentum_ / 200 - 1;
    } else {
        slideDx_ = 0;
    }
}

void Player::startJump() {
    mode = kJumping;
    dy = -5;
    gravityDelay = 0;  // otherwise the jumps differ in height
    jumpDx_ = slideDx_;

    if (controls_.left() && jumpDx_ > -2) {
        jumpDx_ = -2;
    } else if (controls_.right() && jumpDx_ < 2) {
        jumpDx_ = 2;
    }
    jumpReleased_ = false;
}

void Player::startFall() {
    mode = kFalling;
    dy = 0;
    gravityDelay = 0;
    jumpDx_ = slideDx_;

    if (controls_.left() && jumpDx_ > -2) {
        jumpDx_ = -2;
    } else if (controls_.right() && jumpDx_ < 2) {
        jumpDx_ = 2;
    }
}

void Player::land(int ground) {
    mode = kNormal;
    slideMomentum_ = 200 * jumpDx_;
    dy = 0;
    y = ground;
}

void Player::standing() {
    // Down twice quickly: drops through the platform if there is room.
    if (downTime_ == -1 && !solidWall(x, y + 5) && !solidWall(x, y + 10) && !solidWall(x, y + 15) &&
        !solidWall(x, y + 20) && !solidWall(x, y + 25)) {
        y += 10;
        dy = 5;
        mode = kFalling;
        gravityDelay = 0;
        jumpDx_ = slideDx_;
        downTime_ = 0;
        jumping();
        return;
    }

    if (controls_.jump() && !solidWall(x, y - 5)) {
        startJump();
        jumping();
        return;
    }

    if (controls_.left() || controls_.right() || slideDx_ != 0) {
        mode = kWalking;
        walking();
        return;
    }

    if (platformAt(x, y) == 0) {
        startFall();  // and falls from the next tick
        return;
    }

    const int first = facesRight(direction) ? img::kWalkRight : img::kWalkLeft;
    image = own(first + animation.loop(kWalkSteps, 4, 8));
}

void Player::walking() {
    const int cameraX = world().camera.x;
    const int cameraRight = world().camera.right();

    if (controls_.jump() && !solidWall(x, y - 5)) {
        startJump();
        jumping();
        return;
    }

    const int slideX = x + slideDx_;

    if ((slideDx_ > 0 && !solidWall(slideX, y) && slideX < cameraRight - 20) ||
        (slideDx_ < 0 && !solidWall(slideX, y) && slideX > cameraX + 20)) {
        walk(slideDx_);
    } else if (controls_.right() && !solidWall(x + 2, y) && x + 2 < cameraRight - 20) {
        walk(2);
    } else if (controls_.left() && !solidWall(x - 2, y) && x - 2 > cameraX + 20) {
        walk(-2);
    } else {
        mode = kNormal;
    }

    if (platformAt(x, y) == 0) {
        startFall();
        jumping();
        return;
    }

    const int first = facesRight(direction) ? img::kWalkRight : img::kWalkLeft;
    image = own(first + animation.loop(kWalkSteps, 4, 5));
}

void Player::jumping() {
    const int cameraX = world().camera.x;
    const int cameraRight = world().camera.right();

    // Jump pressed again while rising: a somersault.
    if (!controls_.jump()) {
        jumpReleased_ = true;
    }
    if (controls_.jump() && jumpReleased_ && mode == kJumping && dy < 3) {
        dy = -5;
        mode = kSomersault;
        animation.restart();
        firing = false;
        flashStep_ = flashSubStep_ = 0;
        somersault();
        return;
    }

    fallThrough();

    // Jump released: a lower jump.
    if (dy < 0 && !controls_.jump()) {
        gravityDelay += 1;
        gravityDelay %= kGravityDelay;
        if (gravityDelay == 0) {
            dy += 1;
        }
    }

    const int ground = dy > 0 ? platformAt(x, y + dy) : 0;
    if (ground != 0) {
        land(ground);
        standing();
        return;
    }

    // Steering in the air (the momentum is kept while aiming up or down).
    if (!controls_.down() && !controls_.up()) {
        if (controls_.right() && jumpDx_ < 2) {
            jumpDx_ = 2;
        } else if (controls_.left() && jumpDx_ > -2) {
            jumpDx_ = -2;
        }
    }

    const int nextX = x + jumpDx_;
    if (!solidWall(nextX, y) && nextX > cameraX + 20 && nextX < cameraRight - 20) {
        x = nextX;
    }

    image = own(facesRight(direction) ? kJumpRightImages[dy + 5] : kJumpLeftImages[dy + 5]);
}

void Player::somersault() {
    const int cameraX = world().camera.x;
    const int cameraRight = world().camera.right();

    animation.cycle(3, 8);

    fallThrough();

    const int ground = dy > 0 ? platformAt(x, y + dy) : 0;
    if (ground != 0) {
        land(ground);
        standing();
        return;
    }

    if (controls_.right() && jumpDx_ < 2) {
        jumpDx_ = 2;
    } else if (controls_.left() && jumpDx_ > -2) {
        jumpDx_ = -2;
    }

    const int nextX = x + jumpDx_;
    if (!solidWall(nextX, y) && nextX > cameraX + 20 && nextX < cameraRight - 20) {
        x = nextX;
    }

    image = own(img::kSomersault + static_cast<int>(weapon) * 8 + animation.frame);
}

void Player::turn() {
    // The direction asked for.
    if (controls_.up()) {
        if (controls_.right()) {
            direction = aim::kDiagonalUpRight;
        } else if (controls_.left()) {
            direction = aim::kDiagonalUpLeft;
        } else {
            direction = facesRight(direction) ? aim::kUpRight : aim::kUpLeft;
        }
    } else if (controls_.down()) {
        if (controls_.right()) {
            direction = aim::kDiagonalDownRight;
        } else if (controls_.left()) {
            direction = aim::kDiagonalDownLeft;
        } else {
            direction = facesRight(direction) ? aim::kDownRight : aim::kDownLeft;
        }
    } else if (controls_.right()) {
        direction = aim::kRight;
    } else if (controls_.left()) {
        direction = aim::kLeft;
    } else {
        direction = facesRight(direction) ? aim::kRight : aim::kLeft;
    }

    // The gun flips with the body, then turns towards the direction.
    if ((facesRight(direction) && weaponAim > aim::kUpRight) ||
        (!facesRight(direction) && weaponAim <= aim::kUpRight)) {
        weaponAim = aim::kDownLeft - weaponAim;
    }

    turnStep_ += 1;
    turnStep_ %= weaponSpec(weapon).turnDelay;

    if (turnStep_ == 0) {
        if (weaponAim < direction) {
            weaponAim += 1;
        } else if (weaponAim > direction) {
            weaponAim -= 1;
        }
    }
}

void Player::updateWeapon() {
    World& w = world();

    if (mode == kRiding) {
        return;
    }

    // The gun is where it is drawn.
    int gunY = y;
    switch (mode) {
        case kNormal:
        case kWalking:
            gunY -= kHop[bounce_];
            break;
        case kJumping:
        case kFalling:
            if (dy < 0) {
                gunY += dy - 2;
            }
            break;
        default:
            break;
    }

    const WeaponSpec& spec = weaponSpec(weapon);

    if (firing) {
        flashSubStep_ += 1;
        flashSubStep_ %= spec.flashDelay;
        if (flashSubStep_ == 0) {
            flashStep_ += 1;
        }
        if (flashStep_ == spec.flashSteps) {
            firing = false;
        }
    }

    if (!firing && controls_.fire() && !w.holdFire.on) {
        firing = true;
        flashStep_ = 0;
        flashSubStep_ = 0;
        if (weapon == Weapon::Smg) {
            w.media.commonSounds->play(snd::kSmgShot);
        }
        fireStep_ = spec.fireDelay - 1;
    }

    recoilStep_ += 1;
    recoilStep_ %= 5;

    fireStep_ += 1;
    fireStep_ %= spec.fireDelay;

    // A heavy gun (turn delay 1) fires only once aimed.
    if (firing && fireStep_ == 0 && !(spec.turnDelay <= 1 && weaponAim != direction)) {
        fireShots(gunY);
    }
}

void Player::fireShots(int gunY) {
    World& w = world();
    const int gun = gunAim(weaponAim);

    // The vehicle speed the 2002 shots added (dxReferentiel) was only set
    // for a rider, who does not get here: it is always 0 and is gone.
    switch (weapon) {
        case Weapon::Rifle: {
            w.media.commonSounds->play(snd::kRifleShot);
            nextRifle_ = (nextRifle_ + 1) % kRifleBullets;
            RifleBullet& shot = rifleBullets_[static_cast<std::size_t>(nextRifle_)];
            shot.dead = false;
            shot.owner = this;
            shot.x = x + kRifleShotDx[gun] + w.random() % 11 - 5;
            shot.y = gunY + kRifleShotDy[gun] + w.random() % 11 - 5;
            shot.aim(direction);
            w.playerShots.add(&shot);
            break;
        }
        case Weapon::Smg: {
            nextSmg_ = (nextSmg_ + 1) % kSmgBullets;
            SmgBullet& shot = smgBullets_[static_cast<std::size_t>(nextSmg_)];
            shot.dead = false;
            shot.owner = this;
            shot.x = x + kSmgShotDx[gun] + w.random() % 11 - 5;
            shot.y = gunY + kSmgShotDy[gun] + w.random() % 11 - 5;
            shot.aim(weaponAim);
            w.playerShots.add(&shot);
            break;
        }
        case Weapon::Shotgun:
            w.media.commonSounds->play(snd::kShotgunShot);
            for (int i = 0; i < 17; ++i) {
                nextPellet_ = (nextPellet_ + 1) % kPellets;
                ShotgunPellet& shot = pellets_[static_cast<std::size_t>(nextPellet_)];
                shot.dead = false;
                shot.owner = this;
                shot.x = x + kShotgunShotDx[gun] + w.random() % 11 - 5;
                shot.y = gunY + kShotgunShotDy[gun] + w.random() % 11 - 5;
                shot.aim(direction);
                w.playerShots.add(&shot);
            }
            break;
        case Weapon::Laser: {
            nextBeam_ = (nextBeam_ + 1) % kBeams;
            LaserBeam& shot = beams_[static_cast<std::size_t>(nextBeam_)];
            shot.dead = false;
            shot.owner = this;
            shot.x = x + kLaserShotDx[gun] + kRecoilX[gun][recoilStep_];
            shot.y = gunY + kLaserShotDy[gun] + kRecoilY[gun][recoilStep_];
            shot.aim(direction);
            shot.animation.frame = flashStep_;
            w.playerShots.add(&shot);
            break;
        }
        case Weapon::Flamethrower:
            // A strong flame and a weak one.
            for (int power = 2; power >= 1; --power) {
                nextFlame_ = (nextFlame_ + 1) % kFlames;
                Flame& shot = flames_[static_cast<std::size_t>(nextFlame_)];
                shot.dead = false;
                shot.power = power;
                shot.owner = this;
                shot.x = x + kFlameShotDx[gun] + kRecoilX[gun][recoilStep_];
                shot.y = gunY + kFlameShotDy[gun] + kRecoilY[gun][recoilStep_];
                shot.aim(weaponAim);
                w.playerShots.add(&shot);
            }
            break;
    }

    if (weapon != Weapon::Rifle && !w.cheats.infiniteAmmo) {
        ammo -= 1;
        if (ammo <= 0) {
            setWeapon(Weapon::Rifle);
            firing = false;
        }
    }
}

void Player::hitboxFromBody() {
    hitboxFromImage();

    const int margin = mode == kSomersault ? 15 : 10;
    hitbox.x1 += margin;
    hitbox.x2 -= margin;
    hitbox.y1 += margin;
    hitbox.y2 -= margin;
}

//-----------------------------------------------------------------------------
// Hurt, death and return
//-----------------------------------------------------------------------------

void Player::hurt(int damage) {
    // The invincible cheat: enemies and shots never hurt (the scroll still
    // crushes, crushedByScroll()).
    if (invincible > 0 || mode == kDying || world().cheats.invincible) {
        return;
    }

    health.take(damage);
    perfect = false;

    if (!health.left()) {
        mode = kDying;
        firing = false;
        flashStep_ = flashSubStep_ = 0;
        animation.restart();
        hitbox.active = false;
        world().camera.stopped[slot] = true;
        rumble(Rumble::Death);
    } else {
        invincible = 200;
        hurtEye_ = 100;
        rumble(Rumble::Hurt);
    }
}

void Player::dying() {
    World& w = world();

    if (dy < 0) {
        dy = 0;
    }
    fall();

    animation.tick(7);

    if (animation.frame == 15) {
        lives -= 1;
        if (lives == 0) {
            dead = true;
            w.camera.stopped[slot] = false;
            return;
        }

        landingY = y;
        respawn();
        rearm();
        if (w.cowBombUnlocked) {
            cowBombs = 1;
        }
    } else if (animation.frame == 5 && animation.subFrame == 0 && platformAt(x, y) != 0) {
        w.media.effects->engrave(x, y, own(facesRight(direction) ? img::kCorpseRight : img::kCorpseLeft));
    } else if (animation.frame < 5) {
        image = own((facesRight(direction) ? img::kDieRight : img::kDieLeft) + animation.frame);
    }
}

void Player::comingBack() {
    World& w = world();

    w.camera.stopped[slot] = true;

    if (y < landingY) {
        animation.cycle(3, 8);
        y += 5;
        image = own(img::kSomersault + static_cast<int>(weapon) * 8 + animation.frame);
    } else {
        y = landingY;
        mode = kNormal;
        animation.restart();
        w.camera.stopped[slot] = false;
    }
}

//-----------------------------------------------------------------------------
// Vehicles
//-----------------------------------------------------------------------------

void Player::board(bool canFire, bool canTurn) {
    mode = kRiding;
    aimLocked_ = !canTurn;

    // A rider never fires (updateWeapon() stops at once); the 2002 code
    // also kept a "fire locked" flag that nothing read.
    if (!canFire) {
        firing = false;
        flashStep_ = flashSubStep_ = 0;
    }
}

void Player::leave() {
    animation.restart();
    mode = kSomersault;
    dy = -5;
    jumpDx_ = 0;
    firing = false;
    flashStep_ = flashSubStep_ = 0;
}

//-----------------------------------------------------------------------------
// Drawing
//-----------------------------------------------------------------------------

void Player::draw() {
    if (lives <= 0) {
        return;
    }

    // Blinks while invincible (except after a cow bomb).
    if (invincible > 50 && blinkStep_ == 0 && world().blink && !cowInvincible_) {
        return;
    }

    switch (mode) {
        case kNormal:
        case kWalking:
            drawStanding();
            break;
        case kSomersault:
        case kComingBack:
            Actor::draw();
            break;
        case kJumping:
        case kFalling:
            drawJumping();
            break;
        case kDying:
            if (animation.frame < 5) {
                Actor::draw();
            }
            break;
        case kRiding:
            drawRiding();
            break;
        default:
            break;
    }
}

void Player::drawStanding() {
    const int bodyY = y - kHop[bounce_];
    drawAt(x, bodyY, image);
    drawEye(x, bodyY);
    drawWeapon(x, bodyY);
}

void Player::drawJumping() {
    Actor::draw();

    if (dy < 0) {
        drawEye(x, y + dy - 2);
        drawWeapon(x, y + dy - 2);
    } else {
        drawEye(x, y);
        drawWeapon(x, y);
    }
}

void Player::drawRiding() {
    // Sets the image the next update's hitbox uses, as in 2002.
    image = own(facesRight(direction) ? img::kWalkRight : img::kWalkLeft);
    Actor::draw();
    drawEye(x, y);
    drawWeapon(x, y);
}

void Player::drawWeapon(int px, int py) {
    const int gun = gunAim(weaponAim);
    const int hand = gun % 5;

    switch (weapon) {
        case Weapon::Rifle:
            if (firing) {
                drawAt(px + kRifleDx[gun] + kRecoilX[gun][recoilStep_],
                       py + kRifleDy[gun] + kRecoilY[gun][recoilStep_], own(img::kRifle + gun));
            } else {
                drawAt(px + kRifleDx[gun], py + kRifleDy[gun], own(img::kRifle + gun));
            }
            break;

        case Weapon::Smg:
            if (firing) {
                drawAt(px + kSmgDx[gun] + kRecoilX[gun][recoilStep_],
                       py + kSmgDy[gun] + kRecoilY[gun][recoilStep_], own(img::kSmg + gun));
            } else {
                drawAt(px + kSmgDx[gun], py + kSmgDy[gun], own(img::kSmg + gun));
            }
            break;

        case Weapon::Flamethrower:
            drawAt(px + kFlamethrowerDx[gun], py + kFlamethrowerDy[gun], own(img::kFlamethrower + gun));
            break;

        case Weapon::Shotgun:
            if (firing) {
                if (flashStep_ < 5) {  // the recoil
                    drawAt(px + kShotgunDx[gun] + kRecoilX[gun][flashStep_] * 2,
                           py + kShotgunDy[gun] + kRecoilY[gun][flashStep_] * 2, own(img::kShotgun + gun));
                    drawAt(px + kShotgunHandDx[gun] + kShotgunRecoilX[gun][flashStep_],
                           py + kShotgunHandDy[gun] + kShotgunRecoilY[gun][flashStep_],
                           own(img::kShotgunHand + hand));
                } else {  // reloading
                    const int pump = (flashStep_ - 5) / 2;
                    drawAt(px + kShotgunDx[gun], py + kShotgunDy[gun], own(img::kShotgun + gun));
                    drawAt(px + kShotgunHandDx[gun] + kShotgunRecoilX[gun][pump],
                           py + kShotgunHandDy[gun] + kShotgunRecoilY[gun][pump],
                           own(img::kShotgunHand + hand));
                }
            } else {
                drawAt(px + kShotgunDx[gun], py + kShotgunDy[gun], own(img::kShotgun + gun));
                drawAt(px + kShotgunHandDx[gun], py + kShotgunHandDy[gun], own(img::kShotgunHand + hand));
            }
            break;

        case Weapon::Laser:
            if (firing) {
                drawAt(px + kLaserDx[gun] + kRecoilX[gun][recoilStep_],
                       py + kLaserDy[gun] + kRecoilY[gun][recoilStep_], own(img::kLaserGun + gun));
                drawAt(px + kLaserGlowDx[gun] + kRecoilX[gun][recoilStep_],
                       py + kLaserGlowDy[gun] + kRecoilY[gun][recoilStep_],
                       shared(common::kLaserGlow + 4 * gun + flashStep_));
            } else {
                drawAt(px + kLaserDx[gun], py + kLaserDy[gun], own(img::kLaserGun + gun));
            }
            break;
    }

    if (!firing) {
        return;
    }

    // The muzzle flash. The laser's has 8 aims (both straight-down aims
    // share one).
    int laserAim = gun;
    if (gun == 9) {
        laserAim = 0;
    } else if (gun > 4) {
        laserAim -= 1;
    }

    switch (weapon) {
        case Weapon::Rifle: {
            const bool blip = blipKind(hero);
            drawAt(px + (blip ? kRifleFlashDx : kBlopRifleFlashDx)[gun] + kRecoilX[gun][recoilStep_],
                   py + (blip ? kRifleFlashDy : kBlopRifleFlashDy)[gun] + kRecoilY[gun][recoilStep_],
                   shared(common::kRifleFlash + gun * 3 + kFlashFrames[flashStep_]));
            break;
        }
        case Weapon::Smg:
            drawAt(px + kSmgFlashDx[gun] + kRecoilX[gun][recoilStep_],
                   py + kSmgFlashDy[gun] + kRecoilY[gun][recoilStep_],
                   own(img::kSmgFlash + gun * 3 + kFlashFrames[flashStep_]));
            break;
        case Weapon::Shotgun:
            if (flashStep_ < 5) {
                drawAt(px + kShotgunFlashDx[gun] + kShotgunRecoilX[gun][flashStep_],
                       py + kShotgunFlashDy[gun] + kShotgunRecoilY[gun][flashStep_],
                       shared(common::kShotgunFlash + gun * 3 + kFlashFrames[flashStep_]));
            }
            break;
        case Weapon::Laser:
            if (weaponAim == direction) {
                drawAt(px + kLaserFlashDx[gun] + kRecoilX[gun][recoilStep_],
                       py + kLaserFlashDy[gun] + kRecoilY[gun][recoilStep_],
                       shared(common::kLaserFlash + laserAim * 4 + flashStep_));
            }
            break;
        case Weapon::Flamethrower:
            break;
    }
}

void Player::drawEye(int px, int py) {
    if (hurtEye_ != 0) {
        if (facesRight(direction)) {
            drawAt(px + 12, py - 14, own(img::kHurtEyeRight));
        } else {
            drawAt(px - 12, py - 14, own(img::kHurtEyeLeft));
        }
        return;
    }

    const int base = firing ? common::kEyeAngry : common::kEye;

    // 6 looks by direction.
    int look = 0;
    if (direction <= aim::kDiagonalDownRight) {
        look = 0;
    } else if (direction <= aim::kRight) {
        look = 1;
    } else if (direction <= aim::kUpRight) {
        look = 2;
    } else if (direction <= aim::kDiagonalUpLeft) {
        look = 3;
    } else if (direction <= aim::kLeft) {
        look = 4;
    } else {
        look = 5;
    }

    drawAt(facesRight(direction) ? px + 12 : px - 12, py - 14, shared(base + look));
}

}  // namespace bb::game
