"""Import legally-owned music through the external streamrip CLI."""

from __future__ import annotations

import os
import shutil
import sys
import sysconfig
import time
from dataclasses import dataclass
from pathlib import Path
from urllib.parse import urlparse

import mutagen
from tomlkit import dumps, parse

from services.file_safety import atomic_write_text


STREAMRIP_REPOSITORY_URL = "https://github.com/nathom/streamrip"
SUPPORTED_STREAMRIP_FORMATS = {"flac", "mp3", "alac", "aac", "ogg"}
SUPPORTED_STREAMRIP_HOSTS = {
    "tidal.com",
    "listen.tidal.com",
    "qobuz.com",
    "open.qobuz.com",
    "play.qobuz.com",
    "deezer.com",
    "www.deezer.com",
    "soundcloud.com",
    "www.soundcloud.com",
    "open.spotify.com",
}
SUPPORTED_AUDIO_EXTENSIONS = {".flac", ".mp3", ".m4a", ".aac", ".ogg", ".opus"}
STREAMRIP_MAX_QUALITY_BY_SOURCE = {
    "tidal": 3,
    "deezer": 2,
    "qobuz": 4,
    "soundcloud": 4,
    "spotify": 4,
}


class StreamripImportError(RuntimeError):
    """Raised when a streamrip import cannot be started."""


@dataclass(frozen=True)
class StreamripImportRequest:
    command: list[str]
    env: dict[str, str]
    started_at: float
    output_dir: str
    log_path: str


@dataclass(frozen=True)
class StreamripSearchRequest:
    command: list[str]
    env: dict[str, str]
    output_path: str


def _streamrip_template_config_path():
    candidates = [
        Path(sysconfig.get_paths().get("purelib", "")) / "streamrip" / "config.toml",
        Path(__file__).resolve().parents[1] / ".venv" / "lib" / f"python{sysconfig.get_python_version()}" / "site-packages" / "streamrip" / "config.toml",
    ]
    for path in candidates:
        if path.is_file():
            return str(path)
    return ""


def is_supported_streamrip_url(value):
    parsed = urlparse(str(value or "").strip())
    host = parsed.netloc.lower()
    if parsed.scheme not in {"http", "https"} or not host:
        return False
    return host in SUPPORTED_STREAMRIP_HOSTS or any(
        host.endswith(f".{supported}") for supported in SUPPORTED_STREAMRIP_HOSTS
    )


def streamrip_source_for_url(value):
    host = urlparse(str(value or "").strip()).netloc.lower()
    if host == "listen.tidal.com" or host == "tidal.com" or host.endswith(".tidal.com"):
        return "tidal"
    if host in {"qobuz.com", "open.qobuz.com", "play.qobuz.com"} or host.endswith(".qobuz.com"):
        return "qobuz"
    if host == "deezer.com" or host.endswith(".deezer.com"):
        return "deezer"
    if host == "soundcloud.com" or host.endswith(".soundcloud.com"):
        return "soundcloud"
    if host == "open.spotify.com":
        return "spotify"
    return ""


def streamrip_url_info(value):
    parsed = urlparse(str(value or "").strip())
    source = streamrip_source_for_url(value)
    parts = [part for part in parsed.path.split("/") if part]
    media_type = ""
    item_id = ""
    if source == "tidal" and len(parts) >= 2 and parts[0] in {"album", "track"}:
        media_type = parts[0]
        item_id = parts[1]
    elif source == "deezer" and len(parts) >= 2 and parts[0] in {"album", "track"}:
        media_type = parts[0]
        item_id = parts[1]
    elif source == "qobuz":
        if "album" in parts:
            media_type = "album"
            item_id = parts[-1]
        elif "track" in parts:
            media_type = "track"
            item_id = parts[-1]
    elif source == "spotify" and len(parts) >= 2 and parts[0] in {"album", "track", "playlist"}:
        media_type = parts[0]
        item_id = parts[1]
    return {
        "source": source,
        "media_type": media_type,
        "id": item_id,
    }


def is_spotify_playlist_url(value):
    info = streamrip_url_info(value)
    return info.get("source") == "spotify" and info.get("media_type") == "playlist" and bool(info.get("id"))


def _flatten_tag_values(value):
    if value is None:
        return []
    if isinstance(value, bytes):
        return [value.decode("utf-8", errors="replace")]
    if hasattr(value, "text"):
        return _flatten_tag_values(getattr(value, "text"))
    if isinstance(value, (list, tuple, set)):
        flattened = []
        for item in value:
            flattened.extend(_flatten_tag_values(item))
        return flattened
    return [str(value)]


