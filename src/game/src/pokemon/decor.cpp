// The decorative props of dork (fond_poke_alarme_1.h, fond_poke_fleche.h,
// fond_poke_ciel.cpp, fond_poke_level_2.cpp, fond_poke_level_3.cpp).

#include "pokemon/pokemon_world.hpp"

namespace bb::game::pokemon {

void Alarm::update() {
    animation.cycle(3, 14);
    image = levelImage(first_ + animation.frame);
    dieWhenOffscreen();
}

void Arrow::update() {
    static const int kGo[] = {0, 1, 2, 3, 4, 5, 4, 3, 2, 1, 0};
    image = levelImage(prop::kArrow + animation.loop(kGo, 11, 3));
}

//-----------------------------------------------------------------------------

namespace {

constexpr int kSkyWidth = 192;  // the window's width

}  // namespace

SkyWindow::SkyWindow(World& world) : Actor(world) {
    image = levelImage(prop::kSkyWindow);
}

void SkyWindow::update() {
    animation.frame += 4;  // the sky's scroll position
    animation.frame %= 384;
    hitboxFromImage();
}

void SkyWindow::draw() {
    Actor::draw();

    // The visible part of the window, clipped at the screen edges (the
    // 2002 computation; at the right edge the width can go negative).
    const int camera = world().camera.x;
    const int cameraRight = world().camera.right();
    const Image* sky = levelImage(prop::kSky);  // wider than the window, scrolls
    int left = animation.frame;
    int shift = 0;
    int width = 0;
    if (x + kSkyWidth > cameraRight) {
        width = cameraRight - x;
    } else if (x < camera) {
        width = kSkyWidth;
        left = camera - x + animation.frame;
        shift = camera - x;
    } else {
        width = kSkyWidth;
    }
    world().media.canvas->drawPart(sky, left, 0, animation.frame + width, sky->height(), x - camera + shift,
                                   y + 50);

    drawAt(x, y, levelImage(prop::kSkyWindowFrame));
}

//-----------------------------------------------------------------------------

namespace {

// Neon sign images (level bank) and the flicker of each letter: 2 is lit,
// 1 dim, 0 off; a letter flickers for 57 ticks, then stays lit.
constexpr int kNeonTwo = prop::kNeonTwo;
constexpr int kNeonThree = prop::kNeonThree;
constexpr int kNeonL = prop::kNeonL;
constexpr int kNeonE = prop::kNeonE;
constexpr int kNeonV = prop::kNeonV;
constexpr int kFlickerSteps = 57;
const int kFlicker[] = {2, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 1, 2,
                        2, 2, 1, 2, 2, 1, 2, 1, 2, 1, 2, 1, 1, 2, 1, 1, 1, 2, 1, 1,
                        1, 1, 2, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 0, 0};

void advance(int& step, int period) {
    step += 1;
    step %= period;
}

}  // namespace

NeonSign::NeonSign(World& world, bool levelTwo) : Actor(world), levelTwo_(levelTwo) {
    image = levelImage(levelTwo ? kNeonTwo : kNeonThree);
}

void NeonSign::update() {
    // The periods of the digit and of each letter (2002 values).
    if (levelTwo_) {
        advance(animation.frame, 560);
        advance(letterL1_, 155);
        advance(letterE1_, 300);
        advance(letterV_, 210);
        advance(letterE2_, 190);
        advance(letterL2_, 380);
    } else {
        advance(animation.frame, 460);
        advance(letterE1_, 120);
        advance(letterV_, 180);
        advance(letterE2_, 290);
        advance(letterL2_, 190);
    }
    if (animation.frame < kFlickerSteps) {
        image = levelImage((levelTwo_ ? kNeonTwo : kNeonThree) + kFlicker[animation.frame]);
    }
}

void NeonSign::draw() {
    Actor::draw();

    const auto letter = [this](int step, int dx, int first) {
        if (step < kFlickerSteps) {
            drawAt(x + dx, y - 19, levelImage(first + kFlicker[step]));
        }
    };
    // "LEVEL 3" has no first L: its sign is drawn without it.
    if (levelTwo_) {
        letter(letterL1_, 0, kNeonL);
    }
    letter(letterE1_, 12, kNeonE);
    letter(letterV_, 23, kNeonV);
    letter(letterE2_, 36, kNeonE);
    letter(letterL2_, 47, kNeonL);
}

}  // namespace bb::game::pokemon
