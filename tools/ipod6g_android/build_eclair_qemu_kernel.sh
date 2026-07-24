#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
linux_source="${1:-}"
build_dir="${2:-${repo_root}/tools/ipod6g_android/out/eclair-qemu-arm926}"
jobs="${JOBS:-$(nproc)}"
linux_revision="a1bf13a446201cdd97e56815cedc6b1da14f1313"
binder_patch="${repo_root}/tools/ipod6g_android/patches/linux-android-binder-ipc32.patch"
config_fragment="${repo_root}/tools/ipod6g_android/linux-overlay/arch/arm/configs/eclair_qemu_arm926_defconfig"
host_bin="${build_dir}/host-bin"

fatal()
{
    echo "fatal: $*" >&2
    exit 1
}

[[ -n "${linux_source}" ]] || fatal "usage: $0 LINUX_SOURCE [BUILD_DIR]"
linux_source="$(realpath "${linux_source}")"
build_dir="$(realpath -m "${build_dir}")"
[[ "$(git -C "${linux_source}" rev-parse HEAD)" == "${linux_revision}" ]] || \
    fatal "Linux revision does not match the qualified pin"

if ! git -C "${linux_source}" apply --reverse --check "${binder_patch}" \
    >/dev/null 2>&1; then
    git -C "${linux_source}" apply --check "${binder_patch}"
    git -C "${linux_source}" apply "${binder_patch}"
fi

mkdir -p "${build_dir}" "${host_bin}"
ln -sf "${repo_root}/tools/ipod6g_android/timeconst_bc_shim.py" "${host_bin}/bc"
PATH="${host_bin}:/usr/bin:/bin" \
    make -C "${linux_source}" O="${build_dir}" ARCH=arm \
    KCONFIG_ALLCONFIG="${config_fragment}" allnoconfig
PATH="${host_bin}:/usr/bin:/bin" \
    make -C "${linux_source}" O="${build_dir}" ARCH=arm \
    CROSS_COMPILE=arm-none-eabi- -j"${jobs}" \
    zImage arm/versatile-pb.dtb

echo "QEMU ARM926 Eclair kernel built at ${build_dir}"
echo "No drive, network backend, USB device, or physical hardware was accessed."
