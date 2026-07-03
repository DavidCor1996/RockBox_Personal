#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(dirname "$script_dir")
asset_dir="$repo_root/assets/ipodjs/rockbox/runescape_classic"

usage() {
    echo "usage: $0 IPOD_ROOT" >&2
    echo "  IPOD_ROOT is the mounted root of the iPod volume." >&2
    echo "  Example: $0 /run/media/david/IPOD" >&2
    exit 2
}

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
    usage
fi

if [ $# -ne 1 ]; then
    usage
fi

target_root=$1

if [ ! -d "$target_root" ]; then
    echo "Target root directory not found: $target_root" >&2
    exit 1
fi

required_assets="
config85.jag
entity24.jag
entity24.mem
filter2.jag
jagex.jag
land63.jag
land63.mem
maps63.jag
maps63.mem
media59.jag
models36.jag
sounds1.mem
textures17.jag
"

for asset in $required_assets; do
    if [ ! -f "$asset_dir/$asset" ]; then
        echo "RuneScape Classic asset pack is missing $asset: $asset_dir" >&2
        exit 1
    fi
done

game_dir="$target_root/.rockbox/rocks/games/runescape_classic"
ipodjs_dir="$target_root/.rockbox/ipodjs/runescape_classic"

mkdir -p "$game_dir" "$ipodjs_dir"
stale_assets="
lumbridge.rsc
character_male.bmp
character_male_0.bmp
character_male_1.bmp
character_male_2.bmp
character_male_3.bmp
ui_top.bmp
ui_bottom.bmp
scene.bmp
"

for asset in $stale_assets; do
    rm -f "$game_dir/$asset" "$ipodjs_dir/$asset"
done
rm -rf "$game_dir/covers" "$ipodjs_dir/covers"
mkdir -p "$game_dir/covers" "$ipodjs_dir/covers"

for asset in $required_assets; do
    cp -p "$asset_dir/$asset" "$game_dir/"
done

for asset in $required_assets; do
    cp -p "$asset_dir/$asset" "$ipodjs_dir/"
done

if [ -d "$asset_dir/covers" ]; then
    for cover in "$asset_dir"/covers/*.bmp; do
        [ -f "$cover" ] || continue
        cp -p "$cover" "$game_dir/covers/"
        cp -p "$cover" "$ipodjs_dir/covers/"
    done
fi

for asset in $required_assets; do
    sha256sum "$asset_dir/$asset" "$game_dir/$asset" "$ipodjs_dir/$asset"
done

echo "Installed RuneScape Classic assets to $target_root"
