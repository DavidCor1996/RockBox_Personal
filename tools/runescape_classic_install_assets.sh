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

plugin_src="$repo_root/build-sim-ipod6g/apps/plugins/runescape_classic/runescape_classic.rock"
plugin_dst="$target_root/.rockbox/rocks/games/runescape_classic.rock"
if [ -f "$plugin_src" ]; then
    cp -p "$plugin_src" "$plugin_dst"
else
    echo "Runescape Classic plugin binary not found: $plugin_src" >&2
    echo "Build the plugin first with: cd build-sim-ipod6g && make -j4" >&2
    echo "Missing plugin may keep it out of the Games menu." >&2
    exit 1
fi

refresh_sim_rockboy_launcher() {
    sim_build_dir="$repo_root/build-sim-ipod6g"
    sim_disk="$sim_build_dir/simdisk"
    sim_target=0
    if [ "${sim_disk%/}" = "${target_root%/}" ]; then
        sim_target=1
    else
        if [ -d "$sim_disk" ] && [ -d "$target_root/.rockbox" ]; then
            sim_root_abs=$(CDPATH= cd -- "$sim_disk" && pwd -P)
            target_root_abs=$(CDPATH= cd -- "$target_root" && pwd -P)
            if [ "$target_root_abs" = "$sim_root_abs" ]; then
                sim_target=1
            fi
        fi
    fi

    [ "$sim_target" -ne 1 ] && return

    # On simulator builds, keep the launcher binary in sync with source changes
    # so game list entries (including RuneScape Classic) stay current.
    sim_launcher_src="$sim_build_dir/apps/plugins/rockboy_launcher.rock"
    sim_launcher_dst="$target_root/.rockbox/rocks/games/rockboy_launcher.rock"
    if [ ! -f "$sim_launcher_src" ] && [ -d "$sim_build_dir" ]; then
        (
            cd "$sim_build_dir" || exit 1
            make apps/plugins/rockboy_launcher.rock
        )
    fi

    if [ -f "$sim_launcher_src" ]; then
        cp -p "$sim_launcher_src" "$sim_launcher_dst"
        echo "Updated 6G simulator rockboy launcher for target: $target_root"
    else
        echo "Skipping 6G simulator rockboy launcher refresh; build artifact missing: $sim_launcher_src" >&2
    fi
}

invalidate_games_plugin_cache() {
    for candidate in "$target_root" "$target_root/simdisk"; do
        if [ -d "$candidate/.rockbox/rocks" ]; then
            rm -f "$candidate/.rockbox/rocks/plugin.dat" "$candidate/.rockbox/rocks/rb_plugins.dat"
        fi
    done
}

refresh_sim_rockboy_launcher

required_assets="
lumbridge.rsc
character_male.bmp
character_male_0.bmp
character_male_1.bmp
character_male_2.bmp
character_male_3.bmp
ui_top.bmp
ui_bottom.bmp
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
ui_top.bmp
ui_bottom.bmp
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

invalidate_games_plugin_cache

for asset in $required_assets; do
    sha256sum "$asset_dir/$asset" "$game_dir/$asset" "$ipodjs_dir/$asset"
done

echo "Installed RuneScape Classic assets to $target_root"
