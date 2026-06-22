#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:-/home/david/Documents/RockBox_Personal-master/build-sim-video-5g}"
out_dir="${2:-/home/david/Documents/RockBox_Personal-master/docs/album-list-layout-shots}"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_abs="$(cd "${repo_root}/${build_dir}" 2>/dev/null || cd "${build_dir}" && pwd)"
source_sim_root="${build_abs}/simdisk"
rockboxui="${build_abs}/rockboxui"
laptop_music_dir="${IPONE_ALBUMLIST_MUSIC_DIR:-/home/david/Music}"
album_list_layout="${IPONE_ALBUMLIST_LAYOUT:-full}"
enter_key="${IPONE_ALBUMLIST_ENTER_KEY:-KP_5}"
back_key="${IPONE_ALBUMLIST_BACK_KEY:-Left}"
sim_root=""
tmp_dir=""
sim_pid=""
sim_wid=""

require_cmd()
{
    if ! command -v "$1" >/dev/null 2>&1; then
        printf "missing required command: %s\n" "$1" >&2
        exit 1
    fi
}

require_file()
{
    if [ ! -f "$1" ]; then
        printf "missing required file: %s\n" "$1" >&2
        exit 1
    fi
}

install_current_theme_sources()
{
    local rb_cfg_dir="${sim_root}/.rockbox"

    for skin_file in iPone.sbs iPone.wps iPone.fms; do
        if [ -f "${repo_root}/wps/${skin_file}" ]; then
            cp "${repo_root}/wps/${skin_file}" "${rb_cfg_dir}/wps/${skin_file}"
        fi
    done

    if [ -f "${repo_root}/themes/iPone.cfg" ]; then
        cp "${repo_root}/themes/iPone.cfg" "${rb_cfg_dir}/themes/iPone.cfg"
    fi

    if [ -d "${repo_root}/wps/iPone" ]; then
        rm -rf "${rb_cfg_dir}/wps/iPone"
        cp -a "${repo_root}/wps/iPone" "${rb_cfg_dir}/wps/iPone"
    fi
}

seed_albumlist_proof_art()
{
    local album_dir="${sim_root}/.rockbox/albumlist"
    local thumb_dir="${album_dir}/thumbs"

    mkdir -p "${thumb_dir}"

    cat >"${album_dir}/index.tsv" <<'EOF'
# rockpod albumlist v1
album_id	thumb	artist	album	group_key	device_dirs
EOF

    add_album_thumb "4" "4 - Foreigner/cover.jpg" "proof-4.bmp"
    add_album_thumb "7 Years" "7 Years - Lukas Graham/cover.jpg" "proof-7-years.bmp"
    add_album_thumb "A Hard Day's Night" "A Hard Day's Night/cover.jpg" "proof-hard-days-night.bmp"
    add_album_thumb "(Don't Mess With The ) Time Man" "Halestorm - (Don't Mess With The ) Time Man (2000)/cover.jpg" "proof-time-man-full.bmp"
    add_album_thumb "Time Man" "Halestorm - (Don't Mess With The ) Time Man (2000)/cover.jpg" "proof-time-man.bmp"
    add_album_thumb "A Toot and a Snore in '74" "A Toot In an Snore in 74'/cover.jpg" "proof-toot-74.bmp"
    add_album_thumb "A Momentary Lapse of Reason" "A Momentary Lapse of Reason - Pink Floyd/cover.jpg" "proof-momentary-lapse.bmp"
    add_album_thumb "A Question Of Balance" "A Question Of Balance - The Moody Blues/cover.jpg" "proof-question-balance.bmp"
    add_album_thumb "Abbey Road (Remastered)" "Abbey Road (Remastered) - The Beatles/cover.jpg" "proof-abbey-road.bmp"
    add_album_thumb "All the Right Reasons" "All the Right Reasons - Nickelback/cover.jpg" "proof-all-right-reasons.bmp"
    add_album_thumb "All Things Must Pass (Remastered 2014)" "All Things Must Pass (Remastered 2014) - George Harrison/cover.jpg" "proof-all-things.bmp"
    add_album_thumb "Random Access Memories" "Random Access Memories - Daft Punk/cover.jpg" "proof-random-access-memories.bmp"
}

add_album_thumb()
{
    local album="$1"
    local cover_rel="$2"
    local bmp_name="$3"
    local src="${laptop_music_dir}/${cover_rel}"
    local album_dir="${sim_root}/.rockbox/albumlist"
    local dst="${album_dir}/thumbs/${bmp_name}"

    if [ ! -f "${src}" ]; then
        return
    fi

    magick "${src}" \
        -auto-orient \
        -thumbnail 40x40 \
        -background black \
        -alpha remove \
        -alpha off \
        -gravity center \
        -extent 40x40 \
        "BMP3:${dst}"

    printf '%s\t%s\t\t%s\t%s\t\n' \
        "${bmp_name%.bmp}" "thumbs/${bmp_name}" "${album}" "${album}" \
        >>"${album_dir}/index.tsv"
}

