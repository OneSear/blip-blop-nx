#include <doctest.h>

#include "bb/core/assets/asset_types.hpp"
#include "bb/core/gfx/font.hpp"
#include "bb/core/gfx/sprite.hpp"

namespace gfx = bb::core::gfx;

namespace {

constexpr gfx::Pixel kInk = gfx::makePixel(255, 0, 0);
constexpr gfx::Pixel kPaper = gfx::makePixel(0, 0, 0);

gfx::Sprite square(int size, int spotX, int spotY) {
    return gfx::Sprite(gfx::Pixmap(size, size, kInk), spotX, spotY);
}

int inked(const gfx::Pixmap& p) {
    int n = 0;
    for (int y = 0; y < p.height(); ++y) {
        for (int x = 0; x < p.width(); ++x) {
            n += p.at(x, y) == kInk ? 1 : 0;
        }
    }
    return n;
}

}  // namespace

TEST_CASE("A sprite is drawn at its hotspot") {
    gfx::Pixmap screen(640, 480, kPaper);
    gfx::draw(screen, square(4, 1, 2), 10, 10);
    CHECK(screen.at(9, 8) == kInk);
    CHECK(screen.at(12, 11) == kInk);
    CHECK(screen.at(13, 8) == kPaper);
    CHECK(inked(screen) == 16);
}

TEST_CASE("A sprite is cut at the screen edges, and skipped beyond them") {
    gfx::Pixmap screen(640, 480, kPaper);
    gfx::draw(screen, square(10, 0, 0), -4, 476);  // 6 columns, 4 rows visible
    CHECK(inked(screen) == 24);

    gfx::Pixmap other(640, 480, kPaper);
    gfx::draw(other, square(10, 0, 0), 641, 0);  // past the right edge
    gfx::draw(other, square(10, 0, 0), 0, -11);  // above the top
    CHECK(inked(other) == 0);
}

TEST_CASE("A sprite is cut at x 640 even on a wider pixmap (the 2002 BlitTo)") {
    gfx::Pixmap wide(840, 480, kPaper);
    gfx::draw(wide, square(10, 0, 0), 635, 0);
    CHECK(inked(wide) == 50);
}

TEST_CASE("A font advances by glyph widths and the space width") {
    bb::core::assets::Font data;
    data.lineHeight = 12;
    data.spaceWidth = 3;
    bb::core::assets::Image16 glyph;
    glyph.width = 5;
    glyph.height = 7;
    glyph.pixels.assign(35, 0x7C00);  // red in RGB555, not the key
    data.glyphs['A'] = glyph;

    gfx::BitmapFont font;
    font.assign(data);
    CHECK(font.loaded());
    CHECK(font.width("AA A") == 18);
    CHECK(font.width("A?") == 5);  // no glyph: skipped
    CHECK(font.width(nullptr) == 0);

    gfx::Pixmap screen(640, 480, kPaper);
    font.printCentred(screen, 100, 0, "AA");  // starts at 95
    CHECK(screen.at(94, 0) == kPaper);
    CHECK(screen.at(95, 0) != kPaper);
    CHECK(screen.at(104, 6) != kPaper);
    CHECK(screen.at(105, 0) == kPaper);

    gfx::Pixmap wrapped(640, 480, kPaper);
    CHECK(font.printWordWrapped(wrapped, 0, 0, "AA  AA", 12) == 2);  // the second word goes on line 2
    CHECK(wrapped.at(0, 12) != kPaper);
    CHECK(wrapped.at(13, 0) == kPaper);
    CHECK(font.lineHeight() == 12);

    // The lines from y to the last: a first word too wide leaves line 1
    // empty; no word, no line.
    CHECK(font.printWordWrapped(wrapped, 0, 100, "AA AA", 640) == 1);
    CHECK(font.printWordWrapped(wrapped, 0, 200, "AAA", 12) == 2);
    CHECK(font.printWordWrapped(wrapped, 0, 300, "  ", 640) == 0);
    CHECK(font.printWordWrapped(wrapped, 0, 300, nullptr, 640) == 0);
}

TEST_CASE("A sprite bank gives a transparent 1x1 sprite outside its range (D-40)") {
    bb::core::assets::SpriteBank data;
    data.images.resize(2);
    for (auto& image : data.images) {
        image.image.width = 2;
        image.image.height = 2;
        image.image.pixels.assign(4, 0x7C00);
    }
    gfx::SpriteBank bank;
    bank.assign(data);
    REQUIRE(bank.size() == 2);
    CHECK(bank[1]->width() == 2);
    for (const int index : {-1, 2, 1000}) {
        CAPTURE(index);
        const gfx::Sprite* missing = bank[index];
        REQUIRE(missing != nullptr);
        CHECK(missing->width() == 1);
        gfx::Pixmap screen(4, 4, kPaper);
        gfx::draw(screen, *missing, 1, 1);
        CHECK(inked(screen) == 0);
        CHECK(screen.at(1, 1) == kPaper);
    }
}
