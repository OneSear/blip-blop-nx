// The keyboard and the pads (input.cpp of the legacy port).

#include "device_input.hpp"

#include <cstring>
#include <string>

#include "bb/core/input/codes.hpp"
#include "bb/core/log.hpp"
#include "bb/platform/input_source.hpp"
#include "session.hpp"

namespace bb::app {

namespace {

constexpr int kDeadZone = 4200;
constexpr unsigned int kPadUp = 100;
constexpr unsigned int kPadDown = 101;
constexpr unsigned int kPadLeft = 102;
constexpr unsigned int kPadRight = 103;

}  // namespace

DeviceInput::DeviceInput(platform::InputSource& source, Session& session)
    : source_(source), session_(session) {}

void DeviceInput::open() {
    // The platform opens the pads; slots above kPads are ignored.
    pads_ = source_.padSlots();
    if (pads_ > kPads) {
        pads_ = kPads;
    }
    for (int i = 0; i < pads_; ++i) {
        if (source_.padConnected(i)) {
            logPad(i);
        }
    }
}

void DeviceInput::logPad(int slot) const {
    core::Log::info(
        "Pad " + std::to_string(slot + 1) + ": " + source_.padName(slot) +
        (source_.padIsController(slot) ? " (game controller)" : " (joystick, own button numbers)"));
}

// A direction is held while the D-pad (or hat) or the stick holds it.
void DeviceInput::updateDirections(int pad) {
    Pad& p = live_.pads[pad];
    const Directions& h = hats_[pad];
    const Directions& s = sticks_[pad];
    p.up = (h.up | s.up) != 0 ? 1 : 0;
    p.down = (h.down | s.down) != 0 ? 1 : 0;
    p.left = (h.left | s.left) != 0 ? 1 : 0;
    p.right = (h.right | s.right) != 0 ? 1 : 0;
}

void DeviceInput::update() {
    using Event = platform::InputEvent;
    bool quitRequested = false;
    Event e;

    while (source_.poll(e)) {
        switch (e.type) {
            case Event::Type::Quit:
                quitRequested = true;
                break;
            case Event::Type::FocusLost:
            case Event::Type::FocusGained:
                liveUnfocused_ = e.type == Event::Type::FocusLost;
                break;
            case Event::Type::Text:
                if (textEntry_) {
                    text_ += e.text;
                    textTyped_ = true;
                }
                break;
            case Event::Type::KeyDown:
            case Event::Type::KeyUp: {
                const char down = e.type == Event::Type::KeyDown ? 1 : 0;
                if (e.code < 255) {
                    live_.keys[e.code] = down;
                } else if ((e.code & 0xFFF) < 0xFFF) {
                    live_.specials[e.code & 0xFFF] = down;
                }
                break;
            }
            case Event::Type::PadAdded:
                if (e.pad >= 0 && e.pad < kPads) {
                    if (e.pad + 1 > pads_) {
                        pads_ = e.pad + 1;
                    }
                    logPad(e.pad);
                }
                break;
            case Event::Type::PadRemoved:
                if (e.pad >= 0 && e.pad < kPads) {
                    core::Log::info("Pad " + std::to_string(e.pad + 1) + " removed");
                    std::memset(&live_.pads[e.pad], 0, sizeof(Pad));
                    hats_[e.pad] = Directions{};
                    sticks_[e.pad] = Directions{};
                }
                break;
            case Event::Type::PadButtonDown:
            case Event::Type::PadButtonUp:
                if (e.pad >= 0 && e.pad < kPads && e.code < 128) {
                    live_.pads[e.pad].buttons[e.code] = e.type == Event::Type::PadButtonDown ? 1 : 0;
                }
                break;
            case Event::Type::PadHat:
                if (e.pad >= 0 && e.pad < kPads) {
                    Directions& h = hats_[e.pad];
                    h.up = (e.value & Event::kHatUp) != 0 ? 1 : 0;
                    h.right = (e.value & Event::kHatRight) != 0 ? 1 : 0;
                    h.down = (e.value & Event::kHatDown) != 0 ? 1 : 0;
                    h.left = (e.value & Event::kHatLeft) != 0 ? 1 : 0;
                    updateDirections(e.pad);
                }
                break;
            case Event::Type::PadAxis:
                if (e.pad < 0 || e.pad >= kPads) {
                    break;
                }
                if (e.axis == 0) {
                    sticks_[e.pad].left = e.value < -kDeadZone ? 1 : 0;
                    sticks_[e.pad].right = e.value > kDeadZone ? 1 : 0;
                }
                if (e.axis == 1) {
                    sticks_[e.pad].up = e.value < -kDeadZone ? 1 : 0;
                    sticks_[e.pad].down = e.value > kDeadZone ? 1 : 0;
                }
                updateDirections(e.pad);
                break;
            default:
                break;
        }
    }

    publish(quitRequested);
}

void DeviceInput::publish(bool quitRequested) {
    const bool wasUnfocused = unfocused_;

    if (!session_.deterministic()) {
        visible_ = live_;
        unfocused_ = liveUnfocused_;
        if (quitRequested) {
            session_.requestQuit();
        }
    } else {
        // The recorded input of the frame replaces the live one (which is
        // recorded first when recording).
        struct Live : Session::InputSource {
            const Devices& d;
            int pads;
            Live(const Devices& devices, int padCount) : d(devices), pads(padCount) {}
            void forEachPressed(void (*callback)(std::uint32_t, void*), void* context) const override {
                for (int i = 0; i < 255; ++i) {
                    if (d.keys[i] != 0) {
                        callback(static_cast<std::uint32_t>(i), context);
                    }
                }
                for (int i = 0; i < 0xFFF; ++i) {
                    if (d.specials[i] != 0) {
                        callback(0x40000000u | static_cast<std::uint32_t>(i), context);
                    }
                }
                for (int k = 0; k < pads; ++k) {
                    const Pad& p = d.pads[k];
                    const std::uint32_t base = static_cast<std::uint32_t>(k + 1) << 10;
                    for (int b = 0; b < 128; ++b) {
                        if (p.buttons[b] != 0) {
                            callback(base | static_cast<std::uint32_t>(b), context);
                        }
                    }
                    if (p.up != 0) {
                        callback(base | kPadUp, context);
                    }
                    if (p.down != 0) {
                        callback(base | kPadDown, context);
                    }
                    if (p.left != 0) {
                        callback(base | kPadLeft, context);
                    }
                    if (p.right != 0) {
                        callback(base | kPadRight, context);
                    }
                }
            }
        };
        struct Visible : Session::InputSink {
            Devices& d;
            bool unfocused = false;
            explicit Visible(Devices& devices) : d(devices) {}
            void setUnfocused(bool u) override { unfocused = u; }
            void clear() override { std::memset(&d, 0, sizeof(Devices)); }
            void press(std::uint32_t code) override {
                const std::uint32_t slot = (code >> 10) & 0xFF;
                if (code >= 0x40000000u) {
                    if ((code & 0xFFFu) < 0xFFFu) {
                        d.specials[code & 0xFFFu] = 1;
                    }
                } else if (slot > 0) {
                    if (slot > static_cast<std::uint32_t>(kPads)) {
                        return;
                    }
                    Pad& p = d.pads[slot - 1];
                    const std::uint32_t b = code & 0x3FFu;
                    if (b == kPadUp) {
                        p.up = 1;
                    } else if (b == kPadDown) {
                        p.down = 1;
                    } else if (b == kPadLeft) {
                        p.left = 1;
                    } else if (b == kPadRight) {
                        p.right = 1;
                    } else if (b < 128) {
                        p.buttons[b] = 1;
                    }
                } else if (code < 255) {
                    d.keys[code] = 1;
                }
            }
        };

        const Live live(live_, pads_);
        Visible visible(visible_);
        if (session_.filterInput(live, quitRequested, liveUnfocused_, visible)) {
            session_.requestQuit();
        }
        unfocused_ = visible.unfocused;
    }

    // A focus loss is kept until takeFocusLost() or the focus comes back.
    if (unfocused_ && !wasUnfocused) {
        focusLost_ = true;
    } else if (!unfocused_) {
        focusLost_ = false;
    }
}

bool DeviceInput::takeFocusLost() {
    const bool lost = focusLost_;
    focusLost_ = false;
    return lost;
}

bool DeviceInput::backHeld() const {
    // The Switch's B: the bottom button.
    for (int k = 0; k < pads_; ++k) {
        if (visible_.pads[k].buttons[core::input::kPadButtonA] != 0) {
            return true;
        }
    }
    return false;
}

void DeviceInput::requestPlayers(int count) {
    // The applet blocks and changes nothing a recording could replay.
    if (!session_.deterministic()) {
        source_.requestPlayers(count);
    }
}

bool DeviceInput::beginTextEntry() {
    // Recordings hold keys, not text: deterministic runs type the name.
    if (session_.deterministic() || !source_.beginTextEntry()) {
        return false;
    }
    textEntry_ = true;
    textTyped_ = false;
    text_.clear();
    return true;
}

game::Input::TextEntry DeviceInput::textEntry(std::string& text) {
    if (!textEntry_) {
        return TextEntry::Cancelled;
    }
    if (textTyped_) {
        textEntry_ = false;
        text = text_;
        return TextEntry::Done;
    }
    if (!source_.textEntryShown()) {
        textEntry_ = false;
        return TextEntry::Cancelled;
    }
    return TextEntry::Typing;
}

bool DeviceInput::held(unsigned int code) const {
    namespace input = core::input;
    // The pads also confirm and go back in the menus as the Switch does: A
    // (the right button) as Return, + and - (Start and Back) as Escape
    // (pause, skip a cutscene).
    if (code == input::kKeyReturn || code == input::kKeyEscape) {
        for (int k = 0; k < pads_; ++k) {
            const Pad& p = visible_.pads[k];
            if (code == input::kKeyReturn
                    ? p.buttons[input::kPadButtonB] != 0
                    : p.buttons[input::kPadButtonStart] != 0 || p.buttons[input::kPadButtonBack] != 0) {
                return true;
            }
        }
    }
    if (code < 0x40000000u && ((code >> 10) & 0xFF) > 0) {
        // Pads are numbered from 1.
        const unsigned int pad = (code >> 10) & 0xFF;
        const unsigned int b = code & 0xFF;
        if (pad > static_cast<unsigned int>(kPads) ||
            (b >= 128 && b != kPadUp && b != kPadDown && b != kPadLeft && b != kPadRight)) {
            return false;
        }
        const Pad& p = visible_.pads[pad - 1];
        if (b == kPadUp) {
            return p.up != 0;
        }
        if (b == kPadDown) {
            return p.down != 0;
        }
        if (b == kPadRight) {
            return p.right != 0;
        }
        if (b == kPadLeft) {
            return p.left != 0;
        }
        return p.buttons[b] != 0;
    }
    if (code < 255) {
        return visible_.keys[code] != 0;
    }
    return visible_.specials[code & 0xFFF] != 0;
}

// Keys first, then special keys, pad buttons (by button, then pad) and pad
// directions (down, left, right, up for each pad): the order of the 2014
// waitKey().
bool DeviceInput::firstHeld(unsigned int& code) const {
    for (unsigned int i = 0; i < 255; ++i) {
        if (visible_.keys[i] != 0) {
            code = i;
            return true;
        }
    }
    for (unsigned int i = 0; i < 0xFFF; ++i) {
        if (visible_.specials[i] != 0) {
            code = i | 0x40000000u;
            return true;
        }
    }
    for (unsigned int b = 0; b < 128; ++b) {
        for (int k = 0; k < pads_; ++k) {
            if (visible_.pads[k].buttons[b] == 1) {
                code = static_cast<unsigned int>(k + 1) * (1u << 10) + b;
                return true;
            }
        }
    }
    for (int k = 0; k < pads_; ++k) {
        const Pad& p = visible_.pads[k];
        const unsigned int base = static_cast<unsigned int>(k + 1) * (1u << 10);
        if (p.down != 0) {
            code = base + kPadDown;
            return true;
        }
        if (p.left != 0) {
            code = base + kPadLeft;
            return true;
        }
        if (p.right != 0) {
            code = base + kPadRight;
            return true;
        }
        if (p.up != 0) {
            code = base + kPadUp;
            return true;
        }
    }
    return false;
}

bool DeviceInput::anyHeld() const {
    unsigned int code = 0;
    return firstHeld(code);
}

}  // namespace bb::app
