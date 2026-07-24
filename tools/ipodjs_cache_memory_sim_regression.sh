#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-${repo_root}/build-sim-ipod6g}"
out_dir="${2:-/tmp/ipodjs-cache-memory-regression}"
rockboxui="${build_dir}/rockboxui"
source_root="${build_dir}/simdisk"
runtime_root=""
sim_pid=""
sim_wid=""
dump_bmp=""
main_shell_pid="${BASHPID}"
keep_root="${IPODJS_CACHE_MEMORY_KEEP_ROOT:-0}"

cleanup()
{
    if [ "${BASHPID}" != "${main_shell_pid}" ]; then
        return
    fi

    set +e
    if [ -n "${sim_pid}" ] && kill -0 "${sim_pid}" >/dev/null 2>&1; then
        kill -CONT "${sim_pid}" >/dev/null 2>&1 || true
        kill "${sim_pid}" >/dev/null 2>&1 || true
        wait "${sim_pid}" 2>/dev/null || true
    fi
    if [ "${keep_root}" != "1" ] &&
       [ -n "${runtime_root}" ] && [ -d "${runtime_root}" ]; then
        rm -rf "${runtime_root}"
    fi
}

require_cmd()
{
    command -v "$1" >/dev/null 2>&1 || {
        printf "missing required command: %s\n" "$1" >&2
        exit 1
    }
}

tap_key()
{
    xdotool keydown --window "${sim_wid}" "$1"
    sleep "${3:-0.16}"
    xdotool keyup --window "${sim_wid}" "$1"
    sleep "${2:-0.18}"
}

hold_key()
{
    xdotool keydown --window "${sim_wid}" "$1"
    sleep "$2"
    xdotool keyup --window "${sim_wid}" "$1"
    sleep 0.4
}

capture()
{
    local name="$1"
    local source="${dump_bmp}"
    local snapshot="${runtime_root}/${name}.bmp"

    sleep 1
    for attempt in 1 2 3 4 5 6 7 8 9 10; do
        if [ -s "${source}" ] &&
           [ "$(stat -c %s "${source}")" -ge 153600 ]; then
            kill -STOP "${sim_pid}"
            cp "${source}" "${snapshot}"
            kill -CONT "${sim_pid}"
            if magick "${snapshot}" -resize 1280x960 \
                "${out_dir}/${name}.png" 2>/dev/null; then
                return
            fi
        fi
        sleep 0.08
    done

    printf "framebuffer capture timed out: %s\n" "${name}" >&2
    exit 1
}

assert_capture()
{
    local name="$1"
    local dimensions

    dimensions="$(magick identify -format '%wx%h' "${out_dir}/${name}.png")"
    if [ "${dimensions}" != "1280x960" ]; then
        printf "bad capture dimensions for %s: %s\n" \
            "${name}" "${dimensions}" >&2
        exit 1
    fi
}

