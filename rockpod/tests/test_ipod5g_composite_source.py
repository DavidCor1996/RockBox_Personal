from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def _read(relative):
    return (REPO_ROOT / relative).read_text(encoding="utf-8", errors="replace")


def test_ipod5g_registers_persistent_composite_policy():
    settings_h = _read("apps/settings.h")
    settings_list = _read("apps/settings_list.c")
    settings = _read("apps/settings.c")
    display_menu = _read("apps/menus/display_menu.c")

    assert "defined(IPOD_6G) || defined(IPOD_VIDEO)" in settings_h
    assert "IPOD_COMPOSITE_VIDEO_AUTO" in settings_list
    assert "settings_apply_ipod_videoout" in settings_list
    assert "settings_apply_ipod_videoout(global_settings.composite_video_output)" in settings
    assert '#include "bcm2722.h"' in settings
    assert "bcm2722_videoout_set_mode(mode)" in settings
    assert display_menu.count(
        "(defined(IPOD_6G) || defined(IPOD_VIDEO)) && !defined(SIMULATOR)"
    ) >= 2


def test_ipod5g_resident_vmcs_mirror_matches_apple_diagnostics_protocol():
    driver = _read("firmware/target/arm/ipod/video/lcd-video.c")
    header = _read("firmware/export/bcm2722.h")
    mirror = driver.split(
        "static bool bcm_videoout_send_ntsc_frame(void)", 1
    )[1].split("static void bcm2722_videoout_mirror_if_due", 1)[0]
    policy = driver.split(
        "bool bcm2722_videoout_set_mode(int mode)", 1
    )[1].split("static bool bcm_player_write_addr", 1)[0]

    assert "bool bcm2722_videoout_set_mode(int mode);" in header
    assert "BCM_TV_NTSC_WIDTH          LCD_WIDTH" in driver
    assert "BCM_TV_NTSC_HEIGHT         LCD_HEIGHT" in driver
    assert "54u + BCM_TV_NTSC_IMAGE_BYTES + 2u" in driver
    assert "static const unsigned char bcm_tv_ntsc_header[54]" in driver
    assert mirror.index("bcm_videoout_write32(BCMA_COMMAND, command)") < mirror.index(
        "bcm_videoout_write_addr_ready(BCMA_TV_BMPDATA)"
    )
    assert "BCMCMD_TV_NTSCBMP" in mirror
    assert "BCMCMD_TV_MVOFF" not in mirror
    assert "bcm_videoout_read32(0x10002804, &route)" in mirror
    assert "route = (route | 0x40u) & ~0x80u" in mirror
    assert "bcm_videoout_write32(0x10002810, 0x00080000u)" in mirror
    assert "BCM_CONTROL = 0x31" in mirror
    assert "bcm_videoout_read32(BCMA_STATUS, &status)" in mirror
    assert "BCMA_TV_FB + BCM_TV_NTSC_ROW_BYTES" in mirror
    assert "bcm_videoout_write_live_rect" in mirror
    assert "bcm2722_videoout_signal_active = initialized" in driver
    assert "if (!bcm2722_videoout_signal_active)" in driver
    assert "bcm_tv_level_5" in driver
    assert "bcm_tv_level_6" in driver
    assert "BCM_TV_CACHE_FLUSH_LINES" in mirror
    assert "bcm_player_wait(&BCM_CONTROL, 0x2, 0x2" in driver
    assert "lcd_compose_overlay_row" in driver
    assert "BCM_TV_SAFE_X              16" in driver
    assert "BCM_TV_SAFE_Y              12" in driver
    assert "BCM_TV_SAFE_WIDTH         288" in driver
    assert "BCM_TV_SAFE_HEIGHT        216" in driver
    assert "bcm2722_videoout_xmap" in mirror
    assert "bcm2722_videoout_ymap" in mirror
    assert "lcd_write_data((const fb_data *)bcm2722_videoout_line" in mirror
    assert "cpu_boost(true);" in driver
    assert "cpu_boost(false);" in driver
    assert "720" not in mirror
    assert "BCM_TV_MIRROR_INTERVAL" not in driver
    assert "core_alloc" not in mirror
    assert "plugin_get_audio_buffer" not in mirror
    assert "bcm_powerdown();" in policy
    assert "lcd_awake();" in policy


