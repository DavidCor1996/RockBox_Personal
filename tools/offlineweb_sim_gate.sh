#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-${repo_root}/build-sim-ipod6g}"
out_dir="${2:-/tmp/offlineweb-sim-shots}"
fixture_root="${3:-}"
start_path="${4:-rockbox:shortcuts}"
direct_action="${5:-}"
source_root="${build_dir}/simdisk"
rockboxui="${build_dir}/rockboxui"
plugin_binary="${build_dir}/apps/plugins/offlineweb.rock"
mpegplayer_binary="${build_dir}/apps/plugins/mpegplayer/mpegplayer.rock"
sim_root=""
sim_pid=""
wheel_gate=""

require_file()
{
    if [ ! -f "$1" ]; then
        printf "missing required file: %s\n" "$1" >&2
        exit 1
    fi
}

require_command()
{
    if ! command -v "$1" >/dev/null 2>&1; then
        printf "missing required command: %s\n" "$1" >&2
        exit 1
    fi
}

cleanup()
{
    if [ -n "${sim_pid}" ] && kill -0 "${sim_pid}" >/dev/null 2>&1; then
        kill "${sim_pid}" >/dev/null 2>&1 || true
        wait "${sim_pid}" 2>/dev/null || true
    fi
    if [ -n "${sim_root}" ] && [ -d "${sim_root}" ]; then
        rm -rf "${sim_root}"
    fi
}

capture()
{
    local window_id="$1"
    local name="$2"

    xdotool windowactivate --sync "${window_id}" >/dev/null 2>&1 || true
    import -window "${window_id}" "${out_dir}/${name}.png"
}

tap()
{
    local window_id="$1"
    local key="$2"

    xdotool windowactivate --sync "${window_id}" >/dev/null 2>&1 || true
    xdotool keydown --window "${window_id}" --clearmodifiers "${key}"
    sleep 0.15
    xdotool keyup --window "${window_id}" --clearmodifiers "${key}"
    sleep 0.4
}

require_file "${rockboxui}"
require_file "${plugin_binary}"
require_file "${mpegplayer_binary}"
require_file "${repo_root}/apps/plugins/bitmaps/native/rockboxlogo.220x68x16.bmp"
require_command xdotool
require_command import

trap cleanup EXIT INT TERM
sim_root="$(mktemp -d "${TMPDIR:-/tmp}/offlineweb-sim.XXXXXX")"
wheel_gate="${sim_root}/scroll-forward.gate"
mkdir -p "${sim_root}/.rockbox"
cp -a "${source_root}/.rockbox/." "${sim_root}/.rockbox/"
cp "${plugin_binary}" \
   "${sim_root}/.rockbox/rocks/apps/offlineweb.rock"
cp "${mpegplayer_binary}" \
   "${sim_root}/.rockbox/rocks/viewers/mpegplayer.rock"
if [ -n "${fixture_root}" ]; then
    if [ ! -d "${fixture_root}" ]; then
        printf "missing fixture root: %s\n" "${fixture_root}" >&2
        exit 1
    fi
    rm -rf "${sim_root}/.rockbox/offlineweb"
    ln -s "$(readlink -f "${fixture_root}")" \
        "${sim_root}/.rockbox/offlineweb"
else
    mkdir -p "${sim_root}/.rockbox/offlineweb/cache"
    mkdir -p "${sim_root}/.rockbox/offlineweb/test"
    cp "${repo_root}/apps/plugins/bitmaps/native/rockboxlogo.220x68x16.bmp" \
       "${sim_root}/.rockbox/offlineweb/test/hero.bmp"

    cat >"${sim_root}/.rockbox/offlineweb/cache/pages.tsv" <<'EOF'
RockPod Daily	https://daily.rockpod.local/	/.rockbox/offlineweb/test/index.html	Offline Archive	Technology	David	2026-07-25	iPod,offline,media
EOF

    cat >"${sim_root}/.rockbox/offlineweb/test/index.html" <<'EOF'
<!doctype html>
<html>
<head><title>RockPod Daily</title></head>
<body>
<h1>RockPod Daily</h1>
<p>A saved edition designed for the click wheel.</p>
<img src="hero.bmp" width="220" height="68" alt="RockPod">
<h2>Today's highlights</h2>
<p>The complete story remains readable without a network connection.</p>
<p>Compact typography, visible progress, and predictable wheel movement make long articles comfortable to browse.</p>
<p>Images are prepared as local sidecars, so the draw path never performs network work.</p>
<p>Links use the familiar blue iPod selection treatment.</p>
<a href="next.html">Read the next saved story</a>
<h2>Offline media</h2>
<video src="feature.mpg" title="Local feature video"></video>
<audio src="interview.wav" title="Saved audio interview"></audio>
<p>End of the saved edition.</p>
</body>
</html>
EOF

    cat >"${sim_root}/.rockbox/offlineweb/test/next.html" <<'EOF'