def streamrip_source_tag_matches(tags, source, media_type, item_id):
    source = str(source or "").strip().upper()
    media_type = str(media_type or "").strip().upper()
    item_id = str(item_id or "").strip()
    if not source or media_type not in {"ALBUM", "TRACK"} or not item_id:
        return False
    expected_keys = {
        f"{source}_{media_type}_ID",
        f"{source}_{media_type}ID",
        f"{source}{media_type}ID",
    }
    for key, value in (tags or {}).items():
        normalized_key = str(key or "").upper().replace(" ", "_").replace("-", "_")
        normalized_key_parts = {normalized_key, normalized_key.split(":")[-1]}
        if not expected_keys.intersection(normalized_key_parts):
            continue
        if any(str(candidate).strip() == item_id for candidate in _flatten_tag_values(value)):
            return True
    return False


def find_existing_streamrip_source_file(library_root, source, media_type, item_id):
    root = Path(library_root).expanduser()
    if not root.is_dir():
        return ""
    for path in root.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in SUPPORTED_AUDIO_EXTENSIONS:
            continue
        try:
            audio = mutagen.File(str(path), easy=False)
        except Exception:
            continue
        if audio is None or not getattr(audio, "tags", None):
            continue
        if streamrip_source_tag_matches(audio.tags, source, media_type, item_id):
            return str(path)
    return ""


def streamrip_quality_for_url(value, requested_quality=4):
    try:
        quality = max(0, min(4, int(requested_quality)))
    except (TypeError, ValueError):
        quality = 4
    source = streamrip_source_for_url(value)
    return min(quality, STREAMRIP_MAX_QUALITY_BY_SOURCE.get(source, 4))


def discover_imported_audio_files(root, since_timestamp=0.0):
    found = []
    root_path = Path(root).expanduser()
    if not root_path.is_dir():
        return found
    for path in root_path.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in SUPPORTED_AUDIO_EXTENSIONS:
            continue
        try:
            if path.stat().st_mtime + 0.001 < since_timestamp:
                continue
        except OSError:
            continue
        found.append(str(path))
    return sorted(found)


def ensure_rockbox_cover_files(audio_files, library_root):
    """Ensure every imported audio folder has a Rockbox-friendly cover.jpg."""
    library_root = os.path.abspath(os.path.expanduser(str(library_root)))
    cover_names = ("cover.jpg", "folder.jpg", "front.jpg", "album.jpg")
    updated = []
    for audio_file in audio_files:
        folder = os.path.abspath(os.path.dirname(audio_file))
        if os.path.commonpath([library_root, folder]) != library_root:
            continue
        target = os.path.join(folder, "cover.jpg")
        if os.path.exists(target):
            continue

        candidates = []
        current = folder
        while os.path.commonpath([library_root, current]) == library_root:
            for name in cover_names:
                candidates.append(os.path.join(current, name))
            parent = os.path.dirname(current)
            if parent == current:
                break
            current = parent

        source = next((path for path in candidates if os.path.isfile(path)), "")
        if not source:
            continue
        try:
            shutil.copy2(source, target)
        except OSError:
            continue
        updated.append(target)
    return sorted(set(updated))


def ensure_streamrip_config(config_path, music_dir, quality=4):
    """Create/update a streamrip config tuned for RockPod album imports."""
    config_path = os.path.abspath(os.path.expanduser(str(config_path)))
    music_dir = os.path.abspath(os.path.expanduser(str(music_dir)))
    os.makedirs(os.path.dirname(config_path), exist_ok=True)
    if os.path.exists(config_path):
        with open(config_path, "r") as handle:
            data = parse(handle.read())
    else:
        template = _streamrip_template_config_path()
        if not template:
            raise StreamripImportError("Could not find streamrip's default config template.")
        with open(template, "r") as handle:
            data = parse(handle.read())

    quality = max(0, min(4, int(quality or 4)))
    data["downloads"]["folder"] = music_dir
    data["downloads"]["source_subdirectories"] = False
    data["downloads"]["disc_subdirectories"] = True
    data["downloads"]["concurrency"] = True
    data["qobuz"]["quality"] = quality
    data["tidal"]["quality"] = min(3, quality)
    data["deezer"]["quality"] = min(2, quality)
    data["deezer"]["lower_quality_if_not_available"] = True
    data["artwork"]["embed"] = True
    data["artwork"]["embed_size"] = "large"
    data["artwork"]["embed_max_width"] = -1
    data["artwork"]["save_artwork"] = True
    data["artwork"]["saved_max_width"] = -1
    data["metadata"]["exclude"] = []
    data["filepaths"]["add_singles_to_folder"] = True
    data["filepaths"]["folder_format"] = "{albumartist} - {title} ({year})"
    data["filepaths"]["track_format"] = "{tracknumber:02}. {title}"
    data["cli"]["text_output"] = True
    data["cli"]["progress_bars"] = False
    data["misc"]["check_for_updates"] = False

    atomic_write_text(config_path, dumps(data))
    return config_path


