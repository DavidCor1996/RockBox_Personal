#!/usr/bin/env python3
"""Focused source, asset, input, and build gate for Snow Leopard Desktop Mode."""

from __future__ import annotations

import argparse
import json
import math
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "rockpod"))

from services.snow_leopard_assets import ASSET_SPECS, validate_pack  # noqa: E402


# Ceiling for everything Desktop Mode keeps decoded at once.  It is a budget
# against PLUGIN_BUFFER_SIZE, which is 3 MiB and dedicated to the running
# plugin - never playback or core memory - so the limit exists to keep the
# shell honest rather than because the space is contended.
DECODED_LIMIT_BYTES = 1024 * 1024

PLUGIN = ROOT / "apps/plugins/desktop_mode.c"
LIVETV_HEADER = ROOT / "apps/plugins/mpegplayer/livetv.h"
ROOT_MENU = ROOT / "apps/root_menu.c"
BUILD_ARTIFACTS = (
    ROOT / "build-sim-ipod6g/apps/plugins/desktop_mode.rock",
    ROOT / "build-sim-video-5g/apps/plugins/desktop_mode.rock",
    ROOT / "build-hw-ipod6g/apps/plugins/desktop_mode.rock",
    ROOT / "build-hw-ipodvideo/apps/plugins/desktop_mode.rock",
)


