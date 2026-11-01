#include <doctest.h>

#include "bb/game/hero_choice.hpp"

using bb::game::Hero;
using bb::game::HeroChoice;
using Seat = HeroChoice::Seat;

TEST_CASE("hero choice: a player joins on its own hero and takes it") {
    HeroChoice c;
    CHECK(c.joined() == 0);
    CHECK_FALSE(c.everyoneReady());

    c.join(2);
    CHECK(c.seat(2) == Seat::Choosing);
    CHECK(c.cursor(2) == static_cast<int>(Hero::Bleach));
    CHECK_FALSE(c.everyoneReady());

    CHECK(c.take(2));
    CHECK(c.seat(2) == Seat::Ready);
    CHECK(c.holder(static_cast<int>(Hero::Bleach)) == 2);
    CHECK(c.everyoneReady());

    const bb::game::Lineup lineup = c.lineup();
    REQUIRE(lineup.count == 1);
    CHECK(lineup.heroes[0] == Hero::Bleach);
    CHECK(lineup.controls[0] == 2);
}

TEST_CASE("hero choice: the cursor wraps around and skips the heroes taken") {
    HeroChoice c;
    c.join(0);
    c.move(0, -1);
    CHECK(c.cursor(0) == static_cast<int>(Hero::Blitz));
    c.move(0, 1);
    CHECK(c.cursor(0) == static_cast<int>(Hero::Blip));

    c.join(1);
    CHECK(c.take(1));  // Blop
    c.move(0, 1);
    CHECK(c.cursor(0) == static_cast<int>(Hero::Bleach));
    c.move(0, -1);
    CHECK(c.cursor(0) == static_cast<int>(Hero::Blip));

    // Taken, a player's own hero is not where it joins.
    HeroChoice d;
    d.join(0);
    d.move(0, 1);
    CHECK(d.take(0));  // Blop
    d.join(1);
    CHECK(d.cursor(1) == static_cast<int>(Hero::Bleach));
}

TEST_CASE("hero choice: a hero is taken once; the others' cursors move on") {
    HeroChoice c;
    c.join(0);
    c.join(1);
    c.move(1, -1);  // player 2 on Blip as well
    CHECK(c.cursor(1) == static_cast<int>(Hero::Blip));
    CHECK(c.take(0));
    CHECK(c.cursor(1) == static_cast<int>(Hero::Blop));
    CHECK_FALSE(c.everyoneReady());
    CHECK(c.take(1));
    CHECK(c.everyoneReady());
    CHECK_FALSE(c.take(1));  // already ready
}

TEST_CASE("hero choice: back gives the hero back, then leaves") {
    HeroChoice c;
    c.join(0);
    c.join(3);
    CHECK(c.take(0));
    CHECK(c.take(3));
    CHECK(c.everyoneReady());

    c.back(3);
    CHECK(c.seat(3) == Seat::Choosing);
    CHECK(c.holder(static_cast<int>(Hero::Blitz)) == -1);
    CHECK_FALSE(c.everyoneReady());
    c.back(3);
    CHECK(c.seat(3) == Seat::Out);
    CHECK(c.everyoneReady());
    CHECK(c.joined() == 1);

    c.back(0);
    c.back(0);
    CHECK(c.joined() == 0);
    CHECK_FALSE(c.everyoneReady());
    c.back(0);  // nothing more
    CHECK(c.seat(0) == Seat::Out);
}

TEST_CASE("hero choice: the lineup follows the controls, not the order of joining") {
    HeroChoice c;
    c.join(3);
    c.join(1);
    c.move(3, 1);  // Blitz -> Blip
    CHECK(c.take(3));
    CHECK(c.take(1));  // Blop

    const bb::game::Lineup lineup = c.lineup();
    REQUIRE(lineup.count == 2);
    CHECK(lineup.heroes[0] == Hero::Blop);
    CHECK(lineup.controls[0] == 1);
    CHECK(lineup.heroes[1] == Hero::Blip);
    CHECK(lineup.controls[1] == 3);
}

TEST_CASE("hero choice: four players, four heroes") {
    HeroChoice c;
    for (int p = 0; p < 4; ++p) {
        c.join(p);
    }
    for (int p = 3; p >= 0; --p) {
        CHECK(c.take(p));
    }
    const bb::game::Lineup lineup = c.lineup();
    REQUIRE(lineup.count == 4);
    for (int p = 0; p < 4; ++p) {
        CHECK(lineup.heroes[p] == static_cast<Hero>(p));
        CHECK(lineup.controls[p] == p);
    }
}
