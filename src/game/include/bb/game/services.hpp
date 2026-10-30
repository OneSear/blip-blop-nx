#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

#include "bb/core/gfx/font.hpp"
#include "bb/core/gfx/sprite.hpp"
#include "bb/core/gfx/viewport.hpp"
#include "bb/core/input/names.hpp"
#include "bb/game/audio.hpp"
#include "bb/game/input.hpp"

namespace bb::core::assets {
class AssetSource;
}

namespace bb::game {

struct World;
class HiScores;
class Decor;

// The input the screens read: the keys and pads of the current update,
// the aliases (remappable from the options), and the focus.
class Input : public InputState {
public:
    // Reads the platform's events (or, in deterministic runs, the input of
    // the recorded frame).
    virtual void update() = 0;
    // Whether a key, pad button or pad direction is held.
    virtual bool anyHeld() const = 0;
    // The first input held, in the order of the 2014 waitKey(): keys,
    // special keys, pad buttons, pad directions; false if nothing is held.
    virtual bool firstHeld(unsigned int& code) const = 0;
    // True once after the window lost the focus.
    virtual bool takeFocusLost() = 0;
    virtual void setAlias(int alias, unsigned int code) = 0;

    // Whether a pad's back button is held: the Switch's B, the bottom
    // button (the menus go back one page; it jumps in a level).
    virtual bool backHeld() const = 0;

    // Lets `count` players take their controllers before a game (the
    // Switch's controller applet); nothing in deterministic runs.
    virtual void requestPlayers(int count) = 0;

    // The system's on-screen keyboard (the Switch's) for a short text: false
    // where there is none, and in deterministic runs, where keys are typed.
    // After each update(), textEntry() says whether the text came (Done,
    // with it), the player cancelled, or the keyboard is still shown.
    enum class TextEntry { Typing, Done, Cancelled };
    virtual bool beginTextEntry() = 0;
    virtual TextEntry textEntry(std::string& text) = 0;

    // update() then anyHeld() (the 2002 anyKeyPressed()).
    bool anyPressed() {
        update();
        return anyHeld();
    }
};

// What the screens need of the running application.
class Session {
public:
    virtual ~Session() = default;
    // A milestone in the session's log (the replays list them).
    virtual void note(const char* text) = 0;
    // The game's clock: virtual time in deterministic runs.
    virtual std::uint64_t nowMicroseconds() const = 0;
    // The player closed the window (app_killed).
    virtual bool quitRequested() const = 0;
    // A recorded, replayed or --deterministic run (virtual time).
    virtual bool deterministic() const = 0;
    // In deterministic runs: the world-state fingerprint of a tick, and
    // where to print the world state (null when not tracing).
    virtual void onTick(std::uint64_t fingerprint) = 0;
    virtual std::FILE* traceStream() const = 0;
    virtual unsigned long long frame() const = 0;
    // Writes the settings, or the hi-scores, now: a console may sleep or
    // lose power before the game exits. Nothing in deterministic runs.
    virtual void saveSettings() = 0;
    virtual void saveHiScores() = 0;
};

// The options the menus change (the 2002 language; the display options are
// Phase 9 additions, and the 2002 vsync went with the desktop versions:
// the game always syncs).
struct Options {
    bool cheats = false;  // the /cheat switch: the F keys of CheatKeys
    core::input::Language language = core::input::Language::English;
    bool integerScaling = false;  // whole multiples of 640x480 (2 or more), with borders
    core::gfx::Filter filter = core::gfx::Filter::Crisp;
    bool smoothMotion = true;  // draws between the last two ticks (World::Motion)
};

// The banks and fonts of the game, owned by the application: some live as
// long as it, some as long as a campaign or a level (the 2002 pbk_*, fnt_*,
// sbk_*, mbk_* globals).
struct Banks {
    core::gfx::SpriteBank* interface = nullptr;      // inter.gfx: menus, titles (freed during a campaign)
    core::gfx::SpriteBank* decor = nullptr;          // the level's decor screens
    core::gfx::SpriteBank* props = nullptr;          // the level's props (pbk_niveau)
    core::gfx::SpriteBank* enemies = nullptr;        // the level's enemies
    core::gfx::SpriteBank* dialogueFaces = nullptr;  // the level's dialogue faces (pbk_rpg)
    core::gfx::SpriteBank* blip = nullptr;           // the campaign's: the heroes,
    core::gfx::SpriteBank* blop = nullptr;
    core::gfx::SpriteBank* bleach = nullptr;  // (the four-player mode)
    core::gfx::SpriteBank* blitz = nullptr;
    core::gfx::SpriteBank* heroes = nullptr;     // what they share (bb.gfx),
    core::gfx::SpriteBank* misc = nullptr;       // pickups and effects,
    core::gfx::SpriteBank* heroFaces = nullptr;  // their dialogue faces (rpg_bb.gfx)
    core::gfx::BitmapFont* scoreBlip = nullptr;  // and the HUD fonts
    core::gfx::BitmapFont* scoreBlop = nullptr;
    core::gfx::BitmapFont* scoreBleach = nullptr;
    core::gfx::BitmapFont* scoreBlitz = nullptr;
    core::gfx::BitmapFont* ammo = nullptr;
    core::gfx::BitmapFont* ammoUsed = nullptr;
    SoundBank* levelSounds = nullptr;
    SoundBank* miscSounds = nullptr;
    SoundBank* heroSounds = nullptr;
    MusicBank* levelMusic = nullptr;
    MusicBank* jingles = nullptr;       // inter.mbk, not looping
    MusicBank* attractMusic = nullptr;  // interl.mbk
    // The level's music bank, to load it again when the music is switched
    // on (current_mbk); empty: none.
    std::string levelMusicFile;
};

// Everything a screen works with.
struct Services {
    core::gfx::Pixmap* screen = nullptr;  // the back buffer
    // A 640x480 copy of the screen the menus and the level summary keep
    // their darkened background in (the 2002 systemSurface); shared, its
    // content carries over from one user to the next.
    core::gfx::Pixmap* backdrop = nullptr;
    Options* options = nullptr;
    Input* input = nullptr;
    Session* session = nullptr;
    const core::assets::AssetSource* assets = nullptr;
    AudioOutput* audio = nullptr;
    const TextSource* texts = nullptr;
    const Fonts* fonts = nullptr;
    Banks banks;
    Decor* decor = nullptr;  // the level's scrolling background
    World* world = nullptr;
    HiScores* hiScores = nullptr;
};

// Loads the banks and fonts of the asset source into the game's own
// objects; false (with a line in the log) when it is missing. colorKey: each
// image gets the transparent colour of its layout.
bool loadSprites(const core::assets::AssetSource& assets, const std::string& name,
                 core::gfx::SpriteBank& bank, bool colorKey = true);
bool loadFont(const core::assets::AssetSource& assets, const std::string& name, core::gfx::BitmapFont& font);
bool loadSounds(const core::assets::AssetSource& assets, const std::string& name, SoundBank& bank);
// A music bank (playlist and files); false also for a tracker module.
bool loadMusic(const core::assets::AssetSource& assets, const std::string& name, MusicBank& bank,
               bool loop = true);

}  // namespace bb::game
