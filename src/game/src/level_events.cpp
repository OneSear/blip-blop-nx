// The level events (event_*.h/.cpp of 2002) that only act on the World.

#include <algorithm>
#include <string>
#include <utility>

#include "bb/core/assets/asset_types.hpp"
#include "bb/core/log.hpp"
#include "bb/game/actor.hpp"
#include "bb/game/level_event.hpp"
#include "bb/game/world.hpp"

namespace bb::game {

namespace {

using core::assets::LevelEventRecord;
using core::assets::LevelEventType;

// An image of the level bank placed by a level event (FondStatique, PPlan).
class Decal : public Actor {
public:
    Decal(World& world, bool midground) : Actor(world), midground_(midground) {}

    void update() override {
        // 2002 rule (FondStatique): a midground decal left of x = 320 is
        // dropped at once.
        if (midground_ && x < 320) {
            dead = true;
        }
    }

private:
    bool midground_;
};

}  // namespace

bool LevelEvent::wakes(const World& world) {
    return wakeX <= world.camera.x;
}

namespace {

// Whether the event places something right of the middle of the 2002
// screen at its activation (beyond the right edge: 700 to 800 pixels ahead
// in every level).
bool appearsOnTheRight(const LevelEventRecord& r) {
    switch (static_cast<LevelEventType>(r.type)) {
        case LevelEventType::Enemy:
        case LevelEventType::EnemyGenerator:
        case LevelEventType::AnimatedProp:
        case LevelEventType::MidgroundDecal:
        case LevelEventType::Foreground:
        case LevelEventType::Vehicle:
        case LevelEventType::Pickup:
            return r.x - r.activationX > Camera::kNarrowWidth / 2;
        default:
            return false;
    }
}

}  // namespace

int reachableCameraX(int x, int viewWidth, int viewLimit) {
    const int lastCamera2002 = viewLimit - Camera::kNarrowWidth;
    const int lastCamera = std::max(0, viewLimit - viewWidth);
    return x <= lastCamera2002 ? std::min(x, lastCamera) : x;
}

int viewLimit(const std::vector<LevelEventRecord>& events, int levelWidth, int viewWidth, int victoryX) {
    if (viewWidth <= Camera::kNarrowWidth) {
        return levelWidth;
    }
    // The furthest scroll lock at or past the victory X: the 2002 camera
    // stopped there for good.
    int last = 0;
    for (const LevelEventRecord& r : events) {
        if (static_cast<LevelEventType>(r.type) == LevelEventType::ScrollLock && r.activationX >= victoryX) {
            last = std::max(last, r.activationX);
        }
    }
    if (last <= 0) {
        return levelWidth;
    }
    return std::min(levelWidth, std::max(last + Camera::kNarrowWidth, viewWidth));
}

std::vector<int> wakePoints(const std::vector<LevelEventRecord>& events, int viewWidth, int viewLimit) {
    std::vector<int> wake(events.size());
    const int extra = viewWidth - Camera::kNarrowWidth;
    const int lastCamera2002 = viewLimit - Camera::kNarrowWidth;

    // The scroll locks, by activation X: (activation, wake point).
    std::vector<std::pair<int, int>> locks;
    for (const LevelEventRecord& r : events) {
        if (static_cast<LevelEventType>(r.type) == LevelEventType::ScrollLock) {
            locks.emplace_back(r.activationX, reachableCameraX(r.activationX, viewWidth, viewLimit));
        }
    }
    std::sort(locks.begin(), locks.end());

    for (std::size_t i = 0; i < events.size(); ++i) {
        const LevelEventRecord& r = events[i];
        const int activation = r.activationX;
        int x = activation;
        if (extra > 0 && appearsOnTheRight(r) && activation <= lastCamera2002) {
            x = activation - extra;
            // Not before the last scroll lock behind it is released.
            const auto behind = std::lower_bound(locks.begin(), locks.end(), std::make_pair(activation, 0));
            if (behind != locks.begin()) {
                x = std::max(x, std::prev(behind)->second + 1);
            }
        }
        wake[i] = reachableCameraX(x, viewWidth, viewLimit);
    }
    return wake;
}

std::vector<std::size_t> sleepOrder(const std::vector<LevelEventRecord>& events,
                                    const std::vector<int>& wake) {
    std::vector<std::size_t> order(events.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        if (wake[a] != wake[b]) {
            return wake[a] > wake[b];
        }
        return events[a].activationX < events[b].activationX;
    });
    return order;
}

void SpawnEvent::run(World& world) {
    if (world.registry->spawn(world, category_, legacyId_, at_) == nullptr) {
        core::Log::warning("Unknown entity id " + std::to_string(legacyId_) + " in a level event");
    }
}

void ScrollLockEvent::run(World& world) {
    world.scrollLock.on = true;
    world.scrollLock.x = activationX;
    world.scrollLock.condition = condition_;
    world.scrollLock.flag = flag_;
    world.scrollLock.value = value_;
}

void ScrollSpeedEvent::run(World& world) {
    world.camera.speed = speed_;
}

