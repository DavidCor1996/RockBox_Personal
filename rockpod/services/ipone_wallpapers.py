"""iPone lock/charge wallpaper discovery and device deploy bundles."""

from __future__ import annotations

import os
import re
from typing import Dict, List

from PIL import Image, ImageOps, UnidentifiedImageError

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

LOCKSCREEN_CLOCK_POSITIONS = {"center", "left"}

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

    def list_candidates(self, repo_root: str, profile: Dict) -> Dict[str, List[Dict]]:
        repo_root = os.path.abspath(repo_root)
        asset_dir = self._asset_dir(profile)
        generated_dir = os.path.join(repo_root, "rockpod", "generated")
        lock = []
        charge = []
        seen_lock = set()
        seen_charge = set()

        for filename in self._theme_wallpaper_files(repo_root, asset_dir, "lock"):
            full = os.path.join(repo_root, "wps", asset_dir, filename)
            self._append_candidate(lock, seen_lock, full, origin="theme", label=_theme_label(filename, "lock"))
        for filename in self._theme_wallpaper_files(repo_root, asset_dir, "charge"):
            full = os.path.join(repo_root, "wps", asset_dir, filename)
            self._append_candidate(charge, seen_charge, full, origin="theme", label=_theme_label(filename, "charge"))

        if os.path.isdir(generated_dir):
            for name in sorted(os.listdir(generated_dir)):
                full = os.path.join(generated_dir, name)
                if not os.path.isfile(full):
                    continue
                if self._is_intermediate_generated_file(name):
                    continue
                if self._matches(name, LOCK_PATTERNS):
                    self._append_candidate(lock, seen_lock, full, origin="generated")
                if self._matches(name, CHARGE_PATTERNS):
                    self._append_candidate(charge, seen_charge, full, origin="generated")

        return {"lock": lock, "charge": charge}

    def import_candidate(self, repo_root: str, profile: Dict, kind: str, source_path: str) -> Dict:
        repo_root = os.path.abspath(repo_root)
        source_abs = os.path.abspath(source_path)
        if kind not in {"lock", "charge"}:
            raise ValueError("Unknown wallpaper kind")
        if not os.path.isfile(source_abs):
            raise ValueError("Wallpaper source does not exist")

        generated_dir = os.path.join(repo_root, "rockpod", "generated")
        os.makedirs(generated_dir, exist_ok=True)
        stem = _slug(os.path.splitext(os.path.basename(source_abs))[0])
        prefix = "lockscreen" if kind == "lock" else "charge-wallpaper"
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
        return True

    def build_apply_bundle(
        self,
        profile: Dict,
        lock_source: str = "",
        charge_source: str = "",
        clock_position: str = "",
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
        if str(clock_position or "").strip():
            assets.extend(self._clock_layout_assets(profile, clock_position))
        if not assets:
            raise ValueError("No wallpaper or supported clock layout changes selected")
        return {
            "id": "ipone_wallpapers",
            "name": "iPone Wallpapers",
            "assets": assets,
            "preview_path": os.path.abspath(lock_source or charge_source),
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

    def _append_candidate(
        self,
        items: List[Dict],
        seen: set,
        source_abs: str,
        origin: str,
        label: str = "",
    ):
        deploy_abs = self._deployable_path(source_abs)
        if not os.path.isfile(deploy_abs):
            return
        key = os.path.normcase(os.path.abspath(deploy_abs))
        if key in seen:
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
                "width": width,
                "height": height,
            }
        )

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
    def _clock_layout_targets(profile: Dict, repo_root: str):
        theme = str(profile.get("selected_theme") or "").strip().lower()
        resolution = str(profile.get("screen_resolution") or "").strip()
        if resolution != "320x240" or "nano2g" in theme:
            return []
        candidates = [
            ("wps/iPone.sbs", ".rockbox/wps/iPone.sbs"),
        ]
        mount_path = os.path.abspath(profile.get("device_mount_path") or "")
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
        mount_path = os.path.abspath(profile.get("device_mount_path") or "")
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
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        with open(dest_path, "w", encoding="utf-8") as handle:
            handle.write(updated)

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
        prefix = "lockscreen" if kind == "lock" else "charge-wallpaper"
        dest_abs = os.path.join(staged_root, f"{prefix}-normalized.bmp")
        self._render_bmp(source_abs, dest_abs, str(profile.get("screen_resolution") or "320x240"))
        return dest_abs
