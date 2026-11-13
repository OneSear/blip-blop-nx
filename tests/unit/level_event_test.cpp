#include <doctest.h>

#include <algorithm>

#include "bb/core/assets/asset_types.hpp"
#include "bb/game/actor.hpp"
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
    std::vector<int> narrow = wakePoints(events, Camera::kNarrowWidth, kLevel);
    for (std::size_t i = 0; i < events.size(); ++i) {
        CAPTURE(i);
        CHECK(narrow[i] == events[i].activationX);
    }

    const int extra = Camera::kWideWidth - Camera::kNarrowWidth;  // 214
    const std::vector<int> wide = wakePoints(events, Camera::kWideWidth, kLevel);
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

    CHECK(bb::game::reachableCameraX(5759, Camera::kWideWidth, kLevel) == 5546);
    CHECK(bb::game::reachableCameraX(5759, Camera::kNarrowWidth, kLevel) == 5759);
    CHECK(bb::game::reachableCameraX(5800, Camera::kWideWidth, kLevel) == 5800);
}

TEST_CASE("Events that share a wake point run in the order the 2002 camera met them") {
    using bb::game::Camera;
    // snuf2's boss arena: the generators go off at 4322 and back on at 4502,
    // before the lock at 4504. A 16:9 view stops at 4290 (its right edge at
    // the 2002 screen's, 5144), so all three wake there in one tick.
    LevelEventRecord lock = record(LevelEventType::ScrollLock, 4504);
    lock.y = 2;  // until flag 0 is 666
    lock.value = 666;
    LevelEventRecord on = record(LevelEventType::SetFlag, 4502);
    on.capacity = static_cast<int>(Flag::GeneratorsOff);
    on.value = 0;
    LevelEventRecord off = record(LevelEventType::SetFlag, 4322);
    off.capacity = static_cast<int>(Flag::GeneratorsOff);
    off.value = 1;
    const std::vector<LevelEventRecord> events = {lock, on, off};  // file order
    constexpr int kLevel = 8320;

    const auto load = [&](World& world, int viewWidth) {
        const int limit = bb::game::viewLimit(events, kLevel, viewWidth, 4504);
        const std::vector<int> wake = bb::game::wakePoints(events, viewWidth, limit);
        for (const std::size_t i : bb::game::sleepOrder(events, wake)) {
            auto event = bb::game::makeLevelEvent(events[i]);
            REQUIRE(event != nullptr);
            event->wakeX = wake[i];
            world.sleepingEvents.add(event.release());
        }
    };

    SUBCASE("4:3: one at a time, in file order") {
        World world;
        load(world, Camera::kNarrowWidth);
        world.camera.x = 4322;
        world.updateEvents();
        CHECK(world.flags[Flag::GeneratorsOff] == 1);
        world.camera.x = 4504;
        world.updateEvents();
        CHECK(world.flags[Flag::GeneratorsOff] == 0);
        CHECK(world.scrollLock.on);
    }
    SUBCASE("16:9: all at once, off then on") {
        World world;
        load(world, Camera::kWideWidth);
        world.camera.x = 4289;
        world.updateEvents();
        CHECK(world.sleepingEvents.size() == 3);
        world.camera.x = 4290;
        world.updateEvents();
        CHECK(world.sleepingEvents.empty());
        CHECK(world.flags[Flag::GeneratorsOff] == 0);
        CHECK(world.scrollLock.on);
    }

    // For one activation X, the file's order; in 4:3, the file's order.
    const std::vector<LevelEventRecord> same = {record(LevelEventType::Music, 700),
                                                record(LevelEventType::SetFlag, 700),
                                                record(LevelEventType::SetFlag, 600)};
    CHECK(bb::game::sleepOrder(same, {700, 700, 600}) == std::vector<std::size_t>{0, 1, 2});
    CHECK(bb::game::sleepOrder(same, {600, 600, 600}) == std::vector<std::size_t>{2, 0, 1});
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
    // A victory past every lock: the 2002 camera went on to the level's end.
    CHECK(viewLimit(events, 8320, Camera::kWideWidth, 7500) == 8320);
    CHECK(viewLimit({}, 8320, Camera::kWideWidth, 0) == 8320);         // no end: the level's
    CHECK(viewLimit(events, 6400, Camera::kWideWidth, 7037) == 6400);  // never beyond the level
    // The lock wakes at the last camera x, 7677 - 854.
    const std::vector<int> wake = bb::game::wakePoints(events, Camera::kWideWidth, 7677);
    CHECK(wake[0] == 6823);
    CHECK(wake[1] == 5352);
    CHECK(bb::game::reachableCameraX(7037, Camera::kWideWidth, 7677) == 6823);
}

