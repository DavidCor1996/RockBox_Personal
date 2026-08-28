#!/usr/bin/env bash
set -euo pipefail

# Cuts the genuine DIRECTV receiver start-up sequence out of the source
# recording and packs it for the Live TV launcher. The clip is silent, so
# unlike the Xbox 360 and Netflix idents this produces frames only.
#
# The receiver's real start-up runs for the better part of a minute, most of
# it holding one still screen, so the ident is a montage: a couple of seconds
# of each screen the user actually sees, cut together in order. The player
# holds the final frame for as long as the rest of the launch takes, so the
# cut only has to cover the screens themselves, not the whole wait.
#
# Segments are "start:duration" in seconds against the source recording:
#   4.9:2.0   "Hello.  Your DIRECTV receiver is starting up."
#   8.2:2.0   "Almost there.  A few more seconds please..."
#   30.0:4.0  branded screen, "Searching for satellite signal..."
# The diagnostic-test screen between them is left out; it is not part of a
# normal power-up.
#
# usage: tools/prepare_directv_boot_assets.sh [fps] [start:duration ...]

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
source_video="$repo_root/assets/ipodjs/sources/directv/boot/directv-boot.source.mp4"
output_dir="$repo_root/assets/ipodjs/rockbox/livetv/boot"

fps=${1:-12}
shift || true
segments=("$@")
if [[ ${#segments[@]} -eq 0 ]]; then
    segments=(4.9:2.0 8.2:2.0 30.0:4.0)
fi

if [[ ! -f "$source_video" ]]; then
    echo "missing DIRECTV boot source: $source_video" >&2
    exit 1
fi

mkdir -p "$output_dir"

temporary_dir=$(mktemp -d)
trap 'rm -rf "$temporary_dir"' EXIT

# The receiver filled a 4:3 set, and Live TV centre-cuts its own channels to
# 320x240, so the boot screens are scaled the same way rather than letterboxed.
filter="scale=iw*sar:ih,scale=320:240:force_original_aspect_ratio=increase,\
crop=320:240,setsar=1,fps=$fps"

next=0
for segment in "${segments[@]}"; do
    start=${segment%%:*}
    duration=${segment##*:}
    part="$temporary_dir/part"
    rm -rf "$part"
    mkdir -p "$part"
    ffmpeg -hide_banner -loglevel error -y -ss "$start" -t "$duration" \
        -i "$source_video" -vf "$filter" \
        -pix_fmt bgr24 -start_number 0 "$part/frame-%03d.bmp"
    for frame in "$part"/frame-*.bmp; do
        [[ -f "$frame" ]] || continue
        printf -v name 'frame-%04d.bmp' "$next"
        mv "$frame" "$temporary_dir/$name"
        next=$((next + 1))
    done
done
rm -rf "$temporary_dir/part"

if [[ "$next" -lt 2 ]]; then
    echo "no frames extracted from $source_video" >&2
    exit 1
fi

frame_ms=$(awk -v fps="$fps" 'BEGIN { printf "%d", 1000 / fps }')

# Full RGB565 deltas, not the 16-colour palette the Xbox 360 boot uses: the
# branded screen is a blue gradient, which bands badly once quantised.
"$repo_root/tools/build_netflix_launch_pack.py" \
    "$temporary_dir" "$output_dir/boot-320x240.nfr" \
    --width 320 --height 240 --frame-ms "$frame_ms" --rgb565

(
    cd "$output_dir"
    sha256sum boot-320x240.nfr > SHA256SUMS
)

echo "Prepared DIRECTV boot assets in $output_dir ($next frames)"
