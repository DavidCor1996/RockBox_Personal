# Rockbox tagcache guard, ported from tools/deploy_ipod6g_preserve_database.sh.
#
# The Linux deployment script reaches into rockpod/.venv for its deep parser.
# That interpreter does not exist on a user's Mac, so the guard is split in two:
#
#   - a structural check written in shell, which always runs;
#   - a deep indexed-path check in tagcache_check.py, which runs whenever a
#     usable Python 3 is already present on the Mac.
#
# Python is never installed, and macOS's /usr/bin/python3 stub is deliberately
# not probed, because touching it can pop the Command Line Tools installer.

RP_DB_PRESENT=0
RP_DB_FILES=""
RP_DB_TRANSIENT=""
RP_DB_DEEP_CHECK="not run"

RP_TAGCACHE_MAGIC=1413628944   # 0x54434810
RP_ROW_SIZE=96                 # (TAG_COUNT + 1) * 4
RP_STRING_TAG_FILES="database_0.tcd database_1.tcd database_2.tcd database_3.tcd \
database_4.tcd database_5.tcd database_6.tcd database_7.tcd database_8.tcd \
database_12.tcd"

file_size()
{
    ${RP_BIN}/stat -f %z "$1" 2>/dev/null || echo 0
}

# read_u32 <file> <offset> <le|be>
read_u32()
{
    local hex

    hex="$(${RP_BIN}/od -An -tx1 -N4 -j"$2" "$1" 2>/dev/null | tr -d ' \n')"
    [ "${#hex}" -eq 8 ] || return 1
    if [ "$3" = "le" ]; then
        printf '%d\n' "0x${hex:6:2}${hex:4:2}${hex:2:2}${hex:0:2}"
    else
        printf '%d\n' "0x${hex}"
    fi
}

find_python3()
{
    local candidate

    for candidate in \
        /opt/homebrew/bin/python3 \
        /usr/local/bin/python3 \
        /Library/Frameworks/Python.framework/Versions/Current/bin/python3 \
        /Library/Developer/CommandLineTools/usr/bin/python3 \
        /Applications/Xcode.app/Contents/Developer/usr/bin/python3
    do
        if [ -x "${candidate}" ]; then
            echo "${candidate}"
            return 0
        fi
    done
    return 1
}

# Structural validation of the multi-file tagcache. Mirrors the header rules
# enforced by apps/tagcache.c and rockpod/services/rockbox_tagcache.py.
verify_database_structure()
{
    local base="${RP_MOUNT}/.rockbox"
    local master="${base}/database_idx.tcd"
    local endian
    local magic
    local entry_count
    local dirty
    local size
    local name
    local path
    local tag_magic
    local tag_datasize

    if [ ! -s "${master}" ]; then
        fail "RP-DB-INVALID" \
            "The Rockbox database file database_idx.tcd is missing or empty." \
            "Open the RockPod database repair workflow before updating this iPod. Setup will not overwrite an installation whose database cannot be read."
    fi

    magic="$(read_u32 "${master}" 0 le)" || magic=""
    if [ "${magic}" = "${RP_TAGCACHE_MAGIC}" ]; then
        endian="le"
    else
        magic="$(read_u32 "${master}" 0 be)" || magic=""
        if [ "${magic}" = "${RP_TAGCACHE_MAGIC}" ]; then
            endian="be"
        else
            fail "RP-DB-INVALID" \
                "database_idx.tcd does not have a Rockbox database header." \
                "Open the RockPod database repair workflow before updating this iPod."
        fi
    fi

    entry_count="$(read_u32 "${master}" 8 "${endian}")"
    dirty="$(read_u32 "${master}" 20 "${endian}")"
    size="$(file_size "${master}")"

    if [ "${dirty}" != "0" ]; then
        fail "RP-DB-INVALID" \
            "The Rockbox database is marked dirty, which means the last scan did not finish." \
            "Let the iPod finish updating its database, or run the RockPod database repair workflow, then open Setup again."
    fi
    if [ "${entry_count}" -le 0 ] 2>/dev/null; then
        fail "RP-DB-INVALID" \
            "The Rockbox database reports no entries." \
            "Open the RockPod database repair workflow before updating this iPod."
    fi
    if [ "${size}" -lt $((24 + entry_count * RP_ROW_SIZE)) ] 2>/dev/null; then
        fail "RP-DB-INVALID" \
            "database_idx.tcd is shorter than its own entry count." \
            "Open the RockPod database repair workflow before updating this iPod."
    fi

    for name in ${RP_STRING_TAG_FILES}; do
        path="${base}/${name}"
        if [ ! -s "${path}" ]; then
            fail "RP-DB-INVALID" \
                "The Rockbox database file ${name} is missing or empty." \
                "Open the RockPod database repair workflow before updating this iPod."
        fi
        tag_magic="$(read_u32 "${path}" 0 "${endian}")" || tag_magic=""
        tag_datasize="$(read_u32 "${path}" 4 "${endian}")" || tag_datasize=0
        if [ "${tag_magic}" != "${RP_TAGCACHE_MAGIC}" ]; then
            fail "RP-DB-INVALID" \
                "${name} does not have a Rockbox database header." \
                "Open the RockPod database repair workflow before updating this iPod."
        fi
        if [ "$(file_size "${path}")" -lt $((12 + tag_datasize)) ] 2>/dev/null; then
            fail "RP-DB-INVALID" \
                "${name} is shorter than its own header claims." \
                "Open the RockPod database repair workflow before updating this iPod."
        fi
    done

    log "database guard: structural check passed, ${entry_count} entries, ${endian} byte order"
}

