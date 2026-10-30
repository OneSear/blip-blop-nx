#pragma once

#include <array>

#include "bb/core/gfx/pixmap.hpp"
#include "bb/game/services.hpp"
#include "bb/game/waits.hpp"

namespace bb::game {

// What the two menus of the game share (menu_main.cpp and menu_game.cpp of
// 2002, which were copies of each other): pages of items drawn centred over
// a darkened band of the screen, chosen with up, down and return (or the
// first player's aliases), the options page and the key pages. A pad's back
// button (the Switch's B) goes up one page.
//
// The 2002 update() blocked while it waited for a key or for its release;
// update() here stops at those points (wait() says what for) and goes on
// from there at the next call.
class Menu {
public:
    virtual ~Menu() = default;

    MenuWait wait() const noexcept { return wait_; }
    unsigned int& waitedKey() noexcept { return waitedKey_; }

    // Draws the menu over the screen (the darkened band is cached in the
    // backdrop and drawn again only when the page or its width changes).
    void draw();

protected:
    // The pages, numbered as in 2002 (MENU_*).
    enum Page {
        kMainPage = 0,
        kOptionsPage = 7,
        kKeys1Page = 5,
        kKeys2Page = 6,
        kKeys3Page = 10,  // the four-player mode
        kKeys4Page = 11,
        kExitPage = 8,
        kStartPage = 9
    };

    Menu(Services& services, int pauseOptionsPage, int lineHeight) noexcept;

    // The items of the current page.
    virtual void updateItems() = 0;

    // Leaving the options page or a key page saves the settings.
    void setPage(int page, int items, int focusedItem);
    // Back from an options or key page: the page above it; false on another
    // page (each menu handles its own).
    bool backFromOptions();
    // The menu closes: the settings are saved if it was on the options page
    // or a key page (Escape resumes the game from any page).
    void saveIfOnOptions();
    bool waitFor(MenuWait w);
    bool done(int& result, int value);
    bool keyHeld(unsigned int key) const;
    bool aliasHeld(int alias) const;
    void setItem(int i, const char* text);
    // The items of the options page: the display options and the language
    // (Phase 9; 2002 had a commented-out language choice; its vsync went
    // with the desktop versions), the key pages and return.
    enum OptionItem {
        kScalingItem,
        kFilterItem,
        kMotionItem,
        kLanguageItem,
        kKeys1Item,
        kKeys2Item,
        kKeys3Item,  // the four-player mode
        kKeys4Item,
        kOptionsReturnItem,
        kOptionItems
    };

    // The key page of player `player` (0 to 3), and the player of a key page
    // (-1 for another page).
    static int keysPage(int player) noexcept;
    static int keysPlayer(int page) noexcept;

    // The options page and a key page.
    void optionsItems();
    void keyItems(int firstAlias);
    // The key read for item `redefining_` of the current key page.
    void setRedefinedKey(unsigned int code);
    // Return on the options page: switches the focused option or opens its
    // page (RETURN goes back to the first page, on its OPTIONS item).
    void chooseOption();
    // Right or left on the options page: switches the focused option, if
    // it is one; false otherwise.
    bool switchOption();

    Services& services_;
    const int optionsPage_;  // MENU_OPTS: 7 in the main menu, 1 in the pause menu
    const int lineHeight_;   // 30 in the main menu, 40 in the pause menu

    int page_ = kMainPage;  // current_menu
    int focus_ = 0;         // the item in red
    int items_ = 0;         // nb_focus
    int redefining_ = -1;   // the key being redefined, -1 for none
    int stage_ = 0;         // where update() goes on
    MenuWait wait_ = MenuWait::None;
    unsigned int waitedKey_ = 0;

    // The band and the page it was darkened for (old_menu, rec).
    int bandPage_ = 0;
    int bandLeft_ = 0;

    std::array<std::array<char, 100>, 10> text_{};
};

// The main menu of the attract loop (MenuMain).
class MainMenu : public Menu {
public:
    // What update() returns in its result (RET_*).
    enum Result {
        kContinue = 0,
        kStartOnePlayer = 1,
        kExit = 3,
        kStartTwoPlayers = 5,
        kStartThreePlayers = 6,  // the four-player mode
        kStartFourPlayers = 7
    };

    explicit MainMenu(Services& services) noexcept : Menu(services, kOptionsPage, 30) {}

    void start();
    // One update: true with the result; false when the menu waits for
    // wait() to be over - call update() again then.
    bool update(int& result);
    // Leaves the menu: loads or frees the sounds and the music switched on
    // or off.
    void stop();

    // A key was held at the last update (its 2002 "up").
    bool keyWasHeld() const noexcept { return keyHeld_; }

private:
    void updateItems() override;

    bool keyHeld_ = false;
    bool soundWasOn_ = true;
    bool musicWasOn_ = true;
};

// The pause menu of a level (MenuGame).
class PauseMenu : public Menu {
public:
    // What update() returns in its result.
    enum Result { kStay = 0, kResume = 1, kLeaveLevel = 2 };

    explicit PauseMenu(Services& services) noexcept : Menu(services, 1, 40) {}

    void start();
    bool update(int& result);
    void stop();

private:
    void updateItems() override;
};

}  // namespace bb::game
