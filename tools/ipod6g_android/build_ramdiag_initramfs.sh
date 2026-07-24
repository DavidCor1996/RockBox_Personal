#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source_file="${repo_root}/tools/ipod6g_android/ramdiag/init.S"
output_file="${1:-${repo_root}/tools/ipod6g_android/out/n25-ramdiag-initramfs.cpio.gz}"
output_dir="$(dirname "${output_file}")"
stage_dir="$(mktemp -d)"
source_date_epoch="${SOURCE_DATE_EPOCH:-1704067200}"

cleanup()
{
    rm -rf "${stage_dir}"
}
trap cleanup EXIT

mkdir -p "${output_dir}" "${stage_dir}/dev" "${stage_dir}/proc" \
    "${stage_dir}/sys"

clang --target=arm-linux-gnueabi -mcpu=arm926ej-s -marm -nostdlib -static \
    -fuse-ld=lld -Wl,-e,_start -Wl,--build-id=none \
    -o "${stage_dir}/init" "${source_file}"
chmod 0755 "${stage_dir}/init"

llvm-readelf -h -A -l "${stage_dir}/init" | \
    grep -q "Description: ARM v5TE"
if llvm-readelf -l "${stage_dir}/init" | grep -q "INTERP"; then
    echo "fatal: diagnostic init unexpectedly has a dynamic interpreter" >&2
    exit 1
fi

find "${stage_dir}" -exec touch --date="@${source_date_epoch}" {} +

(
    cd "${stage_dir}"
    find . -mindepth 1 -print0 | sort -z | \
        cpio --null --create --format=newc --owner=0:0 --reproducible \
            2>/dev/null | \
        gzip -n -9 > "${output_file}"
)

sha256sum "${output_file}"
