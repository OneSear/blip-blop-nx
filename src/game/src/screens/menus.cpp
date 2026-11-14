// The main menu and the pause menu (menu_main.cpp, menu_game.cpp).

#include "bb/game/menus.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "bb/core/assets/asset_source.hpp"
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
    kXbrText = 867,  // the shaders
    kCrtText = 868,
    kSmoothText = 802,
    kMotionText = 804,
    kWidescreenText = 807,  // an empty id of 2002
    kPlayer1KeysText = 819,
    kPlayer2KeysText = 820,
    kPlayer3KeysText = 860,  // the four-player mode
    kPlayer4KeysText = 861,
    kUpText = 821,  // up, down, left, right, fire, jump, special
    kStartGameText = 830,
    kExitText = 833,
    kCancelText = 834,
    kMusicText = 835,  // the volumes and the rumble
    kSoundText = 836,
    kRumbleText = 837,
    kCheatsText = 990,  // the pause menu's cheats, before the credits (1000)
    kInfiniteAmmoText = 991,
    kInfiniteLivesText = 992,
    kInfiniteCowBombsText = 993,
    kInvincibleText = 994,
    kWinLevelText = 995,
};

// The menus' items stand within this height; a page with more items than
// fit at its line height gets closer lines (the options page of the pause
// menu: 13 items).
constexpr int kItemsHeight = 400;
// The band's growth for a line under a page's items (Menu::footerText()).
constexpr int kFooterHeight = 24;
constexpr int kControllersText = 869;  // "X = controllers"
constexpr int kNoHiScoreText = 996;    // "No hi-score for a game with cheats"

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

Menu::Menu(Services& services, int pauseOptionsPage, int lineHeight, int mainItems) noexcept
    : services_(services), optionsPage_(pauseOptionsPage), lineHeight_(lineHeight), mainItems_(mainItems) {}

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
        setPage(kMainPage, mainItems_, 1);  // on OPTIONS
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

