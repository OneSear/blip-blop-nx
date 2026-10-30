#pragma once

#include "bb/game/player.hpp"

namespace bb::game {

// The game's view of the input of the current tick: the codes held, by
// key code or through the remappable aliases (the 2002 Input).
class InputState {
public:
    virtual ~InputState() = default;
    // Whether the key, special key or pad input `code` is held.
    virtual bool held(unsigned int code) const = 0;
    // The code bound to alias `alias`.
    virtual unsigned int alias(int alias) const = 0;
    // Whether the code bound to `a` is held, or, for a player's alias, an
    // input of the default pad layout (core::input::Bindings::
    // defaultPadCodes()) unless the player bound one of its actions to a pad.
    bool aliasHeld(int a) const;
};

// The seven aliases of a player, from first + 0 (up) to first + 6 (special):
// 0 for the first player, 10 for the second (ALIAS_P1_*, ALIAS_P2_*), 20
// and 30 for the third and fourth (the four-player mode).
class AliasControls : public PlayerControls {
public:
    enum Offset { kUp = 0, kDown = 1, kLeft = 2, kRight = 3, kFire = 4, kJump = 5, kSpecial = 6 };
    static constexpr int kFirstPlayer = 0;
    static constexpr int kSecondPlayer = 10;
    // The first alias of player `player` (0 to 3).
    static constexpr int firstOf(int player) noexcept { return player * 10; }

    AliasControls(const InputState& input, int first) noexcept : input_(input), first_(first) {}

    bool left() const override { return input_.aliasHeld(first_ + kLeft); }
    bool up() const override { return input_.aliasHeld(first_ + kUp); }
    bool right() const override { return input_.aliasHeld(first_ + kRight); }
    bool down() const override { return input_.aliasHeld(first_ + kDown); }
    bool fire() const override { return input_.aliasHeld(first_ + kFire); }
    bool jump() const override { return input_.aliasHeld(first_ + kJump); }
    bool special() const override { return input_.aliasHeld(first_ + kSpecial); }

private:
    const InputState& input_;
    int first_;
};

}  // namespace bb::game
