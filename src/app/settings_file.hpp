#pragma once

#include <string>

#include "bb/core/settings.hpp"
#include "bb/game/services.hpp"

namespace bb::core {
class Vfs;
}
namespace bb::platform {
class FileSystem;
}

namespace bb::app {

// The settings (display, language, the players' keys) as text in the
// user's preferences directory (bb::core::Settings); the 2002 binary bb.cfg
// is only imported, once, when there is no settings file yet (migration.md
// DEC-12).

// Sets the options and the players' keys (set_default_config() for the
// defaults).
void applySettings(const core::Settings& settings, game::Options& options, game::Input& input);
// The settings of settingsPath, else those imported from legacyFile (a data
// file), else the defaults (load_BB3_config()). False when neither file was
// there (the first run).
bool loadSettings(const std::string& settingsPath, const platform::FileSystem& fileSystem, const core::Vfs& dataFiles,
                  const std::string& legacyFile, game::Options& options, game::Input& input);
// Writes the settings to settingsPath (nothing without one).
void saveSettings(const std::string& settingsPath, platform::FileSystem& fileSystem, const game::Options& options,
                  const game::Input& input);

}  // namespace bb::app
