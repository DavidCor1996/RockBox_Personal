#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: $0 /path/to/netflix-ident-2013-video" >&2
    exit 2
fi

source_video=$1
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output_dir="$repo_root/assets/ipodjs/rockbox/netflix/video-launch"
temporary_dir=$(mktemp -d)
trap 'rm -rf "$temporary_dir"' EXIT

mkdir -p "$output_dir"
rm -f "$output_dir"/frame-*.bmp

# Official 2013 ident: keep its short lead-in, complete sound logo, and hold.
ffmpeg -y -hide_banner -loglevel error -ss 1.10 -t 3.30 \
    -i "$source_video" -vn -ar 44100 -ac 2 -f s16le \
    "$output_dir/intro-44100-stereo.pcm"

# Cache the genuine moving portion at 10 fps before video playback takes over.
for frame in $(seq 0 11); do
    timestamp=$(awk -v n="$frame" 'BEGIN { printf "%.6f", 1.10 + n / 10.0 }')
    printf -v name 'frame-%02d.320x180x24.bmp' "$frame"
    ffmpeg -y -hide_banner -loglevel error -ss "$timestamp" \
        -i "$source_video" -frames:v 1 \
        -vf "scale=320:180:flags=lanczos" -pix_fmt bgr24 \
        "$temporary_dir/$name"
    mv "$temporary_dir/$name" "$output_dir/$name"
done

(
    cd "$output_dir"
    sha256sum frame-*.bmp intro-*.pcm > SHA256SUMS
)
