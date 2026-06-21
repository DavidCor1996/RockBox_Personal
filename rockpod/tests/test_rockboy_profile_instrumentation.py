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
    assert "S1.cont = S2.cont = S3.cont = S4.cont = 0;" in sound
    assert "snd.length_phase = 0;" in sound
    assert "snd.frame_step = 0;" in sound
    assert "snd.wave_access = 0;" in sound
    assert "if (!(R_NR52 & 0x80))" in sound


def test_sound_trigger_preserves_nonzero_length_counter():
    sound = _read("apps/plugins/rockboy/sound.c")

    assert "#define SOUND_LENGTH_UNIT 1" in sound
    assert "#define SOUND_LENGTH_CLOCK 4096" in sound
    assert "if (S1.len <= 0)" in sound
    assert "S1.len = SOUND_LENGTH_UNIT * 64;" in sound
    assert "if (S2.len <= 0)" in sound
    assert "S2.len = SOUND_LENGTH_UNIT * 64;" in sound
    assert "if (S3.len <= 0)" in sound
    assert "S3.len = SOUND_LENGTH_UNIT * 256;" in sound
    assert "if (S4.len <= 0)" in sound
    assert "S4.len = SOUND_LENGTH_UNIT * 64;" in sound
    assert "suppress_enable_clock = !(b & 0x40) && !(old & 0x40)" in sound


def test_sound_length_clocks_only_when_enabled():
    sound = _read("apps/plugins/rockboy/sound.c")

    assert "static void sound_clock_lengths(void)" in sound
    assert "if (S1.cont)" in sound
    assert "if (S2.cont)" in sound
    assert "if (S3.cont)" in sound
    assert "if (S4.cont)" in sound
    assert "R_NR52 &= 0xf7;" in sound


def test_sound_dac_off_clears_channel_and_blocks_trigger():
    sound = _read("apps/plugins/rockboy/sound.c")

    assert "static bool sound_env_dac_enabled(byte b)" in sound
    assert "return b & 0xf8;" in sound
    assert "if (!sound_env_dac_enabled(b))" in sound
    assert "if (sound_env_dac_enabled(R_NR12))" in sound
    assert "if (sound_env_dac_enabled(R_NR22))" in sound
    assert "if (sound_env_dac_enabled(R_NR42))" in sound


def test_sound_frame_phase_drives_length_enable_extra_clock():
    sound = _read("apps/plugins/rockboy/sound.c")
    sound_h = _read("apps/plugins/rockboy/sound.h")
    save = _read("apps/plugins/rockboy/save.c")

    assert "int length_phase;" in sound_h
    assert "int frame_step;" in sound_h
    assert "suppress_enable_clock" in sound_h
    assert "static bool sound_length_extra_clock_phase(void)" in sound
    assert "return snd.frame_step & 1;" in sound
    assert "!ch->suppress_enable_clock && sound_length_extra_clock_phase()" in sound
    assert "ch->suppress_enable_clock = 0;" in sound
    assert "suppress_enable_clock = !(b & 0x40) && !(old & 0x40)" in sound
    assert "static void sound_advance_frame_phase(int quality)" in sound
    assert "while (snd.length_phase >= SOUND_LENGTH_CLOCK)" in sound
    assert "sound_clock_lengths();" in sound
    assert "if (R_NR52 & 0x80)" in sound
    assert "snd.length_phase = 0;" in sound
    assert "sound_advance_frame_phase(cnt);" in sound
    assert "sound_apply_length_enable_write(&S1, old, b, 0xfe);" in sound
    assert "sound_apply_length_enable_write(&S2, old, b, 0xfd);" in sound
    assert "sound_apply_length_enable_write(&S3, old, b, 0xfb);" in sound
    assert "sound_apply_length_enable_write(&S4, old, b, 0xf7);" in sound
    assert "bool reloaded_length;" in sound
    assert "static bool sound_trigger_extra_clock_allowed(byte old, byte b)" in sound
    assert "sound_trigger_extra_clock_allowed(old, b)" in sound
    assert 'I4("SPh ", &snd.length_phase)' in save
    assert 'I4("SFs ", &snd.frame_step)' in save
    assert 'I4("S1se", &snd.ch[0].suppress_enable_clock)' in save
    assert 'I4("S4se", &snd.ch[3].suppress_enable_clock)' in save


