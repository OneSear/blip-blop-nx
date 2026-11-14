#pragma once

// The video-game world's first level (video), ported from the 2002 classes
// (EnnemiToad, EnnemiYoshi, EnnemiYoshiDCA, EnnemiTails,
// EnnemiTailsvolant, EnnemiKnukles, EnnemiPrincesse, EnnemiPrincesseguard,
// EnnemiRayman, the four Ennemi*Heros, TirCarapace, TirRaymanpoing,
// MorceauRayman, MorceauTeteRayman and the Fond* props of video). As for
// the other worlds, arithmetic, random draws and states are the 2002 ones
// so that the replays keep their fingerprints.

#include "bb/assets/sounds/misc.hpp"
#include "bb/assets/sounds/video.hpp"
#include "bb/assets/sprites/videoenn.hpp"
#include "bb/assets/sprites/videofonda.hpp"
#include "bb/game/actor.hpp"

namespace bb::game::video {

// videoenn.gfx (enemies), videofonda.gfx (props), video.sfx.
namespace img = bb::assets::sprites::videoenn;
namespace prop = bb::assets::sprites::videofonda;
namespace snd = bb::assets::sounds::video;
namespace miscsnd = bb::assets::sounds::misc;

// Toad (500): an accelerating walker; charges a player on its platform
// layer, otherwise jumps. Its death convulses ("hokuto") a random number
// of times, and the "my brain hurts" gag adds more (2002 file statics,
// see toad.cpp).
class Toad : public Enemy {
public:
    explicit Toad(World& world);
    void update() override;
    void hitBy(Shot& shot) override;

private:
    void walkAbout();  // onAvance()
    void die();        // onMeure()
    void jump();       // onSaute()
    void charge();     // onCharge()
    // Back to walking with a new charge delay (the end of a charge).
    void endCharge(int newDirection);

    int speed_ = 0;
    int speedStep_ = 0;
    int chargeDelay_;
    int chargeWait_ = 0;
    int chargeStep_ = 0;
    int convulsions_;  // hokuto
};

// Yoshi (501): walks, sticks its tongue out at a player ahead on its
// platform layer, throws shells now and then.
class Yoshi : public Enemy {
public:
    explicit Yoshi(World& world);
    void update() override;
    void hitBy(Shot& shot) override;

private:
    void walkAbout();   // onAvance()
    void fallDown();    // onTombe()
    void die();         // onMeure()
    void tongue();      // onAttaque()
    void throwShell();  // onTire()

    bool throwing_ = false;  // attack: the shell (true) or the tongue
    int attackDelay_;
    int attackWait_ = 0;
};

// The Yoshi turret (505): stands and throws shells at the target player.
class YoshiTurret : public Enemy {
public:
    explicit YoshiTurret(World& world);
    void update() override;
    void hitBy(Shot& shot) override;

private:
    void stand();       // onAvance()
    void die();         // onMeure()
    void throwShell();  // onTire()
    // Leaves once the screen has passed it (D-98 fixed).
    void dieWhenPassed();

    int attackDelay_;
    int attackWait_ = 0;
};

// Tails (504) and the flying Tails (508, which enters flying from the top
// of the screen): walks, then flies up above the target player and swoops.
class Tails : public Enemy {
public:
    Tails(World& world, bool entersFlying);
    void update() override;
    void hitBy(Shot& shot) override;

private:
    void walkAbout();  // onAvance()
    void die();        // onMeure()
    void fly();        // onVolle()
    void fallDown();   // onTombe()
    // Lands from a flight onto the walk (wall or screen edge reached).
    void land(int newDirection);
    void aimAt(int targetX);

    bool entersFlying_;
    bool placed_ = false;  // placer (flying Tails)
    int speed_ = 1;
    int speedStep_ = 0;
    int flyDelay_;
    int flyWait_ = 0;
    int goalX_ = 0;
    int goalY_ = 0;
    bool swooping_ = false;  // attack: swoops (true) or rises above the target
    bool flying_ = false;
    int heading_ = 0;  // direction: -1 or 1
};

// Knuckles (506): walks, charges a player ahead on its platform layer,
// jumps at a player above; falls faster than the others.
class Knuckles : public Enemy {
public:
    explicit Knuckles(World& world);
    void update() override;
    void hitBy(Shot& shot) override;

private:
    void walkAbout();  // onAvance()
    void die();        // onMeure()
    void jump();       // onSaute()
    void charge();     // onCharge()
    void fallDown();   // onTombe()
    void heavyFall();  // its own tombe(): falls up to 7 pixels per tick
    void backToWalk();

    int speed_ = 1;
    int speedStep_ = 0;
    int chargeDelay_;
    int chargeWait_ = 0;
};

// The Princess (503): walks right to x 5760 and waits; once register 1 is
// set, three guards spawn and she walks off the screen (banner 517).
// Invulnerable but bleeding until then; killed, banner 519, a cow bomb
// pickup, and register 2 = 1 while guards live.
class Princess : public Enemy {
public:
    explicit Princess(World& world);
    void update() override;
    void hitBy(Shot& shot) override;

private:
    void walkAbout();  // onAvance()
    void die();        // onMeure()

    bool guarded_ = false;  // aide: the guards were called
};

// A guard of the Princess: keeps `distance` pixels left of her; dies when
// she is gone or register 2 is set; register 3 counts down on death.
class PrincessGuard : public Enemy {
public:
    PrincessGuard(World& world, Princess* princess, int distance);
    void update() override;
    void hitBy(Shot& shot) override;

private:
    void walkAbout();  // onAvance()
    void die();        // onMeure()

