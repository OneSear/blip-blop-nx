// The application (blip_n_blop_3.cpp of 2002: InitApp, PlayGame, ReleaseAll;
// and the glue of the legacy port: legacy_media.cpp, legacy_assets.cpp,
// legacy_registry.cpp, video.cpp, screens/screen_runner.cpp).

#include "app.hpp"

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "bb/core/input/codes.hpp"
#include "bb/core/log.hpp"
#include "bb/core/vfs.hpp"
#include "bb/game/campaign.hpp"
#include "bb/game/content.hpp"
#include "bb/game/game.hpp"
#include "bb/game/player.hpp"
#include "bb/platform/audio.hpp"
#include "bb/platform/clock.hpp"
#include "bb/platform/display.hpp"
#include "bb/platform/file_system.hpp"
#include "bb/platform/platform.hpp"
#include "bb/platform/system.hpp"
#include "files.hpp"
#include "session.hpp"
#include "settings_file.hpp"

namespace bb::app {

namespace {

constexpr const char* kLegacySettingsFile = "data/bb.cfg";
// The hi-scores of 2002, in the data directory: read when the preferences
// directory has none yet, and written there when there is no preferences
// directory.
constexpr const char* kLegacyHiScoresFile = "data/bb.scr";

game::EntityRegistry makeRegistry() {
    game::EntityRegistry r;
    game::registerAllEntities(r);
    return r;
}

void loadHiScores(const std::string& path, const platform::FileSystem& fileSystem, const core::Vfs& dataFiles,
                  game::HiScores& table) {
    table.reset();
    if (!path.empty()) {
        if (const auto bytes = fileSystem.readAll(path)) {
            if (table.fromFile(*bytes)) {
                core::Log::info("Using " + path + " as hi-scores file.");
                return;
            }
            core::Log::info(path + ": not a hi-scores file - default hi-scores used");
            table.reset();
            return;
        }
    }
    if (const auto bytes = dataFiles.readAll(kLegacyHiScoresFile)) {
        if (table.fromFile(*bytes)) {
            core::Log::info(std::string("Imported ") + kLegacyHiScoresFile + " (2002 hi-scores).");
            return;
        }
        table.reset();
    }
    core::Log::info("Cannot load hi-scores file. Use default hi-scores");
}

bool saveHiScores(const std::string& path, platform::FileSystem& fileSystem, const GameFiles& files,
                  const game::HiScores& table) {
    const std::vector<std::uint8_t> bytes = table.toFile();
    const std::string target = path.empty() ? files.writablePath(kLegacyHiScoresFile) : path;
    return fileSystem.writeAll(target, bytes.data(), bytes.size());
}

}  // namespace

//-----------------------------------------------------------------------------
// SessionView
//-----------------------------------------------------------------------------

void App::SessionView::note(const char* text) {
    session_.note(text);
}
std::uint64_t App::SessionView::nowMicroseconds() const {
    return session_.nowMicroseconds();
}
bool App::SessionView::quitRequested() const {
    return session_.quitRequested();
}
bool App::SessionView::deterministic() const {
    return session_.deterministic();
}
void App::SessionView::onTick(std::uint64_t fingerprint) {
    session_.onTick(fingerprint);
}
std::FILE* App::SessionView::traceStream() const {
    return session_.traceStream();
}
unsigned long long App::SessionView::frame() const {
    return session_.frame();
}
void App::SessionView::saveSettings() {
    app_.saveSettingsNow();
}
void App::SessionView::saveHiScores() {
    app_.saveHiScoresNow();
}

//-----------------------------------------------------------------------------
// App
//-----------------------------------------------------------------------------

App::App(platform::Platform& platform, Session& session, GameFiles& files)
    : platform_(platform),
      session_(session),
      files_(files),
      registry_(makeRegistry()),
      input_(platform.input(), session) {
    audio_.device = &platform.audio();
    // The command line's switches (also replayed from a recording).
    options_.cheats = session_.cheats();
    if (session_.quiet()) {
        audio_.musicOn = false;
    }

    if (!session_.settingsPath().empty()) {
        settingsPath_ = session_.settingsPath();
    } else if (!platform.files().preferencesDirectory().empty()) {
        settingsPath_ = platform.files().preferencesDirectory() + "settings.cfg";
    }
    if (!platform.files().preferencesDirectory().empty()) {
        hiScoresPath_ = platform.files().preferencesDirectory() + "hiscores.scr";
    }

    connectServices();
}

App::~App() {
    // The entities go before the banks they may use (every level releases
    // its own, so the lists are normally empty here).
    for (game::EntityList<game::Entity>* list :
         {&world_.players, &world_.playerShots, &world_.cowBombs, &world_.impacts, &world_.vehicles,
          &world_.enemies, &world_.enemyShots, &world_.pickups, &world_.props, &world_.midground,
          &world_.foreground, &world_.movingPlatforms, &world_.blood, &world_.gore, &world_.weatherParticles,
          &world_.bubbles}) {
        list->clear();
    }
    world_.sleepingEvents.clear();
    world_.events.clear();
    world_.enemyGenerators.clear();
    world_.pickupGenerators.clear();
    world_.bannerTexts.clear();

    // The platform outlives the application, but nothing may use its audio
    // after release(): the banks only drop their handles from here on.
    audio_.device = nullptr;
}

void App::error(const std::string& text) {
    core::Log::error(text);
    platform_.system().showError("Blip'n Blop : Error reporting", text);
}

void App::warning(const std::string& text) {
    core::Log::warning(text);
    platform_.system().showWarning("Blip'n Blop : Warning", text);
}

void App::connectServices() {
    game::Media& m = world_.media;
    m.enemies = &enemies_;
    m.misc = &misc_;
    m.level = &props_;
    m.players = &heroes_;
    m.blip = &blip_;
    m.blop = &blop_;
    m.bleach = &bleach_;
    m.blitz = &blitz_;
    m.dialogueFaces = &dialogueFaces_;
    m.heroFaces = &heroFaces_;
    m.levelSounds = &levelSounds_;
    m.miscSounds = &miscSounds_;
    m.commonSounds = &heroSounds_;
    m.levelMusic = &levelMusic_;

    fonts_.cool = &cool_;
    fonts_.rpg = &rpg_;
    fonts_.menu = &menu_;
    fonts_.menus = &menus_;
    fonts_.scoreBlip = &scoreBlip_;
    fonts_.scoreBlop = &scoreBlop_;
    fonts_.scoreBleach = &scoreBleach_;
    fonts_.scoreBlitz = &scoreBlitz_;
    fonts_.ammo = &ammo_;
    fonts_.ammoUsed = &ammoUsed_;

    canvas_ = game::Canvas(&screen_, &fonts_, &strings_);
    m.canvas = &canvas_;
    m.fonts = &fonts_;
    m.texts = &strings_;
    m.effects = &decor_;

    world_.rng = &session_.rng();
    world_.registry = &registry_;

    game::Services& s = services_;
    s.screen = &screen_;
    s.backdrop = &backdrop_;
    s.options = &options_;
    s.input = &input_;
    s.session = &sessionView_;
    s.assets = &files_.assets();
    s.audio = &audio_;
    s.texts = &strings_;
    s.fonts = &fonts_;
    s.world = &world_;
    s.decor = &decor_;
    s.hiScores = &hiScores_;

    game::Banks& b = s.banks;
    b.interface = &interface_;
    b.decor = &decorScreens_;
    b.props = &props_;
    b.enemies = &enemies_;
    b.dialogueFaces = &dialogueFaces_;
    b.blip = &blip_;
    b.blop = &blop_;
    b.bleach = &bleach_;
    b.blitz = &blitz_;
    b.heroes = &heroes_;
    b.misc = &misc_;
    b.heroFaces = &heroFaces_;
    b.scoreBlip = &scoreBlip_;
    b.scoreBlop = &scoreBlop_;
    b.scoreBleach = &scoreBleach_;
    b.scoreBlitz = &scoreBlitz_;
    b.ammo = &ammo_;
    b.ammoUsed = &ammoUsed_;
    b.levelSounds = &levelSounds_;
    b.miscSounds = &miscSounds_;
    b.heroSounds = &heroSounds_;
    b.levelMusic = &levelMusic_;
    b.jingles = &jingles_;
    b.attractMusic = &attractMusic_;
}

bool App::init() {
    const core::assets::AssetSource& source = files_.assets();

    core::Log::info("Blip & Blop - (c) LOADED Studio");

    // The platform opened the audio device.
    if (!audio_.device->available()) {
        warning("Cannot open the audio device. Sound will be turned off.");
        audio_.soundOn = false;
        audio_.musicOn = false;
    }

    // Deterministic runs (replays, regression tests) start from the
    // reference settings and the built-in hi-scores, so that saved ones
    // cannot change their outcome; they never write them back either.
    if (session_.deterministic()) {
        applySettings(core::Settings::reference(), options_, input_);
    } else if (!loadSettings(settingsPath_, platform_.files(), files_.dataFiles(), kLegacySettingsFile, options_,
                             input_)) {
        // The first run speaks the system's language.
        if (platform_.system().languageCode() == "fr") {
            options_.language = core::input::Language::French;
            core::Log::info("First run: French, the system's language.");
        }
    }
    if (session_.smoothMotion()) {
        options_.smoothMotion = true;
    }

    hiScores_.reset();
    if (session_.deterministic()) {
        core::Log::info("Deterministic run: using default hi-scores");
    } else {
        loadHiScores(hiScoresPath_, platform_.files(), files_.dataFiles(), hiScores_);
    }

    if (!loadStrings(options_.language)) {
        error("Cannot open the file 'data/uk.dat'");
        return false;
    }

    input_.open();

    // The 2002 game switched the screen to 640x480x16 at 85 Hz; the display
    // now shows the 640x480 frames scaled to the screen (the Switch's, 1280x720
    // or 1920x1080, or a window).
    platform::DisplayOptions display;
    display.title = "Blip & Blop";
    display.width = 640;
    display.height = 480;
    display.headless = session_.headless();
    display.integerScaling = integerScaling_ = options_.integerScaling;
    display.filter = filter_ = options_.filter;
    std::string reason;
    display_ = platform_.createDisplay(display, reason);
    if (!display_) {
        core::Log::warning("videoOpen() - " + reason);
        error("Cannot open the game window.");
        return false;
    }
    core::Log::info("Display: " + std::to_string(display_->refreshRate()) + " Hz, vsync " +
                    (display_->vsync() ? "on" : "off") + ", " + display_->describe());

    struct FontFile {
        const char* name;
        core::gfx::BitmapFont* font;
    };
    const FontFile fonts[] = {{"data/menu.lft", &menu_},
                              {"data/menus.lft", &menus_},
                              {"data/cool.lft", &cool_},
                              {"data/rpg.lft", &rpg_}};
    for (const FontFile& f : fonts) {
        if (!game::loadFont(source, f.name, *f.font)) {
            error(std::string("Cannot open the file ") + f.name);
            return false;
        }
    }

    if (!game::loadSprites(source, "data/inter.gfx", interface_)) {
        core::Log::warning("Cannot load interface.");
        return false;
    }
    if (!game::loadMusic(source, "data/inter.mbk", jingles_, false) ||
        !game::loadMusic(source, "data/interl.mbk", attractMusic_, true)) {
        core::Log::warning("Cannot load interface musics.");
        return false;
    }
    return true;
}

void App::release() {
    for (core::gfx::BitmapFont* f : {&menu_, &menus_, &cool_, &rpg_}) {
        f->clear();
    }
    interface_.clear();
    display_.reset();
    jingles_.clear();
    attractMusic_.clear();
}

bool App::loadStrings(core::input::Language language) {
    const core::assets::AssetSource& source = files_.assets();
    language_ = language;
    // The French text was lost with the 2002 fr.dat (D-25); Phase 9 wrote
    // it again, in the pack only.
    if (language == core::input::Language::French) {
        if (strings_.load(source, "data/fr.dat")) {
            return true;
        }
        core::Log::info("No French text in this data: English used");
    }
    return strings_.load(source, "data/uk.dat");
}

void App::present() {
    session_.onFramePresented(screen_);

    // The menus switched the language: the next frame is drawn with it.
    if (options_.language != language_) {
        loadStrings(options_.language);
    }

    if (!display_) {
        return;
    }

    // The display always syncs with the screen. The frames are also paced to
    // 60 per second here: the screens that move by frame (the character
    // choice) were made for 60 frames per second, the rate the replays are
    // recorded at, and vsync does not hold them there on a faster screen
    // (measured on macOS: 120 frames per second on a 120 Hz screen, and once
    // 717 per second). At 60 Hz, the Switch's rate, the pacing never waits.
    // (Deterministic runs are paced by the session.)
    if (integerScaling_ != options_.integerScaling || filter_ != options_.filter) {
        integerScaling_ = options_.integerScaling;
        filter_ = options_.filter;
        display_->setScaling(integerScaling_, filter_);
        core::Log::info("Display: " + display_->describe());
    }
    display_->present(screen_);

    if (presented_++ == 0) {
        firstPresent_ = platform_.clock().microseconds();
    }

    if (!session_.deterministic() && !session_.headless()) {
        constexpr std::uint64_t kFrame = 1000000 / 60;
        platform::Clock& clock = platform_.clock();
        const std::uint64_t now = clock.microseconds();
        if (now < nextFrame_) {
            clock.sleep(nextFrame_ - now);
            nextFrame_ += kFrame;
        } else {
            nextFrame_ = now + kFrame;
        }
    }
}

// Runs the stack until it is empty.
void App::runScreens(game::ScreenStack& stack) {
    std::size_t stalled = 0;

    while (!stack.empty()) {
        const game::Frame frame = stack.step();

        if (stack.stalledSteps() != stalled) {
            stalled = stack.stalledSteps();
            core::Log::error(std::string("screen '") + (stack.empty() ? "?" : stack.top().name()) +
                             "' neither drew nor changed the screens");
        }

        if (frame == game::Frame::Present) {
            present();
        } else if (frame == game::Frame::Wait) {
            session_.idle();
        }
    }
}

int App::run() {
    if (!init()) {
        release();
        return 1;
    }

    {
        // The screens, from the intro to the main menu's EXIT.
        game::Game game(services_);
        game::ScreenStack screens;
        if (session_.onlyLevel().empty()) {
            screens.push(std::make_unique<game::AttractScreen>(services_, game));
        } else {
            // One level, one player (the level editor's test).
            auto campaign = std::make_unique<game::CampaignScreen>(services_, game, 1, 0);
            campaign->playOnly(session_.onlyLevel(), session_.onlyLevelKind());
            screens.push(std::move(campaign));
        }
        runScreens(screens);
    }

    if (presented_ > 1 && display_) {
        const std::uint64_t elapsed = platform_.clock().microseconds() - firstPresent_;
        core::Log::info(std::to_string(presented_) + " frames presented, " +
                        std::to_string(elapsed > 0 ? (presented_ - 1) * 1000000 / elapsed : 0) +
                        " per second");
    }
    core::Log::info("Releasing Blip & Blop");
    release();

    saveHiScoresNow();
    saveSettingsNow();

    return session_.finish();
}

void App::saveSettingsNow() {
    if (!session_.deterministic()) {
        saveSettings(settingsPath_, platform_.files(), options_, input_);
    }
}

void App::saveHiScoresNow() {
    if (!session_.deterministic() && !saveHiScores(hiScoresPath_, platform_.files(), files_, hiScores_)) {
        core::Log::info("Cannot save hi-scores");
    }
}

}  // namespace bb::app
