#include "bb/game/briefing.hpp"

namespace bb::game {

namespace {

// The sheets, where their English is typed (measured on the pictures: the
// first line's baseline is 15 pixels below its top, 27 for a title).
constexpr BriefingSheet kSheets[] = {
    {"data/brief1.gfx", 870, 495, 21, 367, 102, 40, 318},
    {"data/brief2.gfx", 880, 0, 0, 334, 87, 19, 315},  // the dorkemon bonus stage
    {"data/brief3.gfx", 890, 495, 21, 359, 64, 41, 333},
    {"data/brief4.gfx", 900, 471, 22, 345, 92, 12, 315},
    {"data/brief5.gfx", 910, 0, 0, 356, 114, 40, 318},  // the lemmings bonus stage
    {"data/brief6.gfx", 920, 506, 43, 375, 119, 40, 318},
    {"data/brief7.gfx", 930, 482, 24, 352, 71, 40, 318},
};

}  // namespace

const BriefingSheet* findBriefingSheet(std::string_view picture) {
    for (const BriefingSheet& sheet : kSheets) {
        if (picture == sheet.picture) {
            return &sheet;
        }
    }
    return nullptr;
}

bool hasBriefingText(const BriefingSheet& sheet, const TextSource& texts) {
    return texts.text(sheet.firstText + BriefingSheet::kFirstParagraph)[0] != '\0';
}

void typeBriefing(core::gfx::Pixmap& dst, const BriefingSheet& sheet, const core::gfx::BitmapFont& title,
                  const core::gfx::BitmapFont& text, const TextSource& texts, int extra) {
    const auto string = [&](int i) { return texts.text(sheet.firstText + i); };

    title.printCentred(dst, sheet.titleX + extra / 2, sheet.titleY, string(BriefingSheet::kTitle));

    int y = sheet.storyY;
    for (int i = 0; i < BriefingSheet::kParagraphCount; ++i) {
        const int lines = text.printWordWrapped(dst, sheet.storyX, y,
                                                string(BriefingSheet::kFirstParagraph + i), kBriefingRight + extra);
        if (lines > 0) {
            y += (lines + 1) * text.lineHeight();
        }
    }

    text.print(dst, sheet.objectivesX, sheet.objectivesY, string(BriefingSheet::kObjectiveLabel));
    y = sheet.objectivesY + 2 * text.lineHeight();
    for (int i = 0; i < BriefingSheet::kObjectiveCount; ++i) {
        const int lines =
            text.printWordWrapped(dst, sheet.objectivesX, y, string(BriefingSheet::kFirstObjective + i),
                                  sheet.storyX - kBriefingGutter);
        y += lines * text.lineHeight();
    }
}

}  // namespace bb::game
