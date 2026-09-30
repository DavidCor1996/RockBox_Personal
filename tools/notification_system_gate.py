#!/usr/bin/env python3
"""Static/build gate for the iPodJS notification system."""

from __future__ import annotations

import argparse
import hashlib
import re
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

IOS5_ASSETS = {
    "achievement-icon.22x22x24.bmp":
        "ffa8ffaf60d5fcb411219b018851cb407476ed741684df2ddcd8a302b0ea6d84",
    "sitekick-icon.22x22x24.bmp":
        "c6fc664d15ddb329e97e4ae50dfab33997e63a8c5caf57f3bd59f0164fc03e6f",
    "notification-banner.ios5.320x42x24.bmp":
        "9fee12e586ba6beb8543c62af70592148fddc8736f72a198a3a998a05c5c075b",
    "notification-center-linen.ios5.32x32x24.bmp":
        "4d7152031e9baa894b15c0cce4fbc261115d0bf719aa8d0df0541466c3da1b2e",
    "notification-close.ios5.22x22x24.bmp":
        "9931d3e650324382adedf58bb1f365d61392e222b0097c9ff2f43daeb1f62f43",
    "notification-section.ios5.32x24x24.bmp":
        "7518a4f61bf53a47030a018dd4fc06e77c21db1cd4f80cef102ed1b870cdfa9a",
}


def require(path: str, *needles: str) -> str:
    text = (ROOT / path).read_text(encoding="utf-8")
    missing = [needle for needle in needles if needle not in text]
    if missing:
        raise SystemExit(f"{path}: missing {', '.join(missing)}")
    return text


def check_bss(elf: Path) -> int:
    result = subprocess.run(
        ["arm-elf-eabi-nm", "-S", str(elf)],
        check=True,
        capture_output=True,
        text=True,
    )
    total = 0
    for line in result.stdout.splitlines():
        fields = line.split()
        if len(fields) >= 4 and fields[2] in {"b", "B"} and \
                fields[3].startswith("notification_"):
            total += int(fields[1], 16)
    if total > 39 * 1024:
        raise SystemExit(
            f"notification BSS is {total} bytes; limit is {39 * 1024}")
    return total


