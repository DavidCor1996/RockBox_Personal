#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-${THEME_CAPTURE_BUILD_DIR:-${repo_root}/build-sim-video-5g}}"
theme="${THEME_CAPTURE_THEME:-iPone}"
out_dir="${2:-${THEME_CAPTURE_OUT_DIR:-${repo_root}/docs/theme-regression-shots/${theme}}}"
start_screen="${THEME_CAPTURE_START_SCREEN:-wps}"
source_sim_root="${build_dir}/simdisk"
sim_root=""

normal_track="${THEME_CAPTURE_NORMAL_TRACK:-${NORMAL_TRACK:-/Music/Electric Jewels - April Wine/01 - April Wine - Weeping Widow.flac}}"
long_track="${THEME_CAPTURE_LONG_TRACK:-${LONG_TRACK:-/Music/Abbey Road (Remastered) - The Beatles/13 - The Beatles - She Came In Through The Bathroom Window (Remastered 2009).flac}}"
no_art_track="${THEME_CAPTURE_NO_ART_TRACK:-${NO_ART_TRACK:-/Music/The Beatles (White Album)/The Beatles (White Album) [Disc 1]/01 Back In The U.S.S.R..mp3}}"

rockboxui="${build_dir}/rockboxui"
rb_cfg_dir=""
playlist_file=""
resume_file=""
resume_new_file=""
config_file=""
tmp_dir=""
sim_pid=""
sim_wid=""

set_runtime_paths()
{
    rb_cfg_dir="${sim_root}/.rockbox"
    playlist_file="${rb_cfg_dir}/.playlist_control"
    resume_file="${rb_cfg_dir}/.resume.cfg"
    resume_new_file="${rb_cfg_dir}/.resume.cfg.new"
    config_file="${rb_cfg_dir}/config.cfg"
}

copy_if_exists()
{
    local src="$1"
    local dst="$2"
    if [ -d "${src}" ]; then
        rm -rf "${dst}"
        mkdir -p "$(dirname "${dst}")"
        cp -a "${src}" "${dst}"
    elif [ -f "${src}" ]; then
        mkdir -p "$(dirname "${dst}")"
        cp "${src}" "${dst}"
    fi
}

copy_font_dir()
{
    local src="$1"
    if [ -d "${src}" ]; then
        mkdir -p "${rb_cfg_dir}/fonts"
        cp -a "${src}/." "${rb_cfg_dir}/fonts/"
    fi
}

configured_skin_rel()
{
    local key="$1"
    local cfg="${repo_root}/themes/${theme}.cfg"
    if [ ! -f "${cfg}" ]; then
        return 0
    fi
    awk -F: -v wanted="${key}" '
        tolower($1) == wanted {
            sub(/^[[:space:]]+/, "", $2)
            sub(/[[:space:]]+$/, "", $2)
            print $2
            exit
        }
    ' "${cfg}" | sed 's#^/.rockbox/##; s#^\.rockbox/##'
}

