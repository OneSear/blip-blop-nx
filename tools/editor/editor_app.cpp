#include "editor_app.hpp"

#include <SDL.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <set>
#include <sstream>

#include "bb/core/assets/pack.hpp"
#include "level_events.hpp"
#include "source_assets.hpp"

namespace bb::editor {

namespace fs = std::filesystem;
using core::assets::kEditorFill;
using game::EntityCategory;
using tools::EventEntity;
using tools::EventSlot;

namespace {

constexpr float kLevelHeight = 480;
constexpr int kEventTypes = 19;

// The 11 flags of the game (bb::game::Flag).
constexpr const char* kFlagNames[] = {"0 user",       "1 user",      "2 user",  "3 user",
                                      "4 bubbles",    "5 (unused)",  "6 timer", "7 generators off",
                                      "8 kills",      "9 enemies",   "10 generators"};
constexpr int kFlagCount = 11;

// Platform layers as the 2002 editor coloured them; 6 and 7 are slippery.
constexpr ImU32 kLayerColours[LevelData::kPlatformLayers] = {
    IM_COL32(255, 60, 60, 255),  IM_COL32(255, 160, 40, 255), IM_COL32(255, 240, 60, 255),
    IM_COL32(80, 230, 80, 255),  IM_COL32(60, 220, 230, 255), IM_COL32(80, 120, 255, 255),
    IM_COL32(240, 80, 240, 255), IM_COL32(160, 90, 255, 255)};

ImU32 lineColour(std::int32_t type) {
    switch (static_cast<LevelEventType>(type)) {
        case LevelEventType::ScrollLock:
            return IM_COL32(255, 60, 255, 255);
        case LevelEventType::HoldFire:
            return IM_COL32(255, 230, 60, 255);
        case LevelEventType::ForceScroll:
            return IM_COL32(60, 230, 255, 255);
        case LevelEventType::Dialogue:
            return IM_COL32(90, 140, 255, 255);
        case LevelEventType::SetFlag:
            return IM_COL32(255, 150, 60, 255);
        case LevelEventType::BannerText:
            return IM_COL32(255, 255, 255, 255);
        case LevelEventType::Music:
        case LevelEventType::Sound:
            return IM_COL32(120, 255, 160, 255);
        default:
            return IM_COL32(190, 190, 190, 255);
    }
}

ImTextureID textureId(SDL_Texture* texture) {
    return static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(texture));  // NOLINT: the SDL backend's id
}

bool contains(const std::vector<std::string>& list, const std::string& item) {
    return std::find(list.begin(), list.end(), item) != list.end();
}

// Names of the files of a directory with a given suffix, the suffix
// replaced ("snuf.playlist.json" -> "snuf.mbk").
std::vector<std::string> namesIn(const fs::path& dir, const std::string& suffix, const std::string& replacement,
                                 bool directories = false) {
    std::vector<std::string> names;
    std::error_code ec;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        const std::string name = it->path().filename().u8string();
        if (directories) {
            if (it->is_directory()) {
                names.push_back(name + replacement);
            }
        } else if (name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
            names.push_back(name.substr(0, name.size() - suffix.size()) + replacement);
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::string quote(const std::string& s) {
    return "\"" + s + "\"";
}

// A short summary of an event without a point, for its marker.
std::string lineLabel(const LevelEventRecord& e) {
    char text[96];
    auto flag = [](std::int32_t f) { return f == kEditorFill ? std::string("-") : std::to_string(f); };
    switch (static_cast<LevelEventType>(e.type)) {
        case LevelEventType::ScrollLock: {
            static const char* const kConditions[] = {"no enemies", "no generators", "flag ==", "flag >="};
            const char* c = e.y >= 0 && e.y < 4 ? kConditions[e.y] : "?";
            if (e.y >= 2) {
                std::snprintf(text, sizeof(text), "lock: flag %s %s %s", flag(e.capacity).c_str(), e.y == 2 ? "==" : ">=",
                              flag(e.value).c_str());
            } else {
                std::snprintf(text, sizeof(text), "lock: %s", c);
            }
            break;
        }
        case LevelEventType::HoldFire:
            std::snprintf(text, sizeof(text), "hold fire: flag %s = %s", flag(e.capacity).c_str(), flag(e.value).c_str());
            break;
        case LevelEventType::ForceScroll:
            std::snprintf(text, sizeof(text), "scroll speed %d", e.y);
            break;
        case LevelEventType::Dialogue:
            std::snprintf(text, sizeof(text), "dialogue %d%s", e.id, e.y != 0 ? " (if flag)" : "");
            break;
        case LevelEventType::SetFlag:
            std::snprintf(text, sizeof(text), "flag %s := %s", flag(e.capacity).c_str(), flag(e.value).c_str());
            break;
        case LevelEventType::BannerText:
            std::snprintf(text, sizeof(text), "text %d", e.bank);
            break;
        case LevelEventType::Sound:
            std::snprintf(text, sizeof(text), "sound %d", e.id);
            break;
        case LevelEventType::Weather: {
            static const char* const kKinds[] = {"rain", "snow", "distortion"};
            std::snprintf(text, sizeof(text), "weather %s %d", e.id >= 0 && e.id < 3 ? kKinds[e.id] : "?", e.y);
            break;
        }
        case LevelEventType::Music:
            std::snprintf(text, sizeof(text), "music %s %d", e.y != 0 ? "play" : "stop", e.id);
            break;
        case LevelEventType::PickupGenerator:
            std::snprintf(text, sizeof(text), "pickup generator %d", e.period);
            break;
        default:
            std::snprintf(text, sizeof(text), "%s (ignored by the game)", eventTypeName(e.type));
            break;
    }
    return text;
}

// The entity a field of this event type names.
EventEntity entityField(std::int32_t type) {
    const auto& schema = tools::eventSchemas().at(type);
    for (const auto& field : schema.fields) {
        if (field.entity != EventEntity::None) {
            return field.entity;
        }
    }
    return EventEntity::None;
}

bool isDecal(std::int32_t type) {
    return type == static_cast<std::int32_t>(LevelEventType::MidgroundDecal) ||
           type == static_cast<std::int32_t>(LevelEventType::Foreground);
}

}  // namespace

// --- Setup -----------------------------------------------------------------

EditorApp::EditorApp(SDL_Renderer* renderer, std::string sourceDir, std::string toolDir)
    : sourceDir_(std::move(sourceDir)),
      toolDir_(std::move(toolDir)),
      textures_(renderer, sourceDir_) {
    scanSourceTree();
    loadPreviews();
    status("Source assets: " + sourceDir_);
}

EditorApp::~EditorApp() {
    if (player_.joinable()) {
        player_.join();
    }
}

void EditorApp::scanSourceTree() {
    const fs::path root = fs::u8path(sourceDir_);
    levels_ = namesIn(root / "levels", ".json", "");
    spriteBanks_ = namesIn(root / "sprites", "", ".gfx", true);
    soundBanks_ = namesIn(root / "sounds", "", ".sfx", true);
    playlists_ = namesIn(root / "music", ".playlist.json", ".mbk");
    dialogues_ = namesIn(root / "scripts", ".rpg", ".rpg");
}

void EditorApp::loadPreviews() {
    previews_.clear();
    const fs::path file = fs::u8path(sourceDir_) / "editor" / "entities.json";
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return;  // every entity is then drawn as a box
    }
    try {
        const tools::Json j = tools::Json::parse(in);
        for (const auto& p : j.at("previews")) {
            previews_[p.at("entity").get<std::string>()] = {p.at("sprites").get<std::string>(), p.at("image").get<int>()};
        }
    } catch (const tools::Json::exception& ex) {
        status(file.u8string() + ": " + ex.what(), true);
    }
}

void EditorApp::status(std::string text, bool error) {
    status_ = std::move(text);
    statusError_ = error;
}

bool EditorApp::open(const std::string& stem) {
    std::string error;
    auto doc = LevelDocument::load((fs::u8path(sourceDir_) / "levels" / (stem + ".json")).u8string(), error);
    if (!doc) {
        status(error, true);
        return false;
    }
    doc_ = std::make_unique<LevelDocument>(std::move(*doc));
    stem_ = stem;
    selection_ = {};
    fitPending_ = true;
    screensTextFresh_ = false;
    status("Opened " + stem + ".json: " + std::to_string(doc_->data().events.size()) + " events, " +
           std::to_string(doc_->data().screens.size()) + " screens");
    return true;
}

bool EditorApp::save() {
    if (!doc_) {
        return false;
    }
    std::string error;
    if (!doc_->save(error)) {
        status(error, true);
        return false;
    }
    if (!contains(levels_, stem_)) {
        scanSourceTree();
    }
    status("Saved " + doc_->path());
    return true;
}

void EditorApp::whenSaved(std::function<void()> action) {
    if (doc_ && doc_->dirty()) {
        pendingAction_ = std::move(action);
        unsavedRequested_ = true;
    } else {
        action();
    }
}

void EditorApp::requestClose() {
    whenSaved([this] { done_ = true; });
}

std::string EditorApp::entityName(EntityCategory category, std::int32_t id) const {
    const game::EntityType* type = tools::entityRegistry().find(category, id);
    return type != nullptr ? type->name : "#" + std::to_string(id);
}

const Picture* EditorApp::entityPicture(EntityCategory category, std::int32_t id) {
    const game::EntityType* type = tools::entityRegistry().find(category, id);
    if (type == nullptr) {
        return nullptr;
    }
    const auto it = previews_.find(type->name);
    return it == previews_.end() ? nullptr : textures_.picture(it->second.first, it->second.second);
}

// The world prefix of the entities a level places ("smurf").
std::string EditorApp::worldOfLevel() const {
    if (!doc_) {
        return {};
    }
    std::map<std::string, int> counts;
    for (const auto& e : doc_->data().events) {
        if (e.type == static_cast<std::int32_t>(LevelEventType::Enemy)) {
            const std::string name = entityName(EntityCategory::Enemy, e.id);
            counts[name.substr(0, name.find('.'))] += 1;
        }
    }
    std::string best;
    int most = 0;
    for (const auto& [world, n] : counts) {
        if (n > most) {
            best = world;
            most = n;
        }
    }
    return best;
}

// --- Frame -----------------------------------------------------------------

void EditorApp::frame() {
    shortcuts();
    menuBar();

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float x = vp->WorkPos.x;
    const float y = vp->WorkPos.y;
    const float w = vp->WorkSize.x;
    const float h = vp->WorkSize.y;
    const float statusHeight = ImGui::GetFrameHeight() + 6;
    const float left = std::min(300.0f, w * 0.25f);
    const float right = std::min(340.0f, w * 0.28f);
    toolPanel(x, y, left, h - statusHeight);
    canvas(x + left, y, w - left - right, h - statusHeight);
    inspector(x + w - right, y, right, h - statusHeight);
    statusBar(x, y + h - statusHeight, w, statusHeight);

    if (levelWindowOpen_) {
        levelWindow();
    }
    if (spriteWindowOpen_) {
        spriteWindow();
    }
    newLevelPopup();
    unsavedPopup();

    if (playState_ == 2 || playState_ == 3) {
        if (player_.joinable()) {
            player_.join();
        }
        status(playState_ == 2 ? "Test play finished" : "Test play failed: see the terminal", playState_ == 3);
        playState_ = 0;
    }
}

void EditorApp::shortcuts() {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) {
        return;
    }
    const bool ctrl = io.KeyCtrl || io.KeySuper;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
        save();
    }
    if (doc_ && ctrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        if (io.KeyShift ? doc_->redo() : doc_->undo()) {
            selection_ = {};
        }
    }
    if (doc_ && ctrl && ImGui::IsKeyPressed(ImGuiKey_Y) && doc_->redo()) {
        selection_ = {};
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
        duplicateSelection();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
        deleteSelection();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        selection_ = {};
        tool_ = Tool::Select;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F5)) {
        playLevel(io.KeyShift);
    }
    if (!ctrl) {
        const ImGuiKey keys[] = {ImGuiKey_1, ImGuiKey_2, ImGuiKey_3, ImGuiKey_4};
        const Tool tools[] = {Tool::Select, Tool::Place, Tool::Platform, Tool::Wall};
        for (int i = 0; i < 4; ++i) {
            if (ImGui::IsKeyPressed(keys[i])) {
                tool_ = tools[i];
            }
        }
    }
}

