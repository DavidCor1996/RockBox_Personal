#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 PREPARED_KERNEL BUILD_DIR" >&2
    exit 2
fi

source_dir=$(CDPATH= cd -- "$1" && pwd)
build_dir=$2
config_tool="$source_dir/scripts/config"

test -f "$source_dir/.rockpod-kernel-source-lock.json"
test -x "$config_tool"
mkdir -p "$build_dir"
cp "$source_dir/arch/arm/configs/ipt4_defconfig" "$build_dir/.config"

"$config_tool" --file "$build_dir/.config" \
    --disable SWAP \
    --disable MODULES \
    --disable BLOCK \
    --disable BLK_DEV \
    --disable BLK_DEV_APPLE \
    --disable BLK_DEV_APPLE_VSVFL \
    --disable BLK_DEV_APPLE_LEGACY_VFL \
    --disable BLK_DEV_APPLE_YAFTL \
    --disable BLK_DEV_APPLE_LEGACY_FTL \
    --disable BLK_DEV_H2FMI \
    --disable BLK_DEV_S5L8900 \
    --disable MTD \
    --disable MMC \
    --disable SCSI \
    --disable ATA \
    --disable IDE \
    --disable MD \
    --disable EXT2_FS \
    --disable EXT3_FS \
    --disable EXT4_FS \
    --disable FAT_FS \
    --disable HFS_FS \
    --disable HFSPLUS_FS \
    --disable DEVMEM \
    --disable DEVKMEM \
    --disable OABI_COMPAT \
    --disable LOCALVERSION_AUTO \
    --enable BLK_DEV_INITRD \
    --enable RD_GZIP \
    --enable DEVTMPFS \
    --enable DEVTMPFS_MOUNT \
    --enable TMPFS \
    --enable STAGING \
    --enable ANDROID \
    --enable ANDROID_BINDER_IPC \
    --enable ANDROID_LOGGER \
    --enable ANDROID_LOW_MEMORY_KILLER \
    --set-val PANIC_TIMEOUT 5 \
    --set-str LOCALVERSION "-rockpod-n81-volatile"

make -C "$source_dir" O="$(CDPATH= cd -- "$build_dir" && pwd)" \
    ARCH=arm oldnoconfig
