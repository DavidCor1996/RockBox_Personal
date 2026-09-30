#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: $0 /path/to/netflix-ident-video" >&2
    exit 2
fi

source_video=$1
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output_dir="$repo_root/assets/ipodjs/rockbox/netflix/launch"

mkdir -p "$output_dir"

# Keep the complete official 2019 dark-background ident and Tudum sound.
ffmpeg -y -hide_banner -loglevel error -ss 0 -t 4.00 \
    -i "$source_video" -vn -ar 44100 -ac 2 -f s16le \
    "$output_dir/intro-44100-stereo.pcm"

# Downsample once with Lanczos to the native LCD width. The app streams
# independent RGB565 frames into its existing transition workspace and centers
# the 16:9 image, avoiding low-resolution enlargement and aspect-ratio crops.
rm -f "$output_dir"/frame-*.bmp
ffmpeg -y -hide_banner -loglevel error -i "$source_video" -t 4.00 \
    -vf "fps=10,scale=320:180:flags=lanczos" -pix_fmt bgr24 \
    -start_number 0 "$output_dir/frame-%02d.320x180x24.bmp"
ffmpeg -y -hide_banner -loglevel error -i "$source_video" -t 4.00 \
    -vf "fps=10,scale=320:180:flags=lanczos" -pix_fmt rgb565le \
    -f rawvideo "$output_dir/intro-320x180.rgb565"
ffmpeg -y -hide_banner -loglevel error \
    -f s16le -ar 44100 -ac 2 -i "$output_dir/intro-44100-stereo.pcm" \
    -ac 1 -ar 20000 -c:a pcm_mulaw -f mulaw \
    "$output_dir/intro-20000-mono.mulaw"

(
    cd "$output_dir"
    sha256sum frame-*.bmp intro-*.pcm intro-*.mulaw intro-*.nfr intro-*.rgb565 > SHA256SUMS
)
