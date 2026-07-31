# Optional Rockbox bootloader install, matching the workflows described in
# docs/rockpod-easy-installer-spec.md (see "Bootloader Workflows").
#
# The bootloader helper binaries (mks5lboot for Classic DFU, ipodpatcher for
# Video boot-partition writes) cannot be cross-compiled for macOS from this
# repository's Linux build host: there is no macOS SDK/cross toolchain here.
# Both tools only need IOKit and CoreFoundation on Apple platforms (see
# utils/mks5lboot/Makefile and utils/ipodpatcher/Makefile), so instead their
# plain C sources are bundled and compiled on the Mac itself with the Xcode
# Command Line Tools the first time a bootloader install is requested. This
# has not been exercised on real macOS/hardware from this build environment;
# see docs/rockpod-macos-installer.md for what that implies for testing.
#
# Firmware is always installed and verified before a first bootloader
# install, so the bootloader never points at absent firmware, matching the
# specification. main.sh enforces this ordering regardless of which order the
# options were presented in.
#
# `--single` (mks5lboot) must never be passed. There is no code path here that
# constructs it; tools/macos_installer_gate.sh greps this file to enforce that.

RP_TOOLS_DIR="${HOME}/Library/Application Support/RockPod/tools"
RP_MKS5LBOOT_BIN=""
RP_IPODPATCHER_BIN=""

# Compile one bundled helper on the Mac if it is not already built. Building
# into Application Support (not inside the .app) means a later run can reuse
# it without needing write access inside a bundle under /Applications.
#
# The bundled Makefiles derive TARGET_DIR/OBJDIR from the current directory
# and use them unquoted in make rules (see utils/libtools.make). Both
# "RockPod Setup.app" and "Application Support" contain a space, which GNU
# Make's word-splitting cannot survive there, so the source is copied into a
# space-free temporary directory and built there instead of in place.
build_bootloader_tool()
{
    local name="$1"
    local src="${RP_PAYLOAD}/tool-src/${name}"
    local bin="${RP_TOOLS_DIR}/${name}"
    local build_root

    if [ -x "${bin}" ]; then
        echo "${bin}"
        return 0
    fi

    if [ ! -d "${src}" ]; then
        fail "RP-TOOLCHAIN-MISSING" \
            "This build of RockPod Setup does not include the ${name} source." \
            "Rebuild RockPod Setup with tools/build_macos_installer.sh from a checkout that has utils/${name}."
    fi

    if ! command -v cc >/dev/null 2>&1 && ! command -v clang >/dev/null 2>&1; then
        fail "RP-TOOLCHAIN-MISSING" \
            "Setup needs a compiler to build the bootloader helper (${name})." \
            "Run 'xcode-select --install' in Terminal to install the free Xcode Command Line Tools, then open Setup again."
    fi

    mkdir -p "${RP_TOOLS_DIR}"
    ui_notify "Preparing the bootloader helper (${name})..."

    build_root="$(mktemp -d "${TMPDIR:-/tmp}/rockpod-buildtools.XXXXXX")" || \
        fail "RP-TOOLCHAIN-BUILD" \
            "Setup could not create a temporary build directory." \
            "Free up space in /tmp, then open Setup again."
    mkdir -p "${build_root}/${name}"
    cp -R "${src}/." "${build_root}/${name}/"
    cp "${RP_PAYLOAD}/tool-src/libtools.make" "${build_root}/libtools.make"

    if ! ( cd "${build_root}/${name}" && make APPVERSION="${RP_SETUP_VERSION}" \
            >"${RP_WORK}/${name}-build.log" 2>&1 ); then
        cp "${RP_WORK}/${name}-build.log" "${RP_WORK}/${name}-build.log.kept" 2>/dev/null || true
        rm -rf "${build_root}"
        fail "RP-TOOLCHAIN-BUILD" \
            "Setup could not build the bootloader helper (${name})." \
            "Save the diagnostic report; the build log is at ${RP_WORK}/${name}-build.log."
    fi

    if [ ! -x "${build_root}/${name}/${name}" ]; then
        rm -rf "${build_root}"
        fail "RP-TOOLCHAIN-BUILD" \
            "The ${name} build finished but did not produce a runnable tool." \
            "Save the diagnostic report and open an issue with the build log."
    fi

    cp "${build_root}/${name}/${name}" "${bin}"
    chmod 0755 "${bin}"
    rm -rf "${build_root}"
    log "built bootloader helper: ${name}"
    echo "${bin}"
}

