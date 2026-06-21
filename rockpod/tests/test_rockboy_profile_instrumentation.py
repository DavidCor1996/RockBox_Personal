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
        "cgb_bg_only_eligible=%lu",
        "cgb_bg_only_used=%lu",
        "cgb_bg_only_rejected=%lu",
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


def test_simulator_profile_autowrite_is_opt_in_and_target_gated():
    profiler = _read("apps/plugins/rockboy/profiler.c")
    rockboy = _read("apps/plugins/rockboy/rockboy.c")

    assert "ROCKBOY_PROFILE_AUTOWRITE_FRAMES" in profiler
    assert "#ifdef SIMULATOR" in profiler
    assert "rockboy_profile_start(rom_path)" in rockboy


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
    assert "static void cgb_bg_only_scan(void)" in lcd
    assert "dmg_bg_only_eligible = !hw.cgb && !NS && WX == 160;" in lcd
    assert "cgb_bg_only_eligible = hw.cgb && !NS && WX == 160;" in lcd
    assert "fast_line_rendering_enabled()" in lcd
    assert "ROCKBOY_PERF_QUALITY" in lcd
    assert "ROCKBOY_EVENT_LCD_DMG_BG_ONLY_USED" in lcd
    assert "ROCKBOY_EVENT_LCD_DMG_BG_ONLY_REJECTED" in lcd
    assert "ROCKBOY_EVENT_LCD_CGB_BG_ONLY_USED" in lcd
    assert "ROCKBOY_EVENT_LCD_CGB_BG_ONLY_REJECTED" in lcd


def test_cpu_interpreter_scratch_state_stays_local():
    cpu = _read("apps/plugins/rockboy/cpu.c")

    assert "byte op;" in cpu
    assert "byte cbop;" in cpu
    assert "union reg acc;" in cpu
    assert "static byte op IBSS_ATTR" not in cpu
    assert "static union reg acc IBSS_ATTR" not in cpu
