#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-${repo_root}/build-sim-ipod6g}"
out_dir="${2:-/tmp/ipodjs-charging-regression}"
rockboxui="${build_dir}/rockboxui"
source_root="${build_dir}/simdisk"
runtime_root=""
sim_pid=""
sim_wid=""
dump_bmp=""

require_cmd()
{
    if ! command -v "$1" >/dev/null 2>&1; then
        printf "missing required command: %s\n" "$1" >&2
        exit 1
    fi
}

cleanup_sim()
{
    if [ -n "${sim_pid}" ] && kill -0 "${sim_pid}" >/dev/null 2>&1; then
        kill "${sim_pid}" >/dev/null 2>&1 || true
        wait "${sim_pid}" 2>/dev/null || true
    fi
    sim_pid=""
    sim_wid=""
}

cleanup()
{
    set +e
    cleanup_sim
    if [ "${IPODJS_CHARGING_KEEP_ROOT:-0}" = "1" ]; then
        printf "preserved simulator root: %s\n" "${runtime_root}"
        return
    fi
    if [ -n "${runtime_root}" ] && [ -d "${runtime_root}" ]; then
        rm -rf "${runtime_root}"
    fi
}

tap_key()
{
    xdotool keydown --window "${sim_wid}" "$1"
    # Native preview rendering can span more than one 50 ms action poll.
    # Keep ordinary synthetic presses below repeat timing but long enough
    # that the simulator cannot miss them while a pane is being drawn.
    sleep "${3:-0.16}"
    xdotool keyup --window "${sim_wid}" "$1"
    sleep "${2:-0.12}"
}

capture()
{
    local name="$1"
    local attempt
    local preview_tmp="${dump_bmp}.tmp.bmp"

    for attempt in 1 2 3 4 5 6 7 8 9 10; do
        if [ -s "${preview_tmp}" ] &&
           [ "$(stat -c %s "${preview_tmp}")" -ge 307338 ]; then
            sleep 0.04
            if magick "${preview_tmp}" "${out_dir}/${name}" 2>/dev/null; then
                return
            fi
        fi
        sleep 0.04
    done

    printf "simulator framebuffer dump timed out: %s\n" "${name}" >&2
    exit 1
}

capture_charge()
{
    local name="$1"
    local complete
    local attempt

    for attempt in 1 2 3 4 5 6 7 8; do
        capture "${name}"
        complete="$(magick "${out_dir}/${name}" -format \
            '%[fx:p{235,80}.r>0.70&&p{180,110}.r<0.55]' \
            info:)"
        if [ "${complete}" = "1" ]; then
            return
        fi
        sleep 0.04
    done

    printf "incomplete charging capture after %s attempts: %s\n" \
        "${attempt}" "${name}" >&2
    exit 1
}

capture_usb()
{
    local name="$1"
    local complete
    local attempt

    for attempt in 1 2 3 4 5 6 7 8; do
        capture "${name}"
        complete="$(magick "${out_dir}/${name}" -format \
            '%[fx:p{40,80}.b>p{40,80}.r&&p{160,60}.r>p{160,60}.b]' \
            info:)"
        if [ "${complete}" = "1" ]; then
            return
        fi
        # Reassert the simulator's USB-insert state if the synthetic event
        # landed while the preview renderer was busy.
        tap_key F11 0.2
    done

    printf "stock USB frame not visible after %s attempts: %s\n" \
        "${attempt}" "${name}" >&2
    exit 1
}

