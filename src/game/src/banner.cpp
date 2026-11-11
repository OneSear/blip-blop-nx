// The banner texts (texte_cool.cpp).

#include "bb/game/banner.hpp"

#include "bb/game/world.hpp"

namespace bb::game {

Banner::Banner(int text, int row, bool& rowFree, int centre) noexcept
    : text_(text), y_(150 + row * 30), rowFree_(rowFree), centre_(centre), x_(centre + 640) {
    rowFree_ = false;
}

void Banner::update() {
    if (x_ == centre_) {
        ticks_ += 1;
        if (ticks_ >= 350) {
            x_ -= 16;
            rowFree_ = true;
        }
    } else {
        x_ -= 16;
    }
}

void Banner::draw(Canvas& canvas) const {
    canvas.drawText(x_, y_, text_);
}

void showBanner(World& world, int text) {
    // The first free row, or the first row when all are taken.
    int row = 0;
    while (row < Banner::kRows && !world.bannerRowFree[row]) {
        ++row;
    }
    if (row >= Banner::kRows) {
        row = 0;
    }
    world.bannerTexts.add(new Banner(text, row, world.bannerRowFree[row], world.camera.width / 2));
}

void clearBanners(World& world) {
    for (bool& free : world.bannerRowFree) {
        free = true;
    }
}

}  // namespace bb::game
