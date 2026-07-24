#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
uboot_source="${1:-}"
linux_source="${2:-}"
wind3x_bin="${3:-}"
eclair_root_bundle="${4:-}"
install_dir="${5:-${repo_root}/rockpod/bin/ipod6g-android/eclair-native}"
build_root="${IPOD6G_ANDROID_BUILD_ROOT:-${repo_root}/tools/ipod6g_android/out}/eclair-native"
jobs="${JOBS:-$(nproc)}"
source_date_epoch="${SOURCE_DATE_EPOCH:-1704067200}"
build_timestamp="$(date --utc --date="@${source_date_epoch}" \
    "+%a %b %e %T UTC %Y")"

uboot_revision="bda1eed7f82bc0c8e7a3d88e19602ad6248a25f9"
linux_revision="a1bf13a446201cdd97e56815cedc6b1da14f1313"
uboot_patch="${repo_root}/tools/ipod6g_android/patches/uboot-s5l8702-n25-ramdiag.patch"
linux_patch="${repo_root}/tools/ipod6g_android/patches/linux-s5l8702-armv5-selection.patch"
linux_vic_patch="${repo_root}/tools/ipod6g_android/patches/linux-s5l8702-n25-vic.patch"
linux_timer_patch="${repo_root}/tools/ipod6g_android/patches/linux-s5l8702-n25-timer.patch"
linux_binder_patch="${repo_root}/tools/ipod6g_android/patches/linux-android-binder-ipc32.patch"
linux_lcd_patch="${repo_root}/tools/ipod6g_android/patches/linux-s5l8702-n25-lcd-handoff.patch"
linux_input_reset_patch="${repo_root}/tools/ipod6g_android/patches/linux-s5l8702-n25-input-reset.patch"
linux_overlay="${repo_root}/tools/ipod6g_android/linux-overlay"

usage()
{
    echo "usage: $0 UBOOT_SOURCE LINUX_SOURCE WIND3X_BIN ECLAIR_ROOT_BUNDLE [INSTALL_DIR]" >&2
    echo "builds and qualifies RAM-only artifacts; it never accesses an iPod" >&2
    exit 2
}

fatal()
{
    echo "fatal: $*" >&2
    exit 1
}

[[ -n "${uboot_source}" && -n "${linux_source}" && -n "${wind3x_bin}" && \
   -n "${eclair_root_bundle}" ]] || usage
uboot_source="$(realpath "${uboot_source}")"
linux_source="$(realpath "${linux_source}")"
wind3x_bin="$(realpath "${wind3x_bin}")"
eclair_root_bundle="$(realpath "${eclair_root_bundle}")"
install_dir="$(realpath -m "${install_dir}")"
build_root="$(realpath -m "${build_root}")"
source_initramfs="${eclair_root_bundle}/eclair-native-initramfs.cpio.gz"
source_root_report="${eclair_root_bundle}/qualification.json"
source_system_report="${eclair_root_bundle}/system-emulation.json"
initramfs="${install_dir}/n25-eclair-native-initramfs.cpio.gz"
root_report="${install_dir}/eclair-root-qualification.json"
system_report="${install_dir}/eclair-system-emulation.json"
input_report="${install_dir}/n25-input-emulation.json"
fit="${install_dir}/n25-eclair-native.itb"
dfu="${install_dir}/n25-eclair-native-uboot.dfu"
kernel_ipod="${install_dir}/n25-eclair-kernel.ipod"
initramfs_ipod="${install_dir}/n25-eclair-initramfs.ipod"
dtb_ipod="${install_dir}/n25-eclair-dtb.ipod"
rockbox_bootloader="${install_dir}/n25-eclair-menu-play-bootloader.ipod"
rockbox_bootloader_dfu="${install_dir}/n25-eclair-menu-play-bootloader.dfu"
its="${build_root}/n25-eclair-native.its"
uboot_build="${build_root}/uboot"
linux_build="${build_root}/linux"
host_bin="${build_root}/host-bin"
rockbox_bootloader_build="${build_root}/rockbox-bootloader"

