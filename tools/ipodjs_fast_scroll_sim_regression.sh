#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir_arg="${1:-${repo_root}/build-sim-ipod6g}"
build_dir="$(cd "${build_dir_arg}" && pwd)"
out_dir="${2:-/tmp/ipodjs-fast-scroll-regression}"
rockboxui="${build_dir}/rockboxui"
source_root="${IPODJS_FAST_SCROLL_SOURCE_ROOT:-${build_dir}/simdisk}"
runtime_root=""
sim_pid=""
sim_wid=""
dump_bmp=""
trace=""
wheel_gate=""

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
    if [ "${IPODJS_FAST_SCROLL_KEEP_ROOT:-0}" != "1" ] &&
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
        return
    fi
    tail -n 1 "${trace}" | cut -f 1
}

wait_for_record()
{
    local before="$1"
    local kind="$2"
    local name="$3"
    local update="$4"
    local attempt

    for attempt in $(seq 1 80); do
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

capture()
{
    local name="$1"
    local attempt
    local preview_bmp="${dump_bmp}"

    # LCD updates publish through an atomic host rename.  Allow the SDL render
    # thread to consume the traced update, then read only the published file.
    sleep 0.18
    for attempt in $(seq 1 20); do
        if [ -s "${preview_bmp}" ] &&
           [ "$(stat -c %s "${preview_bmp}")" -ge 307200 ] &&
           magick "${preview_bmp}" \
               "${out_dir}/${name}.png" 2>/dev/null; then
            return
        fi
        sleep 0.05
        sleep 0.05
    done
    printf "framebuffer capture timed out: %s\n" "${name}" >&2
    exit 1
}

assert_overlay_glyph()
{
    local image="$1"
    local minimum="$2"
    local pixels

    pixels="$(magick "${image}" -crop 40x40+140+112 +repage \
        -colorspace gray -threshold 80% \
        -format '%[fx:mean*1600]' info:)"
    pixels="${pixels%.*}"
    if [ "${pixels}" -lt "${minimum}" ]; then
        printf "stock overlay glyph is missing from %s (%s bright pixels)\n" \
            "${image}" "${pixels}" >&2
        exit 1
    fi
}

prepare_root()
{
    local directory
    local file
    local config_file
    local source_track

    runtime_root="$(mktemp -d "${repo_root}/.tmp-ipodjs-fast.XXXXXX")"
    dump_bmp="${runtime_root}/simdump.bmp"
    trace="${runtime_root}/.rockbox/ipodjs-trace.tsv"
    wheel_gate="${runtime_root}/wheel-fast"
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

    mkdir -p "${runtime_root}/.rockbox/ipodjs/apple"
    cp -a "${repo_root}/assets/ipodjs/apple/." \
          "${runtime_root}/.rockbox/ipodjs/apple/"

    source_track="$(find "${source_root}/Music" -type f -iname '*.mp3' -print -quit)"
    if [ -z "${source_track}" ]; then
        printf "no MP3 fixture found under %s/Music\n" "${source_root}" >&2
        exit 1
    fi
    "${repo_root}/rockpod/.venv/bin/python" \
        "${repo_root}/tools/ipodjs_fast_scroll_fixture.py" \
        "${runtime_root}" "${source_track}"

    config_file="${runtime_root}/.rockbox/config.cfg"
    awk '
        /^ui engine:/ { next }
        /^ui engine dark mode:/ { next }
        /^ui engine hold effect:/ { next }
        /^start in screen:/ { next }
        /^(tagcache_autoupdate|autoupdate):/ { next }
        /^resume:/ { next }
        { print }
        END {
            print "ui engine: ipodjs"
            print "ui engine dark mode: off"
            print "ui engine hold effect: lockscreen"
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
    local window_attempt

    sleep 5
    for attempt in 1 2 3 4 5; do
        (
            trap - EXIT INT TERM HUP
            cd "${build_dir}"
            exec env SDL_AUDIODRIVER=dummy \
                SDL_VIDEODRIVER=x11 \
                SDL_RENDER_DRIVER=software \
                ROCKPOD_SIM_IPODJS_TRACE=1 \
                ROCKPOD_SIM_WHEEL_VELOCITY=340 \
                ROCKPOD_SIM_WHEEL_VELOCITY_GATE="${wheel_gate}" \
                ROCKPOD_SIM_PREVIEW_BMP="${dump_bmp}" \
                ROCKPOD_SIM_PREVIEW_INTERVAL_MS=16 \
                ./rockboxui --zoom 1 --nobackground --root "${runtime_root}"
        ) &
        sim_pid=$!

        for window_attempt in 1 2 3 4 5; do
            sleep 1
            ids="$(xdotool search --pid "${sim_pid}" 2>/dev/null || true)"
            sim_wid="${ids%%$'\n'*}"
            if [ -n "${sim_wid}" ]; then
                xdotool windowactivate "${sim_wid}"
                sleep 1
                return
            fi
            if ! kill -0 "${sim_pid}" >/dev/null 2>&1; then
                break
            fi
        done
        wait "${sim_pid}" 2>/dev/null || true
        sim_pid=""
        sleep 5
    done
    printf "unable to find simulator window after %s attempts\n" \
        "${attempt}" >&2
    exit 1
}

enter_music()
{
    local before

    for _ in $(seq 1 80); do
        if [ -s "${trace}" ] && grep -q $'\tscreen\tHome\t' "${trace}"; then
            break
        fi
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
}

assert_list_ready()
{
    local title="$1"
    local minimum="${2:-26}"

    if ! awk -F '\t' -v title="${title}" \
        -v minimum="${minimum}" \
        '$3 == "list" && $4 == title && $12 >= minimum { found = 1 }
         END { exit found ? 0 : 1 }' "${trace}"; then
        printf "%s did not expose at least %s fixture values\n" \
            "${title}" "${minimum}" >&2
        exit 1
    fi
}

assert_short_album_artist_fixture()
{
    if ! awk -F '\t' \
        '$3 == "list" && $4 == "Album Artist" &&
         $12 >= 8 && $12 < 12 { found = 1 }
         END { exit found ? 0 : 1 }' "${trace}"; then
        printf "Album Artist fixture did not exercise the short-list gate\n" \
            >&2
        exit 1
    fi
}

exercise_fast_scroll()
{
    local title="$1"
    local slug="$2"
    local minimum="${3:-26}"
    local first_show
    local last_show
    local before
    local changed

    assert_list_ready "${title}" "${minimum}"
    touch "${wheel_gate}"
    before="$(last_sequence)"
    tap_key KP_2 0.12
    wait_for_record "${before}" fast-scroll "" show
    first_show="$(awk -F '\t' -v before="${before}" \
        '$1 > before && $3 == "fast-scroll" && $5 == "show" {
            print $1; exit
         }' "${trace}")"
    tap_key KP_2 0.25
    for _ in 1 2; do
        tap_key KP_2 0.25
    done
    tap_key KP_2 0.25
    last_show="$(awk -F '\t' '$3 == "fast-scroll" && $5 == "show" {
        seq = $1
    } END { print seq }' "${trace}")"
    if awk -F '\t' -v first="${first_show}" -v last="${last_show}" \
        '$1 > first && $1 < last && $3 == "fast-scroll" &&
         $5 == "expired" { found = 1 }
         END { exit found ? 0 : 1 }' "${trace}"; then
        printf "%s letter expired while wheel input was still arriving\n" \
            "${title}" >&2
        exit 1
    fi
    capture "${slug}-active"
    assert_overlay_glyph "${out_dir}/${slug}-active.png" 8

    sleep 1.35
    wait_for_record "${last_show}" fast-scroll "" expired
    capture "${slug}-stopped"
    changed="$(magick compare -metric AE \
        "${out_dir}/${slug}-active.png" \
        "${out_dir}/${slug}-stopped.png" null: 2>&1 || true)"
    changed="${changed%% *}"
    changed="${changed%.*}"
    if [ "${changed}" -lt 500 ]; then
        printf "%s overlay was not visibly removed (%s pixels changed)\n" \
            "${title}" "${changed}" >&2
        exit 1
    fi
    rm -f "${wheel_gate}"
}

