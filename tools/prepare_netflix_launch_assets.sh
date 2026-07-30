#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: $0 /path/to/netflix-ident-video" >&2
    exit 2
fi

source_video=$1
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output_dir="$repo_root/assets/ipodjs/rockbox/netflix/launch"
temporary_dir=$(mktemp -d)
trap 'rm -rf "$temporary_dir"' EXIT

mkdir -p "$output_dir"

# Keep the complete official 2019 dark-background ident and Tudum sound.
ffmpeg -y -hide_banner -loglevel error -ss 0 -t 4.00 \
    -i "$source_video" -vn -ar 44100 -ac 2 -f s16le \
    "$output_dir/intro-44100-stereo.pcm"

# Preserve every real 10 fps source step. Full-size frames feed the video
# plugins; the app ident uses a compact full-color RGB565 delta stream and
# scales it to the same 320x180 viewport while drawing.
rm -f "$output_dir"/frame-*.bmp
ffmpeg -y -hide_banner -loglevel error -i "$source_video" -t 4.00 \
    -vf "fps=10,scale=320:180:flags=lanczos" -pix_fmt bgr24 \
    -start_number 0 "$output_dir/frame-%02d.320x180x24.bmp"
ffmpeg -y -hide_banner -loglevel error -i "$source_video" -t 4.00 \
    -vf "fps=10,scale=106:60:flags=lanczos" -pix_fmt bgr24 \
    -start_number 0 "$temporary_dir/frame-%02d.bmp"

"$repo_root/tools/build_netflix_launch_pack.py" \
    "$temporary_dir" "$output_dir/intro-106x60.nfr" \
    --frame-ms 100 --expected-frames 40 --width 106 --height 60 --rgb565
rm -f "$output_dir/intro-104x58.nfr" \
    "$output_dir/intro-160x90.nfx" "$output_dir/intro-320x180.nfx"
ffmpeg -y -hide_banner -loglevel error \
    -f s16le -ar 44100 -ac 2 -i "$output_dir/intro-44100-stereo.pcm" \
    -ac 1 -ar 20000 -c:a pcm_mulaw -f mulaw \
    "$output_dir/intro-20000-mono.mulaw"

(
    cd "$output_dir"
    sha256sum frame-*.bmp intro-*.pcm intro-*.mulaw intro-*.nfr > SHA256SUMS
)
