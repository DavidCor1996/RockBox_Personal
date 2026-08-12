#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir_arg="${1:-${repo_root}/build-sim-ipod6g}"
build_dir="$(cd "${build_dir_arg}" && pwd)"
out_dir="${2:-/tmp/ipodjs-navigation-regression}"
rockboxui="${build_dir}/rockboxui"
source_root="${build_dir}/simdisk"
runtime_root=""
sim_pid=""
sim_wid=""
dump_bmp=""

cleanup()
{
    local status=$?

    set +e
    if [ -n "${sim_pid}" ] && kill -0 "${sim_pid}" >/dev/null 2>&1; then
        kill "${sim_pid}" >/dev/null 2>&1
        wait "${sim_pid}" 2>/dev/null
    fi
    if [ -n "${runtime_root}" ] &&
       [ -s "${runtime_root}/.rockbox/ipodjs-trace.tsv" ]; then
        mkdir -p "${out_dir}"
        cp "${runtime_root}/.rockbox/ipodjs-trace.tsv" \
           "${out_dir}/ipodjs-trace.tsv"
    fi
    if [ "${IPODJS_NAVIGATION_KEEP_ROOT:-0}" != "1" ] &&
       [ -n "${runtime_root}" ]; then
        rm -rf "${runtime_root}"
    elif [ -n "${runtime_root}" ]; then
        printf "preserved simulator root: %s\n" "${runtime_root}"
    fi
    return "${status}"
}

cleanup_sim()
{
    if [ -n "${sim_pid}" ] && kill -0 "${sim_pid}" >/dev/null 2>&1; then
        kill "${sim_pid}" >/dev/null 2>&1 || true
        wait "${sim_pid}" 2>/dev/null || true
    fi
    sim_pid=""
}

tap_key()
{
    xdotool keydown --window "${sim_wid}" "$1"
    sleep "${3:-0.16}"
    xdotool keyup --window "${sim_wid}" "$1"
    sleep "${2:-0.20}"
}

hold_key()
{
    xdotool keydown --window "${sim_wid}" "$1"
    sleep "$2"
    xdotool keyup --window "${sim_wid}" "$1"
    sleep "${3:-0.35}"
}

capture()
{
    local name="$1"
    local attempt
    local preview_bmp="${dump_bmp}"
    local capture_png="${out_dir}/${name}.png"

    # The simulator publishes every completed render in regression mode.  The
    # final full LCD update therefore replaces intermediate dirty rectangles
    # even when the destination screen becomes static (notably Hold).
    sleep "${IPODJS_NAVIGATION_CAPTURE_DELAY:-0.35}"
    for attempt in 1 2 3 4 5 6 7 8 9 10; do
        if [ -s "${preview_bmp}" ] &&
           [ "$(stat -c %s "${preview_bmp}")" -ge 307200 ]; then
            if magick "${preview_bmp}" "${capture_png}" 2>/dev/null; then
                return
            fi
        fi
        sleep 0.05
    done
    printf "framebuffer capture timed out: %s\n" "${name}" >&2
    exit 1
}

hold_cycle()
{
    local name="$1"
    local expected="$2"
    local before

    if [ "${IPODJS_NAVIGATION_LYRICS_HOME_STORM:-0}" = "1" ]; then
        return
    fi

    before="$(trace_last_sequence)"
    touch "${runtime_root}/hold.gate"
    wait_for_trace_after "Lockscreen" "${before}"
    capture "${name}-hold"
    before="$(trace_last_sequence)"
    rm -f "${runtime_root}/hold.gate"
    wait_for_trace_after "${expected}" "${before}"
    capture "${name}-unlocked"
}

prepare_root()
{
    local directory
    local file
    local config_file
    local dark_mode="${IPODJS_NAVIGATION_DARK_MODE:-off}"
    local accent="${IPODJS_NAVIGATION_ACCENT:-blue}"
    local extras_pane="${IPODJS_NAVIGATION_EXTRAS_PANE:-clock}"

    runtime_root="$(mktemp -d "${repo_root}/.tmp-ipodjs-nav.XXXXXX")"
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
    # Exercise the current source assets even when simdisk predates the most
    # recent private Apple extraction/package build.
    if [ -d "${repo_root}/assets/ipodjs/apple" ]; then
        mkdir -p "${runtime_root}/.rockbox/ipodjs/apple"
        cp -a "${repo_root}/assets/ipodjs/apple/." \
              "${runtime_root}/.rockbox/ipodjs/apple/"
    fi
    if [ -d "${repo_root}/assets/ipodjs/rockbox/sitekick" ]; then
        mkdir -p "${runtime_root}/.rockbox/sitekick"
        cp -a "${repo_root}/assets/ipodjs/rockbox/sitekick/." \
              "${runtime_root}/.rockbox/sitekick/"
    fi
    if [ "${IPODJS_NAVIGATION_VIDEO:-0}" = "1" ]; then
        mkdir -p "${runtime_root}/.rockbox/rocks/viewers"
        cp "${build_dir}/apps/plugins/openh264_player.rock" \
           "${runtime_root}/.rockbox/rocks/viewers/openh264_player.rock"
    fi
    if [ -n "${IPODJS_NAVIGATION_MUSIC_ROOT:-}" ]; then
        ln -s "${IPODJS_NAVIGATION_MUSIC_ROOT}" "${runtime_root}/Music"
    elif [ "${IPODJS_NAVIGATION_LONG_TITLE:-0}" = "1" ] ||
       [ "${IPODJS_NAVIGATION_SHORT_TRACK:-0}" = "1" ] ||
       [ "${IPODJS_NAVIGATION_ALBUM_ART:-0}" = "1" ]; then
        local long_track
        local long_tmp

        cp -a "${source_root}/Music" "${runtime_root}/Music"
        long_track="$(find "${runtime_root}/Music" -type f -name '*.mp3' \
            | sort | head -n 1)"
        [ -n "${long_track}" ] || {
            printf "long-title fixture has no MP3 track\n" >&2
            exit 1
        }
        if [ "${IPODJS_NAVIGATION_LONG_TITLE:-0}" = "1" ]; then
            long_tmp="${long_track%.*}.long-title.mp3"
            ffmpeg -loglevel error -y -i "${long_track}" -c copy \
                -metadata title="A Very Long Apple Now Playing Title That Must Scroll Without Truncation" \
                "${long_tmp}"
            mv "${long_tmp}" "${long_track}"
        fi
        if [ "${IPODJS_NAVIGATION_SHORT_TRACK:-0}" = "1" ]; then
            long_tmp="${long_track%.*}.short.mp3"
            ffmpeg -loglevel error -y -i "${long_track}" -t 12 -c copy \
                "${long_tmp}"
            mv "${long_tmp}" "${long_track}"
        fi
    else
        ln -s "${source_root}/Music" "${runtime_root}/Music"
    fi
    if [ -n "${IPODJS_NAVIGATION_DB_SNAPSHOT:-}" ]; then
        for file in "${IPODJS_NAVIGATION_DB_SNAPSHOT}"/.rockbox/database*.tcd; do
            [ -f "${file}" ] && cp "${file}" "${runtime_root}/.rockbox/"
        done
    fi
    mkdir -p "${runtime_root}/.rockbox/tagcache_backup"
    for file in "${runtime_root}"/.rockbox/database*.tcd; do
        case "$(basename "${file}")" in
            database_tmp.tcd|database_commit.tcd|database_hostcommit.tcd)
                ;;
            *)
                [ -f "${file}" ] && cp "${file}" \
                    "${runtime_root}/.rockbox/tagcache_backup/"
                ;;
        esac
    done
    if [ "${IPODJS_NAVIGATION_VIDEO:-0}" = "1" ]; then
        if [ ! -f "${source_root}/Videos/Downloaded/segtest.rvp" ]; then
            printf "missing simulator video fixture: %s\n" \
                "${source_root}/Videos/Downloaded/segtest.rvp" >&2
            exit 1
        fi
        ln -s "${source_root}/Videos" "${runtime_root}/Videos"
    fi
    if [ "${IPODJS_NAVIGATION_ALBUM_ART:-0}" = "1" ]; then
        while IFS= read -r -d '' file; do
            local cover_bmp="${file%/*}/cover.138x138.bmp"
            magick "${file}" -resize '138x138^' -gravity center \
                -extent 138x138 "BMP3:${cover_bmp}"
        done < <(find "${runtime_root}/Music" -type f -iname 'cover.jpg' \
            -print0)

        local preview_cover
        preview_cover="$(find "${runtime_root}/Music" -type f \
            -iname 'cover.jpg' | sort | head -n 1)"
        if [ -n "${preview_cover}" ]; then
            mkdir -p "${runtime_root}/.rockbox/albumlist/thumbs" \
                     "${runtime_root}/.rockbox/albumlist/slides"
            magick "${preview_cover}" -resize '40x40^' -gravity center \
                -extent 40x40 \
                "BMP3:${runtime_root}/.rockbox/albumlist/thumbs/navigation.bmp"
            magick "${preview_cover}" -resize '384x384^' -gravity center \
                -extent 384x384 \
                "BMP3:${runtime_root}/.rockbox/albumlist/slides/navigation.bmp"
            printf '%s\n' \
                '# rockpod albumlist v1' \
                $'album_id\tthumb\tslide\tartist\talbum\tgroup_key\tdevice_dirs' \
                $'navigation\tthumbs/navigation.bmp\tslides/navigation.bmp\tThe Beatles\tA Hard Day\x27s Night\tThe Beatles/A Hard Day\x27s Night\tMusic/A Hard Day\x27s Night' \
                >"${runtime_root}/.rockbox/albumlist/index.tsv"
        fi
    fi
    config_file="${runtime_root}/.rockbox/config.cfg"
    awk -v dark_mode="${dark_mode}" -v accent="${accent}" \
        -v extras_pane="${extras_pane}" '
        /^ui engine:/ { next }
        /^ui engine accent:/ { next }
        /^ui engine dark mode:/ { next }
        /^ui engine extras pane:/ { next }
        /^ui engine hold effect:/ { next }
        /^start in screen:/ { next }
        /^(tagcache_autoupdate|autoupdate):/ { next }
        /^resume:/ { next }
        /^repeat:/ { next }
        /^shuffle:/ { next }
        { print }
        END {
            print "ui engine: ipodjs"
            print "ui engine accent: " accent
            print "ui engine dark mode: " dark_mode
            print "ui engine extras pane: " extras_pane
            print "ui engine hold effect: lockscreen"
            print "start in screen: root"
            print "tagcache_autoupdate: off"
            print "resume: off"
            print "repeat: off"
            print "shuffle: off"
        }
    ' "${config_file}" >"${runtime_root}/config.cfg.new"
    mv "${runtime_root}/config.cfg.new" "${config_file}"
    rm -f "${runtime_root}/.rockbox/ipodjs-trace.tsv"
}

