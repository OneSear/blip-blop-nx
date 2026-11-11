// The dialogue player (rpg_player.cpp).

#include "bb/game/dialogue.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "bb/core/assets/asset_source.hpp"
#include "bb/core/gfx/pixmap.hpp"
#include "bb/core/input/codes.hpp"
#include "bb/core/log.hpp"
#include "bb/game/player.hpp"
#include "bb/game/world.hpp"

namespace bb::game {

namespace {

// atoi() of 2002, without its undefined behaviour on overflow.
int toInt(const char* text) {
    return static_cast<int>(std::strtol(text, nullptr, 10));
}

}  // namespace

bool DialoguePlayer::begin(int scene) {
    std::string error;
    const auto text = services_.assets->script(script_, error);
    if (!text) {
        core::Log::warning("RPGPlayer::startPlay() -> Cannot open " + script_);
        return false;
    }

    text_.clear();
    text_.str(*text);

    char wanted[kLineLength];
    std::snprintf(wanted, sizeof(wanted), "rpg=%d", scene);
    text_.getline(value_, kLineLength);
    while (!text_.eof() && std::strcmp(wanted, value_) != 0) {
        text_.getline(value_, kLineLength);
    }
    if (text_.eof()) {
        core::Log::warning("RPGPlayer::startPlay() -> scene not found: " + std::to_string(scene));
        return false;
    }

    image_[kTop] = image_[kBottom] = -1;
    textId_[kTop] = textId_[kBottom] = -1;

    World& w = *services_.world;
    // The scripts are written for one or two heroes (ifnbj=2): with three
    // or four players, the first two in the list speak, with Blip's faces
    // for Blip and Bleach and Blop's for Blop and Blitz.
    playerCount_ = std::min(w.players.count(), 2);
    keyReleased_ = false;
    box_ = kTop;
    player_ = 0;
    waitUntil_ = 0;

    int i = 0;
    for (w.players.rewind(); !w.players.atEnd() && i < 2; w.players.next()) {
        const auto* p = static_cast<const Player*>(w.players.current());
        heroFirstImage_[i] = blipKind(p->hero) ? 0 : 6;
        i += 1;
    }
    return true;
}

void DialoguePlayer::end() {
    text_.str(std::string());
}

bool DialoguePlayer::draw(bool& escaped) {
    namespace gfx = core::gfx;
    Input& input = *services_.input;
    bool goesOn = true;
    escaped = false;

    // Two updates of the input per frame, as in 2002 (update() then
    // anyKeyPressed()).
    input.update();
    if (input.anyPressed()) {
        if (input.held(core::input::kKeyEscape)) {
            escaped = true;
            return false;
        }
        // A new key press after 750 ms shows the next box at once.
        if (keyReleased_ && now() - shownAt_ >= 750) {
            waitUntil_ = 0;
            keyReleased_ = false;
        }
    } else {
        keyReleased_ = true;
    }

    if (now() > waitUntil_) {
        goesOn = update();
    }

    // The boxes reach the screen's edges (a wider one in a level shown in
    // 16:9): the top one from its face on the left, the bottom one to its
    // face on the right.
    gfx::Pixmap& screen = *services_.screen;
    const int width = screen.width();
    if (image_[kTop] >= 0) {
        const gfx::Rect box{100, 120, width - 100, 100};
        gfx::darken(screen, &box);
    }
    if (image_[kBottom] >= 0) {
        const gfx::Rect box{0, 300, width - 100, 100};
        gfx::darken(screen, &box);
    }

    if (faces_[kTop] != nullptr && image_[kTop] >= 0 && image_[kTop] < faces_[kTop]->size()) {
        gfx::draw(screen, *(*faces_[kTop])[image_[kTop]], 0, 120);
    }
    // (The 2002 code drew it at x 538 with a 640-pixel scroll buffer, which
    // the desktop never has.)
    if (faces_[kBottom] != nullptr && image_[kBottom] >= 0 && image_[kBottom] < faces_[kBottom]->size()) {
        gfx::draw(screen, *(*faces_[kBottom])[image_[kBottom]], width - 100, 300);
    }

    const gfx::BitmapFont& font = *services_.fonts->rpg;
    if (textId_[kTop] != -1) {
        font.printWordWrapped(screen, 120, 135, services_.texts->text(textId_[kTop]), width);
    }
    if (textId_[kBottom] != -1) {
        font.printWordWrapped(screen, 20, 315, services_.texts->text(textId_[kBottom]), width - 105);
    }
    return goesOn;
}

void DialoguePlayer::skip() {
    while (update()) {
    }
}

bool DialoguePlayer::update() {
    World& w = *services_.world;
    bool readyToDraw = false;

    while (!readyToDraw && readLine() && std::strcmp(line_, "stop") != 0) {
        if (std::strcmp(line_, "endif") == 0) {
            continue;
        }
        if (std::strcmp(line_, "bbswap") == 0) {
            player_ += 1;
            player_ %= playerCount_;
            continue;
        }

        // "key=value" (a malformed line, which crashed the 2002 code, is
        // reported and skipped).
        char* equals = std::strchr(line_, '=');
        if (equals == nullptr) {
            error(line_);
            continue;
        }
        std::strcpy(value_, equals + 1);
        equals[0] = '\0';

        if (std::strcmp(line_, "ifnbj") == 0) {
            // A false condition skips to its endif.
            if (playerCount_ != toInt(value_)) {
                while (readLine() && std::strcmp(line_, "endif") != 0) {
                }
            }
        } else if (std::strcmp(line_, "focus") == 0) {
            if (std::strcmp(value_, "haut") == 0) {
                box_ = kTop;
            } else if (std::strcmp(value_, "bas") == 0) {
                box_ = kBottom;
            } else {
                error(value_);
            }
        } else if (std::strcmp(line_, "id") == 0) {
            if (std::strcmp(value_, "joueur") == 0) {
                faces_[box_] = w.media.heroFaces;
                owner_[box_] = kPlayer;
            } else if (std::strcmp(value_, "ennemi") == 0) {
                faces_[box_] = w.media.dialogueFaces;
                owner_[box_] = kEnemy;
            } else {
                error(value_);
            }
        } else if (std::strcmp(line_, "img") == 0) {
            image_[box_] = toInt(value_) + (owner_[box_] == kPlayer ? heroFirstImage_[player_] : 0);
        } else if (std::strcmp(line_, "txt") == 0) {
            textId_[box_] = toInt(value_);
        } else if (std::strcmp(line_, "affiche") == 0) {
            shownAt_ = now();
            waitUntil_ = now() + static_cast<std::uint32_t>(toInt(value_));
            readyToDraw = true;
        } else if (std::strcmp(line_, "flag") == 0) {
            // "flag=n.value"
            char* dot = std::strchr(value_, '.');
            if (dot == nullptr) {
                error(value_);
                continue;
            }
            const int value = toInt(dot + 1);
            dot[0] = '\0';
            w.flags.values[toInt(value_)] = value;
        } else {
            error(line_);
        }
    }
    return readyToDraw;
}

bool DialoguePlayer::readLine() {
    text_.getline(line_, kLineLength);
    while (!text_.eof() && line_[0] == ';') {
        text_.getline(line_, kLineLength);
    }
    return !text_.eof();
}

void DialoguePlayer::error(const char* what) {
    core::Log::warning(std::string("RPGPlayer::updateScene() -> Erreur de syntaxe :'") + what + "'");
}

}  // namespace bb::game