class WheelModel:
    """Deterministic reference for the integer device wheel policy."""

    def __init__(self, pointer_speed=2, reverse=False):
        self.pointer_speed = pointer_speed
        self.reverse = reverse
        self.last = -1
        self.velocity = 0
        self.direction = 0
        self.last_tick = 0
        self.x = 160
        self.y = 120
        self.frac_x = 0
        self.frac_y = 0

    def sample(self, wheel, tick, hz=100):
        if wheel < 0:
            self.last = -1
            self.velocity = 0
            self.direction = 0
            self.last_tick = 0
            return (0, 0)
        if self.last < 0:
            self.last = wheel
            self.last_tick = tick
            return (0, 0)
        old = self.last
        if old == wheel:
            return (0, 0)
        delta = wheel - old
        if delta > 48:
            delta -= 96
        elif delta < -48:
            delta += 96
        if abs(delta) <= 2:
            # the anchor is held below the threshold so a slow stroke
            # accumulates across samples rather than being discarded
            return (0, 0)
        direction = -1 if delta < 0 else 1
        elapsed = tick - self.last_tick
        if self.direction and direction != self.direction:
            self.velocity = 0
        if elapsed > hz // 4:
            self.velocity = 0
        self.direction = direction
        self.last = wheel
        self.last_tick = tick
        old_angle = math.radians(old * 360 // 96 - 90)
        new_angle = math.radians(wheel * 360 // 96 - 90)
        dx = round((math.cos(new_angle) - math.cos(old_angle)) * 16384)
        dy = round((math.sin(new_angle) - math.sin(old_angle)) * 16384)
        if self.reverse:
            dx, dy = -dx, -dy
        elapsed = max(1, elapsed)
        self.velocity = min(
            4,
            (self.velocity * 2 + min(4, abs(delta) * hz // elapsed // 12))
            // 3,
        )
        scale = self.pointer_speed + self.velocity - 1
        # displacement carries sixteenths of a pixel between samples
        self.frac_x += int(dx * scale / 128)
        self.frac_y += int(dy * scale / 128)
        dx = max(-14, min(14, int(self.frac_x / 16)))
        dy = max(-14, min(14, int(self.frac_y / 16)))
        self.frac_x -= dx * 16
        self.frac_y -= dy * 16
        self.x = max(0, min(318, self.x + dx))
        self.y = max(0, min(238, self.y + dy))
        return (dx, dy)


def _extract_function(source, name):
    match = re.search(
        r"\b%s\s*\([^;]*?\)\s*\{" % re.escape(name),
        source,
        flags=re.S,
    )
    if not match:
        return ""
    index = match.end()
    depth = 1
    while index < len(source) and depth:
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
        index += 1
    return source[match.start() : index]


def _check_source():
    source = PLUGIN.read_text(encoding="utf-8")
    runtime_source = source + "\n" + LIVETV_HEADER.read_text(encoding="utf-8")
    root_menu = ROOT_MENU.read_text(encoding="utf-8")
    errors = []
    lower = source.lower()
    prohibited_identity = (
        "desktop_mode_xp",
        "bliss",
        "luna",
        "start menu",
        "explorer",
        "control panel",
    )
    for term in prohibited_identity:
        if term in lower:
            errors.append("runtime still contains replaced identity: %s" % term)
    prohibited_calls = (
        "plugin_get_audio_buffer",
        "audio_stop(",
        "core_alloc(",
        "buflib_alloc(",
        "font_load(",
    )
    for call in prohibited_calls:
        if call in source:
            errors.append("forbidden lifecycle call: %s" % call)
    for spec in ASSET_SPECS:
        # A pack carries every panel profile; the shell declares the one it was
        # compiled for, so check each asset is named by some profile's build.
        name = spec.output.split("/", 1)[-1] if "x" in spec.output.split("/")[0] \
            else spec.output
        if ("/" + name) not in runtime_source:
            errors.append("runtime asset is not declared: %s" % spec.output)
    paint_forbidden = (
        "rb->open(",
        "rb->opendir(",
        "rb->read_bmp_file(",
        "rb->plugin_open(",
        "rb->filetype_get_",
        "rb->mkdir(",
        "rb->remove(",
        "rb->rmdir(",
        "plugin_get_buffer",
    )
    paint_names = (
        "dm_draw",
        "dm_draw_menu_bar",
        "dm_draw_desktop_icons",
        "dm_draw_dock",
        "dm_draw_finder",
        "dm_draw_itunes",
        "dm_draw_preferences",
        "dm_draw_apple_menu",
        "dm_draw_finder_context_menu",
        "dm_draw_get_info",
        "dm_draw_diagnostics",
        "dm_draw_confirm",
        "dm_draw_cursor",
    )
    for name in paint_names:
        body = _extract_function(source, name)
        if not body:
            errors.append("paint function is missing: %s" % name)
            continue
        for call in paint_forbidden:
            if call in body:
                errors.append("%s contains paint-time call %s" % (name, call))
    required_config = (
        "snow pointer speed",
        "snow reverse wheel",
        "snow drag lock",
        "snow desktop folders",
        "snow restore session",
        "snow start app",
        "snow last app",
        "snow cursor x",
        "snow cursor y",
        "snow finder path",
    )
    for key in required_config:
        if key not in source:
            errors.append("config round-trip key is missing: %s" % key)
    animation_prepare = _extract_function(source, "dm_prepare_animation_frame")
    if "#define DM_ANIMATION_FRAMES 6" not in source:
        errors.append("minimize/restore is not fixed to the six-frame contract")
    # The compositor scales the window's own chrome straight into the frame it
    # is assembling, so there is no scratch bitmap to prepare.  What still has
    # to hold is that frame selection is elapsed-time arithmetic only, and that
    # the animation never allocates or decodes.
    for token in ("dm_alloc", "read_bmp_file", "simple_resize_bitmap"):
        if token in animation_prepare:
            errors.append(
                "animation frame preparation must be arithmetic only: %s"
                % token
            )
    if "animation->started" not in animation_prepare + _extract_function(
        source, "dm_update_animation"
    ):
        errors.append("minimize/restore is not driven by elapsed ticks")
    if "dm_finish_animation(&state)" not in source:
        errors.append("queued input cannot finish decorative animation")
    # One compose plane, never a source/destination pair.
    if source.count("dm_canvas = dm_alloc(") != 1:
        errors.append("compositor must allocate exactly one full-screen plane")
    present = _extract_function(source, "dm_present")
    if "rb->lcd_bitmap(" not in present or present.count("lcd_update()") != 1:
        errors.append("a committed frame must reach the LCD exactly once")
    if "rb->lcd_bitmap_part(" not in present or "lcd_update_rect(" not in present:
        errors.append("pointer-only frames have no partial LCD update path")
    damage = _extract_function(source, "dm_prepare_damage")
    for token in ("presented_cursor_x", "presented_hover", "DM_ACTION_DOCK_APP"):
        if token not in damage:
            errors.append("damage tracking is missing token: %s" % token)
    for name in ("dm_blit", "dm_blit_panel", "dm_compose", "dm_draw_text"):
        body = _extract_function(source, name)
        if not body:
            errors.append("compositor primitive is missing: %s" % name)
        elif "dm_paint_clip" not in body:
            errors.append("compositor primitive ignores paint clip: %s" % name)
    if "#define DM_CONTROL_LIMIT 64" not in source:
        errors.append("bounded 64-entry control profile is missing")
    controls = _extract_function(source, "dm_register_control")
    if "DM_ERR_CONTROL_OVERFLOW" not in controls:
        errors.append("control overflow is not reported safely")
    hover = _extract_function(source, "dm_update_hover")
    if "dm_file_selected =" in hover:
        errors.append("pointer hover still mutates the selected file")
    click = _extract_function(source, "dm_click")
    if "dm_control_at(" not in click:
        errors.append("click dispatch does not use the control registry")
    if source.count("cleanup:") != 1:
        errors.append("Desktop Mode does not have exactly one cleanup path")
    for token in (
        "lcd_set_viewport(NULL)",
        "lcd_set_foreground(saved_foreground)",
        "lcd_set_background(saved_background)",
        "lcd_set_drawmode(saved_drawmode)",
    ):
        if token not in source:
            errors.append("cleanup does not restore display state: %s" % token)
    # Fallback pointer motion is what lets the focused gate drive the shell at
    # all on a target that reports no wheel contact.
    if "state->wheel_available = true" not in source:
        errors.append("absolute wheel availability is never latched")
    if "dm_fallback_step" not in source:
        errors.append("four-direction accessibility fallback is missing")
    if "#define DM_BOOT_FRAME_COUNT 12" not in source:
        errors.append("authentic boot animation does not declare twelve phases")
    boot = _extract_function(source, "dm_show_boot_animation")
    for token in (
        "dm_boot_background_path",
        "dm_boot_spinner_paths",
        "plugin_get_buffer",
        "button_get_w_tmo",
        "SYS_USB_CONNECTED",
    ):
        if token not in boot:
            errors.append("boot animation is missing lifecycle token: %s" % token)
    native_apps = re.search(
        r"MAKE_MENU\(applications_menu,\s*\"Extras\".*?\);",
        root_menu,
        flags=re.S,
    )
    if not native_apps or "&desktop_mode_item" not in native_apps.group(0):
        errors.append("Desktop Mode is missing from native Extras > Applications")
    ipodjs_apps = re.search(
        r"root_menu_video_application_items\[\]\s*=\s*\{(.*?)\};",
        root_menu,
        flags=re.S,
    )
    if not ipodjs_apps or (
        '"Desktop Mode", launch_desktop_mode' not in ipodjs_apps.group(1)
    ):
        errors.append("Desktop Mode is missing from iPodJS Extras > Applications")
    if source.count("wheel_send_events(true)") < 3:
        errors.append("wheel event delivery is not restored on all handoffs")
    if "wheel_send_events(false)" not in source:
        errors.append("absolute wheel ownership is never enabled")
    return errors


def _check_wheel():
    errors = []
    wheel = WheelModel()
    if wheel.sample(10, 1) != (0, 0):
        errors.append("initial wheel contact moved the pointer")
    if wheel.sample(11, 2) != (0, 0) or wheel.sample(12, 3) != (0, 0):
        errors.append("one/two-count jitter moved the pointer")
    if wheel.sample(13, 4) == (0, 0):
        errors.append("cumulative intentional wheel motion was lost")

    wrap = WheelModel()
    wrap.sample(94, 10)
    dx, dy = wrap.sample(1, 12)
    if (dx, dy) == (0, 0) or abs(dx) > 14 or abs(dy) > 14:
        errors.append("95-to-0 wheel wrap is discontinuous")

    lift = WheelModel()
    lift.sample(20, 1)
    lift.sample(40, 2)
    lift.sample(-1, 3)
    if lift.sample(70, 4) != (0, 0):
        errors.append("finger lift did not reset the wheel anchor")

    cap = WheelModel(pointer_speed=4)
    cap.sample(0, 1)
    for tick, position in enumerate((20, 40, 60, 80), 2):
        dx, dy = cap.sample(position, tick)
        if abs(dx) > 14 or abs(dy) > 14:
            errors.append("wheel acceleration exceeded 14 pixels")
            break

    reversal = WheelModel(pointer_speed=4)
    reversal.sample(10, 1)
    reversal.sample(40, 2)
    reversal.sample(10, 3)
    if reversal.direction != -1:
        errors.append("wheel direction reversal was not detected")
    return errors


def _check_builds():
    errors = []
    source_mtime = PLUGIN.stat().st_mtime
    for artifact in BUILD_ARTIFACTS:
        if not artifact.is_file():
            errors.append("build artifact is missing: %s" % artifact.relative_to(ROOT))
        elif artifact.stat().st_mtime < source_mtime:
            errors.append("build artifact is stale: %s" % artifact.relative_to(ROOT))
    return errors


def _align(value):
    return (value + 3) // 4 * 4


def _asset_resident_bytes(spec):
    """Bytes one asset occupies in the plugin arena once loaded.

    Coverage assets carry Apple's 8-bit alpha beside the RGB565 plane.
    """
    width, height = spec.size
    if spec.kind in {"icon", "alpha_image", "derived_alpha"}:
        return _align(width * height * 2) + _align(width * height)
    return _align(width * height * 2)


def _font_resident_bytes(pack):
    """Coverage atlas bytes, measured rather than guessed.

    Atlas geometry follows the real Lucida Grande faces, so it is read from a
    built pack's manifest when one is supplied and falls back to the figure
    recorded in the size report otherwise.
    """
    if not pack or not pack.get("manifest"):
        return 40_725
    total = 0
    for asset_id, item in pack["manifest"].get("assets", {}).items():
        if not asset_id.startswith("font."):
            continue
        width, height = item["output_size"]
        total += _align(width * height) + int(item.get("metrics_bytes", 0))
    return total


def _size_contract(pack=None, profile="320x240"):
    # Resident cost is one panel profile plus the shared assets, not the whole
    # pack: a build only ever loads the profile matching its LCD.
    resident = sum(
        _asset_resident_bytes(spec)
        for spec in ASSET_SPECS
        if spec.size is not None and spec.runtime_resident
        and spec.output.startswith((profile + "/", "cursor/", "desktop/"))
    ) + _font_resident_bytes(pack)
    transient_boot = sum(
        _align(spec.size[0] * spec.size[1] * 2)
        for spec in ASSET_SPECS
        if spec.size is not None and not spec.runtime_resident
        and spec.output.startswith((profile + "/", "boot/"))
    )
    # One compose plane; format 1's separate 304x174 minimize scratch is gone.
    compose = 320 * 240 * 2
    shell_runtime = resident + compose
    runtime = max(shell_runtime, shell_runtime + transient_boot - resident)
    return {
        "required_assets": len(ASSET_SPECS),
        "resident_asset_bytes": resident,
        "compose_buffer_bytes": compose,
        "transient_boot_bytes": transient_boot,
        "shell_runtime_bytes": shell_runtime,
        "visual_runtime_bytes": runtime,
        "decoded_limit_bytes": DECODED_LIMIT_BYTES,
        "within_limit": runtime <= DECODED_LIMIT_BYTES,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pack", help="validate a completed private pack")
    parser.add_argument("--require-builds", action="store_true")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    errors = _check_source() + _check_wheel()
    if args.require_builds:
        errors.extend(_check_builds())
    pack = None
    if args.pack:
        pack = validate_pack(args.pack)
        if not pack["valid"]:
            errors.extend("pack: " + item for item in pack["errors"])
    sizes = _size_contract(pack)
    if not sizes["within_limit"]:
        errors.append("decoded mandatory asset contract exceeds the ceiling")
    result = {
        "ok": not errors,
        "errors": errors,
        "size_contract": sizes,
        "pack": pack,
        "builds_required": bool(args.require_builds),
    }
    if args.json:
        print(json.dumps(result, indent=2, sort_keys=True))
    elif result["ok"]:
        print(
            "Snow Leopard Desktop Mode gate passed: "
            "%d assets, %d visual runtime bytes"
            % (sizes["required_assets"], sizes["visual_runtime_bytes"])
        )
    else:
        for error in errors:
            print("ERROR: " + error, file=sys.stderr)
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
