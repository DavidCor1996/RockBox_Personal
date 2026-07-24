#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
linux_source="${1:-}"
wind3x_bin="${2:-}"
install_dir="${3:-${repo_root}/rockpod/bin/ipod6g-android/visible-probe}"
build_root="${IPOD6G_ANDROID_BUILD_ROOT:-${repo_root}/tools/ipod6g_android/out/visible-probe}"
jobs="${JOBS:-$(nproc)}"
source_date_epoch="${SOURCE_DATE_EPOCH:-1704067200}"
build_timestamp="$(date --utc --date="@${source_date_epoch}" \
    "+%a %b %e %T UTC %Y")"
unicorn_python="${N25_UNICORN_PYTHON:-python3}"

linux_revision="a1bf13a446201cdd97e56815cedc6b1da14f1313"
linux_overlay="${repo_root}/tools/ipod6g_android/linux-overlay"
linux_patches=(
    "${repo_root}/tools/ipod6g_android/patches/linux-s5l8702-armv5-selection.patch"
    "${repo_root}/tools/ipod6g_android/patches/linux-s5l8702-n25-early-lcd-breadcrumb.patch"
    "${repo_root}/tools/ipod6g_android/patches/linux-s5l8702-n25-vic.patch"
    "${repo_root}/tools/ipod6g_android/patches/linux-s5l8702-n25-timer.patch"
    "${repo_root}/tools/ipod6g_android/patches/linux-s5l8702-n25-timer-lcd-breadcrumbs.patch"
    "${repo_root}/tools/ipod6g_android/patches/linux-s5l8702-n25-input-reset.patch"
    "${repo_root}/tools/ipod6g_android/patches/linux-s5l8702-n25-lcd-handoff.patch"
    "${repo_root}/tools/ipod6g_android/patches/linux-s5l8702-n25-visible-handoff.patch"
)

usage()
{
    echo "usage: $0 LINUX_SOURCE WIND3X_BIN [INSTALL_DIR]" >&2
    echo "builds and qualifies files only; it never accesses an iPod" >&2
    exit 2
}

fatal()
{
    echo "fatal: $*" >&2
    exit 1
}

[[ -n "${linux_source}" && -n "${wind3x_bin}" ]] || usage
linux_source="$(realpath "${linux_source}")"
wind3x_bin="$(realpath "${wind3x_bin}")"
install_dir="$(realpath -m "${install_dir}")"
build_root="$(realpath -m "${build_root}")"
linux_build="${build_root}/linux"
rockbox_build="${build_root}/rockbox-loader"
host_bin="${build_root}/host-bin"
initramfs="${build_root}/n25-visible-initramfs.cpio.gz"
dtb="${linux_build}/arch/arm/boot/dts/samsung/s5l8702-n25-ramdiag.dtb"
zimage="${linux_build}/arch/arm/boot/zImage"
kernel_image="${linux_build}/arch/arm/boot/Image"

kernel_ipod="${install_dir}/n25-visible-kernel.ipod"
initramfs_ipod="${install_dir}/n25-visible-initramfs.ipod"
dtb_ipod="${install_dir}/n25-visible-dtb.ipod"
loader_ipod="${install_dir}/n25-visible-rockbox-loader.ipod"
loader_dfu="${install_dir}/n25-visible-rockbox-loader.dfu"
lcd_emulations=(
    "${install_dir}/n25-lcd-strap0-emulation.json"
    "${install_dir}/n25-lcd-strap1-emulation.json"
    "${install_dir}/n25-lcd-strap2-emulation.json"
    "${install_dir}/n25-lcd-strap3-emulation.json"
)

[[ -x "${wind3x_bin}" ]] || fatal "wInd3x executable not found: ${wind3x_bin}"
"${unicorn_python}" -c 'import unicorn' >/dev/null 2>&1 || \
    fatal "${unicorn_python} cannot import the pinned Unicorn ARM emulator"
[[ "$(git -C "${linux_source}" rev-parse HEAD)" == "${linux_revision}" ]] || \
    fatal "Linux revision does not match the audited pin"

