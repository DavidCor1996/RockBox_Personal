#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source_file="${repo_root}/tools/ipodtouch4_android/ramdiag/init.S"
output_dir="${1:-${repo_root}/tools/ipodtouch4_android/out/ramdiag}"
stage_dir="$(mktemp -d)"
source_date_epoch="${SOURCE_DATE_EPOCH:-1338031427}"

cleanup()
{
    rm -rf "${stage_dir}"
}
trap cleanup EXIT

if [[ -e "${output_dir}" ]]; then
    echo "fatal: output directory already exists: ${output_dir}" >&2
    exit 2
fi
mkdir -p "${output_dir}" "${stage_dir}/dev" "${stage_dir}/proc" \
    "${stage_dir}/sys"

clang --target=arm-linux-gnueabi -mcpu=cortex-a8 -marm -nostdlib -static \
    -fuse-ld=lld -Wl,-e,_start -Wl,--build-id=none \
    -o "${stage_dir}/init" "${source_file}"
chmod 0755 "${stage_dir}/init"
cp "${stage_dir}/init" "${output_dir}/init-n81-ramdiag.elf"

find "${stage_dir}" -exec touch --date="@${source_date_epoch}" {} +
(
    cd "${stage_dir}"
    find . -mindepth 1 -print0 | sort -z | \
        cpio --null --create --format=newc --owner=0:0 --reproducible \
            2>/dev/null | \
        gzip -n -9 > "${output_dir}/n81-ramdiag-initramfs.cpio.gz"
)

"${repo_root}/tools/ipodtouch4_android/qualify_ramdiag.py" \
    "${output_dir}/init-n81-ramdiag.elf" \
    "${output_dir}/n81-ramdiag-initramfs.cpio.gz" \
    --output "${output_dir}/ramdiag-qualification.json"

sha256sum \
    "${output_dir}/init-n81-ramdiag.elf" \
    "${output_dir}/n81-ramdiag-initramfs.cpio.gz" \
    "${output_dir}/ramdiag-qualification.json"
