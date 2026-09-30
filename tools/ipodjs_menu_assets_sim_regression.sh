#!/usr/bin/env bash
# Use the real action/list loop with deterministic simulator-only input.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/ipodjs_navigation_sim_regression.sh"
export IPODJS_NAVIGATION_ROOT_MENU_ORDER="pictureflow,database,videos,photos,applications,settings,wps"

launch_sim()
{
    export ROCKPOD_SIM_SELECT_GATE="${runtime_root}/select.gate"
    export ROCKPOD_SIM_MENU_GATE="${runtime_root}/menu.gate"
    export ROCKPOD_SIM_SCROLL_FWD_GATE="${runtime_root}/forward.gate"
    export ROCKPOD_SIM_SCROLL_BACK_GATE="${runtime_root}/back.gate"
    export ROCKPOD_SIM_PLAY_ACTION_GATE="${runtime_root}/play.gate"
    export ROCKPOD_SIM_LEFT_GATE="${runtime_root}/left.gate"
    export ROCKPOD_SIM_RIGHT_GATE="${runtime_root}/right.gate"
    (
        trap - EXIT INT TERM HUP
        cd "${build_dir}"
        exec env SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=dummy \
            SDL_RENDER_DRIVER=software ROCKPOD_SIM_IPODJS_TRACE=1 \
            ROCKPOD_SIM_PREVIEW_BMP="${dump_bmp}" \
            ROCKPOD_SIM_HOLD_GATE="${runtime_root}/hold.gate" \
            ROCKPOD_SIM_PLAY_HOLD_GATE="${runtime_root}/play-hold.gate" \
            ROCKPOD_SIM_PREVIEW_INTERVAL_MS=0 \
            ./rockboxui --zoom 1 --nobackground --root "${runtime_root}"
    ) >"${out_dir}/simulator.log" 2>&1 &
    sim_pid=$!
}

gate_for_key()
{
    case "$1" in
        KP_5) printf '%s' "$ROCKPOD_SIM_SELECT_GATE";;
        KP_Decimal|w) printf '%s' "$ROCKPOD_SIM_MENU_GATE";;
        KP_2) printf '%s' "$ROCKPOD_SIM_SCROLL_FWD_GATE";;
        KP_8) printf '%s' "$ROCKPOD_SIM_SCROLL_BACK_GATE";;
        space) printf '%s' "$ROCKPOD_SIM_PLAY_ACTION_GATE";;
        KP_4|Left) printf '%s' "$ROCKPOD_SIM_LEFT_GATE";;
        KP_6|Right) printf '%s' "$ROCKPOD_SIM_RIGHT_GATE";;
        *) printf 'unsupported gated key: %s\n' "$1" >&2; return 1;;
    esac
}

tap_key()
{
    local gate
    gate=$(gate_for_key "$1")
    touch "$gate"
    sleep "${3:-0.16}"
    rm -f "$gate"
    sleep "${2:-0.20}"
}

hold_key()
{
    local gate
    gate=$(gate_for_key "$1")
    touch "$gate"
    sleep "$2"
    rm -f "$gate"
    sleep "${3:-0.35}"
}

if [ "${IPODJS_FULL_NAVIGATION_HEADLESS:-0}" = 1 ]; then
    main "$@"
