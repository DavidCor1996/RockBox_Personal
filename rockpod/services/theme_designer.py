"""Guided iPone designer with staged variant generation."""

from __future__ import annotations

import json
import logging
import os
import re
import shutil
import subprocess
import tempfile
import colorsys
import hashlib
from copy import deepcopy

from PIL import Image, ImageColor, ImageDraw, ImageFilter, ImageOps, UnidentifiedImageError

from services.file_safety import atomic_write_json, atomic_write_text
from services.greyscale_images import render_2bpp_greyscale, should_render_2bpp_greyscale
from services.rockbox_themes import RockboxThemeService


BASE_THEME_BY_RESOLUTION = {
    "320x240": "iPoneCustom",
    "160x128": "CoverPod_3g",
    "176x132": "iPone_nano2g",
}

PREVIEW_MODES = ("simulator",)
PREVIEW_THEME_ID = "ipone_preview"

DEFAULT_COLORS = {
    "background": "100F16",
    "foreground": "F7F4FA",
    "selector_start": "2B2234",
    "selector_end": "9D7AE6",
    "selector_text": "FCF9FF",
    "list_separator": "1A1621",
    "sbs_clock": "E8DFF8",
}

RIGHT_PANE_MODES = ("miniplayer", "full art")
DEFAULT_RIGHT_PANE_MODE = "miniplayer"
RIGHT_PANE_VIDEO_FRAME_COUNT = 24
RIGHT_PANE_VIDEO_FPS = 12
RIGHT_PANE_VIDEO_OVERLAY_X_OFFSET = 4

DEFAULT_LOCKSCREEN_CLOCK = {
    "font_rel": "fonts/66-Cantarell-Light.fnt",
    "x": 0,
    "y": 32,
    "width": 320,
    "height": 55,
    "align": "center",
    "color": "FFFFFF",
    "style": "glass",
    "glass_strength": "high",
    "shadow": "soft",
    "stretch": 100,
    "opacity": 82,
}

LOCKSCREEN_CLOCK_FONT_FALLBACK = "fonts/90-Cantarell-Regular.fnt"
LOCKSCREEN_CLOCK_FONT_REJECT_TOKENS = ("icon", "symbol", "dingbat", "emoji")
LOCKSCREEN_STATUS_RESERVED_HEIGHT = 24

WALLPAPER_TARGETS = {
    "Blackery": {
        "main": [
            "Wallpaper.bmp",
            "WallpaperAlt.bmp",
            "WallpaperThird.bmp",
            "WallpaperFourth.bmp",
            "WallpaperFifth.bmp",
            "WallpaperSixth.bmp",
            "iPone_bg.bmp",
            "iPone_bd.bmp",
            "SbsBackdrop.bmp",
        ],
        "charging": [
            "ChargeWallpaper.bmp",
            "ChargeWallpaperAlt.bmp",
            "ChargeWallpaperThird.bmp",
            "ChargeWallpaperFourth.bmp",
        ],
    },
    "iPone": {
        "main": [
            "Wallpaper.bmp",
            "WallpaperAlt.bmp",
            "WallpaperThird.bmp",
            "WallpaperFourth.bmp",
            "WallpaperFifth.bmp",
            "WallpaperSixth.bmp",
            "iPone_bg.bmp",
            "iPone_bd.bmp",
            "SbsBackdrop.bmp",
        ],
        "charging": [
            "ChargeWallpaper.bmp",
            "ChargeWallpaperAlt.bmp",
            "ChargeWallpaperThird.bmp",
            "ChargeWallpaperFourth.bmp",
        ],
    },
    "iPoneCustom": {
        "main": [
            "Wallpaper.bmp",
            "WallpaperAlt.bmp",
            "WallpaperThird.bmp",
            "WallpaperFourth.bmp",
            "WallpaperFifth.bmp",
            "WallpaperSixth.bmp",
            "iPone_bg.bmp",
            "iPone_bd.bmp",
            "SbsBackdrop.bmp",
        ],
        "charging": [
            "ChargeWallpaper.bmp",
            "ChargeWallpaperAlt.bmp",
            "ChargeWallpaperThird.bmp",
            "ChargeWallpaperFourth.bmp",
        ],
        "right_pane": [
            "RightPaneWallpaper.bmp",
        ],
    },
    "iPone_nano2g": {
        "main": [
            "Wallpaper.bmp",
            "WallpaperAlt.bmp",
            "wpsbackdrop-176x132x16.bmp",
        ],
        "charging": [
            "ChargeWallpaper.bmp",
            "ChargeWallpaperAlt.bmp",
            "ChargeWallpaperThird.bmp",
            "ChargeWallpaperFourth.bmp",
        ],
    },
    "iPone_3g": {
        "main": [
            "Wallpaper.bmp",
            "WallpaperAlt.bmp",
            "wpsbackdrop-160x128x2.bmp",
        ],
        "charging": [
            "ChargeWallpaper.bmp",
            "ChargeWallpaperAlt.bmp",
            "ChargeWallpaperThird.bmp",
            "ChargeWallpaperFourth.bmp",
        ],
    },
    "CoverPod_3g": {
        "main": [
            "Wallpaper.bmp",
            "WallpaperAlt.bmp",
            "wpsbackdrop-160x128x2.bmp",
        ],
        "charging": [
            "ChargeWallpaper.bmp",
            "ChargeWallpaperAlt.bmp",
            "ChargeWallpaperThird.bmp",
            "ChargeWallpaperFourth.bmp",
        ],
    },
    "Galaxy": {
        "main": [
            "Wallpaper.bmp",
            "WallpaperAlt.bmp",
            "wpsbackdrop-160x128x2.bmp",
        ],
        "charging": [
            "ChargeWallpaper.bmp",
            "ChargeWallpaperAlt.bmp",
            "ChargeWallpaperThird.bmp",
            "ChargeWallpaperFourth.bmp",
        ],
    },
}

PREVIEW_BACKDROP_TARGETS = {
    "Blackery": [
        "backdrops/Blackery_bd.bmp",
        "wps/Blackery/SbsBackdrop.bmp",
        "wps/Blackery/iPone_bd.bmp",
        "wps/Blackery/iPone_bg.bmp",
    ],
    "iPone": [
        "backdrops/iPone_bd.bmp",
        "wps/iPone/SbsBackdrop.bmp",
        "wps/iPone/iPone_bd.bmp",
        "wps/iPone/iPone_bg.bmp",
    ],
    "iPoneCustom": [
        "wps/iPoneCustom/RightPaneWallpaper.bmp",
        "backdrops/iPoneCustom_bd.bmp",
        "wps/iPoneCustom/SbsBackdrop.bmp",
        "wps/iPoneCustom/iPone_bd.bmp",
        "wps/iPoneCustom/iPone_bg.bmp",
    ],
    "iPone_nano2g": [
        "wps/iPone_nano2g/wpsbackdrop-176x132x16.bmp",
        "wps/iPone_nano2g/Wallpaper.bmp",
    ],
    "iPone_3g": [
        "wps/iPone_3g/wpsbackdrop-160x128x2.bmp",
        "wps/iPone_3g/Wallpaper.bmp",
    ],
    "CoverPod_3g": [
        "wps/CoverPod_3g/wpsbackdrop-160x128x2.bmp",
        "wps/CoverPod_3g/Wallpaper.bmp",
    ],
    "Galaxy": [
        "wps/Galaxy/wpsbackdrop-160x128x2.bmp",
        "wps/Galaxy/Wallpaper.bmp",
    ],
}

SOLID_BACKGROUND_TARGETS = {
    "Blackery": {
        "main": WALLPAPER_TARGETS["Blackery"]["main"],
        "charging": WALLPAPER_TARGETS["Blackery"]["charging"],
    },
    "iPone": {
        "main": WALLPAPER_TARGETS["iPone"]["main"],
        "charging": WALLPAPER_TARGETS["iPone"]["charging"],
    },
    "iPoneCustom": {
        "main": WALLPAPER_TARGETS["iPoneCustom"]["main"],
        "charging": WALLPAPER_TARGETS["iPoneCustom"]["charging"],
    },
    "iPone_nano2g": {
        "main": WALLPAPER_TARGETS["iPone_nano2g"]["main"],
        "charging": WALLPAPER_TARGETS["iPone_nano2g"]["charging"],
    },
    "iPone_3g": {
        "main": WALLPAPER_TARGETS["iPone_3g"]["main"],
        "charging": WALLPAPER_TARGETS["iPone_3g"]["charging"],
    },
    "CoverPod_3g": {
        "main": WALLPAPER_TARGETS["CoverPod_3g"]["main"],
        "charging": WALLPAPER_TARGETS["CoverPod_3g"]["charging"],
    },
    "Galaxy": {
        "main": WALLPAPER_TARGETS["Galaxy"]["main"],
        "charging": WALLPAPER_TARGETS["Galaxy"]["charging"],
    },
}

SBS_SOLID_BACKGROUND_TARGETS = {
    "iPoneCustom": ("iPone_bd.bmp", "SbsBackdrop.bmp", "iPone_bd_fullart.bmp"),
}

TINTED_SPRITE_ASSETS = (
    "LoadingStatus.bmp",
    "LockscreenStyle.bmp",
    "AlwaysOnDisplayStyle.bmp",
    "NotifPlayIcon.bmp",
    "NotifPlayIconLock.bmp",
    "Playing Status.bmp",
    "PlayStatusPurple.bmp",
    "PlayStatusPurpleLarge.bmp",
    "PlayerSliderThinPurple.bmp",
    "PlayerSliderThinPurple12.bmp",
    "SliderThinPurple.bmp",
    "SliderThinPurple12.bmp",
    "SliderBackdropThinPurple.bmp",
    "SliderBackdropThinPurple4Digits.bmp",
    "SliderBackdropThinPurple5Digits.bmp",
    "SliderBackdropThinPurple6Digits.bmp",
    "SliderBackdropThinPurple12.bmp",
    "SliderBackdropThinPurple12_4Digits.bmp",
    "SliderBackdropThinPurple12_5Digits.bmp",
    "SliderBackdropThinPurple12_6Digits.bmp",
    "VolumeSliderBackdropPurple.bmp",
    "VolumeSliderEndPurple.bmp",
    "VolumeSliderPurple.bmp",
)


def _slug(value):
    text = re.sub(r"[^a-z0-9]+", "-", str(value or "").strip().lower())
    return text.strip("-") or "ipone-variant"


def _ensure_hex(value, fallback):
    text = str(value or "").strip().lstrip("#").upper()
    if re.fullmatch(r"[0-9A-F]{6}", text):
        return text
    return fallback


def _hex_to_rgb(value):
    text = _ensure_hex(value, "000000")
    return int(text[0:2], 16), int(text[2:4], 16), int(text[4:6], 16)


def _rgb_to_hex(rgb):
    red, green, blue = [max(0, min(255, int(round(channel)))) for channel in rgb]
    return f"{red:02X}{green:02X}{blue:02X}"


def _mix_hex(left, right, amount):
    amount = max(0.0, min(1.0, float(amount)))
    lred, lgreen, lblue = _hex_to_rgb(left)
    rred, rgreen, rblue = _hex_to_rgb(right)
    return _rgb_to_hex(
        (
            lred + (rred - lred) * amount,
            lgreen + (rgreen - lgreen) * amount,
            lblue + (rblue - lblue) * amount,
        )
    )


def _fit_size(resolution):
    width, height = str(resolution or "320x240").split("x", 1)
    return int(width), int(height)


