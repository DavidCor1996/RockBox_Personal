#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-${repo_root}/build-hw-ipod6g}"
sim_dir="${2:-${repo_root}/build-sim-ipod6g}"
boot_dir="${3:-/tmp/rockbox-ipod6g-bootloader-check}"
elf="${build_dir}/rockbox.elf"

fail()
{
    echo "videoout gate: $*" >&2
    exit 1
}

require_text()
{
    local text="$1"
    local pattern="$2"
    local description="$3"

    rg -q -- "${pattern}" <<<"${text}" || fail "missing ${description}"
}

[ -s "${elf}" ] || fail "missing ${elf}"
[ -s "${build_dir}/rockbox.ipod" ] || fail "missing rockbox.ipod"
[ -s "${build_dir}/rockbox.zip" ] || fail "missing rockbox.zip"
rg -q '^Target: ipod6g$' "${build_dir}/rockbox-info.txt" ||
    fail "hardware build is not target ipod6g"
unzip -tq "${build_dir}/rockbox.zip" >/dev/null || fail "invalid rockbox.zip"

symbols="$(arm-elf-eabi-nm -a "${elf}")"
for symbol in ipod6g_videoout_enable_sync ipod6g_videoout_show_framebuffer \
              ipod6g_videoout_refresh ipod6g_videoout_disable \
              ipod6g_videoout_active ipod6g_videoout_mirror_rgb565 \
              ipod6g_videoout_set_mode ipod6g_videoout_accessory_state \
              settings_apply_ipod6g_videoout power_off system_reboot; do
    require_text "${symbols}" "[[:space:]]${symbol}$" "symbol ${symbol}"
done

image_strings="$(strings "${elf}")"
require_text "${image_strings}" '^Composite Video Output$' "menu label"
require_text "${image_strings}" '^composite video output$' "config key"

enable="$(arm-elf-eabi-objdump -d \
    --disassemble=ipod6g_videoout_enable_sync "${elf}")"
for literal in 39100000 39200000 39300000 000a040f; do
    require_text "${enable}" "${literal}" "enable literal 0x${literal}"
done
if rg -q '2200200c' "${repo_root}/firmware/target/arm/s5l8702/ipod6g/videoout-6g.c"; then
    fail "videoout still jumps into an assumed stock-firmware clock routine"
fi
require_text "${symbols}" '[[:space:]]svid_write_clock$' \
    "bounded local SVID clock writer"
for gate in 14 15 16; do
    require_text "${enable}" "mov[[:space:]]+r0, #${gate}" \
        "enable clock gate ${gate}"
done
[ "$(rg -c '<clockgate_enable>' <<<"${enable}")" -eq 3 ] ||
    fail "enable does not call clockgate_enable exactly three times"

frame="$(arm-elf-eabi-objdump -d \
    --disassemble=ipod6g_videoout_show_framebuffer "${elf}")"
require_text "${frame}" '<commit_dcache_range>' "bounded framebuffer cache commit"
require_text "${frame}" 'mov[[:space:]]+r3, #640' "640-pixel output width"
require_text "${frame}" 'mov[[:space:]]+r3, #480' "480-line output height"
require_text "${frame}" '028001e0' "640x480 router geometry"
require_text "${frame}" '39200000' "router base"

refresh="$(arm-elf-eabi-objdump -d \
    --disassemble=ipod6g_videoout_refresh "${elf}")"
require_text "${refresh}" '<commit_dcache_range>' "bounded refresh cache commit"
require_text "${refresh}" '39200000' "refresh router base"
require_text "${refresh}" '39100000' "refresh compositor latch"

yuv="$(arm-elf-eabi-objdump -d --disassemble=lcd_blit_yuv "${elf}")"
require_text "${yuv}" 'ipod6g_videoout_mirror_rgb565' "YUV RGB565 mirror call"
require_text "${yuv}" 'displaylcd_dma' "YUV display DMA submission"
mirror="$(arm-elf-eabi-objdump -d \
    --disassemble=ipod6g_videoout_mirror_rgb565 "${elf}")"
require_text "${mirror}" '<commit_dcache_range>' \
    "external-only dirty-band cache commit"
display_dma="$(arm-elf-eabi-objdump -d --disassemble=displaylcd_dma "${elf}")"
require_text "${display_dma}" 'commit_dcache' "shared LCD/SVID cache commit"

disable="$(arm-elf-eabi-objdump -d \
    --disassemble=ipod6g_videoout_disable "${elf}")"
for literal in 39100000 39200000 39300000; do
    require_text "${disable}" "${literal}" "disable literal 0x${literal}"
done
[ "$(rg -c '<svid_stop_block>' <<<"${disable}")" -eq 3 ] ||
    fail "disable does not stop all three SVID blocks"
require_text "${disable}" '<svid_platform_restore' \
    "saved clock, power-gate, and GPIO restoration"

apply="$(arm-elf-eabi-objdump -d \
    --disassemble=settings_apply_ipod6g_videoout "${elf}")"
require_text "${apply}" '<ipod6g_videoout_set_mode>' "fail-closed setting policy"
require_text "${apply}" 'mov[[:space:]]+r[0-9]+, #0' \
    "zero-valued setting mode"
require_text "${apply}" 'mov[[:space:]]+r0, r[0-9]+' \
    "forced-off video-output argument"

quick_settings_source="$(<"${repo_root}/apps/root_menu.c")"
require_text "${quick_settings_source}" 'Debug only' \
    "debug-only Quick Settings state"
require_text "${quick_settings_source}" 'Hardware test: System > Debug' \
    "Quick Settings diagnostic route"
if rg -q 'settings_apply_ipod6g_videoout\(value\)' \
   "${repo_root}/apps/root_menu.c"; then
    fail "Quick Settings can still apply an arbitrary video-output mode"
fi

debug_source="$(<"${repo_root}/apps/debug_menu.c")"
require_text "${debug_source}" 'DBG_VIDEOOUT_STAGE_TIMEOUT \(60 \* HZ\)' \
    "60-second diagnostic-stage timeout"
require_text "${debug_source}" 'TIME_AFTER\(current_tick, stage_deadline\)' \
    "diagnostic timeout enforcement"
require_text "${debug_source}" 'Composite test timed out; output off' \
    "diagnostic timeout confirmation"

serial_tick="$(arm-elf-eabi-objdump -d \
    --disassemble=serial_acc_tick "${elf}")"
require_text "${serial_tick}" '<iap_kokkia_present>' "Kokkia classification"
require_text "${serial_tick}" '<ipod6g_videoout_accessory_state>' \
    "classified accessory publication"

for owner in power_off system_reboot; do
    owner_dump="$(arm-elf-eabi-objdump -d --disassemble="${owner}" "${elf}")"
    require_text "${owner_dump}" '<ipod6g_videoout_disable>' "${owner} cleanup"
done

if [ -s "${sim_dir}/rockboxui" ] &&
   rg -q 'ipod6g_videoout_' <<<"$(nm "${sim_dir}/rockboxui")"; then
    fail "hardware videoout leaked into simulator"
fi
if [ -s "${boot_dir}/bootloader.elf" ] &&
   rg -q 'ipod6g_videoout_' <<<"$(arm-elf-eabi-nm "${boot_dir}/bootloader.elf")"; then
    fail "hardware videoout leaked into bootloader"
fi

sha256sum "${build_dir}/rockbox.ipod"
echo "videoout gate: all static checks passed"