    Handle<Princess> princess_;
    int distance_;
};

// Rayman (507), end of video: walks in, summons the Pokémon heroes by
// register 1, punches with an extending fist; loses his fists then his
// feet at 10000, 5000 and 1000 HP (the landing limbs drive the script
// through registers 0 and 2).
class Rayman : public Enemy {
public:
    explicit Rayman(World& world);
    void update() override;
    void hitBy(Shot& shot) override;

private:
    void walkAbout();  // onAvance()
    void summon();     // the first branch of onAvance()
    // The second branch of onAvance(); true when a punch started.
    bool fight();
    void die();       // onMeure()
    void punch();     // onTire()
    void fallDown();  // onTombe()
    void startPunch();
    // Throws limb `image` off (fists and feet), speed away from the
    // nearer screen edge.
    void throwLimb(int image);
    // The fist is back: walk again with a new attack delay.
    void endPunch(int delayBase, int delayRange);

    int attackDelay_;
    int attackWait_ = 0;
    int attackStep_ = 0;
    int goalX_ = 0;
    int reach_ = 0;  // range_poing
    bool fist1_ = true;
    bool fist2_ = true;
    bool feet_ = true;
    int heroes_ = 4;  // dorkemon: hero summons left
    Handle<class RaymanFist> fist_;
};

// The four Pokémon heroes Rayman summons: thrown out in a "dorkeball",
// open, then walk; register 1 counts down when they die.
struct HeroSpec {
    int hitPoints;
    int treasure;
    int startDy, startDx;
    int outImage;  // opening ball, 3 images from outImage + 1
    int walkSpeed;
    int walkRight, walkLeft;  // 2 images each; walkLeft < 0: walkRight both ways
    int dieSteps;             // one image every 4 ticks
    int dieRight, dieLeft;    // dieLeft < 0: dieRight both ways
    bool engraves;            // the corpse is engraved unless on layer 4
    bool landingSound;
    // Hit only once out of the ball, and bleeds; otherwise (the COM) hit
    // at any time, without blood.
    bool bleeds;
};

class PokemonHero : public Enemy {
public:
    PokemonHero(World& world, const HeroSpec& spec);
    void update() override;
    void hitBy(Shot& shot) override;

    static const HeroSpec kPikachu;
    static const HeroSpec kBulbasaur;
    static const HeroSpec kCharmander;
    static const HeroSpec kCom;

private:
    void walkAbout();  // onAvance()
    void die();        // onMeure()

    const HeroSpec& spec_;
    int xSpeed_;
    int ball_ = 2;  // dorkeball: 2 thrown, 1 opening, 0 out
};

//-----------------------------------------------------------------------------
// Shots and debris
//-----------------------------------------------------------------------------

// Yoshi's shell: rolls on the ground, bounces once off a wall.
class Shell : public Projectile {
public:
    explicit Shell(World& world) : Projectile(world) { image = enemyImage(img::kShell); }
    void update() override;

private:
    bool bounced_ = false;
};

// Rayman's fist: flies out, slows down past its goal and comes back. It
// never dies by itself; Rayman removes it when it is back.
class RaymanFist : public Projectile {
public:
    RaymanFist(World& world, int speed, int goalX) : Projectile(world), xSpeed_(speed), goalX_(goalX) {
        animation.subFrame = 1;
    }
    void update() override;

private:
    int xSpeed_;
    int goalX_;
};

// A fist or a foot of Rayman's (in the blood-spray list): engraved on
// landing, where it sets register 0 = 2 and register 2 = which limb.
// One that falls into a hole never lands: it then leaves the level below
// the screen (D-55 fixed), and Rayman stays at stage 1, as in 2002.
class RaymanLimb : public Actor {
public:
    RaymanLimb(World& world, int limb, int speedX, int speedY);
    void update() override;

private:
    int limb_;  // the image
    int xSpeed_;
};

// Rayman's head, rolling (in the blood-spray list): engraved on landing.
class RaymanHead : public Actor {
public:
    RaymanHead(World& world, int speedX, int speedY) : Actor(world), xSpeed_(speedX) {
        image = enemyImage(img::kRaymanHead);
        dy = speedY;
    }
    void update() override;

private:
    int xSpeed_;
};

//-----------------------------------------------------------------------------
// Props
//-----------------------------------------------------------------------------

// The question block (500), the coin (501) and the ring (502): all of a
// kind animate together (2002 class statics, on the blink phase); they go
// when 100 pixels left of the screen.
class SyncedProp : public Actor {
public:
    struct Clock {
        int frame = 0;
        int subFrame = 0;
        bool phase = false;
    };
    SyncedProp(World& world, Clock& clock, int first, int steps, int delay)
        : Actor(world), clock_(clock), first_(first), steps_(steps), delay_(delay) {}
    void update() override;

private:
    Clock& clock_;
    int first_;
    int steps_;
    int delay_;
};

// The sunflowers (503 to 506, 2 images every 10 ticks) and the flag (507,
// 10 images every 5 ticks).
class LoopProp : public Actor {
public:
    LoopProp(World& world, int first, int steps, int delay)
        : Actor(world), first_(first), steps_(steps), delay_(delay) {}
    void update() override;

private:
    int first_;
    int steps_;
    int delay_;
};

}  // namespace bb::game::video
