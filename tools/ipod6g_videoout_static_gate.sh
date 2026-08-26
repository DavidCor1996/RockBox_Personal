#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-${repo_root}/build-hw-ipod6g}"
sim_dir="${2:-${repo_root}/build-sim-ipod6g}"
boot_dir="${3:-/tmp/rockbox-ipod6g-bootloader-check}"
elf="${build_dir}/rockbox.elf"
zip="${build_dir}/rockbox.zip"
if [ ! -s "${zip}" ]; then
    zip="${build_dir}/RockPod-Setup-ipod6g.zip"
fi

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

    rg -q -U -- "${pattern}" <<<"${text}" || fail "missing ${description}"
}

[ -s "${elf}" ] || fail "missing ${elf}"
[ -s "${build_dir}/rockbox.ipod" ] || fail "missing rockbox.ipod"
[ -s "${zip}" ] || fail "missing Rockbox package zip"
rg -q '^Target: ipod6g$' "${build_dir}/rockbox-info.txt" ||
    fail "hardware build is not target ipod6g"
unzip -tq "${zip}" >/dev/null || fail "invalid Rockbox package zip"

symbols="$(arm-elf-eabi-nm -a "${elf}")"
for symbol in ipod6g_videoout_enable_sync ipod6g_videoout_show_background \
              ipod6g_videoout_show_framebuffer \
              ipod6g_videoout_test_config \
              ipod6g_videoout_test_xrgb \
              ipod6g_videoout_test_p420_pattern \
              ipod6g_videoout_test_p420_geometry_pattern \
              ipod6g_videoout_test_p420_framebuffer \
              ipod6g_videoout_test_p420_framebuffer_window \
              ipod6g_videoout_test_p420_framebuffer_geometry \
              ipod6g_videoout_refresh ipod6g_videoout_get_diagnostics \
              ipod6g_videoout_disable \
              ipod6g_videoout_active \
              ipod6g_videoout_lcd_clock_required \
              ipod6g_videoout_mirror_rgb565 lcd_videoout_clock_acquire \
              lcd_videoout_clock_release \
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
videoout_source="$(<"${repo_root}/firmware/target/arm/s5l8702/ipod6g/videoout-6g.c")"
require_text "${videoout_source}" \
    'SVID_MXR_SD_SCAN[[:space:]]+\(1u << 1\)' \
    "RetailOS mixer SD-scan bit"
require_text "${videoout_source}" \
    'SVID_MXR_CONFIG\) & ~4u\) \|[[:space:]]*\n[[:space:]]*SVID_MXR_SD_SCAN' \
    "RetailOS NTSC mixer configuration C=0x02 before layer enable"
require_text "${enable}" \
    'bic[[:space:]]+r[0-9]+, r[0-9]+, #6' \
    "compiled clear of the stale PAL and SD-scan bits"
require_text "${enable}" \
    'orr[[:space:]]+r[0-9]+, r[0-9]+, #2' \
    "compiled stock mixer SD-scan selection"
for gate in 14 15 16; do
    require_text "${enable}" "mov[[:space:]]+r0, #${gate}" \
        "enable clock gate ${gate}"
done
[ "$(rg -c '<clockgate_enable>' <<<"${enable}")" -eq 3 ] ||
    fail "enable does not call clockgate_enable exactly three times"

frame="$(arm-elf-eabi-objdump -d --disassemble=svid_show_framebuffer "${elf}")"
require_text "${frame}" '39200000' "router base"
require_text "${frame}" '<svid_copy_framebuffer_rgb565>' \
    "native RGB565 mixer framebuffer copy"
require_text "${frame}" '<svid_copy_framebuffer_xrgb>' \
    "XRGB mixer framebuffer copy"
require_text "${frame}" '#640' "640-pixel mixer width"
require_text "${frame}" '#480' "480-line mixer height"
rg -q 'SVID_MXR_GRAPHIC0_POS.*0x014u' \
    "${repo_root}/firmware/target/arm/s5l8702/ipod6g/videoout-6g.c" ||
    fail "graphics position is not mapped at mixer offset 0x14"
