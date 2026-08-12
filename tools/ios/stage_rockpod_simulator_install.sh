#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
desktop_root="$repo_root/build-sim-ipod6g/simdisk"
device_root="${ROCKPOD_DEVICE_ROOT:-}"
ios_build="$repo_root/build-ios-ipod6g"
resources="$repo_root/tools/ios/RockPodLink/Resources"
destination="$resources/SimulatorInstall"
frameworks="$resources/Frameworks"
dynamic_manifest="$destination/.rockbox/rockpod-dynamic-code.tsv"

if [[ -n "$device_root" ]]; then
    if [[ ! -f "$device_root/.rockbox/config.cfg" ||
          ! -d "$device_root/.rockbox/ipodjs" ]]; then
        echo "invalid ROCKPOD_DEVICE_ROOT: $device_root" >&2
        exit 1
    fi
    echo "Staging the physical iPod's Rockbox configuration from $device_root."
fi

if [[ ! -d "$desktop_root/.rockbox/ipodjs" ]]; then
    echo "missing current simulator install: $desktop_root/.rockbox/ipodjs" >&2
    exit 1
fi
if [[ ! -f "$ios_build/librockbox-ios.a" ]]; then
    echo "missing embedded iOS core: $ios_build/librockbox-ios.a" >&2
    exit 1
fi

rm -rf "$destination"
rm -rf "$frameworks"
mkdir -p "$destination/.rockbox" "$destination/Music" \
    "$destination/Photos/.photo_thumbs" "$destination/Videos" \
    "$destination/Playlists"
mkdir -p "$frameworks"

# Copy the complete software/configuration tree used by the current desktop
# simulator. Host-machine binaries, personal test media, logs, ROMs and the
# multi-gigabyte optional map tile cache are deliberately not application
# resources; iPhone-local media roots are writable and populated separately.
rsync -a "$desktop_root/.rockbox/" "$destination/.rockbox/" \
    --exclude='.rockbox/' \
    --exclude='*.rock' --exclude='*.codec' --exclude='*.ovl' --exclude='logs/' \
    --exclude='backups/' --exclude='maps/' --exclude='roms/' \
    --exclude='games/' --exclude='ipodgames/' --exclude='db-backup/' \
    --exclude='database*.tcd' --exclude='tagcache*.tcd' \
    --exclude='rockpod-companion-install-*'

# A companion build for a connected owner should look exactly like that
# iPod, without packaging its music database, logs, ROMs, or multi-gigabyte
# optional app data.  Overlay the complete visual/configuration surface onto
# the current simulator software install.
if [[ -n "$device_root" ]]; then
    cp "$device_root/.rockbox/config.cfg" "$destination/.rockbox/config.cfg"
    # The companion panel is a picture of the iPod, not the iPod: there is no
    # backlight to save power on, and a timeout there just blanks the screen
    # the owner is looking at.  Zero means "always" for these settings
    # (formatter_time_unit_0_is_always).  Everything else stays exactly as the
    # physical player has it.
    # Take the player's entire .rockbox, not a hand-picked handful of
    # directories, so the companion has every asset, setting and data file the
    # iPod has.  Executable content is deliberately excluded: .rock and .codec
    # files must come from the iOS build (they are packaged as signed
    # dylibs below), and the multi-gigabyte media/ROM/database trees are not
    # application resources.
    rsync -a "$device_root/.rockbox/" "$destination/.rockbox/" \
        --exclude='.rockbox/' \
        --exclude='*.rock' --exclude='*.codec' --exclude='*.ovl' \
        --exclude='rockbox.ipod' \
        --exclude='logs/' --exclude='backups/' --exclude='maps/' \
        --exclude='roms/' --exclude='games/' --exclude='ipodgames/' \
        --exclude='db-backup/' --exclude='database*.tcd' \
        --exclude='tagcache*.tcd' --exclude='rockpod-companion-install-*' \
        --exclude='offlineweb/' --exclude='rockpod/' --exclude='android/' \
        --exclude='albumlist/' --exclude='videolist/' \
        --exclude='animalcrossing/' --exclude='flash/'

    # Applied after the rsync above, which rewrites config.cfg from the
    # player.  Zero means "always" for these (formatter_time_unit_0_is_always):
    # the companion panel is a picture of the iPod, so a backlight timeout only
    # blanks the screen the owner is looking at.
    sed -i -E '/^[[:space:]]*backlight timeout:/d;
               /^[[:space:]]*backlight timeout plugged:/d;
               /^[[:space:]]*lcd sleep after backlight off:/d' \
        "$destination/.rockbox/config.cfg"
    {
        printf 'backlight timeout: 0\n'
        printf 'backlight timeout plugged: 0\n'
        printf 'lcd sleep after backlight off: -1\n'
    } >> "$destination/.rockbox/config.cfg"
fi

# Selected phone tracks are materialized before the core starts.  Keep the
# simulator database watching /Music so those completed files are indexed on
# this launch instead of remaining visible only in Files.
sed -i -E '/^[[:space:]]*tagcache_autoupdate:/d' \
    "$destination/.rockbox/config.cfg"
printf 'tagcache_autoupdate: on\n' >> "$destination/.rockbox/config.cfg"

# Apple status and control bitmaps are package-time assets rather than normal
# simulator install outputs. Apply this canonical cache after the physical
# iPod theme overlay so an older on-device copy cannot replace a corrected
# stock glyph in the companion package.
if [[ -d "$repo_root/assets/ipodjs/apple" ]]; then
    mkdir -p "$destination/.rockbox/ipodjs/apple"
    rsync -a "$repo_root/assets/ipodjs/apple/" \
        "$destination/.rockbox/ipodjs/apple/"
