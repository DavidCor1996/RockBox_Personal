#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mount_path="${1:-}"
build_dir="${2:-${repo_root}/build-hw-ipod6g}"
firmware="${build_dir}/rockbox.ipod"
package="${build_dir}/rockbox.zip"
backup_dir=""
had_database=0

usage()
{
    echo "usage: $0 <mounted-ipod-path> [build-hw-ipod6g-dir]" >&2
    exit 2
}

cleanup()
{
    if [ -n "${backup_dir}" ] && [ -d "${backup_dir}" ]; then
        rm -rf "${backup_dir}"
    fi
}

enable_tagcache_autoupdate()
{
    local config_path="${mount_path}/.rockbox/config.cfg"
    local config_tmp

    config_tmp="$(mktemp "${mount_path}/.rockbox/.config.deploy.XXXXXX")"
    awk '
        BEGIN { updated = 0 }
        {
            line = $0
            sub(/\r$/, "", line)
            if (line ~ /^(tagcache_autoupdate|autoupdate):/) {
                if (!updated)
                    print "tagcache_autoupdate: on"
                updated = 1
                next
            }
            print line
        }
        END {
            if (!updated)
                print "tagcache_autoupdate: on"
        }
    ' "${config_path}" >"${config_tmp}"
    mv "${config_tmp}" "${config_path}"
}

verify_database()
{
    local python="${repo_root}/rockpod/.venv/bin/python"

    if [ "${had_database}" -eq 0 ]; then
        return
    fi

    if [ ! -s "${mount_path}/.rockbox/database_idx.tcd" ]; then
        echo "database guard: database_idx.tcd is missing or empty" >&2
        exit 1
    fi

    if [ -x "${python}" ]; then
        PYTHONPATH="${repo_root}/rockpod" "${python}" -c '
import os
import sys
from services.rockbox_tagcache import read_rockbox_tagcache_tracks

mount = sys.argv[1]
rows = read_rockbox_tagcache_tracks(mount)
missing = [
    row["device_path"]
    for row in rows
    if not os.path.isfile(os.path.join(mount, row["device_path"].lstrip("/")))
]
if not rows:
    raise SystemExit("database guard: parsed database contains no tracks")
if missing:
    raise SystemExit(
        "database guard: %d indexed paths are missing (first: %s)"
        % (len(missing), missing[0])
    )
print("database guard: validated %d tracks" % len(rows))
' "${mount_path}"
    fi
}

if [ -z "${mount_path}" ]; then
    usage
fi
if [ ! -d "${mount_path}/.rockbox" ]; then
    echo "not a mounted Rockbox volume: ${mount_path}" >&2
    exit 1
fi
if [ ! -s "${firmware}" ] || [ ! -s "${package}" ]; then
    echo "missing iPod 6G hardware build outputs in ${build_dir}" >&2
    exit 1
fi

unzip -tq "${package}" >/dev/null
backup_dir="$(mktemp -d)"
trap cleanup EXIT INT TERM HUP

shopt -s nullglob
database_files=(
    "${mount_path}"/.rockbox/database*.tcd
    "${mount_path}"/.rockbox/tagcache*.tcd
)
if [ -s "${mount_path}/.rockbox/database_idx.tcd" ]; then
    had_database=1
    for path in "${database_files[@]}"; do
        cp -a "${path}" "${backup_dir}/"
    done
    echo "database guard: backed up ${#database_files[@]} tagcache files"
fi

unzip -oq "${package}" -d "${mount_path}"

if [ "${had_database}" -eq 1 ]; then
    for path in "${backup_dir}"/*.tcd; do
        cp -a "${path}" "${mount_path}/.rockbox/"
    done
fi

enable_tagcache_autoupdate
cp "${firmware}" "${mount_path}/rockbox.ipod"
cp "${firmware}" "${mount_path}/.rockbox/rockbox.ipod"

cmp -s "${firmware}" "${mount_path}/rockbox.ipod"
cmp -s "${firmware}" "${mount_path}/.rockbox/rockbox.ipod"
verify_database

sha256sum "${firmware}" \
    "${mount_path}/rockbox.ipod" \
    "${mount_path}/.rockbox/rockbox.ipod"
sync
echo "deploy complete; database preserved and firmware checksums verified"
