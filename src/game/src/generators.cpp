// The enemy and pickup generators (gen_ennemi.cpp, gen_ennemi_tmp.cpp,
// gen_bonus.cpp) and the pickup sets (make_bonus.cpp).

#include "bb/game/generators.hpp"

#include "bb/game/actor.hpp"

namespace bb::game {

namespace {

// The pickup ids of the registry (EventBonus).
constexpr int kSmg = 0;
constexpr int kShotgun = 1;
constexpr int kLaser = 2;
constexpr int kBeer = 3;
constexpr int kInvincibility = 4;
constexpr int kBarrel = 5;
constexpr int kFlamethrower = 7;

constexpr int kSetSize = 20;

// The pickup sets, chosen by Flag::Bonus; each is dealt in turn.
const int kPickupSets[][kSetSize] = {
    // 0 and 1: SMG and shotgun in turn (for bonus levels).
    {kSmg, kShotgun, kSmg, kShotgun, kSmg, kShotgun, kSmg, kShotgun, kSmg, kShotgun,
     kSmg, kShotgun, kSmg, kShotgun, kSmg, kShotgun, kSmg, kShotgun, kSmg, kShotgun},
    {kSmg, kShotgun, kSmg, kShotgun, kSmg, kShotgun, kSmg, kShotgun, kSmg, kShotgun,
     kSmg, kShotgun, kSmg, kShotgun, kSmg, kShotgun, kSmg, kShotgun, kSmg, kShotgun},
    // 2: flamethrowers only.
    {kFlamethrower, kFlamethrower, kFlamethrower, kFlamethrower, kFlamethrower, kFlamethrower, kFlamethrower,
     kFlamethrower, kFlamethrower, kFlamethrower, kFlamethrower, kFlamethrower, kFlamethrower, kFlamethrower,
     kFlamethrower, kFlamethrower, kFlamethrower, kFlamethrower, kFlamethrower, kFlamethrower},
    // 3: lasers only.
    {kLaser, kLaser, kLaser, kLaser, kLaser, kLaser, kLaser, kLaser, kLaser, kLaser,
     kLaser, kLaser, kLaser, kLaser, kLaser, kLaser, kLaser, kLaser, kLaser, kLaser},
    // 4: SMGs only.
    {kSmg, kSmg, kSmg, kSmg, kSmg, kSmg, kSmg, kSmg, kSmg, kSmg,
     kSmg, kSmg, kSmg, kSmg, kSmg, kSmg, kSmg, kSmg, kSmg, kSmg},
    // 5: shotguns only.
    {kShotgun, kShotgun, kShotgun, kShotgun, kShotgun, kShotgun, kShotgun, kShotgun, kShotgun, kShotgun,
     kShotgun, kShotgun, kShotgun, kShotgun, kShotgun, kShotgun, kShotgun, kShotgun, kShotgun, kShotgun},
    // 6: 4 flamethrowers, 8 shotguns, 8 SMGs.
    {kSmg, kShotgun, kSmg,     kShotgun, kFlamethrower, kSmg, kShotgun, kShotgun, kShotgun, kFlamethrower,
     kSmg, kShotgun, kShotgun, kShotgun, kFlamethrower, kSmg, kShotgun, kSmg,     kShotgun, kFlamethrower},
    // 7: 4 lasers, 8 shotguns, 8 SMGs.
    {kSmg, kShotgun, kSmg,     kShotgun, kLaser, kSmg, kShotgun, kShotgun, kShotgun, kLaser,
     kSmg, kShotgun, kShotgun, kShotgun, kLaser, kSmg, kShotgun, kSmg,     kShotgun, kLaser},
    // 8: 9 SMGs, 8 shotguns, a beer, a barrel, an invincibility.
    {kSmg,     kShotgun, kSmg,    kSmg, kShotgun, kBeer, kSmg,     kShotgun,       kShotgun, kSmg,
     kShotgun, kSmg,     kBarrel, kSmg, kShotgun, kSmg,  kShotgun, kInvincibility, kShotgun, kSmg},
};

void spawnPickup(World& world, int id, int x, int y) {
    world.registry->spawn(world, EntityCategory::Pickup, id, SpawnPoint{x, y, 0});
}

}  // namespace

void dropPickup(World& world, int x, int y) {
    world.pickupSeed += 1;
    world.pickupSeed %= kSetSize;
    spawnPickup(world, kPickupSets[world.pickupMode][world.pickupSeed], x, y);
}

void dropWeaponPickup(World& world, int x, int y) {
    world.weaponPickupSeed += 1;
    world.weaponPickupSeed %= kSetSize;
    spawnPickup(world, kPickupSets[1][world.weaponPickupSeed], x, y);
}

//-----------------------------------------------------------------------------

void EnemyGenerator::spawn() {
    World& w = world_;
    w.registry->spawn(w, EntityCategory::Enemy, setup_.legacyId,
                      SpawnPoint{setup_.x, setup_.y, setup_.direction});
}

void EnemyGenerator::update() {
    World& w = world_;
    if (w.flags[Flag::GeneratorsOff] != 0) {
        return;
    }

    tick_ += 1;
    tick_ %= setup_.period;
    if (tick_ == 0) {
        bool clear = true;
        if (!setup_.temporary) {
            // No player close by (the players list's own cursor).
            EntityList<Entity>& players = w.players;
            players.rewind();
            while (!players.atEnd() && clear) {
                const Entity* player = players.current();
                clear = player->x < setup_.x - 100 || player->x > setup_.x + 100 ||
                        player->y < setup_.y - 100 || player->y > setup_.y + 100;
                players.next();
            }
        }
        if (clear) {
            spawn();
        }
        setup_.capacity -= 1;
        if (setup_.capacity <= 0) {
            done_ = true;
        }
    }

    const int camera = w.camera.x;
    if (!setup_.temporary) {
        if (setup_.x < camera - 100) {
            done_ = true;
        }
    } else if ((setup_.direction == kFacingRight && setup_.x < camera - 200) ||
               (setup_.direction == kFacingLeft && setup_.x < w.camera.right() + 50)) {
        done_ = true;
    }
}

void PickupGenerator::update() {
    World& w = world_;
    if (w.pickups.count() < 3) {
        tick_ += 1;
        tick_ %= period_;
        if (tick_ == 0) {
            dropPickup(w, w.camera.centre() - 200 + w.random() % 400, -50);
        }
        if (w.camera.x > x_ + 500) {
            done_ = true;
        }
    }
}

}  // namespace bb::game
