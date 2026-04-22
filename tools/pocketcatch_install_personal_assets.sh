#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 1 ]; then
    echo "usage: $0 <asset-root> [asset-root...]" >&2
    exit 1
fi

species_ids=(1 4 7 10 16 19 25 35 52 54 63 92 129 133)
species_names=(
    "Bulbasaur"
    "Charmander"
    "Squirtle"
    "Caterpie"
    "Pidgey"
    "Rattata"
    "Pikachu"
    "Clefairy"
    "Meowth"
    "Psyduck"
    "Abra"
    "Gastly"
    "Magikarp"
    "Eevee"
)
species_rates=(0.62 0.47 0.38 0.78 0.73 0.76 0.54 0.52 0.64 0.71 0.43 0.56 0.91 0.48)

tmp_dir="$(mktemp -d /tmp/pocketcatch-assets-XXXXXX)"
cleanup() {
    rm -rf "$tmp_dir"
}
trap cleanup EXIT

fetch_png() {
    local url="$1"
    local output="$2"
    curl -fsSL "$url" -o "$output"
}

png_to_bmp() {
    local input_png="$1"
    local output_bmp="$2"
    local size="$3"
    shift 3
    magick "$input_png" \
        -filter point \
        -resize "$size" \
        "$@" \
        -background '#ff00ff' \
        -alpha remove \
        -alpha off \
        BMP3:"$output_bmp"
}

write_pack_json() {
    local pack_root="$1"
    local i

    {
        cat <<'EOF'
{
  "pack_name": "Podemon Go Personal FireRed",
  "version": "0.2.0",
  "target": "rockbox-pocketcatch",
  "author": "local-user",
  "screen_mode": "auto",
  "creatures": [
EOF

        for i in "${!species_ids[@]}"; do
            printf '    {\n'
            printf '      "id": %d,\n' "${species_ids[$i]}"
            printf '      "name": "%s",\n' "${species_names[$i]}"
            printf '      "base_capture_rate": %s,\n' "${species_rates[$i]}"
            printf '      "sprite_prefix": "creature_%03d",\n' "${species_ids[$i]}"
            printf '      "anim": { "idle_frames": 1, "hit_frames": 1, "catch_frames": 1 }\n'
            if [ "$i" -eq $((${#species_ids[@]} - 1)) ]; then
                printf '    }\n'
            else
                printf '    },\n'
            fi
        done

        cat <<'EOF'
  ],
  "balls": [
    {
      "id": "default",
      "name": "Poke Ball",
      "curve_bonus": 0.10,
      "catch_bonus": 1.00,
      "sprite_prefix": "ball_default",
      "spin_frames": 4
    }
  ],
  "scene": {
    "bg_prefix": "scene_day",
    "ring_radius_px": 18,
    "target_y_px": 42
  }
}
EOF
    } > "$pack_root/pack.json"
}

for species_id in "${species_ids[@]}"; do
    fetch_png \
        "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/versions/generation-iii/firered-leafgreen/${species_id}.png" \
        "$tmp_dir/${species_id}.png"
done

fetch_png \
    "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/items/poke-ball.png" \
    "$tmp_dir/pokeball.png"

for dest_root in "$@"; do
    mkdir -p "$dest_root/sprites/creatures"
    mkdir -p "$dest_root/sprites/balls"
    mkdir -p "$dest_root/sprites/ui"
    mkdir -p "$dest_root/backgrounds"

    for species_id in "${species_ids[@]}"; do
        png_to_bmp "$tmp_dir/${species_id}.png" \
            "$dest_root/sprites/creatures/$(printf 'creature_%03d_idle_0.bmp' "$species_id")" \
            "88x88"
    done

    png_to_bmp "$tmp_dir/pokeball.png" "$dest_root/sprites/balls/ball_default_idle_0.bmp" "40x40"
    png_to_bmp "$tmp_dir/pokeball.png" "$dest_root/sprites/balls/ball_default_spin_0.bmp" "40x40" \
        -virtual-pixel transparent -distort SRT 18
    png_to_bmp "$tmp_dir/pokeball.png" "$dest_root/sprites/balls/ball_default_spin_1.bmp" "40x40" \
        -virtual-pixel transparent -distort SRT 36
    png_to_bmp "$tmp_dir/pokeball.png" "$dest_root/sprites/balls/ball_default_spin_2.bmp" "40x40" \
        -virtual-pixel transparent -distort SRT 54
    png_to_bmp "$tmp_dir/pokeball.png" "$dest_root/sprites/balls/ball_default_spin_3.bmp" "40x40" \
        -virtual-pixel transparent -distort SRT 72

    write_pack_json "$dest_root"
    echo "installed assets to $dest_root"
done
