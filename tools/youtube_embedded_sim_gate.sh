#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-${repo_root}/build-sim-ipod6g}"
fixture_root="${2:?offlineweb fixture root required}"
video_path="${3:?device-relative MPEG path required}"
out_dir="${4:-/tmp/youtube-embedded-sim}"
sim_root="$(mktemp -d /tmp/youtube-embedded-sim.XXXXXX)"
sim_pid=""

cleanup()
{
    if [ -n "${sim_pid}" ] && kill -0 "${sim_pid}" 2>/dev/null; then
        kill "${sim_pid}" 2>/dev/null || true
        wait "${sim_pid}" 2>/dev/null || true
    fi
    rm -rf "${sim_root}"
}

trap cleanup EXIT INT TERM
mkdir -p "${sim_root}/.rockbox/rocks/viewers" "${out_dir}"
cp -a "${build_dir}/simdisk/.rockbox/." "${sim_root}/.rockbox/"
rm -rf "${sim_root}/.rockbox/offlineweb"
ln -s "$(readlink -f "${fixture_root}")" \
    "${sim_root}/.rockbox/offlineweb"
cp "${build_dir}/apps/plugins/mpegplayer/mpegplayer.rock" \
    "${sim_root}/.rockbox/rocks/viewers/mpegplayer.rock"
printf '\nbacklight timeout: 60\nstart in screen: root\n' \
    >>"${sim_root}/.rockbox/config.cfg"

SDL_AUDIODRIVER=disk \
SDL_DISKAUDIOFILE="${out_dir}/embedded-audio.raw" \
RBROOT="${sim_root}" \
ROCKBOX_SIM_PLUGIN="/.rockbox/rocks/viewers/mpegplayer.rock" \
ROCKBOX_SIM_PLUGIN_PARAM="youtube:${video_path}" \
"${build_dir}/rockboxui" --zoom 2 --nobackground --root "${sim_root}" \
    >"${out_dir}/simulator.log" 2>&1 &
sim_pid=$!

sleep 8
window_id="$(xdotool search --pid "${sim_pid}" | head -1)"
test -n "${window_id}"
import -window "${window_id}" "${out_dir}/01-embedded-with-audio.png"
xdotool keydown --window "${window_id}" --clearmodifiers KP_5
sleep 0.15
xdotool keyup --window "${window_id}" --clearmodifiers KP_5
sleep 3
import -window "${window_id}" "${out_dir}/02-select-fullscreen.png"
test -s "${out_dir}/embedded-audio.raw"
printf 'YouTube embedded simulator captures: %s\n' "${out_dir}"
