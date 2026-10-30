#pragma once

#include <cstdint>

#include "bb/game/screen.hpp"
#include "bb/game/services.hpp"

namespace bb::game {

// "LOADING" in the middle of the screen (Game::drawLoading()).
void drawLoading(Services& services);

// Choose Blip or Blop: each slides in over a scrolling starfield; right and
// left switch, return (or fire) chooses. Stores 0 for Blip, 1 for Blop (0
// if the application closes).
class CharacterSelectScreen : public Screen {
public:
    CharacterSelectScreen(Services& services, int& character) noexcept
        : services_(services), character_(character) {}
    Frame update(ScreenStack& stack) override;
    const char* name() const override { return "character select"; }

private:
    enum class Step { BlipAppears, BlipLeaves, BlopAppears, BlopLeaves };

    Services& services_;
    int& character_;
    bool started_ = false;
    Step step_ = Step::BlipAppears;
    int blinkTick_ = 0;
    int x_ = -300;  // the character
    int y_ = 255;
    int nameX_ = 1040;
    int nameY_ = 255;
    int titleY_ = -50;
    int background1X_ = 0;
    int background2X_ = 640;
};

// The title picture, for up to 5 seconds or until a key is pressed.
class TitleScreen : public Screen {
public:
    explicit TitleScreen(Services& services) noexcept : services_(services) {}
    Frame update(ScreenStack& stack) override;
    const char* name() const override { return "title"; }

private:
    Services& services_;
    bool started_ = false;
    std::uint32_t start_ = 0;
};

// The ten best scores slide in, stay 10 seconds (or until a key is
// pressed) and slide out.
class HiScoreTableScreen : public Screen {
public:
    explicit HiScoreTableScreen(Services& services) noexcept : services_(services) {}
    Frame update(ScreenStack& stack) override;
    const char* name() const override { return "hi-score table"; }

private:
    void draw();

    enum class Step { Start, Show, SlideOut };

    Services& services_;
    Step step_ = Step::Start;
    int x_[10] = {};
    std::uint32_t start_ = 0;
};

// The game over picture, until a key is pressed.
class GameOverScreen : public Screen {
public:
    explicit GameOverScreen(Services& services) noexcept : services_(services) {}
    Frame update(ScreenStack& stack) override;
    const char* name() const override { return "game over"; }

private:
    Services& services_;
    bool started_ = false;
};

// A player types a name for the hi-score table with the keyboard (19
// characters: the keys whose name is one character, space, backspace;
// return ends).
class HiScoreEntryScreen : public Screen {
public:
    HiScoreEntryScreen(Services& services, int score, int playerNumber);
    Frame update(ScreenStack& stack) override;
    const char* name() const override { return "hi-score name entry"; }

private:
    void drawTitle();
    // The name typed on the on-screen keyboard: in capitals, without the
    // characters the menu font lacks, 19 at most.
    void setName(const std::string& text);

    // Keyboard: the system's on-screen keyboard (the Switch's) is shown.
    enum class Step { SlideIn, NextKey, KeyTyped, Keyboard, SlideOut };

    Services& services_;
    int score_;
    int playerNumber_;
    Step step_ = Step::SlideIn;
    char title_[30] = {};
    char name_[21] = {};
    int length_ = 0;
    int x_ = 640;
    int key_ = -1;
    unsigned int typed_ = 0;
    bool noted_ = false;
    bool keyboardAsked_ = false;
};

class Player;

// The bonuses of a finished level, counted up on a darkened band over the
// last frame: perfect (not hurt), objective, casualties (the % of the
// enemies killed) and time left, for each player; a key speeds the
// counting up; a life for every 500,000 points. With three or four players
// (the four-player mode) the bonuses are a table: the labels once, then a
// column per player.
class LevelSummaryScreen : public Screen {
public:
    // players: kMaxPlayers of them, null when absent. bonusLevel: a bonus
    // level; bonusFailed: a bonus level that was lost (no bonus points at
    // all).
    LevelSummaryScreen(Services& services, Player* const* players, bool playersDead, bool bonusLevel,
                       bool bonusFailed) noexcept;
    Frame update(ScreenStack& stack) override;
    const char* name() const override { return "level summary"; }

private:
    // What is counted and shown for one player (the locals of showPE).
    struct Tally {
        Player* player = nullptr;
        bool shown = false;
        int x = 0;
        const core::gfx::BitmapFont* font = nullptr;
        int stage = 0;  // 0 perfect, 1 objective, 2 casualties, 3 time left, 4 done
        int perfect = 0;
        int perfectShown = 0;
        int objective = 0;
        int objectiveShown = 0;
        int killedPercent = 0;
        int killedShown = 0;
        int timeShown = 0;
        int totalBonus = 0;
        int score = 0;
        int lifeUp = 0;
    };

    void start();
    void count(Tally& t);
    void draw(Tally& t);
    // The table of the four-player mode: its labels, and a player's column.
    void drawLabels();
    void drawColumn(Tally& t);

    enum class Step { Start, Counting, Waited };

    Services& services_;
    Step step_ = Step::Start;
    unsigned int key_ = 0;
    bool playersDead_;
    bool bonusLevel_;
    bool bonusFailed_;
    Tally tally_[kMaxPlayers];
    bool table_ = false;  // three or four players
    int delay_ = 0;
};

}  // namespace bb::game
