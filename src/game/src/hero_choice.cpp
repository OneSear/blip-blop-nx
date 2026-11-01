#include "bb/game/hero_choice.hpp"

#include <cstddef>

namespace bb::game {

namespace {

std::size_t at(int i) {
    return static_cast<std::size_t>(i);
}

}  // namespace

int HeroChoice::holder(int hero) const {
    for (int p = 0; p < kMaxPlayers; ++p) {
        if (seats_[at(p)] == Seat::Ready && cursors_[at(p)] == hero) {
            return p;
        }
    }
    return -1;
}

int HeroChoice::freeFrom(int player, int hero, int step) const {
    for (int i = 0; i < kHeroes; ++i) {
        const int h = ((hero + i * step) % kHeroes + kHeroes) % kHeroes;
        const int p = holder(h);
        if (p == -1 || p == player) {
            return h;
        }
    }
    return hero;  // not reached: four heroes, at most three others
}

void HeroChoice::join(int player) {
    if (seats_.at(at(player)) != Seat::Out) {
        return;
    }
    seats_[at(player)] = Seat::Choosing;
    cursors_[at(player)] = freeFrom(player, player % kHeroes, 1);
}

void HeroChoice::move(int player, int step) {
    if (seats_.at(at(player)) != Seat::Choosing) {
        return;
    }
    cursors_[at(player)] = freeFrom(player, cursors_[at(player)] + step, step);
}

bool HeroChoice::take(int player) {
    if (seats_.at(at(player)) != Seat::Choosing) {
        return false;
    }
    const int hero = cursors_[at(player)];
    if (holder(hero) != -1) {
        return false;
    }
    seats_[at(player)] = Seat::Ready;
    for (int p = 0; p < kMaxPlayers; ++p) {
        if (p != player && seats_[at(p)] == Seat::Choosing && cursors_[at(p)] == hero) {
            cursors_[at(p)] = freeFrom(p, hero + 1, 1);
        }
    }
    return true;
}

void HeroChoice::back(int player) {
    Seat& seat = seats_.at(at(player));
    if (seat == Seat::Ready) {
        seat = Seat::Choosing;
    } else if (seat == Seat::Choosing) {
        seat = Seat::Out;
    }
}

int HeroChoice::joined() const {
    int n = 0;
    for (const Seat s : seats_) {
        n += s != Seat::Out ? 1 : 0;
    }
    return n;
}

bool HeroChoice::everyoneReady() const {
    for (const Seat s : seats_) {
        if (s == Seat::Choosing) {
            return false;
        }
    }
    return joined() > 0;
}

Lineup HeroChoice::lineup() const {
    Lineup lineup;
    for (int p = 0; p < kMaxPlayers; ++p) {
        if (seats_[at(p)] == Seat::Ready) {
            lineup.add(static_cast<Hero>(cursors_[at(p)]), p);
        }
    }
    return lineup;
}

}  // namespace bb::game
