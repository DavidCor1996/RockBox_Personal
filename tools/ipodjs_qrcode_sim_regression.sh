#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir_arg="${1:-${repo_root}/build-sim-ipod6g}"
build_dir="$(cd "${build_dir_arg}" && pwd)"
out_dir="${2:-/tmp/ipodjs-qrcode-regression}"
rockboxui="${build_dir}/rockboxui"
source_root="${build_dir}/simdisk"
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
    if [ "${IPODJS_QRCODE_KEEP_ROOT:-0}" != "1" ] &&
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

last_sequence()
{
    if [ ! -s "${trace}" ]; then
        printf '0\n'
    else
        tail -n 1 "${trace}" | cut -f 1
    fi
}

wait_for_screen()
{
    local before="$1"
    local name="$2"
    local update="${3:-}"
    local attempt

    for attempt in $(seq 1 120); do
        if [ -s "${trace}" ] && awk -F '\t' \
            -v before="${before}" -v name="${name}" -v update="${update}" '
            $1 > before && $3 == "screen" && $4 == name &&
            (update == "" || $5 == update) { found = 1 }
            END { exit found ? 0 : 1 }
        ' "${trace}"; then
            sleep 0.15
            return
        fi
        kill -0 "${sim_pid}" >/dev/null 2>&1 || break
        sleep 0.05
    done
    printf 'missing screen trace: %s / %s after %s\n' \
        "${name}" "${update}" "${before}" >&2
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
    printf 'framebuffer capture timed out: %s\n' "${name}" >&2
    exit 1
}

