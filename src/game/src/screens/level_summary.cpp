// The summary at the end of a level (screens/level_summary_screen.cpp, the
// 2002 Game::showPE()).

#include <cstdio>

#include "bb/assets/music/inter.hpp"
#include "bb/core/gfx/sprite.hpp"
#include "bb/game/player.hpp"
#include "bb/game/screens.hpp"
#include "bb/game/waits.hpp"
#include "bb/game/world.hpp"

namespace bb::game {

namespace {

enum Text {
    kBonusFailedText = 840,    // "BONUS STAGE FAILED!"
    kLevelCompleteText = 841,  // "LEVEL COMPLETE!"
    kLifeUpText = 842,         // "LIFE UP"
    kPerfectText = 843,
    kObjectiveText = 844,
    kCasualtiesText = 845,
    kPercentText = 846,
    kTimeLeftText = 847,
    kBonusText = 848,
    kScoreText = 849,
};

constexpr int kBonus = 50000;  // the perfect and the objective bonuses
constexpr int kDone = 4;       // counting stage: done

// The table of the four-player mode: the labels from x 20, a column of 120
// pixels per player from x 150, its numbers ending 10 pixels before the
// next.
constexpr int kTableLabels = 20;
constexpr int kTableFirstColumn = 150;
constexpr int kTableColumn = 120;

// The summary is laid out for the 640-pixel screen: on the wider one of a
// level shown in 16:9, it is moved to the middle by this much.
int centring(const core::gfx::Pixmap& screen) {
    return (screen.width() - core::gfx::kScreenWidth) / 2;
}

}  // namespace

LevelSummaryScreen::LevelSummaryScreen(Services& services, Player* const* players, bool playersDead,
                                       bool bonusLevel, bool bonusFailed) noexcept
    : services_(services), playersDead_(playersDead), bonusLevel_(bonusLevel), bonusFailed_(bonusFailed) {
    for (int i = 0; i < kMaxPlayers; ++i) {
        tally_[i].player = players[i];
    }
    table_ = players[2] != nullptr || players[3] != nullptr;
}

void LevelSummaryScreen::start() {
    World& w = *services_.world;
    Tally& t1 = tally_[0];
    Tally& t2 = tally_[1];

    // The band, darkened once, kept in the backdrop (the whole width for
    // the table).
    core::gfx::blit(*services_.backdrop, 0, 0, *services_.screen);
    const int dx = centring(*services_.screen);
    const core::gfx::Rect band =
        table_ ? core::gfx::Rect{10 + dx, 100, 620, 300} : core::gfx::Rect{60 + dx, 100, 520, 300};
    core::gfx::darken(*services_.backdrop, &band);

    services_.session->note(bonusFailed_ ? "level summary (bonus failed)" : "level summary");
    services_.banks.jingles->play(bb::assets::music::inter::kEndLevel);

    for (int i = 2; i < kMaxPlayers; ++i) {
        tally_[i].stage = kDone;  // shown below in the table only
    }
    if (table_) {
        // The players still in the level, a column each, in their order.
        int column = 0;
        for (Tally& t : tally_) {
            bool playing = false;
            for (w.players.rewind(); !w.players.atEnd(); w.players.next()) {
                playing = playing || w.players.current() == t.player;
            }
            t.shown = t.player != nullptr && playing;
            t.stage = t.shown ? 0 : kDone;
            t.x = dx + kTableFirstColumn + column * kTableColumn;
            column += t.shown ? 1 : 0;
        }
    } else if (w.players.count() == 2) {
        t1.shown = t2.shown = true;
        t1.x = dx + 80;
        t2.x = dx + 350;
    } else if (t1.player != nullptr && t1.player->lives > 0) {
        t1.shown = true;
        t1.x = dx + 210;
        t2.stage = kDone;
    } else if (t2.player != nullptr) {
        t2.shown = true;
        t2.x = dx + 210;
        t1.stage = kDone;
    }

    const bool objective = !bonusLevel_ || (w.flags[Flag::Timer] > 0 && !playersDead_);
    for (Tally& t : tally_) {
        if (!t.shown) {
            continue;
        }
        t.font = services_.fonts->score(static_cast<int>(t.player->hero));
        if (w.enemiesCreated > 0) {
            t.killedPercent = (100 * t.player->kills) / w.enemiesCreated;
        }
        if (objective) {
            t.objective = kBonus;
        }
        if (t.objective > 0 && t.player->perfect) {
            t.perfect = kBonus;
        }
        t.score = t.player->score;
    }

    if (bonusFailed_) {
        for (Tally& t : tally_) {
            t.perfect = t.objective = t.killedPercent = 0;
        }
        w.flags[Flag::Timer] = 0;
    }
}

void LevelSummaryScreen::count(Tally& t) {
    const int timeLeft = services_.world->flags[Flag::Timer];

    // (As in 2002 the perfect and objective steps are followed, in the same
    // step, by the casualties one when they end.)
    if (t.stage == 0) {
        if (t.perfectShown >= t.perfect) {
            t.stage = 1;
        } else {
            t.perfectShown += 1000;
            t.totalBonus += 1000;
            t.score += 1000;
        }
    } else if (t.stage == 1) {
        if (t.objectiveShown >= t.objective) {
            t.stage = 2;
        } else {
            t.objectiveShown += 1000;
            t.totalBonus += 1000;
            t.score += 1000;
        }
    }
    if (t.stage == 2) {
        if (t.killedShown >= t.killedPercent) {
            t.stage = 3;
        } else {
            t.killedShown += 1;
            t.totalBonus += 500;
            t.score += 500;
        }
    } else if (t.stage == 3) {
        if (t.timeShown >= timeLeft) {
            t.stage = kDone;
        } else {
            t.timeShown += 1;
            t.totalBonus += 1000;
            t.score += 1000;
        }
    }
}

void LevelSummaryScreen::drawLabels() {
    core::gfx::Pixmap& screen = *services_.screen;
    const TextSource& texts = *services_.texts;
    const core::gfx::BitmapFont& rpg = *services_.fonts->rpg;
    const int labels = centring(screen) + kTableLabels;
    bool perfect = false;
    bool objective = false;
    for (const Tally& t : tally_) {
        perfect = perfect || (t.shown && t.perfect > 0);
        objective = objective || (t.shown && t.objective > 0);
    }
    if (perfect) {
        rpg.print(screen, labels, 180, texts.text(kPerfectText));
    }
    if (objective) {
        rpg.print(screen, labels, 200, texts.text(kObjectiveText));
    }
    rpg.print(screen, labels, 220, texts.text(kCasualtiesText));
    if (services_.world->flags[Flag::Timer] > 0) {
        rpg.print(screen, labels, 240, texts.text(kTimeLeftText));
    }
    rpg.print(screen, labels, 300, texts.text(kBonusText));
    rpg.print(screen, labels, 320, texts.text(kScoreText));
}

void LevelSummaryScreen::drawColumn(Tally& t) {
    core::gfx::Pixmap& screen = *services_.screen;
    const TextSource& texts = *services_.texts;
    const core::gfx::BitmapFont& rpg = *services_.fonts->rpg;
    char buffer[20];
    const int right = t.x + kTableColumn - 10;

    const auto number = [&](int y, int value, int end) {
        std::snprintf(buffer, sizeof(buffer), "%d", value);
        t.font->printRight(screen, end, y, buffer);
    };

    if (t.lifeUp > 0) {
        rpg.printCentred(screen, t.x + kTableColumn / 2, 270, texts.text(kLifeUpText));
    }
    if (t.perfect > 0) {
        number(180, t.perfectShown, right);
    }
    if (t.objective > 0) {
        number(200, t.objectiveShown, right);
    }
    number(220, t.killedShown, right - 10);
    rpg.printRight(screen, right, 220, texts.text(kPercentText));
    if (services_.world->flags[Flag::Timer] > 0) {
        number(240, t.timeShown, right);
    }
    number(300, t.totalBonus, right);
    number(320, t.score, right);

    // A life every 500,000 points.
    if (t.score / 500000 > t.player->bonusLives) {
        t.player->bonusLives = t.score / 500000;
        t.player->lives++;
        t.lifeUp = 100;
    }
}

void LevelSummaryScreen::draw(Tally& t) {
    core::gfx::Pixmap& screen = *services_.screen;
    const TextSource& texts = *services_.texts;
    const core::gfx::BitmapFont& rpg = *services_.fonts->rpg;
    char buffer[20];

    // A label on the left, a number in the player's font on the right.
    const auto line = [&](int y, int text, int value) {
        rpg.print(screen, t.x, y, texts.text(text));
        std::snprintf(buffer, sizeof(buffer), "%d", value);
        t.font->printRight(screen, t.x + 210, y, buffer);
    };

    if (t.lifeUp > 0) {
        rpg.printCentred(screen, t.x + 105, 270, texts.text(kLifeUpText));
    }
    if (t.perfect > 0) {
        line(180, kPerfectText, t.perfectShown);
    }
    if (t.objective > 0) {
        line(200, kObjectiveText, t.objectiveShown);
    }
    line(220, kCasualtiesText, t.killedShown);
    rpg.printRight(screen, t.x + 220, 220, texts.text(kPercentText));
    if (services_.world->flags[Flag::Timer] > 0) {
        line(240, kTimeLeftText, t.timeShown);
    }
    line(300, kBonusText, t.totalBonus);
    line(320, kScoreText, t.score);

    // A life every 500,000 points.
    if (t.score / 500000 > t.player->bonusLives) {
        t.player->bonusLives = t.score / 500000;
        t.player->lives++;
        t.lifeUp = 100;
    }
}

Frame LevelSummaryScreen::update(ScreenStack& stack) {
    Input& in = *services_.input;
    const bool quitting = services_.session->quitRequested();

    if (step_ == Step::Start) {
        start();
        step_ = Step::Counting;
    }

    if (step_ == Step::Counting) {
        bool counting = false;
        for (const Tally& t : tally_) {
            counting = counting || t.stage < kDone;
        }
        if (!quitting && counting) {
            in.update();

            // A step every 3 frames, every frame while a key is held.
            delay_ += 1;
            delay_ %= 3;
            if (in.anyPressed()) {
                delay_ = 0;
            }
            if (delay_ == 0) {
                for (Tally& t : tally_) {
                    count(t);
                }
            }

            core::gfx::blit(*services_.screen, 0, 0, *services_.backdrop);
            services_.fonts->cool->printCentred(
                *services_.screen, services_.screen->width() / 2, 120,
                services_.texts->text(bonusFailed_ ? kBonusFailedText : kLevelCompleteText));
            if (table_) {
                drawLabels();
            }
            for (Tally& t : tally_) {
                if (t.shown) {
                    if (table_) {
                        drawColumn(t);
                    } else {
                        draw(t);
                    }
                }
            }
            return Frame::Present;
        }

        step_ = Step::Waited;
        if (!quitting) {
            // Waits for the release, then for a key.
            stack.push(std::make_unique<WaitKeyScreen>(services_, key_));
            stack.push(std::make_unique<WaitReleaseScreen>(services_));
            return Frame::Continue;
        }
    }

    for (Tally& t : tally_) {
        if (t.shown) {
            t.player->score = t.score;
        }
    }
    services_.banks.jingles->stopAll();
    stack.finish();
    return Frame::Continue;
}

}  // namespace bb::game
