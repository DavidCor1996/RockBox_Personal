# macOS iPod discovery and preflight.
#
# Detection may not depend on an existing .rockbox directory, because a
# first-time install does not have one. Identification is based on the USB bus
# protocol, the media name reported by IOKit, the whole-disk size, and the
# partition layout, exactly as the Linux deployment guard does with lsblk.

# Minimum whole-disk size accepted as an iPod Classic. Mirrors the 32 GB guard
# in tools/deploy_ipod6g_preserve_database.sh so a stray small USB stick can
# never reach a write.
RP_MIN_DISK_BYTES=$((32 * 1024 * 1024 * 1024))

# Result fields for the selected device.
RP_DISK=""            # e.g. disk4
RP_PART=""            # e.g. disk4s2
RP_MOUNT=""           # e.g. /Volumes/IPOD
RP_FSTYPE=""
RP_FSNAME=""
RP_VOLUME_NAME=""
RP_DISK_SIZE=""
RP_FREE_SPACE=""
RP_WRITABLE=""
RP_MEDIA_NAME=""
RP_DEVICE_SIGNATURE=""

# plist_get <file> <keypath>
# Prints the raw scalar, or nothing and returns non-zero when absent.
plist_get()
{
    ${RP_BIN}/plutil -extract "$2" raw -o - "$1" 2>/dev/null
}

human_size()
{
    local bytes="$1"

    if [ -z "${bytes}" ] || [ "${bytes}" -lt 1 ] 2>/dev/null; then
        echo "unknown"
        return 0
    fi
    echo "${bytes}" | awk '{ printf "%.0f GB", $1 / 1000000000 }'
}

# Build the stable identity string compared before and after every privileged
# or destructive step.
device_signature()
{
    local disk="$1"
    local info="${RP_WORK}/whole-${disk}.plist"

    printf '%s|%s|%s|%s' \
        "$(plist_get "${info}" MediaName)" \
        "$(plist_get "${info}" Size)" \
        "$(plist_get "${info}" DeviceNode)" \
        "$(plist_get "${info}" IORegistryEntryName)"
}

# Populate RP_* for a single positively identified iPod, or fail.
discover_ipod()
{
    local disks_plist="${RP_WORK}/disks.plist"
    local disk_count
    local i
    local j
    local disk
    local info
    local bus
    local media
    local size
    local part_count
    local part
    local candidates=""
    local candidate_count=0

    stage "detecting"

    if ! ${RP_SBIN}/diskutil list -plist external physical >"${disks_plist}" 2>/dev/null; then
        fail "RP-DISK-NOT-FOUND" \
            "Setup could not ask macOS for the list of connected disks." \
            "Reconnect the iPod and open Setup again."
    fi

    disk_count="$(plist_get "${disks_plist}" AllDisksAndPartitions)"
    [ -n "${disk_count}" ] || disk_count=0

    i=0
    while [ "${i}" -lt "${disk_count}" ]; do
        disk="$(plist_get "${disks_plist}" "AllDisksAndPartitions.${i}.DeviceIdentifier")"
        i=$((i + 1))
        [ -n "${disk}" ] || continue

        info="${RP_WORK}/whole-${disk}.plist"
        ${RP_SBIN}/diskutil info -plist "/dev/${disk}" >"${info}" 2>/dev/null || continue

        bus="$(plist_get "${info}" BusProtocol)"
        media="$(plist_get "${info}" MediaName)"
        size="$(plist_get "${info}" Size)"

        case "${bus}" in
            USB|"USB "*) ;;
            *) continue ;;
        esac
        case "${media}" in
            *iPod*|*IPOD*) ;;
            *) continue ;;
        esac
        if [ -z "${size}" ] || [ "${size}" -lt "${RP_MIN_DISK_BYTES}" ] 2>/dev/null; then
            log "skipping ${disk}: ${size:-unknown} bytes is below the iPod size guard"
            continue
        fi

        log "candidate ${disk}: ${media}, ${size} bytes, bus ${bus}"
        candidates="${candidates}${disk} "
        candidate_count=$((candidate_count + 1))
    done

    if [ "${candidate_count}" -eq 0 ]; then
        fail "RP-DISK-NOT-FOUND" \
            "Setup did not find a supported iPod." \
            "Connect one iPod with its USB cable, wait for it to appear in Finder, then open Setup again."
    fi
    if [ "${candidate_count}" -gt 1 ]; then
        fail "RP-DISK-AMBIGUOUS" \
            "More than one iPod is connected (${candidates})." \
            "Disconnect the other iPods so exactly one remains, then open Setup again."
    fi

    RP_DISK="$(echo "${candidates}" | awk '{ print $1 }')"
    info="${RP_WORK}/whole-${RP_DISK}.plist"
    RP_MEDIA_NAME="$(plist_get "${info}" MediaName)"
    RP_DISK_SIZE="$(plist_get "${info}" Size)"
    RP_DEVICE_SIGNATURE="$(device_signature "${RP_DISK}")"

    # Locate the mounted data partition on that whole disk.
    i=0
    while [ "${i}" -lt "${disk_count}" ]; do
        if [ "$(plist_get "${disks_plist}" "AllDisksAndPartitions.${i}.DeviceIdentifier")" = "${RP_DISK}" ]; then
            break
        fi
        i=$((i + 1))
    done

    part_count="$(plist_get "${disks_plist}" "AllDisksAndPartitions.${i}.Partitions")"
    [ -n "${part_count}" ] || part_count=0

    j=0
    while [ "${j}" -lt "${part_count}" ]; do
        part="$(plist_get "${disks_plist}" "AllDisksAndPartitions.${i}.Partitions.${j}.DeviceIdentifier")"
        j=$((j + 1))
        [ -n "${part}" ] || continue

        info="${RP_WORK}/part-${part}.plist"
        ${RP_SBIN}/diskutil info -plist "/dev/${part}" >"${info}" 2>/dev/null || continue

        if [ -n "$(plist_get "${info}" MountPoint)" ]; then
            RP_PART="${part}"
            break
        fi
        # Remember an unmounted data candidate in case nothing is mounted.
        if [ -z "${RP_PART}" ]; then
            case "$(plist_get "${info}" FilesystemType)" in
                msdos|exfat|hfs) RP_PART="${part}" ;;
            esac
        fi
    done

    if [ -z "${RP_PART}" ]; then
        fail "RP-DISK-NOT-FOUND" \
            "The iPod was found but Setup could not identify its data partition." \
            "Disconnect and reconnect the iPod, then open Setup again."
    fi

    info="${RP_WORK}/part-${RP_PART}.plist"
    RP_MOUNT="$(plist_get "${info}" MountPoint)"
    RP_FSTYPE="$(plist_get "${info}" FilesystemType)"
    RP_FSNAME="$(plist_get "${info}" FilesystemName)"
    RP_VOLUME_NAME="$(plist_get "${info}" VolumeName)"
    RP_FREE_SPACE="$(plist_get "${info}" FreeSpace)"
    RP_WRITABLE="$(plist_get "${info}" WritableVolume)"

    if [ -z "${RP_MOUNT}" ]; then
        if ! ${RP_SBIN}/diskutil mount "/dev/${RP_PART}" >/dev/null 2>&1; then
            fail "RP-REMOUNT-FAILED" \
                "The iPod's music volume is not mounted and macOS refused to mount it." \
                "Disconnect and reconnect the iPod, wait for it to appear in Finder, then open Setup again."
        fi
        ${RP_SBIN}/diskutil info -plist "/dev/${RP_PART}" >"${info}" 2>/dev/null
        RP_MOUNT="$(plist_get "${info}" MountPoint)"
        RP_FREE_SPACE="$(plist_get "${info}" FreeSpace)"
    fi

    log "selected /dev/${RP_PART} mounted at ${RP_MOUNT} (${RP_FSNAME:-${RP_FSTYPE}})"
}

