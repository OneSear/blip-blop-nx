#pragma once

#include <memory>
#include <vector>

#include "bb/core/gfx/pixmap.hpp"

namespace bb::core::assets {
struct SpriteBank;
}

namespace bb::core::gfx {

// The screen the game draws on (XPIC_MAX, YPIC_MAX of the 2002 code).
constexpr int kScreenWidth = 640;
constexpr int kScreenHeight = 480;
// The screen of the widescreen option: 16:9 at 480 lines.
constexpr int kWideScreenWidth = 854;

// An image with a hotspot, the point placed at the drawing position (the
// 2002 Picture).
class Sprite {
public:
    Sprite(Pixmap pixels, int spotX, int spotY) : pixels_(std::move(pixels)), spotX_(spotX), spotY_(spotY) {}

    int width() const noexcept { return pixels_.width(); }
    int height() const noexcept { return pixels_.height(); }
    int spotX() const noexcept { return spotX_; }
    int spotY() const noexcept { return spotY_; }
    const Pixmap& pixels() const noexcept { return pixels_; }

private:
    Pixmap pixels_;
    int spotX_;
    int spotY_;
};

// Draws sprite with its hotspot at (x, y), with its colour key (the 2002
// Picture::BlitTo and PasteTo, which drew alike). Nothing is drawn when the
// sprite lies beyond dst; a sprite crossing its right or bottom edge is cut
// there. (2002 cut at the 640x480 screen whatever the destination; the game
// draws on the 640-pixel screen, or on the wider one of a level shown in
// 16:9.)
void draw(Pixmap& dst, const Sprite& sprite, int x, int y);

// A bank of sprites addressed by index (the 2002 PictureBank). An index
// outside the bank gives a transparent 1x1 sprite (D-40 fixed: 2002 read
// past the bank).
class SpriteBank {
public:
    // Replaces the content by the images of bank; with colorKey, each image
    // gets the transparent colour of its 16-bit layout.
    void assign(const assets::SpriteBank& bank, bool colorKey = true);
    void clear() noexcept { sprites_.clear(); }

    int size() const noexcept { return static_cast<int>(sprites_.size()); }
    bool empty() const noexcept { return sprites_.empty(); }
    const Sprite* operator[](int index) const {
        return index >= 0 && index < size() ? sprites_[static_cast<std::size_t>(index)].get() : &missing();
    }
    const Sprite* image(int index) const { return (*this)[index]; }

private:
    static const Sprite& missing();

    std::vector<std::unique_ptr<Sprite>> sprites_;
};

}  // namespace bb::core::gfx
