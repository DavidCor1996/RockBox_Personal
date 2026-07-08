#!/bin/sh
# Build and validate a Rockbox simulator before any physical iPod deploy.

set -eu

usage() {
    cat <<'EOF'
Usage: tools/simulator_first_gate.sh [options]

Options:
  --target TARGET          Simulator target: ipodvideo, ipod6g, or ipod3g.
                           Default: ipodvideo.
  --build-dir DIR         Build directory. Default depends on target.
  --jobs N                Parallel make jobs. Default: nproc or 4.
  --rockpod-tests         Run the RockPod pytest suite before simulator build.
  --theme-tests           Run focused WPS/SBS/FMS RockPod theme tests.
  --skip-build            Skip configure/make/make install and only validate files.
  --smoke                 Launch the simulator for a timed smoke run.
  --timeout SECONDS       Smoke-run duration. Default: 12.
  --allow-mounted-ipod    Do not fail if a likely physical iPod is mounted.
  --evidence-file PATH    Write a simulator gate evidence note.
  --known-warning TEXT    Add an expected warning to the evidence note.
  --manual-checklist      Print the manual feature checklist after the gate.
  --list-targets          Print supported simulator targets and exit.
  -h, --help              Show this help.

The script never writes to a physical iPod mount. It uses the simulator simdisk
under the selected build directory.
EOF
}

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
original_args=$*
target="ipodvideo"
build_dir=""
jobs="${ROCKBOX_JOBS:-}"
run_rockpod_tests=0
run_theme_tests=0
skip_build=0
smoke=0
timeout_seconds=12
allow_mounted_ipod=0
evidence_file=""
known_warnings=""
manual_checklist=0
build_result="not run"
smoke_result="not run"
smoke_elapsed_seconds="not measured"
tests_result="not run"
theme_tests_result="not run"
source_result="not checked"
simdisk_result="not checked"
theme_overlay_result="not run"
smoke_log=""

print_targets() {
    cat <<'EOF'
Supported simulator targets:
  ipodvideo, 5g   -> build-sim-video-5g   iPod Video 5G/5.5G
  ipod6g, 6g      -> build-sim-ipod6g     iPod Classic 6G/7G
  ipod3g, 3g      -> build-sim-3g         iPod 3G

For targets without a matching simulator, use the closest available simulator
and document the validation gap before hardware deploy.
EOF
}

print_manual_checklist() {
    cat <<'EOF'
Manual simulator checklist:
  [ ] Root menu/settings: scroll top to bottom, enter changed items, back out cleanly.
  [ ] WPS/SBS/FMS/theme: load theme, open WPS/menus, check hold/lockscreen state.
  [ ] Fonts/icons/bitmaps: no missing image, clipped title, blank backdrop, or bad color.
  [ ] Video browser: verify empty state, supported .mpg entry, unsupported format message.
  [ ] Plugins/games: launch changed plugin, exit cleanly, keep save/config under simdisk.
  [ ] Database/tagcache: scan small Music set, reboot simulator, Music opens as expected.
  [ ] Playlists: exported/imported .m3u8 paths are device-relative or Rockbox-absolute.
  [ ] Boot/runtime assets: first visible screen is not blank or corrupted.
  [ ] RockPod sync: inspect created/updated/deleted paths, then open simulator on that disk.
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --target)
            [ "$#" -gt 1 ] || { echo "missing value for --target" >&2; exit 2; }
            target=$2
            shift 2
            ;;
        --build-dir)
            [ "$#" -gt 1 ] || { echo "missing value for --build-dir" >&2; exit 2; }
            build_dir=$2
            shift 2
            ;;
        --jobs)
            [ "$#" -gt 1 ] || { echo "missing value for --jobs" >&2; exit 2; }
            jobs=$2
            shift 2
            ;;
        --rockpod-tests)
            run_rockpod_tests=1
            shift
            ;;
        --theme-tests)
            run_theme_tests=1
            shift
            ;;
        --skip-build)
            skip_build=1
            shift
            ;;
        --smoke)
            smoke=1
            shift
            ;;
        --timeout)
            [ "$#" -gt 1 ] || { echo "missing value for --timeout" >&2; exit 2; }
            timeout_seconds=$2
            shift 2
            ;;
        --allow-mounted-ipod)
            allow_mounted_ipod=1
            shift
            ;;
        --evidence-file)
            [ "$#" -gt 1 ] || { echo "missing value for --evidence-file" >&2; exit 2; }
            evidence_file=$2
            shift 2
            ;;
        --known-warning)
            [ "$#" -gt 1 ] || { echo "missing value for --known-warning" >&2; exit 2; }
            if [ -n "$known_warnings" ]; then
                known_warnings="${known_warnings}