configure_optional_wps_status()
{
    local before
    local repeat_mode="${IPODJS_NAVIGATION_REPEAT:-off}"

    if [ "${IPODJS_NAVIGATION_SHUFFLE:-off}" != "on" ] &&
       [ "${repeat_mode}" = "off" ]; then
        return
    fi

    before="$(trace_last_sequence)"
    hold_key w 1.3
    wait_for_trace_after "Quick Settings" "${before}"
    tap_key KP_2
    tap_key KP_2
    tap_key KP_2
    if [ "${IPODJS_NAVIGATION_SHUFFLE:-off}" = "on" ]; then
        tap_key KP_5
    fi
    tap_key KP_2
    case "${repeat_mode}" in
        off) ;;
        all) tap_key KP_5 ;;
        one) tap_key KP_5; tap_key KP_5 ;;
        *)
            printf "unsupported repeat test mode: %s\n" "${repeat_mode}" >&2
            exit 1
            ;;
    esac
    before="$(trace_last_sequence)"
    tap_key w
    wait_for_trace_after "Home" "${before}"
}

launch_sim()
{
    local attempt
    local ids
    local window_attempt

    # KWin/Xwayland can briefly reject a new X11 client immediately after a
    # previous SDL simulator exits (SDL reports "x11 not available").
    # Spacing launches avoids Rockbox's pre-window panic path and is unrelated
    # to firmware behavior.
    sleep 5
    for attempt in 1 2 3 4 5; do
        (
            trap - EXIT INT TERM HUP
            cd "${build_dir}"
            if [ "${IPODJS_NAVIGATION_GDB:-0}" = "1" ]; then
                exec env -u IPODJS_NAVIGATION_SHUFFLE \
                    -u IPODJS_NAVIGATION_REPEAT gdb -q -batch \
                    -ex 'set confirm off' \
                    -ex 'set env SDL_AUDIODRIVER dummy' \
                    -ex 'set env SDL_VIDEODRIVER x11' \
                    -ex 'set env SDL_RENDER_DRIVER software' \
                    -ex 'handle SIGUSR1 nostop noprint pass' \
                    -ex 'set env ROCKPOD_SIM_IPODJS_TRACE 1' \
                    -ex "set env ROCKPOD_SIM_PREVIEW_BMP ${dump_bmp}" \
                    -ex "set env ROCKPOD_SIM_HOLD_GATE ${runtime_root}/hold.gate" \
                    -ex "set env ROCKPOD_SIM_PLAY_HOLD_GATE ${runtime_root}/play-hold.gate" \
                    -ex 'set env ROCKPOD_SIM_PREVIEW_INTERVAL_MS 0' \
                    -ex run --args ./rockboxui --zoom 1 --nobackground \
                    --root "${runtime_root}"
            else
                exec env -u IPODJS_NAVIGATION_SHUFFLE \
                    -u IPODJS_NAVIGATION_REPEAT \
                    SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=x11 \
                    SDL_RENDER_DRIVER=software \
                    ROCKPOD_SIM_IPODJS_TRACE=1 \
                    ROCKPOD_SIM_PREVIEW_BMP="${dump_bmp}" \
                    ROCKPOD_SIM_HOLD_GATE="${runtime_root}/hold.gate" \
                    ROCKPOD_SIM_PLAY_HOLD_GATE="${runtime_root}/play-hold.gate" \
                    ROCKPOD_SIM_PREVIEW_INTERVAL_MS=0 \
                    ./rockboxui --zoom 1 --nobackground \
                    --root "${runtime_root}"
            fi
        ) &
        sim_pid=$!

        for window_attempt in 1 2 3 4 5; do
            sleep 1
            if [ "${IPODJS_NAVIGATION_GDB:-0}" = "1" ]; then
                ids="$(xdotool search --name 'iPod 6G' 2>/dev/null || true)"
            else
                ids="$(xdotool search --pid "${sim_pid}" 2>/dev/null || true)"
            fi
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

wait_for_home()
{
    local attempt
    local trace="${runtime_root}/.rockbox/ipodjs-trace.tsv"

    # A cold simulator may spend several seconds loading codecs, fonts, and
    # tagcache state after its SDL window appears.  Do not inject Hold/Menu
    # into the boot splash and accidentally validate a stale framebuffer.
    for attempt in $(seq 1 60); do
        if [ -s "${trace}" ] && grep -q $'\tscreen\tHome\t' "${trace}"; then
            sleep 0.35
            return
        fi
        if ! kill -0 "${sim_pid}" >/dev/null 2>&1; then
            break
        fi
        sleep 0.5
    done
    printf "simulator did not reach iPodJS Home\n" >&2
    exit 1
}

trace_last_sequence()
{
    local trace="${runtime_root}/.rockbox/ipodjs-trace.tsv"

    if [ ! -s "${trace}" ]; then
        printf '0\n'
        return
    fi
    tail -n 1 "${trace}" | cut -f 1
}

wait_for_trace_after()
{
    local expected="$1"
    local before="$2"
    local trace="${runtime_root}/.rockbox/ipodjs-trace.tsv"
    local attempt

    for attempt in $(seq 1 60); do
        if [ -s "${trace}" ] && awk -F '\t' -v before="${before:-0}" \
            -v expected="${expected}" \
            '$1 > before && $4 == expected { found = 1 }
             END { exit found ? 0 : 1 }' "${trace}"; then
            sleep 0.25
            return
        fi
        if ! kill -0 "${sim_pid}" >/dev/null 2>&1; then
            break
        fi
        sleep 0.1
    done
    printf "simulator did not reach %s after trace %s\n" \
        "${expected}" "${before}" >&2
    exit 1
}

wait_for_new_list_after()
{
    local before="$1"
    local previous="$2"
    local trace="${runtime_root}/.rockbox/ipodjs-trace.tsv"
    local attempt

    for attempt in $(seq 1 60); do
        if [ -s "${trace}" ] && awk -F '\t' -v before="${before:-0}" \
            -v previous="${previous}" \
            '$1 > before && $3 == "list" && $4 != previous { found = 1 }
             END { exit found ? 0 : 1 }' "${trace}"; then
            sleep 0.25
            return
        fi
        sleep 0.1
    done
    printf "simulator did not enter a new list after %s\n" "${previous}" >&2
    exit 1
}

wait_for_recovered_list_after()
{
    local before="$1"
    local previous="$2"
    local trace="${runtime_root}/.rockbox/ipodjs-trace.tsv"
    local attempt

    # A transaction recovery scans the whole configured music root before it
    # can publish a coherent replacement database.  Real-library simulator
    # runs therefore need a substantially longer bound than an ordinary list
    # transition.
    for attempt in $(seq 1 1800); do
        if [ -s "${trace}" ] && awk -F '\t' -v before="${before:-0}" \
            -v previous="${previous}" \
            '$1 > before && $3 == "list" && $4 != previous { found = 1 }
             END { exit found ? 0 : 1 }' "${trace}"; then
            sleep 0.25
            return
        fi
        if ! kill -0 "${sim_pid}" >/dev/null 2>&1; then
            break
        fi
        sleep 0.1
    done
    printf "simulator did not recover a list after %s\n" "${previous}" >&2
    exit 1
}

wait_for_trace_kind_after()
{
    local kind="$1"
    local before="$2"
    local trace="${runtime_root}/.rockbox/ipodjs-trace.tsv"
    local attempt

    for attempt in $(seq 1 60); do
        if [ -s "${trace}" ] && awk -F '\t' -v before="${before:-0}" \
            -v kind="${kind}" \
            '$1 > before && $3 == kind { found = 1 }
             END { exit found ? 0 : 1 }' "${trace}"; then
            sleep 0.25
            return
        fi
        sleep 0.1
    done
    printf "simulator did not emit trace kind %s after %s\n" \
        "${kind}" "${before}" >&2
    exit 1
}

wait_for_audio_after()
{
    local expected="$1"
    local before="$2"
    local trace="${runtime_root}/.rockbox/ipodjs-trace.tsv"
    local attempt

    for attempt in $(seq 1 60); do
        if [ -s "${trace}" ] && awk -F '\t' -v before="${before:-0}" \
            -v expected="${expected}" \
            '$1 > before && $13 == expected { found = 1 }
             END { exit found ? 0 : 1 }' "${trace}"; then
            sleep 0.25
            return
        fi
        sleep 0.1
    done
    printf "simulator did not reach audio state %s after trace %s\n" \
        "${expected}" "${before}" >&2
    exit 1
}

trace_last_name_of_kind()
{
    local kind="$1"
    local trace="${runtime_root}/.rockbox/ipodjs-trace.tsv"

    awk -F '\t' -v kind="${kind}" '$3 == kind { name = $4 }
        END { print name }' "${trace}"
}

run_music_fd_stress()
{
    local music_name="$1"
    local artist_name="$2"
    local album_name="$3"
    local song_name="$4"
    local cycles="${IPODJS_NAVIGATION_FD_STRESS_CYCLES:-0}"
    local report="${out_dir}/fd-stress.tsv"
    local baseline
    local current
    local before
    local cycle
    local fault_fds
    local fault_name

    if [ "${cycles}" -le 0 ]; then
        return
    fi

    baseline="$(find "/proc/${sim_pid}/fd" -mindepth 1 -maxdepth 1 | wc -l)"
    printf 'cycle\topen_fds\n0\t%s\n' "${baseline}" >"${report}"

    for cycle in $(seq 1 "${cycles}"); do
        before="$(trace_last_sequence)"
        tap_key KP_5 0.35
        wait_for_trace_after "${music_name}" "${before}"
        if { [ "${IPODJS_NAVIGATION_TAGCACHE_FAULT:-0}" = "1" ] ||
             [ "${IPODJS_NAVIGATION_TAGCACHE_COMMIT_FAULT:-0}" = "1" ] ||
             [ "${IPODJS_NAVIGATION_TAGCACHE_STORAGE_FAULT:-0}" = "1" ] ||
             [ "${IPODJS_NAVIGATION_TAGCACHE_STALE_FAULT:-0}" = "1" ]; } &&
           [ "${cycle}" -eq 1 ]; then
            if [ "${IPODJS_NAVIGATION_TAGCACHE_COMMIT_FAULT:-0}" = "1" ]; then
                touch "${runtime_root}/.rockbox/tagcache-commit-fail.gate"
            elif [ "${IPODJS_NAVIGATION_TAGCACHE_STORAGE_FAULT:-0}" = "1" ]; then
                if [ "${IPODJS_NAVIGATION_TAGCACHE_MENU_STORM:-0}" = "1" ]; then
                    touch "${runtime_root}/.rockbox/tagcache-storage-persistent.gate"
                    touch "${runtime_root}/.rockbox/ipodjs-menu-storm.gate"
                else
                    touch "${runtime_root}/.rockbox/tagcache-storage-stuck.gate"
                fi
                if [ "${IPODJS_NAVIGATION_TAGCACHE_SCAN_STALL:-0}" = "1" ]; then
                    touch "${runtime_root}/.rockbox/tagcache-scan-updating.gate"
                fi
            elif [ "${IPODJS_NAVIGATION_TAGCACHE_STALE_FAULT:-0}" = "1" ]; then
                touch "${runtime_root}/.rockbox/tagcache-stale-search.gate"
            else
                touch "${runtime_root}/.rockbox/tagcache-open-fail.gate"
            fi
            before="$(trace_last_sequence)"
            tap_key KP_5 0.35
            wait_for_trace_after "Loading Music" "${before}"
            if [ "${IPODJS_NAVIGATION_TAGCACHE_COMMIT_FAULT:-0}" = "1" ]; then
                wait_for_recovered_list_after "${before}" "${music_name}"
                if [ -e "${runtime_root}/.rockbox/database_tmp.tcd" ] ||
                   [ -e "${runtime_root}/.rockbox/database_commit.tcd" ]; then
                    printf 'transaction recovery left commit artifacts\n' >&2
                    exit 1
                fi
                if ! awk -F '\t' -v before="${before}" \
                    '$1 > before && $4 == "Loading Music" &&
                     ($13 + 0) == 0 { stopped = 1 }
                     END { exit stopped ? 1 : 0 }' \
                    "${runtime_root}/.rockbox/ipodjs-trace.tsv"; then
                    printf 'transaction recovery stopped active playback\n' >&2
                    exit 1
                fi
            elif [ "${IPODJS_NAVIGATION_TAGCACHE_STORAGE_FAULT:-0}" = "1" ] ||
                 [ "${IPODJS_NAVIGATION_TAGCACHE_STALE_FAULT:-0}" = "1" ]; then
                wait_for_recovered_list_after "${before}" "${music_name}"
                if [ -e "${runtime_root}/.rockbox/tagcache-storage-stuck.gate" ] ||
                   [ -e "${runtime_root}/.rockbox/tagcache-stale-search.gate" ] ||
                   [ -e "${runtime_root}/.rockbox/tagcache-scan-updating.gate" ]; then
                    printf 'runtime recovery left a fault gate active\n' >&2
                    exit 1
                fi
                if awk -F '\t' -v before="${before}" \
                    '$1 > before && $4 == "Loading Music" &&
                     ($13 + 0) == 0 { stopped = 1 }
                     END { exit stopped ? 0 : 1 }' \
                    "${runtime_root}/.rockbox/ipodjs-trace.tsv"; then
                    printf 'runtime recovery stopped active playback\n' >&2
                    exit 1
                fi
            else
                wait_for_new_list_after "${before}" "${music_name}"
            fi
            if [ "${IPODJS_NAVIGATION_TAGCACHE_MENU_STORM:-0}" = "1" ]; then
                wait_for_trace_after "Menu Storm" "${before}"
                wait_for_trace_after "Home" "${before}"
                if awk -F '\t' -v before="${before}" \
                    '$1 > before && $4 == "Loading Music" &&
                     $5 == "failed" { failed = 1 }
                     END { exit failed ? 0 : 1 }' \
                    "${runtime_root}/.rockbox/ipodjs-trace.tsv"; then
                    printf 'Menu storm aborted tagcache recovery\n' >&2
                    exit 1
                fi
                current="$(find "/proc/${sim_pid}/fd" -mindepth 1 \
                    -maxdepth 1 | wc -l)"
                printf '%s\t%s\n' "${cycle}" "${current}" >>"${report}"
                if [ "${current}" -gt $((baseline + 1)) ]; then
                    printf 'Menu-storm recovery leaked descriptors: %s -> %s\n' \
                        "${baseline}" "${current}" >&2
                    exit 1
                fi
                return
            fi
            fault_fds="$(find "/proc/${sim_pid}/fd" -mindepth 1 \
                -maxdepth 1 | wc -l)"
            if [ "${fault_fds}" -gt $((baseline + 1)) ]; then
                printf 'failed tagcache open leaked a descriptor: %s -> %s\n' \
                    "${baseline}" "${fault_fds}" >&2
                exit 1
            fi
            fault_name="$(trace_last_name_of_kind list)"
            # Let the recovered list consume the final release event and
            # complete its transition before injecting the back action.
            sleep 0.6
            before="$(trace_last_sequence)"
            tap_key KP_Decimal 0.4
            wait_for_trace_after "${music_name}" "${before}"
            before="$(trace_last_sequence)"
            tap_key KP_Decimal 0.4
            wait_for_trace_after "Home" "${before}"
            current="$(find "/proc/${sim_pid}/fd" -mindepth 1 \
                -maxdepth 1 | wc -l)"
            printf '%s\t%s\n' "${cycle}" "${current}" >>"${report}"
            if [ "${current}" -gt $((baseline + 1)) ]; then
                printf 'tagcache recovery leaked descriptors: %s -> %s (%s)\n' \
                    "${baseline}" "${current}" "${fault_name}" >&2
                exit 1
            fi
            return
        fi
        before="$(trace_last_sequence)"
        tap_key KP_5 0.35
        wait_for_trace_after "${artist_name}" "${before}"
        before="$(trace_last_sequence)"
        tap_key KP_5 0.35
        wait_for_trace_after "${album_name}" "${before}"
        before="$(trace_last_sequence)"
        tap_key KP_5 0.35
        wait_for_trace_after "${song_name}" "${before}"

        before="$(trace_last_sequence)"
        tap_key KP_Decimal 0.3
        wait_for_trace_after "${album_name}" "${before}"
        before="$(trace_last_sequence)"
        tap_key KP_Decimal 0.3
        wait_for_trace_after "${artist_name}" "${before}"
        before="$(trace_last_sequence)"
        tap_key KP_Decimal 0.3
        wait_for_trace_after "${music_name}" "${before}"
        before="$(trace_last_sequence)"
        tap_key KP_Decimal 0.35
        wait_for_trace_after "Home" "${before}"

        current="$(find "/proc/${sim_pid}/fd" -mindepth 1 -maxdepth 1 | wc -l)"
        printf '%s\t%s\n' "${cycle}" "${current}" >>"${report}"
    done

    current="$(tail -n 1 "${report}" | cut -f 2)"
    if [ "${current}" -gt $((baseline + 1)) ]; then
        printf 'music navigation leaked descriptors: %s -> %s\n' \
            "${baseline}" "${current}" >&2
        exit 1
    fi
}

run_album_artist_switch_stress()
{
    local music_name="$1"
    local cycles="${IPODJS_NAVIGATION_RAPID_SWITCH_CYCLES:-0}"
    local before
    local list_name
    local cycle
    local pulse

    if [ "${cycles}" -le 0 ]; then
        return
    fi

    # Reproduce the hardware report verbatim while playback remains active:
    # Home -> Music -> Albums, scroll, back, Artist, then repeat rapidly.
    before="$(trace_last_sequence)"
    tap_key KP_5 0.25
    wait_for_trace_after "${music_name}" "${before}"

    # Selection history is intentionally persistent. Clamp to the first root
    # item, then move to the deterministic stock tagnavi Albums entry (index 2).
    for pulse in $(seq 1 20); do
        tap_key KP_8 0.025 0.025
    done
    tap_key KP_2 0.04 0.04
    tap_key KP_2 0.04 0.04

    for cycle in $(seq 1 "${cycles}"); do
        before="$(trace_last_sequence)"
        tap_key KP_5 0.12
        wait_for_new_list_after "${before}" "${music_name}"
        list_name="$(trace_last_name_of_kind list)"
        for pulse in $(seq 1 14); do
            tap_key KP_2 0.018 0.018
        done
        before="$(trace_last_sequence)"
        tap_key KP_Decimal 0.25 0.18
        wait_for_trace_after "${music_name}" "${before}"

        # Albums is index 2 and Artist is index 1 in the deployed stock menu.
        tap_key KP_8 0.04 0.04
        before="$(trace_last_sequence)"
        tap_key KP_5 0.12
        wait_for_new_list_after "${before}" "${music_name}"
        list_name="$(trace_last_name_of_kind list)"
        for pulse in $(seq 1 10); do
            tap_key KP_2 0.018 0.018
        done
        before="$(trace_last_sequence)"
        tap_key KP_Decimal 0.25 0.18
        wait_for_trace_after "${music_name}" "${before}"

        # Restore Albums for the next cycle.
        tap_key KP_2 0.04 0.04
    done

    before="$(trace_last_sequence)"
    tap_key KP_Decimal 0.18
    wait_for_trace_after "Home" "${before}"
}

run_music_journey()
{
    local pulse
    local before
    local music_name
    local artist_name
    local album_name
    local song_name
    local wps_name

    capture "00-home"
    hold_cycle "00-home" "Home"

    if [ "${IPODJS_NAVIGATION_COVERFLOW:-0}" = "1" ]; then
        before="$(trace_last_sequence)"
        tap_key KP_5 0.8
        sleep 8
        capture "00-coverflow-idle"
        tap_key KP_2 0.15
        capture "00-coverflow-scroll"
        tap_key KP_5 0.8
        capture "00-coverflow-tracks"
        tap_key KP_Decimal 0.5
        capture "00-coverflow-return"
        before="$(trace_last_sequence)"
        tap_key KP_Decimal 0.8
        wait_for_trace_after "Home" "${before}"
        capture "00-coverflow-home"
        sleep 0.5
    fi

    if [ "${IPODJS_NAVIGATION_SLIDERS:-0}" = "1" ]; then
        before="$(trace_last_sequence)"
        hold_key w 1.3
        wait_for_trace_after "Quick Settings" "${before}"
        before="$(trace_last_sequence)"
        tap_key KP_5 0.4
        wait_for_trace_after "Volume" "${before}"
        before="$(trace_last_sequence)"
        tap_key KP_2 0.25
        wait_for_trace_after "Volume" "${before}"
        capture "00-apple-volume-slider"
        tap_key KP_5 0.3
        tap_key KP_2 0.2
        before="$(trace_last_sequence)"
        tap_key KP_5 0.4
        wait_for_trace_after "Brightness" "${before}"
        before="$(trace_last_sequence)"
        tap_key KP_8 0.25
        wait_for_trace_after "Brightness" "${before}"
        capture "00-apple-brightness-slider"
        before="$(trace_last_sequence)"
        tap_key KP_Decimal 0.5
        wait_for_trace_after "Quick Settings" "${before}"
        sleep 0.4
        before="$(trace_last_sequence)"
        tap_key KP_Decimal 0.8
        wait_for_trace_after "Home" "${before}"
        capture "00-sliders-home"
    fi

    if [ "${IPODJS_NAVIGATION_UTILITIES:-0}" = "1" ]; then
        # The default stock Home order places Extras after Photos.  Exercise
        # the shared menu renderer and lockscreen without launching a plugin;
        # plugin internals and any plugin audio lifecycle remain untouched.
        for pulse in 1 2 3 4 5 6 7 8; do
            utility_selected="$(awk -F '\t' '$3 == "screen" && $4 == "Home" {
                value = $10
            } END { print value + 0 }' \
                "${runtime_root}/.rockbox/ipodjs-trace.tsv")"
            [ "${utility_selected}" -eq 4 ] && break
            if [ "${utility_selected}" -lt 4 ]; then
                tap_key KP_2 0.15
            else
                tap_key KP_8 0.15
            fi
        done
        capture "00-extras-home-pane"
        before="$(trace_last_sequence)"
        tap_key KP_5 0.6
        wait_for_trace_after "Extras" "${before}"
        capture "00-extras"
        if [ "${IPODJS_NAVIGATION_SITEKICK:-0}" = "1" ]; then
            tap_key KP_2 0.12
            tap_key KP_2 0.12
            tap_key KP_2 0.12
            before="$(trace_last_sequence)"
            tap_key KP_5 0.5
            wait_for_trace_after "Applications" "${before}"
            tap_key KP_2 0.12
            capture "00-desktop-mode-application"
            tap_key KP_2 0.35
            tap_key KP_2 0.35
            capture "00-sitekick-application"
            sleep 0.35
            capture "00-sitekick-application-float"
            before="$(trace_last_sequence)"
            tap_key KP_Decimal 0.5
            wait_for_trace_after "Extras" "${before}"
            tap_key KP_8 0.12
            tap_key KP_8 0.12
            tap_key KP_8 0.12
        fi
        hold_cycle "00-extras" "Extras"
        tap_key KP_2 0.15
        tap_key KP_8 0.25
        capture "00-extras-wheel"
        before="$(trace_last_sequence)"
        tap_key KP_Decimal 0.6
        wait_for_trace_after "Home" "${before}"
        for pulse in 1 2 3 4; do
            tap_key KP_8 0.08
        done
        capture "00-extras-home"
    fi

    if [ "${IPODJS_NAVIGATION_SITEKICK:-0}" = "1" ]; then
        before="$(trace_last_sequence)"
        hold_key w 1.3
        wait_for_trace_after "Quick Settings" "${before}"
        for pulse in 1 2 3 4 5 6 7 8 9 10 11 12; do
            quick_selected="$(awk -F '\t' \
                '$3 == "screen" && $4 == "Quick Settings" {
                    value = $10
                } END { print value + 0 }' \
                "${runtime_root}/.rockbox/ipodjs-trace.tsv")"
            [ "${quick_selected}" -eq 9 ] && break
            tap_key KP_2 0.12
        done
        capture "00-sitekick-quick-setting"
        tap_key KP_5 0.25
        capture "00-sitekick-quick-setting-cycled"
        tap_key KP_5 0.12
        tap_key KP_5 0.12
        before="$(trace_last_sequence)"
        tap_key w 0.5
        wait_for_trace_after "Home" "${before}"
    fi

    if [ "${IPODJS_NAVIGATION_SETTINGS:-0}" = "1" ]; then
        for pulse in 1 2 3 4 5; do
            tap_key KP_2 0.08
        done
        before="$(trace_last_sequence)"
        tap_key KP_5 0.6
        wait_for_trace_after "Settings" "${before}"
        tap_key KP_2 0.15
        tap_key KP_8 0.25
        capture "00-settings"
        tap_key KP_2 0.15
        before="$(trace_last_sequence)"
        tap_key KP_5 0.6
        wait_for_trace_kind_after "list" "${before}"
        tap_key KP_2 0.15
        tap_key KP_8 0.25
        capture "00-settings-sound"
        before="$(trace_last_sequence)"
        tap_key KP_Decimal 0.6
        wait_for_trace_after "Settings" "${before}"
        tap_key KP_2 0.15
        tap_key KP_8 0.25
        capture "00-settings-return"
        before="$(trace_last_sequence)"
        tap_key KP_Decimal 0.6
        wait_for_trace_after "Home" "${before}"
        for pulse in 1 2 3 4 5; do
            tap_key KP_8 0.08
        done
        capture "00-settings-home"
    fi

    # Default Home order begins with Cover Flow, followed by Music.
    before="$(trace_last_sequence)"
    tap_key KP_2
    tap_key KP_5 0.6
    wait_for_trace_after "Music" "${before}"
    music_name="$(trace_last_name_of_kind list)"
    capture "01-music"
    hold_cycle "01-music" "${music_name}"

    # The stock 6G tag browser begins with Album Artist then Artist.  The
    # native browser begins with Now Playing then Artists; one forward step
    # reaches the artist path in either implementation.
    before="$(trace_last_sequence)"
    tap_key KP_2
    tap_key KP_5 0.6
    wait_for_new_list_after "${before}" "${music_name}"
    artist_name="$(trace_last_name_of_kind list)"
    capture "02-artists"
    hold_cycle "02-artists" "${artist_name}"

    # Select the final deterministic artist in the compact simulator DB.
    for pulse in 1 2 3 4 5 6 7 8 9 10; do
        tap_key KP_2 0.08
    done
    before="$(trace_last_sequence)"
    tap_key KP_5 0.8
    wait_for_new_list_after "${before}" "${artist_name}"
    album_name="$(trace_last_name_of_kind list)"
    capture "03-albums"
    hold_cycle "03-albums" "${album_name}"

    before="$(trace_last_sequence)"
    tap_key KP_5 0.8
    wait_for_new_list_after "${before}" "${album_name}"
    song_name="$(trace_last_name_of_kind list)"
    capture "04-songs"
    hold_cycle "04-songs" "${song_name}"

    if [ "${IPODJS_NAVIGATION_CONTEXT:-0}" = "1" ]; then
        local context_name

        before="$(trace_last_sequence)"
        hold_key KP_5 1.2 0.5
        wait_for_new_list_after "${before}" "${song_name}"
        context_name="$(trace_last_name_of_kind list)"
        capture "04-song-context"
        hold_cycle "04-song-context" "${context_name}"
        before="$(trace_last_sequence)"
        tap_key KP_Decimal 0.6
        wait_for_trace_after "${song_name}" "${before}"
        capture "04-song-context-return"
    fi

    if [ "${IPODJS_NAVIGATION_FLAC:-0}" = "1" ]; then
        # [By album] contains 13 A Hard Day's Night MP3s followed by the
        # 17 Abbey Road FLACs. Move to the first actual FLAC track.
        for pulse in 1 2 3 4 5 6 7 8 9 10 11 12 13; do
            tap_key KP_2 0.08
        done
        capture "04-flac-selected"
    fi

    before="$(trace_last_sequence)"
    tap_key KP_5 1.2
    wait_for_trace_kind_after "wps" "${before}"
    wps_name="$(trace_last_name_of_kind wps)"
    capture "05-wps"

    if [ "${IPODJS_NAVIGATION_WPS_MENU_ONLY:-0}" = "1" ]; then
        # Exact hardware contract: from an actively playing Now Playing
        # screen, one ordinary Menu press returns to the saved song list.
        # Do not involve Select, Lyrics, pause, Hold, or another navigation
        # action in this focused regression.
        before="$(trace_last_sequence)"
        # Keep Menu physically down long enough to prove WPS reacts to the
        # press edge, not to the release that follows.
        hold_key KP_Decimal 0.8 0.2
        wait_for_trace_after "${song_name}" "${before}"
        if awk -F '\t' -v before="${before}" \
            '$1 > before && $4 == "Loading Music" { loading = 1 }
             END { exit loading ? 0 : 1 }' \
            "${runtime_root}/.rockbox/ipodjs-trace.tsv"; then
            printf 'single WPS Menu touched tagcache recovery\n' >&2
            exit 1
        fi
        capture "07-return-songs-menu-only"
        return
    fi

    if [ "${IPODJS_NAVIGATION_LYRICS_HOME_STORM:-0}" = "1" ]; then
        # Exact hardware report: one freshly started song, enter Lyrics, then
        # rapidly press Menu/Home. The first Menu intent must survive generic
        # plugin teardown and return directly to the saved Music browser; the
        # remaining taps then unwind ordinary Rockbox history without touching
        # tagcache recovery.
        before="$(trace_last_sequence)"
        hold_key KP_5 1.2 0.25
        wait_for_trace_after "WPS Lyrics" "${before}"
        # WPS emits the launch marker before the plugin has finished parsing
        # the lyric file and clearing the Select-hold queue. Model the report:
        # wait until Lyrics is visibly settled, then press Menu once. This is
        # the exact contract that previously failed because plugin_load()
        # discarded the release before WPS could act on it.
        sleep 4
        tap_key KP_Decimal 0.2
        wait_for_trace_after "${song_name}" "${before}"
        if awk -F '\t' -v before="${before}" \
            '$1 > before && ($4 == "Loading Music" || $4 == "Home") {
                 bad = 1
             }
             END { exit bad ? 0 : 1 }' \
            "${runtime_root}/.rockbox/ipodjs-trace.tsv"; then
            printf 'Lyrics Menu entered Home or tagcache recovery\n' >&2
            exit 1
        fi

        # Keep this reproducer faithful to the reported cycle: no subsequent
        # navigation or plugin test is folded into its lifecycle assertion.
        return
    fi

    if [ "${IPODJS_NAVIGATION_WPS_SELECT_BEHAVIOR:-0}" = "1" ]; then
        # A short center press is deliberately inert on iPodJS Now Playing.
        before="$(trace_last_sequence)"
        tap_key KP_5 0.45
        wait_for_trace_after "WPS Select" "${before}"
        if awk -F '\t' -v before="${before}" \
            '$1 > before && ($4 == "Playlist" || $4 == "Loading Music") {
                 bad = 1
             }
             END { exit bad ? 0 : 1 }' \
            "${runtime_root}/.rockbox/ipodjs-trace.tsv"; then
            printf 'short WPS Select entered Playlist or Music loading\n' >&2
            exit 1
        fi

        # A center hold launches Lyrics directly. Short Select is inert there
        # as well: Menu is the only browser-return action.
        before="$(trace_last_sequence)"
        hold_key KP_5 1.2 0.5
        wait_for_trace_after "WPS Lyrics" "${before}"
        # The launch trace precedes plugin_load() and lyric parsing.  Do not
        # inject Select into the queue that generic plugin setup clears.
        sleep 4
        before="$(trace_last_sequence)"
        tap_key KP_5 0.6
        if awk -F '\t' -v before="${before}" \
            '$1 > before && ($2 == "wps" || $2 == "list" ||
                              $4 == "Home" || $4 == "Loading Music") {
                 bad = 1
             }
             END { exit bad ? 0 : 1 }' \
            "${runtime_root}/.rockbox/ipodjs-trace.tsv"; then
            printf 'short Lyrics Select left Lyrics\n' >&2
            exit 1
        fi

        tap_key KP_Decimal 0.6
        wait_for_trace_after "${song_name}" "${before}"
        capture "05-lyrics-menu-contract"
        return
    fi

    if [ "${IPODJS_NAVIGATION_LONG_TITLE:-0}" = "1" ]; then
        local changed

        capture "05-wps-title-start"
        sleep 2
        capture "05-wps-title-scrolled"
        magick "${out_dir}/05-wps-title-start.png" \
            -crop 153x20+157+56 "${out_dir}/05-wps-title-start-crop.png"
        magick "${out_dir}/05-wps-title-scrolled.png" \
            -crop 153x20+157+56 "${out_dir}/05-wps-title-scrolled-crop.png"
        changed="$(magick compare -metric AE \
            "${out_dir}/05-wps-title-start-crop.png" \
            "${out_dir}/05-wps-title-scrolled-crop.png" null: 2>&1 || true)"
        changed="${changed%% *}"
        rm -f "${out_dir}/05-wps-title-start-crop.png" \
              "${out_dir}/05-wps-title-scrolled-crop.png"
        if [ "${changed:-0}" -lt 20 ]; then
            printf "long Now Playing title did not visibly scroll (%s pixels)\n" \
                "${changed:-0}" >&2
            exit 1
        fi
    fi
    hold_cycle "05-wps" "${wps_name}"
    before="$(trace_last_sequence)"
    tap_key space 0.3
    wait_for_audio_after 3 "${before}"
    capture "05-wps-paused"
    before="$(trace_last_sequence)"
    tap_key space 0.3
    wait_for_audio_after 1 "${before}"
    capture "05-wps-resumed"
    sleep 2.2
    capture "06-wps-progress"

    if [ "${IPODJS_NAVIGATION_WPS_MENU_STORM:-0}" = "1" ] ||
       [ "${IPODJS_NAVIGATION_WPS_LATE_MENU:-0}" = "1" ]; then
        before="$(trace_last_sequence)"
        for pulse in 1 2 3 4 5 6; do
            tap_key KP_Decimal 0.04 0.04
        done
        # Stock transitions may intentionally discard releases that arrive
        # while their frame animation owns input.  After the burst settles,
        # prove ordinary Menu navigation remains live all the way to Home.
        sleep 0.5
        for pulse in 1 2 3 4 5 6; do
            tap_key KP_Decimal 0.35
        done
        wait_for_trace_after "Home" "${before}"
        if awk -F '\t' -v before="${before}" \
            '$1 > before && $4 == "Loading Music" { loading = 1 }
             END { exit loading ? 0 : 1 }' \
            "${runtime_root}/.rockbox/ipodjs-trace.tsv"; then
            printf 'rapid WPS Menu navigation touched tagcache recovery\n' >&2
            exit 1
        fi

        # Prove ordinary database and plugin-backed navigation still opens
        # after the rapid sequential return. Home retains Music at index 1.
        before="$(trace_last_sequence)"
        tap_key KP_5 0.6
        wait_for_trace_after "${music_name}" "${before}"
        before="$(trace_last_sequence)"
        tap_key KP_5 0.6
        wait_for_new_list_after "${before}" "${music_name}"
        tap_key KP_Decimal 0.4
        tap_key KP_Decimal 0.4
        wait_for_trace_after "Home" "${before}"
        for pulse in 1 2 3; do
            tap_key KP_2 0.08
        done
        before="$(trace_last_sequence)"
        tap_key KP_5 0.5
        wait_for_trace_after "Extras" "${before}"
        tap_key KP_2 0.1
        before="$(trace_last_sequence)"
        tap_key KP_5 0.8
        tap_key KP_Decimal 0.5
        wait_for_trace_after "Extras" "${before}"
        return
    fi

    # Menu must return to the exact source list, then pop one level at a time.
    before="$(trace_last_sequence)"
    tap_key KP_Decimal 0.8
    wait_for_trace_after "${song_name}" "${before}"
    capture "07-return-songs"
    before="$(trace_last_sequence)"
    tap_key KP_Decimal 0.6
    wait_for_trace_after "${album_name}" "${before}"
    capture "08-return-albums"
    before="$(trace_last_sequence)"
    tap_key KP_Decimal 0.6
    wait_for_trace_after "${artist_name}" "${before}"
    capture "09-return-artists"
    before="$(trace_last_sequence)"
    tap_key KP_Decimal 0.6
    wait_for_trace_after "${music_name}" "${before}"
    capture "10-return-music"
    before="$(trace_last_sequence)"
    tap_key KP_Decimal 0.6
    wait_for_trace_after "Home" "${before}"
    capture "11-return-home"

    # Exercise the stock push while playback owns its full audio buffer.  The
    # animation must preserve the track/playlist identity and use only the
    # fixed 6G UI workspace.
    before="$(trace_last_sequence)"
    tap_key KP_5 0.6
    wait_for_trace_after "${music_name}" "${before}"
    before="$(trace_last_sequence)"
    tap_key KP_Decimal 0.6
    wait_for_trace_after "Home" "${before}"

    run_music_fd_stress "${music_name}" "${artist_name}" \
        "${album_name}" "${song_name}"
    run_album_artist_switch_stress "${music_name}"

    if [ "${IPODJS_NAVIGATION_SHUTDOWN:-0}" = "1" ]; then
        capture "12-home-before-shutdown"
    elif [ "${IPODJS_NAVIGATION_VIDEO:-0}" != "1" ]; then
        # A medium Play hold stops playback without entering Now Playing. A
        # much longer hold remains reserved for the shutdown countdown.
        before="$(trace_last_sequence)"
        hold_key space 1.0
        wait_for_audio_after 0 "${before}"
        capture "12-home-stopped"
    fi
}

