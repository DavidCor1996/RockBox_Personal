"""Rockbox-specific device helpers."""

from __future__ import annotations

import glob
import os
from datetime import datetime


ROCKBOX_DATABASE_GLOBS = [
    ".rockbox/database*.tcd",
    ".rockbox/tagcache*.tcd",
    ".rockbox/database/*.tcd",
]

RUNTIME_EXPORT_GLOBS = [
    ".rockbox/database_changelog.txt",
    ".rockbox/database_changelog.json",
    ".rockbox/database_changelog.csv",
]

AUTOUPDATE_KEYS = ("tagcache_autoupdate", "autoupdate")


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
    with open(config_path, "w", encoding="utf-8") as handle:
        handle.writelines(lines)
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
