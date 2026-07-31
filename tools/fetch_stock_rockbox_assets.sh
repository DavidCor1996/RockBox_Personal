#!/usr/bin/env bash
#
# Fetch and hash-verify the upstream stock Rockbox bootloader and release
# package for a target, so the macOS offline installer can offer them as an
# optional prerequisite step before installing Personal Rockbox (RockPod).
#
# These are the same artifacts Rockbox Utility would install from
# download.rockbox.org (see utils/rbutilqt/rbutil.ini): the universal Rockbox
# bootloader for the target, and the current stock release build. Hashes are
# pinned here so a compromised or stale mirror cannot silently substitute a
# different file; this script only ever writes a file to the cache once its
# hash has been checked.
#
# usage: tools/fetch_stock_rockbox_assets.sh [--target ipod6g|ipodvideo|all]
#                                             [--cache-dir <dir>]

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
target="all"
cache_dir="${repo_root}/tools/.vendor-cache/rockbox-stock"

die()
{
    echo "fetch_stock_rockbox_assets: $*" >&2
    exit 1
}

while [ $# -gt 0 ]; do
    case "$1" in
        --target)    target="$2"; shift 2 ;;
        --cache-dir) cache_dir="$2"; shift 2 ;;
        -h|--help)
            sed -n '2,15p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *) die "unknown option $1" ;;
    esac
done

# name url sha256
manifest_ipod6g="
bootloader-ipod6g.ipod https://download.rockbox.org/bootloader/ipod/bootloader-ipod6g.ipod d5a18e56b46ec8a509471720ffe0900328f60ab3dd9d5d7d0c38fe74ff712176
rockbox-ipod6g-4.0.zip https://download.rockbox.org/release/4.0/rockbox-ipod6g-4.0.zip 974304b7d7fa9cd916ee15eaccb7e55707dc68c77aeb75286c4c1f62935c37ac
"
manifest_ipodvideo="
bootloader-ipodvideo.ipod https://download.rockbox.org/bootloader/ipod/bootloader-ipodvideo.ipod 19dfa0e930689f5afdeaae18f4c56b472cbed9c6c3a7039bb32b646d8040298f
rockbox-ipodvideo-4.0.zip https://download.rockbox.org/release/4.0/rockbox-ipodvideo-4.0.zip 010334b02a89f43cd64f069807cb52228c0cfd55a8ee084d6a05aab829f42732
"

fetch_one()
{
    local name="$1"
    local url="$2"
    local want="$3"
    local dest="${cache_dir}/${name}"
    local got

    mkdir -p "${cache_dir}"

    if [ -s "${dest}" ]; then
        got="$(sha256sum "${dest}" | awk '{ print $1 }')"
        if [ "${got}" = "${want}" ]; then
            echo "cached: ${name} ${got}"
            return 0
        fi
        echo "cached copy of ${name} failed verification; refetching" >&2
        rm -f "${dest}"
    fi

    command -v curl >/dev/null 2>&1 || die "curl is required to fetch ${name}"

    if ! curl -fsSL --retry 3 -o "${dest}.part" "${url}"; then
        rm -f "${dest}.part"
        die "download failed: ${url}"
    fi

    got="$(sha256sum "${dest}.part" | awk '{ print $1 }')"
    if [ "${got}" != "${want}" ]; then
        rm -f "${dest}.part"
        die "hash mismatch for ${name}: got ${got}, expected ${want}. Refusing to use it."
    fi

    mv "${dest}.part" "${dest}"
    echo "fetched: ${name} ${got}"
}

fetch_target()
{
    local t="$1"
    local manifest_var="manifest_${t}"
    local manifest="${!manifest_var:-}"
    local line

    [ -n "${manifest}" ] || die "no stock asset manifest for target ${t}"

    while IFS= read -r line; do
        [ -n "${line}" ] || continue
        set -- ${line}
        fetch_one "$1" "$2" "$3"
    done <<<"${manifest}"
}

case "${target}" in
    ipod6g|ipodvideo) fetch_target "${target}" ;;
    all)
        fetch_target ipod6g
        fetch_target ipodvideo
        ;;
    *) die "unsupported target ${target}" ;;
esac

echo "cache dir: ${cache_dir}"
