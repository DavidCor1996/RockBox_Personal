"""YouTube movie imports for legally-owned or authorized videos."""

from __future__ import annotations

import os
import re
import shutil
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from urllib.parse import urlparse

from PIL import Image


SUPPORTED_YOUTUBE_HOSTS = {
    "youtube.com",
    "www.youtube.com",
    "m.youtube.com",
    "music.youtube.com",
    "youtu.be",
}

YTDLP_PROGRESS_RE = re.compile(r"\[download\]\s+(?P<percent>\d+(?:\.\d+)?)%")
FFMPEG_TIME_RE = re.compile(r"\btime=(?P<time>\d+:\d{2}:\d{2}(?:\.\d+)?)")
UNSAFE_FILENAME_RE = re.compile(r"[<>:\"/\\|?*]+")
WHITESPACE_RE = re.compile(r"\s+")


class YoutubeMovieImportError(RuntimeError):
    """Raised when a YouTube movie import cannot be started."""


@dataclass(frozen=True)
class YoutubeMovieImportRequest:
    command: list[str]
    env: dict[str, str]
    output_dir: str
    log_path: str


@dataclass(frozen=True)
class YoutubeMovieBrowseRequest:
    command: list[str]
    env: dict[str, str]
    output_path: str


def is_supported_youtube_url(value):
    parsed = urlparse(str(value or "").strip())
    host = parsed.netloc.lower()
    if parsed.scheme not in {"http", "https"} or not host:
        return False
    return host in SUPPORTED_YOUTUBE_HOSTS or any(host.endswith(f".{item}") for item in SUPPORTED_YOUTUBE_HOSTS)


def sanitize_movie_title(value, max_len=96):
    text = str(value or "").strip() or "YouTube Movie"
    text = UNSAFE_FILENAME_RE.sub("_", text)
    text = WHITESPACE_RE.sub(" ", text).strip(" ._-")
    if len(text) > max_len:
        text = text[:max_len].rstrip(" ._-")
    return text or "YouTube Movie"


def normalize_movie_match_text(value):
    return WHITESPACE_RE.sub(" ", str(value or "").strip().lower())


def existing_movie_duplicate(rows, movie_result=None, output_dir=""):
    result = dict(movie_result or {})
    title = normalize_movie_match_text(result.get("title"))
    safe_title = normalize_movie_match_text(sanitize_movie_title(result.get("title"))) if title else ""
    url = normalize_movie_match_text(result.get("url"))
    expected_path = ""
    if output_dir and title:
        expected_path = os.path.join(output_dir, f"{sanitize_movie_title(result.get('title'))}.mpg")
    expected_name = normalize_movie_match_text(os.path.basename(expected_path))

    for row in rows or []:
        item = dict(row or {})
        file_path = str(item.get("file_path") or "")
        row_title = normalize_movie_match_text(item.get("title") or item.get("album") or Path(file_path).stem)
        row_comment = normalize_movie_match_text(item.get("comment"))
        row_name = normalize_movie_match_text(os.path.basename(file_path))
        if url and url in row_comment:
            return item
        if title and row_title in {title, safe_title}:
            return item
        if expected_path and os.path.abspath(file_path) == os.path.abspath(expected_path):
            return item
        if expected_name and row_name == expected_name:
            return item
    if expected_path and os.path.exists(expected_path):
        return {"title": result.get("title") or Path(expected_path).stem, "file_path": expected_path}
    return None


def persist_movie_import_poster(output_path, poster_source):
    video_path = Path(str(output_path or ""))
    source = Path(str(poster_source or ""))
    if not video_path or not source.is_file():
        return ""
    target = video_path.with_suffix(".jpg")
    try:
        with Image.open(source) as image:
            image.convert("RGB").save(target, "JPEG", quality=90, optimize=True)
    except Exception:
        return ""
    return str(target) if target.is_file() else ""


def parse_movie_import_progress(lines):
    if isinstance(lines, str):
        items = [line.strip() for line in lines.splitlines() if line.strip()]
    else:
        items = [str(line or "").strip() for line in (lines or []) if str(line or "").strip()]
    phase = "Working"
    progress = ""
    detail = items[-1][-500:] if items else ""

    for line in items:
        lower = line.lower()
        match = YTDLP_PROGRESS_RE.search(line)
        if match:
            phase = "Downloading"
            percent = float(match.group("percent"))
            progress = f"{percent:.1f}%" if percent % 1 else f"{int(percent)}%"
            detail = line[-500:]
            continue
        if "has already been downloaded" in lower:
            phase = "Downloading"
            progress = "100%"
            detail = line[-500:]
            continue
        if "merging formats" in lower:
            phase = "Preparing"
            detail = line[-500:]
            continue
        if "running:" in lower and "ffmpeg" in lower:
            phase = "Converting"
            progress = "Starting"
            detail = "Starting video conversion"
            continue
        if lower.startswith("frame=") or "mpeg2video" in lower:
            phase = "Converting"
            time_match = FFMPEG_TIME_RE.search(line)
            progress = time_match.group("time") if time_match else "Converting"
            detail = line[-500:]
            continue
        if line.startswith("ROCKPOD_MOVIE_OUTPUT="):
            phase = "Completed"
            progress = "100%"
            detail = os.path.basename(line.split("=", 1)[1].strip()) or "Movie import complete"

    return {"phase": phase, "progress": progress, "detail": detail}


