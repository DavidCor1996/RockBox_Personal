#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
recovery="${IPOD6G_RECOVERY_IMAGE:-${repo_root}/.backups/ipod6g-videoout-readiness-20260811/rockbox-recovery-usb-stable.ipod}"
qualification="${IPOD6G_QUALIFICATION_IMAGE:-${repo_root}/build-hw-ipod6g/rockbox.ipod}"
qualification_zip="${IPOD6G_QUALIFICATION_ZIP:-${repo_root}/build-hw-ipod6g/rockbox.zip}"
pack_dir="${IPOD6G_READINESS_PACK:-${repo_root}/dist/ipod6g-dcp750-readiness-20260811}"
known_recovery_sha="466bc88dd599cabd514ae9ed1980a7dae3ae2ab4e1ca87667a54f9b0aaff60e2"

fail()
{
    echo "DCP750 readiness: FAIL: $*" >&2
    exit 1
}

"${repo_root}/tools/ipod6g_videoout_static_gate.sh"

[ -s "${recovery}" ] || fail "missing known-good recovery image: ${recovery}"
[ -s "${qualification}" ] || fail "missing qualification image: ${qualification}"
[ -s "${qualification_zip}" ] || fail "missing qualification package: ${qualification_zip}"

recovery_sha="$(sha256sum "${recovery}" | cut -d' ' -f1)"
qualification_sha="$(sha256sum "${qualification}" | cut -d' ' -f1)"
[ "${recovery_sha}" = "${known_recovery_sha}" ] ||
    fail "known-good recovery checksum changed"
[ "${recovery_sha}" != "${qualification_sha}" ] ||
    fail "recovery and qualification images unexpectedly match"

pack_recovery_sha="$(sha256sum "${pack_dir}/rockbox-recovery-usb-stable.ipod" | cut -d' ' -f1)"
pack_qualification_sha="$(sha256sum "${pack_dir}/rockbox-dcp750-qualification.ipod" | cut -d' ' -f1)"
zip_sha="$(sha256sum "${qualification_zip}" | cut -d' ' -f1)"
pack_zip_sha="$(sha256sum "${pack_dir}/rockbox-dcp750-qualification.zip" | cut -d' ' -f1)"
[ "${pack_recovery_sha}" = "${recovery_sha}" ] || fail "frozen recovery image mismatch"
[ "${pack_qualification_sha}" = "${qualification_sha}" ] ||
    fail "frozen qualification image mismatch"
[ "${pack_zip_sha}" = "${zip_sha}" ] || fail "frozen qualification ZIP is stale"
unzip -tq "${qualification_zip}" >/dev/null || fail "qualification ZIP is invalid"
for frozen_sha in "${recovery_sha}" "${qualification_sha}" "${zip_sha}"; do
    rg -q "${frozen_sha}" "${pack_dir}/README.md" ||
        fail "readiness-pack README has a stale checksum"
done

rg -q 'DBG_VIDEOOUT_STAGE_TIMEOUT \(60 \* HZ\)' "${repo_root}/apps/debug_menu.c" ||
    fail "diagnostic stage timeout is missing"
rg -q 'global_settings\.composite_video_output = IPOD6G_VIDEOOUT_OFF' \
    "${repo_root}/apps/settings.c" || fail "normal setting path is not fail-closed"
! rg -q '2200200c' \
    "${repo_root}/firmware/target/arm/s5l8702/ipod6g/videoout-6g.c" ||
    fail "unsafe assumed stock-firmware function address remains"

if [ "$#" -gt 0 ]; then
    "${repo_root}/tools/ipod6g_stock_firmware_verify.py" "$1"
fi

echo "recovery      ${recovery_sha}  ${recovery}"
echo "qualification ${qualification_sha}  ${qualification}"
echo "package       ${zip_sha}  ${qualification_zip}"
echo "DCP750 readiness: static firmware and recovery gates PASS"
echo "DCP750 readiness: analog CVBS remains pending the physical dock test"
