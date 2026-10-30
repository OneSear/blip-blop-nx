# Optional sanitizers for every target, e.g.
#   cmake --preset asan          (address;undefined)
# or -DBB_SANITIZERS="address;undefined".

option(BB_SANITIZERS_RECOVER "Keep running after a sanitizer finding (to collect them all)" OFF)

if(BB_SANITIZERS)
    string(REPLACE ";" "," _bb_san "${BB_SANITIZERS}")
    add_compile_options(-fsanitize=${_bb_san} -fno-omit-frame-pointer)
    if(NOT BB_SANITIZERS_RECOVER)
        # The first finding fails the run.
        add_compile_options(-fno-sanitize-recover=all)
    endif()
    add_link_options(-fsanitize=${_bb_san})
    unset(_bb_san)
    message(STATUS "Sanitizers enabled: ${BB_SANITIZERS}")
endif()

# Locals filled with a pattern instead of left uninitialised (migration.md
# section 6): a read of an uninitialised local then changes the replays'
# fingerprints instead of passing by luck. On in the asan preset.
option(BB_POISON_LOCALS "Fill uninitialised locals with a pattern (-ftrivial-auto-var-init=pattern)" OFF)
if(BB_POISON_LOCALS)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag(-ftrivial-auto-var-init=pattern BB_HAS_AUTO_VAR_INIT)
    if(BB_HAS_AUTO_VAR_INIT)
        add_compile_options(-ftrivial-auto-var-init=pattern)
        message(STATUS "Uninitialised locals filled with a pattern")
    endif()
endif()
