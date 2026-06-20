"""Path safety helpers for RockPod device and simulator roots."""

from __future__ import annotations

import os


def normalize_path(path):
    """Return an absolute normalized path for user/profile supplied paths."""
    return os.path.abspath(os.path.normpath(str(path or "")))


def is_within_root(root, path):
    """Return whether path is root or a descendant of root."""
    root_abs = normalize_path(root)
    path_abs = normalize_path(path)
    try:
        return os.path.commonpath([root_abs, path_abs]) == root_abs
    except ValueError:
        return False


def resolve_under_root(root, rel_path):
    """Resolve a relative device path and reject root escapes."""
    root_abs = normalize_path(root)
    rel = str(rel_path or "").replace("\\", "/").lstrip("/")
    if not rel or rel in {".", ".."}:
        raise ValueError("Destination path is empty or unsafe")
    candidate = normalize_path(os.path.join(root_abs, rel))
    if not is_within_root(root_abs, candidate):
        raise ValueError(f"Destination escapes device root: {rel_path}")
    return candidate


def validate_device_root(path):
    """Reject obviously dangerous write roots while allowing mock/sim disks."""
    root = normalize_path(path)
    if not root or not os.path.isdir(root):
        raise ValueError("Profile mount path does not exist")

    dangerous = {
        os.path.abspath(os.sep),
        normalize_path(os.path.expanduser("~")),
        normalize_path("/home"),
        normalize_path("/media"),
        normalize_path("/mnt"),
        normalize_path("/run/media"),
        normalize_path("/tmp"),
    }
    if root in dangerous:
        raise ValueError(f"Refusing to use unsafe device root: {root}")
    return root
