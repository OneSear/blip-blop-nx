#include "bb/core/input/names.hpp"

#include <cstdio>
#include <cstdlib>

namespace bb::core::input {

#include "input_names_tables.inc"

namespace {

constexpr int kMaxPadSlots = 255;

bool isPadCode(Code code) {
    const int slot = padSlot(code);
    if (slot < 0 || slot >= kMaxPadSlots) {
        return false;
    }
    const Code button = padButton(code);
    return button < 128 || (button >= kPadUp && button <= kPadRight);
}

const char* padDirectionName(Code button) {
    switch (button) {
        case kPadUp:
            return "up";
        case kPadDown:
            return "down";
        case kPadLeft:
            return "left";
        case kPadRight:
            return "right";
        default:
            return nullptr;
    }
}

}  // namespace

std::string codeName(Code code) {
    for (const auto& k : kKeyNames) {
        if (k.code == code) {
            return std::string("key:") + k.name;
        }
    }
    if (isPadCode(code)) {
        const std::string pad = "pad" + std::to_string(padSlot(code) + 1) + ":";
        const Code button = padButton(code);
        if (const char* dir = padDirectionName(button)) {
            return pad + dir;
        }
        return pad + "button" + std::to_string(button);
    }
    char buffer[24];
    std::snprintf(buffer, sizeof(buffer), "code:0x%X", static_cast<unsigned>(code));
    return buffer;
}

std::optional<Code> codeFromName(std::string_view name) {
    if (name.substr(0, 4) == "key:") {
        const std::string_view key = name.substr(4);
        for (const auto& k : kKeyNames) {
            if (key == k.name) {
                return k.code;
            }
        }
        return std::nullopt;
    }
    if (name.substr(0, 3) == "pad") {
        const std::size_t colon = name.find(':');
        if (colon == std::string_view::npos || colon == 3) {
            return std::nullopt;
        }
        const std::string slotText(name.substr(3, colon - 3));
        char* end = nullptr;
        const long slot = std::strtol(slotText.c_str(), &end, 10);
        if (*end != '\0' || slot < 1 || slot > kMaxPadSlots) {
            return std::nullopt;
        }
        const std::string_view what = name.substr(colon + 1);
        for (Code b = kPadUp; b <= kPadRight; ++b) {
            if (what == padDirectionName(b)) {
                return padCode(static_cast<int>(slot - 1), b);
            }
        }
        if (what.substr(0, 6) == "button") {
            const std::string number(what.substr(6));
            const long button = std::strtol(number.c_str(), &end, 10);
            if (!number.empty() && *end == '\0' && button >= 0 && button < 128) {
                return padCode(static_cast<int>(slot - 1), static_cast<Code>(button));
            }
        }
        return std::nullopt;
    }
    if (name.substr(0, 5) == "code:") {
        const std::string number(name.substr(5));
        char* end = nullptr;
        const unsigned long value = std::strtoul(number.c_str(), &end, 0);
        if (!number.empty() && *end == '\0' && value <= 0xFFFFFFFFul) {
            return static_cast<Code>(value);
        }
    }
    return std::nullopt;
}

bool isKnownCode(Code code) {
    for (const auto& k : kKeyNames) {
        if (k.code == code) {
            return true;
        }
    }
    return isPadCode(code);
}

namespace {

// The Switch's name of a game controller's button (SDL's number, by
// position: codes.hpp), or null.
const char* switchButtonName(Code button) {
    switch (button) {
        case kPadButtonA:
            return "B";
        case kPadButtonB:
            return "A";
        case kPadButtonX:
            return "Y";
        case kPadButtonY:
            return "X";
        case kPadButtonBack:
            return "-";
        case kPadButtonGuide:
            return "HOME";
        case kPadButtonStart:
            return "+";
        case kPadButtonLeftStick:
            return "L STICK";
        case kPadButtonRightStick:
            return "R STICK";
        case kPadButtonLeftShoulder:
            return "L";
        case kPadButtonRightShoulder:
            return "R";
        case kPadButtonLeftTrigger:
            return "ZL";
        case kPadButtonRightTrigger:
            return "ZR";
        default:
            return nullptr;
    }
}

}  // namespace

std::string displayName(Code code, Language language) {
    const int slot = padSlot(code);
    if (slot >= 0) {
        // The Switch's names, the pad's number being the player's (the
        // controller's lights): "P1 A", "P2 ZR", "P3 UP". Same wording in
        // both languages; buttons without a Switch name (a pad that is not a
        // game controller) by number.
        const Code button = padButton(code);
        char buffer[40];
        if (const char* dir = padDirectionName(button)) {
            std::string upper(dir);
            for (auto& c : upper) {
                c = static_cast<char>(c - 'a' + 'A');
            }
            std::snprintf(buffer, sizeof(buffer), "P%d %s", slot + 1, upper.c_str());
        } else if (const char* name = switchButtonName(button)) {
            std::snprintf(buffer, sizeof(buffer), "P%d %s", slot + 1, name);
        } else {
            std::snprintf(buffer, sizeof(buffer), "P%d BUTTON%u", slot + 1, static_cast<unsigned>(button));
        }
        return buffer;
    }
    const char* english = nullptr;
    for (const auto& k : kDisplayEnglish) {
        if (k.code == code) {
            english = k.name;
            break;
        }
    }
    if (language == Language::French) {
        // The 2002 French table named DirectInput scan codes, the places of
        // the keys on an AZERTY keyboard ("Q" for the key at A's place).
        // The codes are SDL key codes now, which follow the keyboard's
        // layout: a letter or a sign is the one on the key.
        if (english != nullptr && english[0] != '\0' && english[1] == '\0') {
            return english;
        }
        for (const auto& k : kDisplayFrench) {
            if (k.code == code) {
                return k.name;
            }
        }
        return "INDEFINIE";
    }
    return english != nullptr ? english : "UNDEFINED";
}

}  // namespace bb::core::input
