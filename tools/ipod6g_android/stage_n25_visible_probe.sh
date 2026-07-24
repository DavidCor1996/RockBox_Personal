#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
bundle="${repo_root}/rockpod/bin/ipod6g-android/visible-probe"
mount_root="${1:-}"
target_rel=".rockbox/android/diagnostic-trace2"

fatal()
{
    echo "fatal: $*" >&2
    exit 1
}

usage()
{
    echo "usage: $0 /absolute/path/to/mounted-ipod" >&2
    echo "create-only staging; never accepts a block-device path" >&2
    exit 2
}

protected_snapshot()
{
    local root="$1"
    local path

    for path in \
        "${root}/rockbox.ipod" \
        "${root}/.rockbox/rockbox.ipod" \
        "${root}/.rockbox/config.cfg"; do
        [[ ! -f "${path}" ]] || sha256sum "${path}"
    done
    find "${root}/.rockbox" -maxdepth 1 -type f \
        \( -name 'database*.tcd' -o -name 'tagcache*.tcd' \) \
        -print0 | sort -z | xargs -0 -r sha256sum
}

[[ -n "${mount_root}" ]] || usage
[[ "${mount_root}" == /* ]] || fatal "mount path must be absolute"
[[ -d "${mount_root}" ]] || fatal "mount path is not a directory"
mount_root="$(realpath "${mount_root}")"

mounted_target="$(findmnt -f -n -o TARGET --target "${mount_root}")"
[[ -n "${mounted_target}" ]] || fatal "path is not on a mounted filesystem"
[[ "$(realpath "${mounted_target}")" == "${mount_root}" ]] || \
    fatal "path is not the filesystem mount root: ${mounted_target}"
mounted_fstype="$(findmnt -f -n -o FSTYPE --target "${mount_root}")"
[[ "${mounted_fstype}" == "vfat" ]] || \
    fatal "expected the iPod FAT volume, found ${mounted_fstype}"
[[ -d "${mount_root}/.rockbox" ]] || fatal "mounted volume has no .rockbox"
[[ ! -L "${mount_root}/.rockbox" ]] || fatal ".rockbox must not be a symlink"
[[ -f "${mount_root}/rockbox.ipod" ]] || fatal "root rockbox.ipod is missing"
[[ -f "${mount_root}/.rockbox/rockbox.ipod" ]] || \
    fatal ".rockbox/rockbox.ipod is missing"
cmp "${mount_root}/rockbox.ipod" \
    "${mount_root}/.rockbox/rockbox.ipod" >/dev/null || \
    fatal "the two installed Rockbox firmware copies do not match"

python3 -c \
    'import json,sys; q=json.load(open(sys.argv[1])); assert q.get("artifact_gate_passed") is True and q.get("device_test_ready") is True and q.get("hardware_qualified") is False' \
    "${bundle}/qualification.json" || fatal "bundle is not device-test ready"
(
    cd "${bundle}"
    sha256sum -c SHA256SUMS
) || fatal "source bundle checksum verification failed"

target="${mount_root}/${target_rel}"
staging="${target}.staging"
[[ ! -e "${target}" ]] || fatal "target already exists; refusing overwrite: ${target}"
[[ ! -e "${staging}" ]] || \
    fatal "incomplete staging directory exists; inspect it first: ${staging}"
[[ ! -L "${mount_root}/.rockbox/android" ]] || \
    fatal ".rockbox/android must not be a symlink"

before="$(mktemp /tmp/n25-visible-protected-before.XXXXXX)"
after="$(mktemp /tmp/n25-visible-protected-after.XXXXXX)"
trap 'rm -f "${before}" "${after}"' EXIT
protected_snapshot "${mount_root}" > "${before}"

mkdir -p "${mount_root}/.rockbox/android"
mkdir "${staging}"
for name in \
    n25-visible-kernel.ipod \
    n25-visible-initramfs.ipod \
    n25-visible-dtb.ipod; do
    cp --no-clobber "${bundle}/${name}" "${staging}/${name}"
    cmp "${bundle}/${name}" "${staging}/${name}" >/dev/null || \
        fatal "read-back mismatch while staging ${name}"
done
mv "${staging}" "${target}"
sync -f "${mount_root}"

for name in \
    n25-visible-kernel.ipod \
    n25-visible-initramfs.ipod \
    n25-visible-dtb.ipod; do
    cmp "${bundle}/${name}" "${target}/${name}" >/dev/null || \
        fatal "post-sync mismatch for ${name}"
done

protected_snapshot "${mount_root}" > "${after}"
cmp "${before}" "${after}" >/dev/null || \
    fatal "protected Rockbox firmware/database/config hashes changed"

echo "N25 visible probe staged and read-back verified at ${target}"
echo "Protected Rockbox firmware/database/config hashes are unchanged."
echo "Unmount the volume cleanly before entering DFU."
