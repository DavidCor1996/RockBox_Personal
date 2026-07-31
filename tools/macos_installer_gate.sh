#!/usr/bin/env bash
#
# Regression gate for the offline macOS installer.
#
# The installer only ever runs on a Mac, but its safety rules are the same rules
# tools/deploy_ipod6g_preserve_database.sh enforces on Linux, and they are worth
# testing on every build host. This gate stands up a fake iPod mount and a stub
# /usr/bin and /usr/sbin, then drives the installer's own functions through the
# hard-stop cases the specification requires.
#
# It does not test AppleScript rendering, Gatekeeper, or diskutil itself.
#
# usage: tools/macos_installer_gate.sh [--keep]

set -uo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
setup_src="${repo_root}/tools/macos-installer/setup"
keep=0
work=""
passes=0
failures=0

[ "${1:-}" = "--keep" ] && keep=1

cleanup()
{
    if [ "${keep}" -eq 0 ] && [ -n "${work}" ] && [ -d "${work}" ]; then
        rm -rf "${work}"
    elif [ -n "${work}" ]; then
        echo "kept: ${work}"
    fi
}

say()
{
    echo "== $*"
}

ok()
{
    passes=$((passes + 1))
    echo "  PASS  $*"
}

bad()
{
    failures=$((failures + 1))
    echo "  FAIL  $*"
}

work="$(mktemp -d)"
trap cleanup EXIT INT TERM HUP

# ---------------------------------------------------------------- stub tooling

stub_bin="${work}/stub/usr/bin"
stub_sbin="${work}/stub/usr/sbin"
mkdir -p "${stub_bin}" "${stub_sbin}"

# BSD stat -f %z, backed by GNU stat.
cat >"${stub_bin}/stat" <<'EOF'
#!/bin/sh
if [ "$1" = "-f" ] && [ "$2" = "%z" ]; then
    exec /usr/bin/stat -c %s "$3"
fi
exec /usr/bin/stat "$@"
EOF

cat >"${stub_bin}/shasum" <<'EOF'
#!/bin/sh
# shasum -a 256 [files...]
shift 2 2>/dev/null || true
exec /usr/bin/sha256sum "$@"
EOF

for tool in od unzip open; do
    printf '#!/bin/sh\nexec /usr/bin/%s "$@"\n' "${tool}" >"${stub_bin}/${tool}"
done

# Dialogs answer from a script file so each case can choose Install or Cancel.
cat >"${stub_bin}/osascript" <<'EOF'
#!/bin/sh
echo "OSASCRIPT: $*" >>"${RP_GATE_DIALOGS}"
if [ -n "${RP_GATE_ANSWER:-}" ]; then
    echo "${RP_GATE_ANSWER}"
fi
EOF

# diskutil and plutil are driven by fixture plists under ${RP_GATE_FIXTURES}.
cat >"${stub_sbin}/diskutil" <<'EOF'
#!/bin/sh
case "$1 $2" in
    "list -plist")
        cat "${RP_GATE_FIXTURES}/list.plist"
        ;;
    "info -plist")
        node="$(basename "$3")"
        if [ -f "${RP_GATE_FIXTURES}/${node}.plist" ]; then
            cat "${RP_GATE_FIXTURES}/${node}.plist"
        else
            exit 1
        fi
        ;;
    "eject "*)
        echo "ejected $2" >>"${RP_GATE_DIALOGS}"
        ;;
    *)
        exit 1
        ;;
esac
EOF

# Minimal plutil -extract <keypath> raw -o - <file> over the flat "key = value"
# fixture format used by this gate.
cat >"${stub_bin}/plutil" <<'EOF'
#!/bin/sh
# plutil -extract KEYPATH raw -o - FILE
key="$2"
file="$6"
[ -f "${file}" ] || exit 1
value="$(awk -v k="${key}" -F' = ' '$1 == k { print $2; found = 1; exit } END { if (!found) exit 1 }' "${file}")" || exit 1
printf '%s\n' "${value}"
EOF

