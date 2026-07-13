#!/usr/bin/env bash

set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
source_png="$repo_root/assets/ipodjs/source/ipodjs-extras-rendered-transparent.png"
font="$repo_root/assets/ipodjs/source/texgyreheros-bold.otf"
out_dir="$repo_root/assets/ipodjs/rockbox/previews"

names=(clock applications pokemini files playlists plugins shortcuts system)
labels=(Clock Applications PokeMini Files Playlists Plugins Shortcuts System)
crops=(
    "414x474+0+0"
    "414x474+415+0"
    "414x474+830+0"
    "414x474+1245+0"
    "414x474+0+474"
    "414x474+415+474"
    "414x474+830+474"
    "414x474+1245+474"
)
sizes=(132 138 132 138 135 130 130 130)

render_pane()
{
    local index=$1
    local height=$2
    local dark=$3
    local top bottom shadow text suffix object_offset label_offset output

    if (( dark )); then
        top="#596371"
        bottom="#151b24"
        shadow="#05080c"
        text="#f3f5f7"
        suffix="-dark"
    else
        top="#cbd6e3"
        bottom="#71869f"
        shadow="#26384d"
        text="#ffffff"
        suffix=""
    fi

    if (( height == 240 )); then
        object_offset=-22
        label_offset=18
    else
        object_offset=-16
        label_offset=14
    fi

    output="$out_dir/${names[index]}.174x${height}x24${suffix}.bmp"
    if [[ ${names[index]} == clock ]]; then
        magick -size "174x${height}" "gradient:${top}-${bottom}" \
            \( "$source_png" -crop "${crops[index]}" +repage -trim \
               +repage -resize "${sizes[index]}x${sizes[index]}>" \) \
            -gravity center -geometry "+0${object_offset}" -composite \
            -colorspace sRGB -depth 8 -type TrueColor "BMP3:$output"
    else
        magick -size "174x${height}" "gradient:${top}-${bottom}" \
            \( "$source_png" -crop "${crops[index]}" +repage -trim \
               +repage -resize "${sizes[index]}x${sizes[index]}>" \) \
            -gravity center -geometry "+0${object_offset}" -composite \
            -font "$font" -pointsize 16 -gravity south \
            -fill "$shadow" -annotate "+0+$((label_offset - 1))" \
                "${labels[index]}" \
            -fill "$text" -annotate "+0+${label_offset}" \
                "${labels[index]}" \
            -colorspace sRGB -depth 8 -type TrueColor "BMP3:$output"
    fi
}

for i in "${!names[@]}"; do
    render_pane "$i" 240 0
    render_pane "$i" 240 1
    render_pane "$i" 220 0
    render_pane "$i" 220 1
done

printf 'generated iPodJS Extras previews in %s\n' "$out_dir"