def test_ipod5g_player_retains_lcd_and_mirrors_to_tv_at_launch():
    player = _read("apps/video_playback_5g.c")
    detector = player.split(
        "static uint8_t video5_detect_output_display(void)", 1
    )[1].split("static bool video5_set_region_for_display", 1)[0]
    region = player.split(
        "static bool video5_set_region(struct video5_player *player, bool fill)", 1
    )[1].split("static bool video5_enable_output", 1)[0]

    assert "IPOD_COMPOSITE_VIDEO_OFF" in detector
    assert "return VIDEO5_DISPLAY_LCD;" in detector
    assert "IPOD_COMPOSITE_VIDEO_ON" in detector
    assert "return VIDEO5_DISPLAY_TV;" in detector
    assert "GPIOA_INPUT_VAL & 0x10" in detector
    assert "video5_set_region_for_display(player, VIDEO5_DISPLAY_LCD" in region
    assert "video5_set_region_for_display(player, VIDEO5_DISPLAY_TV" in region


def test_ipod5g_ipodjs_quick_settings_exposes_composite_output():
    root_menu = _read("apps/root_menu.c")

    assert "(defined(IPOD_6G) || defined(IPOD_VIDEO))" in root_menu
    assert 'return "Composite Out";' in root_menu
    assert 'static const char * const states[] = {"Off", "Auto", "On"};' in root_menu
    assert "settings_apply_ipod_videoout(value);" in root_menu


