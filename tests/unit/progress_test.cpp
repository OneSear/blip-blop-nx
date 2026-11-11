#include <doctest.h>

#include <string>

#include "bb/core/progress.hpp"

using bb::core::Progress;

TEST_CASE("the progress round-trips through its text form") {
    Progress p;
    CHECK(p.missionsReached == 1);
    p.missionsReached = 4;
    CHECK(p.serialize() == "blipblop-progress 1\nmissions 4\n");

    std::string error;
    const auto back = Progress::parse(p.serialize() + "# a comment\nfuture.thing 2\n", error);
    REQUIRE_MESSAGE(back.has_value(), error);
    CHECK(back->missionsReached == 4);

    CHECK_FALSE(Progress::parse("", error).has_value());
    CHECK(error == "empty file: expected 'blipblop-progress 1'");
    CHECK_FALSE(Progress::parse("blipblop-progress 2\n", error).has_value());
    CHECK_FALSE(Progress::parse("blipblop-progress 1\nmissions 0\n", error).has_value());
    CHECK(error == "line 2: missions must be 1 to 50");
    CHECK_FALSE(Progress::parse("blipblop-progress 1\nmissions x\n", error).has_value());
    CHECK_FALSE(Progress::parse("blipblop-progress 1\nmissions\n", error).has_value());
    const auto all = Progress::parse("blipblop-progress 1\nmissions 50\n", error);
    REQUIRE(all.has_value());
    CHECK(all->missionsReached == Progress::kAllMissions);
}