ensure_bootloader_tools()
{
    case "${RP_TARGET}" in
        ipod6g)
            RP_MKS5LBOOT_BIN="$(build_bootloader_tool mks5lboot)"
            ;;
        ipodvideo)
            RP_IPODPATCHER_BIN="$(build_bootloader_tool ipodpatcher)"
            ;;
    esac
}

bootloader_asset_available()
{
    [ -s "${RP_PAYLOAD}/bootloader-${RP_TARGET}.ipod" ]
}

# --- Classic 6G/7G: DFU dual-boot install via mks5lboot ---------------------

classic_dfu_instructions()
{
    printf '%s' "Setup will now put your iPod Classic into DFU mode.

1. Safely disconnect the music volume when prompted.
2. Press and hold Select and Menu together.
3. Keep holding through the Apple logo and the reset; release only once the
   screen goes black and stays black. Setup is scanning for the device and
   will continue automatically once it is detected.

If nothing happens after about 30 seconds, release the buttons, reconnect the
iPod normally, and try again."
}

# Poll --dfuscan until exactly one supported DFU device answers, or time out.
# RP_DFU_MAX_TRIES/RP_DFU_POLL_INTERVAL are only overridden by
# tools/macos_installer_gate.sh, to test the timeout path without waiting a
# full minute on every gate run.
wait_for_classic_dfu()
{
    local tries=0
    local max_tries="${RP_DFU_MAX_TRIES:-60}"
    local interval="${RP_DFU_POLL_INTERVAL:-1}"
    local out

    ui_notify "Waiting for the iPod to enter DFU mode..."
    while [ "${tries}" -lt "${max_tries}" ]; do
        out="$("${RP_MKS5LBOOT_BIN}" --dfuscan 2>&1)"
        echo "${out}" >>"${RP_WORK}/dfuscan.log"
        case "${out}" in
            *"1 device"*)
                log "DFU device detected after ${tries}s"
                return 0
                ;;
        esac
        tries=$((tries + 1))
        sleep "${interval}"
    done
    return 1
}

install_bootloader_classic()
{
    stage "bootloader-waiting"
    ensure_bootloader_tools
    bootloader_asset_available || fail "RP-STOCK-MISSING" \
        "Setup was asked to install the bootloader, but no bootloader file was bundled in this build." \
        "Rebuild RockPod Setup with tools/fetch_stock_rockbox_assets.sh run first, or skip this step."

    if ! ui_confirm "Install the Rockbox bootloader" \
        "$(classic_dfu_instructions)" "Ready"; then
        log "bootloader install cancelled before DFU"
        return 1
    fi

    ${RP_SBIN}/diskutil unmount "/dev/${RP_PART}" >/dev/null 2>&1 || true

    if ! wait_for_classic_dfu; then
        fail "RP-DFU-NOT-FOUND" \
            "The iPod did not appear in DFU mode." \
            "Repeat the guided button sequence: hold Select and Menu together through the reset, then open Setup again."
    fi

    stage "bootloader-writing"
    ui_notify "Installing the Rockbox bootloader..."
    mark_device_changed

    # Deliberately never passes --single: a single-boot install would remove
    # the ability to start Apple's firmware, which the specification forbids
    # this installer from ever doing.
    if ! "${RP_MKS5LBOOT_BIN}" --bl-inst "${RP_PAYLOAD}/bootloader-${RP_TARGET}.ipod" \
            >"${RP_WORK}/bl-install.log" 2>&1; then
        fail "RP-BOOT-WRITE" \
            "The bootloader helper reported a failure while installing the bootloader." \
            "Keep the iPod connected. The log is at ${RP_WORK}/bl-install.log."
    fi

    stage "bootloader-verified"
    ui_info "Bootloader installed" \
        "The Rockbox bootloader was installed. The iPod will restart on its own; reconnect it once it reappears in Finder."

    sleep "${RP_DFU_POLL_INTERVAL:-5}"
    discover_ipod
    mark_safe_state "Rockbox bootloader installed and iPod re-identified"
}