prepare_root()
{
    local mode="$1"
    local config_file

    runtime_root="$(mktemp -d)"
    dump_bmp="${runtime_root}/simdump.bmp"
    if [ "${IPODJS_CHARGING_VERBOSE:-0}" = "1" ]; then
        printf "simulator root: %s\n" "${runtime_root}"
    fi
    mkdir -p "${runtime_root}/.rockbox"
    (cd "${source_root}/.rockbox" && tar -cf - .) | \
        (cd "${runtime_root}/.rockbox" && tar -xf -)
    mkdir -p "${runtime_root}/.rockbox/ipodjs" \
             "${runtime_root}/.rockbox/fonts"
    for font in 12-Adobe-Helvetica.fnt 14-Adobe-Helvetica-Bold.fnt \
                16-Adobe-Helvetica-Bold.fnt 18-Adobe-Helvetica-Bold.fnt; do
        cp "${repo_root}/assets/ipodjs/rockbox/${font}" \
            "${runtime_root}/.rockbox/ipodjs/${font}"
        cp "${repo_root}/assets/ipodjs/rockbox/${font}" \
            "${runtime_root}/.rockbox/fonts/${font}"
    done
    for font in 35-Adobe-Helvetica-Bold.fnt 150-Adwaitapod-Icons.fnt; do
        cp "${repo_root}/fonts/${font}" \
            "${runtime_root}/.rockbox/fonts/${font}"
    done
    for directory in Music Playlists Podcasts Recordings; do
        if [ -e "${source_root}/${directory}" ]; then
            ln -s "${source_root}/${directory}" \
                "${runtime_root}/${directory}"
        fi
    done
    config_file="${runtime_root}/.rockbox/config.cfg"

    awk '
        /^ui engine:/ { next }
        /^ui engine dark mode:/ { next }
        /^start in screen:/ { next }
        { print }
        END {
            print "ui engine: ipodjs"
            print "ui engine dark mode: " mode
            print "start in screen: root"
        }
    ' mode="${mode}" "${config_file}" >"${runtime_root}/config.cfg.new"
    mv "${runtime_root}/config.cfg.new" "${config_file}"
}

launch_sim()
{
    local attempt
    local ids

    for attempt in 1 2 3 4 5; do
        pushd "${build_dir}" >/dev/null
        ROCKPOD_SIM_PREVIEW_BMP="${dump_bmp}" \
        ROCKPOD_SIM_PREVIEW_INTERVAL_MS=16 \
            ./rockboxui --zoom 1 --nobackground --root "${runtime_root}" &
        sim_pid=$!
        popd >/dev/null
        sleep 2
        ids="$(xdotool search --pid "${sim_pid}" 2>/dev/null || true)"
        sim_wid="${ids%%$'\n'*}"
        if [ -n "${sim_wid}" ]; then
            xdotool windowactivate "${sim_wid}"
            sleep 1
            return
        fi

        wait "${sim_pid}" 2>/dev/null || true
        sim_pid=""
        if [ "${IPODJS_CHARGING_VERBOSE:-0}" = "1" ]; then
            printf "simulator launch attempt %s failed; retrying\n" \
                "${attempt}" >&2
        fi
        sleep 1
    done

    printf "unable to launch simulator after 5 attempts\n" >&2
    exit 1
}

