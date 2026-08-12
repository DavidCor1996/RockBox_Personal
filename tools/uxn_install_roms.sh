#!/bin/sh
# Install the bundled demo, open-source test games, and optional personal Uxn
# ROMs on an iPod/simdisk. Steam metadata is installed when .rockbox exists.

set -eu

usage() {
    echo "Usage: $0 MOUNT_OR_SIMDISK [ROM.rom ...]" >&2
    exit 2
}

[ "$#" -ge 1 ] || usage
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
destination=$1/Uxn
rockbox=$1/.rockbox
shift

[ -d "${destination%/Uxn}" ] || {
    echo "Not a directory: ${destination%/Uxn}" >&2
    exit 1
}
mkdir -p "$destination"
base64 -d "$repo_root/apps/plugins/uxn/roms/rockbox-demo.rom.b64" \
    > "$destination/rockbox-demo.rom"

for rom in "$repo_root"/assets/uxn_games/roms/*.rom; do
    cp "$rom" "$destination/$(basename "$rom")"
done

if [ -d "$rockbox" ]; then
    metadata="$rockbox/rocks/viewers/uxn"
    covers="$rockbox/games/library/covers/uxn"
    mkdir -p "$metadata" "$covers"
    cp "$repo_root/assets/uxn_games/games.tsv" "$metadata/games.tsv"
    cp "$repo_root/assets/uxn_games/SOURCES.tsv" "$metadata/SOURCES.tsv"
    cp "$repo_root/assets/uxn_games/THIRDPARTY-NOTICES.txt" \
        "$metadata/THIRDPARTY-NOTICES.txt"
    for cover in "$repo_root"/assets/game_covers/uxn/*.bmp; do
        cp "$cover" "$covers/$(basename "$cover")"
    done
else
    echo "warning: no .rockbox directory; Steam metadata was not installed" >&2
fi

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
echo "Installed Uxn demo and test games in $destination"
