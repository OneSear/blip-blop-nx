#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace bb::core {

// How far the players got in the campaign, saved as a versioned text file
// next to the settings:
//
//   blipblop-progress 1
//   missions 3                  (the missions reached, from 1)
//
// A mission is reached when its briefing shows (bb::game::findMissions());
// the main menu then offers every mission reached. Unknown statements are
// skipped, so that newer files still load.
struct Progress {
    // More missions than a campaign holds (bb.lst has at most 50 parts):
    // every mission reached.
    static constexpr int kAllMissions = 50;

    int missionsReached = 1;

    std::string serialize() const;

    // nullopt (and error) when the header is missing or a value is invalid.
    static std::optional<Progress> parse(std::string_view text, std::string& error);
};

}  // namespace bb::core
