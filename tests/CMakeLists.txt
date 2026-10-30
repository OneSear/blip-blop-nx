add_library(bb_doctest INTERFACE)
target_include_directories(bb_doctest SYSTEM INTERFACE "${CMAKE_SOURCE_DIR}/third_party/doctest")

add_executable(bb_unit_tests
    unit/main.cpp
    unit/rng_test.cpp
    unit/hash_test.cpp
    unit/input_timeline_test.cpp
    unit/vfs_test.cpp
    unit/input_test.cpp
    unit/log_test.cpp
    unit/screen_stack_test.cpp
    unit/entity_list_test.cpp
    unit/collision_map_test.cpp
    unit/entity_handle_test.cpp
    unit/entity_registry_test.cpp
    unit/level_event_test.cpp
    unit/trig_test.cpp
    unit/components_test.cpp
    unit/actor_test.cpp
    unit/fixed_clock_test.cpp
    unit/sprite_font_test.cpp
    unit/audio_test.cpp
    unit/hi_scores_test.cpp
    unit/mixer_test.cpp
    unit/viewport_test.cpp
)
target_link_libraries(bb_unit_tests PRIVATE bb::core bb::game bb_doctest)
bb_set_strict_warnings(bb_unit_tests)
add_test(NAME unit COMMAND bb_unit_tests)

# Conformance tests: compare bb::core against the SDL behaviour it replaces.
add_executable(bb_conformance_tests
    unit/main.cpp
    conformance/blit_vs_sdl_test.cpp
    conformance/sdl2_display_test.cpp
    conformance/sdl2_audio_test.cpp
    conformance/sdl2_input_test.cpp
    conformance/sdl2_files_test.cpp
)
target_link_libraries(bb_conformance_tests PRIVATE bb::core bb::platform_sdl2 bb_doctest SDL2::SDL2)
# The audio test decodes every shipped sound and music.
target_compile_definitions(bb_conformance_tests PRIVATE BB_TEST_DATA_DIR="${BB_RUN_DIR}/data")
add_dependencies(bb_conformance_tests legacy_data)
bb_set_strict_warnings(bb_conformance_tests)
add_test(NAME conformance COMMAND bb_conformance_tests)

# Format tests: every shipped data file through the bb::core readers and
# writers (round trips, counts from migration.md appendices).
add_executable(bb_format_tests
    unit/main.cpp
    formats/legacy_formats_test.cpp
    formats/pack_test.cpp
    formats/fuzz_test.cpp
)
target_link_libraries(bb_format_tests PRIVATE bb::core bb_doctest)
target_compile_definitions(bb_format_tests PRIVATE BB_TEST_DATA_DIR="${BB_RUN_DIR}/data")
add_dependencies(bb_format_tests legacy_data)
bb_set_strict_warnings(bb_format_tests)
add_test(NAME formats COMMAND bb_format_tests)

# Content tests: the levels place registered entities and existing assets,
# read from the data directory and from the runtime pack.
if(TARGET assets_pack)
    add_executable(bb_content_tests
        unit/main.cpp
        content/levels_test.cpp
    )
    target_link_libraries(bb_content_tests PRIVATE bb::core bb::game bb_doctest)
    target_compile_definitions(bb_content_tests PRIVATE
        BB_TEST_DATA_DIR="${BB_RUN_DIR}/data" BB_TEST_PACK="${BB_PACK}")
    add_dependencies(bb_content_tests legacy_data assets_pack)
    bb_set_strict_warnings(bb_content_tests)
    add_test(NAME content COMMAND bb_content_tests)
    set_tests_properties(content PROPERTIES LABELS assets)
endif()

# The level editor's document: every source level loads and saves byte for
# byte, and the edits keep the game's event order.
if(TARGET bb_editor)
    add_executable(bb_editor_tests
        unit/main.cpp
        editor/level_document_test.cpp
    )
    target_link_libraries(bb_editor_tests PRIVATE bb_editor bb_doctest)
    target_compile_definitions(bb_editor_tests PRIVATE BB_TEST_SOURCE_DIR="${BB_PACK_SOURCE}")
    if(NOT EXISTS "${BB_ASSET_SOURCE_DIR}")
        add_dependencies(bb_editor_tests assets_pack)
    endif()
    bb_set_strict_warnings(bb_editor_tests)
    add_test(NAME editor COMMAND bb_editor_tests)
endif()
if(TARGET bbedit)
    add_test(NAME editor.ui COMMAND bbedit "${BB_PACK_SOURCE}" --self-test)
endif()

# No French identifier in the code (migration.md 4.5, the glossary of
# Appendix B).
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    # The interpreter's path, not its target: CMake would run a target
    # through the cross-compiling emulator (Wine), which loses the result.
    add_test(NAME naming.french
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/check_identifiers.py" "${CMAKE_SOURCE_DIR}")
    # The translations use only characters the fonts have, and the
    # dialogues fit their box (tools/data/check_strings.py).
    add_test(NAME strings.fonts
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/data/check_strings.py"
                "${CMAKE_SOURCE_DIR}/assets/source")
endif()

add_subdirectory(replays)

# The source assets rebuild the shipped 2002 files (byte for byte, except
# the bytes docs/formats/source-assets.md lists).
if(TARGET bbassets)
    add_test(NAME assets.verify COMMAND bbassets verify "${BB_PACK_SOURCE}" "${BB_RUN_DIR}/data")
    set_tests_properties(assets.verify PROPERTIES LABELS assets TIMEOUT 1800)
    add_test(NAME assets.reproducible
        COMMAND "${CMAKE_COMMAND}" -DBBASSETS=$<TARGET_FILE:bbassets> -DSOURCE=${BB_PACK_SOURCE}
                "-DEMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}"
                -DPACK=${BB_PACK} -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/reproducible.bbpack
                -P ${CMAKE_CURRENT_SOURCE_DIR}/assets/pack_reproducible.cmake)
    set_tests_properties(assets.reproducible PROPERTIES LABELS assets TIMEOUT 1800)
    if(EXISTS "${BB_ASSET_SOURCE_DIR}")
        add_test(NAME assets.ids
            COMMAND bbassets ids-check "${BB_ASSET_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/src/assets/include")
        set_tests_properties(assets.ids PROPERTIES LABELS assets)
    endif()
endif()

# Benchmarks: built with the tests, run by hand (they open windows).
add_executable(bb_present_bench bench/present_bench.cpp)
target_link_libraries(bb_present_bench PRIVATE SDL2::SDL2)
bb_set_strict_warnings(bb_present_bench)
