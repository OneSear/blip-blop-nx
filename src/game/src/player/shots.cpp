// The players' shots and the cow bomb (tir_bb.cpp, tir_bbm16.cpp,
// tir_bbpm.cpp, tir_bb_fusil.cpp, tir_bb_laser.cpp, tir_bblf.cpp,
// tir_bb_vache.cpp).

#include "bb/assets/sounds/misc.hpp"
#include "bb/assets/sprites/bb.hpp"
#include "bb/assets/sprites/misc.hpp"
#include "bb/game/gore.hpp"
#include "bb/game/player.hpp"

namespace bb::game {

namespace {

namespace img = bb::assets::sprites::bb;

// Speeds by the 16 shot headings (0 down, 4 right, 8 up, 12 left).
const int kRifleDx[] = {0, 3, 6, 8, 9, 8, 6, 3, 0, -3, -6, -8, -9, -8, -6, -3};
const int kRifleDy[] = {9, 8, 6, 3, 0, -2, -6, -8, -9, -8, -6, -3, 0, 3, 6, 8};
const int kSmgDx[] = {0, 5, 8, 11, 12, 11, 8, 5, 0, -5, -8, -11, -12, -11, -8, -5};
const int kSmgDy[] = {12, 11, 8, 5, 0, -5, -8, -11, -12, -11, -8, -5, 0, 5, 8, 11};
const int kPelletDx[] = {0, 6, 11, 14, 15, 14, 11, 6, 0, -6, -11, -14, -15, -14, -11, -6};
const int kPelletDy[] = {15, 14, 11, 6, 0, -6, -11, -14, -15, -14, -11, -6, 0, 6, 11, 14};

// The shotgun spread: cyclic tables walked by counters that all blasts of
// both players share, from the start of the level (2002 file statics,
// in World::shared(); reset at each level start, D-50).
const int kLongDelays[] = {1, 2, 2, 1, 2, 1};
const int kDiagonalDelays[] = {1, 2, 3, 2, 1, 3};
const int kLargeSpreads[] = {3, -2, -1, 0, 1, 2, 3};
const int kSmallSpreads[] = {-2, 0, 2, 0, 1, -1, 2, -1, -2, 1};
struct ShotgunSpread {
    int longDelay = 0;
    int diagonalDelay = 0;
    int largeSpread = 0;
    int smallSpread = 0;
};

// Whether a straight shot left the playing area (more than `margin` pixels
// beyond the view's right edge).
bool gone(const World& w, int x, int y, int margin) {
    return w.collision.solidWall(x, y) || x < w.camera.x - 20 || x > w.camera.right() + margin || y < -20 ||
           y > 500;
}

}  // namespace

//-----------------------------------------------------------------------------
// PlayerShot (TirBB)
//-----------------------------------------------------------------------------

void PlayerShot::onHit(int points) {
    dead = true;
    hitbox.active = false;
    owner->addPoints(points);
}

void PlayerShot::onKill() {
    owner->addKill();
}

void PlayerShot::aim(int playerAim) {
    // The 18 player aims onto 16 headings: both straight-down aims are 0.
    if (playerAim == aim::kDownLeft) {
        direction = 0;
    } else if (playerAim > aim::kUpRight) {
        direction = playerAim - 1;
    } else {
        direction = playerAim;
    }
}

//-----------------------------------------------------------------------------
// RifleBullet, SmgBullet
//-----------------------------------------------------------------------------

void RifleBullet::update() {
    x += kRifleDx[direction];
    y += kRifleDy[direction];

    if (gone(world(), x, y, 50)) {
        image = nullptr;
        noHitbox();
        dead = true;
    } else {
        image = playerImage(img::kRifleBullet + direction);
        hitboxFromImage();
    }
}

void SmgBullet::update() {
    x += kSmgDx[direction];
    y += kSmgDy[direction];

    if (gone(world(), x, y, 50)) {
        image = nullptr;
        noHitbox();
        dead = true;
    } else {
        image = playerImage(img::kSmgBullet + direction);
        hitboxFromImage();
    }
}

//-----------------------------------------------------------------------------
// ShotgunPellet (TirBBFusil)
//-----------------------------------------------------------------------------

void ShotgunPellet::aim(int playerAim) {
    PlayerShot::aim(playerAim);
    ShotgunSpread& spread = world().shared<ShotgunSpread>();

    spread.longDelay = (spread.longDelay + 1) % 6;
    spread.largeSpread = (spread.largeSpread + 1) % 7;
    spread.smallSpread = (spread.smallSpread + 1) % 10;

    switch (direction / 2) {
        case 0:  // down and up
        case 4:
            delayX_ = kLongDelays[spread.longDelay];
            delayY_ = 1;
            dx_ = 0;
            dy = kLargeSpreads[spread.largeSpread];
            spreadX_ = kSmallSpreads[spread.smallSpread];
            spreadY_ = 0;
            break;

        case 2:  // right and left
        case 6:
            delayY_ = kLongDelays[spread.longDelay];
            delayX_ = 1;
            dx_ = kLargeSpreads[spread.largeSpread];
            dy = 0;
            spreadX_ = 0;
            spreadY_ = kSmallSpreads[spread.smallSpread];
            break;

        case 1:  // down-right and up-left
        case 5:
            spread.diagonalDelay = (spread.diagonalDelay + 1) % 6;
            delayY_ = kDiagonalDelays[spread.diagonalDelay];
            spread.diagonalDelay = (spread.diagonalDelay + 1) % 6;
            delayX_ = kDiagonalDelays[spread.diagonalDelay];
            dx_ = kSmallSpreads[spread.smallSpread];
            dy = kSmallSpreads[spread.smallSpread];
            spread.largeSpread = (spread.largeSpread + 1) % 7;
            spreadX_ = kLargeSpreads[spread.largeSpread];
            spread.largeSpread = (spread.largeSpread + 1) % 7;
            spreadY_ = kLargeSpreads[spread.largeSpread];
            break;

        case 3:  // up-right and down-left: the first 3 delays only
        case 7:
            spread.diagonalDelay = (spread.diagonalDelay + 1) % 3;
            delayY_ = kDiagonalDelays[spread.diagonalDelay];
            spread.diagonalDelay = (spread.diagonalDelay + 1) % 3;
            delayX_ = kDiagonalDelays[spread.diagonalDelay];
            dx_ = kSmallSpreads[spread.smallSpread];
            dy = -kSmallSpreads[spread.smallSpread];
            spread.largeSpread = (spread.largeSpread + 1) % 7;
            spreadX_ = kLargeSpreads[spread.largeSpread];
            spread.largeSpread = (spread.largeSpread + 1) % 7;
            spreadY_ = kLargeSpreads[spread.largeSpread];
            break;

        default:
            break;
    }

    dx_ += kPelletDx[direction];
    dy += kPelletDy[direction];

    range_ = 10 + world().random() % 7;
    age_ = 0;
}

void ShotgunPellet::update() {
    y += dy;
    x += dx_;

    stepY_ += 1;
    stepY_ %= delayY_;
    if (stepY_ == 0) {
        y += spreadY_;
    }

    stepX_ += 1;
    stepX_ %= delayX_;
    if (stepX_ == 0) {
        x += spreadX_;
    }

    age_ += 1;

    if (gone(world(), x, y, 20)) {
        image = nullptr;
        noHitbox();
        dead = true;
    } else {
        // Drawn as a big bullet, then as a small one once weaker.
        image = playerImage((age_ < range_ ? img::kSmgBullet : img::kRifleBullet) + direction);
        hitboxFromImage();
    }
}

//-----------------------------------------------------------------------------
// LaserBeam (TirBBLaser)
//-----------------------------------------------------------------------------

void LaserBeam::aim(int playerAim) {
    const World& w = world();

    PlayerShot::aim(playerAim);
    firstImage_ = height_ = width_ = 0;
    direction /= 2;  // 8 directions

    int nx = x;
    int ny = y;

    // The straight beams reach the first wall or the screen edge; the
    // diagonal ones are traced when drawn, and keep the hitbox flag of the
    // beam's last use (hits() does not read it).
    switch (direction) {
        case 0:  // down
            if (y < 0) {
                ny = y = 0;
            }
            while (ny > -5 && ny < 485 && !w.collision.solidWall(nx, ny)) {
                ny += 8;
            }
            height_ = ny - y;
            firstImage_ = img::kLaserBeamVertical;
            hitbox.set(x, y, x + 6, ny);
            break;

        case 2:  // right
            while (nx > w.camera.x - 5 && nx < w.camera.right() + 5 && !w.collision.solidWall(nx, ny)) {
                nx += 8;
            }
            width_ = nx - x;
            firstImage_ = img::kLaserBeamHorizontal;
            hitbox.set(x, y, nx, y + 6);
            break;

        case 4:  // up
            while (ny > -5 && ny < 485 && !w.collision.solidWall(nx, ny)) {
                ny -= 8;
            }
            height_ = y - ny;
            firstImage_ = img::kLaserBeamVertical;
            hitbox.set(x, ny, x + 6, y);
            y = ny;
            break;

        case 6:  // left
            while (nx > w.camera.x - 5 && nx < w.camera.right() + 5 && !w.collision.solidWall(nx, ny)) {
                nx -= 8;
            }
            width_ = x - nx;
            firstImage_ = img::kLaserBeamHorizontal;
            hitbox.set(nx, y, x, y + 6);
            x = nx;
            break;

        default:
            break;
    }

    animation.subFrame = 2;
}

void LaserBeam::update() {
    animation.subFrame -= 1;
    if (animation.subFrame == 0) {
        dead = true;
    }
}

void LaserBeam::onHit(int points) {
    owner->addPoints(points);
}

void LaserBeam::draw() {
    const World& w = world();
    Canvas& canvas = *w.media.canvas;
    const ImageBank& misc = *w.media.misc;
    const Image* beam = playerImage(firstImage_ + animation.frame);
    const Image* impact = misc.image(bb::assets::sprites::misc::kLaserImpact + animation.frame);

    int screenX = x - w.camera.x;
    int nx = x;
    int ny = y;

    // A horizontal beam: the image is 640 pixels long, as the 2002 screen;
    // a longer beam across a wider view (16:9) takes more of it.
    const auto drawRow = [&](int left, int length) {
        canvas.drawPart(beam, 0, 0, length, 6, left, y);
        if (w.camera.extra() > 0) {
            for (int done = beam->width(); done < length; done += beam->width()) {
                canvas.drawPart(beam, 0, 0, length - done, 6, left + done, y);
            }
        }
    };

    // The straight beams are a part of a long image; the diagonal ones are
    // drawn dot by dot. The impact is where the beam stops.
    switch (direction) {
        case 0:  // down
            if (y + height_ > 479) {
                height_ = 479 - y;
            }
            canvas.drawPart(beam, 0, 0, 6, height_, screenX, y);
            if (y + height_ < 480) {
                drawAt(x + 2, y + height_, impact);
            }
            break;

        case 1:  // down-right
            while (nx > w.camera.x - 20 && nx < w.camera.right() + 20 && ny > -50 && ny < 500 &&
                   !w.collision.solidWall(nx, ny)) {
                drawAt(nx, ny, playerImage(img::kLaserBeamFalling + animation.frame));
                nx += 8;
                ny += 8;
            }
            drawAt(nx, ny + 5, impact);
            break;

        case 2: {  // right
            const int view = w.camera.width;
            const int right = screenX + width_ > view ? view - screenX : width_;
            drawRow(screenX, right);
            if (screenX + width_ < view) {
                drawAt(x + width_, y + 3, impact);
            }
            break;
        }

        case 3:  // up-right
            while (nx > w.camera.x - 20 && nx < w.camera.right() + 20 && ny > -20 && ny < 500 &&
                   !w.collision.solidWall(nx, ny)) {
                drawAt(nx, ny, playerImage(img::kLaserBeamRising + animation.frame));
                nx += 8;
                ny -= 8;
            }
            drawAt(nx - 5, ny + 10, impact);
            break;

        case 4:  // up (the beam's top is clipped to the screen for good)
            if (y < 0) {
                height_ += y;
                y = 0;
            }
            canvas.drawPart(beam, 0, 0, 6, height_, screenX, y);
            if (y > 0) {
                drawAt(x + 3, y, impact);
            }
            break;

        case 5:  // up-left
            while (nx > w.camera.x - 20 && nx < w.camera.right() + 20 && ny > -20 && ny < 500 &&
                   !w.collision.solidWall(nx, ny)) {
                drawAt(nx, ny, playerImage(img::kLaserBeamFalling + animation.frame));
                nx -= 8;
                ny -= 8;
            }
            drawAt(nx + 5, ny + 10, impact);
            break;

        case 6: {  // left
            // Off the left edge, only the part on the screen (D-92 fixed:
            // 2002 copied width - x, wider than the beam).
            int right = width_;
            if (screenX < 0) {
                right = width_ + screenX;
                screenX = 0;
            }
            drawRow(screenX, right);
            if (screenX > 0) {
                drawAt(x + 2, y + 3, impact);
            }
            break;
        }

        case 7:  // down-left
            while (nx > w.camera.x - 20 && nx < w.camera.right() + 20 && ny > -50 && ny < 500 &&
                   !w.collision.solidWall(nx, ny)) {
                drawAt(nx, ny, playerImage(img::kLaserBeamRising + animation.frame));
                nx -= 8;
                ny += 8;
            }
            drawAt(nx, ny + 5, impact);
            break;

        default:
            break;
    }
}

bool LaserBeam::hits(const Entity& other) const {
    if (direction % 2 == 0) {
        return overlaps(other);
    }
    if (!other.hitbox.active) {
        return false;
    }

    // A diagonal beam hits a box that one of its vertical sides crosses.
    int atLeft = 0;
    int atRight = 0;
    switch (direction) {
        case 1:  // down-right
            if (other.hitbox.x2 < x || other.hitbox.y2 < y) {
                return false;
            }
            atLeft = (other.hitbox.x1 - x) + y;
            atRight = (other.hitbox.x2 - x) + y;
            break;
        case 3:  // up-right
            if (other.hitbox.x2 < x || other.hitbox.y1 > y) {
                return false;
            }
            atLeft = -(other.hitbox.x1 - x) + y;
            atRight = -(other.hitbox.x2 - x) + y;
            break;
        case 5:  // up-left
            if (other.hitbox.x1 > x || other.hitbox.y1 > y) {
                return false;
            }
            atLeft = (other.hitbox.x1 - x) + y;
            atRight = (other.hitbox.x2 - x) + y;
            break;
        default:  // 7, down-left
            if (other.hitbox.x1 > x || other.hitbox.y2 < y) {
                return false;
            }
            atLeft = -(other.hitbox.x1 - x) + y;
            atRight = -(other.hitbox.x2 - x) + y;
            break;
    }
    return (atLeft > other.hitbox.y1 && atLeft < other.hitbox.y2) ||
           (atRight > other.hitbox.y1 && atRight < other.hitbox.y2);
}

//-----------------------------------------------------------------------------
// Flame (TirBBLF)
//-----------------------------------------------------------------------------

void Flame::aim(int playerAim) {
    World& w = world();

    PlayerShot::aim(playerAim);
    animation.restart();
    rise_ = 0;
    large_ = w.random() % 2 == 1;
    shrinking_ = false;
    image = nullptr;

    // The 2002 code had a third, faster type that random() % 2 never chose.
    speed_ = large_ ? 2 + w.random() % 2 : 4 + w.random() % 3;

    switch (direction) {
        case 0:
            dx_ = 1 - w.random() % 3;
            dy = 3 + w.random() % 3;
            break;
        case 1:
            dx_ = 1 + w.random() % 3;
            dy = 2 + w.random() % 3;
            break;
        case 2:
            dx_ = 3 + w.random() % 3;
            dy = 1 + w.random() % 3;
            break;
        case 3:
            dx_ = 3 + w.random() % 3;
            dy = w.random() % 3;
            break;
        case 4:
            dx_ = 3 + w.random() % 3;
            dy = 1 - w.random() % 3;
            break;
        case 5:
            dx_ = 3 + w.random() % 3;
            dy = -w.random() % 3;
            break;
        case 6:
            dx_ = 3 + w.random() % 3;
            dy = -1 - w.random() % 3;
            break;
        case 7:
            dx_ = w.random() % 3;
            dy = -2 - w.random() % 3;
            break;
        case 8:
            dx_ = 1 - w.random() % 3;
            dy = -3 - w.random() % 3;
            break;
        case 9:
            dx_ = -w.random() % 3;
            dy = -3 - w.random() % 3;
            break;
        case 10:
            dx_ = -3 - w.random() % 3;
            dy = -1 - w.random() % 3;
            break;
        case 11:
            dx_ = -3 - w.random() % 3;
            dy = -w.random() % 3;
            break;
        case 12:
            dx_ = -3 - w.random() % 3;
            dy = 1 - w.random() % 3;
            break;
        case 13:
            dx_ = -3 - w.random() % 3;
            dy = w.random() % 3;
            break;
        case 14:
            dx_ = -3 - w.random() % 3;
            dy = 1 + w.random() % 3;
            break;
        case 15:
            dx_ = -1 - w.random() % 3;
            dy = 2 + w.random() % 3;
            break;
        default:
            break;
    }
}

void Flame::update() {
    animation.subFrame += 1;
    animation.subFrame %= speed_;

    // Grows through its images, then shrinks back and is gone.
    if (animation.subFrame == 0) {
        const int first = large_ ? img::kFlameLarge : img::kFlameSmall;
        const int steps = large_ ? 10 : 5;
        if (shrinking_) {
            animation.frame -= 1;
            if (animation.frame < 0) {
                dead = true;
            } else {
                image = playerImage(first + animation.frame);
            }
        } else {
            animation.frame += 1;
            if (animation.frame >= steps) {
                shrinking_ = true;
            } else {
                image = playerImage(first + animation.frame);
            }
        }
    }

    x += dx_;
    y += dy;

    // Flames rise.
    rise_ += 1;
    rise_ %= 17;
    if (rise_ == 0 && dy > -3) {
        dy -= 1;
    }

    hitboxFromImage();
    dieWhenOffscreen();
}

void Flame::onHit(int points) {
    // Burns on, without hitting again this tick.
    if (!dead) {
        hitbox.active = false;
        owner->addPoints(points);
    }
}

//-----------------------------------------------------------------------------
// FallingCow (TirBBVache)
//-----------------------------------------------------------------------------

void FallingCow::update() {
    World& w = world();

    y += dy;

    animation.cycle(8, 8);

    if (solidWall(x, y) && y > 50) {
        // Bursts: a carcass in the decor, gore, a screen shake.
        image = nullptr;
        w.media.effects->engrave(
            x, y, playerImage(animation.frame < 4 ? img::kCowCarcassHeadRight : img::kCowCarcassHeadLeft));
        hitbox.active = false;
        dead = true;
        w.shake.start(10);

        for (int i = 0; i < 15; ++i) {
            const int speedX = w.random() % 11 - 5;
            const int speedY = -2 - w.random() % 6;
            auto* drop = new GoreDrop(w, speedX, speedY);
            drop->x = x + w.random() % 11 - 5;
            drop->y = y - w.random() % 30 - 10;
            w.gore.add(drop);
        }
        w.media.miscSounds->play(bb::assets::sounds::misc::kExplosion);
    } else if (y > 550 || x < w.camera.x - 50) {
        dead = true;
    } else {
        image = playerImage(img::kCowBomb + animation.frame);
        hitboxFromImage();
    }
}

void FallingCow::onHit(int points) {
    owner->addPoints(points);
}

}  // namespace bb::game
