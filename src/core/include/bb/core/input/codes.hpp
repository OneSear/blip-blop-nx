#pragma once

#include <cstdint>

namespace bb::core::input {

// Physical input codes, as used by the legacy game, its configuration file
// (bb.cfg) and the replay files:
//   * keys: the SDL 2 key code when it is below 255 (printable characters,
//     Return, Escape, ...), otherwise 0x40000000 | (scan code & 0xFFF);
//   * joysticks: ((slot + 1) << 10) | button, with kPadUp..kPadRight for the
//     D-pad or stick directions.
// The values are SDL 2's, frozen here so that the game no longer needs SDL
// to name a key (generated from SDL_keycode.h).
using Code = std::uint32_t;

constexpr Code kKeyBackspace = 0x00000008;       // SDLK_BACKSPACE
constexpr Code kKeyTab = 0x00000009;             // SDLK_TAB
constexpr Code kKeyReturn = 0x0000000D;          // SDLK_RETURN
constexpr Code kKeyEscape = 0x0000001B;          // SDLK_ESCAPE
constexpr Code kKeySpace = 0x00000020;           // SDLK_SPACE
constexpr Code kKeyExclaim = 0x00000021;         // SDLK_EXCLAIM
constexpr Code kKeyQuote = 0x00000027;           // SDLK_QUOTE
constexpr Code kKeyComma = 0x0000002C;           // SDLK_COMMA
constexpr Code kKeyMinus = 0x0000002D;           // SDLK_MINUS
constexpr Code kKeyPeriod = 0x0000002E;          // SDLK_PERIOD
constexpr Code kKeySlash = 0x0000002F;           // SDLK_SLASH
constexpr Code kKey0 = 0x00000030;               // SDLK_0
constexpr Code kKey1 = 0x00000031;               // SDLK_1
constexpr Code kKey2 = 0x00000032;               // SDLK_2
constexpr Code kKey3 = 0x00000033;               // SDLK_3
constexpr Code kKey4 = 0x00000034;               // SDLK_4
constexpr Code kKey5 = 0x00000035;               // SDLK_5
constexpr Code kKey6 = 0x00000036;               // SDLK_6
constexpr Code kKey7 = 0x00000037;               // SDLK_7
constexpr Code kKey8 = 0x00000038;               // SDLK_8
constexpr Code kKey9 = 0x00000039;               // SDLK_9
constexpr Code kKeySemicolon = 0x0000003B;       // SDLK_SEMICOLON
constexpr Code kKeyEquals = 0x0000003D;          // SDLK_EQUALS
constexpr Code kKeyLeftBracket = 0x0000005B;     // SDLK_LEFTBRACKET
constexpr Code kKeyBackslash = 0x0000005C;       // SDLK_BACKSLASH
constexpr Code kKeyRightBracket = 0x0000005D;    // SDLK_RIGHTBRACKET
constexpr Code kKeyA = 0x00000061;               // SDLK_a
constexpr Code kKeyB = 0x00000062;               // SDLK_b
constexpr Code kKeyC = 0x00000063;               // SDLK_c
constexpr Code kKeyD = 0x00000064;               // SDLK_d
constexpr Code kKeyE = 0x00000065;               // SDLK_e
constexpr Code kKeyF = 0x00000066;               // SDLK_f
constexpr Code kKeyG = 0x00000067;               // SDLK_g
constexpr Code kKeyH = 0x00000068;               // SDLK_h
constexpr Code kKeyI = 0x00000069;               // SDLK_i
constexpr Code kKeyJ = 0x0000006A;               // SDLK_j
constexpr Code kKeyK = 0x0000006B;               // SDLK_k
constexpr Code kKeyL = 0x0000006C;               // SDLK_l
constexpr Code kKeyM = 0x0000006D;               // SDLK_m
constexpr Code kKeyN = 0x0000006E;               // SDLK_n
constexpr Code kKeyO = 0x0000006F;               // SDLK_o
constexpr Code kKeyP = 0x00000070;               // SDLK_p
constexpr Code kKeyQ = 0x00000071;               // SDLK_q
constexpr Code kKeyR = 0x00000072;               // SDLK_r
constexpr Code kKeyS = 0x00000073;               // SDLK_s
constexpr Code kKeyT = 0x00000074;               // SDLK_t
constexpr Code kKeyU = 0x00000075;               // SDLK_u
constexpr Code kKeyV = 0x00000076;               // SDLK_v
constexpr Code kKeyW = 0x00000077;               // SDLK_w
constexpr Code kKeyX = 0x00000078;               // SDLK_x
constexpr Code kKeyY = 0x00000079;               // SDLK_y
constexpr Code kKeyZ = 0x0000007A;               // SDLK_z
constexpr Code kKeyDelete = 0x0000007F;          // SDLK_DELETE
constexpr Code kKeyCapsLock = 0x40000039;        // SDLK_CAPSLOCK
constexpr Code kKeyF1 = 0x4000003A;              // SDLK_F1
constexpr Code kKeyF2 = 0x4000003B;              // SDLK_F2
constexpr Code kKeyF3 = 0x4000003C;              // SDLK_F3
constexpr Code kKeyF4 = 0x4000003D;              // SDLK_F4
constexpr Code kKeyF5 = 0x4000003E;              // SDLK_F5
constexpr Code kKeyF6 = 0x4000003F;              // SDLK_F6
constexpr Code kKeyF7 = 0x40000040;              // SDLK_F7
constexpr Code kKeyF8 = 0x40000041;              // SDLK_F8
constexpr Code kKeyF9 = 0x40000042;              // SDLK_F9
constexpr Code kKeyF10 = 0x40000043;             // SDLK_F10
constexpr Code kKeyF11 = 0x40000044;             // SDLK_F11
constexpr Code kKeyF12 = 0x40000045;             // SDLK_F12
constexpr Code kKeyScrollLock = 0x40000047;      // SDLK_SCROLLLOCK
constexpr Code kKeyInsert = 0x40000049;          // SDLK_INSERT
constexpr Code kKeyHome = 0x4000004A;            // SDLK_HOME
constexpr Code kKeyPageUp = 0x4000004B;          // SDLK_PAGEUP
constexpr Code kKeyEnd = 0x4000004D;             // SDLK_END
constexpr Code kKeyPageDown = 0x4000004E;        // SDLK_PAGEDOWN
constexpr Code kKeyRight = 0x4000004F;           // SDLK_RIGHT
constexpr Code kKeyLeft = 0x40000050;            // SDLK_LEFT
constexpr Code kKeyDown = 0x40000051;            // SDLK_DOWN
constexpr Code kKeyUp = 0x40000052;              // SDLK_UP
constexpr Code kKeyNumLock = 0x40000053;         // SDLK_NUMLOCKCLEAR
constexpr Code kKeyKeypadDivide = 0x40000054;    // SDLK_KP_DIVIDE
constexpr Code kKeyKeypadMultiply = 0x40000055;  // SDLK_KP_MULTIPLY
constexpr Code kKeyKeypadPlus = 0x40000057;      // SDLK_KP_PLUS
constexpr Code kKeyKeypadEnter = 0x40000058;     // SDLK_KP_ENTER
constexpr Code kKeyKeypad1 = 0x40000059;         // SDLK_KP_1
constexpr Code kKeyKeypad2 = 0x4000005A;         // SDLK_KP_2
constexpr Code kKeyKeypad3 = 0x4000005B;         // SDLK_KP_3
constexpr Code kKeyKeypad4 = 0x4000005C;         // SDLK_KP_4
constexpr Code kKeyKeypad5 = 0x4000005D;         // SDLK_KP_5
constexpr Code kKeyKeypad6 = 0x4000005E;         // SDLK_KP_6
constexpr Code kKeyKeypad7 = 0x4000005F;         // SDLK_KP_7
constexpr Code kKeyKeypad8 = 0x40000060;         // SDLK_KP_8
constexpr Code kKeyKeypad9 = 0x40000061;         // SDLK_KP_9
constexpr Code kKeyKeypad0 = 0x40000062;         // SDLK_KP_0
constexpr Code kKeyF13 = 0x40000068;             // SDLK_F13
constexpr Code kKeyF14 = 0x40000069;             // SDLK_F14
constexpr Code kKeyF15 = 0x4000006A;             // SDLK_F15
constexpr Code kKeyMenu = 0x40000076;            // SDLK_MENU
constexpr Code kKeyKeypadDecimal = 0x400000DC;   // SDLK_KP_DECIMAL
constexpr Code kKeyLeftCtrl = 0x400000E0;        // SDLK_LCTRL
constexpr Code kKeyLeftShift = 0x400000E1;       // SDLK_LSHIFT
constexpr Code kKeyLeftAlt = 0x400000E2;         // SDLK_LALT
constexpr Code kKeyRightCtrl = 0x400000E4;       // SDLK_RCTRL
constexpr Code kKeyRightShift = 0x400000E5;      // SDLK_RSHIFT
constexpr Code kKeyRightAlt = 0x400000E6;        // SDLK_RALT

constexpr Code kPadUp = 100;
constexpr Code kPadDown = 101;
constexpr Code kPadLeft = 102;
constexpr Code kPadRight = 103;

// Buttons of the pads the platform recognises as game controllers (SDL 2's
// SDL_GameControllerButton): the same numbers for every such pad, on every
// system, by position. Other pads report their own button numbers. The
// Switch's labels are not SDL's: its A is SDL's B (right), its B SDL's A
// (bottom), its X SDL's Y (top), its Y SDL's X (left).
constexpr Code kPadButtonA = 0;  // bottom face button (Switch B, Xbox A, PlayStation cross)
constexpr Code kPadButtonB = 1;  // right (Switch A)
constexpr Code kPadButtonX = 2;  // left (Switch Y)
constexpr Code kPadButtonY = 3;  // top (Switch X)
constexpr Code kPadButtonBack = 4;  // Switch -
constexpr Code kPadButtonGuide = 5;
constexpr Code kPadButtonStart = 6;  // Switch +
constexpr Code kPadButtonLeftStick = 7;
constexpr Code kPadButtonRightStick = 8;
constexpr Code kPadButtonLeftShoulder = 9;   // Switch L
constexpr Code kPadButtonRightShoulder = 10;  // Switch R
// The triggers, which SDL reports as axes: as buttons, past SDL 2's 21
// buttons so that a later SDL cannot take their numbers.
constexpr Code kPadButtonLeftTrigger = 32;   // Switch ZL (SL on a single Joy-Con)
constexpr Code kPadButtonRightTrigger = 33;  // Switch ZR (SR on a single Joy-Con)

constexpr Code padCode(int slot, Code button) noexcept {
    return (static_cast<Code>(slot + 1) << 10) | button;
}

// Slot of a joystick code (0-based), or -1 for keys.
constexpr int padSlot(Code code) noexcept {
    return code < 0x40000000u && (code >> 10) != 0 ? static_cast<int>(((code >> 10) & 0xFFu)) - 1 : -1;
}

constexpr Code padButton(Code code) noexcept {
    return code & 0x3FFu;
}

// Code of an SDL 2 key code (see above).
constexpr Code fromSdl2Key(std::int32_t key) noexcept {
    const auto k = static_cast<Code>(key);
    return k < 255 ? k : (0x40000000u | (k & 0xFFFu));
}

}  // namespace bb::core::input