run_mode()
{
    local mode="$1"
    local prefix="$2"

    prepare_root "${mode}"
    launch_sim

    sleep 3
    capture "${prefix}-00-home.png"

    # Capture every fixed iPodJS root selection. One additional Down press at
    # the seventh item must clamp, proving the simulator is not exposing the
    # longer configurable Rockbox root list.
    for root_index in 1 2 3 4 5 6; do
        tap_key KP_2 0.65
        capture "${prefix}-20-root-${root_index}.png"
    done
    tap_key KP_2 0.65
    capture "${prefix}-26-root-end-clamped.png"

    # Restore the first item before exercising preview settling.
    for root_index in 1 2 3 4 5 6 7 8 9 10; do
        tap_key KP_8 0.25
    done

    # Settle on Extras, then scroll rapidly to Now Playing. Stock iPod OS
    # leaves the old pane in place until the wheel stops; only the left-side
    # highlight should move during the burst.
    for root_index in 1 2 3 4; do
        tap_key KP_2 0.6
    done
    sleep 1
    capture "${prefix}-33-preview-extras-settled.png"
    # Hold long enough to cross an action poll and leave a release poll
    # between pulses, while keeping the final capture inside the one-third
    # second preview-settle window.
    tap_key KP_2 0.07 0.12
    tap_key KP_2 0.02 0.12
    sleep 0.02
    capture "${prefix}-34-preview-fast-scroll.png"
    sleep 0.4
    capture "${prefix}-35-preview-now-playing-settled.png"
    for root_index in 1 2 3 4 5 6 7 8 9 10; do
        tap_key KP_8 0.25
    done
    sleep 0.4

    # Exercise Select and Back separately after restoring the first row.
    tap_key KP_2 0.4
    sleep 1
    capture "${prefix}-16-home-next.png"
    tap_key KP_5 0.6
    capture "${prefix}-17-child.png"
    tap_key KP_4 0.6
    capture "${prefix}-18-home-returned.png"
    tap_key KP_8 0.4
    capture "${prefix}-19-home-restored.png"
    sleep 1

    tap_key F11 1.5
    capture_usb "${prefix}-11-usb-data.png"
    tap_key F12 1
    sleep 1
    tap_key F11 1.5
    capture_usb "${prefix}-36-usb-data-repeat.png"
    tap_key F12 1
    sleep 2

    tap_key F10
    tap_key F11 1.1
    capture_charge "${prefix}-01-charge-a.png"
    sleep 0.12
    capture_charge "${prefix}-02-charge-b.png"
    sleep 0.12
    capture_charge "${prefix}-03-charge-c.png"
    sleep 0.12
    capture_charge "${prefix}-04-charge-d.png"
    sleep 0.12
    capture_charge "${prefix}-12-charge-e.png"
    sleep 0.12
    capture_charge "${prefix}-13-charge-f.png"
    sleep 0.12
    capture_charge "${prefix}-14-charge-g.png"
    sleep 0.12
    capture_charge "${prefix}-15-charge-h.png"
    sleep 3
    tap_key KP_8 0.3
    capture_charge "${prefix}-05-charged.png"

    tap_key h
    capture_charge "${prefix}-06-hold-enabled.png"
    capture_charge "${prefix}-07-hold-before-select.png"
    tap_key KP_5 0.3
    capture_charge "${prefix}-08-hold-after-select.png"
    tap_key h
    tap_key KP_5
    capture "${prefix}-09-hold-released-dismissed.png"
    tap_key F12 2
    tap_key F10

    tap_key F6 2
    capture_charge "${prefix}-10-main-power.png"
    tap_key KP_5
    tap_key F6 1

    cleanup_sim
    rm -rf "${runtime_root}"
    runtime_root=""
}

