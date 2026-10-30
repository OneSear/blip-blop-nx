#include <doctest.h>

#include "bb/core/gfx/viewport.hpp"

namespace gfx = bb::core::gfx;

namespace {

bool is(const gfx::Rect& r, int x, int y, int w, int h) {
    return r.x == x && r.y == y && r.w == w && r.h == h;
}

}  // namespace

TEST_CASE("handheld: the 1280x720 screen shows the frame at 960x720") {
    const gfx::Viewport crisp = gfx::viewport(640, 480, 1280, 720, false, gfx::Filter::Crisp);
    CHECK(is(crisp.picture, 160, 0, 960, 720));  // 160-pixel bars at the sides
    CHECK(crisp.prescale == 2);                   // 1280x960 with sharp pixels,
    CHECK(crisp.smooth);                          // then filtered down

    const gfx::Viewport sharp = gfx::viewport(640, 480, 1280, 720, false, gfx::Filter::Sharp);
    CHECK(is(sharp.picture, 160, 0, 960, 720));
    CHECK(sharp.prescale == 1);
    CHECK_FALSE(sharp.smooth);

    const gfx::Viewport smooth = gfx::viewport(640, 480, 1280, 720, false, gfx::Filter::Smooth);
    CHECK(smooth.prescale == 1);
    CHECK(smooth.smooth);

    // A whole factor of 1 would leave two thirds of the screen empty.
    const gfx::Viewport integer = gfx::viewport(640, 480, 1280, 720, true, gfx::Filter::Crisp);
    CHECK(is(integer.picture, 160, 0, 960, 720));
}

TEST_CASE("docked: 1920x1080 shows the frame at 1440x1080, or 1280x960 by a whole factor") {
    const gfx::Viewport fit = gfx::viewport(640, 480, 1920, 1080, false, gfx::Filter::Crisp);
    CHECK(is(fit.picture, 240, 0, 1440, 1080));
    CHECK(fit.prescale == 3);  // 1920x1440, then filtered down
    CHECK(fit.smooth);

    const gfx::Viewport integer = gfx::viewport(640, 480, 1920, 1080, true, gfx::Filter::Crisp);
    CHECK(is(integer.picture, 320, 60, 1280, 960));
    CHECK(integer.prescale == 1);  // exact with sharp pixels
    CHECK_FALSE(integer.smooth);
}

TEST_CASE("a window: whole multiples are drawn with sharp pixels, other sizes keep the frame's shape") {
    const gfx::Viewport same = gfx::viewport(640, 480, 640, 480, false, gfx::Filter::Crisp);
    CHECK(is(same.picture, 0, 0, 640, 480));
    CHECK(same.prescale == 1);
    CHECK_FALSE(same.smooth);

    const gfx::Viewport retina = gfx::viewport(640, 480, 1280, 960, false, gfx::Filter::Crisp);
    CHECK(is(retina.picture, 0, 0, 1280, 960));
    CHECK_FALSE(retina.smooth);

    const gfx::Viewport tall = gfx::viewport(640, 480, 640, 800, false, gfx::Filter::Crisp);
    CHECK(is(tall.picture, 0, 160, 640, 480));

    // Smaller than the frame: filtered down, nothing to enlarge first.
    const gfx::Viewport small = gfx::viewport(640, 480, 320, 240, false, gfx::Filter::Crisp);
    CHECK(is(small.picture, 0, 0, 320, 240));
    CHECK(small.prescale == 1);
    CHECK(small.smooth);

    const gfx::Viewport none = gfx::viewport(640, 480, 0, 0, false, gfx::Filter::Crisp);
    CHECK(none.picture.empty());
}
