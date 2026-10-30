# The game executable (an ELF on the Switch, which cmake/BBSwitch.cmake
# makes the .nro and the .nsp of): the entry point of the SDL 2 backend and
# the application (bbMain, App): the session, the device input, the settings,
# the string table and the files; the game itself is bb::game.
add_executable(blipblop
    main.cpp
    app.cpp
    device_input.cpp
    files.cpp
    session.cpp
    settings_file.cpp
    strings.cpp
)
target_link_libraries(blipblop PRIVATE
    bb::core
    bb::game
    bb::platform
    bb::platform_sdl2
    bb_platform_sdl2_main)
bb_set_strict_warnings(blipblop)

# Put the executable in the run directory for every configuration (the
# $<0:> generator expression stops multi-config generators from appending
# Debug/ or Release/), next to the extracted data/ folder.
set_target_properties(blipblop PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BB_RUN_DIR}/$<0:>"
    XCODE_SCHEME_WORKING_DIRECTORY "${BB_RUN_DIR}")

if(TARGET legacy_data)
    add_dependencies(blipblop legacy_data)
endif()
