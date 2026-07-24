#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir_arg="${1:-${repo_root}/build-sim-ipod6g}"
build_dir="$(cd "${build_dir_arg}" && pwd)"
out_dir="${2:-/tmp/ipodjs-photos-regression}"
source_root="${build_dir}/simdisk"
rockboxui="${build_dir}/rockboxui"
dark_mode="${IPODJS_PHOTOS_DARK:-0}"
runtime_root=""
sim_pid=""
sim_wid=""
trace=""
dump_bmp=""

cleanup()
{
    local status=$?
    set +e
    if [ -n "${sim_pid}" ] && kill -0 "${sim_pid}" >/dev/null 2>&1; then
        kill "${sim_pid}" >/dev/null 2>&1
        wait "${sim_pid}" 2>/dev/null
    fi
    if [ -s "${trace}" ]; then
        cp "${trace}" "${out_dir}/ipodjs-trace.tsv"
    fi
    if [ "${IPODJS_PHOTOS_KEEP_ROOT:-0}" != "1" ] &&
       [ -n "${runtime_root}" ]; then
        rm -rf "${runtime_root}"
    elif [ -n "${runtime_root}" ]; then
        printf 'preserved simulator root: %s\n' "${runtime_root}"
    fi
    return "${status}"
}

tap_key()
{
    xdotool keydown --window "${sim_wid}" "$1"
    sleep "${3:-0.08}"
    xdotool keyup --window "${sim_wid}" "$1"
    sleep "${2:-0.20}"
}

capture()
{
    local name="$1"
    local attempt

    sleep 0.3
    for attempt in $(seq 1 30); do
        if [ -s "${dump_bmp}" ] &&
           [ "$(stat -c %s "${dump_bmp}")" -ge 307200 ] &&
           magick "${dump_bmp}" "${out_dir}/${name}.png" 2>/dev/null; then
            return
        fi
        sleep 0.05
    done
    printf 'framebuffer capture timed out: %s\n' "${name}" >&2
    exit 1
}