main()
{
    local before

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
    [ -f "${repo_root}/assets/ipodjs/apple/retailos-2.0.4/resources/004.rga" ] || {
        printf "prepared private Apple fast-scroll assets are missing\n" >&2
        exit 1
    }

    mkdir -p "${out_dir}"
    rm -f "${out_dir}"/*.png "${out_dir}/ipodjs-trace.tsv"
    trap cleanup EXIT INT TERM HUP
    prepare_root
    launch_sim
    enter_music

    before="$(last_sequence)"
    tap_key KP_5 0.4
    wait_for_record "${before}" list "Album Artist" ""
    assert_short_album_artist_fixture
    exercise_fast_scroll "Album Artist" album-artist 8
    if ! awk -F '\t' \
        '$3 == "fast-scroll" && $4 == "B" && $5 == "show" { found = 1 }
         END { exit found ? 0 : 1 }' "${trace}"; then
        printf "accented Bírch album artist did not fold into Apple B bucket\n" \
            >&2
        exit 1
    fi

    before="$(last_sequence)"
    tap_key h 0.05
    wait_for_record "${before}" screen Lockscreen ""
    capture "album-artist-lockscreen"
    before="$(last_sequence)"
    tap_key h 0.05
    wait_for_record "${before}" list "Album Artist" ""

    before="$(last_sequence)"
    tap_key KP_Decimal 0.35
    wait_for_record "${before}" list Music ""
    before="$(last_sequence)"
    tap_key KP_2
    tap_key KP_5 0.4
    wait_for_record "${before}" list Artist ""
    exercise_fast_scroll Artist artist

    before="$(last_sequence)"
    tap_key KP_Decimal 0.35
    wait_for_record "${before}" list Music ""
    before="$(last_sequence)"
    tap_key KP_2
    tap_key KP_5 0.4
    wait_for_record "${before}" list Albums ""
    exercise_fast_scroll Albums albums

    before="$(last_sequence)"
    tap_key KP_Decimal 0.35
    wait_for_record "${before}" list Music ""
    before="$(last_sequence)"
    for _ in 1 2 3 4 5; do
        tap_key KP_2 0.08
    done
    tap_key KP_5 0.4
    wait_for_record "${before}" list "Tracks by" ""
    before="$(last_sequence)"
    tap_key KP_2
    tap_key KP_5 0.4
    wait_for_record "${before}" list Title ""
    exercise_fast_scroll Title songs

    cp "${trace}" "${out_dir}/ipodjs-trace.tsv"
    printf "iPodJS A-Z simulator regression passed for short Album Artist, Artist, Albums, and title-sorted Songs; captures: %s\n" \
        "${out_dir}"
}

main "$@"
