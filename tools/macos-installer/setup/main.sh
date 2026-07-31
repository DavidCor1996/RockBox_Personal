#!/bin/bash
#
# RockPod Setup for macOS - offline installer for Personal Rockbox.
#
# Runs entirely on tools that ship with macOS: bash 3.2, diskutil, plutil,
# unzip, shasum, and osascript. Nothing is downloaded and nothing is installed
# on the Mac itself.

set -o pipefail

RP_SETUP_DIR="$(cd "$(dirname "$0")" && pwd)"
RP_RESOURCES="$(cd "${RP_SETUP_DIR}/.." && pwd)"
RP_PAYLOAD="${RP_RESOURCES}/payload"
RP_WORK=""
RP_JOURNAL_FILE=""

# Release facts written by tools/build_macos_installer.sh.
. "${RP_PAYLOAD}/release.env"

. "${RP_SETUP_DIR}/lib/platform.sh"
. "${RP_SETUP_DIR}/lib/log.sh"
. "${RP_SETUP_DIR}/lib/ui.sh"
. "${RP_SETUP_DIR}/lib/discovery.sh"
. "${RP_SETUP_DIR}/lib/package.sh"
. "${RP_SETUP_DIR}/lib/database.sh"
. "${RP_SETUP_DIR}/lib/deploy.sh"
. "${RP_SETUP_DIR}/lib/stock.sh"
. "${RP_SETUP_DIR}/lib/bootloader.sh"

RP_WANT_BOOTLOADER=0
RP_WANT_STOCK=0

cleanup()
{
    if [ -n "${RP_WORK}" ] && [ -d "${RP_WORK}" ]; then
        rm -rf "${RP_WORK}"
    fi
}

welcome()
{
    ui_confirm "Set up Personal Rockbox on your iPod" \
"This installer carries Personal Rockbox ${RP_RELEASE_ID} for the ${RP_DEVICE_LABEL}.

It will:
  - check the connected iPod and the files it already has
  - optionally install the Rockbox bootloader and a stock Rockbox release,
    if this iPod does not already have them
  - back up your Rockbox database and settings to this Mac
  - install Personal Rockbox without touching your music
  - verify both copies of the firmware and eject the iPod safely

Your music, playlists, themes, saved games, and Rockbox database are preserved.
Connect exactly one iPod before continuing." \
        "Set Up iPod"
}

# Both are optional and default to skipped, since most iPods reaching this
# installer already have a bootloader and some Rockbox installation. Neither
# question depends on the device being connected yet; the user is asked to
# judge this from what they already know about their own iPod.
ask_bootloader_option()
{
    if ! bootloader_asset_available; then
        RP_WANT_BOOTLOADER=0
        return 0
    fi

    if ui_confirm "Install the Rockbox bootloader?" \
"If this iPod does not already dual-boot into Rockbox, Setup can install the
Rockbox bootloader for the ${RP_DEVICE_LABEL}.

Skip this if the bootloader is already installed. Setup only ever installs
the dual-boot bootloader; it will never remove Apple's firmware or install a
single-boot bootloader." \
        "Install Bootloader"; then
        RP_WANT_BOOTLOADER=1
    else
        RP_WANT_BOOTLOADER=0
    fi
}

ask_stock_option()
{
    if ! stock_rockbox_available; then
        RP_WANT_STOCK=0
        return 0
    fi

    if ui_confirm "Install stock Rockbox first?" \
"If this iPod has never run Rockbox, Setup can install the current stock
Rockbox ${RP_STOCK_VERSION} release first, so you can confirm plain Rockbox
starts before Personal Rockbox replaces it.

Skip this if this iPod already has Rockbox or Personal Rockbox installed." \
        "Install Stock Rockbox First"; then
        RP_WANT_STOCK=1
    else
        RP_WANT_STOCK=0
    fi
}

review()
{
    local installed_version
    local installed_target
    local mode
    local stock_line
    local bootloader_line

    installed_version="$(installed_rockbox_version)"
    installed_target="$(installed_rockbox_target)"

    if [ "${RP_DB_PRESENT}" -eq 1 ]; then
        mode="Update an existing installation"
    else
        mode="First-time installation"
    fi

    if [ "${RP_WANT_STOCK}" -eq 1 ]; then
        stock_line="Install stock Rockbox ${RP_STOCK_VERSION}, then Personal Rockbox on top"
    else
        stock_line="Skipped (not selected)"
    fi

    if [ "${RP_WANT_BOOTLOADER}" -eq 1 ]; then
        bootloader_line="Install after Personal Rockbox is installed and verified"
    else
        bootloader_line="Skipped (not selected)"
    fi

    ui_confirm "Review before installing" \
"iPod            ${RP_MEDIA_NAME} ($(human_size "${RP_DISK_SIZE}"))
Volume          ${RP_VOLUME_NAME:-untitled} at ${RP_MOUNT}
Disk            /dev/${RP_PART} on /dev/${RP_DISK}
Filesystem      ${RP_FSNAME:-${RP_FSTYPE}}, $(human_size "${RP_FREE_SPACE}") free
Installed now   ${installed_version:-none} (${installed_target:-no Rockbox})
Installing      ${RP_RELEASE_ID} for ${RP_TARGET}
Action          ${mode}
Stock Rockbox   ${stock_line}
Bootloader      ${bootloader_line}
Database        ${RP_DB_DEEP_CHECK}
Backup          ~/Library/Application Support/RockPod/recovery

Setup will replace the files inside .rockbox that belong to Rockbox, and write
rockbox.ipod to both /rockbox.ipod and /.rockbox/rockbox.ipod. It does not
delete your Music folder, your playlists, or your database.

Regardless of the order shown above, Setup always installs and verifies
firmware before it ever writes a bootloader, so a bootloader can never point
at absent firmware." \
        "Install"
}

