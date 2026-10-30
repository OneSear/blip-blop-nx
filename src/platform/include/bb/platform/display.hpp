#pragma once

#include <string>

#include "bb/core/gfx/pixmap.hpp"
#include "bb/core/gfx/viewport.hpp"

namespace bb::platform {

struct DisplayOptions {
    std::string title = "Blip & Blop";
    int width = 640;  // logical size of the frames presented
    int height = 480;
    bool integerScaling = false;  // see Display::setScaling()
    core::gfx::Filter filter = core::gfx::Filter::Crisp;
    bool headless = false;  // never opens a window, present() does nothing
};

// Shows finished frames. Frames are software-rendered pixmaps
// (migration.md DEC-01); the display only uploads and scales them, always
// in step with the screen's refresh (vsync). On the Switch the picture
// fills the screen of the console's mode, 1280x720 in handheld mode and
// 1920x1080 docked, and follows the mode when it changes; elsewhere it
// fills a window.
class Display {
public:
    virtual ~Display() = default;

    // Shows the frame, scaled to the screen with its aspect ratio kept
    // (core::gfx::viewport()), black bars around it.
    virtual void present(const core::gfx::Pixmap& frame) = 0;

    // How frames are scaled: to fill the screen or by the largest whole
    // factor of 2 or more; with sharp pixels, CRISP or smoothed.
    virtual void setScaling(bool integer, core::gfx::Filter filter) = 0;

    // Refresh rate of the screen in Hz, 0 when unknown.
    virtual int refreshRate() const = 0;

    // For the log: the window's size, the size it is drawn at in pixels,
    // the renderer, the picture and the scaling ("" for a headless display).
    virtual std::string describe() const = 0;

    // Whether present() waits for the screen's refresh (false for a headless
    // display or a renderer without vsync).
    virtual bool vsync() const = 0;
};

}  // namespace bb::platform