if rg -q 'SVID_MXR_GRAPHIC0_SPAN|SVID_MXR_NATIVE_' \
   "${repo_root}/firmware/target/arm/s5l8702/ipod6g/videoout-6g.c"; then
    fail "videoout still assigns later-Samsung span/format meanings"
fi

normal_frame="$(arm-elf-eabi-objdump -d \
    --disassemble=ipod6g_videoout_show_framebuffer "${elf}")"
require_text "${normal_frame}" '<svid_show_planar' \
    "RetailOS format-8 planar normal mirror path"
for geometry in \
    'SVID_UI_DESTINATION_X[[:space:]]+36u' \
    'SVID_UI_DESTINATION_Y[[:space:]]+24u' \
    'SVID_UI_DESTINATION_WIDTH[[:space:]]+648u' \
    'SVID_UI_DESTINATION_HEIGHT[[:space:]]+432u'; do
    require_text "${videoout_source}" "${geometry}" \
        "qualified centered 90-percent UI geometry"
done

copy_rgb565="$(arm-elf-eabi-objdump -d \
    --disassemble=svid_copy_framebuffer_rgb565 "${elf}")"
require_text "${copy_rgb565}" '<commit_dcache_range>' \
    "bounded 2x RGB565 framebuffer cache commit"
require_text "${copy_rgb565}" '#640' "RGB565 640-pixel destination"
require_text "${copy_rgb565}" '#480' "RGB565 480-line destination"

copy_frame="$(arm-elf-eabi-objdump -d \
    --disassemble=svid_copy_framebuffer_xrgb "${elf}")"
require_text "${copy_frame}" '<commit_dcache_range>' \
    "bounded XRGB framebuffer cache commit"
require_text "${copy_frame}" '<svid_rgb565_to_xrgb8888>' \
    "RGB565 to native mixer XRGB8888 conversion"
require_text "${copy_frame}" '(cmp|rsb|mov)[a-z]*[[:space:]].*#640' \
    "640-pixel NTSC active width"
require_text "${copy_frame}" '(cmp|rsb|mov)[a-z]*[[:space:]].*#480' \
    "480-line NTSC active height"

planar_symbol="$(awk '$3 ~ /^svid_show_planar\.part/ { print $3; exit }' \
    <<<"${symbols}")"
if [ -z "${planar_symbol}" ]; then
    planar_symbol="$(awk '$3 ~ /^svid_show_planar/ { print $3; exit }' \
        <<<"${symbols}")"
fi
[ -n "${planar_symbol}" ] || fail "missing format-8 planar worker"
planar="$(arm-elf-eabi-objdump -d --disassemble="${planar_symbol}" "${elf}")"
require_text "${planar}" '39100000' "Samsung VP base"
require_text "${planar}" '39200000' "output mixer base"
require_text "${planar}" '<svid_copy_framebuffer_planar>' \
    "native-frame RGB565 to private planar YUV420 conversion"
require_text "${planar}" '#76800' "complete stock 320x240 luma-plane size"
require_text "${planar}" '#19200' "complete stock 160x120 chroma-plane size"
require_text "${planar}" '#320' "stock planar luma span"
require_text "${planar}" '#160' "stock half-width planar chroma span"
require_text "${videoout_source}" \
    'y < SVID_PLANAR_Y_HEIGHT' \
    "bounded 240-line planar pattern loop"
require_text "${videoout_source}" \
    'SVID_PLANAR_Y_HEIGHT[[:space:]]+LCD_HEIGHT' \
    "stock 240-line format-8 luma plane"
require_text "${videoout_source}" \
    'SVID_PLANAR_Y_WIDTH[[:space:]]+LCD_WIDTH' \
    "stock 320-pixel format-8 luma plane"
require_text "${videoout_source}" \
    'SVID_PLANAR_C_HEIGHT[[:space:]]+\(SVID_PLANAR_Y_HEIGHT / 2\)' \
    "planar 4:2:0 half-height chroma planes"
require_text "${videoout_source}" \
    'SVID_VP_PLANE0_PTR\) = luma' "format-8 luma-plane assignment"
