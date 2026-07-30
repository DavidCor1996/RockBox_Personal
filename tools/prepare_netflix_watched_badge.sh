#!/usr/bin/env bash
# Render the Netflix "watched" badge from a real font glyph.
#
# The badge is a solid 16x16 tile: the brand red measured from the shipped
# period wordmark, with U+2713 CHECK MARK set in white on top. It is opaque on
# purpose - the catalog blits it straight into a poster corner, so no
# transparency key is involved. Nothing here is drawn by hand; the check is a
# glyph taken from an installed font, recorded in SOURCES.tsv.
set -euo pipefail

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output_dir="$repo_root/assets/ipodjs/rockbox/netflix"
source_dir="$repo_root/assets/ipodjs/sources/netflix/badges"
size=16

# Measured dominant red of netflix-logo-2001.150x70x24.bmp.
brand_red="#B4131D"

mkdir -p "$output_dir" "$source_dir"

font=""
for candidate in \
    /usr/share/fonts/Adwaita/AdwaitaSans-Regular.ttf \
    /usr/share/fonts/TTF/DejaVuSans.ttf \
    /usr/share/fonts/dejavu/DejaVuSans.ttf \
    /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf \
    /System/Library/Fonts/Supplemental/Arial\ Unicode.ttf
do
    [ -f "$candidate" ] || continue
    charset=$(fc-query --format '%{charset}\n' "$candidate" 2>/dev/null || true)
    if python3 - "$charset" <<'PY'
import sys
target = 0x2713
for token in sys.argv[1].split():
    try:
        if "-" in token:
            low, high = token.split("-")
            if int(low, 16) <= target <= int(high, 16):
                sys.exit(0)
        elif int(token, 16) == target:
            sys.exit(0)
    except ValueError:
        continue
sys.exit(1)
PY
    then
        font=$candidate
        break
    fi
done

if [ -z "$font" ]; then
    echo "no installed font provides U+2713; install DejaVu Sans" >&2
    exit 1
fi

# A hairline check disappears at 16 px against red, so the glyph is stroked
# to about a two-pixel weight. That thickens the real outline; it does not
# redraw it.
magick -size "${size}x${size}" "xc:$brand_red" \
    -font "$font" -pointsize 16 -fill white \
    -stroke white -strokewidth 0.9 -gravity center \
    -annotate +0+0 $'✓' \
    -depth 8 -type TrueColor \
    "BMP3:$output_dir/watched.${size}x${size}x24.bmp"

{
    printf 'asset\tsource\tusage\n'
    printf 'watched.%sx%sx24.bmp\tU+2713 CHECK MARK glyph from %s over the #B4131D brand red measured from netflix-logo-2001.150x70x24.bmp\tNetflix watched indicator\n' \
        "$size" "$size" "$(basename "$font")"
} > "$source_dir/SOURCES.tsv"

(
    cd "$output_dir"
    sha256sum "watched.${size}x${size}x24.bmp" > "watched.SHA256SUMS"
)

echo "wrote $output_dir/watched.${size}x${size}x24.bmp from $font"
