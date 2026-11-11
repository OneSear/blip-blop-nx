#pragma once

#include <memory>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include "bb/core/assets/asset_types.hpp"
#include "bb/core/gfx/sprite.hpp"
#include "bb/core/rng.hpp"
#include "bb/game/banner.hpp"
#include "bb/game/entity.hpp"
#include "bb/game/entity_list.hpp"
#include "bb/game/entity_registry.hpp"
#include "bb/game/level_event.hpp"
#include "bb/game/media.hpp"

namespace bb::core::assets {
struct LevelData;
}

namespace bb::game {

// The level's collision data: per-pixel platform heights on 8 layers and
// two grids of 8x8 cells, the solid walls and the walls that bleed when
// shot. Built from the level file; moving platforms rewrite platform
// heights while the level runs.
class CollisionMap {
public:
    static constexpr int kLayers = 8;  // layers 6 and 7 are slippery
    static constexpr int kWallRows = 60;
    static constexpr int kCellSize = 8;
    static constexpr int kScreenWidth = 640;
    static constexpr int kScreenHeight = 480;

    CollisionMap() = default;
    CollisionMap(const CollisionMap&) = delete;
    CollisionMap& operator=(const CollisionMap&) = delete;

    void load(const core::assets::LevelData& level);
    void clear();

    // The height of the platform under (x, y), that is a platform whose
    // height h has h <= y <= h + 10; 0 when there is none or outside the
    // level (the 2002 plat()).
    int platformAt(int x, int y) const;
    // The layer of that platform, or -1 (plat2()).
    int platformLayerAt(int x, int y) const;
    // Sets the platform height of `layer` over [from, to), clipped to the
    // level (the 2002 moving platforms wrote unchecked, D-41).
    void setPlatform(int layer, int from, int to, int height);
    // mur_opaque() and mur_sanglant().
    bool solidWall(int x, int y) const;
    bool bloodWall(int x, int y) const;

    // Level size in 640-pixel screens and in pixels.
    int screens = 0;
    int width = 0;

    // Row tables in the 2002 layout, for code that indexes them directly:
    // platforms[layer][x], solidWalls[row][column], bloodWalls[row][column]
    // (null when no level is loaded). One allocation per row, as in 2002.
    int** platforms = nullptr;
    bool** solidWalls = nullptr;
    bool** bloodWalls = nullptr;

private:
    std::vector<std::unique_ptr<int[]>> platformRows_;
    std::vector<std::unique_ptr<bool[]>> solidRows_;
    std::vector<std::unique_ptr<bool[]>> bloodRows_;
    std::vector<int*> platformTable_;
    std::vector<bool*> solidTable_;
    std::vector<bool*> bloodTable_;
};

// The 11 level flags: 4 registers for level scripts, then values the game
// keeps up to date for events and victory conditions.
enum class Flag {
    User0 = 0,
    User1 = 1,
    User2 = 2,
    User3 = 3,
    Bubbles = 4,  // non-zero: players and enemies give off bubbles
    Bonus = 5,    // unused (one commented-out reference)
    Timer = 6,    // seconds left on a timed level
    GeneratorsOff = 7,
    Kills = 8,
    Enemies = 9,      // enemies alive
    Generators = 10,  // enemy generators alive
};

struct GameFlags {
    static constexpr int kCount = 11;
    int values[kCount] = {};

    int& operator[](Flag flag) noexcept { return values[static_cast<int>(flag)]; }
    int operator[](Flag flag) const noexcept { return values[static_cast<int>(flag)]; }
};

// The scrolling camera and the view it shows: 640 pixels wide as in 2002
// (4:3), or 854 with the widescreen option (16:9 at 480 lines). The view
// width is set at each level start; the entities use the view's edges
// (right(), centre()) where the 2002 code used the screen's, so that a
// level plays the same in 4:3 as in 2002.
struct Camera {
    static constexpr int kNarrowWidth = core::gfx::kScreenWidth;
    static constexpr int kWideWidth = core::gfx::kWideScreenWidth;

    int x = 0;      // left edge of the screen in level pixels
    int width = kNarrowWidth;  // of the view
    int speed = 0;  // scroll speed (scroll events)
    // Scrolling stops while a player dies or comes back (one flag per
    // player, four in the four-player mode).
    bool stopped[4] = {false, false, false, false};

