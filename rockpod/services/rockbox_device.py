"""Rockbox-specific device helpers."""

from __future__ import annotations

import glob
import os
from datetime import datetime

from services.file_safety import atomic_write_text


ROCKBOX_DATABASE_GLOBS = [
    ".rockbox/database*.tcd",
    ".rockbox/tagcache*.tcd",
    ".rockbox/database/*.tcd",
]

PICTUREFLOW_CACHE_GLOBS = [
    ".rockbox/rocks/demos/pictureflow/*.pfraw",
    ".rockbox/rocks/demos/pictureflow/pictureflow_album.idx",
]

PICTUREFLOW_CONFIG_REL = ".rockbox/rocks/demos/pictureflow.cfg"

RUNTIME_EXPORT_GLOBS = [
    ".rockbox/database_changelog.txt",
    ".rockbox/database_changelog.json",
    ".rockbox/database_changelog.csv",
]

AUTOUPDATE_KEYS = ("tagcache_autoupdate", "autoupdate")
ROCKBOX_UI_ENGINE_KEY = "ui engine"
ROCKBOX_UI_ACCENT_KEY = "ui engine accent"
ROCKBOX_UI_DENSITY_KEY = "ui engine density"
ROCKBOX_ROOT_MENU_KEY = "root menu order"
ROCKBOX_UI_FONT_SCALE_KEY = "ui engine font scale"
ROCKBOX_UI_SURFACE_KEY = "ui engine surface"
ROCKBOX_UI_HOLD_EFFECT_KEY = "ui engine hold effect"
ROCKBOX_UI_EXTRAS_PANE_KEY = "ui engine extras pane"
ROCKBOX_UI_DARK_MODE_KEY = "ui engine dark mode"

ROCKBOX_UI_ACCENTS = {
    "blue",
    "graphite",
    "u2",
    "teal",
    "green",
    "gold",
    "orange",
    "purple",
    "pink",
}


def _set_rockbox_config_value(mount_path, setting_key, setting_value):
    if not mount_path:
        return False
    config_path = os.path.join(mount_path, ".rockbox", "config.cfg")
    os.makedirs(os.path.dirname(config_path), exist_ok=True)
    lines = []
    if os.path.exists(config_path):
        with open(config_path, "r", encoding="utf-8", errors="replace") as handle:
            lines = handle.readlines()

    normalized_key = str(setting_key or "").strip().lower()
    updated = False
    for idx, raw in enumerate(lines):
        stripped = raw.strip()
        if not stripped or stripped.startswith("#"):
            continue
        colon_pos = stripped.find(":")
        equals_pos = stripped.find("=")
        if colon_pos == -1 and equals_pos == -1:
            continue
        split_at = (
            equals_pos
            if colon_pos == -1 or (equals_pos != -1 and equals_pos < colon_pos)
            else colon_pos
        )
        key = stripped[:split_at].strip()
        separator = stripped[split_at]
        if key.lower() != normalized_key:
            continue
        lines[idx] = f"{key}{separator} {setting_value}\n"
        updated = True
        break
    if not updated:
        if lines and not lines[-1].endswith("\n"):
            lines[-1] += "\n"
        lines.append(f"{setting_key}: {setting_value}\n")
    atomic_write_text(config_path, "".join(lines))
    return True


def set_rockbox_ui_engine(device, engine):
    mount_path = getattr(device, "mount_path", "")
    value = "ipodjs" if str(engine or "").strip().lower() == "ipodjs" else "rockbox"
    return _set_rockbox_config_value(mount_path, ROCKBOX_UI_ENGINE_KEY, value)


def set_rockbox_ui_accent(device, accent):
    mount_path = getattr(device, "mount_path", "")
    normalized = str(accent or "").strip().lower()
    value = normalized if normalized in ROCKBOX_UI_ACCENTS else "blue"
    return _set_rockbox_config_value(mount_path, ROCKBOX_UI_ACCENT_KEY, value)


def set_rockbox_ui_density(device, density):
    mount_path = getattr(device, "mount_path", "")
    normalized = str(density or "").strip().lower()
    value = "compact" if normalized == "compact" else "comfortable"
    return _set_rockbox_config_value(mount_path, ROCKBOX_UI_DENSITY_KEY, value)


def _get_rockbox_config_value(mount_path, setting_key):
    config_path = os.path.join(mount_path, ".rockbox", "config.cfg")
    if not os.path.exists(config_path):
        return ""
    normalized_key = str(setting_key or "").strip().lower()
    with open(config_path, "r", encoding="utf-8", errors="replace") as handle:
        for raw in handle:
            stripped = raw.strip()
            if not stripped or stripped.startswith("#"):
                continue
            colon_pos = stripped.find(":")
            equals_pos = stripped.find("=")
            if colon_pos == -1 and equals_pos == -1:
                continue
            split_at = (
                equals_pos
                if colon_pos == -1 or (equals_pos != -1 and equals_pos < colon_pos)
                else colon_pos
            )
            if stripped[:split_at].strip().lower() == normalized_key:
                return stripped[split_at + 1:].strip()
    return ""