def test_ipod5g_player_warm_loads_apples_retail_videocore():
    driver = _read("firmware/target/arm/ipod/video/lcd-video.c")
    bootstrap = driver.split(
        "static bool bcm_player_bootstrap_retail(void)", 1
    )[1].split("static bool bcm_player_wait_initial_upload_ready", 1)[0]
    upload_gate = driver.split(
        "static bool bcm_player_wait_initial_upload_ready(void)", 1
    )[1].split("static bool bcm_player_read_buffer_raw", 1)[0]
    loader = driver.split(
        "static bool bcm_player_start_retail_image", 1
    )[1].split("static void bcm_player_restore_lcd", 1)[0]
    recovery = driver.split(
        "static void bcm_player_restore_lcd(void)", 1
    )[1].split("bool bcm2722_video_start", 1)[0]
    start = driver.split(
        "bool bcm2722_video_start(const void *vmcs, size_t length)", 1
    )[1].split("void bcm2722_video_stop", 1)[0]
    stop = driver.split(
        "void bcm2722_video_stop", 1
    )[1].split("bool bcm2722_video_active", 1)[0]
    player = _read("apps/video_playback_5g.c")

    assert "BCM_CONTROL = bcm_bootstrapdata[i]" in bootstrap
    assert "BCM_ALT_CONTROL = bcm_bootstrapdata[i]" in bootstrap
    assert "(void)BCM_WR_ADDR" in bootstrap
    assert "(void)BCM_ALT_WR_ADDR" in bootstrap
    assert "bcm_player_write_buffer_raw(BCMA_SRAM_BASE, image, length)" in loader
    assert "lcd_write_data(image, length / sizeof(unsigned short))" not in loader
    assert "BCM2722_VIDEO_STAGE_VERIFY" in loader
    assert "bcm_player_verify_buffer_raw(BCMA_SRAM_BASE, image, length)" in loader
    verifier = driver.split(
        "static bool bcm_player_verify_buffer_raw", 1
    )[1].split("static bool bcm_player_start_retail_image", 1)[0]
    assert "bcm_player_read32(address + offset, &actual)" in verifier
    assert "Apple upload verify chunk 1 mismatch" in verifier
    assert "bcm_player_write32(BCMA_COMMAND, 0)" in loader
    assert "bcm_player_write32(0x10000C00, 0xC0000000)" in loader
    assert "bcm_player_write32(0x10000400, 0xA5A50002)" in loader
    assert "bcm_player_wait32_nonzero(BCMA_COMMAND" in loader
    assert "BCM_PLAYER_START_TIMEOUT" in loader
    assert "Apple start mailbox arm timed out" in loader
    assert "Apple VideoCore ready timed out" in loader
    assert loader.count("bcm_player_bootstrap_retail()") == 2
    assert loader.count("bcm_player_wait_initial_upload_ready()") == 1
    assert "bcm_player_wait(&BCM_ALT_CONTROL" not in bootstrap
    assert "bcm_player_wait(&BCM_ALT_CONTROL" in upload_gate
    assert "bcm_player_read_buffer_raw(0x1f0, runtime_header" in loader
    assert "runtime_ready != 1" in loader
    assert "service_base == 0 || (service_base & 3) != 0" in loader
    assert "BCM2722_VIDEO_STAGE_RUNTIME_BOOTSTRAP" in loader
    assert "bcm_player_cold_reset_retail" not in driver
    write_addr = driver.split(
        "static bool bcm_player_write_addr(unsigned address)", 1
    )[1].split("static bool bcm_player_write32", 1)[0]
    assert "BCM_WR_ADDR = address" in write_addr
    assert "BCM_WR_ADDR = address >> 16" in write_addr
    assert "BCM_WR_ADDR32 = address" not in write_addr
    assert "bcm_player_wait" not in write_addr
    write32 = driver.split(
        "static bool bcm_player_write32(unsigned address, unsigned value)", 1
    )[1].split("static bool bcm_player_write_halfwords_paced", 1)[0]
    assert "BCM_DATA = value" in write32
    assert "BCM_DATA = value >> 16" in write32
    assert "BCM_DATA32" not in write32
    dma = driver.split(
        "static bool bcm_player_dma_write(const void *buffer, size_t length)", 1
    )[1].split("static bool bcm_player_write_buffer_raw", 1)[0]
    raw_write = driver.split(
        "static bool bcm_player_write_buffer_raw(unsigned address, const void *buffer,", 1
    )[1].split("static bool bcm_player_read32", 1)[0]
    assert "0x60008000" in driver
    assert "0x60009000" in driver
    assert "BCM_DMA_RAM_CONFIG_32  0x22000000u" in driver
    assert "BCM_DMA_PER_CONFIG_32  0x26000000u" in driver
    assert "DMA_CMD_INTR | DMA_CMD_RAM_TO_PER" in driver
    assert "BCM_DMA_CHUNK_BYTES    0x10000u" in driver
    assert "CPU_INT_DIS = BCM_DMA_IRQ_MASK" in dma
    assert "BCM_DMA_PER_ADDR = (unsigned long)&BCM_DATA32" in dma
    assert "commit_dcache();" in dma
    assert "UNCACHED_ADDR(source)" in dma
    assert "while ((BCM_DMA_CMD & DMA_CMD_START) != 0)" in dma
    assert "status = BCM_DMA_STATUS" in dma
    assert "bulk = length & ~(BCM_DMA_BULK_ALIGN - 1)" in raw_write
    assert "bcm_player_dma_write(source, chunk)" in raw_write
    assert "bcm_player_write_halfwords_paced(source, length)" in raw_write
    read32 = driver.split(
        "static bool bcm_player_read32(unsigned address, unsigned *value)", 1
    )[1].split("static bool bcm_player_wait32", 1)[0]
    assert "BCM_RD_ADDR = address" in read32
    assert "BCM_RD_ADDR = address >> 16" in read32
    assert "(unsigned)BCM_DATA" in read32
    assert "BCM_RD_ADDR32" not in read32
    assert "BCM_DATA32" not in read32
    assert start.index("while (lcd_state.state != LCD_IDLE)") < start.index(
        "tick_remove_task(&lcd_tick)"
    ) < start.index("bcm_player_start_retail_image(vmcs, length)")
    assert start.index("lcd_state.display_on = false") > start.index(
        "while (lcd_state.state != LCD_IDLE)"
    )
    assert "BCM2722_VIDEO_STAGE_POWER_RESET" not in start
    assert "bcm_powerdown()" not in start
    assert "GPO32_VAL" not in start
    assert "bcm_player_restore_lcd()" in stop
    assert "_backlight_hw_enable(true)" in recovery
    assert "_backlight_led_on()" in recovery
    assert "bcm_player_services_ready" not in driver
    assert "bcm2722_video_start(vmcs, VIDEO5_VMCS_BYTES)" in player
    assert '#define VIDEO5_VMCS_BYTES       201376u' in player
    assert 'ROCKBOX_DIR "/videocore/vmcs.bin"' in player
    assert "bcm2722_video_error()" in player
    assert "bcm2722_video_faulted()" in player
    assert 'set_vll_dir /Resources/VideoCore' in player
    assert 'ROCKBOX_DIR "/video5-last-status.txt"' in player
    assert "backlight_on();" in player