chmod +x "${stub_bin}"/* "${stub_sbin}"/*

export RP_BIN="${stub_bin}"
export RP_SBIN="${stub_sbin}"
export RP_GATE_DIALOGS="${work}/dialogs.txt"
: >"${RP_GATE_DIALOGS}"

# Stub bootloader helpers, logging every invocation to RP_GATE_TOOLLOG so
# ordering (backup before write, no write after a failed read, etc.) can be
# checked afterwards. Placed directly at the path build_bootloader_tool()
# looks for an already-built tool, so these tests never need a real compiler
# or the bundled tool-src to be present.
export RP_GATE_TOOLLOG="${work}/toollog.txt"
: >"${RP_GATE_TOOLLOG}"
tools_dir="${work}/home/Library/Application Support/RockPod/tools"
mkdir -p "${tools_dir}"

cat >"${tools_dir}/mks5lboot" <<'EOF'
#!/bin/sh
echo "mks5lboot $*" >>"${RP_GATE_TOOLLOG}"
case "$1" in
    --dfuscan)
        if [ "${RP_GATE_DFU_FOUND:-0}" = "1" ]; then
            echo "1 device found in DFU mode"
        else
            echo "No DFU devices found"
        fi
        ;;
    --bl-inst)
        [ "${RP_GATE_BLINST_FAIL:-0}" = "1" ] && { echo "install failed" >&2; exit 1; }
        echo "install ok"
        ;;
    *)
        echo "unhandled mks5lboot invocation: $*" >&2
        exit 1
        ;;
esac
EOF

cat >"${tools_dir}/ipodpatcher" <<'EOF'
#!/bin/sh
device="$1"
shift
echo "ipodpatcher ${device} $*" >>"${RP_GATE_TOOLLOG}"
case "$1" in
    --read-partition)
        [ "${RP_GATE_READ_FAIL:-0}" = "1" ] && { echo "read failed" >&2; exit 1; }
        printf 'BOOTPARTITIONDATA' >"$2"
        ;;
    --add-bootloader)
        [ "${RP_GATE_WRITE_FAIL:-0}" = "1" ] && { echo "write failed" >&2; exit 1; }
        echo "bootloader added"
        ;;
    *)
        echo "unhandled ipodpatcher invocation: $*" >&2
        exit 1
        ;;
esac
EOF

chmod +x "${tools_dir}/mks5lboot" "${tools_dir}/ipodpatcher"

# ------------------------------------------------------------------- fixtures

fixtures="${work}/fixtures"
mkdir -p "${fixtures}"
export RP_GATE_FIXTURES="${fixtures}"

ipod_size=$((160 * 1000 * 1000 * 1000))

write_fixtures()
{
    local disks="$1"           # space separated whole-disk ids
    local part_fstype="${2:-msdos}"
    local writable="${3:-true}"
    local index=0
    local disk

    : >"${fixtures}/list.plist"
    {
        echo "AllDisksAndPartitions = $(echo "${disks}" | wc -w | tr -d ' ')"
        for disk in ${disks}; do
            echo "AllDisksAndPartitions.${index}.DeviceIdentifier = ${disk}"
            echo "AllDisksAndPartitions.${index}.Partitions = 1"
            echo "AllDisksAndPartitions.${index}.Partitions.0.DeviceIdentifier = ${disk}s2"
            index=$((index + 1))
        done
    } >"${fixtures}/list.plist"

    for disk in ${disks}; do
        cat >"${fixtures}/${disk}.plist" <<EOF
BusProtocol = USB
MediaName = iPod
Size = ${ipod_size}
DeviceNode = /dev/${disk}
IORegistryEntryName = iPod
EOF
        cat >"${fixtures}/${disk}s2.plist" <<EOF
MountPoint = ${mount}
FilesystemType = ${part_fstype}
FilesystemName = MS-DOS FAT32
VolumeName = IPOD
FreeSpace = 100000000000
WritableVolume = ${writable}
EOF
    done
}

# ------------------------------------------------------- fake iPod data volume

mount="${work}/Volumes/IPOD"

sample_db=""
for candidate in "${repo_root}"/.tmp-ipodjs-*/.rockbox "${repo_root}"/simdisk/.rockbox; do
    if [ -f "${candidate}/database_idx.tcd" ] && [ -f "${candidate}/database_4.tcd" ]; then
        sample_db="${candidate}"
        break
    fi