install_current_theme_sources()
{
    mkdir -p "${rb_cfg_dir}/themes" "${rb_cfg_dir}/wps" \
             "${rb_cfg_dir}/backdrops" "${rb_cfg_dir}/icons"

    copy_if_exists "${repo_root}/themes/${theme}.cfg" "${rb_cfg_dir}/themes/${theme}.cfg"
    if [ -f "${rb_cfg_dir}/themes/${theme}.cfg" ]; then
        cp "${rb_cfg_dir}/themes/${theme}.cfg" "${config_file}"
    fi

    for skin_file in "${theme}.sbs" "${theme}.wps" "${theme}.fms"; do
        copy_if_exists "${repo_root}/wps/${skin_file}" "${rb_cfg_dir}/wps/${skin_file}"
    done
    copy_if_exists "${repo_root}/wps/${theme}" "${rb_cfg_dir}/wps/${theme}"
    copy_if_exists "${repo_root}/backdrops/${theme}_bd.bmp" "${rb_cfg_dir}/backdrops/${theme}_bd.bmp"
    copy_if_exists "${repo_root}/icons/${theme}.bmp" "${rb_cfg_dir}/icons/${theme}.bmp"

    for skin_key in wps sbs fms; do
        local rel
        rel="$(configured_skin_rel "${skin_key}")"
        if [ -n "${rel}" ]; then
            copy_if_exists "${repo_root}/${rel}" "${rb_cfg_dir}/${rel}"
            if [[ "${rel}" == wps/*.sbs || "${rel}" == wps/*.wps || "${rel}" == wps/*.fms ]]; then
                copy_if_exists "${repo_root}/${rel%.*}" "${rb_cfg_dir}/${rel%.*}"
            fi
        fi
    done

    copy_font_dir "${source_sim_root}/.rockbox/fonts"
    copy_font_dir "${repo_root}/build-sim-video-5g/simdisk/.rockbox/fonts"
    copy_font_dir "${repo_root}/rockpod/.theme_designer/simulator/ipod-320x240/build-sim-video-5g/simdisk/.rockbox/fonts"
    copy_font_dir "${repo_root}/fonts"
    copy_if_exists "${repo_root}/assets/ipodjs/rockbox" "${rb_cfg_dir}/ipodjs"
}

prepare_runtime_root()
{
    sim_root="$(mktemp -d)"
    mkdir -p "${sim_root}/.rockbox"
    (cd "${source_sim_root}/.rockbox" && tar --exclude='./maps' -cf - .) | \
        (cd "${sim_root}/.rockbox" && tar -xf -)

    if [ -n "${THEME_CAPTURE_MUSIC_ROOT:-}" ] &&
       [ -d "${THEME_CAPTURE_MUSIC_ROOT}" ]; then
        ln -s "${THEME_CAPTURE_MUSIC_ROOT}" "${sim_root}/Music"
    fi

    for dir_name in Music Playlists Podcasts Recordings; do
        if [ "${dir_name}" = "Music" ] &&
           [ -L "${sim_root}/Music" ]; then
            continue
        fi
        if [ -e "${source_sim_root}/${dir_name}" ]; then
            ln -s "${source_sim_root}/${dir_name}" "${sim_root}/${dir_name}"
        fi
    done

    set_runtime_paths
    install_current_theme_sources

    if [ ! -f "${resume_new_file}" ] && [ -f "${resume_file}" ]; then
        cp "${resume_file}" "${resume_new_file}"
    fi
}

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

capture_window()
{
    local filename="$1"
    import -window "${sim_wid}" "${out_dir}/${filename}"
}

tap_key()
{
    local key="$1"
    xdotool keydown --window "${sim_wid}" "${key}"
    sleep "${THEME_CAPTURE_KEY_HOLD:-0.2}"
    xdotool keyup --window "${sim_wid}" "${key}"
    sleep "${THEME_CAPTURE_KEY_WAIT:-0.8}"
}

enter_wps_if_requested()
{
    if [ -n "${THEME_CAPTURE_ENTER_WPS_KEY:-}" ]; then
        tap_key "${THEME_CAPTURE_ENTER_WPS_KEY}"
        sleep "${THEME_CAPTURE_ENTER_WPS_WAIT:-1}"
    fi
}

ensure_active_playback()
{
    local attempt raw delta

    for attempt in 1 2 3 4 5 6 7 8; do
        capture_window "__playcheck-a.png"
        sleep 3
        capture_window "__playcheck-b.png"

        magick "${out_dir}/__playcheck-a.png" -crop 320x90+0+350 +repage \
            "${tmp_dir}/playcheck-a-crop.png"
        magick "${out_dir}/__playcheck-b.png" -crop 320x90+0+350 +repage \
            "${tmp_dir}/playcheck-b-crop.png"

        raw="$(magick compare -metric AE "${tmp_dir}/playcheck-a-crop.png" \
            "${tmp_dir}/playcheck-b-crop.png" null: 2>&1 || true)"
        delta="${raw%% *}"
        rm -f "${out_dir}/__playcheck-a.png" "${out_dir}/__playcheck-b.png"

        printf "playback-check attempt=%s delta=%s\n" "${attempt}" "${delta}"

        case "${delta}" in
            ''|*[!0-9]*)
                ;;
            *)
                if [ "${delta}" -ge "${THEME_CAPTURE_PLAYBACK_DELTA_MIN:-40}" ]; then
                    printf "active WPS playback confirmed (delta=%s)\n" "${delta}"
                    capture_window "00-active-playback-validated.png"
                    return 0
                fi
                ;;
        esac

        tap_key KP_Add
    done

    capture_window "00-active-playback-validation-failed.png"
    printf "failed to confirm active WPS playback before lockscreen tests\n" >&2
    return 1
}

write_playlist_and_resume()
{
    local track_path="$1"

    printf "P:6::\nA:0:0:%s\n" "$track_path" >"${playlist_file}"

    cat >"${resume_file}" <<'EOF'
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

    cp "${resume_file}" "${resume_new_file}"
}

launch_and_prepare_window()
{
    pushd "${build_dir}" >/dev/null
    ./rockboxui --zoom "${THEME_CAPTURE_ZOOM:-2}" --nobackground --root "${sim_root}" &
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

cleanup_sim()
{
    if [ -n "${sim_pid:-}" ] && kill -0 "${sim_pid}" >/dev/null 2>&1; then
        kill "${sim_pid}" >/dev/null 2>&1 || true
        wait "${sim_pid}" 2>/dev/null || true
    fi
    sim_pid=""
    sim_wid=""
}

prepare_runtime_config()
{
    awk '
        BEGIN { start_replaced = 0; repeat_replaced = 0; aa_replaced = 0 }
        /^start in screen:/ {
            if (!start_replaced) {
                print "start in screen: " start_screen
                start_replaced = 1
            }
            next
        }
        /^repeat:/ {
            if (!repeat_replaced) {
                print "repeat: all"
                repeat_replaced = 1
            }
            next
        }
        /^album art:/ {
            if (!aa_replaced) {
                print "album art: prefer image file"
                aa_replaced = 1
            }
            next
        }
        { print }
        END {
            if (!start_replaced)
                print "start in screen: " start_screen
            if (!repeat_replaced)
                print "repeat: all"
            if (!aa_replaced)
                print "album art: prefer image file"
        }
    ' start_screen="${start_screen}" "${config_file}" >"${tmp_dir}/config.cfg.capture"
    cp "${tmp_dir}/config.cfg.capture" "${config_file}"

    if [ -n "${THEME_CAPTURE_UI_ENGINE:-}" ]; then
        awk '
            /^ui engine:/ { next }
            /^ui engine accent:/ { next }
            /^ui engine density:/ { next }
            /^ui engine font scale:/ { next }
            /^ui engine surface:/ { next }
            /^ui engine hold effect:/ { next }
            /^ui engine dark mode:/ { next }
            { print }
            END {
                print "ui engine: " ui_engine
                print "ui engine accent: blue"
                print "ui engine density: comfortable"
                print "ui engine font scale: normal"
                print "ui engine surface: solid"
                print "ui engine hold effect: lockscreen"
                if (dark_mode != "")
                    print "ui engine dark mode: " dark_mode
            }
        ' ui_engine="${THEME_CAPTURE_UI_ENGINE}" \
          dark_mode="${THEME_CAPTURE_UI_ENGINE_DARK:-}" \
            "${config_file}" >"${tmp_dir}/config.cfg.engine"
        cp "${tmp_dir}/config.cfg.engine" "${config_file}"
    fi
}

cleanup_runtime()
{
    set +e
    cleanup_sim
    if [ "${THEME_CAPTURE_KEEP_ROOT:-0}" = "1" ]; then
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
    if [ ! -d "${source_sim_root}" ]; then
        printf "missing simulator root: %s\n" "${source_sim_root}" >&2
        exit 1
    fi

    mkdir -p "${out_dir}"
    tmp_dir="$(mktemp -d)"

    trap cleanup_runtime EXIT INT TERM HUP
    prepare_runtime_root
    require_file "${playlist_file}"
    require_file "${resume_file}"
    require_file "${config_file}"
    prepare_runtime_config

    write_playlist_and_resume "${normal_track}"
    launch_and_prepare_window
    rm -f "${out_dir}"/*.png
    enter_wps_if_requested
    ensure_active_playback
    capture_window "01-normal-playback.png"

    tap_key "${THEME_CAPTURE_BROWSE_KEY:-KP_5}"
    capture_window "11-menu-mini-player.png"

    enter_wps_if_requested
    tap_key KP_Add
    ensure_active_playback

    tap_key KP_Add
    capture_window "02-paused.png"

    tap_key KP_Add
    ensure_active_playback

    tap_key h
    capture_window "03-lockscreen.png"

    tap_key h
    enter_wps_if_requested
    ensure_active_playback

    tap_key Up
    tap_key Up
    capture_window "04-volume-overlay-active.png"

    tap_key Up
    tap_key h
    capture_window "08-lockscreen-after-volume-change.png"

    tap_key h
    enter_wps_if_requested
    ensure_active_playback

    tap_key h
    tap_key h
    enter_wps_if_requested
    tap_key Up
    capture_window "09-lock-unlock-then-volume-overlay.png"

    for _ in 1 2 3; do
        tap_key Up
        tap_key h
        tap_key h
        enter_wps_if_requested
    done

    tap_key Up
    tap_key h
    capture_window "10-rapid-volume-lock-cycle.png"

    tap_key h
    enter_wps_if_requested
    tap_key F10
    tap_key F11
    capture_window "12-charging-docked.png"

    cleanup_sim

    write_playlist_and_resume "${long_track}"
    launch_and_prepare_window
    enter_wps_if_requested
    capture_window "05-long-title-a.png"
    sleep 2
    capture_window "06-long-title-b.png"
    cleanup_sim

    write_playlist_and_resume "${no_art_track}"
    launch_and_prepare_window
    enter_wps_if_requested
    capture_window "07-no-album-art.png"
    cleanup_sim

    printf "saved screenshots to %s\n" "${out_dir}"
}

main "$@"
