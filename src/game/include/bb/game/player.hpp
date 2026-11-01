#pragma once

#include <vector>

#include "bb/game/actor.hpp"
#include "bb/game/entity_registry.hpp"

namespace bb::game {

// Blip and Blop (ID_BLIP, ID_BLOP): the same player with its own images
// and starting weapon.
// Bleach and Blitz, added with the four-player mode, can be chosen as well.
enum class Hero { Blip = 0, Blop = 1, Bleach = 2, Blitz = 3 };

// Players in a game: 1 or 2 as in 2002, 3 or 4 in the four-player mode.
constexpr int kMaxPlayers = 4;

// Who plays a campaign (the hero choice): players 1 to `count` in order,
// each with its hero and the controls it plays with (the aliases of input
// player 0 to 3: AliasControls::firstOf()). Any hero may be any player.
struct Lineup {
    int count = 0;
    Hero heroes[kMaxPlayers] = {};
    int controls[kMaxPlayers] = {};

    void add(Hero hero, int controlsOf) noexcept {
        if (count < kMaxPlayers) {
            heroes[count] = hero;
            controls[count] = controlsOf;
            count += 1;
        }
    }
};

// Bleach is a pink Blip and Blitz a green Blop: the same weapons, poses and
// dialogue faces as the hero they are made from.
constexpr bool blipKind(Hero hero) noexcept {
    return hero == Hero::Blip || hero == Hero::Bleach;
}

// The weapons, numbered as in 2002 (ID_M16 ...): the level summary and
// the HUD show them.
enum class Weapon { Rifle = 0, Shotgun = 1, Smg = 2, Flamethrower = 3, Laser = 4 };

// How a weapon fires: its muzzle flash runs `flashSteps` steps of
// `flashDelay` ticks (the shot lasts as long), a bullet leaves every
// `fireDelay` ticks, and the gun turns one aim step every `turnDelay`
// ticks (latence_arme, nb_etape_arme, cadence_arme, poid_arme). The 2002
// code repeated these numbers in five places.
struct WeaponSpec {
    int flashDelay;
    int flashSteps;
    int fireDelay;
    int turnDelay;
};

const WeaponSpec& weaponSpec(Weapon weapon);

// The 18 aim directions of the players (BBDIR_*), clockwise from straight
// down on the right side: 0..8 face right, 9..17 face left.
namespace aim {
constexpr int kDownRight = 0;          // BBDIR_BAS_B_D
constexpr int kDiagonalDownRight = 2;  // BBDIR_BAS_DROITE
constexpr int kRight = 4;              // BBDIR_DROITE
constexpr int kDiagonalUpRight = 6;    // BBDIR_HAUT_DROITE
constexpr int kUpRight = 8;            // BBDIR_HAUT_H_D (last facing right)
constexpr int kUpLeft = 9;             // BBDIR_HAUT_H_G
constexpr int kDiagonalUpLeft = 11;    // BBDIR_HAUT_GAUCHE
constexpr int kLeft = 13;              // BBDIR_GAUCHE
constexpr int kDiagonalDownLeft = 15;  // BBDIR_BAS_GAUCHE
constexpr int kDownLeft = 17;          // BBDIR_BAS_B_G
}  // namespace aim

// A player's buttons, read once per tick (the 2002 Controlor; its inertia
// switch was always on, so the players always keep their jump speed).
class PlayerControls {
public:
    virtual ~PlayerControls() = default;
    virtual bool left() const = 0;
    virtual bool up() const = 0;
    virtual bool right() const = 0;
    virtual bool down() const = 0;
    virtual bool fire() const = 0;
    virtual bool jump() const = 0;
    virtual bool special() const = 0;  // the cow bomb
};

class Player;

// A shot of a player (TirBB): it scores for its player. The players keep
// their shots in pools and reuse them without resetting them, as in 2002.
class PlayerShot : public Actor, public Shot {
public:
    explicit PlayerShot(World& world) noexcept : Actor(world) {}

