#!/usr/bin/env bash
set -euo pipefail

# External Cydia/WinterBoard themes were used only as visual reference for this
# look. The generated BMPs below are original local raster chrome.

theme_dir="wps/SpringPod3"
backdrop_dir="backdrops"
im="${IMAGEMAGICK:-magick}"

mkdir -p "$theme_dir" "$backdrop_dir"

make_menu_backdrop() {
    local out="$1"
    "$im" -size 320x240 gradient:'#000000-#050505' \
        -fill '#030303' -draw 'rectangle 0,20 320,172' \
        -fill '#0c0c0e' -draw 'rectangle 0,84 320,86' \
        -fill '#0c0c0e' -draw 'rectangle 0,151 320,153' \
        -fill '#000000' -draw 'rectangle 0,0 320,19' \
        -fill '#ffffff22' -draw 'rectangle 0,20 320,20' \
        -fill '#000000d0' -draw 'roundRectangle 0,174 320,240 22,22' \
        -fill '#2b2d31' -draw 'roundRectangle 12,176 308,238 18,18' \
        -fill '#0d0f13' -draw 'roundRectangle 14,182 306,240 17,17' \
        -fill '#f2f4f7' -draw 'roundRectangle 18,176 302,184 10,10' \
        -fill '#8f969f' -draw 'rectangle 20,184 300,186' \
        -fill '#ffffff45' -draw 'rectangle 22,187 298,188' \
        -fill '#ffffff' -draw 'circle 152,162 154,162' \
        -fill '#6d7480' -draw 'circle 160,162 162,162' \
        -fill '#6d7480' -draw 'circle 168,162 170,162' \
        \
        -fill '#1f2a38' -draw 'roundRectangle 24,34 68,78 10,10' \
        -fill '#586575' -draw 'rectangle 27,37 65,50' \
        -fill '#ffffff66' -draw 'roundRectangle 26,36 66,51 8,8' \
        -fill '#ffffff' -draw 'polygon 34,48 48,39 62,48 48,58' \
        -fill '#d6e7ff' -draw 'polygon 34,59 48,50 62,59 48,70' \
        \
        -fill '#e97c18' -draw 'roundRectangle 100,34 144,78 10,10' \
        -fill '#ffce50' -draw 'rectangle 103,37 141,50' \
        -fill '#ffffff66' -draw 'roundRectangle 102,36 142,51 8,8' \
        -fill '#ffffff' -draw 'rectangle 124,43 129,63 circle 117,66 124,66 circle 135,62 142,62 polygon 128,43 140,47 140,53 128,50' \
        \
        -fill '#d79b28' -draw 'roundRectangle 176,34 220,78 10,10' \
        -fill '#f5cc63' -draw 'rectangle 179,47 217,73' \
        -fill '#f8d56d' -draw 'polygon 180,44 196,44 200,49 217,49 217,55 180,55' \
        -fill '#ffffff66' -draw 'roundRectangle 178,36 218,51 8,8' \
        \
        -fill '#202328' -draw 'roundRectangle 252,34 296,78 10,10' \
        -fill '#1d7ad1' -draw 'rectangle 255,50 293,75' \
        -fill '#ffffff66' -draw 'roundRectangle 254,36 294,51 8,8' \
        -fill '#f2f4f7' -draw 'polygon 257,39 266,39 261,48 252,48 polygon 271,39 280,39 275,48 266,48 polygon 285,39 294,39 289,48 280,48' \
        \
        -fill '#f4f4f4' -draw 'roundRectangle 24,101 68,145 10,10' \
        -fill '#8bc543' -draw 'circle 46,123 59,123' \
        -fill '#f2d13b' -draw 'circle 46,123 46,110' \
        -fill '#f58634' -draw 'circle 46,123 33,123' \
        -fill '#4aa4df' -draw 'circle 46,123 46,136' \
        -fill '#ffffff66' -draw 'roundRectangle 26,103 66,118 8,8' \
        \
        -fill '#4f7ac0' -draw 'roundRectangle 100,101 144,145 10,10' \
        -fill '#dce8ff' -draw 'rectangle 108,110 136,136' \
        -fill '#4f7ac0' -draw 'line 112,116 132,116 line 112,123 132,123 line 112,130 128,130' \
        -fill '#ffffff66' -draw 'roundRectangle 102,103 142,118 8,8' \
        \
        -fill '#835431' -draw 'roundRectangle 176,101 220,145 10,10' \
        -fill '#b9864e' -draw 'polygon 188,117 208,117 213,136 183,136' \
        -fill '#d3ad75' -draw 'polygon 188,117 198,110 208,117' \
        -fill '#ffffff66' -draw 'roundRectangle 178,103 218,118 8,8' \
        \
        -fill '#277dd0' -draw 'roundRectangle 252,101 296,145 10,10' \
        -fill '#69b7ff' -draw 'rectangle 255,104 293,118' \
        -fill '#ffffff' -draw 'roundRectangle 266,123 282,133 4,4 circle 270,120 274,120 circle 278,120 282,120' \
        -fill '#ffffff66' -draw 'roundRectangle 254,103 294,118 8,8' \
        \
        -fill '#858d98' -draw 'roundRectangle 24,181 68,225 10,10' \
        -fill '#cbd1da' -draw 'rectangle 27,184 65,199' \
        -fill '#ffffff66' -draw 'roundRectangle 26,183 66,198 8,8' \
        -fill '#ffffff' -draw 'circle 46,203 58,203 circle 46,203 52,203 circle 54,212 64,212 circle 54,212 59,212' \
        -fill '#4e87d5' -draw 'roundRectangle 100,181 144,225 10,10' \
        -fill '#9bc6ff' -draw 'circle 112,197 119,197 circle 132,197 139,197 rectangle 114,198 136,215 circle 118,214 123,214 circle 128,214 133,214' \
        -fill '#ffffff66' -draw 'roundRectangle 102,183 142,198 8,8' \
        -fill '#ffffff' -draw 'circle 116,208 118,208 circle 132,208 134,208' \
        -fill '#858d98' -draw 'roundRectangle 176,181 220,225 10,10' \
        -fill '#ffffff66' -draw 'roundRectangle 178,183 218,198 8,8' \
        -fill '#ffffff' -draw 'circle 198,203 210,203 circle 198,203 204,203 circle 206,212 216,212 circle 206,212 211,212' \
        -fill '#343940' -draw 'roundRectangle 252,181 296,225 10,10' \
        -fill '#868f99' -draw 'circle 274,203 291,203 circle 274,203 283,203' \
        -fill '#cfd5dc' -draw 'circle 274,203 280,203' \
        -fill '#ffffff66' -draw 'roundRectangle 254,183 294,198 8,8' \
        \
        -font DejaVu-Sans-Bold -pointsize 9 -fill '#ffffff' \
        -annotate +25+91 'Covers' -annotate +108+91 'Music' \
        -annotate +184+91 'Files' -annotate +257+91 'Videos' \
        -annotate +30+158 'Photos' -annotate +100+158 'Playlists' \
        -annotate +179+158 'Plugins' -annotate +246+158 'Shortcuts' \
        -annotate +23+235 'Settings' -annotate +107+235 'Games' \
        -annotate +176+235 'Podemon' -annotate +258+235 'System' \
        BMP3:"$out"
}

