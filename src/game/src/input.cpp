#include "bb/game/input.hpp"

#include "bb/core/input/bindings.hpp"
#include "bb/core/input/codes.hpp"

namespace bb::game {

bool InputState::aliasHeld(int a) const {
    namespace input = core::input;
    if (held(alias(a))) {
        return true;
    }
    for (int player = 0; player < input::kPlayerCount; ++player) {
        const int first = AliasControls::firstOf(player);
        if (a < first || a >= first + input::kActionCount) {
            continue;
        }
        for (int i = 0; i < input::kActionCount; ++i) {
            if (input::padSlot(static_cast<input::Code>(alias(first + i))) >= 0) {
                return false;
            }
        }
        // The offsets of AliasControls follow the order of input::kActions.
        for (const input::Code code :
             input::Bindings::defaultPadCodes(player, input::kActions[static_cast<std::size_t>(a - first)])) {
            if (code != 0 && held(code)) {
                return true;
            }
        }
        return false;
    }
    return false;
}

}  // namespace bb::game
