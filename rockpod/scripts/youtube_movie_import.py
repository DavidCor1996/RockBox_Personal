#!/usr/bin/env python3
"""Download an authorized YouTube video and convert it for Rockbox."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from services.android_media import build_ffmpeg_command  # noqa: E402
from services.youtube_movies import is_supported_youtube_url, sanitize_movie_title  # noqa: E402


VIDEO_EXTENSIONS = {".mp4", ".mkv", ".webm", ".mov", ".m4v"}


def _sanitize_component(value, max_len=96):
    return sanitize_movie_title(value, max_len=max_len)


def _downloaded_video(download_dir, safe_title, baseline=None):
    changed_candidates = []
    matched_candidates = []
    prefix = f"{safe_title}."
    for path in Path(download_dir).glob(f"{safe_title}*"):
        if not path.is_file() or path.suffix.lower() not in VIDEO_EXTENSIONS:
            continue
        try:
            stat = path.stat()
        except OSError:
            continue
        previous = (baseline or {}).get(str(path))
        matched_candidates.append((stat.st_mtime, path))
        if previous and previous == (stat.st_size, stat.st_mtime):
            continue
        if not path.name.startswith(prefix):
            continue
        changed_candidates.append((stat.st_mtime, path))
    if changed_candidates:
        return sorted(changed_candidates)[-1][1]
    if matched_candidates:
        return sorted(matched_candidates)[-1][1]
    fallback_candidates = []
    for path in Path(download_dir).glob("*"):
        if not path.is_file() or path.suffix.lower() not in VIDEO_EXTENSIONS:
            continue
        try:
            stat = path.stat()
        except OSError:
            continue
        fallback_candidates.append((stat.st_mtime, path))
    if not fallback_candidates:
        for path in Path(download_dir).glob("*"):
            if not path.is_file() or path.suffix.lower() not in VIDEO_EXTENSIONS:
                continue
            try:
                stat = path.stat()
            except OSError:
                continue
            fallback_candidates.append((stat.st_mtime, path))
    if not fallback_candidates:
        return None
    return sorted(fallback_candidates)[-1][1]


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


def _title_from_ytdlp(yt_dlp, url):
    command = [yt_dlp, "--no-playlist", "--print", "%(title)s", "--skip-download", url]
    try:
        result = subprocess.run(command, text=True, capture_output=True, check=True)
    except (OSError, subprocess.CalledProcessError):
        return "YouTube Movie"
    for line in result.stdout.splitlines():
        title = line.strip()
        if title:
            return title
    return "YouTube Movie"


def import_movie(url, download_dir, output_dir, yt_dlp, ffmpeg):
    if not is_supported_youtube_url(url):
        raise ValueError("Only YouTube URLs are supported.")
    os.makedirs(download_dir, exist_ok=True)
    os.makedirs(output_dir, exist_ok=True)
    title = _title_from_ytdlp(yt_dlp, url)
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
