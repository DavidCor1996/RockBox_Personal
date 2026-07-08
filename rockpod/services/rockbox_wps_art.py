"""Helpers for Rockbox WPS album-art sizing."""

from __future__ import annotations

import os
import re


_CL_RE = re.compile(r"%Cl\(([^)]*)\)")
_FALLBACK_SIZES = {
    ("320x240", "ipone"): [(138, 138)],
    ("320x240", "springpod3"): [(138, 138)],
    ("176x132", "ipone_nano2g"): [(54, 54)],
}


def parse_wps_album_art_sizes(text):
    """Return unique `(width, height)` pairs requested by `%Cl(...)` tags."""
    sizes = []
    seen = set()
    for match in _CL_RE.finditer(str(text or "")):
        parts = [part.strip() for part in match.group(1).split(",")]
        if len(parts) < 4:
            continue
        try:
            width = int(parts[2])
            height = int(parts[3])
        except ValueError:
            continue
        if width <= 0 or height <= 0:
            continue
        size = (width, height)
        if size in seen:
            continue
        seen.add(size)
        sizes.append(size)
    return sizes


def wps_album_art_sizes_for_profile(profile, max_sizes=2):
    profile = dict(profile or {})
    repo_root = os.path.abspath(profile.get("source_repo_path") or "")
    theme = str(profile.get("selected_theme") or "").strip()
    sizes = []
    if repo_root and theme:
        sizes = _sizes_from_skin_paths(
            [
                os.path.join(repo_root, "wps", f"{theme}.wps"),
                os.path.join(repo_root, "wps", f"{theme}.sbs"),
            ]
        )
    if not sizes:
        sizes = list(_FALLBACK_SIZES.get((str(profile.get("screen_resolution") or ""), theme.casefold()), []))
    try:
        limit = max(1, int(max_sizes or 2))
    except (TypeError, ValueError):
        limit = 2
    return sizes[:limit]


def wps_album_art_sizes_for_device(device_mount_path, max_sizes=2):
    mount_root = os.path.abspath(str(device_mount_path or "").strip()) if device_mount_path else ""
    if not mount_root or not os.path.isdir(mount_root):
        return []
    cfg_path = os.path.join(mount_root, ".rockbox", "config.cfg")
    skin_paths = []
    try:
        with open(cfg_path, "r", encoding="utf-8", errors="replace") as handle:
            for raw_line in handle:
                key, sep, value = raw_line.partition(":")
                if not sep or key.strip().lower() not in {"wps", "sbs"}:
                    continue
                rel = value.strip()
                if not rel or rel == "-":
                    continue
                skin_paths.append(os.path.join(mount_root, rel.lstrip("/")))
    except OSError:
        return []
    try:
        limit = max(1, int(max_sizes or 2))
    except (TypeError, ValueError):
        limit = 2
    return _sizes_from_skin_paths(skin_paths)[:limit]


def wps_album_art_sizes_for_config(config, device_mount_path=""):
    getter = getattr(config, "get", None)
    if not callable(getter):
        return []
    max_sizes = getter("max_wps_cover_sizes_per_device", 2)
    device_sizes = wps_album_art_sizes_for_device(device_mount_path, max_sizes=max_sizes)
    if device_sizes:
        return device_sizes
    profiles = list(getter("rockbox_profiles", []) or [])
    selected_id = str(getter("rockbox_selected_profile_id", "") or "")
    device_mount = os.path.abspath(str(device_mount_path or "").strip()) if device_mount_path else ""
    selected = None
    for profile in profiles:
        mount = os.path.abspath(str(profile.get("device_mount_path") or "").strip()) if profile.get("device_mount_path") else ""
        if device_mount and mount == device_mount:
            selected = profile
            break
        if selected_id and profile.get("id") == selected_id:
            selected = profile
    if selected is None and profiles:
        selected = profiles[0]
    if selected is None:
        return []
    return wps_album_art_sizes_for_profile(selected, max_sizes=max_sizes)


def _sizes_from_skin_paths(paths):
    sizes = []
    seen = set()
    for path in paths:
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as handle:
                found = parse_wps_album_art_sizes(handle.read())
        except OSError:
            continue
        for size in found:
            if size in seen:
                continue
            seen.add(size)
            sizes.append(size)
    return sizes
