#!/bin/sh
set -eu

src="${POKEMINI_SOURCE_DIR:-${HOME:-.}/Documents/PokeMini}"

usage() {
    echo "usage: $0 IPOD_ROOT [SOURCE_DIR]" >&2
    echo "  IPOD_ROOT is the mounted root of the iPod volume." >&2
    echo "  Example:" >&2
    echo "    $0 /run/media/david/IPOD" >&2
    exit 2
}

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
    usage
fi

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    usage
fi

target_root=$1
if [ $# -eq 2 ]; then
    src=$2
fi

if [ ! -d "$src" ]; then
    echo "PokeMini source directory not found: $src" >&2
    exit 1
fi

if [ ! -d "$target_root" ]; then
    echo "Target root directory not found: $target_root" >&2
    exit 1
fi

dest="$target_root/PokeMini"
mkdir -p "$dest"

count=0
for game in "$src"/*.zip "$src"/*.min; do
    if [ ! -f "$game" ]; then
        continue
    fi

    case "$game" in
        *.zip)
            unzip -jo "$game" '*.min' -d "$dest" >/dev/null
            ;;
        *.min)
            cp -p "$game" "$dest/"
            ;;
    esac
    count=$((count + 1))
done

echo "Installed PokeMini games from $count source file(s) to $dest"
