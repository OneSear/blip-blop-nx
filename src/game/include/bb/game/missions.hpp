#pragma once

#include <string>
#include <vector>

#include "bb/core/assets/asset_types.hpp"
#include "bb/game/briefing.hpp"

namespace bb::game {

// The missions of the campaign: each starts at a briefing of bb.lst that is
// a mission sheet (brief1.gfx to brief7.gfx: five missions and two bonus
// stages; the briefings between them have no text), and runs to the next.
// Each starts with a new world (the levels after them are of the kinds
// first, bonus, complete or end), where the players drop in afresh, so
// that a campaign can start at any of them: the saved progress
// (core::Progress) counts the missions reached, and the main menu offers
// them.
struct Mission {
    int part = 0;  // the briefing's index in bb.lst
    const BriefingSheet* sheet = nullptr;
};

// The missions of a campaign, in its order; none when it has no mission
// sheet (a campaign then simply starts at its first part).
std::vector<Mission> findMissions(const core::assets::Campaign& campaign);

// The mission whose briefing is part `part`, or -1.
int missionAt(const std::vector<Mission>& missions, int part);

// A mission's name in the menu: the sheet's title ("MISSION 1"), or for the
// bonus stages, whose sheets have none, "BONUS STAGE 1", "BONUS STAGE 2".
std::string missionName(const std::vector<Mission>& missions, int mission, const TextSource& texts);

}  // namespace bb::game
