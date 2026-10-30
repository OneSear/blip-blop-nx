#pragma once

#include <memory>
#include <string>

#include "bb/core/gfx/font.hpp"
#include "bb/core/gfx/pixmap.hpp"
#include "bb/core/gfx/sprite.hpp"
#include "bb/game/audio.hpp"
#include "bb/game/decor.hpp"
#include "bb/game/entity_registry.hpp"
#include "bb/game/hi_scores.hpp"
#include "bb/game/media.hpp"
#include "bb/game/screen.hpp"
#include "bb/game/services.hpp"
#include "bb/game/world.hpp"
#include "device_input.hpp"
#include "strings.hpp"

namespace bb::platform {
class Display;
class Platform;
}  // namespace bb::platform

namespace bb::app {

class GameFiles;
class Session;

// The application (blip_n_blop_3.cpp of 2002 and the legacy glue): opens
// the window, loads the settings, the hi-scores, the strings, the fonts and
// the interface, runs the screens from the intro to the main menu's EXIT,
// and saves the settings and the hi-scores. It owns everything the game
// works with (game::Services).
class App {
public:
    App(platform::Platform& platform, Session& session, GameFiles& files);
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // The whole run; returns the process exit status.
    int run();

private:
    // The session as the screens see it.
    class SessionView : public game::Session {
    public:
        SessionView(app::Session& session, App& app) : session_(session), app_(app) {}
        void note(const char* text) override;
        std::uint64_t nowMicroseconds() const override;
        bool quitRequested() const override;
        bool deterministic() const override;
        void onTick(std::uint64_t fingerprint) override;
        std::FILE* traceStream() const override;
        unsigned long long frame() const override;
        void saveSettings() override;
        void saveHiScores() override;

    private:
        app::Session& session_;
        App& app_;
    };

    bool init();
    void release();
    void connectServices();
    void runScreens(game::ScreenStack& stack);
    void present();
    // Write the settings and the hi-scores (not in deterministic runs: they
    // start from the reference settings and never write).
    void saveSettingsNow();
    void saveHiScoresNow();
    // Loads the string table of the language (fr.dat or uk.dat); English
    // when the French table is missing (the 2002 data has none).
    bool loadStrings(core::input::Language language);
    // An error or a warning, logged and shown in a message box.
    void error(const std::string& text);
    void warning(const std::string& text);

    platform::Platform& platform_;
    Session& session_;
    GameFiles& files_;
    std::unique_ptr<platform::Display> display_;
    // When the next frame may be presented (microseconds of the platform
    // clock).
    std::uint64_t nextFrame_ = 0;
    // The options the display and the strings were last set up for.
    bool integerScaling_ = false;
    core::gfx::Filter filter_ = core::gfx::Filter::Crisp;
    core::input::Language language_ = core::input::Language::English;
    // Presented frames and when the first was (for the log).
    std::uint64_t presented_ = 0;
    std::uint64_t firstPresent_ = 0;
    std::string settingsPath_;
    // In the user's preferences directory ("" without one: the data
    // directory's bb.scr, as in 2002).
    std::string hiScoresPath_;

    core::gfx::Pixmap screen_{640, 480};    // the back buffer
    core::gfx::Pixmap backdrop_{640, 480};  // systemSurface

    game::World world_;
    game::Decor decor_{world_};
    game::EntityRegistry registry_;
    DeviceInput input_;
    StringTable strings_;
    game::AudioOutput audio_;
    game::Options options_;
    game::HiScores hiScores_;
    SessionView sessionView_{session_, *this};

    // The banks and fonts (the pbk_*, fnt_*, sbk_*, mbk_* of 2002).
    core::gfx::SpriteBank interface_, decorScreens_, props_, enemies_, dialogueFaces_;
    core::gfx::SpriteBank blip_, blop_, bleach_, blitz_, heroes_, misc_, heroFaces_;
    core::gfx::BitmapFont cool_, rpg_, menu_, menus_, scoreBlip_, scoreBlop_, scoreBleach_, scoreBlitz_,
        ammo_, ammoUsed_;
    game::SoundBank levelSounds_{audio_}, miscSounds_{audio_}, heroSounds_{audio_};
    game::MusicBank levelMusic_{audio_}, jingles_{audio_}, attractMusic_{audio_};

    game::Fonts fonts_;
    game::Canvas canvas_;
    game::Services services_;
};

}  // namespace bb::app