make_wps_backdrop() {
    local out="$1"
    "$im" -size 320x240 gradient:'#eef0f5-#c4c9d2' \
        -fill '#0c0d10' -draw 'rectangle 0,0 320,19' \
        -fill '#6bb7f5' -draw 'rectangle 0,20 320,31' \
        -fill '#0f67b7' -draw 'rectangle 0,32 320,47' \
        -fill '#075092' -draw 'rectangle 0,47 320,48' \
        -fill '#ffffff' -draw 'roundRectangle 18,56 166,204 12,12' \
        -fill '#a9b2be' -draw 'roundRectangle 22,60 162,200 10,10' \
        -fill '#f5f7fa' -draw 'roundRectangle 26,64 158,196 8,8' \
        -fill '#ffffff' -draw 'roundRectangle 176,57 306,142 10,10' \
        -fill '#d0d6df' -draw 'rectangle 176,88 306,88' \
        -fill '#d0d6df' -draw 'rectangle 176,115 306,115' \
        -fill '#e7ebf1' -draw 'roundRectangle 176,153 306,188 10,10' \
        -fill '#ffffff' -draw 'rectangle 179,155 303,168' \
        -fill '#2f88e6' -draw 'roundRectangle 205,160 277,181 10,10' \
        -fill '#7fc3ff' -draw 'rectangle 208,161 274,166' \
        -fill '#ffffff' -draw 'roundRectangle 18,211 302,234 11,11' \
        -fill '#a7b0bd' -draw 'roundRectangle 27,219 293,226 4,4' \
        -fill '#edf2f8' -draw 'roundRectangle 29,220 291,224 3,3' \
        BMP3:"$out"
}

