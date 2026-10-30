// Presents a frame through the SDL 2 display and reads it back: checks the
// texture format (channel order) and that nothing is blended or shifted.
// Runs without a screen: SDL's dummy video driver with the software renderer.

#include <doctest.h>

#include <SDL.h>

#include <string>

#include "bb/core/gfx/pixmap.hpp"
#include "bb/core/gfx/viewport.hpp"
#include "bb/platform/sdl2/sdl2_platform.hpp"

namespace gfx = bb::core::gfx;

TEST_CASE("the SDL 2 display shows frames unchanged") {
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);

    std::string error;
    auto platform = bb::platform::sdl2::createPlatform({}, error);
    REQUIRE_MESSAGE(platform != nullptr, error);

    bb::platform::DisplayOptions options;
    auto display = platform->createDisplay(options, error);
    REQUIRE_MESSAGE(display != nullptr, error);

    gfx::Pixmap frame(640, 480);
    for (int y = 0; y < 480; ++y) {
        for (int x = 0; x < 640; ++x) {
            frame.at(x, y) = gfx::makePixel(static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y),
                                            static_cast<std::uint8_t>((x * 7 + y * 3) & 0xFF));
        }
    }
    gfx::Rect red{10, 10, 20, 20};
    gfx::fill(frame, &red, gfx::makePixel(255, 0, 0));

    display->present(frame);

    gfx::Pixmap shown;
    REQUIRE(bb::platform::sdl2::readBack(*display, shown));
    REQUIRE(shown.width() == 640);
    REQUIRE(shown.height() == 480);

    int mismatches = 0;
    for (int y = 0; y < 480; ++y) {
        for (int x = 0; x < 640; ++x) {
            if ((shown.at(x, y) & gfx::kRgbMask) != (frame.at(x, y) & gfx::kRgbMask)) {
                ++mismatches;
            }
        }
    }
    CHECK(mismatches == 0);
    CHECK(gfx::red(shown.at(15, 15)) == 255);
    CHECK(gfx::blue(shown.at(15, 15)) == 0);
}

TEST_CASE("the SDL 2 display fits the frame, by whole factors when asked, CRISP with square pixels") {
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);

    std::string error;
    auto platform = bb::platform::sdl2::createPlatform({}, error);
    REQUIRE_MESSAGE(platform != nullptr, error);

    bb::platform::DisplayOptions options;
    auto display = platform->createDisplay(options, error);
    REQUIRE_MESSAGE(display != nullptr, error);

    // The display's window: the only one open (ids grow from test to test).
    SDL_Window* window = nullptr;
    for (Uint32 id = 1; id < 100 && window == nullptr; ++id) {
        window = SDL_GetWindowFromID(id);
    }
    REQUIRE(window != nullptr);
    const auto resize = [&](int w, int h) {
        SDL_SetWindowSize(window, w, h);
        SDL_PumpEvents();
    };

    // Red on the left half, blue on the right: the edge shows the filter.
    gfx::Pixmap frame(640, 480);
    gfx::Rect left{0, 0, 320, 480};
    gfx::Rect right{320, 0, 320, 480};
    gfx::fill(frame, &left, gfx::makePixel(255, 0, 0));
    gfx::fill(frame, &right, gfx::makePixel(0, 0, 255));
    const auto isRed = [](gfx::Pixel p) { return gfx::red(p) == 255 && gfx::blue(p) == 0; };
    const auto isBlue = [](gfx::Pixel p) { return gfx::red(p) == 0 && gfx::blue(p) == 255; };
    const auto isBlack = [](gfx::Pixel p) { return (p & gfx::kRgbMask) == 0; };
    gfx::Pixmap shown;

    // The Switch's handheld screen: fit, 960x720 between 160-pixel bars.
    resize(1280, 720);
    display->setScaling(false, gfx::Filter::Sharp);
    display->present(frame);
    REQUIRE(bb::platform::sdl2::readBack(*display, shown));
    REQUIRE(shown.width() == 1280);
    REQUIRE(shown.height() == 720);
    CHECK(isBlack(shown.at(159, 360)));
    CHECK(isRed(shown.at(160, 360)));
    CHECK(isBlue(shown.at(1119, 360)));
    CHECK(isBlack(shown.at(1120, 360)));
    CHECK(display->describe().find("picture 960x720 at 160,0, scaling fit, filter sharp") != std::string::npos);

    // CRISP: enlarged by 2 with sharp pixels, then down to 960x720; the edge
    // between the halves blends over one pixel at most.
    display->setScaling(false, gfx::Filter::Crisp);
    display->present(frame);
    REQUIRE(bb::platform::sdl2::readBack(*display, shown));
    int blended = 0;
    for (int x = 160; x < 1120; ++x) {
        const gfx::Pixel p = shown.at(x, 360);
        if (!isRed(p) && !isBlue(p)) {
            ++blended;
        }
        if (x < 638) {
            CHECK(isRed(p));
        } else if (x > 641) {
            CHECK(isBlue(p));
        }
    }
    CHECK(blended <= 1);
    CHECK(display->describe().find("filter crisp (x2 first)") != std::string::npos);

    // Integer scaling with a whole factor of 1 would leave most of the
    // screen empty: fit instead.
    display->setScaling(true, gfx::Filter::Crisp);
    display->present(frame);
    REQUIRE(bb::platform::sdl2::readBack(*display, shown));
    CHECK(isRed(shown.at(160, 360)));

    // Docked, 1920x1080: integer gives 1280x960 at 320,60.
    resize(1920, 1080);
    display->present(frame);
    REQUIRE(bb::platform::sdl2::readBack(*display, shown));
    REQUIRE(shown.width() == 1920);
    CHECK(isBlack(shown.at(319, 540)));
    CHECK(isRed(shown.at(320, 540)));
    CHECK(isBlack(shown.at(960, 59)));
    CHECK(isBlue(shown.at(960, 60)));
    CHECK(isBlue(shown.at(1599, 1019)));
    CHECK(isBlack(shown.at(1600, 1019)));
    CHECK(display->describe().find("picture 1280x960 at 320,60, scaling integer") != std::string::npos);
}
