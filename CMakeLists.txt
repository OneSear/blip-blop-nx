cmake_minimum_required(VERSION 3.16)

project(BlipBlop
    VERSION 0.1.0
    DESCRIPTION "Blip & Blop (LOADED Studio, 2002) for the Nintendo Switch - C++17 / SDL2 / CMake"
    LANGUAGES C CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not allowed. Use a preset, for example:\n"
        "  cmake --preset switch && cmake --build --preset switch")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BB_WARNINGS_AS_ERRORS "Treat warnings as errors in new code (src/, tests/, tools/)" ON)
option(BB_BUILD_TESTS "Build the unit and regression tests" ON)
# The Switch build builds bbassets and the runtime pack for the machine it
# runs on first (cmake/BBSwitch.cmake): this source tree again, in this mode.
option(BB_TOOLS_ONLY "Build bbassets and the runtime pack only (no SDL 2, game, tests or editor)" OFF)
set(BB_SANITIZERS "" CACHE STRING "Semicolon-separated sanitizers for all targets, e.g. address;undefined")
set(BB_PACKAGE_VERSION "${PROJECT_VERSION}" CACHE STRING "Version of the .nro and the .nsp (default: the project's)")

include(BBWarnings)
include(BBSanitizers)
if(NOT BB_TOOLS_ONLY)
    include(BBFindSDL)
endif()

# Local builds (a Mac, Linux) read the game data from ./data relative to the
# working directory: everything that runs (the game, the tests) lives in the
# build directory, next to the extracted data. The Switch reads the runtime
# pack from its RomFS.
set(BB_RUN_DIR "${CMAKE_BINARY_DIR}")
set(BB_LEGACY_DATA_ZIP "${CMAKE_SOURCE_DIR}/vc-projects/Blip_n_Blop_3/data.zip"
    CACHE FILEPATH "Legacy data archive extracted into the build directory")

add_subdirectory(src/core)
add_subdirectory(src/platform)
add_subdirectory(src/assets)
add_subdirectory(src/game)
if(NOT BB_TOOLS_ONLY)
    add_subdirectory(src/app)
endif()

# The game data: the 2002 data.zip extracted into <build>/data, again only
# when the archive changes (saved settings and scores survive rebuilds).
if(NOT NINTENDO_SWITCH)
    set(BB_DATA_STAMP "${BB_RUN_DIR}/data/.extracted")
    add_custom_command(
        OUTPUT "${BB_DATA_STAMP}"
        COMMAND "${CMAKE_COMMAND}" -E tar xf "${BB_LEGACY_DATA_ZIP}"
        COMMAND "${CMAKE_COMMAND}" -E touch "${BB_DATA_STAMP}"
        DEPENDS "${BB_LEGACY_DATA_ZIP}"
        WORKING_DIRECTORY "${BB_RUN_DIR}"
        COMMENT "Extracting the game data into ${BB_RUN_DIR}/data"
        VERBATIM)
    add_custom_target(legacy_data ALL DEPENDS "${BB_DATA_STAMP}")
endif()

option(BB_BUILD_TOOLS "Build the asset tools (bbassets) and the level editor (bbedit)" ON)
if(BB_BUILD_TOOLS OR BB_TOOLS_ONLY)
    add_subdirectory(tools)
endif()

if(BB_BUILD_TESTS AND NOT BB_TOOLS_ONLY)
    enable_testing()
    add_subdirectory(tests)
endif()

# The .nro and the .nsp (switch-port.md, section 6).
if(NINTENDO_SWITCH)
    include(BBSwitch)
endif()
