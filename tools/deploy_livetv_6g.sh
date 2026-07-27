#!/usr/bin/env bash
# Install the iPod Classic 6G build, the Live TV plugins and the DIRECTV
# wordmark onto a mounted iPod, verifying every copy.
#
# Both firmware locations are written because different bootloader paths load
# the root copy or the .rockbox copy; updating only one is not a valid deploy.
#
# Usage: tools/deploy_livetv_6g.sh ["/run/media/$USER/<IPOD>"]

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$REPO/build-hw-ipod6g"

if [ $# -ge 1 ]; then
    MOUNT="$1"
else
    MOUNT=""
    for candidate in /run/media/"$USER"/*; do
        if [ -f "$candidate/.rockbox/rockbox-info.txt" ]; then
            MOUNT="$candidate"
            break
        fi
    done
fi

if [ -z "$MOUNT" ] || [ ! -d "$MOUNT" ]; then
    echo "No mounted Rockbox iPod found. Plug it in, then pass the mount path." >&2
    exit 1
fi

TARGET="$(sed -n 's/^Target: //p' "$MOUNT/.rockbox/rockbox-info.txt" 2>/dev/null || true)"
if [ "$TARGET" != "ipod6g" ]; then
    echo "Refusing to deploy: $MOUNT reports target '${TARGET:-unknown}', not ipod6g." >&2
    exit 1
fi

for required in "$BUILD/rockbox.ipod" \
                "$BUILD/apps/plugins/livetv.rock" \
                "$BUILD/apps/plugins/mpegplayer/mpegplayer.rock"; do
    [ -f "$required" ] || { echo "Missing build output: $required" >&2; exit 1; }
done

echo "Deploying to $MOUNT"
cp "$BUILD/rockbox.ipod" "$MOUNT/rockbox.ipod"
cp "$BUILD/rockbox.ipod" "$MOUNT/.rockbox/rockbox.ipod"

mkdir -p "$MOUNT/.rockbox/rocks/apps" "$MOUNT/.rockbox/rocks/viewers"
cp "$BUILD/apps/plugins/livetv.rock" "$MOUNT/.rockbox/rocks/apps/livetv.rock"
cp "$BUILD/apps/plugins/mpegplayer/mpegplayer.rock" \
   "$MOUNT/.rockbox/rocks/viewers/mpegplayer.rock"

BRAND="$HOME/.rockpod/livetv/directv.bmp"
if [ -f "$BRAND" ]; then
    mkdir -p "$MOUNT/Videos/LiveTV/logos"
    cp "$BRAND" "$MOUNT/Videos/LiveTV/logos/directv.bmp"
fi

sync

status=0
check() {
    local a="$1" b="$2"
    if [ "$(sha256sum <"$a" | cut -d' ' -f1)" = "$(sha256sum <"$b" | cut -d' ' -f1)" ]; then
        echo "  ok   $(basename "$b")"
    else
        echo "  FAIL $b" >&2
        status=1
    fi
}

echo "Verifying:"
check "$BUILD/rockbox.ipod" "$MOUNT/rockbox.ipod"
check "$BUILD/rockbox.ipod" "$MOUNT/.rockbox/rockbox.ipod"
check "$BUILD/apps/plugins/livetv.rock" "$MOUNT/.rockbox/rocks/apps/livetv.rock"
check "$BUILD/apps/plugins/mpegplayer/mpegplayer.rock" \
      "$MOUNT/.rockbox/rocks/viewers/mpegplayer.rock"

if [ "$status" -ne 0 ]; then
    echo "Deploy verification failed; do not eject yet." >&2
    exit 1
fi

echo "Deploy verified. Safe to eject."
