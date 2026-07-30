#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
uboot_source="${1:-}"
linux_source="${2:-}"
eclair_root_bundle="${3:-}"
install_dir="${4:-${repo_root}/rockpod/bin/ipod6g-android/eclair-native}"
build_root="${IPOD6G_ANDROID_BUILD_ROOT:-${repo_root}/tools/ipod6g_android/out}/eclair-native"
jobs="${JOBS:-$(nproc)}"
unicorn_python="${UNICORN_PYTHON:-python3}"
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
    echo "usage: $0 UBOOT_SOURCE LINUX_SOURCE ECLAIR_ROOT_BUNDLE [INSTALL_DIR]" >&2
    echo "builds and qualifies RAM-only artifacts; it never accesses an iPod" >&2
    exit 2
}

fatal()
{
    echo "fatal: $*" >&2
    exit 1
}

[[ -n "${uboot_source}" && -n "${linux_source}" && \
   -n "${eclair_root_bundle}" ]] || usage
uboot_source="$(realpath "${uboot_source}")"
linux_source="$(realpath "${linux_source}")"
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
boot_chord_report="${install_dir}/n25-boot-chord-emulation.json"
volatile_force_report="${install_dir}/n25-forced-volatile-boot-emulation.json"
select_right_chain_report="${install_dir}/n25-select-right-chain-emulation.json"
fit="${install_dir}/n25-eclair-native.itb"
dfu="${install_dir}/n25-eclair-native-uboot.dfu"
kernel_ipod="${install_dir}/n25-eclair-kernel.ipod"
initramfs_ipod="${install_dir}/n25-eclair-initramfs.ipod"
dtb_ipod="${install_dir}/n25-eclair-dtb.ipod"
rockbox_bootloader="${install_dir}/n25-eclair-select-right-bootloader.ipod"
rockbox_bootloader_dfu="${install_dir}/n25-eclair-select-right-bootloader.dfu"
rockbox_volatile_test_dfu="${install_dir}/n25-eclair-select-right-forced-volatile-test.dfu"
rockbox_nor_installer="${install_dir}/n25-eclair-select-right-nor-installer.dfu"
rockbox_nor_uninstaller="${install_dir}/n25-eclair-select-right-nor-uninstaller.dfu"
its="${build_root}/n25-eclair-native.its"
uboot_build="${build_root}/uboot"
linux_build="${build_root}/linux"
host_bin="${build_root}/host-bin"
rockbox_bootloader_build="${build_root}/rockbox-bootloader"
rockbox_volatile_test_build="${build_root}/rockbox-volatile-test"

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

wrapped_body_size()
{
    local size
    size="$(stat -c %s "$1")"
    echo $(( (size + 3) & ~3 ))
}

kernel_size="$(wrapped_body_size \
    "${linux_build}/arch/arm/boot/zImage")"
initramfs_size="$(wrapped_body_size "${initramfs}")"
dtb_size="$(wrapped_body_size \
    "${linux_build}/arch/arm/boot/dts/samsung/s5l8702-n25-eclair-native.dtb")"
extra_defines="-DBOOTLOADER -ffunction-sections -fdata-sections"
extra_defines+=" -DN25_ANDROID_KERNEL_SIZE=${kernel_size}"
extra_defines+=" -DN25_ANDROID_INITRD_SIZE=${initramfs_size}"
extra_defines+=" -DN25_ANDROID_DTB_SIZE=${dtb_size}"

mkdir -p "${rockbox_bootloader_build}"
if [[ ! -f "${rockbox_bootloader_build}/Makefile" ]]; then
    (
        cd "${rockbox_bootloader_build}"
        "${repo_root}/tools/configure" --target=ipod6g --type=b --no-ccache
    )
fi
SOURCE_DATE_EPOCH="${source_date_epoch}" \
    make -C "${rockbox_bootloader_build}" -j"${jobs}" \
    EXTRA_DEFINES="${extra_defines}"
cp "${rockbox_bootloader_build}/bootloader-ipod6g.ipod" \
    "${rockbox_bootloader}"
python3 "${repo_root}/tools/ipod6g_android/make_s5l8702_img1.py" \
    "${rockbox_bootloader_build}/bootloader.bin" "${rockbox_bootloader_dfu}"
"${repo_root}/utils/mks5lboot/mks5lboot" --mkdfu-inst \
    "${rockbox_bootloader}" "${rockbox_nor_installer}"
"${repo_root}/utils/mks5lboot/mks5lboot" --mkdfu-uninst \
    ipod6g "${rockbox_nor_uninstaller}"
python3 "${repo_root}/tools/ipod6g_android/emulate_rockbox_boot_chords.py" \
    --bootloader-bin "${rockbox_bootloader_build}/bootloader.bin" \
    --bootloader-elf "${rockbox_bootloader_build}/bootloader.elf" \
    --output "${boot_chord_report}"

mkdir -p "${rockbox_volatile_test_build}"
if [[ ! -f "${rockbox_volatile_test_build}/Makefile" ]]; then
    (
        cd "${rockbox_volatile_test_build}"
        "${repo_root}/tools/configure" --target=ipod6g --type=b --no-ccache
    )
