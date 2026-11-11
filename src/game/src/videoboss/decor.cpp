// The props of videoboss (fond_lcd.cpp, fond_worldmap.cpp,
// fond_electronics.cpp, fond_ecraneige.h, fond_ecrancamera.cpp,
// fond_assembleur.cpp, fond_windows.h, fond_dos.h, fond_bossventilo.cpp).

#include "videoboss/videoboss_world.hpp"

namespace bb::game::videoboss {

void TableProp::update() {
    image = levelImage(first_ + animation.loop(table_, steps_, delay_));
}

void Electronics::update() {
    animation.cycle(2, 15);
    image = levelImage(prop::kElectronics + animation.frame);
}

void Electronics::draw() {
    Actor::draw();
    if (overlay_ >= 0) {
        drawAt(x, y, levelImage(overlay_));
    }
}

void ScreenLoop::update() {
    animation.cycle(delay_, steps_);
    image = levelImage(first_ + animation.frame);
    dieWhenOffscreen();
}

void CameraScreen::update() {
    animation.cycle(87, 3);
    image = levelImage(prop::kCameraScreen + animation.frame);
}

void CameraScreen::draw() {
    Actor::draw();
    drawAt(x, y + animation.subFrame, levelImage(prop::kCameraScreenBar));
}

void Assembler::update() {
    animation.frame += 1;
    animation.frame %= 432;
}

void Assembler::draw() {
    // An 87-pixel window on the listing, clipped at the right of the
    // screen.
    const int camera = world().camera.x;
    const int cameraRight = world().camera.right();
    const int width = x + 75 > cameraRight ? cameraRight - x : 75;
    world().media.canvas->drawPart(levelImage(prop::kAssembler), 0, animation.frame, width,
                                   87 + animation.frame, x - camera, y);
}

void WindowsScreen::update() {
    animation.cycle(180, 4);
    // Two steps on the second image (the bank's 55 is unused).
    int picture = prop::kWindows1;
    if (animation.frame == 1 || animation.frame == 2) {
        picture = prop::kWindows2;
    } else if (animation.frame == 3) {
        picture = prop::kWindows3;
    }
    image = levelImage(picture);
    dieWhenOffscreen();
}

}  // namespace bb::game::videoboss