<html><body><h1>Next story</h1><p>Menu returns to the previous page.</p></body></html>
EOF

    touch "${sim_root}/.rockbox/offlineweb/test/feature.mpg"
    touch "${sim_root}/.rockbox/offlineweb/test/interview.wav"
fi
printf "\nbacklight timeout: 60\nstart in screen: root\n" \
    >>"${sim_root}/.rockbox/config.cfg"
mkdir -p "${out_dir}"

RBROOT="${sim_root}" \
ROCKBOX_SIM_PLUGIN="/.rockbox/rocks/apps/offlineweb.rock" \
ROCKBOX_SIM_PLUGIN_PARAM="${start_path}" \
ROCKPOD_SIM_SCROLL_FWD_GATE="${wheel_gate}" \
"${rockboxui}" --zoom 2 --nobackground --root "${sim_root}" \
    >"${out_dir}/simulator.log" 2>&1 &
sim_pid=$!

sleep "${ROCKBOX_SIM_START_DELAY:-7}"
window_id="$(xdotool search --pid "${sim_pid}" | head -1)"
if [ -z "${window_id}" ]; then
    printf "unable to find OfflineWeb simulator window\n" >&2
    exit 1
fi

if [ "${start_path}" != "rockbox:shortcuts" ]; then
    capture "${window_id}" "01-page-top"
    if [ "${direct_action}" = "natural-scroll" ]; then
        touch "${wheel_gate}"
        sleep 1
        rm -f "${wheel_gate}"
        sleep 1
        capture "${window_id}" "02-page-scrolled"
    elif [ -z "${direct_action}" ]; then
        tap "${window_id}" Down
        sleep 1
        capture "${window_id}" "02-page-scrolled"
        xdotool keydown --window "${window_id}" --clearmodifiers Down
        sleep 3
        xdotool keyup --window "${window_id}" --clearmodifiers Down
        sleep 2
        capture "${window_id}" "03-page-media"
    fi
    if [ "${direct_action}" = "select" ]; then
        sleep 2
        tap "${window_id}" KP_5
        sleep 10
        capture "${window_id}" "04-selected-fullscreen"
        tap "${window_id}" KP_Decimal
        sleep 1
        capture "${window_id}" "05-returned-page"
    elif [ "${direct_action}" = "subscriptions" ]; then
        xdotool keydown --window "${window_id}" \
            --clearmodifiers KP_Decimal
        sleep 1.2
        xdotool keyup --window "${window_id}" \
            --clearmodifiers KP_Decimal
        sleep 3
        capture "${window_id}" "04-subscriptions"
        capture "${window_id}" "05-channel-focus"
        tap "${window_id}" KP_5
        sleep 3
        capture "${window_id}" "06-channel-videos"
    fi
    printf "OfflineWeb simulator captures: %s\n" "${out_dir}"
    exit 0
fi

capture "${window_id}" "01-library"
tap "${window_id}" KP_5
sleep 1
capture "${window_id}" "02-page-top"
xdotool key --window "${window_id}" --repeat 14 --delay 55 Down
sleep 1
capture "${window_id}" "03-page-scrolled"
if [ -n "${fixture_root}" ]; then
    xdotool key --window "${window_id}" --repeat 70 --delay 45 Down
    sleep 2
    capture "${window_id}" "04-first-site-media"
    tap "${window_id}" KP_Decimal
    sleep 1
    capture "${window_id}" "05-library-return"
    tap "${window_id}" KP_6
    capture "${window_id}" "06-second-site-focus"
    tap "${window_id}" KP_5
    sleep 1
    capture "${window_id}" "07-second-site-top"
    xdotool key --window "${window_id}" --repeat 22 --delay 55 Down
    sleep 1
    capture "${window_id}" "08-second-site-scrolled"
else
    tap "${window_id}" KP_6
    capture "${window_id}" "04-link-focus"
    xdotool key --window "${window_id}" --repeat 70 --delay 45 Down
    sleep 1
    capture "${window_id}" "05-media"
    tap "${window_id}" KP_Decimal
    capture "${window_id}" "06-library-return"
fi

printf "OfflineWeb simulator captures: %s\n" "${out_dir}"
