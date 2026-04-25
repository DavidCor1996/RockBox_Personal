"""Deterministic Rockbox theme stack definitions for RockPod."""

from __future__ import annotations

import os
from copy import deepcopy


THEME_DEFINITIONS = {
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
    "iPone_optimized": {
        "name": "iPone Optimized",
        "description": "Performance-tuned iPone stack for 320x240 iPods.",
        "resolutions": ["320x240"],
        "assets": [
            {"kind": "cfg", "source": "themes/iPone_optimized.cfg", "destination": ".rockbox/themes/iPone_optimized.cfg"},
            {"kind": "wps", "source": "wps/iPone_optimized.wps", "destination": ".rockbox/wps/iPone_optimized.wps"},
            {"kind": "sbs", "source": "wps/iPone_optimized.sbs", "destination": ".rockbox/wps/iPone_optimized.sbs"},
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
            {"kind": "iconset", "source": "icons/tango_icons.12x12.bmp", "destination": ".rockbox/icons/tango_icons.12x12.bmp"},
            {"kind": "font", "source": "fonts/12-Adobe-Helvetica.fnt", "destination": ".rockbox/fonts/12-Adobe-Helvetica.fnt"},
            {"kind": "wps_assets", "source": "wps/Galaxy", "destination": ".rockbox/wps/Galaxy", "recursive": True},
        ],
        "preview_candidates": [
            "wps/Galaxy/MenuBackdrop.bmp",
            "wps/Galaxy/Wallpaper.bmp",
            "wps/Galaxy/wpsbackdrop-160x128x2.bmp",
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

    def list_themes(self, source_repo_path, screen_resolution=""):
        resolution = str(screen_resolution or "").strip()
        result = []
        for theme_id, definition in THEME_DEFINITIONS.items():
            if resolution and definition["resolutions"] and resolution not in definition["resolutions"]:
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
        }

    def bundle_for_theme(self, theme_id, source_repo_path):
        return self.inspect_theme(theme_id, source_repo_path)

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