prepare_root()
{
    local directory
    local file
    local config_file
    local dark_value="off"

    [ "${dark_mode}" = "1" ] && dark_value="on"
    runtime_root="$(mktemp -d "${repo_root}/.tmp-ipodjs-photos.XXXXXX")"
    trace="${runtime_root}/.rockbox/ipodjs-trace.tsv"
    dump_bmp="${runtime_root}/simdump.bmp"
    mkdir -p "${runtime_root}/.rockbox"
    for file in "${source_root}"/.rockbox/*; do
        [ -f "${file}" ] && cp "${file}" "${runtime_root}/.rockbox/"
    done
    for directory in langs icons backdrops wps themes fonts albumlist \
                     rockpod ipodjs codecs rocks; do
        if [ -d "${source_root}/.rockbox/${directory}" ]; then
            mkdir -p "${runtime_root}/.rockbox/${directory}"
            cp -a "${source_root}/.rockbox/${directory}/." \
                  "${runtime_root}/.rockbox/${directory}/"
        fi
    done
    if [ -d "${repo_root}/assets/ipodjs/apple" ]; then
        mkdir -p "${runtime_root}/.rockbox/ipodjs/apple"
        cp -a "${repo_root}/assets/ipodjs/apple/." \
              "${runtime_root}/.rockbox/ipodjs/apple/"
    fi

    mkdir -p "${runtime_root}/Photos/.photo_thumbs" \
             "${runtime_root}/.rockbox/rocks/apps"
    cp "${build_dir}/apps/plugins/photos.rock" \
       "${runtime_root}/.rockbox/rocks/apps/photos.rock"
    magick -size 160x100 gradient:'#64a8ff-#102b55' \
        "${runtime_root}/Photos/Locked.bmp"
    magick "${runtime_root}/Photos/Locked.bmp" -resize 80x50 \
        "${runtime_root}/Photos/.photo_thumbs/Locked.bmp.bmp"
    printf 'Locked.bmp|1234\n' \
        >"${runtime_root}/.rockbox/rocks/apps/photos.locks"

    config_file="${runtime_root}/.rockbox/config.cfg"
    awk -v dark="${dark_value}" '
        /^ui engine:/ { next }
        /^ui engine dark mode:/ { next }
        /^start in screen:/ { next }
        /^(tagcache_autoupdate|autoupdate):/ { next }
        /^resume:/ { next }
        { print }
        END {
            print "ui engine: ipodjs"
            print "ui engine dark mode: " dark
            print "start in screen: root"
            print "tagcache_autoupdate: off"
            print "resume: off"
        }
    ' "${config_file}" >"${runtime_root}/config.cfg.new"
    mv "${runtime_root}/config.cfg.new" "${config_file}"
    rm -f "${trace}"
}

launch_sim()
{
    local attempt
    local ids

    sleep 5
    for attempt in 1 2 3 4 5; do
        (
            trap - EXIT INT TERM HUP
            cd "${build_dir}"
            exec env SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=x11 \
                SDL_RENDER_DRIVER=software ROCKPOD_SIM_IPODJS_TRACE=1 \
                ROCKPOD_SIM_PREVIEW_BMP="${dump_bmp}" \
                ROCKPOD_SIM_PREVIEW_INTERVAL_MS=16 \
                ./rockboxui --zoom 1 --nobackground --root "${runtime_root}"
        ) &
        sim_pid=$!
        for _ in 1 2 3 4 5; do
            sleep 1
            ids="$(xdotool search --pid "${sim_pid}" 2>/dev/null || true)"
            sim_wid="${ids%%$'\n'*}"
            if [ -n "${sim_wid}" ]; then
                xdotool windowactivate "${sim_wid}"
                sleep 1
                return
            fi
            kill -0 "${sim_pid}" >/dev/null 2>&1 || break
        done
        wait "${sim_pid}" 2>/dev/null || true
        sim_pid=""
        sleep 5
    done
    printf 'unable to find simulator window\n' >&2
    exit 1
}

main()
{
    local changed
    local mode="light"

    [ "${dark_mode}" = "1" ] && mode="dark"
    for command in awk magick stat xdotool; do
        command -v "${command}" >/dev/null 2>&1 || {
            printf 'missing required command: %s\n' "${command}" >&2
            exit 1
        }
    done
    [ -x "${rockboxui}" ] || {
        printf 'missing simulator build: %s\n' "${rockboxui}" >&2
        exit 1
    }
    [ -f "${build_dir}/apps/plugins/photos.rock" ] || {
        printf 'missing built Photos plugin\n' >&2
        exit 1
    }

    mkdir -p "${out_dir}"
    rm -f "${out_dir}"/*.png "${out_dir}/ipodjs-trace.tsv"
    trap cleanup EXIT INT TERM HUP
    prepare_root
    launch_sim

    for _ in $(seq 1 80); do
        [ -s "${trace}" ] && grep -q $'\tscreen\tHome\t' "${trace}" && break
        sleep 0.1
    done
    grep -q $'\tscreen\tHome\t' "${trace}" || {
        printf 'simulator did not reach Home\n' >&2
        exit 1
    }

    # Default iPodJS order: Cover Flow, Music, Videos, Photos.
    for _ in 1 2 3; do tap_key KP_2 0.08; done
    tap_key KP_5 1.0
    sleep 1.5
    # Force one plugin-owned redraw after the launch transition; the host
    # preview publisher can otherwise retain the final Home loading frame.
    tap_key KP_2 0.2
    capture "photos-${mode}"

    # The only fixture entry is locked; opening it must show the stock numeric
    # wheel instead of Rockbox's full text keyboard.
    tap_key KP_5 0.5
    capture "photos-pin-${mode}-0"
    tap_key KP_2 0.08
    capture "photos-pin-${mode}-1"
    tap_key KP_5 0.15
    tap_key KP_2 0.08
    tap_key KP_5 0.15
    tap_key KP_2 0.08
    tap_key KP_5 0.15
    tap_key KP_2 0.08
    capture "photos-pin-${mode}-4"
    tap_key KP_5 1.0
    capture "photos-unlocked-${mode}"

    changed="$(magick compare -metric AE \
        "${out_dir}/photos-pin-${mode}-4.png" \
        "${out_dir}/photos-unlocked-${mode}.png" null: 2>&1 || true)"
    changed="${changed%% *}"
    changed="${changed%.*}"
    if [ "${changed}" -lt 5000 ]; then
        printf 'numeric PIN did not open the locked photo (%s pixels)\n' \
            "${changed}" >&2
        exit 1
    fi

    printf 'iPodJS Photos %s-mode numeric wheel regression passed; captures: %s\n' \
        "${mode}" "${out_dir}"
}

main "$@"