void EditorApp::menuBar() {
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }
    if (ImGui::BeginMenu("File")) {
        if (ImGui::BeginMenu("Open level")) {
            for (const std::string& stem : levels_) {
                if (ImGui::MenuItem(stem.c_str(), nullptr, stem == stem_)) {
                    levelToOpen_ = stem;
                    whenSaved([this] { open(levelToOpen_); });
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("New level...", nullptr, false, doc_ != nullptr)) {
            whenSaved([this] {
                newLevelName_.clear();
                newLevelRequested_ = true;
            });
        }
        if (ImGui::MenuItem("Save", "Ctrl+S", false, doc_ != nullptr)) {
            save();
        }
        if (ImGui::MenuItem("Revert", nullptr, false, doc_ && doc_->dirty())) {
            const std::string stem = stem_;
            open(stem);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit")) {
            requestClose();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, doc_ && doc_->canUndo())) {
            doc_->undo();
            selection_ = {};
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Shift+Z", false, doc_ && doc_->canRedo())) {
            doc_->redo();
            selection_ = {};
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, selection_.kind == Selected::Event)) {
            duplicateSelection();
        }
        if (ImGui::MenuItem("Delete", "Del", false, selection_.kind == Selected::Event)) {
            deleteSelection();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Whole level")) {
            showWhole();
        }
        if (ImGui::MenuItem("Actual size")) {
            zoomAt(1 / zoom_, originX_ + viewWidth_ / 2, originY_ + viewHeight_ / 2);
        }
        ImGui::Separator();
        ImGui::MenuItem("Decor", nullptr, &showDecor_);
        ImGui::MenuItem("Midground and foreground", nullptr, &showDecals_);
        ImGui::MenuItem("Entities", nullptr, &showEntities_);
        ImGui::MenuItem("Events without a point", nullptr, &showLineEvents_);
        ImGui::MenuItem("Solid walls", nullptr, &showSolidWalls_);
        ImGui::MenuItem("Blood walls", nullptr, &showBloodWalls_);
        for (int layer = 0; layer < LevelData::kPlatformLayers; ++layer) {
            const std::string label =
                "Platforms, layer " + std::to_string(layer) + (layer >= 6 ? " (slippery)" : "");
            ImGui::MenuItem(label.c_str(), nullptr, &showPlatforms_[layer]);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Level")) {
        ImGui::MenuItem("Properties...", nullptr, &levelWindowOpen_, doc_ != nullptr);
        if (ImGui::MenuItem("Play", "F5", false, doc_ != nullptr && playState_ == 0)) {
            playLevel(false);
        }
        if (ImGui::MenuItem("Play with rebuilt assets", "Shift+F5", false, doc_ != nullptr && playState_ == 0)) {
            playLevel(true);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Sprites")) {
        ImGui::MenuItem("Hotspots and names...", nullptr, &spriteWindowOpen_);
        ImGui::EndMenu();
    }
    if (doc_) {
        const std::string title = stem_ + (doc_->dirty() ? " *" : "");
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(title.c_str()).x - 16);
        ImGui::TextUnformatted(title.c_str());
    }
    ImGui::EndMainMenuBar();
}

void EditorApp::statusBar(float x, float y, float width, float height) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::Begin("##status", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    if (statusError_) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", status_.c_str());
    } else {
        ImGui::TextUnformatted(status_.c_str());
    }
    if (doc_ && ImGui::IsMousePosValid()) {
        char where[64];
        std::snprintf(where, sizeof(where), "x %d  y %d   zoom %.0f%%",
                      static_cast<int>(std::floor(toWorldX(ImGui::GetMousePos().x))),
                      static_cast<int>(std::floor(toWorldY(ImGui::GetMousePos().y))), zoom_ * 100);
        ImGui::SameLine(width - ImGui::CalcTextSize(where).x - 16);
        ImGui::TextUnformatted(where);
    }
    ImGui::End();
}

// --- Tools and palette -----------------------------------------------------

void EditorApp::toolPanel(float x, float y, float width, float height) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::Begin("Tools", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    if (!doc_) {
        ImGui::TextWrapped("Open a level (File > Open level).");
        ImGui::End();
        return;
    }
    const char* names[] = {"1 Select and move", "2 Place", "3 Platforms", "4 Walls",
                           "Player 1 start",    "Player 2 start", "Victory"};
    for (int i = 0; i < 7; ++i) {
        if (ImGui::RadioButton(names[i], tool_ == static_cast<Tool>(i))) {
            tool_ = static_cast<Tool>(i);
        }
    }
    ImGui::Separator();
    switch (tool_) {
        case Tool::Select:
            ImGui::TextWrapped(
                "Click an object to select it, drag it to move it. Del deletes, Ctrl+D duplicates. "
                "Wheel: scroll; Ctrl+wheel: zoom; middle button: pan.");
            break;
        case Tool::Place:
            palette();
            break;
        case Tool::Platform:
            ImGui::TextWrapped("Left button paints the platform height under the cursor, right button erases.");
            for (int layer = 0; layer < LevelData::kPlatformLayers; ++layer) {
                ImGui::PushStyleColor(ImGuiCol_Text, kLayerColours[layer]);
                const std::string label = "Layer " + std::to_string(layer) + (layer >= 6 ? " (slippery)" : "");
                if (ImGui::RadioButton(label.c_str(), platformLayer_ == layer)) {
                    platformLayer_ = layer;
                }
                ImGui::PopStyleColor();
            }
            ImGui::SliderInt("Brush width", &brushWidth_, 1, 100);
            break;
        case Tool::Wall:
            ImGui::TextWrapped("8x8 cells: left button sets, right button clears.");
            if (ImGui::RadioButton("Solid walls", !wallBlood_)) {
                wallBlood_ = false;
            }
            if (ImGui::RadioButton("Blood walls", wallBlood_)) {
                wallBlood_ = true;
            }
            ImGui::SliderInt("Brush (cells)", &wallBrush_, 1, 8);
            break;
        default:
            ImGui::TextWrapped("Click in the level to set it.");
            break;
    }
    ImGui::End();
}

