"""iPone lock/charge wallpaper discovery and device deploy bundles."""

from __future__ import annotations

import os
import json
import re
from typing import Dict, List

from PIL import Image, ImageFilter, ImageOps, ImageStat, UnidentifiedImageError

from services.file_safety import atomic_write_json, atomic_write_text
from services.greyscale_images import render_2bpp_greyscale, should_render_2bpp_greyscale

LOCK_DEFAULTS = [
    "Wallpaper.bmp",
    "WallpaperAlt.bmp",
    "WallpaperThird.bmp",
    "WallpaperFourth.bmp",
    "WallpaperFifth.bmp",
    "WallpaperSixth.bmp",
]

CHARGE_DEFAULTS = [
    "ChargeWallpaper.bmp",
    "ChargeWallpaperAlt.bmp",
    "ChargeWallpaperThird.bmp",
    "ChargeWallpaperFourth.bmp",
]

LOCK_PATTERNS = (
    re.compile(r"^lockscreen-.*\.(png|bmp|jpg|jpeg)$", re.I),
    re.compile(r"^wallpaper.*\.(png|bmp|jpg|jpeg)$", re.I),
)

CHARGE_PATTERNS = (
    re.compile(r"^charge-wallpaper.*\.(png|bmp|jpg|jpeg)$", re.I),
)

PICTUREFLOW_PATTERNS = (
    re.compile(r"^pictureflow-loading-bg.*\.(png|bmp|jpg|jpeg)$", re.I),
)

PICTUREFLOW_DEFAULT = "apps/plugins/bitmaps/native/pictureflow_loading_bg.320x240x24.bmp"
PICTUREFLOW_DESTINATION = ".rockbox/rocks/demos/pictureflow_loading_bg.bmp"

LOCKSCREEN_CLOCK_POSITIONS = {"top", "center", "custom", "left"}
LOCKSCREEN_CLOCK_ALIGNS = {"left", "center"}
LOCKSCREEN_CLOCK_STYLES = {"solid", "soft shadow", "outline", "glass", "glass tinted"}
LOCKSCREEN_GLASS_STRENGTHS = {"off", "low", "medium", "high"}
LOCKSCREEN_DATE_MODES = {"follow", "above", "below"}

LOCKSCREEN_TIME_CENTER = "%Vl(iPoneLockscreen,0,55,-,60,4)%Vf(FFFFFF)%ac%cl:%cM %cP"
LOCKSCREEN_TIME_LEFT_V1 = "%Vl(iPoneLockscreen,18,55,152,60,4)%Vf(FFFFFF)%al%cl:%cM %cP"
LOCKSCREEN_TIME_LEFT_V2 = "%Vl(iPoneLockscreen,14,50,156,60,4)%Vf(FFFFFF)%al%cl:%cM %cP"

LOCKSCREEN_DATE_CENTER = (
    "%Vl(iPoneLockscreen,0,120,-,20,3)%Vf(FFFFFF)%ac"
    "%?if(%ss(0,7,%St(lang)), =, english)<%?cu<Monday|Tuesday|Wednesday|Thursday|Friday|Saturday|Sunday>|%ca> "
    "%?or(%if(%ss(0,7,%St(lang)), =, chinese),%if(%St(lang), =, magyar),%if(%St(lang), =, lietuviu),"
    "%if(%St(lang), =, japanese),%if(%St(lang), =, korean))<%cb %cd|%?if(%St(lang), =, english-us)<%cb %cd|%cd %cb>>"
)
LOCKSCREEN_DATE_LEFT_V2 = (
    "%Vl(iPoneLockscreen,14,106,190,20,3)%Vf(FFFFFF)%al"
    "%?if(%ss(0,7,%St(lang)), =, english)<%?cu<Monday|Tuesday|Wednesday|Thursday|Friday|Saturday|Sunday>|%ca> "
    "%?or(%if(%ss(0,7,%St(lang)), =, chinese),%if(%St(lang), =, magyar),%if(%St(lang), =, lietuviu),"
    "%if(%St(lang), =, japanese),%if(%St(lang), =, korean))<%cb %cd|%?if(%St(lang), =, english-us)<%cb %cd|%cd %cb>>"
)

HIDDEN_WALLPAPERS_FILE = ".hidden_wallpapers.json"

DEFAULT_LOCKSCREEN_CUSTOMIZATION = {
    "wallpaper_id": "",
    "clock": {
        "position": "center",
        "x": 0,
        "y": 32,
        "width": 320,
        "height": 55,
        "align": "center",
        "font": "35-Adobe-Helvetica-Bold.fnt",
        "style": "solid",
        "color": "FFFFFF",
        "shadow": "soft",
        "glass_strength": "off",
        "opacity": 82,
    },
    "date": {
        "mode": "below",
        "y": 101,
        "font": "16-Adobe-Helvetica-Bold.fnt",
        "color": "FFFFFF",
    },
    "readability": {
        "auto_contrast": True,
        "min_contrast": 4.5,
        "sample_region": "clock_box",
    },
    "mini_player": {
        "style": "matched_blur",
        "blur_strength": "medium",
        "tint_source": "wallpaper",
        "tint_color": "2D2936",
        "text_color": "FFFFFF",
        "secondary_text_color": "C8BED7",
    },
}


def _clean_label(name: str) -> str:
    stem = os.path.splitext(os.path.basename(name))[0]
    stem = re.sub(r"[-_]+", " ", stem).strip()
    stem = re.sub(r"\bv(\d+)\b", r"V\1", stem, flags=re.I)
    return stem.title() or "Wallpaper"


def _slug(value: str) -> str:
    text = re.sub(r"[^a-z0-9]+", "-", str(value or "").strip().lower())
    return text.strip("-") or "wallpaper"


def _fit_size(resolution: str):
    width, height = str(resolution or "320x240").split("x", 1)
    return int(width), int(height)


