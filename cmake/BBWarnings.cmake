# Warning policy.
#
# All the code (src/, tests/, tools/) is built with a strict warning set and,
# by default, warnings as errors (the 2002 code, built with the compiler's
# default warnings, is gone since Phase 5): GCC (devkitA64 for the Switch,
# Linux) and Clang (a Mac).

function(bb_set_strict_warnings target)
    target_compile_options(${target} PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        -Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual)
    if(BB_WARNINGS_AS_ERRORS)
        target_compile_options(${target} PRIVATE -Werror)
    endif()
endfunction()
