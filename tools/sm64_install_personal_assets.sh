#!/usr/bin/env bash
set -euo pipefail

if (( $# < 1 || $# > 2 )); then
    echo "usage: $0 DEVICE_OR_SIM_ROOT [SM64_US_V1_ROM]" >&2
    exit 2
fi

root=$1
rom=${2:-"/home/david/Downloads/Super Mario 64 (USA).z64"}
repo=$(cd "$(dirname "$0")/.." && pwd)
expected=9bef1128717f958171a4afac3ed78ee2bb4e86ce

if ! test -f "$rom"; then
    echo "missing ROM: $rom" >&2
    exit 1
fi
actual=$(sha1sum "$rom" | awk '{print $1}')
if [[ $actual != "$expected" ]]; then
    echo "wrong SM64 ROM revision: expected US v1.0 SHA1 $expected" >&2
    exit 1
fi

rockbox="$root/.rockbox"
rom_dir="$rockbox/games/n64"
cover_dir="$rockbox/games/library/covers/n64"
system_dir="$rockbox/games/library/covers/systems"
mkdir -p "$rom_dir/saves" "$cover_dir" "$system_dir"

cp "$rom" "$rom_dir/Super Mario 64 (USA).z64"
cp "$repo/assets/game_covers/n64/Super Mario 64 (USA).bmp" \
   "$cover_dir/Super Mario 64 (USA).bmp"
cp "$repo/assets/game_covers/n64/n64-system.bmp" "$system_dir/n64.bmp"

test "$(sha1sum "$rom_dir/Super Mario 64 (USA).z64" | awk '{print $1}')" = "$expected"
echo "PASS: installed verified personal SM64 ROM and N64 cover art under $rockbox"
