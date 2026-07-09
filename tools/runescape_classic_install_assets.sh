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

plugin_src=""
if [ "$sim_target" -eq 1 ]; then
    for candidate in \
        "$repo_root/build-sim-ipod6g/apps/plugins/runescape_classic/runescape_classic.rock" \
        "$repo_root/build-hw-ipod6g/apps/plugins/runescape_classic/runescape_classic.rock"
    do
        if [ -f "$candidate" ]; then
            plugin_src="$candidate"
            break
        fi
    done
else
    for candidate in \
        "$repo_root/build-hw-ipod6g/apps/plugins/runescape_classic/runescape_classic.rock" \
        "$repo_root/build-sim-ipod6g/apps/plugins/runescape_classic/runescape_classic.rock"
    do
        if [ -f "$candidate" ]; then
            plugin_src="$candidate"
            break
        fi
    done
fi

plugin_dst="$target_root/.rockbox/rocks/games/runescape_classic.rock"
if [ -n "$plugin_src" ] && [ -f "$plugin_src" ]; then
    :
else
    echo "Runescape Classic plugin binary not found in expected build outputs." >&2
    echo "Build apps/plugins/runescape_classic/runescape_classic.rock in build-hw-ipod6g or build-sim-ipod6g." >&2
    echo "Missing plugin may keep it out of the Games menu." >&2
    exit 1
fi

refresh_sim_rockboy_launcher() {
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
npc_spawns.tsv
item_spawns.tsv
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
backup_dir="$target_root/.rockbox/backups/runescape_classic-$(date +%Y%m%d-%H%M%S)"
stale_plugin_apps="$target_root/.rockbox/rocks/apps/runescape_classic.rock"
stale_plugin_data="$game_dir/runescape_classic.rock"

backup_asset_dir() {
    src_dir=$1
    dst_dir=$2

    [ -d "$src_dir" ] || return
    mkdir -p "$dst_dir"
    for asset in $required_assets; do
        [ -f "$src_dir/$asset" ] || continue
        cp -p "$src_dir/$asset" "$dst_dir/"
    done

    if [ -d "$src_dir/covers" ]; then
        mkdir -p "$dst_dir/covers"
        for cover in "$src_dir"/covers/*.bmp; do
            [ -f "$cover" ] || continue
            cp -p "$cover" "$dst_dir/covers/"
        done
    fi
}

mkdir -p "$backup_dir"
if [ -f "$plugin_dst" ]; then
    cp -p "$plugin_dst" "$backup_dir/runescape_classic.rock"
fi
if [ -f "$stale_plugin_apps" ]; then
    cp -p "$stale_plugin_apps" "$backup_dir/rocks-apps-runescape_classic.rock"
fi
if [ -f "$stale_plugin_data" ]; then
    cp -p "$stale_plugin_data" "$backup_dir/rocks-games-data-runescape_classic.rock"
fi
backup_asset_dir "$game_dir" "$backup_dir/rocks-games-runescape_classic"
backup_asset_dir "$ipodjs_dir" "$backup_dir/ipodjs-runescape_classic"
echo "Backed up existing RuneScape Classic files to $backup_dir"

mkdir -p "$game_dir" "$ipodjs_dir"
cp -p "$plugin_src" "$plugin_dst"
rm -f "$stale_plugin_apps" "$stale_plugin_data"
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

sha256sum "$plugin_src" "$plugin_dst"
for asset in $required_assets; do
    sha256sum "$asset_dir/$asset" "$game_dir/$asset" "$ipodjs_dir/$asset"
done

sync

echo "Installed RuneScape Classic to $target_root"
echo "Plugin source: $plugin_src"
echo "Plugin destination: $plugin_dst"
echo "Data destinations: $game_dir and $ipodjs_dir"