bool Menu::switchOption(int step) {
    Options& o = *services_.options;
    switch (focus_) {
        case kScalingItem:
            o.integerScaling = !o.integerScaling;
            break;
        case kFilterItem: {
            // SHARP, CRISP, SMOOTH, XBR, CRT and round again.
            constexpr int kCount = sizeof(core::gfx::kFilters) / sizeof(core::gfx::kFilters[0]);
            int i = 0;
            while (i < kCount && core::gfx::kFilters[i] != o.filter) {
                ++i;
            }
            o.filter = core::gfx::kFilters[(i + 1) % kCount];
            break;
        }
        case kMotionItem:
            o.smoothMotion = !o.smoothMotion;
            break;
        case kWidescreenItem:
            // From the next level start.
            o.widescreen = !o.widescreen;
            break;
        case kLanguageItem:
            // The application loads the other string table before the
            // next frame is drawn.
            o.language =
                o.language == input::Language::English ? input::Language::French : input::Language::English;
            break;
        case kMusicItem:
        case kSoundItem: {
            // The application gives the mixer the new volume.
            int& volume = focus_ == kMusicItem ? o.musicVolume : o.soundVolume;
            volume = std::clamp(volume + step, 0, Options::kMaxVolume);
            break;
        }
        case kRumbleItem:
            o.rumble = !o.rumble;
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
            setPage(kMainPage, mainItems_, 1);
            break;
        case kMusicItem:
        case kSoundItem: {
            Options& o = *services_.options;
            int& volume = focus_ == kMusicItem ? o.musicVolume : o.soundVolume;
            volume = volume == Options::kMaxVolume ? 0 : volume + 1;
            updateItems();
            break;
        }
        default:
            switchOption(1);
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
    const auto filterText = [](core::gfx::Filter f) {
        switch (f) {
            case core::gfx::Filter::Sharp:
                return kSharpText;
            case core::gfx::Filter::Crisp:
                return kCrispText;
            case core::gfx::Filter::Smooth:
                return kSmoothText;
            case core::gfx::Filter::Xbr:
                return kXbrText;
            case core::gfx::Filter::Crt:
                return kCrtText;
        }
        return kCrispText;
    };
    option(kFilterItem, kFilterText, filterText(o.filter));
    option(kMotionItem, kMotionText, o.smoothMotion ? kOnText : kOffText);
    option(kWidescreenItem, kWidescreenText, o.widescreen ? kOnText : kOffText);
    option(kLanguageItem, kLanguageText, o.language == input::Language::French ? kFrenchText : kEnglishText);
    const auto volume = [&](int item, int label, int level) {
        std::snprintf(text_[static_cast<std::size_t>(item)].data(), text_[0].size(), "%s %d", t.text(label),
                      level);
    };
    volume(kMusicItem, kMusicText, o.musicVolume);
    volume(kSoundItem, kSoundText, o.soundVolume);
    option(kRumbleItem, kRumbleText, o.rumble ? kOnText : kOffText);
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

    const int lineHeight = std::min(lineHeight_, kItemsHeight / std::max(items_, 1));
    const int halfHeight = items_ * lineHeight / 2;
    int y = 240 - halfHeight;
    const int centre = screen.width() / 2;  // 320, or the middle of a level in 16:9

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
    const int footer = footerText();
    const int moved = (centre - halfWidth) - bandLeft_;
    if (page_ != bandPage_ || moved > 20 || moved < -20) {
        gfx::blit(backdrop, 0, 0, screen);
        bandLeft_ = centre - halfWidth;
        const int footerHeight = footer != 0 ? kFooterHeight : 0;
        const gfx::Rect band{centre - halfWidth, 220 - halfHeight, 2 * halfWidth,
                             40 + 2 * halfHeight + footerHeight};
        gfx::darken(backdrop, &band);
        bandPage_ = page_;
    }
    gfx::blit(screen, 0, 0, backdrop);

    for (int i = 0; i < items_; ++i) {
        (focus_ == i ? focusedFont : font)
            .printCentred(screen, centre, y, text_[static_cast<std::size_t>(i)].data());
        y += lineHeight;
    }
    if (footer != 0) {
        services_.fonts->rpg->printCentred(screen, centre, 240 + halfHeight + 8,
                                           services_.texts->text(footer));
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

    std::string error;
    const auto campaign = services_.assets->campaign("data/bb.lst", error);
    missions_ = campaign ? findMissions(*campaign) : std::vector<Mission>();
    firstPart_ = 0;
}

int MainMenu::missionsOffered() const {
    const int reached = services_.progress != nullptr ? services_.progress->missionsReached : 1;
    const int fit = static_cast<int>(text_.size()) - 1;  // and RETURN
    return std::min({reached, static_cast<int>(missions_.size()), fit});
}

void MainMenu::updateItems() {
    const TextSource& t = *services_.texts;
    switch (page_) {
        case kMainPage:
            setItem(0, t.text(kStartGameText));
            setItem(1, t.text(kOptionsText));
            setItem(2, t.text(kExitText));
            break;
        case kExitPage:
            setItem(0, t.text(kExitText));
            setItem(1, t.text(kCancelText));
            break;
        case kOptionsPage:
            optionsItems();
            break;
        case kMissionPage: {
            const int count = missionsOffered();
            for (int i = 0; i < count; ++i) {
                setItem(i, missionName(missions_, i, t).c_str());
            }
            setItem(count, t.text(kReturnText));
            break;
        }
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
                                // Once a second mission was reached: the
                                // missions, on the last one reached.
                                if (missionsOffered() > 1) {
                                    setPage(kMissionPage, missionsOffered() + 1, missionsOffered() - 1);
                                    return done(result, kContinue);
                                }
                                firstPart_ = 0;
                                return done(result, kStartGame);
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

                    case kMissionPage:
                        if (focus_ < missionsOffered()) {
                            firstPart_ = missions_[static_cast<std::size_t>(focus_)].part;
                            return done(result, kStartGame);
                        }
                        setPage(kMainPage, 3, 0);  // RETURN
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
                    sideStep_ = keyHeld(input::kKeyRight) || aliasHeld(kAliasRight) ? 1 : -1;
                    stage_ = kSideAction;
                    return waitFor(MenuWait::Release);
                }
                stage_ = kBackKey;
                break;

            case kSideAction:
                // Right and left switch the option (a volume goes up or
                // down).
                if (page_ == kOptionsPage) {
                    switchOption(sideStep_);
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
                    if (page_ == kExitPage) {
                        setPage(kMainPage, 3, 2);
                    } else if (page_ == kMissionPage) {
                        setPage(kMainPage, 3, 0);
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
    kPauseControllersKey,
    kPauseControllersAction,
};

}  // namespace

void PauseMenu::start() {
    setPage(kMainPage, mainItems_, 0);
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
        setItem(2, t.text(kCheatsText));
        setItem(3, t.text(kExitGameText));
    } else if (page_ == optionsPage_) {
        optionsItems();
    } else if (page_ == kCheatsPage) {
        cheatItems();
    } else if (keysPlayer(page_) >= 0) {
        keyItems(AliasControls::firstOf(keysPlayer(page_)));
    }
}

int PauseMenu::footerText() const {
    return page_ == kMainPage ? kControllersText : page_ == kCheatsPage ? kNoHiScoreText : 0;
}

void PauseMenu::cheatItems() {
    const TextSource& t = *services_.texts;
    const MenuCheats& c = services_.world->cheats;
    const auto option = [&](int item, int label, bool on) {
        std::snprintf(text_[static_cast<std::size_t>(item)].data(), text_[0].size(), "%s %s", t.text(label),
                      t.text(on ? kOnText : kOffText));
    };
    option(kAmmoItem, kInfiniteAmmoText, c.infiniteAmmo);
    option(kLivesItem, kInfiniteLivesText, c.infiniteLives);
    option(kCowBombItem, kInfiniteCowBombsText, c.infiniteCowBombs);
    option(kInvincibleItem, kInvincibleText, c.invincible);
    setItem(kWinItem, t.text(kWinLevelText));
    setItem(kCheatsReturnItem, t.text(kReturnText));
}

bool PauseMenu::switchCheat() {
    MenuCheats& c = services_.world->cheats;
    bool* cheat = nullptr;
    const char* name = "";
    switch (focus_) {
        case kAmmoItem:
            cheat = &c.infiniteAmmo;
            name = "infinite ammo";
            break;
        case kLivesItem:
            cheat = &c.infiniteLives;
            name = "infinite lives";
            break;
        case kCowBombItem:
            cheat = &c.infiniteCowBombs;
            name = "infinite cow bombs";
            break;
        case kInvincibleItem:
            cheat = &c.invincible;
            name = "invincible";
            break;
        default:
            return false;
    }
    *cheat = !*cheat;
    c.used = c.used || *cheat;
    services_.session->note((std::string("cheat: ") + name + (*cheat ? " on" : " off")).c_str());
    updateItems();
    return true;
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
                            setPage(kCheatsPage, kCheatItems, kAmmoItem);
                            return done(result, kStay);
                        case 3:
                            return done(result, kLeaveLevel);
                        default:
                            break;
                    }
                } else if (page_ == kCheatsPage) {
                    if (focus_ == kWinItem) {
                        // Back to the game, which the next tick wins.
                        MenuCheats& c = services_.world->cheats;
                        c.winLevel = true;
                        c.used = true;
                        services_.session->note("cheat: win level");
                        return done(result, kResume);
                    }
                    if (focus_ == kCheatsReturnItem) {
                        setPage(kMainPage, mainItems_, 2);  // on CHEATS
                    } else {
                        switchCheat();
                    }
                    return done(result, kStay);
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
                if ((page_ == optionsPage_ && switchOption(1)) || (page_ == kCheatsPage && switchCheat())) {
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
                if ((page_ == optionsPage_ && switchOption(-1)) || (page_ == kCheatsPage && switchCheat())) {
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
                stage_ = kPauseControllersKey;
                break;

            case kPauseControllersKey:
                // X (SDL's Y, the top button) on any pad, as on the hero
                // choice: the controller screen, where a Joy-Con pair can
                // become two players' controllers (who then join in the
                // level).
                for (int pad = 0; pad < kMaxPlayers; ++pad) {
                    if (keyHeld(input::padCode(pad, input::kPadButtonY))) {
                        stage_ = kPauseControllersAction;
                        return waitFor(MenuWait::Release);
                    }
                }
                return done(result, kStay);

            case kPauseControllersAction:
                services_.input->requestPlayers(kMaxPlayers);
                return done(result, kStay);

            case kPauseBackAction:
                // Up one page; from the first, back to the game.
                if (page_ == kMainPage) {
                    return done(result, kResume);
                }
                if (page_ == kCheatsPage) {
                    setPage(kMainPage, mainItems_, 2);  // on CHEATS
                } else {
                    backFromOptions();
                }
                return done(result, kStay);

            default:
                return done(result, kStay);
        }
    }
}

}  // namespace bb::game
