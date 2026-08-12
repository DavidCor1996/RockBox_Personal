#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
theos_root="${THEOS:-/tmp/rockpod-theos}"
sdl_version="2.32.10"
sdl_source="/tmp/SDL2-$sdl_version"
sdl_archive="/tmp/SDL2-$sdl_version.tar.gz"
sdl_build="/tmp/rockpod-sdl-ios"
ios_build="$repo_root/build-ios-ipod6g"
sdk="$theos_root/sdks/iPhoneOS16.5.sdk"
toolchain="$theos_root/toolchain/linux/iphone/bin"
jobs="${ROCKPOD_BUILD_JOBS:-4}"

test -x "$toolchain/clang"
test -d "$sdk"
if [[ ! -d "$sdl_source/include" ]]; then
    if [[ ! -f "$sdl_archive" ]]; then
        curl -fL "https://github.com/libsdl-org/SDL/releases/download/release-$sdl_version/SDL2-$sdl_version.tar.gz" \
            -o "$sdl_archive"
    fi
    tar -xzf "$sdl_archive" -C /tmp
fi

cmake -S "$sdl_source" -B "$sdl_build" \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT="$sdk" \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_C_COMPILER="$toolchain/clang" \
    -DCMAKE_C_FLAGS="-target arm64-apple-ios12.0" \
    -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF
cmake --build "$sdl_build" --parallel "$jobs"

mkdir -p "$ios_build"
(cd "$ios_build" && "$repo_root/tools/configure" --target=ipod6g \
    --ram=64 --rbdir=/.rockbox --type=s --no-ccache)
# configure selects HAVE_SIGALTSTACK_THREADS for this target, but Rockbox's
# sigaltstack scheduler needs swapcontext()-style stack switching, which arm64
# iOS does not permit.  Every Rockbox thread after the first therefore never
# runs, and the first thing that waits on another thread blocks forever --
# show_logo_boot()'s sleep(HZ), then tagcache_init()'s wait loop.  SDL threads
# make each Rockbox thread a real pthread, which is the supported model for
# hosted SDL targets.  firmware/SOURCES keys thread-sdl.c off this define.
if grep -q 'HAVE_SIGALTSTACK_THREADS' "$ios_build/autoconf.h"; then
    sed -i 's/#define HAVE_SIGALTSTACK_THREADS/#define HAVE_SDL_THREADS/' \
        "$ios_build/autoconf.h"
    echo "Switched embedded core to SDL threads."
    # The threading model changes struct layouts and kernel internals, so a
    # partial tree cannot be trusted.
    rm -f "$ios_build/librockbox-ios.a" "$ios_build/firmware/libfirmware.a"
    find "$ios_build" -name '*.o' -delete
fi

if ! grep -q 'rockpod_ios_overrides.make' "$ios_build/Makefile"; then
    sed -i '/include $(TOOLSDIR)\/root.make/i include $(ROOTDIR)/tools/ios/rockpod_ios_overrides.make' \
        "$ios_build/Makefile"
fi

# The rule is defined as $(BUILDDIR)/librockbox-ios.a with an absolute
# BUILDDIR, so the relative name matches no rule.  It only ever seemed to
# work because a stale archive already existed and make reported "nothing
# to be done" -- meaning core changes were silently never relinked.
make -C "$ios_build" -j"$jobs" "$ios_build/librockbox-ios.a"
# Build every plugin, not a hand-picked four, so the companion's Extras and
# Games menus behave like the player's.  -k keeps going past plugins that
# cannot link here (sm64 needs an imported ROM); the staging step packages
# whatever .rock files exist.
make -C "$ios_build" -j"$jobs" -k ENABLEDPLUGINS=yes rocks || true
for codec in mpa aac alac wav flac vorbis opus wma aiff; do
    make -C "$ios_build" -j"$jobs" \
        "$ios_build/lib/rbcodec/codecs/$codec.codec"
done

"$repo_root/tools/ios/stage_rockpod_simulator_install.sh"

# The app shows CFBundleShortVersionString, and crash reports are filed under
# it, but the release version lives in debian/control.  When they drift the
# build looks stale on screen and crash logs name the wrong build -- which is
# exactly how a fixed crash appeared to still be happening.  Derive the plist
# from control so there is one source of truth.
app_version="$(sed -n 's/^Version: *//p' "$repo_root/tools/ios/RockPodLink/control")"
plist="$repo_root/tools/ios/RockPodLink/Resources/Info.plist"
if [[ -n "$app_version" && -f "$plist" ]]; then
    build_number="$(printf '%s' "$app_version" | tr -d '.')"
    perl -0pi -e "s{(<key>CFBundleShortVersionString</key>\s*<string>)[^<]*}{\${1}$app_version}s" "$plist"
    perl -0pi -e "s{(<key>CFBundleVersion</key>\s*<string>)[^<]*}{\${1}$build_number}s" "$plist"
    echo "App version: $app_version (build $build_number)"
fi
# Theos only tracks the app's own .m files, so a change confined to the
# Rockbox core (librockbox-ios.a) leaves the previously linked executable in
# place and the .ipa ships stale code under a new version number.  That
# silently cost a debugging round.  Drop the link products so the app is
# always relinked against the core just built.
rm -rf "$repo_root/tools/ios/RockPodLink/.theos/obj"

make -C "$repo_root/tools/ios/RockPodLink" package FINALPACKAGE=1 \
    THEOS="$theos_root" ROCKPOD_SDL_SOURCE="$sdl_source" \
    ROCKPOD_SDL_LIBRARY="$sdl_build/libSDL2.a"
