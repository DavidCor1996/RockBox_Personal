"""Application configuration and settings management."""

import json
import os
from pathlib import Path


def _normalize_media_dir_list(value):
    if isinstance(value, (list, tuple)):
        raw_items = value
    else:
        text = str(value or "")
        raw_items = text.replace(";", "\n").splitlines()
    normalized = []
    seen = set()
    for item in raw_items:
        path = os.path.abspath(os.path.expanduser(str(item or "").strip()))
        if not path or path in seen:
            continue
        seen.add(path)
        normalized.append(path)
    return normalized


def _normalize_games_target_dir(value, default_value="gameboy"):
    text = str(value or "").strip()
    legacy_values = {
        "",
        "Gameboy",
        "GameBoy",
        "gameboy",
        ".rockbox/rockpod/games/rockboy",
        ".rockbox/rocks/games/roms",
    }
    if text in legacy_values:
        return default_value
    return text


DEFAULT_MUSIC_DIR = str(Path.home() / "Music")
DEFAULT_VIDEO_DIR = str(Path.home() / "Videos")
DEFAULT_DB_PATH = str(Path.home() / ".rockpod" / "library.db")
DEFAULT_CACHE_DIR = str(Path.home() / ".rockpod" / "cache")
DEFAULT_ARTWORK_CACHE = str(Path.home() / ".rockpod" / "cache" / "artwork")
DEFAULT_GAMES_LIBRARY_DIR = str(Path.home() / "Documents" / "Gameboy")
DEFAULT_PHOTOS_LIBRARY_DIR = str(Path.home() / "Pictures")

SUPPORTED_FORMATS = {
    ".mp3", ".flac", ".ogg", ".m4a", ".aac", ".alac",
    ".aiff", ".aif", ".wav", ".wma", ".ape", ".wv", ".opus",
}
SUPPORTED_VIDEO_FORMATS = {
    ".mp4", ".m4v", ".mov", ".mkv", ".avi", ".webm", ".mpg", ".mpeg", ".mpe",
}

ARTWORK_FILENAMES = [
    "cover.jpg", "cover.png", "folder.jpg", "folder.png",
    "front.jpg", "front.png", "album.jpg", "album.png",
    "albumart.jpg", "albumart.png", "artwork.jpg", "artwork.png",
    "Cover.jpg", "Cover.png", "Folder.jpg", "Folder.png",
]

ARTWORK_THUMB_SIZE = (56, 56)
ARTWORK_DISPLAY_SIZE = (300, 300)

DEVICE_MUSIC_PATH_TEMPLATE = "Music/{album_artist}/{album}"
DEVICE_FILE_TEMPLATE = "{track_number:02d} - {title}{ext}"
DEVICE_OVERRIDE_KEYS = frozenset(
    {
        "display_name",
        "auto_sync_on_connect",
        "auto_rebuild_rockbox_database_after_sync",
        "rockbox_database_update_mode",
        "resync_metadata_changes",
        "force_full_resync",
        "duplicate_strictness",
        "duration_match_tolerance_seconds",
        "copy_artwork_to_device",
        "export_album_list_thumbnails",
        "sync_playlists_to_device",
        "device_music_template",
        "device_file_template",
        "verify_device_in_background",
        "convert_audio_for_device",
        "audio_conversion_mode",
        "audio_conversion_codec",
        "audio_conversion_bitrate_kbps",
        "video_sync_profile",
        "rockbox_ui_engine",
        "rockbox_ui_accent",
        "rockbox_ui_density",
        "rockbox_ui_font_scale",
        "rockbox_ui_surface",
        "rockbox_ui_hold_effect",
        "rockbox_show_applications_menu",
        "weather_enabled",
        "weather_location_name",
        "weather_latitude",
        "weather_longitude",
        "weather_units",
    }
)