done

reset_mount()
{
    local with_rockbox="$1"

    rm -rf "${work}/Volumes"
    mkdir -p "${mount}/Music"
    if [ "${with_rockbox}" = "yes" ]; then
        mkdir -p "${mount}/.rockbox"
        cp "${sample_db}"/database_*.tcd "${mount}/.rockbox/" 2>/dev/null
        printf 'Target: ipod6g\nVersion: gate-baseline\n' \
            >"${mount}/.rockbox/rockbox-info.txt"
        printf 'volume: 0\n' >"${mount}/.rockbox/config.cfg"
    fi
}

# ------------------------------------------------------------- installer under test

payload="${work}/payload"
mkdir -p "${payload}"

make_payload()
{
    local target="${1:-ipod6g}"
    local zip_dir="${work}/pkgsrc"

    rm -rf "${zip_dir}"
    mkdir -p "${zip_dir}/.rockbox/rocks"
    printf 'firmware bytes\n' >"${zip_dir}/.rockbox/rockbox-info.txt"
    printf 'plugin\n' >"${zip_dir}/.rockbox/rocks/test.rock"
    rm -f "${payload}/rockbox.zip"
    ( cd "${zip_dir}" && zip -qr "${payload}/rockbox.zip" . )

    printf 'FIRMWARE-PAYLOAD-%s\n' "${target}" >"${payload}/rockbox.ipod"
    printf 'Target: %s\nVersion: gate-release\n' "${target}" >"${payload}/rockbox-info.txt"
    printf 'BOOTLOADER-PAYLOAD-%s\n' "${target}" >"${payload}/bootloader-${target}.ipod"
    cat >"${payload}/release.env" <<EOF
RP_RELEASE_ID="gate-release"
RP_TARGET="${target}"
RP_DEVICE_LABEL="iPod Classic 6G/7G"
RP_SETUP_VERSION="1.0.0"
RP_REQUIRED_BYTES=1048576
EOF
    printf '{}\n' >"${payload}/manifest.json"
    ( cd "${payload}" && sha256sum rockbox.zip rockbox.ipod rockbox-info.txt \
        release.env manifest.json >SHA256SUMS )
}

# Run one installer function in a subshell with the libraries loaded, and report
# the error code it stopped on (or "OK").
run_case()
{
    local body="$1"
    local out

    out="$(
        export HOME="${work}/home"
        mkdir -p "${HOME}"
        exec 2>/dev/null

        # The libraries clear their own RP_* state when sourced, exactly as they
        # do in main.sh, so the fixture values are assigned afterwards.
        . "${setup_src}/lib/platform.sh"
        . "${payload}/release.env"
        . "${setup_src}/lib/log.sh"
        . "${setup_src}/lib/ui.sh"
        . "${setup_src}/lib/discovery.sh"
        . "${setup_src}/lib/package.sh"
        . "${setup_src}/lib/database.sh"
        . "${setup_src}/lib/deploy.sh"
        . "${setup_src}/lib/stock.sh"
        . "${setup_src}/lib/bootloader.sh"

        RP_SETUP_DIR="${setup_src}"
        RP_PAYLOAD="${payload}"
        RP_WORK="$(mktemp -d "${work}/run.XXXXXX")"
        RP_JOURNAL_FILE="${RP_WORK}/journal.log"
        RP_LOG_FILE="${RP_WORK}/setup.log"
        RP_MOUNT="${mount}"
        RP_DISK="disk4"
        RP_PART="disk4s2"
        RP_FSTYPE="msdos"
        RP_FSNAME="MS-DOS FAT32"
        RP_WRITABLE="true"
        RP_FREE_SPACE=100000000000
        RP_DISK_SIZE=${ipod_size}
        RP_DEVICE_SIGNATURE="iPod|${ipod_size}|/dev/disk4|iPod"

        # Report the code instead of drawing a dialog.
        ui_failure() { echo "CODE:$1"; }
        ui_info() { :; }
        ui_notify() { :; }

        eval "${body}" && echo "CODE:OK"
    )"

    echo "${out}" | grep -o 'CODE:[A-Z-]*' | tail -1 | sed 's/^CODE://'
}

