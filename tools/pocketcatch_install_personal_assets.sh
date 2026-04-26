#!/usr/bin/env bash
set -euo pipefail
shopt -s nullglob

profile="sim"

if [ "${1:-}" = "--hw" ]; then
    profile="hw"
    shift
fi

if [ "$#" -lt 1 ]; then
    echo "usage: $0 [--hw] <asset-root> [asset-root...]" >&2
    exit 1
fi

species_ids=(1 4 7 10 13 16 19 21 23 25 27 29 32 35 39 41 43 46 48 52 54 58 60 63 66 69 74 79 81 84 92 96 98 104 109 111 113 116 118 120 123 129 133)
species_names=(
    "Bulbasaur"
    "Charmander"
    "Squirtle"
    "Caterpie"
    "Weedle"
    "Pidgey"
    "Rattata"
    "Spearow"
    "Ekans"
    "Pikachu"
    "Sandshrew"
    "NidoranF"
    "NidoranM"
    "Clefairy"
    "Jigglypuff"
    "Zubat"
    "Oddish"
    "Paras"
    "Venonat"
    "Meowth"
    "Psyduck"
    "Growlithe"
    "Poliwag"
    "Abra"
    "Machop"
    "Bellsprout"
    "Geodude"
    "Slowpoke"
    "Magnemite"
    "Doduo"
    "Gastly"
    "Drowzee"
    "Krabby"
    "Cubone"
    "Koffing"
    "Rhyhorn"
    "Chansey"
    "Horsea"
    "Goldeen"
    "Staryu"
    "Scyther"
    "Magikarp"
    "Eevee"
)
species_rates=(0.62 0.47 0.38 0.78 0.76 0.73 0.76 0.72 0.64 0.54 0.68 0.62 0.62 0.52 0.60 0.72 0.70 0.68 0.66 0.64 0.71 0.50 0.70 0.43 0.65 0.69 0.71 0.72 0.60 0.72 0.56 0.67 0.72 0.69 0.58 0.45 0.30 0.72 0.72 0.70 0.48 0.91 0.48)
species_ids+=(152 155 158 161 163 165 167 170 172 177 179 183 187 190 194 200 218)
species_names+=("Chikorita" "Cyndaquil" "Totodile" "Sentret" "Hoothoot" "Ledyba" "Spinarak" "Chinchou" "Pichu" "Natu" "Mareep" "Marill" "Hoppip" "Aipom" "Wooper" "Misdreavus" "Slugma")
species_rates+=(0.54 0.45 0.43 0.71 0.62 0.66 0.70 0.62 0.69 0.68 0.56 0.69 0.72 0.62 0.68 0.45 0.58)
extra_species_ids=(2 3 5 6 8 9 11 12 14 15 17 18 20 22 24 26 28 30 31 33 34 36 40 44 45 47 49 53 55 59 61 62 70 71 97 99 105 110 112 119 121 130 153 154 156 157 159 160 162 164 166 168 171 178 180 181 184 188 189 195 219)
extra_species_names=("Ivysaur" "Venusaur" "Charmeleon" "Charizard" "Wartortle" "Blastoise" "Metapod" "Butterfree" "Kakuna" "Beedrill" "Pidgeotto" "Pidgeot" "Raticate" "Fearow" "Arbok" "Raichu" "Sandslash" "Nidorina" "Nidoqueen" "Nidorino" "Nidoking" "Clefable" "Wigglytuff" "Gloom" "Vileplume" "Parasect" "Venomoth" "Persian" "Golduck" "Arcanine" "Poliwhirl" "Poliwrath" "Weepinbell" "Victreebel" "Hypno" "Kingler" "Marowak" "Weezing" "Rhydon" "Seaking" "Starmie" "Gyarados" "Bayleef" "Meganium" "Quilava" "Typhlosion" "Croconaw" "Feraligatr" "Furret" "Noctowl" "Ledian" "Ariados" "Lanturn" "Xatu" "Flaaffy" "Ampharos" "Azumarill" "Skiploom" "Jumpluff" "Quagsire" "Magcargo")

