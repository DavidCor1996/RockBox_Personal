"""YouTube movie imports for legally-owned or authorized videos."""

from __future__ import annotations

import os
import shutil
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from urllib.parse import urlparse


SUPPORTED_YOUTUBE_HOSTS = {
    "youtube.com",
    "www.youtube.com",
    "m.youtube.com",
    "music.youtube.com",
    "youtu.be",
}


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
        ]
        return YoutubeMovieBrowseRequest(command=command, env=os.environ.copy(), output_path=output_path)
