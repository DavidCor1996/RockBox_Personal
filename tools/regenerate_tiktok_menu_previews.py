#!/usr/bin/env python3
"""Rebuild the bounded TikTok right-pane preview set from source videos."""

from __future__ import annotations

import argparse
import json
import shutil
import sqlite3
import sys
import tempfile
from pathlib import Path
from types import SimpleNamespace


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "rockpod"))

from services.file_safety import atomic_write_text  # noqa: E402
from services.path_safety import resolve_under_root, validate_device_root  # noqa: E402
from services.tiktok_app import (  # noqa: E402
    TIKTOK_MENU_PREVIEW_LIMIT,
    TIKTOK_MENU_PREVIEW_VERSION,
    TIKTOK_PREVIEW_ROOT,
    TikTokAppService,
    _source_signature,
    _thumbnail_signature,
)


ORDER = (
    "CASE source_type WHEN 'manual' THEN 0 ELSE 1 END, "
    "CASE WHEN source_type='archive' THEN pin_order ELSE 0 END DESC, "
    "upload_date DESC, date_added DESC"
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("mount")
    parser.add_argument("--database", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--limit", type=int, default=TIKTOK_MENU_PREVIEW_LIMIT)
    args = parser.parse_args()

    mount = validate_device_root(args.mount)
    config = json.loads(Path(args.config).read_text(encoding="utf-8"))
    database = Path(args.database).resolve()
    connection = sqlite3.connect(
        f"file:{database}?mode=ro", uri=True, timeout=5.0
    )
    connection.row_factory = sqlite3.Row
    rows = [
        dict(row)
        for row in connection.execute(
            f"SELECT * FROM tiktok_videos ORDER BY {ORDER} LIMIT ?",
            (max(1, min(args.limit, TIKTOK_MENU_PREVIEW_LIMIT)),),
        )
    ]
    connection.close()

    preview_root = Path(resolve_under_root(mount, TIKTOK_PREVIEW_ROOT))
    preview_root.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix="tiktok-pane-"))
    renderer = SimpleNamespace(config=config)
    try:
        for index, row in enumerate(rows, 1):
            target = preview_root / f"{row['id']}.bmp"
            TikTokAppService._render_menu_preview(
                renderer, row, target, staging
            )
            signature = (
                f"{TIKTOK_MENU_PREVIEW_VERSION}:"
                f"{_source_signature(row['source_path'])}:"
                f"{_thumbnail_signature(row)}"
            )
            atomic_write_text(f"{target}.source", signature)
            if index % 8 == 0 or index == len(rows):
                print(f"TikTok pane previews: {index}/{len(rows)}", flush=True)
    finally:
        shutil.rmtree(staging, ignore_errors=True)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
