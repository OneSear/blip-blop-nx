#pragma once

#include <array>
#include <memory>
#include <string>

#include "bb/core/fixed_clock.hpp"
#include "bb/core/gfx/sprite.hpp"
#include "bb/game/dialogue.hpp"
#include "bb/game/level.hpp"
#include "bb/game/player.hpp"
#include "bb/game/screen.hpp"
#include "bb/game/services.hpp"

namespace bb::game {

// A campaign and its levels (the 2002 Game): the campaign's banks and
// players, the parts of bb.lst, and each level from its loading to its
// summary. The screens drive it (CampaignScreen, LevelScreen); it draws,
// ticks and loads, but never waits.
class Game {
public:
    static constexpr int kMaxParts = 50;  // MAX_PART

    // The parts of the campaign (PART_*).
    enum PartKind { kCutscene = 0, kLevel = 1, kBriefing = 2 };
    // The kinds of levels (LVL_*).
    enum LevelKind {
        kBonusLevel = 0,
        kFirstLevel = 1,
        kIntermediateLevel = 2,
        kLastLevel = 3,
        kCompleteLevel = 4,
        kEndLevel = 5
    };

    explicit Game(Services& services);
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    // The campaign, as CampaignScreen plays it (jouePartie): startCampaign()
    // loads the campaign's banks and creates the players (false if a bank
    // is missing); then each part in turn (levelPlayed() after each level);
    // the ending if the last level was won; finishCampaign(); game over and
    // hi-scores unless the players left the game (campaignLeft());
    // deletePlayers().
    bool startCampaign(const Lineup& lineup);
    int partCount() const noexcept { return partCount_; }
    int partKind(int i) const noexcept { return parts_[static_cast<std::size_t>(i)].kind; }
    int partLevelKind(int i) const noexcept { return parts_[static_cast<std::size_t>(i)].levelKind; }
    const char* partFile(int i) const noexcept { return parts_[static_cast<std::size_t>(i)].file.c_str(); }
    void levelPlayed();
    void finishCampaign();
    bool campaignLeft() const noexcept { return skipped_; }
    // Player n, from 1; null when absent.
    Player* player(int n) const noexcept { return n >= 1 && n <= kMaxPlayers ? players_[n - 1] : nullptr; }
    void deletePlayers();

    // A level, as LevelScreen plays it (joueNiveau, gameLoop, updateAll):
    // startLevel() loads the level and places the players (false if it
    // cannot be loaded); after the 2-second wait, the briefing prompt if
    // hasBriefing(); the first tick (updateEvents() then a tick);
    // startLevelLoop(); then while levelRunning(): beginFrame(), the ticks
    // the frame needs (frameNeedsTick(), frameTickDone()), drawFrame()
    // presented, updateVictoryAndDefeat(). Then finishLevelLoop(), the
    // closing pictures and the levelSummary(), and releaseLevel().
    //
    // A tick: tickBegin(); the pause menu if pauseRequested(), then
    // resetTimeBase(); the dialogue if a scene is to play, then
    // resetTimeBase(); tickEnd().
    bool startLevel(const char* name, int kind);
    void showBriefing(const char* name);
    bool hasBriefing() const noexcept { return briefing_; }
    void drawBriefingPrompt();
    void closeBriefing();
    void startLevelLoop();
    bool levelRunning() const;
    void beginFrame();
    bool frameNeedsTick() const noexcept { return frameTicks_ > 0; }
    void frameTickDone() noexcept { frameTicks_ -= 1; }
    void drawFrame() { drawAll(); }
    void finishLevelLoop();
    bool levelEndsWithPicture() const;
    std::unique_ptr<Screen> levelSummary();
    // The summary of the level just played, not a bonus level (the end of
    // the campaign shows it after the credits; showPE(false)).
    std::unique_ptr<Screen> summaryScreen();
    bool levelWon() const noexcept { return level_.won; }
    void releaseLevel();

    void tickBegin();
    bool pauseRequested();
    void tickEnd();
    void resetTimeBase();
    bool& levelLeftFlag() noexcept { return skipped_; }
    DialoguePlayer& dialoguePlayer() noexcept { return dialogue_; }

    // The events alone (the first tick of a level).
    void updateEvents();
    void updateVictoryAndDefeat() { level_.updateVictoryAndDefeat(); }

    // The level, the HUD and the overlays; flip false: for a menu or a
    // dialogue over the level (the blinking stops).
    void drawAll(bool flip = true);

private:
    struct Part {
        int kind = 0;
        int levelKind = 0;
        std::string file;
    };
    // What a bonus level takes away from a player and gives back.
    struct BonusLives {
        int lives = 0;
        bool bringBack = false;
    };

    bool loadCampaign(const Lineup& lineup);  // chargePartie()
    void releaseCampaign();
    bool loadParts(const char* name);
    bool loadLevel(const char* name);  // chargeNiveau()

    Services& services_;
    World& world_;
    Level level_;
    DialoguePlayer dialogue_;
    AliasControls
        controls_[kMaxPlayers];  // NOLINT(modernize-use-default-member-init): built from the services

    std::array<Part, kMaxParts> parts_{};
    int partCount_ = 0;

    Player* players_[kMaxPlayers] = {};
    bool lastPerfect_[kMaxPlayers] = {};
    // The level's two starting points (players 3 and 4 start on those of 1
    // and 2).
    int startX_[2] = {0, 0};
    int startY_[2] = {0, 0};

    core::gfx::SpriteBank briefingPicture_;
    bool briefing_ = false;
    bool skipped_ = false;
    int levelKind_ = 0;
    BonusLives bonus_[kMaxPlayers];

    core::FixedClock frameClock_;
    int frameTicks_ = 0;

    // The F12 frame counter: frames drawn in the last second of game time.
    int fps_ = 0;
    int framesThisSecond_ = 0;
    std::uint64_t secondStart_ = 0;
};

}  // namespace bb::game
