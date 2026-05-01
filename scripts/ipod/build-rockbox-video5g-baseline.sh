#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TARGET=${TARGET:-ipodvideo}
RAM=${RAM:-64}
TYPE=${TYPE:-N}
VARIANT_NAME=${VARIANT_NAME:-video5g-baseline}
RB_DIR=${RB_DIR:-"/.rockbox-${VARIANT_NAME}"}
BUILD_DIR=${BUILD_DIR:-"${ROOT_DIR}/build-hw-ipodvideo-5g-${VARIANT_NAME}"}
PACKAGE_TARGET=${PACKAGE_TARGET:-fullzip}
MENU_LABEL=${MENU_LABEL:-"Video 5G Baseline"}
PLAYER_NAME=${PLAYER_NAME:-"${MENU_LABEL}"}
FORCE_RECONFIGURE=${FORCE_RECONFIGURE:-0}
CONFIGURE_SCRIPT=${CONFIGURE_SCRIPT:-"${ROOT_DIR}/tools/configure"}
CURATE_PLUGINS=${CURATE_PLUGINS:-1}
PLUGIN_ALLOWLIST_FILE=${PLUGIN_ALLOWLIST_FILE:-"${ROOT_DIR}/scripts/ipod/plugin-allowlist-video5g-baseline.txt"}

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

load_keep_patterns() {
    KEEP_PLUGIN_PATTERNS=()
    [ -f "${PLUGIN_ALLOWLIST_FILE}" ] || fail "Missing plugin allowlist: ${PLUGIN_ALLOWLIST_FILE}"
    while IFS= read -r line; do
        case "${line}" in
            ''|'#'*) continue ;;
        esac
        KEEP_PLUGIN_PATTERNS+=("${line}")
    done < "${PLUGIN_ALLOWLIST_FILE}"
}

matches_keep_pattern() {
    local name=$1
    local pattern
    for pattern in "${KEEP_PLUGIN_PATTERNS[@]}"; do
        case "${name}" in
            ${pattern}) return 0 ;;
        esac
    done
    return 1
}

curate_plugin_tree() {
    local tree_root=$1
    local path
    local base

    [ -d "${tree_root}" ] || return 0

    while IFS= read -r path; do
        base=$(basename "${path}")
        if ! matches_keep_pattern "${base}"; then
            rm -rf "${path}"
        fi
    done < <(find "${tree_root}" -mindepth 1 -type f | sort)

    find "${tree_root}" -depth -type d -empty -delete
}

case "${TARGET}" in
    ipodvideo) ;;
    *)
        fail "This script is only for the iPod Video 5G/5.5G target. Set TARGET=ipodvideo."
        ;;
esac

case "${RB_DIR}" in
    /*) ;;
    *)
        fail "RB_DIR must be an absolute iPod path such as /.rockbox-video5g-baseline."
        ;;
esac

case "${RB_DIR}" in
    /.rockbox|/.rockbox/*)
        fail "RB_DIR must not reuse the live Rockbox tree. Pick a separate directory."
        ;;
esac

[ -x "${CONFIGURE_SCRIPT}" ] || fail "Missing configure script: ${CONFIGURE_SCRIPT}"
load_keep_patterns

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

if [ "${FORCE_RECONFIGURE}" = "1" ] || [ ! -f Makefile ] || ! grep -Fq "export RBDIR=${RB_DIR}" Makefile; then
    "${CONFIGURE_SCRIPT}" --target="${TARGET}" --ram="${RAM}" --type="${TYPE}" --rbdir="${RB_DIR}"
fi

make "${PACKAGE_TARGET}"

PACKAGE_PATH="${BUILD_DIR}/rockbox-full.zip"
[ -f "${PACKAGE_PATH}" ] || fail "Expected package not found: ${PACKAGE_PATH}"

RB_SUBDIR=${RB_DIR#/}
FULL_STAGE_ROOT="${BUILD_DIR}/stage-root-full"
CURATED_STAGE_ROOT="${BUILD_DIR}/stage-root-curated"
CURATED_PACKAGE_PATH="${BUILD_DIR}/rockbox-${VARIANT_NAME}-curated.zip"

rm -rf "${FULL_STAGE_ROOT}" "${CURATED_STAGE_ROOT}"
mkdir -p "${FULL_STAGE_ROOT}" "${CURATED_STAGE_ROOT}"
unzip -oq "${PACKAGE_PATH}" -d "${FULL_STAGE_ROOT}"
cp -a "${FULL_STAGE_ROOT}/." "${CURATED_STAGE_ROOT}/"

if [ "${CURATE_PLUGINS}" = "1" ]; then
    curate_plugin_tree "${CURATED_STAGE_ROOT}/${RB_SUBDIR}/rocks"
    curate_plugin_tree "${CURATED_STAGE_ROOT}/${RB_SUBDIR}/rocks.data"
fi

mkdir -p "${CURATED_STAGE_ROOT}${RB_DIR}"
printf '%s\n' "${PLAYER_NAME}" > "${CURATED_STAGE_ROOT}${RB_DIR}/playername.txt"
printf '%s @ (hd0,1)%s/rockbox.ipod\n' "${MENU_LABEL}" "${RB_DIR}" > "${BUILD_DIR}/ipodloader2-${VARIANT_NAME}.entry.txt"

(
    cd "${CURATED_STAGE_ROOT}"
    rm -f "${CURATED_PACKAGE_PATH}"
    zip -rq "${CURATED_PACKAGE_PATH}" "${RB_SUBDIR}"
)

cat <<EOF
Built separate Rockbox baseline for iPod Video 5G/5.5G.

Target:        ${TARGET}
Build dir:     ${BUILD_DIR}
Runtime dir:   ${RB_DIR}
Full package:  ${PACKAGE_PATH}
Curated zip:   ${CURATED_PACKAGE_PATH}
Staged tree:   ${CURATED_STAGE_ROOT}${RB_DIR}
Player name:   ${PLAYER_NAME}
Loader entry:  ${BUILD_DIR}/ipodloader2-${VARIANT_NAME}.entry.txt
Allowlist:     ${PLUGIN_ALLOWLIST_FILE}

This build is isolated from the live /.rockbox install.
EOF
