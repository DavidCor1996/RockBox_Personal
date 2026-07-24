#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
astroterm_commit="5c571959dbd7ceca95b964e9a92154192bc76a09"

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <canonical-bsc5-binary> <destination-directory>" >&2
    echo "example destination: build-sim-ipod6g/simdisk/.rockbox/apps/pocketsky" >&2
    exit 2
fi

bsc5="$1"
destination="$2"

if [ ! -f "$bsc5" ]; then
    echo "BSC5 file not found: $bsc5" >&2
    exit 1
fi

for command in curl tar python3; do
    if ! command -v "$command" >/dev/null 2>&1; then
        echo "required command not found: $command" >&2
        exit 1
    fi
done

work="$(mktemp -d /tmp/pocketsky-resources-XXXXXX)"
cleanup() {
    rm -rf "$work"
}
trap cleanup EXIT INT TERM HUP

archive="$work/astroterm.tar.gz"
source_dir="$work/astroterm-$astroterm_commit"
generated="$work/generated"

curl -fsSL \
    "https://github.com/da-luce/astroterm/archive/$astroterm_commit.tar.gz" \
    -o "$archive"
tar -xzf "$archive" -C "$work"

python3 "$repo_root/tools/pocketsky_import_data.py" \
    --bsc5 "$bsc5" \
    --astroterm-data "$source_dir/data" \
    --astronomy-c \
        "$repo_root/apps/plugins/pocketsky/upstream/astronomy_engine/astronomy.c" \
    --output "$generated"
python3 "$repo_root/tools/pocketsky_reference_gate.py" "$generated"

mkdir -p "$destination"
cp "$generated/catalog.psc" \
   "$generated/names.psc" \
   "$generated/constellations.psc" \
   "$generated/cities.psc" \
   "$generated/PROVENANCE.txt" \
   "$destination/"

python3 "$repo_root/tools/pocketsky_reference_gate.py" "$destination"
echo "Pocket Sky resources installed in: $destination"
