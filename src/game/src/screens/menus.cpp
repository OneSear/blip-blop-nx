// The main menu and the pause menu (menu_main.cpp, menu_game.cpp).

#include "bb/game/menus.hpp"

#include <cstdio>
#include <cstring>
#include <string>

#include "bb/core/input/codes.hpp"
#include "bb/core/input/names.hpp"
#include "bb/game/world.hpp"

namespace bb::game {

namespace {

namespace input = core::input;

// The strings of the menus.
enum Text {
    kResumeText = 800,
    kOptionsText = 801,
    kExitGameText = 805,
    kReturnText = 808,
    kOnText = 809,
    kOffText = 810,
    kEnglishText = 811,  // each language in its own words
    kFrenchText = 812,
    kScalingText = 813,
    kFitText = 814,
    kIntegerText = 815,
    kLanguageText = 816,
    kFilterText = 817,
    kSharpText = 818,
    kCrispText = 862,
    kSmoothText = 802,
    kMotionText = 804,
    kPlayer1KeysText = 819,
    kPlayer2KeysText = 820,
    kPlayer3KeysText = 860,  // the four-player mode
    kPlayer4KeysText = 861,
    kUpText = 821,  // up, down, left, right, fire, jump, special
    kStartGameText = 830,
    kOnePlayerText = 831,
    kTwoPlayersText = 832,
    kThreePlayersText = 858,
    kFourPlayersText = 859,
    kExitText = 833,
    kCancelText = 834,
};

// The aliases of the first player's up, down, left, right and fire.
constexpr int kAliasUp = AliasControls::kFirstPlayer + AliasControls::kUp;
constexpr int kAliasDown = AliasControls::kFirstPlayer + AliasControls::kDown;
constexpr int kAliasLeft = AliasControls::kFirstPlayer + AliasControls::kLeft;
constexpr int kAliasRight = AliasControls::kFirstPlayer + AliasControls::kRight;
constexpr int kAliasFire = AliasControls::kFirstPlayer + AliasControls::kFire;

}  // namespace

//-----------------------------------------------------------------------------
// Menu
//-----------------------------------------------------------------------------

Menu::Menu(Services& services, int pauseOptionsPage, int lineHeight) noexcept
    : services_(services), optionsPage_(pauseOptionsPage), lineHeight_(lineHeight) {}

void Menu::setPage(int page, int items, int focusedItem) {
    // What the options and key pages changed is written when they are left,
    // not only when the game exits: a console may sleep or lose power first.
    if (page != page_ && (page_ == optionsPage_ || keysPlayer(page_) >= 0)) {
        services_.session->saveSettings();
    }
    page_ = page;
    items_ = items;
    focus_ = focusedItem;
    updateItems();
}

void Menu::saveIfOnOptions() {
    if (page_ == optionsPage_ || keysPlayer(page_) >= 0) {
        services_.session->saveSettings();
    }
}

bool Menu::backFromOptions() {
    if (page_ == optionsPage_) {
        setPage(kMainPage, 3, 1);  // on OPTIONS
        return true;
    }
    if (const int player = keysPlayer(page_); player >= 0) {
        setPage(optionsPage_, kOptionItems, kKeys1Item + player);
        return true;
    }
    return false;
}

bool Menu::waitFor(MenuWait w) {
    wait_ = w;
    return false;
}

bool Menu::done(int& result, int value) {
    stage_ = 0;
    wait_ = MenuWait::None;
    result = value;
    return true;
}

bool Menu::keyHeld(unsigned int key) const {
    return services_.input->held(key);
}

bool Menu::aliasHeld(int alias) const {
    return services_.input->aliasHeld(alias);
}

void Menu::setItem(int i, const char* text) {
    std::snprintf(text_[static_cast<std::size_t>(i)].data(), text_[0].size(), "%s", text);
}

bool Menu::switchOption() {
    Options& o = *services_.options;
    switch (focus_) {
        case kScalingItem:
            o.integerScaling = !o.integerScaling;
            break;
        case kFilterItem:
            // SHARP, CRISP, SMOOTH and round again.
            o.filter = o.filter == core::gfx::Filter::Sharp   ? core::gfx::Filter::Crisp
                       : o.filter == core::gfx::Filter::Crisp ? core::gfx::Filter::Smooth
                                                              : core::gfx::Filter::Sharp;
            break;
        case kMotionItem:
            o.smoothMotion = !o.smoothMotion;
            break;
        case kLanguageItem:
            // The application loads the other string table before the
            // next frame is drawn.
            o.language =
                o.language == input::Language::English ? input::Language::French : input::Language::English;
            break;
        default:
            return false;
    }
    updateItems();
    return true;
}

int Menu::keysPage(int player) noexcept {
    const int pages[] = {kKeys1Page, kKeys2Page, kKeys3Page, kKeys4Page};
    return pages[player];
}

int Menu::keysPlayer(int page) noexcept {
    for (int player = 0; player < kMaxPlayers; ++player) {
        if (keysPage(player) == page) {
            return player;
        }
    }
    return -1;
}

void Menu::chooseOption() {
    switch (focus_) {
        case kKeys1Item:
        case kKeys2Item:
        case kKeys3Item:
        case kKeys4Item:
            setPage(keysPage(focus_ - kKeys1Item), 8, 7);
            break;
        case kOptionsReturnItem:
            setPage(kMainPage, 3, 1);
            break;
        default:
            switchOption();
            break;
    }
}

void Menu::optionsItems() {
    const TextSource& t = *services_.texts;
    const Options& o = *services_.options;
    const auto option = [&](int item, int label, int value) {
        std::snprintf(text_[static_cast<std::size_t>(item)].data(), text_[0].size(), "%s %s", t.text(label),
                      t.text(value));
    };
    option(kScalingItem, kScalingText, o.integerScaling ? kIntegerText : kFitText);
    option(kFilterItem, kFilterText,
           o.filter == core::gfx::Filter::Sharp   ? kSharpText
           : o.filter == core::gfx::Filter::Crisp ? kCrispText
                                                  : kSmoothText);
    option(kMotionItem, kMotionText, o.smoothMotion ? kOnText : kOffText);
    option(kLanguageItem, kLanguageText, o.language == input::Language::French ? kFrenchText : kEnglishText);
    setItem(kKeys1Item, t.text(kPlayer1KeysText));
    setItem(kKeys2Item, t.text(kPlayer2KeysText));
    setItem(kKeys3Item, t.text(kPlayer3KeysText));
    setItem(kKeys4Item, t.text(kPlayer4KeysText));
    setItem(kOptionsReturnItem, t.text(kReturnText));
}

void Menu::keyItems(int firstAlias) {
    const TextSource& t = *services_.texts;
    // The page lists the seven actions in the order of the alias table.
    static const int kItemAlias[7] = {AliasControls::kUp,     AliasControls::kDown, AliasControls::kLeft,
                                      AliasControls::kRight,  AliasControls::kFire, AliasControls::kJump,
                                      AliasControls::kSpecial};
    for (int i = 0; i < 7; ++i) {
        const std::string key =
            redefining_ == i
                ? std::string("...")
                : input::displayName(
                      static_cast<input::Code>(services_.input->alias(firstAlias + kItemAlias[i])),
                      services_.options->language);
        std::snprintf(text_[static_cast<std::size_t>(i)].data(), text_[0].size(), "%s = %s",
                      t.text(kUpText + i), key.c_str());
    }
    setItem(7, t.text(kReturnText));
}

void Menu::setRedefinedKey(unsigned int code) {
    static const int kItemAlias[7] = {AliasControls::kUp,     AliasControls::kDown, AliasControls::kLeft,
                                      AliasControls::kRight,  AliasControls::kFire, AliasControls::kJump,
                                      AliasControls::kSpecial};
    if (redefining_ < 0 || redefining_ > 6) {
        return;
    }
    const int player = keysPlayer(page_);
    if (player < 0) {
        return;
    }
    services_.input->setAlias(AliasControls::firstOf(player) + kItemAlias[redefining_], code);
}

void Menu::draw() {
    namespace gfx = core::gfx;
    gfx::Pixmap& screen = *services_.screen;
    gfx::Pixmap& backdrop = *services_.backdrop;
    const gfx::BitmapFont& font = *services_.fonts->menu;
    const gfx::BitmapFont& focusedFont = *services_.fonts->menus;

    const int halfHeight = items_ * lineHeight_ / 2;
    int y = 240 - halfHeight;

    // The band: the widest item and 30 pixels on each side, from 400 to 640
    // pixels wide.
    int halfWidth = 0;
    for (int i = 0; i < items_; ++i) {
        const int w = font.width(text_[static_cast<std::size_t>(i)].data());
        if (w > halfWidth) {
            halfWidth = w;
        }
    }
    halfWidth += 60;
    halfWidth /= 2;
    if (halfWidth < 200) {
        halfWidth = 200;
    } else if (halfWidth > 320) {
        halfWidth = 320;
    }

    // Darkened again when the page changes or the band moves by more than
    // 20 pixels.
    const int moved = (320 - halfWidth) - bandLeft_;
    if (page_ != bandPage_ || moved > 20 || moved < -20) {
        gfx::blit(backdrop, 0, 0, screen);
        bandLeft_ = 320 - halfWidth;
        const gfx::Rect band{320 - halfWidth, 220 - halfHeight, 2 * halfWidth, 40 + 2 * halfHeight};
        gfx::darken(backdrop, &band);
        bandPage_ = page_;
    }
    gfx::blit(screen, 0, 0, backdrop);

    for (int i = 0; i < items_; ++i) {
        (focus_ == i ? focusedFont : font)
            .printCentred(screen, 320, y, text_[static_cast<std::size_t>(i)].data());
        y += lineHeight_;
    }
}

//-----------------------------------------------------------------------------
// MainMenu
//-----------------------------------------------------------------------------

namespace {

enum MainStage {
    kStart,
    kRedefineKey,
    kRedefineDone,
    kKeys,
    kReturnKey,
    kReturnAction,
    kSideKeys,
    kSideAction,
    kBackKey,
    kBackAction
};

}  // namespace

void MainMenu::start() {
    setPage(kMainPage, 3, 0);
    musicWasOn_ = services_.audio->musicOn;
    soundWasOn_ = services_.audio->soundOn;
}

void MainMenu::updateItems() {
    const TextSource& t = *services_.texts;
    switch (page_) {
        case kMainPage:
            setItem(0, t.text(kStartGameText));
            setItem(1, t.text(kOptionsText));
            setItem(2, t.text(kExitText));
            break;
        case kStartPage:
            setItem(0, t.text(kOnePlayerText));
            setItem(1, t.text(kTwoPlayersText));
            setItem(2, t.text(kThreePlayersText));
            setItem(3, t.text(kFourPlayersText));
            setItem(4, t.text(kReturnText));
            break;
        case kExitPage:
            setItem(0, t.text(kExitText));
            setItem(1, t.text(kCancelText));
            break;
        case kOptionsPage:
            optionsItems();
            break;
        default:
            if (keysPlayer(page_) >= 0) {
                keyItems(AliasControls::firstOf(keysPlayer(page_)));
            }
            break;
    }
}

bool MainMenu::update(int& result) {
    Input& in = *services_.input;

    for (;;) {
        switch (stage_) {
            case kStart:
                // Two updates of the input, as in 2002 (update() then
                // anyKeyPressed()).
                in.update();
                keyHeld_ = in.anyPressed();
                if (redefining_ != -1) {
                    stage_ = kRedefineKey;
                    return waitFor(MenuWait::Key);
                }
                stage_ = kKeys;
                break;

            case kRedefineKey:
                setRedefinedKey(waitedKey_);
                stage_ = kRedefineDone;
                return waitFor(MenuWait::Release);

            case kRedefineDone:
                redefining_ = -1;
                stage_ = kKeys;
                break;

            case kKeys:
                updateItems();
                if (keyHeld(input::kKeyUp) || aliasHeld(kAliasUp)) {
                    focus_ -= 1;
                    if (focus_ < 0) {
                        focus_ = items_ - 1;
                    }
                    stage_ = kReturnKey;
                    return waitFor(MenuWait::Release);
                }
                if (keyHeld(input::kKeyDown) || aliasHeld(kAliasDown)) {
                    focus_ += 1;
                    focus_ %= items_;
                    stage_ = kReturnKey;
                    return waitFor(MenuWait::Release);
                }
                stage_ = kReturnKey;
                break;

            case kReturnKey:
                if (keyHeld(input::kKeyReturn) || aliasHeld(kAliasFire)) {
                    stage_ = kReturnAction;
                    return waitFor(MenuWait::Release);
                }
                stage_ = kSideKeys;
                break;

            case kReturnAction:
                stage_ = kSideKeys;
                switch (page_) {
                    case kMainPage:
                        switch (focus_) {
                            case 0:
                                setPage(kStartPage, 5, 0);
                                return done(result, kContinue);
                            case 1:
                                setPage(kOptionsPage, kOptionItems, kOptionsReturnItem);
                                return done(result, kContinue);
                            case 2:
                                setPage(kExitPage, 2, 0);
                                return done(result, kContinue);
                            default:
                                break;
                        }
                        break;

                    case kStartPage:
                        switch (focus_) {
                            case 0:
                                return done(result, kStartOnePlayer);
                            case 1:
                                return done(result, kStartTwoPlayers);
                            case 2:
                                return done(result, kStartThreePlayers);
                            case 3:
                                return done(result, kStartFourPlayers);
                            case 4:
                                setPage(kMainPage, 3, 0);  // and on to the side keys
                                break;
                            default:
                                break;
                        }
                        break;

                    case kExitPage:
                        switch (focus_) {
                            case 0:
                                return done(result, kExit);
                            case 1:
                                setPage(kMainPage, 3, 2);
                                break;
                            default:
                                break;
                        }
                        break;

                    case kOptionsPage:
                        chooseOption();
                        return done(result, kContinue);

                    case kKeys1Page:
                    case kKeys2Page:
                    case kKeys3Page:
                    case kKeys4Page:
                        if (focus_ == 7) {
                            setPage(kOptionsPage, kOptionItems, kKeys1Item + keysPlayer(page_));
                        } else {
                            redefining_ = focus_;
                            updateItems();
                        }
                        return done(result, kContinue);

                    default:
                        break;
                }
                break;

            case kSideKeys:
                if (keyHeld(input::kKeyRight) || aliasHeld(kAliasRight) || keyHeld(input::kKeyLeft) ||
                    aliasHeld(kAliasLeft)) {
                    stage_ = kSideAction;
                    return waitFor(MenuWait::Release);
                }
                stage_ = kBackKey;
                break;

            case kSideAction:
                // Right and left do the same: they switch the option.
                if (page_ == kOptionsPage) {
                    switchOption();
                }
                return done(result, kContinue);

            case kBackKey:
                if (services_.input->backHeld()) {
                    stage_ = kBackAction;
                    return waitFor(MenuWait::Release);
                }
                return done(result, kContinue);

            case kBackAction:
                // Up one page; the first page has nothing above it.
                if (!backFromOptions()) {
                    if (page_ == kStartPage) {
                        setPage(kMainPage, 3, 0);
                    } else if (page_ == kExitPage) {
                        setPage(kMainPage, 3, 2);
                    }
                }
                return done(result, kContinue);

            default:
                return done(result, kContinue);
        }
    }
}

void MainMenu::stop() {
    AudioOutput& audio = *services_.audio;
    Banks& banks = services_.banks;

    // The sound switched on loads the heroes' sounds (and only them).
    if (audio.soundOn != soundWasOn_) {
        if (audio.soundOn) {
            loadSounds(*services_.assets, "data/bb.sfx", *banks.heroSounds);
        } else {
            banks.heroSounds->clear();
        }
    }

    // The music switched on loads and plays the level's music again.
    if (audio.musicOn != musicWasOn_) {
        if (audio.musicOn) {
            if (!banks.levelMusicFile.empty()) {
                if (!loadMusic(*services_.assets, banks.levelMusicFile, *banks.levelMusic)) {
                    audio.musicOn = false;
                } else if (services_.world->currentMusic >= 0) {
                    banks.levelMusic->play(services_.world->currentMusic);
                }
            }
        } else {
            banks.levelMusic->stopAll();
            banks.levelMusic->clear();
        }
    }

    saveIfOnOptions();
    bandPage_ = -1;
}

//-----------------------------------------------------------------------------
// PauseMenu
//-----------------------------------------------------------------------------

namespace {

enum PauseStage {
    kPauseStart,
    kPauseRedefineKey,
    kPauseRedefineDone,
    kPauseKeys,
    kPauseReturnKey,
    kPauseReturnAction,
    kPauseRightKey,
    kPauseRightAction,
    kPauseLeftKey,
    kPauseLeftAction,
    kPauseEscapeKey,
    kPauseEscapeAction,
    kPauseBackKey,
    kPauseBackAction,
};

}  // namespace

void PauseMenu::start() {
    setPage(kMainPage, 3, 0);
}

void PauseMenu::stop() {
    saveIfOnOptions();
    bandPage_ = -1;
}

void PauseMenu::updateItems() {
    const TextSource& t = *services_.texts;
    if (page_ == kMainPage) {
        setItem(0, t.text(kResumeText));
        setItem(1, t.text(kOptionsText));
        setItem(2, t.text(kExitGameText));
    } else if (page_ == optionsPage_) {
        optionsItems();
    } else if (keysPlayer(page_) >= 0) {
        keyItems(AliasControls::firstOf(keysPlayer(page_)));
    }
}

bool PauseMenu::update(int& result) {
    Input& in = *services_.input;

    for (;;) {
        switch (stage_) {
            case kPauseStart:
                in.update();
                if (redefining_ != -1) {
                    stage_ = kPauseRedefineKey;
                    return waitFor(MenuWait::Key);
                }
                stage_ = kPauseKeys;
                break;

            case kPauseRedefineKey:
                setRedefinedKey(waitedKey_);
                stage_ = kPauseRedefineDone;
                return waitFor(MenuWait::Release);

            case kPauseRedefineDone:
                redefining_ = -1;
                stage_ = kPauseKeys;
                break;

            case kPauseKeys:
                updateItems();
                if (keyHeld(input::kKeyUp) || aliasHeld(kAliasUp)) {
                    focus_ -= 1;
                    if (focus_ < 0) {
                        focus_ = items_ - 1;
                    }
                    stage_ = kPauseReturnKey;
                    return waitFor(MenuWait::Release);
                }
                if (keyHeld(input::kKeyDown) || aliasHeld(kAliasDown)) {
                    focus_ += 1;
                    focus_ %= items_;
                    stage_ = kPauseReturnKey;
                    return waitFor(MenuWait::Release);
                }
                stage_ = kPauseReturnKey;
                break;

            case kPauseReturnKey:
                if (keyHeld(input::kKeyReturn) || aliasHeld(kAliasFire)) {
                    stage_ = kPauseReturnAction;
                    return waitFor(MenuWait::Release);
                }
                stage_ = kPauseRightKey;
                break;

            case kPauseReturnAction:
                stage_ = kPauseRightKey;
                if (page_ == kMainPage) {
                    switch (focus_) {
                        case 0:
                            return done(result, kResume);
                        case 1:
                            setPage(optionsPage_, kOptionItems, kOptionsReturnItem);
                            return done(result, kStay);
                        case 2:
                            return done(result, kLeaveLevel);
                        default:
                            break;
                    }
                } else if (page_ == optionsPage_) {
                    chooseOption();
                    return done(result, kStay);
                } else if (keysPlayer(page_) >= 0) {
                    if (focus_ == 7) {
                        setPage(optionsPage_, kOptionItems, kOptionsReturnItem);
                    } else {
                        redefining_ = focus_;
                        updateItems();
                    }
                    return done(result, kStay);
                }
                break;

            case kPauseRightKey:
                if (keyHeld(input::kKeyRight) || aliasHeld(kAliasRight)) {
                    stage_ = kPauseRightAction;
                    return waitFor(MenuWait::Release);
                }
                stage_ = kPauseLeftKey;
                break;

            case kPauseRightAction:
                stage_ = kPauseLeftKey;
                if (page_ == optionsPage_ && switchOption()) {
                    return done(result, kStay);
                }
                break;

            case kPauseLeftKey:
                if (keyHeld(input::kKeyLeft) || aliasHeld(kAliasLeft)) {
                    stage_ = kPauseLeftAction;
                    return waitFor(MenuWait::Release);
                }
                stage_ = kPauseEscapeKey;
                break;

            case kPauseLeftAction:
                stage_ = kPauseEscapeKey;
                if (page_ == optionsPage_ && switchOption()) {
                    return done(result, kStay);
                }
                break;

            case kPauseEscapeKey:
                if (keyHeld(input::kKeyEscape)) {
                    stage_ = kPauseEscapeAction;
                    return waitFor(MenuWait::Release);
                }
                stage_ = kPauseBackKey;
                break;

            case kPauseEscapeAction:
                return done(result, kResume);

            case kPauseBackKey:
                if (services_.input->backHeld()) {
                    stage_ = kPauseBackAction;
                    return waitFor(MenuWait::Release);
                }
                return done(result, kStay);

            case kPauseBackAction:
                // Up one page; from the first, back to the game.
                if (page_ == kMainPage) {
                    return done(result, kResume);
                }
                backFromOptions();
                return done(result, kStay);

            default:
                return done(result, kStay);
        }
    }
}

}  // namespace bb::game
