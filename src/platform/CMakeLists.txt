# bb_platform: the platform interfaces (headers only). They depend on
# bb::core for the shared value types (Pixmap, input codes).
add_library(bb_platform INTERFACE)
add_library(bb::platform ALIAS bb_platform)
target_include_directories(bb_platform INTERFACE include)
target_link_libraries(bb_platform INTERFACE bb::core)

# The asset tools alone (BB_TOOLS_ONLY, the Switch build's host tools) need
# the interfaces only.
if(BB_TOOLS_ONLY)
    return()
endif()

# bb_platform_sdl2: the SDL 2 backend: the Switch (devkitPro's SDL 2, with
# the few libnx calls of sdl2/switch) and local builds on a Mac or Linux.
# The only code that includes SDL headers, and libnx's.
add_library(bb_platform_sdl2 STATIC
    sdl2/sdl2_platform.cpp
)
if(NINTENDO_SWITCH)
    target_sources(bb_platform_sdl2 PRIVATE sdl2/switch/switch_system.cpp)
endif()
add_library(bb::platform_sdl2 ALIAS bb_platform_sdl2)
target_include_directories(bb_platform_sdl2 PUBLIC sdl2/include)
target_link_libraries(bb_platform_sdl2 PUBLIC bb::platform PRIVATE SDL2::SDL2)
bb_set_strict_warnings(bb_platform_sdl2)

# The process entry point (calls bbMain, see entry.hpp). An object library:
# its main() must be linked into the executable. On the Switch it also
# starts the system services the game needs (switch_main.cpp).
if(NINTENDO_SWITCH)
    add_library(bb_platform_sdl2_main OBJECT sdl2/switch/switch_main.cpp)
else()
    add_library(bb_platform_sdl2_main OBJECT sdl2/sdl2_main.cpp)
endif()
target_link_libraries(bb_platform_sdl2_main PUBLIC bb::platform PRIVATE SDL2::SDL2)
bb_set_strict_warnings(bb_platform_sdl2_main)
