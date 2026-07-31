#!/usr/bin/env bash
#
# Build the offline macOS installer for a Personal Rockbox hardware build.
#
# The RockPod Easy Installer specification allows firmware to ship inside the
# application bundle when building "an explicitly versioned offline installer",
# which is exactly what this produces: one self-contained "RockPod Setup.app"
# carrying the firmware that was just built.
#
# The bundle is assembled here on Linux, so it is deliberately free of anything
# that needs a macOS toolchain. It is not code-signed or notarized; see
# docs/rockpod-macos-installer.md for the deviations from the specification.
#
# usage: tools/build_macos_installer.sh [options]
#   --build-dir <dir>   hardware build directory (default build-hw-<target>)
#   --target <name>     ipod6g (default) or ipodvideo
#   --output-dir <dir>  where to write the installer (default the build dir)
#   --desktop-copy      also replace the copy on the build host's Desktop
#   --desktop <dir>     Desktop directory (default $HOME/Desktop)
#   --skip-stock-assets skip the optional stock bootloader/Rockbox bundle
#                       (the installer then offers Rockpod only, matching the
#                       previous behavior)

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
src_dir="${repo_root}/tools/macos-installer"

target="ipod6g"
build_dir=""
output_dir=""
desktop_copy=0
desktop_dir="${HOME}/Desktop"
skip_stock_assets=0
work_dir=""

die()
{
    echo "build_macos_installer: $*" >&2
    exit 1
}

cleanup()
{
    if [ -n "${work_dir}" ] && [ -d "${work_dir}" ]; then
        rm -rf "${work_dir}"
    fi
}

while [ $# -gt 0 ]; do
    case "$1" in
        --build-dir)  build_dir="$2"; shift 2 ;;
        --target)     target="$2"; shift 2 ;;
        --output-dir) output_dir="$2"; shift 2 ;;
        --desktop)    desktop_dir="$2"; shift 2 ;;
        --desktop-copy) desktop_copy=1; shift ;;
        --skip-stock-assets) skip_stock_assets=1; shift ;;
        -h|--help)
            sed -n '2,24p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *) die "unknown option $1" ;;
    esac
done

case "${target}" in
    ipod6g)     device_label="iPod Classic 6G/7G" ;;
    ipodvideo)  device_label="iPod Video 5G/5.5G" ;;
    *) die "unsupported target ${target}" ;;
esac

[ -n "${build_dir}" ] || build_dir="${repo_root}/build-hw-${target}"
[ -n "${output_dir}" ] || output_dir="${build_dir}"

firmware="${build_dir}/rockbox.ipod"
package="${build_dir}/rockbox.zip"
info="${build_dir}/rockbox-info.txt"

for path in "${firmware}" "${package}" "${info}"; do
    [ -s "${path}" ] || die "missing hardware build output: ${path}"
done

for tool in zip unzip sha256sum awk; do
    command -v "${tool}" >/dev/null 2>&1 || die "required tool not found: ${tool}"
done

build_target="$(awk -F': *' '/^Target:/ { print $2; exit }' "${info}")"
[ "${build_target}" = "${target}" ] ||
    die "build in ${build_dir} is for '${build_target}', not ${target}"

release_id="$(awk -F': *' '/^Version:/ { print $2; exit }' "${info}")"
[ -n "${release_id}" ] || die "cannot read the release version from ${info}"

setup_version="$(cd "${repo_root}" && sh tools/version.sh 2>/dev/null | tr -d '\n')"
setup_version="${setup_version#-}"
[ -n "${setup_version}" ] || setup_version="0"
# CFBundleShortVersionString must be numeric components only.
bundle_version="1.0.${setup_version//[^0-9]/}"
bundle_version="${bundle_version%.}"

# Space the installer needs on the iPod: the expanded package plus both copies
# of the firmware plus slack for FAT32 cluster overhead.
expanded="$(unzip -l "${package}" | awk 'END { print $1 }')"
firmware_size="$(stat -c %s "${firmware}")"
required_bytes=$((expanded + (2 * firmware_size) + (32 * 1024 * 1024)))

work_dir="$(mktemp -d)"
trap cleanup EXIT INT TERM HUP

