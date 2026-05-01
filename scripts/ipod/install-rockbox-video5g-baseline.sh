#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
VARIANT_NAME=${VARIANT_NAME:-video5g-baseline}
RB_DIR=${RB_DIR:-"/.rockbox-${VARIANT_NAME}"}
BUILD_DIR=${BUILD_DIR:-"${ROOT_DIR}/build-hw-ipodvideo-5g-${VARIANT_NAME}"}
PACKAGE_PATH=${PACKAGE_PATH:-"${BUILD_DIR}/rockbox-${VARIANT_NAME}-curated.zip"}
MENU_LABEL=${MENU_LABEL:-"Video 5G Baseline"}
PLAYER_NAME=${PLAYER_NAME:-"${MENU_LABEL}"}
IPOD_MOUNT=${IPOD_MOUNT:-}
UPDATE_LOADER=${UPDATE_LOADER:-0}
DRY_RUN=${DRY_RUN:-1}
CONFIRM_IPOD_INSTALL=${CONFIRM_IPOD_INSTALL:-0}
TIMESTAMP=$(date +%Y%m%d-%H%M%S)
BACKUP_ROOT=${BACKUP_ROOT:-"${ROOT_DIR}/backups/ipodvideo-5g/${TIMESTAMP}"}

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

detect_ipod_mount() {
    local base
    local dir
    local matches=()

    for base in "/run/media/${USER}" "/media/${USER}" "/Volumes"; do
        [ -d "${base}" ] || continue
        while IFS= read -r dir; do
            if [ -d "${dir}/.rockbox" ] && [ -f "${dir}/loader.cfg" ]; then
                matches+=("${dir}")
            fi
        done < <(find "${base}" -mindepth 1 -maxdepth 2 -type d 2>/dev/null | sort)
    done

    if [ "${#matches[@]}" -eq 1 ]; then
        printf '%s\n' "${matches[0]}"
        return 0
    fi

    if [ "${#matches[@]}" -eq 0 ]; then
        fail "Could not auto-detect the mounted iPod. Set IPOD_MOUNT=/path/to/mountpoint."
    fi

    printf 'Multiple plausible iPod mounts found:\n' >&2
    printf '  %s\n' "${matches[@]}" >&2
    fail "Refusing to guess. Set IPOD_MOUNT explicitly."
}

case "${RB_DIR}" in
    /*) ;;
    *)
        fail "RB_DIR must be an absolute iPod path such as /.rockbox-video5g-baseline."
        ;;
esac

case "${RB_DIR}" in
    /.rockbox|/.rockbox/*)
        fail "RB_DIR must not point at the live /.rockbox tree."
        ;;
esac

[ -f "${PACKAGE_PATH}" ] || fail "Missing build package: ${PACKAGE_PATH}"

if [ -z "${IPOD_MOUNT}" ]; then
    IPOD_MOUNT=$(detect_ipod_mount)
fi

[ -d "${IPOD_MOUNT}" ] || fail "Mountpoint does not exist: ${IPOD_MOUNT}"
[ -d "${IPOD_MOUNT}/.rockbox" ] || fail "Mountpoint does not look like the current Rockbox iPod install: ${IPOD_MOUNT}"

LOADER_CFG="${IPOD_MOUNT}/loader.cfg"
LOADER_ENTRY="${MENU_LABEL} @ (hd0,1)${RB_DIR}/rockbox.ipod"

cat <<EOF
iPod mount:     ${IPOD_MOUNT}
Package:        ${PACKAGE_PATH}
Runtime dir:    ${RB_DIR}
Player name:    ${PLAYER_NAME}
Loader update:  ${UPDATE_LOADER}
Dry run:        ${DRY_RUN}
Backup root:    ${BACKUP_ROOT}

Planned install:
  unzip -o ${PACKAGE_PATH} -d ${IPOD_MOUNT}
  write ${IPOD_MOUNT}${RB_DIR}/playername.txt
EOF

if [ "${UPDATE_LOADER}" = "1" ]; then
    printf '  add loader entry: %s\n' "${LOADER_ENTRY}"
fi

if [ "${DRY_RUN}" = "1" ]; then
    exit 0
fi

[ "${CONFIRM_IPOD_INSTALL}" = "1" ] || fail "Set CONFIRM_IPOD_INSTALL=1 to perform the copy."

mkdir -p "${BACKUP_ROOT}"

if [ -d "${IPOD_MOUNT}${RB_DIR}" ]; then
    cp -a "${IPOD_MOUNT}${RB_DIR}" "${BACKUP_ROOT}/"
fi

if [ -f "${LOADER_CFG}" ]; then
    cp -a "${LOADER_CFG}" "${BACKUP_ROOT}/loader.cfg"
fi

unzip -oq "${PACKAGE_PATH}" -d "${IPOD_MOUNT}"
mkdir -p "${IPOD_MOUNT}${RB_DIR}"
printf '%s\n' "${PLAYER_NAME}" > "${IPOD_MOUNT}${RB_DIR}/playername.txt"

if [ "${UPDATE_LOADER}" = "1" ]; then
    touch "${LOADER_CFG}"
    if ! grep -Fqx "${LOADER_ENTRY}" "${LOADER_CFG}"; then
        printf '\n%s\n' "${LOADER_ENTRY}" >> "${LOADER_CFG}"
    fi
fi

cat <<EOF
Installed separate Rockbox baseline to ${IPOD_MOUNT}${RB_DIR}
Backups saved under ${BACKUP_ROOT}
EOF
