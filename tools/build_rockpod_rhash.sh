#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
output="${1:-${repo_root}/tools/rockpod_rhash}"

cc -O2 -std=c99 -D_POSIX_C_SOURCE=200809L \
    -DRC_HASH_NO_DISC -DRC_HASH_NO_ENCRYPTED -DRC_HASH_NO_ZIP \
    -I"${repo_root}/lib/rcheevos/include" \
    -I"${repo_root}/lib/rcheevos/src" \
    "${repo_root}/tools/rockpod_rhash.c" \
    "${repo_root}/lib/rcheevos/src/rc_compat.c" \
    "${repo_root}/lib/rcheevos/src/rhash/hash.c" \
    "${repo_root}/lib/rcheevos/src/rhash/hash_rom.c" \
    "${repo_root}/lib/rcheevos/src/rhash/md5.c" \
    -o "${output}"

echo "built ${output}"
