#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:-build-sim-ipod6g}"
out_dir="${2:-docs/sbs-miniplayer-shots/ipod6g}"
theme="${3:-iPone}"
track="${SBS_TEST_TRACK:-/Music/Electric Jewels - April Wine/01 - April Wine - Weeping Widow.flac}"
no_playback="${SBS_NO_PLAYBACK:-0}"
start_screen="${SBS_START_SCREEN:-wps}"
idle_slideshow="${SBS_IDLE_SLIDESHOW:-0}"
play_key="${SBS_PLAY_KEY:-KP_Add}"
right_pane="${SBS_RIGHT_PANE:-}"
if [ "${no_playback}" = "1" ] && [ -z "${SBS_START_SCREEN:-}" ]; then
    start_screen="root"
fi
if [ -n "${SBS_MENU_KEY:-}" ]; then
    menu_key="${SBS_MENU_KEY}"
else
    menu_key="w"
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_abs="$(cd "${repo_root}/${build_dir}" && pwd)"
source_sim_root="${build_abs}/simdisk"
rockboxui="${build_abs}/rockboxui"
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

copy_if_exists()
{
    local src="$1"
    local dst="$2"
    if [ -e "${src}" ]; then
        mkdir -p "$(dirname "${dst}")"
        if [ -d "${src}" ]; then
            rm -rf "${dst}"
            cp -a "${src}" "${dst}"
        else
            cp -a "${src}" "${dst}"
        fi
    fi
}

cleanup()
{
    set +e
    if [ -n "${sim_pid}" ] && kill -0 "${sim_pid}" >/dev/null 2>&1; then
        kill "${sim_pid}" >/dev/null 2>&1 || true
        wait "${sim_pid}" 2>/dev/null || true
    fi
    if [ "${SBS_CAPTURE_KEEP_ROOT:-0}" = "1" ]; then
        printf "preserved simulator root: %s\n" "${sim_root:-}"
        printf "preserved temp dir: %s\n" "${tmp_dir:-}"
        return
    fi
    [ -n "${sim_root}" ] && [ -d "${sim_root}" ] && rm -rf "${sim_root}"
    [ -n "${tmp_dir}" ] && [ -d "${tmp_dir}" ] && rm -rf "${tmp_dir}"
}

stop_sim()
{
    if [ -n "${sim_pid}" ] && kill -0 "${sim_pid}" >/dev/null 2>&1; then
        kill "${sim_pid}" >/dev/null 2>&1 || true
        wait "${sim_pid}" 2>/dev/null || true
    fi
    sim_pid=""
    sim_wid=""
}

copy_font_dir()
{
    local src="$1"
    if [ -d "${src}" ]; then
        cp -a "${src}/." "${sim_root}/.rockbox/fonts/"
    fi
}

