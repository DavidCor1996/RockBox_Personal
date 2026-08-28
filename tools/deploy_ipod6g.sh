#!/usr/bin/env bash
# Deploy a built iPod tree to the mounted player.
#
# Defaults to the iPod Classic 6G/7G build.  Set ROCKBOX_BUILD to deploy a
# different target, e.g. ROCKBOX_BUILD=build-hw-ipodvideo for the iPod Video
# 5G/5.5G.  The two players load different firmware, so deploying the wrong
# build dir bricks the boot until it is corrected.
#
# Copying rockbox.ipod alone is not a deploy.  Plugins (.rock), image decoder
# overlays (.ovl) and codecs are separate files loaded at run time, and a stale
# one against fresh firmware fails in ways that read as unrelated bugs -- a
# stale viewers/jpeg.ovl makes the Photos app report "Can't open
# /.rockbox/rocks/viewers/jpeg.ovl" when you open a JPEG, which looks like a
# broken photo rather than a stale overlay.
#
# Usage:  tools/deploy_ipod6g.sh [/path/to/mounted/ipod]
#         ROCKBOX_BUILD=build-hw-ipodvideo tools/deploy_ipod6g.sh
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
build="$repo_root/${ROCKBOX_BUILD:-build-hw-ipod6g}"
ipod="${1:-}"

if [[ ! -d "$build" ]]; then
    echo "no such build directory: $build" >&2
    exit 1
fi

if [[ -z "$ipod" ]]; then
    for candidate in /run/media/"$USER"/*; do
        if [[ -f "$candidate/.rockbox/config.cfg" ]]; then
            ipod="$candidate"
            break
        fi
    done
fi

if [[ -z "$ipod" || ! -d "$ipod/.rockbox" ]]; then
    echo "No mounted iPod with a .rockbox directory found." >&2
    echo "Usage: $0 /run/media/\$USER/<IPOD>" >&2
    exit 1
fi
if [[ ! -f "$build/rockbox.ipod" ]]; then
    echo "missing $build/rockbox.ipod -- build first" >&2
    exit 1
fi

# The 5G and 6G share a mount layout but not a firmware format, so refuse a
# cross-target deploy rather than leaving the player unbootable.
build_target="$(sed -n 's/^Target: //p' "$build/rockbox-info.txt" 2>/dev/null)"
ipod_target="$(sed -n 's/^Target: //p' "$ipod/.rockbox/rockbox-info.txt" \
    2>/dev/null)"
if [[ -n "$build_target" && -n "$ipod_target" &&
      "$build_target" != "$ipod_target" ]]; then
    echo "Target mismatch: build is '$build_target', iPod is '$ipod_target'." >&2
    echo "Set ROCKBOX_BUILD to the matching build directory." >&2
    exit 1
fi

echo "Deploying to $ipod ($build_target from ${build##*/})"

# Both firmware locations: different bootloader paths load different copies,
# so updating only one is not a valid deploy.
cp "$build/rockbox.ipod" "$ipod/rockbox.ipod"
cp "$build/rockbox.ipod" "$ipod/.rockbox/rockbox.ipod"

# Plugins, overlays and codecs, preserving the rocks/ subdirectory layout that
# buildzip.pl produces (viewers/, games/, apps/, ...).
if [[ -f "$build/rockbox.zip" ]]; then
    staging="$(mktemp -d)"
    trap 'rm -rf "$staging"' EXIT
    unzip -q -o "$build/rockbox.zip" '.rockbox/rocks/*' '.rockbox/codecs/*' \
        -d "$staging"
    rsync -a "$staging/.rockbox/rocks/" "$ipod/.rockbox/rocks/"
    rsync -a "$staging/.rockbox/codecs/" "$ipod/.rockbox/codecs/"
    # The ipodjs asset tree as a whole is far larger than a player carries,
    # so it is not synced wholesale.  The Live TV boot ident is the one part
    # the firmware needs to have: without it Live TV falls back to the black
    # screen it used to show, with nothing on screen to say why.
    unzip -q -o "$build/rockbox.zip" '.rockbox/ipodjs/livetv/boot/*' \
        -d "$staging" 2>/dev/null || true
    if [[ -d "$staging/.rockbox/ipodjs/livetv/boot" ]]; then
        mkdir -p "$ipod/.rockbox/ipodjs/livetv/boot"
        rsync -a "$staging/.rockbox/ipodjs/livetv/boot/" \
            "$ipod/.rockbox/ipodjs/livetv/boot/"
    fi
    echo "Synced rocks/, codecs/ and the Live TV boot ident from rockbox.zip"
else
    echo "warning: $build/rockbox.zip not found; run 'make zip' to ship" >&2
    echo "         plugins, decoder overlays and codecs too." >&2
fi

sync

firmware_ok=1
for path in "$ipod/rockbox.ipod" "$ipod/.rockbox/rockbox.ipod"; do
    if ! cmp -s "$build/rockbox.ipod" "$path"; then
        echo "MISMATCH: $path" >&2
        firmware_ok=0
    fi
done
boot_pack="${staging:-}/.rockbox/ipodjs/livetv/boot/boot-320x240.nfr"
if [[ -n "${staging:-}" && -f "$boot_pack" ]]; then
    if ! cmp -s "$boot_pack" \
                "$ipod/.rockbox/ipodjs/livetv/boot/boot-320x240.nfr"; then
        echo "MISMATCH: ipodjs/livetv/boot/boot-320x240.nfr" >&2
        firmware_ok=0
    fi
fi
if [[ -f "$build/apps/plugins/imageviewer/jpeg/jpeg.ovl" ]]; then
    if ! cmp -s "$build/apps/plugins/imageviewer/jpeg/jpeg.ovl" \
                "$ipod/.rockbox/rocks/viewers/jpeg.ovl"; then
        echo "MISMATCH: viewers/jpeg.ovl" >&2
        firmware_ok=0
    fi
fi
if [[ "$firmware_ok" -ne 1 ]]; then
    echo "Deploy verification FAILED." >&2
    exit 1
fi

sha256sum "$build/rockbox.ipod" "$ipod/rockbox.ipod" \
    "$ipod/.rockbox/rockbox.ipod"
echo "Deploy verified.  Eject before unplugging, then reboot the iPod."
