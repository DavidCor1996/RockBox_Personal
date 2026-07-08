#!/usr/bin/env python3
"""Browse YouTube videos for the RockPod movie store."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from urllib.parse import urlparse
from urllib.request import Request, urlopen


def _video_url(item: dict) -> str:
    url = str(item.get("webpage_url") or item.get("url") or "").strip()
    if url.startswith("http"):
        return url
    video_id = str(item.get("id") or url or "").strip()
    if video_id:
        return f"https://www.youtube.com/watch?v={video_id}"
    return ""


def _thumbnail_url(item: dict) -> str:
    url = str(item.get("thumbnail") or "").strip()
    if url.startswith("http"):
        return url

    thumbnails = item.get("thumbnails") or []
    if isinstance(thumbnails, list):
        for thumb in reversed(thumbnails):
            if not isinstance(thumb, dict):
                continue
            url = str(thumb.get("url") or "").strip()
            if url.startswith("http"):
                return url

    video_id = str(item.get("id") or "").strip()
    if video_id:
        return f"https://i.ytimg.com/vi/{video_id}/hqdefault.jpg"
    return ""


def _download_thumbnail(url: str, thumbnail_dir: str, key: str = "") -> str:
    source = str(url or "").strip()
    if not source.startswith("http") or not thumbnail_dir:
        return ""

    parsed = urlparse(source)
    ext = Path(parsed.path).suffix.lower()
    if ext not in {".jpg", ".jpeg", ".png", ".webp"}:
        ext = ".jpg"
    digest = hashlib.sha1((key or source).encode("utf-8", errors="ignore")).hexdigest()
    target = Path(thumbnail_dir) / f"{digest}{ext}"
    if target.is_file():
        return str(target)

    target.parent.mkdir(parents=True, exist_ok=True)
    request = Request(source, headers={"User-Agent": "RockPod/0.1"})
    try:
        with urlopen(request, timeout=15) as response:
            data = response.read()
    except OSError:
        return ""
    if not data:
        return ""
    try:
        target.write_bytes(data)
    except OSError:
        return ""
    return str(target)


def _duration_text(value) -> str:
    try:
        seconds = int(float(value))
    except (TypeError, ValueError):
        return ""
    if seconds <= 0:
        return ""
    hours = seconds // 3600
    minutes = (seconds % 3600) // 60
    remaining = seconds % 60
    if hours:
        return f"{hours}:{minutes:02d}:{remaining:02d}"
    return f"{minutes}:{remaining:02d}"


def browse(query: str, limit: int, yt_dlp: str, thumbnail_dir: str = "") -> list[dict]:
    search = f"ytsearch{limit}:{query}"
    command = [
        yt_dlp,
        "--dump-json",
        "--flat-playlist",
        "--no-warnings",
        "--playlist-end",
        str(limit),
        search,
    ]
    process = subprocess.run(command, text=True, capture_output=True, check=True)
    results = []
    for line in process.stdout.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            item = json.loads(line)
        except json.JSONDecodeError:
            continue
        url = _video_url(item)
        title = str(item.get("title") or "YouTube Movie").strip()
        if not url or not title:
            continue
        thumbnail = _thumbnail_url(item)
        results.append(
            {
                "title": title,
                "uploader": str(item.get("uploader") or item.get("channel") or "").strip(),
                "duration": int(item.get("duration") or 0) if str(item.get("duration") or "").isdigit() else 0,
                "duration_text": _duration_text(item.get("duration")),
                "url": url,
                "id": str(item.get("id") or "").strip(),
                "thumbnail": thumbnail,
                "thumbnail_path": _download_thumbnail(thumbnail, thumbnail_dir, str(item.get("id") or url)),
            }
        )
        if len(results) >= limit:
            break
    return results


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--query", required=True)
    parser.add_argument("--limit", type=int, default=18)
    parser.add_argument("--yt-dlp", default="yt-dlp")
    parser.add_argument("--output", required=True)
    parser.add_argument("--thumbnail-dir", default="")
    args = parser.parse_args()

    results = browse(args.query, max(1, min(40, args.limit)), args.yt_dlp, args.thumbnail_dir)
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(results, indent=2))
    print(f"Wrote {len(results)} movie results to {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
