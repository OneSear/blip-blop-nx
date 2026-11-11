#include "bb/game/actor.hpp"

#include "bb/assets/sprites/misc.hpp"
#include "bb/game/effects.hpp"
#include "bb/game/generators.hpp"

namespace bb::game {

namespace {

// Below the screen (480 pixels high, no platform under it), with a margin.
constexpr int kFallenOut = 600;

}  // namespace

//-----------------------------------------------------------------------------
// Actor (Sprite)
//-----------------------------------------------------------------------------

void Actor::draw() {
    drawAt(x, y, image);
}

EntityState Actor::state() const {
    EntityState s;
    s.dy = dy;
    s.frame = animation.frame;
    s.subFrame = animation.subFrame;
    s.direction = direction;
    return s;
}

void Actor::fall() {
    gravityDelay += 1;
    gravityDelay %= kGravityDelay;

    if (gravityDelay == 0 && dy < kMaxFallSpeed) {
        dy += 1;
    }
    if (dy < 0 && world_.collision.solidWall(x, y + dy)) {
        dy = kMaxFallSpeed;
    }
    const int ground = world_.collision.platformAt(x, y + dy);
    if (ground != 0 && dy > 0) {
        y = ground;
    } else {
        y += dy;
    }
}

void Actor::fallThrough() {
    gravityDelay += 1;
    gravityDelay %= kGravityDelay;

    if (gravityDelay == 0 && dy < kMaxFallSpeed) {
        dy += 1;
    }
    if (dy < 0 && world_.collision.solidWall(x, y + dy)) {
        dy = kMaxFallSpeed;
    }
    const int ground = world_.collision.platformAt(x, y + dy);
    if (ground != 0 && dy > 0 && world_.collision.platformAt(x, y) == 0) {
        y = ground;
    } else {
        y += dy;
    }
}

void Actor::walk(int dx) {
    x += dx;

    int ground = world_.collision.platformAt(x, y);
    if (ground != 0) {
        y = ground;
        return;
    }
    ground = world_.collision.platformAt(x, y + 5);
    if (ground != 0) {
        y = ground;
        return;
    }
    ground = world_.collision.platformAt(x, y - 5);
    if (ground != 0) {
        y = ground;
    }
}

void Actor::hitboxFromImage() {
    if (image == nullptr) {
        hitbox.active = false;
    } else {
        const int left = x - image->spotX();
        const int top = y - image->spotY();
        hitbox.set(left, top, left + image->width(), top + image->height());
    }
}

void Actor::dieWhenOffscreen() {
    const Camera& camera = world_.camera;
    if (x < camera.x - 250 || x > camera.right() + 260 || y < -200 || y > kFallenOut) {
        dead = true;
    }
}

void Actor::dieWhenFallenOut() {
    if (y > kFallenOut) {
        dead = true;
    }
}

//-----------------------------------------------------------------------------
// MovingPlatform
//-----------------------------------------------------------------------------

void MovingPlatform::carryPlayers(int width, int top, int step) {
    // The players list's own cursor, as in 2002.
    EntityList<Entity>& players = world().players;
    for (players.rewind(); !players.atEnd(); players.next()) {
        Entity* player = players.current();
        const int px = player->x;
        if (px > x && px < x + width && platformAt(px, player->y) == top) {
            player->y += step;
        }
    }
}

//-----------------------------------------------------------------------------
// Character (Personnage)
//-----------------------------------------------------------------------------

EntityState Character::state() const {
    EntityState s = Actor::state();
    s.hitPoints = health.points;
    s.state = mode;
    return s;
}

//-----------------------------------------------------------------------------
// Enemy (Ennemi)
//-----------------------------------------------------------------------------

Enemy::Enemy(World& world) noexcept : Character(world), minX(world.camera.x) {}

void Enemy::hitBy(Shot& shot) {
    World& w = world();
    const int wound = shot.damage();

    shot.onHit(health.take(wound));
    if (health.left()) {
        return;
    }

    animation.restart();
    hitbox.active = false;

    if (shot.burning() == 2) {
        mode = kCarbonised;
        shot.onKill();
    } else {
        mode = kDying;
        shot.onKill();

        // A trace of blood on the wall behind.
        if (image != nullptr) {
            const int traceY = y - (image->height() >> 1) - w.random() % 15;
            if (w.collision.bloodWall(x, traceY)) {
                w.bloodDecal += 1;
                w.bloodDecal %= 14;
                w.media.effects->engrave(
                    x, traceY, w.media.misc->image(bb::assets::sprites::misc::kBloodDecal + w.bloodDecal));
            }
        }
    }

    // Weapon pickups and the kill counter.
    if (countsForKills()) {
        w.treasure += treasure;
        if (w.treasure >= 50 && w.weaponPickupWanted) {
            dropWeaponPickup(w, x, y);
            w.treasure = 0;
        }
        w.flags[Flag::Kills] += 1;
    }
}

void Enemy::sprayBlood(const Shot& shot, const int* offsetX, const int* offsetY) {
    if (bleeding > 0 || shot.burning() != 0) {
        return;
    }
    const int d = shot.bloodDirection();
    spawnBlood(world(), *this, d, offsetX[d], offsetY[d]);
    bleeding = 20;
}

void Enemy::burn(int firstLeft, int firstRight, int steps, int delay) {
    animation.tick(delay);
    if (animation.frame >= steps) {
        dead = true;
    } else {
        image = enemyImage((direction == kFacingLeft ? firstLeft : firstRight) + animation.frame);
    }
}

void Enemy::lob(int fromX, int fromY, int toX, int toY, int& xSpeed, int& ySpeed) const {
    World& w = world();
    int steps = 0;
    int dx = toX - fromX;

    if (fromY < toY) {
        ySpeed = w.random() % 3 - 5;
        int speed = ySpeed;
        while (fromY < toY && steps < 40) {
            fromY += speed * kGravityDelay;
            if (speed < kMaxFallSpeed) {
                speed += 1;
            }
            steps += 1;
        }
    } else {
        ySpeed = w.random() % 2 - 6;
        int speed = ySpeed;
        while (fromY > toY && steps < 40) {
            fromY += speed * kGravityDelay;
            if (speed < kMaxFallSpeed) {
                speed += 1;
            }
            steps += 1;
        }
        dx += dx / 2;
    }

    if (steps == 0) {
        xSpeed = 0;
    } else {
        xSpeed = dx / (steps * kGravityDelay);
        if (dx < -20) {
            xSpeed -= 1;
        } else if (dx > 20) {
            xSpeed += 1;
        }
    }

    if (xSpeed < -6) {
        xSpeed = -6;
    } else if (xSpeed > 6) {
        xSpeed = 6;
    }
}

}  // namespace bb::game
