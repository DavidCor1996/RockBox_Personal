#!/usr/bin/env bash
set -eu

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
out_dir="${1:-"$repo_root/assets/game_covers/native"}"
manual_dir="$repo_root/manual/plugins/images"
tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/native-covers.XXXXXX")"

font_regular="${COVER_FONT_REGULAR:-DejaVu-Sans}"
font_bold="${COVER_FONT_BOLD:-DejaVu-Sans-Bold}"

cleanup()
{
    rm -rf "$tmp_dir"
}

trap cleanup EXIT INT TERM

mkdir -p "$out_dir"

write_source()
{
    printf '%s\t%s\t%s\n' "$1" "$2" "$3" >> "$out_dir/SOURCES.tsv"
}

reset_manifest()
{
    {
        printf 'plugin\tkind\tsource\n'
    } > "$out_dir/SOURCES.tsv"
}

screenshot_cover()
{
    stem="$1"
    shot="$2"
    src="$manual_dir/ss-$shot-320x240x16.png"

    if [ ! -f "$src" ]; then
        src="$manual_dir/ss-$shot-320x240x24.png"
    fi

    if [ ! -f "$src" ]; then
        echo "missing screenshot: $shot" >&2
        exit 1
    fi

    magick "$src" -auto-orient -resize 160x120^ -gravity center \
        -extent 160x120 -colorspace sRGB -strip BMP3:"$out_dir/$stem.bmp"
    write_source "$stem" "rockbox-manual-screenshot" "${src#$repo_root/}"
}

file_cover()
{
    stem="$1"
    src="$2"

    if [ ! -f "$src" ]; then
        echo "missing cover source: $src" >&2
        exit 1
    fi

    magick "$src" -auto-orient -resize 160x120^ -gravity center \
        -extent 160x120 -colorspace sRGB -strip BMP3:"$out_dir/$stem.bmp"
    write_source "$stem" "existing-cover" "${src#$repo_root/}"
}

online_cover()
{
    stem="$1"
    url="$2"
    page="$3"
    src="$tmp_dir/$stem-online"

    if ! command -v curl >/dev/null 2>&1; then
        return
    fi

    if ! curl -L -f -s -A "Mozilla/5.0" -o "$src" "$url"; then
        return
    fi

    magick "$src" -auto-orient -resize 160x120^ -gravity center \
        -extent 160x120 -colorspace sRGB -strip BMP3:"$out_dir/$stem.bmp"
    write_source "$stem" "online-cover" "$page"
}

online_logo_cover()
{
    stem="$1"
    url="$2"
    page="$3"
    top="$4"
    bottom="$5"
    src="$tmp_dir/$stem-online"
    logo="$tmp_dir/$stem-online-logo.png"

    if ! command -v curl >/dev/null 2>&1; then
        return
    fi

    if ! curl -L -f -s -A "Mozilla/5.0" -o "$src" "$url"; then
        return
    fi

    magick "$src" -auto-orient -resize 140x80 -background none \
        -gravity center -extent 140x80 "$logo"

    magick -size 160x120 gradient:"$top-$bottom" "$logo" \
        -gravity center -composite \
        -colorspace sRGB -strip BMP3:"$out_dir/$stem.bmp"
    write_source "$stem" "online-logo" "$page"
}

generated_cover()
{
    stem="$1"
    title="$2"
    subtitle="$3"
    palette="$4"

    case "$palette" in
        puzzle)
            top="#f7f7f4"
            bottom="#a9acaf"
            accent="#666b70"
            fg="#171a1d"
            sub="#3f454c"
            ;;
        arcade)
            top="#222a33"
            bottom="#0b1018"
            accent="#d9dde4"
            fg="#f4f6f8"
            sub="#c1c7d0"
            ;;
        classic)
            top="#edf0f4"
            bottom="#8d939b"
            accent="#4e555d"
            fg="#111418"
            sub="#363c43"
            ;;
        *)
            top="#f2f3f5"
            bottom="#969da5"
            accent="#535a62"
            fg="#111418"
            sub="#363c43"
            ;;
    esac

    base="$tmp_dir/$stem-base.png"
    title_img="$tmp_dir/$stem-title.png"
    subtitle_img="$tmp_dir/$stem-subtitle.png"

    magick -size 160x120 gradient:"$top-$bottom" \
        -fill "rgba(255,255,255,0.40)" -draw "rectangle 0,0 160,38" \
        -fill "rgba(0,0,0,0.12)" -draw "rectangle 0,96 160,120" \
        -fill "$accent" -draw "rectangle 0,0 160,2 rectangle 0,118 160,120" \
        -fill "rgba(255,255,255,0.50)" -draw "rectangle 12,14 148,15" \
        -fill "rgba(0,0,0,0.18)" -draw "rectangle 12,92 148,93" \
        "$base"

    magick -background none -fill "$fg" -font "$font_bold" \
        -gravity center -size 136x58 caption:"$title" "$title_img"

    magick -background none -fill "$sub" -font "$font_regular" \
        -gravity center -size 140x22 caption:"$subtitle" "$subtitle_img"

    magick "$base" "$title_img" -gravity center -geometry +0-9 -composite \
        "$subtitle_img" -gravity south -geometry +0+14 -composite \
        -colorspace sRGB -strip BMP3:"$out_dir/$stem.bmp"
    write_source "$stem" "generated-typographic-cover" "$title / $subtitle"
}