prepare_root()
{
    sim_root="$(mktemp -d)"
    tmp_dir="$(mktemp -d)"
    cp -a "${source_sim_root}/.rockbox" "${sim_root}/.rockbox"

    local track_dir track_parent source_track_dir
    track_dir="$(dirname "${track}")"
    track_parent="$(dirname "${track_dir}")"
    source_track_dir="${source_sim_root}${track_dir}"
    if [ -n "${SBS_MUSIC_ROOT:-}" ] && [ -d "${SBS_MUSIC_ROOT}" ]; then
        ln -s "${SBS_MUSIC_ROOT}" "${sim_root}/Music"
    elif [ -d "${source_track_dir}" ]; then
        mkdir -p "${sim_root}${track_parent}"
        ln -s "${source_track_dir}" "${sim_root}${track_dir}"
    elif [ -e "${source_sim_root}/Music" ]; then
        ln -s "${source_sim_root}/Music" "${sim_root}/Music"
    fi

    mkdir -p "${sim_root}/.rockbox/wps" \
             "${sim_root}/.rockbox/themes" \
             "${sim_root}/.rockbox/backdrops" \
             "${sim_root}/.rockbox/icons" \
             "${sim_root}/.rockbox/fonts"

    if [ -n "${SBS_ALBUMLIST_SOURCE:-}" ] && [ -d "${SBS_ALBUMLIST_SOURCE}" ]; then
        rm -rf "${sim_root}/.rockbox/albumlist"
        cp -a "${SBS_ALBUMLIST_SOURCE}" "${sim_root}/.rockbox/albumlist"
    fi

    copy_if_exists "${repo_root}/themes/${theme}.cfg" "${sim_root}/.rockbox/themes/${theme}.cfg"
    copy_if_exists "${repo_root}/wps/${theme}.sbs" "${sim_root}/.rockbox/wps/${theme}.sbs"
    copy_if_exists "${repo_root}/wps/${theme}.wps" "${sim_root}/.rockbox/wps/${theme}.wps"
    copy_if_exists "${repo_root}/wps/${theme}.fms" "${sim_root}/.rockbox/wps/${theme}.fms"
    copy_if_exists "${repo_root}/wps/${theme}" "${sim_root}/.rockbox/wps/${theme}"
    if [ -f "${repo_root}/themes/${theme}.cfg" ]; then
        local configured_sbs configured_sbs_rel configured_sbs_dir
        configured_sbs="$(awk -F: 'tolower($1) == "sbs" { sub(/^[[:space:]]+/, "", $2); print $2; exit }' "${repo_root}/themes/${theme}.cfg")"
        configured_sbs_rel="${configured_sbs#/.rockbox/wps/}"
        configured_sbs_rel="${configured_sbs_rel#.rockbox/wps/}"
        if [ -n "${configured_sbs_rel}" ] && [ "${configured_sbs_rel}" != "${configured_sbs}" ]; then
            copy_if_exists "${repo_root}/wps/${configured_sbs_rel}" "${sim_root}/.rockbox/wps/${configured_sbs_rel}"
            configured_sbs_dir="${configured_sbs_rel%.sbs}"
            copy_if_exists "${repo_root}/wps/${configured_sbs_dir}" "${sim_root}/.rockbox/wps/${configured_sbs_dir}"
        fi
    fi

    if [ "${theme}" = "iPone7G" ]; then
        copy_if_exists "${repo_root}/wps/iPone.wps" "${sim_root}/.rockbox/wps/iPone.wps"
        copy_if_exists "${repo_root}/wps/iPone.fms" "${sim_root}/.rockbox/wps/iPone.fms"
        copy_if_exists "${repo_root}/wps/iPone" "${sim_root}/.rockbox/wps/iPone"
        copy_if_exists "${repo_root}/icons/iPone.bmp" "${sim_root}/.rockbox/icons/iPone.bmp"
    fi

    copy_if_exists "${repo_root}/backdrops/${theme}_bd.bmp" "${sim_root}/.rockbox/backdrops/${theme}_bd.bmp"
    copy_if_exists "${repo_root}/icons/${theme}.bmp" "${sim_root}/.rockbox/icons/${theme}.bmp"

    copy_font_dir "${source_sim_root}/.rockbox/fonts"
    copy_font_dir "${repo_root}/build-sim-video-5g/simdisk/.rockbox/fonts"
    copy_font_dir "${repo_root}/rockpod/.theme_designer/simulator/ipod-320x240/build-sim-video-5g/simdisk/.rockbox/fonts"
    copy_font_dir "${repo_root}/fonts"

    if [ -f "${repo_root}/themes/${theme}.cfg" ]; then
        cp "${repo_root}/themes/${theme}.cfg" "${sim_root}/.rockbox/config.cfg"
    else
        cat >"${sim_root}/.rockbox/config.cfg" <<EOF
wps: /.rockbox/wps/${theme}.wps
sbs: /.rockbox/wps/${theme}.sbs
fms: /.rockbox/wps/${theme}.fms
statusbar: off
album art: prefer image file
EOF
    fi

    awk '
        BEGIN { start = 0; repeat = 0; aa = 0; pane = 0 }
        /^start in screen:/ { print "start in screen: " start_screen; start = 1; next }
        /^repeat:/ { print "repeat: all"; repeat = 1; next }
        /^album art:/ { print "album art: prefer image file"; aa = 1; next }
        /^ipone right pane:/ {
            if (right_pane != "") {
                print "ipone right pane: " right_pane
                pane = 1
                next
            }
        }
        { print }
        END {
            if (!start) print "start in screen: " start_screen
            if (!repeat) print "repeat: all"
            if (!aa) print "album art: prefer image file"
            if (right_pane != "" && !pane) print "ipone right pane: " right_pane
        }
    ' start_screen="${start_screen}" right_pane="${right_pane}" "${sim_root}/.rockbox/config.cfg" >"${tmp_dir}/config.cfg"
    cp "${tmp_dir}/config.cfg" "${sim_root}/.rockbox/config.cfg"

    if [ "${no_playback}" != "1" ]; then
        printf "P:6::\nA:0:0:%s\n" "${track}" >"${sim_root}/.rockbox/.playlist_control"
        cat >"${sim_root}/.rockbox/.resume.cfg" <<'EOF'
volume: 6
pitch: 10000
speed: 10000
IDX: 0
CRC: 0
ELA: 0
OFF: 0
PLM: 0
CRT: 1
TRT: 1
PVS: -1
PFQ: 0
EOF
        cp "${sim_root}/.rockbox/.resume.cfg" "${sim_root}/.rockbox/.resume.cfg.new"
    else
        rm -f "${sim_root}/.rockbox/.playlist_control" \
              "${sim_root}/.rockbox/.resume.cfg" \
              "${sim_root}/.rockbox/.resume.cfg.new"
    fi
}