fi

if [[ -d "$ios_build/apps/plugins" ]]; then
    # Plugins must land in the same subdirectories buildzip.pl uses, because
    # the menus open them by absolute path -- PLUGIN_APPS_DIR "/photos.rock"
    # is .rockbox/rocks/apps/photos.rock.  Staging them flat under rocks/ made
    # every plugin fail to open.  apps/plugins/CATEGORIES is the same mapping
    # buildzip.pl reads.
    categories="$repo_root/apps/plugins/CATEGORIES"
    while IFS= read -r plugin; do
        relative="${plugin#"$ios_build/apps/plugins/"}"
        base="$(basename "$relative" .rock)"
        subdir=""
        if [[ -f "$categories" ]]; then
            subdir="$(awk -F, -v n="$base" '$1==n {print $2; exit}' \
                "$categories")"
        fi
        if [[ -n "$subdir" ]]; then
            install_path=".rockbox/rocks/$subdir/$base.rock"
        else
            install_path=".rockbox/rocks/$base.rock"
        fi
        framework_name="RockPodPlugin__${relative//\//__}"
        framework_name="${framework_name%.rock}.dylib"
        cp "$plugin" "$frameworks/$framework_name"
        printf '%s\tFrameworks/%s\n' "$install_path" \
            "$framework_name" >> "$dynamic_manifest"
    done < <(find "$ios_build/apps/plugins" -type f -name '*.rock' | sort)
fi

if [[ -d "$ios_build/lib/rbcodec/codecs" ]]; then
    while IFS= read -r codec; do
        codec_name="$(basename "$codec")"
        framework_name="RockPodCodec__${codec_name%.codec}.dylib"
        cp "$codec" "$frameworks/$framework_name"
        printf '.rockbox/codecs/%s\tFrameworks/%s\n' "$codec_name" \
            "$framework_name" >> "$dynamic_manifest"
    done < <(find "$ios_build/lib/rbcodec/codecs" -maxdepth 1 \
        -type f -name '*.codec' | sort)
fi

# Imageviewer loads format decoders as overlays.  The desktop simulator's
# install contains x86-64 ELF .ovl files, which cannot be loaded by the arm64
# iOS core and caused Photos to terminate when an image was opened.  Package
# the arm64 Mach-O overlays as signed Framework contents and link them into
# the exact viewers paths imageviewer requests.
if [[ -d "$ios_build/apps/plugins/imageviewer" ]]; then
    while IFS= read -r overlay; do
        overlay_name="$(basename "$overlay")"
        framework_name="RockPodOverlay__${overlay_name%.ovl}.dylib"
        cp "$overlay" "$frameworks/$framework_name"
        printf '.rockbox/rocks/viewers/%s\tFrameworks/%s\n' \
            "$overlay_name" "$framework_name" >> "$dynamic_manifest"
    done < <(find "$ios_build/apps/plugins/imageviewer" -mindepth 2 \
        -maxdepth 2 -type f -name '*.ovl' | sort)
fi

# The companion shell is a source-controlled product asset.  Copying a stale
# generated build image here could silently put the old hand-drawn surround
# back into a newly packaged app.
cp "$repo_root/uisimulator/bitmaps/UI-ipod6g.bmp" "$resources/UI256.bmp"

# The iPodJS shell draws entirely from .rockbox/ipodjs.  The earlier existence
# check passed on an empty directory, so a 254 MB install shipped with none of
# those assets and the companion had nothing to render.  Require real files,
# and name the fix, rather than letting it ship silently again.
# The all-black panel was the capture path (see window-sdl.c), not the shell,
# so the companion keeps whatever engine the physical iPod uses -- iPodJS.
# Force a different one only for testing:  ROCKPOD_SIM_UI_ENGINE=rockbox
if [[ -n "${ROCKPOD_SIM_UI_ENGINE:-}" &&
      -f "$destination/.rockbox/config.cfg" ]]; then
    sed -i -E '/^[[:space:]]*ui engine:/d' "$destination/.rockbox/config.cfg"
    printf 'ui engine: %s\n' "$ROCKPOD_SIM_UI_ENGINE" \
        >> "$destination/.rockbox/config.cfg"
    echo "Companion simulator UI engine overridden: $ROCKPOD_SIM_UI_ENGINE"
fi

ipodjs_files="$(find "$destination/.rockbox/ipodjs" -type f 2>/dev/null | wc -l)"
if [[ "$ipodjs_files" -lt 50 ]]; then
    echo "staged .rockbox/ipodjs has only $ipodjs_files files;" \
         "the iPodJS shell will render nothing." >&2
    echo "Set ROCKPOD_DEVICE_ROOT to the mounted iPod so its ipodjs assets" \
         "are staged, e.g. ROCKPOD_DEVICE_ROOT=\"/run/media/\$USER/<IPOD>\"." >&2
    exit 1
fi
echo "Staged .rockbox/ipodjs with $ipodjs_files files."
install_id="$({
    cd "$destination"
    find .rockbox -type f -print0 | LC_ALL=C sort -z |
        xargs -0 sha256sum
} | sha256sum | cut -d' ' -f1)"
printf '%s\n' "$install_id" \
    > "$destination/.rockbox/rockpod-companion-install.id"

test -f "$destination/.rockbox/config.cfg"
test -d "$destination/.rockbox/ipodjs"
test -d "$destination/.rockbox/themes"
echo "Staged Rockbox simulator install ($(du -sh "$destination" | cut -f1))."