reset_manifest

screenshot_cover "2048" "2048"
screenshot_cover "amaze" "amaze"
screenshot_cover "blackjack" "blackjack"
screenshot_cover "brickmania" "brickmania"
screenshot_cover "bubbles" "bubbles"
screenshot_cover "chessbox" "chessbox"
screenshot_cover "chopper" "chopper"
screenshot_cover "clix" "clix"
screenshot_cover "codebuster" "codebuster"
screenshot_cover "doom" "doom"
screenshot_cover "doom_play" "doom"
screenshot_cover "flipit" "flipit"
screenshot_cover "goban" "goban"
screenshot_cover "invadrox" "invadrox"
screenshot_cover "jackpot" "jackpot"
screenshot_cover "jewels" "jewels"
screenshot_cover "mazezam" "mazezam"
screenshot_cover "minesweeper" "minesweeper"
screenshot_cover "pacbox" "pacbox"
screenshot_cover "pegbox" "pegbox"
screenshot_cover "pong" "pong"
screenshot_cover "robotfindskitten" "robotfindskitten"
screenshot_cover "rockblox" "rockblox"
screenshot_cover "rockboy" "rockboy"
screenshot_cover "sliding_puzzle" "sliding"
screenshot_cover "snake" "snake"
screenshot_cover "snake2" "snake2"
screenshot_cover "sokoban" "sokoban"
screenshot_cover "solitaire" "solitaire"
screenshot_cover "spacerocks" "spacerocks"
screenshot_cover "star" "star"
screenshot_cover "sudoku" "sudoku"
screenshot_cover "wormlet" "wormlet"
screenshot_cover "xobox" "xobox"
screenshot_cover "xrick" "xrick"

file_cover "clubpenguin" "$repo_root/assets/ipodjs/rockbox/clubpenguin/covers/Club Penguin.bmp"
file_cover "runescape_classic" "$repo_root/assets/ipodjs/rockbox/runescape_classic/covers/RuneScape Classic.bmp"

generated_cover "arduboy" "Arduboy" "Game Launcher" "classic"
generated_cover "cdogs" "C-Dogs" "SDL Classic" "arcade"
generated_cover "dice" "Dice" "Rockbox Game" "classic"
generated_cover "maze" "Maze" "Rockbox Game" "classic"
generated_cover "pocketcatch" "PocketCatch" "Adventure" "classic"
generated_cover "pokemini" "Pokemon Mini" "Emulator" "classic"
generated_cover "pokemini_launcher" "Pokemon Mini" "Launcher" "classic"
generated_cover "reversi" "Reversi" "Board Game" "classic"
generated_cover "rockblox1d" "Rockblox 1D" "Rockbox Game" "arcade"
generated_cover "rockboy_launcher" "Game Cover Flow" "Rockbox Library" "classic"
generated_cover "runepod" "RunePod" "Adventure" "classic"
generated_cover "smsgg" "Sega Master System" "Game Gear" "classic"
generated_cover "superdom" "Superdom" "Strategy" "classic"
generated_cover "tamagotchi" "Tamagotchi" "Virtual Pet" "classic"
generated_cover "wwe_backstage" "WWE Backstage" "Survival" "arcade"
generated_cover "xworld" "XWorld" "Adventure" "arcade"