for i in "${!extra_species_ids[@]}"; do
    species_ids+=("${extra_species_ids[$i]}")
    species_names+=("${extra_species_names[$i]}")
    species_rates+=(0.36)
done

if command -v magick >/dev/null 2>&1; then
    IM_CMD=(magick)
else
    IM_CMD=(convert)
fi

if [ "$profile" = "hw" ]; then
    CREATURE_SIZE="64x64"
    BALL_SIZE="32x32"
    TRAINER_SIZE="32x32"
    MENU_SIZE="48x48"
else
    CREATURE_SIZE="88x88"
    BALL_SIZE="40x40"
    TRAINER_SIZE="40x40"
    MENU_SIZE="72x72"
fi

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

fetch_optional_png() {
    local url="$1"
    local output="$2"
    curl -fsSL "$url" -o "$output" 2>/dev/null || return 1
}

png_to_bmp() {
    local input_png="$1"
    local output_bmp="$2"
    local size="$3"
    shift 3
    "${IM_CMD[@]}" "$input_png" \
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
  "pack_name": "Podemon Go Personal Johto Pack",
  "version": "0.3.0",
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

extract_frames() {
    local input_png="$1"
    local prefix="$2"

    rm -f "${prefix}"_*.png
    "${IM_CMD[@]}" "$input_png" -coalesce "${prefix}_%02d.png" >/dev/null 2>&1
}

install_walk_frames() {
    local source_png="$1"
    local fallback_png="$2"
    local output_prefix="$3"
    local extra_arg="${4:-}"
    local frames=()
    local idx0=0
    local idx1=0
    local idx2=0

    if [ -f "$source_png" ]; then
        extract_frames "$source_png" "$tmp_dir/frame"
        frames=("$tmp_dir"/frame_*.png)
    fi

    if [ "${#frames[@]}" -gt 0 ]; then
        idx1=$(( ${#frames[@]} > 1 ? 1 : 0 ))
        idx2=$(( ${#frames[@]} > 2 ? 2 : idx1 ))
        if [ -n "$extra_arg" ]; then
            png_to_bmp "${frames[$idx0]}" "${output_prefix}_0.bmp" "$TRAINER_SIZE" "$extra_arg"
            png_to_bmp "${frames[$idx1]}" "${output_prefix}_1.bmp" "$TRAINER_SIZE" "$extra_arg"
            png_to_bmp "${frames[$idx2]}" "${output_prefix}_2.bmp" "$TRAINER_SIZE" "$extra_arg"
        else
            png_to_bmp "${frames[$idx0]}" "${output_prefix}_0.bmp" "$TRAINER_SIZE"
            png_to_bmp "${frames[$idx1]}" "${output_prefix}_1.bmp" "$TRAINER_SIZE"
            png_to_bmp "${frames[$idx2]}" "${output_prefix}_2.bmp" "$TRAINER_SIZE"
        fi
        return
    fi

    if [ -n "$extra_arg" ]; then
        png_to_bmp "$fallback_png" "${output_prefix}_0.bmp" "$TRAINER_SIZE" "$extra_arg"
        png_to_bmp "$fallback_png" "${output_prefix}_1.bmp" "$TRAINER_SIZE" "$extra_arg"
        png_to_bmp "$fallback_png" "${output_prefix}_2.bmp" "$TRAINER_SIZE" "$extra_arg"
    else
        png_to_bmp "$fallback_png" "${output_prefix}_0.bmp" "$TRAINER_SIZE"
        png_to_bmp "$fallback_png" "${output_prefix}_1.bmp" "$TRAINER_SIZE"
        png_to_bmp "$fallback_png" "${output_prefix}_2.bmp" "$TRAINER_SIZE"
    fi
}

install_frlg_sheet_frames() {
    local source_png="$1"
    local output_prefix="$2"
    local idx_a="$3"
    local idx_stand="$4"
    local idx_b="$5"
    local extra_arg="${6:-}"
    local crop_a="$tmp_dir/$(basename "$output_prefix")_a.png"
    local crop_stand="$tmp_dir/$(basename "$output_prefix")_stand.png"
    local crop_b="$tmp_dir/$(basename "$output_prefix")_b.png"

    "${IM_CMD[@]}" "$source_png" -crop "16x32+$((idx_a * 16))+0" +repage "$crop_a"
    "${IM_CMD[@]}" "$source_png" -crop "16x32+$((idx_stand * 16))+0" +repage "$crop_stand"
    "${IM_CMD[@]}" "$source_png" -crop "16x32+$((idx_b * 16))+0" +repage "$crop_b"

    if [ -n "$extra_arg" ]; then
        png_to_bmp "$crop_a" "${output_prefix}_0.bmp" "$TRAINER_SIZE" "$extra_arg"
        png_to_bmp "$crop_stand" "${output_prefix}_1.bmp" "$TRAINER_SIZE" "$extra_arg"
        png_to_bmp "$crop_b" "${output_prefix}_2.bmp" "$TRAINER_SIZE" "$extra_arg"
    else
        png_to_bmp "$crop_a" "${output_prefix}_0.bmp" "$TRAINER_SIZE"
        png_to_bmp "$crop_stand" "${output_prefix}_1.bmp" "$TRAINER_SIZE"
        png_to_bmp "$crop_b" "${output_prefix}_2.bmp" "$TRAINER_SIZE"
    fi
}

for species_id in "${species_ids[@]}"; do
    if [ "$species_id" -le 151 ]; then
        fetch_png \
            "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/versions/generation-iii/firered-leafgreen/${species_id}.png" \
            "$tmp_dir/${species_id}.png"
    else
        fetch_png \
            "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/versions/generation-iv/heartgold-soulsilver/${species_id}.png" \
            "$tmp_dir/${species_id}.png"
    fi
done

fetch_png \
    "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/items/poke-ball.png" \
    "$tmp_dir/pokeball.png"

fetch_png \
    "https://archives.bulbagarden.net/wiki/Special:Redirect/file/Ethan_OD.png" \
    "$tmp_dir/ethan_od.png"
fetch_png \
    "https://archives.bulbagarden.net/wiki/Special:Redirect/file/Lyra_OD.png" \
    "$tmp_dir/lyra_od.png"
fetch_png \
    "https://archives.bulbagarden.net/wiki/Special:Redirect/file/HGSS_Ethan_Back.png" \
    "$tmp_dir/ethan_back.png"
fetch_png \
    "https://archives.bulbagarden.net/wiki/Special:Redirect/file/HGSS_Lyra_Back.png" \
    "$tmp_dir/lyra_back.png"
fetch_png \
    "https://archives.bulbagarden.net/wiki/Special:Redirect/file/Spr_FRLG_Red.png" \
    "$tmp_dir/red_menu.png"
fetch_png \
    "https://archives.bulbagarden.net/wiki/Special:Redirect/file/Spr_FRLG_Leaf.png" \
    "$tmp_dir/leaf_menu.png"
fetch_png \
    "https://raw.githubusercontent.com/pret/pokefirered/master/graphics/object_events/pics/people/green_normal.png" \
    "$tmp_dir/leaf_frlg_walk_sheet.png"
fetch_png \
    "https://archives.bulbagarden.net/wiki/Special:Redirect/file/New_Bark_Town_HGSS.png" \
    "$tmp_dir/new_bark_town_hgss.png"
fetch_png \
    "https://archives.bulbagarden.net/wiki/Special:Redirect/file/Clear_icon_SwSh.png" \
    "$tmp_dir/weather_clear.png"
fetch_png \
    "https://archives.bulbagarden.net/wiki/Special:Redirect/file/Cloudy_icon_SwSh.png" \
    "$tmp_dir/weather_cloudy.png"
fetch_png \
    "https://archives.bulbagarden.net/wiki/Special:Redirect/file/Rain_icon_SwSh.png" \
    "$tmp_dir/weather_rain.png"
fetch_png \
    "https://archives.bulbagarden.net/wiki/Special:Redirect/file/Fog_icon_SwSh.png" \
    "$tmp_dir/weather_fog.png"
fetch_png \
    "https://archives.bulbagarden.net/wiki/Special:Redirect/file/Sandstorm_icon_SwSh.png" \
    "$tmp_dir/weather_sand.png"

fetch_optional_png \
    "https://archives.bulbagarden.net/wiki/Special:Redirect/file/Ethanwalkdown.png" \
    "$tmp_dir/ethan_walk_s.png" || true
fetch_optional_png \
    "https://archives.bulbagarden.net/wiki/Special:Redirect/file/Ethanwalkup.png" \
    "$tmp_dir/ethan_walk_n.png" || true
fetch_optional_png \
    "https://archives.bulbagarden.net/wiki/Special:Redirect/file/Ethanwalkright.png" \
    "$tmp_dir/ethan_walk_e.png" || true
fetch_optional_png \
    "https://archives.bulbagarden.net/wiki/Special:Redirect/file/Ethanwalkleft.png" \
    "$tmp_dir/ethan_walk_w.png" || true
fetch_optional_png \
    "https://archives.bulbagarden.net/wiki/Special:Redirect/file/Lyrawalkdown.png" \
    "$tmp_dir/lyra_walk_s.png" || true
fetch_optional_png \
    "https://archives.bulbagarden.net/wiki/Special:Redirect/file/Lyrawalkup.png" \
    "$tmp_dir/lyra_walk_n.png" || true
fetch_optional_png \
    "https://archives.bulbagarden.net/wiki/Special:Redirect/file/Lyrawalkright.png" \
    "$tmp_dir/lyra_walk_e.png" || true
fetch_optional_png \
    "https://archives.bulbagarden.net/wiki/Special:Redirect/file/Lyrawalkleft.png" \
    "$tmp_dir/lyra_walk_w.png" || true

for dest_root in "$@"; do
    mkdir -p "$dest_root/sprites/creatures"
    mkdir -p "$dest_root/sprites/balls"
    mkdir -p "$dest_root/sprites/trainers"
    mkdir -p "$dest_root/sprites/ui"
    mkdir -p "$dest_root/backgrounds"

    for species_id in "${species_ids[@]}"; do
        png_to_bmp "$tmp_dir/${species_id}.png" \
            "$dest_root/sprites/creatures/$(printf 'creature_%03d_idle_0.bmp' "$species_id")" \
            "$CREATURE_SIZE"
    done

    png_to_bmp "$tmp_dir/pokeball.png" "$dest_root/sprites/balls/ball_default_idle_0.bmp" "$BALL_SIZE"
    png_to_bmp "$tmp_dir/pokeball.png" "$dest_root/sprites/balls/ball_default_spin_0.bmp" "$BALL_SIZE" \
        -virtual-pixel transparent -distort SRT 18
    png_to_bmp "$tmp_dir/pokeball.png" "$dest_root/sprites/balls/ball_default_spin_1.bmp" "$BALL_SIZE" \
        -virtual-pixel transparent -distort SRT 36
    png_to_bmp "$tmp_dir/pokeball.png" "$dest_root/sprites/balls/ball_default_spin_2.bmp" "$BALL_SIZE" \
        -virtual-pixel transparent -distort SRT 54
    png_to_bmp "$tmp_dir/pokeball.png" "$dest_root/sprites/balls/ball_default_spin_3.bmp" "$BALL_SIZE" \
        -virtual-pixel transparent -distort SRT 72

    png_to_bmp "$tmp_dir/ethan_od.png" "$dest_root/sprites/trainers/ethan_od.bmp" "$TRAINER_SIZE"
    png_to_bmp "$tmp_dir/lyra_od.png" "$dest_root/sprites/trainers/lyra_od.bmp" "$TRAINER_SIZE"
    png_to_bmp "$tmp_dir/ethan_back.png" "$dest_root/sprites/trainers/ethan_back.bmp" "$TRAINER_SIZE"
    png_to_bmp "$tmp_dir/lyra_back.png" "$dest_root/sprites/trainers/lyra_back.bmp" "$TRAINER_SIZE"
    png_to_bmp "$tmp_dir/red_menu.png" "$dest_root/sprites/trainers/red_menu.bmp" "$MENU_SIZE"
    png_to_bmp "$tmp_dir/leaf_menu.png" "$dest_root/sprites/trainers/leaf_menu.bmp" "$MENU_SIZE"
    png_to_bmp "$tmp_dir/new_bark_town_hgss.png" \
        "$dest_root/backgrounds/new_bark_town_hgss.bmp" "493x397>"
    png_to_bmp "$tmp_dir/weather_clear.png" \
        "$dest_root/sprites/ui/weather_clear.bmp" "18x18"
    png_to_bmp "$tmp_dir/weather_cloudy.png" \
        "$dest_root/sprites/ui/weather_cloudy.bmp" "18x18"
    png_to_bmp "$tmp_dir/weather_rain.png" \
        "$dest_root/sprites/ui/weather_rain.bmp" "18x18"
    png_to_bmp "$tmp_dir/weather_fog.png" \
        "$dest_root/sprites/ui/weather_fog.bmp" "18x18"
    png_to_bmp "$tmp_dir/weather_sand.png" \
        "$dest_root/sprites/ui/weather_sand.bmp" "18x18"

    install_frlg_sheet_frames "$tmp_dir/leaf_frlg_walk_sheet.png" \
        "$dest_root/sprites/trainers/leaf_walk_s" 3 0 4
    install_frlg_sheet_frames "$tmp_dir/leaf_frlg_walk_sheet.png" \
        "$dest_root/sprites/trainers/leaf_walk_n" 5 1 6
    install_frlg_sheet_frames "$tmp_dir/leaf_frlg_walk_sheet.png" \
        "$dest_root/sprites/trainers/leaf_walk_w" 7 2 8
    install_frlg_sheet_frames "$tmp_dir/leaf_frlg_walk_sheet.png" \
        "$dest_root/sprites/trainers/leaf_walk_e" 7 2 8 -flop

    install_walk_frames "$tmp_dir/ethan_walk_s.png" "$tmp_dir/ethan_od.png" \
        "$dest_root/sprites/trainers/ethan_walk_s"
    install_walk_frames "$tmp_dir/ethan_walk_n.png" "$tmp_dir/ethan_back.png" \
        "$dest_root/sprites/trainers/ethan_walk_n"
    install_walk_frames "$tmp_dir/ethan_walk_e.png" "$tmp_dir/ethan_od.png" \
        "$dest_root/sprites/trainers/ethan_walk_e"
    install_walk_frames "$tmp_dir/ethan_walk_w.png" "$tmp_dir/ethan_od.png" \
        "$dest_root/sprites/trainers/ethan_walk_w" -flop

    install_walk_frames "$tmp_dir/lyra_walk_s.png" "$tmp_dir/lyra_od.png" \
        "$dest_root/sprites/trainers/lyra_walk_s"
    install_walk_frames "$tmp_dir/lyra_walk_n.png" "$tmp_dir/lyra_back.png" \
        "$dest_root/sprites/trainers/lyra_walk_n"
    install_walk_frames "$tmp_dir/lyra_walk_e.png" "$tmp_dir/lyra_od.png" \
        "$dest_root/sprites/trainers/lyra_walk_e"
    install_walk_frames "$tmp_dir/lyra_walk_w.png" "$tmp_dir/lyra_od.png" \
        "$dest_root/sprites/trainers/lyra_walk_w" -flop

    write_pack_json "$dest_root"
    echo "installed assets to $dest_root"
done
