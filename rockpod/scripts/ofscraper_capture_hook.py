#!/usr/bin/env python3
"""Persist OF-Scraper's creator-specific result for RockPod ingestion."""

import json
import os
import sys
from pathlib import Path


def main():
    target = os.environ.get("ROCKPOD_OFSCRAPER_MANIFEST", "")
    expected = os.environ.get("ROCKPOD_OFSCRAPER_USERNAME", "").lower()
    if not target or not expected:
        return 2
    payload = json.load(sys.stdin)
    username = str(payload.get("username") or "").lower()
    if username != expected or payload.get("action") != "download":
        return 3
    path = Path(target)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(
        json.dumps(payload, ensure_ascii=False), encoding="utf-8"
    )
    os.replace(temporary, path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
