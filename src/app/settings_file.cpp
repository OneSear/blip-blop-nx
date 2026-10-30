// The settings file (config.cpp of the legacy port).

#include "settings_file.hpp"

#include <optional>
#include <vector>

#include "bb/core/log.hpp"
#include "bb/core/settings.hpp"
#include "bb/core/vfs.hpp"
#include "bb/platform/file_system.hpp"
#include "files.hpp"

namespace bb::app {

namespace {

namespace input = core::input;

// The alias of a player's action: from 0 for the first player, 10 for the
// second, and so on, in the order of input::kActions.
int aliasOf(int player, int action) {
    return game::AliasControls::firstOf(player) + action;
}

core::Settings current(const game::Options& options, const game::Input& in) {
    core::Settings settings;
    settings.language = options.language;
    settings.integerScaling = options.integerScaling;
    settings.filter = options.filter;
    settings.smoothMotion = options.smoothMotion;
    for (int p = 0; p < input::kPlayerCount; ++p) {
        for (int a = 0; a < input::kActionCount; ++a) {
            settings.bindings.set(p, input::kActions[static_cast<std::size_t>(a)],
                                  static_cast<input::Code>(in.alias(aliasOf(p, a))));
        }
    }
    return settings;
}

}  // namespace

void applySettings(const core::Settings& settings, game::Options& options, game::Input& in) {
    options.language = settings.language;
    options.integerScaling = settings.integerScaling;
    options.filter = settings.filter;
    options.smoothMotion = settings.smoothMotion;
    for (int p = 0; p < input::kPlayerCount; ++p) {
        for (int a = 0; a < input::kActionCount; ++a) {
            in.setAlias(aliasOf(p, a),
                        settings.bindings.get(p, input::kActions[static_cast<std::size_t>(a)]));
        }
    }
}

bool loadSettings(const std::string& settingsPath, const platform::FileSystem& fileSystem, const core::Vfs& dataFiles,
                  const std::string& legacyFile, game::Options& options, game::Input& in) {
    applySettings(core::Settings(), options, in);

    const auto bytes = settingsPath.empty() ? std::nullopt : fileSystem.readAll(settingsPath);
    if (bytes) {
        const std::string text(bytes->begin(), bytes->end());
        std::string error;
        std::vector<std::string> warnings;
        const auto settings = core::Settings::parse(text, error, warnings);
        for (const std::string& w : warnings) {
            core::Log::info(settingsPath + ": " + w);
        }
        if (settings) {
            core::Log::info("Using " + settingsPath + " as settings file.");
            applySettings(*settings, options, in);
        } else {
            core::Log::info(settingsPath + ": " + error + " - default settings used");
        }
        return true;
    }
    if (const auto legacy = dataFiles.readAll(legacyFile)) {
        std::vector<std::string> notes;
        const auto settings = core::Settings::importLegacyCfg(*legacy, notes);
        for (const std::string& n : notes) {
            core::Log::info(legacyFile + ": " + n);
        }
        if (settings) {
            core::Log::info("Imported " + legacyFile + " (2002 binary configuration).");
            applySettings(*settings, options, in);
        }
        return true;
    }
    core::Log::info("Cannot find a settings file. Will use default settings.");
    return false;
}

void saveSettings(const std::string& settingsPath, platform::FileSystem& fileSystem, const game::Options& options,
                  const game::Input& in) {
    if (settingsPath.empty()) {
        core::Log::info("No settings directory: settings not saved.");
        return;
    }
    const std::string text = current(options, in).serialize();
    if (fileSystem.writeAll(settingsPath, text.data(), text.size())) {
        core::Log::info("Saving " + settingsPath + " as settings file.");
    } else {
        core::Log::warning("Cannot save " + settingsPath);
    }
}

}  // namespace bb::app