generated_cover "sgt-blackbox" "Black Box" "Puzzle Collection" "puzzle"
generated_cover "sgt-bridges" "Bridges" "Puzzle Collection" "puzzle"
screenshot_cover "sgt-cube" "puzzles-cube"
generated_cover "sgt-dominosa" "Dominosa" "Puzzle Collection" "puzzle"
generated_cover "sgt-fifteen" "Fifteen" "Puzzle Collection" "puzzle"
generated_cover "sgt-filling" "Filling" "Puzzle Collection" "puzzle"
generated_cover "sgt-flip" "Flip" "Puzzle Collection" "puzzle"
generated_cover "sgt-flood" "Flood" "Puzzle Collection" "puzzle"
generated_cover "sgt-galaxies" "Galaxies" "Puzzle Collection" "puzzle"
generated_cover "sgt-group" "Group" "Puzzle Collection" "puzzle"
generated_cover "sgt-guess" "Guess" "Puzzle Collection" "puzzle"
generated_cover "sgt-inertia" "Inertia" "Puzzle Collection" "puzzle"
generated_cover "sgt-keen" "Keen" "Puzzle Collection" "puzzle"
generated_cover "sgt-lightup" "Light Up" "Puzzle Collection" "puzzle"
generated_cover "sgt-loopy" "Loopy" "Puzzle Collection" "puzzle"
generated_cover "sgt-magnets" "Magnets" "Puzzle Collection" "puzzle"
screenshot_cover "sgt-map" "puzzles-map"
generated_cover "sgt-mines" "Mines" "Puzzle Collection" "puzzle"
screenshot_cover "sgt-mosaic" "mosaic"
generated_cover "sgt-net" "Net" "Puzzle Collection" "puzzle"
generated_cover "sgt-netslide" "Netslide" "Puzzle Collection" "puzzle"
generated_cover "sgt-palisade" "Palisade" "Puzzle Collection" "puzzle"
generated_cover "sgt-pattern" "Pattern" "Puzzle Collection" "puzzle"
generated_cover "sgt-pearl" "Pearl" "Puzzle Collection" "puzzle"
generated_cover "sgt-pegs" "Pegs" "Puzzle Collection" "puzzle"
generated_cover "sgt-range" "Range" "Puzzle Collection" "puzzle"
generated_cover "sgt-rect" "Rectangles" "Puzzle Collection" "puzzle"
generated_cover "sgt-samegame" "SameGame" "Puzzle Collection" "puzzle"
generated_cover "sgt-signpost" "Signpost" "Puzzle Collection" "puzzle"
generated_cover "sgt-singles" "Singles" "Puzzle Collection" "puzzle"
generated_cover "sgt-sixteen" "Sixteen" "Puzzle Collection" "puzzle"
generated_cover "sgt-slant" "Slant" "Puzzle Collection" "puzzle"
generated_cover "sgt-slide" "Slide" "Puzzle Collection" "puzzle"
generated_cover "sgt-sokoban" "Sokoban" "Puzzle Collection" "puzzle"
generated_cover "sgt-solo" "Solo" "Puzzle Collection" "puzzle"
generated_cover "sgt-tents" "Tents" "Puzzle Collection" "puzzle"
generated_cover "sgt-towers" "Towers" "Puzzle Collection" "puzzle"
generated_cover "sgt-tracks" "Tracks" "Puzzle Collection" "puzzle"
generated_cover "sgt-twiddle" "Twiddle" "Puzzle Collection" "puzzle"
generated_cover "sgt-undead" "Undead" "Puzzle Collection" "puzzle"
generated_cover "sgt-unequal" "Unequal" "Puzzle Collection" "puzzle"
generated_cover "sgt-unruly" "Unruly" "Puzzle Collection" "puzzle"
generated_cover "sgt-untangle" "Untangle" "Puzzle Collection" "puzzle"

if [ "${COVER_FETCH_ONLINE:-1}" != "0" ]; then
    online_cover "doom" \
        "https://upload.wikimedia.org/wikipedia/en/5/57/Doom_cover_art.jpg" \
        "https://en.wikipedia.org/wiki/Doom_(1993_video_game)"
    cp "$out_dir/doom.bmp" "$out_dir/doom_play.bmp"
    write_source "doom_play" "online-cover" \
        "https://en.wikipedia.org/wiki/Doom_(1993_video_game)"

    online_logo_cover "clubpenguin" \
        "https://upload.wikimedia.org/wikipedia/commons/7/78/Club_Penguin_Logo_2012_-_2017.png" \
        "https://en.wikipedia.org/wiki/Club_Penguin" "#ecf7ff" "#6aaed6"
fi

count="$(find "$out_dir" -maxdepth 1 -type f -name '*.bmp' | wc -l)"
echo "Generated $count native game covers in $out_dir"
