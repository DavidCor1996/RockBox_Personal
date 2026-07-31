# Optional upstream stock Rockbox install, staged before Rockpod itself.
#
# Some users want to confirm stock Rockbox boots on a given iPod before
# trusting a heavily customized personal build, or simply have not installed
# either yet. This mirrors what Rockbox Utility would fetch from
# download.rockbox.org (see utils/rbutilqt/rbutil.ini): the universal Rockbox
# bootloader for the target, and the current stock release package. Both are
# bundled at build time with pinned hashes (tools/fetch_stock_rockbox_assets.sh)
# rather than downloaded here, so "nothing is downloaded" during Setup still
# holds and every byte written to the iPod was hashed before Setup ran.
#
# Installing the stock package here, then letting the normal Rockpod flow in
# main.sh treat the result as an existing installation to update, is exactly
# the composition the specification's "Existing Personal Rockbox Installation"
# path already assumes: nothing here bypasses guard_existing_database.

RP_STOCK_ZIP=""
RP_STOCK_VERSION=""

# Populate RP_STOCK_* if the optional stock payload was bundled for this
# target, or return 1 if this build was made without it.
stock_rockbox_available()
{
    local dir="${RP_PAYLOAD}/stock"
    local zip="${dir}/rockbox-${RP_TARGET}.zip"
    local info="${dir}/rockbox-${RP_TARGET}-info.txt"

    [ -s "${zip}" ] || return 1
    RP_STOCK_ZIP="${zip}"
    RP_STOCK_VERSION="$(awk -F': *' '/^Version:/ { print $2; exit }' "${info}" 2>/dev/null)"
    [ -n "${RP_STOCK_VERSION}" ] || RP_STOCK_VERSION="unknown"
    return 0
}

# Install the bundled stock Rockbox package onto the mounted volume. Does not
# touch the boot partition; that is bootloader_install's job.
install_stock_rockbox()
{
    stage "stock-rockbox-staged"

    if [ ! -s "${RP_STOCK_ZIP}" ]; then
        fail "RP-STOCK-MISSING" \
            "Setup was asked to install stock Rockbox, but no stock package was bundled in this build." \
            "Rebuild RockPod Setup with tools/fetch_stock_rockbox_assets.sh run first, or skip this step."
    fi

    verify_zip_entries "${RP_STOCK_ZIP}" "stock Rockbox"

    ui_notify "Installing stock Rockbox ${RP_STOCK_VERSION}..."
    assert_identity_unchanged
    mark_device_changed

    if ! ${RP_BIN}/unzip -oq "${RP_STOCK_ZIP}" -d "${RP_MOUNT}"; then
        fail "RP-BOOT-WRITE" \
            "Setup could not finish copying the stock Rockbox files to the iPod." \
            "Keep the iPod connected and open Setup again to resume."
    fi

    log "stock Rockbox ${RP_STOCK_VERSION} installed for ${RP_TARGET}"
    mark_safe_state "stock Rockbox ${RP_STOCK_VERSION} installed"
}
