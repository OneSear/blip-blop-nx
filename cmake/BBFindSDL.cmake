# Locates SDL2 and exposes the imported target SDL2::SDL2 (SDL2_mixer is
# no longer used: the game mixes its sound itself, Phase 9).
#
# The Switch: devkitPro's SDL 2 through pkg-config (Switch.cmake sets it to
# aarch64-none-elf-pkg-config, which links statically: EGL, Mesa, libnx).
# A Mac or Linux: CMake package configs first (Homebrew, recent distros),
# pkg-config for the distributions that do not ship them.

if(NOT NINTENDO_SWITCH)
    find_package(SDL2 CONFIG QUIET)
endif()
if(NOT TARGET SDL2::SDL2)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(BB_PC_SDL2 REQUIRED IMPORTED_TARGET sdl2)
    add_library(SDL2::SDL2 INTERFACE IMPORTED)
    target_link_libraries(SDL2::SDL2 INTERFACE PkgConfig::BB_PC_SDL2)
    message(STATUS "SDL2 found through pkg-config: ${BB_PC_SDL2_VERSION}")
else()
    message(STATUS "SDL2 found through its CMake package: ${SDL2_VERSION}")
endif()
