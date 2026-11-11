#include <doctest.h>

#include "bb/core/assets/asset_types.hpp"
#include "bb/game/level_event.hpp"
#include "bb/game/world.hpp"

namespace {

using bb::core::assets::LevelEventRecord;
using bb::core::assets::LevelEventType;
using bb::game::Flag;
using bb::game::World;

LevelEventRecord record(LevelEventType type, int activationX) {
    LevelEventRecord r;
    r.type = static_cast<int>(type);
    r.activationX = activationX;
    return r;
}

void addEvent(World& world, const LevelEventRecord& r) {
    auto event = bb::game::makeLevelEvent(r);
    REQUIRE(event != nullptr);
    world.sleepingEvents.add(event.release());
}

}  // namespace

TEST_CASE("makeLevelEvent makes every used event type") {
    for (auto type : {LevelEventType::EnemyGenerator, LevelEventType::BannerText,
                      LevelEventType::Music, LevelEventType::PickupGenerator,
                      LevelEventType::Vehicle, LevelEventType::Enemy, LevelEventType::Dialogue}) {
        CHECK(bb::game::makeLevelEvent(record(type, 0)) != nullptr);
    }
    // Never used by the shipped levels.
    for (auto type : {LevelEventType::Overkill, LevelEventType::DynamicLoad}) {
        CHECK(bb::game::makeLevelEvent(record(type, 0)) == nullptr);
    }
}

TEST_CASE("A banner text waits for its flag condition") {
    World world;
    bb::game::BannerTextEvent always(500, 0, 2, 7);
    bb::game::BannerTextEvent equal(500, 1, 2, 7);
    bb::game::BannerTextEvent atLeast(500, 2, 2, 7);

    CHECK(always.ready(world));
    CHECK_FALSE(equal.ready(world));
    CHECK_FALSE(atLeast.ready(world));

    world.flags[Flag::User2] = 7;
    CHECK(equal.ready(world));
    CHECK(atLeast.ready(world));

    world.flags[Flag::User2] = 8;
    CHECK_FALSE(equal.ready(world));
    CHECK(atLeast.ready(world));
}

namespace {

class Dummy : public bb::game::Entity {
public:
    void update() override {}
    void draw() override {}
    bb::game::EntityState state() const override { return {}; }
};

bb::game::Entity* makeDummy(World&, const bb::game::SpawnPoint& at) {
    auto* dummy = new Dummy;
    dummy->x = at.x;
    dummy->y = at.y;
    return dummy;
}

}  // namespace

TEST_CASE("An enemy generator spawns every period, away from players, until its capacity") {
    using bb::game::EnemyGenerator;
    bb::game::EntityRegistry registry;
    registry.add({bb::game::EntityCategory::Enemy, 3, "dummy", bb::game::Placement::Enemies, makeDummy});
    World world;
    world.registry = &registry;

    Dummy player;
    player.x = 1000;
    player.y = 300;
    world.players.add(&player);

    // At (500, 300), every 4 ticks, 2 times.
    EnemyGenerator generator(world, EnemyGenerator::Setup{3, 500, 300, 0, 4, 2, false});
    for (int tick = 0; tick < 4; tick++) {
        generator.update();
    }
    REQUIRE(world.enemies.count() == 1);
    CHECK_FALSE(generator.finished());

    // A player within 100 pixels: that spawn is skipped, but it counts.
    player.x = 550;
    for (int tick = 0; tick < 4; tick++) {
        generator.update();
    }
    CHECK(world.enemies.count() == 1);
    CHECK(generator.finished());

    // Off while the generators are off.
    EnemyGenerator paused(world, EnemyGenerator::Setup{3, 500, 300, 0, 1, 5, false});
    player.x = 1000;
    world.flags[Flag::GeneratorsOff] = 1;
    paused.update();
    CHECK(world.enemies.count() == 1);
    world.flags[Flag::GeneratorsOff] = 0;
    paused.update();
    CHECK(world.enemies.count() == 2);

    world.players.clear();
}

TEST_CASE("Events wake with the camera, then run when ready") {
    World world;

    // Added in reverse level order: the list visits the newest first.
    LevelEventRecord dialogue = record(LevelEventType::Dialogue, 200);
    dialogue.id = 4;
    dialogue.y = 1;  // conditional
    dialogue.capacity = 2;
    dialogue.value = 7;
    addEvent(world, dialogue);

    LevelEventRecord lock = record(LevelEventType::ScrollLock, 100);
    lock.y = 2;
    lock.capacity = 1;
    lock.value = 3;
    addEvent(world, lock);

    LevelEventRecord flag = record(LevelEventType::SetFlag, 0);
    flag.capacity = 2;
    flag.value = 7;
    addEvent(world, flag);

    world.camera.x = 50;
    world.updateEvents();
    CHECK(world.flags[Flag::User2] == 7);
    CHECK_FALSE(world.scrollLock.on);
    CHECK(world.sleepingEvents.size() == 2);
    CHECK(world.events.size() == 0);

    world.camera.x = 250;
    world.flags[Flag::User2] = 0;
    world.updateEvents();
    CHECK(world.scrollLock.on);
    CHECK(world.scrollLock.x == 100);
    CHECK(world.scrollLock.condition == 2);
    CHECK(world.scrollLock.flag == 1);
    CHECK(world.scrollLock.value == 3);
    CHECK(world.dialogueToPlay == -1);  // waits for flag 2 == 7
    CHECK(world.events.size() == 1);

    world.flags[Flag::User2] = 7;
    world.updateEvents();
    CHECK(world.dialogueToPlay == 4);
    CHECK(world.events.empty());
}

