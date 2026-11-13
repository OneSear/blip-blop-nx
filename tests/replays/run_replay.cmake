# Runs one regression replay and compares (or, with -DUPDATE=ON, stores) its
# fingerprints. Invoked by ctest and by the update_replay_hashes target.
# EMULATOR (optional) runs the game when it is cross-compiled (Wine).
# With -DSMOOTH=ON the game runs with --smooth-motion and only the tick
# (world state) fingerprints are compared: drawing between ticks must change
# the frames and nothing of the simulation. With -DWIDE=ON it runs with
# --widescreen (the levels in 16:9), against fingerprints of their own.

foreach(var GAME REPLAY EXPECTED OUTPUT RUN_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "run_replay.cmake: ${var} is not set")
    endif()
endforeach()

get_filename_component(out_dir "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${out_dir}")
get_filename_component(name "${REPLAY}" NAME_WE)

set(asset_options "")
if(DEFINED PACK)
    set(asset_options --pack "${PACK}")
endif()
if(SMOOTH)
    list(APPEND asset_options --smooth-motion)
endif()
if(WIDE)
    list(APPEND asset_options --widescreen)
endif()

execute_process(
    COMMAND ${EMULATOR} "${GAME}" --headless --replay "${REPLAY}" --hash "${OUTPUT}" ${asset_options}
    WORKING_DIRECTORY "${RUN_DIR}"
    RESULT_VARIABLE status
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr)

string(REGEX MATCHALL "\\[frame [^\n]*" milestones "${stdout}")
string(REGEX MATCH "session: [^\n]*" summary "${stdout}")
message(STATUS "${name}: ${summary}")

if(NOT status EQUAL 0)
    list(JOIN milestones "\n  " notes)
    message(FATAL_ERROR "${name}: the game exited with status ${status}\n  ${notes}\n${stderr}")
endif()

# The milestones the replay must reach, in this order ('# expect <milestone>'
# lines of the replay, written by make_replays.py): a level it must win. Fingerprints
# alone would pass a run stuck in a level, if it was recorded so.
file(STRINGS "${REPLAY}" expected_milestones REGEX "^# expect ")
if(expected_milestones)
    set(reached "")
    foreach(m IN LISTS milestones)
        string(REGEX REPLACE "^\\[frame [0-9]+, tick [0-9]+\\] " "" m "${m}")
        list(APPEND reached "${m}")
    endforeach()
    list(LENGTH reached n_reached)
    set(next 0)
    foreach(e IN LISTS expected_milestones)
        string(REGEX REPLACE "^# expect " "" e "${e}")
        set(found FALSE)
        while(next LESS n_reached)
            list(GET reached ${next} m)
            math(EXPR next "${next} + 1")
            if(m STREQUAL e)
                set(found TRUE)
                break()
            endif()
        endwhile()
        if(NOT found)
            list(JOIN milestones "\n  " notes)
            message(FATAL_ERROR "${name}: never reached '${e}' (in order). Its milestones:\n  ${notes}")
        endif()
    endforeach()
endif()

if(UPDATE)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E copy "${OUTPUT}" "${EXPECTED}")
    message(STATUS "${name}: expected fingerprints updated")
    return()
endif()

if(NOT EXISTS "${EXPECTED}")
    message(FATAL_ERROR "${name}: no expected fingerprints (${EXPECTED}); "
                        "record them with the update_replay_hashes target")
endif()

file(STRINGS "${EXPECTED}" expected_lines)
file(STRINGS "${OUTPUT}" actual_lines)
if(SMOOTH)
    set(expected_frames ${expected_lines})
    set(actual_frames ${actual_lines})
    list(FILTER expected_frames INCLUDE REGEX "^frame ")
    list(FILTER actual_frames INCLUDE REGEX "^frame ")
    if(expected_frames STREQUAL actual_frames)
        message(FATAL_ERROR "${name}: smooth motion changed no frame")
    endif()
    list(FILTER expected_lines INCLUDE REGEX "^tick ")
    list(FILTER actual_lines INCLUDE REGEX "^tick ")
endif()
list(LENGTH expected_lines n_expected)
list(LENGTH actual_lines n_actual)
set(i 0)
while(i LESS n_expected AND i LESS n_actual)
    list(GET expected_lines ${i} e)
    list(GET actual_lines ${i} a)
    if(NOT e STREQUAL a)
        math(EXPR line "${i} + 1")
        # The frames of this build around the divergence, as BMP files: from
        # the frame checkpoint before it to the one after (one frame in 6).
        set(from 0)
        set(j ${i})
        while(j GREATER 0)
            math(EXPR j "${j} - 1")
            list(GET actual_lines ${j} previous)
            if(previous MATCHES "^frame ([0-9]+) ")
                set(from ${CMAKE_MATCH_1})
                break()
            endif()
        endwhile()
        math(EXPR to "${from} + 60")
        set(j ${i})
        while(j LESS n_actual)
            list(GET actual_lines ${j} next)
            if(next MATCHES "^frame ([0-9]+) ")
                set(to ${CMAKE_MATCH_1})
                break()
            endif()
            math(EXPR j "${j} + 1")
        endwhile()
        set(dump_dir "${out_dir}/${name}-frames")
        file(REMOVE_RECURSE "${dump_dir}")
        file(MAKE_DIRECTORY "${dump_dir}")
        execute_process(
            COMMAND ${EMULATOR} "${GAME}" --headless --replay "${REPLAY}" ${asset_options}
                    --dump-frames "${dump_dir}" ${from} ${to} --dump-every 6
            WORKING_DIRECTORY "${RUN_DIR}" OUTPUT_QUIET ERROR_QUIET)
        message(FATAL_ERROR "${name}: first divergence at line ${line}\n"
                            "  expected: ${e}\n  actual:   ${a}\n"
                            "Frames ${from} to ${to} of this build (one in 6): ${dump_dir}")
    endif()
    math(EXPR i "${i} + 1")
endwhile()
if(NOT n_expected EQUAL n_actual)
    message(FATAL_ERROR "${name}: ${n_actual} fingerprint lines, ${n_expected} expected")
endif()
message(STATUS "${name}: fingerprints match")
