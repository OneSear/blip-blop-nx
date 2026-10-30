#pragma once

#include "bb/core/gfx/pixmap.hpp"

namespace bb::core::gfx {

// How frames are filtered when they are scaled to the screen: sharp pixels
// (nearest), CRISP or smoothed (bilinear). CRISP first enlarges the frame
// with sharp pixels by the next whole factor, then draws that at the final
// size with bilinear filtering: square, sharp pixels with at most one
// blended pixel at their edges (switch-port.md, 3.2).
enum class Filter { Sharp, Crisp, Smooth };

// Where a frame goes on the screen and how it gets there.
struct Viewport {
    Rect picture;      // in output pixels, centred; the rest is black
    int prescale = 1;  // whole factor CRISP enlarges the frame by first (1: none)
    bool smooth = false;  // whether the last step, to the picture, is bilinear
};

// The picture keeps the frame's shape: as large as the output allows (fit),
// or scaled by the largest whole factor when integer is asked and that
// factor is at least 2 (a factor of 1 would leave most of a large screen
// empty, as 640x480 on the Switch's 1280x720 screen: fit is drawn instead).
// An output of no size gives an empty picture.
Viewport viewport(int frameWidth, int frameHeight, int outputWidth, int outputHeight, bool integer,
                  Filter filter) noexcept;

}  // namespace bb::core::gfx
