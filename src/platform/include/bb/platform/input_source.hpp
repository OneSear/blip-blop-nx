#pragma once

#include <cstdint>
#include <string>

#include "bb/core/input/codes.hpp"

namespace bb::platform {

struct InputEvent {
    enum class Type {
        KeyDown,  // code: key code (bb::core::input)
        KeyUp,
        PadButtonDown,  // pad: slot, code: button index
        PadButtonUp,
        PadAxis,     // pad: slot, axis: index, value: -32768..32767
        PadHat,      // pad: slot, value: kHat* bits
        PadAdded,    // pad: slot
        PadRemoved,  // pad: slot
        Quit,        // the window was closed or the system asks to quit
        FocusLost,   // the window lost the focus or the application is suspended
        FocusGained,
        Text,        // text: what the on-screen keyboard typed (UTF-8)
    };

    static constexpr int kHatUp = 1;
    static constexpr int kHatRight = 2;
    static constexpr int kHatDown = 4;
    static constexpr int kHatLeft = 8;

    Type type = Type::Quit;
    core::input::Code code = 0;
    int pad = -1;
    int axis = 0;
    int value = 0;
    std::string text;
};

// Keyboard, joysticks and window events. Joysticks are identified by a
// stable slot (0, 1, ...): a reconnected pad gets the first free slot.
class InputSource {
public:
    virtual ~InputSource() = default;

    // Next pending event; false when there is none.
    virtual bool poll(InputEvent& event) = 0;

    // Number of slots ever used (some may be empty after a disconnection).
    virtual int padSlots() const = 0;
    virtual bool padConnected(int slot) const = 0;
    virtual std::string padName(int slot) const = 0;
    // Whether the pad is a recognised game controller, whose buttons have
    // the numbers of codes.hpp (kPadButtonA...); other pads report their
    // own numbers. Its triggers come as the buttons kPadButtonLeftTrigger
    // and kPadButtonRightTrigger.
    virtual bool padIsController(int slot) const = 0;

    // Lets `count` players take their controllers before a game: the
    // Switch's controller applet (each player presses L and R, the lights
    // then show the player's number, single Joy-Cons allowed); blocks until
    // it closes. Nothing where the system has no such screen.
    virtual void requestPlayers(int count) = 0;

    // Shows the system's on-screen keyboard (the Switch's) for a short
    // text, which then comes as a Text event; false where there is none (a
    // keyboard types instead). textEntryShown() is false again once the
    // text came or the player cancelled.
    virtual bool beginTextEntry() = 0;
    virtual bool textEntryShown() const = 0;
};

}  // namespace bb::platform
