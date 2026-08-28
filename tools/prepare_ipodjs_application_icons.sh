#!/bin/sh
# Build the fixed-size, cache-ready icon set used by the iOS 3 Applications
# view.  The brand marks are composited from their real source artwork; the
# custom utilities use the checked-in ImageGen source renders.

set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source_root="$repo_root/assets/ipodjs/sources"
application_sources="$source_root/applications"
output_root="$repo_root/assets/ipodjs/rockbox/applications"
temporary_root=$(mktemp -d)

trap 'rm -rf "$temporary_root"' EXIT HUP INT TERM

if ! command -v magick >/dev/null 2>&1; then
    echo "ImageMagick 7 (magick) is required" >&2
    exit 1
fi

mkdir -p "$output_root"

finish_icon()
{
    source=$1
    output=$2

    magick "$source" -auto-orient -resize '46x46^' -gravity center \
        -extent 46x46 \
        \( -size 46x46 xc:none -fill white \
           -draw 'roundrectangle 0,0 45,45 8,8' \) \
        -alpha off -compose CopyOpacity -composite \
        -background magenta -alpha remove -alpha off \
        -colorspace sRGB -depth 8 -type TrueColor \
        "BMP3:$output_root/$output.46x46x24.bmp"
}

make_glass_icon()
{
    logo=$1
    top=$2
    bottom=$3
    geometry=$4
    output=$5
    base="$temporary_root/$output-base.png"

    magick -size 512x512 "gradient:$top-$bottom" \
        \( "$logo" -resize "$geometry" \) \
        -gravity center -compose over -composite \
        \( -size 512x230 \
           "gradient:rgba(255,255,255,0.42)-rgba(255,255,255,0)" \) \
        -gravity north -compose over -composite "$base"
    finish_icon "$base" "$output"
}

authentic="$application_sources/authentic"
generated="$application_sources/generated"

finish_icon "$authentic/clock-ios6.jpg" clock
finish_icon "$authentic/youtube-ios6.png" youtube
# Authentic iOS application artwork extracted from Spotify 1.8.1's signed
# iOS 6 package. Unlike the 2009 horizontal press wordmark, this icon remains
# crisp and recognisable at the iPodJS grid's 46-pixel size.
finish_icon "$authentic/spotify-ios6-itunes-artwork.jpg" spotify-wrapped
finish_icon "$authentic/instagram-ios6.png" instagram
finish_icon "$authentic/reddit-alien-blue-ios6.png" reddit
finish_icon "$authentic/directv-ios6.png" directv
finish_icon "$authentic/maps-ios6.jpg" maps
finish_icon "$authentic/weather-ios6.jpg" weather
finish_icon "$authentic/safari-ios6.jpg" internet

finish_icon "$generated/desktop-mode-ios3.png" desktop-mode
finish_icon "$generated/rololauncher-ios3.png" rolo-launcher
finish_icon "$authentic/xbox-live-ios2011.jpg" achievements
finish_icon "$generated/tamagotchi-ios3.png" tamagotchi
finish_icon "$generated/pocket-sky-ios3.png" pocket-sky
finish_icon "$generated/qr-codes-ios3.png" qr-codes
finish_icon "$generated/pokedex-ios3.png" pokedex
finish_icon "$authentic/netflix-ios2010.jpg" netflix

# Preserve the official TikTok note mark, but discard the horizontal wordmark
# because it is illegible at the iPod's 46-pixel icon size.
tiktok_mark="$temporary_root/tiktok-mark.png"
magick "$source_root/tiktok/official/"\
"TikTok-logo-RGB-Horizontal-white-simplified.png" \
    -crop 1900x1900+1250+1120 +repage -trim +repage "$tiktok_mark"
make_glass_icon "$tiktok_mark" '#252333' '#050507' '300x300' tiktok

onlyfans_mark="$temporary_root/onlyfans-mark.png"
magick "$repo_root/assets/ipodjs/rockbox/onlyfans/"\
"onlyfans-logo-official.40x40.bmp" \
    -fuzz 12% -transparent white -trim +repage "$onlyfans_mark"
make_glass_icon "$onlyfans_mark" '#69dcff' '#0879ba' '330x260' onlyfans

make_glass_icon "$source_root/sitekick/YTV_Logo_2003.png" \
    '#7e4fb7' '#241241' '330x300' sitekick
finish_icon "$source_root/calm/calm-app-store-icon-512.jpg" calm

echo "Prepared Applications icons in $output_root"