make_fms_backdrop() {
    local out="$1"
    "$im" -size 320x240 gradient:'#eef0f5-#c4c9d2' \
        -fill '#0c0d10' -draw 'rectangle 0,0 320,19' \
        -fill '#6bb7f5' -draw 'rectangle 0,20 320,31' \
        -fill '#0f67b7' -draw 'rectangle 0,32 320,47' \
        -fill '#075092' -draw 'rectangle 0,47 320,48' \
        -fill '#ffffff' -draw 'roundRectangle 22,62 298,180 12,12' \
        -fill '#d0d6df' -draw 'rectangle 22,111 298,111' \
        -fill '#d0d6df' -draw 'rectangle 22,145 298,145' \
        -fill '#ffffff' -draw 'roundRectangle 18,205 302,232 11,11' \
        -fill '#a7b0bd' -draw 'roundRectangle 27,214 293,221 4,4' \
        -fill '#edf2f8' -draw 'roundRectangle 29,215 291,219 3,3' \
        BMP3:"$out"
}

make_wallpaper() {
    local out="$1"
    local top="$2"
    local bottom="$3"
    "$im" -size 320x240 gradient:"$top-$bottom" \
        -fill '#101216' -draw 'rectangle 0,0 320,19' \
        -fill '#ffffff20' -draw 'rectangle 0,20 320,21' \
        -fill '#000000' -draw 'rectangle 0,0 320,19' \
        -fill '#00000080' -draw 'rectangle 0,202 320,240' \
        -fill '#ffffff35' -draw 'rectangle 20,205 300,206' \
        -fill '#1d2734' -draw 'roundRectangle 63,207 257,234 14,14' \
        -fill '#b6c0cd' -draw 'roundRectangle 67,210 253,231 12,12' \
        -fill '#edf2f8' -draw 'roundRectangle 93,212 132,229 10,10' \
        -fill '#3c4654' -draw 'polygon 108,216 108,225 118,221' \
        BMP3:"$out"
}

make_charge_wallpaper() {
    local out="$1"
    local accent="$2"
    "$im" -size 320x240 gradient:'#12335c-#07111f' \
        -fill '#0c0d10' -draw 'rectangle 0,0 320,19' \
        -fill '#ffffff22' -draw 'rectangle 0,20 320,21' \
        -fill '#eef2f7' -draw 'roundRectangle 52,78 258,134 12,12' \
        -fill '#a8b0ba' -draw 'roundRectangle 54,80 256,132 10,10' \
        -fill '#f8fafc' -draw 'roundRectangle 58,84 242,128 8,8' \
        -fill '#cbd2db' -draw 'rectangle 244,96 254,116' \
        -fill "$accent" -draw 'roundRectangle 64,90 228,122 6,6' \
        -fill '#ffffff70' -draw 'rectangle 66,92 226,100' \
        BMP3:"$out"
}

make_player_fallback() {
    local out="$1"
    "$im" -size 146x146 gradient:'#f8f9fb-#c5cbd4' \
        -fill '#ffffff' -draw 'roundRectangle 8,8 138,138 16,16' \
        -fill '#d3d9e2' -draw 'roundRectangle 12,12 134,134 14,14' \
        -fill '#f5f7fa' -draw 'roundRectangle 18,18 128,128 12,12' \
        -fill '#2f88e6' -draw 'rectangle 82,42 92,93' \
        -fill '#2f88e6' -draw 'circle 65,103 82,103' \
        -fill '#2f88e6' -draw 'circle 96,95 113,95' \
        -fill '#2f88e6' -draw 'polygon 90,42 111,48 111,58 90,52' \
        BMP3:"$out"
}

make_notification() {
    local out="$1"
    "$im" -size 288x75 gradient:'#ffffff-#d8dde5' \
        -fill '#eef2f7' -draw 'rectangle 0,0 287,0' \
        -fill '#9aa5b3' -draw 'rectangle 0,74 287,74' \
        -fill '#ffffff90' -draw 'rectangle 0,1 287,15' \
        BMP3:"$out"
}

make_music_tile() {
    local out="$1"
    "$im" -size 51x51 gradient:'#f8fbff-#b7c5d6' \
        -fill '#2f88e6' -draw 'rectangle 30,10 35,33' \
        -fill '#2f88e6' -draw 'circle 21,37 31,37' \
        -fill '#2f88e6' -draw 'circle 36,34 46,34' \
        -fill '#2f88e6' -draw 'polygon 34,10 44,13 44,18 34,15' \
        BMP3:"$out"
}

