#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir_arg="${1:-${repo_root}/build-sim-ipod6g}"
build_dir="$(cd "${build_dir_arg}" && pwd)"
out_dir="${2:-/tmp/ipodjs-search-regression}"
rockboxui="${build_dir}/rockboxui"
source_root="${build_dir}/simdisk"
runtime_root=""
sim_pid=""
sim_wid=""
trace=""
dump_bmp=""
stress_cycles="${IPODJS_SEARCH_STRESS_CYCLES:-8}"

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
    if [ "${IPODJS_SEARCH_KEEP_ROOT:-0}" != "1" ] &&
       [ -n "${runtime_root}" ]; then
        rm -rf "${runtime_root}"
    elif [ -n "${runtime_root}" ]; then
        printf "preserved simulator root: %s\n" "${runtime_root}"
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

last_sequence()
{
    if [ ! -s "${trace}" ]; then
        printf '0\n'
    else
        tail -n 1 "${trace}" | cut -f 1
    fi
}

wait_for_record()
{
    local before="$1"
    local kind="$2"
    local name="$3"
    local update="$4"
    local attempt

    for attempt in $(seq 1 100); do
        if [ -s "${trace}" ] && awk -F '\t' \
            -v before="${before}" -v kind="${kind}" \
            -v name="${name}" -v update="${update}" \
            '$1 > before && $3 == kind &&
             (name == "" || $4 == name) &&
             (update == "" || $5 == update) { found = 1 }
             END { exit found ? 0 : 1 }' "${trace}"; then
            sleep 0.2
            return
        fi
        if ! kill -0 "${sim_pid}" >/dev/null 2>&1; then
            break
        fi
        sleep 0.1
    done
    printf "missing trace record: %s / %s / %s after %s\n" \
        "${kind}" "${name}" "${update}" "${before}" >&2
    exit 1
}

wait_for_search_character()
{
    local before="$1"
    local character="$2"
    local attempt

    for attempt in $(seq 1 100); do
        if [ -s "${trace}" ] && awk -F '\t' \
            -v before="${before}" -v character="${character}" \
            '$1 > before && $3 == "screen" && $4 == "Search" &&
             $5 == "input" && $10 == character { found = 1 }
             END { exit found ? 0 : 1 }' "${trace}"; then
            sleep 0.08
            return
        fi
        if ! kill -0 "${sim_pid}" >/dev/null 2>&1; then
            break
        fi
        sleep 0.05
    done
    printf "Search did not render character index %s after %s\n" \
        "${character}" "${before}" >&2
    exit 1
}

capture()
{
    local name="$1"
    local attempt

    sleep 0.18
    for attempt in $(seq 1 20); do
        if [ -s "${dump_bmp}" ] &&
           [ "$(stat -c %s "${dump_bmp}")" -ge 307200 ] &&
           magick "${dump_bmp}" "${out_dir}/${name}.png" 2>/dev/null; then
            return
        fi
        sleep 0.05
    done
    printf "framebuffer capture timed out: %s\n" "${name}" >&2
    exit 1
}

