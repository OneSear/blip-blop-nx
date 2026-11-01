// The hero choice, where the players join (in 2002, the character choice
// of screens/character_select_screen.cpp, Game::selectPlayer(): Blip or
// Blop for player 1), and the "LOADING" text.

#include <cstdio>
#include <cstdlib>
#include <string>

#include "bb/assets/sprites/inter.hpp"
#include "bb/core/input/bindings.hpp"
#include "bb/core/input/codes.hpp"
#include "bb/game/screens.hpp"
#include "bb/game/world.hpp"

namespace bb::game {

namespace {

namespace img = bb::assets::sprites::inter;
namespace input = core::input;
namespace gfx = core::gfx;

constexpr int kLoadingText = 851;  // "LOADING"
constexpr int kPressText = 863;    // "Press +": a player who has not joined
constexpr int kReadyText = 864;    // "READY!": a player who took a hero
constexpr int kButtonsText = 865;  // "A = choose   B = back   X = controllers"
constexpr int kTagText = 866;      // "%dP": a player's tag

const char* const kHeroNames[HeroChoice::kHeroes] = {"BLIP", "BLOP", "BLEACH", "BLITZ"};

// The players' colours: red, blue, yellow, green.
const gfx::Pixel kPlayerColours[kMaxPlayers] = {gfx::makePixel(214, 41, 41), gfx::makePixel(41, 98, 230),
                                                gfx::makePixel(230, 197, 25), gfx::makePixel(41, 164, 57)};

// The heroes: a column of 160 pixels each; in it the hero's card (its
// portrait and its name, the frame of the player whose tag is on it) under
// a row of tags, one place for each player.
constexpr int kColumnWidth = 160;
constexpr int kCardWidth = 148;
constexpr int kCardTop = 96;
constexpr int kCardHeight = 160;
constexpr int kTagTop = 74;
constexpr int kTagWidth = 35;
constexpr int kTagHeight = 19;
// The players' panels, and the line of buttons.
constexpr int kPanelTop = 300;
constexpr int kPanelTagWidth = 54;
constexpr int kPanelTagHeight = 30;
constexpr int kButtonsY = 440;
// The height of the fonts' glyphs.
constexpr int kMenuHeight = 24;
constexpr int kRpgHeight = 16;

int columnX(int i) {
    return kColumnWidth / 2 + i * kColumnWidth;
}

// The pad a player plays with: the one its controls are bound to, else
// pad 1 for player 1, pad 2 for player 2 ... (the default pad layout).
int padOf(const Input& in, int player) {
    const int first = AliasControls::firstOf(player);
    for (int i = 0; i < input::kActionCount; ++i) {
        const int slot = input::padSlot(static_cast<input::Code>(in.alias(first + i)));
        if (slot >= 0) {
            return slot;
        }
    }
    return player;
}

void frame(gfx::Pixmap& screen, const gfx::Rect& r, gfx::Pixel colour, int thickness) {
    const gfx::Rect sides[] = {{r.x, r.y, r.w, thickness},
                               {r.x, r.y + r.h - thickness, r.w, thickness},
                               {r.x, r.y, thickness, r.h},
                               {r.x + r.w - thickness, r.y, thickness, r.h}};
    for (const gfx::Rect& side : sides) {
        gfx::fill(screen, &side, colour);
    }
}

// A player's tag: "1P" ("J1" in French).
std::string tagText(const TextSource& texts, int player) {
    char tag[16];
    std::snprintf(tag, sizeof(tag), texts.text(kTagText), player + 1);  // NOLINT(cert-err33-c)
    return tag;
}

// A player's tag on a box of its colour, in a font whose glyphs are
// `textHeight` pixels high.
void drawTag(gfx::Pixmap& screen, const gfx::BitmapFont& font, int textHeight, const std::string& tag,
             int player, const gfx::Rect& box) {
    gfx::fill(screen, &box, kPlayerColours[player]);
    frame(screen, box, gfx::makePixel(0, 0, 0), 1);
    font.printCentred(screen, box.x + box.w / 2, box.y + (box.h - textHeight) / 2, tag.c_str());
}

// Text centred at x: on one line if it fits in `width` pixels, else on two,
// cut at the space nearest to its middle ("Appuyez" over "sur +").
void printLines(gfx::Pixmap& screen, const gfx::BitmapFont& font, int x, int y, int width, int lineHeight,
                const std::string& text) {
    std::string::size_type cut = std::string::npos;
    if (font.width(text.c_str()) > width) {
        const auto middle = static_cast<long>(text.size() / 2);
        for (auto space = text.find(' '); space != std::string::npos; space = text.find(' ', space + 1)) {
            if (cut == std::string::npos ||
                std::labs(static_cast<long>(space) - middle) < std::labs(static_cast<long>(cut) - middle)) {
                cut = space;
            }
        }
    }
    if (cut == std::string::npos) {
        font.printCentred(screen, x, y, text.c_str());
        return;
    }
    font.printCentred(screen, x, y, text.substr(0, cut).c_str());
    font.printCentred(screen, x, y + lineHeight, text.substr(cut + 1).c_str());
}

}  // namespace

void drawLoading(Services& services) {
    services.fonts->cool->printCentred(*services.screen, 320, 205, services.texts->text(kLoadingText));
}

CharacterSelectScreen::Keys CharacterSelectScreen::keys(int player) const {
    const Input& in = *services_.input;
    const int first = AliasControls::firstOf(player);
    const int pad = padOf(in, player);
    const auto button = [&](input::Code b) { return in.held(input::padCode(pad, b)); };
    const bool firstPlayer = player == 0;

    Keys k;
    k.left = in.aliasHeld(first + AliasControls::kLeft) || (firstPlayer && in.held(input::kKeyLeft));
    k.right = in.aliasHeld(first + AliasControls::kRight) || (firstPlayer && in.held(input::kKeyRight));
    k.take = in.aliasHeld(first + AliasControls::kFire) || button(input::kPadButtonB) ||
             button(input::kPadButtonStart) || (firstPlayer && in.keyHeld(input::kKeyReturn));
    k.join = k.take || button(input::kPadButtonBack);
    k.back = in.aliasHeld(first + AliasControls::kJump) || button(input::kPadButtonA);
    k.applet = button(input::kPadButtonY);
    return k;
}

bool CharacterSelectScreen::apply(const Keys* now, bool escape) {
    using Seat = HeroChoice::Seat;
    bool leave = escape && !escapeBefore_;
    bool applet = false;
    for (int p = 0; p < kMaxPlayers; ++p) {
        const Keys& k = now[p];
        const Keys& b = before_[p];
        applet = applet || (k.applet && !b.applet);
        switch (choice_.seat(p)) {
            case Seat::Out:
                if (k.join && !b.join) {
                    choice_.join(p);
                } else if (k.back && !b.back && choice_.joined() == 0) {
                    leave = true;
                }
                break;
            case Seat::Choosing:
                if (k.back && !b.back) {
                    choice_.back(p);
                } else if (k.take && !b.take) {
                    choice_.take(p);
                } else if (k.left && !b.left) {
                    choice_.move(p, -1);
                } else if (k.right && !b.right) {
                    choice_.move(p, 1);
                }
                break;
            case Seat::Ready:
                if (k.back && !b.back) {
                    choice_.back(p);
                }
                break;
        }
    }
    if (applet && !leave) {
        // Blocks until it closes; the players keep their places.
        services_.input->requestPlayers(kMaxPlayers);
    }
    return leave;
}

Frame CharacterSelectScreen::update(ScreenStack& stack) {
    Input& in = *services_.input;

    if (services_.session->quitRequested()) {
        lineup_ = Lineup{};
        stack.finish();
        return Frame::Continue;
    }

    in.update();
    Keys now[kMaxPlayers];
    for (int p = 0; p < kMaxPlayers; ++p) {
        now[p] = keys(p);
    }
    const bool escape = in.keyHeld(input::kKeyEscape);

    if (!started_) {
        // What is held when the screen opens does nothing.
        started_ = true;
        lineup_ = Lineup{};
        services_.session->note("character select");
        for (int p = 0; p < kMaxPlayers; ++p) {
            before_[p] = now[p];
        }
        escapeBefore_ = escape;
    }

    const bool leave = apply(now, escape);
    for (int p = 0; p < kMaxPlayers; ++p) {
        before_[p] = now[p];
    }
    escapeBefore_ = escape;
    if (leave) {
        services_.session->note("character select: back");
        lineup_ = Lineup{};
        stack.finish();
        return Frame::Continue;
    }

    frames_ += 1;
    background1X_ -= 20;
    background2X_ -= 20;
    if (background1X_ == -640) {
        background1X_ = 640;
    }
    if (background2X_ == -640) {
        background2X_ = 640;
    }
    if (titleY_ < 40) {
        titleY_ += 10;
    }

    // The start: kStartDelay frames after everyone who joined took a hero;
    // a player who joins or gives a hero back calls it off.
    if (!choice_.everyoneReady()) {
        countdown_ = -1;
    } else if (countdown_ < 0) {
        countdown_ = kStartDelay;
    } else if (--countdown_ == 0) {
        lineup_ = choice_.lineup();
        draw();
        drawLoading(services_);
        stack.finish();
        return Frame::Present;
    }

    draw();
    return Frame::Present;
}

void CharacterSelectScreen::draw() {
    gfx::Pixmap& screen = *services_.screen;
    const ImageBank& images = *services_.banks.interface;
    const bool french = services_.options->language == input::Language::French;

    gfx::draw(screen, *images[img::kStarfield], background1X_, 0);
    gfx::draw(screen, *images[img::kStarfield + 1], background2X_, 0);
    gfx::draw(screen, *images[french ? img::kSelectTitleFr : img::kSelectTitle], 320, titleY_);

    for (int hero = 0; hero < HeroChoice::kHeroes; ++hero) {
        drawHero(hero);
    }
    for (int player = 0; player < kMaxPlayers; ++player) {
        drawPanel(player);
    }
    services_.fonts->rpg->printCentred(screen, 320, kButtonsY, services_.texts->text(kButtonsText));
}

void CharacterSelectScreen::drawHero(int hero) {
    using Seat = HeroChoice::Seat;
    gfx::Pixmap& screen = *services_.screen;
    const ImageBank& images = *services_.banks.interface;
    const Fonts& fonts = *services_.fonts;
    const int x = columnX(hero);
    const bool blinkOn = frames_ / 10 % 2 == 0;

    const gfx::Rect card{x - kCardWidth / 2, kCardTop, kCardWidth, kCardHeight};
    gfx::darken(screen, &card);

    // The frame: the colour of the player who took the hero, or, blinking,
    // of the first player whose tag is on it.
    const int holder = choice_.holder(hero);
    int framed = holder;
    if (framed < 0 && blinkOn) {
        for (int p = 0; p < kMaxPlayers && framed < 0; ++p) {
            if (choice_.seat(p) == Seat::Choosing && choice_.cursor(p) == hero) {
                framed = p;
            }
        }
    }
    gfx::draw(screen, *images[img::kPortraitBlip + hero], x, kCardTop + 64);
    const gfx::BitmapFont& nameFont = holder >= 0 ? *fonts.menus : *fonts.menu;
    nameFont.printCentred(screen, x, kCardTop + kCardHeight - 36, kHeroNames[hero]);
    if (framed >= 0) {
        frame(screen, card, kPlayerColours[framed], holder >= 0 ? 4 : 2);
    }

    // The tags of the players on it, each in its own place.
    for (int p = 0; p < kMaxPlayers; ++p) {
        if (choice_.seat(p) != Seat::Out && choice_.cursor(p) == hero) {
            const gfx::Rect box{x - kCardWidth / 2 + p * (kTagWidth + 2) + 1, kTagTop, kTagWidth, kTagHeight};
            drawTag(screen, *fonts.rpg, kRpgHeight, tagText(*services_.texts, p), p, box);
        }
    }
}

void CharacterSelectScreen::drawPanel(int player) {
    using Seat = HeroChoice::Seat;
    gfx::Pixmap& screen = *services_.screen;
    const Fonts& fonts = *services_.fonts;
    const TextSource& texts = *services_.texts;
    const int x = columnX(player);
    const bool blinkOn = frames_ / 20 % 2 == 0;

    const gfx::Rect box{x - kPanelTagWidth / 2, kPanelTop, kPanelTagWidth, kPanelTagHeight};
    const std::string tag = tagText(texts, player);
    const int y = kPanelTop + kPanelTagHeight + 12;
    switch (choice_.seat(player)) {
        case Seat::Out:
            gfx::darken(screen, &box);
            fonts.menu->printCentred(screen, x, box.y + (box.h - kMenuHeight) / 2, tag.c_str());
            if (blinkOn) {
                printLines(screen, *fonts.rpg, x, y, kColumnWidth - 30, kRpgHeight + 2,
                           texts.text(kPressText));
            }
            break;
        case Seat::Choosing: {
            drawTag(screen, *fonts.menu, kMenuHeight, tag, player, box);
            const std::string name = std::string("< ") + kHeroNames[choice_.cursor(player)] + " >";
            fonts.rpg->printCentred(screen, x, y, name.c_str());
            break;
        }
        case Seat::Ready:
            drawTag(screen, *fonts.menu, kMenuHeight, tag, player, box);
            fonts.menus->printCentred(screen, x, y, texts.text(kReadyText));
            break;
    }
}

}  // namespace bb::game