class ThemeDesignerService:
    """Persist and generate safe iPone-based theme variants."""

    def __init__(self, theme_service=None):
        self._themes = theme_service or RockboxThemeService()

    def base_theme_id_for_profile(self, profile):
        resolution = str(profile.get("screen_resolution") or "320x240").strip() or "320x240"
        return BASE_THEME_BY_RESOLUTION.get(resolution, "iPone")

    def fonts_for_profile(self, repo_root):
        fonts_dir = os.path.join(os.path.abspath(repo_root), "fonts")
        fonts = []
        if os.path.isdir(fonts_dir):
            for name in sorted(os.listdir(fonts_dir)):
                if not name.lower().endswith(".fnt"):
                    continue
                rel = f"fonts/{name}"
                fonts.append(
                    {
                        "id": rel,
                        "label": name,
                        "path_rel": rel,
                        "path_abs": os.path.abspath(os.path.join(fonts_dir, name)),
                    }
                )
        return fonts

    def load_base_theme(self, repo_root, profile):
        theme_id = self.base_theme_id_for_profile(profile)
        bundle = self._themes.bundle_for_theme(theme_id, repo_root)
        cfg_asset = next((item for item in bundle["assets"] if item["kind"] == "cfg"), None)
        if cfg_asset and not cfg_asset.get("exists") and theme_id == "iPoneCustom":
            theme_id = "iPone"
            bundle = self._themes.bundle_for_theme(theme_id, repo_root)
            cfg_asset = next((item for item in bundle["assets"] if item["kind"] == "cfg"), None)
        settings = self._read_cfg_settings(cfg_asset["source_abs"]) if cfg_asset else {}
        base_right_pane_mode = self._normalize_right_pane_mode(settings.get("ipone right pane", DEFAULT_RIGHT_PANE_MODE))
        font_rel = self._normalize_font_rel(settings.get("font", ""))
        resolution = str(profile.get("screen_resolution") or "").strip() or "320x240"
        return {
            "id": "",
            "name": f"{bundle['name']} Custom",
            "base_theme_id": theme_id,
            "screen_resolution": resolution,
            "font_rel": font_rel or self._first_font_rel(repo_root),
            "fit_mode": "fill",
            "charging_fit_mode": "fill",
            "right_pane_fit_mode": "fill",
            "right_pane_mode": base_right_pane_mode,
            "base_right_pane_mode": base_right_pane_mode,
            "wallpaper_source": "",
            "charging_wallpaper_source": "",
            "right_pane_wallpaper_source": "",
            "right_pane_video_source": "",
            "show_line_separators": True,
            "lockscreen_clock": deepcopy(DEFAULT_LOCKSCREEN_CLOCK),
            "color_profile": "default",
            "appearance_mode": "dark",
            "colors": {
                "background": _ensure_hex(settings.get("background color"), DEFAULT_COLORS["background"]),
                "foreground": _ensure_hex(settings.get("foreground color"), DEFAULT_COLORS["foreground"]),
                "selector_start": _ensure_hex(settings.get("line selector start color"), DEFAULT_COLORS["selector_start"]),
                "selector_end": _ensure_hex(settings.get("line selector end color"), DEFAULT_COLORS["selector_end"]),
                "selector_text": _ensure_hex(settings.get("line selector text color"), DEFAULT_COLORS["selector_text"]),
                "list_separator": _ensure_hex(settings.get("list separator color"), DEFAULT_COLORS["list_separator"]),
                "sbs_clock": DEFAULT_COLORS["sbs_clock"],
            },
        }

    def new_variant(self, repo_root, profile, name=""):
        variant = self.load_base_theme(repo_root, profile)
        if name:
            variant["name"] = name
        return variant

    def list_variants(self, repo_root, resolution=""):
        variants = []
        folder = self._variants_dir(repo_root)
        if not os.path.isdir(folder):
            return variants
        for name in sorted(os.listdir(folder)):
            if not name.endswith(".json"):
                continue
            path = os.path.join(folder, name)
            try:
                with open(path, "r", encoding="utf-8") as handle:
                    item = json.load(handle)
            except (OSError, json.JSONDecodeError):
                continue
            normalized = self._normalize_variant(item, repo_root)
            if resolution and normalized["screen_resolution"] != resolution:
                continue
            variants.append(normalized)
        return variants

    def load_variant(self, repo_root, variant_id):
        path = self._variant_path(repo_root, variant_id)
        if not os.path.isfile(path):
            raise FileNotFoundError(variant_id)
        with open(path, "r", encoding="utf-8") as handle:
            return self._normalize_variant(json.load(handle), repo_root)

    def save_variant(self, repo_root, variant):
        normalized = self._normalize_variant(variant, repo_root)
        variant_id = normalized["id"] or self._unique_variant_id(repo_root, normalized["name"])
        normalized["id"] = variant_id
        atomic_write_json(self._variant_path(repo_root, variant_id), normalized)
        return normalized

    def rename_variant(self, repo_root, variant_id, new_name):
        variant = self.load_variant(repo_root, variant_id)
        variant["name"] = str(new_name or "").strip() or variant["name"]
        return self.save_variant(repo_root, variant)

    def duplicate_variant(self, repo_root, variant_id):
        original = self.load_variant(repo_root, variant_id)
        duplicated = deepcopy(original)
        duplicated["id"] = ""
        duplicated["name"] = f"{original['name']} Copy"
        return self.save_variant(repo_root, duplicated)

    def delete_variant(self, repo_root, variant_id):
        path = self._variant_path(repo_root, variant_id)
        if not os.path.isfile(path):
            return False
        os.remove(path)
        return True

    def build_remove_bundle(self, repo_root, profile, variant):
        normalized = self._normalize_variant(variant, repo_root)
        theme_name = normalized.get("id", "")
        if not theme_name:
            raise ValueError("Variant must be saved before it can be deleted")
        assets = []
        for kind, rel in (
            ("cfg", f"themes/{theme_name}.cfg"),
            ("wps", f"wps/{theme_name}.wps"),
            ("sbs", f"wps/{theme_name}.sbs"),
            ("fms", f"wps/{theme_name}.fms"),
            ("backdrop", f"backdrops/{theme_name}_bd.bmp"),
            ("iconset", f"icons/{theme_name}.bmp"),
            ("metadata", f"rockpod/theme_designer/{theme_name}.json"),
        ):
            assets.append(self._remove_asset_record(kind, f".rockbox/{rel}"))

        generated_assets_dir = os.path.join(self._generated_dir(repo_root), theme_name, "wps", theme_name)
        if os.path.isdir(generated_assets_dir):
            for root, _dirs, files in os.walk(generated_assets_dir):
                for filename in sorted(files):
                    full = os.path.join(root, filename)
                    suffix = os.path.relpath(full, generated_assets_dir).replace("\\", "/")
                    assets.append(self._remove_asset_record("wps_assets", f".rockbox/wps/{theme_name}/{suffix}"))
        return {
            "id": f"{theme_name}-remove",
            "name": f"Remove {normalized['name']}",
            "variant": normalized,
            "assets": assets,
        }

    def build_preview_state(self, repo_root, profile, variant):
        normalized = self._normalize_variant(variant, repo_root)
        width, height = _fit_size(normalized["screen_resolution"])
        base_dir = self._base_asset_dir(repo_root, normalized["base_theme_id"])
        base_targets = WALLPAPER_TARGETS.get(normalized["base_theme_id"], {})
        main_path = self._preview_image_path(
            normalized.get("wallpaper_source", ""),
            base_dir,
            base_targets.get("main", []),
        )
        charging_path = self._preview_image_path(
            normalized.get("charging_wallpaper_source", ""),
            base_dir,
            base_targets.get("charging", []),
        )
        right_pane_path = self._preview_image_path(
            normalized.get("right_pane_wallpaper_source", ""),
            base_dir,
            base_targets.get("right_pane", []),
        )
        menu_backdrop_path = self._preview_repo_image_path(
            repo_root,
            PREVIEW_BACKDROP_TARGETS.get(normalized["base_theme_id"], []),
        )
        font_label = os.path.basename(normalized.get("font_rel") or "")
        return {
            "profile_id": profile.get("id", ""),
            "theme_name": normalized["name"],
            "base_theme_id": normalized["base_theme_id"],
            "screen_resolution": normalized["screen_resolution"],
            "width": width,
            "height": height,
            "font_label": font_label,
            "colors": deepcopy(normalized["colors"]),
            "wallpaper_path": main_path,
            "charging_wallpaper_path": charging_path,
            "right_pane_wallpaper_path": right_pane_path,
            "menu_backdrop_path": menu_backdrop_path,
            "preview_modes": list(PREVIEW_MODES),
        }

    def build_bundle(self, repo_root, profile, variant):
        normalized = self._normalize_variant(variant, repo_root)
        if not normalized.get("id"):
            raise ValueError("Variant must be saved before deployment")
        bundle = self._themes.bundle_for_theme(normalized["base_theme_id"], repo_root)
        stage_root = self._stage_variant(repo_root, normalized, bundle)
        theme_name = normalized["id"]
        sbs_skin_name = self._sbs_skin_name(normalized)
        font_rel = normalized["font_rel"]
        font_abs = os.path.join(os.path.abspath(repo_root), font_rel)
        if not os.path.isfile(font_abs):
            raise ValueError(f"Font not found: {font_rel}")

        assets = []
        for kind, rel in (
            ("cfg", f"themes/{theme_name}.cfg"),
            ("wps", f"wps/{theme_name}.wps"),
            ("sbs", f"wps/{sbs_skin_name}.sbs"),
            ("fms", f"wps/{theme_name}.fms"),
            ("backdrop", f"backdrops/{theme_name}_bd.bmp"),
            ("iconset", f"icons/{theme_name}.bmp"),
            ("metadata", "metadata.json"),
        ):
            abs_path = os.path.join(stage_root, rel)
            destination = f".rockbox/{rel}"
            if kind == "metadata":
                destination = f".rockbox/rockpod/theme_designer/{theme_name}.json"
            assets.append(self._asset_record(kind, rel, destination, abs_path))

        assets.extend(self._font_assets_for_generated_skins(repo_root, stage_root, theme_name, sbs_skin_name, font_rel))

        wps_dir = os.path.join(stage_root, "wps", theme_name)
        asset_dirs = [(theme_name, wps_dir)]
        sbs_wps_dir = os.path.join(stage_root, "wps", sbs_skin_name)
        if sbs_skin_name != theme_name and os.path.isdir(sbs_wps_dir):
            asset_dirs.append((sbs_skin_name, sbs_wps_dir))
        for skin_name, skin_dir in asset_dirs:
            for root, _dirs, files in os.walk(skin_dir):
                for filename in sorted(files):
                    full = os.path.join(root, filename)
                    suffix = os.path.relpath(full, skin_dir).replace("\\", "/")
                    rel = f"wps/{skin_name}/{suffix}"
                    assets.append(self._asset_record("wps_assets", rel, f".rockbox/{rel}", full))

        glass_path = os.path.join(stage_root, "wps", "LockClockGlassGenerated.bmp")
        if os.path.isfile(glass_path):
            assets.append(
                self._asset_record(
                    "lockscreen_clock_glass",
                    "wps/LockClockGlassGenerated.bmp",
                    ".rockbox/wps/LockClockGlassGenerated.bmp",
                    glass_path,
                )
            )

        return {
            "id": normalized["id"],
            "name": normalized["name"],
            "variant": normalized,
            "assets": assets,
            "preview_path": self._preview_image_path(
                normalized.get("wallpaper_source", ""),
                os.path.join(stage_root, "wps", theme_name),
                WALLPAPER_TARGETS.get(normalized["base_theme_id"], {}).get("main", []),
            ),
        }

    def _font_assets_for_generated_skins(self, repo_root, stage_root, theme_name, sbs_skin_name, primary_font_rel):
        names = set()
        if primary_font_rel:
            names.add(os.path.basename(primary_font_rel))
        for rel in (
            f"wps/{theme_name}.wps",
            f"wps/{sbs_skin_name}.sbs",
            f"wps/{theme_name}.fms",
        ):
            names.update(self._skin_font_references(os.path.join(stage_root, rel)))

        assets = []
        for name in sorted(names):
            source_abs = os.path.join(os.path.abspath(repo_root), "fonts", name)
            assets.append(self._asset_record("font", f"fonts/{name}", f".rockbox/fonts/{name}", source_abs))
        return assets

    def _skin_font_references(self, skin_path):
        if not os.path.isfile(skin_path):
            return set()
        with open(skin_path, "r", encoding="utf-8") as handle:
            content = handle.read()

        names = set()
        for match in re.finditer(r"%Fl\(([^)]*)\)", content):
            args = [item.strip() for item in match.group(1).split(",")]
            if len(args) < 2:
                continue
            font_ref = args[1].strip("\"'")
            if not font_ref:
                continue
            names.add(os.path.basename(font_ref))
        return names

    def build_preview_bundle(self, repo_root, profile, variant):
        normalized = self._normalize_variant(variant, repo_root)
        normalized["id"] = PREVIEW_THEME_ID
        return self.build_bundle(repo_root, profile, normalized)

    def _stage_variant(self, repo_root, variant, base_bundle):
        theme_name = variant["id"]
        sbs_skin_name = self._sbs_skin_name(variant)
        generated_root = self._generated_dir(repo_root)
        os.makedirs(generated_root, exist_ok=True)
        if self._is_preview_theme_id(theme_name):
            stage_root = tempfile.mkdtemp(prefix=f"{theme_name}-", dir=generated_root)
        else:
            stage_root = os.path.join(generated_root, theme_name)
            if os.path.isdir(stage_root):
                shutil.rmtree(stage_root, ignore_errors=True)
            if os.path.isdir(stage_root):
                raise ValueError(f"Could not reset staged theme output: {stage_root}")
        os.makedirs(stage_root, exist_ok=True)
        os.makedirs(os.path.join(stage_root, "themes"), exist_ok=True)
        os.makedirs(os.path.join(stage_root, "wps"), exist_ok=True)
        os.makedirs(os.path.join(stage_root, "backdrops"), exist_ok=True)
        os.makedirs(os.path.join(stage_root, "icons"), exist_ok=True)

        base_dir = self._base_asset_dir(repo_root, variant["base_theme_id"])
        staged_wps_dir = os.path.join(stage_root, "wps", theme_name)
        shutil.copytree(base_dir, staged_wps_dir)

        self._apply_wallpaper_overrides(staged_wps_dir, variant)
        staged_lockscreen = os.path.join(staged_wps_dir, "Wallpaper.bmp")
        if os.path.isfile(staged_lockscreen):
            variant["_staged_lockscreen_wallpaper_source"] = staged_lockscreen
        self._apply_sprite_color_overrides(staged_wps_dir, variant)
        self._write_stock_battery_asset(staged_wps_dir, variant)
        if sbs_skin_name != theme_name:
            staged_sbs_wps_dir = os.path.join(stage_root, "wps", sbs_skin_name)
            if os.path.isdir(staged_sbs_wps_dir):
                shutil.rmtree(staged_sbs_wps_dir)
            shutil.copytree(staged_wps_dir, staged_sbs_wps_dir)
        self._write_iconset(stage_root, variant, base_bundle)
        self._write_cfg(stage_root, variant, base_bundle)
        self._copy_template(base_bundle, "wps", stage_root, f"wps/{theme_name}.wps", variant)
        self._copy_template(base_bundle, "sbs", stage_root, f"wps/{sbs_skin_name}.sbs", variant)
        self._apply_right_pane_video_skin(os.path.join(stage_root, f"wps/{sbs_skin_name}.sbs"), variant)
        self._copy_template(base_bundle, "fms", stage_root, f"wps/{theme_name}.fms", variant)
        self._write_backdrop(stage_root, variant, staged_wps_dir)
        self._write_metadata(stage_root, variant)
        return stage_root

    @staticmethod
    def _sbs_skin_name(variant):
        theme_name = str(variant.get("id") or "").strip()
        if str(variant.get("base_theme_id") or "").startswith("iPone"):
            if len(theme_name) <= 12:
                return f"iPoneD-{theme_name}"
            digest = hashlib.sha1(theme_name.encode("utf-8", errors="ignore")).hexdigest()[:6]
            return f"iPoneD-{theme_name[:10]}-{digest}"
        return theme_name

    @staticmethod
    def _is_preview_theme_id(theme_name):
        return str(theme_name or "") == PREVIEW_THEME_ID or str(theme_name or "").startswith("theme-designer-preview-")

    def _apply_wallpaper_overrides(self, staged_wps_dir, variant):
        targets = WALLPAPER_TARGETS.get(variant["base_theme_id"], {})
        sbs_solid_targets = set(SBS_SOLID_BACKGROUND_TARGETS.get(variant["base_theme_id"], ()))
        protected_full_art_targets = {"iPone_bd_fullart.bmp"}
        if not sbs_solid_targets:
            sbs_solid_targets = {
                name
                for name in targets.get("main", ())
                if os.path.basename(name) in {"iPone_bd.bmp", "SbsBackdrop.bmp", "iPone_bd_fullart.bmp"}
            }
        if variant.get("wallpaper_source"):
            for name in targets.get("main", []):
                if name in sbs_solid_targets or os.path.basename(name) in protected_full_art_targets:
                    continue
                self._render_image(
                    variant["wallpaper_source"],
                    os.path.join(staged_wps_dir, name),
                    variant["screen_resolution"],
                    variant.get("fit_mode", "fill"),
                    variant["colors"]["background"],
                )
            for name in sbs_solid_targets:
                self._render_sbs_background_image(
                    os.path.join(staged_wps_dir, name),
                    variant["screen_resolution"],
                    variant,
                )
        else:
            if self._uses_customized_colors(variant):
                for name in targets.get("main", []):
                    if name in sbs_solid_targets or os.path.basename(name) in protected_full_art_targets:
                        continue
                    self._render_solid_image(
                        os.path.join(staged_wps_dir, name),
                        variant["screen_resolution"],
                        variant["colors"]["background"],
                    )
            for name in sbs_solid_targets:
                self._render_sbs_background_image(
                    os.path.join(staged_wps_dir, name),
                    variant["screen_resolution"],
                    variant,
                )
        if variant.get("charging_wallpaper_source"):
            for name in targets.get("charging", []):
                self._render_image(
                    variant["charging_wallpaper_source"],
                    os.path.join(staged_wps_dir, name),
                    variant["screen_resolution"],
                    variant.get("charging_fit_mode", "fill"),
                    variant["colors"]["background"],
                )
        if variant.get("right_pane_wallpaper_source"):
            sbs_right_pane_targets = [
                name for name in sbs_solid_targets
                if os.path.basename(name) != "iPone_bd_fullart.bmp"
            ]
            if not sbs_right_pane_targets:
                sbs_right_pane_targets = targets.get("right_pane", [])
            for name in sbs_right_pane_targets:
                self._render_right_pane_image(
                    variant["right_pane_wallpaper_source"],
                    os.path.join(staged_wps_dir, name),
                    variant["screen_resolution"],
                    variant.get("right_pane_fit_mode", "fill"),
                    variant["colors"]["background"],
                    os.path.join(staged_wps_dir, name),
                    variant.get("right_pane_offset_x", 0),
                    variant.get("right_pane_offset_y", 0),
                )
            for name in targets.get("right_pane", []):
                self._render_right_pane_image(
                    variant["right_pane_wallpaper_source"],
                    os.path.join(staged_wps_dir, name),
                    variant["screen_resolution"],
                    variant.get("right_pane_fit_mode", "fill"),
                    variant["colors"]["background"],
                    os.path.join(staged_wps_dir, "iPone_bd.bmp"),
                    variant.get("right_pane_offset_x", 0),
                    variant.get("right_pane_offset_y", 0),
                )
        if variant.get("right_pane_video_source"):
            self._apply_right_pane_video_overrides(
                staged_wps_dir,
                variant,
                targets,
                sbs_solid_targets,
            )

    @staticmethod
    def _uses_customized_colors(variant):
        if variant.get("color_profile") and variant.get("color_profile") != "default":
            return True
        colors = variant.get("colors") or {}
        for key, default in DEFAULT_COLORS.items():
            if _ensure_hex(colors.get(key), default) != _ensure_hex(default, default):
                return True
        return False

    def _apply_sprite_color_overrides(self, staged_wps_dir, variant):
        if variant.get("color_profile") == "default":
            return
        for filename in TINTED_SPRITE_ASSETS:
            path = os.path.join(staged_wps_dir, filename)
            if os.path.isfile(path):
                self._tint_luminance_bitmap(path, variant["colors"]["background"], variant["colors"]["selector_end"])

    def _write_cfg(self, stage_root, variant, base_bundle):
        cfg_asset = next((item for item in base_bundle["assets"] if item["kind"] == "cfg"), None)
        if not cfg_asset:
            raise ValueError("Base theme cfg missing")
        with open(cfg_asset["source_abs"], "r", encoding="utf-8") as handle:
            lines = handle.readlines()

        cfg_background = "FFFFFF" if variant.get("appearance_mode") == "light" else variant["colors"]["background"]
        cfg_foreground = "15121D" if variant.get("appearance_mode") == "light" else variant["colors"]["foreground"]
        separator_height = "1" if variant.get("show_line_separators", True) else "0"
        if variant.get("right_pane_video_source"):
            separator_height = "0"
        backdrop_value = f"/.rockbox/backdrops/{variant['id']}_bd.bmp"
        if str(variant.get("base_theme_id") or "").startswith("iPone"):
            backdrop_value = "-"
        overrides = {
            "wps": f"/.rockbox/wps/{variant['id']}.wps",
            "sbs": f"/.rockbox/wps/{self._sbs_skin_name(variant)}.sbs",
            "fms": f"/.rockbox/wps/{variant['id']}.fms",
            "backdrop": backdrop_value,
            "font": f"/.rockbox/fonts/{os.path.basename(variant['font_rel'])}",
            "background color": cfg_background,
            "foreground color": cfg_foreground,
            "iconset": f"/.rockbox/icons/{variant['id']}.bmp",
            "line selector start color": variant["colors"]["selector_start"],
            "line selector end color": variant["colors"]["selector_end"],
            "line selector text color": variant["colors"]["selector_text"],
            "list separator color": variant["colors"]["list_separator"],
            "list separator height": separator_height,
            "statusbar": "off",
            "ui viewport": "-",
        }
        right_pane_mode = self._normalize_right_pane_mode(
            variant.get("right_pane_mode"),
            base_default=variant.get("base_right_pane_mode", DEFAULT_RIGHT_PANE_MODE),
        )
        overrides["ipone right pane"] = right_pane_mode

        written = set()
        output = []
        for raw in lines:
            stripped = raw.strip()
            lower = stripped.lower()
            replaced = False
            for key, value in overrides.items():
                if lower.startswith(f"{key}:"):
                    output.append(f"{key}: {value}\n")
                    written.add(key)
                    replaced = True
                    break
            if not replaced:
                output.append(raw)
        for key, value in overrides.items():
            if key not in written:
                output.append(f"{key}: {value}\n")

        theme_label = (str(variant.get("name") or variant["id"]).strip() or variant["id"] or "Custom iPone")
        variant_id = str(variant.get("id") or "").strip()
        if variant_id and variant_id not in theme_label:
            theme_label = f"{theme_label} ({variant_id})"
        theme_label = re.sub(r"\s+", " ", theme_label).strip()[:140]
        updated_output = []
        replaced_theme_comment = False
        for line in output:
            if line.startswith("# Theme:"):
                updated_output.append(f"# Theme: {theme_label}\n")
                replaced_theme_comment = True
            else:
                updated_output.append(line)
        if not replaced_theme_comment:
            updated_output.insert(0, f"# Theme: {theme_label}\n")
        output = updated_output

        path = os.path.join(stage_root, "themes", f"{variant['id']}.cfg")
        atomic_write_text(path, "".join(output))

    def _copy_template(self, base_bundle, kind, stage_root, dest_rel, variant=None):
        asset = next((item for item in base_bundle["assets"] if item["kind"] == kind), None)
        if not asset:
            raise ValueError(f"Base theme {kind} missing")
        target = os.path.join(stage_root, dest_rel)
        os.makedirs(os.path.dirname(target), exist_ok=True)
        shutil.copy2(asset["source_abs"], target)
        if target.lower().endswith((".wps", ".sbs", ".fms")):
            self._apply_template_color_overrides(target, base_bundle["id"], stage_root, variant)
        if variant and target.lower().endswith((".wps", ".sbs")):
            self._apply_lockscreen_clock_overrides(target, variant)
        if variant and target.lower().endswith(".sbs"):
            self._apply_designer_sbs_layout_overrides(target, variant)

    def _write_iconset(self, stage_root, variant, base_bundle):
        asset = next((item for item in base_bundle["assets"] if item["kind"] == "iconset"), None)
        if not asset or not asset.get("exists"):
            return
        target = os.path.join(stage_root, "icons", f"{variant['id']}.bmp")
        if variant.get("color_profile") == "default":
            shutil.copy2(asset["source_abs"], target)
            return
        try:
            with Image.open(asset["source_abs"]) as img:
                source = img.convert("RGB")
        except (OSError, UnidentifiedImageError):
            shutil.copy2(asset["source_abs"], target)
            return

        output = self._replace_purple_accent_image(source, variant["colors"]["selector_end"])
        if output is None:
            shutil.copy2(asset["source_abs"], target)
            return
        output.save(target, "BMP")

    def _tint_luminance_bitmap(self, path, low_hex, high_hex):
        try:
            with Image.open(path) as img:
                source = img.convert("RGB")
        except (OSError, UnidentifiedImageError):
            return

        output = self._replace_purple_accent_image(source, high_hex)
        if output is None:
            return
        output.save(path, "BMP")

    def _replace_purple_accent_image(self, source, accent_hex):
        target_red, target_green, target_blue = _hex_to_rgb(accent_hex)
        target_hue, target_sat, target_val = colorsys.rgb_to_hsv(
            target_red / 255.0,
            target_green / 255.0,
            target_blue / 255.0,
        )
        if source.mode == "P" and source.getpalette():
            palette = list(source.getpalette())
            changed = False
            for index in range(0, len(palette), 3):
                red, green, blue = palette[index:index + 3]
                hue, sat, val = colorsys.rgb_to_hsv(red / 255.0, green / 255.0, blue / 255.0)
                is_stock_purple = (
                    sat >= 0.18
                    and 0.66 <= hue <= 0.86
                    and blue >= green + 10
                    and red >= green - 4
                )
                if not is_stock_purple:
                    continue
                new_sat = max(0.16, min(1.0, sat * (0.72 + target_sat * 0.38)))
                new_val = max(0.0, min(1.0, val * (0.86 + target_val * 0.18)))
                new_red, new_green, new_blue = colorsys.hsv_to_rgb(target_hue, new_sat, new_val)
                palette[index:index + 3] = [
                    int(round(new_red * 255)),
                    int(round(new_green * 255)),
                    int(round(new_blue * 255)),
                ]
                changed = True
            if not changed:
                return None
            output = source.copy()
            output.putpalette(palette)
            return output

        source = source.convert("RGB")
        pixels = []
        changed = False
        for red, green, blue in source.getdata():
            hue, sat, val = colorsys.rgb_to_hsv(red / 255.0, green / 255.0, blue / 255.0)
            is_stock_purple = (
                sat >= 0.18
                and 0.66 <= hue <= 0.86
                and blue >= green + 10
                and red >= green - 4
            )
            if not is_stock_purple:
                pixels.append((red, green, blue))
                continue

            new_sat = max(0.16, min(1.0, sat * (0.72 + target_sat * 0.38)))
            new_val = max(0.0, min(1.0, val * (0.86 + target_val * 0.18)))
            new_red, new_green, new_blue = colorsys.hsv_to_rgb(target_hue, new_sat, new_val)
            pixels.append(
                (int(round(new_red * 255)), int(round(new_green * 255)), int(round(new_blue * 255)))
            )
            changed = True
        if not changed:
            return None
        output = Image.new("RGB", source.size)
        output.putdata(pixels)
        return output

    def _write_stock_battery_asset(self, staged_wps_dir, variant):
        path = os.path.join(staged_wps_dir, "Battery.bmp")
        if variant.get("color_profile") == "default":
            return
        if not os.path.isfile(path):
            return
        try:
            with Image.open(path) as source:
                image = source.convert("RGB")
        except (OSError, UnidentifiedImageError):
            return
        image = self._replace_purple_accent_image(image, variant["colors"]["selector_end"])
        if image is None:
            return
        if should_render_2bpp_greyscale(variant["screen_resolution"]):
            image = render_2bpp_greyscale(image)
        image.save(path, "BMP")

    def _write_backdrop(self, stage_root, variant, staged_wps_dir):
        backdrop_path = os.path.join(stage_root, "backdrops", f"{variant['id']}_bd.bmp")
        source = ""
        if variant.get("base_theme_id") == "iPoneCustom":
            candidate = os.path.join(staged_wps_dir, "iPone_bd.bmp")
            if os.path.isfile(candidate):
                source = candidate
        if not source and variant.get("wallpaper_source"):
            source = variant["wallpaper_source"]
        if not source:
            for name in WALLPAPER_TARGETS.get(variant["base_theme_id"], {}).get("main", []):
                candidate = os.path.join(staged_wps_dir, name)
                if os.path.isfile(candidate):
                    source = candidate
                    break
        if not source:
            raise ValueError("No wallpaper available for backdrop generation")
        self._render_image(
            source,
            backdrop_path,
            variant["screen_resolution"],
            variant.get("fit_mode", "fill"),
            variant["colors"]["background"],
        )

    def _write_metadata(self, stage_root, variant):
        path = os.path.join(stage_root, "metadata.json")
        atomic_write_json(
            path,
            {
                "id": variant["id"],
                "name": variant["name"],
                "base_theme_id": variant["base_theme_id"],
                "screen_resolution": variant["screen_resolution"],
                "wallpaper_source": variant.get("wallpaper_source", ""),
                "charging_wallpaper_source": variant.get("charging_wallpaper_source", ""),
                "right_pane_wallpaper_source": variant.get("right_pane_wallpaper_source", ""),
                "right_pane_video_source": variant.get("right_pane_video_source", ""),
                "right_pane_offset_x": variant.get("right_pane_offset_x", 0),
                "right_pane_offset_y": variant.get("right_pane_offset_y", 0),
                "show_line_separators": variant.get("show_line_separators", True),
                "lockscreen_clock": deepcopy(variant.get("lockscreen_clock", DEFAULT_LOCKSCREEN_CLOCK)),
                "color_profile": variant.get("color_profile", "custom"),
                "appearance_mode": variant.get("appearance_mode", "dark"),
            },
        )

    def _preview_image_path(self, explicit_source, base_dir, candidates):
        explicit = os.path.abspath(explicit_source or "")
        if explicit and os.path.isfile(explicit):
            return explicit
        for name in candidates:
            candidate = os.path.join(base_dir, name)
            if os.path.isfile(candidate):
                return candidate
        return ""

    @staticmethod
    def _preview_repo_image_path(repo_root, candidates):
        root = os.path.abspath(repo_root)
        for rel in candidates:
            candidate = os.path.join(root, rel)
            if os.path.isfile(candidate):
                return candidate
        return ""

    def _normalize_variant(self, data, repo_root):
        item = deepcopy(data or {})
        name = str(item.get("name") or "iPone Custom").strip() or "iPone Custom"
        resolution = str(item.get("screen_resolution") or "320x240").strip() or "320x240"
        base_theme_id = str(item.get("base_theme_id") or BASE_THEME_BY_RESOLUTION.get(resolution, "iPone")).strip()
        base = self.load_base_theme(repo_root, {"screen_resolution": resolution})
        base["name"] = name
        base["id"] = str(item.get("id") or "").strip()
        base["base_theme_id"] = base_theme_id
        base["screen_resolution"] = resolution
        base["font_rel"] = self._normalize_font_rel(item.get("font_rel") or base["font_rel"])
        base["fit_mode"] = self._normalize_fit_mode(item.get("fit_mode") or base["fit_mode"])
        base["charging_fit_mode"] = self._normalize_fit_mode(item.get("charging_fit_mode") or base["charging_fit_mode"])
        base["right_pane_fit_mode"] = self._normalize_fit_mode(item.get("right_pane_fit_mode") or base["right_pane_fit_mode"])
        base["base_right_pane_mode"] = base["right_pane_mode"]
        requested_right_pane_mode = str(item.get("right_pane_mode") or "").strip()
        raw_right_pane_wallpaper_source = str(item.get("right_pane_wallpaper_source") or "").strip()
        raw_right_pane_video_source = str(item.get("right_pane_video_source") or "").strip()
        if requested_right_pane_mode:
            right_pane_mode = self._normalize_right_pane_mode(
                requested_right_pane_mode,
                base_default=base.get("right_pane_mode", DEFAULT_RIGHT_PANE_MODE),
            )
        else:
            right_pane_mode = self._normalize_right_pane_mode(
                "",
                base_default=base.get("right_pane_mode", DEFAULT_RIGHT_PANE_MODE),
            )
        base["right_pane_mode"] = right_pane_mode
        base["right_pane_offset_x"] = self._normalize_offset(item.get("right_pane_offset_x", base.get("right_pane_offset_x", 0)))
        base["right_pane_offset_y"] = self._normalize_offset(item.get("right_pane_offset_y", base.get("right_pane_offset_y", 0)))
        base["show_line_separators"] = bool(item.get("show_line_separators", base.get("show_line_separators", True)))
        base["lockscreen_clock"] = self._normalize_lockscreen_clock(item.get("lockscreen_clock") or base.get("lockscreen_clock"))
        base["wallpaper_source"] = os.path.abspath(str(item.get("wallpaper_source") or "").strip()) if item.get("wallpaper_source") else ""
        base["charging_wallpaper_source"] = os.path.abspath(str(item.get("charging_wallpaper_source") or "").strip()) if item.get("charging_wallpaper_source") else ""
        base["right_pane_wallpaper_source"] = os.path.abspath(raw_right_pane_wallpaper_source) if raw_right_pane_wallpaper_source else ""
        base["right_pane_video_source"] = os.path.abspath(raw_right_pane_video_source) if raw_right_pane_video_source else ""
        base["color_profile"] = str(item.get("color_profile") or base.get("color_profile") or "custom").strip() or "custom"
        appearance_mode = str(item.get("appearance_mode") or base.get("appearance_mode") or "dark").strip().lower()
        base["appearance_mode"] = "light" if appearance_mode == "light" else "dark"
        colors = dict(base["colors"])
        colors.update(item.get("colors") or {})
        base["colors"] = {
            "background": _ensure_hex(colors.get("background"), DEFAULT_COLORS["background"]),
            "foreground": _ensure_hex(colors.get("foreground"), DEFAULT_COLORS["foreground"]),
            "selector_start": _ensure_hex(colors.get("selector_start"), DEFAULT_COLORS["selector_start"]),
            "selector_end": _ensure_hex(colors.get("selector_end"), DEFAULT_COLORS["selector_end"]),
            "selector_text": _ensure_hex(colors.get("selector_text"), DEFAULT_COLORS["selector_text"]),
            "list_separator": _ensure_hex(colors.get("list_separator"), DEFAULT_COLORS["list_separator"]),
            "sbs_clock": _ensure_hex(
                colors.get("sbs_clock"),
                _ensure_hex(colors.get("foreground"), DEFAULT_COLORS["sbs_clock"]),
            ),
        }
        return base

    def _normalize_lockscreen_clock(self, value):
        source = value if isinstance(value, dict) else {}
        result = deepcopy(DEFAULT_LOCKSCREEN_CLOCK)
        result["font_rel"] = self._normalize_lockscreen_clock_font_rel(
            source.get("font_rel") or source.get("font") or result["font_rel"]
        )
        result["x"] = self._int_between(source.get("x"), 0, 319, result["x"])
        result["y"] = self._int_between(source.get("y"), 0, 180, result["y"])
        result["width"] = self._int_between(source.get("width"), 40, 320, result["width"])
        result["height"] = self._int_between(source.get("height"), 12, 120, result["height"])
        align = str(source.get("align") or result["align"]).strip().lower()
        result["align"] = align if align in {"left", "center", "right"} else DEFAULT_LOCKSCREEN_CLOCK["align"]
        result["color"] = _ensure_hex(source.get("color"), result["color"])
        style = str(source.get("style") or result["style"]).strip().lower()
        result["style"] = style if style in {"solid", "soft shadow", "outline", "glass", "glass tinted"} else DEFAULT_LOCKSCREEN_CLOCK["style"]
        glass = str(source.get("glass_strength") or result["glass_strength"]).strip().lower()
        result["glass_strength"] = glass if glass in {"off", "low", "medium", "high"} else DEFAULT_LOCKSCREEN_CLOCK["glass_strength"]
        result["stretch"] = self._int_between(source.get("stretch"), 100, 160, result["stretch"])
        result["opacity"] = self._int_between(source.get("opacity"), 20, 100, result["opacity"])
        return result

    def readability_recommendations(self, repo_root, profile, variant_data):
        variant = self._normalize_variant(variant_data, repo_root)
        issues = []
        colors = variant["colors"]
        left_background = "FFFFFF" if variant.get("appearance_mode") == "light" else "000000"
        self._add_contrast_issue(
            issues,
            "Menu text",
            "colors.foreground",
            colors["foreground"],
            self._rgb_from_hex(left_background),
            "left menu background",
        )
        self._add_contrast_issue(
            issues,
            "Selected menu text",
            "colors.selector_text",
            colors["selector_text"],
            self._rgb_from_hex(colors["selector_end"]),
            "selected row",
        )

        right_wallpaper = str(variant.get("right_pane_wallpaper_source") or "").strip()
        if right_wallpaper and os.path.isfile(right_wallpaper):
            right_rgb = self._average_image_rgb(right_wallpaper)
            if right_rgb:
                self._add_contrast_issue(
                    issues,
                    "Right-side clock/miniplayer text",
                    "colors.sbs_clock",
                    colors["sbs_clock"],
                    right_rgb,
                    "right-side wallpaper",
                )

        lock_wallpaper = str(variant.get("wallpaper_source") or "").strip()
        if lock_wallpaper and os.path.isfile(lock_wallpaper):
            lock_rgb = self._average_image_rgb(lock_wallpaper)
            if lock_rgb:
                clock = self._normalize_lockscreen_clock(variant.get("lockscreen_clock"))
                self._add_contrast_issue(
                    issues,
                    "Lockscreen clock",
                    "lockscreen_clock.color",
                    clock["color"],
                    lock_rgb,
                    "lockscreen wallpaper",
                    minimum=3.0,
                )
        return issues

    def readability_recommendations_for_bundle(self, repo_root, profile, bundle):
        variant = dict((bundle or {}).get("variant") or {})
        issues = self.readability_recommendations(repo_root, profile, variant)
        for asset in (bundle or {}).get("assets", []):
            if asset.get("kind") not in {"sbs", "wps"}:
                continue
            skin_path = asset.get("source_abs") or ""
            if not os.path.isfile(skin_path):
                continue
            issues.extend(self._skin_readability_issues(skin_path, asset.get("kind"), variant))
        return self._dedupe_readability_issues(issues)

    def _skin_readability_issues(self, skin_path, skin_kind, variant):
        screen_width, screen_height = _fit_size(variant.get("screen_resolution") or "320x240")
        skin_name = os.path.splitext(os.path.basename(skin_path))[0]
        image_dir = os.path.join(os.path.dirname(skin_path), skin_name)
        entries = self._skin_text_viewports(skin_path, screen_width, screen_height)
        issues = []
        for entry in entries:
            target = self._readability_target_for_viewport(entry, skin_kind, screen_width)
            if not target:
                continue
            background = self._skin_background_for_viewport(
                image_dir,
                skin_kind,
                variant,
                entry,
                screen_width,
            )
            if not background:
                continue
            self._add_contrast_region_issue(
                issues,
                f"{skin_kind.upper()} {entry['name']} font",
                target,
                entry["color"],
                background,
                entry["x"],
                entry["y"],
                entry["width"],
                entry["height"],
                minimum=self._minimum_contrast_for_font(entry),
            )
        return issues

    def _skin_text_viewports(self, skin_path, screen_width, screen_height):
        try:
            with open(skin_path, "r", encoding="utf-8", errors="replace") as handle:
                lines = handle.readlines()
        except OSError:
            return []

        fonts = self._skin_font_map(lines)
        entries = []
        current = None
        emitted = set()
        for raw in lines:
            line = raw.strip()
            viewport = self._parse_viewport_line(line, screen_width, screen_height)
            if viewport:
                current = viewport
            elif line.startswith("%V(") or line.startswith("%Vi("):
                current = None

            if current is None:
                continue
            color = self._last_skin_foreground(line)
            if color:
                current["color"] = color
            if not current.get("color") or not self._skin_line_has_text(line):
                continue
            current["font_name"] = fonts.get(str(current.get("font_slot") or "").strip(), "")
            key = (current["name"], current["x"], current["y"], current["color"])
            if key in emitted:
                continue
            emitted.add(key)
            entries.append(dict(current))
        return entries

    @staticmethod
    def _skin_font_map(lines):
        fonts = {}
        for raw in lines:
            match = re.search(r"%Fl\(([^)]*)\)", str(raw or ""))
            if not match:
                continue
            parts = [part.strip() for part in match.group(1).split(",")]
            if len(parts) >= 2:
                fonts[parts[0]] = os.path.basename(parts[1])
        return fonts

    @staticmethod
    def _parse_viewport_line(line, screen_width, screen_height):
        match = re.search(r"%(?:Vl|Vi)\(([^)]*)\)", line)
        if not match:
            return None
        parts = [part.strip() for part in match.group(1).split(",")]
        if len(parts) < 6:
            return None

        def parse_coord(value, default=0):
            text = str(value or "").strip()
            if text == "-":
                return default
            try:
                return int(float(text))
            except (TypeError, ValueError):
                return default

        name = parts[0]
        x = parse_coord(parts[1], 0)
        y = parse_coord(parts[2], 0)
        width = parse_coord(parts[3], screen_width - x)
        height = parse_coord(parts[4], screen_height - y)
        return {
            "name": name,
            "x": max(0, min(screen_width - 1, x)),
            "y": max(0, min(screen_height - 1, y)),
            "width": max(1, min(screen_width - max(0, x), width)),
            "height": max(1, min(screen_height - max(0, y), height)),
            "font_slot": parts[5],
            "color": ThemeDesignerService._last_skin_foreground(line),
        }

    @staticmethod
    def _last_skin_foreground(line):
        matches = re.findall(r"%Vf\(([0-9A-Fa-f]{6})(?:[0-9A-Fa-f]{2})?\)", line)
        return matches[-1].upper() if matches else ""

    @staticmethod
    def _skin_line_has_text(line):
        if not line or line.startswith("#"):
            return False
        if line.startswith(("%xl(", "%xd(", "%Cl(", "%Cd(", "%dr(", "%pb(", "%pv(")):
            return False
        text_tokens = (
            "%it",
            "%fn",
            "%ia",
            "%id",
            "%cl",
            "%cM",
            "%cP",
            "%Lt",
            "%bl",
            "%pc",
            "%pt",
            "%tf",
            "%Tn",
            "Now Playing",
            "HOLD",
            "FM Radio",
            "Signal",
        )
        if any(token in line for token in text_tokens):
            return True
        stripped = re.sub(r"%[A-Za-z?][A-Za-z0-9]*(?:\([^)]*\))?", " ", line)
        stripped = re.sub(r"[<|>;:=,/%()0-9_-]+", " ", stripped)
        return bool(re.search(r"[A-Za-z]{2,}", stripped))

    @staticmethod
    def _is_large_font_slot(slot):
        try:
            number = int(str(slot).strip())
        except (TypeError, ValueError):
            return False
        return number >= 8

    @staticmethod
    def _minimum_contrast_for_font(entry):
        font_name = str(entry.get("font_name") or "").lower()
        minimum = 3.0 if ThemeDesignerService._is_large_font_slot(entry.get("font_slot")) else 4.5
        if any(marker in font_name for marker in ("light", "thin", "extralight", "ultralight")):
            minimum = max(minimum, 4.5)
        return minimum

    @staticmethod
    def _readability_target_for_viewport(entry, skin_kind, screen_width):
        lowered = str((entry or {}).get("name") or "").strip().lower()
        if not lowered or "shadow" in lowered:
            return ""
        if lowered in {"clock"}:
            return "colors.sbs_clock"
        if lowered in {"minititle", "minisub", "nowplayingbadge", "radio"}:
            return "colors.sbs_clock"
        if lowered in {"t", "n"}:
            return "colors.foreground"
        if lowered in {"iponelockscreen", "lockscreen"}:
            return "lockscreen_clock.color"
        if lowered in {"lockmusic", "lockradio", "chargenowplaying"}:
            return "colors.selector_text"
        if lowered in {"alwaysondisplay", "aodnowplayingcard", "chargeclock", "chargemeta"}:
            return "colors.foreground"
        if skin_kind == "sbs" and int((entry or {}).get("x") or 0) >= max(1, screen_width // 2):
            return "colors.sbs_clock"
        return "colors.foreground"

    @staticmethod
    def _skin_background_for_viewport(image_dir, skin_kind, variant, entry, screen_width):
        lowered = str((entry or {}).get("name") or "").strip().lower()
        if skin_kind == "sbs":
            if "charge" in lowered:
                for candidate in ("ChargeWallpaper.bmp", "ChargeWallpaperAlt.bmp", "ChargeWallpaperThird.bmp", "ChargeWallpaperFourth.bmp"):
                    path = os.path.join(image_dir, candidate)
                    if os.path.isfile(path):
                        return path
            if lowered in {"iponelockscreen", "lockmusic", "lockradio", "lockplayer"}:
                path = os.path.join(image_dir, "Wallpaper.bmp")
                if os.path.isfile(path):
                    return path
            if lowered in {"iponeaod", "aodnowplayingcard"}:
                path = os.path.join(image_dir, "AODBackdrop.bmp")
                if os.path.isfile(path):
                    return path
            if lowered in {"clock", "minititle", "minisub", "nowplayingbadge", "radio", "t", "n"}:
                right_mode = str(variant.get("right_pane_mode") or DEFAULT_RIGHT_PANE_MODE).lower()
                candidate = "iPone_bd_fullart.bmp" if right_mode == "full art" else "iPone_bd.bmp"
                path = os.path.join(image_dir, candidate)
                if os.path.isfile(path):
                    return path
                fallback = os.path.join(image_dir, "RightPaneWallpaper.bmp")
                if os.path.isfile(fallback):
                    return fallback
            if int((entry or {}).get("x") or 0) >= max(1, screen_width // 2):
                for candidate in ("iPone_bd_fullart.bmp", "iPone_bd.bmp", "RightPaneWallpaper.bmp", "SbsBackdrop.bmp"):
                    path = os.path.join(image_dir, candidate)
                    if os.path.isfile(path):
                        return path
            for candidate in ("iPone_bd.bmp", "SbsBackdrop.bmp"):
                path = os.path.join(image_dir, candidate)
                if os.path.isfile(path):
                    return path
        if skin_kind == "wps" and lowered in {"lockscreen"}:
            path = os.path.join(image_dir, "Wallpaper.bmp")
            if os.path.isfile(path):
                return path
        if skin_kind == "wps" and lowered in {"alwaysondisplay"}:
            path = os.path.join(image_dir, "AODBackdrop.bmp")
            if os.path.isfile(path):
                return path
        return ""

    @staticmethod
    def _average_image_region_rgb(path, x, y, width, height):
        try:
            with Image.open(path) as image:
                source = image.convert("RGB")
                left = max(0, min(source.width - 1, int(x)))
                top = max(0, min(source.height - 1, int(y)))
                right = max(left + 1, min(source.width, left + int(width)))
                bottom = max(top + 1, min(source.height, top + int(height)))
                sampled = source.crop((left, top, right, bottom))
                sampled.thumbnail((48, 48))
                pixels = list(sampled.getdata())
        except (OSError, UnidentifiedImageError):
            return None
        if not pixels:
            return None
        red = sum(pixel[0] for pixel in pixels) / len(pixels)
        green = sum(pixel[1] for pixel in pixels) / len(pixels)
        blue = sum(pixel[2] for pixel in pixels) / len(pixels)
        return (int(round(red)), int(round(green)), int(round(blue)))

    def _add_contrast_region_issue(self, issues, label, target, current_hex, background_path, x, y, width, height, minimum=4.5):
        stats = self._contrast_region_stats(background_path, x, y, width, height, current_hex)
        if not stats:
            return
        ratio = stats["ratio"]
        if ratio >= minimum:
            return
        recommended = self._best_text_hex(stats["background_rgb"])
        recommended_ratio = self._contrast_ratio(self._rgb_from_hex(recommended), stats["background_rgb"])
        if recommended_ratio <= ratio:
            return
        issues.append(
            {
                "label": label,
                "target": target,
                "current": _ensure_hex(current_hex, "FFFFFF"),
                "recommended": recommended,
                "ratio": ratio,
                "recommended_ratio": recommended_ratio,
                "background": f"{os.path.basename(background_path)} region {int(x)},{int(y)},{int(width)}x{int(height)}",
                "minimum": minimum,
            }
        )

    @classmethod
    def _contrast_region_stats(cls, path, x, y, width, height, foreground_hex):
        try:
            with Image.open(path) as image:
                source = image.convert("RGB")
                left = max(0, min(source.width - 1, int(x)))
                top = max(0, min(source.height - 1, int(y)))
                right = max(left + 1, min(source.width, left + int(width)))
                bottom = max(top + 1, min(source.height, top + int(height)))
                crop = source.crop((left, top, right, bottom))
        except (OSError, UnidentifiedImageError):
            return None
        if crop.width <= 0 or crop.height <= 0:
            return None
        foreground_rgb = cls._rgb_from_hex(foreground_hex)
        tile = max(4, min(16, max(4, min(crop.width, crop.height) // 3)))
        samples = []
        for top in range(0, crop.height, tile):
            for left in range(0, crop.width, tile):
                region = crop.crop((left, top, min(crop.width, left + tile), min(crop.height, top + tile)))
                pixels = list(region.getdata())
                if not pixels:
                    continue
                background_rgb = (
                    int(round(sum(pixel[0] for pixel in pixels) / len(pixels))),
                    int(round(sum(pixel[1] for pixel in pixels) / len(pixels))),
                    int(round(sum(pixel[2] for pixel in pixels) / len(pixels))),
                )
                samples.append((cls._contrast_ratio(foreground_rgb, background_rgb), background_rgb))
        if not samples:
            return None
        samples.sort(key=lambda item: item[0])
        index = min(len(samples) - 1, max(0, int(round((len(samples) - 1) * 0.10))))
        ratio, background_rgb = samples[index]
        return {"ratio": ratio, "background_rgb": background_rgb}

    @staticmethod
    def _dedupe_readability_issues(issues):
        deduped = []
        seen = set()
        for issue in issues:
            key = (issue.get("label"), issue.get("target"), issue.get("current"), issue.get("background"))
            if key in seen:
                continue
            seen.add(key)
            deduped.append(issue)
        return deduped

    def _add_contrast_issue(self, issues, label, target, current_hex, background_rgb, background_label, minimum=4.5):
        current_rgb = self._rgb_from_hex(current_hex)
        ratio = self._contrast_ratio(current_rgb, background_rgb)
        if ratio >= minimum:
            return
        recommended = self._best_text_hex(background_rgb)
        recommended_ratio = self._contrast_ratio(self._rgb_from_hex(recommended), background_rgb)
        if recommended_ratio <= ratio:
            return
        issues.append(
            {
                "label": label,
                "target": target,
                "current": _ensure_hex(current_hex, "FFFFFF"),
                "recommended": recommended,
                "ratio": ratio,
                "recommended_ratio": recommended_ratio,
                "background": background_label,
                "minimum": minimum,
            }
        )

    @staticmethod
    def _rgb_from_hex(value):
        return ImageColor.getrgb(f"#{_ensure_hex(value, '000000')}")

    @staticmethod
    def _relative_luminance(rgb):
        values = []
        for component in rgb:
            channel = component / 255.0
            values.append(channel / 12.92 if channel <= 0.03928 else ((channel + 0.055) / 1.055) ** 2.4)
        return 0.2126 * values[0] + 0.7152 * values[1] + 0.0722 * values[2]

    @classmethod
    def _contrast_ratio(cls, foreground_rgb, background_rgb):
        first = cls._relative_luminance(foreground_rgb)
        second = cls._relative_luminance(background_rgb)
        light = max(first, second)
        dark = min(first, second)
        return (light + 0.05) / (dark + 0.05)

    @classmethod
    def _best_text_hex(cls, background_rgb):
        white_ratio = cls._contrast_ratio((255, 255, 255), background_rgb)
        black_ratio = cls._contrast_ratio((0, 0, 0), background_rgb)
        return "FFFFFF" if white_ratio >= black_ratio else "000000"

    @staticmethod
    def _average_image_rgb(path):
        try:
            with Image.open(path) as image:
                sampled = image.convert("RGB")
                sampled.thumbnail((48, 48))
                pixels = list(sampled.getdata())
        except (OSError, UnidentifiedImageError):
            return None
        if not pixels:
            return None
        red = sum(pixel[0] for pixel in pixels) / len(pixels)
        green = sum(pixel[1] for pixel in pixels) / len(pixels)
        blue = sum(pixel[2] for pixel in pixels) / len(pixels)
        return (int(round(red)), int(round(green)), int(round(blue)))

    @staticmethod
    def _normalize_fit_mode(value):
        text = str(value or "fill").strip().lower()
        if text in {"fill", "fit", "stretch"}:
            return text
        return "fill"

    @staticmethod
    def _normalize_offset(value):
        try:
            number = int(value)
        except (TypeError, ValueError):
            number = 0
        return max(-100, min(100, number))

    @staticmethod
    def _normalize_right_pane_mode(value, base_default=DEFAULT_RIGHT_PANE_MODE):
        mode = str(value or "").strip().lower()
        if mode in RIGHT_PANE_MODES:
            return mode
        default = str(base_default or DEFAULT_RIGHT_PANE_MODE).strip().lower()
        if default in RIGHT_PANE_MODES:
            return default
        return DEFAULT_RIGHT_PANE_MODE

    @staticmethod
    def _int_between(value, low, high, default):
        try:
            number = int(value)
        except (TypeError, ValueError):
            number = int(default)
        return max(low, min(high, number))

    @staticmethod
    def _normalize_font_rel(value):
        text = str(value or "").strip()
        if text.startswith("/.rockbox/fonts/"):
            return f"fonts/{os.path.basename(text)}"
        return text.replace("\\", "/")

    @staticmethod
    def _normalize_lockscreen_clock_font_rel(value):
        font_rel = ThemeDesignerService._normalize_font_rel(value)
        font_name = os.path.basename(font_rel).lower()
        if any(token in font_name for token in LOCKSCREEN_CLOCK_FONT_REJECT_TOKENS):
            return LOCKSCREEN_CLOCK_FONT_FALLBACK
        return font_rel

    def _read_cfg_settings(self, path):
        settings = {}
        with open(path, "r", encoding="utf-8") as handle:
            for raw in handle:
                line = raw.strip()
                if not line or line.startswith("#") or ":" not in line:
                    continue
                key, value = line.split(":", 1)
                settings[key.strip().lower()] = value.strip()
        return settings

    def _first_font_rel(self, repo_root):
        fonts = self.fonts_for_profile(repo_root)
        return fonts[0]["path_rel"] if fonts else "fonts/24 iLike.fnt"

    def _unique_variant_id(self, repo_root, name):
        base = _slug(name)
        existing = {item["id"] for item in self.list_variants(repo_root)}
        if base not in existing:
            return base
        index = 2
        while f"{base}-{index}" in existing:
            index += 1
        return f"{base}-{index}"

    def _base_asset_dir(self, repo_root, theme_id):
        bundle = self._themes.bundle_for_theme(theme_id, repo_root)
        asset = next((item for item in bundle["assets"] if item["kind"] == "wps_assets"), None)
        if not asset:
            raise ValueError(f"Base theme asset directory missing for {theme_id}")
        return os.path.dirname(asset["source_abs"]) if os.path.isfile(asset["source_abs"]) else asset["source_abs"]

    @staticmethod
    def _asset_record(kind, source_rel, destination_rel, source_abs):
        return {
            "kind": kind,
            "source_rel": source_rel.replace("\\", "/"),
            "source_abs": os.path.abspath(source_abs),
            "destination_rel": destination_rel.replace("\\", "/"),
            "exists": os.path.isfile(source_abs),
            "size": os.path.getsize(source_abs) if os.path.isfile(source_abs) else 0,
            "preview_path": os.path.abspath(source_abs) if os.path.isfile(source_abs) else "",
        }

    @staticmethod
    def _remove_asset_record(kind, destination_rel):
        rel = str(destination_rel or "").replace("\\", "/")
        return {
            "kind": kind,
            "source_rel": rel,
            "source_abs": "",
            "destination_rel": rel,
            "exists": True,
            "size": 0,
            "preview_path": "",
            "action": "remove",
        }

    def _render_image(self, source_path, dest_path, resolution, fit_mode, matte_hex):
        width, height = _fit_size(resolution)
        try:
            with Image.open(source_path) as img:
                image = img.convert("RGB")
        except (OSError, UnidentifiedImageError) as exc:
            raise ValueError(f"Unreadable wallpaper: {source_path}") from exc

        if fit_mode == "stretch":
            rendered = image.resize((width, height), Image.Resampling.LANCZOS)
        elif fit_mode == "fit":
            rendered = Image.new("RGB", (width, height), ImageColor.getrgb(f"#{matte_hex}"))
            fitted = ImageOps.contain(image, (width, height), Image.Resampling.LANCZOS)
            left = (width - fitted.width) // 2
            top = (height - fitted.height) // 2
            rendered.paste(fitted, (left, top))
        else:
            scale = max(width / image.width, height / image.height)
            resized = image.resize(
                (max(1, int(round(image.width * scale))), max(1, int(round(image.height * scale)))),
                Image.Resampling.LANCZOS,
            )
            left = max(0, (resized.width - width) // 2)
            top = max(0, (resized.height - height) // 2)
            rendered = resized.crop((left, top, left + width, top + height))

        if should_render_2bpp_greyscale(resolution):
            rendered = render_2bpp_greyscale(rendered)
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        rendered.save(dest_path, "BMP")

    def _render_solid_image(self, dest_path, resolution, fill_hex):
        width, height = _fit_size(resolution)
        image = Image.new("RGB", (width, height), ImageColor.getrgb(f"#{_ensure_hex(fill_hex, '000000')}"))
        if should_render_2bpp_greyscale(resolution):
            image = render_2bpp_greyscale(image)
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        image.save(dest_path, "BMP")

    def _render_sbs_background_image(self, dest_path, resolution, variant):
        width, height = _fit_size(resolution)
        pane_x = width // 2
        background = "000000" if variant.get("appearance_mode") == "dark" else "FFFFFF"
        if variant.get("appearance_mode") == "light":
            status_hex = _mix_hex(background, "FFFFFF", 0.45)
            divider_hex = _mix_hex(background, "000000", 0.22)
        else:
            status_hex = _mix_hex(background, "000000", 0.42)
            divider_hex = _mix_hex(background, variant["colors"]["selector_end"], 0.42)
        shadow_hex = _mix_hex(background, variant["colors"]["selector_end"], 0.22)
        shadow_hex = _mix_hex(shadow_hex, "000000", 0.58)
        highlight_hex = _mix_hex(background, "FFFFFF", 0.28)
        image = Image.new("RGB", (width, height), ImageColor.getrgb(f"#{background}"))
        pixels = image.load()
        status_rgb = ImageColor.getrgb(f"#{status_hex}")
        divider_rgb = ImageColor.getrgb(f"#{divider_hex}")
        for y in range(0, min(20, height)):
            for x in range(pane_x):
                pixels[x, y] = status_rgb
        if height > 20:
            for x in range(pane_x):
                pixels[x, 20] = divider_rgb
        self._draw_split_shadow(image, pane_x, shadow_hex, highlight_hex)
        if should_render_2bpp_greyscale(resolution):
            image = render_2bpp_greyscale(image)
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        image.save(dest_path, "BMP")

    def _apply_right_pane_video_overrides(self, staged_wps_dir, variant, targets, sbs_solid_targets):
        source_path = str(variant.get("right_pane_video_source") or "").strip()
        if not source_path:
            return
        if not os.path.isfile(source_path):
            raise ValueError(f"Right pane video not found: {source_path}")

        frame_paths = self._extract_right_pane_video_frames(source_path)
        rendered_frame_paths = []
        try:
            if not frame_paths:
                raise ValueError(f"No frames extracted from right pane video: {source_path}")

            resolution = variant["screen_resolution"]
            fit_mode = variant.get("right_pane_fit_mode", "fill")
            matte_hex = variant["colors"]["background"]
            offset_x = variant.get("right_pane_offset_x", 0)
            offset_y = variant.get("right_pane_offset_y", 0)
            for index in range(RIGHT_PANE_VIDEO_FRAME_COUNT):
                frame_path = frame_paths[index % len(frame_paths)]
                rendered_frame_path = os.path.join(staged_wps_dir, f"RightPaneVideoFrame_{index:02d}.bmp")
                self._render_right_pane_video_frame(
                    frame_path,
                    rendered_frame_path,
                    resolution,
                    fit_mode,
                    matte_hex,
                    offset_x,
                    offset_y,
                )
                rendered_frame_paths.append(rendered_frame_path)
            self._write_right_pane_video_framepack(
                rendered_frame_paths,
                os.path.join(staged_wps_dir, "RightPaneVideo.rbvp"),
            )

            first_frame = frame_paths[0]
            sbs_right_pane_targets = [
                name for name in sbs_solid_targets
                if os.path.basename(name) != "iPone_bd_fullart.bmp"
            ]
            if not sbs_right_pane_targets:
                sbs_right_pane_targets = targets.get("right_pane", [])
            for name in sbs_right_pane_targets:
                self._render_right_pane_image(
                    first_frame,
                    os.path.join(staged_wps_dir, name),
                    resolution,
                    fit_mode,
                    matte_hex,
                    os.path.join(staged_wps_dir, name),
                    offset_x,
                    offset_y,
                )
            for name in targets.get("right_pane", []):
                self._render_right_pane_image(
                    first_frame,
                    os.path.join(staged_wps_dir, name),
                    resolution,
                    fit_mode,
                    matte_hex,
                    os.path.join(staged_wps_dir, "iPone_bd.bmp"),
                    offset_x,
                    offset_y,
                )
        finally:
            for path in frame_paths:
                try:
                    os.remove(path)
                except OSError:
                    pass
            for path in rendered_frame_paths:
                try:
                    os.remove(path)
                except OSError:
                    pass

    def _extract_right_pane_video_frames(self, source_path):
        with tempfile.TemporaryDirectory(prefix="ipone-video-") as temp_dir:
            pattern = os.path.join(temp_dir, "frame_%03d.png")
            command = [
                "ffmpeg",
                "-hide_banner",
                "-loglevel",
                "error",
                "-y",
                "-i",
                source_path,
                "-vf",
                f"fps={RIGHT_PANE_VIDEO_FPS},scale=320:-2:flags=lanczos",
                "-frames:v",
                str(RIGHT_PANE_VIDEO_FRAME_COUNT),
                pattern,
            ]
            try:
                subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            except FileNotFoundError as exc:
                raise ValueError("ffmpeg is required to process video wallpapers") from exc
            except subprocess.CalledProcessError as exc:
                detail = (exc.stderr or b"").decode("utf-8", errors="replace").strip()
                message = f"Could not process right pane video wallpaper: {detail}" if detail else "Could not process right pane video wallpaper"
                raise ValueError(message) from exc

            extracted = [
                os.path.join(temp_dir, name)
                for name in sorted(os.listdir(temp_dir))
                if name.lower().endswith(".png")
            ]
            copied = []
            for index, path in enumerate(extracted):
                dest = os.path.join(tempfile.gettempdir(), f"ipone-video-frame-{os.getpid()}-{index:02d}.png")
                shutil.copy2(path, dest)
                copied.append(dest)
            return copied

    def _render_right_pane_video_frame(self, source_path, dest_path, resolution, fit_mode, matte_hex, offset_x=0, offset_y=0):
        width, height = _fit_size(resolution)
        pane_x = width // 2
        pane_width = max(1, width - pane_x - RIGHT_PANE_VIDEO_OVERLAY_X_OFFSET)
        offset_x = self._normalize_offset(offset_x)
        offset_y = self._normalize_offset(offset_y)

        try:
            with Image.open(source_path) as img:
                image = img.convert("RGB")
        except (OSError, UnidentifiedImageError) as exc:
            raise ValueError(f"Unreadable video frame: {source_path}") from exc

        if fit_mode == "stretch":
            pane = image.resize((pane_width, height), Image.Resampling.LANCZOS)
        elif fit_mode == "fit":
            pane = Image.new("RGB", (pane_width, height), ImageColor.getrgb(f"#{matte_hex}"))
            fitted = ImageOps.contain(image, (pane_width, height), Image.Resampling.LANCZOS)
            left = self._offset_position(pane_width - fitted.width, offset_x)
            top = self._offset_position(height - fitted.height, offset_y)
            pane.paste(fitted, (left, top))
        else:
            scale = max(pane_width / image.width, height / image.height)
            resized = image.resize(
                (max(1, int(round(image.width * scale))), max(1, int(round(image.height * scale)))),
                Image.Resampling.LANCZOS,
            )
            left = self._offset_position(resized.width - pane_width, offset_x)
            top = self._offset_position(resized.height - height, offset_y)
            pane = resized.crop((left, top, left + pane_width, top + height))

        if should_render_2bpp_greyscale(resolution):
            pane = render_2bpp_greyscale(pane)
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        pane.save(dest_path, "BMP")

    def _write_right_pane_video_sheet(self, frame_paths, dest_path):
        frames = []
        for path in frame_paths:
            try:
                with Image.open(path) as image:
                    frames.append(image.convert("RGB"))
            except (OSError, UnidentifiedImageError) as exc:
                raise ValueError(f"Unreadable rendered video frame: {path}") from exc
        if not frames:
            return
        width, height = frames[0].size
        sheet = Image.new("RGB", (width, height * len(frames)))
        for index, frame in enumerate(frames):
            if frame.size != (width, height):
                frame = frame.resize((width, height), Image.Resampling.LANCZOS)
            sheet.paste(frame, (0, height * index))
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        sheet.save(dest_path, "BMP")

    def _write_right_pane_video_framepack(self, frame_paths, dest_path):
        frames = []
        for path in frame_paths:
            try:
                with Image.open(path) as image:
                    frames.append(image.convert("RGB"))
            except (OSError, UnidentifiedImageError) as exc:
                raise ValueError(f"Unreadable rendered video frame: {path}") from exc
        if not frames:
            return

        width, height = frames[0].size
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        with open(dest_path, "wb") as handle:
            handle.write(b"RBVP")
            handle.write((1).to_bytes(2, "little"))
            handle.write((32).to_bytes(2, "little"))
            handle.write(width.to_bytes(2, "little"))
            handle.write(height.to_bytes(2, "little"))
            handle.write(RIGHT_PANE_VIDEO_FPS.to_bytes(2, "little"))
            handle.write(len(frames).to_bytes(2, "little"))
            handle.write((2).to_bytes(2, "little"))
            handle.write(bytes(14))

            for frame in frames:
                if frame.size != (width, height):
                    frame = frame.resize((width, height), Image.Resampling.LANCZOS)
                for red, green, blue in frame.getdata():
                    rgb565 = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
                    handle.write(rgb565.to_bytes(2, "little"))

    def _apply_right_pane_video_skin(self, sbs_path, variant):
        return
        if not variant.get("right_pane_video_source") or not os.path.isfile(sbs_path):
            return
        try:
            with open(sbs_path, "r", encoding="utf-8", errors="replace") as handle:
                content = handle.read()
        except OSError:
            return
        if "%xv(/.rockbox/wps/" in content:
            return

        width, height = _fit_size(variant.get("screen_resolution") or "320x240")
        overlay_x = width // 2 + RIGHT_PANE_VIDEO_OVERLAY_X_OFFSET
        pane_width = width - overlay_x
        sbs_skin_name = self._sbs_skin_name(variant)
        framepack = (
            f"%xv(/.rockbox/wps/{sbs_skin_name}/RightPaneVideo.rbvp,"
            f"0,0,{pane_width},{height},"
            f"{RIGHT_PANE_VIDEO_FPS},{RIGHT_PANE_VIDEO_FRAME_COUNT})"
        )
        animation_lines = [
            f"%V({overlay_x},0,{pane_width},{height},-)",
            f"%?if(%cs, =, 21)<|%?mh<|%?if(%St(ipone right pane), =, miniplayer)<{framepack}|>>>",
            "%V(0,0,-,-,-)",
        ]
        lines = content.splitlines()
        for index, line in enumerate(lines):
            if line.strip() == "%V(0,0,-,-,4)%VB":
                lines[index + 2:index + 2] = animation_lines
                break
        content = "\n".join(lines)
        if content and not content.endswith("\n"):
            content += "\n"
        atomic_write_text(sbs_path, content)

    def _render_right_pane_image(self, source_path, dest_path, resolution, fit_mode, matte_hex, base_path, offset_x=0, offset_y=0):
        width, height = _fit_size(resolution)
        pane_x = width // 2
        pane_width = width - pane_x
        offset_x = self._normalize_offset(offset_x)
        offset_y = self._normalize_offset(offset_y)
        if os.path.isfile(base_path):
            try:
                with Image.open(base_path) as base:
                    rendered = base.convert("RGB").resize((width, height), Image.Resampling.LANCZOS)
            except (OSError, UnidentifiedImageError):
                rendered = Image.new("RGB", (width, height), ImageColor.getrgb(f"#{matte_hex}"))
        else:
            rendered = Image.new("RGB", (width, height), ImageColor.getrgb(f"#{matte_hex}"))
        shadow_hex = _mix_hex(matte_hex, "000000", 0.52)
        highlight_hex = _mix_hex(matte_hex, "FFFFFF", 0.24)

        try:
            with Image.open(source_path) as img:
                image = img.convert("RGB")
        except (OSError, UnidentifiedImageError) as exc:
            raise ValueError(f"Unreadable right pane wallpaper: {source_path}") from exc

        if fit_mode == "stretch":
            pane = image.resize((pane_width, height), Image.Resampling.LANCZOS)
        elif fit_mode == "fit":
            pane = Image.new("RGB", (pane_width, height), ImageColor.getrgb(f"#{matte_hex}"))
            fitted = ImageOps.contain(image, (pane_width, height), Image.Resampling.LANCZOS)
            left = self._offset_position(pane_width - fitted.width, offset_x)
            top = self._offset_position(height - fitted.height, offset_y)
            pane.paste(fitted, (left, top))
        else:
            scale = max(pane_width / image.width, height / image.height)
            resized = image.resize(
                (max(1, int(round(image.width * scale))), max(1, int(round(image.height * scale)))),
                Image.Resampling.LANCZOS,
            )
            left = self._offset_position(resized.width - pane_width, offset_x)
            top = self._offset_position(resized.height - height, offset_y)
            pane = resized.crop((left, top, left + pane_width, top + height))

        rendered.paste(pane, (pane_x, 0))
        self._draw_split_shadow(rendered, pane_x, shadow_hex, highlight_hex)
        if should_render_2bpp_greyscale(resolution):
            rendered = render_2bpp_greyscale(rendered)
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        rendered.save(dest_path, "BMP")

    def _apply_split_shadow_to_existing(self, path, resolution, variant):
        if not os.path.isfile(path):
            return
        width, height = _fit_size(resolution)
        background = variant["colors"]["background"]
        shadow_hex = _mix_hex(background, variant["colors"]["selector_end"], 0.22)
        shadow_hex = _mix_hex(shadow_hex, "000000", 0.58)
        highlight_hex = _mix_hex(background, "FFFFFF", 0.28)
        try:
            with Image.open(path) as img:
                image = img.convert("RGB").resize((width, height), Image.Resampling.LANCZOS)
        except (OSError, UnidentifiedImageError):
            return
        self._draw_split_shadow(image, width // 2, shadow_hex, highlight_hex)
        if should_render_2bpp_greyscale(resolution):
            image = render_2bpp_greyscale(image)
        image.save(path, "BMP")

    @staticmethod
    def _draw_split_shadow(image, pane_x, shadow_hex, highlight_hex):
        width, height = image.size
        pixels = image.load()
        shadow = ImageColor.getrgb(f"#{_ensure_hex(shadow_hex, '000000')}")
        highlight = ImageColor.getrgb(f"#{_ensure_hex(highlight_hex, 'FFFFFF')}")
        layers = (
            (-4, shadow, 0.08),
            (-3, shadow, 0.14),
            (-2, shadow, 0.28),
            (-1, shadow, 0.56),
            (0, shadow, 0.68),
            (1, highlight, 0.22),
            (2, shadow, 0.12),
            (3, shadow, 0.06),
        )
        for y in range(height):
            for offset, color, alpha in layers:
                x = pane_x + offset
                if 0 <= x < width:
                    pixels[x, y] = ThemeDesignerService._blend_rgb(pixels[x, y], color, alpha)

    @staticmethod
    def _blend_rgb(base, overlay, alpha):
        alpha = max(0.0, min(1.0, float(alpha)))
        return tuple(int(round(base[index] + (overlay[index] - base[index]) * alpha)) for index in range(3))

    @staticmethod
    def _offset_position(extra_space, offset):
        extra_space = max(0, int(extra_space))
        if extra_space <= 0:
            return 0
        return int(round(extra_space * ((max(-100, min(100, int(offset))) + 100) / 200.0)))

    def _designer_root(self, repo_root):
        root = os.path.abspath(repo_root)
        if os.path.basename(root) == "rockpod":
            return os.path.join(root, ".theme_designer")
        return os.path.join(root, "rockpod", ".theme_designer")

    def _variants_dir(self, repo_root):
        return os.path.join(self._designer_root(repo_root), "variants")

    def _generated_dir(self, repo_root):
        return os.path.join(self._designer_root(repo_root), "generated")

    def _variant_path(self, repo_root, variant_id):
        return os.path.join(self._variants_dir(repo_root), f"{variant_id}.json")

    def _apply_template_color_overrides(self, path, base_theme_id, stage_root, variant=None):
        if str(base_theme_id or "").strip() not in {"iPone", "iPoneCustom"}:
            return

        theme_cfg_path = os.path.join(stage_root, "themes")
        cfg_candidates = [name for name in os.listdir(theme_cfg_path)] if os.path.isdir(theme_cfg_path) else []
        if not cfg_candidates:
            return
        settings = self._read_cfg_settings(os.path.join(theme_cfg_path, cfg_candidates[0]))
        background = _ensure_hex(settings.get("background color"), DEFAULT_COLORS["background"])
        foreground = _ensure_hex(settings.get("foreground color"), DEFAULT_COLORS["foreground"])
        selector_start = _ensure_hex(settings.get("line selector start color"), DEFAULT_COLORS["selector_start"])
        selector_end = _ensure_hex(settings.get("line selector end color"), DEFAULT_COLORS["selector_end"])
        selector_text = _ensure_hex(settings.get("line selector text color"), DEFAULT_COLORS["selector_text"])
        variant_colors = (variant or {}).get("colors", {}) if isinstance(variant, dict) else {}
        sbs_clock = _ensure_hex(variant_colors.get("sbs_clock"), _mix_hex(foreground, selector_text, 0.26))

        ui_background = background
        if not (isinstance(variant, dict) and variant.get("appearance_mode") == "light"):
            ui_background = _mix_hex(background, "000000", 0.12)

        replacements = {
            "1F1F24": background,
            "1B1B20": _mix_hex(background, "000000", 0.18),
            "151519": _mix_hex(background, "000000", 0.32),
            "242429": _mix_hex(background, foreground, 0.08),
            "202025": _mix_hex(background, foreground, 0.04),
            "18181D": _mix_hex(background, "000000", 0.24),
            "0B0B0D": _mix_hex(background, "000000", 0.42),
            "0F0E14": ui_background,
            "15121B": _mix_hex(selector_start, "000000", 0.42),
            "16121D": _mix_hex(selector_start, "000000", 0.38),
            "18161F": _mix_hex(selector_start, "000000", 0.28),
            "1B1722": _mix_hex(selector_start, "000000", 0.34),
            "1C1C1C": _mix_hex(background, "000000", 0.22),
            "201C26": _mix_hex(selector_start, "000000", 0.22),
            "26222F": _mix_hex(selector_start, background, 0.22),
            "282434": _mix_hex(selector_start, background, 0.20),
            "292334": _mix_hex(selector_start, background, 0.24),
            "2A2431": _mix_hex(selector_start, background, 0.28),
            "2D2936": _mix_hex(selector_start, background, 0.34),
            "322B3B": _mix_hex(selector_start, background, 0.40),
            "333333": _mix_hex(background, "FFFFFF", 0.12),
            "3584E4": selector_end,
            "4A494A": _mix_hex(foreground, background, 0.24),
            "5C5572": selector_end,
            "6E6390": _mix_hex(selector_end, selector_text, 0.35),
            "888888": _mix_hex(foreground, background, 0.38),
            "AAAAAA": _mix_hex(foreground, background, 0.28),
            "BDBABD": _mix_hex(foreground, background, 0.20),
            "C7C4C7": _mix_hex(selector_text, background, 0.22),
            "C8BED7": _mix_hex(selector_text, background, 0.18),
            "D4CAE4": _mix_hex(selector_text, foreground, 0.34),
            "D7CCF0": _mix_hex(selector_text, foreground, 0.42),
            "E3D8FB": _mix_hex(selector_text, foreground, 0.62),
            "E8DFF8": sbs_clock,
            "F1EAFF": _mix_hex(foreground, selector_text, 0.42),
            "F4EFFB": _mix_hex(foreground, selector_text, 0.18),
            "F7F4FA": foreground,
            "FCF9FF": selector_text,
            "FFFFFF": foreground,
            "186DBD": selector_end,
        }

        try:
            with open(path, "r", encoding="utf-8") as handle:
                content = handle.read()
        except OSError:
            return

        pattern = re.compile(
            "|".join(re.escape(source) for source in sorted(replacements, key=len, reverse=True)),
            re.IGNORECASE,
        )
        content = pattern.sub(lambda match: replacements.get(match.group(0).upper(), match.group(0)), content)
        if os.path.basename(path).lower().endswith(".sbs"):
            right_pane_text = re.compile(
                r"(%Vl\((?:MiniTitle|MiniSub|NowPlayingBadge|radio)[^)]*\)%Vf\()([0-9A-Fa-f]{6})(\))"
            )
            content = right_pane_text.sub(lambda match: f"{match.group(1)}{sbs_clock}{match.group(3)}", content)

        atomic_write_text(path, content)

    def _apply_lockscreen_clock_overrides(self, path, variant):
        if not str((variant or {}).get("base_theme_id") or "").startswith("iPone"):
            return
        clock = self._normalize_lockscreen_clock((variant or {}).get("lockscreen_clock"))
        try:
            with open(path, "r", encoding="utf-8") as handle:
                content = handle.read()
        except OSError:
            return

        content = self._ensure_font_slot(content, 10, clock["font_rel"])
        is_sbs = os.path.basename(path).lower().endswith(".sbs")
        use_glass = False

        if is_sbs:
            content = self._replace_lockscreen_clock_block(
                content,
                "iPoneLockscreen",
                clock,
                time_font=10,
                date_font=6,
                use_glass=use_glass,
                variant=variant,
            )
            if use_glass:
                content = ThemeDesignerService._enable_generated_clock_glass(content)
        else:
            content = self._replace_lockscreen_clock_block(
                content,
                "Lockscreen",
                clock,
                time_font=10,
                date_font=4,
                use_glass=False,
                variant=variant,
            )
        atomic_write_text(path, content)

    def _apply_designer_sbs_layout_overrides(self, path, variant):
        if not str((variant or {}).get("base_theme_id") or "").startswith("iPone"):
            return
        clock = self._normalize_lockscreen_clock((variant or {}).get("lockscreen_clock"))
        lock_color = clock["color"]
        try:
            with open(path, "r", encoding="utf-8") as handle:
                content = handle.read()
        except OSError:
            return

        content = content.replace("%V(134,5,25,11,-)", "%V(133,5,25,11,-)", 1)
        content = re.sub(
            r"(%Vl\(iPoneLockscreen,32,8,60,16,2\)%Vf\()[0-9A-Fa-f]{6}(\)%alHOLD)",
            rf"\g<1>{lock_color}\g<2>",
            content,
            count=1,
        )
        content = re.sub(
            r"(%Vl\(iPoneLockscreen,242,8,44,16,2\)%Vf\()[0-9A-Fa-f]{6}(\)%ar%bl%%)",
            rf"\g<1>{lock_color}\g<2>",
            content,
            count=1,
        )
        content = re.sub(
            r"(%Vl\(iPoneAOD,32,8,60,16,2\)%Vf\()[0-9A-Fa-f]{6}(\)%alHOLD)",
            rf"\g<1>{lock_color}\g<2>",
            content,
            count=1,
        )
        content = re.sub(
            r"(%Vl\(iPoneAOD,242,8,44,16,2\)%Vf\()[0-9A-Fa-f]{6}(\)%ar%bl%%)",
            rf"\g<1>{lock_color}\g<2>",
            content,
            count=1,
        )
        content = self._remove_full_art_pulse(content)
        atomic_write_text(path, content)

    @staticmethod
    def _remove_full_art_pulse(content):
        content = content.replace("%Vd(SbsAnimPulse)%?mp<", "%?mp<")
        return re.sub(r"^%Vl\(SbsAnimPulse,[^\n]*(?:\n|$)", "", content, flags=re.MULTILINE)

    @staticmethod
    def _ensure_font_slot(content, slot, font_rel):
        name = os.path.basename(str(font_rel or DEFAULT_LOCKSCREEN_CLOCK["font_rel"]).strip())
        replacement = f"%Fl({slot},{name})"
        pattern = re.compile(rf"%Fl\({slot},[^\n]*")
        if pattern.search(content):
            return pattern.sub(replacement, content, count=1)
        matches = list(re.finditer(r"%Fl\([^\n]*", content))
        if not matches:
            return replacement + "\n" + content
        last = matches[-1]
        return content[:last.end()] + "\n" + replacement + content[last.end():]

    @staticmethod
    def _font_pixel_size(font_name: str) -> int:
        match = re.match(r"(\d+)-", os.path.basename(str(font_name or "")))
        return int(match.group(1)) if match else 0

    @staticmethod
    def _clock_contrast_color(hex_color: str) -> str:
        try:
            r, g, b = int(hex_color[0:2], 16), int(hex_color[2:4], 16), int(hex_color[4:6], 16)
            luminance = 0.299 * r + 0.587 * g + 0.114 * b
            return "2A2A3E" if luminance > 128 else "D0D0E0"
        except (ValueError, IndexError):
            return "2A2A3E"

    @staticmethod
    def _render_clock_glass(source_path: str, dest_path: str, clock: Dict):
        font_pixel_size = ThemeDesignerService._font_pixel_size(clock.get("font_rel", ""))
        clock_height = int(clock.get("height", 55))
        effective_height = max(clock_height, font_pixel_size)
        x = int(clock.get("x", 0))
        y = max(0, int(clock.get("y", 32)) - 4)
        width = int(clock.get("width", 320))
        height = min(240 - y, effective_height + 8)
        if width >= 320:
            x = 0
            width = 320
        try:
            with Image.open(source_path) as img:
                base = ImageOps.fit(img.convert("RGB"), (320, 240), Image.Resampling.LANCZOS)
        except (OSError, UnidentifiedImageError) as exc:
            raise ValueError(f"Unreadable lockscreen wallpaper: {source_path}") from exc

        crop = base.crop((x, y, min(320, x + width), min(240, y + height)))
        strength = clock.get("glass_strength", "medium")
        style = clock.get("style", "glass")
        blur_radius = {"low": 3, "medium": 6, "high": 9}.get(strength, 6)
        crop = crop.filter(ImageFilter.GaussianBlur(radius=blur_radius)).convert("RGBA")
        alpha = {"low": 34, "medium": 58, "high": 84}.get(strength, 58)
        tint = (255, 255, 255) if style in ("glass", "glass tinted") else (224, 210, 255)
        overlay = Image.new("RGBA", crop.size, (*tint, alpha))
        glass = Image.alpha_composite(crop, overlay)
        draw = ImageDraw.Draw(glass, "RGBA")
        for row in range(max(1, glass.height // 2)):
            fade = int(54 * (1.0 - row / max(1, glass.height // 2)))
            draw.line((0, row, glass.width, row), fill=(255, 255, 255, fade))
        draw.line((0, 0, glass.width, 0), fill=(255, 255, 255, 118))
        draw.line((0, glass.height - 1, glass.width, glass.height - 1), fill=(0, 0, 0, 52))
        draw.line((0, 1, glass.width, glass.height - 2), fill=(255, 255, 255, 26), width=2)
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        glass.convert("RGB").save(dest_path, "BMP")

    @staticmethod
    def _enable_generated_clock_glass(content: str) -> str:
        if "%xl(LockClockGlassGenerated,LockClockGlassGenerated.bmp)" not in content:
            marker = "%xl(LsStyle,LockscreenStyle.bmp)\n"
            content = content.replace(marker, marker + "%xl(LockClockGlassGenerated,LockClockGlassGenerated.bmp)\n", 1)
        if "%Vd(LockClockGlass)%Vd(iPoneLockscreen)" in content:
            return content
        content = content.replace("%Vd(iPoneLockscreen)%?mp<", "%Vd(LockClockGlass)%Vd(iPoneLockscreen)%?mp<", 1)
        return content

    def _replace_lockscreen_clock_block(self, content, viewport_name, clock, time_font, date_font, use_glass=False, variant=None):
        x = int(clock["x"])
        y = self._lockscreen_clock_safe_y(viewport_name, int(clock["y"]))
        width = int(clock["width"])
        height = int(clock["height"])
        width_text = "-" if width >= 320 else str(width)
        align = {"left": "%al", "center": "%ac", "right": "%ar"}.get(clock["align"], "%ac")
        font_pixel_size = ThemeDesignerService._font_pixel_size(clock.get("font_rel", ""))
        stretch_percent = self._int_between(clock.get("stretch"), 100, 160, DEFAULT_LOCKSCREEN_CLOCK["stretch"])
        stretch_height = self._lockscreen_clock_stretch_height(clock, font_pixel_size)
        effective_height = max(height, stretch_height)
        date_y = self._lockscreen_clock_date_y(y, height, font_pixel_size, stretch_percent)
        color = self._lockscreen_clock_skin_color(clock)
        date_color = self._lockscreen_clock_skin_color(clock)
        base_viewport_name = viewport_name
        display_underlay_viewports = []
        display_base_viewports = []
        display_overlay_viewports = []
        safe_top = LOCKSCREEN_STATUS_RESERVED_HEIGHT if viewport_name == "iPoneLockscreen" else 0

        time_parts = []
        style = clock.get("style", "solid")
        glass_underlay_lines = []
        glass_overlay_lines = []
        if use_glass:
            glass_y = max(0, y - 4)
            glass_height = min(240 - glass_y, effective_height + 8)
            time_parts.append(
                f"%Vl(LockClockGlass,{x},{glass_y},{width_text},{glass_height},-)%xd(LockClockGlassGenerated)"
            )
        if style.startswith("glass"):
            strength = clock.get("glass_strength", "high")
            shadow_alpha = self._lockscreen_clock_glass_layer_opacity(
                clock, {"low": 3, "medium": 5, "high": 7}.get(strength, 7)
            )
            shine_alpha = self._lockscreen_clock_glass_layer_opacity(
                clock, {"low": 18, "medium": 28, "high": 40}.get(strength, 40)
            )
            top_edge_alpha = self._lockscreen_clock_glass_layer_opacity(
                clock, {"low": 40, "medium": 64, "high": 88}.get(strength, 88)
            )
            stroke_alpha = self._lockscreen_clock_glass_layer_opacity(
                clock, {"low": 10, "medium": 15, "high": 20}.get(strength, 20)
            )
            refract_cool_alpha = self._lockscreen_clock_glass_layer_opacity(
                clock, {"low": 14, "medium": 24, "high": 36}.get(strength, 36)
            )
            clock_hex = _ensure_hex(clock.get("color"), DEFAULT_LOCKSCREEN_CLOCK["color"])
            shine_hex = _mix_hex(clock_hex, "FFFFFF", 0.18)
            top_edge_hex = _mix_hex(clock_hex, "FFFFFF", 0.78)
            refract_cool_hex = _mix_hex(clock_hex, "BFF6FF", 0.42)
            shadow_color = self._skin_color_with_opacity("000000", shadow_alpha, force_alpha=True)
            shine_color = self._skin_color_with_opacity(shine_hex, shine_alpha, force_alpha=True)
            top_edge_color = self._skin_color_with_opacity(top_edge_hex, top_edge_alpha, force_alpha=True)
            stroke_color = self._skin_color_with_opacity("000000", stroke_alpha, force_alpha=True)
            refract_cool_color = self._skin_color_with_opacity(
                refract_cool_hex, refract_cool_alpha, force_alpha=True
            )
            shadow_x = min(319, max(0, x + 1))
            shadow_y = min(239, max(0, y + 1))
            top_edge_x = max(0, x - 1)
            top_edge_y = max(safe_top, y - 2)
            shine_x = min(319, max(0, x + 1))
            shine_y = max(0, y)
            refract_cool_x = max(0, x - 2)
            refract_cool_y = min(239, max(0, y + 1))
            shine_height = effective_height
            glass_underlay_lines.append(
                f"%Vl({viewport_name},{shadow_x},{shadow_y},{width_text},{effective_height},{time_font})"
                f"%Vf({shadow_color}){align}%cl:%cM %cP"
            )
            glass_overlay_lines.append(
                f"%Vl({viewport_name},{refract_cool_x},{refract_cool_y},{width_text},{effective_height},{time_font})"
                f"%Vf({refract_cool_color}){align}%cl:%cM %cP"
            )
            glass_overlay_lines.append(
                f"%Vl({viewport_name},{x},{max(safe_top, y - 1)},{width_text},{effective_height},{time_font})"
                f"%Vf({stroke_color}){align}%cl:%cM %cP"
            )
            glass_overlay_lines.append(
                f"%Vl({viewport_name},{top_edge_x},{top_edge_y},{width_text},{effective_height},{time_font})"
                f"%Vf({top_edge_color}){align}%cl:%cM %cP"
            )
            glass_overlay_lines.append(
                f"%Vl({viewport_name},{shine_x},{shine_y},{width_text},{shine_height},{time_font})"
                f"%Vf({shine_color}){align}%cl:%cM %cP"
            )
        if style == "outline":
            outline_color = ThemeDesignerService._clock_contrast_color(clock["color"])
            shadow_x = min(319, max(0, x + 1))
            shadow_y = min(239, max(0, y + 1))
            time_parts.append(
                f"%Vl(iPoneClockShadow,{shadow_x},{shadow_y},{width_text},{effective_height},{time_font})"
                f"%Vf({outline_color}){align}%cl:%cM %cP"
            )
        time_parts.extend(glass_underlay_lines)
        time_parts.append(
            f"%Vl({base_viewport_name},{x},{y},{width_text},{effective_height},{time_font})%Vf({color}){align}%cl:%cM %cP"
        )
        time_parts.extend(glass_overlay_lines)
        time_line = "\n".join(time_parts)

        date_parts = []
        if style.startswith("glass"):
            date_shadow_color = self._skin_color_with_opacity("000000", 12, force_alpha=True)
            date_shadow_x = min(319, max(0, x + 1))
            date_shadow_y = min(239, max(0, date_y + 1))
            date_parts.append(
                f"%Vl({viewport_name},{date_shadow_x},{date_shadow_y},{width_text},18,{date_font})"
                f"%Vf({date_shadow_color}){align}"
                "%?if(%ss(0,7,%St(lang)), =, english)<%?cu<Monday|Tuesday|Wednesday|Thursday|Friday|Saturday|Sunday>|%ca> "
                "%?or(%if(%ss(0,7,%St(lang)), =, chinese),%if(%St(lang), =, magyar),%if(%St(lang), =, lietuviu),"
                "%if(%St(lang), =, japanese),%if(%St(lang), =, korean))<%cb %cd|%?if(%St(lang), =, english-us)<%cb %cd|%cd %cb>>"
            )
        date_parts.append(
            f"%Vl({viewport_name},{x},{date_y},{width_text},18,{date_font})%Vf({date_color}){align}"
            "%?if(%ss(0,7,%St(lang)), =, english)<%?cu<Monday|Tuesday|Wednesday|Thursday|Friday|Saturday|Sunday>|%ca> "
            "%?or(%if(%ss(0,7,%St(lang)), =, chinese),%if(%St(lang), =, magyar),%if(%St(lang), =, lietuviu),"
            "%if(%St(lang), =, japanese),%if(%St(lang), =, korean))<%cb %cd|%?if(%St(lang), =, english-us)<%cb %cd|%cd %cb>>"
        )
        date_line = "\n".join(date_parts)
        time_pattern = re.compile(
            rf"(?:%Vl\((?:{re.escape(viewport_name)}|iPoneClockBase(?:Scale)?\d+|iPoneClockGlass[^)]*),"
            rf"[^\n]*%cl:%cM %cP(?:\n|$))+"
        )
        date_pattern = re.compile(rf"%Vl\({re.escape(viewport_name)},[^\n]*%cd\|%cd %cb>>")
        updated = time_pattern.sub(time_line + "\n", content, count=1)
        date_matches = list(date_pattern.finditer(updated))
        if date_matches:
            updated = (
                updated[:date_matches[0].start()]
                + date_line
                + updated[date_matches[-1].end():]
            )
        updated = self._sync_lockscreen_clock_display_chain(
            updated,
            viewport_name,
            display_underlay_viewports,
            display_base_viewports,
            display_overlay_viewports,
        )
        return updated

    @staticmethod
    def _sync_lockscreen_clock_display_chain(
        content,
        viewport_name,
        display_underlay_viewports,
        display_base_viewports,
        display_overlay_viewports,
    ):
        if viewport_name != "iPoneLockscreen":
            return content

        underlay_tags = "".join(f"%Vd({name})" for name in display_underlay_viewports)
        base_tags = "".join(f"%Vd({name})" for name in display_base_viewports)
        overlay_tags = "".join(f"%Vd({name})" for name in display_overlay_viewports)
        if base_tags:
            replacement = f"%Vd({viewport_name}){underlay_tags}{base_tags}{overlay_tags}%?mp<"
        else:
            replacement = f"{underlay_tags}%Vd({viewport_name}){overlay_tags}%?mp<"
        generated_display = r"%Vd\(iPoneClock(?:Base|Glass|Stretch)[^)]+\)"
        pattern = re.compile(
            rf"(?:{generated_display})*"
            rf"%Vd\({re.escape(viewport_name)}\)"
            rf"(?:{generated_display})*"
            r"%\?mp<"
        )
        return pattern.sub(replacement, content, count=1)

    def _lockscreen_clock_skin_color(self, clock):
        color = _ensure_hex(clock.get("color"), DEFAULT_LOCKSCREEN_CLOCK["color"])
        opacity = self._lockscreen_clock_render_opacity(clock)
        return self._skin_color_with_opacity(color, opacity)

    def _lockscreen_clock_stretch_height(self, clock, font_pixel_size):
        stretch = self._int_between(clock.get("stretch"), 100, 160, DEFAULT_LOCKSCREEN_CLOCK["stretch"])
        return max(font_pixel_size, int(round(font_pixel_size * stretch / 100.0)))

    @staticmethod
    def _lockscreen_clock_safe_y(viewport_name, y):
        if viewport_name in {"iPoneLockscreen", "Lockscreen"}:
            return max(LOCKSCREEN_STATUS_RESERVED_HEIGHT, int(y))
        return int(y)

    @staticmethod
    def _lockscreen_clock_date_y(y, height, font_pixel_size, stretch_percent):
        if font_pixel_size <= 0:
            return min(220, y + height + 4)

        stretch_extra = max(0, stretch_percent - 100)
        visible_clock_height = int(round(font_pixel_size * (1.0 + (stretch_extra * 0.42 / 100.0))))
        gap = 2
        return min(220, y + visible_clock_height + gap)

    def _lockscreen_clock_render_opacity(self, clock):
        opacity = self._int_between(clock.get("opacity"), 20, 100, DEFAULT_LOCKSCREEN_CLOCK["opacity"])
        if str(clock.get("style") or "").lower().startswith("glass"):
            strength = str(clock.get("glass_strength") or "high").lower()
            cap = {"low": 76, "medium": 64, "high": 52}.get(strength, 52)
            opacity = min(opacity, cap)
        return opacity

    def _lockscreen_clock_glass_layer_opacity(self, clock, base_opacity):
        source_opacity = self._int_between(clock.get("opacity"), 20, 100, DEFAULT_LOCKSCREEN_CLOCK["opacity"])
        factor = 0.40 + (source_opacity / 100.0) * 1.40
        return max(0, min(100, int(round(base_opacity * factor))))

    def _skin_color_with_opacity(self, color, opacity, force_alpha=False):
        color = _ensure_hex(color, DEFAULT_LOCKSCREEN_CLOCK["color"])
        opacity = self._int_between(opacity, 0, 100, 100)
        if opacity >= 100 and not force_alpha:
            return color
        alpha = max(0, min(255, int(round(opacity * 255 / 100.0))))
        return f"{color}{alpha:02X}"

    def _effective_lockscreen_clock_color(self, variant, clock, x, y, width, height):
        color = _ensure_hex(clock.get("color"), DEFAULT_LOCKSCREEN_CLOCK["color"])
        opacity = self._int_between(clock.get("opacity"), 20, 100, DEFAULT_LOCKSCREEN_CLOCK["opacity"])
        if opacity >= 100:
            return color
        background = self._lockscreen_region_background_rgb(variant, x, y, width, height)
        foreground = _hex_to_rgb(color)
        amount = opacity / 100.0
        return _rgb_to_hex(
            (
                background[0] + (foreground[0] - background[0]) * amount,
                background[1] + (foreground[1] - background[1]) * amount,
                background[2] + (foreground[2] - background[2]) * amount,
            )
        )

    def _lockscreen_region_background_rgb(self, variant, x, y, width, height):
        fallback = (255, 255, 255) if (variant or {}).get("appearance_mode") == "light" else (0, 0, 0)
        source_path = str(
            (variant or {}).get("_staged_lockscreen_wallpaper_source")
            or (variant or {}).get("wallpaper_source")
            or ""
        ).strip()
        if not source_path or not os.path.isfile(source_path):
            return fallback
        try:
            image = self._rendered_wallpaper_for_sampling(
                source_path,
                (variant or {}).get("screen_resolution", "320x240"),
                (variant or {}).get("fit_mode", "fill"),
                ((variant or {}).get("colors") or {}).get("background", DEFAULT_COLORS["background"]),
            )
        except ValueError:
            return fallback
        if image.width <= 0 or image.height <= 0:
            return fallback
        left = max(0, min(image.width - 1, int(x)))
        top = max(0, min(image.height - 1, int(y)))
        right = max(left + 1, min(image.width, left + max(1, int(width))))
        bottom = max(top + 1, min(image.height, top + max(1, int(height))))
        region = image.crop((left, top, right, bottom))
        pixels = list(region.getdata())
        if not pixels:
            pixels = list(image.getdata())
        if not pixels:
            return fallback
        return (
            int(round(sum(pixel[0] for pixel in pixels) / len(pixels))),
            int(round(sum(pixel[1] for pixel in pixels) / len(pixels))),
            int(round(sum(pixel[2] for pixel in pixels) / len(pixels))),
        )

    @staticmethod
    def _rendered_wallpaper_for_sampling(source_path, resolution, fit_mode, matte_hex):
        width, height = _fit_size(resolution)
        try:
            with Image.open(source_path) as img:
                image = img.convert("RGB")
        except (OSError, UnidentifiedImageError) as exc:
            raise ValueError(f"Unreadable wallpaper: {source_path}") from exc

        if fit_mode == "stretch":
            return image.resize((width, height), Image.Resampling.LANCZOS)
        if fit_mode == "fit":
            rendered = Image.new("RGB", (width, height), ImageColor.getrgb(f"#{_ensure_hex(matte_hex, '000000')}"))
            fitted = ImageOps.contain(image, (width, height), Image.Resampling.LANCZOS)
            left = (width - fitted.width) // 2
            top = (height - fitted.height) // 2
            rendered.paste(fitted, (left, top))
            return rendered

        scale = max(width / image.width, height / image.height)
        resized = image.resize(
            (max(1, int(round(image.width * scale))), max(1, int(round(image.height * scale)))),
            Image.Resampling.LANCZOS,
        )
        left = max(0, (resized.width - width) // 2)
        top = max(0, (resized.height - height) // 2)
        return resized.crop((left, top, left + width, top + height))