app="${work_dir}/RockPod Setup.app"
contents="${app}/Contents"
resources="${contents}/Resources"
payload="${resources}/payload"

mkdir -p "${contents}/MacOS" "${resources}/setup" "${payload}"

sed -e "s/@RP_SETUP_VERSION@/${bundle_version}/g" \
    -e "s/@RP_TARGET@/${target}/g" \
    "${src_dir}/Info.plist.in" >"${contents}/Info.plist"
printf 'APPL????' >"${contents}/PkgInfo"

install -m 0755 "${src_dir}/launcher.sh" "${contents}/MacOS/RockPodSetup"
cp -R "${src_dir}/setup/." "${resources}/setup/"
# Never ship build leftovers from a developer running the checker locally.
find "${resources}/setup" \( -name '__pycache__' -o -name '*.pyc' \) \
    -exec rm -rf {} + 2>/dev/null || true
chmod 0755 "${resources}/setup/main.sh"
chmod 0644 "${resources}/setup/lib/"*.sh "${resources}/setup/tagcache_check.py"
cp "${src_dir}/NOTICE.txt" "${resources}/NOTICE.txt"
cp "${repo_root}/docs/COPYING" "${resources}/COPYING"

# Bootloader helper sources. Compiled on the Mac itself the first time a
# bootloader install is requested: both tools only need IOKit/CoreFoundation
# on Apple platforms (see utils/mks5lboot/Makefile and
# utils/ipodpatcher/Makefile), and there is no macOS cross toolchain on this
# Linux build host to produce a binary here.
tool_src="${payload}/tool-src"
mkdir -p "${tool_src}/mks5lboot" "${tool_src}/ipodpatcher"
cp "${repo_root}/utils/libtools.make" "${tool_src}/libtools.make"

for f in dualboot.c dualboot.h ipoddfu.c main.c Makefile mkdfu.c mks5lboot.h \
         nano3g_dualboot.c nano3g_dualboot.h; do
    cp "${repo_root}/utils/mks5lboot/${f}" "${tool_src}/mks5lboot/${f}"
done
for f in arc4.c arc4.h fat32format.c ipodio.h ipodio-posix.c ipodio-win32.c \
         ipodio-win32-scsi.c ipodpatcher.c ipodpatcher.h ipodpatcher-aupd.c \
         main.c parttypes.h Makefile; do
    cp "${repo_root}/utils/ipodpatcher/${f}" "${tool_src}/ipodpatcher/${f}"
done

cp "${package}" "${payload}/rockbox.zip"
cp "${firmware}" "${payload}/rockbox.ipod"
cp "${info}" "${payload}/rockbox-info.txt"

# Optional stock Rockbox bootloader and release, hash-pinned at fetch time.
# Missing them (no network, or --skip-stock-assets) only disables the
# optional bootloader/stock-Rockbox steps; Rockpod itself still installs.
bootloader_bundled=0
stock_bundled=0
if [ "${skip_stock_assets}" -eq 0 ]; then
    stock_cache="${repo_root}/tools/.vendor-cache/rockbox-stock"
    if "${repo_root}/tools/fetch_stock_rockbox_assets.sh" --target "${target}" \
            --cache-dir "${stock_cache}" >"${work_dir}/stock-fetch.log" 2>&1; then
        stock_release="$(ls "${stock_cache}/rockbox-${target}-"*.zip 2>/dev/null | head -1)"
        if [ -s "${stock_cache}/bootloader-${target}.ipod" ]; then
            cp "${stock_cache}/bootloader-${target}.ipod" \
                "${payload}/bootloader-${target}.ipod"
            bootloader_bundled=1
        fi
        if [ -n "${stock_release}" ] && [ -s "${stock_release}" ]; then
            mkdir -p "${payload}/stock"
            cp "${stock_release}" "${payload}/stock/rockbox-${target}.zip"
            unzip -p "${stock_release}" .rockbox/rockbox-info.txt \
                >"${payload}/stock/rockbox-${target}-info.txt"
            stock_bundled=1
        fi
    else
        echo "warning: could not fetch stock Rockbox assets ($(tail -1 "${work_dir}/stock-fetch.log" 2>/dev/null)); building without the optional bootloader/stock-Rockbox step" >&2
    fi
