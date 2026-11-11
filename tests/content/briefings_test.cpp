// The mission sheets in every language: the text of each sheet has the
// glyphs it needs and, typed as the game types it on a 640-pixel screen and
// on a 16:9 one, lands on the sheet's black or on the BONUS STAGE stamp,
// never on the art and never below the picture (where the game prints its
// prompt). Read from the 2002 data
// directory and from the runtime pack.

#include <doctest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "bb/core/assets/asset_source.hpp"
#include "bb/core/assets/pack.hpp"
#include "bb/core/assets/text_encoding.hpp"
#include "bb/core/gfx/font.hpp"
#include "bb/core/gfx/sprite.hpp"
#include "bb/core/vfs.hpp"
#include "bb/game/briefing.hpp"
#include "bb/game/missions.hpp"
#include "bb/game/services.hpp"

namespace {

namespace gfx = bb::core::gfx;
using bb::core::assets::CampaignEntry;
using bb::game::BriefingSheet;

class Strings final : public bb::game::TextSource {
public:
    explicit Strings(bb::core::assets::StringTable table) : table_(std::move(table)) {}
    const char* text(int id) const override {
        const std::string* s = table_.find(id);
        return s == nullptr ? "" : s->c_str();
    }

private:
    bb::core::assets::StringTable table_;
};

void checkGlyphs(const gfx::BitmapFont& font, std::string_view text) {
    std::size_t i = 0;
    while (i < text.size()) {
        const std::uint32_t c = bb::core::assets::nextCodePoint(text, i);
        CAPTURE(c);
        CHECK(font.has(c));
    }
}

// The stamp's colours: pure reds (tools/art/briefings.py).
bool blackOrStamp(gfx::Pixel p) {
    return gfx::green(p) == 0 && gfx::blue(p) == 0;
}

void checkSheet(const bb::core::assets::AssetSource& source, const BriefingSheet& sheet,
                const bb::game::TextSource& texts, const gfx::BitmapFont& title,
                const gfx::BitmapFont& text) {
    CAPTURE(sheet.picture);
    REQUIRE(bb::game::hasBriefingText(sheet, texts));
    checkGlyphs(title, texts.text(sheet.firstText + BriefingSheet::kTitle));
    for (int i = BriefingSheet::kObjectiveLabel; i < 10; ++i) {
        CAPTURE(sheet.firstText + i);
        checkGlyphs(text, texts.text(sheet.firstText + i));
    }

    gfx::SpriteBank pictures;
    REQUIRE(bb::game::loadSprites(source, sheet.picture, pictures, false));
    REQUIRE(pictures.size() == 2);
    const gfx::Sprite& plain = *pictures[bb::game::kSheetWithoutText];
    REQUIRE(plain.width() == 640);

    // On the 640-pixel screen, and on the 16:9 one of the widescreen option
    // (the picture on the left, the story across).
    for (const int width : {gfx::kScreenWidth, gfx::kWideScreenWidth}) {
        CAPTURE(width);
        gfx::Pixmap before(width, 480);
        gfx::draw(before, plain, 0, 0);
        gfx::Pixmap after = before;
        bb::game::typeBriefing(after, sheet, title, text, texts, width - gfx::kScreenWidth);

        int typed = 0;
        int wrong = 0;
        for (int y = 0; y < 480; ++y) {
            for (int x = 0; x < width; ++x) {
                if (after.at(x, y) != before.at(x, y)) {
                    ++typed;
                    wrong += y >= plain.height() || !blackOrStamp(before.at(x, y)) ? 1 : 0;
                }
            }
        }
        CHECK(typed > 1000);
        CHECK(wrong == 0);
    }
}

void checkBriefings(const bb::core::assets::AssetSource& source) {
    std::string error;
    const auto campaign = source.campaign("data/bb.lst", error);
    REQUIRE_MESSAGE(campaign.has_value(), error);

    gfx::BitmapFont title;
    gfx::BitmapFont text;
    gfx::BitmapFont menu;
    REQUIRE(bb::game::loadFont(source, bb::game::kBriefingTitleFont, title));
    REQUIRE(bb::game::loadFont(source, bb::game::kBriefingTextFont, text));
    REQUIRE(bb::game::loadFont(source, "data/menu.lft", menu));

    // The missions the main menu offers: those of the sheets, each starting
    // with a new world.
    const std::vector<bb::game::Mission> missions = bb::game::findMissions(*campaign);
    REQUIRE(missions.size() == 7);
    const int parts[] = {0, 4, 6, 11, 14, 16, 20};
    for (std::size_t i = 0; i < missions.size(); ++i) {
        CAPTURE(i);
        CHECK(missions[i].part == parts[i]);
        CHECK(bb::game::missionAt(missions, parts[i]) == static_cast<int>(i));
        const CampaignEntry& level = campaign->entries.at(static_cast<std::size_t>(parts[i] + 1));
        CHECK(level.kind == CampaignEntry::Kind::Level);
        CHECK(level.levelKind != 2);  // not an intermediate level
        CHECK(level.levelKind != 3);  // nor the last of a world
    }
    CHECK(bb::game::missionAt(missions, 2) == -1);

    for (const char* table : {"data/uk.dat", "data/fr.dat"}) {
        CAPTURE(table);
        auto strings = source.strings(table, error);
        REQUIRE_MESSAGE(strings.has_value(), error);
        const Strings texts(std::move(*strings));
        int sheets = 0;
        for (const CampaignEntry& entry : campaign->entries) {
            if (entry.kind != CampaignEntry::Kind::Briefing) {
                continue;
            }
            if (const BriefingSheet* sheet = bb::game::findBriefingSheet(entry.path)) {
                checkSheet(source, *sheet, texts, title, text);
                ++sheets;
            }
        }
        CHECK(sheets == 7);

        const char* const english[] = {"MISSION 1", "BONUS STAGE 1", "MISSION 2",    "MISSION 3",
                                       "BONUS STAGE 2", "MISSION 4", "FINAL MISSION"};
        const char* const french[] = {"MISSION 1",      "NIVEAU BONUS 1", "MISSION 2",     "MISSION 3",
                                      "NIVEAU BONUS 2", "MISSION 4",      "MISSION FINALE"};
        const bool uk = std::string_view(table) == "data/uk.dat";
        for (std::size_t i = 0; i < missions.size(); ++i) {
            const std::string name = bb::game::missionName(missions, static_cast<int>(i), texts);
            CHECK(name == (uk ? english[i] : french[i]));
            checkGlyphs(menu, name);
        }
    }
}

}  // namespace

TEST_CASE("the mission sheets of the 2002 data can be typed in every language") {
    const bb::core::Vfs files(BB_TEST_DATA_DIR);
    const bb::core::assets::LegacyDataSource source(files);
    checkBriefings(source);
}

TEST_CASE("the mission sheets of the runtime pack can be typed in every language") {
    std::string error;
    std::shared_ptr<const bb::core::assets::PackFile> pack =
        bb::core::assets::PackFile::open(BB_TEST_PACK, error);
    REQUIRE_MESSAGE(static_cast<bool>(pack), error);
    const bb::core::assets::PackSource source(pack);
    checkBriefings(source);
}
