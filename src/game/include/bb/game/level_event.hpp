#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "bb/game/entity_registry.hpp"
#include "bb/game/generators.hpp"

namespace bb::core::assets {
struct LevelEventRecord;
}

namespace bb::game {

struct World;

// A scripted event of the level (the 2002 Event). Events wait in
// World::sleepingEvents, in the order they wake, until the camera reaches
// their wake X (their activation X in 4:3); then they wait in World::events
// until their condition holds, run once and are destroyed.
class LevelEvent {
public:
    virtual ~LevelEvent() = default;

    // Whether it leaves the sleeping list (aReveiller()): the camera
    // reached wakeX.
    virtual bool wakes(const World& world);
    // Whether it runs now (aActiver()).
    virtual bool ready(const World&) { return true; }
    // What it does (doEvent()).
    virtual void run(World& world) = 0;

    int activationX = 0;
    // The camera x at which it wakes (wakePoints()).
    int wakeX = 0;
};

// The camera x at which each event of a level wakes when the view is
// viewWidth pixels wide (Camera::width), for the level's events in file
// order (descending activation X). In 4:3, the activation X: as in 2002.
// In a wider view:
//  * what appears beyond the right edge (an enemy, generator, prop,
//    pickup, decal or vehicle placed right of the middle of the 2002
//    screen: the 2002 editor put them 700 to 800 pixels ahead of their
//    activation) wakes earlier by the extra width, so that it still
//    appears beyond the right edge; but not before the scroll lock behind
//    it is released (an arena does not get the next section's enemies,
//    which could keep the lock from opening);
//  * nothing waits for a camera x the view cannot reach (the last
//    camera x is viewLimit less the view's width; viewLimit is
//    viewLimit() below): a scroll lock or the victory at the end of a
//    level then wakes at the last x;
//  * what never woke in 2002 (an activation beyond the 2002 camera's
//    reach, viewLimit less 640) still never wakes. (Before, the reach was
//    the level's end, and snuf2's 43 events beyond its final lock, never
//    seen in 2002, joined its boss fight.)
std::vector<int> wakePoints(const std::vector<core::assets::LevelEventRecord>& events, int viewWidth,
                            int viewLimit);

// The order in which a level's events (in file order) join the sleeping
// list, given their wake points (wakePoints()): by descending wake point;
// for one wake point, by ascending activation X, then in file order. The
// events that wake in one tick run in that order (World::updateEvents()).
// In 4:3 one wake point is one activation X: the file's order, as in 2002.
// In a wider view, events of several activation Xs can share a wake point
// (the last camera x, or just past a scroll lock) and run in the order the
// 2002 camera met them. (In file order, snuf2's "generators on" ran before
// its "generators off", and its boss fight never ended.)
std::vector<std::size_t> sleepOrder(const std::vector<core::assets::LevelEventRecord>& events,
                                    const std::vector<int>& wake);

// A camera x of the 2002 screen as the view reaches it (the victory X): the
// same, or the last camera x when the view cannot reach it; unchanged past
// the 2002 camera's reach (viewLimit less 640), which nothing reaches.
int reachableCameraX(int x, int viewWidth, int viewLimit);

// The level x a view viewWidth pixels wide never shows past (Camera::limit):
// the level's width in 4:3; in a wider view, the right edge of the 2002
// screen where it stopped for good: at the furthest scroll lock at or past
// the victory X (every level's waits for a flag value that nothing sets:
// the level ends there; the decor beyond was never seen, and some of it is
// filler: snuf1 repeats its last picture there), or at the level's end
// without one (snorkniv2, whose last lock opens before its victory X).
int viewLimit(const std::vector<core::assets::LevelEventRecord>& events, int levelWidth, int viewWidth,
              int victoryX);

// Creates an enemy, a prop or a pickup of the entity registry.
class SpawnEvent : public LevelEvent {
public:
    SpawnEvent(EntityCategory category, int legacyId, const SpawnPoint& at) noexcept
        : category_(category), legacyId_(legacyId), at_(at) {}
    void run(World& world) override;

private:
    EntityCategory category_;
    int legacyId_;
    SpawnPoint at_;
};

// Locks the scrolling at the activation X until a condition holds.
class ScrollLockEvent : public LevelEvent {
public:
    ScrollLockEvent(int condition, int flag, int value) noexcept
        : condition_(condition), flag_(flag), value_(value) {}
    void run(World& world) override;

private:
    int condition_;  // ScrollLock::Condition
    int flag_;
    int value_;
};

// Sets the scrolling speed (ForceScroll).
class ScrollSpeedEvent : public LevelEvent {
public:
    explicit ScrollSpeedEvent(int speed) noexcept : speed_(speed) {}
    void run(World& world) override;

private:
    int speed_;
};

// Sets a level flag.
class SetFlagEvent : public LevelEvent {
public:
    SetFlagEvent(int flag, int value) noexcept : flag_(flag), value_(value) {}
    void run(World& world) override;

private:
    int flag_;
    int value_;
};

// The players cannot fire until a flag reaches a value.
class HoldFireEvent : public LevelEvent {
public:
    HoldFireEvent(int flag, int value) noexcept : flag_(flag), value_(value) {}
    void run(World& world) override;

private:
    int flag_;
    int value_;
};

// Starts rain, snow or the underwater distortion, or stops it.
class WeatherEvent : public LevelEvent {
public:
    WeatherEvent(int type, int intensity) noexcept : type_(type), intensity_(intensity) {}
    void run(World& world) override;

private:
    int type_;
    int intensity_;
};

// Plays a sound of the level bank.
class SoundEvent : public LevelEvent {
public:
    explicit SoundEvent(int sound) noexcept : sound_(sound) {}
    void run(World& world) override;

private:
    int sound_;
};

// Starts a dialogue scene, at once or when a flag equals a value.
class DialogueEvent : public LevelEvent {
public:
    DialogueEvent(int scene, bool conditional, int flag, int value) noexcept
        : scene_(scene), conditional_(conditional), flag_(flag), value_(value) {}
    bool ready(const World& world) override;
    void run(World& world) override;

private:
    int scene_;
    bool conditional_;
    int flag_;
    int value_;
};

// Places an image of the level bank behind the entities (midground) or in
// front of them (foreground).
class DecalEvent : public LevelEvent {
public:
    enum class Layer { Midground, Foreground };
    DecalEvent(Layer layer, int image, int x, int y) noexcept : layer_(layer), image_(image), x_(x), y_(y) {}
    void run(World& world) override;

private:
    Layer layer_;
    int image_;
    int x_;
    int y_;
};

// Starts an enemy generator (EventGenEnnemi).
class EnemyGeneratorEvent : public LevelEvent {
public:
    explicit EnemyGeneratorEvent(const EnemyGenerator::Setup& setup) noexcept : setup_(setup) {}
    void run(World& world) override;

private:
    EnemyGenerator::Setup setup_;
};

// Starts a pickup generator (EventGenBonus).
class PickupGeneratorEvent : public LevelEvent {
public:
    explicit PickupGeneratorEvent(int period) noexcept : period_(period) {}
    void run(World& world) override;

private:
    int period_;
};

// Shows a banner text, at once or when a flag equals or reaches a value
// (EventTexte).
class BannerTextEvent : public LevelEvent {
public:
    BannerTextEvent(int text, int condition, int flag, int value) noexcept
        : text_(text), condition_(condition), flag_(flag), value_(value) {}
    bool ready(const World& world) override;
    void run(World& world) override;

private:
    int text_;
    int condition_;  // 0 at once, 1 flag == value, else flag >= value
    int flag_;
    int value_;
};

// Plays a music track of the level, stopping the one playing, or stops
// one (EventMusic).
class MusicEvent : public LevelEvent {
public:
    MusicEvent(int track, bool play) noexcept : track_(track), play_(play) {}
    void run(World& world) override;

private:
    int track_;
    bool play_;
};

// The event of a level file record, or null for the unused types
// (overkill, dynamic load).
std::unique_ptr<LevelEvent> makeLevelEvent(const core::assets::LevelEventRecord& record);

}  // namespace bb::game
