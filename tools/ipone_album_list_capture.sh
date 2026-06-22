#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:-/home/david/Documents/RockBox_Personal-master/build-sim-video-5g}"
out_dir="${2:-/home/david/Documents/RockBox_Personal-master/docs/album-list-layout-shots}"
source_sim_root="${build_dir}/simdisk"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
rockboxui="${build_dir}/rockboxui"
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

require_file()
{
    if [ ! -f "$1" ]; then
        printf "missing required file: %s\n" "$1" >&2
        exit 1
    fi
}

install_current_theme_sources()
{
    local rb_cfg_dir="${sim_root}/.rockbox"

    for skin_file in iPone.sbs iPone.wps iPone.fms; do
        if [ -f "${repo_root}/wps/${skin_file}" ]; then
            cp "${repo_root}/wps/${skin_file}" "${rb_cfg_dir}/wps/${skin_file}"
        fi
    done

    if [ -d "${repo_root}/wps/iPone" ]; then
        rm -rf "${rb_cfg_dir}/wps/iPone"
        cp -a "${repo_root}/wps/iPone" "${rb_cfg_dir}/wps/iPone"
    fi
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

    install_current_theme_sources

    awk '
        BEGIN { start_replaced = 0; icons_replaced = 0 }
        /^start in screen:/ {
            print "start in screen: db"
            start_replaced = 1
            next
        }
        /^show icons:/ {
            print "show icons: on"
            icons_replaced = 1
            next
        }
        { print }
        END {
            if (!start_replaced)
                print "start in screen: db"
            if (!icons_replaced)
                print "show icons: on"
        }
    ' "${sim_root}/.rockbox/config.cfg" >"${tmp_dir}/config.cfg"
    cp "${tmp_dir}/config.cfg" "${sim_root}/.rockbox/config.cfg"
}

launch_sim()
{
    pushd "${build_dir}" >/dev/null
    ./rockboxui --zoom 2 --nobackground --root "${sim_root}" &
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
    import -window "${sim_wid}" "${out_dir}/$1"
}

cleanup()
{
    set +e
    if [ -n "${sim_pid:-}" ] && kill -0 "${sim_pid}" >/dev/null 2>&1; then
        kill "${sim_pid}" >/dev/null 2>&1 || true
        wait "${sim_pid}" 2>/dev/null || true
    fi
    if [ "${IPONE_ALBUMLIST_KEEP_ROOT:-0}" = "1" ]; then
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

    if [ ! -d "${source_sim_root}/.rockbox" ]; then
        printf "missing simulator root: %s\n" "${source_sim_root}" >&2
        exit 1
    fi

    mkdir -p "${out_dir}"
    rm -f "${out_dir}"/*.png "${out_dir}/layout-report.txt"
    tmp_dir="$(mktemp -d)"
    trap cleanup EXIT INT TERM HUP

    prepare_runtime_root
    launch_sim

    capture_window "00-database-root.png"
    xdotool key --window "${sim_wid}" KP_2
    sleep 0.2
    capture_window "01-root-after-one-down.png"
    xdotool key --window "${sim_wid}" KP_2
    sleep 0.2
    capture_window "02-root-after-two-down-album-selected.png"
    xdotool keydown --window "${sim_wid}" Return
    sleep 0.15
    xdotool keyup --window "${sim_wid}" Return
    sleep 1
    capture_window "03-albums-list.png"

    cat >"${out_dir}/layout-report.txt" <<EOF
Reference target: stock iPod classic 7G album list is a 320x240 full-width
text list with a thin top title/status area, single horizontal selection bar,
and roughly 9 to 10 visible rows. Colors are intentionally not compared
against stock; this repo keeps the iPone/Rockbox theme colors.

Captured:
- 00-database-root.png: non-album database root, used to verify normal list
  layout remains scoped outside album rows.
- 01-root-after-one-down.png: database root after one scroll step.
- 02-root-after-two-down-album-selected.png: database root with Album selected.
- 03-albums-list.png: Database -> Album after two scroll steps and Select.

Expected after implementation:
- album list uses the full 320px screen width, apart from normal top inset;
- album rows use compact text layout with no 32x32 thumbnails;
- visible row density is stock-like and significantly denser than the old
  34px thumbnail rows;
- selector/theme colors remain those from the current iPone config.
EOF

    printf "saved album-list screenshots to %s\n" "${out_dir}"
}

main "$@"