prepare_runtime_root()
{
    sim_root="$(mktemp -d)"
    cp -a "${source_sim_root}/.rockbox" "${sim_root}/.rockbox"
    cp "${repo_root}/apps/tagnavi.config" "${sim_root}/.rockbox/tagnavi.config"

    for dir_name in Playlists Podcasts Recordings; do
        if [ -e "${source_sim_root}/${dir_name}" ]; then
            ln -s "${source_sim_root}/${dir_name}" "${sim_root}/${dir_name}"
        fi
    done
    if [ -e "${sim_root}/Music" ] || [ -L "${sim_root}/Music" ]; then
        rm -rf "${sim_root}/Music"
    fi
    if [ -d "${laptop_music_dir}" ]; then
        ln -s "${laptop_music_dir}" "${sim_root}/Music"
    fi

    install_current_theme_sources
    seed_albumlist_proof_art

    if [ ! -f "${sim_root}/.rockbox/config.cfg" ]; then
        if [ -f "${sim_root}/.rockbox/themes/iPone.cfg" ]; then
            cp "${sim_root}/.rockbox/themes/iPone.cfg" \
               "${sim_root}/.rockbox/config.cfg"
        else
            cat >"${sim_root}/.rockbox/config.cfg" <<'EOF'
wps: /.rockbox/wps/iPone.wps
sbs: /.rockbox/wps/iPone.sbs
fms: /.rockbox/wps/iPone.fms
statusbar: off
album art: prefer image file
EOF
        fi
    fi

    awk '
        BEGIN { start_replaced = 0; icons_replaced = 0; layout_replaced = 0 }
        /^start in screen:/ {
            print "start in screen: db"
            start_replaced = 1
            next
        }
        /^show icons:/ {
            print "show icons: on"
            icons_replaced = 1
            next
        }
        /^album list layout:/ {
            print "album list layout: " album_list_layout
            layout_replaced = 1
            next
        }
        { print }
        END {
            if (!start_replaced)
                print "start in screen: db"
            if (!icons_replaced)
                print "show icons: on"
            if (!layout_replaced)
                print "album list layout: " album_list_layout
        }
    ' album_list_layout="${album_list_layout}" "${sim_root}/.rockbox/config.cfg" >"${tmp_dir}/config.cfg"
    cp "${tmp_dir}/config.cfg" "${sim_root}/.rockbox/config.cfg"
}

launch_sim()
{
    pushd "${repo_root}" >/dev/null
    "${rockboxui}" --zoom 2 --nobackground --root "${sim_root}" &
    sim_pid=$!
    popd >/dev/null

    sleep 2
    local win_ids
    win_ids="$(xdotool search --pid "${sim_pid}" || true)"
    sim_wid="${win_ids%%$'\n'*}"
    if [ -z "${sim_wid}" ]; then
        printf "unable to find simulator window for pid %s\n" "${sim_pid}" >&2
        exit 1
    fi
    xdotool windowactivate "${sim_wid}"
    sleep 1
}

capture_window()
{
    import -window "${sim_wid}" "${out_dir}/$1"
}

tap_key()
{
    local key="$1"

    xdotool windowactivate --sync "${sim_wid}"
    sleep 0.1
    xdotool key --clearmodifiers "${key}"
    sleep 1
}

cleanup()
{
    set +e
    if [ -n "${sim_pid:-}" ] && kill -0 "${sim_pid}" >/dev/null 2>&1; then
        kill "${sim_pid}" >/dev/null 2>&1 || true
        wait "${sim_pid}" 2>/dev/null || true
    fi
    if [ "${IPONE_ALBUMLIST_KEEP_ROOT:-0}" = "1" ]; then
        printf "preserved simulator root: %s\n" "${sim_root:-}"
        printf "preserved temp dir: %s\n" "${tmp_dir:-}"
        return
    fi
    if [ -n "${sim_root:-}" ] && [ -d "${sim_root}" ]; then
        rm -rf "${sim_root}"
    fi
    if [ -n "${tmp_dir:-}" ] && [ -d "${tmp_dir}" ]; then
        rm -rf "${tmp_dir}"
    fi
}

main()
{
    require_cmd xdotool
    require_cmd import
    require_cmd magick
    require_file "${rockboxui}"

    if [ ! -d "${source_sim_root}/.rockbox" ]; then
        printf "missing simulator root: %s\n" "${source_sim_root}" >&2
        exit 1
    fi

    mkdir -p "${out_dir}"
    rm -f "${out_dir}"/*.png "${out_dir}/layout-report.txt"
    tmp_dir="$(mktemp -d)"
    trap cleanup EXIT INT TERM HUP

    prepare_runtime_root
    launch_sim

    capture_window "00-database-root.png"
    tap_key Down
    capture_window "01-root-after-one-down.png"
    tap_key Down
    capture_window "02-root-after-two-down-album-selected.png"
    tap_key "${enter_key}"
    sleep 2
    capture_window "03-albums-list.png"
    tap_key "${back_key}"
    capture_window "04-root-after-albums-back.png"

    cat >"${out_dir}/layout-report.txt" <<EOF
Reference target: stock iPod classic 7G album list is a 320x240 full-width
album-cover list with a thin top title/status area, single horizontal
selection bar, and square cover thumbnails at the left of album rows. Colors
are intentionally not compared against stock; this repo keeps the
iPone/Rockbox theme colors.

Captured:
- 00-database-root.png: non-album database root, used to verify normal list
  layout remains scoped outside album rows.
- 01-root-after-one-down.png: database root after one scroll step.
- 02-root-after-two-down-album-selected.png: database root with Albums selected.
- 03-albums-list.png: Database -> Albums, using the laptop music folder and
  real cover.jpg files converted to 40x40 album-list BMPs.
- 04-root-after-albums-back.png: returned from Albums to the normal database
  root, used to verify the themed pane and selector are restored.

Expected after implementation:
- selected album list layout: ${album_list_layout};
- full mode uses the full 320x240 screen without a pinned title row;
- full mode album rows show 40x40 cover thumbnails in 44px rows;
- compact mode keeps the active theme/list viewport with small album art in
  normal compact rows;
- non-album database root keeps the normal iPone right-side pane;
- selector/theme colors remain those from the current iPone config.
EOF

    printf "saved album-list screenshots to %s\n" "${out_dir}"
}

main "$@"
