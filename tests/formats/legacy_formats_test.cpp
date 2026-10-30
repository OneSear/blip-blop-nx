// Reads every shipped data file with the bb::core legacy readers, writes it
// back and compares bytes (migration.md Phase 2: converters must round-trip
// data.zip bit-exactly where the format allows).

#include <doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "bb/core/assets/legacy_formats.hpp"
#include "bb/core/assets/text_encoding.hpp"

namespace fs = std::filesystem;
using namespace bb::core::assets;

namespace {

const fs::path kData = BB_TEST_DATA_DIR;

Bytes readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    Bytes bytes(static_cast<std::size_t>(fs::file_size(path)));
    in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return bytes;
}

std::vector<fs::path> dataFiles(const std::string& extension) {
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(kData)) {
        if (entry.path().extension() == extension) {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

std::string text(const Bytes& b) { return std::string(b.begin(), b.end()); }

}  // namespace

TEST_CASE("pixel expansion matches the 2002 decoder tables") {
    // The decoded colour keys the 2002 banks compare against.
    CHECK(toPixel(colorKey16(PixelFormat16::Rgb555), PixelFormat16::Rgb555) == bb::core::gfx::makePixel(246, 205, 148));
    CHECK(toPixel(colorKey16(PixelFormat16::Rgb565), PixelFormat16::Rgb565) == bb::core::gfx::makePixel(246, 210, 148));
    // fromPixel inverts toPixel for every 16-bit value.
    for (std::uint32_t v = 0; v < 0x10000; ++v) {
        const auto value = static_cast<std::uint16_t>(v);
        CHECK_EQ(fromPixel(toPixel(value, PixelFormat16::Rgb565), PixelFormat16::Rgb565), value);
        if (v < 0x8000) {
            CHECK_EQ(fromPixel(toPixel(value, PixelFormat16::Rgb555), PixelFormat16::Rgb555), value);
        }
    }
}

TEST_CASE("every .gfx bank round-trips bit for bit") {
    const auto files = dataFiles(".gfx");
    CHECK(files.size() == 56 + 2);  // and bleach.gfx, blitz.gfx (the four-player mode)
    std::size_t images = 0;
    std::size_t rgb565 = 0;
    for (const auto& path : files) {
        CAPTURE(path.filename().string());
        const Bytes original = readFile(path);
        std::string error;
        const auto bank = readGfx(original, error);
        REQUIRE_MESSAGE(bank.has_value(), error);
        images += bank->images.size();
        for (const auto& sprite : bank->images) {
            rgb565 += sprite.image.format == PixelFormat16::Rgb565 ? 1 : 0;
            CHECK(sprite.image.pixels.size() ==
                  static_cast<std::size_t>(sprite.image.width) * static_cast<std::size_t>(sprite.image.height));
        }
        CHECK(writeGfx(*bank) == original);
    }
    // migration.md A.2: 3,755 images in 2002, 191 of them LGX version 0;
    // plus the 34 Big Smurf images added to snufenn in Phase 5a (version 1)
    // and Bleach's and Blitz's 149 each (recoloured blip.gfx and blop.gfx).
    CHECK(images == 3755 + 34 + 2 * 149);
    CHECK(rgb565 == 191);
}

TEST_CASE("every .lft font round-trips bit for bit") {
    const auto files = dataFiles(".lft");
    CHECK(files.size() == 8 + 2);  // and scorep.lft, scoreg.lft (the four-player mode)
    for (const auto& path : files) {
        CAPTURE(path.filename().string());
        const Bytes original = readFile(path);
        std::string error;
        const auto font = readLft(original, error);
        REQUIRE_MESSAGE(font.has_value(), error);
        const auto written = writeLft(*font, error);
        REQUIRE_MESSAGE(written.has_value(), error);
        CHECK(*written == original);
    }
    std::string error;
    const auto rpg = readLft(readFile(kData / "rpg.lft"), error);
    REQUIRE(rpg.has_value());
    // The accented glyphs of rpg.lft (D-73), now by code point.
    for (const std::uint32_t cp : {0xE0u, 0xE8u, 0xE9u, 0xEAu, 0xF4u}) {
        CHECK(rpg->glyphs.count(cp) == 1);
    }
    CHECK(rpg->glyphs.size() == 84);
}

TEST_CASE("every .sfx bank round-trips bit for bit and holds PCM mono WAVs") {
    const auto files = dataFiles(".sfx");
    CHECK(files.size() == 9);
    std::size_t sounds = 0;
    std::map<std::string, int> formats;
    for (const auto& path : files) {
        CAPTURE(path.filename().string());
        const Bytes original = readFile(path);
        std::string error;
        const auto bank = readSfx(original, error);
        REQUIRE_MESSAGE(bank.has_value(), error);
        CHECK(writeSfx(*bank) == original);
        for (const auto& sound : bank->sounds) {
            ++sounds;
            CHECK(sound.voices == 1);
            const auto info = parseWav(sound.wav, error);
            REQUIRE_MESSAGE(info.has_value(), error);
            CHECK(info->formatTag == 1);
            CHECK(info->channels == 1);
            CHECK(info->dataOffset + info->dataSize <= sound.wav.size());
            ++formats[std::to_string(info->sampleRate) + "/" + std::to_string(info->bitsPerSample)];
        }
    }
    // migration.md A.4.
    CHECK(sounds == 175);
    CHECK(formats["22050/16"] == 142);
    CHECK(formats["22050/8"] == 18);
    CHECK(formats["44100/16"] == 12);
}

TEST_CASE("every .lvl level round-trips except the bytes after the asset names") {
    const auto files = dataFiles(".lvl");
    CHECK(files.size() == 12);
    for (const auto& path : files) {
        CAPTURE(path.filename().string());
        Bytes original = readFile(path);
        std::string error;
        const auto level = readLvl(original, error);
        REQUIRE_MESSAGE(level.has_value(), error);
        const auto written = writeLvl(*level, error);
        REQUIRE_MESSAGE(written.has_value(), error);
        // The editor left stack garbage after each name's NUL.
        for (std::size_t n = 0; n < 7; ++n) {
            bool afterNul = false;
            for (std::size_t i = n * 20; i < n * 20 + 20; ++i) {
                if (afterNul) {
                    original[i] = 0;
                }
                afterNul = afterNul || original[i] == 0;
            }
        }
        CHECK(*written == original);
        // Events are stored in descending activation order (A.6).
        for (std::size_t i = 1; i < level->events.size(); ++i) {
            CHECK(level->events[i - 1].activationX >= level->events[i].activationX);
        }
    }
}

TEST_CASE("uk.dat reads as UTF-8 strings and writes back unchanged") {
    const Bytes original = readFile(kData / "uk.dat");
    std::string error;
    const auto table = readDat(original, error);
    REQUIRE_MESSAGE(table.has_value(), error);
    // 136 strings in 2002, plus the 109 that game.cpp printed (Phase 2),
    // the 8 of the screens and the HUD (Phase 3), the 4 menu items of the
    // four-player mode and the CRISP filter of the Switch port.
    CHECK(table->strings.size() == 253 + 4 + 1);
    REQUIRE(table->find(854) != nullptr);
    CHECK(*table->find(854) == "PLAYER %d GOT A HIGH SCORE!");
    REQUIRE(table->find(1013) != nullptr);
    CHECK(*table->find(1013) == "J\xC3\xA9r\xC3\xB4me Karaban");
    REQUIRE(table->find(207) != nullptr);
    // 0x92 is U+2019 (right single quotation mark).
    CHECK(table->find(207)->find("Bears\xE2\x80\x99 ruler") != std::string::npos);
    REQUIRE(table->find(828) != nullptr);
    CHECK(*table->find(828) == "TIME");
    CHECK(table->find(517) == nullptr);  // D-63
    for (const auto& [id, s] : table->strings) {
        CHECK(isValidUtf8(s));
    }
    const auto written = writeDat(*table, error);
    REQUIRE(written.has_value());
    CHECK(*written == original);
}

TEST_CASE("bb.lst lists the 22 campaign entries") {
    std::string error;
    const auto campaign = readLst(readFile(kData / "bb.lst"), error);
    REQUIRE_MESSAGE(campaign.has_value(), error);
    REQUIRE(campaign->entries.size() == 22);
    CHECK(campaign->entries[0].kind == CampaignEntry::Kind::Briefing);
    CHECK(campaign->entries[0].path == "data/brief1.gfx");
    CHECK(campaign->entries[1].kind == CampaignEntry::Kind::Level);
    CHECK(campaign->entries[1].levelKind == 1);
    CHECK(campaign->entries[1].path == "data/snuf1.lvl");
    CHECK(campaign->entries[21].path == "data/finalboss.lvl");
    CHECK(campaign->entries[21].levelKind == 5);
    int levels = 0;
    for (const auto& e : campaign->entries) {
        levels += e.kind == CampaignEntry::Kind::Level ? 1 : 0;
    }
    CHECK(levels == 12);
}

TEST_CASE("every .mbk lists streamed tracks that exist") {
    const auto files = dataFiles(".mbk");
    CHECK(files.size() == 12);
    std::size_t tracks = 0;
    for (const auto& path : files) {
        CAPTURE(path.filename().string());
        std::string error;
        const auto list = readMbk(readFile(path), error);
        REQUIRE_MESSAGE(list.has_value(), error);
        for (const auto& track : list->tracks) {
            ++tracks;
            CHECK(track.type == 1);
            CHECK(track.path.rfind("data/", 0) == 0);  // '/' separators since D-10
            CHECK(fs::exists(kData / track.path.substr(5)));
        }
    }
    CHECK(tracks == 22);
}

TEST_CASE("the two cutscenes parse with known commands only") {
    const std::set<std::string> known = {
        "endscene", "loadgfx",   "loadmbk",   "playmusic",     "stopmusic",  "setpos",        "setpic",
        "setmove",  "settext",   "reset",     "resetall",      "setclip",    "noclip",        "setanim",
        "setxwarp", "setywarp",  "stopanim",  "renderscene",   "setalpha",   "setdeltaalpha", "setbackcolor",
        "setclipcolor", "setvolume", "setdeltavolume"};
    for (const char* name : {"intro.cin", "end.cin"}) {
        CAPTURE(name);
        std::string error;
        const auto commands = parseCutscene(text(readFile(kData / name)), error);
        REQUIRE_MESSAGE(commands.has_value(), error);
        CHECK(!commands->empty());
        for (const auto& c : *commands) {
            CAPTURE(c.line);
            CHECK(known.count(c.name) == 1);
        }
        CHECK(commands->back().name == "endscene");
    }
}

TEST_CASE("dialogue scripts list their scenes") {
    const auto files = dataFiles(".rpg");
    CHECK(files.size() == 7);
    for (const auto& path : files) {
        CAPTURE(path.filename().string());
        const std::string script = text(readFile(path));
        const auto scenes = dialogueScenes(script);
        CHECK(!scenes.empty());
        for (const int n : scenes) {
            const auto statements = dialogueScene(script, n);
            REQUIRE(statements.has_value());
            CHECK(!statements->empty());
        }
    }
}

TEST_CASE("bb.scr reads and writes back unchanged") {
    const Bytes original = readFile(kData / "bb.scr");
    std::string error;
    const auto table = readScr(original, error);
    REQUIRE_MESSAGE(table.has_value(), error);
    CHECK(std::string(table->entries[2].name.data()) == "LOADED STUDIO");
    CHECK(table->entries[2].score == 100000);
    CHECK(writeScr(*table) == original);
    Bytes corrupt = original;
    corrupt[5] ^= 1;
    CHECK_FALSE(readScr(corrupt, error).has_value());
}

TEST_CASE("truncated files are rejected, never read past the end") {
    for (const char* name : {"ammo1.lft", "bb.sfx", "finalboss.lvl", "scorei.lft"}) {
        CAPTURE(name);
        const Bytes original = readFile(kData / name);
        // Every prefix of the headers, then a spread of prefixes.
        const std::size_t step = std::max<std::size_t>(1, original.size() / 300);
        for (std::size_t size = 0; size < original.size(); size += size < 256 ? 1 : step) {
            const Bytes prefix(original.begin(), original.begin() + static_cast<std::ptrdiff_t>(size));
            std::string error;
            const std::string ext = fs::path(name).extension().string();
            if (ext == ".lft") {
                CHECK_FALSE(readLft(prefix, error).has_value());
            } else if (ext == ".sfx") {
                CHECK_FALSE(readSfx(prefix, error).has_value());
            } else {
                CHECK_FALSE(readLvl(prefix, error).has_value());
            }
        }
    }
    const Bytes bank = readFile(kData / "misc.gfx");
    for (std::size_t size = 0; size < bank.size(); size += size < 256 ? 1 : 4999) {
        std::string error;
        CHECK_FALSE(readGfx(Bytes(bank.begin(), bank.begin() + static_cast<std::ptrdiff_t>(size)), error).has_value());
    }
}

TEST_CASE("Windows-1252 and UTF-8 conversions are reversible") {
    std::string all;
    for (int b = 1; b < 256; ++b) {
        all.push_back(static_cast<char>(b));
    }
    const std::string utf8 = windows1252ToUtf8(all);
    CHECK(isValidUtf8(utf8));
    const auto back = utf8ToWindows1252(utf8);
    REQUIRE(back.has_value());
    CHECK(*back == all);
    CHECK(windows1252ToCodePoint(0x92) == 0x2019);
    CHECK(windows1252ToCodePoint(0xE9) == 0xE9);
    CHECK_FALSE(utf8ToWindows1252("\xE2\x82\xBD").has_value());  // U+20BD has no byte
    std::size_t pos = 0;
    CHECK(nextCodePoint("\xE9t\xC3\xA9", pos) == 0xE9);  // a stray Latin-1 byte
    CHECK(nextCodePoint("\xE9t\xC3\xA9", pos) == 't');
    CHECK(nextCodePoint("\xE9t\xC3\xA9", pos) == 0xE9);
    CHECK(pos == 4);
}