ready()
{
    local bootloader_note

    if [ "${RP_WANT_BOOTLOADER}" -eq 1 ]; then
        bootloader_note="The Rockbox bootloader was installed this run. Unplug the iPod and it
will boot Personal Rockbox. Hold MENU while it starts to load Apple's
firmware instead."
    else
        bootloader_note="If this iPod already has the Rockbox bootloader, unplug it and it will
boot Personal Rockbox. Hold MENU while it starts to load Apple's firmware
instead.

If Rockbox has never been installed on this iPod, it still needs the
Rockbox bootloader. Run Setup again and choose to install the bootloader,
or follow the bootloader guide in the RockPod documentation."
    fi

    ui_info "Your iPod is ready" \
"Personal Rockbox ${RP_RELEASE_ID} is installed and verified.

Verified:
  - /rockbox.ipod matches the release
  - /.rockbox/rockbox.ipod matches the release
  - database files preserved: ${RP_DB_DEEP_CHECK}
  - the iPod was ejected safely

${bootloader_note}

A full log is in ~/Library/Logs/RockPod."
}

main()
{
    local installed_target
    local had_existing=0

    RP_WORK="$(mktemp -d /tmp/rockpod-setup.XXXXXX)" || exit 1
    trap cleanup EXIT INT TERM HUP

    mkdir -p "${HOME}/Library/Application Support/RockPod" 2>/dev/null || true
    RP_JOURNAL_FILE="${HOME}/Library/Application Support/RockPod/setup-journal.log"

    log_init
    stage "idle"

    if ! welcome; then
        log "cancelled at welcome"
        exit 0
    fi

    ask_bootloader_option
    ask_stock_option

    verify_payload_hashes
    verify_package_target
    verify_package_entries

    discover_ipod
    preflight

    # Captured before the optional stock Rockbox step, so a fresh device that
    # gets stock Rockbox laid down first is still treated as a first-time
    # Rockpod install rather than one with a database to preserve: a stock
    # package carries no tagcache for guard_existing_database to find.
    if [ -f "${RP_MOUNT}/.rockbox/rockbox-info.txt" ]; then
        had_existing=1
        installed_target="$(installed_rockbox_target)"
        if [ -n "${installed_target}" ] && [ "${installed_target}" != "${RP_TARGET}" ]; then
            fail "RP-PKG-TARGET" \
                "This iPod has Rockbox for '${installed_target}' installed, but this installer is for ${RP_TARGET}." \
                "Use the RockPod Setup build that matches this iPod."
        fi
    fi

    if [ "${RP_WANT_STOCK}" -eq 1 ]; then
        install_stock_rockbox
    fi

    if [ "${had_existing}" -eq 1 ]; then
        guard_existing_database
    else
        stage "database-ready"
        RP_DB_PRESENT=0
        RP_DB_DEEP_CHECK="first-time install, the iPod will build its database on first boot"
        log "no existing Rockbox installation; nothing to preserve"
    fi

    if ! review; then
        log "cancelled at review"
        exit 0
    fi

    create_recovery_bundle
    stage_firmware
    restore_database

    if [ "${RP_DB_PRESENT}" -eq 1 ]; then
        install_recovery_snapshot
    fi
    enable_tagcache_autoupdate

    if [ "${RP_DB_PRESENT}" -eq 1 ]; then
        verify_database_unchanged
        verify_database_structure
        verify_database_paths
    fi

    install_firmware_binaries
    mark_safe_state "firmware installed and verified on the iPod"

    # Always after firmware is installed and verified, regardless of the order
    # the options were presented in, so a bootloader never points at absent
    # firmware.
    if [ "${RP_WANT_BOOTLOADER}" -eq 1 ]; then
        if ! install_bootloader; then
            RP_WANT_BOOTLOADER=0
            log "bootloader install skipped or cancelled"
        fi
    fi

    finish_and_eject

    stage "complete"
    ready
}

main "$@"