make_slider_track() {
    local w="$1"
    local out="$2"
    "$im" -size "${w}x12" xc:'#b7c1cc' \
        -fill '#eef2f7' -draw "roundRectangle 0,3 $((w - 1)),8 5,5" \
        -fill '#9ba6b3' -draw "rectangle 3,8 $((w - 4)),8" \
        BMP3:"$out"
}

make_slider_fill() {
    local w="$1"
    local out="$2"
    "$im" -size "${w}x12" xc:none \
        -fill '#0b69c7' -draw "roundRectangle 0,3 $((w - 1)),8 5,5" \
        -fill '#7fc3ff' -draw "rectangle 3,3 $((w - 4)),4" \
        BMP3:"$out"
}

make_menu_backdrop "$theme_dir/SpringPod3_bd.bmp"
make_menu_backdrop "$theme_dir/SpringPod3_bg.bmp"
make_menu_backdrop "$theme_dir/SbsBackdrop.bmp"
make_menu_backdrop "$backdrop_dir/SpringPod3_bd.bmp"
make_wps_backdrop "$theme_dir/WpsBackdrop.bmp"
make_fms_backdrop "$theme_dir/FmsBackdrop.bmp"

make_wallpaper "$theme_dir/Wallpaper.bmp" '#356da7' '#07111f'
make_wallpaper "$theme_dir/WallpaperAlt.bmp" '#5a6877' '#111821'
make_wallpaper "$theme_dir/WallpaperThird.bmp" '#2e5d8e' '#07111f'
make_wallpaper "$theme_dir/WallpaperFourth.bmp" '#536170' '#101820'
make_wallpaper "$theme_dir/WallpaperFifth.bmp" '#234b75' '#07111f'
make_wallpaper "$theme_dir/WallpaperSixth.bmp" '#68727d' '#151b22'

make_charge_wallpaper "$theme_dir/ChargeWallpaper.bmp" '#66c941'
make_charge_wallpaper "$theme_dir/ChargeWallpaperAlt.bmp" '#3eb75f'
make_charge_wallpaper "$theme_dir/ChargeWallpaperThird.bmp" '#82d04a'
make_charge_wallpaper "$theme_dir/ChargeWallpaperFourth.bmp" '#55bd7a'

"$im" -size 320x92 xc:'#ffffff' BMP3:"$theme_dir/AODBackdrop.bmp"
make_player_fallback "$theme_dir/PlayerFallback.bmp"
make_music_tile "$theme_dir/NotifMusic.bmp"
make_notification "$theme_dir/Notification.bmp"

"$im" -size 180x45 gradient:'#f8f9fb-#c9d0da' \
    -fill '#ffffff' -draw 'rectangle 0,0 179,0' \
    -fill '#8994a3' -draw 'rectangle 0,44 179,44' \
    BMP3:"$theme_dir/VolumeBackdrop.bmp"
"$im" -size 117x5 xc:'#9da8b5' -fill '#edf2f8' -draw 'rectangle 0,1 116,3' BMP3:"$theme_dir/VolumeSliderBackdropPurple.bmp"
"$im" -size 117x5 xc:'#0a6fcf' -fill '#7fc3ff' -draw 'rectangle 0,0 116,1' BMP3:"$theme_dir/VolumeSliderPurple.bmp"
"$im" -size 3x5 xc:'#0a6fcf' BMP3:"$theme_dir/VolumeSliderEndPurple.bmp"

make_slider_fill 242 "$theme_dir/SliderThinPurple12.bmp"
make_slider_track 242 "$theme_dir/SliderBackdropThinPurple12.bmp"
make_slider_track 226 "$theme_dir/SliderBackdropThinPurple12_4Digits.bmp"
make_slider_track 200 "$theme_dir/SliderBackdropThinPurple12_5Digits.bmp"
make_slider_track 184 "$theme_dir/SliderBackdropThinPurple12_6Digits.bmp"
"$im" -size 12x12 xc:none -fill '#f8fbff' -draw 'circle 5,5 11,5' -fill '#2f88e6' -draw 'circle 5,5 8,5' BMP3:"$theme_dir/PlayerSliderThinPurple12.bmp"

"$im" -size 125x28 gradient:'#d8dee7-#9aa6b5' -fill '#ffffff' -draw 'roundRectangle 0,4 124,23 10,10' BMP3:"$theme_dir/LargeSliderBackdrop.bmp"
"$im" -size 30x28 gradient:'#ffffff-#c9d0da' -fill '#2f88e6' -draw 'polygon 11,8 20,14 11,20' BMP3:"$theme_dir/LargeSliderFallback.bmp"