TEST_CASE("What lies beyond the 2002 camera's reach never wakes in a wider view") {
    using bb::game::Camera;
    using bb::game::viewLimit;
    // snuf2: 13 screens, the victory and the final lock at 4504. The
    // generators, Smurfs and storks placed beyond never woke in 2002.
    LevelEventRecord generator = record(LevelEventType::EnemyGenerator, 5308);
    generator.x = 6108;
    const std::vector<LevelEventRecord> snuf2 = {generator, record(LevelEventType::Music, 5000),
                                                 record(LevelEventType::ScrollLock, 4504),
                                                 record(LevelEventType::SetFlag, 4322)};
    const int limit = viewLimit(snuf2, 8320, Camera::kWideWidth, 4504);
    CHECK(limit == 5144);
    const std::vector<int> wake = bb::game::wakePoints(snuf2, Camera::kWideWidth, limit);
    CHECK(wake[0] == 5308);  // never: the last camera x is 4290
    CHECK(wake[1] == 5000);
    CHECK(wake[2] == 4290);
    CHECK(wake[3] == 4290);

    // snorkniv2: its last lock (4988) opens before its victory X (5759); the
    // 2002 camera went on to the level's end, where the boss's dialogue
    // starts the fight.
    const std::vector<LevelEventRecord> snorkniv2 = {record(LevelEventType::Dialogue, 5760),
                                                     record(LevelEventType::ScrollLock, 4988)};
    const int end = viewLimit(snorkniv2, 6400, Camera::kWideWidth, 5759);
    CHECK(end == 6400);
    CHECK(bb::game::wakePoints(snorkniv2, Camera::kWideWidth, end)[0] == 6400 - Camera::kWideWidth);
}

TEST_CASE("A generator placed right of the 2002 screen leaves as in 2002 in a wider view") {
    using bb::game::Camera;
    // The camera x past which each stays (x - 690): lem's first lemmings,
    // from just right of the first screen, inside a 16:9 view at the start;
    // and those of its arena locked at 425 (x 1159), which a 16:9 view wakes
    // at 145 and which must stay while the lock holds the camera.
    struct Case {
        int activationX, x, lastCamera;
    };
    for (const Case c : {Case{-86, 714, 24}, Case{359, 1159, 469}}) {
        LevelEventRecord r = record(LevelEventType::EnemyGenerator, c.activationX);
        r.x = c.x;
        r.value = bb::game::kFacingLeft;
        r.period = 1000;
        r.capacity = 100;
        r.temporary = 1;
        for (const int width : {Camera::kNarrowWidth, Camera::kWideWidth}) {
            CAPTURE(c.x);
            CAPTURE(width);
            World world;
            world.camera.width = width;
            world.camera.limit = width == Camera::kNarrowWidth ? 2560 : 1910;
            auto event = bb::game::makeLevelEvent(r);
            REQUIRE(event != nullptr);
            event->wakeX = bb::game::wakePoints({r}, width, world.camera.limit)[0];
            world.camera.x = std::max(0, event->wakeX);
            world.sleepingEvents.add(event.release());
            world.updateEvents();
            REQUIRE(world.enemyGenerators.size() == 1);

            world.enemyGenerators.rewind();
            bb::game::Generator* generator = world.enemyGenerators.current();
            generator->update();
            CHECK_FALSE(generator->finished());
            world.camera.x = c.lastCamera;
            generator->update();
            CHECK_FALSE(generator->finished());
            world.camera.x = c.lastCamera + 1;
            generator->update();
            CHECK(generator->finished());
        }
    }
}