def set_rockbox_applications_menu(device, enabled):
    mount_path = getattr(device, "mount_path", "")
    current = _get_rockbox_config_value(mount_path, ROCKBOX_ROOT_MENU_KEY)
    default_items = [
        "pictureflow",
        "database",
        "videos",
        "photos",
        "games",
        "files",
        "wps",
        "playlists",
        "plugins",
        "shortcuts",
        "settings",
        "system_menu",
    ]
    items = [
        item.strip()
        for item in current.replace(";", ",").split(",")
        if item.strip()
    ] or default_items
    items = [item for item in items if item != "applications"]
    if enabled:
        insert_at = 3 if len(items) >= 3 else len(items)
        items.insert(insert_at, "applications")
    return _set_rockbox_config_value(
        mount_path,
        ROCKBOX_ROOT_MENU_KEY,
        ", ".join(items) + ",",
    )


def set_rockbox_ui_font_scale(device, font_scale):
    mount_path = getattr(device, "mount_path", "")
    normalized = str(font_scale or "").strip().lower()
    value = normalized if normalized in {"small", "normal", "large"} else "normal"
    return _set_rockbox_config_value(mount_path, ROCKBOX_UI_FONT_SCALE_KEY, value)


def set_rockbox_ui_surface(device, surface):
    mount_path = getattr(device, "mount_path", "")
    normalized = str(surface or "").strip().lower()
    value = normalized if normalized in {"solid", "soft", "transparent"} else "solid"
    return _set_rockbox_config_value(mount_path, ROCKBOX_UI_SURFACE_KEY, value)


def set_rockbox_ui_hold_effect(device, hold_effect):
    mount_path = getattr(device, "mount_path", "")
    normalized = str(hold_effect or "").strip().lower()
    value = normalized if normalized in {"dim", "lockscreen"} else "lockscreen"
    return _set_rockbox_config_value(mount_path, ROCKBOX_UI_HOLD_EFFECT_KEY, value)


def set_rockbox_ui_extras_pane(device, pane):
    mount_path = getattr(device, "mount_path", "")
    normalized = str(pane or "").strip().lower()
    value = normalized if normalized in {"clock", "avatar", "sitekick"} else "clock"
    return _set_rockbox_config_value(
        mount_path, ROCKBOX_UI_EXTRAS_PANE_KEY, value
    )


def set_rockbox_ui_dark_mode(device, enabled):
    mount_path = getattr(device, "mount_path", "")
    value = "on" if bool(enabled) else "off"
    return _set_rockbox_config_value(mount_path, ROCKBOX_UI_DARK_MODE_KEY, value)


def detect_rockbox_database_state(device, device_row=None):
    device_row = dict(device_row) if device_row is not None and hasattr(device_row, "keys") else (device_row or {})
    mount_path = getattr(device, "mount_path", "") if device else device_row.get("mount_path_last_seen", "")
    state = {
        "database_present": False,
        "database_files": [],
        "database_status": "Unavailable",
        "database_needs_refresh": False,
        "database_latest_mtime": "",
        "runtime_data_available": False,
        "runtime_export_files": [],
        "tagcache_autoupdate": False,
    }
    if not mount_path or not os.path.isdir(mount_path):
        return state

    database_files = []
    for pattern in ROCKBOX_DATABASE_GLOBS:
        database_files.extend(glob.glob(os.path.join(mount_path, pattern)))
    runtime_files = [path for rel in RUNTIME_EXPORT_GLOBS for path in [os.path.join(mount_path, rel)] if os.path.isfile(path)]
    database_files = sorted({os.path.realpath(path): path for path in database_files}.values())
    state["database_present"] = bool(database_files)
    state["database_files"] = database_files
    state["runtime_data_available"] = bool(runtime_files)
    state["runtime_export_files"] = runtime_files
    state["tagcache_autoupdate"] = rockbox_tagcache_autoupdate_enabled(mount_path)

    latest_mtime = 0.0
    for path in database_files:
        try:
            latest_mtime = max(latest_mtime, os.path.getmtime(path))
        except OSError:
            continue
    if latest_mtime > 0:
        state["database_latest_mtime"] = datetime.fromtimestamp(latest_mtime).isoformat(timespec="seconds")

    last_sync = device_row.get("last_sync_at", "")
    if last_sync and latest_mtime > 0:
        try:
            sync_dt = datetime.fromisoformat(str(last_sync).replace(" ", "T"))
            state["database_needs_refresh"] = latest_mtime < sync_dt.timestamp()
        except ValueError:
            state["database_needs_refresh"] = False
    elif last_sync and not database_files:
        state["database_needs_refresh"] = True

    if state["database_needs_refresh"]:
        state["database_status"] = "Auto-update enabled" if state["tagcache_autoupdate"] else "Needs refresh"
    elif state["database_present"]:
        state["database_status"] = "Ready"
    else:
        state["database_status"] = "Not built"
    return state