# --- Video 5G/5.5G: boot-partition install via ipodpatcher ------------------

install_bootloader_video()
{
    local device="/dev/${RP_DISK}"
    local backup="${RP_BACKUP}/firmware/boot-partition-backup.bin"
    local reread="${RP_WORK}/boot-partition-reread.bin"

    stage "bootloader-waiting"
    ensure_bootloader_tools
    bootloader_asset_available || fail "RP-STOCK-MISSING" \
        "Setup was asked to install the bootloader, but no bootloader file was bundled in this build." \
        "Rebuild RockPod Setup with tools/fetch_stock_rockbox_assets.sh run first, or skip this step."

    if ! ui_confirm "Install the Rockbox bootloader" \
        "Setup will back up the current boot partition, then add the Rockbox bootloader alongside Apple's firmware. Do not disconnect the iPod during this step." \
        "Install"; then
        log "bootloader install cancelled before boot-partition write"
        return 1
    fi

    ${RP_SBIN}/diskutil unmountDisk "${device}" >/dev/null 2>&1 || true

    stage "bootloader-writing"
    assert_identity_unchanged

    # The backup must be read, hashed, and confirmed readable before a single
    # byte of the boot partition is written.
    if ! "${RP_IPODPATCHER_BIN}" "${device}" --read-partition "${backup}" \
            >"${RP_WORK}/boot-read.log" 2>&1 || [ ! -s "${backup}" ]; then
        fail "RP-BOOT-WRITE" \
            "Setup could not read the current boot partition before writing to it." \
            "Reconnect the iPod and open Setup again. No bootloader change was made."
    fi
    log "boot partition backup: ${backup} ($(sha256_of "${backup}"))"

    mark_device_changed
    if ! "${RP_IPODPATCHER_BIN}" "${device}" --add-bootloader \
            "${RP_PAYLOAD}/bootloader-${RP_TARGET}.ipod" \
            >"${RP_WORK}/boot-write.log" 2>&1; then
        fail "RP-BOOT-WRITE" \
            "Setup could not write the bootloader to the boot partition." \
            "Do not disconnect the iPod. The pre-write backup is at ${backup}; open Setup again to retry."
    fi

    stage "bootloader-verified"
    if ! "${RP_IPODPATCHER_BIN}" "${device}" --read-partition "${reread}" \
            >"${RP_WORK}/boot-reread.log" 2>&1 || [ ! -s "${reread}" ]; then
        fail "RP-BOOT-WRITE" \
            "The bootloader was written but Setup could not re-read the boot partition to verify it." \
            "Keep the iPod connected. The pre-write backup is at ${backup}."
    fi
    log "boot partition re-read after write: $(sha256_of "${reread}")"

    ${RP_SBIN}/diskutil mountDisk "${device}" >/dev/null 2>&1 || true
    discover_ipod
    mark_safe_state "Rockbox bootloader installed; pre-write backup at ${backup}"
}

install_bootloader()
{
    case "${RP_TARGET}" in
        ipod6g)    install_bootloader_classic ;;
        ipodvideo) install_bootloader_video ;;
        *)
            fail "RP-PKG-TARGET" \
                "Setup does not know a bootloader install method for ${RP_TARGET}." \
                "Skip the bootloader step for this build."
            ;;
    esac
}
