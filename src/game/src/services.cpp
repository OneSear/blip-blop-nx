// Loading banks and fonts from the asset source (the loaders of
// picture_bank.cpp, fonte.cpp, sound_bank.cpp and music_bank.cpp).

#include "bb/game/services.hpp"

#include <vector>

#include "bb/core/assets/asset_source.hpp"
#include "bb/core/log.hpp"

namespace bb::game {

namespace {

constexpr int kTrackerModule = 0;  // MusicTrack::type of a tracker module

void logFailure(const char* what, const std::string& error) {
    core::Log::warning(std::string(what) + " - " + error);
}

}  // namespace

void setScreenWidth(Services& services, int width) {
    for (core::gfx::Pixmap* pixmap : {services.screen, services.backdrop}) {
        if (pixmap != nullptr && pixmap->width() != width) {
            *pixmap = core::gfx::Pixmap(width, core::gfx::kScreenHeight);
        }
    }
}

bool loadSprites(const core::assets::AssetSource& assets, const std::string& name,
                 core::gfx::SpriteBank& bank, bool colorKey) {
    std::string error;
    const auto data = assets.spriteBank(name, error);
    if (!data) {
        logFailure("PictureBank::loadGFX()", error);
        return false;
    }
    bank.assign(*data, colorKey);
    return true;
}

bool loadFont(const core::assets::AssetSource& assets, const std::string& name, core::gfx::BitmapFont& font) {
    if (font.loaded()) {
        core::Log::warning("Fonte::load -> font already loaded: " + name);
        return false;
    }
    std::string error;
    const auto data = assets.font(name, error);
    if (!data) {
        logFailure("Fonte::load", error);
        return false;
    }
    font.assign(*data);
    return true;
}

bool loadSounds(const core::assets::AssetSource& assets, const std::string& name, SoundBank& bank) {
    // With the sound off nothing is loaded (the options load the players'
    // sounds when it is switched on).
    if (!bank.output().soundOn) {
        return true;
    }
    if (bank.loaded()) {
        core::Log::warning("SoundBank::loadSFX() -> bank already loaded: " + name);
        return false;
    }
    std::string error;
    const auto data = assets.soundBank(name, error);
    if (!data) {
        logFailure("SoundBank::loadSFX()", error);
        return false;
    }
    bank.assign(*data);
    return true;
}

bool loadMusic(const core::assets::AssetSource& assets, const std::string& name, MusicBank& bank, bool loop) {
    if (!bank.output().musicOn) {
        return true;
    }
    std::string error;
    const auto list = assets.musicList(name, error);
    if (!list) {
        logFailure("MusicBank::load()", error);
        return false;
    }

    std::vector<std::vector<std::uint8_t>> files;
    for (const core::assets::MusicTrack& track : list->tracks) {
        if (track.type == kTrackerModule) {
            core::Log::warning("MusicBank::load() -> tracker module refused: " + track.path);
            bank.clear();
            return false;
        }
        auto bytes = assets.music(track.path, error);
        if (!bytes) {
            core::Log::warning("MusicBank::load() -> missing music " + track.path);
            bank.clear();
            return false;
        }
        files.push_back(std::move(*bytes));
    }

    // A file that cannot be decoded only stays silent (2014 port).
    bank.assign(std::move(files), loop);
    return true;
}

}  // namespace bb::game
