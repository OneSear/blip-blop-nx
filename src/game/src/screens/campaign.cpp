// The screens that drive the game (screens/attract_screen.cpp,
// campaign_screen.cpp, level_screen.cpp, pause_screen.cpp,
// dialogue_screen.cpp of the legacy port).

#include "bb/game/campaign.hpp"

#include <cstdio>

#include "bb/assets/music/credits.hpp"
#include "bb/assets/music/end.hpp"
#include "bb/assets/music/inter.hpp"
#include "bb/assets/music/interl.hpp"
#include "bb/assets/sprites/inter.hpp"
#include "bb/game/credits.hpp"
#include "bb/game/hi_scores.hpp"
#include "bb/game/screens.hpp"
#include "bb/game/waits.hpp"
#include "bb/game/world.hpp"

namespace bb::game {

namespace {

constexpr int kPleaseWaitText = 850;  // "PLEASE WAIT"

}  // namespace

//-----------------------------------------------------------------------------
// AttractScreen
//-----------------------------------------------------------------------------

Frame AttractScreen::menuDraw() {
    menu_.draw();
    if (menu_.keyWasHeld()) {
        lastInput_ = now();
    }
    return Frame::Present;
}

// while (!r && !app_killed) { ... } of the 2002 go()
Frame AttractScreen::menuStep(ScreenStack& stack) {
    Input& in = *services_.input;

    in.update();
    if (in.anyPressed()) {
        lastInput_ = now();
    }

    // 10 seconds without a key: the next screen of the attract cycle.
    if (now() - lastInput_ >= 10000) {
        step_ = Step::AfterAttract;

        switch (attract_) {
            case 0:
            case 2:
            case 4:
            case 6:
                stack.push(std::make_unique<TitleScreen>(services_));
                return Frame::Continue;
            case 1:
                stack.push(std::make_unique<HiScoreTableScreen>(services_));
                return Frame::Continue;
            case 5:
                stack.push(std::make_unique<CreditsScreen>(services_, false));
                return Frame::Continue;
            case 3:
            case 7:
                cutscene_.loadSprites("data/intro.gfx");
                if (cutscene_.begin("data/intro.cin")) {
                    stack.push(std::make_unique<CutsceneScreen>(cutscene_));
                    return Frame::Continue;
                }
                return update(stack);
            default:
                break;
        }
        return update(stack);
    }

    core::gfx::draw(*services_.screen, *(*services_.banks.interface)[bb::assets::sprites::inter::kTitle], 0,
                    0);

    if (!menu_.update(result_)) {
        step_ = Step::MenuWaited;
        pushMenuWait(services_, stack, menu_.wait(), menu_.waitedKey());
        return Frame::Continue;
    }
    return menuDraw();
}

Frame AttractScreen::update(ScreenStack& stack) {
    Session& session = *services_.session;
    Banks& banks = services_.banks;

    for (;;) {
        switch (step_) {
            case Step::Start:
                core::gfx::fill(*services_.screen, nullptr, core::gfx::kAlphaMask);
                services_.fonts->cool->printCentred(*services_.screen, 320, 240,
                                                    services_.texts->text(kPleaseWaitText));

                cutscene_.loadSprites("data/intro.gfx");
                banks.attractMusic->play(bb::assets::music::interl::kTheme);

                step_ = Step::AfterIntro;
                if (cutscene_.begin("data/intro.cin")) {
                    stack.push(std::make_unique<CutsceneScreen>(cutscene_));
                    return Frame::Continue;
                }
                break;

            case Step::AfterIntro:
                session.note("main menu");
                menu_.start();
                lastInput_ = 0;
                step_ = Step::Outer;
                stack.push(std::make_unique<WaitReleaseScreen>(services_));
                return Frame::Continue;

            case Step::Outer:
                result_ = MainMenu::kContinue;
                step_ = Step::Menu;
                break;

            case Step::Menu:
                if (result_ == MainMenu::kContinue && !session.quitRequested()) {
                    return menuStep(stack);
                }
                step_ = Step::AfterMenu;
                break;

            case Step::MenuWaited:
                // The menu's update goes on after its wait.
                if (!menu_.update(result_)) {
                    pushMenuWait(services_, stack, menu_.wait(), menu_.waitedKey());
                    return Frame::Continue;
                }
                step_ = Step::Menu;
                return menuDraw();

            case Step::AfterAttract:
                attract_ = (attract_ + 1) % 8;
                menu_.stop();
                menu_.start();
                step_ = Step::Menu;
                if (services_.input->anyPressed()) {
                    lastInput_ = now();
                    stack.push(std::make_unique<WaitReleaseScreen>(services_));
                    return Frame::Continue;
                }
                break;

            case Step::AfterMenu:
                if (result_ == MainMenu::kExit) {
                    session.note("main menu: exit");
                }
                if (result_ == MainMenu::kStartOnePlayer || result_ == MainMenu::kStartTwoPlayers ||
                    result_ == MainMenu::kStartThreePlayers || result_ == MainMenu::kStartFourPlayers) {
                    // Each player takes a controller (the Switch's applet:
                    // its lights then show the player's number).
                    if (result_ != MainMenu::kStartOnePlayer) {
                        services_.input->requestPlayers(result_ == MainMenu::kStartTwoPlayers     ? 2
                                                        : result_ == MainMenu::kStartThreePlayers ? 3
                                                                                                  : 4);
                    }
                    step_ = Step::StartGame;
                    stack.push(std::make_unique<CharacterSelectScreen>(services_, character_));
                    return Frame::Continue;
                }
                step_ = Step::OuterEnd;
                break;

            case Step::StartGame:
                step_ = Step::AfterGame;
                stack.push(std::make_unique<CampaignScreen>(services_, game_,
                                                            result_ == MainMenu::kStartOnePlayer      ? 1
                                                            : result_ == MainMenu::kStartTwoPlayers   ? 2
                                                            : result_ == MainMenu::kStartThreePlayers ? 3
                                                                                                      : 4,
                                                            character_));
                return Frame::Continue;

            case Step::AfterGame:
                session.note("main menu");
                menu_.stop();
                menu_.start();
                banks.attractMusic->play(bb::assets::music::interl::kTheme);
                lastInput_ = now();
                step_ = Step::OuterEnd;
                break;

            case Step::OuterEnd:
                if (result_ != MainMenu::kExit && !session.quitRequested()) {
                    step_ = Step::Outer;
                    break;
                }
                menu_.stop();
                banks.attractMusic->stopAll();
                stack.finish();
                return Frame::Continue;
        }
    }
}

//-----------------------------------------------------------------------------
// CampaignScreen
//-----------------------------------------------------------------------------

void CampaignScreen::pushKeyPress(ScreenStack& stack) {
    // Pushed in reverse: they run release, key, release.
    stack.push(std::make_unique<WaitReleaseScreen>(services_));
    stack.push(std::make_unique<WaitKeyScreen>(services_, key_));
    stack.push(std::make_unique<WaitReleaseScreen>(services_));
}

Frame CampaignScreen::update(ScreenStack& stack) {
    Session& session = *services_.session;
    core::gfx::Pixmap& screen = *services_.screen;

    for (;;) {
        switch (step_) {
            case Step::Start:
                if (session.quitRequested() || !game_.startCampaign(players_, character_)) {
                    stack.finish();
                    return Frame::Continue;
                }
                part_ = 0;
                step_ = onlyFile_.empty() ? Step::Parts : Step::OnlyLevel;
                break;

            case Step::OnlyLevel:
                session.note("test level");
                step_ = Step::OnlyLevelDone;
                stack.push(std::make_unique<LevelScreen>(services_, game_, onlyFile_.c_str(), onlyKind_, won_));
                return Frame::Continue;

            case Step::OnlyLevelDone:
                game_.finishCampaign();
                step_ = Step::Done;
                break;

            // Every part of bb.lst.
            case Step::Parts: {
                if (!(part_ < game_.partCount() && won_ && !session.quitRequested())) {
                    step_ = Step::AfterParts;
                    break;
                }

                const int kind = game_.partKind(part_);
                if (kind == Game::kLevel) {
                    step_ = Step::AfterLevel;
                    stack.push(std::make_unique<LevelScreen>(services_, game_, game_.partFile(part_),
                                                             game_.partLevelKind(part_), won_));
                    return Frame::Continue;
                }
                if (kind == Game::kBriefing) {
                    game_.showBriefing(game_.partFile(part_));
                    part_ += 1;
                    return Frame::Present;
                }

                step_ = Step::AfterPart;
                if (cutscene_.begin(game_.partFile(part_))) {
                    stack.push(std::make_unique<CutsceneScreen>(cutscene_));
                    return Frame::Continue;
                }
                break;
            }

            case Step::AfterLevel:
                game_.levelPlayed();
                part_ += 1;
                step_ = Step::Parts;
                break;

            case Step::AfterPart:
                part_ += 1;
                step_ = Step::Parts;
                break;

            case Step::AfterParts:
                step_ = (won_ && !session.quitRequested()) ? Step::EndingScene : Step::Release;
                break;

            // The ending: cutscene, "the end", credits, the last picture and
            // the summary of the last level.
            case Step::EndingScene:
                ending_ = std::make_unique<Ending>(*services_.audio);
                loadSprites(*services_.assets, "data/theend.gfx", ending_->pictures, false);
                loadMusic(*services_.assets, "data/credits.mbk", ending_->credits);
                loadMusic(*services_.assets, "data/end.mbk", ending_->end, false);
                cutscene_.loadSprites("data/end.gfx");
                ending_->credits.play(bb::assets::music::credits::kHard);
                session.note("ending");

                step_ = Step::EndingPicture1;
                if (cutscene_.begin("data/end.cin")) {
                    stack.push(std::make_unique<CutsceneScreen>(cutscene_));
                    return Frame::Continue;
                }
                break;

            case Step::EndingPicture1:
                ending_->credits.stopAll();
                core::gfx::draw(screen, *ending_->pictures[1], 0, 0);
                step_ = Step::EndingPicture2;
                return Frame::Present;

            case Step::EndingPicture2:
                core::gfx::draw(screen, *ending_->pictures[1], 0, 0);
                step_ = Step::EndingKey;
                return Frame::Present;

            case Step::EndingKey:
                if (!services_.input->anyPressed()) {
                    ending_->end.play(bb::assets::music::end::kOrgan);
                }
                step_ = Step::EndingCredits;
                pushKeyPress(stack);
                return Frame::Continue;

            case Step::EndingCredits:
                ending_->end.stopAll();
                ending_->credits.play(bb::assets::music::credits::kCredits);
                step_ = Step::EndingLast1;
                stack.push(std::make_unique<CreditsScreen>(services_, true));
                return Frame::Continue;

            case Step::EndingLast1:
                ending_->end.clear();
                core::gfx::draw(screen, *ending_->pictures[0], 0, 0);
                step_ = Step::EndingLast2;
                return Frame::Present;

            case Step::EndingLast2:
                core::gfx::draw(screen, *ending_->pictures[0], 0, 0);
                step_ = Step::EndingLastKey;
                return Frame::Present;

            case Step::EndingLastKey:
                ending_->pictures.clear();
                step_ = Step::EndingSummary;
                pushKeyPress(stack);
                return Frame::Continue;

            case Step::EndingSummary:
                ending_->credits.stopAll();
                ending_->credits.clear();
                step_ = Step::EndingDone;
                stack.push(game_.summaryScreen());
                return Frame::Continue;

            case Step::EndingDone:
                ending_.reset();
                step_ = Step::Release;
                break;

            // Game over and hi-scores.
            case Step::Release:
                game_.finishCampaign();
                step_ = (!game_.campaignLeft() && !session.quitRequested()) ? Step::GameOver : Step::Done;
                break;

            case Step::GameOver:
                step_ = Step::HiScore;
                hiScorePlayer_ = 1;
                if (!won_) {
                    stack.push(std::make_unique<GameOverScreen>(services_));
                    return Frame::Continue;
                }
                break;

            case Step::HiScore: {
                const int number = hiScorePlayer_++;
                if (hiScorePlayer_ > kMaxPlayers) {
                    step_ = Step::Table;
                }
                const Player* p = game_.player(number);
                if (p != nullptr && services_.hiScores->qualifies(p->score)) {
                    stack.push(std::make_unique<HiScoreEntryScreen>(services_, p->score, number));
                    return Frame::Continue;
                }
                break;
            }

            case Step::Table:
                step_ = Step::Done;
                stack.push(std::make_unique<HiScoreTableScreen>(services_));
                return Frame::Continue;

            case Step::Done:
                game_.deletePlayers();
                stack.finish();
                return Frame::Continue;
        }
    }
}

//-----------------------------------------------------------------------------
// LevelScreen
//-----------------------------------------------------------------------------

bool LevelScreen::runTick(ScreenStack& stack) {
    switch (tick_) {
        case TickStep::Begin:
            game_.tickBegin();
            tick_ = TickStep::Middle;
            if (game_.pauseRequested()) {
                paused_ = true;
                stack.push(std::make_unique<PauseScreen>(services_, game_, game_.levelLeftFlag()));
                return false;
            }
            [[fallthrough]];

        case TickStep::Middle:
            if (paused_) {
                paused_ = false;
                game_.resetTimeBase();
            }
            tick_ = TickStep::End;
            if (services_.world->dialogueToPlay != -1) {
                inDialogue_ = true;
                stack.push(std::make_unique<DialogueScreen>(services_, game_));
                return false;
            }
            [[fallthrough]];

        case TickStep::End:
            if (inDialogue_) {
                inDialogue_ = false;
                game_.resetTimeBase();
            }
            game_.tickEnd();
            tick_ = TickStep::Begin;
            return true;
    }
    return true;
}

Frame LevelScreen::update(ScreenStack& stack) {
    for (;;) {
        switch (step_) {
            case Step::Start:
                if (!game_.startLevel(file_.c_str(), kind_)) {
                    won_ = false;
                    stack.finish();
                    return Frame::Continue;
                }
                step_ = Step::Briefing;
                stack.push(std::make_unique<DelayScreen>(services_, 2000));
                return Frame::Continue;

            case Step::Briefing:
                if (game_.hasBriefing()) {
                    game_.drawBriefingPrompt();
                    step_ = Step::BriefingKey;
                    return Frame::Present;
                }
                step_ = Step::FirstEvents;
                break;

            case Step::BriefingKey:
                services_.banks.jingles->play(bb::assets::music::inter::kDrumRoll);
                step_ = Step::BriefingClose;
                stack.push(std::make_unique<WaitKeyScreen>(services_, key_));
                stack.push(std::make_unique<WaitReleaseScreen>(services_));
                return Frame::Continue;

            case Step::BriefingClose:
                game_.closeBriefing();
                step_ = Step::FirstEvents;
                break;

            case Step::FirstEvents:
                game_.updateEvents();
                step_ = Step::FirstTick;
                break;

            case Step::FirstTick:
                if (!runTick(stack)) {
                    return Frame::Continue;
                }
                game_.startLevelLoop();
                step_ = Step::Frame;
                break;

            case Step::Frame:
                if (framePending_) {
                    // The rest of the 2002 loop iteration, after the frame
                    // was presented.
                    framePending_ = false;
                    game_.updateVictoryAndDefeat();
                }
                if (!game_.levelRunning()) {
                    step_ = Step::End;
                    break;
                }
                game_.beginFrame();
                step_ = Step::Ticks;
                break;

            case Step::Ticks:
                while (tick_ != TickStep::Begin || game_.frameNeedsTick()) {
                    if (!runTick(stack)) {
                        return Frame::Continue;
                    }
                    game_.frameTickDone();
                }
                game_.drawFrame();
                framePending_ = true;
                step_ = Step::Frame;
                return Frame::Present;

            case Step::End:
                game_.finishLevelLoop();
                if (game_.levelEndsWithPicture()) {
                    game_.drawAll(false);
                    step_ = Step::EndPicture;
                    return Frame::Present;
                }
                step_ = Step::Summary;
                break;

            case Step::EndPicture:
                game_.drawAll(false);
                step_ = Step::Summary;
                break;

            case Step::Summary: {
                step_ = Step::Loading;
                std::unique_ptr<Screen> summary = game_.levelSummary();
                if (summary) {
                    stack.push(std::move(summary));
                    return Frame::Continue;
                }
                break;
            }

            case Step::Loading:
                game_.drawAll(false);
                drawLoading(services_);
                step_ = Step::Release;
                return Frame::Present;

            case Step::Release:
                game_.releaseLevel();
                won_ = game_.levelWon();
                stack.finish();
                return Frame::Continue;
        }
    }
}

//-----------------------------------------------------------------------------
// PauseScreen
//-----------------------------------------------------------------------------

Frame PauseScreen::update(ScreenStack& stack) {
    Session& session = *services_.session;

    switch (step_) {
        case Step::Enter1:
            // The level twice (the 2002 game drew into both buffers of a
            // page flip), then the menu over it.
            game_.drawAll(false);
            step_ = Step::Enter2;
            return Frame::Present;

        case Step::Enter2:
            game_.drawAll(false);
            step_ = Step::Enter3;
            return Frame::Present;

        case Step::Enter3:
            session.note("pause menu");
            menu_.start();
            menu_.draw();
            step_ = Step::WaitRelease;
            return Frame::Present;

        case Step::WaitRelease:
            step_ = Step::Menu;
            stack.push(std::make_unique<WaitReleaseScreen>(services_));
            return Frame::Continue;

        case Step::Menu: {
            int result = 0;
            if (!menu_.update(result)) {
                pushMenuWait(services_, stack, menu_.wait(), menu_.waitedKey());
                return Frame::Continue;
            }
            if (result == PauseMenu::kStay && !session.quitRequested()) {
                game_.drawAll(false);
                menu_.draw();
                return Frame::Present;
            }
            if (result == PauseMenu::kLeaveLevel) {
                levelLeft_ = true;
            }
            session.note(result == PauseMenu::kLeaveLevel ? "pause menu: exit level" : "pause menu: resume");
            menu_.stop();
            game_.drawAll(false);
            step_ = Step::Leave2;
            return Frame::Present;
        }

        case Step::Leave2:
            break;
    }

    game_.drawAll(false);
    stack.finish();
    return Frame::Present;
}

//-----------------------------------------------------------------------------
// DialogueScreen
//-----------------------------------------------------------------------------

Frame DialogueScreen::update(ScreenStack& stack) {
    Session& session = *services_.session;
    World& w = *services_.world;
    DialoguePlayer& player = game_.dialoguePlayer();

    switch (step_) {
        case Step::Start: {
            player.begin(w.dialogueToPlay);
            char note[64];
            std::snprintf(note, sizeof(note), "dialogue %d", w.dialogueToPlay);
            session.note(note);
            step_ = Step::Scene;
        }
            [[fallthrough]];

        case Step::Scene: {
            if (!continued_ || session.quitRequested()) {
                step_ = Step::End;
                return update(stack);
            }
            bool escaped = false;
            game_.drawAll(false);
            continued_ = player.draw(escaped);
            if (escaped) {
                step_ = Step::Escaped;
                stack.push(std::make_unique<WaitReleaseScreen>(services_));
                return Frame::Continue;
            }
            return Frame::Present;
        }

        case Step::Escaped:
            // The level drawn before the wait, without the dialogue.
            player.skip();
            step_ = Step::Scene;
            return Frame::Present;

        case Step::End:
            player.end();
            step_ = Step::Done;
            stack.push(std::make_unique<WaitReleaseScreen>(services_));
            return Frame::Continue;

        case Step::Done:
            break;
    }

    session.note("dialogue ends");
    w.dialogueToPlay = -1;
    stack.finish();
    return Frame::Continue;
}

}  // namespace bb::game
