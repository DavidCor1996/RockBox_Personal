"""Device-side audio transcoding for sync without touching the source library."""

from __future__ import annotations

import hashlib
import logging
import os
import shutil
import subprocess
from pathlib import Path

from models.track import compute_metadata_hash
from services.metadata_reader import compute_file_hash

logger = logging.getLogger(__name__)

LOSSLESS_EXTENSIONS = {".flac", ".alac", ".wav", ".aiff", ".aif", ".ape", ".wv"}
DIRECT_COPY_EXTENSIONS = {".mp3", ".m4a", ".aac"}
LOSSLESS_CODECS = {"flac", "alac", "wav", "aiff", "ape", "wavpack"}
AUDIO_CONVERSION_MODES = (
    "unsupported_or_lossless",
    "above_bitrate_limit",
    "unsupported_lossless_or_above_limit",
    "always",
)


def output_extension_for_codec(codec: str) -> str:
    text = str(codec or "").strip().lower()
    if text == "mp3":
        return ".mp3"
    if text in {"aac", "m4a"}:
        return ".m4a"
    return ".mp3"


class AudioSyncTranscoder:
    """Create cached, sync-only transcodes for a specific device/settings tuple."""

    def __init__(self, cache_root: str, ffmpeg_path: str = ""):
        self._cache_root = os.path.abspath(cache_root)
        self._ffmpeg_path = str(ffmpeg_path or "").strip()

    def ffmpeg_bin(self) -> str:
        explicit = os.path.abspath(self._ffmpeg_path) if self._ffmpeg_path else ""
        if explicit and os.path.isfile(explicit):
            return explicit
        return shutil.which("ffmpeg") or ""

    def is_available(self) -> bool:
        return bool(self.ffmpeg_bin())

    def prepare_track_for_sync(self, track_row, device_key: str, settings: dict):
        row = dict(track_row) if hasattr(track_row, "keys") else dict(track_row or {})
        source_path = os.path.abspath(str(row.get("file_path") or ""))
        ext = Path(source_path).suffix.lower()

        row["sync_source_path"] = source_path
        row["sync_output_ext"] = ext or output_extension_for_codec(settings.get("target_codec", "mp3"))
        row["sync_transcoded"] = False

        if not self._should_transcode(row, ext, settings):
            return row, {"converted": False, "cache_path": "", "reason": "direct_copy"}

        ffmpeg_bin = self.ffmpeg_bin()
        if not ffmpeg_bin:
            raise RuntimeError("ffmpeg is required for device audio conversion")

        target_codec = self._normalize_codec(settings.get("target_codec"))
        target_bitrate = self._normalize_bitrate(settings.get("target_bitrate_kbps"))
        cache_path = self._cache_path(source_path, row, device_key, target_codec, target_bitrate)
        self._ensure_transcode(ffmpeg_bin, source_path, cache_path, target_codec, target_bitrate)

        stat = os.stat(cache_path)
        row["sync_source_path"] = cache_path
        row["sync_output_ext"] = output_extension_for_codec(target_codec)
        row["sync_transcoded"] = True
        row["bitrate"] = target_bitrate
        row["codec"] = target_codec.upper()
        row["file_size"] = int(stat.st_size or 0)
        row["file_hash"] = compute_file_hash(cache_path)
        row["metadata_hash"] = compute_metadata_hash(
            row.get("title", ""),
            row.get("artist", ""),
            row.get("album", ""),
            row.get("album_artist", ""),
            row.get("track_number"),
            row.get("disc_number", 1),
            row.get("genre", ""),
            row.get("year"),
            row.get("composer", ""),
            row.get("duration", 0.0),
            row.get("bitrate", 0),
            row.get("codec", ""),
        )
        return row, {"converted": True, "cache_path": cache_path, "reason": "transcoded"}

    @staticmethod
    def _normalize_codec(value: str) -> str:
        text = str(value or "mp3").strip().lower()
        if text in {"aac", "m4a"}:
            return "aac"
        return "mp3"

    @staticmethod
    def _normalize_bitrate(value) -> int:
        try:
            bitrate = int(value or 160)
        except (TypeError, ValueError):
            bitrate = 160
        return max(64, min(320, bitrate))

    @staticmethod
    def _normalize_mode(value: str) -> str:
        text = str(value or "unsupported_or_lossless").strip().lower()
        if text in AUDIO_CONVERSION_MODES:
            return text
        return "unsupported_or_lossless"

    @staticmethod
    def _is_lossless(row: dict, ext: str) -> bool:
        codec = str(row.get("codec") or "").strip().lower()
        return ext in LOSSLESS_EXTENSIONS or codec in LOSSLESS_CODECS

    @staticmethod
    def _is_direct_copy_supported(row: dict, ext: str) -> bool:
        codec = str(row.get("codec") or "").strip().lower()
        if AudioSyncTranscoder._is_lossless(row, ext):
            return False
        return ext in DIRECT_COPY_EXTENSIONS or codec in {"mp3", "aac"}

    def _should_transcode(self, row: dict, ext: str, settings: dict) -> bool:
        if not bool(settings.get("enabled")):
            return False

        mode = self._normalize_mode(settings.get("mode"))
        bitrate_limit = self._normalize_bitrate(settings.get("target_bitrate_kbps"))
        direct_copy_supported = self._is_direct_copy_supported(row, ext)
        above_limit = int(row.get("bitrate") or 0) > bitrate_limit
        unsupported_or_lossless = self._is_lossless(row, ext) or not direct_copy_supported

        if mode == "always":
            return True
        if mode == "unsupported_or_lossless":
            return unsupported_or_lossless
        if mode == "above_bitrate_limit":
            return above_limit
        if mode == "unsupported_lossless_or_above_limit":
            return unsupported_or_lossless or above_limit
        return False

    def _cache_path(self, source_path: str, row: dict, device_key: str, target_codec: str, target_bitrate: int) -> str:
        try:
            stat = os.stat(source_path)
            fingerprint = f"{source_path}|{int(stat.st_mtime)}|{int(stat.st_size)}"
        except OSError:
            fingerprint = source_path

        source_hash = str(row.get("file_hash") or "")
        material = "|".join(
            [
                fingerprint,
                source_hash,
                str(device_key or "device"),
                target_codec,
                str(target_bitrate),
            ]
        )
        digest = hashlib.sha256(material.encode("utf-8")).hexdigest()[:16]
        source_stem = Path(source_path).stem[:64].strip() or "track"
        filename = f"{source_stem}-{digest}{output_extension_for_codec(target_codec)}"
        device_dir = os.path.join(self._cache_root, str(device_key or "device"))
        return os.path.join(device_dir, filename)

    def _ensure_transcode(self, ffmpeg_bin: str, source_path: str, cache_path: str, target_codec: str, target_bitrate: int):
        if os.path.isfile(cache_path) and os.path.getsize(cache_path) > 0:
            return

        os.makedirs(os.path.dirname(cache_path), exist_ok=True)
        tmp_path = cache_path + ".tmp"
        if os.path.exists(tmp_path):
            os.remove(tmp_path)

        command = self._build_ffmpeg_command(
            ffmpeg_bin,
            source_path,
            tmp_path,
            target_codec,
            target_bitrate,
        )
        try:
            result = subprocess.run(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )
        except OSError as exc:
            raise RuntimeError(f"ffmpeg launch failed: {exc}") from exc

        if result.returncode != 0:
            if os.path.exists(tmp_path):
                try:
                    os.remove(tmp_path)
                except OSError:
                    pass
            message = (result.stderr or result.stdout or f"ffmpeg exited with {result.returncode}").strip()
            raise RuntimeError(f"audio conversion failed: {message}")

        os.replace(tmp_path, cache_path)
        logger.info("Prepared sync transcode: %s -> %s", source_path, cache_path)

    @staticmethod
    def _build_ffmpeg_command(ffmpeg_bin: str, source_path: str, target_path: str, target_codec: str, target_bitrate: int):
        command = [
            ffmpeg_bin,
            "-y",
            "-i",
            source_path,
            "-map_metadata",
            "0",
            "-id3v2_version",
            "3",
            "-write_id3v1",
            "1",
        ]
        if target_codec == "aac":
            command.extend(["-c:a", "aac", "-b:a", f"{target_bitrate}k"])
        else:
            command.extend(["-c:a", "libmp3lame", "-b:a", f"{target_bitrate}k"])
        command.append(target_path)
        return command
