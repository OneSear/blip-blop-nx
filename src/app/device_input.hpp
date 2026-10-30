#pragma once

#include <cstdint>
#include <string>

#include "bb/game/services.hpp"

namespace bb::platform {
class InputSource;
}

namespace bb::app {

class Session;

// The keyboard and the pads (the 2002 Input): what the game reads is the
// state of the last update(), copied from the platform's events, or in
// deterministic runs the recorded input of the frame (session).
//
// Codes: key codes below 255, 0x40000000 | (key & 0xFFF) for the other
// keys, ((pad + 1) << 10) | button for pads, with 100 to 103 for the
// directions (up, down, left, right).
class DeviceInput : public game::Input {
public:
    static constexpr int kPads = 5;  // MAX_JOY

    DeviceInput(platform::InputSource& source, Session& session);

    // Logs the pads the platform opened.
    void open();

    // game::Input
    void update() override;
    bool held(unsigned int code) const override;
    unsigned int alias(int a) const override { return aliases_[a]; }
    void setAlias(int a, unsigned int code) override { aliases_[a] = code; }
    bool anyHeld() const override;
    bool firstHeld(unsigned int& code) const override;
    bool takeFocusLost() override;
    bool backHeld() const override;
    void requestPlayers(int count) override;
    bool beginTextEntry() override;
    TextEntry textEntry(std::string& text) override;

private:
    struct Pad {
        unsigned char buttons[128];
        unsigned char up;
        unsigned char down;
        unsigned char left;
        unsigned char right;
    };
    struct Devices {
        char keys[256];
        char specials[0xFFF];
        Pad pads[kPads];
    };
    struct Directions {
        unsigned char up = 0;
        unsigned char down = 0;
        unsigned char left = 0;
        unsigned char right = 0;
    };

    void publish(bool quitRequested);
    void logPad(int slot) const;
    void updateDirections(int pad);

    platform::InputSource& source_;
    Session& session_;
    int pads_ = 0;
    Devices visible_{};         // what the game reads
    Devices live_{};            // from the platform's events
    Directions hats_[kPads];    // live: the D-pad or hat of each pad
    Directions sticks_[kPads];  // live: the stick (axes 0 and 1) of each pad
    unsigned int aliases_[256] = {};
    // Focus: live from the window events, then as the game sees it (from
    // the recording in deterministic runs), and a loss not yet handled.
    bool liveUnfocused_ = false;
    bool unfocused_ = false;
    bool focusLost_ = false;
    // The on-screen keyboard: shown, and the text it typed.
    bool textEntry_ = false;
    bool textTyped_ = false;
    std::string text_;
};

}  // namespace bb::app