$2"
            else
                known_warnings=$2
            fi
            shift 2
            ;;
        --manual-checklist)
            manual_checklist=1
            shift
            ;;
        --list-targets)
            print_targets
            exit 0
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

case "$target" in
    ipodvideo|5g)
        target="ipodvideo"
        default_build_dir="build-sim-video-5g"
        ;;
    ipod6g|6g)
        target="ipod6g"
        default_build_dir="build-sim-ipod6g"
        ;;
    ipod3g|3g)
        target="ipod3g"
        default_build_dir="build-sim-3g"
        ;;
    *)
        echo "unsupported target: $target" >&2
        exit 2
        ;;
esac

if [ -z "$build_dir" ]; then
    build_dir=$default_build_dir
fi

case "$timeout_seconds" in
    *[!0-9]*|"")
        echo "--timeout must be a positive integer" >&2
        exit 2
        ;;
esac

if [ -z "$jobs" ]; then
    if command -v nproc >/dev/null 2>&1; then
        jobs=$(nproc)
    else
        jobs=4
    fi
fi

case "$build_dir" in
    /*) build_abs=$build_dir ;;
    *) build_abs=$repo_root/$build_dir ;;
esac
binary=$build_abs/rockboxui
simdisk=$build_abs/simdisk
rockbox_root=$simdisk/.rockbox
gate_root=
active_simdisk=

cleanup() {
    if [ -n "$gate_root" ] && [ -d "$gate_root" ]; then
        rm -rf "$gate_root"
    fi
}
trap cleanup EXIT INT TERM

log() {
    printf '%s\n' "$*"
}

mounted_ipod_paths() {
    [ -r /proc/mounts ] || return 0
    awk '{print $2}' /proc/mounts | sed 's/\\040/ /g' | while IFS= read -r mount_point; do
        case "$mount_point" in
            /media/*|/run/media/*|/mnt/*)
                if [ -d "$mount_point/.rockbox" ] ||
                   [ -d "$mount_point/iPod_Control" ] ||
                   printf '%s\n' "$mount_point" | grep -qi 'ipod'; then
                    printf '%s\n' "$mount_point"
                fi
                ;;
        esac
    done
}

assert_no_mounted_ipod() {
    mounted=$(mounted_ipod_paths || true)
    if [ -n "$mounted" ] && [ "$allow_mounted_ipod" -ne 1 ]; then
        echo "Refusing simulator gate while a likely physical iPod is mounted:" >&2
        printf '%s\n' "$mounted" >&2
        echo "Unmount it first, or pass --allow-mounted-ipod if this is intentional." >&2
        exit 1
    fi
}

append_smoke_warnings() {
    [ -n "$smoke_log" ] || return 0
    [ -f "$smoke_log" ] || return 0
    warnings=$(grep -E "NOT FOUND|WARNING|WARN|panic|PANIC|error|ERROR" "$smoke_log" || true)
    [ -n "$warnings" ] || return 0
    if [ -n "$known_warnings" ]; then
        known_warnings="${known_warnings}
Smoke output:
${warnings}"
    else
        known_warnings="Smoke output:
${warnings}"
    fi
}

write_evidence() {
    output_path=$1
    {
        echo "Simulator gate:"
        echo "- Date: $(date '+%Y-%m-%d %H:%M:%S %Z')"
        echo "- Target: $target"
        echo "- Command: tools/simulator_first_gate.sh $original_args"
        echo "- Build result: $build_result"
        echo "- Smoke result: $smoke_result"
        echo "- Smoke elapsed seconds: $smoke_elapsed_seconds"
        echo "- RockPod tests: $tests_result"
        echo "- WPS/SBS/FMS theme tests: $theme_tests_result"
        echo "- Source hygiene: $source_result"
        echo "- Simdisk isolation: $simdisk_result"
        echo "- Theme source overlay: $theme_overlay_result"
        echo "- Simulator build directory: $build_abs"
        echo "- Simulator binary: $binary"
        echo "- Simulator source simdisk: $simdisk"
        echo "- Hardware deploy allowed: yes"
        echo "- Known warnings:"
        if [ -n "$known_warnings" ]; then
            printf '%s\n' "$known_warnings" | sed 's/^/  - /'
        else
            echo "  - none recorded"
        fi
        echo "- Manual checks:"
        echo "  - not recorded by automated gate"
    } > "$output_path"
}

show_source_state() {
    log "== Source state =="
    git -C "$repo_root" status --short -- . ':(exclude)build-*' | awk '
        {
            printed++
            if (printed <= 120)
                print
            else
                omitted++
        }
        END {
            if (omitted > 0)
                printf("... %d more status entries omitted\n", omitted)
        }
    '
    git -C "$repo_root" diff --stat -- . ':(exclude)build-*' | awk '
        {
            printed++
            if (printed <= 80)
                print
            else
                omitted++
        }
        END {
            if (omitted > 0)
                printf("... %d more diff-stat lines omitted\n", omitted)
        }
    ' || true
    omitted_build=$(git -C "$repo_root" status --short -- build-* 2>/dev/null | wc -l | tr -d ' ')
    if [ "${omitted_build:-0}" -gt 0 ]; then
        log "... $omitted_build build output status entries omitted"
    fi
    source_result="checked"
}

run_tests() {
    log "== RockPod tests =="
    if [ ! -x "$repo_root/rockpod/.venv/bin/python" ]; then
        echo "RockPod virtualenv is missing: rockpod/.venv/bin/python" >&2
        exit 1
    fi
    cd "$repo_root/rockpod"
    ./.venv/bin/python -m pytest tests/ -v
    tests_result="passed"
}

run_focused_theme_tests() {
    log "== WPS/SBS/FMS theme tests =="
    if [ ! -x "$repo_root/rockpod/.venv/bin/python" ]; then
        echo "RockPod virtualenv is missing: rockpod/.venv/bin/python" >&2
        exit 1
    fi
    cd "$repo_root/rockpod"
    ./.venv/bin/python -m pytest \
        tests/test_rockbox_theme_phase1.py \
        tests/test_ipone_wallpapers.py \
        tests/test_rockbox_simulator.py \
        -q
    theme_tests_result="passed"
}

build_simulator() {
    log "== Simulator build: $target in $build_dir =="
    mkdir -p "$build_abs"
    if [ ! -f "$build_abs/Makefile" ]; then
        cd "$build_abs"
        ../tools/configure --target="$target" --type=s
    fi
    make -C "$build_abs" -j"$jobs"
    make -C "$build_abs" install
    build_result="passed"
}

prepare_simdisk() {
    log "== Simdisk validation =="
    [ -x "$binary" ] || { echo "missing simulator binary: $binary" >&2; exit 1; }
    [ -d "$simdisk" ] || { echo "missing simulator simdisk: $simdisk" >&2; exit 1; }
    [ -d "$rockbox_root" ] || { echo "missing Rockbox runtime dir: $rockbox_root" >&2; exit 1; }

    gate_root=${TMPDIR:-/tmp}/rockbox-sim-gate-${target}-$$
    active_simdisk=$gate_root/simdisk
    mkdir -p "$gate_root"
    cp -a "$simdisk" "$active_simdisk"
    rm -rf "$active_simdisk/Music"
    mkdir -p "$active_simdisk/Music" "$active_simdisk/Playlists" "$active_simdisk/Videos"
    [ -d "$active_simdisk/.rockbox" ] || { echo "isolated simdisk is missing .rockbox" >&2; exit 1; }
    overlay_theme_sources

    log "Simulator binary: $binary"
    log "Simulator simdisk: $simdisk"
    log "Isolated test simdisk: $active_simdisk"
    simdisk_result="passed"
}

overlay_theme_sources() {
    log "Overlaying current source theme assets into isolated simdisk."
    for dir_name in themes wps backdrops icons fonts; do
        if [ -d "$repo_root/$dir_name" ]; then
            mkdir -p "$active_simdisk/.rockbox/$dir_name"
            cp -a "$repo_root/$dir_name/." "$active_simdisk/.rockbox/$dir_name/"
        fi
    done
    theme_overlay_result="passed"
}

run_smoke() {
    log "== Timed simulator smoke run (${timeout_seconds}s) =="
    cd "$build_abs"
    smoke_log=$gate_root/smoke.log
    smoke_started_at=$(date +%s)
    SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-x11}" \
    SDL_RENDER_DRIVER="${SDL_RENDER_DRIVER:-software}" \
    "$binary" --nobackground --root "$active_simdisk" --zoom 1 > "$smoke_log" 2>&1 &
    pid=$!

    sleep "$timeout_seconds"

    if kill -0 "$pid" >/dev/null 2>&1; then
        smoke_finished_at=$(date +%s)
        smoke_elapsed_seconds=$((smoke_finished_at - smoke_started_at))
        kill "$pid" >/dev/null 2>&1 || true
        sleep 2
        if kill -0 "$pid" >/dev/null 2>&1; then
            kill -KILL "$pid" >/dev/null 2>&1 || true
        fi
        wait "$pid" >/dev/null 2>&1 || true
        log "Simulator stayed alive for ${timeout_seconds}s (${smoke_elapsed_seconds}s measured)."
        append_smoke_warnings
        smoke_result="passed (${timeout_seconds}s requested, ${smoke_elapsed_seconds}s measured)"
        return 0
    fi

    set +e
    wait "$pid"
    status=$?
    set -e
    smoke_finished_at=$(date +%s)
    smoke_elapsed_seconds=$((smoke_finished_at - smoke_started_at))
    append_smoke_warnings
    smoke_result="failed before ${timeout_seconds}s, status $status (${smoke_elapsed_seconds}s measured)"
    echo "Simulator exited before ${timeout_seconds}s with status $status (${smoke_elapsed_seconds}s measured)" >&2
    exit "$status"
}

log "Simulator-first gate"
log "Repo: $repo_root"
log "Target: $target"

assert_no_mounted_ipod
show_source_state

if [ "$run_rockpod_tests" -eq 1 ]; then
    run_tests
fi

if [ "$run_theme_tests" -eq 1 ]; then
    run_focused_theme_tests
fi

if [ "$skip_build" -ne 1 ]; then
    build_simulator
else
    log "== Simulator build skipped =="
    build_result="skipped"
fi

prepare_simdisk

if [ "$smoke" -eq 1 ]; then
    run_smoke
fi

if [ "$manual_checklist" -eq 1 ]; then
    print_manual_checklist
fi

if [ -n "$evidence_file" ]; then
    evidence_dir=$(dirname -- "$evidence_file")
    mkdir -p "$evidence_dir"
    write_evidence "$evidence_file"
    log "Evidence written: $evidence_file"
fi

log "Simulator-first gate passed."
