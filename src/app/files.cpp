// The data files and the asset source (legacy_io.cpp of Phases 1 and 2).

#include "files.hpp"

#include "bb/core/file_reader.hpp"
#include "bb/core/assets/asset_source.hpp"
#include "bb/core/assets/pack.hpp"
#include "bb/core/log.hpp"
#include "bb/core/vfs.hpp"

namespace bb::app {

namespace {

// The file every run reads first (App::init()).
constexpr const char* kProbeFile = "data/uk.dat";

}  // namespace

GameFiles::GameFiles(const core::FileReader& reader)
    : reader_(reader), vfs_(std::make_unique<core::Vfs>("data", reader)) {}

GameFiles::~GameFiles() {
    // The default asset source reads through the files: it goes first.
    overlaySource_.reset();
    assetSource_.reset();
}

void GameFiles::setDataRoot(const std::string& dataRoot) {
    // The default asset source reads through the files: drop it first.
    overlaySource_.reset();
    assetSource_.reset();
    vfs_ = std::make_unique<core::Vfs>(dataRoot, reader_);
    chosen_ = true;
}

const core::Vfs& GameFiles::dataFiles() const {
    return *vfs_;
}

std::string GameFiles::writablePath(const std::string& path) const {
    return vfs_->writablePath(path);
}

void GameFiles::locateGameData(const std::string& baseDirectory) {
    if (chosen_ || vfs_->resolve(kProbeFile)) {
        return;
    }
    if (baseDirectory.empty()) {
        core::Log::warning("No game data: no data directory here, and the executable's directory is unknown");
        return;
    }
    const std::string packPath = baseDirectory + "blipblop.bbpack";
    if (reader_.kind(packPath) == core::FileReader::Kind::File) {
        std::string error;
        std::shared_ptr<const core::assets::PackFile> pack = core::assets::PackFile::open(packPath, error, reader_);
        if (pack) {
            core::Log::info("Game data: " + packPath);
            setAssetSource(std::make_unique<core::assets::PackSource>(pack));
            return;
        }
        core::Log::warning(error);
    }
    const std::string dataRoot = baseDirectory + "data";
    if (core::Vfs(dataRoot, reader_).resolve(kProbeFile)) {
        core::Log::info("Game data: " + dataRoot);
        setDataRoot(dataRoot);
        return;
    }
    core::Log::warning("No game data: neither " + packPath + " nor " + dataRoot);
}

void GameFiles::setAssetSource(std::unique_ptr<core::assets::AssetSource> source) {
    overlaySource_.reset();
    assetSource_ = std::move(source);
    chosen_ = true;
}

void GameFiles::setOverlay(std::shared_ptr<const core::assets::PackFile> overlay) {
    overlaySource_.reset();
    overlay_ = std::move(overlay);
}

const core::assets::AssetSource& GameFiles::assets() {
    if (!assetSource_) {
        assetSource_ = std::make_unique<core::assets::LegacyDataSource>(*vfs_);
    }
    if (overlay_) {
        if (!overlaySource_) {
            overlaySource_ = std::make_unique<core::assets::OverlayPackSource>(overlay_, *assetSource_);
        }
        return *overlaySource_;
    }
    return *assetSource_;
}

}  // namespace bb::app
