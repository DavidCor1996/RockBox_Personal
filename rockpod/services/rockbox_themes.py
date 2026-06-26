"""Deterministic Rockbox theme stack definitions for RockPod."""

from __future__ import annotations

import os
import re
from copy import deepcopy


THEME_DEFINITIONS = {
    "Blackery": {
        "name": "Blackery",
        "description": "Modern dark teal Blackery stack for 320x240 iPods.",
        "resolutions": ["320x240"],
        "assets": [
            {"kind": "cfg", "source": "themes/Blackery.cfg", "destination": ".rockbox/themes/Blackery.cfg"},
            {"kind": "wps", "source": "wps/Blackery.wps", "destination": ".rockbox/wps/Blackery.wps"},
            {"kind": "sbs", "source": "wps/Blackery.sbs", "destination": ".rockbox/wps/Blackery.sbs"},
            {"kind": "fms", "source": "wps/Blackery.fms", "destination": ".rockbox/wps/Blackery.fms"},
            {"kind": "backdrop", "source": "backdrops/Blackery_bd.bmp", "destination": ".rockbox/backdrops/Blackery_bd.bmp"},
            {"kind": "iconset", "source": "icons/Blackery.bmp", "destination": ".rockbox/icons/Blackery.bmp"},
            {"kind": "font", "source": "fonts/24 iLike.fnt", "destination": ".rockbox/fonts/24 iLike.fnt"},
            {"kind": "wps_assets", "source": "wps/Blackery", "destination": ".rockbox/wps/Blackery", "recursive": True},
        ],
        "preview_candidates": [
            "wps/Blackery/Wallpaper.bmp",
            "wps/Blackery/SbsBackdrop.bmp",
            "backdrops/Blackery_bd.bmp",
            "icons/Blackery.bmp",
        ],
    },
    "iPone": {
        "name": "iPone",
        "description": "Dark-mode iPone stack for 320x240 iPods.",
        "resolutions": ["320x240"],
        "assets": [
            {"kind": "cfg", "source": "themes/iPone.cfg", "destination": ".rockbox/themes/iPone.cfg"},
            {"kind": "wps", "source": "wps/iPone.wps", "destination": ".rockbox/wps/iPone.wps"},
            {"kind": "sbs", "source": "wps/iPone.sbs", "destination": ".rockbox/wps/iPone.sbs"},
            {"kind": "fms", "source": "wps/iPone.fms", "destination": ".rockbox/wps/iPone.fms"},
            {"kind": "backdrop", "source": "backdrops/iPone_bd.bmp", "destination": ".rockbox/backdrops/iPone_bd.bmp"},
            {"kind": "iconset", "source": "icons/iPone.bmp", "destination": ".rockbox/icons/iPone.bmp"},
            {"kind": "font", "source": "fonts/24 iLike.fnt", "destination": ".rockbox/fonts/24 iLike.fnt"},
            {"kind": "wps_assets", "source": "wps/iPone", "destination": ".rockbox/wps/iPone", "recursive": True},
        ],
        "preview_candidates": [
            "wps/iPone/Wallpaper.bmp",
            "wps/iPone/SbsBackdrop.bmp",
            "backdrops/iPone_bd.bmp",
            "icons/iPone.bmp",
        ],
    },
    "iPoneCustom": {
        "name": "iPone Custom",
        "description": "Experimental iPone stack with RockPod-managed colors and right-pane wallpaper.",
        "resolutions": ["320x240"],
        "assets": [
            {"kind": "cfg", "source": "themes/iPoneCustom.cfg", "destination": ".rockbox/themes/iPoneCustom.cfg"},
            {"kind": "wps", "source": "wps/iPoneCustom.wps", "destination": ".rockbox/wps/iPoneCustom.wps"},
            {"kind": "sbs", "source": "wps/iPoneCustom.sbs", "destination": ".rockbox/wps/iPoneCustom.sbs"},
            {"kind": "fms", "source": "wps/iPoneCustom.fms", "destination": ".rockbox/wps/iPoneCustom.fms"},
            {"kind": "backdrop", "source": "backdrops/iPoneCustom_bd.bmp", "destination": ".rockbox/backdrops/iPoneCustom_bd.bmp"},
            {"kind": "iconset", "source": "icons/iPoneCustom.bmp", "destination": ".rockbox/icons/iPoneCustom.bmp"},
            {"kind": "font", "source": "fonts/24 iLike.fnt", "destination": ".rockbox/fonts/24 iLike.fnt"},
            {"kind": "wps_assets", "source": "wps/iPoneCustom", "destination": ".rockbox/wps/iPoneCustom", "recursive": True},
        ],
        "preview_candidates": [
            "wps/iPoneCustom/RightPaneWallpaper.bmp",
            "wps/iPoneCustom/Wallpaper.bmp",
            "wps/iPoneCustom/SbsBackdrop.bmp",
            "backdrops/iPoneCustom_bd.bmp",
            "icons/iPoneCustom.bmp",
        ],
    },
    "SpringPod3": {
        "name": "SpringPod3",
        "description": "iOS 3-inspired Aqua stack for iPod Video 5G / 5.5G only.",
        "resolutions": ["320x240"],
        "compatible_device_models": [
            "iPod Video 5G",
            "iPod Video 5.5G",
            "ipodvideo",
            "ipodvideo64mb",
            "build-sim-video-5g",
        ],
        "assets": [
            {"kind": "cfg", "source": "themes/SpringPod3.cfg", "destination": ".rockbox/themes/SpringPod3.cfg"},
            {"kind": "wps", "source": "wps/SpringPod3.wps", "destination": ".rockbox/wps/SpringPod3.wps"},
            {"kind": "sbs", "source": "wps/SpringPod3.sbs", "destination": ".rockbox/wps/SpringPod3.sbs"},
            {"kind": "fms", "source": "wps/SpringPod3.fms", "destination": ".rockbox/wps/SpringPod3.fms"},
            {"kind": "backdrop", "source": "backdrops/SpringPod3_bd.bmp", "destination": ".rockbox/backdrops/SpringPod3_bd.bmp"},
            {"kind": "iconset", "source": "icons/SpringPod3.bmp", "destination": ".rockbox/icons/SpringPod3.bmp"},
            {"kind": "font", "source": "fonts/24 iLike.fnt", "destination": ".rockbox/fonts/24 iLike.fnt"},
            {"kind": "wps_assets", "source": "wps/SpringPod3", "destination": ".rockbox/wps/SpringPod3", "recursive": True},
        ],
        "preview_candidates": [
            "wps/SpringPod3/Wallpaper.bmp",
            "wps/SpringPod3/SpringPod3_bd.bmp",
            "backdrops/SpringPod3_bd.bmp",
            "icons/SpringPod3.bmp",
        ],
    },
    "iPone_nano2g": {
        "name": "iPone Nano 2G",
        "description": "Compact iPone port for the 176x132 nano 2G.",
        "resolutions": ["176x132"],
        "assets": [
            {"kind": "cfg", "source": "themes/iPone_nano2g.cfg", "destination": ".rockbox/themes/iPone_nano2g.cfg"},
            {"kind": "wps", "source": "wps/iPone_nano2g.wps", "destination": ".rockbox/wps/iPone_nano2g.wps"},
            {"kind": "sbs", "source": "wps/iPone_nano2g.sbs", "destination": ".rockbox/wps/iPone_nano2g.sbs"},
            {"kind": "fms", "source": "wps/iPone_nano2g.fms", "destination": ".rockbox/wps/iPone_nano2g.fms"},
            {"kind": "backdrop", "source": "wps/iPone_nano2g/wpsbackdrop-176x132x16.bmp", "destination": ".rockbox/wps/iPone_nano2g/wpsbackdrop-176x132x16.bmp"},
            {"kind": "iconset", "source": "icons/tango_icons.12x12.bmp", "destination": ".rockbox/icons/tango_icons.12x12.bmp"},
            {"kind": "font", "source": "fonts/12-Adobe-Helvetica.fnt", "destination": ".rockbox/fonts/12-Adobe-Helvetica.fnt"},
            {"kind": "wps_assets", "source": "wps/iPone_nano2g", "destination": ".rockbox/wps/iPone_nano2g", "recursive": True},
        ],
        "preview_candidates": [
            "wps/iPone_nano2g/Wallpaper.bmp",
            "wps/iPone_nano2g/BootLogo.bmp",
        ],
    },
    "Galaxy": {
        "name": "Galaxy",
        "description": "Modern monochrome split-screen stack for the 160x128 iPod 3G.",
        "resolutions": ["160x128"],
        "assets": [
            {"kind": "cfg", "source": "themes/Galaxy.cfg", "destination": ".rockbox/themes/Galaxy.cfg"},
            {"kind": "wps", "source": "wps/Galaxy.wps", "destination": ".rockbox/wps/Galaxy.wps"},
            {"kind": "sbs", "source": "wps/Galaxy.sbs", "destination": ".rockbox/wps/Galaxy.sbs"},
            {"kind": "fms", "source": "wps/Galaxy.fms", "destination": ".rockbox/wps/Galaxy.fms"},
            {"kind": "iconset", "source": "icons/tango_small_mono.bmp", "destination": ".rockbox/icons/tango_small_mono.bmp"},
            {"kind": "font", "source": "fonts/12-Adobe-Helvetica.fnt", "destination": ".rockbox/fonts/12-Adobe-Helvetica.fnt"},
            {"kind": "wps_assets", "source": "wps/Galaxy", "destination": ".rockbox/wps/Galaxy", "recursive": True},
        ],
        "preview_candidates": [
            "wps/Galaxy/MenuBackdrop.bmp",
            "wps/Galaxy/Wallpaper.bmp",
            "wps/Galaxy/wpsbackdrop-160x128x2.bmp",
        ],
    },
    "CoverPod_3g": {
        "name": "CoverPod 3G",
        "description": "CoverMax-derived four-shade greyscale stack for the 160x128 iPod 3G.",
        "resolutions": ["160x128"],
        "assets": [
            {"kind": "cfg", "source": "themes/CoverPod_3g.cfg", "destination": ".rockbox/themes/CoverPod_3g.cfg"},
            {"kind": "wps", "source": "wps/CoverPod_3g.wps", "destination": ".rockbox/wps/CoverPod_3g.wps"},
            {"kind": "sbs", "source": "wps/CoverPod_3g.sbs", "destination": ".rockbox/wps/CoverPod_3g.sbs"},
            {"kind": "fms", "source": "wps/CoverPod_3g.fms", "destination": ".rockbox/wps/CoverPod_3g.fms"},
            {"kind": "iconset", "source": "icons/tango_icons.12x12.bmp", "destination": ".rockbox/icons/tango_icons.12x12.bmp"},
            {"kind": "font", "source": "fonts/12-Adobe-Helvetica.fnt", "destination": ".rockbox/fonts/12-Adobe-Helvetica.fnt"},
            {"kind": "wps_assets", "source": "wps/CoverPod_3g", "destination": ".rockbox/wps/CoverPod_3g", "recursive": True},
        ],
        "preview_candidates": [
            "wps/CoverPod_3g/Wallpaper.bmp",
            "wps/CoverPod_3g/wpsbackdrop-160x128x2.bmp",
            "wps/CoverPod_3g/ChargeWallpaper.bmp",
        ],
    },
    "iPone_3g": {
        "name": "iPone 3G",
        "description": "Monochrome iPone stack for the 160x128 iPod 3G.",
        "resolutions": ["160x128"],
        "assets": [
            {"kind": "cfg", "source": "themes/iPone_3g.cfg", "destination": ".rockbox/themes/iPone_3g.cfg"},
            {"kind": "wps", "source": "wps/iPone_3g.wps", "destination": ".rockbox/wps/iPone_3g.wps"},
            {"kind": "sbs", "source": "wps/iPone_3g.sbs", "destination": ".rockbox/wps/iPone_3g.sbs"},
            {"kind": "fms", "source": "wps/iPone_3g.fms", "destination": ".rockbox/wps/iPone_3g.fms"},
            {"kind": "iconset", "source": "icons/tango_icons.12x12.bmp", "destination": ".rockbox/icons/tango_icons.12x12.bmp"},
            {"kind": "font", "source": "fonts/12-Adobe-Helvetica.fnt", "destination": ".rockbox/fonts/12-Adobe-Helvetica.fnt"},
            {"kind": "wps_assets", "source": "wps/iPone_3g", "destination": ".rockbox/wps/iPone_3g", "recursive": True},
        ],
        "preview_candidates": [
            "wps/iPone_3g/Wallpaper.bmp",
            "wps/iPone_3g/wpsbackdrop-160x128x2.bmp",
            "wps/iPone_3g/ChargeWallpaper.bmp",
        ],
    },
}