    // Whether no player stops the scrolling.
    bool scrolls() const noexcept { return !stopped[0] && !stopped[1] && !stopped[2] && !stopped[3]; }
    // The view's right edge and middle, in level pixels (x + 640 and
    // x + 320 in 2002).
    int right() const noexcept { return x + width; }
    int centre() const noexcept { return x + width / 2; }
    // How much wider than the 2002 screen the view is (0 in 4:3).
    int extra() const noexcept { return width - kNarrowWidth; }
};

// Drawing between the last two ticks (the smooth motion option, Phase 9).
// The simulation runs 84.7 ticks per second and the screen shows 60
// frames, so a frame shows one or two ticks of movement: a 2-pixel-per-tick
// walk moves 2 or 4 pixels per frame, unevenly. With the option on, a
// frame shows positions part of the way from where they were at the start
// of the last tick to where they are, by the fraction of a tick that the
// clock has owed since; movement is then even, one tick late. Drawing
// only: the simulation and its fingerprints do not see it.
struct Motion {
    // Larger moves in one tick are jumps (teleports, a new place): they
    // are drawn where they end.
    static constexpr int kMaxStep = 16;

    bool on = false;
    int fraction = 256;  // of a tick, in 256ths: 256 draws where things are
    int previousCameraX = 0;

    // The offset to draw at, from `to` towards `from`.
    int offset(int from, int to) const noexcept {
        const int back = (from - to) * (256 - fraction);
        return back >= 0 ? (back + 128) / 256 : -((-back + 128) / 256);
    }
    // The offsets of an entity that moved from (fromX, fromY) to (x, y)
    // (Entity::kNowhere: none).
    void offsets(int fromX, int fromY, int x, int y, int& dx, int& dy) const noexcept {
        dx = 0;
        dy = 0;
        if (!on || fromX == Entity::kNowhere || fromX - x > kMaxStep || x - fromX > kMaxStep ||
            fromY - y > kMaxStep || y - fromY > kMaxStep) {
            return;
        }
        dx = offset(fromX, x);
        dy = offset(fromY, y);
    }
};

// A scroll lock: the camera stays at x until a condition holds.
struct ScrollLock {
    enum Condition { NoEnemies = 0, NoGenerators = 1, FlagEquals = 2, FlagAtLeast = 3 };
    bool on = false;
    int x = 0;
    int condition = 0;
    int flag = 0;
    int value = 0;
};

// Players cannot fire until a flag reaches a value.
struct HoldFire {
    bool on = false;
    int flag = 0;
    int value = 0;
};

struct Weather {
    enum Type { Rain = 0, Snow = 1, Distortion = 2 };
    int type = 0;
    int intensity = 0;
};

// The vertical screen shake started by explosions.
struct ScreenShake {
    int dy = 0;
    int step = 0;
    int amplitude = 0;
    int ddy = 0;

    // tremblement()
    void start(int newAmplitude) noexcept {
        amplitude = newAmplitude;
        ddy = (newAmplitude >> 1) + 1;
        dy = -newAmplitude;
        step = 0;
    }
};

// The banks, drawing and effects the entities use.
struct Media {
    const ImageBank* enemies = nullptr;  // the level's enemy bank
    const ImageBank* misc = nullptr;     // pickups, blood decals, HUD
    const ImageBank* level = nullptr;    // the level's props
    const ImageBank* players = nullptr;  // shared by Blip and Blop
    const ImageBank* blip = nullptr;     // each hero's own images
    const ImageBank* blop = nullptr;
    const ImageBank* bleach = nullptr;  // (the four-player mode)
    const ImageBank* blitz = nullptr;
    const ImageBank* dialogueFaces = nullptr;  // the level's (rpg bank)
    const ImageBank* heroFaces = nullptr;      // Blip's and Blop's (rpg_bb.gfx)
    SoundPlayer* levelSounds = nullptr;
    SoundPlayer* miscSounds = nullptr;
    SoundPlayer* commonSounds = nullptr;  // Blip and Blop, shared effects
    MusicPlayer* levelMusic = nullptr;
    Canvas* canvas = nullptr;
    const Fonts* fonts = nullptr;
    const TextSource* texts = nullptr;
    LevelEffects* effects = nullptr;
};

// Everything a level runs on (migration.md, sections 3.4 and Phase 4).
// One World is reused from level to level; the level start resets it.
struct World {
    Camera camera;
    Motion motion;
    ScrollLock scrollLock;
    HoldFire holdFire;
    GameFlags flags;
    CollisionMap collision;
    Weather weather;
    ScreenShake shake;

    Media media;

    // The entity types level events and generators create.
    const EntityRegistry* registry = nullptr;

    // The game's random generator (one stream for the session, as in
    // 2002: the plan's per-level generator would change every replay).
    core::Rng* rng = nullptr;
    int random() { return rng->next(); }

    // The player enemies aim at, chosen in turn every tick among the
    // players (a stand-in at the screen centre when there is none).
    Entity* target = nullptr;
    int targetTurn = 0;

