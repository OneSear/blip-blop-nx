# bb_core: platform-independent services. Depends on the C++17 standard
# library only - no SDL, no OS headers (migration.md, section 3.1).
add_library(bb_core STATIC
    src/input_timeline.cpp
    src/gfx_pixmap.cpp
    src/gfx_bmp.cpp
    src/gfx_font.cpp
    src/gfx_sprite.cpp
    src/gfx_fingerprint.cpp
    src/gfx_viewport.cpp
    src/file_reader.cpp
    src/vfs.cpp
    src/input_names.cpp
    src/input_bindings.cpp
    src/settings.cpp
    src/log.cpp
    src/assets_image16.cpp
    src/assets_text_encoding.cpp
    src/assets_legacy_formats.cpp
    src/assets_level_markers.cpp
    src/assets_asset_source.cpp
    src/assets_pack.cpp
    src/audio_mixer.cpp
)
add_library(bb::core ALIAS bb_core)

target_include_directories(bb_core PUBLIC include)
target_compile_features(bb_core PUBLIC cxx_std_17)
bb_set_strict_warnings(bb_core)

# The mixer's music decoders (codecs/audio_codecs.cpp): stb_vorbis and
# dr_mp3 (third_party, DEC-11), built without the strict warnings.
add_library(bb_core_codecs STATIC codecs/audio_codecs.cpp)
target_include_directories(bb_core_codecs PRIVATE include)
target_include_directories(bb_core_codecs SYSTEM PRIVATE
    "${CMAKE_SOURCE_DIR}/third_party/stb"
    "${CMAKE_SOURCE_DIR}/third_party/dr_libs")
target_compile_features(bb_core_codecs PRIVATE cxx_std_17)
target_link_libraries(bb_core PRIVATE bb_core_codecs)
