#include "bb/game/missions.hpp"

#include <cstdio>

namespace bb::game {

namespace {

constexpr int kBonusStageText = 838;  // "BONUS STAGE %d"

bool hasTitle(const Mission& mission, const TextSource& texts) {
    return texts.text(mission.sheet->firstText + BriefingSheet::kTitle)[0] != '\0';
}

}  // namespace

std::vector<Mission> findMissions(const core::assets::Campaign& campaign) {
    std::vector<Mission> missions;
    for (std::size_t i = 0; i < campaign.entries.size(); ++i) {
        const core::assets::CampaignEntry& entry = campaign.entries[i];
        if (entry.kind != core::assets::CampaignEntry::Kind::Briefing) {
            continue;
        }
        if (const BriefingSheet* sheet = findBriefingSheet(entry.path)) {
            missions.push_back(Mission{static_cast<int>(i), sheet});
        }
    }
    return missions;
}

int missionAt(const std::vector<Mission>& missions, int part) {
    for (std::size_t i = 0; i < missions.size(); ++i) {
        if (missions[i].part == part) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::string missionName(const std::vector<Mission>& missions, int mission, const TextSource& texts) {
    const Mission& m = missions.at(static_cast<std::size_t>(mission));
    if (hasTitle(m, texts)) {
        return texts.text(m.sheet->firstText + BriefingSheet::kTitle);
    }
    // The bonus stages are numbered among themselves.
    int bonus = 0;
    for (int i = 0; i <= mission; ++i) {
        bonus += hasTitle(missions[static_cast<std::size_t>(i)], texts) ? 0 : 1;
    }
    char name[64];
    std::snprintf(name, sizeof(name), texts.text(kBonusStageText), bonus);  // NOLINT(cert-err33-c)
    return name;
}

}  // namespace bb::game
