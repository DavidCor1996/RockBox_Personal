#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT

cc -std=c99 -O2 \
    -I"$repo_root/lib/rcheevos/include" \
    -I"$repo_root/lib/rcheevos/src" \
    "$repo_root/tools/rockachievements_runtime_gate.c" \
    "$repo_root/lib/rcheevos/src/rcheevos/alloc.c" \
    "$repo_root/lib/rcheevos/src/rcheevos/condition.c" \
    "$repo_root/lib/rcheevos/src/rcheevos/condset.c" \
    "$repo_root/lib/rcheevos/src/rcheevos/format.c" \
    "$repo_root/lib/rcheevos/src/rcheevos/lboard.c" \
    "$repo_root/lib/rcheevos/src/rcheevos/memref.c" \
    "$repo_root/lib/rcheevos/src/rcheevos/operand.c" \
    "$repo_root/lib/rcheevos/src/rcheevos/runtime.c" \
    "$repo_root/lib/rcheevos/src/rcheevos/runtime_progress.c" \
    "$repo_root/lib/rcheevos/src/rcheevos/richpresence.c" \
    "$repo_root/lib/rcheevos/src/rcheevos/trigger.c" \
    "$repo_root/lib/rcheevos/src/rcheevos/value.c" \
    "$repo_root/lib/rcheevos/src/rhash/md5.c" \
    "$repo_root/lib/rcheevos/src/rc_compat.c" \
    "$repo_root/lib/rcheevos/src/rc_util.c" \
    -lm -o "$build_dir/runtime-gate"

"$build_dir/runtime-gate"
