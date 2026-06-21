"""Static checks for Rockboy profiling detail instrumentation."""

from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def _read(rel_path):
    return (REPO_ROOT / rel_path).read_text(encoding="utf-8", errors="replace")


def test_profile_log_includes_phase1_detail_counters():
    profiler = _read("apps/plugins/rockboy/profiler.c")

    for field in (
        "cpu_ops=%lu",
        "slow_mem_reads=%lu",
        "slow_mem_writes=%lu",
        "vram_dirty_writes=%lu",
        "lcd_lines=%lu",
        "dmg_bg_only_eligible=%lu",
        "dmg_bg_only_used=%lu",
        "dmg_bg_only_rejected=%lu",
        "dmg_bg_window_no_spr_used=%lu",
        "dmg_bg_window_no_spr_rejected=%lu",
        "cgb_bg_only_eligible=%lu",
        "cgb_bg_only_used=%lu",
        "cgb_bg_only_rejected=%lu",
        "cgb_bg_window_no_spr_eligible=%lu",
        "cgb_bg_window_no_spr_used=%lu",
        "cgb_bg_window_no_spr_rejected=%lu",
        "cgb_no_sprite_lines=%lu",
    ):
        assert field in profiler


def test_hot_paths_increment_profile_detail_counters():
    cpu = _read("apps/plugins/rockboy/cpu.c")
    fastmem = _read("apps/plugins/rockboy/fastmem.c")
    lcd = _read("apps/plugins/rockboy/lcd.c")

    assert "ROCKBOY_EVENT_CPU_OPS" in cpu
    assert "ROCKBOY_EVENT_SLOW_MEM_READS" in fastmem
    assert "ROCKBOY_EVENT_SLOW_MEM_WRITES" in fastmem
    assert "ROCKBOY_EVENT_VRAM_DIRTY_WRITES" in lcd
    assert "ROCKBOY_EVENT_LCD_DMG_BG_ONLY_ELIGIBLE" in lcd
    assert "CPU_MEMORY_ACCESS()" in fastmem
    assert "cpu_mem_access();" in fastmem


def test_simulator_profile_autowrite_is_opt_in_and_target_gated():
    profiler = _read("apps/plugins/rockboy/profiler.c")
    rockboy = _read("apps/plugins/rockboy/rockboy.c")

    assert "ROCKBOY_PROFILE_AUTOWRITE_FRAMES" in profiler
    assert "#ifdef SIMULATOR" in profiler
    assert "rockboy_profile_start(rom_path)" in rockboy


def test_simulator_serial_log_is_opt_in_and_target_gated():
    mem = _read("apps/plugins/rockboy/mem.c")

    assert "ROCKBOY_SERIAL_LOG" in mem
    assert "#ifdef SIMULATOR" in mem
    assert "case RI_SB:" in mem
    assert "rockboy_serial_log_byte(R_SB);" in mem
    assert 'snprintf(path, sizeof(path), "%s/serial.log", savedir);' in mem


def test_profile_hot_path_calls_are_inline_guarded():
    profiler_h = _read("apps/plugins/rockboy/profiler.h")
    profiler_c = _read("apps/plugins/rockboy/profiler.c")

    assert "#define rockboy_profile_count(which, count)" in profiler_h
    assert "if (rockboy_profile_is_enabled())" in profiler_h
    assert "rockboy_profile_count_enabled" in profiler_h
    assert "void rockboy_profile_count_enabled" in profiler_c


def test_no_sprite_lines_skip_sprite_scan_call():
    lcd = _read("apps/plugins/rockboy/lcd.c")

    assert "if (NS)\n        spr_scan();" in lcd


def test_dmg_background_only_fast_path_is_guarded():
    lcd = _read("apps/plugins/rockboy/lcd.c")

    assert "static void dmg_bg_only_scan(void)" in lcd
    assert "static void dmg_bg_window_no_spr_scan(void)" in lcd
    assert "static void cgb_bg_only_scan(void)" in lcd
    assert "static void cgb_bg_window_no_spr_scan(void)" in lcd
    assert "dmg_bg_only_eligible = !hw.cgb && !NS && WX == 160;" in lcd
    assert "dmg_bg_window_no_spr_eligible = !hw.cgb && !NS && WX >= 0 && WX < 160;" in lcd
    assert "cgb_bg_only_eligible = hw.cgb && !NS && WX == 160;" in lcd
    assert "cgb_bg_window_no_spr_eligible = hw.cgb && !NS && WX >= 0 && WX < 160;" in lcd
    assert "fast_line_rendering_enabled()" in lcd
    assert "ROCKBOY_PERF_QUALITY" in lcd
    assert "ROCKBOY_EVENT_LCD_DMG_BG_ONLY_USED" in lcd
    assert "ROCKBOY_EVENT_LCD_DMG_BG_ONLY_REJECTED" in lcd
    assert "ROCKBOY_EVENT_LCD_DMG_BG_WINDOW_NO_SPR_USED" in lcd
    assert "ROCKBOY_EVENT_LCD_DMG_BG_WINDOW_NO_SPR_REJECTED" in lcd
    assert "ROCKBOY_EVENT_LCD_CGB_BG_ONLY_USED" in lcd
    assert "ROCKBOY_EVENT_LCD_CGB_BG_ONLY_REJECTED" in lcd
    assert "ROCKBOY_EVENT_LCD_CGB_BG_WINDOW_NO_SPR_USED" in lcd
    assert "ROCKBOY_EVENT_LCD_CGB_BG_WINDOW_NO_SPR_REJECTED" in lcd