launch_sim()
{
    mkdir -p "${out_dir}"
    pushd "${repo_root}" >/dev/null
    env -u SBS_NO_PLAYBACK \
        -u SBS_START_SCREEN \
        -u SBS_IDLE_SLIDESHOW \
        -u SBS_PLAY_KEY \
        -u SBS_MENU_KEY \
        -u SBS_RIGHT_PANE \
        -u SBS_TEST_TRACK \
        -u SBS_MUSIC_ROOT \
        -u SBS_ALBUMLIST_SOURCE \
        -u SBS_PRE_CAPTURE_KEYS \
        -u SBS_SLIDESHOW_WAIT \
        -u SBS_HOLD_WAIT \
        -u SBS_CAPTURE_KEEP_ROOT \
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
    local filename="$1"
    import -window "${sim_wid}" "${out_dir}/${filename}"
}

record_window()
{
    local filename="$1"
    local seconds="${2:-6}"
    local geometry width height x y

    require_cmd ffmpeg
    geometry="$(xdotool getwindowgeometry --shell "${sim_wid}")"
    width="$(printf "%s\n" "${geometry}" | awk -F= '/^WIDTH=/{print $2}')"
    height="$(printf "%s\n" "${geometry}" | awk -F= '/^HEIGHT=/{print $2}')"
    x="$(printf "%s\n" "${geometry}" | awk -F= '/^X=/{print $2}')"
    y="$(printf "%s\n" "${geometry}" | awk -F= '/^Y=/{print $2}')"

    ffmpeg -y -hide_banner -loglevel error \
        -f x11grab -framerate "${SBS_RECORD_FPS:-30}" \
        -video_size "${width}x${height}" \
        -i "${DISPLAY:-:0}+${x},${y}" \
        -t "${seconds}" "${out_dir}/${filename}"
}

profile_recording()
{
    local filename="$1"
    local report="$2"
    local profile_python="python3"
    if [ -x "${repo_root}/rockpod/.venv/bin/python" ]; then
        profile_python="${repo_root}/rockpod/.venv/bin/python"
    fi
    if [ -f "${out_dir}/${filename}" ] && command -v "${profile_python}" >/dev/null 2>&1; then
        "${profile_python}" "${repo_root}/tools/slideshow_motion_profile.py" \
            "${out_dir}/${filename}" "${out_dir}/${report}" || true
    fi
}

press_key()
{
    local key="$1"
    xdotool keydown --window "${sim_wid}" "${key}"
    sleep 0.2
    xdotool keyup --window "${sim_wid}" "${key}"
}

