#pragma once

#include <array>

#include "bb/game/player.hpp"

namespace bb::game {

// The rules of the hero choice (CharacterSelectScreen), apart from input
// and drawing. Up to four players, each on its own controls, join; a
// player who joined moves a cursor over the four heroes and takes the one
// under it, if no one else took it. The game starts once someone joined
// and everyone who joined took a hero.
class HeroChoice {
public:
    static constexpr int kHeroes = 4;  // Blip, Blop, Bleach, Blitz (Hero values)

    enum class Seat {
        Out,       // has not joined
        Choosing,  // joined, the cursor on a hero
        Ready      // took the hero under the cursor
    };

    // Joins (Out only): the cursor on the player's own hero (player 1 Blip,
    // player 2 Blop, ...), or the next one no one took.
    void join(int player);
    // Moves the cursor (Choosing only) one hero to the right (step 1) or
    // left (-1), around, past the heroes someone took.
    void move(int player, int step);
    // Takes the hero under the cursor (Choosing only); false if someone
    // took it. The cursors of the others on that hero move on.
    bool take(int player);
    // Gives the hero back (Ready), or leaves (Choosing).
    void back(int player);

    Seat seat(int player) const { return seats_.at(static_cast<std::size_t>(player)); }
    // The hero under a player's cursor (a Hero value; Choosing or Ready).
    int cursor(int player) const { return cursors_.at(static_cast<std::size_t>(player)); }
    // The player who took a hero, or -1.
    int holder(int hero) const;
    // How many players joined.
    int joined() const;
    // Someone joined and everyone who joined took a hero.
    bool everyoneReady() const;
    // The players who took a hero, in the order of their controls: player
    // 1 of the campaign is the first of them.
    Lineup lineup() const;

private:
    // From `hero` on, in the direction of `step`, the first hero no other
    // player than `player` took.
    int freeFrom(int player, int hero, int step) const;

    std::array<Seat, kMaxPlayers> seats_{};
    std::array<int, kMaxPlayers> cursors_{};
};

}  // namespace bb::game