prepare_root()
{
    local directory
    local file
    local config_file
    local source_track

    runtime_root="$(mktemp -d "${repo_root}/.tmp-ipodjs-search.XXXXXX")"
    trace="${runtime_root}/.rockbox/ipodjs-trace.tsv"
    dump_bmp="${runtime_root}/simdump.bmp"
    mkdir -p "${runtime_root}/.rockbox"
    for file in "${source_root}"/.rockbox/*; do
        [ -f "${file}" ] && cp "${file}" "${runtime_root}/.rockbox/"
    done
    for directory in langs icons backdrops wps themes fonts albumlist \
                     rockpod ipodjs codecs; do
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

    source_track="$(find "${source_root}/Music" -type f -iname '*.mp3' -print -quit)"
    [ -n "${source_track}" ] || {
        printf "no MP3 fixture found under %s/Music\n" "${source_root}" >&2
        exit 1
    }
    "${repo_root}/rockpod/.venv/bin/python" \
        "${repo_root}/tools/ipodjs_fast_scroll_fixture.py" \
        "${runtime_root}" "${source_track}"
    mkdir -p "${runtime_root}/Playlists"
    printf '#EXTM3U\n/Music/Fast Scroll/Xylosma/24.mp3\n' \
        >"${runtime_root}/Playlists/Xylosma Mix.m3u8"

    config_file="${runtime_root}/.rockbox/config.cfg"
    awk '
        /^ui engine:/ { next }
        /^ui engine dark mode:/ { next }
        /^start in screen:/ { next }
        /^(tagcache_autoupdate|autoupdate):/ { next }
        /^resume:/ { next }
        { print }
        END {
            print "ui engine: ipodjs"
            print "ui engine dark mode: off"
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
    printf "unable to find simulator window\n" >&2
    exit 1
}

main()
{
    local before
    local character
    local sweep_start

    for command in awk find magick stat xdotool; do
        command -v "${command}" >/dev/null 2>&1 || {
            printf "missing required command: %s\n" "${command}" >&2
            exit 1
        }
    done
    [ -x "${rockboxui}" ] || {
        printf "missing simulator build: %s\n" "${rockboxui}" >&2
        exit 1
    }
    for asset in search-song.apple.16x16x24.bmp \
                 search-artist.apple.16x16x24.bmp \
                 search-album.apple.16x16x24.bmp \
                 search-playlist.apple.16x16x24.bmp \
                 fast-scroll-blank.apple.95x82x32.bmp \
                 search-field.apple.97x32x24.bmp \
                 search-selected.apple.97x32x24.bmp; do
        [ -f "${repo_root}/assets/ipodjs/apple/${asset}" ] || {
            printf "missing verified Apple Search asset: %s\n" "${asset}" >&2
            exit 1
        }
    done

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
        printf "simulator did not reach Home\n" >&2
        exit 1
    }

    before="$(last_sequence)"
    tap_key KP_2
    tap_key KP_5 0.5
    wait_for_record "${before}" list Music ""
    for _ in $(seq 1 9); do tap_key KP_2 0.06; done
    before="$(last_sequence)"
    tap_key KP_5 0.4
    wait_for_record "${before}" screen Search input
    capture search-empty

    # Exercise every selection pill width.  In particular, Apple's narrow I
    # glyph used to make the nine-slice reject the frame and abort the strip.
    sweep_start="$(last_sequence)"
    for character in $(seq 1 25); do
        before="$(last_sequence)"
        tap_key KP_2 0.04
        wait_for_search_character "${before}" "${character}"
        if [ "${character}" -eq 8 ]; then
            # The host preview publisher trails the LCD trace by one frame on
            # some SDL builds; hold I steady so this capture proves its pill.
            sleep 0.5
            capture search-i-input
        fi
    done
    if ! awk -F '\t' -v before="${sweep_start}" '
        $1 > before && $3 == "screen" && $4 == "Search" &&
        $5 == "input" && $10 >= 1 && $10 <= 25 { seen[$10] = 1 }
        END {
            for (i = 1; i <= 25; i++)
                if (!seen[i]) exit 1
        }' "${trace}"; then
        printf "Search wheel did not expose every A-Z character\n" >&2
        exit 1
    fi
    before="$(last_sequence)"
    tap_key KP_2 0.08
    wait_for_search_character "${before}" 0

    before="$(last_sequence)"
    tap_key KP_8 0.08
    tap_key KP_8 0.08
    tap_key KP_8 0.1
    tap_key KP_5 0.6
    wait_for_record "${before}" screen Search input
    if ! awk -F '\t' -v before="${before}" \
        '$1 > before && $3 == "screen" && $4 == "Search" &&
         $5 == "input" && $12 >= 4 { found = 1 }
         END { exit found ? 0 : 1 }' "${trace}"; then
        printf "live X search did not return artist, album, song, and playlist\n" >&2
        exit 1
    fi
    capture search-x-input

    before="$(last_sequence)"
    tap_key h 0.05
    wait_for_record "${before}" screen Lockscreen ""
    capture search-lockscreen
    before="$(last_sequence)"
    tap_key h 0.05
    wait_for_record "${before}" screen Search input

    before="$(last_sequence)"
    tap_key KP_Decimal 0.4
    wait_for_record "${before}" screen Search results
    capture search-x-results
    tap_key KP_2 0.08
    tap_key KP_2 0.08
    before="$(last_sequence)"
    tap_key KP_5 0.8
    wait_for_record "${before}" wps "Now Playing" ""
    if ! awk -F '\t' -v before="${before}" \
        '$1 > before && $3 == "wps" && $4 == "Now Playing" && $12 != 0 {
            found = 1
         } END { exit found ? 0 : 1 }' "${trace}"; then
        printf "song Search result did not enter playing WPS\n" >&2
        exit 1
    fi

    # Exercise the hardware route repeatedly: iPod 6G uses Rockbox's stock
    # tagtree/WPS lifecycle even while the surrounding engine is iPodJS.
    # Returning from WPS must preserve Rockbox's live Music state without
    # pushing a parallel iPodJS Database/Files history frame.  Rockbox owns
    # the nested tagtree callback, so its stable return point is the Music
    # list with Search selected; reopening Search starts a fresh query.
    for _ in $(seq 1 "${stress_cycles}"); do
        before="$(last_sequence)"
        tap_key KP_Decimal 0.35
        wait_for_record "${before}" list Music ""
        if ! awk -F '\t' -v before="${before}" '
            $1 > before && $3 == "list" && $4 == "Music" &&
            $10 == 9 && $13 != 0 { found = 1 }
            END { exit found ? 0 : 1 }' "${trace}"; then
            printf "stock Music return lost Search selection or playback\n" >&2
            exit 1
        fi
        before="$(last_sequence)"
        tap_key KP_5 0.35
        wait_for_record "${before}" screen Search input
        tap_key KP_8 0.06
        tap_key KP_8 0.06
        tap_key KP_8 0.08
        before="$(last_sequence)"
        tap_key KP_5 0.4
        wait_for_record "${before}" screen Search input
        before="$(last_sequence)"
        tap_key KP_Decimal 0.3
        wait_for_record "${before}" screen Search results
        tap_key KP_2 0.06
        tap_key KP_2 0.06
        before="$(last_sequence)"
        tap_key KP_5 0.55
        wait_for_record "${before}" wps "Now Playing" ""
    done
    if awk -F '\t' '
        $3 == "origin" && $4 ~ /^push / &&
        ($4 ~ / origin=1 / || $4 ~ / origin=2 /) { found = 1 }
        END { exit found ? 0 : 1 }' "${trace}"; then
        printf "stock iPod 6G music route used an iPodJS database history frame\n" >&2
        exit 1
    fi

    cp "${trace}" "${out_dir}/ipodjs-trace.tsv"
    printf "iPodJS stock Search A-Z simulator regression passed; captures: %s\n" \
        "${out_dir}"
}

main "$@"