class YoutubeMovieImporter:
    def __init__(self, config):
        self._config = config

    @property
    def video_dir(self):
        configured = getattr(self._config, "video_dir", "") or self._config.get("video_dir", "")
        return os.path.abspath(os.path.expanduser(configured))

    @property
    def state_dir(self):
        cache_dir = os.path.abspath(os.path.expanduser(self._config.cache_dir))
        return os.path.join(cache_dir, "youtube-movies")

    @property
    def temp_dir(self):
        return os.path.join(self.state_dir, "downloads")

    @property
    def log_dir(self):
        return os.path.join(self.state_dir, "logs")

    @property
    def browse_dir(self):
        return os.path.join(self.state_dir, "browse")

    @property
    def thumbnail_dir(self):
        return os.path.join(self.browse_dir, "thumbnails")

    @property
    def output_dir(self):
        return os.path.join(self.video_dir, "YouTube")

    def new_log_path(self):
        os.makedirs(self.log_dir, exist_ok=True)
        stamp = time.strftime("%Y%m%d-%H%M%S")
        return os.path.join(self.log_dir, f"youtube-movie-{stamp}.log")

    def prepare_import(self, url):
        item_url = str(url or "").strip()
        if not is_supported_youtube_url(item_url):
            raise YoutubeMovieImportError("Enter a YouTube URL for a video you own or are allowed to download.")
        ytdlp = self._config.get("youtube_movie_binary", "") or shutil.which("yt-dlp") or "yt-dlp"
        ffmpeg = self._config.get("ffmpeg_binary", "") or shutil.which("ffmpeg") or "ffmpeg"
        if shutil.which(ytdlp) is None and not os.path.exists(ytdlp):
            raise YoutubeMovieImportError("yt-dlp is required for YouTube movie imports.")
        if shutil.which(ffmpeg) is None and not os.path.exists(ffmpeg):
            raise YoutubeMovieImportError("ffmpeg is required to convert YouTube movies for Rockbox.")

        os.makedirs(self.temp_dir, exist_ok=True)
        os.makedirs(self.output_dir, exist_ok=True)
        log_path = self.new_log_path()
        script_path = Path(__file__).resolve().parents[1] / "scripts" / "youtube_movie_import.py"
        command = [
            sys.executable,
            str(script_path),
            "--url",
            item_url,
            "--download-dir",
            self.temp_dir,
            "--output-dir",
            self.output_dir,
            "--yt-dlp",
            ytdlp,
            "--ffmpeg",
            ffmpeg,
        ]
        with open(log_path, "w") as handle:
            handle.write("RockPod YouTube movie import log\n")
            handle.write(f"Started: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
            handle.write(f"URL: {item_url}\n")
            handle.write(f"Command: {' '.join(command)}\n\n")
        return YoutubeMovieImportRequest(
            command=command,
            env=os.environ.copy(),
            output_dir=self.output_dir,
            log_path=log_path,
        )

    def prepare_browse(self, query, limit=18):
        text = str(query or "").strip()
        if not text:
            raise YoutubeMovieImportError("Enter a movie search or choose a browse tab.")
        ytdlp = self._config.get("youtube_movie_binary", "") or shutil.which("yt-dlp") or "yt-dlp"
        if shutil.which(ytdlp) is None and not os.path.exists(ytdlp):
            raise YoutubeMovieImportError("yt-dlp is required to browse YouTube movies.")
        os.makedirs(self.browse_dir, exist_ok=True)
        os.makedirs(self.thumbnail_dir, exist_ok=True)
        stamp = time.strftime("%Y%m%d-%H%M%S")
        output_path = os.path.join(self.browse_dir, f"browse-{stamp}.json")
        script_path = Path(__file__).resolve().parents[1] / "scripts" / "youtube_movie_browse.py"
        command = [
            sys.executable,
            str(script_path),
            "--query",
            text,
            "--limit",
            str(max(1, min(40, int(limit or 18)))),
            "--yt-dlp",
            ytdlp,
            "--output",
            output_path,
            "--thumbnail-dir",
            self.thumbnail_dir,
        ]
        return YoutubeMovieBrowseRequest(command=command, env=os.environ.copy(), output_path=output_path)