void SetFlagEvent::run(World& world) {
    world.flags.values[flag_] = value_;
}

void HoldFireEvent::run(World& world) {
    world.holdFire.on = true;
    world.holdFire.flag = flag_;
    world.holdFire.value = value_;
}

void WeatherEvent::run(World& world) {
    world.weather.intensity = intensity_;
    world.weather.type = type_;
}

void SoundEvent::run(World& world) {
    world.media.levelSounds->play(sound_);
}

bool DialogueEvent::ready(const World& world) {
    return !conditional_ || world.flags.values[flag_] == value_;
}

void DialogueEvent::run(World& world) {
    world.dialogueToPlay = scene_;
}

void DecalEvent::run(World& world) {
    const bool midground = layer_ == Layer::Midground;
    auto* decal = new Decal(world, midground);
    decal->x = x_;
    decal->y = y_;
    decal->image = world.media.level->image(image_);
    (midground ? world.midground : world.foreground).add(decal);
}

void EnemyGeneratorEvent::run(World& world) {
    world.enemyGenerators.add(new EnemyGenerator(world, setup_));
}

void PickupGeneratorEvent::run(World& world) {
    world.pickupGenerators.add(new PickupGenerator(world, activationX, period_));
}

bool BannerTextEvent::ready(const World& world) {
    if (condition_ == 0) {
        return true;
    }
    if (condition_ == 1) {
        return world.flags.values[flag_] == value_;
    }
    return world.flags.values[flag_] >= value_;
}

void BannerTextEvent::run(World& world) {
    showBanner(world, text_);
}

void MusicEvent::run(World& world) {
    MusicPlayer& music = *world.media.levelMusic;
    if (play_) {
        if (world.currentMusic != -1) {
            music.stop(world.currentMusic);
        }
        music.play(track_);
        world.currentMusic = track_;
    } else {
        music.stop(track_);
        world.currentMusic = -1;
    }
}

std::unique_ptr<LevelEvent> makeLevelEvent(const LevelEventRecord& r) {
    std::unique_ptr<LevelEvent> event;

    switch (static_cast<LevelEventType>(r.type)) {
        case LevelEventType::Enemy:
            event = std::make_unique<SpawnEvent>(EntityCategory::Enemy, r.id, SpawnPoint{r.x, r.y, r.value});
            break;
        case LevelEventType::AnimatedProp:
            event = std::make_unique<SpawnEvent>(EntityCategory::Prop, r.id, SpawnPoint{r.x, r.y, 0});
            break;
        case LevelEventType::Pickup:
            event = std::make_unique<SpawnEvent>(EntityCategory::Pickup, r.id, SpawnPoint{r.x, r.y, 0});
            break;
        case LevelEventType::ScrollLock:
            event = std::make_unique<ScrollLockEvent>(r.y, r.capacity, r.value);
            break;
        case LevelEventType::ForceScroll:
            event = std::make_unique<ScrollSpeedEvent>(r.y);
            break;
        case LevelEventType::SetFlag:
            event = std::make_unique<SetFlagEvent>(r.capacity, r.value);
            break;
        case LevelEventType::HoldFire:
            event = std::make_unique<HoldFireEvent>(r.capacity, r.value);
            break;
        case LevelEventType::Weather:
            event = std::make_unique<WeatherEvent>(r.id, r.y);
            break;
        case LevelEventType::Sound:
            event = std::make_unique<SoundEvent>(r.id);
            break;
        case LevelEventType::Dialogue:
            event = std::make_unique<DialogueEvent>(r.id, r.y != 0, r.capacity, r.value);
            break;
        case LevelEventType::MidgroundDecal:
            event = std::make_unique<DecalEvent>(DecalEvent::Layer::Midground, r.id, r.x, r.y);
            break;
        case LevelEventType::Foreground:
            event = std::make_unique<DecalEvent>(DecalEvent::Layer::Foreground, r.id, r.x, r.y);
            break;
        case LevelEventType::EnemyGenerator:
            event = std::make_unique<EnemyGeneratorEvent>(
                EnemyGenerator::Setup{r.id, r.x, r.y, r.value, r.period, r.capacity, r.temporary != 0});
            break;
        case LevelEventType::PickupGenerator:
            // (The record's pickup type is unused, as in 2002.)
            event = std::make_unique<PickupGeneratorEvent>(r.period);
            break;
        case LevelEventType::BannerText:
            event = std::make_unique<BannerTextEvent>(r.bank, r.y, r.capacity, r.value);
            break;
        case LevelEventType::Music:
            event = std::make_unique<MusicEvent>(r.id, r.y != 0);
            break;
        case LevelEventType::Vehicle:
            event =
                std::make_unique<SpawnEvent>(EntityCategory::Vehicle, r.id, SpawnPoint{r.x, r.y, r.value});
            break;
        case LevelEventType::Overkill:
        case LevelEventType::DynamicLoad:
            return nullptr;
    }
    if (event) {
        event->activationX = r.activationX;
        event->wakeX = r.activationX;
    }
    return event;
}

}  // namespace bb::game