[[ -x "${wind3x_bin}" ]] || fatal "wInd3x executable not found: ${wind3x_bin}"
[[ -f "${source_initramfs}" && ! -L "${source_initramfs}" ]] || \
    fatal "qualified Eclair initramfs not found"
[[ -f "${source_root_report}" && ! -L "${source_root_report}" ]] || \
    fatal "qualified Eclair root report not found"
[[ -f "${source_system_report}" && ! -L "${source_system_report}" ]] || \
    fatal "qualified Eclair system-emulation report not found"
[[ "$(git -C "${uboot_source}" rev-parse HEAD)" == "${uboot_revision}" ]] || \
    fatal "U-Boot revision does not match the qualified pin"
[[ "$(git -C "${linux_source}" rev-parse HEAD)" == "${linux_revision}" ]] || \
    fatal "Linux revision does not match the qualified pin"

apply_patch_set_once()
{
    local source_tree="$1"
    local patch_file
    local all_applied=true
    shift

    for patch_file in "$@"; do
        if ! git -C "${source_tree}" apply --reverse --check "${patch_file}" \
            >/dev/null 2>&1; then
            all_applied=false
            break
        fi
    done
    if [[ "${all_applied}" == true ]]; then
        return
    fi
    if ! git -C "${source_tree}" diff --quiet || \
       ! git -C "${source_tree}" diff --cached --quiet || \
       [[ -n "$(git -C "${source_tree}" ls-files --others --exclude-standard)" ]]; then
        fatal "refusing to patch a dirty source tree: ${source_tree}"
    fi
    git -C "${source_tree}" apply --check "$@"
    git -C "${source_tree}" apply "$@"
}

install_overlay_file()
{
    local source_file="$1"
    local destination_file="$2"

    if [[ -e "${destination_file}" ]]; then
        cmp "${source_file}" "${destination_file}" >/dev/null || \
            fatal "source overlay conflicts with ${destination_file}"
        return
    fi
    mkdir -p "$(dirname "${destination_file}")"
    cp "${source_file}" "${destination_file}"
}

apply_patch_set_once "${uboot_source}" "${uboot_patch}"
apply_patch_set_once "${linux_source}" "${linux_patch}" \
    "${linux_vic_patch}" "${linux_timer_patch}" "${linux_binder_patch}" \
    "${linux_lcd_patch}" "${linux_input_reset_patch}"
install_overlay_file \
    "${linux_overlay}/arch/arm/configs/apple_n25_eclair_native_defconfig" \
    "${linux_source}/arch/arm/configs/apple_n25_eclair_native_defconfig"
install_overlay_file \
    "${linux_overlay}/arch/arm/boot/dts/samsung/s5l8702-n25-eclair-native.dts" \
    "${linux_source}/arch/arm/boot/dts/samsung/s5l8702-n25-eclair-native.dts"

mkdir -p "${install_dir}" "${build_root}" "${host_bin}"
ln -sf "${repo_root}/tools/ipod6g_android/timeconst_bc_shim.py" \
    "${host_bin}/bc"
cp "${source_initramfs}" "${initramfs}"
cp "${source_root_report}" "${root_report}"
cp "${source_system_report}" "${system_report}"
python3 \
    "${repo_root}/tools/ipod6g_android/emulate_n25_clickwheel.py" \
    --output "${input_report}"

SOURCE_DATE_EPOCH="${source_date_epoch}" \
    make -C "${uboot_source}" O="${uboot_build}" apple_n25_ramdiag_defconfig
SOURCE_DATE_EPOCH="${source_date_epoch}" \
    make -C "${uboot_source}" O="${uboot_build}" \
    CROSS_COMPILE=arm-none-eabi- \
    KCFLAGS="-ffile-prefix-map=${uboot_source}=freemyipod-u-boot" \
    KAFLAGS="-ffile-prefix-map=${uboot_source}=freemyipod-u-boot" \
    -j"${jobs}" u-boot.bin

SOURCE_DATE_EPOCH="${source_date_epoch}" \
    KBUILD_BUILD_TIMESTAMP="${build_timestamp}" \
    KBUILD_BUILD_USER=rockpod KBUILD_BUILD_HOST=reproducible \
    KBUILD_BUILD_VERSION=1 PATH="${host_bin}:/usr/bin:/bin" \
    make -C "${linux_source}" O="${linux_build}" ARCH=arm \
    apple_n25_eclair_native_defconfig
