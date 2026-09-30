#!/usr/bin/env bash
# Source after ipodjs_navigation_sim_regression.sh to use the simulator's
# existing button gates and software LCD capture without an X11 desktop.
launch_sim()
{
    (
        trap - EXIT INT TERM HUP
        cd "${build_dir}"
        exec env SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=dummy \
            SDL_RENDER_DRIVER=software ROCKPOD_SIM_IPODJS_TRACE=1 \
            ROCKPOD_SIM_PREVIEW_BMP="${dump_bmp}" \
            ROCKPOD_SIM_PREVIEW_INTERVAL_MS=0 \
            ROCKPOD_SIM_HOLD_GATE="${runtime_root}/hold.gate" \
            ROCKPOD_SIM_SELECT_GATE="${runtime_root}/select.gate" \
            ROCKPOD_SIM_MENU_GATE="${runtime_root}/menu.gate" \
            ROCKPOD_SIM_PLAY_ACTION_GATE="${runtime_root}/play.gate" \
            ROCKPOD_SIM_SCROLL_FWD_GATE="${runtime_root}/forward.gate" \
            ROCKPOD_SIM_SCROLL_BACK_GATE="${runtime_root}/back.gate" \
            ROCKPOD_SIM_LEFT_GATE="${runtime_root}/left.gate" \
            ROCKPOD_SIM_RIGHT_GATE="${runtime_root}/right.gate" \
            ./rockboxui --zoom 1 --nobackground --root "${runtime_root}"
    ) &
    sim_pid=$!
}

headless_key_path()
{
    case "$1" in
        KP_5) printf '%s/select.gate' "${runtime_root}" ;;
        KP_Decimal|w) printf '%s/menu.gate' "${runtime_root}" ;;
        space) printf '%s/play.gate' "${runtime_root}" ;;
        KP_2) printf '%s/forward.gate' "${runtime_root}" ;;
        KP_8) printf '%s/back.gate' "${runtime_root}" ;;
        KP_4) printf '%s/left.gate' "${runtime_root}" ;;
        KP_6) printf '%s/right.gate' "${runtime_root}" ;;
        *) printf 'unsupported headless key %s\n' "$1" >&2; return 1 ;;
    esac
}

tap_key()
{
    local gate
    gate="$(headless_key_path "$1")"
    touch "${gate}"
    sleep "${3:-0.16}"
    rm "${gate}"
    sleep "${2:-0.20}"
}

hold_key()
{
    tap_key "$1" "${3:-0.35}" "$2"
}

cleanup_sim()
{
    if [ -n "${sim_pid}" ] && kill -0 "${sim_pid}" 2>/dev/null; then
        kill -KILL "${sim_pid}"
        wait "${sim_pid}" 2>/dev/null || true
    fi
    sim_pid=""
}
