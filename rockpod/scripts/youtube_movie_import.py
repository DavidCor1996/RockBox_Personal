#!/usr/bin/env python3
"""Download an authorized YouTube video and convert it for Rockbox."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from services.android_media import build_ffmpeg_command  # noqa: E402
from services.youtube_movies import (  # noqa: E402
    _downloaded_video,
    is_supported_youtube_url,
    persist_movie_import_poster,
    sanitize_movie_title,
)


VIDEO_EXTENSIONS = {".mp4", ".mkv", ".webm", ".mov", ".m4v"}


def _sanitize_component(value, max_len=96):
    return sanitize_movie_title(value, max_len=max_len)


def _safe_int(value, default=0):
    try:
        return int(float(value))
    except (TypeError, ValueError):
        return default


def _run_streamed(command):
    print("Running:", " ".join(command), flush=True)
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    output = []
    assert process.stdout is not None
    for line in process.stdout:
        print(line.rstrip(), flush=True)
        output.append(line)
    code = process.wait()
    if code != 0:
        raise RuntimeError(f"command exited with {code}: {' '.join(command)}")
    return "".join(output)


def _metadata_from_ytdlp(yt_dlp, url):
    command = [yt_dlp, "--no-playlist", "--dump-single-json", "--skip-download", url]
    try:
        result = subprocess.run(command, text=True, capture_output=True, check=True)
        raw = json.loads(result.stdout)
    except (OSError, subprocess.CalledProcessError, json.JSONDecodeError):
        return {}
    upload_date = str(raw.get("upload_date") or raw.get("release_date") or "")
    year = raw.get("release_year")
    if not year and len(upload_date) >= 4 and upload_date[:4].isdigit():
        year = int(upload_date[:4])
    return {
        "id": str(raw.get("id") or ""),
        "url": str(raw.get("webpage_url") or url),
        "title": str(raw.get("title") or raw.get("fulltitle") or "YouTube Movie"),
        "description": str(raw.get("description") or ""),
        "uploader": str(raw.get("uploader") or raw.get("channel") or ""),
        "channel_url": str(raw.get("channel_url") or raw.get("uploader_url") or ""),
        "thumbnail": str(raw.get("thumbnail") or ""),
        "duration": _safe_int(raw.get("duration")),
        "year": _safe_int(year, None),
        "show_title": str(raw.get("series") or raw.get("series_title") or ""),
        "season_number": raw.get("season_number"),
        "episode_number": raw.get("episode_number"),
        "episode_title": str(raw.get("episode") or ""),
        "categories": (
            list(raw.get("categories") or [])
            if isinstance(raw.get("categories"), (list, tuple))
            else [str(raw.get("categories"))] if raw.get("categories") else []
        ),
    }


def _downloaded_poster(download_dir, safe_title):
    candidates = []
    for path in Path(download_dir).glob(f"{safe_title}*"):
        if not path.is_file() or path.suffix.lower() not in {".jpg", ".jpeg", ".png", ".webp"}:
            continue
        try:
            candidates.append((path.stat().st_mtime, path))
        except OSError:
            continue
    return sorted(candidates)[-1][1] if candidates else None


def import_movie(url, download_dir, output_dir, yt_dlp, ffmpeg):
    if not is_supported_youtube_url(url):
        raise ValueError("Only YouTube URLs are supported.")
    os.makedirs(download_dir, exist_ok=True)
    os.makedirs(output_dir, exist_ok=True)
    metadata = _metadata_from_ytdlp(yt_dlp, url)
    title = metadata.get("title") or "YouTube Movie"
    safe_title = _sanitize_component(title)
    baseline = {}
    for path in Path(download_dir).glob(f"{safe_title}*"):
        if not path.is_file() or path.suffix.lower() not in VIDEO_EXTENSIONS:
            continue
        try:
            stat = path.stat()
        except OSError:
            continue
        baseline[str(path)] = (stat.st_size, stat.st_mtime)
    source_template = os.path.join(download_dir, f"{safe_title}.%(ext)s")
    ytdlp_command = [
        yt_dlp,
        "--no-playlist",
        "--merge-output-format",
        "mp4",
        "--write-thumbnail",
        "--convert-thumbnails",
        "jpg",
        "-f",
        "bv*[height<=480]+ba/b[height<=480]/best",
        "-o",
        source_template,
        url,
    ]
    _run_streamed(ytdlp_command)
    source_path = _downloaded_video(download_dir, safe_title, baseline)
    if source_path is None:
        raise RuntimeError("yt-dlp finished but no downloaded video file was found.")

    target_path = Path(output_dir) / f"{safe_title}.mpg"
    if target_path.exists():
        suffix = time.strftime("%Y%m%d-%H%M%S")
        target_path = Path(output_dir) / f"{safe_title}-{suffix}.mpg"
    ffmpeg_command = build_ffmpeg_command(str(source_path), str(target_path), "video", ffmpeg_path=ffmpeg)
    _run_streamed(ffmpeg_command)
    poster_source = _downloaded_poster(download_dir, safe_title)
    if poster_source:
        metadata["poster_path"] = persist_movie_import_poster(target_path, poster_source)
    print(f"ROCKPOD_MOVIE_METADATA={json.dumps(metadata, separators=(',', ':'))}", flush=True)
    print(f"ROCKPOD_MOVIE_OUTPUT={target_path}", flush=True)
    return str(target_path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", required=True)
    parser.add_argument("--download-dir", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--yt-dlp", default="yt-dlp")
    parser.add_argument("--ffmpeg", default="ffmpeg")
    args = parser.parse_args()
    import_movie(args.url, args.download_dir, args.output_dir, args.yt_dlp, args.ffmpeg)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
