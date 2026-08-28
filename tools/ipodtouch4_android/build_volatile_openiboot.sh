#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 PREPARED_OPENIBOOT OUTPUT_DIR" >&2
    exit 2
fi

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source_dir=$(CDPATH= cd -- "$1" && pwd)
output_dir=$2
cross_prefix=${CROSS:-arm-elf-eabi-}
scons_python=${SCONS_PYTHON:-python3}
commit=866562fdb1cfd019bcd77885c80fbf0af65d5c15

test -f "$source_dir/.rockpod-source-lock.json"
command -v "${cross_prefix}gcc" >/dev/null
"$scons_python" -c 'import SCons' >/dev/null

(
    cd "$source_dir"
    OPENIBOOT_SOURCE_COMMIT=$commit CROSS=$cross_prefix \
        "$scons_python" -m SCons ipt_4g_volatile_openiboot
)

mkdir -p "$output_dir"
cp "$source_dir/ipt_4g_volatile_openiboot" "$output_dir/openiboot-n81-volatile.elf"
"$script_dir/flatten_openiboot.py" \
    "$output_dir/openiboot-n81-volatile.elf" \
    "$output_dir/openiboot-n81-volatile.bin"

"$script_dir/qualify_volatile_openiboot.py" \
    "$source_dir" \
    "$output_dir/openiboot-n81-volatile.elf" \
    "$output_dir/openiboot-n81-volatile.bin" \
    --cross-prefix "$cross_prefix" \
    --output "$output_dir/qualification.json"

sha256sum \
    "$output_dir/openiboot-n81-volatile.elf" \
    "$output_dir/openiboot-n81-volatile.bin" \
    "$output_dir/qualification.json"
