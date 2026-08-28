#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 PREPARED_KERNEL OUTPUT_DIR" >&2
    exit 2
fi

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source_dir=$(CDPATH= cd -- "$1" && pwd)
output_dir=$2
build_dir="$output_dir/build"
cross_prefix=${CROSS:-arm-elf-eabi-}
jobs=${JOBS:-4}

test -f "$source_dir/.rockpod-kernel-source-lock.json"
command -v "${cross_prefix}gcc" >/dev/null
if [ -e "$output_dir" ]; then
    echo "output directory already exists: $output_dir" >&2
    exit 2
fi
mkdir -p "$build_dir"

"$script_dir/configure_volatile_kernel.sh" "$source_dir" "$build_dir"

KBUILD_BUILD_TIMESTAMP="2012-05-26 11:23:47 +0000" \
KBUILD_BUILD_USER=rockpod \
KBUILD_BUILD_HOST=n81-volatile \
SOURCE_DATE_EPOCH=1338031427 \
make -C "$source_dir" O="$(CDPATH= cd -- "$build_dir" && pwd)" \
    ARCH=arm CROSS_COMPILE="$cross_prefix" KCFLAGS=-fgnu89-inline \
    -j "$jobs" zImage vmlinux

cp "$build_dir/arch/arm/boot/zImage" "$output_dir/zImage-n81-volatile"
cp "$build_dir/vmlinux" "$output_dir/vmlinux-n81-volatile"
cp "$build_dir/System.map" "$output_dir/System.map-n81-volatile"
cp "$build_dir/.config" "$output_dir/config-n81-volatile"

"$script_dir/qualify_volatile_kernel.py" \
    "$source_dir" \
    "$output_dir/zImage-n81-volatile" \
    "$output_dir/vmlinux-n81-volatile" \
    "$output_dir/config-n81-volatile" \
    --cross-prefix "$cross_prefix" \
    --output "$output_dir/kernel-qualification.json"

sha256sum \
    "$output_dir/zImage-n81-volatile" \
    "$output_dir/vmlinux-n81-volatile" \
    "$output_dir/config-n81-volatile" \
    "$output_dir/kernel-qualification.json"
