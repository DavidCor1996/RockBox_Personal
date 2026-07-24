#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mount_path="${1:-}"
build_dir="${2:-${repo_root}/build-hw-ipod6g}"
firmware="${build_dir}/rockbox.ipod"
package="${build_dir}/rockbox.zip"
backup_dir=""
had_database=0
transaction_artifacts=()
mount_source=""
parent_disk=""
disk_size=0
disk_serial=""

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

verify_database_unchanged()
{
    local original
    local name

    for original in "${backup_dir}"/*.tcd; do
        name="$(basename "${original}")"
        if [ ! -f "${mount_path}/.rockbox/${name}" ]; then
            echo "database guard: ${name} disappeared during deploy" >&2
            exit 1
        fi
        if ! cmp -s "${original}" "${mount_path}/.rockbox/${name}"; then
            echo "database guard: ${name} changed during deploy" >&2
            exit 1
        fi
    done

    echo "database guard: all ${#database_files[@]} tagcache files are byte-identical"
}

install_recovery_snapshot()
{
    local snapshot="${mount_path}/.rockbox/tagcache_backup"
    local staging="${mount_path}/.rockbox/.tagcache_backup.new"
    local original
    local name

    rm -rf "${staging}"
    mkdir "${staging}"
    for original in "${backup_dir}"/*.tcd; do
        cp -a "${original}" "${staging}/"
    done
    for original in "${backup_dir}"/*.tcd; do
        name="$(basename "${original}")"
        if ! cmp -s "${original}" "${staging}/${name}"; then
            echo "database guard: recovery snapshot ${name} differs" >&2
            exit 1
        fi
    done

    rm -rf "${snapshot}"
    mv "${staging}" "${snapshot}"
    echo "database guard: installed verified last-known-good recovery snapshot"
}

if [ -z "${mount_path}" ]; then
    usage
fi
if [ ! -d "${mount_path}/.rockbox" ]; then
    echo "not a mounted Rockbox volume: ${mount_path}" >&2
    exit 1
fi

mount_source="$(findmnt -rn -o SOURCE --target "${mount_path}")"
parent_disk="$(lsblk -no PKNAME "${mount_source}")"
if [ -z "${mount_source}" ] || [ -z "${parent_disk}" ]; then
    echo "hardware guard: cannot identify the mounted block device" >&2
    exit 1
fi
disk_size="$(lsblk -bdno SIZE "/dev/${parent_disk}")"
disk_serial="$(lsblk -dno SERIAL "/dev/${parent_disk}")"
if [ "${disk_size}" -lt $((32 * 1024 * 1024 * 1024)) ]; then
    echo "hardware guard: refusing small device ${mount_source} (${disk_size} bytes, serial ${disk_serial})" >&2
    exit 1
fi
echo "hardware guard: target ${mount_source} on /dev/${parent_disk}, ${disk_size} bytes, serial ${disk_serial}"

if [ ! -s "${firmware}" ] || [ ! -s "${package}" ]; then
    echo "missing iPod 6G hardware build outputs in ${build_dir}" >&2
    exit 1
fi

unzip -tq "${package}" >/dev/null
backup_dir="$(mktemp -d)"
trap cleanup EXIT INT TERM HUP

shopt -s nullglob
database_files=()
for path in "${mount_path}"/.rockbox/database*.tcd \
            "${mount_path}"/.rockbox/tagcache*.tcd; do
    case "$(basename "${path}")" in
        database_tmp.tcd|database_commit.tcd|database_hostcommit.tcd)
            transaction_artifacts+=("${path}")
            ;;
        *)
            database_files+=("${path}")
            ;;
    esac
done
if [ ! -s "${mount_path}/.rockbox/database_idx.tcd" ]; then
    echo "database guard: refusing deploy without a readable database_idx.tcd" >&2
    exit 1
fi
if [ "${#database_files[@]}" -eq 0 ]; then
    echo "database guard: refusing deploy without tagcache files" >&2
    exit 1
fi
had_database=1
verify_database
for path in "${database_files[@]}"; do
    cp -a "${path}" "${backup_dir}/"
done
echo "database guard: backed up ${#database_files[@]} tagcache files"
if [ "${#transaction_artifacts[@]}" -gt 0 ]; then
    mkdir "${backup_dir}/transaction-artifacts"
    for path in "${transaction_artifacts[@]}"; do
        cp -a "${path}" "${backup_dir}/transaction-artifacts/"
    done
    echo "database guard: quarantined ${#transaction_artifacts[@]} transient transaction artifact(s)"
fi

unzip -oq "${package}" -d "${mount_path}"

if [ "${had_database}" -eq 1 ]; then
    for path in "${backup_dir}"/*.tcd; do
        cp -a "${path}" "${mount_path}/.rockbox/"
    done
fi

# A scan/commit temp file is not part of the live multi-file database. Never
# carry one across a host deployment: firmware recovery may otherwise treat a
# clean restored database as an interrupted transaction.
rm -f "${mount_path}/.rockbox/database_tmp.tcd" \
      "${mount_path}/.rockbox/database_commit.tcd" \
      "${mount_path}/.rockbox/database_hostcommit.tcd"

install_recovery_snapshot
enable_tagcache_autoupdate
verify_database_unchanged
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