# Deep check: parse the database and confirm indexed media still exists.
verify_database_paths()
{
    local python
    local output

    python="$(find_python3)" || {
        RP_DB_DEEP_CHECK="skipped, no Python 3 on this Mac"
        log "database guard: ${RP_DB_DEEP_CHECK}"
        return 0
    }

    if output="$("${python}" "${RP_SETUP_DIR}/tagcache_check.py" "${RP_MOUNT}" 2>&1)"; then
        RP_DB_DEEP_CHECK="${output}"
        log "database guard: ${output}"
        return 0
    fi

    fail "RP-DB-INVALID" \
        "The Rockbox database on this iPod failed validation: ${output}" \
        "Open the RockPod database repair workflow. Setup will not update an installation whose database is inconsistent."
}

# Collect the live tagcache files and quarantine transient transaction files.
collect_database_files()
{
    local path
    local name

    RP_DB_FILES=""
    RP_DB_TRANSIENT=""

    for path in "${RP_MOUNT}"/.rockbox/database*.tcd "${RP_MOUNT}"/.rockbox/tagcache*.tcd; do
        [ -e "${path}" ] || continue
        name="$(basename "${path}")"
        case "${name}" in
            database_tmp.tcd|database_commit.tcd|database_hostcommit.tcd)
                RP_DB_TRANSIENT="${RP_DB_TRANSIENT}${name}
"
                ;;
            *)
                RP_DB_FILES="${RP_DB_FILES}${name}
"
                ;;
        esac
    done
}

database_file_count()
{
    printf '%s' "${RP_DB_FILES}" | grep -c . 2>/dev/null || echo 0
}

# Full guard for an existing installation. Any doubt is a hard stop.
guard_existing_database()
{
    stage "database-ready"

    verify_database_structure
    verify_database_paths
    collect_database_files

    if [ "$(database_file_count)" -eq 0 ]; then
        fail "RP-DB-INVALID" \
            "Setup found a Rockbox installation but no tagcache files to preserve." \
            "Open the RockPod database repair workflow before updating this iPod."
    fi

    RP_DB_PRESENT=1
}

# Confirm the preserved files came back byte-for-byte after the package deploy.
verify_database_unchanged()
{
    local name

    printf '%s' "${RP_DB_FILES}" | while IFS= read -r name; do
        [ -n "${name}" ] || continue
        if [ ! -f "${RP_MOUNT}/.rockbox/${name}" ]; then
            echo "missing ${name}"
            return 0
        fi
        if ! cmp -s "${RP_BACKUP}/database/${name}" "${RP_MOUNT}/.rockbox/${name}"; then
            echo "changed ${name}"
            return 0
        fi
    done >"${RP_WORK}/db-diff.txt"

    if [ -s "${RP_WORK}/db-diff.txt" ]; then
        fail "RP-DB-INVALID" \
            "A Rockbox database file did not survive the update ($(cat "${RP_WORK}/db-diff.txt"))." \
            "Keep the iPod connected and restore the database from the recovery bundle at ${RP_BACKUP}."
    fi

    log "database guard: all $(database_file_count) tagcache files are byte-identical"
}

# Install the verified last-known-good snapshot the firmware can fall back to.
install_recovery_snapshot()
{
    local snapshot="${RP_MOUNT}/.rockbox/tagcache_backup"
    local staging="${RP_MOUNT}/.rockbox/.tagcache_backup.new"
    local name

    rm -rf "${staging}"
    mkdir -p "${staging}"

    printf '%s' "${RP_DB_FILES}" | while IFS= read -r name; do
        [ -n "${name}" ] || continue
        cp -p "${RP_BACKUP}/database/${name}" "${staging}/${name}"
        cmp -s "${RP_BACKUP}/database/${name}" "${staging}/${name}" || echo "bad ${name}"
    done >"${RP_WORK}/snapshot-diff.txt"

    if [ -s "${RP_WORK}/snapshot-diff.txt" ]; then
        fail "RP-DB-INVALID" \
            "Setup could not write a verified database recovery snapshot to the iPod." \
            "Check the iPod for disk errors, then open Setup again."
    fi

    rm -rf "${snapshot}"
    mv "${staging}" "${snapshot}"
    log "database guard: installed verified last-known-good recovery snapshot"
}

# tagcache_autoupdate must stay on so the iPod refreshes its own index.
enable_tagcache_autoupdate()
{
    local config="${RP_MOUNT}/.rockbox/config.cfg"
    local tmp="${RP_WORK}/config.cfg"

    if [ ! -f "${config}" ]; then
        printf 'tagcache_autoupdate: on\n' >"${config}"
        return 0
    fi

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
    ' "${config}" >"${tmp}"
    cp "${tmp}" "${config}"
    log "database guard: tagcache_autoupdate left enabled"
}
