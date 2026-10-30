// The SDL 2 input backend with a virtual game controller (no pad needed):
// what the game sees of a pad SDL recognises.

#include <doctest.h>

#include <SDL.h>

#include <memory>
#include <string>

#include "bb/core/input/codes.hpp"
#include "bb/platform/input_source.hpp"
#include "bb/platform/sdl2/sdl2_platform.hpp"

#if SDL_VERSION_ATLEAST(2, 24, 0)

namespace {

using bb::platform::InputEvent;
namespace input = bb::core::input;

// The next event of `type`, skipping the others; false after `tries` polls.
bool next(bb::platform::InputSource& source, InputEvent::Type type, InputEvent& event, int tries = 100) {
    for (int i = 0; i < tries; ++i) {
        while (source.poll(event)) {
            if (event.type == type) {
                return true;
            }
        }
        SDL_Delay(1);
    }
    return false;
}

}  // namespace

TEST_CASE(
    "a game controller reports SDL's button numbers, its D-pad as a hat, its left stick as axes 0 and 1") {
    bb::platform::sdl2::Options options;
    options.headless = true;
    std::string error;
    auto platform = bb::platform::sdl2::createPlatform(options, error);
    REQUIRE_MESSAGE(platform != nullptr, error);
    bb::platform::InputSource& source = platform->input();

    SDL_VirtualJoystickDesc desc;
    SDL_zero(desc);
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    desc.naxes = SDL_CONTROLLER_AXIS_MAX;
    desc.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
    desc.name = "bb virtual controller";
    const int index = SDL_JoystickAttachVirtualEx(&desc);
    REQUIRE_MESSAGE(index >= 0, SDL_GetError());
    REQUIRE(SDL_IsGameController(index));

    InputEvent e;
    REQUIRE(next(source, InputEvent::Type::PadAdded, e));
    const int slot = e.pad;
    CHECK(source.padConnected(slot));
    CHECK(source.padIsController(slot));
    CHECK(source.padName(slot).empty() == false);

    // The test's own handle on the device, to move its buttons and axes.
    SDL_Joystick* pad = SDL_JoystickOpen(index);
    REQUIRE(pad != nullptr);

    SDL_JoystickSetVirtualButton(pad, SDL_CONTROLLER_BUTTON_A, 1);
    REQUIRE(next(source, InputEvent::Type::PadButtonDown, e));
    CHECK(e.pad == slot);
    CHECK(e.code == input::kPadButtonA);
    SDL_JoystickSetVirtualButton(pad, SDL_CONTROLLER_BUTTON_A, 0);
    REQUIRE(next(source, InputEvent::Type::PadButtonUp, e));
    CHECK(e.code == input::kPadButtonA);

    SDL_JoystickSetVirtualButton(pad, SDL_CONTROLLER_BUTTON_START, 1);
    REQUIRE(next(source, InputEvent::Type::PadButtonDown, e));
    CHECK(e.code == input::kPadButtonStart);

    SDL_JoystickSetVirtualButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT, 1);
    REQUIRE(next(source, InputEvent::Type::PadHat, e));
    CHECK(e.value == InputEvent::kHatLeft);
    SDL_JoystickSetVirtualButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP, 1);
    REQUIRE(next(source, InputEvent::Type::PadHat, e));
    CHECK(e.value == (InputEvent::kHatLeft | InputEvent::kHatUp));
    SDL_JoystickSetVirtualButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT, 0);
    REQUIRE(next(source, InputEvent::Type::PadHat, e));
    CHECK(e.value == InputEvent::kHatUp);

    // (SDL may first report the other axes at rest.)
    SDL_JoystickSetVirtualAxis(pad, SDL_CONTROLLER_AXIS_LEFTY, 30000);
    bool moved = false;
    while (!moved && next(source, InputEvent::Type::PadAxis, e)) {
        moved = e.axis == 1 && e.value > 20000;
    }
    CHECK(moved);

    // The triggers come as buttons (the Switch's ZL and ZR).
    SDL_JoystickSetVirtualAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 32767);
    bool pressed = false;
    while (!pressed && next(source, InputEvent::Type::PadButtonDown, e)) {
        pressed = e.code == input::kPadButtonRightTrigger;
    }
    CHECK(pressed);
    SDL_JoystickSetVirtualAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, -32768);
    bool released = false;
    while (!released && next(source, InputEvent::Type::PadButtonUp, e)) {
        released = e.code == input::kPadButtonRightTrigger;
    }
    CHECK(released);

    SDL_JoystickClose(pad);
    REQUIRE(SDL_JoystickDetachVirtual(index) == 0);
    REQUIRE(next(source, InputEvent::Type::PadRemoved, e));
    CHECK(e.pad == slot);
    CHECK_FALSE(source.padConnected(slot));
}

#endif
