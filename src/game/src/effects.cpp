// Blood sprays, bubbles, rain and snow (giclure.cpp, bulle.cpp,
// meteo_pluie.cpp, meteo_neige.cpp, Game::creeBulle(),
// Game::updateMeteo()).

#include "bb/game/effects.hpp"

#include "bb/assets/sprites/misc.hpp"
#include "bb/game/trig.hpp"

namespace bb::game {

namespace {

namespace img = bb::assets::sprites::misc;

constexpr int kDrops = 500;   // NB_GOUTTES
constexpr int kFlakes = 500;  // NB_FLOCONS

}  // namespace

//-----------------------------------------------------------------------------
// BloodSpray
//-----------------------------------------------------------------------------

BloodSpray::BloodSpray(World& world, Entity& owner, int sprayDirection, int offsetX, int offsetY)
    : Actor(world), owner_(&owner), offsetX_(offsetX), offsetY_(offsetY) {
    direction = sprayDirection;
}

void BloodSpray::update() {
    Entity* owner = owner_.get();
    if (owner == nullptr || owner->dead || owner->state().state == kDying) {
        dead = true;
        return;
    }

    animation.tick(5);

    if (animation.frame == 4) {
        dead = true;
    } else {
        image = world().media.misc->image(img::kBloodSpray + direction * 4 + animation.frame);
    }

    x = owner->x + offsetX_;
    y = owner->y + offsetY_;
}

void spawnBlood(World& world, Entity& owner, int direction, int offsetX, int offsetY) {
    world.blood.add(new BloodSpray(world, owner, direction, offsetX, offsetY));
}

//-----------------------------------------------------------------------------
// Bubble
//-----------------------------------------------------------------------------

void Bubble::update() {
    phase += phaseStep;
    phase %= 360;

    y += dy;
    x = centreX + ((10 * cosine1024(phase)) >> kTrigShift);

    if (centreX < world().camera.x - 10 || y < -10 || solidWall(x, y)) {
        dead = true;
    }
}

void emitBubble(World& world, Entity& source) {
    source.bubbleDelay += world.random() % 10;

    if (source.bubbleDelay > 300) {
        auto* bubble = new Bubble(world);
        bubble->centreX = source.x;
        bubble->y = source.y - 10;
        bubble->image = world.media.misc->image(img::kBubble + world.random() % 3);
        bubble->phaseStep = 5 + world.random() % 3;
        bubble->dy = -(2 + world.random() % 2);
        world.bubbles.add(bubble);

        source.bubbleDelay = 0;
    }
}

//-----------------------------------------------------------------------------
// Rain and snow
//-----------------------------------------------------------------------------

void Raindrop::update() {
    y += dy;

    // Stops under a roof, on a wall, or at the bottom.
    if (x < world().camera.x || (y > -100 && solidWall(x, 0)) || solidWall(x, y) || y > 540) {
        dead = true;
    }
}

void Snowflake::update() {
    World& w = world();

    phase += 3;
    phase %= 360;
    centreX -= 2;

    x = centreX + ((sway * cosine1024(phase)) >> kTrigShift);
    y += dy;

    // Gone under a roof, settled (on a platform, one time in six; on a
    // wall), or at the bottom; in that order, as the random draw depends on
    // it.
    if ((y > -25 && y < -10 && solidWall(x, 0)) || (platformAt(x, y) != 0 && w.random() % 6 == 0) ||
        solidWall(x, y) || y > 500) {
        dead = true;
    } else if (centreX < w.camera.x - 10) {
        // Blown off the left: comes back on the right unless there is a roof.
        if (!solidWall(w.camera.right(), 0)) {
            centreX += w.camera.width + 20;
        } else {
            dead = true;
        }
    }
}

Precipitation::Precipitation(World& world) : world_(world) {
    drops_.reserve(kDrops);
    flakes_.reserve(kFlakes);
    for (int i = 0; i < kDrops; ++i) {
        drops_.emplace_back(world);
    }
    for (int i = 0; i < kFlakes; ++i) {
        flakes_.emplace_back(world);
    }
}

void Precipitation::update() {
    World& w = world_;
    EntityList<Entity>& particles = w.weatherParticles;

    for (particles.rewind(); !particles.atEnd(); particles.next()) {
        particles.current()->update();
    }

    // Called for rain and snow only (the loop would not end otherwise).
    while (particles.count() < w.weather.intensity) {
        if (w.weather.type == Weather::Snow) {
            nextFlake_ = (nextFlake_ + 1) % kFlakes;
            Snowflake& flake = flakes_[static_cast<std::size_t>(nextFlake_)];

            const int size = w.random() % 3;
            flake.dead = false;
            flake.centreX = w.camera.x + w.random() % (w.camera.width + 160);
            flake.y = w.random() % 480 - 500;
            flake.image = w.media.misc->image(size <= 1 ? img::kSnowflake : img::kSnowflakeLarge);
            flake.dy = w.weather.intensity / 20 + size;
            flake.phase = w.random() % 360;
            flake.sway = 10 + size * 4;

            // Under a roof: starts higher, so that it falls on the roof later.
            if (w.collision.solidWall(flake.centreX, 0)) {
                flake.y -= 550;
            }
            particles.add(&flake);
        } else if (w.weather.type == Weather::Rain) {
            nextDrop_ = (nextDrop_ + 1) % kDrops;
            Raindrop& drop = drops_[static_cast<std::size_t>(nextDrop_)];

            const int size = w.random() % 4;
            drop.dead = false;
            drop.x = w.camera.x + w.random() % (w.camera.width + 160);
            drop.y = w.random() % 480 - 480;
            drop.image = w.media.misc->image(img::kRaindrop + size);
            drop.dy = 7 + size * 2;

            if (w.collision.solidWall(drop.x, 0)) {
                drop.y -= 550;
            }
            particles.add(&drop);
        }
    }
}

}  // namespace bb::game
