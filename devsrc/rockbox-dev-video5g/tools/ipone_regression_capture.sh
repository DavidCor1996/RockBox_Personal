#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:-/home/david/Documents/RockBox_Personal-master/build-sim-video-5g}"
out_dir="${2:-/home/david/Documents/RockBox_Personal-master/docs/ipone-regression-shots}"
source_sim_root="${build_dir}/simdisk"
sim_root=""

normal_track="${NORMAL_TRACK:-/Music/Electric Jewels - April Wine/01 - April Wine - Weeping Widow.flac}"
long_track="${LONG_TRACK:-/Music/Abbey Road (Remastered) - The Beatles/13 - The Beatles - She Came In Through The Bathroom Window (Remastered 2009).flac}"
no_art_track="${NO_ART_TRACK:-/Music/The Beatles (White Album)/The Beatles (White Album) [Disc 1]/01 Back In The U.S.S.R..mp3}"

rockboxui="${build_dir}/rockboxui"
rb_cfg_dir=""
playlist_file=""
resume_file=""
resume_new_file=""
config_file=""

set_runtime_paths()
{
    rb_cfg_dir="${sim_root}/.rockbox"
    playlist_file="${rb_cfg_dir}/.playlist_control"
    resume_file="${rb_cfg_dir}/.resume.cfg"
    resume_new_file="${rb_cfg_dir}/.resume.cfg.new"
    config_file="${rb_cfg_dir}/config.cfg"
}

prepare_runtime_root()
{
    sim_root="$(mktemp -d)"
    cp -a "${source_sim_root}/.rockbox" "${sim_root}/.rockbox"

    for dir_name in Music Playlists Podcasts Recordings; do
        if [ -e "${source_sim_root}/${dir_name}" ]; then
            ln -s "${source_sim_root}/${dir_name}" "${sim_root}/${dir_name}"
        fi
    done

    set_runtime_paths

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

elapsed_from_file()
{
    local value
    value="$(awk -F': ' '$1 == "ELA" { print $2; exit }' "$1" 2>/dev/null || true)"
    case "${value}" in
        ''|*[!0-9-]*)
            printf -- "-1\n"
            ;;
        *)
            printf "%s\n" "${value}"
            ;;
    esac
}

get_elapsed_seconds()
{
    local ela_a ela_b

    ela_a="$(elapsed_from_file "${resume_file}")"
    ela_b="$(elapsed_from_file "${resume_new_file}")"

    if [ "${ela_b}" -gt "${ela_a}" ]; then
        printf "%s\n" "${ela_b}"
    else
        printf "%s\n" "${ela_a}"
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
                if [ "${delta}" -ge 40 ]; then
                    printf "active WPS playback confirmed (delta=%s)\n" "${delta}"
                    capture_window "00-active-playback-validated.png"
                    return 0
                fi
                ;;
        esac

        xdotool key --window "${sim_wid}" KP_Add
        sleep 0.9
    done

    capture_window "00-active-playback-validation-failed.png"
    printf "failed to confirm active WPS playback before lockscreen tests\n" >&2
    return 1
}

ensure_playback_started()
{
    ensure_active_playback
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
    "${rockboxui}" --zoom 2 --nobackground --root "${sim_root}" \
        >/tmp/ipone_regression_capture.log 2>&1 &
    sim_pid=$!
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
        BEGIN { start_replaced = 0; repeat_replaced = 0 }
        /^start in screen:/ {
            if (!start_replaced) {
                print "start in screen: wps"
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
        { print }
        END {
            if (!start_replaced)
                print "start in screen: wps"
            if (!repeat_replaced)
                print "repeat: all"
        }
    ' "${config_file}" >"${tmp_dir}/config.cfg.wps"
    cp "${tmp_dir}/config.cfg.wps" "${config_file}"
}

cleanup_runtime()
{
    set +e
    cleanup_sim
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
    rm -f "${out_dir}"/*.png
    tmp_dir="$(mktemp -d)"

    trap cleanup_runtime EXIT INT TERM HUP
    prepare_runtime_root
    require_file "${playlist_file}"
    require_file "${resume_file}"
    require_file "${config_file}"
    prepare_runtime_config

    write_playlist_and_resume "${normal_track}"
    launch_and_prepare_window
    ensure_playback_started
    capture_window "01-normal-playback.png"

    xdotool key --window "${sim_wid}" KP_Add
    sleep 1
    capture_window "02-paused.png"

    xdotool key --window "${sim_wid}" KP_Add
    sleep 0.8
    ensure_playback_started

    xdotool key --window "${sim_wid}" h
    sleep 1
    capture_window "03-lockscreen.png"

    xdotool key --window "${sim_wid}" h
    sleep 0.8
    ensure_playback_started

    xdotool key --window "${sim_wid}" Up
    sleep 0.2
    xdotool key --window "${sim_wid}" Up
    sleep 0.5
    capture_window "04-volume-overlay-active.png"

    xdotool key --window "${sim_wid}" Up
    sleep 0.08
    xdotool key --window "${sim_wid}" h
    sleep 0.4
    capture_window "08-lockscreen-after-volume-change.png"

    xdotool key --window "${sim_wid}" h
    sleep 0.6
    ensure_playback_started

    xdotool key --window "${sim_wid}" h
    sleep 0.3
    xdotool key --window "${sim_wid}" h
    sleep 0.3
    xdotool key --window "${sim_wid}" Up
    sleep 0.5
    capture_window "09-lock-unlock-then-volume-overlay.png"

    for _ in 1 2 3; do
        xdotool key --window "${sim_wid}" Up
        sleep 0.08
        xdotool key --window "${sim_wid}" h
        sleep 0.2
        xdotool key --window "${sim_wid}" h
        sleep 0.2
    done

    xdotool key --window "${sim_wid}" Up
    sleep 0.08
    xdotool key --window "${sim_wid}" h
    sleep 0.4
    capture_window "10-rapid-volume-lock-cycle.png"

    cleanup_sim

    write_playlist_and_resume "${long_track}"
    launch_and_prepare_window
    capture_window "05-long-title-a.png"
    sleep 2
    capture_window "06-long-title-b.png"
    cleanup_sim

    write_playlist_and_resume "${no_art_track}"
    launch_and_prepare_window
    capture_window "07-no-album-art.png"
    cleanup_sim

    printf "saved screenshots to %s\n" "${out_dir}"
}

main "$@"
