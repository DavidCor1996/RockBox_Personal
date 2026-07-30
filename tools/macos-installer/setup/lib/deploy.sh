# Transactional firmware install, recovery bundle, and safe eject.
#
# This is the macOS port of tools/deploy_ipod6g_preserve_database.sh. The order
# of operations is deliberately identical: back up, stage, overlay without
# replacing .rockbox wholesale, restore the database byte-for-byte, drop the
# transient transaction files, install the recovery snapshot, then write and
# verify both firmware copies.

RP_BACKUP=""

# Create the host recovery bundle before the first byte is written to the iPod.
create_recovery_bundle()
{
    local root="${HOME}/Library/Application Support/RockPod/recovery"
    local name

    stage "recovery-backed-up"

    RP_BACKUP="${root}/${RP_RELEASE_ID}-$(date -u +%Y%m%dT%H%M%SZ)"
    if ! mkdir -p "${RP_BACKUP}/database" "${RP_BACKUP}/firmware" "${RP_BACKUP}/config"; then
        fail "RP-BACKUP-FAILED" \
            "Setup could not create a recovery backup on this Mac." \
            "Free up space in your home folder, then open Setup again."
    fi

    printf '%s' "${RP_DB_FILES}" | while IFS= read -r name; do
        [ -n "${name}" ] || continue
        cp -p "${RP_MOUNT}/.rockbox/${name}" "${RP_BACKUP}/database/${name}" || exit 1
        cmp -s "${RP_MOUNT}/.rockbox/${name}" "${RP_BACKUP}/database/${name}" || exit 1
    done || fail "RP-BACKUP-FAILED" \
        "Setup could not copy and verify the iPod's database files." \
        "Check the iPod for disk errors, then open Setup again."

    if [ -f "${RP_MOUNT}/rockbox.ipod" ]; then
        cp -p "${RP_MOUNT}/rockbox.ipod" "${RP_BACKUP}/firmware/rockbox.ipod.root"
    fi
    if [ -f "${RP_MOUNT}/.rockbox/rockbox.ipod" ]; then
        cp -p "${RP_MOUNT}/.rockbox/rockbox.ipod" "${RP_BACKUP}/firmware/rockbox.ipod.rbdir"
    fi
    if [ -f "${RP_MOUNT}/.rockbox/config.cfg" ]; then
        cp -p "${RP_MOUNT}/.rockbox/config.cfg" "${RP_BACKUP}/config/config.cfg"
    fi

    cp -p "${RP_PAYLOAD}/manifest.json" "${RP_BACKUP}/manifest.json" 2>/dev/null || true

    # File manifest with size and SHA-256, so a restore can be validated.
    ( cd "${RP_BACKUP}" && find . -type f ! -name MANIFEST.txt -print0 \
        | xargs -0 ${RP_BIN}/shasum -a 256 ) >"${RP_BACKUP}/MANIFEST.txt" 2>/dev/null || true

    log "recovery bundle: ${RP_BACKUP}"
    mark_safe_state "recovery bundle written to ${RP_BACKUP}"
}

# Overlay the release package. The mounted .rockbox directory is never removed.
stage_firmware()
{
    stage "firmware-staged"
    assert_identity_unchanged
    ui_notify "Installing Personal Rockbox files..."

    mark_device_changed
    if ! ${RP_BIN}/unzip -oq "${RP_PAYLOAD}/rockbox.zip" -d "${RP_MOUNT}"; then
        fail "RP-BOOT-WRITE" \
            "Setup could not finish copying the Rockbox files to the iPod." \
            "Keep the iPod connected and open Setup again to resume. The recovery bundle is at ${RP_BACKUP}."
    fi
}

# Put the preserved database back exactly as it was.
restore_database()
{
    local name

    [ "${RP_DB_PRESENT}" -eq 1 ] || return 0

    printf '%s' "${RP_DB_FILES}" | while IFS= read -r name; do
        [ -n "${name}" ] || continue
        cp -p "${RP_BACKUP}/database/${name}" "${RP_MOUNT}/.rockbox/${name}" || exit 1
    done || fail "RP-DB-INVALID" \
        "Setup could not restore the preserved database files." \
        "Keep the iPod connected and restore from the recovery bundle at ${RP_BACKUP}."

    # A scan/commit temp file is not part of the live multi-file database.
    # Carrying one across a host deployment makes firmware recovery treat a
    # clean restored database as an interrupted transaction.
    rm -f "${RP_MOUNT}/.rockbox/database_tmp.tcd" \
          "${RP_MOUNT}/.rockbox/database_commit.tcd" \
          "${RP_MOUNT}/.rockbox/database_hostcommit.tcd"
}

# Write rockbox.ipod to both boot locations and verify both against the release.
install_firmware_binaries()
{
    local want
    local got_root
    local got_rbdir

    stage "firmware-verified"
    want="$(sha256_of "${RP_PAYLOAD}/rockbox.ipod")"

    cp "${RP_PAYLOAD}/rockbox.ipod" "${RP_MOUNT}/rockbox.ipod"
    cp "${RP_PAYLOAD}/rockbox.ipod" "${RP_MOUNT}/.rockbox/rockbox.ipod"

    got_root="$(sha256_of "${RP_MOUNT}/rockbox.ipod")"
    got_rbdir="$(sha256_of "${RP_MOUNT}/.rockbox/rockbox.ipod")"

    if [ "${got_root}" != "${want}" ] || [ "${got_rbdir}" != "${want}" ]; then
        fail "RP-HASH-MISMATCH" \
            "One of the two firmware copies on the iPod does not match the release." \
            "Do not eject the iPod. Open Setup again to rewrite the firmware."
    fi

    log "firmware: /rockbox.ipod ${got_root}"
    log "firmware: /.rockbox/rockbox.ipod ${got_rbdir}"
    log "firmware: release ${want}"
}

# Flush writes and eject through macOS so the volume is left consistent.
finish_and_eject()
{
    stage "final-device-verified"
    assert_identity_unchanged
    sync

    stage "safely-ejected"
    ui_notify "Ejecting the iPod..."
    if ! ${RP_SBIN}/diskutil eject "/dev/${RP_DISK}" >/dev/null 2>&1; then
        ui_info "Personal Rockbox is installed, but the iPod is still mounted." \
            "The install finished and both firmware copies were verified. Quit any app that is using the iPod, then eject it from Finder before unplugging it."
        log "eject failed; install is complete but the volume is still mounted"
        return 0
    fi
    log "iPod ejected"
}