def build_streamrip_command(binary, music_dir, url, output_format="flac", quality=4, config_path=""):
    binary = str(binary or "rip").strip() or "rip"
    music_dir = os.path.abspath(os.path.expanduser(str(music_dir or "")))
    item_url = str(url or "").strip()
    output_format = str(output_format or "flac").strip().lower()
    if not is_supported_streamrip_url(item_url):
        raise StreamripImportError("Enter a Tidal, Qobuz, Deezer, SoundCloud, or Spotify URL.")
    if output_format not in SUPPORTED_STREAMRIP_FORMATS:
        raise StreamripImportError("RockPod supports FLAC, ALAC, AAC, OGG, or MP3 streamrip imports.")
    quality = streamrip_quality_for_url(item_url, quality)
    url_info = streamrip_url_info(item_url)

    if url_info.get("source") == "spotify":
        if url_info.get("media_type") != "playlist" or not url_info.get("id"):
            raise StreamripImportError("RockPod can convert Spotify playlist URLs to Tidal imports.")
        if not config_path:
            raise StreamripImportError("Spotify playlist conversion requires a streamrip config path.")
        script_path = Path(__file__).resolve().parents[1] / "scripts" / "spotify_playlist_tidal_import.py"
        return [
            sys.executable,
            str(script_path),
            "--config-path",
            os.path.abspath(os.path.expanduser(str(config_path))),
            "--url",
            item_url,
            "--output-format",
            output_format,
            "--quality",
            str(quality),
            "--source",
            "tidal",
        ]

    command = [
        binary,
        "-f",
        music_dir,
        "--no-db",
        "-q",
        str(quality),
        "-c",
        output_format.upper(),
        "--no-progress",
    ]
    if config_path:
        command.extend(["--config-path", os.path.abspath(os.path.expanduser(str(config_path)))])
    command.extend(["url", item_url])
    return command


