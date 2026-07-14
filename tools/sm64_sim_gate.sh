#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
build_dir=${SM64_BUILD_DIR:-"$root/build-sim-ipod6g"}
rom=${SM64_ROM:-"/home/david/Downloads/Super Mario 64 (USA).z64"}
frames=${SM64_TEST_FRAMES:-1800}
log="$build_dir/simdisk/.rockbox/rocks/games/sm64/sm64.log"

"$root/tools/sm64_sim_gate.py" --build-dir "$build_dir" --rom "$rom" \
    --frames "$frames"

cd "$build_dir"
if [[ ${SM64_TEST_REALTIME:-0} == 1 ]]; then
    SM64_TEST_FRAMES="$frames" \
    SM64_TEST_AUTOPLAY=1 \
    SM64_TEST_DUMP=/.rockbox/rocks/games/sm64/test-frame.ppm \
    SDL_VIDEODRIVER=x11 \
    ./rockboxui --zoom 1 --nobackground &
else
    SM64_TEST_FRAMES="$frames" \
    SM64_TEST_UNTHROTTLED=1 \
    SM64_TEST_AUTOPLAY=1 \
    SM64_TEST_DUMP=/.rockbox/rocks/games/sm64/test-frame.ppm \
    SDL_VIDEODRIVER=x11 \
    ./rockboxui --zoom 1 --nobackground &
fi
sim_pid=$!

cleanup() {
    kill "$sim_pid" 2>/dev/null || true
    wait "$sim_pid" 2>/dev/null || true
}
trap cleanup EXIT

for _ in $(seq 1 1200); do
    if test -f "$log" && grep -q "exit frames=$frames " "$log"; then
        break
    fi
    if ! kill -0 "$sim_pid" 2>/dev/null; then
        echo "FAIL: simulator exited before the SM64 gate completed" >&2
        exit 1
    fi
    sleep 0.1
done

if ! test -f "$log" || ! grep -q "exit frames=$frames " "$log"; then
    echo "FAIL: simulator gate timed out" >&2
    exit 1
fi

cleanup
trap - EXIT
cd "$root"
"$root/tools/sm64_sim_gate.py" --build-dir "$build_dir" --frames "$frames" \
    --validate
