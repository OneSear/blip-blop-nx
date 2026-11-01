#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "bb/core/gfx/sprite.hpp"
#include "bb/game/cutscene.hpp"
#include "bb/game/game.hpp"
#include "bb/game/menus.hpp"
#include "bb/game/screen.hpp"

namespace bb::game {

// The application's first screen: the intro, then the main menu, with the
// attract cycle (title, hi-scores, credits, intro) after 10 seconds without
// a key, until EXIT (the 2002 go()).
class AttractScreen : public Screen {
public:
    AttractScreen(Services& services, Game& game) : services_(services), game_(game) {}
    Frame update(ScreenStack& stack) override;
    const char* name() const override { return "attract"; }

private:
    // One iteration of the menu loop; Frame::Continue when it pushed a
    // screen.
    Frame menuStep(ScreenStack& stack);
    Frame menuDraw();
    std::uint32_t now() const {
        return static_cast<std::uint32_t>(services_.session->nowMicroseconds() / 1000);
    }

    enum class Step {
        Start,
        AfterIntro,
        Outer,
        Menu,
        MenuWaited,
        AfterAttract,
        AfterMenu,
        StartGame,
        AfterGame,
        OuterEnd
    };

    Services& services_;
    Game& game_;
    Step step_ = Step::Start;
    CutscenePlayer cutscene_{services_};
    MainMenu menu_{services_};
    int result_ = MainMenu::kContinue;
    int attract_ = 0;              // the next screen of the cycle
    std::uint32_t lastInput_ = 0;  // when a key was last pressed
    Lineup lineup_;                // who plays (none: back from the hero choice)
};

// A campaign: the parts of bb.lst in turn, the ending after the last
// level, then game over and the hi-scores (the 2002 jouePartie).
class CampaignScreen : public Screen {
public:
    CampaignScreen(Services& services, Game& game, const Lineup& lineup)
        : services_(services), game_(game), lineup_(lineup) {}
    // Plays one level only ("data/xxx.lvl", a level kind of bb.lst), then
    // stops: no ending, game over or hi-scores (the level editor's test).
    void playOnly(std::string file, int kind) {
        onlyFile_ = std::move(file);
        onlyKind_ = kind;
    }
    Frame update(ScreenStack& stack) override;
    const char* name() const override { return "campaign"; }

private:
    // Waits for the release of the keys, a key, and its release.
    void pushKeyPress(ScreenStack& stack);

    enum class Step {
        Start,
        Parts,
        AfterLevel,
        AfterPart,
        AfterParts,
        EndingScene,
        EndingPicture1,
        EndingPicture2,
        EndingKey,
        EndingCredits,
        EndingLast1,
        EndingLast2,
        EndingLastKey,
        EndingSummary,
        EndingDone,
        Release,
        GameOver,
        HiScore,  // for each player in turn (hiScorePlayer_)
        Table,
        Done,
        OnlyLevel,
        OnlyLevelDone,
    };

    // What the ending loads (the locals of the 2002 jouePartie).
    struct Ending {
        explicit Ending(AudioOutput& audio) : credits(audio), end(audio) {}
        MusicBank credits;
        MusicBank end;
        core::gfx::SpriteBank pictures;  // theend.gfx
    };

    Services& services_;
    Game& game_;
    Lineup lineup_;
    Step step_ = Step::Start;
    CutscenePlayer cutscene_{services_};
    bool won_ = true;
    int part_ = 0;
    std::unique_ptr<Ending> ending_;
    unsigned int key_ = 0;
    int hiScorePlayer_ = 1;  // the next player whose score may enter the table
    std::string onlyFile_;
    int onlyKind_ = 0;
};

// A level: the briefing, the frames and their ticks (with the pause menu
// and the dialogues in between), the end, the summary (joueNiveau).
class LevelScreen : public Screen {
public:
    // won: set when the level is over, true if the players passed it.
    LevelScreen(Services& services, Game& game, const char* file, int kind, bool& won)
        : services_(services), game_(game), file_(file), kind_(kind), won_(won) {}
    Frame update(ScreenStack& stack) override;
    const char* name() const override { return "level"; }

private:
    // Runs or resumes one tick: true when it is complete, false when it
    // pushed the pause menu or a dialogue (the tick goes on when that
    // screen has finished).
    bool runTick(ScreenStack& stack);

    enum class Step {
        Start,
        Briefing,
        BriefingKey,
        BriefingClose,
        FirstEvents,
        FirstTick,
        Frame,
        Ticks,
        End,
        EndPicture,
        Summary,
        Loading,
        Release,
    };
    enum class TickStep { Begin, Middle, End };

    Services& services_;
    Game& game_;
    std::string file_;
    int kind_;
    bool& won_;
    Step step_ = Step::Start;
    TickStep tick_ = TickStep::Begin;
    bool paused_ = false;
    bool inDialogue_ = false;
    bool framePending_ = false;
    unsigned int key_ = 0;
};

// The pause menu over the level.
class PauseScreen : public Screen {
public:
    // levelLeft is set when the player chooses to leave the level.
    PauseScreen(Services& services, Game& game, bool& levelLeft)
        : services_(services), game_(game), levelLeft_(levelLeft) {}
    Frame update(ScreenStack& stack) override;
    const char* name() const override { return "pause"; }

private:
    enum class Step { Enter1, Enter2, Enter3, WaitRelease, Menu, Leave2 };

    Services& services_;
    Game& game_;
    bool& levelLeft_;
    Step step_ = Step::Enter1;
    PauseMenu menu_{services_};
};

// The scene World::dialogueToPlay of the level's script, over the level;
// then dialogueToPlay is -1 again.
class DialogueScreen : public Screen {
public:
    DialogueScreen(Services& services, Game& game) : services_(services), game_(game) {}
    Frame update(ScreenStack& stack) override;
    const char* name() const override { return "dialogue"; }

private:
    enum class Step { Start, Scene, Escaped, End, Done };

    Services& services_;
    Game& game_;
    Step step_ = Step::Start;
    bool continued_ = true;
};

}  // namespace bb::game