class StreamripImporter:
    """Builds streamrip commands for RockPod.

    streamrip creates its own config and auth state. RockPod points that state at
    the application cache so imports do not depend on writing to the user's home
    config directory.
    """

    def __init__(self, config):
        self._config = config

    @property
    def music_dir(self):
        return os.path.abspath(os.path.expanduser(self._config.music_dir))

    @property
    def streamrip_state_dir(self):
        cache_dir = os.path.abspath(os.path.expanduser(self._config.cache_dir))
        return os.path.join(cache_dir, "streamrip")

    @property
    def config_path(self):
        return os.path.join(self.streamrip_state_dir, "config.toml")

    @property
    def log_dir(self):
        return os.path.join(self.streamrip_state_dir, "logs")

    @property
    def search_dir(self):
        return os.path.join(self.streamrip_state_dir, "store-search")

    @property
    def cover_dir(self):
        return os.path.join(self.streamrip_state_dir, "store-covers")

    def new_log_path(self):
        os.makedirs(self.log_dir, exist_ok=True)
        stamp = time.strftime("%Y%m%d-%H%M%S")
        return os.path.join(self.log_dir, f"import-{stamp}.log")

    def env(self):
        env = os.environ.copy()
        env["XDG_CONFIG_HOME"] = os.path.join(self.streamrip_state_dir, "xdg-config")
        env["XDG_CACHE_HOME"] = os.path.join(self.streamrip_state_dir, "xdg-cache")
        return env

    def command_for(self, url, output_format=None):
        return build_streamrip_command(
            self._config.get("streamrip_binary", "rip"),
            self.music_dir,
            url,
            output_format or self._config.get("streamrip_preferred_format", "flac"),
            self._config.get("streamrip_quality", 4),
            self.config_path,
        )

    def prepare_album_search(self, query, source="tidal", limit=24):
        text = str(query or "").strip()
        source = str(source or "tidal").strip().lower()
        if not text:
            raise StreamripImportError("Enter an album, artist, or song to search.")
        if source not in {"tidal", "qobuz", "deezer"}:
            raise StreamripImportError("Store search supports Tidal, Qobuz, or Deezer.")

        os.makedirs(self.streamrip_state_dir, exist_ok=True)
        os.makedirs(self.search_dir, exist_ok=True)
        os.makedirs(self.cover_dir, exist_ok=True)
        ensure_streamrip_config(
            self.config_path,
            self.music_dir,
            self._config.get("streamrip_quality", 4),
        )
        stamp = time.strftime("%Y%m%d-%H%M%S")
        output_path = os.path.join(self.search_dir, f"search-{stamp}.json")
        script_path = Path(__file__).resolve().parents[1] / "scripts" / "streamrip_store_search.py"
        command = [sys.executable]
        command.extend(
            [
                str(script_path),
                "--config-path",
                self.config_path,
                "--source",
                source,
                "--media-type",
                "album",
                "--query",
                text,
                "--limit",
                str(max(1, min(50, int(limit or 24)))),
                "--cover-dir",
                self.cover_dir,
                "--output",
                output_path,
            ]
        )
        return StreamripSearchRequest(command=command, env=self.env(), output_path=output_path)

    def prepare_store_homepage(self, limit=24, home_tab="featured"):
        home_tab = str(home_tab or "featured").strip().lower()
        os.makedirs(self.streamrip_state_dir, exist_ok=True)
        os.makedirs(self.search_dir, exist_ok=True)
        os.makedirs(self.cover_dir, exist_ok=True)
        ensure_streamrip_config(
            self.config_path,
            self.music_dir,
            self._config.get("streamrip_quality", 4),
        )
        stamp = time.strftime("%Y%m%d-%H%M%S")
        output_path = os.path.join(self.search_dir, f"homepage-{home_tab}-{stamp}.json")
        script_path = Path(__file__).resolve().parents[1] / "scripts" / "streamrip_store_search.py"
        command = [
            sys.executable,
            str(script_path),
            "--config-path",
            self.config_path,
            "--homepage",
            "--home-tab",
            home_tab,
            "--limit",
            str(max(1, min(50, int(limit or 24)))),
            "--cover-dir",
            self.cover_dir,
            "--output",
            output_path,
        ]
        return StreamripSearchRequest(command=command, env=self.env(), output_path=output_path)

    def prepare_album_detail(self, source, album_id):
        source = str(source or "tidal").strip().lower()
        item_id = str(album_id or "").strip()
        if source not in {"tidal", "qobuz", "deezer"}:
            raise StreamripImportError("Album details support Tidal, Qobuz, or Deezer.")
        if not item_id:
            raise StreamripImportError("Album details require an album id.")

        os.makedirs(self.streamrip_state_dir, exist_ok=True)
        os.makedirs(self.search_dir, exist_ok=True)
        os.makedirs(self.cover_dir, exist_ok=True)
        ensure_streamrip_config(
            self.config_path,
            self.music_dir,
            self._config.get("streamrip_quality", 4),
        )
        stamp = time.strftime("%Y%m%d-%H%M%S")
        output_path = os.path.join(self.search_dir, f"album-detail-{source}-{item_id}-{stamp}.json")
        script_path = Path(__file__).resolve().parents[1] / "scripts" / "streamrip_store_search.py"
        command = [
            sys.executable,
            str(script_path),
            "--config-path",
            self.config_path,
            "--source",
            source,
            "--album-id",
            item_id,
            "--cover-dir",
            self.cover_dir,
            "--output",
            output_path,
        ]
        return StreamripSearchRequest(command=command, env=self.env(), output_path=output_path)

    def prepare_import(self, url, output_format=None):
        binary = self._config.get("streamrip_binary", "rip")
        if not is_spotify_playlist_url(url) and shutil.which(binary) is None and not os.path.exists(binary):
            raise StreamripImportError(
                f"streamrip is not installed or not on PATH. Install {STREAMRIP_REPOSITORY_URL} "
                "or set the streamrip binary in RockPod config."
            )
        os.makedirs(self.music_dir, exist_ok=True)
        os.makedirs(self.streamrip_state_dir, exist_ok=True)
        ensure_streamrip_config(
            self.config_path,
            self.music_dir,
            self._config.get("streamrip_quality", 4),
        )
        command = self.command_for(url, output_format)
        log_path = self.new_log_path()
        with open(log_path, "w") as handle:
            handle.write("RockPod streamrip import log\n")
            handle.write(f"Started: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
            handle.write(f"URL: {url}\n")
            handle.write(f"Output format: {output_format or self._config.get('streamrip_preferred_format', 'flac')}\n")
            handle.write(f"Command: {' '.join(command)}\n\n")
        return StreamripImportRequest(
            command=command,
            env=self.env(),
            started_at=time.time(),
            output_dir=self.music_dir,
            log_path=log_path,
        )