def test_ipod5g_passthru_sample_matches_retailos_48_byte_contract():
    player = _read("apps/video_playback_5g.c")
    sample = player.split(
        "static bool video5_send_sample(struct video5_player *player,", 1
    )[1].split("static struct video5_track *video5_next_track", 1)[0]

    assert "uint8_t payload[48]" in sample
    assert "video5_put_u16(payload, track->audio ? 0 : 1)" in sample
    assert "video5_put_u16(payload + 2, 0)" in sample
    assert "video5_put_u32(payload + 4," in sample
    assert "video5_put_u32(payload + 8, track->sequence++)" in sample
    assert "video5_put_u32(payload + 12, 0)" in sample
    assert "video5_put_u32(payload + 16, bytes)" in sample
    assert "video5_put_u32(payload + 20, flag)" in sample
    assert "video5_put_u32(payload + 24" not in sample
    assert "player->slot_address[slot])" not in sample


def test_ipod5g_hostfs_fread_reply_matches_retailos():
    player = _read("apps/video_playback_5g.c")
    hostfs = player.split(
        "static bool video5_pump_hostfs(struct video5_player *player)", 1
    )[1].split("static bool video5_gencmd", 1)[0]

    assert "frame.length >= 16" in hostfs
    assert "memcpy(reply, frame.payload, 16)" in hostfs
    assert "(uint32_t)bytes / size" in hostfs
    assert "read(fd, reply + 16" in hostfs


def test_ipod5g_binds_and_acknowledges_retailos_host_notifications():
    player = _read("apps/video_playback_5g.c")
    discovery = player.split(
        "static bool video5_discover_services(struct video5_player *player)", 1
    )[1].split("static const char *video5_basename", 1)[0]
    hostreq = player.split(
        "static bool video5_pump_hostreq(struct video5_player *player)", 1
    )[1].split("static bool video5_gencmd", 1)[0]

    assert "#define VIDEO5_TAG_HOSTREQ      6" in player
    assert "video5_channel_from_tag(base, VIDEO5_TAG_HOSTREQ" in discovery
    assert "VIDEO5_HR_NOTIFY        0x47" in player
    assert "frame.opcode == VIDEO5_HR_NOTIFY" in hostreq
    assert "frame.opcode, NULL, 0" in hostreq
    assert "VIDEO5_HR_UNSUPPORTED   10" in player
    assert player.count("video5_pump_hostreq(player)") >= 3
    assert "video5_pump_hostreq(&player)" in player