require_text "${videoout_source}" \
    'SVID_VP_PLANE2_PTR\) = cb' "qualified format-8 Cb-plane assignment"
require_text "${videoout_source}" \
    'SVID_VP_PLANE1_PTR\) = cr' "qualified format-8 Cr-plane assignment"
require_text "${videoout_source}" \
    'SVID_VP_UNUSED_PTR\) = 0' "RetailOS zero fourth descriptor"
require_text "${videoout_source}" \
    'SVID_VP_DST_V_POS\) = destination_y' \
    "RetailOS full-frame destination position"
require_text "${videoout_source}" \
    'SVID_VP_DST_HEIGHT\) =' "destination-height register write"
require_text "${videoout_source}" \
    'destination_height;' "RetailOS full-frame destination height"
if rg -q 'SVID_VP_DST_V_POS\) = destination_y / 2|destination_height / 2;' \
   <<<"${videoout_source}"; then
    fail "later-S5P field-line destination scaling remains"
fi
require_text "${videoout_source}" \
    '\(\(SVID_PLANAR_Y_HEIGHT << 12\) / destination_height\) >> 4' \
    "physical-row 8-bit fixed-point VP vertical ratio"
require_text "${videoout_source}" \
    'SVID_VP_IMG_HEIGHT\) =[[:space:]]*\n[[:space:]]*SVID_PLANAR_Y_HEIGHT' \
    "stock native 240-line image height"
require_text "${videoout_source}" \
    '\(\(SVID_PLANAR_Y_WIDTH << 12\) / destination_width\) >> 3' \
    "exact stock 9-bit fixed-point VP horizontal ratio"
require_text "${videoout_source}" \
    'SVID_VP_SRC_HEIGHT\) =[[:space:]]*\n[[:space:]]*source_register_height' \
    "independent interlaced source-height register"
require_text "${videoout_source}" \
    'SVID_PLANAR_SOURCE_HEIGHT[[:space:]]+SVID_PLANAR_Y_HEIGHT' \
    "stock 240-line private format-8 source register count"
require_text "$(<"${repo_root}/firmware/target/arm/s5l8702/ipod6g/videoout-6g.c")" \
    'SVID_NTSC_OUTPUT_WIDTH[[:space:]]+720u' \
    "stock 720-pixel NTSC destination width"
require_text "$(<"${repo_root}/firmware/target/arm/s5l8702/ipod6g/videoout-6g.c")" \
    'cpu_boost\(enable\)' "balanced VP bus-clock boost owner"
for literal in SVID_VP_IMG_WIDTH SVID_VP_IMG_HEIGHT \
               SVID_VP_PLANE0_PTR SVID_VP_PLANE1_PTR \
               SVID_VP_PLANE2_PTR SVID_VP_UNUSED_PTR \
               SVID_VP_SRC_WIDTH SVID_VP_DST_WIDTH \
               SVID_VP_MODE SVID_VP_PLANE_MODE \
               SVID_VP_LUMA_SPAN SVID_VP_CHROMA_SPAN \
               SVID_VP_ENDIAN_MODE; do
    require_text "$(<"${repo_root}/firmware/target/arm/s5l8702/ipod6g/videoout-6g.c")" \
        "#define ${literal}" "VP register ${literal}"
done
require_text "$(<"${repo_root}/firmware/target/arm/s5l8702/ipod6g/videoout-6g.c")" \
    'SVID_MXR_VIDEO_ENABLE' "mixer VP enable"
if rg -q 'SVID_VP_SHADOW_UPDATE' \
   "${repo_root}/firmware/target/arm/s5l8702/ipod6g/videoout-6g.c"; then
    fail "format-8 path still issues the non-RetailOS VP shadow update"
fi

copy_planar="$(arm-elf-eabi-objdump -d \
    --disassemble=svid_copy_framebuffer_planar "${elf}")"
require_text "${copy_planar}" '<svid_rgb565_to_ycbcr>' \
    "BT.601 RGB565 to YCbCr conversion"
require_text "${copy_planar}" '<commit_dcache_range>' \
    "bounded planar YUV420 framebuffer cache commit"
