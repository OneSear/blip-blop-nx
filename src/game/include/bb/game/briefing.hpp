#pragma once

#include <string_view>

#include "bb/core/gfx/font.hpp"
#include "bb/core/gfx/pixmap.hpp"
#include "bb/game/media.hpp"

namespace bb::game {

// The mission sheets: the briefings of bb.lst that tell a mission
// (brief1.gfx to brief7.gfx; the pictures between them have no text). The
// 2002 pictures hold their English text. In another language the game draws
// the sheet without its text (the second image of the bank) and types the
// strings of that language where the English was, in the typewriter faces
// cut from the sheets (tools/art/briefings.py).
struct BriefingSheet {
    // The ten strings of a sheet from firstText: the title (none on the
    // bonus stages' sheets), the objectives' label, two objectives, the
    // paragraphs of the story. An empty string is not typed.
    static constexpr int kTitle = 0;
    static constexpr int kObjectiveLabel = 1;
    static constexpr int kFirstObjective = 2;
    static constexpr int kObjectiveCount = 2;
    static constexpr int kFirstParagraph = 4;
    static constexpr int kParagraphCount = 6;

    const char* picture = nullptr;  // the bank, as bb.lst names it
    int firstText = 0;
    // Where the English was typed: the centre of the title, the left of
    // the story and of the objectives, and the top of their first lines.
    int titleX = 0;
    int titleY = 0;
    int storyX = 0;
    int storyY = 0;
    int objectivesX = 0;
    int objectivesY = 0;
};

// The image of a sheet's bank without its text, and the faces of the text
// and of the titles.
constexpr int kSheetWithoutText = 1;
constexpr const char* kBriefingTextFont = "data/brief.lft";
constexpr const char* kBriefingTitleFont = "data/mission.lft";

// The lines of the story end before this x; those of the objectives end
// this far left of the story.
constexpr int kBriefingRight = 636;
constexpr int kBriefingGutter = 12;

// The sheet a briefing picture is, or null (a picture without text).
const BriefingSheet* findBriefingSheet(std::string_view picture);

// Whether texts has the sheet's text (a story), so that it can be typed.
bool hasBriefingText(const BriefingSheet& sheet, const TextSource& texts);

// Types the sheet's text on dst (the sheet without text drawn first):
// the title centred, the story's paragraphs word-wrapped one blank line
// apart, the objectives' label, a blank line and the objectives.
void typeBriefing(core::gfx::Pixmap& dst, const BriefingSheet& sheet, const core::gfx::BitmapFont& title,
                  const core::gfx::BitmapFont& text, const TextSource& texts);

}  // namespace bb::game