void EditorApp::palette() {
    std::string current = eventTypeName(placeType_);
    if (ImGui::BeginCombo("Event", current.c_str())) {
        for (std::int32_t type = 0; type < kEventTypes; ++type) {
            if (ImGui::Selectable(eventTypeName(type), type == placeType_)) {
                placeType_ = type;
                placeEntity_ = 0;
            }
        }
        ImGui::EndCombo();
    }
    const EventEntity entity = entityField(placeType_);
    if (entity != EventEntity::None) {
        if (paletteFilter_.empty()) {
            const std::string world = worldOfLevel();
            paletteFilter_ = world.empty() ? world : world + ".";
        }
        char filter[64];
        std::snprintf(filter, sizeof(filter), "%s", paletteFilter_.c_str());
        if (ImGui::InputText("Filter", filter, sizeof(filter))) {
            paletteFilter_ = filter;
        }
        ImGui::BeginChild("entities", ImVec2(0, 0), ImGuiChildFlags_Borders);
        const EntityCategory category = tools::categoryOf(entity);
        for (const game::EntityType& type : tools::entityRegistry().types()) {
            if (type.category != category || type.name.find(paletteFilter_) == std::string::npos) {
                continue;
            }
            ImGui::PushID(type.name.c_str());
            const Picture* p = entityPicture(category, type.legacyId);
            const float thumb = 40;
            if (p != nullptr) {
                const float s = thumb / static_cast<float>(std::max(p->width, p->height));
                ImGui::Image(textureId(p->texture), ImVec2(static_cast<float>(p->width) * s, static_cast<float>(p->height) * s));
            } else {
                ImGui::Dummy(ImVec2(thumb, thumb));
            }
            ImGui::SameLine(thumb + 16);
            if (ImGui::Selectable(type.name.c_str(), placeEntity_ == type.legacyId, 0, ImVec2(0, thumb))) {
                placeEntity_ = type.legacyId;
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
    } else if (isDecal(placeType_)) {
        const std::string& bank = doc_->data().assets[1];
        ImGui::Text("Images of %s", bank.empty() ? "(no level bank)" : bank.c_str());
        ImGui::BeginChild("images", ImVec2(0, 0), ImGuiChildFlags_Borders);
        const auto& pictures = textures_.bank(bank);
        const float thumb = 64;
        const float avail = ImGui::GetContentRegionAvail().x;
        const int perRow = std::max(1, static_cast<int>(avail / (thumb + 8)));
        for (int i = 0; i < static_cast<int>(pictures.size()); ++i) {
            const Picture& p = pictures[static_cast<std::size_t>(i)];
            ImGui::PushID(i);
            const float s = thumb / static_cast<float>(std::max({p.width, p.height, 1}));
            const ImVec4 bg = i == placeImage_ ? ImVec4(0.3f, 0.5f, 1, 1) : ImVec4(0, 0, 0, 0);
            if (p.texture != nullptr &&
                ImGui::ImageButton("img", textureId(p.texture),
                                   ImVec2(std::max(4.0f, static_cast<float>(p.width) * s), std::max(4.0f, static_cast<float>(p.height) * s)), ImVec2(0, 0),
                                   ImVec2(1, 1), bg)) {
                placeImage_ = i;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("image %d (%dx%d)", i, p.width, p.height);
            }
            if ((i + 1) % perRow != 0) {
                ImGui::SameLine();
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
    } else {
        ImGui::TextWrapped("Click in the level: the event acts when the screen's left edge reaches that x.");
    }
}

// --- Inspector -------------------------------------------------------------

void EditorApp::inspector(float x, float y, float width, float height) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    if (!doc_) {
        ImGui::End();
        return;
    }
    const LevelData& level = doc_->data();
    switch (selection_.kind) {
        case Selected::Event: {
            if (selection_.index < 0 || selection_.index >= static_cast<int>(level.events.size())) {
                selection_ = {};
                break;
            }
            LevelEventRecord e = level.events[static_cast<std::size_t>(selection_.index)];
            ImGui::Text("Event %d: %s", selection_.index, eventTypeName(e.type));
            ImGui::Text("Activation x: %d", e.activationX);
            ImGui::Separator();
            bool changed = false;
            eventFields(e, changed);
            if (changed) {
                selection_.index = doc_->replaceEvent(selection_.index, e);
            }
            ImGui::Separator();
            if (ImGui::Button("Duplicate")) {
                duplicateSelection();
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete")) {
                deleteSelection();
            }
            break;
        }
        case Selected::Start1:
        case Selected::Start2: {
            const int player = selection_.kind == Selected::Start1 ? 0 : 1;
            int xy[2] = {player == 0 ? level.start1X : level.start2X, player == 0 ? level.start1Y : level.start2Y};
            ImGui::Text("Player %d start", player + 1);
            if (ImGui::InputInt2("x, y", xy)) {
                doc_->setStart(player, xy[0], xy[1]);
            }
            break;
        }
        case Selected::Victory:
            ImGui::TextWrapped("Victory: see Level > Properties.");
            levelWindowOpen_ = true;
            break;
        case Selected::None: {
            ImGui::Text("%s", stem_.c_str());
            ImGui::Text("%d screens, %d pixels", static_cast<int>(level.screens.size()), level.width());
            ImGui::Separator();
            std::map<std::string, int> counts;
            for (const auto& e : level.events) {
                counts[eventTypeName(e.type)] += 1;
            }
            for (const auto& [name, n] : counts) {
                ImGui::Text("%4d  %s", n, name.c_str());
            }
            break;
        }
    }
    ImGui::End();
}

// Widgets for the fields of an event; `changed` when one was edited.
void EditorApp::eventFields(LevelEventRecord& e, bool& changed) {
    const tools::EventSchema& schema = tools::eventSchemas().at(e.type);
    const auto type = static_cast<LevelEventType>(e.type);
    if (type == LevelEventType::Overkill || type == LevelEventType::DynamicLoad) {
        ImGui::TextWrapped("The game ignores this event (it does nothing in 2002 either).");
    }
    if (type == LevelEventType::PickupGenerator) {
        ImGui::TextWrapped("The game uses the period only (as in 2002).");
    }
    if (isDecal(e.type)) {
        const Picture* p = textures_.picture(doc_->data().assets[1], e.id);
        if (p != nullptr) {
            const float s = std::min(1.0f, 200.0f / static_cast<float>(std::max(p->width, p->height)));
            ImGui::Image(textureId(p->texture), ImVec2(static_cast<float>(p->width) * s, static_cast<float>(p->height) * s));
        }
    }
    for (const tools::EventField& field : schema.fields) {
        std::int32_t& v = tools::slotRef(e, field.slot);
        const std::string name = field.name;
        ImGui::PushID(field.name);
        if (field.entity != EventEntity::None) {
            const EntityCategory category = tools::categoryOf(field.entity);
            const std::string current = entityName(category, v);
            if (ImGui::BeginCombo(field.name, current.c_str())) {
                for (const game::EntityType& t : tools::entityRegistry().types()) {
                    if (t.category == category && ImGui::Selectable(t.name.c_str(), t.legacyId == v)) {
                        v = t.legacyId;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
            const Picture* p = entityPicture(category, v);
            if (p != nullptr) {
                const float s = std::min(1.0f, 160.0f / static_cast<float>(std::max(p->width, p->height)));
                ImGui::Image(textureId(p->texture), ImVec2(static_cast<float>(p->width) * s, static_cast<float>(p->height) * s));
            }
        } else if (name == "facing") {
            int facing = v == 0 ? 0 : 1;
            if (ImGui::Combo("facing", &facing, "left\0right\0")) {
                v = facing;
                changed = true;
            }
        } else if (name == "flag") {
            const bool unset = v == kEditorFill;
            const char* label = unset ? "(not set)" : (v >= 0 && v < kFlagCount ? kFlagNames[v] : "?");
            if (ImGui::BeginCombo("flag", label)) {
                for (int f = 0; f < kFlagCount; ++f) {
                    if (ImGui::Selectable(kFlagNames[f], f == v)) {
                        v = f;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
        } else if (name == "condition" && type == LevelEventType::ScrollLock) {
            int c = std::clamp(v, 0, 3);
            if (ImGui::Combo("until", &c, "no enemies left\0no generators left\0flag == value\0flag >= value\0")) {
                v = c;
                changed = true;
            }
        } else if (name == "condition" && type == LevelEventType::Dialogue) {
            int c = v != 0 ? 1 : 0;
            if (ImGui::Combo("when", &c, "always\0flag == value\0")) {
                v = c;
                changed = true;
            }
        } else if (name == "condition" && type == LevelEventType::BannerText) {
            int c = std::clamp(v, 0, 2);
            if (ImGui::Combo("when", &c, "always\0flag == value\0flag >= value\0")) {
                v = c;
                changed = true;
            }
        } else if (name == "kind" && type == LevelEventType::Weather) {
            int k = std::clamp(v, 0, 2);
            if (ImGui::Combo("kind", &k, "rain\0snow\0water distortion\0")) {
                v = k;
                changed = true;
            }
        } else if (name == "play") {
            int p = v != 0 ? 1 : 0;
            if (ImGui::Combo("action", &p, "stop\0play\0")) {
                v = p;
                changed = true;
            }
        } else {
            const bool unset = v == kEditorFill;
            int value = unset ? 0 : v;
            if (unset) {
                ImGui::TextDisabled("%s: not set (2002 fill)", field.name);
                ImGui::SameLine();
                if (ImGui::SmallButton("set")) {
                    v = 0;
                    changed = true;
                }
            } else if (ImGui::InputInt(field.name, &value)) {
                v = value;
                changed = true;
            }
        }
        ImGui::PopID();
    }
    if (schema.temporary) {
        bool temporary = e.temporary == 1;
        if (ImGui::Checkbox("temporary", &temporary)) {
            e.temporary = temporary ? 1 : 0;
            changed = true;
        }
    }
    if (!hasPosition(e.type)) {
        int x = e.activationX;
        if (ImGui::InputInt("level x", &x)) {
            e.activationX = x;
            changed = true;
        }
    }
}

// --- Level properties ------------------------------------------------------

void EditorApp::levelWindow() {
    ImGui::SetNextWindowSize(ImVec2(520, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Level properties", &levelWindowOpen_) || !doc_) {
        ImGui::End();
        return;
    }
    const LevelData& level = doc_->data();

    if (ImGui::CollapsingHeader("Assets", ImGuiTreeNodeFlags_DefaultOpen)) {
        static const char* const kLabels[LevelData::kAssetCount] = {"decor",  "level (props)", "enemies", "sounds",
                                                                    "music",  "dialogue",      "portraits"};
        const std::vector<std::string>* lists[LevelData::kAssetCount] = {
            &spriteBanks_, &spriteBanks_, &spriteBanks_, &soundBanks_, &playlists_, &dialogues_, &spriteBanks_};
        for (int i = 0; i < LevelData::kAssetCount; ++i) {
            const std::string& current = level.assets[static_cast<std::size_t>(i)];
            if (ImGui::BeginCombo(kLabels[i], current.empty() ? "(none)" : current.c_str())) {
                if (ImGui::Selectable("(none)", current.empty())) {
                    doc_->setAsset(i, "");
                }
                for (const std::string& name : *lists[i]) {
                    if (ImGui::Selectable(name.c_str(), name == current)) {
                        doc_->setAsset(i, name);
                    }
                }
                ImGui::EndCombo();
            }
        }
    }

    if (ImGui::CollapsingHeader("Screens", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!screensTextFresh_) {
            std::ostringstream text;
            for (std::size_t i = 0; i < level.screens.size(); ++i) {
                text << (i ? " " : "") << level.screens[i];
            }
            screensText_ = text.str();
            screensTextFresh_ = true;
        }
        ImGui::TextWrapped("The decor image of each 640-pixel screen, left to right (%d screens, %d pixels).",
                           static_cast<int>(level.screens.size()), level.width());
        char text[1024];
        std::snprintf(text, sizeof(text), "%s", screensText_.c_str());
        if (ImGui::InputText("##screens", text, sizeof(text))) {
            screensText_ = text;
        }
        ImGui::SameLine();
        if (ImGui::Button("Apply")) {
            std::istringstream in(screensText_);
            std::vector<std::int32_t> screens;
            std::int32_t n = 0;
            while (in >> n) {
                screens.push_back(n);
            }
            if (screens.empty() || !in.eof()) {
                status("Screens: numbers separated by spaces", true);
            } else {
                doc_->setScreens(screens);
                screensTextFresh_ = false;
            }
        }
        const int decorImages = static_cast<int>(textures_.bank(level.assets[0]).size());
        ImGui::TextDisabled("The decor bank has %d images (0 to %d).", decorImages, decorImages - 1);
    }

    if (ImGui::CollapsingHeader("Start and victory", ImGuiTreeNodeFlags_DefaultOpen)) {
        int s1[2] = {level.start1X, level.start1Y};
        int s2[2] = {level.start2X, level.start2Y};
        if (ImGui::InputInt2("player 1 start", s1)) {
            doc_->setStart(0, s1[0], s1[1]);
        }
        if (ImGui::InputInt2("player 2 start", s2)) {
            doc_->setStart(1, s2[0], s2[1]);
        }
        int v[5] = {level.victoryX, level.victoryFlag1, level.victoryValue1, level.victoryFlag2, level.victoryValue2};
        bool changed = ImGui::InputInt("victory x", &v[0]);
        ImGui::TextWrapped(
            "The level is won once the screen's left edge is at victory x or further and flag 1 == value 1 and "
            "flag 2 == value 2 (flags 0 to 10; level.cpp).");
        changed |= ImGui::InputInt2("flag 1, value 1", &v[1]);
        changed |= ImGui::InputInt2("flag 2, value 2", &v[3]);
        if (changed) {
            doc_->setVictory(v[0], v[1], v[2], v[3], v[4]);
        }
    }

    if (ImGui::CollapsingHeader("Markers", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped(
            "Positions and settings the level's entities read by name (docs/formats/source-assets.md). The "
            "content test checks that a level has the markers of the entities it places.");
        core::assets::LevelMarkers markers = level.markers;
        std::string remove;
        for (auto& [name, values] : markers) {
            std::ostringstream text;
            for (std::size_t i = 0; i < values.size(); ++i) {
                text << (i ? " " : "") << values[i];
            }
            ImGui::PushID(name.c_str());
            char buffer[512];
            std::snprintf(buffer, sizeof(buffer), "%s", text.str().c_str());
            if (ImGui::InputText(name.c_str(), buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
                std::istringstream in(buffer);
                std::vector<std::int32_t> parsed;
                std::int32_t n = 0;
                while (in >> n) {
                    parsed.push_back(n);
                }
                if (in.eof()) {
                    values = parsed;
                    doc_->setMarkers(markers);
                } else {
                    status("Marker values: numbers separated by spaces (Enter applies)", true);
                }
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("x")) {
                remove = name;
            }
            ImGui::PopID();
        }
        if (!remove.empty()) {
            markers.erase(remove);
            doc_->setMarkers(markers);
        }
        char name[64];
        char values[256];
        std::snprintf(name, sizeof(name), "%s", markerName_.c_str());
        std::snprintf(values, sizeof(values), "%s", markerValues_.c_str());
        if (ImGui::InputText("new marker", name, sizeof(name))) {
            markerName_ = name;
        }
        if (ImGui::InputText("values", values, sizeof(values))) {
            markerValues_ = values;
        }
        if (ImGui::Button("Add marker") && !markerName_.empty()) {
            std::istringstream in(markerValues_);
            std::vector<std::int32_t> parsed;
            std::int32_t n = 0;
            while (in >> n) {
                parsed.push_back(n);
            }
            markers = level.markers;
            markers[markerName_] = parsed;
            doc_->setMarkers(markers);
            markerName_.clear();
            markerValues_.clear();
        }
    }
    ImGui::End();
}

// --- Sprite banks ----------------------------------------------------------

void EditorApp::spriteWindow() {
    ImGui::SetNextWindowSize(ImVec2(760, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Sprite banks", &spriteWindowOpen_)) {
        ImGui::End();
        return;
    }
    const fs::path manifestPath = fs::u8path(sourceDir_) / "sprites" / tools::stemOf(spriteBank_) / "sprites.json";
    auto loadManifest = [&](const std::string& bank) {
        spriteBank_ = bank;
        spriteImage_ = 0;
        spriteDirty_ = false;
        spriteManifest_ = tools::Json();
        std::ifstream in(fs::u8path(sourceDir_) / "sprites" / tools::stemOf(bank) / "sprites.json", std::ios::binary);
        try {
            spriteManifest_ = tools::Json::parse(in);
        } catch (const tools::Json::exception& ex) {
            status(bank + ": " + ex.what(), true);
        }
    };
    if (ImGui::BeginCombo("bank", spriteBank_.empty() ? "(choose)" : spriteBank_.c_str())) {
        for (const std::string& bank : spriteBanks_) {
            if (ImGui::Selectable(bank.c_str(), bank == spriteBank_)) {
                if (!spriteDirty_) {
                    loadManifest(bank);
                } else {
                    status("Save or revert the hotspots of " + spriteBank_ + " first", true);
                }
            }
        }
        ImGui::EndCombo();
    }
    if (spriteBank_.empty() || !spriteManifest_.contains("images")) {
        ImGui::End();
        return;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!spriteDirty_);
    if (ImGui::Button("Save")) {
        std::ofstream out(manifestPath, std::ios::binary | std::ios::trunc);
        const std::string text = tools::toJsonText(spriteManifest_);
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (out) {
            spriteDirty_ = false;
            textures_.reload(spriteBank_);
            status("Saved " + manifestPath.u8string() +
                   ". If names changed, run: bbassets ids assets/source src/assets/include");
        } else {
            status("Cannot write " + manifestPath.u8string(), true);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Revert")) {
        loadManifest(spriteBank_);
    }
    ImGui::EndDisabled();

    tools::Json& images = spriteManifest_["images"];
    const auto& pictures = textures_.bank(spriteBank_);
    const int count = static_cast<int>(std::min(images.size(), pictures.size()));

    ImGui::BeginChild("list", ImVec2(220, 0), ImGuiChildFlags_Borders);
    for (int i = 0; i < count; ++i) {
        const Picture& p = pictures[static_cast<std::size_t>(i)];
        ImGui::PushID(i);
        const float s = 32.0f / static_cast<float>(std::max({p.width, p.height, 1}));
        if (p.texture != nullptr) {
            ImGui::Image(textureId(p.texture), ImVec2(static_cast<float>(p.width) * s, static_cast<float>(p.height) * s));
            ImGui::SameLine(44);
        }
        const tools::Json& item = images[static_cast<std::size_t>(i)];
        const std::string label = std::to_string(i) + (item.contains("name") ? " " + item["name"].get<std::string>() : "");
        if (ImGui::Selectable(label.c_str(), i == spriteImage_, 0, ImVec2(0, 32))) {
            spriteImage_ = i;
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("image");
    if (spriteImage_ >= 0 && spriteImage_ < count) {
        tools::Json& item = images[static_cast<std::size_t>(spriteImage_)];
        const Picture& p = pictures[static_cast<std::size_t>(spriteImage_)];
        int hotspot[2] = {item["hotspot"][0].get<int>(), item["hotspot"][1].get<int>()};
        if (ImGui::InputInt2("hotspot", hotspot)) {
            item["hotspot"] = tools::Json::array({hotspot[0], hotspot[1]});
            spriteDirty_ = true;
        }
        char name[64];
        std::snprintf(name, sizeof(name), "%s", item.contains("name") ? item["name"].get<std::string>().c_str() : "");
        if (ImGui::InputText("name", name, sizeof(name), ImGuiInputTextFlags_EnterReturnsTrue)) {
            static const std::regex kName("[a-z0-9]+(_[a-z0-9]+)*");
            const std::string n = name;
            if (n.empty()) {
                item.erase("name");
                spriteDirty_ = true;
            } else if (std::regex_match(n, kName)) {
                item["name"] = n;
                spriteDirty_ = true;
            } else {
                status("Names are lower_case: letters, digits and single underscores", true);
            }
        }
        ImGui::SliderInt("zoom", &spriteZoom_, 1, 8);
        ImGui::TextDisabled("%dx%d, %s. Click the image to set the hotspot (the point drawn at an entity's x, y).",
                            p.width, p.height, item.value("layout", "").c_str());
        const ImVec2 at = ImGui::GetCursorScreenPos();
        const float z = static_cast<float>(spriteZoom_);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 size(static_cast<float>(p.width) * z, static_cast<float>(p.height) * z);
        dl->AddRectFilled(at, ImVec2(at.x + size.x, at.y + size.y), IM_COL32(60, 60, 70, 255));
        if (p.texture != nullptr) {
            dl->AddImage(textureId(p.texture), at, ImVec2(at.x + size.x, at.y + size.y));
        }
        ImGui::InvisibleButton("canvas", ImVec2(std::max(size.x, 1.0f), std::max(size.y, 1.0f)));
        if (ImGui::IsItemClicked()) {
            const ImVec2 m = ImGui::GetMousePos();
            item["hotspot"] = tools::Json::array({static_cast<int>((m.x - at.x) / z), static_cast<int>((m.y - at.y) / z)});
            spriteDirty_ = true;
        }
        const float hx = at.x + (static_cast<float>(item["hotspot"][0].get<int>()) + 0.5f) * z;
        const float hy = at.y + (static_cast<float>(item["hotspot"][1].get<int>()) + 0.5f) * z;
        dl->AddLine(ImVec2(hx - 12, hy), ImVec2(hx + 12, hy), IM_COL32(255, 0, 0, 255), 2);
        dl->AddLine(ImVec2(hx, hy - 12), ImVec2(hx, hy + 12), IM_COL32(255, 0, 0, 255), 2);
    }
    ImGui::EndChild();
    ImGui::End();
}

// --- Popups ----------------------------------------------------------------

void EditorApp::newLevelPopup() {
    if (newLevelRequested_) {
        ImGui::OpenPopup("New level");
        newLevelRequested_ = false;
    }
    if (!ImGui::BeginPopupModal("New level", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    ImGui::TextWrapped("A level with one screen and the assets of %s. Add it to campaign/bb.json to play it in the campaign; Level > Play tests it alone.",
                       stem_.c_str());
    char name[64];
    std::snprintf(name, sizeof(name), "%s", newLevelName_.c_str());
    if (ImGui::InputText("file name", name, sizeof(name))) {
        newLevelName_ = name;
    }
    static const std::regex kStem("[a-z0-9_]{1,15}");
    const bool valid = std::regex_match(newLevelName_, kStem) && !contains(levels_, newLevelName_);
    if (!valid) {
        ImGui::TextDisabled("Lower case letters, digits and _, at most 15, not an existing level.");
    }
    ImGui::BeginDisabled(!valid);
    if (ImGui::Button("Create")) {
        const std::string path = (fs::u8path(sourceDir_) / "levels" / (newLevelName_ + ".json")).u8string();
        doc_ = std::make_unique<LevelDocument>(LevelDocument::created(path, LevelDocument::blankLevel(doc_->data())));
        stem_ = newLevelName_;
        selection_ = {};
        fitPending_ = true;
        screensTextFresh_ = false;
        save();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void EditorApp::unsavedPopup() {
    if (unsavedRequested_) {
        ImGui::OpenPopup("Unsaved changes");
        unsavedRequested_ = false;
    }
    if (!ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    ImGui::Text("%s has changes that are not saved.", stem_.c_str());
    auto run = [this] {
        auto action = std::move(pendingAction_);
        pendingAction_ = nullptr;
        ImGui::CloseCurrentPopup();
        if (action) {
            action();
        }
    };
    if (ImGui::Button("Save")) {
        if (save()) {
            run();
        } else {
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Discard")) {
        const std::string stem = stem_;
        open(stem);  // back to the saved level
        run();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        pendingAction_ = nullptr;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// --- Canvas ----------------------------------------------------------------

void EditorApp::show(const std::string& part) {
    if (part == "place") {
        tool_ = Tool::Place;
    } else if (part == "platforms") {
        tool_ = Tool::Platform;
    } else if (part == "level") {
        levelWindowOpen_ = true;
    } else if (part == "sprites" && doc_) {
        spriteWindowOpen_ = true;
        spriteBank_ = doc_->data().assets[2];
        std::ifstream in(fs::u8path(sourceDir_) / "sprites" / tools::stemOf(spriteBank_) / "sprites.json", std::ios::binary);
        spriteManifest_ = tools::Json::parse(in, nullptr, false);
        spriteImage_ = 5;
    } else if (part == "select" && doc_) {
        const auto& events = doc_->data().events;
        for (int i = static_cast<int>(events.size()) - 1; i >= 0; --i) {
            if (events[static_cast<std::size_t>(i)].type == static_cast<std::int32_t>(LevelEventType::Enemy)) {
                selection_ = {Selected::Event, i};
                break;
            }
        }
    }
}

void EditorApp::lookAt(float x, float zoom) {
    fitPending_ = false;
    zoom_ = std::clamp(zoom, 0.05f, 8.0f);
    cameraX_ = x;
    cameraY_ = -20;
}

void EditorApp::showWhole() {
    if (!doc_ || viewWidth_ <= 0) {
        return;
    }
    zoom_ = std::clamp(std::min(viewWidth_ / static_cast<float>(doc_->data().width()), viewHeight_ / kLevelHeight), 0.05f, 8.0f);
    cameraX_ = 0;
    cameraY_ = -(viewHeight_ / zoom_ - kLevelHeight) / 2;
}

void EditorApp::zoomAt(float factor, float sx, float sy) {
    const float wx = toWorldX(sx);
    const float wy = toWorldY(sy);
    zoom_ = std::clamp(zoom_ * factor, 0.05f, 8.0f);
    cameraX_ = wx - (sx - originX_) / zoom_;
    cameraY_ = wy - (sy - originY_) / zoom_;
}

void EditorApp::canvas(float x, float y, float width, float height) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Level", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoTitleBar);
    ImGui::PopStyleVar();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 size = ImGui::GetContentRegionAvail();
    originX_ = origin.x;
    originY_ = origin.y;
    viewWidth_ = size.x;
    viewHeight_ = size.y;
    ImGui::InvisibleButton("canvas", ImVec2(std::max(size.x, 1.0f), std::max(size.y, 1.0f)),
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                               ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
    dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(35, 35, 40, 255));
    if (doc_) {
        if (fitPending_ && size.x > 0) {
            showWhole();
            if (zoom_ < 0.5f) {
                // Long levels: start at the first screens, readable.
                zoom_ = std::min(1.0f, viewHeight_ / kLevelHeight);
                cameraY_ = -(viewHeight_ / zoom_ - kLevelHeight) / 2;
            }
            fitPending_ = false;
        }
        drawLevel(dl);
        canvasInput(hovered);
    }
    dl->PopClipRect();
    ImGui::End();
}

void EditorApp::drawPicture(ImDrawList* dl, const Picture& p, float wx, float wy, std::uint32_t tint) {
    const float x1 = toScreenX(wx - static_cast<float>(p.hotspotX));
    const float y1 = toScreenY(wy - static_cast<float>(p.hotspotY));
    dl->AddImage(textureId(p.texture), ImVec2(x1, y1),
                 ImVec2(x1 + static_cast<float>(p.width) * zoom_, y1 + static_cast<float>(p.height) * zoom_), ImVec2(0, 0),
                 ImVec2(1, 1), tint);
}

void EditorApp::drawBox(ImDrawList* dl, float wx, float wy, const std::string& label, std::uint32_t colour) {
    const ImVec2 a(toScreenX(wx) - 12, toScreenY(wy) - 24);
    const ImVec2 b(toScreenX(wx) + 12, toScreenY(wy));
    dl->AddRectFilled(a, b, (colour & 0x00FFFFFFu) | 0x60000000u);
    dl->AddRect(a, b, colour);
    dl->AddText(ImVec2(a.x, a.y - ImGui::GetFontSize()), colour, label.c_str());
}

void EditorApp::drawLevel(ImDrawList* dl) {
    const LevelData& level = doc_->data();
    hits_.clear();
    const float left = toWorldX(originX_);
    const float right = toWorldX(originX_ + viewWidth_);

    // The level's frame.
    dl->AddRectFilled(ImVec2(toScreenX(0), toScreenY(0)),
                      ImVec2(toScreenX(static_cast<float>(level.width())), toScreenY(kLevelHeight)), IM_COL32(15, 15, 20, 255));

    // Decor, one image per screen.
    if (showDecor_) {
        for (std::size_t i = 0; i < level.screens.size(); ++i) {
            const float sx = static_cast<float>(i) * 640;
            if (sx + 640 < left || sx > right) {
                continue;
            }
            const Picture* p = textures_.picture(level.assets[0], level.screens[i]);
            if (p != nullptr) {
                drawPicture(dl, *p, sx + static_cast<float>(p->hotspotX), static_cast<float>(p->hotspotY), IM_COL32_WHITE);
            }
            dl->AddText(ImVec2(toScreenX(sx) + 4, toScreenY(kLevelHeight) + 2), IM_COL32(150, 150, 150, 255),
                        ("screen " + std::to_string(i) + ": " + std::to_string(level.screens[i])).c_str());
        }
    }
    for (std::size_t i = 0; i <= level.screens.size(); ++i) {
        const float sx = toScreenX(static_cast<float>(i) * 640);
        dl->AddLine(ImVec2(sx, toScreenY(0)), ImVec2(sx, toScreenY(kLevelHeight)), IM_COL32(255, 255, 255, 40));
    }

    // Events in the game's drawing order: midground decals, props, enemies
    // and the rest, foreground decals last.
    auto drawPass = [&](auto keep) {
        for (int i = static_cast<int>(level.events.size()) - 1; i >= 0; --i) {
            const LevelEventRecord& e = level.events[static_cast<std::size_t>(i)];
            if (keep(e)) {
                drawEvent(dl, i, e);
            }
        }
    };
    using T = LevelEventType;
    auto is = [](const LevelEventRecord& e, T t) { return e.type == static_cast<std::int32_t>(t); };
    if (showDecals_) {
        drawPass([&](const LevelEventRecord& e) { return is(e, T::MidgroundDecal); });
    }
    if (showEntities_) {
        drawPass([&](const LevelEventRecord& e) { return is(e, T::AnimatedProp); });
        drawPass([&](const LevelEventRecord& e) {
            return hasPosition(e.type) && !isDecal(e.type) && !is(e, T::AnimatedProp);
        });
    }

    // Walls (8x8 cells) and platforms.
    const int columns = level.wallColumns();
    const int c1 = std::max(0, static_cast<int>(left / 8) - 1);
    const int c2 = std::min(columns - 1, static_cast<int>(right / 8) + 1);
    for (int pass = 0; pass < 2; ++pass) {
        const bool blood = pass == 1;
        if (blood ? !showBloodWalls_ : !showSolidWalls_) {
            continue;
        }
        const auto& cells = blood ? level.bloodWalls : level.solidWalls;
        const ImU32 colour = blood ? IM_COL32(255, 230, 0, 110) : IM_COL32(255, 40, 40, 110);
        for (int row = 0; row < LevelData::kWallRows; ++row) {
            for (int c = c1; c <= c2; ++c) {
                if (cells[static_cast<std::size_t>(row) * static_cast<std::size_t>(columns) + static_cast<std::size_t>(c)] != 0) {
                    const float wx = static_cast<float>(c) * 8;
                    const float wy = static_cast<float>(row) * 8;
                    dl->AddRectFilled(ImVec2(toScreenX(wx), toScreenY(wy)), ImVec2(toScreenX(wx + 8), toScreenY(wy + 8)), colour);
                }
            }
        }
    }
    const int x1 = std::max(0, static_cast<int>(left) - 1);
    const int x2 = std::min(level.width() - 1, static_cast<int>(right) + 1);
    for (int layer = 0; layer < LevelData::kPlatformLayers; ++layer) {
        if (!showPlatforms_[layer]) {
            continue;
        }
        const auto& heights = level.platforms[static_cast<std::size_t>(layer)];
        int start = x1;
        for (int x = x1; x <= x2 + 1; ++x) {
            const bool end = x > x2 || heights[static_cast<std::size_t>(x)] != heights[static_cast<std::size_t>(start)];
            if (!end) {
                continue;
            }
            const std::int32_t h = heights[static_cast<std::size_t>(start)];
            if (h != LevelData::kNoPlatform) {
                const float wy = static_cast<float>(h);
                dl->AddLine(ImVec2(toScreenX(static_cast<float>(start)), toScreenY(wy)), ImVec2(toScreenX(static_cast<float>(x)), toScreenY(wy)),
                            kLayerColours[layer], 2);
            }
            start = x;
        }
    }

    if (showDecals_) {
        drawPass([&](const LevelEventRecord& e) { return is(e, T::Foreground); });
    }
    if (showLineEvents_) {
        drawPass([&](const LevelEventRecord& e) { return !hasPosition(e.type); });
    }

    // Starts and victory.
    auto cross = [&](float wx, float wy, ImU32 colour, const char* label, Selected what) {
        const float sx = toScreenX(wx);
        const float sy = toScreenY(wy);
        dl->AddLine(ImVec2(sx - 10, sy), ImVec2(sx + 10, sy), colour, 2);
        dl->AddLine(ImVec2(sx, sy - 10), ImVec2(sx, sy + 10), colour, 2);
        dl->AddText(ImVec2(sx + 6, sy - 22), colour, label);
        hits_.push_back({{what, -1}, sx - 10, sy - 22, sx + 30, sy + 10});
    };
    cross(static_cast<float>(level.start1X), static_cast<float>(level.start1Y), IM_COL32(80, 160, 255, 255), "P1", Selected::Start1);
    cross(static_cast<float>(level.start2X), static_cast<float>(level.start2Y), IM_COL32(255, 80, 80, 255), "P2", Selected::Start2);
    const float vx = toScreenX(static_cast<float>(level.victoryX));
    dl->AddLine(ImVec2(vx, toScreenY(0)), ImVec2(vx, toScreenY(kLevelHeight)), IM_COL32(60, 255, 60, 255), 2);
    dl->AddText(ImVec2(vx + 4, toScreenY(kLevelHeight) - 40), IM_COL32(60, 255, 60, 255), "victory (screen's left edge)");
    hits_.push_back({{Selected::Victory, -1}, vx - 4, toScreenY(0), vx + 4, toScreenY(kLevelHeight)});

    // The selection.
    for (const Hit& h : hits_) {
        if (h.what.kind == selection_.kind && h.what.index == selection_.index && selection_.kind != Selected::None) {
            dl->AddRect(ImVec2(h.x1 - 2, h.y1 - 2), ImVec2(h.x2 + 2, h.y2 + 2), IM_COL32(255, 255, 255, 255), 0.0f, 0, 2.0f);
        }
    }
}

void EditorApp::drawEvent(ImDrawList* dl, int index, const LevelEventRecord& e) {
    const LevelData& level = doc_->data();
    const auto type = static_cast<LevelEventType>(e.type);
    const Selection self{Selected::Event, index};

    if (!hasPosition(e.type)) {
        const float sx = toScreenX(static_cast<float>(e.activationX));
        const ImU32 colour = lineColour(e.type);
        dl->AddLine(ImVec2(sx, toScreenY(0)), ImVec2(sx, toScreenY(kLevelHeight)), colour, 1);
        const float ly = toScreenY(0) + 4 + static_cast<float>(e.type % 8) * (ImGui::GetFontSize() + 2);
        const std::string label = lineLabel(e);
        dl->AddText(ImVec2(sx + 3, ly), colour, label.c_str());
        hits_.push_back({self, sx - 4, toScreenY(0), sx + 4 + ImGui::CalcTextSize(label.c_str()).x, toScreenY(kLevelHeight)});
        return;
    }

    const float wx = static_cast<float>(e.x);
    const float wy = static_cast<float>(e.y);
    const Picture* p = nullptr;
    std::string label;
    ImU32 colour = IM_COL32(255, 255, 255, 255);
    ImU32 tint = IM_COL32_WHITE;
    switch (type) {
        case LevelEventType::MidgroundDecal:
        case LevelEventType::Foreground:
            p = textures_.picture(level.assets[1], e.id);
            label = std::string(type == LevelEventType::Foreground ? "foreground " : "midground ") + std::to_string(e.id);
            break;
        case LevelEventType::Enemy:
        case LevelEventType::EnemyGenerator:
            p = entityPicture(EntityCategory::Enemy, e.id);
            label = entityName(EntityCategory::Enemy, e.id);
            if (type == LevelEventType::EnemyGenerator) {
                char g[64];
                std::snprintf(g, sizeof(g), " [generator %d/%d%s]", e.capacity, e.period, e.temporary == 1 ? " temporary" : "");
                label += g;
                tint = IM_COL32(255, 200, 255, 200);
                colour = IM_COL32(255, 140, 255, 255);
            }
            break;
        case LevelEventType::AnimatedProp:
            p = entityPicture(EntityCategory::Prop, e.id);
            label = entityName(EntityCategory::Prop, e.id);
            colour = IM_COL32(120, 180, 255, 255);
            break;
        case LevelEventType::Vehicle:
            label = entityName(EntityCategory::Vehicle, e.id) + " dir " + std::to_string(e.value);
            colour = IM_COL32(255, 180, 80, 255);
            break;
        case LevelEventType::Pickup:
            label = entityName(EntityCategory::Pickup, e.id);
            colour = IM_COL32(255, 70, 70, 255);
            break;
        default:
            break;
    }

    float x1 = 0;
    float y1 = 0;
    float x2 = 0;
    float y2 = 0;
    if (p != nullptr) {
        drawPicture(dl, *p, wx, wy, tint);
        x1 = toScreenX(wx - static_cast<float>(p->hotspotX));
        y1 = toScreenY(wy - static_cast<float>(p->hotspotY));
        x2 = x1 + static_cast<float>(p->width) * zoom_;
        y2 = y1 + static_cast<float>(p->height) * zoom_;
        if (type == LevelEventType::EnemyGenerator) {
            dl->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), colour);
            dl->AddText(ImVec2(x1, y1 - ImGui::GetFontSize()), colour, label.c_str());
        }
    } else {
        drawBox(dl, wx, wy, label, colour);
        x1 = toScreenX(wx) - 12;
        y1 = toScreenY(wy) - 24;
        x2 = toScreenX(wx) + 12;
        y2 = toScreenY(wy);
    }
    // Facing: an arrow at the feet.
    if (type == LevelEventType::Enemy || type == LevelEventType::EnemyGenerator) {
        const float sx = toScreenX(wx);
        const float sy = toScreenY(wy) + 4;
        const float d = e.value == 0 ? -8.0f : 8.0f;
        dl->AddTriangleFilled(ImVec2(sx + d, sy), ImVec2(sx, sy - 4), ImVec2(sx, sy + 4), IM_COL32(255, 255, 0, 220));
    }
    hits_.push_back({self, x1, y1, x2, y2});
}

void EditorApp::select(Selection s) {
    selection_ = s;
}

void EditorApp::deleteSelection() {
    if (doc_ && selection_.kind == Selected::Event) {
        doc_->removeEvent(selection_.index);
        selection_ = {};
    }
}

void EditorApp::duplicateSelection() {
    if (!doc_ || selection_.kind != Selected::Event) {
        return;
    }
    const LevelEventRecord e = doc_->data().events[static_cast<std::size_t>(selection_.index)];
    LevelEventRecord copy = e;
    if (hasPosition(copy.type)) {
        copy.x += 32;
    } else {
        copy.activationX += 32;
    }
    selection_.index = doc_->addEvent(copy);
}

void EditorApp::paintStroke(float wx, float wy, bool erase) {
    // Every column between the last point and this one, so a fast stroke
    // leaves no gap.
    const float fromX = painting_ ? lastPaintX_ : wx;
    const float fromY = painting_ ? lastPaintY_ : wy;
    const int a = static_cast<int>(std::floor(std::min(fromX, wx)));
    const int b = static_cast<int>(std::floor(std::max(fromX, wx)));
    for (int x = a; x <= b; ++x) {
        const float t = b == a ? 1.0f : static_cast<float>(x - a) / static_cast<float>(b - a);
        const float y = fromX <= wx ? fromY + (wy - fromY) * t : wy + (fromY - wy) * t;
        if (tool_ == Tool::Platform) {
            const auto height = erase ? LevelData::kNoPlatform : static_cast<std::int32_t>(std::floor(y));
            doc_->setPlatform(platformLayer_, x - (brushWidth_ - 1), x + (brushWidth_ - 1), height);
        } else {
            const int column = static_cast<int>(std::floor(static_cast<float>(x) / 8));
            const int row = static_cast<int>(std::floor(y / 8));
            for (int dx = 0; dx < wallBrush_; ++dx) {
                for (int dy = 0; dy < wallBrush_; ++dy) {
                    doc_->setWall(wallBlood_, column + dx, row + dy, erase ? 0 : 1);
                }
            }
        }
    }
    lastPaintX_ = wx;
    lastPaintY_ = wy;
    painting_ = true;
}

void EditorApp::canvasInput(bool hovered) {
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 mouse = ImGui::GetMousePos();
    const float wx = toWorldX(mouse.x);
    const float wy = toWorldY(mouse.y);

    if (hovered) {
        if (io.MouseWheel != 0 && (io.KeyCtrl || io.KeySuper)) {
            zoomAt(io.MouseWheel > 0 ? 1.25f : 0.8f, mouse.x, mouse.y);
        } else {
            cameraX_ -= (io.MouseWheelH + io.MouseWheel) * 120 / zoom_;
        }
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0)) {
        cameraX_ -= io.MouseDelta.x / zoom_;
        cameraY_ -= io.MouseDelta.y / zoom_;
    }

    // Brush outline.
    if (hovered && (tool_ == Tool::Platform || tool_ == Tool::Wall)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (tool_ == Tool::Platform) {
            const float w = static_cast<float>(brushWidth_ - 1);
            dl->AddLine(ImVec2(toScreenX(wx - w), mouse.y), ImVec2(toScreenX(wx + w + 1), mouse.y), kLayerColours[platformLayer_], 2);
        } else {
            const float cx = std::floor(wx / 8) * 8;
            const float cy = std::floor(wy / 8) * 8;
            const float s = static_cast<float>(wallBrush_) * 8;
            dl->AddRect(ImVec2(toScreenX(cx), toScreenY(cy)), ImVec2(toScreenX(cx + s), toScreenY(cy + s)), IM_COL32_WHITE);
        }
    }

    // Before the first mouse event Dear ImGui puts the mouse at -FLT_MAX,
    // which does not fit an int (UBSan, the self-test on Linux).
    const auto toLevel = [](float v) {
        return static_cast<std::int32_t>(std::clamp(std::floor(v), -1.0e9f, 1.0e9f));
    };
    const auto ix = toLevel(wx);
    const auto iy = toLevel(wy);
    switch (tool_) {
        case Tool::Select: {
            if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                Selection hit;
                for (auto it = hits_.rbegin(); it != hits_.rend(); ++it) {
                    if (mouse.x >= it->x1 && mouse.x <= it->x2 && mouse.y >= it->y1 && mouse.y <= it->y2) {
                        hit = it->what;
                        break;
                    }
                }
                select(hit);
                const LevelData& level = doc_->data();
                if (hit.kind != Selected::None) {
                    dragging_ = true;
                    dragStartX_ = wx;
                    dragStartY_ = wy;
                    if (hit.kind == Selected::Event) {
                        const LevelEventRecord& e = level.events[static_cast<std::size_t>(hit.index)];
                        dragOriginX_ = positionX(e);
                        dragOriginY_ = e.y;
                    } else if (hit.kind == Selected::Start1 || hit.kind == Selected::Start2) {
                        dragOriginX_ = hit.kind == Selected::Start1 ? level.start1X : level.start2X;
                        dragOriginY_ = hit.kind == Selected::Start1 ? level.start1Y : level.start2Y;
                    } else {
                        dragOriginX_ = level.victoryX;
                        dragOriginY_ = 0;
                    }
                    doc_->beginGroup();
                }
            }
            if (dragging_ && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                const auto nx = dragOriginX_ + static_cast<std::int32_t>(std::lround(wx - dragStartX_));
                const auto ny = dragOriginY_ + static_cast<std::int32_t>(std::lround(wy - dragStartY_));
                const LevelData& level = doc_->data();
                switch (selection_.kind) {
                    case Selected::Event:
                        selection_.index = doc_->moveEvent(selection_.index, nx, ny);
                        break;
                    case Selected::Start1:
                    case Selected::Start2:
                        doc_->setStart(selection_.kind == Selected::Start1 ? 0 : 1, nx, ny);
                        break;
                    case Selected::Victory:
                        doc_->setVictory(nx, level.victoryFlag1, level.victoryValue1, level.victoryFlag2, level.victoryValue2);
                        break;
                    case Selected::None:
                        break;
                }
            }
            if (dragging_ && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                dragging_ = false;
                doc_->endGroup();
            }
            break;
        }
        case Tool::Place:
            if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                LevelEventRecord e = newEvent(static_cast<LevelEventType>(placeType_), ix, iy);
                if (entityField(placeType_) != EventEntity::None) {
                    e.id = placeEntity_;
                } else if (isDecal(placeType_)) {
                    e.id = placeImage_;
                    e.capacity = placeImage_;  // the 2002 editor's list index: none here
                }
                select({Selected::Event, doc_->addEvent(e)});
            }
            break;
        case Tool::Platform:
        case Tool::Wall: {
            const bool left = ImGui::IsMouseDown(ImGuiMouseButton_Left);
            const bool right = ImGui::IsMouseDown(ImGuiMouseButton_Right);
            if (!painting_ && hovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
                paintErase_ = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
                doc_->beginGroup();
                paintStroke(wx, wy, paintErase_);
            } else if (painting_ && (paintErase_ ? right : left)) {
                paintStroke(wx, wy, paintErase_);
            } else if (painting_) {
                painting_ = false;
                doc_->endGroup();
            }
            break;
        }
        case Tool::Start1:
        case Tool::Start2:
            if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                doc_->setStart(tool_ == Tool::Start1 ? 0 : 1, ix, iy);
            }
            break;
        case Tool::Victory:
            if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                const LevelData& level = doc_->data();
                doc_->setVictory(ix, level.victoryFlag1, level.victoryValue1, level.victoryFlag2, level.victoryValue2);
            }
            break;
    }
}

// --- Test play -------------------------------------------------------------

void EditorApp::playLevel(bool rebuild) {
    if (!doc_ || playState_ != 0) {
        return;
    }
    if (doc_->dirty() && !save()) {
        return;
    }
    // The game and bbassets next to bbedit in a build tree (tools/ under
    // the game's directory) or in a package.
    const fs::path tools = fs::u8path(toolDir_);
    fs::path game = tools.parent_path() / "blipblop";
    if (!fs::exists(game)) {
        game = tools / "blipblop";
    }
    const fs::path bbassets = tools / "bbassets";
    if (!fs::exists(game) || (rebuild && !fs::exists(bbassets))) {
        status("Test play needs blipblop (and bbassets to rebuild the assets) next to bbedit or one directory up", true);
        return;
    }
    // The level's kind in the campaign (docs/formats/lst.md); 1, the first
    // of a world, when it is not in it.
    int kind = 1;
    {
        std::ifstream in(fs::u8path(sourceDir_) / "campaign" / "bb.json", std::ios::binary);
        try {
            const tools::Json campaign = tools::Json::parse(in);
            static const std::map<std::string, int> kKinds = {{"bonus", 0}, {"first", 1},    {"intermediate", 2},
                                                              {"last", 3},  {"complete", 4}, {"end", 5}};
            for (const auto& entry : campaign.at("entries")) {
                if (entry.value("level", "") == "data/" + stem_ + ".lvl" && kKinds.count(entry.value("kind", "")) != 0) {
                    kind = kKinds.at(entry.value("kind", ""));
                }
            }
        } catch (const tools::Json::exception&) {
            kind = 1;  // no readable campaign: the default
        }
    }
    const std::string play = " --level data/" + stem_ + ".lvl " + std::to_string(kind);
    std::string packCommand;
    std::string playCommand;
    if (rebuild) {
        // Every asset from the source tree.
        const fs::path pack = fs::temp_directory_path() / "bbedit-test.bbpack";
        packCommand = quote(bbassets.u8string()) + " pack " + quote(sourceDir_) + " " + quote(pack.u8string());
        playCommand = quote(game.u8string()) + " --pack " + quote(pack.u8string()) + play;
    } else {
        // The level alone in an overlay pack, the other assets from the
        // game's own pack (or data directory).
        core::assets::PackWriter writer;
        writer.addLevel(stem_ + ".lvl", doc_->data());
        std::string error;
        const auto bytes = writer.finish(error);
        const fs::path overlay = fs::temp_directory_path() / "bbedit-level.bbpack";
        std::ofstream out(overlay, std::ios::binary | std::ios::trunc);
        if (bytes) {
            out.write(reinterpret_cast<const char*>(bytes->data()), static_cast<std::streamsize>(bytes->size()));
        }
        out.close();
        if (!bytes || !out) {
            status("Cannot write " + overlay.u8string() + (error.empty() ? "" : ": " + error), true);
            return;
        }
        const fs::path gamePack = game.parent_path() / "blipblop.bbpack";
        const std::string assets = fs::exists(gamePack) ? " --pack " + quote(gamePack.u8string())
                                                        : " --data " + quote((game.parent_path() / "data").u8string());
        playCommand = quote(game.u8string()) + assets + " --overlay " + quote(overlay.u8string()) + play;
    }
    if (player_.joinable()) {
        player_.join();
    }
    playState_ = 1;
    status((rebuild ? "Building the assets, then playing " : "Playing ") + stem_ + " (level kind " +
           std::to_string(kind) + ")...");
    player_ = std::thread([this, packCommand, playCommand] {
        // NOLINTNEXTLINE(bugprone-command-processor): runs the tools; the paths are quoted.
        if (!packCommand.empty() && std::system(packCommand.c_str()) != 0) {
            playState_ = 3;
            return;
        }
        // NOLINTNEXTLINE(bugprone-command-processor): runs the game; the paths are quoted.
        playState_ = std::system(playCommand.c_str()) == 0 ? 2 : 3;
    });
}

}  // namespace bb::editor
