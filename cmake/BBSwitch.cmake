# The Switch build (switch-port.md, section 6), with devkitPro's Switch.cmake
# toolchain (the `switch` preset):
#
#   * bbassets and the runtime pack, built first for the machine the build
#     runs on: this source tree again, with BB_TOOLS_ONLY, in host-tools/;
#   * blipblop.nro, the game with the pack in its RomFS, for the Homebrew
#     Menu (CI builds it, and nothing else: .github/workflows);
#   * the target blipblop_nsp: blipblop-<version>.nsp, the installable title,
#     packed by hacBrewPack with the maintainer's own console keys, outside
#     ALL (tools/switch/make-nsp.sh runs the whole chain).

include(ExternalProject)

# The identity of the installed game: chosen once at random (an
# application's own ID: bits 0 to 12 clear), never changed. Not a secret.
set(BB_SWITCH_TITLE_ID "0x011142be60644000" CACHE STRING "Title ID of the .nsp")
set(BB_SWITCH_KEYS "$ENV{HOME}/.switch/prod.keys" CACHE FILEPATH
    "Keys hacBrewPack encrypts the .nsp with (header_key, key_area_key_application_00), from your own console")
set(BB_SWITCH_PACK "" CACHE FILEPATH "A runtime pack to use instead of the one built here (empty: built)")
set(BB_SWITCH_ICON "${CMAKE_SOURCE_DIR}/assets/switch/icon.jpg")

# The runtime pack.
set(bb_romfs "${CMAKE_BINARY_DIR}/romfs")
if(BB_SWITCH_PACK)
    set(bb_pack "${BB_SWITCH_PACK}")
    add_custom_target(bb_host_tools)
else()
    set(bb_host_dir "${CMAKE_BINARY_DIR}/host-tools")
    set(bb_pack "${bb_host_dir}/blipblop.bbpack")
    # A configure of its own, with the machine's compiler: the Switch's
    # toolchain is not passed on. Built every time: its own build is
    # incremental, and the pack follows assets/source.
    ExternalProject_Add(bb_host_tools
        SOURCE_DIR "${CMAKE_SOURCE_DIR}"
        BINARY_DIR "${bb_host_dir}"
        CMAKE_ARGS -DBB_TOOLS_ONLY=ON -DCMAKE_BUILD_TYPE=Release
        BUILD_COMMAND "${CMAKE_COMMAND}" --build . --target assets_pack
        INSTALL_COMMAND ""
        BUILD_ALWAYS ON
        BUILD_BYPRODUCTS "${bb_pack}"
        USES_TERMINAL_BUILD ON)
endif()

# The RomFS: the pack at its root, where the game looks first (romfs:/ is
# SDL's base path); the replays too in debug builds, to play them on the
# console (docs/replays.md).
add_custom_target(blipblop_pack)
add_dependencies(blipblop_pack bb_host_tools)
dkp_set_target_file(blipblop_pack "${bb_pack}")
dkp_add_asset_target(blipblop_romfs "${bb_romfs}")
dkp_install_assets(blipblop_romfs TARGETS blipblop_pack)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    file(GLOB bb_replays "${CMAKE_SOURCE_DIR}/tests/replays/*.bbreplay")
    add_custom_target(blipblop_replays)
    dkp_set_target_file(blipblop_replays ${bb_replays})
    dkp_install_assets(blipblop_romfs DESTINATION replays TARGETS blipblop_replays)
endif()

# The .nro: the program, its title and icon, the RomFS.
nx_generate_nacp(blipblop.nacp NAME "Blip & Blop" AUTHOR "LOADED Studio" VERSION "${BB_PACKAGE_VERSION}")
nx_create_nro(blipblop OUTPUT "${CMAKE_BINARY_DIR}/blipblop.nro" NACP blipblop.nacp ICON "${BB_SWITCH_ICON}"
              ROMFS blipblop_romfs)

# The .nsp's parts: the program as an NSO and its permissions (exefs/), the
# title and the icon (control/); hacBrewPack takes the RomFS above.
set(bb_nsp_dir "${CMAKE_BINARY_DIR}/nsp")
set(bb_npdm_json "${CMAKE_SOURCE_DIR}/cmake/switch/npdm.json")
add_custom_command(
    OUTPUT "${bb_nsp_dir}/exefs/main" "${bb_nsp_dir}/exefs/main.npdm"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${bb_nsp_dir}/exefs"
    COMMAND "${NX_ELF2NSO_EXE}" "$<TARGET_FILE:blipblop>" "${bb_nsp_dir}/exefs/main"
    COMMAND "${NX_NPDMTOOL_EXE}" "${bb_npdm_json}" "${bb_nsp_dir}/exefs/main.npdm"
    DEPENDS blipblop "${bb_npdm_json}"
    COMMENT "The .nsp's ExeFS: main (NSO) and main.npdm"
    VERBATIM)
add_custom_command(
    OUTPUT "${bb_nsp_dir}/control/control.nacp" "${bb_nsp_dir}/control/icon_AmericanEnglish.dat"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${bb_nsp_dir}/control"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_BINARY_DIR}/blipblop.nacp"
            "${bb_nsp_dir}/control/control.nacp"
    COMMAND "${CMAKE_COMMAND}" -E copy "${BB_SWITCH_ICON}" "${bb_nsp_dir}/control/icon_AmericanEnglish.dat"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/blipblop.nacp" "${BB_SWITCH_ICON}"
    COMMENT "The .nsp's control data: control.nacp and the icon"
    VERBATIM)
add_custom_target(blipblop_nsp_parts
    DEPENDS "${bb_nsp_dir}/exefs/main" "${bb_nsp_dir}/exefs/main.npdm" "${bb_nsp_dir}/control/control.nacp"
            "${bb_nsp_dir}/control/icon_AmericanEnglish.dat")
add_dependencies(blipblop_nsp_parts blipblop_romfs)

# The .nsp itself: needs hacBrewPack and the keys (cmake/switch/MakeNsp.cmake
# says what is missing).
find_program(BB_HACBREWPACK NAMES hacbrewpack hacBrewPack)
set(bb_nsp "${CMAKE_BINARY_DIR}/blipblop-${BB_PACKAGE_VERSION}.nsp")
add_custom_target(blipblop_nsp
    COMMAND "${CMAKE_COMMAND}"
            "-DHACBREWPACK=${BB_HACBREWPACK}" "-DKEYS=${BB_SWITCH_KEYS}" "-DTITLE_ID=${BB_SWITCH_TITLE_ID}"
            "-DWORK=${bb_nsp_dir}" "-DROMFS=${bb_romfs}" "-DOUTPUT=${bb_nsp}"
            -P "${CMAKE_SOURCE_DIR}/cmake/switch/MakeNsp.cmake"
    COMMENT "Packing ${bb_nsp}"
    VERBATIM)
add_dependencies(blipblop_nsp blipblop_nsp_parts)
