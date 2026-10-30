# Packs the .nsp (the blipblop_nsp target of cmake/BBSwitch.cmake): runs
# hacBrewPack on the ExeFS and control folders of WORK and the RomFS, then
# names the result OUTPUT.
#
#   cmake -DHACBREWPACK=... -DKEYS=... -DTITLE_ID=... -DWORK=... -DROMFS=... -DOUTPUT=...
#         -P MakeNsp.cmake

if(NOT HACBREWPACK)
    message(FATAL_ERROR
        "hacBrewPack is needed for the .nsp: build it (make) from a mirror of the deleted The-4n/hacBrewPack, "
        "for example https://github.com/rlaphoenix/hacBrewPack, and put it on the PATH, then configure again")
endif()
if(NOT EXISTS "${KEYS}")
    message(FATAL_ERROR
        "No keys at ${KEYS}: dump them from your own console (Lockpick_RCM) into ~/.switch/prod.keys, "
        "or pass -DBB_SWITCH_KEYS=<file>. Never commit them.")
endif()

# hacBrewPack writes its NCAs, temporary files and a backup next to the NSP:
# all in a fresh folder of WORK.
set(out "${WORK}/hacbrewpack")
file(REMOVE_RECURSE "${out}")
file(MAKE_DIRECTORY "${out}")
execute_process(
    COMMAND "${HACBREWPACK}" --keyset "${KEYS}" --titleid "${TITLE_ID}"
            --exefsdir "${WORK}/exefs" --controldir "${WORK}/control" --romfsdir "${ROMFS}" --nologo
            --nspdir "${out}/nsp" --ncadir "${out}/nca" --tempdir "${out}/temp" --backupdir "${out}/backup"
    WORKING_DIRECTORY "${out}"
    RESULT_VARIABLE result)
file(GLOB nsp "${out}/nsp/*.nsp")
list(LENGTH nsp count)
if(NOT result EQUAL 0 OR NOT count EQUAL 1)
    message(FATAL_ERROR "hacBrewPack failed (exit status ${result}, ${count} .nsp written)")
endif()
file(REMOVE "${OUTPUT}")
file(RENAME "${nsp}" "${OUTPUT}")
file(SHA256 "${OUTPUT}" sha)
message(STATUS "${OUTPUT}\n  SHA-256 ${sha}")