expect()
{
    local label="$1"
    local want="$2"
    local body="$3"
    local got

    got="$(run_case "${body}")"
    if [ "${got}" = "${want}" ]; then
        ok "${label} (${got})"
    else
        bad "${label}: expected ${want}, got ${got:-<nothing>}"
    fi
}

# --------------------------------------------------------------------- cases

if [ -z "${sample_db}" ]; then
    echo "macos_installer_gate: no sample Rockbox database found; cannot run" >&2
    exit 2
fi

make_payload ipod6g

say "package verification"
expect "intact payload passes" OK "verify_payload_hashes"
expect "matching target passes" OK "verify_package_target"
expect "safe archive passes" OK "verify_package_entries"

( printf 'FIRMWARE-TAMPERED\n' >"${payload}/rockbox.ipod" )
expect "tampered payload is rejected" RP-HASH-MISMATCH "verify_payload_hashes"
make_payload ipod6g

# A bundle whose firmware was built for another iPod must never be installed,
# even though release.env still claims this installer's target.
printf 'Target: ipodvideo\nVersion: gate-release\n' >"${payload}/rockbox-info.txt"
expect "wrong-target package is rejected" RP-PKG-TARGET "verify_package_target"
make_payload ipod6g

hostile="${work}/hostile"
rm -rf "${hostile}"; mkdir -p "${hostile}"
ln -s /etc/passwd "${hostile}/link"
( cd "${hostile}" && zip -qry "${payload}/rockbox.zip" . )
( cd "${payload}" && sha256sum rockbox.zip rockbox.ipod rockbox-info.txt \
    release.env manifest.json >SHA256SUMS )
expect "archive with a symlink is rejected" RP-PKG-UNSAFE-ZIP "verify_package_entries"
make_payload ipod6g

say "device discovery"
reset_mount yes
write_fixtures "disk4"
expect "one iPod is accepted" OK "discover_ipod"
write_fixtures "disk4 disk6"
expect "two iPods are rejected" RP-DISK-AMBIGUOUS "discover_ipod"
write_fixtures "disk4"

expect "HFS volume is rejected" RP-FS-HFS \
    'RP_FSTYPE=hfs; RP_FSNAME="Mac OS Extended"; RP_WRITABLE=true; preflight'
expect "read-only volume is rejected" RP-FS-READONLY \
    'RP_FSTYPE=msdos; RP_WRITABLE=false; preflight'
expect "too little free space is rejected" RP-SPACE \
    'RP_FSTYPE=msdos; RP_WRITABLE=true; RP_FREE_SPACE=1024; preflight'
expect "FAT32 volume is accepted" OK \
    'RP_FSTYPE=msdos; RP_WRITABLE=true; RP_FREE_SPACE=100000000000; preflight'
expect "changed disk identity is rejected" RP-DISK-AMBIGUOUS \
    'RP_DEVICE_SIGNATURE="something|else|entirely|now"; assert_identity_unchanged'

say "database guard"
reset_mount yes
expect "healthy database passes the structural check" OK "verify_database_structure"

reset_mount yes
rm -f "${mount}/.rockbox/database_idx.tcd"
expect "missing database_idx.tcd is a hard stop" RP-DB-INVALID "verify_database_structure"

reset_mount yes
: >"${mount}/.rockbox/database_idx.tcd"
expect "empty database_idx.tcd is a hard stop" RP-DB-INVALID "verify_database_structure"

reset_mount yes
printf 'NOTATAGCACHEHEADER!!!!!!' >"${mount}/.rockbox/database_idx.tcd"
expect "corrupt database header is a hard stop" RP-DB-INVALID "verify_database_structure"