fi
volatile_extra_defines="${extra_defines} -DN25_ANDROID_FORCE_VOLATILE_TEST"
SOURCE_DATE_EPOCH="${source_date_epoch}" \
    make -C "${rockbox_volatile_test_build}" -j"${jobs}" \
    EXTRA_DEFINES="${volatile_extra_defines}"
python3 "${repo_root}/tools/ipod6g_android/make_s5l8702_img1.py" \
    "${rockbox_volatile_test_build}/bootloader.bin" \
    "${rockbox_volatile_test_dfu}"
python3 "${repo_root}/tools/ipod6g_android/emulate_rockbox_boot_chords.py" \
    --bootloader-bin "${rockbox_volatile_test_build}/bootloader.bin" \
    --bootloader-elf "${rockbox_volatile_test_build}/bootloader.elf" \
    --require-forced-android \
    --output "${volatile_force_report}"

"${unicorn_python}" -c "import unicorn" >/dev/null 2>&1 || \
    fatal "UNICORN_PYTHON must provide the pinned Unicorn ARM emulator"
"${unicorn_python}" \
    "${repo_root}/tools/ipod6g_android/emulate_n25_arm_head.py" \
    --image "${linux_build}/arch/arm/boot/zImage" \
    --dtb \
    "${linux_build}/arch/arm/boot/dts/samsung/s5l8702-n25-eclair-native.dtb" \
    --system-map "${linux_build}/System.map" \
    --initramfs "${initramfs}" \
    --loader-bin "${rockbox_bootloader_build}/bootloader.bin" \
    --loader-elf "${rockbox_bootloader_build}/bootloader.elf" \
    --model-n25-timer --verify-n25-irq \
    --stop-at s5l_lcd_probe --max-instructions 250000000 \
    --json-output "${select_right_chain_report}"

sed \
    -e "s|@KERNEL@|${linux_build}/arch/arm/boot/zImage|g" \
    -e "s|@INITRAMFS@|${initramfs}|g" \
    -e "s|@DTB@|${linux_build}/arch/arm/boot/dts/samsung/s5l8702-n25-eclair-native.dtb|g" \
    "${repo_root}/tools/ipod6g_android/eclair/n25-eclair-native.its.in" > "${its}"

SOURCE_DATE_EPOCH="${source_date_epoch}" \
    "${uboot_build}/tools/mkimage" -f "${its}" "${fit}"
python3 "${repo_root}/tools/ipod6g_android/make_s5l8702_img1.py" \
    "${uboot_build}/u-boot.bin" "${dfu}"
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
    --boot-chord-report "${boot_chord_report}" \
    --volatile-force-report "${volatile_force_report}" \
    --select-right-chain-report "${select_right_chain_report}" \
    --fit "${fit}" \
    --dfu "${dfu}" \
    --kernel-ipod "${kernel_ipod}" \
    --initramfs-ipod "${initramfs_ipod}" \
    --dtb-ipod "${dtb_ipod}" \
    --rockbox-bootloader "${rockbox_bootloader}" \
    --rockbox-bootloader-dfu "${rockbox_bootloader_dfu}" \
    --rockbox-volatile-test-dfu "${rockbox_volatile_test_dfu}" \
    --rockbox-volatile-test-bin "${rockbox_volatile_test_build}/bootloader.bin" \
    --rockbox-volatile-test-elf "${rockbox_volatile_test_build}/bootloader.elf" \
    --rockbox-nor-installer "${rockbox_nor_installer}" \
    --rockbox-nor-uninstaller "${rockbox_nor_uninstaller}" \
    --rockbox-bootloader-bin "${rockbox_bootloader_build}/bootloader.bin" \
    --rockbox-bootloader-elf "${rockbox_bootloader_build}/bootloader.elf" \
    --mks5lboot "${repo_root}/utils/mks5lboot/mks5lboot" \
    > "${install_dir}/qualification.json"

(
    cd "${install_dir}"
    sha256sum n25-eclair-native-initramfs.cpio.gz \
        eclair-root-qualification.json eclair-system-emulation.json \
        n25-input-emulation.json n25-boot-chord-emulation.json \
        n25-forced-volatile-boot-emulation.json \
        n25-select-right-chain-emulation.json \
        n25-eclair-kernel.ipod n25-eclair-initramfs.ipod \
        n25-eclair-dtb.ipod \
        n25-eclair-select-right-bootloader.ipod \
        n25-eclair-select-right-bootloader.dfu \
        n25-eclair-select-right-forced-volatile-test.dfu \
        n25-eclair-select-right-nor-installer.dfu \
        n25-eclair-select-right-nor-uninstaller.dfu \
        n25-eclair-native.itb \
        n25-eclair-native-uboot.dfu qualification.json > SHA256SUMS
)

echo "Rockpod Eclair native RAM bundle installed at ${install_dir}"
echo "No hardware command was run; hardware actions remain disabled."