require_text "${copy_planar}" '#76800' "complete stock 320x240 luma plane"
require_text "${copy_planar}" '#19200' "complete stock 160x120 chroma plane"
require_text "${copy_planar}" '0001c200' "complete stock 320x240 YUV420 frame"
require_text "${videoout_source}" \
    'uint8_t \*luma0 = luma \+ y \* SVID_PLANAR_Y_WIDTH' \
    "one physical luma row for every Rockbox framebuffer row"
require_text "${videoout_source}" \
    'luma0\[x\] = pixel_y' \
    "native one-to-one luma packing"
require_text "${videoout_source}" \
    'cb0\[x / 2\] = \(cb00 \+ cb01 \+ cb10 \+ cb11 \+ 2\) / 4' \
    "2x2 averaged stock 4:2:0 Cb sample"
require_text "${videoout_source}" \
    'cr0\[x / 2\] = \(cr00 \+ cr01 \+ cr10 \+ cr11 \+ 2\) / 4' \
    "2x2 averaged stock 4:2:0 Cr sample"
require_text "${videoout_source}" \
    'SVID_VP_MODE_RETAIL_FMT8[[:space:]]+0u' \
    "RetailOS format-8 reset mode"
require_text "${videoout_source}" \
    'SVID_VP_PLANE_PLANAR[[:space:]]+1u' \
    "RetailOS format-8 planar mode"
if rg -q 'SVID_VP_(TOP_Y|BOTTOM_Y|BOT_Y|TOP_C|BOTTOM_C|BOT_C)_PTR|SVID_VP_MODE_(LINE_SKIP|FIELD_AUTO|P_TO_I)|SVID_PLANAR_CHROMA_HEIGHT|uint8_t \*chroma[[:alnum:]_]* =' \
   <<<"${videoout_source}"; then
    fail "rejected NV12 field-pointer or interleaved-chroma path remains"
fi
if rg -q 'SVID_UI_VERTICAL_RATIO|source_height / 2|destination_height / 2|uint8_t \*luma1' \
   <<<"${videoout_source}"; then
    fail "fake half-height, doubled-luma, or fixed vertical-ratio path remains"
fi

refresh="$(arm-elf-eabi-objdump -d \
    --disassemble=ipod6g_videoout_refresh "${elf}")"
require_text "${refresh}" '<svid_copy_framebuffer_rgb565>' \
    "RGB565 refresh and bounded cache commit"
require_text "${refresh}" '<svid_copy_framebuffer_xrgb>' \
    "XRGB refresh and bounded cache commit"
require_text "${refresh}" '<svid_copy_framebuffer_planar>' \
    "private planar refresh and bounded cache commit"

yuv="$(arm-elf-eabi-objdump -d --disassemble=lcd_blit_yuv "${elf}")"
require_text "${yuv}" 'ipod6g_videoout_mirror_rgb565' "YUV RGB565 mirror call"
require_text "${yuv}" 'displaylcd_dma' "YUV display DMA submission"
lcd_update="$(arm-elf-eabi-objdump -d --disassemble=lcd_update_rect "${elf}")"
require_text "${lcd_update}" 'ipod6g_videoout_mirror_rgb565' \
    "ordinary LCD update mirror call"
mirror="$(arm-elf-eabi-objdump -d \
    --disassemble=ipod6g_videoout_mirror_rgb565 "${elf}")"
require_text "${mirror}" '<svid_rgb565_to_ycbcr>' \
    "private planar dirty-band conversion"
require_text "${mirror}" '<commit_dcache_range>' \
    "external-only private planar dirty-band cache commit"
display_dma="$(arm-elf-eabi-objdump -d --disassemble=displaylcd_dma "${elf}")"
require_text "${display_dma}" 'commit_dcache' "shared LCD/SVID cache commit"

disable="$(arm-elf-eabi-objdump -d \
    --disassemble=ipod6g_videoout_disable "${elf}")"
for literal in 39100000 39200000 39300000; do
    require_text "${disable}" "${literal}" "disable literal 0x${literal}"