def test_cpu_interpreter_scratch_state_stays_local():
    cpu = _read("apps/plugins/rockboy/cpu.c")

    assert "byte op;" in cpu
    assert "byte cbop;" in cpu
    assert "union reg acc;" in cpu
    assert "static byte op IBSS_ATTR" not in cpu
    assert "static union reg acc IBSS_ATTR" not in cpu


def test_pop_af_masks_unused_flag_bits():
    cpu = _read("apps/plugins/rockboy/cpu.c")

    assert "F=LB(acc)&0xF0;" in cpu
    assert "F &= 0xF0;" in cpu


def test_daa_uses_explicit_gameboy_flag_math():
    cpu = _read("apps/plugins/rockboy/cpu.c")

    assert "if ((F & FH) || ((A & 0x0F) > 0x09)) b |= 0x06;" in cpu
    assert "if ((F & FC) || (A > 0x99)) { b |= 0x60; w = FC; }" in cpu
    assert "F = (F & FN) | ZFLAG(A) | w;" in cpu


def test_halt_idles_without_requiring_ime():
    cpu = _read("apps/plugins/rockboy/cpu.c")

    assert "if (!cpu.halt) return 0;" in cpu
    assert "if (!(cpu.halt && IME)) return 0;" not in cpu


def test_dmg_halt_bug_skips_next_pc_increment():
    cpu = _read("apps/plugins/rockboy/cpu.c")
    cpu_h = _read("apps/plugins/rockboy/cpu-gb.h")

    assert "unsigned int halt_bug;" in cpu_h
    assert "static byte cpu_fetch_byte(void)" in cpu
    assert "if (cpu.halt_bug)" in cpu
    assert "cpu.halt_bug = 1;" in cpu
    assert "if (!IME && (IF & IE))" in cpu


def test_sp_relative_add_uses_low_byte_flag_math():
    cpu = _read("apps/plugins/rockboy/cpu.c")

    assert "((SP & 0x0F) + ((n) & 0x0F) > 0x0F) ? FH : 0" in cpu
    assert "((SP & 0xFF) + (n) > 0xFF) ? FC : 0" in cpu


def test_cpu_memory_access_timing_splits_instruction_cycles():
    cpu = _read("apps/plugins/rockboy/cpu.c")
    cpu_h = _read("apps/plugins/rockboy/cpu-gb.h")

    assert "void cpu_mem_access(void)" in cpu
    assert "cpu_start_instruction_timing(op == 0xCB ? 2 : clen);" in cpu
    assert "cpu_finish_instruction_timing(clen);" in cpu
    assert "cpu.mem_access_total = clen;" in cpu
    assert "int mem_access_active;" in cpu_h


def test_interrupt_entry_charges_five_machine_cycles():
    cpu = _read("apps/plugins/rockboy/cpu.c")

    assert "static int cpu_interrupt_entry_timing(void)" in cpu
    assert "int cycles = 5;" in cpu
    assert "i -= cpu_interrupt_entry_timing();" in cpu


def test_if_register_reads_with_unused_bits_set():
    mem = _read("apps/plugins/rockboy/mem.c")

    assert "case RI_IF:" in mem
    assert "return REG(r) | 0xE0;" in mem


def test_lcd_enable_first_line_matches_blargg_sync_boundary():
    lcdc = _read("apps/plugins/rockboy/lcdc.c")

    assert "C = 38;" in lcdc


def test_sound_register_read_masks_and_power_off_are_modelled():
    sound = _read("apps/plugins/rockboy/sound.c")

    assert "static const byte sound_read_mask[0x30]" in sound
    assert "return REG(r) | sound_read_mask[r - RI_NR10];" in sound
    assert "static void sound_power_off(void)" in sound
    assert "for (r = RI_NR10; r <= RI_NR51; r++)" in sound
    assert "if (!(R_NR52 & 0x80))" in sound


def test_sound_trigger_preserves_nonzero_length_counter():
    sound = _read("apps/plugins/rockboy/sound.c")

    assert "#define SOUND_LENGTH_UNIT 172" in sound
    assert "if (S1.len <= 0)\n                S1.len = SOUND_LENGTH_UNIT * 64;" in sound
    assert "if (S2.len <= 0)\n                S2.len = SOUND_LENGTH_UNIT * 64;" in sound
    assert "if (S3.len <= 0)\n                S3.len = SOUND_LENGTH_UNIT * 256;" in sound
    assert "if (S4.len <= 0)\n                S4.len = SOUND_LENGTH_UNIT * 64;" in sound


def test_sound_length_clocks_only_when_enabled():
    sound = _read("apps/plugins/rockboy/sound.c")

    assert "if(S1.cont && S1.len > 0)" in sound
    assert "if(S2.cont && S2.len > 0)" in sound
    assert "if(S3.cont && S3.len > 0)" in sound
    assert "if(S4.cont && S4.len > 0)" in sound
    assert "R_NR52 &= 0xf7;" in sound


def test_sound_dac_off_clears_channel_and_blocks_trigger():
    sound = _read("apps/plugins/rockboy/sound.c")

    assert "static bool sound_env_dac_enabled(byte b)" in sound
    assert "return b & 0xf8;" in sound
    assert "if (!sound_env_dac_enabled(b))" in sound
    assert "if (sound_env_dac_enabled(R_NR12))" in sound
    assert "if (sound_env_dac_enabled(R_NR22))" in sound
    assert "if (sound_env_dac_enabled(R_NR42))" in sound
