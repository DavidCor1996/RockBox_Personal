"""Read the contents actually advertised by one mounted iPod."""

from __future__ import annotations

import csv
from pathlib import Path

from services.path_safety import resolve_under_root


def present_manifest_rows(mount_path, manifest_path, media_columns):
    """Return rows whose manifest still points at a file on this iPod."""
    path = Path(resolve_under_root(mount_path, manifest_path))
    try:
        with path.open("r", encoding="utf-8", newline="") as stream:
            rows = list(csv.DictReader(stream, delimiter="\t"))
    except (OSError, csv.Error):
        return {}
    present = {}
    for row in rows:
        item_id = str(row.get("id") or "").strip()
        for column in media_columns:
            relative = str(row.get(column) or "").lstrip("/")
            if not item_id or not relative:
                continue
            try:
                if Path(resolve_under_root(mount_path, relative)).is_file():
                    present[item_id] = row
                    break
            except ValueError:
                continue
    return present


def present_manifest_ids(mount_path, manifest_path, media_columns):
    return set(present_manifest_rows(mount_path, manifest_path, media_columns))