    Shot* asShot() override { return this; }
    int bloodDirection() const override { return direction / 2; }
    int heading() const override { return direction; }
    void onHit(int points) override;
    void onKill() override;

    // Aims along a player aim (0..17), as the shot's 16 headings (setDir()).
    virtual void aim(int playerAim);

    Player* owner = nullptr;

protected:
    const Image* playerImage(int index) const { return world().media.players->image(index); }
};

class RifleBullet : public PlayerShot {
public:
    using PlayerShot::PlayerShot;
    void update() override;
    int damage() const override { return 40; }
    int burning() const override { return 0; }
};

class SmgBullet : public PlayerShot {
public:
    using PlayerShot::PlayerShot;
    void update() override;
    int damage() const override { return 75; }
    int burning() const override { return 0; }
};

// One of the 17 pellets of a shotgun blast: it spreads by the shared
// patterns of all blasts, and hurts less after its first ticks.
class ShotgunPellet : public PlayerShot {
public:
    using PlayerShot::PlayerShot;
    void update() override;
    void aim(int playerAim) override;
    int damage() const override { return age_ < range_ ? 40 : 30; }
    int burning() const override { return 0; }

private:
    int dx_ = 0;
    int spreadX_ = 0;
    int spreadY_ = 0;
    int stepX_ = 0;  // never reset: they carry over when the pellet is reused
    int stepY_ = 0;
    int delayX_ = 0;
    int delayY_ = 0;
    int range_ = 0;  // duree_vie
    int age_ = 0;
};

// A beam that reaches the first wall or the screen edge in one of 8
// directions and lasts 2 ticks; it hits along the beam.
class LaserBeam : public PlayerShot {
public:
    using PlayerShot::PlayerShot;
    void update() override;
    void draw() override;
    bool hits(const Entity& other) const override;
    void aim(int playerAim) override;
    int damage() const override { return 20; }
    int burning() const override { return 0; }
    int bloodDirection() const override { return direction; }
    void onHit(int points) override;  // the beam goes on

private:
    int width_ = 0;
    int height_ = 0;
    int firstImage_ = 0;
};

// A flame of the flamethrower: it grows, shrinks and rises.
class Flame : public PlayerShot {
public:
    using PlayerShot::PlayerShot;
    void update() override;
    void aim(int playerAim) override;
    int damage() const override { return power; }
    int burning() const override { return 2; }
    void onHit(int points) override;  // the flame goes on

    int power = 0;  // deg

private:
    bool large_ = false;  // type 1 (type 0 is small; the 2002 type 2 was never drawn)
    bool shrinking_ = false;
    int dx_ = 0;
    int speed_ = 0;
    int rise_ = 0;
};

// A cow of the cow bomb: falls from the sky, crushes what it meets and
// bursts on the ground.
class FallingCow : public PlayerShot {
public:
    using PlayerShot::PlayerShot;
    void update() override;
    int damage() const override { return 50; }
    int burning() const override { return 2; }
    void onHit(int points) override;  // the cow goes on
};

// Blip or Blop (the 2002 Couille, Blip, Blop and Joueur).
class Player : public Character, public Hurtable, public Rider {
public:
    // The player states beyond CharacterState (ETAT_*).
    enum Mode {
        kComingBack = 9,   // ETAT_COME_BACK: drops in from the top
        kSomersault = 10,  // ETAT_SALETO
        kRiding = 14,      // ETAT_LOCKEDV
    };

    // index: 0 for the first player, 1 for the second (its scroll flag).
    Player(World& world, Hero hero, const PlayerControls& controls, int index);

    void update() override;
    void draw() override;

    Hurtable* asHurtable() override { return this; }
    Rider* asRider() override { return this; }

    // Hurtable (estTouche()).
    void hurt(int damage) override;