class Config:
    """Application configuration backed by a JSON file."""

    _defaults = {
        "music_dir": DEFAULT_MUSIC_DIR,
        "video_dir": DEFAULT_VIDEO_DIR,
        "video_dirs": [DEFAULT_VIDEO_DIR],
        "db_path": DEFAULT_DB_PATH,
        "cache_dir": DEFAULT_CACHE_DIR,
        "artwork_cache_dir": DEFAULT_ARTWORK_CACHE,
        "artwork_cache_max_mb": 1024,
        "artwork_cache_cleanup_interval_hours": 24,
        "device_mount_path": "",
        "device_music_template": DEVICE_MUSIC_PATH_TEMPLATE,
        "device_file_template": DEVICE_FILE_TEMPLATE,
        "auto_sync_on_connect": False,
        "auto_rebuild_rockbox_database_after_sync": False,
        "rockbox_database_update_mode": "active",
        "sync_mode": "missing_only",
        "max_auto_duplicate_deletes_per_sync": 50,
        "resync_metadata_changes": True,
        "force_full_resync": False,
        "duplicate_strictness": "metadata_and_hash",
        "duration_match_tolerance_seconds": 2.0,
        "copy_artwork_to_device": True,
        "export_album_list_thumbnails": True,
        "sync_playlists_to_device": True,
        "convert_audio_for_device": False,
        "audio_conversion_mode": "unsupported_or_lossless",
        "audio_conversion_codec": "mp3",
        "audio_conversion_bitrate_kbps": 160,
        "video_sync_profile": "quality",
        "rockbox_ui_engine": "rockbox",
        "rockbox_ui_accent": "blue",
        "rockbox_ui_density": "comfortable",
        "rockbox_ui_font_scale": "normal",
        "rockbox_ui_surface": "solid",
        "rockbox_ui_hold_effect": "lockscreen",
        "rockbox_show_applications_menu": False,
        "weather_enabled": True,
        "weather_location_name": "Moncton, NB",
        "weather_latitude": 46.0878,
        "weather_longitude": -64.7782,
        "weather_units": "metric",
        "weather_cache_max_age_minutes": 60,
        "weather_sync_stale_cache": True,
        "enable_online_artwork_lookup": False,
        "prefer_local_artwork": True,
        "fetch_hires_online_artwork": True,
        "export_device_cover_jpg": True,
        "export_wps_sized_covers": True,
        "wps_cover_fit_mode": "contain",
        "max_wps_cover_sizes_per_device": 2,
        "online_artwork_storefront": "us",
        "online_artwork_retry_hours": 24,
        "online_artwork_min_interval_seconds": 60.0,
        "online_artwork_failure_retry_seconds": 3600,
        "online_artwork_max_queue_size": 50,
        "background_artwork_lookup_enabled": True,
        "browser_home_url": "https://www.rockbox.org/",
        "games_browser_home_url": "https://www.rockbox.org/",
        "store_auto_accept_cookies": True,
        "streamrip_binary": str(Path(__file__).resolve().parents[1] / ".venv" / "bin" / "rip"),
        "streamrip_preferred_format": "flac",
        "streamrip_quality": 4,
        "music_share_relay_url": "",
        "music_share_pair_code": "",
        "music_share_display_name": "",
        "music_share_sender_id": "",
        "youtube_movie_binary": "yt-dlp",
        "ffmpeg_binary": "ffmpeg",
        "games_library_path": DEFAULT_GAMES_LIBRARY_DIR,
        "games_device_target_dir": "gameboy",
        "games_simulator_target_dir": "gameboy",
        "games_show_builtin_doom": True,
        "games_show_builtin_stickrpg": True,
        "games_show_builtin_runescape": True,
        "photos_library_path": DEFAULT_PHOTOS_LIBRARY_DIR,
        "photos_device_target_dir": "Photos",
        "photos_simulator_target_dir": "Photos",
        "android_source_path": "",
        "android_import_device_dir": "Videos/Android Phone",
        "android_import_include_photos": True,
        "android_import_include_videos": True,
        "android_photo_duration_seconds": 8.0,
        "android_import_for_tiktok_plugin": False,
        "device_cover_art_size": 320,
        "device_cover_art_quality": 85,
        "device_poll_interval_connected_ms": 5000,
        "device_poll_interval_disconnected_ms": 1500,
        "device_detection_debounce_polls": 2,
        "device_space_refresh_seconds": 15.0,
        "verify_device_in_background": False,
        "artwork_thumb_size": list(ARTWORK_THUMB_SIZE),
        "scan_on_startup": True,
        "mock_device_enabled": False,
        "mock_device_path": "",
        "theme_mode": "default",
        "use_imported_theme_assets": False,
        "theme_asset_pack_path": "",
        "device_overrides": {},
        "window_geometry": None,
        "sidebar_width": 170,
        "column_widths": {},
        "sort_column": "artist",
        "sort_order": "ascending",
        "show_column_browser": False,
        "hidden_wallpapers_password_hash": "",
        "hidden_wallpapers_password_salt": "",
        "last_view": "music",
    }

    def __init__(self, config_path=None):
        if config_path is None:
            config_path = str(Path.home() / ".rockpod" / "config.json")
        self._path = config_path
        self._data = dict(self._defaults)
        self._load()

    def _load(self):
        if os.path.exists(self._path):
            try:
                with open(self._path, "r") as f:
                    stored = json.load(f)
                self._data.update(stored)
                self._normalize_video_settings(stored)
                self._data["games_device_target_dir"] = _normalize_games_target_dir(
                    self._data.get("games_device_target_dir"),
                    self._defaults["games_device_target_dir"],
                )
                self._data["games_simulator_target_dir"] = _normalize_games_target_dir(
                    self._data.get("games_simulator_target_dir"),
                    self._defaults["games_simulator_target_dir"],
                )
                overrides = self._data.get("device_overrides", {})
                self._data["device_overrides"] = overrides if isinstance(overrides, dict) else {}
                if "theme_mode" not in stored and stored.get("use_imported_theme_assets"):
                    self._data["theme_mode"] = "personal"
            except (json.JSONDecodeError, OSError):
                pass
        else:
            self._normalize_video_settings()

    def _normalize_video_settings(self, stored=None):
        if isinstance(stored, dict) and "video_dirs" not in stored and "video_dir" in stored:
            video_dirs = stored.get("video_dir")
        else:
            video_dirs = self._data.get("video_dirs")
        if video_dirs in (None, "", []):
            video_dirs = self._data.get("video_dir", self._defaults["video_dir"])
        normalized = _normalize_media_dir_list(video_dirs)
        if not normalized:
            fallback = str(self._data.get("video_dir", self._defaults["video_dir"]) or "").strip()
            normalized = _normalize_media_dir_list([fallback]) if fallback else []
        if not normalized and self._defaults.get("video_dir"):
            normalized = _normalize_media_dir_list([self._defaults["video_dir"]])
        self._data["video_dirs"] = normalized
        self._data["video_dir"] = normalized[0] if normalized else ""

    def save(self):
        os.makedirs(os.path.dirname(self._path), exist_ok=True)
        with open(self._path, "w") as f:
            json.dump(self._data, f, indent=2)

    def get(self, key, default=None):
        return self._data.get(key, default if default is not None else self._defaults.get(key))

    def set(self, key, value):
        if key == "video_dirs":
            self._data["video_dirs"] = _normalize_media_dir_list(value)
            self._data["video_dir"] = self._data["video_dirs"][0] if self._data["video_dirs"] else ""
            return
        if key == "video_dir":
            normalized = _normalize_media_dir_list([value] if value is not None else [])
            self._data["video_dirs"] = normalized
            self._data["video_dir"] = normalized[0] if normalized else ""
            return
        self._data[key] = value

    @staticmethod
    def _device_key(device=None, stable_device_key=None):
        key = stable_device_key
        if not key and device is not None:
            key = getattr(device, "stable_device_key", "") or ""
        return str(key or "").strip()

    def get_device_overrides(self, device=None, stable_device_key=None):
        key = self._device_key(device, stable_device_key)
        if not key:
            return {}
        overrides = self._data.setdefault("device_overrides", {})
        item = overrides.get(key, {})
        return dict(item) if isinstance(item, dict) else {}

    def get_effective(self, key, device=None, stable_device_key=None, default=None):
        overrides = self.get_device_overrides(device=device, stable_device_key=stable_device_key)
        if key in overrides:
            return overrides[key]
        return self.get(key, default)

    def set_device_override(self, key, value, device=None, stable_device_key=None):
        if key not in DEVICE_OVERRIDE_KEYS:
            raise KeyError(f"Unsupported device override key: {key}")
        device_key = self._device_key(device, stable_device_key)
        if not device_key:
            raise ValueError("Device override requires a stable device key")
        overrides = self._data.setdefault("device_overrides", {})
        current = overrides.get(device_key, {})
        if not isinstance(current, dict):
            current = {}
        current[key] = value
        overrides[device_key] = current

    def remove_device_override(self, key, device=None, stable_device_key=None):
        device_key = self._device_key(device, stable_device_key)
        if not device_key:
            return
        overrides = self._data.setdefault("device_overrides", {})
        current = overrides.get(device_key)
        if not isinstance(current, dict):
            return
        current.pop(key, None)
        if current:
            overrides[device_key] = current
        else:
            overrides.pop(device_key, None)

    def clear_device_overrides(self, device=None, stable_device_key=None):
        device_key = self._device_key(device, stable_device_key)
        if not device_key:
            return
        overrides = self._data.setdefault("device_overrides", {})
        overrides.pop(device_key, None)

    def get_device_display_name(self, device=None, stable_device_key=None):
        value = self.get_effective("display_name", device=device, stable_device_key=stable_device_key, default="")
        return str(value or "").strip()

    def __getattr__(self, name):
        if name.startswith("_"):
            return object.__getattribute__(self, name)
        if name in self._data:
            return self._data[name]
        raise AttributeError(f"No config key: {name}")

    def __setattr__(self, name, value):
        if name.startswith("_"):
            object.__setattr__(self, name, value)
        elif name == "video_dirs":
            self.set(name, value)
        elif name == "video_dir":
            self.set(name, value)
        else:
            self._data[name] = value

    def ensure_dirs(self):
        """Create all required cache/data directories."""
        for key in ("cache_dir", "artwork_cache_dir"):
            d = self._data.get(key)
            if d:
                os.makedirs(d, exist_ok=True)
        db_dir = os.path.dirname(self._data.get("db_path", ""))
        if db_dir:
            os.makedirs(db_dir, exist_ok=True)
