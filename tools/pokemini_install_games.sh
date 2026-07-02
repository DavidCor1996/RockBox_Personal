#!/bin/sh
set -eu

src="${POKEMINI_SOURCE_DIR:-${HOME:-.}/Documents/PokeMini}"
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(dirname "$script_dir")
asset_pokemini="$repo_root/assets/ipodjs/rockbox/pokemini"

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

if [ -d "$asset_pokemini/covers" ]; then
    launcher_covers="$target_root/.rockbox/rocks/games/pokemini_launcher/covers"
    ipodjs_pokemini="$target_root/.rockbox/ipodjs/pokemini"
    mkdir -p "$launcher_covers" "$ipodjs_pokemini"

    art_count=0
    for cover in "$asset_pokemini"/covers/*.bmp; do
        if [ ! -f "$cover" ]; then
            continue
        fi

        cp -p "$cover" "$launcher_covers/"
        art_count=$((art_count + 1))
    done

    cp -a "$asset_pokemini/." "$ipodjs_pokemini/"
    echo "Installed PokeMini artwork from $art_count BMP file(s)"
fi