elif [ "${IPODJS_COVERFLOW_WPS_FOCUSED:-0}" = 1 ]; then
    mkdir -p "${out_dir}"
    trap cleanup EXIT INT TERM HUP
    prepare_root
    cp "${build_dir}/apps/plugins/pictureflow/pictureflow.rock" \
        "${runtime_root}/.rockbox/rocks/demos/pictureflow.rock"
    if [ "${IPODJS_WPS_INPUT_TRACE:-0}" = 1 ]; then
        touch "${runtime_root}/.rockbox/ipodjs-wps-input.enable"
    fi
    # Suppress unrelated asynchronous banners only in this disposable fixture.
    printf '\nnotification banners: off\n' >> "${runtime_root}/.rockbox/config.cfg"
    if [ "${IPODJS_WPS_PAGES:-0}" = 1 ]; then
        printf '\ngather runtime data: off\nautoresume enable: on\n' >> \
            "${runtime_root}/.rockbox/config.cfg"
    fi
    launch_sim
    wait_for_home
    tap_key KP_5 8
    capture coverflow
    tap_key KP_5 1.2
    capture coverflow-tracks
    before="$(trace_last_sequence)"
    tap_key KP_5 2
    wait_for_trace_kind_after "wps" "${before}"
    sleep 2
    capture coverflow-wps
    if [ "${IPODJS_WPS_PAGES:-0}" = 1 ]; then
        tap_key KP_5 0.4
        for pulse in 1 2 3 4 5 6; do tap_key KP_8 0.12; done
        capture wps-rating
        for pulse in 1 2 3 4 5 6; do tap_key KP_2 0.12; done
        capture wps-rating-five
        for pulse in 1 2 3 4 5 6; do tap_key KP_8 0.12; done
        capture wps-rating-cleared
        for pulse in 1 2 3; do tap_key KP_2 0.12; done
        capture wps-rating-edited
        tap_key KP_5 0.4
        capture wps-bars
        sleep 0.5
        capture wps-bars-next-frame
        tap_key KP_5 0.4
        capture wps-art-return
        tap_key KP_5 0.4
        capture wps-rating-reopened
        tap_key KP_5 0.3
        tap_key KP_5 0.3
        if [ "${IPODJS_WPS_INPUT_TRACE:-0}" = 1 ]; then
            tap_key KP_Decimal 0.5
            cp "${runtime_root}/.rockbox/ipodjs-wps-input.log" \
                "${out_dir}/ipodjs-wps-input.log"
        fi
        cleanup_sim
        PYTHONPATH="${repo_root}/rockpod" "${repo_root}/rockpod/.venv/bin/python" \
            -c 'import sys
from services.rockbox_tagcache import read_rockbox_tagcache_tracks
rows = read_rockbox_tagcache_tracks(sys.argv[1], include_runtime=True)
row = next(row for row in rows if row["device_path"].endswith("01 A Hard Day\x27s Night.mp3"))
assert row["rating"] == 6, row
print("PASS: three Apple stars persisted as Rockbox rating 6")' "${runtime_root}"
    fi
    printf 'PASS: Cover Flow song reached WPS\n'
elif [ "${IPODJS_SCREENSAVER_FOCUSED:-0}" = 1 ]; then
    mkdir -p "${out_dir}"
    trap cleanup EXIT INT TERM HUP
    prepare_root
    launch_sim
    wait_for_home
    hold_key w 1.3
    tap_key KP_8 0.3
    capture screensaver-default-on
    tap_key KP_5 0.3
    capture screensaver-off
    tap_key KP_Decimal 0.5
    sleep 1
    cleanup_sim
    saved_config="${runtime_root}/.rockbox/config.cfg.new"
    [ -f "${saved_config}" ] || saved_config="${runtime_root}/.rockbox/config.cfg"
    rg -q '^ui engine playback screensaver: off' \
        "${saved_config}"
    # A fresh boot must not pass wait_for_home using the previous run's trace.
    rm -f "${runtime_root}/.rockbox/ipodjs-trace.tsv"
    launch_sim
    wait_for_home
    hold_key w 1.3
    tap_key KP_8 0.3
    capture screensaver-off-after-restart
    tap_key KP_5 0.3
    capture screensaver-on
    tap_key KP_Decimal 0.5
    sleep 1
    cleanup_sim
    saved_config="${runtime_root}/.rockbox/config.cfg.new"
    [ -f "${saved_config}" ] || saved_config="${runtime_root}/.rockbox/config.cfg"
    # Rockbox omits default-valued settings from its changed-settings file.
    if rg -q '^ui engine playback screensaver: off' "${saved_config}"; then
        printf 'screensaver setting did not return to On\n' >&2
        exit 1
    fi
    printf 'PASS: screensaver toggle On/Off and persistence across restart\n'
