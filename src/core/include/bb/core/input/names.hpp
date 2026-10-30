#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "bb/core/input/codes.hpp"

namespace bb::core::input {

enum class Language { English, French };

// Machine name of a physical input, for configuration files:
// "key:left_ctrl", "key:a", "pad1:button3", "pad2:up" (pads numbered from 1).
// Unknown codes are written as "code:0x...".
std::string codeName(Code code);

// Inverse of codeName(); nullopt for an unknown name.
std::optional<Code> codeFromName(std::string_view name);

// True for the codes of codes.hpp and for valid joystick codes.
bool isKnownCode(Code code);

// The name shown to the player (key binding menu, name entry): keys as the
// 2002 game printed them, "LEFT CONTROL", "A"...; pad inputs by the
// Switch's names, "P1 A", "P2 ZR", "P3 UP". "UNDEFINED" / "INDEFINIE" for
// codes without a name. UTF-8 (one French name has a non-ASCII letter:
// "\u00f9" for the quote key).
std::string displayName(Code code, Language language);

}  // namespace bb::core::input
