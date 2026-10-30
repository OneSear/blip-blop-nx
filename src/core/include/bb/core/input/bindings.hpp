#pragma once

#include <array>
#include <string_view>

#include "bb/core/input/codes.hpp"

namespace bb::core::input {

// What a player can do; one physical input is bound to each action.
enum class Action { Up, Down, Left, Right, Fire, Jump, Special };

constexpr int kActionCount = 7;
// Blip, Blop and, in the four-player mode, Bleach and Blitz. Players 3 and
// 4 have no keys by default: they play with pads 3 and 4.
constexpr int kPlayerCount = 4;

constexpr std::array<Action, kActionCount> kActions = {
    Action::Up, Action::Down, Action::Left, Action::Right, Action::Fire, Action::Jump, Action::Special};

// "up", "down", "left", "right", "fire", "jump", "special".
std::string_view actionName(Action action);

class Bindings {
public:
    // The defaults of the 2002 game (config.cpp): arrows, Left Ctrl to fire,
    // Left Alt to jump, Space for the special for player 1; Q S D F, Tab,
    // G, H for player 2.
    static Bindings defaults();

    // The pad layout a player gets without binding any pad input (see
    // usesPad()): pad 1 for the first player, pad 2 for the second, and so
    // on; the Switch's (switch-port.md, 4.2): the D-pad or the left stick to
    // move, A (the right button) to fire, B (bottom) to jump, Y (left) for
    // the special; ZR also fires and ZL also drops the special. Each action
    // has up to two inputs; an unused one is 0.
    static std::array<Code, 2> defaultPadCodes(int player, Action action);

    Code get(int player, Action action) const;
    // Whether one of the player's actions is bound to a pad input; the
    // default pad layout then no longer applies to the player.
    bool usesPad(int player) const;
    void set(int player, Action action, Code code);

    bool operator==(const Bindings& other) const { return codes_ == other.codes_; }
    bool operator!=(const Bindings& other) const { return !(*this == other); }

private:
    std::array<std::array<Code, kActionCount>, kPlayerCount> codes_{};
};

}  // namespace bb::core::input
