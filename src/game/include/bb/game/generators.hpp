#pragma once

#include "bb/game/entity.hpp"

namespace bb::game {

struct World;

// Spawns an enemy of the registry every `period` ticks, `capacity` times
// (gen_ennemi.cpp): not while a player stands within 100 pixels, nor while
// the generators are off (Flag::GeneratorsOff); it leaves when the camera
// is 100 pixels past it. A temporary one (gen_ennemi_tmp.cpp) spawns even
// with a player close and leaves when its spot leaves the screen
// (facing right: 200 pixels to the left; facing left: once the camera
// passes x - 690).
//
// Facing left, it watches the 2002 screen's right edge (camera x + 640),
// not the view's, as no other entity does: the 2002 levels fill their
// locked arenas from generators placed 60 to 160 pixels right of the
// screen, which a wider view would show, and which would then leave before
// the camera reached the lock (fewer enemies in 25 arenas of 8 levels;
// lem's lock at 425 waited for 100 kills that came too slowly, and its
// first lemmings and dork's first Pokémon never came). In a wider view
// their enemies can then appear inside the view's right part.
class EnemyGenerator : public Generator {
public:
    struct Setup {
        int legacyId;
        int x, y;
        int direction;
        int period;
        int capacity;
        bool temporary;
    };
    EnemyGenerator(World& world, const Setup& setup) noexcept : world_(world), setup_(setup) {}
    void update() override;
    bool finished() override { return done_; }

private:
    void spawn();

    World& world_;
    Setup setup_;
    int tick_ = 0;
    bool done_ = false;
};

// Drops a pickup from the top of the screen every `period` ticks while
// fewer than 3 are out (gen_bonus.cpp); it leaves when the camera is 500
// pixels past its activation X.
class PickupGenerator : public Generator {
public:
    PickupGenerator(World& world, int x, int period) noexcept : world_(world), x_(x), period_(period) {}
    void update() override;
    bool finished() override { return done_; }

private:
    World& world_;
    int x_;
    int period_;
    int tick_ = 0;
    bool done_ = false;
};

// The next pickup of the level's set (World::pickupMode, Flag::Bonus) at
// (x, y) (MakeBonus).
void dropPickup(World& world, int x, int y);
// The next pickup of the weapons-only set (MakeBonusWeapon): what killed
// enemies drop.
void dropWeaponPickup(World& world, int x, int y);

}  // namespace bb::game
