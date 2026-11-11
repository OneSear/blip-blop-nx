# Regression replays (docs/replays.md): every *.bbreplay in this directory is
# played headless and its fingerprints compared with expected/<name>.bbhash.

file(GLOB BB_REPLAYS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.bbreplay")
set(BB_REPLAY_OUT "${CMAKE_CURRENT_BINARY_DIR}/out")

set(BB_UPDATE_COMMANDS "")
foreach(replay IN LISTS BB_REPLAYS)
    get_filename_component(name "${replay}" NAME_WE)
    add_test(NAME replay.${name}
        COMMAND "${CMAKE_COMMAND}"
            -DGAME=$<TARGET_FILE:blipblop> "-DEMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}"
            -DREPLAY=${replay}
            -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/expected/${name}.bbhash
            -DOUTPUT=${BB_REPLAY_OUT}/${name}.bbhash
            -DRUN_DIR=${BB_RUN_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_replay.cmake")
    set_tests_properties(replay.${name} PROPERTIES LABELS replay TIMEOUT 3600)
    # The same replay with the assets read from the runtime pack: the
    # fingerprints must not change (migration.md Phase 2 exit criterion).
    if(TARGET assets_pack)
        add_test(NAME replay.${name}.pack
            COMMAND "${CMAKE_COMMAND}"
                -DGAME=$<TARGET_FILE:blipblop> "-DEMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}"
                -DREPLAY=${replay}
                -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/expected/${name}.bbhash
                -DOUTPUT=${BB_REPLAY_OUT}/${name}.pack.bbhash
                -DRUN_DIR=${BB_RUN_DIR}
                -DPACK=${BB_PACK}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_replay.cmake")
        set_tests_properties(replay.${name}.pack PROPERTIES LABELS "replay;pack" TIMEOUT 3600)
    endif()
    list(APPEND BB_UPDATE_COMMANDS
        COMMAND "${CMAKE_COMMAND}"
            -DGAME=$<TARGET_FILE:blipblop> "-DEMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}"
            -DREPLAY=${replay}
            -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/expected/${name}.bbhash
            -DOUTPUT=${BB_REPLAY_OUT}/${name}.bbhash
            -DRUN_DIR=${BB_RUN_DIR}
            -DUPDATE=ON
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_replay.cmake")
    # The same replay with the levels in 16:9 (--widescreen), but attract,
    # which plays no level: fingerprints of their own.
    if(NOT name STREQUAL "attract")
        add_test(NAME replay.${name}.wide
            COMMAND "${CMAKE_COMMAND}"
                -DGAME=$<TARGET_FILE:blipblop> "-DEMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}"
                -DREPLAY=${replay}
                -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/expected/${name}.wide.bbhash
                -DOUTPUT=${BB_REPLAY_OUT}/${name}.wide.bbhash
                -DRUN_DIR=${BB_RUN_DIR}
                -DWIDE=ON
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_replay.cmake")
        set_tests_properties(replay.${name}.wide PROPERTIES LABELS "replay;wide" TIMEOUT 3600)
        list(APPEND BB_UPDATE_COMMANDS
            COMMAND "${CMAKE_COMMAND}"
                -DGAME=$<TARGET_FILE:blipblop> "-DEMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}"
                -DREPLAY=${replay}
                -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/expected/${name}.wide.bbhash
                -DOUTPUT=${BB_REPLAY_OUT}/${name}.wide.bbhash
                -DRUN_DIR=${BB_RUN_DIR}
                -DWIDE=ON
                -DUPDATE=ON
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_replay.cmake")
    endif()
endforeach()

# Smooth motion draws between ticks: the smurf level (scrolling, enemies,
# shots) must run the same ticks with it, and show other frames.
add_test(NAME replay.smurf.smooth
    COMMAND "${CMAKE_COMMAND}"
        -DGAME=$<TARGET_FILE:blipblop> "-DEMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}"
        -DREPLAY=${CMAKE_CURRENT_SOURCE_DIR}/smurf.bbreplay
        -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/expected/smurf.bbhash
        -DOUTPUT=${BB_REPLAY_OUT}/smurf.smooth.bbhash
        -DRUN_DIR=${BB_RUN_DIR}
        -DSMOOTH=ON
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_replay.cmake")
set_tests_properties(replay.smurf.smooth PROPERTIES LABELS replay TIMEOUT 3600)

add_custom_target(update_replay_hashes
    ${BB_UPDATE_COMMANDS}
    DEPENDS blipblop
    COMMENT "Re-recording the expected replay fingerprints"
    VERBATIM)