def check_ios5_assets() -> None:
    asset_dir = ROOT / "assets/ipodjs/rockbox/notifications"
    for name, expected in IOS5_ASSETS.items():
        path = asset_dir / name
        if not path.is_file():
            raise SystemExit(f"missing iOS 5 notification asset: {path}")
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != expected:
            raise SystemExit(f"unexpected checksum for {path}: {actual}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--elf",
        type=Path,
        default=ROOT / "build-hw-ipod6g/rockbox.elf",
        help="6G ELF used for the fixed-memory budget check",
    )
    args = parser.parse_args()

    manager = require(
        "apps/notification_manager.c",
        "NOTIFICATION_HISTORY_MAX",
        "NOTIFICATION_SCHEDULE_MAX",
        "NOTIFICATION_STATE_NEW",
        "notification_checksum_update",
        "lcd_set_overlay_row_hook",
        "notification_render_banner",
        "notification_blend_round_rect",
        "NOTIFICATION_DESKTOP_X",
        "notification_load_desktop_icon",
        "notification_desktop_display_x",
        "notification_manager_set_desktop_mode",
        "notification_manager_hibernate_prepare",
        "notification_manager_hibernate_resume",
        "notification_hibernate_suspended",
        "notification_hibernate_resume_pending",
        "notification_music_reset_baseline",
        "beep_play(0, 0, 0)",
        "notification_manager_test_banner",
        "notification_puts_fit",
        "15-Helvetica-Bold-RetailOS-Apple.fnt",
        "13-Helvetica-RetailOS-Apple.fnt",
        "notification-banner.ios5.320x42x24.bmp",
    )
    reset_start = manager.index(
        "static void notification_music_reset_baseline(void)"
    )
    reset_end = manager.index(
        "static void notification_music_service(void)", reset_start
    )
    reset_body = manager[reset_start:reset_end]
    for forbidden in ("audio_status(", "audio_current_track(",
                      "notification_post(", "beep_play("):
        if forbidden in reset_body:
            raise SystemExit(
                "hibernate notification baseline touches live service: " +
                forbidden
            )
    if ("notification_music_known = false" not in reset_body or
            "notification_music_path[0] = '\\0'" not in reset_body):
        raise SystemExit(
            "hibernate notification baseline is not invalidated for a "
            "silent later relearn"
        )
    require(
        "apps/menus/theme_menu.c",
        "Test Notification",
        "notification_manager_test_banner",
    )
    require(
        "apps/root_menu.c",
        "root_menu_video_notification_center",
        "Clear all notifications?",
        "launch_sitekick_plugin(record->request.route",
        "notification-center-linen.ios5.32x32x24.bmp",
        "achievement-icon.22x22x24.bmp",
        "root_menu_video_notification_age",
        "notification_manager_set_desktop_mode(true)",
        "notification_manager_set_desktop_mode(false)",
        "ipodjs_ui_transition_begin_vertical(1)",
        "ipodjs_ui_transition_begin_vertical(-1)",
    )
    center_source = (ROOT / "apps/root_menu.c").read_text(encoding="utf-8")
    center_start = center_source.index(
        "static int root_menu_video_notification_center(void)")
    center_end = center_source.index(
        "static int root_menu_video_dashboard", center_start)
    center_source = center_source[center_start:center_end]
    if not re.search(
            r"case ACTION_STD_CONTEXT:.*?clear_pending = true",
            center_source, re.DOTALL):
        raise SystemExit("Select hold must offer to clear all notifications")
    if "case ACTION_STD_MENU:" not in center_source:
        raise SystemExit("Menu must dismiss Notification Center")
    if not re.search(
            r"case ACTION_STD_MENU:.*?notification_manager_set_center_active"
            r"\(false\);",
            center_source, re.DOTALL):
        raise SystemExit("Menu must cancel confirmation or dismiss the center")
    dashboard_source = (ROOT / "apps/root_menu.c").read_text(encoding="utf-8")
    if "left_hold_start" in dashboard_source:
        raise SystemExit("Notification Center must not require Left hold")
    if not all(token in dashboard_source for token in (
            "left_toggle_armed = true",
            "left_toggle_armed = false")):
        raise SystemExit("Left toggle must consume the dismissal release")
    if not re.search(
            r"case ACTION_STD_CANCEL:.*?"
            r"root_menu_video_present_notification_center\(\)",
            dashboard_source, re.DOTALL):
        raise SystemExit("Short Left must launch Notification Center")
    if not re.search(
            r"case ACTION_STD_MENU:.*?Left-toggle only.*?break;",
            dashboard_source, re.DOTALL):
        raise SystemExit("Home Menu must not launch Notification Center")
    require(
        "apps/gui/ipodjs_ui.c",
        "ipodjs_ui_transition_begin_vertical",
        'direction > 0 ? "down" : "up"',
        "LCD_HEIGHT - reveal",
    )
    settings = require(
        "apps/settings_list.c",
        "notifications_enabled",
        "notification_banners",
        "notification_achievements",
        "notification_music",
        "notification_sitekick",
        "notification_livetv",
        "notification_weather",
        "notification_battery",
        "notification_storage",
        "notification_sound",
    )
    for name, default in (
        ("notifications_enabled", "true"),
        ("notification_banners", "true"),
        ("notification_achievements", "true"),
        ("notification_music", "true"),
        ("notification_sitekick", "false"),
        ("notification_livetv", "true"),
        ("notification_weather", "true"),
        ("notification_battery", "true"),
        ("notification_storage", "true"),
        ("notification_sound", "false"),
    ):
        if not re.search(
            rf"OFFON_SETTING\([^\n]*{name},[^\n]*{default},", settings
        ):
            raise SystemExit(f"unexpected default for {name}")

    for token in (
        "notification_battery_service",
        "NOTIFICATION_BATTERY_LOW",
        "NOTIFICATION_BATTERY_CRITICAL",
        "notification_storage_service",
        "NOTIFICATION_STORAGE_LOW",
        "NOTIFICATION_STORAGE_CRITICAL",
        "volume_size(IF_MV(0,) &size, &free)",
    ):
        if token not in manager:
            raise SystemExit(f"notification manager missing {token}")

    require(
        "apps/rockachievements_telemetry.c",
        "achievement_evaluate_baseline",
        "NOTIFICATION_ACHIEVEMENT_UNLOCKED",
        "notification_post(&request)",
    )
    require(
        "apps/plugins/sitekick.c",
        "NOTIFICATION_SITEKICK_DUMP_READY",
        "NOTIFICATION_SITEKICK_SHOP_READY",
        "NOTIFICATION_SITEKICK_CHIP_ACQUIRED",
        "NOTIFICATION_SITEKICK_INBOX_APPLIED",
        '"dump-view"',
        '"shop-view"',
        '"workshop-view"',
    )
    require(
        "apps/plugins/mpegplayer/livetv_guide.c",
        "Upcoming Program",
        "Set Reminder",
        "Cancel Reminder",
        "NOTIFICATION_LIVETV_SHOW_REMINDER",
        "notification_schedule(&request, notify_at)",
    )
    require(
        "apps/notification_manager.c",
        "notification_music_service",
        "NOTIFICATION_MUSIC_NOW_PLAYING",
        "notification_weather_service",
        "NOTIFICATION_WEATHER_SYNC_STALE",
        "NOTIFICATION_WEATHER_SYNC_MISSING",
    )
    require(
        "firmware/target/arm/s5l8702/lcd-s5l8702.c",
        "lcd_compose_overlay_row",
    )
    require(
        "firmware/target/arm/ipod/video/lcd-video.c",
        "lcd_overlay_line",
        "lcd_compose_overlay_row",
    )
    require(
        "firmware/target/hosted/sdl/lcd-bitmap.c",
        "lcd_compose_overlay_row",
    )
    require(
        "firmware/target/hosted/sdl/lcd-sdl.c",
        "overlay_line",
        "lcd_compose_overlay_row",
    )
    if "struct notification_disk_state" in manager:
        raise SystemExit("persistence snapshot must not live on the stack")
    if not args.elf.is_file():
        raise SystemExit(f"missing ELF: {args.elf}")
    check_ios5_assets()
    bss = check_bss(args.elf)
    print(f"notification gate passed; notification BSS={bss} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
