// The screens of the attract loop (screens/attract_screens.cpp, the 2002
// Game::showMainScreen(), showHighScores(), showGameOver() and getName()).

#include <cstdio>
#include <cstring>
#include <string>

#include "bb/assets/music/inter.hpp"
#include "bb/assets/sprites/inter.hpp"
#include "bb/core/input/codes.hpp"
#include "bb/core/input/names.hpp"
#include "bb/game/hi_scores.hpp"
#include "bb/game/screens.hpp"
#include "bb/game/waits.hpp"

namespace bb::game {

namespace {

namespace img = bb::assets::sprites::inter;
namespace input = core::input;

constexpr int kHighScoreText = 854;  // "PLAYER %d GOT A HIGH SCORE!"
constexpr int kEnterNameText = 855;  // "PLEASE ENTER YOUR NAME :"

std::uint32_t milliseconds(const Services& services) {
    return static_cast<std::uint32_t>(services.session->nowMicroseconds() / 1000);
}

void drawImage(Services& services, int image) {
    core::gfx::draw(*services.screen, *(*services.banks.interface)[image], 0, 0);
}

}  // namespace

//-----------------------------------------------------------------------------
// TitleScreen
//-----------------------------------------------------------------------------

Frame TitleScreen::update(ScreenStack& stack) {
    if (!started_) {
        started_ = true;
        start_ = milliseconds(services_);
        services_.session->note("title screen");
    }

    if (!services_.input->anyPressed() && !services_.session->quitRequested() &&
        milliseconds(services_) - start_ <= 5000) {
        services_.input->update();
        drawImage(services_, img::kTitle);
        return Frame::Present;
    }

    stack.finish();
    return Frame::Continue;
}

//-----------------------------------------------------------------------------
// HiScoreTableScreen
//-----------------------------------------------------------------------------

void HiScoreTableScreen::draw() {
    const core::gfx::BitmapFont& font = *services_.fonts->cool;
    const HiScores& table = *services_.hiScores;
    char buffer[50];

    for (int i = 0; i < HiScores::kCount; ++i) {
        std::snprintf(buffer, sizeof(buffer), "%d", table.score(i));
        font.printRight(*services_.screen, 300 - x_[i], 160 + 30 * i, buffer);
        std::snprintf(buffer, sizeof(buffer), "%s", table.name(i));
        font.print(*services_.screen, 320 + x_[i], 160 + 30 * i, buffer);
    }
}

Frame HiScoreTableScreen::update(ScreenStack& stack) {
    Session& session = *services_.session;

    if (step_ == Step::Start) {
        for (int i = 0; i < HiScores::kCount; ++i) {
            x_[i] = 400 + 160 * i;
        }
        session.note("hi-score table");
        start_ = milliseconds(services_);
        step_ = Step::Show;
    }

    if (step_ == Step::Show) {
        if (!session.quitRequested() && !services_.input->anyPressed() &&
            milliseconds(services_) - start_ <= 10000) {
            drawImage(services_, img::kTitle);
            for (int& x : x_) {
                if (x > 0) {
                    x -= 20;
                }
            }
            draw();
            return Frame::Present;
        }
        step_ = Step::SlideOut;
    }

    // Out to the right, each line once the one above has gone 160 pixels.
    if (!session.quitRequested() && x_[HiScores::kCount - 1] < 350) {
        drawImage(services_, img::kTitle);
        for (int i = 0; i < HiScores::kCount; ++i) {
            if (i == 0 || x_[i - 1] >= 160) {
                x_[i] += 20;
            }
        }
        draw();
        return Frame::Present;
    }

    stack.replace(std::make_unique<WaitReleaseScreen>(services_));
    return Frame::Continue;
}

//-----------------------------------------------------------------------------
// GameOverScreen
//-----------------------------------------------------------------------------

Frame GameOverScreen::update(ScreenStack& stack) {
    if (!started_) {
        started_ = true;
        services_.session->note("game over");
        services_.banks.jingles->play(bb::assets::music::inter::kGameOver);
    }

    // The 2002 "GAME OVER" text animation is commented out; the picture
    // stays until a key is pressed.
    if (!services_.session->quitRequested() && !services_.input->anyPressed()) {
        drawImage(services_, img::kGameOver);
        return Frame::Present;
    }

    services_.banks.jingles->stopAll();
    stack.finish();
    return Frame::Continue;
}

//-----------------------------------------------------------------------------
// HiScoreEntryScreen
//-----------------------------------------------------------------------------

HiScoreEntryScreen::HiScoreEntryScreen(Services& services, int score, int playerNumber)
    : services_(services), score_(score), playerNumber_(playerNumber) {
    // The title is a format string of the string table.
    const std::string format = services.texts->text(kHighScoreText);
    std::snprintf(title_, sizeof(title_), format.c_str(), playerNumber);  // NOLINT(cert-err33-c): 2002 format
}

void HiScoreEntryScreen::drawTitle() {
    const core::gfx::BitmapFont& font = *services_.fonts->menu;
    drawImage(services_, img::kTitle);
    font.printCentred(*services_.screen, 320 - x_, 160, title_);
    font.printCentred(*services_.screen, 320 + x_, 210, services_.texts->text(kEnterNameText));
}

Frame HiScoreEntryScreen::update(ScreenStack& stack) {
    const bool quitting = services_.session->quitRequested();
    const core::gfx::BitmapFont& font = *services_.fonts->menu;

    if (!noted_) {
        noted_ = true;
        services_.session->note(playerNumber_ == 1 ? "name entry: player 1" : "name entry: player 2");
    }

    switch (step_) {
        case Step::SlideIn:
            if (!quitting && x_ >= 40) {
                x_ -= 20;
                drawTitle();
                return Frame::Present;
            }
            step_ = Step::NextKey;
            [[fallthrough]];

        case Step::NextKey:
            // The system's on-screen keyboard where there is one (the
            // Switch): the whole name at once.
            if (!quitting && !keyboardAsked_) {
                keyboardAsked_ = true;
                if (services_.input->beginTextEntry()) {
                    step_ = Step::Keyboard;
                    return update(stack);
                }
            }
            if (!quitting && key_ != static_cast<int>(input::kKeyReturn)) {
                // Waits for the release, then for a key.
                step_ = Step::KeyTyped;
                stack.push(std::make_unique<WaitKeyScreen>(services_, typed_));
                stack.push(std::make_unique<WaitReleaseScreen>(services_));
                return Frame::Continue;
            }
            step_ = Step::SlideOut;
            return update(stack);

        case Step::KeyTyped: {
            key_ = static_cast<int>(typed_);
            // A pad's Start ends the entry (the pads cannot type Return).
            if (input::padSlot(typed_) >= 0 && input::padButton(typed_) == input::kPadButtonStart) {
                key_ = static_cast<int>(input::kKeyReturn);
            }
            const std::string keyName =
                input::displayName(static_cast<input::Code>(key_), input::Language::English);

            if (key_ == static_cast<int>(input::kKeySpace) && length_ < 19) {
                name_[length_++] = ' ';
                name_[length_] = '\0';
            }
            if (key_ == static_cast<int>(input::kKeyBackspace) && length_ > 0) {
                name_[--length_] = '\0';
            } else if (keyName.size() == 1 && length_ < 19) {
                name_[length_++] = keyName[0];
                name_[length_] = '\0';
            }

            drawTitle();
            font.printCentred(*services_.screen, 320, 260, name_);
            step_ = Step::NextKey;
            return Frame::Present;
        }

        case Step::Keyboard: {
            services_.input->update();
            std::string text;
            const Input::TextEntry entry = services_.input->textEntry(text);
            if (entry == Input::TextEntry::Typing && !quitting) {
                drawTitle();
                return Frame::Present;
            }
            if (entry == Input::TextEntry::Done) {
                setName(text);
            }
            step_ = Step::SlideOut;
            return update(stack);
        }

        case Step::SlideOut:
            if (!quitting && x_ < 640) {
                x_ += 20;
                drawTitle();
                font.printCentred(*services_.screen, 320, 260 + x_, name_);
                return Frame::Present;
            }
            break;
    }

    services_.hiScores->add(score_, name_);
    services_.session->saveHiScores();
    stack.finish();
    return Frame::Continue;
}

void HiScoreEntryScreen::setName(const std::string& text) {
    // The key-by-key entry types what the keys are named: capitals, digits,
    // a few signs. The same here, in the hi-score file's single bytes.
    const core::gfx::BitmapFont& font = *services_.fonts->menu;
    length_ = 0;
    for (const char c : text) {
        if (length_ >= 19) {
            break;
        }
        const auto u = static_cast<unsigned char>(c);
        const char upper = u >= 'a' && u <= 'z' ? static_cast<char>(u - 'a' + 'A') : c;
        if (u >= 0x20 && u < 0x7F && font.has(static_cast<unsigned char>(upper))) {
            name_[length_++] = upper;
        }
    }
    name_[length_] = '\0';
}

}  // namespace bb::game