def rockbox_tagcache_autoupdate_enabled(mount_path):
    config_path = os.path.join(mount_path, ".rockbox", "config.cfg")
    if not os.path.isfile(config_path):
        return False
    try:
        with open(config_path, "r", encoding="utf-8", errors="replace") as handle:
            for line in handle:
                text = line.strip()
                if not text or text.startswith("#") or ":" not in text:
                    continue
                key, value = text.split(":", 1)
                if key.strip().lower() in AUTOUPDATE_KEYS:
                    return value.strip().lower() in ("on", "true", "yes", "1")
    except OSError:
        return False
    return False


def enable_rockbox_tagcache_autoupdate(device):
    mount_path = getattr(device, "mount_path", "")
    if not mount_path:
        return False
    config_path = os.path.join(mount_path, ".rockbox", "config.cfg")
    os.makedirs(os.path.dirname(config_path), exist_ok=True)
    lines = []
    if os.path.exists(config_path):
        with open(config_path, "r", encoding="utf-8", errors="replace") as handle:
            lines = handle.readlines()

    updated = False
    for idx, raw in enumerate(lines):
        stripped = raw.strip()
        if ":" not in stripped or stripped.startswith("#"):
            continue
        key, _value = stripped.split(":", 1)
        if key.strip().lower() in AUTOUPDATE_KEYS:
            lines[idx] = "tagcache_autoupdate: on\n"
            updated = True
            break
    if not updated:
        if lines and not lines[-1].endswith("\n"):
            lines[-1] += "\n"
        lines.append("tagcache_autoupdate: on\n")
    atomic_write_text(config_path, "".join(lines))
    return True


def clear_rockbox_database_cache(device):
    mount_path = getattr(device, "mount_path", "")
    if not mount_path or not os.path.isdir(mount_path):
        return {"success": False, "removed": [], "failures": ["Device mount path is unavailable"]}

    removed = []
    failures = []
    seen = set()
    for pattern in ROCKBOX_DATABASE_GLOBS:
        for path in glob.glob(os.path.join(mount_path, pattern)):
            real = os.path.realpath(path)
            if real in seen:
                continue
            seen.add(real)
            if not os.path.isfile(path):
                continue
            try:
                os.remove(path)
                removed.append(path)
            except OSError as exc:
                failures.append(f"{path}: {exc}")

    return {
        "success": not failures,
        "removed": removed,
        "failures": failures,
    }


def invalidate_pictureflow_cache(device):
    """Remove generated PictureFlow cache files so album art is rebuilt from current tags/covers."""
    mount_path = getattr(device, "mount_path", "")
    if not mount_path or not os.path.isdir(mount_path):
        return {"success": False, "removed": [], "failures": ["Device mount path is unavailable"]}

    removed = []
    failures = []
    seen = set()
    for pattern in PICTUREFLOW_CACHE_GLOBS:
        for path in glob.glob(os.path.join(mount_path, pattern)):
            real = os.path.realpath(path)
            if real in seen:
                continue
            seen.add(real)
            if not os.path.isfile(path):
                continue
            try:
                os.remove(path)
                removed.append(path)
            except OSError as exc:
                failures.append(f"{path}: {exc}")

    config_path = os.path.join(mount_path, PICTUREFLOW_CONFIG_REL)
    if os.path.isfile(config_path):
        try:
            with open(config_path, "r", encoding="utf-8", errors="replace") as handle:
                lines = handle.readlines()
            updated = False
            for idx, raw in enumerate(lines):
                stripped = raw.strip()
                if ":" not in stripped or stripped.startswith("#"):
                    continue
                key, _value = stripped.split(":", 1)
                normalized = key.strip().lower()
                if normalized == "cache version":
                    lines[idx] = "cache version:          0\n"
                    updated = True
                elif normalized == "update albumart":
                    lines[idx] = "update albumart:          0\n"
                    updated = True
            if updated:
                atomic_write_text(config_path, "".join(lines))
        except OSError as exc:
            failures.append(f"{config_path}: {exc}")

    return {
        "success": not failures,
        "removed": removed,
        "failures": failures,
    }
