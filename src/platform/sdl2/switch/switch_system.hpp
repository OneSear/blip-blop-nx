#pragma once

// What the SDL 2 backend asks of the Switch's system (libnx) itself:
// devkitPro's SDL 2 does the rest (switch-port.md, 2.2). Only Switch builds
// compile switch_system.cpp; the files of this directory are the only ones
// that include <switch.h>.

#include <cstdint>
#include <string>

namespace bb::platform::sdl2::nx {

// The size of the screen in the console's mode: 1280x720 in handheld
// (and tabletop) mode, 1920x1080 docked.
void screenSize(int& width, int& height);

// sdmc:/switch/blipblop/, created when missing: the settings, hi-scores and
// log of the .nro and the .nsp alike.
std::string preferencesDirectory();

// The system's error applet (SDL has no message boxes on the Switch).
void showError(const std::string& title, const std::string& message);

// The controller applet for `count` players, single Joy-Cons allowed;
// blocks until it closes.
void requestPlayers(int count);

// The console's language: "fr" for French and Canadian French, "en" for
// the others.
std::string languageCode();

// The process's memory in use, in bytes.
std::uint64_t memoryInUse();

// Watches the applet's focus messages (HOME, sleep): the game loses the
// focus when it leaves the foreground and gains it back on return.
class FocusWatch {
public:
    FocusWatch();
    ~FocusWatch();
    FocusWatch(const FocusWatch&) = delete;
    FocusWatch& operator=(const FocusWatch&) = delete;

    // The next change since the last call: true with focused set, false
    // when there is none. Changes come from the applet's messages, which
    // SDL processes while it pumps its events.
    bool take(bool& focused);

    struct State;  // what the applet's hook writes (switch_system.cpp)

private:
    State* state_;
};

}  // namespace bb::platform::sdl2::nx