apply_patch_set_once()
{
    local source_tree="$1"
    local patch_file
    local -a patch_files reverted
    local i
    local dirty=false
    shift
    patch_files=("$@")

    if ! git -C "${source_tree}" diff --quiet || \
       ! git -C "${source_tree}" diff --cached --quiet || \
       [[ -n "$(git -C "${source_tree}" ls-files --others --exclude-standard)" ]]; then
        dirty=true
    fi

    if [[ "${dirty}" == true ]]; then
        # Later patches can extend lines introduced by earlier patches, so an
        # independent reverse-check gives a false failure.  Verify the whole
        # stack transactionally in the only valid order, then restore it.
        reverted=()
        for ((i=${#patch_files[@]} - 1; i >= 0; i--)); do
            patch_file="${patch_files[i]}"
            if ! git -C "${source_tree}" apply --reverse --check \
                    "${patch_file}" >/dev/null 2>&1; then
                for ((i=${#reverted[@]} - 1; i >= 0; i--)); do
                    git -C "${source_tree}" apply "${reverted[i]}"
                done
                fatal "dirty Linux tree is not the complete audited patch set"
            fi
            git -C "${source_tree}" apply --reverse "${patch_file}"
            reverted+=("${patch_file}")
        done
        if ! git -C "${source_tree}" diff --quiet || \
           ! git -C "${source_tree}" diff --cached --quiet; then
            for ((i=${#reverted[@]} - 1; i >= 0; i--)); do
                git -C "${source_tree}" apply "${reverted[i]}"
            done
            fatal "dirty Linux tree contains changes outside the audited patch set"
        fi
        for ((i=${#reverted[@]} - 1; i >= 0; i--)); do
            git -C "${source_tree}" apply "${reverted[i]}"
        done
        return
    fi

    # Apply in order because the visible handoff patch extends the N25 LCD
    # driver introduced by the preceding base handoff patch.
    for patch_file in "${patch_files[@]}"; do
        git -C "${source_tree}" apply --check "${patch_file}"
        git -C "${source_tree}" apply "${patch_file}"
    done
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

apply_patch_set_once "${linux_source}" "${linux_patches[@]}"
install_overlay_file \
    "${linux_overlay}/arch/arm/configs/apple_n25_ramdiag_defconfig" \
    "${linux_source}/arch/arm/configs/apple_n25_ramdiag_defconfig"
install_overlay_file \
    "${linux_overlay}/arch/arm/boot/dts/samsung/s5l8702-n25-ramdiag.dts" \
    "${linux_source}/arch/arm/boot/dts/samsung/s5l8702-n25-ramdiag.dts"

mkdir -p "${install_dir}" "${build_root}" "${host_bin}"
ln -sf "${repo_root}/tools/ipod6g_android/timeconst_bc_shim.py" \
    "${host_bin}/bc"

SOURCE_DATE_EPOCH="${source_date_epoch}" \
    "${repo_root}/tools/ipod6g_android/build_ramdiag_initramfs.sh" \
    "${initramfs}"
initramfs_size="$(stat -c %s "${initramfs}")"
[[ "${initramfs_size}" == 1280 ]] || \
    fatal "initramfs size ${initramfs_size} does not match the pinned DTB end"

SOURCE_DATE_EPOCH="${source_date_epoch}" \
    KBUILD_BUILD_TIMESTAMP="${build_timestamp}" \
    KBUILD_BUILD_USER=rockpod KBUILD_BUILD_HOST=reproducible \
    KBUILD_BUILD_VERSION=1 PATH="${host_bin}:/usr/bin:/bin" \
    make -C "${linux_source}" O="${linux_build}" ARCH=arm \
    apple_n25_ramdiag_defconfig
SOURCE_DATE_EPOCH="${source_date_epoch}" \
    KBUILD_BUILD_TIMESTAMP="${build_timestamp}" \
    KBUILD_BUILD_USER=rockpod KBUILD_BUILD_HOST=reproducible \
    KBUILD_BUILD_VERSION=1 PATH="${host_bin}:/usr/bin:/bin" \
    make -C "${linux_source}" O="${linux_build}" ARCH=arm \
    CROSS_COMPILE=arm-none-eabi- -j"${jobs}" \
    zImage samsung/s5l8702-n25-ramdiag.dtb

kernel_size="$(stat -c %s "${kernel_image}")"
dtb_size="$(stat -c %s "${dtb}")"
extra_defines="-DBOOTLOADER -ffunction-sections -fdata-sections"
extra_defines+=" -DN25_ANDROID_VISIBLE_DIAG_TEST"
extra_defines+=" -DN25_ANDROID_KERNEL_SIZE=${kernel_size}"
extra_defines+=" -DN25_ANDROID_INITRD_SIZE=${initramfs_size}"
extra_defines+=" -DN25_ANDROID_DTB_SIZE=${dtb_size}"

mkdir -p "${rockbox_build}"
if [[ ! -f "${rockbox_build}/Makefile" ]]; then
    (
        cd "${rockbox_build}"
        "${repo_root}/tools/configure" --target=ipod6g --type=b --no-ccache
    )
fi
SOURCE_DATE_EPOCH="${source_date_epoch}" \
    make -C "${rockbox_build}" -j"${jobs}" EXTRA_DEFINES="${extra_defines}"

for strap in 0 1 2 3; do
    "${unicorn_python}" \
        "${repo_root}/tools/ipod6g_android/emulate_n25_arm_head.py" \
        --image "${kernel_image}" \
        --dtb "${dtb}" \
        --system-map "${linux_build}/System.map" \
        --initramfs "${initramfs}" \
        --loader-bin "${rockbox_build}/bootloader.bin" \
        --loader-elf "${rockbox_build}/bootloader.elf" \
        --model-n25-timer --verify-n25-irq --verify-n25-lcd \
        --panel-strap "${strap}" \
        --stop-at s5l_lcd_n25_first_frame_complete \
        --max-instructions 60000000 \
        --json-output "${lcd_emulations[strap]}"
done

loader_size="$(stat -c %s "${rockbox_build}/bootloader.bin")"
(( loader_size <= 114688 )) || \
    fatal "Rockbox loader exceeds the conservative 112 KiB DFU body gate"

# Boot the linked Image directly at PHYS_OFFSET + TEXT_OFFSET.  This removes
# the zImage relocation/decompression stage from the first hardware probe.
"${repo_root}/tools/scramble" -add=ip6g "${kernel_image}" "${kernel_ipod}"
"${repo_root}/tools/scramble" -add=ip6g "${initramfs}" "${initramfs_ipod}"
"${repo_root}/tools/scramble" -add=ip6g "${dtb}" "${dtb_ipod}"
"${repo_root}/tools/scramble" -add=ip6g \
    "${rockbox_build}/bootloader.bin" "${loader_ipod}"
"${wind3x_bin}" makedfu --kind n3g \
    "${rockbox_build}/bootloader.bin" "${loader_dfu}"

"${repo_root}/tools/ipod6g_android/qualify_n25_visible_probe.py" \
    --linux-dir "${linux_build}" \
    --initramfs "${initramfs}" \
    --kernel-ipod "${kernel_ipod}" \
    --initramfs-ipod "${initramfs_ipod}" \
    --dtb-ipod "${dtb_ipod}" \
    --loader-bin "${rockbox_build}/bootloader.bin" \
    --loader-elf "${rockbox_build}/bootloader.elf" \
    --loader-ipod "${loader_ipod}" \
    --loader-dfu "${loader_dfu}" \
    --lcd-emulation "${lcd_emulations[0]}" \
    --lcd-emulation "${lcd_emulations[1]}" \
    --lcd-emulation "${lcd_emulations[2]}" \
    --lcd-emulation "${lcd_emulations[3]}" \
    > "${install_dir}/qualification.json"

(
    cd "${install_dir}"
    sha256sum n25-visible-kernel.ipod n25-visible-initramfs.ipod \
        n25-visible-dtb.ipod n25-visible-rockbox-loader.ipod \
        n25-visible-rockbox-loader.dfu n25-lcd-strap?-emulation.json \
        qualification.json > SHA256SUMS
)

echo "Visible RAM-only N25 probe installed at ${install_dir}"
echo "No iPod was accessed and no hardware command was run."
