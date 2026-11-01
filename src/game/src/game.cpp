// A campaign and its levels (game.cpp of 2002: jouePartie, joueNiveau,
// chargePartie, chargeNiveau and the frame of the level loop).

#include "bb/game/game.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "bb/core/assets/asset_source.hpp"
#include "bb/core/input/codes.hpp"
#include "bb/core/log.hpp"
#include "bb/game/decor.hpp"
#include "bb/game/hud.hpp"
#include "bb/game/screens.hpp"
#include "bb/game/world.hpp"

namespace bb::game {

namespace {

namespace input = core::input;

constexpr int kPressKeyText = 852;     // "Press a key to start."
constexpr int kLoadingDotsText = 853;  // "Loading..."
constexpr int kFullHealth = 5;

void log(const std::string& line) {
    core::Log::info(line);
}

}  // namespace

Game::Game(Services& services)
    : services_(services),
      world_(*services.world),
      level_(*services.world),
      dialogue_(services),
      controls_{AliasControls(*services.input, AliasControls::firstOf(0)),
                AliasControls(*services.input, AliasControls::firstOf(1)),
                AliasControls(*services.input, AliasControls::firstOf(2)),
                AliasControls(*services.input, AliasControls::firstOf(3))} {}

//-----------------------------------------------------------------------------
// The campaign
//-----------------------------------------------------------------------------

bool Game::startCampaign(const Lineup& lineup) {
    Banks& banks = services_.banks;

    // The interface pictures are freed while the campaign runs.
    banks.interface->clear();
    banks.attractMusic->stopAll();

    if (!loadCampaign(lineup)) {
        releaseCampaign();
        return false;
    }

    level_.deathWait = 0;
    world_.currentMusic = -1;

    // Players 1 to count, each with its hero and with the controls of the
    // input player that chose it.
    static const char* const kHeroNames[] = {"Blip", "Blop", "Bleach", "Blitz"};
    std::string note = "campaign starts:";
    for (int i = 0; i < lineup.count && i < kMaxPlayers; ++i) {
        const int c = lineup.controls[i];
        const int controls = c >= 0 && c < kMaxPlayers ? c : i;
        players_[i] = new Player(world_, lineup.heroes[i], controls_[controls], i);
        world_.players.add(players_[i]);
        level_.players[i] = players_[i];
        note += std::string(i == 0 ? " " : ", ") + kHeroNames[static_cast<int>(lineup.heroes[i])] +
                " (controls " + std::to_string(controls + 1) + ")";
    }
    services_.session->note(note.c_str());

    world_.cowBombUnlocked = false;
    for (bool& perfect : lastPerfect_) {
        perfect = false;
    }
    return true;
}

void Game::levelPlayed() {
    // The cow bomb is given after the first level.
    if (!world_.cowBombUnlocked) {
        for (Player* p : players_) {
            if (p != nullptr) {
                p->cowBombs = 1;
            }
        }
        world_.cowBombUnlocked = true;
    }
}

void Game::finishCampaign() {
    releaseCampaign();
    loadSprites(*services_.assets, "data/inter.gfx", *services_.banks.interface);
}

void Game::deletePlayers() {
    for (int i = 0; i < kMaxPlayers; ++i) {
        delete players_[i];
        players_[i] = nullptr;
        level_.players[i] = nullptr;
    }
}

bool Game::loadCampaign(const Lineup& lineup) {
    const core::assets::AssetSource& assets = *services_.assets;
    Banks& banks = services_.banks;

    log("Loading global game files...");

    struct SpriteFile {
        const char* name;
        core::gfx::SpriteBank* bank;
    };
    const SpriteFile sprites[] = {{"data/blip.gfx", banks.blip},
                                  {"data/blop.gfx", banks.blop},
                                  {"data/bb.gfx", banks.heroes},
                                  {"data/misc.gfx", banks.misc},
                                  {"data/rpg_bb.gfx", banks.heroFaces}};
    for (const SpriteFile& f : sprites) {
        if (!loadSprites(assets, f.name, *f.bank)) {
            return false;
        }
    }
    // Bleach and Blitz, recoloured Blip and Blop (tools/art/new_heroes.py),
    // only when one of them plays.
    bool newHeroes = false;
    for (int i = 0; i < lineup.count && i < kMaxPlayers; ++i) {
        newHeroes = newHeroes || lineup.heroes[i] == Hero::Bleach || lineup.heroes[i] == Hero::Blitz;
    }
    if (newHeroes && (!loadSprites(assets, "data/bleach.gfx", *banks.bleach) ||
                        !loadSprites(assets, "data/blitz.gfx", *banks.blitz) ||
                        !loadFont(assets, "data/scorep.lft", *banks.scoreBleach) ||
                        !loadFont(assets, "data/scoreg.lft", *banks.scoreBlitz))) {
        return false;
    }

    struct FontFile {
        const char* name;
        core::gfx::BitmapFont* font;
    };
    const FontFile fonts[] = {{"data/scorei.lft", banks.scoreBlip},
                              {"data/scoreo.lft", banks.scoreBlop},
                              {"data/ammo1.lft", banks.ammo},
                              {"data/ammo2.lft", banks.ammoUsed}};
    for (const FontFile& f : fonts) {
        if (!loadFont(assets, f.name, *f.font)) {
            return false;
        }
    }

    if (!loadSounds(assets, "data/misc.sfx", *banks.miscSounds) ||
        !loadSounds(assets, "data/bb.sfx", *banks.heroSounds) || !loadParts("data/bb.lst")) {
        return false;
    }

    log("Successfully loaded all needed global game files");
    return true;
}

void Game::releaseCampaign() {
    Banks& banks = services_.banks;

    banks.blip->clear();
    banks.blop->clear();
    banks.bleach->clear();
    banks.blitz->clear();
    banks.scoreBleach->clear();
    banks.scoreBlitz->clear();
    banks.heroes->clear();
    banks.misc->clear();
    banks.heroFaces->clear();
    banks.scoreBlip->clear();
    banks.scoreBlop->clear();
    banks.ammo->clear();
    banks.ammoUsed->clear();
    banks.heroSounds->clear();
    banks.miscSounds->clear();
    world_.players.clear();
}

bool Game::loadParts(const char* name) {
    std::string error;
    const auto campaign = services_.assets->campaign(name, error);
    if (!campaign) {
        log(std::string("Cannot open ") + name + " (" + error + ")");
        return false;
    }
    if (campaign->entries.size() > static_cast<std::size_t>(kMaxParts)) {
        log(std::string("File ") + name + " corrupted!");
        return false;
    }

    partCount_ = 0;
    for (const auto& entry : campaign->entries) {
        if (entry.path.size() >= 200) {  // the 2002 name buffers
            log(std::string("File ") + name + " corrupted!");
            return false;
        }
        Part& part = parts_[static_cast<std::size_t>(partCount_)];
        part.kind = static_cast<int>(entry.kind);
        part.levelKind = entry.levelKind;
        part.file = entry.path;
        partCount_ += 1;
    }
    return true;
}

//-----------------------------------------------------------------------------
// A level
//-----------------------------------------------------------------------------

bool Game::startLevel(const char* name, int kind) {
    // (The locals of the 2002 joueNiveau, zero at each call.)
    for (BonusLives& b : bonus_) {
        b = BonusLives{};
    }
    levelKind_ = kind;

    skipped_ = false;
    level_.reset();
    services_.decor->rewind();

    if (!loadLevel(name)) {
        return false;
    }

    char note[256];
    std::snprintf(note, sizeof(note), "level %s loaded (kind %d)", name, kind);
    services_.session->note(note);

    // No flamethrower under water (the 2002 code tested the level names).
    world_.flamethrowerAllowed = world_.marker("level.flamethrower", 0, 1) != 0;

    // The players at their starting points (D-89 fixed: 2002 reset the
    // first player's animation while placing the second); players 3 and 4
    // on those of 1 and 2.
    for (int i = 0; i < kMaxPlayers; ++i) {
        Player* p = players_[i];
        if (p == nullptr || p->lives <= 0) {
            bonus_[i].bringBack = false;
            continue;
        }
        p->x = startX_[i % 2];
        p->y = startY_[i % 2];
        p->mode = kNormal;
        p->animation.frame = 0;
        p->animation.subFrame = 0;
        p->direction = aim::kRight;
        p->firing = false;

        // A bonus level is played with one life; the others come back.
        if (kind == kBonusLevel) {
            bonus_[i].lives = p->lives;
            bonus_[i].bringBack = true;
            if (bonus_[i].lives > 0) {
                p->lives = 1;
            }
        }
    }

    // A new world: the kills counted again, the players drop in with the
    // rifle (or the super weapon after a perfect level).
    if (kind == kBonusLevel || kind == kCompleteLevel || kind == kFirstLevel || kind == kEndLevel) {
        world_.enemiesCreated = 0;

        for (int i = 0; i < kMaxPlayers; ++i) {
            Player* p = players_[i];
            if (p == nullptr) {
                continue;
            }
            p->kills = 0;
            if (kind != kEndLevel) {
                p->mode = Player::kComingBack;
                p->y = -50;
                p->landingY = startY_[i % 2];
            }
            p->setWeapon(Weapon::Rifle);
            p->perfect = true;
            p->health.points = kFullHealth;
            if (lastPerfect_[i]) {
                p->equipSuperWeapon();
            }
        }
    }

    for (bool& perfect : lastPerfect_) {
        perfect = false;
    }

    // A bonus level is timed.
    level_.bonusLevel = kind == kBonusLevel;

    // The camera on the screen of the first player in the list.
    world_.players.rewind();
    if (const Entity* first = world_.players.current()) {
        world_.camera.x = first->x - first->x % 640;
    }

    services_.decor->rewind();
    log("Now getting into game loop.");
    return true;
}

bool Game::loadLevel(const char* name) {
    const core::assets::AssetSource& assets = *services_.assets;
    Banks& banks = services_.banks;

    std::string error;
    const auto data = assets.level(name, error);
    if (!data) {
        log(std::string("Game::chargeNiveau() -> Cannot load <") + name + "> (" + error + ")");
        return false;
    }
    log(std::string("Loading level <") + name + ">");

    // The seven asset names, in file order: decor, props, enemies, sounds,
    // music, dialogue script, dialogue faces; all but the decor optional.
    const auto file = [&](int i) { return "data/" + data->assets[static_cast<std::size_t>(i)]; };
    const auto present = [&](int i) { return !data->assets[static_cast<std::size_t>(i)].empty(); };

    if (!loadSprites(assets, file(0), *banks.decor)) {
        return false;
    }
    if (present(1) && !loadSprites(assets, file(1), *banks.props)) {
        return false;
    }
    if (present(2) && !loadSprites(assets, file(2), *banks.enemies)) {
        return false;
    }
    if (present(3) && !loadSounds(assets, file(3), *banks.levelSounds)) {
        return false;
    }
    if (present(4)) {
        banks.levelMusicFile = file(4);
        if (!loadMusic(assets, banks.levelMusicFile, *banks.levelMusic)) {
            return false;
        }
    } else {
        banks.levelMusicFile.clear();
    }
    // Without a script of its own, the level keeps the previous one.
    if (present(5)) {
        dialogue_.setScript(file(5).c_str());
    }
    if (present(6) && !loadSprites(assets, file(6), *banks.dialogueFaces)) {
        return false;
    }

    world_.collision.load(*data);
    world_.markers = data->markers;
    services_.decor->setLevel(banks.decor, std::vector<int>(data->screens.begin(), data->screens.end()));

    startX_[0] = data->start1X;
    startY_[0] = data->start1Y;
    startX_[1] = data->start2X;
    startY_[1] = data->start2Y;

    level_.victory.x = data->victoryX;
    level_.victory.flag1 = data->victoryFlag1;
    level_.victory.value1 = data->victoryValue1;
    level_.victory.flag2 = data->victoryFlag2;
    level_.victory.value2 = data->victoryValue2;

    // Every event type is ported; the unused ones give none.
    for (const core::assets::LevelEventRecord& record : data->events) {
        std::unique_ptr<LevelEvent> event = makeLevelEvent(record);
        if (event) {
            world_.sleepingEvents.add(event.release());
        }
    }

    log("Successfully loaded all level files");
    return true;
}

void Game::releaseLevel() {
    Banks& banks = services_.banks;
    World& w = world_;

    w.collision.clear();

    // The entities first: their destructors may use the level banks (the
    // lightsaber stops its looping sound, D-01).
    w.sleepingEvents.clear();
    w.events.clear();
    w.playerShots.clear();
    w.cowBombs.clear();
    w.impacts.clear();
    w.vehicles.clear();
    w.enemies.clear();
    w.enemyShots.clear();
    w.enemyGenerators.clear();
    w.pickups.clear();
    w.pickupGenerators.clear();
    w.bannerTexts.clear();
    w.props.clear();
    w.midground.clear();
    w.foreground.clear();
    w.movingPlatforms.clear();
    w.blood.clear();
    w.gore.clear();
    w.weatherParticles.clear();
    w.bubbles.clear();

    banks.levelMusic->clear();
    banks.levelSounds->clear();
    banks.enemies->clear();
    banks.decor->clear();
    banks.props->clear();
    banks.dialogueFaces->clear();
}

void Game::showBriefing(const char* name) {
    loadSprites(*services_.assets, name, briefingPicture_, false);

    char note[256];
    std::snprintf(note, sizeof(note), "briefing %s", name);
    services_.session->note(note);

    briefing_ = true;

    core::gfx::Pixmap& screen = *services_.screen;
    core::gfx::fill(screen, nullptr, core::gfx::kAlphaMask);
    core::gfx::draw(screen, *briefingPicture_[0], 0, 0);
    services_.fonts->rpg->printCentred(screen, 320, 460, services_.texts->text(kLoadingDotsText));
    // The caller presents the frame.
}

void Game::drawBriefingPrompt() {
    core::gfx::Pixmap& screen = *services_.screen;
    core::gfx::fill(screen, nullptr, core::gfx::kAlphaMask);
    core::gfx::draw(screen, *briefingPicture_[0], 0, 0);
    services_.fonts->rpg->printCentred(screen, 320, 460, services_.texts->text(kPressKeyText));
}

void Game::closeBriefing() {
    briefingPicture_.clear();
    briefing_ = false;
}

void Game::startLevelLoop() {
    frameClock_.reset(services_.session->nowMicroseconds());
    frameTicks_ = 0;
    services_.session->note("level starts");
}

bool Game::levelRunning() const {
    return !level_.playersDead && !level_.won && !skipped_ && !services_.session->quitRequested();
}

// Frame pacing: as many 11.8 ms ticks as the time since the last frame
// allows (bb::core::FixedClock, at most 3 per frame). The 2002 gameLoop ran
// one tick per frame while the moving average of its last 50 frame
// durations was within 1 ms of a tick, and caught up with the clock
// otherwise (migration.md, Phase 3).
void Game::beginFrame() {
    frameTicks_ = frameClock_.advance(services_.session->nowMicroseconds());
}

void Game::finishLevelLoop() {
    Session& session = *services_.session;
    const bool quitting = session.quitRequested();

    session.note(level_.playersDead ? "level ends: players died"
                 : level_.won       ? "level ends: complete"
                 : skipped_         ? "level ends: skipped"
                                    : "level ends: application closing");

    for (Player* p : players_) {
        if (p != nullptr) {
            p->endLevel();
        }
    }

    world_.currentMusic = -1;
    if (services_.audio->musicOn) {
        services_.banks.levelMusic->stopAll();
    }

    // After a bonus level the players get their lives back, and cannot
    // have lost.
    if (levelKind_ == kBonusLevel && !skipped_ && !quitting) {
        for (int i = 0; i < kMaxPlayers; ++i) {
            Player* p = players_[i];
            if (p == nullptr || !bonus_[i].bringBack) {
                continue;
            }
            if (p->dead && bonus_[i].lives > 0) {
                p->dead = false;
                world_.players.add(p);
            }
            p->lives = bonus_[i].lives;
        }
        level_.won = true;
    }
}

bool Game::levelEndsWithPicture() const {
    return level_.won && !level_.playersDead && levelKind_ != kEndLevel;
}

std::unique_ptr<Screen> Game::levelSummary() {
    const int kind = levelKind_;
    const auto summary = [&](bool bonus, bool failed) {
        return std::make_unique<LevelSummaryScreen>(services_, players_, level_.playersDead, bonus, failed);
    };

    if (level_.won && !level_.playersDead && kind != kEndLevel) {
        if ((kind == kBonusLevel && !skipped_ && world_.flags[Flag::Timer] > 0) || kind == kCompleteLevel ||
            kind == kLastLevel) {
            for (int i = 0; i < kMaxPlayers; ++i) {
                if (players_[i] != nullptr) {
                    lastPerfect_[i] = players_[i]->perfect;
                }
            }
            return summary(kind == kBonusLevel, false);
        }
        if (kind == kBonusLevel) {
            return summary(true, true);
        }
    } else if (kind == kBonusLevel) {
        return summary(true, true);
    }
    return nullptr;
}

std::unique_ptr<Screen> Game::summaryScreen() {
    return std::make_unique<LevelSummaryScreen>(services_, players_, level_.playersDead, false, false);
}

//-----------------------------------------------------------------------------
// Ticks and frames
//-----------------------------------------------------------------------------

void Game::tickBegin() {
    Input& in = *services_.input;
    in.update();

    CheatKeys cheats;
    cheats.enabled = services_.options->cheats;
    if (cheats.enabled) {
        cheats.ammo[0] = in.held(input::kKeyF1);
        cheats.cowBomb[0] = in.held(input::kKeyF2);
        cheats.life[0] = in.held(input::kKeyF3);
        cheats.ammo[1] = in.held(input::kKeyF5);
        cheats.cowBomb[1] = in.held(input::kKeyF6);
        cheats.life[1] = in.held(input::kKeyF7);
        cheats.win = in.held(input::kKeyF9);
        cheats.listsDisplay = in.held(input::kKeyF11);
        cheats.fpsDisplay = in.held(input::kKeyF12);
    }

    level_.tick(cheats);
}

bool Game::pauseRequested() {
    // The focus loss is consumed first, so that it does not open the menu a
    // second time after an Escape pause.
    const bool focusLost = services_.input->takeFocusLost();
    return focusLost || services_.input->held(input::kKeyEscape);
}

void Game::tickEnd() {
    Session& session = *services_.session;
    if (session.deterministic()) {
        session.onTick(level_.fingerprint(session.traceStream(), session.frame()));
    }
}

void Game::resetTimeBase() {
    // The time spent in a pause menu or a dialogue is not simulated.
    frameClock_.rebase(services_.session->nowMicroseconds());
}

void Game::updateEvents() {
    world_.updateEvents();
}

void Game::drawAll(bool flip) {
    if (!flip) {
        world_.blink = false;
        world_.slowBlink = false;
    }

    // Frames drawn during the last second of game time (D-45).
    framesThisSecond_ += 1;
    const std::uint64_t now = services_.session->nowMicroseconds();
    if (now - secondStart_ >= 1000000) {
        fps_ = framesThisSecond_;
        framesThisSecond_ = 0;
        secondStart_ = now;
    }

    // Smooth motion: the fraction of a tick owed since the last one.
    world_.motion.on = services_.options->smoothMotion;
    world_.motion.fraction = static_cast<int>(frameClock_.owed() * 256 / frameClock_.tickMicroseconds());

    core::gfx::Pixmap& screen = *services_.screen;
    level_.drawWorld(screen, *services_.decor);
    Hud::draw(world_, players_, level_.bonusLevel, world_.blink);
    level_.drawOverlays(screen);

    if (level_.showFps) {
        char buffer[40];
        std::snprintf(buffer, sizeof(buffer), "FPS = %d", fps_);
        services_.fonts->rpg->print(screen, 10, 130, buffer);
    }
    // The caller presents the frame.
}

}  // namespace bb::game
