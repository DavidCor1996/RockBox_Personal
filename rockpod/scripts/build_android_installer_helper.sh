#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
rockpod_dir="$(cd -- "${script_dir}/.." && pwd)"
crate_dir="${rockpod_dir}/native/ipod6g_android_installer"
destination_dir="${rockpod_dir}/bin/linux-x86_64"

cargo build --locked --release --manifest-path "${crate_dir}/Cargo.toml"
mkdir -p "${destination_dir}"
install -m 0755 \
    "${crate_dir}/target/release/ipod6g-android-installer" \
    "${destination_dir}/ipod6g-android-installer"

"${destination_dir}/ipod6g-android-installer" hello
