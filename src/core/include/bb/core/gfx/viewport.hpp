#pragma once

#include <optional>
#include <string_view>

#include "bb/core/gfx/pixmap.hpp"

namespace bb::core::gfx {

// How frames are filtered when they are scaled to the screen: sharp pixels
// (nearest), CRISP or smoothed (bilinear). CRISP first enlarges the frame
// with sharp pixels by the next whole factor, then draws that at the final
// size with bilinear filtering: square, sharp pixels with at most one
// blended pixel at their edges (switch-port.md, 3.2). XBR and CRT are
// shaders, drawn where the display has OpenGL (else as CRISP): XBR smooths
// the edges of the art (an edge-directed enlargement in the manner of
// Hyllian's xBR), CRT shows the frame as a tube showed it (scanlines and an
// aperture grille).
enum class Filter { Sharp, Crisp, Smooth, Xbr, Crt };

// The filters in the order the options menu shows them, and their names in
// the settings file and the log ("sharp", "crisp", "smooth", "xbr", "crt").
constexpr Filter kFilters[] = {Filter::Sharp, Filter::Crisp, Filter::Smooth, Filter::Xbr, Filter::Crt};
const char* filterName(Filter filter) noexcept;
std::optional<Filter> filterFromName(std::string_view name) noexcept;

// Where a frame goes on the screen and how it gets there.
struct Viewport {
    Rect picture;      // in output pixels, centred; the rest is black
    int prescale = 1;  // whole factor CRISP (XBR, CRT) enlarges the frame by first (1: none)
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
