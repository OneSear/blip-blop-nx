#include "bb/core/gfx/sprite.hpp"

#include "bb/core/assets/asset_types.hpp"
#include "bb/core/assets/image16.hpp"

namespace bb::core::gfx {

void draw(Pixmap& dst, const Sprite& sprite, int x, int y) {
    x -= sprite.spotX();
    y -= sprite.spotY();
    const int right = x + sprite.width();
    const int bottom = y + sprite.height();

    const int width = dst.width();
    const int height = dst.height();
    if (x > width || right < 0 || y > height || bottom < 0) {
        return;
    }

    const int left = x < 0 ? -x : 0;
    const int top = y < 0 ? -y : 0;
    const int cutRight = right > width ? width - x : sprite.width();
    const int cutBottom = bottom > height ? height - y : sprite.height();

    const Rect part{left, top, cutRight - left, cutBottom - top};
    blit(dst, x < 0 ? 0 : x, y < 0 ? 0 : y, sprite.pixels(), &part);
}

const Sprite& SpriteBank::missing() {
    static const Sprite sprite = [] {
        Pixmap pixel(1, 1);
        pixel.at(0, 0) = makePixel(255, 0, 255);
        pixel.setColorKey(pixel.at(0, 0));
        return Sprite(std::move(pixel), 0, 0);
    }();
    return sprite;
}

void SpriteBank::assign(const assets::SpriteBank& bank, bool colorKey) {
    sprites_.clear();
    sprites_.reserve(bank.images.size());
    for (const assets::SpriteImage& image : bank.images) {
        sprites_.push_back(std::make_unique<Sprite>(assets::toPixmap(image.image, colorKey), image.hotspotX,
                                                    image.hotspotY));
    }
}

}  // namespace bb::core::gfx
