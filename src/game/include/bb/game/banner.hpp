#pragma once

#include "bb/game/media.hpp"

namespace bb::game {

// A text that slides in from the right, stays centred for 350 ticks and
// slides out to the left, on one of 10 rows (kill streaks and messages from
// level scripts; the 2002 TexteCool). Centred on the view: x 320 in 4:3.
class Banner {
public:
    static constexpr int kRows = 10;

    // String `text` of the string table on `row`, whose free flag the
    // banner clears now and sets again when it starts to leave.
    Banner(int text, int row, bool& rowFree, int centre = 320) noexcept;

    void update();
    void draw(Canvas& canvas) const;
    // True once the text has left the screen.
    bool finished() const noexcept { return x_ <= -320; }

private:
    int text_;
    int y_;
    bool& rowFree_;
    int centre_;
    int x_;  // from 640 pixels right of the centre, 16 per tick
    int ticks_ = 0;
};

}  // namespace bb::game
