// The Care Bear shots: rainbow (tir_arc_ciel.h), bouncing star
// (tir_etoile.h), storm cloud (tir_nuage.cpp), lightsaber
// (tir_epee_laser.cpp), the shaman's fireball and column
// (tir_boule_chaman.cpp, tir_colonne_chaman.cpp).

#include "carebears/carebears_world.hpp"

namespace bb::game::carebears {

//-----------------------------------------------------------------------------
// Rainbow
//-----------------------------------------------------------------------------

void Rainbow::update() {
    animation.tick(3);
    if (animation.frame >= 15) {
        dead = true;
    } else {
        image = enemyImage((direction == kFacingRight ? img::kRainbowRight : img::kRainbowLeft) +
                           animation.frame);
        hitboxFromImage();
    }
}

//-----------------------------------------------------------------------------
// Bouncing star
//-----------------------------------------------------------------------------

void BouncingStar::update() {
    static const int kSpeed = 2;

    fall();
    if (platformAt(x, y) != 0 || platformAt(x, y + 5) != 0) {
        dy = -3 - world().random() % 2;  // bounces
    }

    animation.cycle(3, 24);
    image = enemyImage(img::kStarShot + animation.frame);

    // A wall turns it back, both ways (D-80 fixed: 2002 left a star going
    // right stuck against the wall).
    if (direction == kFacingRight) {
        if (solidWall(x + kSpeed, y)) {
            direction = kFacingLeft;
        } else {
            x += kSpeed;
        }
    } else {
        if (solidWall(x - kSpeed, y)) {
            direction = kFacingRight;
        } else {
            x -= kSpeed;
        }
    }

    lifetime_ -= 1;
    if (solidWall(x, y) || lifetime_ <= 0) {
        dead = true;
    }
    hitboxFromImage();
    dieWhenOffscreen();
}

//-----------------------------------------------------------------------------
// Storm cloud
//-----------------------------------------------------------------------------

StormCloud::StormCloud(World& world) : Projectile(world) {
    lifetime_ = 600 + world.random() % 400;
}

void StormCloud::update() {
    switch (phase_) {
        case 0:
            appear();
            break;
        case 1:
            drift();
            break;
        case 2:
            vanish();
            break;
        case 3:
            strike();
            break;
        default:
            break;
    }
    dieWhenOffscreen();
}

void StormCloud::appear() {
    static const int kSpeed = 1;
    const int camera = world().camera.x;
    const int cameraRight = world().camera.right();

    animation.subFrame += 1;
    animation.subFrame %= 10;
    if (direction == kFacingRight) {
        x += kSpeed;
        if (x > cameraRight - 40) {
            direction = kFacingLeft;
        }
    } else {
        x -= kSpeed;
        if (x < camera + 30) {
            direction = kFacingRight;
        }
    }

    if (animation.subFrame == 0) {
        animation.frame += 1;
        if (animation.frame >= 3) {
            animation.restart();
            phase_ = 1;
        } else {
            image = enemyImage(img::kCloudShotAppear + animation.frame);
        }
    }
    noHitbox();
}

void StormCloud::vanish() {
    static const int kSpeed = 1;

    animation.subFrame += 1;
    animation.subFrame %= 6;
    x += direction == kFacingRight ? kSpeed : -kSpeed;

    if (animation.subFrame == 0) {
        animation.frame += 1;
        if (animation.frame >= 3) {
            dead = true;
        } else {
            image = enemyImage(img::kCloudShotAppear + 2 - animation.frame);
        }
    }
    noHitbox();
}

void StormCloud::drift() {
    static const int kPuff[] = {0, 1, 2, 1};
    static const int kSpeed = 1;
    World& w = world();
    const int camera = w.camera.x;
    const int cameraRight = w.camera.right();

    lifetime_ -= 1;
    if (lifetime_ <= 0) {
        animation.restart();
        phase_ = 2;
    }

    image = enemyImage(img::kCloudShot + animation.loop(kPuff, 4, 9));

    if (direction == kFacingRight) {
        x += kSpeed;
        if (x > cameraRight - 40) {
            direction = kFacingLeft;
        }
    } else {
        x -= kSpeed;
        if (x < camera + 30) {
            direction = kFacingRight;
        }
    }

    // Right above the target player: strike.
    if (strikeStep_ == 0) {
        if (w.target != nullptr && w.target->y > y) {
            const int dx = x - w.target->x;
            if (dx > -10 && dx < 10) {
                strikeStep_ = 0;
                phase_ = 3;
            }
        }
    } else {
        strikeStep_ -= 1;
    }
    noHitbox();
}

void StormCloud::strike() {
    static const int kPuff[] = {0, 1, 2, 1};
    World& w = world();

    boltSubFrame_ += 1;
    boltSubFrame_ %= 2;
    if (boltSubFrame_ == 0) {
        bolt_ += 1;
        bolt_ %= 4;
    }

    lifetime_ -= 1;
    if (lifetime_ <= 0) {
        animation.restart();
        phase_ = 2;
    }

    image = enemyImage(img::kCloudShot + animation.loop(kPuff, 4, 9));

    strikeStep_ += 1;
    if (strikeStep_ == 50) {
        w.media.levelSounds->play(snd::kThunder);
    }
    if (strikeStep_ >= 100) {
        phase_ = 1;
        strikeStep_ = 100;
        hitboxFromImage();
    } else if (strikeStep_ > 50) {
        // The bolt: from the cloud to the bottom of the screen.
        hitbox.set(x - 20, y - 10, x + 20, 480);
    }
    dy = w.random() % 20;  // where the bolt images start (dy as in 2002)
}

void StormCloud::draw() {
    int bolt = bolt_;
    if (phase_ == 3 && strikeStep_ > 50) {
        for (int boltY = y - 10 + dy; boltY < 480; boltY += 100) {
            drawAt(x, boltY, enemyImage(img::kLightning + bolt));
            bolt = (bolt + 1) % 4;
        }
    }
    Actor::draw();
}

//-----------------------------------------------------------------------------
// Lightsaber
//-----------------------------------------------------------------------------

Lightsaber::Lightsaber(World& world) : Projectile(world) {
    lifetime_ = 200 + world.random() % 100;
    world.media.levelSounds->play(snd::kLightsaber, SoundPlayer::kLoop);
}

Lightsaber::~Lightsaber() {
    world().media.levelSounds->stop(snd::kLightsaber);
}

void Lightsaber::update() {
    World& w = world();

    animation.cycle(4, 24);
    image = enemyImage(img::kLightsaber + animation.frame);

    x += xSpeed / 10;
    y += dy / 10;

    // Default goal: far off the left of the screen.
    int goalX = w.camera.x - 400;
    int goalY = 240;

    if (!lost) {
        if (!thrower || thrower->dead || thrower->mode == kDying) {
            lost = true;
        }
    }

    if (lost) {
        dieWhenOffscreen();
        if (dead) {
            w.media.levelSounds->stop(snd::kLightsaber);
        }
    } else if (returning_) {
        goalX = thrower->x;
        goalY = thrower->y - 25;
        const int dx = goalX - x;
        const int dyGoal = goalY - y;
        // Caught within 10 pixels (D-79 fixed: 2002 compared dy, the
        // speed, where dyGoal was meant).
        if (dx > -10 && dx < 10 && dyGoal > -10 && dyGoal < 10) {
            dead = true;
            w.media.levelSounds->stop(snd::kLightsaber);
            thrower->holdsSword = true;
        }
    } else {
        lifetime_ -= 1;
        if (!target || target->dead || lifetime_ < 0) {
            acceleration_ = 4;
            returning_ = true;
        } else {
            goalX = target->x;
            goalY = target->y - 15;
        }
    }

    if (goalX > x && xSpeed < 50) {
        xSpeed += acceleration_;
    } else if (xSpeed > -50) {
        xSpeed -= acceleration_;
    }
    if (goalY > y && dy < 50) {
        dy += acceleration_;
    } else if (dy > -50) {
        dy -= acceleration_;
    }
    hitboxFromImage();
}

//-----------------------------------------------------------------------------
// The shaman's fireball
//-----------------------------------------------------------------------------

void ShamanBall::update() {
    if (!bursting_) {
        x += xSpeed;
        y += dy;
    }

    animation.subFrame += 1;
    animation.subFrame %= 4;

    const bool right = xSpeed > 0;
    if (bursting_) {
        if (animation.subFrame == 0) {
            animation.frame += 1;
            if (animation.frame > 4) {
                dead = true;
                return;
            }
        }
        image =
            enemyImage((right ? img::kShamanBallBurstRight : img::kShamanBallBurstLeft) + animation.frame);
    } else if (flying_) {
        if (animation.subFrame == 0) {
            animation.frame += 1;
            animation.frame %= 5;
        }
        if (solidWall(x, y)) {
            animation.restart();
            bursting_ = true;
        }
        image = enemyImage((right ? img::kShamanBallRight : img::kShamanBallLeft) + animation.frame);
    } else {
        // Grows first.
        if (animation.subFrame == 0) {
            animation.frame += 1;
            if (animation.frame > 4) {
                flying_ = true;
                return;
            }
        }
        image =
            enemyImage((right ? img::kShamanBallStartRight : img::kShamanBallStartLeft) + animation.frame);
    }
    hitboxFromImage();
    dieWhenOffscreen();
}

//-----------------------------------------------------------------------------
// The shaman's column
//-----------------------------------------------------------------------------

ShamanColumn::ShamanColumn(World& world) : Projectile(world) {
    lifetime_ = 250 + world.random() % 100;
}

void ShamanColumn::update() {
    roll();

    animation.subFrame += 1;
    animation.subFrame %= 6;

    // img::kShamanColumnRise is the lowest of the 5 rising images.
    switch (phase_) {
        case 0:  // rising
            if (animation.subFrame == 0) {
                animation.frame += 1;
                if (animation.frame > 4) {
                    phase_ = 1;
                    return;
                }
            }
            image = enemyImage(img::kShamanColumnRise + 4 - animation.frame);
            break;
        case 1:  // rolling
            if (animation.subFrame == 0) {
                animation.frame += 1;
                animation.frame %= 6;
            }
            lifetime_ -= 1;
            if (lifetime_ <= 0) {
                phase_ = 2;
                animation.restart();
                return;
            }
            image = enemyImage(img::kShamanColumn + animation.frame);
            break;
        case 2:  // sinking
            if (animation.subFrame == 0) {
                animation.frame += 1;
                if (animation.frame > 4) {
                    dead = true;
                    return;
                }
            }
            image = enemyImage(img::kShamanColumnRise + animation.frame);
            break;
        default:
            break;
    }
    hitboxFromImage();
    dieWhenOffscreen();
}

void ShamanColumn::roll() {
    static const int kSpeed = 4;
    const int camera = world().camera.x;
    const int cameraRight = world().camera.right();

    fall();

    if (solidWall(x - kSpeed, y) || x + kSpeed < camera) {
        direction = kFacingRight;
    } else if (x + kSpeed > cameraRight) {
        direction = kFacingLeft;
    }
    walk(direction == kFacingRight ? kSpeed : -kSpeed);
}

}  // namespace bb::game::carebears