elif [ "${IPODJS_MENU_FOCUSED:-0}" = 1 ]; then
    mkdir -p "${out_dir}"
    trap cleanup EXIT INT TERM HUP
    prepare_root
    # Disposable fixture only: exercise the user's grid presentation.
    printf '\nui engine applications appearance: ios3\n' >> \
        "${runtime_root}/.rockbox/config.cfg"
    if [ -n "${IPODJS_ABOUT_SNAPSHOT:-}" ]; then
        cp "${IPODJS_ABOUT_SNAPSHOT}" "${runtime_root}/.rockbox/ipodjs/about.tsv"
    else
        python3 "${repo_root}/tools/ipodjs_about_inventory.py" "${source_root}" \
            "${runtime_root}/.rockbox/ipodjs/about.tsv"
    fi
    # Use the matching built plugin, not a stale fixture plugin ABI.
    cp "${build_dir}/apps/plugins/main_menu_config.rock" \
        "${runtime_root}/.rockbox/rocks/apps/main_menu_config.rock"
    launch_sim
    wait_for_home
    capture home-before
    for pulse in 1 2 3 4; do tap_key KP_2; done
    tap_key KP_5 0.6
    for pulse in 1 2 3; do tap_key KP_2; done
    tap_key KP_5 0.6
    capture applications-header-join
    sleep 0.6
    capture applications-labels-mid-scroll
    sleep 2
    capture applications-labels-scrolled
    tap_key KP_Decimal 0.6
    # Extras retains Applications at index 3; About now lives at index 10.
    for pulse in 1 2 3 4 5 6 7; do tap_key KP_2; done
    capture extras-about-preview
    tap_key KP_5 0.7
    capture about-capacity
    sleep 2
    capture about-capacity-scrolled
    tap_key KP_5 0.4
    capture about-apps-first
    tap_key KP_2
    capture about-apps-second
    tap_key KP_2
    capture about-apps-third
    tap_key KP_5 0.4
    capture about-counts
    tap_key KP_5 0.4
    capture about-identity
    sleep 3
    capture about-identity-scrolled
    tap_key KP_Decimal 0.6
    tap_key KP_Decimal 0.6
    tap_key KP_2
    tap_key KP_5 0.5
    capture settings-main-menu-preview
    tap_key KP_5 0.7
    capture main-menu-controls
    tap_key KP_2
    capture main-menu-checkmarks
    tap_key KP_Decimal 0.6
    tap_key KP_2
    capture settings-sound-preview
    tap_key KP_5 0.6
    capture sound-menu-fonts
    for pulse in $(seq 1 15); do tap_key KP_2 0.08; done
    capture sound-menu-scrolled
    tap_key KP_Decimal 0.6
    for pulse in 1 2 3 4 5 6; do tap_key KP_2; done
    tap_key KP_5 0.6
    capture manage-settings
    tap_key KP_2
    tap_key KP_5 0.3
    capture reset-confirmation
    # Move both ways without accepting Yes; Select must confirm No.
    tap_key KP_8 0.3
    capture reset-confirmation-yes-selected
    tap_key KP_2 0.3
    capture reset-confirmation-no-selected
    tap_key KP_5 0.12
    capture reset-cancelled
    sleep 2
    tap_key KP_Decimal 0.6
    tap_key KP_2
    capture settings-long-label-start
    sleep 3
    capture settings-long-label-scrolled
    printf 'PASS: captured Settings, About preview, menu controls and fonts\n'
else
    export IPODJS_NAVIGATION_SETTINGS=1
    export IPODJS_NAVIGATION_FD_STRESS_CYCLES=10
    export IPODJS_NAVIGATION_RAPID_SWITCH_CYCLES=20
    main
fi