wait_for_video_profile_stage()
{
    local expected="$1"
    local profile="${runtime_root}/.rockbox/openh264/openh264_profile.log"
    local attempt

    for attempt in $(seq 1 160); do
        if [ -s "${profile}" ] &&
           grep -q "mode=raw_audio_state stage=${expected} " "${profile}"; then
            sleep 0.25
            return
        fi
        if ! kill -0 "${sim_pid}" >/dev/null 2>&1; then
            break
        fi
        sleep 0.1
    done
    printf "OpenH264 did not reach audio lifecycle stage %s\n" \
        "${expected}" >&2
    exit 1
}

wait_for_video_control_stage()
{
    local expected="$1"
    local profile="${runtime_root}/.rockbox/openh264/openh264_profile.log"
    local attempt

    for attempt in $(seq 1 160); do
        if [ -s "${profile}" ] &&
           grep -q "mode=raw_controls stage=${expected} " "${profile}"; then
            sleep 0.25
            return
        fi
        if ! kill -0 "${sim_pid}" >/dev/null 2>&1; then
            break
        fi
        sleep 0.1
    done
    printf "OpenH264 did not reach control stage %s\n" "${expected}" >&2
    exit 1
}

run_optional_video_transition()
{
    local before
    local music_name
    local artist_name
    local album_name
    local song_name
    local video_name

    if [ "${IPODJS_NAVIGATION_VIDEO:-0}" != "1" ]; then
        return
    fi

    # Home still has Music selected and user music is active.  Enter the
    # adjacent Videos item, play the deterministic raw fixture, and exit only
    # after OpenH264 proves that it took and released the playback buffer.
    tap_key KP_2 0.15
    before="$(trace_last_sequence)"
    tap_key KP_5 0.7
    wait_for_trace_kind_after "list" "${before}"
    video_name="$(trace_last_name_of_kind list)"
    capture "12-video-root"

    before="$(trace_last_sequence)"
    tap_key KP_5 0.5
    # Video folders intentionally keep the stock "Videos" title, so entering
    # Downloaded is a new list render without a different trace name.
    wait_for_trace_kind_after "list" "${before}"
    video_name="$(trace_last_name_of_kind list)"
    capture "12-video-folder"

    tap_key KP_5 1.0
    wait_for_video_profile_stage "prepare-after-stop"
    wait_for_video_profile_stage "start-at-frame"
    capture "12-video-playing"

    # Stock click-wheel video controls: Play/Pause freezes the current frame,
    # held Previous rewinds, and Center exposes the scrubber for wheel input.
    tap_key space 0.5
    wait_for_video_control_stage "pause"
    capture "12-video-paused"
    hold_key KP_4 0.8
    wait_for_video_control_stage "seek"
    capture "12-video-rewound"
    tap_key KP_5 0.4
    tap_key KP_8 0.2
    capture "12-video-scrubber"
    tap_key KP_5 0.4
    tap_key space 0.5
    wait_for_video_control_stage "resume"

    before="$(trace_last_sequence)"
    tap_key KP_Decimal 0.8
    wait_for_video_profile_stage "shutdown-after-restore"
    wait_for_trace_after "${video_name}" "${before}"
    if [ "${IPODJS_NAVIGATION_VIDEO_CONTROLS_ONLY:-0}" = "1" ]; then
        capture "12-video-returned"
        return
    fi

    # Pop to Home, re-enter the same stock tagtree state, start music again,
    # and prove WPS playback works.  This catches the reported intermittent
    # plugin -> Database state split without mutating playlist or PCM code.
    before="$(trace_last_sequence)"
    # Hidden-video category layers all retain the stock Videos title. Unwind
    # the bounded stack; Menu at Home is intentionally a no-op.
    for _video_menu_level in 1 2 3 4 5 6; do
        tap_key KP_Decimal 0.35
    done
    wait_for_trace_after "Home" "${before}"
    tap_key KP_8 0.15
    before="$(trace_last_sequence)"
    tap_key KP_5 0.6
    wait_for_trace_after "Music" "${before}"
    music_name="$(trace_last_name_of_kind list)"
    capture "13-music-after-video"

    before="$(trace_last_sequence)"
    tap_key KP_5 0.6
    wait_for_new_list_after "${before}" "${music_name}"
    artist_name="$(trace_last_name_of_kind list)"
    before="$(trace_last_sequence)"
    tap_key KP_5 0.6
    wait_for_new_list_after "${before}" "${artist_name}"
    album_name="$(trace_last_name_of_kind list)"
    before="$(trace_last_sequence)"
    tap_key KP_5 0.6
    wait_for_new_list_after "${before}" "${album_name}"
    song_name="$(trace_last_name_of_kind list)"
    before="$(trace_last_sequence)"
    tap_key KP_5 1.0
    wait_for_trace_kind_after "wps" "${before}"
    wait_for_audio_after 1 "${before}"
    capture "14-wps-after-video"

    before="$(trace_last_sequence)"
    tap_key KP_Decimal 0.6
    wait_for_trace_after "${song_name}" "${before}"
    before="$(trace_last_sequence)"
    tap_key KP_Decimal 0.6
    wait_for_trace_after "${album_name}" "${before}"
    before="$(trace_last_sequence)"
    tap_key KP_Decimal 0.6
    wait_for_trace_after "${artist_name}" "${before}"
    before="$(trace_last_sequence)"
    tap_key KP_Decimal 0.6
    wait_for_trace_after "${music_name}" "${before}"
    before="$(trace_last_sequence)"
    tap_key KP_Decimal 0.6
    wait_for_trace_after "Home" "${before}"

    if [ "${IPODJS_NAVIGATION_SHUTDOWN:-0}" != "1" ]; then
        before="$(trace_last_sequence)"
        hold_key space 1.0
        wait_for_audio_after 0 "${before}"
        capture "15-home-stopped-after-video"
    fi
}

