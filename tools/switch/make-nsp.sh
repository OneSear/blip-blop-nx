#!/bin/sh
# Builds the Switch's installable title, build/switch/blipblop-<version>.nsp
# (switch-port.md, 6.4): the `switch` preset (which builds bbassets and the
# runtime pack first), then hacBrewPack with your own console's keys. CI
# builds the .nro only: it has no keys.
#
#   tools/switch/make-nsp.sh                  from the working tree, the project's version
#   tools/switch/make-nsp.sh --upload v1.2.0  from the tag (clean tree checked out at it),
#                                             then attached to its GitHub release
#
# Needs: devkitPro's switch-dev and switch-sdl2 (DEVKITPRO set), hacBrewPack
# on the PATH (or HACBREWPACK=<path>), the keys in ~/.switch/prod.keys (or
# BB_SWITCH_KEYS=<file>), and gh for --upload.
set -e
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root"

tag=
case "$1" in
    "") ;;
    --upload)
        tag=$2
        [ -n "$tag" ] || { echo "make-nsp.sh: --upload needs a tag (v1.2.0)" >&2; exit 2; }
        ;;
    *) echo "usage: $0 [--upload <tag>]" >&2; exit 2 ;;
esac

[ -n "$DEVKITPRO" ] && [ -f "$DEVKITPRO/cmake/Switch.cmake" ] || {
    echo "make-nsp.sh: devkitPro's Switch packages are needed: sudo dkp-pacman -S switch-dev switch-sdl2," >&2
    echo "             then DEVKITPRO=/opt/devkitpro" >&2
    exit 1
}
hacbrewpack=${HACBREWPACK:-$(command -v hacbrewpack || command -v hacBrewPack || true)}
[ -n "$hacbrewpack" ] || {
    echo "make-nsp.sh: hacBrewPack is needed: build it (make) from a mirror of the deleted" >&2
    echo "             The-4n/hacBrewPack, for example https://github.com/rlaphoenix/hacBrewPack" >&2
    exit 1
}
keys=${BB_SWITCH_KEYS:-$HOME/.switch/prod.keys}
[ -f "$keys" ] || {
    echo "make-nsp.sh: no keys at $keys: dump them from your own console (Lockpick_RCM)." >&2
    echo "             They are Nintendo's: never commit them." >&2
    exit 1
}

version_option=
if [ -n "$tag" ]; then
    [ -z "$(git status --porcelain)" ] || { echo "make-nsp.sh: the working tree has changes" >&2; exit 1; }
    [ "$(git rev-parse HEAD)" = "$(git rev-parse "$tag^{commit}")" ] || {
        echo "make-nsp.sh: HEAD is not $tag: git checkout $tag first" >&2
        exit 1
    }
    command -v gh > /dev/null || { echo "make-nsp.sh: --upload needs gh (GitHub CLI)" >&2; exit 1; }
    version=${tag#v}
    version_option=-DBB_PACKAGE_VERSION=$version
fi

cmake --preset switch $version_option "-DBB_HACBREWPACK=$hacbrewpack" "-DBB_SWITCH_KEYS=$keys"
cmake --build --preset switch
cmake --build --preset switch --target blipblop_nsp

version=$(sed -n 's/^BB_PACKAGE_VERSION:STRING=//p' build/switch/CMakeCache.txt)
nsp=build/switch/blipblop-$version.nsp
[ -f "$nsp" ] || { echo "make-nsp.sh: $nsp was not made" >&2; exit 1; }
if [ -n "$tag" ]; then
    gh release upload "$tag" "$nsp"
    echo "Attached $nsp to the release $tag"
fi
