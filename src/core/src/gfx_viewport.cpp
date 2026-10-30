#include "bb/core/gfx/viewport.hpp"

namespace bb::core::gfx {

Viewport viewport(int frameWidth, int frameHeight, int outputWidth, int outputHeight, bool integer,
                  Filter filter) noexcept {
    Viewport v;
    if (frameWidth <= 0 || frameHeight <= 0 || outputWidth <= 0 || outputHeight <= 0) {
        return v;
    }

    const int across = outputWidth / frameWidth;
    const int down = outputHeight / frameHeight;
    const int factor = across < down ? across : down;
    int w = 0;
    int h = 0;
    if (integer && factor >= 2) {
        w = frameWidth * factor;
        h = frameHeight * factor;
    } else if (static_cast<long long>(outputWidth) * frameHeight >=
               static_cast<long long>(outputHeight) * frameWidth) {
        // The output is wider than the frame: the whole height, bars at the
        // sides.
        h = outputHeight;
        w = static_cast<int>(static_cast<long long>(frameWidth) * outputHeight / frameHeight);
    } else {
        w = outputWidth;
        h = static_cast<int>(static_cast<long long>(frameHeight) * outputWidth / frameWidth);
    }
    v.picture = Rect{(outputWidth - w) / 2, (outputHeight - h) / 2, w, h};

    const bool whole = w % frameWidth == 0 && h % frameHeight == 0 && w / frameWidth == h / frameHeight;
    switch (filter) {
        case Filter::Sharp:
            break;
        case Filter::Smooth:
            v.smooth = true;
            break;
        case Filter::Crisp:
            // A whole multiple is exact with sharp pixels; a larger picture
            // is enlarged by the next whole factor, then filtered down; a
            // smaller one is only filtered.
            if (!whole) {
                v.smooth = true;
                if (w > frameWidth) {
                    v.prescale = (w + frameWidth - 1) / frameWidth;
                }
            }
            break;
    }
    return v;
}

}  // namespace bb::core::gfx