prepare_root()
{
    local directory
    local file
    local config_file

    runtime_root="$(mktemp -d "${repo_root}/.tmp-ipodjs-qrcode.XXXXXX")"
    trace="${runtime_root}/.rockbox/ipodjs-trace.tsv"
    dump_bmp="${runtime_root}/simdump.bmp"
    mkdir -p "${runtime_root}/.rockbox"
    for file in "${source_root}"/.rockbox/*; do
        [ -f "${file}" ] && cp "${file}" "${runtime_root}/.rockbox/"
    done
    for directory in langs icons backdrops wps themes fonts ipodjs codecs; do
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

    config_file="${runtime_root}/.rockbox/config.cfg"
    awk '
        /^ui engine:/ { next }
        /^ui engine dark mode:/ { next }
        /^start in screen:/ { next }
        /^resume:/ { next }
        { print }
        END {
            print "ui engine: ipodjs"
            print "ui engine dark mode: off"
            print "start in screen: root"
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
        sleep 2
    done
    printf 'unable to find simulator window\n' >&2
    exit 1
}

move_to_selection()
{
    local name="$1"
    local target="$2"
    local before
    local selected

    for _ in $(seq 1 50); do
        selected="$(awk -F '\t' -v name="${name}" '
            $3 == "screen" && $4 == name { value = $10 }
            END { print value + 0 }
        ' "${trace}")"
        [ "${selected}" -eq "${target}" ] && return
        before="$(last_sequence)"
        if [ "${selected}" -lt "${target}" ]; then
            tap_key KP_2 0.30 0.02
        else
            tap_key KP_8 0.30 0.02
        fi
        for _ in $(seq 1 20); do
            if awk -F '\t' -v before="${before}" -v name="${name}" '
                $1 > before && $3 == "screen" && $4 == name {
                    found = 1
                }
                END { exit found ? 0 : 1 }
            ' "${trace}"; then
                break
            fi
            sleep 0.05
        done
    done
    printf 'unable to select row %s on %s\n' "${target}" "${name}" >&2
    exit 1
}

open_selection()
{
    local name="$1"
    local target="$2"
    local destination="$3"
    local update="${4:-}"
    local before

    if [ $((target % 2)) -eq 0 ]; then
        move_to_selection "${name}" "${target}"
        before="$(last_sequence)"
        tap_key KP_5 0.4
    else
        move_to_selection "${name}" "$((target - 1))"
        before="$(last_sequence)"
        xdotool keydown --window "${sim_wid}" KP_2
        sleep 0.01
        xdotool keydown --window "${sim_wid}" KP_5
        sleep 0.02
        xdotool keyup --window "${sim_wid}" KP_5
        xdotool keyup --window "${sim_wid}" KP_2
        sleep 0.2
    fi
    wait_for_screen "${before}" "${destination}" "${update}"
}

main()
{
    local before
    local saved_file
    local saved_source

    for command in awk magick stat xdotool zbarimg; do
        command -v "${command}" >/dev/null 2>&1 || {
            printf 'missing required command: %s\n' "${command}" >&2
            exit 1
        }
    done
    [ -x "${rockboxui}" ] || {
        printf 'missing simulator build: %s\n' "${rockboxui}" >&2
        exit 1
    }

    mkdir -p "${out_dir}"
    rm -f "${out_dir}"/*.png "${out_dir}/ipodjs-trace.tsv"
    trap cleanup EXIT INT TERM HUP
    prepare_root
    launch_sim

    for _ in $(seq 1 100); do
        [ -s "${trace}" ] && grep -q $'\tscreen\tHome\t' "${trace}" && break
        sleep 0.1
    done
    grep -q $'\tscreen\tHome\t' "${trace}" || {
        printf 'simulator did not reach Home\n' >&2
        exit 1
    }
    xdotool windowactivate "${sim_wid}"
    sleep 1

    open_selection "Home" 5 "Extras"
    open_selection "Extras" 3 "Applications"
    before="$(last_sequence)"
    open_selection "Applications" 11 "QR Codes" "list"
    if ! awk -F '\t' -v before="${before}" '
        $1 > before && $3 == "screen" && $4 == "QR Codes" &&
        $5 == "list" && $10 == 0 && $12 == 1 { found = 1 }
        END { exit found ? 0 : 1 }
    ' "${trace}"; then
        printf 'QR app did not open on an empty saved-code list\n' >&2
        exit 1
    fi
    capture qrcode-empty-list

    before="$(last_sequence)"
    tap_key KP_5 0.4
    wait_for_screen "${before}" "QR Code" "input"
    capture qrcode-empty-input

    tap_key KP_5 0.08
    tap_key KP_2 0.08
    tap_key KP_5 0.08
    tap_key KP_2 0.08
    before="$(last_sequence)"
    tap_key KP_5 0.3
    wait_for_screen "${before}" "QR Code" "input"
    if ! awk -F '\t' -v before="${before}" '
        $1 > before && $3 == "screen" && $4 == "QR Code" &&
        $5 == "input" && $12 == 3 { found = 1 }
        END { exit found ? 0 : 1 }
    ' "${trace}"; then
        printf 'QR keyboard did not enter ABC\n' >&2
        exit 1
    fi
    capture qrcode-abc-input

    before="$(last_sequence)"
    tap_key KP_Decimal 0.5
    wait_for_screen "${before}" "QR Code" "display"
    if ! awk -F '\t' -v before="${before}" '
        $1 > before && $3 == "screen" && $4 == "QR Code" &&
        $5 == "display" && $10 == 21 && $11 == 7 && $12 == 3 &&
        $6 == 58 && $7 == 30 && $8 == 203 && $9 == 203 {
            found = 1
        }
        END { exit found ? 0 : 1 }
    ' "${trace}"; then
        printf 'QR display geometry or version differs from the golden ABC case\n' >&2
        exit 1
    fi
    capture qrcode-abc-display

    before="$(last_sequence)"
    tap_key KP_5 0.4
    wait_for_screen "${before}" "QR Code" "saved"
    saved_file="${runtime_root}/QR Codes/QR Code 001.bmp"
    if [ ! -s "${saved_file}" ]; then
        printf 'saved QR bitmap is missing: %s\n' "${saved_file}" >&2
        exit 1
    fi
    if [ "$(magick identify -format '%wx%h' "${saved_file}")" != "116x116" ]; then
        printf 'saved QR bitmap has incorrect dimensions\n' >&2
        exit 1
    fi
    if [ "$(zbarimg --quiet --raw "${saved_file}" 2>/dev/null)" != "ABC" ]; then
        printf 'saved QR bitmap did not decode back to ABC\n' >&2
        exit 1
    fi
    cp "${saved_file}" "${out_dir}/QR Code 001.bmp"
    saved_source="${runtime_root}/.rockbox/qrcodes/QR Code 001.txt"
    if [ ! -s "${saved_source}" ] ||
       [ "$(awk '{ printf "%s", $0 }' "${saved_source}")" != "ABC" ]; then
        printf 'saved QR source record is missing or incorrect\n' >&2
        exit 1
    fi
    wait_for_screen "${before}" "QR Codes" "list"
    if ! awk -F '\t' -v before="${before}" '
        $1 > before && $3 == "screen" && $4 == "QR Codes" &&
        $5 == "list" && $10 == 1 && $12 == 2 { found = 1 }
        END { exit found ? 0 : 1 }
    ' "${trace}"; then
        printf 'saved QR code was not added to the opening list\n' >&2
        exit 1
    fi
    capture qrcode-saved-list

    before="$(last_sequence)"
    tap_key KP_5 0.4
    wait_for_screen "${before}" "QR Code" "display"
    if ! awk -F '\t' -v before="${before}" '
        $1 > before && $3 == "screen" && $4 == "QR Code" &&
        $5 == "display" && $10 == 21 && $11 == 7 && $12 == 3 {
            found = 1
        }
        END { exit found ? 0 : 1 }
    ' "${trace}"; then
        printf 'saved QR code did not reopen as ABC\n' >&2
        exit 1
    fi
    capture qrcode-reopened

    before="$(last_sequence)"
    tap_key KP_Decimal 0.4
    wait_for_screen "${before}" "QR Codes" "list"
    move_to_selection "QR Codes" 0
    before="$(last_sequence)"
    tap_key KP_5 0.4
    wait_for_screen "${before}" "QR Code" "input"
    before="$(last_sequence)"
    tap_key KP_Add 0.25
    if ! awk -F '\t' -v before="${before}" '
        $1 > before && $3 == "screen" && $4 == "QR Code" &&
        $5 == "input" && $10 == 1 { found = 1 }
        END { exit found ? 0 : 1 }
    ' "${trace}"; then
        printf 'QR keyboard did not switch to the lowercase bank\n' >&2
        exit 1
    fi
    capture qrcode-lowercase-bank

    cp "${trace}" "${out_dir}/ipodjs-trace.tsv"
    printf 'iPodJS QR Code simulator regression passed; captures: %s\n' \
        "${out_dir}"
}

main "$@"
