#!/usr/bin/env bash
set -euo pipefail

photos_dir="/run/media/david/DAVID_S IPO/Photos"
thumb_dir="$photos_dir/.photo_thumbs"
thumb_size="${THUMB_SIZE:-80x60>}"
rebuild_all=0

usage() {
    cat <<'EOF'
Usage:
  tools/generate_ipod_photo_thumbs.sh [PHOTOS_DIR] [--rebuild]

Behavior:
  - Scans PHOTOS_DIR for supported image files.
  - Writes missing or invalid thumbnails to PHOTOS_DIR/.photo_thumbs.
  - Matches RockPod: aspect-preserving, at most 80x60, Windows BMP3.

Examples:
  tools/generate_ipod_photo_thumbs.sh
  tools/generate_ipod_photo_thumbs.sh "/run/media/david/DAVID_S IPO/Photos" --rebuild
EOF
}

is_supported_photo() {
    local lower
    lower="${1,,}"
    case "$lower" in
        *.bmp|*.gif|*.jpg|*.jpe|*.jpeg|*.png|*.ppm) return 0 ;;
        *) return 1 ;;
    esac
}

relative_path() {
    local path="$1"
    path="${path#"$photos_dir"/}"
    printf '%s\n' "$path"
}

thumb_is_valid() {
    local thumb="$1"
    local desc
    local dimensions
    local width
    local height

    [[ -s "$thumb" ]] || return 1
    desc="$(file -b "$thumb" 2>/dev/null || true)"
    [[ "$desc" == *"PC bitmap"* ]] || return 1
    [[ "$desc" == *" x 24"* ]] || return 1
    dimensions="$(magick identify -format '%w %h' "$thumb" 2>/dev/null || true)"
    read -r width height <<<"$dimensions"
    [[ "$width" =~ ^[0-9]+$ && "$height" =~ ^[0-9]+$ ]] || return 1
    (( width > 0 && width <= 80 && height > 0 && height <= 60 )) || return 1
    return 0
}

generate_thumb() {
    local src="$1"
    local dst="$2"

    magick "$src" \
        -auto-orient \
        -thumbnail "$thumb_size" \
        -background white \
        -alpha remove \
        -alpha off \
        "BMP3:$dst"
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

for arg in "$@"; do
    case "$arg" in
        --rebuild) rebuild_all=1 ;;
        *) photos_dir="$arg" ;;
    esac
done

thumb_dir="$photos_dir/.photo_thumbs"

if [[ ! -d "$photos_dir" ]]; then
    echo "Photos directory not found: $photos_dir" >&2
    exit 1
fi

mkdir -p "$thumb_dir"

created=0
skipped=0
failed=0

while IFS= read -r -d '' src; do
    rel="$(relative_path "$src")"
    thumb="$thumb_dir/$rel.bmp"
    mkdir -p "$(dirname "$thumb")"

    if (( rebuild_all == 0 )) && thumb_is_valid "$thumb"; then
        skipped=$((skipped + 1))
        continue
    fi

    if generate_thumb "$src" "$thumb"; then
        created=$((created + 1))
        echo "thumb: $rel"
    else
        failed=$((failed + 1))
        echo "failed: $rel" >&2
        rm -f "$thumb"
    fi
done < <(
    find "$photos_dir" \
        \( -path "$thumb_dir" -o -path "$thumb_dir/*" \
           -o -path "$photos_dir/.photo_previews" \
           -o -path "$photos_dir/.photo_previews/*" \) -prune -o \
        -type f -print0 |
    while IFS= read -r -d '' candidate; do
        if is_supported_photo "$candidate"; then
            printf '%s\0' "$candidate"
        fi
    done
)

echo "created=$created skipped=$skipped failed=$failed"

if (( failed > 0 )); then
    exit 2
fi