main()
{
    require_cmd xdotool
    require_cmd import
    require_cmd magick
    [ -x "${rockboxui}" ] || { printf "missing simulator: %s\n" "${rockboxui}" >&2; exit 1; }
    [ -d "${source_sim_root}/.rockbox" ] || { printf "missing simdisk: %s\n" "${source_sim_root}" >&2; exit 1; }

    trap cleanup EXIT INT TERM HUP
    prepare_root
    rm -f "${out_dir}"/*.png "${out_dir}/analysis.txt"
    launch_sim

    if [ "${idle_slideshow}" = "1" ]; then
        press_key "${play_key}"
        sleep 1
        press_key "${menu_key}"
        sleep 1

        for key in ${SBS_PRE_CAPTURE_KEYS:-}; do
            press_key "${key}"
            sleep 0.4
        done

        sleep 1
        if [ -n "${SBS_RECORD_SECONDS:-}" ]; then
            record_window "slideshow-glide.mp4" "${SBS_RECORD_SECONDS}"
            profile_recording "slideshow-glide.mp4" "motion-report.txt"
        fi
        capture_window "00-sbs-slideshow-a.png"
        sleep "${SBS_SLIDESHOW_WAIT:-4}"
        capture_window "01-sbs-slideshow-b.png"

        magick "${out_dir}/00-sbs-slideshow-a.png" -crop 320x480+320+0 +repage \
            "${out_dir}/00-sbs-slideshow-art-crop.png"
        magick "${out_dir}/01-sbs-slideshow-b.png" -crop 320x480+320+0 +repage \
            "${out_dir}/01-sbs-slideshow-art-crop.png"

        {
            printf "build_dir=%s\n" "${build_dir}"
            printf "theme=%s\n" "${theme}"
            printf "idle_slideshow=1\n"
            printf "track=%s\n" "${track}"
            printf "menu_key=%s\n" "${menu_key}"
            printf "play_key=%s\n" "${play_key}"
            printf "root=%s\n" "${sim_root}"
            if [ -n "${SBS_RECORD_SECONDS:-}" ]; then
                printf "recording=%s\n" "${out_dir}/slideshow-glide.mp4"
                [ -f "${out_dir}/motion-report.txt" ] && cat "${out_dir}/motion-report.txt"
            fi
            printf "slideshow_art_a_unique_colors="
            magick "${out_dir}/00-sbs-slideshow-art-crop.png" -format "%k" info:
            printf "\nslideshow_art_b_unique_colors="
            magick "${out_dir}/01-sbs-slideshow-art-crop.png" -format "%k" info:
            printf "\nslideshow_art_a_sha256="
            sha256sum "${out_dir}/00-sbs-slideshow-art-crop.png" | awk '{print $1}'
            printf "slideshow_art_b_sha256="
            sha256sum "${out_dir}/01-sbs-slideshow-art-crop.png" | awk '{print $1}'
        } >"${out_dir}/analysis.txt"

        printf "saved screenshots to %s\n" "${out_dir}"
        cat "${out_dir}/analysis.txt"
        return
    fi

    if [ "${no_playback}" = "1" ]; then
        for key in ${SBS_PRE_CAPTURE_KEYS:-Down}; do
            press_key "${key}"
            sleep 0.4
        done

        sleep 1
        if [ -n "${SBS_RECORD_SECONDS:-}" ]; then
            record_window "slideshow-glide.mp4" "${SBS_RECORD_SECONDS}"
            profile_recording "slideshow-glide.mp4" "motion-report.txt"
        fi
        capture_window "00-sbs-slideshow-a.png"
        sleep "${SBS_SLIDESHOW_WAIT:-4}"
        capture_window "01-sbs-slideshow-b.png"

        magick "${out_dir}/00-sbs-slideshow-a.png" -crop 320x480+320+0 +repage \
            "${out_dir}/00-sbs-slideshow-art-crop.png"
        magick "${out_dir}/01-sbs-slideshow-b.png" -crop 320x480+320+0 +repage \
            "${out_dir}/01-sbs-slideshow-art-crop.png"

        {
            printf "build_dir=%s\n" "${build_dir}"
            printf "theme=%s\n" "${theme}"
            printf "no_playback=1\n"
            printf "start_screen=%s\n" "${start_screen}"
            printf "pre_capture_keys=%s\n" "${SBS_PRE_CAPTURE_KEYS:-Down}"
            printf "root=%s\n" "${sim_root}"
            if [ -n "${SBS_RECORD_SECONDS:-}" ]; then
                printf "recording=%s\n" "${out_dir}/slideshow-glide.mp4"
                [ -f "${out_dir}/motion-report.txt" ] && cat "${out_dir}/motion-report.txt"
            fi
            printf "slideshow_art_a_unique_colors="
            magick "${out_dir}/00-sbs-slideshow-art-crop.png" -format "%k" info:
            printf "\nslideshow_art_b_unique_colors="
            magick "${out_dir}/01-sbs-slideshow-art-crop.png" -format "%k" info:
            printf "\nslideshow_art_a_sha256="
            sha256sum "${out_dir}/00-sbs-slideshow-art-crop.png" | awk '{print $1}'
            printf "slideshow_art_b_sha256="
            sha256sum "${out_dir}/01-sbs-slideshow-art-crop.png" | awk '{print $1}'
        } >"${out_dir}/analysis.txt"

        printf "saved screenshots to %s\n" "${out_dir}"
        cat "${out_dir}/analysis.txt"
        return
    fi

    # This gate is intentionally SBS-only. Most captures resume playback in
    # WPS, then press the iPod MENU button into the SBS/root screen. Tests can
    # start directly in root and skip this key to avoid target-specific keymap
    # ambiguity.
    if [ "${SBS_SKIP_MENU_KEY:-0}" != "1" ]; then
        press_key "${menu_key}"
        sleep 1
    fi
    capture_window "00-sbs-mini-player.png"
    if [ "${SBS_SECOND_CAPTURE:-0}" = "1" ]; then
        sleep "${SBS_SLIDESHOW_WAIT:-4}"
        capture_window "00-sbs-mini-player-b.png"
        magick "${out_dir}/00-sbs-mini-player.png" -crop 320x480+320+0 +repage \
            "${out_dir}/00-sbs-right-pane-a.png"
        magick "${out_dir}/00-sbs-mini-player-b.png" -crop 320x480+320+0 +repage \
            "${out_dir}/00-sbs-right-pane-b.png"
    fi

    press_key h
    sleep "${SBS_HOLD_WAIT:-1}"
    capture_window "01-sbs-lockscreen-from-menu.png"

    magick "${out_dir}/00-sbs-mini-player.png" -crop 128x128+356+66 +repage \
        "${out_dir}/00-sbs-mini-art-crop.png"
    magick "${out_dir}/01-sbs-lockscreen-from-menu.png" -crop 51x51+56+322 +repage \
        "${out_dir}/01-sbs-lock-art-crop.png"
    magick "${out_dir}/01-sbs-lockscreen-from-menu.png" -crop 320x216+0+48 +repage \
        "${out_dir}/01-sbs-lock-ui-crop.png"

    {
        printf "build_dir=%s\n" "${build_dir}"
        printf "theme=%s\n" "${theme}"
        printf "track=%s\n" "${track}"
        printf "menu_key=%s\n" "${menu_key}"
        printf "root=%s\n" "${sim_root}"
        printf "sbs_mini_art_unique_colors="
        magick "${out_dir}/00-sbs-mini-art-crop.png" -format "%k" info:
        printf "\nsbs_lock_art_unique_colors="
        magick "${out_dir}/01-sbs-lock-art-crop.png" -format "%k" info:
        printf "\nsbs_lock_ui_unique_colors="
        magick "${out_dir}/01-sbs-lock-ui-crop.png" -format "%k" info:
        if [ "${SBS_SECOND_CAPTURE:-0}" = "1" ]; then
            printf "\nsbs_right_pane_a_sha256="
            sha256sum "${out_dir}/00-sbs-right-pane-a.png" | awk '{print $1}'
            printf "sbs_right_pane_b_sha256="
            sha256sum "${out_dir}/00-sbs-right-pane-b.png" | awk '{print $1}'
        fi
        printf "\n"
    } >"${out_dir}/analysis.txt"

    printf "saved screenshots to %s\n" "${out_dir}"
    cat "${out_dir}/analysis.txt"
}

main "$@"