    // Rider.
    bool canBoard() const override {
        return mode != kDying && mode != kComingBack && !dead && mode != kRiding;
    }
    bool riding() const override { return mode == kRiding; }
    void board(bool canFire, bool canTurn) override;
    void leave() override;
    void faceRight() override {
        direction = aim::kRight;
        weaponAim = aim::kRight;
    }

    // The weapon and its tuning, keeping ammo and the firing state.
    void setWeapon(Weapon w);
    // Takes up a weapon with `rounds` of ammo, its flash reset.
    void equip(Weapon w, int rounds);
    // The weapon after a perfect level: the laser, or Blop's flamethrower
    // where it is allowed (setSuperWeapon()).
    void equipSuperWeapon();
    // Whether a weapon pickup would help (okBonus()).
    bool wantsWeaponPickup() const;
    // Stops the looping weapon sounds (endLevel()).
    void endLevel();

    void addPoints(int points) { score += points; }
    void addKill() { kills += 1; }

    const Hero hero;
    const int slot;  // 0 for the first player, 1 for the second

    int lives = 5;     // nb_life
    int cowBombs = 0;  // nb_cow_bomb
    int score = 0;
    int kills = 0;        // nb_killed
    int bonusLives = 0;   // lives won by score so far (mod_life)
    bool perfect = true;  // not hurt in this level
    int invincible = 0;   // ticks left
    int landingY = 0;     // where a player coming back stops (y_to_go)

    Weapon weapon = Weapon::Rifle;
    int ammo = 0;
    int weaponAim = 0;    // dir_arme: follows direction
    bool firing = false;  // tire: a burst is running

private:
    void drawStanding();
    void drawJumping();
    void drawRiding();
    void drawWeapon(int px, int py);
    void drawEye(int px, int py);
    const Image* own(int index) const { return own_->image(index); }
    const Image* shared(int index) const { return world().media.players->image(index); }

    void updateWeaponSounds();
    void dropCowBomb();
    void crushedByScroll();
    void slide();
    void turn();
    void updateWeapon();
    void fireShots(int gunY);
    void standing();
    void walking();
    void jumping();
    void somersault();
    void dying();
    void comingBack();
    void startJump();
    void startFall();
    void land(int ground);
    // Back with a new life, falling from the top of the screen.
    void respawn();
    void hitboxFromBody();
    // The starting weapon of a new life (rearme()).
    void rearm();

    const PlayerControls& controls_;
    const ImageBank* own_;

    int bounce_ = 0;              // sauti: the permanent hop
    int flashStep_ = 0;           // etape_arme
    int flashSubStep_ = 0;        // ss_etape_arme
    int fireStep_ = 0;            // pruno
    int turnStep_ = 0;            // etape_bouge_arme
    int recoilStep_ = 0;          // etape_recul
    int jumpDx_ = 0;              // dx_saut
    int slideDx_ = 0;             // dx_glisse
    int slideMomentum_ = 0;       // latence_glisse
    bool cowInvincible_ = false;  // inv_cow: no blinking
    int hurtEye_ = 0;             // a_mal
    int blinkStep_ = 0;           // etape_cli
    int cowBombWait_ = 0;
    bool jumpReleased_ = false;
    int downTime_ = 0;        // ticks since down was last pressed; -1: pressed twice
    bool aimLocked_ = false;  // by the vehicle
    bool flameSound_ = false;
    bool laserSound_ = false;

    std::vector<RifleBullet> rifleBullets_;
    std::vector<SmgBullet> smgBullets_;
    std::vector<ShotgunPellet> pellets_;
    std::vector<LaserBeam> beams_;
    std::vector<Flame> flames_;
    int nextRifle_ = 0;
    int nextSmg_ = 0;
    int nextPellet_ = 0;
    int nextBeam_ = 0;
    int nextFlame_ = 0;
};

// Registers the 8 pickups (legacy pickup ids 0 to 7 of the level files).
void registerPickups(EntityRegistry& registry);

}  // namespace bb::game