SOURCE_DATE_EPOCH="${source_date_epoch}" \
    KBUILD_BUILD_TIMESTAMP="${build_timestamp}" \
    KBUILD_BUILD_USER=rockpod KBUILD_BUILD_HOST=reproducible \
    KBUILD_BUILD_VERSION=1 PATH="${host_bin}:/usr/bin:/bin" \
    make -C "${linux_source}" O="${linux_build}" ARCH=arm \
    CROSS_COMPILE=arm-none-eabi- -j"${jobs}" \
    zImage samsung/s5l8702-n25-eclair-native.dtb

mkdir -p "${rockbox_bootloader_build}"
if [[ ! -f "${rockbox_bootloader_build}/Makefile" ]]; then
    (
        cd "${rockbox_bootloader_build}"
        "${repo_root}/tools/configure" --target=ipod6g --type=b --no-ccache
    )
fi
make -C "${rockbox_bootloader_build}" -j"${jobs}"
cp "${rockbox_bootloader_build}/bootloader-ipod6g.ipod" \
    "${rockbox_bootloader}"
"${wind3x_bin}" makedfu --kind n3g \
    "${rockbox_bootloader_build}/bootloader.bin" "${rockbox_bootloader_dfu}"

sed \
    -e "s|@KERNEL@|${linux_build}/arch/arm/boot/zImage|g" \
    -e "s|@INITRAMFS@|${initramfs}|g" \
    -e "s|@DTB@|${linux_build}/arch/arm/boot/dts/samsung/s5l8702-n25-eclair-native.dtb|g" \
    "${repo_root}/tools/ipod6g_android/eclair/n25-eclair-native.its.in" > "${its}"

SOURCE_DATE_EPOCH="${source_date_epoch}" \
    "${uboot_build}/tools/mkimage" -f "${its}" "${fit}"
"${wind3x_bin}" makedfu --kind n3g "${uboot_build}/u-boot.bin" "${dfu}"
"${repo_root}/tools/scramble" -add=ip6g \
    "${linux_build}/arch/arm/boot/zImage" "${kernel_ipod}"
"${repo_root}/tools/scramble" -add=ip6g "${initramfs}" "${initramfs_ipod}"
"${repo_root}/tools/scramble" -add=ip6g \
    "${linux_build}/arch/arm/boot/dts/samsung/s5l8702-n25-eclair-native.dtb" \
    "${dtb_ipod}"

"${repo_root}/tools/ipod6g_android/qualify_n25_eclair_native.py" \
    --uboot-dir "${uboot_build}" \
    --linux-dir "${linux_build}" \
    --initramfs "${initramfs}" \
    --root-report "${root_report}" \
    --system-report "${system_report}" \
    --input-report "${input_report}" \
    --fit "${fit}" \
    --dfu "${dfu}" \
    --kernel-ipod "${kernel_ipod}" \
    --initramfs-ipod "${initramfs_ipod}" \
    --dtb-ipod "${dtb_ipod}" \
    --rockbox-bootloader "${rockbox_bootloader}" \
    --rockbox-bootloader-dfu "${rockbox_bootloader_dfu}" \
    --rockbox-bootloader-bin "${rockbox_bootloader_build}/bootloader.bin" \
    --rockbox-bootloader-elf "${rockbox_bootloader_build}/bootloader.elf" \
    > "${install_dir}/qualification.json"

(
    cd "${install_dir}"
    sha256sum n25-eclair-native-initramfs.cpio.gz \
        eclair-root-qualification.json eclair-system-emulation.json \
        n25-input-emulation.json \
        n25-eclair-kernel.ipod n25-eclair-initramfs.ipod \
        n25-eclair-dtb.ipod \
        n25-eclair-menu-play-bootloader.ipod \
        n25-eclair-menu-play-bootloader.dfu \
        n25-eclair-native.itb \
        n25-eclair-native-uboot.dfu qualification.json > SHA256SUMS
)

echo "Rockpod Eclair native RAM bundle installed at ${install_dir}"
echo "No hardware command was run; hardware actions remain disabled."