done
[ "$(rg -c '<svid_stop_block>' <<<"${disable}")" -eq 3 ] ||
    fail "disable does not stop the SDO, mixer, and VP blocks"
require_text "${disable}" '<svid_platform_restore' \
    "saved clock, power-gate, and GPIO restoration"
require_text "${disable}" '<lcd_videoout_clock_release>' \
    "shared LCD/output-mixer gate release"

lcd_source="${repo_root}/firmware/target/arm/s5l8702/lcd-s5l8702.c"
require_text "$(<"${lcd_source}")" \
    'ipod6g_videoout_lcd_clock_required' \
    "shared LCD/output-mixer gate retention while SVID is active"
require_text "$(<"${lcd_source}")" \
    'if \(ipod6g_videoout_lcd_clock_required\(\)\)' \
    "panel-sleep suppression while SVID is active"

apply="$(arm-elf-eabi-objdump -d \
    --disassemble=settings_apply_ipod6g_videoout "${elf}")"
require_text "${apply}" '<ipod6g_videoout_set_mode>' \
    "persistent composite-output setting policy"
settings_source="$(<"${repo_root}/apps/settings.c")"
require_text "${settings_source}" \
    'global_settings\.composite_video_output = mode' \
    "validated mode persistence"
require_text "${settings_source}" \
    'ipod6g_videoout_set_mode\(\(enum ipod6g_videoout_mode\)mode' \
    "validated mode forwarding"

quick_settings_source="$(<"${repo_root}/apps/root_menu.c")"
require_text "${quick_settings_source}" \
    'Select to turn composite output on or off' \
    "Quick Settings on/off help"
require_text "${quick_settings_source}" \
    'IPOD6G_VIDEOOUT_ON \? IPOD6G_VIDEOOUT_OFF' \
    "Quick Settings on/off toggle"
require_text "${quick_settings_source}" \
    'settings_apply_ipod6g_videoout\(value\)' \
    "Quick Settings immediate apply"

debug_source="$(<"${repo_root}/apps/debug_menu.c")"
if rg -q 'DBG_VIDEOOUT_STAGE_TIMEOUT|stage_deadline|timed_out' \
   <<<"${debug_source}"; then
    fail "composite qualification still has an automatic shutdown"
fi
require_text "${debug_source}" 'Stage 3: fmt8 bars' \
    "RetailOS format-8 bar qualification stage"
require_text "${debug_source}" 'Stage 4: fmt8 grid' \
    "stock-ratio full-height bordered grid stage"
require_text "${debug_source}" 'Stage 5: fmt8 live' \
    "stock-format live framebuffer stage"
if rg -q 'grid V(512|544|568|592)|live V568' <<<"${debug_source}"; then
    fail "obsolete ratio-sweep stages still mask the format-8 layout"
fi
require_text "${debug_source}" 'ipod6g_videoout_test_p420_pattern' \
    "stock-width planar 4:2:0 pattern call"
require_text "${debug_source}" 'ipod6g_videoout_test_p420_geometry_pattern' \
    "native-height bordered stock-ratio grid"
require_text "${debug_source}" \
    '36, 24, 648, 432, 240, 142' \
    "qualified viewport with exact stock source geometry"
require_text "${debug_source}" 'Stock 320x240 H252 V142' \
    "diagnostic label for exact stock source geometry"
if rg -q 'IMG480 SRC(240|480)|36, 24, 648, 432, 480, 142' \
   <<<"${debug_source}"; then
    fail "rejected 640-wide/interlaced-height diagnostic remains"
fi
require_text "${debug_source}" \
    'ipod6g_videoout_test_p420_framebuffer_geometry' \
    "static/live source-geometry isolation calls"
require_text "${debug_source}" 'dbg_videoout_poll_registers' \
    "live critical-register change capture"
require_text "${debug_source}" 'backlight_set_timeout\(0\)' \
    "idle-timeout isolation during composite qualification"
require_text "${debug_source}" \
    'backlight_set_timeout\(global_settings.backlight_timeout\)' \
    "backlight timeout restoration after composite qualification"
require_text "${debug_source}" 'saved_videoout_mode' \
    "persistent output suspension and restoration around diagnostics"

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