reset_mount yes
rm -f "${mount}/.rockbox/database_4.tcd"
expect "missing filename tag file is a hard stop" RP-DB-INVALID "verify_database_structure"

reset_mount yes
head -c 200 "${sample_db}/database_idx.tcd" >"${mount}/.rockbox/database_idx.tcd"
expect "truncated database is a hard stop" RP-DB-INVALID "verify_database_structure"

reset_mount no
expect "a fresh iPod has no database to guard" OK "true"

say "install and preservation"
reset_mount yes
before_sums="$(cd "${mount}/.rockbox" && sha256sum database_*.tcd | sort)"

install_result="$(run_case '
    guard_existing_database
    create_recovery_bundle
    stage_firmware
    restore_database
    install_recovery_snapshot
    enable_tagcache_autoupdate
    verify_database_unchanged
    install_firmware_binaries
')"
if [ "${install_result}" = "OK" ]; then
    ok "full install path completes"
else
    bad "full install path stopped on ${install_result}"
fi

after_sums="$(cd "${mount}/.rockbox" && sha256sum database_*.tcd | sort)"
if [ "${before_sums}" = "${after_sums}" ]; then
    ok "database files are byte-identical after the install"
else
    bad "database files changed during the install"
fi

want_fw="$(sha256sum "${payload}/rockbox.ipod" | awk '{ print $1 }')"
got_root="$(sha256sum "${mount}/rockbox.ipod" 2>/dev/null | awk '{ print $1 }')"
got_rbdir="$(sha256sum "${mount}/.rockbox/rockbox.ipod" 2>/dev/null | awk '{ print $1 }')"
if [ "${got_root}" = "${want_fw}" ] && [ "${got_rbdir}" = "${want_fw}" ]; then
    ok "both firmware copies match the release"
else
    bad "firmware copies do not match: root=${got_root:-missing} rbdir=${got_rbdir:-missing}"
fi

if [ -f "${mount}/.rockbox/rocks/test.rock" ]; then
    ok "package contents were extracted onto the volume"
else
    bad "package contents are missing from the volume"
fi

if [ -d "${mount}/Music" ]; then
    ok "the Music folder was left alone"
else
    bad "the Music folder disappeared"
fi

if grep -q '^tagcache_autoupdate: on$' "${mount}/.rockbox/config.cfg"; then
    ok "tagcache_autoupdate is enabled"
else
    bad "tagcache_autoupdate was not enabled"
fi

if [ -d "${mount}/.rockbox/tagcache_backup" ] &&
   [ -f "${mount}/.rockbox/tagcache_backup/database_idx.tcd" ]; then
    ok "the last-known-good recovery snapshot was installed"
else
    bad "no recovery snapshot on the volume"
fi

if [ -n "$(find "${work}/home/Library/Application Support/RockPod/recovery" \
        -name MANIFEST.txt 2>/dev/null)" ]; then
    ok "a host recovery bundle with a manifest exists"
else
    bad "no host recovery bundle was created"
fi

say "transient transaction artifacts"
reset_mount yes
printf 'stale\n' >"${mount}/.rockbox/database_tmp.tcd"
printf 'stale\n' >"${mount}/.rockbox/database_commit.tcd"
run_case '
    guard_existing_database
    create_recovery_bundle
    stage_firmware
    restore_database
' >/dev/null
if [ ! -e "${mount}/.rockbox/database_tmp.tcd" ] &&
   [ ! -e "${mount}/.rockbox/database_commit.tcd" ]; then
    ok "stale transaction files were removed"
else
    bad "stale transaction files survived the install"
fi

say "bootloader safety invariants"

bootloader_src="${repo_root}/tools/macos-installer/setup/lib/bootloader.sh"
if grep -v '^[[:space:]]*#' "${bootloader_src}" | grep -q -- '--single\|"-S"'; then
    bad "bootloader.sh must never construct mks5lboot's --single/-S option"
else
    ok "bootloader.sh never constructs --single/-S"
fi

make_payload ipod6g
reset_mount yes
write_fixtures "disk4"