run_optional_shutdown_journey()
{
    local pid
    local attempt

    if [ "${IPODJS_NAVIGATION_SHUTDOWN:-0}" != "1" ]; then
        return
    fi

    pid="${sim_pid}"
    touch "${runtime_root}/play-hold.gate"
    for attempt in $(seq 1 200); do
        if ! kill -0 "${pid}" >/dev/null 2>&1; then
            break
        fi
        sleep 0.1
    done
    rm -f "${runtime_root}/play-hold.gate"
    if kill -0 "${pid}" >/dev/null 2>&1; then
        cp "${runtime_root}/.rockbox/ipodjs-trace.tsv" \
           "${out_dir}/shutdown-timeout-trace.tsv" 2>/dev/null || true
        printf "long Play hold did not shut down the simulator\n" >&2
        exit 1
    fi
    wait "${pid}" 2>/dev/null || true
    sim_pid=""
}

assert_no_legacy_frame()
{
    local frame
    local colors

    for frame in "${out_dir}"/*.png; do
        colors="$(magick "${frame}" -format '%k' info:)"
        if [ "${colors}" -lt 8 ]; then
            printf "suspicious blank/legacy transition frame: %s (%s colors)\n" \
                "${frame}" "${colors}" >&2
            exit 1
        fi
    done
}

assert_optional_wps_status_icons()
{
    local colors

    if [ "${IPODJS_NAVIGATION_SHUFFLE:-off}" = "on" ]; then
        colors="$(magick "${out_dir}/05-wps.png" \
            -crop 21x19+264+24 +repage -format '%k' info:)"
        if [ "${colors}" -lt 2 ]; then
            printf "verified Apple shuffle indicator was not rendered\n" >&2
            exit 1
        fi
    fi
    if [ "${IPODJS_NAVIGATION_REPEAT:-off}" != "off" ]; then
        colors="$(magick "${out_dir}/05-wps.png" \
            -crop 21x19+290+24 +repage -format '%k' info:)"
        if [ "${colors}" -lt 2 ]; then
            printf "verified Apple repeat indicator was not rendered\n" >&2
            exit 1
        fi
    fi
}

main()
{
    local command
    local -a trace_gate_args=()

    for command in magick stat xdotool; do
        command -v "${command}" >/dev/null 2>&1 || {
            printf "missing required command: %s\n" "${command}" >&2
            exit 1
        }
    done
    if [ ! -x "${rockboxui}" ]; then
        printf "missing simulator build: %s\n" "${rockboxui}" >&2
        exit 1
    fi
    mkdir -p "${out_dir}"
    rm -f "${out_dir}"/*.png "${out_dir}/ipodjs-trace.tsv"
    trap cleanup EXIT INT TERM HUP

    prepare_root
    launch_sim
    wait_for_home
    configure_optional_wps_status
    run_music_journey
    run_optional_video_transition
    run_optional_shutdown_journey
    cp "${runtime_root}/.rockbox/ipodjs-trace.tsv" \
       "${out_dir}/ipodjs-trace.tsv"
    cleanup_sim
    if [ "${IPODJS_NAVIGATION_KEEP_ROOT:-0}" != "1" ]; then
        rm -rf "${runtime_root}"
    else
        printf "preserved simulator root: %s\n" "${runtime_root}"
    fi
    runtime_root=""

    if [ "${IPODJS_NAVIGATION_VIDEO_CONTROLS_ONLY:-0}" = "1" ]; then
        for _video_capture in 12-video-playing 12-video-paused \
                              12-video-rewound 12-video-scrubber \
                              12-video-returned; do
            [ -s "${out_dir}/${_video_capture}.png" ] || {
                printf "missing video control capture: %s\n" \
                    "${_video_capture}" >&2
                return 1
            }
        done
        assert_no_legacy_frame
        printf "iPodJS video-control simulator regression passed; captures: %s\n" \
            "${out_dir}"
        return 0
    fi

    if [ "${IPODJS_NAVIGATION_WPS_SELECT_BEHAVIOR:-0}" = "1" ]; then
        trace_gate_args+=(--allow-no-lockscreen \
                          --allow-no-playback-transition \
                          --lyrics-home-cycle)
    fi
    if [ "${IPODJS_NAVIGATION_LYRICS_HOME_STORM:-0}" = "1" ]; then
        trace_gate_args+=(--allow-no-lockscreen \
                          --allow-no-playback-transition \
                          --lyrics-home-cycle)
    fi
    if [ "${IPODJS_NAVIGATION_WPS_MENU_ONLY:-0}" = "1" ]; then
        # This focused path intentionally returns after the first WPS Menu
        # action instead of unwinding every parent list in the full journey.
        trace_gate_args+=(--allow-no-lockscreen \
                          --allow-no-playback-transition \
                          --lyrics-home-cycle)
    fi

    "${repo_root}/rockpod/.venv/bin/python" \
        "${repo_root}/tools/ipodjs_navigation_trace_gate.py" \
        "${trace_gate_args[@]}" \
        "${out_dir}/ipodjs-trace.tsv"
    assert_no_legacy_frame
    assert_optional_wps_status_icons
    printf "iPodJS navigation simulator regression passed; captures: %s\n" \
        "${out_dir}"
}

main "$@"
