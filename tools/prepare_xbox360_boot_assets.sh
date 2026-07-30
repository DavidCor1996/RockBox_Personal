#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
source_video="$repo_root/assets/ipodjs/sources/xbox360/boot/xbox360-boot-2005.source.mp4"
output_dir="$repo_root/assets/ipodjs/rockbox/achievements/boot"

if [[ ! -f "$source_video" ]]; then
    echo "missing Xbox 360 boot source: $source_video" >&2
    exit 1
fi

mkdir -p "$output_dir"

# Keep every genuine 10 fps source frame in a delta-compressed indexed stream.
# This fits beside the sound in the plugin's existing fixed 1 MiB buffer.
temporary_dir=$(mktemp -d)
trap 'rm -rf "$temporary_dir"' EXIT
rm -f "$output_dir"/frame-*.bmp
ffmpeg -hide_banner -loglevel error -y -ss 0.70 -t 5.80 \
    -i "$source_video" -vf "fps=10,scale=320:180:flags=lanczos" \
    -pix_fmt bgr24 -start_number 0 "$temporary_dir/frame-%02d.bmp"
"$repo_root/tools/build_netflix_launch_pack.py" \
    "$temporary_dir" "$output_dir/boot-320x180.nfx" \
    --frame-ms 100 --expected-frames 58

# Preserve the real mnemonic while fitting beside the cached frames. G.711
# mu-law gives the launch sound useful dynamic range at one byte per sample;
# the plugin expands and resamples it into the current mixer rate.
ffmpeg -hide_banner -loglevel error -y \
    -ss 0.70 -t 5.85 -i "$source_video" -vn -ac 1 -ar 20000 \
    -c:a pcm_mulaw -f mulaw "$output_dir/boot-20000-mono.mulaw"

(
    cd "$output_dir"
    sha256sum boot-320x180.nfx boot-20000-mono.mulaw > SHA256SUMS
)

echo "Prepared Xbox 360 boot assets in $output_dir"