# Hard stops that must be evaluated before anything is written.
preflight()
{
    stage "preflight"

    case "${RP_FSTYPE}" in
        msdos|exfat)
            ;;
        hfs|hfsplus|apfs)
            fail "RP-FS-HFS" \
                "This iPod's music volume is formatted as ${RP_FSNAME:-${RP_FSTYPE}}. Rockbox cannot run from an HFS-formatted iPod." \
                "Follow the FAT32 preparation guide in the RockPod documentation, then open Setup again."
            ;;
        *)
            fail "RP-FS-HFS" \
                "Setup does not recognise the filesystem on this iPod (${RP_FSTYPE:-unknown})." \
                "Restore the iPod to a FAT32 layout with iTunes or Finder, then open Setup again."
            ;;
    esac

    if [ "${RP_WRITABLE}" != "true" ] && [ "${RP_WRITABLE}" != "1" ]; then
        fail "RP-FS-READONLY" \
            "The iPod's music volume is mounted read-only." \
            "Eject the iPod, reconnect it, and make sure the hold switch is off, then open Setup again."
    fi

    if [ -n "${RP_FREE_SPACE}" ] && [ "${RP_FREE_SPACE}" -lt "${RP_REQUIRED_BYTES}" ] 2>/dev/null; then
        fail "RP-SPACE" \
            "The iPod needs about $(human_size "${RP_REQUIRED_BYTES}") free but only has $(human_size "${RP_FREE_SPACE}")." \
            "Remove some files from the iPod, then open Setup again."
    fi

    if [ ! -d "${RP_MOUNT}" ] || [ ! -w "${RP_MOUNT}" ]; then
        fail "RP-FS-READONLY" \
            "Setup cannot write to ${RP_MOUNT}." \
            "Check the iPod is not locked, then open Setup again."
    fi
}

# Re-read the device identity and refuse to continue if anything moved.
assert_identity_unchanged()
{
    local info="${RP_WORK}/recheck-${RP_DISK}.plist"
    local now

    if ! ${RP_SBIN}/diskutil info -plist "/dev/${RP_DISK}" >"${info}" 2>/dev/null; then
        fail "RP-DISK-AMBIGUOUS" \
            "The iPod disappeared from macOS while Setup was working." \
            "Reconnect the iPod and open Setup again to resume the checks."
    fi

    now="$(printf '%s|%s|%s|%s' \
        "$(plist_get "${info}" MediaName)" \
        "$(plist_get "${info}" Size)" \
        "$(plist_get "${info}" DeviceNode)" \
        "$(plist_get "${info}" IORegistryEntryName)")"

    if [ "${now}" != "${RP_DEVICE_SIGNATURE}" ]; then
        fail "RP-DISK-AMBIGUOUS" \
            "The connected disk changed identity while Setup was working." \
            "Disconnect every other USB disk, reconnect the iPod, and open Setup again."
    fi
}

# Detect an existing Personal Rockbox installation on the volume.
installed_rockbox_version()
{
    local info="${RP_MOUNT}/.rockbox/rockbox-info.txt"

    if [ -f "${info}" ]; then
        awk -F': *' '/^Version:/ { print $2; exit }' "${info}"
    fi
}

installed_rockbox_target()
{
    local info="${RP_MOUNT}/.rockbox/rockbox-info.txt"

    if [ -f "${info}" ]; then
        awk -F': *' '/^Target:/ { print $2; exit }' "${info}"
    fi
}