class RockboxThemeService:
    """Expose deterministic theme stacks and completeness information."""

    def list_themes(self, source_repo_path, screen_resolution="", target_device_model="", device_mount_path=""):
        resolution = str(screen_resolution or "").strip()
        result = []
        for theme_id, definition in THEME_DEFINITIONS.items():
            if resolution and definition["resolutions"] and resolution not in definition["resolutions"]:
                continue
            if not self._is_compatible_with_model(definition, target_device_model):
                continue
            info = self.inspect_theme(theme_id, source_repo_path)
            result.append({
                "id": theme_id,
                "name": info["name"],
                "description": info["description"],
                "status": info["status"],
                "found_count": info["found_count"],
                "asset_count": info["asset_count"],
                "preview_path": info["preview_path"],
            })
        result.extend(self.list_device_themes(device_mount_path, known_ids={item["id"] for item in result}))
        return result

    def list_device_themes(self, device_mount_path, known_ids=None):
        known = {str(item or "").strip() for item in (known_ids or set())}
        themes_dir = os.path.join(os.path.abspath(device_mount_path or ""), ".rockbox", "themes")
        if not os.path.isdir(themes_dir):
            return []
        result = []
        for name in sorted(os.listdir(themes_dir)):
            if not name.lower().endswith(".cfg"):
                continue
            theme_id = os.path.splitext(name)[0]
            if not theme_id or theme_id in known:
                continue
            result.append(
                {
                    "id": theme_id,
                    "name": f"{theme_id} (device)",
                    "description": "Theme found on the connected device but not in this RockPod repo.",
                    "status": "Device only",
                    "found_count": 1,
                    "asset_count": 1,
                    "preview_path": "",
                    "device_only": True,
                }
            )
        return result

    def inspect_theme(self, theme_id, source_repo_path):
        base = os.path.abspath(source_repo_path)
        definition = deepcopy(THEME_DEFINITIONS[theme_id])
        assets = []
        for asset in definition["assets"]:
            assets.extend(self._expand_asset(base, asset))
        found_count = sum(1 for item in assets if item["exists"])
        missing_count = len(assets) - found_count
        preview_path = ""
        for rel in definition.get("preview_candidates", []):
            candidate = os.path.join(base, rel)
            if os.path.isfile(candidate):
                preview_path = candidate
                break
        if not preview_path:
            for item in assets:
                if item["exists"] and item["source_abs"].lower().endswith((".bmp", ".png", ".jpg", ".jpeg")):
                    preview_path = item["source_abs"]
                    break
        status = "Complete" if missing_count == 0 else f"Missing {missing_count}"
        return {
            "id": theme_id,
            "name": definition["name"],
            "description": definition["description"],
            "status": status,
            "asset_count": len(assets),
            "found_count": found_count,
            "missing_count": missing_count,
            "preview_path": preview_path,
            "assets": assets,
            "resolutions": list(definition.get("resolutions", [])),
            "compatible_device_models": list(definition.get("compatible_device_models", [])),
        }

    def inspect_device_theme(self, theme_id, device_mount_path):
        theme_id = str(theme_id or "").strip()
        root = os.path.abspath(device_mount_path or "")
        cfg_rel = f".rockbox/themes/{theme_id}.cfg"
        cfg_abs = os.path.join(root, cfg_rel)
        assets = [self._device_asset_record("cfg", cfg_rel, cfg_abs)]
        settings = self._read_cfg_settings(cfg_abs)
        for key, kind in (
            ("wps", "wps"),
            ("sbs", "sbs"),
            ("fms", "fms"),
            ("backdrop", "backdrop"),
            ("iconset", "iconset"),
            ("viewers iconset", "iconset"),
            ("font", "font"),
        ):
            rel = self._device_setting_rel(settings.get(key, ""))
            if rel:
                assets.append(self._device_asset_record(kind, rel, os.path.join(root, rel)))

        wps_dir = os.path.join(root, ".rockbox", "wps", theme_id)
        if os.path.isdir(wps_dir):
            for dir_root, _dirs, files in os.walk(wps_dir):
                for filename in sorted(files):
                    full = os.path.join(dir_root, filename)
                    rel = os.path.relpath(full, root).replace("\\", "/")
                    assets.append(self._device_asset_record("wps_assets", rel, full))

        unique = []
        seen = set()
        for asset in assets:
            key = asset["destination_rel"]
            if key in seen:
                continue
            seen.add(key)
            unique.append(asset)

        found_count = sum(1 for item in unique if item["exists"])
        return {
            "id": theme_id,
            "name": f"{theme_id} (device)",
            "description": "Theme found on the connected device but not in this RockPod repo.",
            "status": "Device only",
            "asset_count": len(unique),
            "found_count": found_count,
            "missing_count": len(unique) - found_count,
            "preview_path": "",
            "assets": unique,
            "resolutions": [],
            "compatible_device_models": [],
            "device_only": True,
        }

    def bundle_for_theme(self, theme_id, source_repo_path):
        return self.inspect_theme(theme_id, source_repo_path)

    def remove_bundle_for_theme(self, theme_id, source_repo_path, device_mount_path=""):
        if str(theme_id or "").strip() in THEME_DEFINITIONS:
            bundle = self.inspect_theme(theme_id, source_repo_path)
        else:
            bundle = self.inspect_device_theme(theme_id, device_mount_path)
        assets = []
        for asset in bundle["assets"]:
            item = dict(asset)
            item["action"] = "remove"
            item["exists"] = True
            assets.append(item)
        return {
            "id": f"{theme_id}-remove",
            "name": f"Remove {bundle['name']}",
            "description": f"Remove {bundle['name']} files from the selected device.",
            "status": "Remove",
            "asset_count": len(assets),
            "found_count": len(assets),
            "missing_count": 0,
            "preview_path": bundle.get("preview_path", ""),
            "assets": assets,
            "resolutions": bundle.get("resolutions", []),
            "compatible_device_models": bundle.get("compatible_device_models", []),
        }

    def validate_skin_references(self, theme_id, source_repo_path, kinds=("wps", "sbs", "fms")):
        """Validate bitmap references in a theme's WPS/SBS/FMS files."""
        base = os.path.abspath(source_repo_path)
        bundle = self.bundle_for_theme(theme_id, base)
        wanted = {str(kind).strip().lower() for kind in kinds}
        skin_assets = [
            item for item in bundle["assets"]
            if item["kind"] in wanted and item["exists"]
        ]
        missing = []
        checked = []
        for skin in skin_assets:
            checked.append(skin["source_rel"])
            for image_ref in self._skin_image_references(skin["source_abs"]):
                resolved = self._resolve_skin_image_reference(base, theme_id, image_ref)
                if not resolved:
                    missing.append(
                        {
                            "skin": skin["source_rel"],
                            "reference": image_ref,
                        }
                    )
        return {
            "theme_id": theme_id,
            "checked": checked,
            "missing": missing,
            "success": not missing,
        }

    def _expand_asset(self, base, asset):
        source_rel = asset["source"]
        dest_rel = asset["destination"]
        source_abs = os.path.join(base, source_rel)
        if asset.get("recursive"):
            return self._expand_directory(base, source_rel, dest_rel, asset["kind"])
        return [self._asset_record(asset["kind"], source_rel, dest_rel, source_abs)]

    def _expand_directory(self, base, source_rel, dest_rel, kind):
        source_abs = os.path.join(base, source_rel)
        if not os.path.isdir(source_abs):
            return [self._asset_record(kind, source_rel, dest_rel, source_abs)]
        records = []
        for root, _dirs, files in os.walk(source_abs):
            files = sorted(files)
            for filename in files:
                full = os.path.join(root, filename)
                rel_suffix = os.path.relpath(full, source_abs).replace("\\", "/")
                records.append(
                    self._asset_record(
                        kind,
                        f"{source_rel.rstrip('/')}/{rel_suffix}",
                        f"{dest_rel.rstrip('/')}/{rel_suffix}",
                        full,
                    )
                )
        return records

    @staticmethod
    def _asset_record(kind, source_rel, dest_rel, source_abs):
        exists = os.path.isfile(source_abs)
        size = 0
        if exists:
            try:
                size = os.path.getsize(source_abs)
            except OSError:
                size = 0
        return {
            "kind": kind,
            "source_rel": source_rel.replace("\\", "/"),
            "source_abs": os.path.abspath(source_abs),
            "destination_rel": dest_rel.replace("\\", "/"),
            "exists": exists,
            "size": size,
            "preview_path": os.path.abspath(source_abs) if exists else "",
        }

    @staticmethod
    def _device_asset_record(kind, rel_path, source_abs):
        rel = str(rel_path or "").replace("\\", "/").lstrip("/")
        exists = os.path.isfile(source_abs)
        size = 0
        if exists:
            try:
                size = os.path.getsize(source_abs)
            except OSError:
                size = 0
        return {
            "kind": kind,
            "source_rel": rel,
            "source_abs": os.path.abspath(source_abs),
            "destination_rel": rel,
            "exists": exists,
            "size": size,
            "preview_path": os.path.abspath(source_abs) if exists and source_abs.lower().endswith((".bmp", ".png", ".jpg", ".jpeg")) else "",
        }

    @staticmethod
    def _read_cfg_settings(path):
        settings = {}
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as handle:
                for raw in handle:
                    line = raw.strip()
                    if not line or line.startswith("#") or ":" not in line:
                        continue
                    key, value = line.split(":", 1)
                    settings[key.strip().lower()] = value.strip()
        except OSError:
            return settings
        return settings

    @staticmethod
    def _device_setting_rel(value):
        text = str(value or "").strip().replace("\\", "/")
        if not text:
            return ""
        if text.startswith("/"):
            text = text.lstrip("/")
        if text.startswith(".rockbox/"):
            return text
        return ""

    @staticmethod
    def _skin_image_references(path):
        references = []
        pattern = re.compile(r"%xl\(([^)]*)\)")
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as handle:
                text = handle.read()
        except OSError:
            return references
        for match in pattern.finditer(text):
            parts = [part.strip() for part in match.group(1).split(",")]
            if len(parts) < 2:
                continue
            image_path = parts[1]
            if image_path:
                references.append(image_path)
        return references

    @staticmethod
    def _resolve_skin_image_reference(base, theme_id, image_ref):
        normalized = str(image_ref or "").replace("\\", "/").lstrip("/")
        if not normalized:
            return ""
        candidates = [
            os.path.join(base, "wps", normalized),
            os.path.join(base, "wps", theme_id, normalized),
        ]
        for candidate in candidates:
            if os.path.isfile(candidate):
                return os.path.abspath(candidate)
        return ""

    @staticmethod
    def _is_compatible_with_model(definition, target_device_model):
        compatible = [str(item or "").strip().lower() for item in definition.get("compatible_device_models", [])]
        compatible = [item for item in compatible if item]
        if not compatible:
            return True
        model = str(target_device_model or "").strip().lower()
        if not model:
            return False
        return any(item in model for item in compatible)
