#!/bin/sh
# Install the bundled demo and optional personal Uxn ROMs on an iPod/simdisk.

set -eu

usage() {
    echo "Usage: $0 MOUNT_OR_SIMDISK [ROM.rom ...]" >&2
    exit 2
}

[ "$#" -ge 1 ] || usage
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
destination=$1/Uxn
shift

[ -d "${destination%/Uxn}" ] || {
    echo "Not a directory: ${destination%/Uxn}" >&2
    exit 1
}
mkdir -p "$destination"
base64 -d "$repo_root/apps/plugins/uxn/roms/rockbox-demo.rom.b64" \
    > "$destination/rockbox-demo.rom"

for rom in "$@"; do
    [ -f "$rom" ] || { echo "ROM not found: $rom" >&2; exit 1; }
    case "$rom" in
        *.rom|*.ROM) ;;
        *) echo "Expected a .rom file: $rom" >&2; exit 1 ;;
    esac
    size=$(wc -c < "$rom")
    [ "$size" -le 1048320 ] || {
        echo "ROM exceeds the 16-bank Uxn limit: $rom" >&2
        exit 1
    }
    cp "$rom" "$destination/"
done

sync
echo "Installed Uxn ROMs in $destination"
