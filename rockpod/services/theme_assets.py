"""Manifest-driven local theme asset support."""

import json
import logging
from copy import deepcopy
from pathlib import Path


logger = logging.getLogger(__name__)

PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_THEME_DIR = PROJECT_ROOT / "assets" / "theme_default"
PERSONAL_THEME_DIR = PROJECT_ROOT / "assets" / "theme_itunes_personal"
THEME_MANIFESTS = ("theme.json", "manifest.json")

DEFAULT_ASSET_MAP = {
    "branding_app_icon": "branding/app_icon.png",
    "branding_title": "branding/title.png",
    "toolbar_sync": "toolbar/sync.png",
    "toolbar_refresh": "toolbar/refresh.png",
    "toolbar_new_playlist": "toolbar/new_playlist.png",
    "playback_previous": "playback/previous.png",
    "playback_play": "playback/play.png",
    "playback_pause": "playback/pause.png",
    "playback_next": "playback/next.png",
    "sidebar_music": "sidebar/music.png",
    "sidebar_artists": "sidebar/artists.png",
    "sidebar_albums": "sidebar/albums.png",
    "sidebar_genres": "sidebar/genres.png",
    "sidebar_playlist": "sidebar/playlist.png",
    "sidebar_device": "sidebar/device.png",
    "chrome_texture": "chrome/toolbar_texture.png",
    "search_field_left": "toolbar/search_left.png",
    "search_field_middle": "toolbar/search_middle.png",
    "search_field_right": "toolbar/search_right.png",
    "table_header_texture": "table/header.png",
    "table_selection_texture": "table/selection.png",
    "sidebar_selection_texture": "sidebar/selection.png",
    "statusbar_texture": "statusbar/background.png",
    "statusbar_divider": "statusbar/divider.png",
    "storagebar_frame": "statusbar/storage_frame.png",
    "device_summary_header": "device/summary_header.png",
    "device_summary_sidebar_icon": "device/ipod_icon.png",
    "album_placeholder": "artwork/album_placeholder.png",
    "album_frame": "artwork/album_frame.png",
}


