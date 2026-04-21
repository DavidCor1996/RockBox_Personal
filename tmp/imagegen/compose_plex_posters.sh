#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "usage: $0 <background-dir> <manifest.tsv> <backup-suffix>" >&2
  exit 1
fi

bg_dir="$1"
manifest="$2"
backup_suffix="$3"

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

calc_timestamp() {
  python3 - "$1" "$2" <<'PY'
import sys
duration = float(sys.argv[1] or 0)
ratio = float(sys.argv[2] or 0.2)
if duration <= 0:
    print("1")
else:
    ts = max(1.0, min(duration - 1.0, duration * ratio))
    print(f"{ts:.3f}")
PY
}

make_frame() {
  local source="$1"
  local ratio="$2"
  local target="$3"
  local duration ts
  duration="$(ffprobe -v error -show_entries format=duration -of default=nw=1:nk=1 "$source" 2>/dev/null || echo 0)"
  ts="$(calc_timestamp "$duration" "$ratio")"
  ffmpeg -loglevel error -y -ss "$ts" -i "$source" -frames:v 1 "$target"
}

while IFS=$'\t' read -r bg_name out_path source_video ratio style label title subtitle meta1 meta2 accent; do
  [[ -n "${out_path:-}" ]] || continue
  [[ -f "$source_video" ]] || { echo "missing source video: $source_video" >&2; exit 1; }

  mkdir -p "$(dirname "$out_path")"
  if [[ -f "$out_path" && ! -f "${out_path%.jpg}${backup_suffix}.jpg" ]]; then
    cp "$out_path" "${out_path%.jpg}${backup_suffix}.jpg"
  fi

  base="$(basename "${out_path%.jpg}")"
  raw="$tmp_dir/${base}-raw.jpg"
  bg="$tmp_dir/${base}-bg.jpg"
  hero="$tmp_dir/${base}-hero.png"
  hero_shadow="$tmp_dir/${base}-hero-shadow.png"
  label_img="$tmp_dir/${base}-label.png"
  title_img="$tmp_dir/${base}-title.png"
  meta_img="$tmp_dir/${base}-meta.png"
  year_chip="$tmp_dir/${base}-year.png"
  year=""
  title_pt=52

  make_frame "$source_video" "$ratio" "$raw"

  magick "$raw" \
    -resize '1000x1500^' \
    -gravity center -extent 1000x1500 \
    -blur 0x14 \
    -modulate 78,85,100 \
    \( -size 1000x1500 radial-gradient:'#00000000-#000000cc' \) -compose multiply -composite \
    "$bg"

  magick "$raw" \
    -resize '760x840^' \
    -gravity center -extent 760x840 \
    -bordercolor white -border 10 \
    -alpha set "$hero"

  magick "$hero" -background black -shadow 80x6+0+8 "$hero_shadow"

  if (( ${#title} > 42 )); then
    title_pt=36
  elif (( ${#title} > 28 )); then
    title_pt=44
  fi

  magick -background none -size 320x60 \
    pango:"<span font='DejaVu Sans 22' foreground='${accent}'>${label}</span>" \
    "$label_img"

  magick -background none -size 820x260 \
    -define pango:justify=false \
    pango:"<span font='DejaVu Sans Bold ${title_pt}' foreground='white'>${title}</span>" \
    "$title_img"

  magick -background none -size 820x170 \
    pango:"<span font='DejaVu Sans 20' foreground='#d8dee5'>${subtitle}\n${meta1}\n${meta2}</span>" \
    "$meta_img"

  if [[ "$title" =~ \(([0-9]{4})\) ]]; then
    year="${BASH_REMATCH[1]}"
  elif [[ "$title" =~ ^([0-9]{4})$ ]]; then
    year="${BASH_REMATCH[1]}"
  fi

  if [[ -n "$year" ]]; then
    magick -size 210x92 xc:'#081018d8' \
      -fill "$accent" -draw 'rectangle 0,0 7,92' \
      -fill white -font 'DejaVu-Sans-Bold' -pointsize 38 -gravity center \
      -annotate +12+0 "$year" "$year_chip"
  fi

  if [[ -f "$year_chip" ]]; then
    magick "$bg" \
      "$hero_shadow" -gravity north -geometry +0+86 -composite \
      "$hero" -gravity north -geometry +0+86 -composite \
      \( -size 1000x390 xc:'#081018e8' \) -gravity south -composite \
      "$label_img" -gravity southwest -geometry +84+286 -composite \
      "$title_img" -gravity southwest -geometry +78+148 -composite \
      "$meta_img" -gravity southwest -geometry +82+42 -composite \
      "$year_chip" -gravity northeast -geometry +64+58 -composite \
      -quality 94 "$out_path"
  else
    magick "$bg" \
      "$hero_shadow" -gravity north -geometry +0+86 -composite \
      "$hero" -gravity north -geometry +0+86 -composite \
      \( -size 1000x390 xc:'#081018e8' \) -gravity south -composite \
      "$label_img" -gravity southwest -geometry +84+286 -composite \
      "$title_img" -gravity southwest -geometry +78+148 -composite \
      "$meta_img" -gravity southwest -geometry +82+42 -composite \
      -quality 94 "$out_path"
  fi

  echo "Wrote $out_path"
done < "$manifest"