    // Treasure points of the enemies killed since the last weapon pickup
    // dropped (a pickup at 50), and the next blood decal (of 14). Never
    // reset, as in 2002: they carry over from level to level.
    int treasure = 0;
    int bloodDecal = 0;
    // The pickup set (Flag::Bonus, copied once per tick as in 2002) and
    // the next pickup of it and of the weapons-only set; never reset.
    int pickupMode = 0;
    int pickupSeed = 0;
    int weaponPickupSeed = 0;
    // The level's markers (LevelData::markers): its positions and settings
    // that content reads by name.
    core::assets::LevelMarkers markers;
    // Value `index` of a marker; `fallback` when the level has none (the
    // levels that need one have it, `content` test).
    int marker(const char* name, std::size_t index = 0, int fallback = 0) const {
        const auto it = markers.find(name);
        return it != markers.end() && index < it->second.size() ? it->second[index] : fallback;
    }

    // The level music track playing, -1 for none (current_zik).
    int currentMusic = -1;

    // Blinking phases: toggled every tick, and every few ticks.
    bool blink = false;
    bool slowBlink = false;

    int enemiesCreated = 0;            // counted for the kill percentage
    int dialogueToPlay = -1;           // dialogue scene to start, -1 = none
    bool weaponPickupWanted = false;   // a player could use a weapon pickup
    bool flamethrowerAllowed = false;  // set by each level start (not underwater)
    // The campaign state the players read: the cow bomb is given from the
    // second level on, and with each new life (cowBombOn).
    bool cowBombUnlocked = false;

    // The entity lists. Players and player shots belong to the players,
    // weather particles to fixed pools; the lists own everything else.
    EntityList<Entity> players{Ownership::Borrowing};
    EntityList<Entity> playerShots{Ownership::Borrowing};
    EntityList<Entity> cowBombs{Ownership::Owning};
    EntityList<Entity> impacts{Ownership::Owning};
    EntityList<Entity> vehicles{Ownership::Owning};
    EntityList<Entity> enemies{Ownership::Owning};
    EntityList<Entity> enemyShots{Ownership::Owning};
    EntityList<Entity> pickups{Ownership::Owning};
    EntityList<Entity> props{Ownership::Owning};
    EntityList<Entity> midground{Ownership::Owning};
    EntityList<Entity> foreground{Ownership::Owning};
    EntityList<Entity> movingPlatforms{Ownership::Owning};
    EntityList<Entity> blood{Ownership::Owning};
    EntityList<Entity> gore{Ownership::Owning};
    EntityList<Entity> weatherParticles{Ownership::Borrowing};
    EntityList<Entity> bubbles{Ownership::Owning};

    // Events waiting for their activation X, and active ones.
    EntityList<LevelEvent> sleepingEvents{Ownership::Owning};
    EntityList<LevelEvent> events{Ownership::Owning};
    EntityList<Generator> enemyGenerators{Ownership::Owning};
    EntityList<Generator> pickupGenerators{Ownership::Owning};
    EntityList<Banner> bannerTexts{Ownership::Owning};
    // The banner rows not in use (all false until the first level start).
    bool bannerRowFree[Banner::kRows] = {};

    // The collisions of a tick, in the 2002 order (Game::manageCollisions):
    // player shots and cow bombs against enemies, pickups against players,
    // then, while the players can be hurt, enemies and enemy shots against
    // players.
    void resolveCollisions(bool playersVulnerable);

    // Keeps the positions of the entities and of the camera at the start of
    // a tick (Motion).
    void rememberPositions();

    // Wakes the sleeping events the camera reached (in level order), then
    // runs and destroys the active events whose condition holds
    // (Game::updateEvents).
    void updateEvents();

    // State the 2002 content kept in file statics and static members,
    // shared by every instance of a class. Created on first use, cleared at
    // each level start (D-50 fixed: in 2002 it carried over from level to
    // level, so a level played differently after another). Each world keeps
    // its own types in its source files.
    template <typename T>
    T& shared() {
        std::shared_ptr<void>& slot = shared_[std::type_index(typeid(T))];
        if (!slot) {
            slot = std::make_shared<T>();
        }
        return *static_cast<T*>(slot.get());
    }
    void clearShared() { shared_.clear(); }

private:
    std::unordered_map<std::type_index, std::shared_ptr<void>> shared_;
};

// Slides in a banner with string `text` of the string table, on the first
// free row (World::bannerTexts).
void showBanner(World& world, int text);
// Frees every banner row (each level start).
void clearBanners(World& world);

}  // namespace bb::game