fi

cat >"${payload}/release.env" <<EOF
# Generated by tools/build_macos_installer.sh. Do not edit.
RP_RELEASE_ID="${release_id}"
RP_TARGET="${target}"
RP_DEVICE_LABEL="${device_label}"
RP_SETUP_VERSION="${bundle_version}"
RP_REQUIRED_BYTES=${required_bytes}
EOF

firmware_sha="$(sha256sum "${firmware}" | awk '{ print $1 }')"
package_sha="$(sha256sum "${package}" | awk '{ print $1 }')"

if [ "${bootloader_bundled}" -eq 1 ]; then
    bootloader_manifest_line="\"bootloader_${target}\""
    bootloader_method="mks5lboot-dual"
    [ "${target}" = "ipodvideo" ] && bootloader_method="ipodpatcher-add-bootloader"
else
    bootloader_manifest_line="null"
    bootloader_method="not-included"
fi
[ "${stock_bundled}" -eq 1 ] && stock_manifest_flag=true || stock_manifest_flag=false

cat >"${payload}/manifest.json" <<EOF
{
  "schema": 1,
  "channel": "local-offline",
  "release": "${release_id}",
  "published_at": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "setup_version": "${bundle_version}",
  "source_revision": "$(cd "${repo_root}" && git rev-parse HEAD 2>/dev/null || echo unknown)",
  "artifacts": {
    "rockbox_${target}": {
      "path": "rockbox.zip",
      "size": $(stat -c %s "${package}"),
      "sha256": "${package_sha}",
      "firmware_sha256": "${firmware_sha}"
    }
  },
  "compatibility": {
    "${target}": {
      "firmware": "rockbox_${target}",
      "bootloader": ${bootloader_manifest_line},
      "bootloader_method": "${bootloader_method}",
      "stock_rockbox_bundled": ${stock_manifest_flag}
    }
  }
}
EOF

( cd "${payload}" && sha256sum rockbox.zip rockbox.ipod rockbox-info.txt \
    release.env manifest.json >SHA256SUMS )
if [ "${bootloader_bundled}" -eq 1 ]; then
    ( cd "${payload}" && sha256sum "bootloader-${target}.ipod" >>SHA256SUMS )
fi
if [ "${stock_bundled}" -eq 1 ]; then
    ( cd "${payload}" && sha256sum "stock/rockbox-${target}.zip" \
        "stock/rockbox-${target}-info.txt" >>SHA256SUMS )
fi

installer_name="RockPod-Setup-${target}.zip"
staged="${work_dir}/${installer_name}"

( cd "${work_dir}" && zip -q -r -X "${staged}" "RockPod Setup.app" )

mkdir -p "${output_dir}"
mv -f "${staged}" "${output_dir}/${installer_name}"
( cd "${output_dir}" && sha256sum "${installer_name}" >"${installer_name}.sha256" )

echo "macOS installer: ${output_dir}/${installer_name}"
echo "  release ${release_id}, target ${target}, setup ${bundle_version}"
echo "  firmware sha256 ${firmware_sha}"
if [ "${bootloader_bundled}" -eq 1 ]; then
    echo "  bootloader: bundled (${bootloader_method})"
else
    echo "  bootloader: not bundled; the bootloader step will be unavailable in this build"
fi
if [ "${stock_bundled}" -eq 1 ]; then
    echo "  stock Rockbox: bundled"
else
    echo "  stock Rockbox: not bundled; the stock-Rockbox step will be unavailable in this build"
fi

if [ "${desktop_copy}" -eq 1 ]; then
    if [ -d "${desktop_dir}" ]; then
        # Stage on the same filesystem, then rename, so an interrupted copy can
        # never leave a truncated installer on the Desktop.
        cp "${output_dir}/${installer_name}" "${desktop_dir}/.${installer_name}.part"
        mv -f "${desktop_dir}/.${installer_name}.part" "${desktop_dir}/${installer_name}"
        echo "  copied to ${desktop_dir}/${installer_name}"
    else
        echo "  Desktop directory ${desktop_dir} not found; skipped the Desktop copy" >&2
    fi
fi
