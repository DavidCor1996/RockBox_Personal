#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
qemu_arm="${QEMU_ARM_STATIC:-$(command -v qemu-arm-static || true)}"

if [[ -z "${qemu_arm}" || ! -x "${qemu_arm}" ]]; then
    echo "qemu-arm-static not found; set QEMU_ARM_STATIC to its path" >&2
    exit 2
fi

work_dir="$(mktemp -d -t n25-ramdiag-emulation.XXXXXX)"
cleanup()
{
    rm -rf -- "${work_dir}"
}
trap cleanup EXIT

arm-none-eabi-as -mcpu=arm926ej-s \
    -o "${work_dir}/init.o" "${repo_root}/tools/ipod6g_android/ramdiag/init.S"
arm-none-eabi-ld -nostdlib -static -Ttext=0x10000 \
    -o "${work_dir}/init" "${work_dir}/init.o"

set +e
timeout 3 "${qemu_arm}" -cpu arm926 "${work_dir}/init" --emulate \
    </dev/null >"${work_dir}/heartbeat.log" 2>&1
heartbeat_status=$?
set -e
[[ ${heartbeat_status} -eq 124 ]] || {
    cat "${work_dir}/heartbeat.log" >&2
    echo "heartbeat emulation exited unexpectedly: ${heartbeat_status}" >&2
    exit 1
}
grep -q "ROCKPOD N25 RAMDIAG: PID1 started" "${work_dir}/heartbeat.log"
grep -q "ROCKPOD N25 RAMDIAG HEARTBEAT" "${work_dir}/heartbeat.log"

set +e
printf r | timeout 1 "${qemu_arm}" -strace -cpu arm926 \
    "${work_dir}/init" --emulate >"${work_dir}/reboot.log" 2>&1
reboot_status=$?
set -e
[[ ${reboot_status} -eq 124 ]] || {
    cat "${work_dir}/reboot.log" >&2
    echo "reboot emulation exited unexpectedly: ${reboot_status}" >&2
    exit 1
}
grep -q "reboot(-18751827,672274793,19088743" "${work_dir}/reboot.log"
grep -q "errno=1 (Operation not permitted)" "${work_dir}/reboot.log"

echo "PASS: ARM926 heartbeat and reboot syscall ABI emulation"
echo "This test does not emulate S5L8702 peripherals or access an iPod."
