#!/bin/sh
# Convert Twitch's current official SVG marks into fixed Rockbox RGB24 BMPs.

set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source_root="$repo_root/assets/ipodjs/sources/twitch/current"
output_root="$repo_root/assets/ipodjs/rockbox/twitch"

if ! command -v magick >/dev/null 2>&1; then
    echo "ImageMagick 7 (magick) is required" >&2
    exit 1
fi

mkdir -p "$output_root"

# Render the current white wordmark on the exact official Twitch-purple header
# colour. Baking the target-size antialiasing against its real backdrop avoids
# magenta-key fringes on the RGB565 display.
magick -background none "$source_root/twitch_wordmark_flat_white.svg" \
    -alpha on -resize '128x40>' -gravity center -extent 136x50 \
    -background '#9146ff' -alpha remove -alpha off \
    -colorspace sRGB -depth 8 -type TrueColor \
    "BMP3:$output_root/twitch-wordmark-current-white.136x50.bmp"

# The compact current Glitch is shared by both native video renderers.
magick -background none "$source_root/glitch_flat_purple.svg" \
    -alpha on -resize '16x19>' -gravity center -extent 18x20 \
    -background magenta -alpha remove -alpha off \
    -colorspace sRGB -depth 8 -type TrueColor \
    "BMP3:$output_root/twitch-glitch-current.18x20.bmp"

echo "Prepared current Twitch brand assets in $output_root"