def test_sound_sweep_uses_frame_clocked_shadow_model():
    sound = _read("apps/plugins/rockboy/sound.c")
    sound_h = _read("apps/plugins/rockboy/sound.h")
    save = _read("apps/plugins/rockboy/save.c")

    assert "swshadow" in sound_h
    assert "swenabled" in sound_h
    assert "swneg_used" in sound_h
    assert "static void sound_sweep_clock(void)" in sound
    assert "if (!(snd.frame_step & 1))" in sound
    assert "sound_sweep_clock();" in sound
    assert "S1.swshadow = ((int)(b & 7) << 8) | R_NR13;" in sound
    assert "S1.swenabled = ((R_NR10 >> 4) & 7) || (R_NR10 & 7);" in sound
    assert "if (S1.swneg_used && (old & 0x08) && !(b & 0x08))" in sound
    assert 'I4("S1sh", &snd.ch[0].swshadow)' in save
    assert 'I4("S1sn", &snd.ch[0].swenabled)' in save


def test_sound_wave_reads_use_cpu_cycle_timer():
    sound = _read("apps/plugins/rockboy/sound.c")
    sound_h = _read("apps/plugins/rockboy/sound.h")
    save = _read("apps/plugins/rockboy/save.c")
    cpu = _read("apps/plugins/rockboy/cpu.c")

    assert "int wave_timer;" in sound_h
    assert "int wave_access;" in sound_h
    assert "int wave_index;" in sound_h
    assert "int wave_startup;" in sound_h
    assert "void sound_tick(int cnt)" in sound_h
    assert "static void sound_wave_tick(int cnt)" in sound
    assert "snd.wave_timer = sound_wave_period() + 1;" in sound
    assert "snd.wave_startup = 1;" in sound
    assert "static int sound_wave_access_offset(void)" in sound
    assert "if (!snd.wave_access || (snd.wave_startup && snd.wave_index == 1))" in sound
    assert "if (snd.wave_timer != 1 || (snd.wave_startup && snd.wave_index == 1))" in sound
    assert "static void sound_wave_retrigger_dmg(void)" in sound
    assert "if (offset < 4)" in sound
    assert "ram.hi[0x30] = ram.hi[0x30 + offset];" in sound
    assert "src = 0x30 + (offset & 0x0c);" in sound
    assert "if (snd.wave_startup && snd.wave_index != 1)" in sound
    assert "snd.wave_access = 1;" in sound
    assert "return ((snd.wave_index - 1) >> 1) & 0x0f;" in sound
    assert "return (snd.wave_index >> 1) & 0x0f;" in sound
    assert "return ram.hi[0x30 + offset];" in sound
    assert "ram.hi[0x30 + offset] = b;" in sound
    assert "sound_wave_retrigger_dmg();" in sound
    assert "sound_wave_restart_timer();" in sound
    assert 'I4("SWt ", &snd.wave_timer)' in save
    assert 'I4("SWa ", &snd.wave_access)' in save
    assert 'I4("SWi ", &snd.wave_index)' in save
    assert 'I4("SWs ", &snd.wave_startup)' in save
    assert '#include "sound.h"' in cpu
    assert "sound_tick(cnt);" in cpu


def test_sound_powered_off_keeps_noise_length_writable():
    sound = _read("apps/plugins/rockboy/sound.c")

    assert "byte nr41 = R_NR41;" in sound
    assert "int s1_len = S1.len;" in sound
    assert "int s2_len = S2.len;" in sound
    assert "int s3_len = S3.len;" in sound
    assert "int s4_len = S4.len;" in sound
    assert "R_NR41 = nr41;" in sound
    assert "S1.len = s1_len;" in sound
    assert "S2.len = s2_len;" in sound
    assert "S3.len = s3_len;" in sound
    assert "case RI_NR11:" in sound
    assert "case RI_NR21:" in sound
    assert "case RI_NR31:" in sound
    assert "case RI_NR41:" in sound
    assert "S4.len = SOUND_LENGTH_UNIT * (64 - (b & 0x3f));" in sound
