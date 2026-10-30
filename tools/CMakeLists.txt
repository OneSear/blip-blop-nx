# Asset tools (migration.md Phase 2). They may use third-party libraries
# (DEC-11): stb for PNG files, nlohmann/json for the source manifests.

add_library(bb_tools_thirdparty INTERFACE)
target_include_directories(bb_tools_thirdparty SYSTEM INTERFACE
    "${CMAKE_SOURCE_DIR}/third_party/stb"
    "${CMAKE_SOURCE_DIR}/third_party/nlohmann")

add_library(bb_asset_tools STATIC
    bbassets/json_text.cpp
    bbassets/level_events.cpp
    bbassets/png_io.cpp
    bbassets/source_assets.cpp
    bbassets/sprite_ids.cpp
    bbassets/stb_impl.cpp
)
target_include_directories(bb_asset_tools PUBLIC bbassets)
# bb::game for the entity names levels use (its registry).
target_link_libraries(bb_asset_tools PUBLIC bb::core bb::game bb_tools_thirdparty)
bb_set_strict_warnings(bb_asset_tools)

add_executable(bbassets bbassets/main.cpp)
target_link_libraries(bbassets PRIVATE bb_asset_tools)
bb_set_strict_warnings(bbassets)

# The level editor (Phase 8): its document model (no user interface, tested
# by tests/editor) and the bbedit application (Dear ImGui over SDL 2); not in the
# tools-only build of the Switch build, which needs bbassets only.
if(NOT BB_TOOLS_ONLY)
    add_library(bb_editor STATIC
        editor/level_document.cpp
    )
    target_include_directories(bb_editor PUBLIC editor)
    target_link_libraries(bb_editor PUBLIC bb_asset_tools)
    bb_set_strict_warnings(bb_editor)

    # Dear ImGui (third_party/imgui, DEC-11) with its SDL 2 platform and
    # renderer backends: third-party code, built without the strict warnings.
    add_library(bb_imgui STATIC
        "${CMAKE_SOURCE_DIR}/third_party/imgui/imgui.cpp"
        "${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp"
        "${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp"
        "${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp"
        "${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_sdl2.cpp"
        "${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_sdlrenderer2.cpp"
    )
    target_include_directories(bb_imgui SYSTEM PUBLIC
        "${CMAKE_SOURCE_DIR}/third_party/imgui"
        "${CMAKE_SOURCE_DIR}/third_party/imgui/backends")
    target_link_libraries(bb_imgui PUBLIC SDL2::SDL2)

    add_executable(bbedit
        editor/bbedit.cpp
        editor/editor_app.cpp
        editor/self_test.cpp
        editor/textures.cpp
    )
    target_link_libraries(bbedit PRIVATE bb_editor bb_imgui)
    bb_set_strict_warnings(bbedit)
endif()

# stb's PNG coder is third-party code nobody debugs here: optimise it in
# every configuration, so debug builds of the tools stay usable.
set_source_files_properties(bbassets/stb_impl.cpp PROPERTIES COMPILE_OPTIONS "-O2")

# The runtime pack the game can run from (--pack), built from the source
# assets. Without an assets/source tree, the source assets are first
# extracted from the legacy data into the build directory.
set(BB_ASSET_SOURCE_DIR "${CMAKE_SOURCE_DIR}/assets/source" CACHE PATH
    "Source assets (PNG, JSON, WAV, Ogg, UTF-8 text) the runtime pack is built from")
set(BB_PACK "${BB_RUN_DIR}/blipblop.bbpack" CACHE INTERNAL "")

if(EXISTS "${BB_ASSET_SOURCE_DIR}")
    set(BB_PACK_SOURCE "${BB_ASSET_SOURCE_DIR}" CACHE INTERNAL "")
    file(GLOB_RECURSE pack_inputs CONFIGURE_DEPENDS "${BB_ASSET_SOURCE_DIR}/*")
else()
    set(BB_PACK_SOURCE "${CMAKE_BINARY_DIR}/assets-source" CACHE INTERNAL "")
    set(pack_inputs "${BB_PACK_SOURCE}/.extracted")
    add_custom_command(
        OUTPUT "${BB_PACK_SOURCE}/.extracted"
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${BB_PACK_SOURCE}"
        COMMAND bbassets extract "${BB_RUN_DIR}/data" "${BB_PACK_SOURCE}"
        COMMAND "${CMAKE_COMMAND}" -E touch "${BB_PACK_SOURCE}/.extracted"
        DEPENDS bbassets legacy_data "${BB_RUN_DIR}/data/.extracted"
        COMMENT "Extracting source assets from the legacy data (no ${BB_ASSET_SOURCE_DIR})"
        VERBATIM)
endif()

add_custom_command(
    OUTPUT "${BB_PACK}"
    COMMAND bbassets pack "${BB_PACK_SOURCE}" "${BB_PACK}"
    DEPENDS bbassets ${pack_inputs}
    COMMENT "Building the runtime pack from ${BB_PACK_SOURCE}"
    VERBATIM)
add_custom_target(assets_pack ALL DEPENDS "${BB_PACK}")
