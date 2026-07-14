#!/usr/bin/env python3
"""Static, build, and package regression gate for the SM64 native port."""

from __future__ import annotations

import argparse
import struct
import zipfile
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def bmp_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()[:26]
    require(data[:2] == b"BM" and len(data) >= 26, f"invalid BMP: {path}")
    return struct.unpack_from("<ii", data, 18)


def require_native_seqfile(path: Path, magic: int, count: int) -> None:
    data = path.read_bytes()
    require(len(data) >= 16, f"32-bit audio table is truncated: {path}")
    actual_magic, actual_count = struct.unpack_from("<HH", data)
    require((actual_magic, actual_count) == (magic, count),
            f"wrong 32-bit audio table header: {path}")
    first_offset = struct.unpack_from("<I", data, 4)[0]
    expected_offset = (4 + count * 8 + 15) & ~15
    require(first_offset == expected_offset,
            f"audio table uses non-32-bit pointer layout: {path}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path("."))
    parser.add_argument("--sim-build", type=Path, default=Path("build-sim-ipod6g"))
    parser.add_argument("--hw-build", type=Path, default=Path("build-hw-ipod6g"))
    args = parser.parse_args()

    root = args.root.resolve()
    sim = root / args.sim_build
    hw = root / args.hw_build
    launcher = (root / "apps/plugins/rockboy_launcher.c").read_text(encoding="utf-8")
    frontend = (root / "apps/plugins/sm64/sm64_rockbox.c").read_text(encoding="utf-8")
    controls = (root / "apps/plugins/sm64/sm64_input.c").read_text(encoding="utf-8")
    video = (root / "apps/plugins/sm64/sm64_video.c").read_text(encoding="utf-8")
    renderer = (
        root / "apps/plugins/sm64/upstream/src/pc/gfx/gfx_soft.c"
    ).read_text(encoding="utf-8")
    gfx_pc = (
        root / "apps/plugins/sm64/upstream/src/pc/gfx/gfx_pc.c"
    ).read_text(encoding="utf-8")
    categories = (root / "apps/plugins/CATEGORIES").read_text(encoding="utf-8")
    sound_source = (root / "apps/plugins/sm64/upstream/sound/sound_data.c").read_text(
        encoding="utf-8"
    )
    makefile = (root / "apps/plugins/sm64/sm64.make").read_text(encoding="utf-8")

    for fragment in (
        '#define SM64_PLUGIN_PATH      PLUGIN_GAMES_DIR "/sm64.rock"',
        'add_system_entry("n64", "Nintendo 64", "Super Mario 64 native port"',
        'rb->strlcpy(system->extensions, ".z64,.n64,.v64"',
        'detect_library_cover(system->id, child, cover, sizeof(cover))',
        'return &bm_game_system_n64',
    ):
        require(fragment in launcher, f"launcher integration missing {fragment!r}")
    require("sm64,games" in categories, "SM64 is not categorized as a game")

    require("sound32/sound_data.ctl.inc.c" in sound_source and
            "sound32/sequences.bin.inc.c" in sound_source,
            "native builds are not selecting 32-bit audio assets")
    audio32 = root / "apps/plugins/sm64/upstream/build/us_rockbox32/sound32"
    require_native_seqfile(audio32 / "sound_data.ctl", 1, 38)
    require_native_seqfile(audio32 / "sound_data.tbl", 2, 38)
    require_native_seqfile(audio32 / "sequences.bin", 3, 35)

    for fragment in (
        "rb->wheel_status()", "A_BUTTON", "B_BUTTON", "Z_TRIG", "R_TRIG",
        "START_BUTTON", "BUTTON_SCROLL_FWD", "BUTTON_SCROLL_BACK",
        "rb->button_hold()", "rb->haptic_feedback",
    ):
        require(fragment in controls, f"control mapping missing {fragment!r}")

    for fragment in (
        "plugin_get_audio_buffer", "plugin_release_audio_buffer",
        "destroy_memory_pool", "backlight_use_settings", "rb->cpu_boost(false)",
        "static int16_t frame_audio_buffer[SAMPLES_HIGH * 4];",
    ):
        require(fragment in frontend, f"lifecycle cleanup missing {fragment!r}")
    require("int16_t audio_buffer[SAMPLES_HIGH * 4];" not in frontend,
            "4.25 KiB audio buffer remains on the native 8 KiB main stack")
    require("skip_next" not in frontend,
            "unsafe display-list frame skipping is enabled")
    require("-Wstack-usage=2048" in makefile,
            "native SM64 build does not guard against large stack frames")

    require("lcd_fb = select_lcd_framebuffer();" in video,
            "renderer does not select the main LCD framebuffer")
    require("*(rb->screens[SCREEN_MAIN]->current_viewport)" in video,
            "renderer does not read back the newly-current viewport")
    require("lcd_fb = rb->lcd_set_viewport(NULL)" not in video,
            "renderer mistakes the previous viewport for the main framebuffer")
    require("rb->lcd_bitmap(lcd_stage, 0, 0, LCD_WIDTH, LCD_HEIGHT);" in video,
            "renderer does not present through the Rockbox LCD blitter")
    for fragment in (
        "typedef int32_t fixed_t;",
        "cur_shader->combine(FIXED_ONE, fp + 4)",
        "vertex[property] *= w;",
        "u >> FIXED_SHIFT, v >> FIXED_SHIFT",
    ):
        require(fragment in renderer,
                f"ARM fixed-point renderer path missing {fragment!r}")
    require("1.f / p[3]" not in renderer,
            "software-float reciprocal remains in the per-pixel loop")
    require("cur_shader->combine(w, p + 4)" not in renderer,
            "float properties remain in the per-pixel combiner")
    require("fixed_recip(fp[3])" not in renderer,
            "integer software division remains in the per-pixel loop")
    require("Frame %lu  pixels %lu" not in video,
            "hardware frame-counter diagnostic is still visible")
    require("static uint8_t rgba32_buf[32768];" in gfx_pc,
            "texture conversion buffer is not in persistent storage")
    for size in (8192, 16384):
        require(f"uint8_t rgba32_buf[{size}];" not in gfx_pc,
                f"{size}-byte texture buffer remains on the native stack")
    require(gfx_pc.count("uint8_t rgba32_buf[32768];") == 1,
            "32 KiB texture conversion buffer is duplicated on the stack")

    sim_plugin = sim / "apps/plugins/sm64/sm64.rock"
    hw_stub = hw / "apps/plugins/sm64.rock"
    hw_overlay = hw / "apps/plugins/sm64/sm64.ovl"
    require(sim_plugin.is_file(), "simulator plugin missing")
    require(hw_stub.is_file() and hw_stub.stat().st_size < 3 * 1024 * 1024,
            "hardware launcher stub missing or too large")
    require(hw_overlay.is_file() and hw_overlay.stat().st_size < 20 * 1024 * 1024,
            "hardware overlay missing or unexpectedly large")

    cover_root = root / "assets/game_covers/n64"
    require(bmp_size(cover_root / "Super Mario 64 (USA).bmp") == (160, 120),
            "SM64 Cover Flow art is not 160x120")
    require(bmp_size(cover_root / "n64-system.bmp") == (160, 120),
            "N64 console art is not 160x120")
    sources = (cover_root / "SOURCES.tsv").read_text(encoding="utf-8")
    require("libretro-thumbnails" in sources and "commons.wikimedia.org" in sources,
            "online cover sources are not recorded")

    leaked = [
        path for path in (root / "apps/plugins/sm64").rglob("*")
        if path.is_file() and path.suffix.lower() in {".z64", ".n64", ".v64"}
    ]
    require(not leaked, f"ROM leaked into source tree: {leaked}")

    archive = hw / "rockbox.zip"
    require(archive.is_file(), "hardware rockbox.zip missing")
    with zipfile.ZipFile(archive) as package:
        names = {name.lower() for name in package.namelist()}
    for name in (
        ".rockbox/rocks/games/sm64.rock",
        ".rockbox/rocks/games/sm64.ovl",
        ".rockbox/games/library/covers/systems/n64.bmp",
        ".rockbox/games/library/covers/n64/super mario 64 (usa).bmp",
    ):
        require(name in names, f"package missing {name}")
    require(not any(Path(name).suffix in {".z64", ".n64", ".v64"} for name in names),
            "copyrighted ROM present in firmware ZIP")

    print(
        "PASS: n64-coverflow controls lifecycle sim=present "
        f"arm_overlay={hw_overlay.stat().st_size} package=complete rom_leaks=0"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
