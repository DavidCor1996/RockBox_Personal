"""iPone lock/charge wallpaper discovery and device deploy bundles."""

from __future__ import annotations

import os
import re
from typing import Dict, List

from PIL import Image, ImageOps, UnidentifiedImageError

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

    def build_apply_bundle(self, profile: Dict, lock_source: str = "", charge_source: str = "") -> Dict:
        asset_dir = self._asset_dir(profile)
        assets = []
        if lock_source:
            lock_abs = os.path.abspath(lock_source)
            assets.extend(
                [
                    self._asset_record(
                        "lock_wallpaper",
                        lock_abs,
                        f".rockbox/wps/{asset_dir}/Wallpaper.bmp",
                    ),
                    self._asset_record(
                        "lock_wallpaper",
                        lock_abs,
                        f".rockbox/wps/{asset_dir}/WallpaperCurrent.bmp",
                    ),
                ]
            )
        if charge_source:
            charge_abs = os.path.abspath(charge_source)
            assets.append(
                self._asset_record(
                    "charge_wallpaper",
                    charge_abs,
                    f".rockbox/wps/{asset_dir}/ChargeWallpaper.bmp",
                )
            )
        if not assets:
            raise ValueError("No wallpaper selected")
        return {
            "id": "ipone_wallpapers",
            "name": "iPone Wallpapers",
            "assets": assets,
            "preview_path": os.path.abspath(lock_source or charge_source),
        }

    def _append_candidate(self, items: List[Dict], seen: set, source_abs: str, origin: str, label: str = ""):
        deploy_abs = self._deployable_path(source_abs)
        if not os.path.isfile(deploy_abs):
            return
        key = os.path.normcase(os.path.abspath(deploy_abs))
        if key in seen:
            return
        seen.add(key)
        preview_abs = self._previewable_path(source_abs)
        items.append(
            {
                "id": key,
                "label": label or _clean_label(source_abs),
                "source_path": deploy_abs,
                "preview_path": preview_abs,
                "origin": origin,
                "removable": origin == "generated",
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
        if "nano2g" in theme or str(profile.get("screen_resolution") or "") == "176x132":
            return "iPone_nano2g"
        return "iPone"

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
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        rendered.save(dest_path, "BMP")