export RP_GATE_ANSWER="Ready"
# Keep the DFU poll loop fast and deterministic in the gate; production code
# defaults to a real 60 x 1s scan and a 5s post-install settle.
export RP_DFU_MAX_TRIES=3
export RP_DFU_POLL_INTERVAL=0

: >"${RP_GATE_TOOLLOG}"
unset RP_GATE_DFU_FOUND
expect "Classic: DFU not found is a hard stop" RP-DFU-NOT-FOUND \
    'install_bootloader_classic'
if grep -q -- '--bl-inst' "${RP_GATE_TOOLLOG}"; then
    bad "Classic: bootloader install must not run when DFU was never found"
else
    ok "Classic: bootloader install did not run without a detected DFU device"
fi

: >"${RP_GATE_TOOLLOG}"
export RP_GATE_DFU_FOUND=1
expect "Classic: detected DFU device completes the install" OK \
    'install_bootloader_classic'
if grep -q -- '--dfuscan' "${RP_GATE_TOOLLOG}" && grep -q -- '--bl-inst' "${RP_GATE_TOOLLOG}"; then
    ok "Classic: scanned for DFU before installing the bootloader"
else
    bad "Classic: expected both a DFU scan and a bootloader install in the tool log"
fi
unset RP_GATE_DFU_FOUND

make_payload ipodvideo
reset_mount yes
write_fixtures "disk4"

export RP_GATE_ANSWER="Install"

: >"${RP_GATE_TOOLLOG}"
export RP_GATE_READ_FAIL=1
expect "Video: unreadable boot partition is a hard stop before any write" RP-BOOT-WRITE \
    'RP_BACKUP="${RP_WORK}/host-backup"; mkdir -p "${RP_BACKUP}/firmware"; install_bootloader_video'
if grep -q -- '--add-bootloader' "${RP_GATE_TOOLLOG}"; then
    bad "Video: must never attempt --add-bootloader when the pre-write backup read failed"
else
    ok "Video: no bootloader write was attempted after a failed backup read"
fi
unset RP_GATE_READ_FAIL

: >"${RP_GATE_TOOLLOG}"
export RP_GATE_WRITE_FAIL=1
expect "Video: a failed bootloader write is reported, backup already taken" RP-BOOT-WRITE \
    'RP_BACKUP="${RP_WORK}/host-backup"; mkdir -p "${RP_BACKUP}/firmware"; install_bootloader_video'
if grep -q -- '--read-partition' "${RP_GATE_TOOLLOG}" && grep -q -- '--add-bootloader' "${RP_GATE_TOOLLOG}"; then
    read_line="$(grep -n -- '--read-partition' "${RP_GATE_TOOLLOG}" | head -1 | cut -d: -f1)"
    write_line="$(grep -n -- '--add-bootloader' "${RP_GATE_TOOLLOG}" | head -1 | cut -d: -f1)"
    if [ "${read_line}" -lt "${write_line}" ]; then
        ok "Video: the boot partition backup was read before any write was attempted"
    else
        bad "Video: the bootloader write happened before the backup read"
    fi
else
    bad "Video: expected both a partition read and a bootloader write attempt in the tool log"
fi
unset RP_GATE_WRITE_FAIL

: >"${RP_GATE_TOOLLOG}"
expect "Video: full bootloader install reads, writes, and re-reads the boot partition" OK \
    'RP_BACKUP="${RP_WORK}/host-backup"; mkdir -p "${RP_BACKUP}/firmware"; install_bootloader_video'
read_count="$(grep -c -- '--read-partition' "${RP_GATE_TOOLLOG}")"
if [ "${read_count}" -ge 2 ]; then
    ok "Video: the boot partition was re-read after the write to verify it"
else
    bad "Video: expected at least two partition reads (backup, then verify); got ${read_count}"
fi

unset RP_GATE_ANSWER RP_DFU_MAX_TRIES RP_DFU_POLL_INTERVAL

echo
echo "macos_installer_gate: ${passes} passed, ${failures} failed"
[ "${failures}" -eq 0 ]