def _theme_label(name: str, kind: str) -> str:
    base = os.path.basename(name)
    if kind == "lock":
        names = {
            "Wallpaper.bmp": "Theme: Original Lockscreen",
            "WallpaperAlt.bmp": "Theme: Besties",
            "WallpaperThird.bmp": "Theme: Portrait 1",
            "WallpaperFourth.bmp": "Theme: Portrait 2",
            "WallpaperFifth.bmp": "Theme: Portrait 3",
            "WallpaperSixth.bmp": "Theme: Portrait 4",
        }
    else:
        names = {
            "ChargeWallpaper.bmp": "Theme: Original Charge",
            "ChargeWallpaperAlt.bmp": "Theme: Charge Alt 1",
            "ChargeWallpaperThird.bmp": "Theme: Charge Alt 2",
            "ChargeWallpaperFourth.bmp": "Theme: Charge Alt 3",
        }
    return names.get(base, f"Theme: {_clean_label(base)}")


class IPoneWallpaperService:
    """Expose wallpaper choices and build direct device-copy bundles."""

    def list_candidates(self, repo_root: str, profile: Dict, include_hidden: bool = False) -> Dict[str, List[Dict]]:
        repo_root = os.path.abspath(repo_root)
        asset_dir = self._asset_dir(profile)
        generated_dir = os.path.join(repo_root, "rockpod", "generated")
        hidden = self._load_hidden(repo_root)
        lock = []
        charge = []
        pictureflow = []
        seen_lock = set()
        seen_charge = set()
        seen_pictureflow = set()

        for filename in self._theme_wallpaper_files(repo_root, asset_dir, "lock"):
            full = os.path.join(repo_root, "wps", asset_dir, filename)
            self._append_candidate(lock, seen_lock, full, repo_root, hidden, include_hidden, origin="theme", label=_theme_label(filename, "lock"))
        for filename in self._theme_wallpaper_files(repo_root, asset_dir, "charge"):
            full = os.path.join(repo_root, "wps", asset_dir, filename)
            self._append_candidate(charge, seen_charge, full, repo_root, hidden, include_hidden, origin="theme", label=_theme_label(filename, "charge"))
        pictureflow_default = os.path.join(repo_root, PICTUREFLOW_DEFAULT)
        self._append_candidate(
            pictureflow,
            seen_pictureflow,
            pictureflow_default,
            repo_root,
            hidden,
            include_hidden,
            origin="theme",
            label="Current Default PictureFlow Init",
        )

        if os.path.isdir(generated_dir):
            for name in sorted(os.listdir(generated_dir)):
                full = os.path.join(generated_dir, name)
                if not os.path.isfile(full):
                    continue
                if self._is_intermediate_generated_file(name):
                    continue
                if self._matches(name, LOCK_PATTERNS):
                    self._append_candidate(lock, seen_lock, full, repo_root, hidden, include_hidden, origin="generated")
                if self._matches(name, CHARGE_PATTERNS):
                    self._append_candidate(charge, seen_charge, full, repo_root, hidden, include_hidden, origin="generated")
                if self._matches(name, PICTUREFLOW_PATTERNS):
                    self._append_candidate(pictureflow, seen_pictureflow, full, repo_root, hidden, include_hidden, origin="generated")

        return {"lock": lock, "charge": charge, "pictureflow": pictureflow}

    def import_candidate(self, repo_root: str, profile: Dict, kind: str, source_path: str) -> Dict:
        repo_root = os.path.abspath(repo_root)
        source_abs = os.path.abspath(source_path)
        if kind not in {"lock", "charge", "pictureflow"}:
            raise ValueError("Unknown wallpaper kind")
        if not os.path.isfile(source_abs):
            raise ValueError("Wallpaper source does not exist")

        generated_dir = os.path.join(repo_root, "rockpod", "generated")
        os.makedirs(generated_dir, exist_ok=True)
        stem = _slug(os.path.splitext(os.path.basename(source_abs))[0])
        prefix = {
            "lock": "lockscreen",
            "charge": "charge-wallpaper",
            "pictureflow": "pictureflow-loading-bg",
        }[kind]
        dest_name = self._unique_generated_name(generated_dir, prefix, stem)
        dest_abs = os.path.join(generated_dir, dest_name)
        self._render_bmp(source_abs, dest_abs, str(profile.get("screen_resolution") or "320x240"))
        items = self.list_candidates(repo_root, profile)[kind]
        for item in items:
            if os.path.normcase(item["source_path"]) == os.path.normcase(dest_abs):
                return item
        return {
            "id": os.path.normcase(dest_abs),
            "label": _clean_label(dest_name),
            "source_path": dest_abs,
            "preview_path": dest_abs,
            "origin": "generated",
        }

    def remove_candidate(self, repo_root: str, source_path: str) -> bool:
        repo_root = os.path.abspath(repo_root)
        generated_dir = os.path.join(repo_root, "rockpod", "generated")
        source_abs = os.path.abspath(source_path)
        if not source_abs.startswith(os.path.abspath(generated_dir) + os.sep):
            raise ValueError("Only generated wallpapers can be removed")
        if not os.path.isfile(source_abs):
            return False
        os.remove(source_abs)
        self.set_hidden(repo_root, source_abs, False)
        return True

    def set_hidden(self, repo_root: str, source_path: str, hidden: bool = True) -> bool:
        repo_root = os.path.abspath(repo_root)
        source_abs = os.path.abspath(source_path)
        if not os.path.isfile(source_abs):
            return False
        key = self._hidden_key(repo_root, source_abs)
        data = self._load_hidden(repo_root)
        keys = set(data.get("hidden", []))
        if hidden:
            keys.add(key)
        else:
            keys.discard(key)
        data["hidden"] = sorted(keys)
        self._save_hidden(repo_root, data)
        return True

    def build_apply_bundle(
        self,
        profile: Dict,
        lock_source: str = "",
        charge_source: str = "",
        pictureflow_source: str = "",
        clock_position: str = "",
        lockscreen_customization: Dict | None = None,
    ) -> Dict:
        asset_dir = self._asset_dir(profile)
        repo_root = os.path.abspath(profile.get("source_repo_path") or "")
        assets = []
        if lock_source:
            lock_abs = self._normalized_apply_source(repo_root, profile, "lock", lock_source)
            lock_destinations = [
                f".rockbox/wps/{asset_dir}/Wallpaper.bmp",
                f".rockbox/wps/{asset_dir}/WallpaperCurrent.bmp",
            ]
            lock_destinations.extend(self._legacy_destinations(profile, "lock"))
            assets.extend(
                [
                    self._asset_record("lock_wallpaper", lock_abs, destination_rel)
                    for destination_rel in lock_destinations
                ]
            )
        if charge_source:
            charge_abs = self._normalized_apply_source(repo_root, profile, "charge", charge_source)
            charge_destinations = [
                f".rockbox/wps/{asset_dir}/{filename}"
                for filename in CHARGE_DEFAULTS
            ]
            charge_destinations.extend(self._legacy_destinations(profile, "charge"))
            assets.extend(
                [
                    self._asset_record("charge_wallpaper", charge_abs, destination_rel)
                    for destination_rel in charge_destinations
                ]
            )
        if pictureflow_source:
            pictureflow_abs = self._normalized_apply_source(repo_root, profile, "pictureflow", pictureflow_source)
            assets.append(
                self._asset_record("pictureflow_loading_wallpaper", pictureflow_abs, PICTUREFLOW_DESTINATION)
            )
        if str(clock_position or "").strip():
            assets.extend(self._clock_layout_assets(profile, clock_position))
        customization = self.normalize_lockscreen_customization(lockscreen_customization or {})
        if lockscreen_customization:
            assets.extend(
                self._lockscreen_customization_assets(
                    profile,
                    customization,
                    lock_source=lock_source,
                )
            )
        if not assets:
            raise ValueError("No wallpaper or supported clock layout changes selected")
        return {
            "id": "ipone_wallpapers",
            "name": "iPone Wallpapers",
            "assets": assets,
            "preview_path": os.path.abspath(lock_source or charge_source or pictureflow_source),
        }

    def _clock_layout_assets(self, profile: Dict, clock_position: str) -> List[Dict]:
        normalized = self._normalize_clock_position(clock_position)
        repo_root = os.path.abspath(profile.get("source_repo_path") or "")
        targets = self._clock_layout_targets(profile, repo_root)
        if not targets:
            return []
        staged_root = os.path.join(repo_root, "rockpod", ".wallpaper_clock", str(profile.get("id") or "default"))
        os.makedirs(staged_root, exist_ok=True)
        assets = []
        for source_rel, destination_rel in targets:
            source_abs = self._clock_layout_source_path(profile, repo_root, source_rel, destination_rel)
            asset_source = os.path.join(staged_root, os.path.basename(source_rel))
            self._render_clock_layout(source_abs, asset_source, normalized)
            assets.append(
                self._asset_record(
                    "lockscreen_clock_layout",
                    asset_source,
                    destination_rel,
                )
            )
        return assets

    def _lockscreen_customization_assets(self, profile: Dict, customization: Dict, lock_source: str = "") -> List[Dict]:
        repo_root = os.path.abspath(profile.get("source_repo_path") or "")
        if str(profile.get("screen_resolution") or "") != "320x240":
            return []
        targets = self._lockscreen_customization_targets(profile, repo_root)
        if not targets:
            return []

        profile_id = str(profile.get("id") or "default")
        staged_root = os.path.join(repo_root, "rockpod", ".wallpaper_clock", profile_id)
        os.makedirs(staged_root, exist_ok=True)

        wallpaper_source = self._lockscreen_wallpaper_source(profile, repo_root, lock_source)
        card_sources = {}
        clock_glass_sources = {}
        if wallpaper_source:
            customization = self._with_lockscreen_auto_contrast(wallpaper_source, customization)
            for asset_dir in {asset_dir for _source_rel, _destination_rel, asset_dir in targets}:
                card_path = os.path.join(staged_root, asset_dir, "LockMiniCardGenerated.bmp")
                self._render_lock_mini_card(wallpaper_source, card_path, customization)
                card_sources[asset_dir] = card_path
                if str(customization.get("clock", {}).get("style", "")).startswith("glass"):
                    glass_path = os.path.join(staged_root, asset_dir, "LockClockGlassGenerated.bmp")
                    self._render_lock_clock_glass(wallpaper_source, glass_path, customization)
                    clock_glass_sources[asset_dir] = glass_path

        assets = []
        for source_rel, destination_rel, asset_dir in targets:
            source_abs = self._clock_layout_source_path(profile, repo_root, source_rel, destination_rel)
            staged_sbs = os.path.join(staged_root, os.path.basename(source_rel))
            self._render_lockscreen_sbs(
                source_abs,
                staged_sbs,
                customization,
                bool(card_sources.get(asset_dir)),
                bool(clock_glass_sources.get(asset_dir)),
            )
            assets.append(
                self._asset_record(
                    "lockscreen_customization_sbs",
                    staged_sbs,
                    destination_rel,
                )
            )
            card_path = card_sources.get(asset_dir)
            if card_path:
                assets.append(
                    self._asset_record(
                        "lockscreen_mini_player_blur",
                        card_path,
                        f".rockbox/wps/{asset_dir}/LockMiniCardGenerated.bmp",
                    )
                )
            glass_path = clock_glass_sources.get(asset_dir)
            if glass_path:
                assets.append(
                    self._asset_record(
                        "lockscreen_clock_glass",
                        glass_path,
                        f".rockbox/wps/{asset_dir}/LockClockGlassGenerated.bmp",
                    )
                )
        metadata_path = os.path.join(staged_root, "lockscreen_customization.json")
        atomic_write_json(metadata_path, customization)
        assets.append(
            self._asset_record(
                "lockscreen_customization_metadata",
                metadata_path,
                ".rockbox/rockpod/lockscreen_customization.json",
            )
        )
        return assets

    def _append_candidate(
        self,
        items: List[Dict],
        seen: set,
        source_abs: str,
        repo_root: str,
        hidden: Dict,
        include_hidden: bool,
        origin: str,
        label: str = "",
    ):
        deploy_abs = self._deployable_path(source_abs)
        if not os.path.isfile(deploy_abs):
            return
        key = os.path.normcase(os.path.abspath(deploy_abs))
        if key in seen:
            return
        hidden_key = self._hidden_key(repo_root, deploy_abs)
        is_hidden = hidden_key in set(hidden.get("hidden", []))
        if is_hidden and not include_hidden:
            return
        seen.add(key)
        preview_abs = self._previewable_path(source_abs)
        width, height = self._image_size(preview_abs)
        items.append(
            {
                "id": key,
                "label": label or _clean_label(source_abs),
                "source_path": deploy_abs,
                "preview_path": preview_abs,
                "origin": origin,
                "removable": origin == "generated",
                "hidden": is_hidden,
                "hidden_key": hidden_key,
                "width": width,
                "height": height,
            }
        )

    @staticmethod
    def _hidden_path(repo_root: str) -> str:
        return os.path.join(os.path.abspath(repo_root), "rockpod", "generated", HIDDEN_WALLPAPERS_FILE)

    @staticmethod
    def _hidden_key(repo_root: str, source_abs: str) -> str:
        repo_root = os.path.abspath(repo_root)
        source_abs = os.path.abspath(source_abs)
        try:
            rel = os.path.relpath(source_abs, repo_root)
            if not rel.startswith("..") and rel != os.curdir:
                return rel.replace(os.sep, "/").casefold()
        except ValueError:
            pass
        return os.path.normcase(source_abs)

    def _load_hidden(self, repo_root: str) -> Dict:
        path = self._hidden_path(repo_root)
        try:
            with open(path, "r", encoding="utf-8") as handle:
                data = json.load(handle)
        except (OSError, json.JSONDecodeError):
            return {"hidden": []}
        hidden = data.get("hidden", [])
        if not isinstance(hidden, list):
            hidden = []
        return {"hidden": [str(item) for item in hidden]}

    def _save_hidden(self, repo_root: str, data: Dict):
        path = self._hidden_path(repo_root)
        atomic_write_json(path, {"hidden": list(data.get("hidden", []))})

    @staticmethod
    def _matches(name: str, patterns) -> bool:
        return any(pattern.match(name) for pattern in patterns)

    @staticmethod
    def _theme_wallpaper_files(repo_root: str, asset_dir: str, kind: str) -> List[str]:
        theme_dir = os.path.join(repo_root, "wps", asset_dir)
        defaults = LOCK_DEFAULTS[:] if kind == "lock" else CHARGE_DEFAULTS[:]
        if not os.path.isdir(theme_dir):
            return defaults
        names = []
        prefix = "Wallpaper" if kind == "lock" else "ChargeWallpaper"
        for name in sorted(os.listdir(theme_dir)):
            if not name.lower().endswith(".bmp"):
                continue
            if not name.startswith(prefix):
                continue
            if "Preview" in name:
                continue
            names.append(name)
        if not names:
            return defaults
        ordered = [name for name in defaults if name in names]
        ordered.extend(name for name in names if name not in ordered)
        return ordered

    @staticmethod
    def _is_intermediate_generated_file(name: str) -> bool:
        stem = os.path.splitext(name.lower())[0]
        return stem.endswith("-subject") or stem.endswith("-mask")

    @staticmethod
    def _previewable_path(path: str) -> str:
        return os.path.abspath(path)

    @staticmethod
    def _deployable_path(path: str) -> str:
        path = os.path.abspath(path)
        root, ext = os.path.splitext(path)
        if ext.lower() != ".bmp":
            bmp_path = root + ".bmp"
            if os.path.isfile(bmp_path):
                return bmp_path
        return path

    @staticmethod
    def _asset_record(kind: str, source_abs: str, destination_rel: str) -> Dict:
        return {
            "kind": kind,
            "source_rel": os.path.basename(source_abs),
            "source_abs": os.path.abspath(source_abs),
            "destination_rel": destination_rel,
            "exists": os.path.isfile(source_abs),
            "preview_path": os.path.abspath(source_abs) if os.path.isfile(source_abs) else "",
        }

    @staticmethod
    def _asset_dir(profile: Dict) -> str:
        theme = str(profile.get("selected_theme") or "").strip().lower()
        if theme == "blackery":
            return "Blackery"
        if theme == "ipone7g":
            return "iPone7G"
        if theme == "galaxy":
            return "Galaxy"
        if theme == "coverpod_3g":
            return "CoverPod_3g"
        if theme == "ipone_3g":
            return "iPone_3g"
        if str(profile.get("screen_resolution") or "") == "160x128":
            return "CoverPod_3g"
        if "nano2g" in theme or str(profile.get("screen_resolution") or "") == "176x132":
            return "iPone_nano2g"
        return "iPone"

    @staticmethod
    def normalize_lockscreen_customization(value: Dict | None) -> Dict:
        source = value if isinstance(value, dict) else {}
        result = json.loads(json.dumps(DEFAULT_LOCKSCREEN_CUSTOMIZATION))
        clock = source.get("clock") if isinstance(source.get("clock"), dict) else {}
        date = source.get("date") if isinstance(source.get("date"), dict) else {}
        readability = source.get("readability") if isinstance(source.get("readability"), dict) else {}
        mini_player = source.get("mini_player") if isinstance(source.get("mini_player"), dict) else {}

        result["wallpaper_id"] = str(source.get("wallpaper_id") or "").strip()
        result["clock"]["position"] = IPoneWallpaperService._choice(
            clock.get("position"),
            LOCKSCREEN_CLOCK_POSITIONS,
            result["clock"]["position"],
        )
        align_default = result["clock"]["align"]
        if "align" in clock:
            align_default = IPoneWallpaperService._choice(clock.get("align"), LOCKSCREEN_CLOCK_ALIGNS, align_default)
        x_default = result["clock"]["x"]
        width_default = result["clock"]["width"]
        if align_default == "left":
            x_default = 28
            width_default = 264
        result["clock"]["x"] = IPoneWallpaperService._int_between(clock.get("x"), 0, 319, x_default)
        result["clock"]["y"] = IPoneWallpaperService._int_between(clock.get("y"), 0, 64, result["clock"]["y"])
        result["clock"]["width"] = IPoneWallpaperService._int_between(clock.get("width"), 80, 320, width_default)
        result["clock"]["height"] = IPoneWallpaperService._int_between(clock.get("height"), 20, 100, result["clock"]["height"])
        result["clock"]["align"] = align_default
        result["clock"]["font"] = IPoneWallpaperService._clock_font(clock.get("font"))
        result["clock"]["style"] = IPoneWallpaperService._choice(
            clock.get("style"),
            LOCKSCREEN_CLOCK_STYLES,
            result["clock"]["style"],
        )
        result["clock"]["color"] = IPoneWallpaperService._hex_color(clock.get("color"), result["clock"]["color"])
        result["clock"]["shadow"] = "soft" if bool(clock.get("shadow", result["clock"]["shadow"] != "off")) else "off"
        result["clock"]["glass_strength"] = IPoneWallpaperService._choice(
            clock.get("glass_strength"),
            LOCKSCREEN_GLASS_STRENGTHS,
            result["clock"]["glass_strength"],
        )
        result["clock"]["opacity"] = IPoneWallpaperService._int_between(clock.get("opacity"), 40, 100, result["clock"]["opacity"])

        result["date"]["mode"] = IPoneWallpaperService._choice(date.get("mode"), LOCKSCREEN_DATE_MODES, result["date"]["mode"])
        clock_font_name = result["clock"]["font"]
        font_pixel_size = self._font_pixel_size(clock_font_name)
        effective_height = max(result["clock"]["height"], font_pixel_size)
        if "y" in date:
            date_y_default = result["date"]["y"]
        elif result["date"]["mode"] == "above":
            date_y_default = max(0, result["clock"]["y"] - 24)
        else:
            date_y_default = min(220, result["clock"]["y"] + effective_height + 4)
        result["date"]["y"] = IPoneWallpaperService._int_between(date.get("y"), 0, 220, date_y_default)
        result["date"]["color"] = IPoneWallpaperService._hex_color(date.get("color"), result["date"]["color"])
        result["readability"]["auto_contrast"] = bool(readability.get("auto_contrast", result["readability"]["auto_contrast"]))
        result["readability"]["min_contrast"] = IPoneWallpaperService._float_between(
            readability.get("min_contrast"),
            1.0,
            7.0,
            result["readability"]["min_contrast"],
        )
        result["mini_player"]["style"] = "matched_blur" if mini_player.get("style", "matched_blur") != "current" else "current"
        result["mini_player"]["blur_strength"] = IPoneWallpaperService._choice(
            mini_player.get("blur_strength"),
            {"low", "medium", "high"},
            result["mini_player"]["blur_strength"],
        )
        result["mini_player"]["tint_color"] = IPoneWallpaperService._hex_color(
            mini_player.get("tint_color"),
            result["mini_player"]["tint_color"],
        )
        result["mini_player"]["text_color"] = IPoneWallpaperService._hex_color(
            mini_player.get("text_color"),
            result["mini_player"]["text_color"],
        )
        result["mini_player"]["secondary_text_color"] = IPoneWallpaperService._hex_color(
            mini_player.get("secondary_text_color"),
            result["mini_player"]["secondary_text_color"],
        )
        return result

    @staticmethod
    def _legacy_destinations(profile: Dict, kind: str) -> List[str]:
        resolution = str(profile.get("screen_resolution") or "").strip()
        if resolution != "176x132":
            return []
        if kind == "lock":
            return [".rockbox/wps/Wallpaper.bmp"]
        if kind == "charge":
            return [".rockbox/wps/ChargeWallpaper.bmp"]
        return []

    @staticmethod
    def _choice(value, allowed, default):
        text = str(value or "").strip().lower()
        return text if text in allowed else default

    @staticmethod
    def _int_between(value, minimum: int, maximum: int, default: int) -> int:
        try:
            number = int(value)
        except (TypeError, ValueError):
            return default
        return max(minimum, min(maximum, number))

    @staticmethod
    def _float_between(value, minimum: float, maximum: float, default: float) -> float:
        try:
            number = float(value)
        except (TypeError, ValueError):
            return default
        return max(minimum, min(maximum, number))

    @staticmethod
    def _hex_color(value, default: str) -> str:
        text = str(value or "").strip().lstrip("#").upper()
        if re.fullmatch(r"[0-9A-F]{6}", text):
            return text
        return default

    @staticmethod
    def _font_pixel_size(font_name: str) -> int:
        match = re.match(r"(\d+)-", os.path.basename(str(font_name or "")))
        return int(match.group(1)) if match else 0

    def _clock_font(value) -> str:
        text = str(value or "").strip()
        base_allowed = {
            "35-Adobe-Helvetica-Bold.fnt",
            "16-Adobe-Helvetica-Bold.fnt",
            "18-Cantarell-Bold.fnt",
            "66-Cantarell-Light.fnt",
        }
        if text in base_allowed:
            return text
        match = re.match(r"(\d+)-", text)
        if match:
            size = int(match.group(1))
            if 50 <= size <= 90:
                return text
        return DEFAULT_LOCKSCREEN_CUSTOMIZATION["clock"]["font"]

    @staticmethod
    def _ensure_font_slot(content, slot, font_name):
        name = os.path.basename(str(font_name).strip())
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
    def _lockscreen_customization_targets(profile: Dict, repo_root: str):
        theme = str(profile.get("selected_theme") or "").strip().lower()
        resolution = str(profile.get("screen_resolution") or "").strip()
        if resolution != "320x240":
            return []
        candidates = []
        if theme == "ipone7g":
            candidates.append(("wps/iPone7G.sbs", ".rockbox/wps/iPone7G.sbs", "iPone7G"))
        elif theme == "ipone":
            candidates.append(("wps/iPone.sbs", ".rockbox/wps/iPone.sbs", "iPone"))
            if os.path.isfile(os.path.join(repo_root, "wps", "iPone7G.sbs")):
                candidates.append(("wps/iPone7G.sbs", ".rockbox/wps/iPone7G.sbs", "iPone7G"))
        else:
            return []

        mount_value = str(profile.get("device_mount_path") or "").strip()
        mount_path = os.path.abspath(mount_value) if mount_value else ""
        targets = []
        for source_rel, destination_rel, asset_dir in candidates:
            if mount_path:
                device_abs = os.path.join(mount_path, destination_rel.lstrip("/"))
                if os.path.isfile(device_abs):
                    targets.append((source_rel, destination_rel, asset_dir))
            elif os.path.isfile(os.path.join(repo_root, source_rel)):
                targets.append((source_rel, destination_rel, asset_dir))
        return targets

    @staticmethod
    def _lockscreen_wallpaper_source(profile: Dict, repo_root: str, lock_source: str) -> str:
        if lock_source and os.path.isfile(lock_source):
            return os.path.abspath(lock_source)
        asset_dir = IPoneWallpaperService._asset_dir(profile)
        candidates = [
            os.path.join(repo_root, "wps", asset_dir, "Wallpaper.bmp"),
            os.path.join(repo_root, "wps", "iPone", "Wallpaper.bmp"),
        ]
        mount_value = str(profile.get("device_mount_path") or "").strip()
        mount_path = os.path.abspath(mount_value) if mount_value else ""
        if mount_path:
            candidates.insert(0, os.path.join(mount_path, ".rockbox", "wps", asset_dir, "Wallpaper.bmp"))
            candidates.insert(1, os.path.join(mount_path, ".rockbox", "wps", "iPone", "Wallpaper.bmp"))
        for candidate in candidates:
            if os.path.isfile(candidate):
                return os.path.abspath(candidate)
        return ""

    @staticmethod
    def _render_lock_mini_card(source_path: str, dest_path: str, customization: Dict):
        try:
            with Image.open(source_path) as img:
                base = ImageOps.fit(img.convert("RGB"), (320, 240), Image.Resampling.LANCZOS)
        except (OSError, UnidentifiedImageError) as exc:
            raise ValueError(f"Unreadable lockscreen wallpaper: {source_path}") from exc

        crop = base.crop((22, 158, 272, 212))
        blur_name = customization.get("mini_player", {}).get("blur_strength", "medium")
        radius = {"low": 4, "medium": 7, "high": 10}.get(blur_name, 7)
        crop = crop.filter(ImageFilter.GaussianBlur(radius=radius)).convert("RGBA")

        stat = ImageStat.Stat(crop.convert("RGB"))
        avg = tuple(int(item) for item in stat.mean[:3])
        luminance = (avg[0] * 299 + avg[1] * 587 + avg[2] * 114) // 1000
        tint_hex = customization.get("mini_player", {}).get("tint_color", "2D2936")
        tint = tuple(int(tint_hex[index:index + 2], 16) for index in (0, 2, 4))
        overlay_alpha = 150 if luminance > 132 else 95
        overlay = Image.new("RGBA", crop.size, (*tint, overlay_alpha))
        card = Image.alpha_composite(crop, overlay)

        highlight = Image.new("RGBA", (card.width, 1), (255, 255, 255, 46))
        lowlight = Image.new("RGBA", (card.width, 1), (0, 0, 0, 72))
        card.alpha_composite(highlight, (0, 0))
        card.alpha_composite(lowlight, (0, card.height - 1))
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        card.convert("RGB").save(dest_path, "BMP")

    @staticmethod
    def _with_lockscreen_auto_contrast(source_path: str, customization: Dict) -> Dict:
        if not customization.get("readability", {}).get("auto_contrast", True):
            return customization
        clock = customization.get("clock", {})
        x = int(clock.get("x", 0))
        y = int(clock.get("y", 32))
        width = int(clock.get("width", 320))
        height = int(clock.get("height", 55))
        try:
            with Image.open(source_path) as img:
                base = ImageOps.fit(img.convert("RGB"), (320, 240), Image.Resampling.LANCZOS)
        except (OSError, UnidentifiedImageError) as exc:
            raise ValueError(f"Unreadable lockscreen wallpaper: {source_path}") from exc

        sample = base.crop((x, y, min(320, x + width), min(240, y + height)))
        stat = ImageStat.Stat(sample)
        avg = tuple(int(item) for item in stat.mean[:3])
        luminance = (avg[0] * 299 + avg[1] * 587 + avg[2] * 114) // 1000
        updated = json.loads(json.dumps(customization))
        if luminance >= 150:
            updated["clock"]["color"] = "16121D"
            updated["date"]["color"] = "2A2633"
        else:
            updated["clock"]["color"] = "FFFFFF"
            updated["date"]["color"] = "FFFFFF"
        return updated

    @staticmethod
    def _render_lock_clock_glass(source_path: str, dest_path: str, customization: Dict):
        clock = customization.get("clock", {})
        font_pixel_size = IPoneWallpaperService._font_pixel_size(clock.get("font", ""))
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

        crop = base.crop((x, y, min(320, x + width), min(240, y + height))).filter(
            ImageFilter.GaussianBlur(radius=5)
        ).convert("RGBA")
        strength = clock.get("glass_strength", "medium")
        style = clock.get("style", "glass")
        alpha = {"low": 38, "medium": 68, "high": 96}.get(strength, 68)
        tint = (255, 255, 255) if style == "glass" else (224, 210, 255)
        overlay = Image.new("RGBA", crop.size, (*tint, alpha))
        glass = Image.alpha_composite(crop, overlay)
        glass.alpha_composite(Image.new("RGBA", (glass.width, 1), (255, 255, 255, 90)), (0, 0))
        glass.alpha_composite(Image.new("RGBA", (glass.width, 1), (0, 0, 0, 58)), (0, glass.height - 1))
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        glass.convert("RGB").save(dest_path, "BMP")

    @staticmethod
    def _render_lockscreen_sbs(
        source_path: str,
        dest_path: str,
        customization: Dict,
        use_generated_card: bool,
        use_clock_glass: bool,
    ):
        try:
            with open(source_path, "r", encoding="utf-8") as handle:
                content = handle.read()
        except OSError as exc:
            raise ValueError(f"Unreadable SBS layout: {source_path}") from exc

        updated = IPoneWallpaperService._replace_lockscreen_clock(content, customization, use_clock_glass)
        if use_generated_card:
            updated = IPoneWallpaperService._enable_generated_lock_card(updated)
        if use_clock_glass:
            updated = IPoneWallpaperService._enable_generated_clock_glass(updated)
        atomic_write_text(dest_path, updated)

    @staticmethod
    def _replace_lockscreen_clock(content: str, customization: Dict, use_clock_glass: bool = False) -> str:
        clock = customization.get("clock", {})
        date = customization.get("date", {})
        x = int(clock.get("x", 0))
        y = int(clock.get("y", 32))
        width = int(clock.get("width", 320))
        height = int(clock.get("height", 55))
        if width >= 320:
            width_text = "-"
        else:
            width_text = str(width)
        clock_font_name = clock.get("font") or ""
        font_pixel_size = IPoneWallpaperService._font_pixel_size(clock_font_name)
        effective_height = max(height, font_pixel_size)
        base_font_ids = {
            "35-Adobe-Helvetica-Bold.fnt": "4",
            "16-Adobe-Helvetica-Bold.fnt": "3",
            "18-Cantarell-Bold.fnt": "9",
            "66-Cantarell-Light.fnt": "8",
        }
        font_id = base_font_ids.get(clock_font_name)
        if font_id is None:
            assigned = set(base_font_ids.values())
            slot = 30
            while str(slot) in assigned:
                slot += 1
            font_id = str(slot)
            content = IPoneWallpaperService._ensure_font_slot(content, slot, clock_font_name)
        align_tag = {"left": "%al", "center": "%ac"}.get(clock.get("align"), "%ac")
        color = clock.get("color", "FFFFFF")

        time_replacement = f"%Vl(iPoneLockscreen,{x},{y},{width_text},{effective_height},{font_id})%Vf({color}){align_tag}%cl:%cM %cP"
        if use_clock_glass:
            glass_y = max(0, y - 4)
            glass_height = min(240 - glass_y, effective_height + 8)
            glass_width = width_text
            time_replacement = (
                f"%Vl(LockClockGlass,{x},{glass_y},{glass_width},{glass_height},-)%xd(LockClockGlassGenerated)\n"
                f"{time_replacement}"
            )

        date_mode = date.get("mode", "below")
        date_y = int(date.get("y", min(220, y + effective_height + 4)))
        date_color = date.get("color", color)
        date_replacement = (
            f"%Vl(iPoneLockscreen,{x},{date_y},{width_text},20,6)%Vf({date_color}){align_tag}"
            "%?if(%ss(0,7,%St(lang)), =, english)<%?cu<Monday|Tuesday|Wednesday|Thursday|Friday|Saturday|Sunday>|%ca> "
            "%?or(%if(%ss(0,7,%St(lang)), =, chinese),%if(%St(lang), =, magyar),%if(%St(lang), =, lietuviu),"
            "%if(%St(lang), =, japanese),%if(%St(lang), =, korean))<%cb %cd|%?if(%St(lang), =, english-us)<%cb %cd|%cd %cb>>"
        )

        time_pattern = re.compile(r"%Vl\(iPoneLockscreen,[^\n]*%cl:%cM %cP")
        date_pattern = re.compile(r"%Vl\(iPoneLockscreen,[^\n]*%cd\|%cd %cb>>")
        updated, time_count = time_pattern.subn(time_replacement, content, count=1)
        updated, date_count = date_pattern.subn(date_replacement, updated, count=1)
        if time_count != 1 or date_count != 1:
            raise ValueError(f"Unsupported lockscreen clock layout: {time_count}/{date_count}")
        return updated

    @staticmethod
    def _enable_generated_clock_glass(content: str) -> str:
        if "%xl(LockClockGlassGenerated,LockClockGlassGenerated.bmp)" not in content:
            marker = "%xl(LsStyle,LockscreenStyle.bmp)\n"
            content = content.replace(marker, marker + "%xl(LockClockGlassGenerated,LockClockGlassGenerated.bmp)\n", 1)
        if "%Vd(LockClockGlass)%Vd(iPoneLockscreen)" in content:
            return content
        content = content.replace("%Vd(iPoneLockscreen)%?mp<", "%Vd(LockClockGlass)%Vd(iPoneLockscreen)%?mp<", 1)
        return content

    @staticmethod
    def _enable_generated_lock_card(content: str) -> str:
        if "%xl(LockMiniCardGenerated,LockMiniCardGenerated.bmp)" not in content:
            marker = "%xl(NotificationBackdrop,Notification.bmp,16,150)\n"
            content = content.replace(marker, marker + "%xl(LockMiniCardGenerated,LockMiniCardGenerated.bmp)\n", 1)
        content = re.sub(
            r"%Vl\(LockCardShadow,24,161,248,52,-\)%dr\(0,0,-,-,15121b\)\n",
            "%Vl(LockCardShadow,24,161,248,52,-)%dr(0,0,-,-,15121b)\n",
            content,
            count=1,
        )
        card_block = (
            "%Vl(LockCardOuter,22,159,248,52,-)%dr(0,0,-,-,26222f)\n"
            "%Vl(LockCardInner,24,161,244,48,-)%dr(0,0,-,-,2d2936)\n"
            "%Vl(LockCardHighlight,24,161,244,1,-)%dr(0,0,-,-,464056)\n"
            "%Vl(LockCardLowlight,24,208,244,1,-)%dr(0,0,-,-,18161f)"
        )
        generated_block = "%Vl(LockMiniCardGenerated,22,159,250,54,-)%xd(LockMiniCardGenerated)"
        if card_block in content:
            content = content.replace(card_block, generated_block, 1)
        return content

    @staticmethod
    def _clock_layout_targets(profile: Dict, repo_root: str):
        theme = str(profile.get("selected_theme") or "").strip().lower()
        resolution = str(profile.get("screen_resolution") or "").strip()
        if resolution != "320x240" or "nano2g" in theme:
            return []
        candidates = [
            ("wps/iPone.sbs", ".rockbox/wps/iPone.sbs"),
        ]
        mount_value = str(profile.get("device_mount_path") or "").strip()
        mount_path = os.path.abspath(mount_value) if mount_value else ""
        targets = []
        for source_rel, destination_rel in candidates:
            if mount_path:
                device_abs = os.path.join(mount_path, destination_rel.lstrip("/"))
                if os.path.isfile(device_abs):
                    targets.append((source_rel, destination_rel))
            elif os.path.isfile(os.path.join(repo_root, source_rel)):
                targets.append((source_rel, destination_rel))
        return targets

    @staticmethod
    def _clock_layout_source_path(profile: Dict, repo_root: str, source_rel: str, destination_rel: str) -> str:
        mount_value = str(profile.get("device_mount_path") or "").strip()
        mount_path = os.path.abspath(mount_value) if mount_value else ""
        if mount_path:
            device_abs = os.path.join(mount_path, destination_rel.lstrip("/"))
            if os.path.isfile(device_abs):
                return device_abs
        return os.path.join(repo_root, source_rel)

    @staticmethod
    def _normalize_clock_position(value: str) -> str:
        text = str(value or "").strip().lower()
        if text in LOCKSCREEN_CLOCK_POSITIONS:
            return text
        return "center"

    @staticmethod
    def _render_clock_layout(source_path: str, dest_path: str, clock_position: str):
        try:
            with open(source_path, "r", encoding="utf-8") as handle:
                content = handle.read()
        except OSError as exc:
            raise ValueError(f"Unreadable SBS layout: {source_path}") from exc

        time_variants = (
            LOCKSCREEN_TIME_CENTER,
            LOCKSCREEN_TIME_LEFT_V1,
            LOCKSCREEN_TIME_LEFT_V2,
        )
        date_variants = (
            LOCKSCREEN_DATE_CENTER,
            LOCKSCREEN_DATE_LEFT_V2,
        )
        if not any(item in content for item in time_variants) or not any(item in content for item in date_variants):
            raise ValueError(f"Unsupported SBS layout: {source_path}")

        if clock_position == "left":
            updated = IPoneWallpaperService._replace_first(content, time_variants, LOCKSCREEN_TIME_LEFT_V2)
            updated = IPoneWallpaperService._replace_first(updated, date_variants, LOCKSCREEN_DATE_LEFT_V2)
        else:
            updated = IPoneWallpaperService._replace_first(content, time_variants, LOCKSCREEN_TIME_CENTER)
            updated = IPoneWallpaperService._replace_first(updated, date_variants, LOCKSCREEN_DATE_CENTER)
        atomic_write_text(dest_path, updated)

    @staticmethod
    def _replace_first(content: str, candidates, replacement: str) -> str:
        for candidate in candidates:
            if candidate in content:
                return content.replace(candidate, replacement, 1)
        return content

    @staticmethod
    def _unique_generated_name(generated_dir: str, prefix: str, stem: str) -> str:
        candidate = f"{prefix}-{stem}.bmp"
        if not os.path.exists(os.path.join(generated_dir, candidate)):
            return candidate
        index = 2
        while True:
            candidate = f"{prefix}-{stem}-v{index}.bmp"
            if not os.path.exists(os.path.join(generated_dir, candidate)):
                return candidate
            index += 1

    @staticmethod
    def _render_bmp(source_path: str, dest_path: str, resolution: str):
        width, height = _fit_size(resolution)
        try:
            with Image.open(source_path) as img:
                image = img.convert("RGB")
        except (OSError, UnidentifiedImageError) as exc:
            raise ValueError(f"Unreadable wallpaper: {source_path}") from exc
        rendered = ImageOps.fit(image, (width, height), Image.Resampling.LANCZOS)
        if should_render_2bpp_greyscale(resolution):
            rendered = render_2bpp_greyscale(rendered)
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        rendered.save(dest_path, "BMP")

    @staticmethod
    def _image_size(source_path: str):
        try:
            with Image.open(source_path) as img:
                return img.size
        except (OSError, UnidentifiedImageError):
            return (0, 0)

    def _normalized_apply_source(self, repo_root: str, profile: Dict, kind: str, source_path: str) -> str:
        source_abs = os.path.abspath(source_path)
        staged_root = os.path.join(repo_root, "rockpod", ".wallpaper_apply", str(profile.get("id") or "default"))
        os.makedirs(staged_root, exist_ok=True)
        prefix = {
            "lock": "lockscreen",
            "charge": "charge-wallpaper",
            "pictureflow": "pictureflow-loading-bg",
        }.get(kind, "wallpaper")
        dest_abs = os.path.join(staged_root, f"{prefix}-normalized.bmp")
        self._render_bmp(source_abs, dest_abs, str(profile.get("screen_resolution") or "320x240"))
        return dest_abs