class ThemeAssetManager:
    """Resolve default and personal themes with per-asset fallback."""

    def __init__(self, config):
        self._config = config
        self._resolution_log = set()
        self._ensure_dirs()
        self._default = self._load_theme("default", DEFAULT_THEME_DIR)
        self._personal = self._load_theme("personal", PERSONAL_THEME_DIR)

    def _ensure_dirs(self):
        DEFAULT_THEME_DIR.mkdir(parents=True, exist_ok=True)
        PERSONAL_THEME_DIR.mkdir(parents=True, exist_ok=True)

    @property
    def default_pack_dir(self):
        return str(DEFAULT_THEME_DIR)

    @property
    def personal_pack_dir(self):
        return str(PERSONAL_THEME_DIR)

    @property
    def requested_theme(self):
        theme = (self._config.get("theme_mode", "default") or "default").strip().lower()
        return theme if theme in {"default", "personal"} else "default"

    @property
    def personal_is_valid(self):
        return bool(self._personal["valid"])

    @property
    def active_theme_id(self):
        if self.requested_theme == "personal" and self.personal_is_valid:
            return "personal"
        return "default"

    @property
    def active_theme(self):
        return self._personal if self.active_theme_id == "personal" else self._default

    def active_pack_dir(self):
        return self.active_theme["dir"]

    def theme_status(self):
        return {
            "requested": self.requested_theme,
            "active": self.active_theme_id,
            "default_valid": self._default["valid"],
            "personal_valid": self.personal_is_valid,
            "personal_missing": list(self._personal["missing"]),
            "personal_optional_present": sorted(self._personal["resolved_assets"].keys()),
            "personal_path": str(PERSONAL_THEME_DIR),
        }

    def validate_pack(self, folder):
        return self._load_theme("external", Path(folder), emit_logs=False)

    def manifest_data(self, theme_id=None):
        if theme_id == "personal":
            return dict(self._personal["manifest"])
        if theme_id == "default":
            return dict(self._default["manifest"])
        return dict(self.active_theme["manifest"])

    def asset_path(self, asset_name):
        rel = self._asset_relpath(asset_name)
        fallback_name = None
        if asset_name == "branding_app_icon":
            fallback_name = "branding_title"
        personal_candidate = self._personal["resolved_assets"].get(asset_name)
        if self.requested_theme == "personal" and personal_candidate:
            self._log_resolution(asset_name, "personal", personal_candidate)
            return personal_candidate

        default_candidate = self._default["resolved_assets"].get(asset_name)
        if default_candidate:
            source = "default"
            if self.requested_theme == "personal":
                source = "fallback-default"
            self._log_resolution(asset_name, source, default_candidate)
            return default_candidate

        if fallback_name:
            fallback_path = self.asset_path(fallback_name)
            if fallback_path:
                self._log_resolution(asset_name, f"fallback-{fallback_name}", fallback_path)
                return fallback_path

        self._log_resolution(asset_name, "missing", rel)
        return ""

    def color(self, name, default=None):
        manifest = self.active_theme["manifest"]
        colors = manifest.get("colors", {})
        if name in colors:
            return colors[name]
        return self._default["manifest"].get("colors", {}).get(name, default)

    def metric(self, name, default=None):
        manifest = self.active_theme["manifest"]
        metrics = manifest.get("metrics", {})
        if name in metrics:
            return metrics[name]
        return self._default["manifest"].get("metrics", {}).get(name, default)

    def reset_to_default(self):
        self._config.theme_mode = "default"

    def known_assets(self):
        return dict(DEFAULT_ASSET_MAP)

    def asset_report(self):
        report = []
        for asset_name, rel in DEFAULT_ASSET_MAP.items():
            personal_path = self._personal["resolved_assets"].get(asset_name, "")
            default_path = self._default["resolved_assets"].get(asset_name, "")
            active_path = self.asset_path(asset_name)
            if self.requested_theme == "personal" and personal_path:
                source = "personal"
            elif default_path:
                source = "default"
            else:
                source = "missing"
            report.append(
                {
                    "asset": asset_name,
                    "relative_path": self.active_theme["manifest"].get("assets", {}).get(asset_name, rel),
                    "personal_path": personal_path,
                    "default_path": default_path,
                    "active_path": active_path,
                    "source": source,
                }
            )
        return report

    def validate_active_theme(self):
        assets = self.asset_report()
        found = [item for item in assets if item["source"] != "missing"]
        personal = [item for item in assets if item["source"] == "personal"]
        missing = [item for item in assets if item["source"] == "missing"]
        return {
            "requested": self.requested_theme,
            "active": self.active_theme_id,
            "theme_name": self.active_theme["manifest"].get("name", self.active_theme_id),
            "manifest_path": self.active_theme["manifest_path"],
            "asset_count": len(assets),
            "found_count": len(found),
            "personal_count": len(personal),
            "missing_count": len(missing),
            "completeness_percent": round((len(found) / len(assets)) * 100, 1) if assets else 0.0,
            "assets": assets,
            "missing_assets": [item["asset"] for item in missing],
        }

    def update_personal_manifest(self, asset_overrides=None, metadata=None):
        self._ensure_dirs()
        manifest = deepcopy(self._personal["manifest"]) if self._personal["valid"] else (
            deepcopy(self._default["manifest"]) if self._default["manifest"] else {
            "name": "Personal iTunes 2007",
            "assets": dict(DEFAULT_ASSET_MAP),
            "colors": {},
            "metrics": {},
            "fallback_behavior": "per_asset",
        })
        manifest["name"] = "Personal iTunes 2007"
        manifest["description"] = "Local-only personal theme using user-supplied iTunes-era assets."
        manifest.setdefault("assets", dict(DEFAULT_ASSET_MAP))
        manifest.setdefault("colors", {})
        manifest.setdefault("metrics", {})
        manifest["fallback_behavior"] = "per_asset"
        if asset_overrides:
            manifest["assets"].update(asset_overrides)
        if metadata:
            manifest["import_metadata"] = metadata

        manifest_path = PERSONAL_THEME_DIR / "theme.json"
        with open(manifest_path, "w", encoding="utf-8") as handle:
            json.dump(manifest, handle, indent=2, sort_keys=True)
            handle.write("\n")
        self._personal = self._load_theme("personal", PERSONAL_THEME_DIR)
        return str(manifest_path)

    def _asset_relpath(self, asset_name):
        return DEFAULT_ASSET_MAP.get(asset_name, asset_name)

    def _load_theme(self, theme_id, folder, emit_logs=True):
        path = Path(folder)
        manifest_path = self._find_manifest(path)
        manifest, manifest_valid = self._read_manifest(manifest_path) if manifest_path else ({}, False)
        assets = dict(DEFAULT_ASSET_MAP)
        assets.update(manifest.get("assets", {}))

        missing = []
        if manifest_path is None:
            missing.append("theme.json")
        elif not manifest_valid:
            missing.append("invalid theme.json")

        resolved_assets = {}
        for asset_name, rel in assets.items():
            candidate = path / rel
            if candidate.is_file():
                resolved_assets[asset_name] = str(candidate)

        data = {
            "id": theme_id,
            "dir": str(path),
            "manifest_path": str(manifest_path) if manifest_path else "",
            "manifest": manifest,
            "resolved_assets": resolved_assets,
            "missing": missing,
            "valid": manifest_path is not None and manifest_valid,
        }
        if emit_logs:
            if data["valid"]:
                logger.info(
                    "Loaded %s theme manifest: %s (%d assets present)",
                    theme_id,
                    data["manifest_path"],
                    len(resolved_assets),
                )
            else:
                logger.info(
                    "Theme %s unavailable: %s in %s",
                    theme_id,
                    ", ".join(missing) or "missing theme.json",
                    path,
                )
        return data

    def _find_manifest(self, folder):
        if not folder.is_dir():
            return None
        for name in THEME_MANIFESTS:
            candidate = folder / name
            if candidate.is_file():
                return candidate
        return None

    def _read_manifest(self, path):
        try:
            with open(path, "r", encoding="utf-8") as handle:
                payload = json.load(handle)
        except (OSError, json.JSONDecodeError):
            logger.warning("Theme manifest is invalid: %s", path)
            return {}, False

        if not isinstance(payload, dict):
            logger.warning("Theme manifest must be an object: %s", path)
            return {}, False
        payload.setdefault("name", path.parent.name)
        payload.setdefault("assets", {})
        payload.setdefault("colors", {})
        payload.setdefault("metrics", {})
        payload.setdefault("fallback_behavior", "per_asset")
        return payload, True

    def _log_resolution(self, asset_name, source, path):
        key = (asset_name, source, path)
        if key in self._resolution_log:
            return
        self._resolution_log.add(key)
        if source == "missing":
            logger.info("Theme asset missing: %s", asset_name)
        else:
            logger.info("Theme asset %s -> %s (%s)", asset_name, source, path)