TEST_CASE("Events wake at their activation in 4:3, earlier on the right of a wider view") {
    using bb::game::Camera;
    using bb::game::wakePoints;
    const auto at = [](LevelEventType type, int activationX, int x) {
        LevelEventRecord r = record(type, activationX);
        r.x = x;
        return r;
    };
    // A level of 10 screens: the 2002 camera reaches 5760, a 16:9 one 5546.
    constexpr int kLevel = 6400;
    const std::vector<LevelEventRecord> events = {
        at(LevelEventType::Enemy, 5700, 6440),       // from the right, near the end
        at(LevelEventType::Music, 5700, 0),          // camera-bound, the view cannot get there
        at(LevelEventType::Enemy, 6000, 6740),       // never reached in 2002
        at(LevelEventType::Enemy, 3100, 3840),       // from the right, behind a lock
        at(LevelEventType::Enemy, 3050, 3790),       // from the right, behind the same lock
        at(LevelEventType::ScrollLock, 3000, 0),     // the lock
        at(LevelEventType::Enemy, 3000, 3740),       // the lock's wave, from the right
        at(LevelEventType::Enemy, 2500, 2450),       // from the left
        at(LevelEventType::Foreground, 1000, 1740),  // a decal from the right
        at(LevelEventType::SetFlag, 1000, 0),
    };
    // 4:3: as in 2002 (then in file order, nothing moves).
    std::vector<int> narrow = wakePoints(events, kLevel, Camera::kNarrowWidth, kLevel);
    for (std::size_t i = 0; i < events.size(); ++i) {
        CAPTURE(i);
        CHECK(narrow[i] == events[i].activationX);
    }

    const int extra = Camera::kWideWidth - Camera::kNarrowWidth;  // 214
    const std::vector<int> wide = wakePoints(events, kLevel, Camera::kWideWidth, kLevel);
    CHECK(wide[0] == 5700 - extra);
    CHECK(wide[1] == kLevel - Camera::kWideWidth);  // the last camera x: 5546
    CHECK(wide[2] == 6000);                         // still never
    CHECK(wide[3] == 3001);                         // not before the lock at 3000 opens
    CHECK(wide[4] == 3001);
    CHECK(wide[5] == 3000);
    CHECK(wide[6] == 3000 - extra);  // comes from the right before the lock
    CHECK(wide[7] == 2500);
    CHECK(wide[8] == 1000 - extra);
    CHECK(wide[9] == 1000);

    CHECK(bb::game::reachableCameraX(5759, kLevel, Camera::kWideWidth, kLevel) == 5546);
    CHECK(bb::game::reachableCameraX(5759, kLevel, Camera::kNarrowWidth, kLevel) == 5759);
    CHECK(bb::game::reachableCameraX(5800, kLevel, Camera::kWideWidth, kLevel) == 5800);
}

TEST_CASE("A wider view stops where the 2002 screen stopped for good") {
    using bb::game::Camera;
    using bb::game::viewLimit;
    LevelEventRecord lock = record(LevelEventType::ScrollLock, 7037);
    const std::vector<LevelEventRecord> events = {lock, record(LevelEventType::ScrollLock, 5352)};
    // snuf1: 13 screens, the last scroll lock and the victory at 7037; the
    // 2002 screen showed up to 7677, never its last picture again (8320).
    CHECK(viewLimit(events, 8320, Camera::kNarrowWidth, 7037) == 8320);
    CHECK(viewLimit(events, 8320, Camera::kWideWidth, 7037) == 7677);
    CHECK(viewLimit(events, 8320, Camera::kWideWidth, 7500) == 8140);  // the victory further
    CHECK(viewLimit({}, 8320, Camera::kWideWidth, 0) == 8320);          // no end: the level's
    CHECK(viewLimit(events, 6400, Camera::kWideWidth, 7037) == 6400);   // never beyond the level
    // The lock wakes at the last camera x, 7677 - 854.
    const std::vector<int> wake = bb::game::wakePoints(events, 8320, Camera::kWideWidth, 7677);
    CHECK(wake[0] == 6823);
    CHECK(wake[1] == 5352);
    CHECK(bb::game::reachableCameraX(7037, 8320, Camera::kWideWidth, 7677) == 6823);
}
