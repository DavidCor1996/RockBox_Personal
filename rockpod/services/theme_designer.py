"""Guided iPone designer with staged variant generation."""

from __future__ import annotations

import json
import os
import re
import shutil
import tempfile
from copy import deepcopy

from PIL import Image, ImageColor, ImageOps, UnidentifiedImageError

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
}

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
    "iPoneCustom": ("iPone_bd.bmp", "SbsBackdrop.bmp"),
}

TINTED_SPRITE_ASSETS = (
    "Battery.bmp",
    "LoadingStatus.bmp",
    "LockscreenStyle.bmp",
    "AlwaysOnDisplayStyle.bmp",
    "NotifMusic.bmp",
    "NotifPlayIcon.bmp",
    "NotifPlayIconLock.bmp",
    "Notification.bmp",
    "LosslessIcon.bmp",
    "LosslessIconLock.bmp",
    "MiniRecordSpin.bmp",
    "MiniRecordSpinSmall.bmp",
    "Playing Status.bmp",
    "PlayStatus.bmp",
    "PlayStatusPurple.bmp",
    "PlayStatusPurpleLarge.bmp",
    "PlaybackStatusIcons.bmp",
    "PlayerStatusButton.bmp",
    "PlayerSlider.bmp",
    "PlayerSliderThin.bmp",
    "PlayerSliderThinPurple.bmp",
    "PlayerSliderThinPurple12.bmp",
    "Slider.bmp",
    "SliderThin.bmp",
    "SliderThinPurple.bmp",
    "SliderThinPurple12.bmp",
    "SliderBackdrop.bmp",
    "SliderBackdrop4Digits.bmp",
    "SliderBackdrop5Digits.bmp",
    "SliderBackdrop6Digits.bmp",
    "SliderBackdropThin.bmp",
    "SliderBackdropThin4Digits.bmp",
    "SliderBackdropThin5Digits.bmp",
    "SliderBackdropThin6Digits.bmp",
    "SliderBackdropThinPurple.bmp",
    "SliderBackdropThinPurple4Digits.bmp",
    "SliderBackdropThinPurple5Digits.bmp",
    "SliderBackdropThinPurple6Digits.bmp",
    "SliderBackdropThinPurple12.bmp",
    "SliderBackdropThinPurple12_4Digits.bmp",
    "SliderBackdropThinPurple12_5Digits.bmp",
    "SliderBackdropThinPurple12_6Digits.bmp",
    "VolumeBackdrop.bmp",
    "VolumePromptIcons.bmp",
    "VolumeSlider.bmp",
    "VolumeSliderBackdrop.bmp",
    "VolumeSliderBackdropPurple.bmp",
    "VolumeSliderEnd.bmp",
    "VolumeSliderEndPurple.bmp",
    "VolumeSliderPurple.bmp",
    "WpsTL.bmp",
    "WpsTR.bmp",
    "WpsBL.bmp",
    "WpsBR.bmp",
    "WpsBackdropL.bmp",
    "WpsBackdropR.bmp",
    "WpsBackdropT.bmp",
    "WpsBackdropB.bmp",
    "PlayerFallback.bmp",
    "FrameTop.bmp",
    "FrameLeft.bmp",
    "FrameRight.bmp",
    "FrameBottom.bmp",
    "LargeSliderBackdrop.bmp",
    "LargeSliderFallback.bmp",
    "LargeSliderTop.bmp",
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
            "wallpaper_source": "",
            "charging_wallpaper_source": "",
            "right_pane_wallpaper_source": "",
            "color_profile": "default",
            "appearance_mode": "dark",
            "colors": {
                "background": _ensure_hex(settings.get("background color"), DEFAULT_COLORS["background"]),
                "foreground": _ensure_hex(settings.get("foreground color"), DEFAULT_COLORS["foreground"]),
                "selector_start": _ensure_hex(settings.get("line selector start color"), DEFAULT_COLORS["selector_start"]),
                "selector_end": _ensure_hex(settings.get("line selector end color"), DEFAULT_COLORS["selector_end"]),
                "selector_text": _ensure_hex(settings.get("line selector text color"), DEFAULT_COLORS["selector_text"]),
                "list_separator": _ensure_hex(settings.get("list separator color"), DEFAULT_COLORS["list_separator"]),
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
        font_rel = normalized["font_rel"]
        font_abs = os.path.join(os.path.abspath(repo_root), font_rel)
        if not os.path.isfile(font_abs):
            raise ValueError(f"Font not found: {font_rel}")

        assets = []
        for kind, rel in (
            ("cfg", f"themes/{theme_name}.cfg"),
            ("wps", f"wps/{theme_name}.wps"),
            ("sbs", f"wps/{theme_name}.sbs"),
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

        assets.extend(self._font_assets_for_generated_skins(repo_root, stage_root, theme_name, font_rel))

        wps_dir = os.path.join(stage_root, "wps", theme_name)
        for root, _dirs, files in os.walk(wps_dir):
            for filename in sorted(files):
                full = os.path.join(root, filename)
                suffix = os.path.relpath(full, wps_dir).replace("\\", "/")
                rel = f"wps/{theme_name}/{suffix}"
                assets.append(self._asset_record("wps_assets", rel, f".rockbox/{rel}", full))

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

    def _font_assets_for_generated_skins(self, repo_root, stage_root, theme_name, primary_font_rel):
        names = set()
        if primary_font_rel:
            names.add(os.path.basename(primary_font_rel))
        for rel in (
            f"wps/{theme_name}.wps",
            f"wps/{theme_name}.sbs",
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
        self._apply_sprite_color_overrides(staged_wps_dir, variant)
        self._write_iconset(stage_root, variant, base_bundle)
        self._write_cfg(stage_root, variant, base_bundle)
        self._copy_template(base_bundle, "wps", stage_root, f"wps/{theme_name}.wps", variant)
        self._copy_template(base_bundle, "sbs", stage_root, f"wps/{theme_name}.sbs", variant)
        self._copy_template(base_bundle, "fms", stage_root, f"wps/{theme_name}.fms", variant)
        self._write_backdrop(stage_root, variant, staged_wps_dir)
        self._write_metadata(stage_root, variant)
        return stage_root

    @staticmethod
    def _is_preview_theme_id(theme_name):
        return str(theme_name or "") == PREVIEW_THEME_ID or str(theme_name or "").startswith("theme-designer-preview-")

    def _apply_wallpaper_overrides(self, staged_wps_dir, variant):
        targets = WALLPAPER_TARGETS.get(variant["base_theme_id"], {})
        sbs_solid_targets = set(SBS_SOLID_BACKGROUND_TARGETS.get(variant["base_theme_id"], ()))
        if variant.get("wallpaper_source"):
            for name in targets.get("main", []):
                if name in sbs_solid_targets:
                    continue
                self._render_image(
                    variant["wallpaper_source"],
                    os.path.join(staged_wps_dir, name),
                    variant["screen_resolution"],
                    variant.get("fit_mode", "fill"),
                    variant["colors"]["background"],
                )
            for name in sbs_solid_targets:
                self._render_solid_image(
                    os.path.join(staged_wps_dir, name),
                    variant["screen_resolution"],
                    variant["colors"]["background"],
                )
        else:
            for name in SOLID_BACKGROUND_TARGETS.get(variant["base_theme_id"], {}).get("main", []):
                self._render_solid_image(
                    os.path.join(staged_wps_dir, name),
                    variant["screen_resolution"],
                    variant["colors"]["background"],
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
        else:
            charging_fill = _mix_hex(variant["colors"]["background"], variant["colors"]["selector_start"], 0.55)
            for name in SOLID_BACKGROUND_TARGETS.get(variant["base_theme_id"], {}).get("charging", []):
                self._render_solid_image(
                    os.path.join(staged_wps_dir, name),
                    variant["screen_resolution"],
                    charging_fill,
                )
        if variant.get("right_pane_wallpaper_source"):
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

        overrides = {
            "wps": f"/.rockbox/wps/{variant['id']}.wps",
            "sbs": f"/.rockbox/wps/{variant['id']}.sbs",
            "fms": f"/.rockbox/wps/{variant['id']}.fms",
            "backdrop": f"/.rockbox/backdrops/{variant['id']}_bd.bmp",
            "font": f"/.rockbox/fonts/{os.path.basename(variant['font_rel'])}",
            "background color": variant["colors"]["background"],
            "foreground color": variant["colors"]["foreground"],
            "iconset": f"/.rockbox/icons/{variant['id']}.bmp",
            "line selector start color": variant["colors"]["selector_start"],
            "line selector end color": variant["colors"]["selector_end"],
            "line selector text color": variant["colors"]["selector_text"],
            "list separator color": variant["colors"]["list_separator"],
        }
        if variant.get("right_pane_wallpaper_source"):
            overrides["ipone right pane"] = "custom wallpaper"

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
            self._apply_template_color_overrides(target, base_bundle["id"], stage_root)
        if kind == "sbs" and variant and variant.get("right_pane_wallpaper_source"):
            self._force_generated_right_pane_wallpaper(target)

    def _force_generated_right_pane_wallpaper(self, path):
        try:
            with open(path, "r", encoding="utf-8") as handle:
                content = handle.read()
        except OSError:
            return
        image_branch = "%?if(%St(ipone right pane), =, custom wallpaper)<%xd(SbsBg)%xd(SbsRightWallpaper)|%?if(%St(ipone right pane), =, full art)<%xd(SbsBgFullArt)|%xd(SbsBg)>>"
        content = content.replace(image_branch, "%xd(SbsBg)%xd(SbsRightWallpaper)")
        draw_branch = "%?if(%St(ipone right pane), =, custom wallpaper)<%Vd(normal)|%?if(%St(ipone right pane), =, full art)<%Vd(SbsAnimPulse)%?mp<%Vd(normal)|%Vd(normal)|%Vd(normal)|%Vd(normal)|%Vd(normal)|%Vd(normal)|%Vd(normal)|%Vd(normal)|%Vd(normal)>|%?mp<%Vd(SbsAlbumArt)%Vd(normal)|%Vd(SbsAlbumArt)%Vd(normal)|%Vd(SbsAlbumArt)%Vd(normal)|%Vd(SbsAlbumArt)%Vd(normal)|%Vd(SbsAlbumArt)%Vd(normal)|%Vd(SbsAlbumArt)%Vd(normal)|%Vd(SbsAlbumArt)%Vd(normal)|%Vd(radio)|%Vd(radio)>>"
        content = content.replace(draw_branch, "%Vd(normal)")
        atomic_write_text(path, content)

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

        low = _hex_to_rgb(_mix_hex(variant["colors"]["background"], "000000", 0.35))
        high = _hex_to_rgb(variant["colors"]["selector_end"])
        output = Image.new("RGB", source.size)
        pixels = []
        for red, green, blue in source.getdata():
            luminance = (0.2126 * red + 0.7152 * green + 0.0722 * blue) / 255.0
            if luminance < 0.04:
                pixels.append((red, green, blue))
                continue
            pixels.append(
                (
                    int(round(low[0] + (high[0] - low[0]) * luminance)),
                    int(round(low[1] + (high[1] - low[1]) * luminance)),
                    int(round(low[2] + (high[2] - low[2]) * luminance)),
                )
            )
        output.putdata(pixels)
        output.save(target, "BMP")

    def _tint_luminance_bitmap(self, path, low_hex, high_hex):
        try:
            with Image.open(path) as img:
                source = img.convert("RGB")
        except (OSError, UnidentifiedImageError):
            return

        low = _hex_to_rgb(_mix_hex(low_hex, "000000", 0.25))
        high = _hex_to_rgb(high_hex)
        pixels = []
        for red, green, blue in source.getdata():
            luminance = (0.2126 * red + 0.7152 * green + 0.0722 * blue) / 255.0
            if luminance < 0.05:
                pixels.append((red, green, blue))
                continue
            pixels.append(
                (
                    int(round(low[0] + (high[0] - low[0]) * luminance)),
                    int(round(low[1] + (high[1] - low[1]) * luminance)),
                    int(round(low[2] + (high[2] - low[2]) * luminance)),
                )
            )
        output = Image.new("RGB", source.size)
        output.putdata(pixels)
        output.save(path, "BMP")

    def _write_backdrop(self, stage_root, variant, staged_wps_dir):
        backdrop_path = os.path.join(stage_root, "backdrops", f"{variant['id']}_bd.bmp")
        source = ""
        if variant.get("wallpaper_source"):
            source = variant["wallpaper_source"]
        else:
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
                "right_pane_offset_x": variant.get("right_pane_offset_x", 0),
                "right_pane_offset_y": variant.get("right_pane_offset_y", 0),
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
        base["right_pane_offset_x"] = self._normalize_offset(item.get("right_pane_offset_x", base.get("right_pane_offset_x", 0)))
        base["right_pane_offset_y"] = self._normalize_offset(item.get("right_pane_offset_y", base.get("right_pane_offset_y", 0)))
        base["wallpaper_source"] = os.path.abspath(str(item.get("wallpaper_source") or "").strip()) if item.get("wallpaper_source") else ""
        base["charging_wallpaper_source"] = os.path.abspath(str(item.get("charging_wallpaper_source") or "").strip()) if item.get("charging_wallpaper_source") else ""
        base["right_pane_wallpaper_source"] = os.path.abspath(str(item.get("right_pane_wallpaper_source") or "").strip()) if item.get("right_pane_wallpaper_source") else ""
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
        }
        return base

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
    def _normalize_font_rel(value):
        text = str(value or "").strip()
        if text.startswith("/.rockbox/fonts/"):
            return f"fonts/{os.path.basename(text)}"
        return text.replace("\\", "/")

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
        if should_render_2bpp_greyscale(resolution):
            rendered = render_2bpp_greyscale(rendered)
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        rendered.save(dest_path, "BMP")

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

    def _apply_template_color_overrides(self, path, base_theme_id, stage_root):
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

        replacements = {
            "1F1F24": background,
            "1B1B20": _mix_hex(background, "000000", 0.18),
            "151519": _mix_hex(background, "000000", 0.32),
            "242429": _mix_hex(background, foreground, 0.08),
            "202025": _mix_hex(background, foreground, 0.04),
            "18181D": _mix_hex(background, "000000", 0.24),
            "0B0B0D": _mix_hex(background, "000000", 0.42),
            "0F0E14": _mix_hex(background, "000000", 0.12),
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
            "E8DFF8": _mix_hex(foreground, selector_text, 0.26),
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

        for source, target in replacements.items():
            content = content.replace(source, target)
            content = content.replace(source.lower(), target.lower())

        atomic_write_text(path, content)