prepare_root()
{
    local asset
    local config_file
    local directory
    local file

    runtime_root="$(mktemp -d)"
    dump_bmp="${runtime_root}/simdump.bmp"
    mkdir -p "${runtime_root}/.rockbox/ipodjs/qs" \
             "${runtime_root}/.rockbox/ipodjs/apple" \
             "${runtime_root}/.rockbox/ipodjs" \
             "${runtime_root}/.rockbox/fonts"
    for file in "${source_root}"/.rockbox/*; do
        [ -f "${file}" ] && cp "${file}" "${runtime_root}/.rockbox/"
    done
    for directory in langs icons backdrops wps themes fonts; do
        if [ -d "${source_root}/.rockbox/${directory}" ]; then
            mkdir -p "${runtime_root}/.rockbox/${directory}"
            cp -a "${source_root}/.rockbox/${directory}/." \
                  "${runtime_root}/.rockbox/${directory}/"
        fi
    done
    cp "${repo_root}"/assets/ipodjs/rockbox/qs/*.bmp \
       "${runtime_root}/.rockbox/ipodjs/qs/"
    for asset in "${repo_root}"/assets/ipodjs/rockbox/*.bmp; do
        cp "${asset}" "${runtime_root}/.rockbox/ipodjs/"
    done
    cp -a "${repo_root}/assets/ipodjs/apple/." \
          "${runtime_root}/.rockbox/ipodjs/apple/"
    for font in 14-Adobe-Helvetica-Bold.fnt \
                16-Adobe-Helvetica-Bold.fnt \
                18-Adobe-Helvetica-Bold.fnt \
                24-iLike.fnt; do
        cp "${repo_root}/assets/ipodjs/rockbox/${font}" \
           "${runtime_root}/.rockbox/ipodjs/${font}"
        cp "${repo_root}/assets/ipodjs/rockbox/${font}" \
           "${runtime_root}/.rockbox/fonts/${font}"
    done

    config_file="${runtime_root}/.rockbox/config.cfg"
    touch "${config_file}"
    awk '
        /^ui engine:/ { next }
        /^ui engine font scale:/ { next }
        /^start in screen:/ { next }
        { print }
        END {
            print "ui engine: ipodjs"
            print "ui engine font scale: normal"
            print "start in screen: root"
        }
    ' "${config_file}" >"${runtime_root}/config.cfg.new"
    mv "${runtime_root}/config.cfg.new" "${config_file}"
}

launch_sim()
{
    local ids

    for launch_attempt in 1 2 3 4 5; do
        (
            trap - EXIT INT TERM HUP
            cd "${build_dir}"
            exec env SDL_VIDEODRIVER=x11 \
                SDL_RENDER_DRIVER=software \
                ROCKPOD_SIM_PREVIEW_BMP="${dump_bmp}" \
                ROCKPOD_SIM_PREVIEW_INTERVAL_MS=250 \
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
        sim_wid=""
        if [ "${launch_attempt}" -lt 5 ]; then
            sleep 1
        fi
    done

    printf "unable to launch simulator\n" >&2
    exit 1
}

main()
{
    local header_left_green
    local header_right_green
    local header_height
    local header_separator_y
    local header_separator_full_width

    for command in magick stat xdotool; do
        require_cmd "${command}"
    done
    [ -x "${rockboxui}" ] || {
        printf "missing simulator: %s\n" "${rockboxui}" >&2
        exit 1
    }
    [ -d "${source_root}/.rockbox" ] || {
        printf "missing simulator root: %s\n" "${source_root}" >&2
        exit 1
    }
    [ -f "${repo_root}/assets/ipodjs/apple/status-header.apple.320x24x24.bmp" ] || {
        printf "missing verified Apple status header\n" >&2
        exit 1
    }

    mkdir -p "${out_dir}"
    rm -f "${out_dir}"/*.png "${out_dir}"/*.txt
    trap cleanup EXIT INT TERM HUP
    prepare_root
    launch_sim

    sleep 3
    hold_key w 1.3
    # The framebuffer exporter publishes on lcd_update. Paired navigation
    # redraws ensure each saved image contains a completed prior frame.
    tap_key KP_2
    tap_key KP_8
    capture quick-settings
    tap_key KP_8 3.0 0.25
    tap_key KP_5
    capture cache-row
    tap_key KP_2
    tap_key KP_8
    capture maintenance
    tap_key KP_5 0.3 0.2
    sleep 1.1
    tap_key KP_2
    tap_key KP_8
    capture refreshed
    tap_key KP_2
    tap_key KP_5
    tap_key w
    sleep 0.7
    capture restart-confirmation
    tap_key KP_2
    capture restart-cancelled

    kill -0 "${sim_pid}"
    for capture_name in quick-settings cache-row maintenance refreshed \
                        restart-confirmation restart-cancelled; do
        assert_capture "${capture_name}"
    done
    # Quick Settings owns the complete stock status bar. The left/header body
    # must not contain the stale battery baked into the old split backdrop,
    # while the one compositor-owned battery remains at the stock right edge.
    header_left_green="$(magick "${out_dir}/quick-settings.png" \
        -crop 1100x80+0+0 +repage \
        -fx 'g>r*1.12&&g>b*1.12?1:0' \
        -format '%[fx:round(mean*88000)]' info:)"
    header_right_green="$(magick "${out_dir}/quick-settings.png" \
        -crop 180x80+1100+0 +repage \
        -fx 'g>r*1.12&&g>b*1.12?1:0' \
        -format '%[fx:round(mean*14400)]' info:)"
    if [ "${header_left_green}" -ge 100 ] ||
       [ "${header_right_green}" -lt 2000 ] ||
       [ "${header_right_green}" -gt 4500 ]; then
        printf 'Quick Settings status bar does not contain exactly one right-edge battery (left=%s right=%s)\n' \
            "${header_left_green}" "${header_right_green}" >&2
        exit 1
    fi
    header_height="$(magick identify -format '%h' \
        "${repo_root}/assets/ipodjs/apple/status-header.apple.320x24x24.bmp")"
    # Captures are enlarged 4x with interpolation. Sample the center of the
    # final source row, not its blended lower edge.
    header_separator_y="$((header_height * 4 - 3))"
    header_separator_full_width="$(magick \
        "${out_dir}/quick-settings.png" -format \
        "%[fx:p{0,${header_separator_y}}.r<0.65&&p{640,${header_separator_y}}.r<0.65&&p{1279,${header_separator_y}}.r<0.65]" \
        info:)"
    if [ "${header_separator_full_width}" != "1" ]; then
        printf 'Quick Settings status bar does not span the full screen\n' >&2
        exit 1
    fi
    if cmp -s "${out_dir}/quick-settings.png" \
              "${out_dir}/cache-row.png"; then
        printf "Quick Settings did not scroll to the cache row\n" >&2
        exit 1
    fi
    if cmp -s "${out_dir}/maintenance.png" \
              "${out_dir}/restart-confirmation.png"; then
        printf "restart confirmation did not replace maintenance screen\n" >&2
        exit 1
    fi
    if cmp -s "${out_dir}/restart-confirmation.png" \
              "${out_dir}/restart-cancelled.png"; then
        printf "restart confirmation did not cancel\n" >&2
        exit 1
    fi

    printf "iPodJS cache/memory simulator regression passed; captures: %s\n" \
        "${out_dir}"
}

main "$@"
