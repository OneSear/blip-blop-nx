#pragma once

#include <SDL.h>

#include <memory>
#include <string>

#include "bb/platform/display.hpp"

namespace bb::platform::sdl2 {

// What the displays of this backend add for the conformance tests: the
// picture as the screen shows it.
class Sdl2DisplayBase : public Display {
public:
    virtual bool readBack(core::gfx::Pixmap& out) = 0;
};

// The OpenGL context's attributes, set before the window is created: OpenGL
// ES 2.0 on the Switch (EGL, as SDL's own renderer there), the default
// elsewhere (2.1 on macOS).
void prepareOpenGl();

// The display that draws the frames with OpenGL, a shader for each filter
// (core::gfx::Filter: XBR and CRT need it), on window, created with
// SDL_WINDOW_OPENGL, which it takes. Null, with error, when there is no
// OpenGL context, a function is missing or a shader does not build: the
// window is then destroyed, and the caller makes another for the SDL
// renderer's display.
std::unique_ptr<Sdl2DisplayBase> createGlDisplay(SDL_Window* window, const DisplayOptions& options,
                                                 std::string& error);

}  // namespace bb::platform::sdl2