assert_captures()
{
    local mode
    local first_hash
    local unique
    local green_pixel
    local green_found
    local green_area
    local green_areas
    local unique_green_areas
    local frame
    local region
    local outside_delta
    local dismissed_hash
    local main_hash
    local usb_hash
    local usb_repeat_hash
    local home_hash
    local home_left_hash
    local next_left_hash
    local child_left_hash
    local returned_left_hash
    local restored_left_hash
    local root_last_left_hash
    local root_clamped_left_hash
    local extras_preview_hash
    local fast_preview_hash
    local settled_preview_hash
    local settled_extras_left_hash
    local fast_left_hash

    for mode in light dark; do
        first_hash="$(sha256sum "${out_dir}/${mode}-01-charge-a.png" | \
            awk '{ print $1 }')"
        home_hash="$(sha256sum "${out_dir}/${mode}-00-home.png" | \
            awk '{ print $1 }')"
        home_left_hash="$(magick "${out_dir}/${mode}-00-home.png" \
            -crop 159x220+0+20 rgba:- | sha256sum | awk '{ print $1 }')"
        next_left_hash="$(magick "${out_dir}/${mode}-16-home-next.png" \
            -crop 159x220+0+20 rgba:- | sha256sum | awk '{ print $1 }')"
        child_left_hash="$(magick "${out_dir}/${mode}-17-child.png" \
            -crop 159x220+0+20 rgba:- | sha256sum | awk '{ print $1 }')"
        returned_left_hash="$(magick \
            "${out_dir}/${mode}-18-home-returned.png" \
            -crop 159x220+0+20 rgba:- | sha256sum | awk '{ print $1 }')"
        restored_left_hash="$(magick \
            "${out_dir}/${mode}-19-home-restored.png" \
            -crop 159x220+0+20 rgba:- | sha256sum | awk '{ print $1 }')"
        root_last_left_hash="$(magick \
            "${out_dir}/${mode}-20-root-6.png" \
            -crop 159x220+0+20 rgba:- | sha256sum | awk '{ print $1 }')"
        root_clamped_left_hash="$(magick \
            "${out_dir}/${mode}-26-root-end-clamped.png" \
            -crop 159x220+0+20 rgba:- | sha256sum | awk '{ print $1 }')"
        if [ "${root_last_left_hash}" != "${root_clamped_left_hash}" ]; then
            printf "%s iPodJS root exposes more than seven items\n" \
                "${mode}" >&2
            exit 1
        fi
        if [ "${home_left_hash}" = "${next_left_hash}" ]; then
            printf "%s root selection did not advance\n" "${mode}" >&2
            exit 1
        fi
        if [ "${next_left_hash}" = "${child_left_hash}" ]; then
            printf "%s root Select did not open its child\n" "${mode}" >&2
            exit 1
        fi
        if [ "${next_left_hash}" != "${returned_left_hash}" ]; then
            printf "%s Back did not restore root selection\n" "${mode}" >&2
            exit 1
        fi
        if [ "${home_left_hash}" != "${restored_left_hash}" ]; then
            printf "%s reverse scroll did not restore root selection\n" \
                "${mode}" >&2
            exit 1
        fi
        extras_preview_hash="$(magick \
            "${out_dir}/${mode}-33-preview-extras-settled.png" \
            -crop 156x240+164+0 rgba:- | sha256sum | awk '{ print $1 }')"
        fast_preview_hash="$(magick \
            "${out_dir}/${mode}-34-preview-fast-scroll.png" \
            -crop 156x240+164+0 rgba:- | sha256sum | awk '{ print $1 }')"
        settled_preview_hash="$(magick \
            "${out_dir}/${mode}-35-preview-now-playing-settled.png" \
            -crop 156x240+164+0 rgba:- | sha256sum | awk '{ print $1 }')"
        settled_extras_left_hash="$(magick \
            "${out_dir}/${mode}-33-preview-extras-settled.png" \
            -crop 159x220+0+20 rgba:- | sha256sum | awk '{ print $1 }')"
        fast_left_hash="$(magick \
            "${out_dir}/${mode}-34-preview-fast-scroll.png" \
            -crop 159x220+0+20 rgba:- | sha256sum | awk '{ print $1 }')"
        if [ "${settled_extras_left_hash}" = "${fast_left_hash}" ]; then
            printf "%s fast scroll did not advance the root highlight\n" \
                "${mode}" >&2
            exit 1
        fi
        if [ "${extras_preview_hash}" != "${fast_preview_hash}" ]; then
            printf "%s fast scroll switched the preview before settling\n" \
                "${mode}" >&2
            exit 1
        fi
        if [ "${fast_preview_hash}" = "${settled_preview_hash}" ]; then
            printf "%s preview did not switch after scrolling settled\n" \
                "${mode}" >&2
            exit 1
        fi
        if [ "${first_hash}" = "${home_hash}" ]; then
            printf "%s first charging capture remained on home\n" \
                "${mode}" >&2
            exit 1
        fi
        unique="$({
            printf "%s\n" "${first_hash}"
            sha256sum "${out_dir}/${mode}-02-charge-b.png" \
                "${out_dir}/${mode}-03-charge-c.png" \
                "${out_dir}/${mode}-04-charge-d.png" \
                "${out_dir}/${mode}-12-charge-e.png" \
                "${out_dir}/${mode}-13-charge-f.png" \
                "${out_dir}/${mode}-14-charge-g.png" \
                "${out_dir}/${mode}-15-charge-h.png" | awk '{ print $1 }'
        } | sort -u | wc -l)"
        if [ "${unique}" -lt 5 ]; then
            printf "%s charge animation is not smooth (%s unique frames)\n" \
                "${mode}" "${unique}" >&2
            exit 1
        fi

        green_found=0
        green_areas=""
        for frame in 01-charge-a 02-charge-b 03-charge-c 04-charge-d \
                     12-charge-e 13-charge-f 14-charge-g 15-charge-h; do
            green_pixel="$(magick "${out_dir}/${mode}-${frame}.png" \
                -format '%[fx:p{105,110}.g>p{105,110}.r&&p{105,110}.g>p{105,110}.b]' \
                info:)"
            if [ "${green_pixel}" = "1" ]; then
                green_found=1
            fi
            green_area="$(magick "${out_dir}/${mode}-${frame}.png" \
                -crop 136x64+96+81 +repage \
                -fx 'g>r*1.08&&g>b*1.08?1:0' \
                -format '%[fx:round(mean*8704)]' info:)"
            green_areas="${green_areas} ${green_area}"
        done
        if [ "${green_found}" != "1" ]; then
            printf "%s charging frame does not contain the green well\n" \
                "${mode}" >&2
            exit 1
        fi
        unique_green_areas="$(printf '%s\n' ${green_areas} | \
            sort -u | wc -l)"
        if [ "${unique_green_areas}" -lt 4 ]; then
            printf "%s charge fill does not advance smoothly (%s extents)\n" \
                "${mode}" "${unique_green_areas}" >&2
            exit 1
        fi

        for region in 320x70+0+0 84x130+0+70 71x130+249+70 \
                      320x40+0+200; do
            outside_delta="$(magick compare -metric AE \
                "${out_dir}/${mode}-03-charge-c.png[${region}]" \
                "${out_dir}/${mode}-04-charge-d.png[${region}]" null: \
                2>&1 || true)"
            outside_delta="${outside_delta%% *}"
            if [ "${outside_delta}" != "0" ]; then
                printf "%s animation changed outside damage area %s (AE=%s)\n" \
                    "${mode}" "${region}" "${outside_delta}" >&2
                exit 1
            fi
        done

        dismissed_hash="$(sha256sum \
            "${out_dir}/${mode}-09-hold-released-dismissed.png" | \
            awk '{ print $1 }')"
        main_hash="$(sha256sum \
            "${out_dir}/${mode}-10-main-power.png" | awk '{ print $1 }')"
        usb_hash="$(sha256sum \
            "${out_dir}/${mode}-11-usb-data.png" | awk '{ print $1 }')"
        usb_repeat_hash="$(sha256sum \
            "${out_dir}/${mode}-36-usb-data-repeat.png" | awk '{ print $1 }')"
        if [ "${usb_hash}" = "${home_hash}" ] ||
           [ "${usb_repeat_hash}" = "${home_hash}" ]; then
            printf "%s USB data connection did not replace the dashboard\n" \
                "${mode}" >&2
            exit 1
        fi
        if [ "${main_hash}" = "${dismissed_hash}" ]; then
            printf "%s main-power charging screen did not open\n" \
                "${mode}" >&2
            exit 1
        fi
        for frame in 01-charge-a 02-charge-b 03-charge-c 04-charge-d \
                     12-charge-e 13-charge-f 14-charge-g 15-charge-h \
                     05-charged; do
            first_hash="$(sha256sum \
                "${out_dir}/${mode}-${frame}.png" | awk '{ print $1 }')"
            if [ "${usb_hash}" = "${first_hash}" ]; then
                printf "%s USB data handoff showed a charging frame\n" \
                    "${mode}" >&2
                exit 1
            fi
        done
    done

    if rg -n -i 'charg(e|ing)[^\n]*(bmp|png|svg|bitmap)' \
        "${repo_root}/apps/gui/ipodjs_ui.c" >/dev/null; then
        printf "charging implementation references an image asset\n" >&2
        exit 1
    fi

    printf "iPodJS charging simulator regression passed; captures: %s\n" \
        "${out_dir}"
}

main()
{
    require_cmd magick
    require_cmd rg
    require_cmd sha256sum
    require_cmd stat
    require_cmd xdotool

    if [ ! -x "${rockboxui}" ] || [ ! -d "${source_root}/.rockbox" ]; then
        printf "missing ipod6g simulator build under %s\n" \
            "${build_dir}" >&2
        exit 1
    fi

    mkdir -p "${out_dir}"
    rm -f "${out_dir}"/*.png
    trap cleanup EXIT INT TERM HUP

    run_mode off light
    run_mode on dark
    assert_captures
}

main "$@"
