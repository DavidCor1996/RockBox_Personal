"""iPod video sync conversion with efficient MPEG-2 and raw RVP profiles."""

from __future__ import annotations

import hashlib
import fcntl
import logging
import math
import os
import shutil
import json
from contextlib import contextmanager
from pathlib import Path

from models.track import compute_metadata_hash
from services.command_runner import CommandRunner

logger = logging.getLogger(__name__)

RVP_WIDTH = 320
RVP_HEIGHT = 240
RVP_FPS = 20
RVP_SAMPLE_RATE = 44100
RVP_CHANNELS = 2
RVP_BYTES_PER_SAMPLE = 2
RVP_PCM_BYTES_PER_FRAME = RVP_SAMPLE_RATE * RVP_CHANNELS * RVP_BYTES_PER_SAMPLE // RVP_FPS
RVP_SEGMENT_SECONDS = 120
RVP_SEGMENT_FRAMES = RVP_SEGMENT_SECONDS * RVP_FPS
RVP_SEGMENT_YUV_LIMIT = 512 * 1024 * 1024
RVP_SEGMENT_PCM_LIMIT = 40 * 1024 * 1024
RVP_PROFILE_KEY = "sync-profile-v2-screen-padded"
RVP_MPEG_PROFILE_KEY = "sync-mpeg2-v3-seekable-hq-320x240-20fps"
RVP_PROFILE_DEFAULT = "raw"
RVP_CACHE_SPACE_RESERVE = 1024 * 1024 * 1024
RVP_PROFILE_DEFINITIONS = {
    "quality": {
        "format": "mpeg2",
        "max_width": 320,
        "max_height": 240,
        "fps": 20,
        "sample_rate": 44100,
    },
    "compact_raw": {
        "format": "rvp",
        "max_width": 160,
        "max_height": 120,
        "fps": 20,
        "sample_rate": 22050,
    },
    "native_raw": {
        "format": "rvp",
        "max_width": 320,
        "max_height": 240,
        "fps": 20,
        "sample_rate": 22050,
    },
    "raw": {
        "format": "rvp",
        "max_width": 320,
        "max_height": 240,
        "fps": 20,
        "sample_rate": 44100,
    },
}


class VideoRvpTranscoder:
    """Prepare MPEG-2 files or raw RVP bundles for Rockbox playback."""

    def __init__(
        self,
        cache_root: str,
        ffmpeg_path: str = "",
        command_runner=None,
        profile: str = RVP_PROFILE_DEFAULT,
    ):
        self._cache_root = os.path.abspath(cache_root)
        self._ffmpeg_path = str(ffmpeg_path or "").strip()
        self._command_runner = command_runner or CommandRunner(log_dir=os.path.join(self._cache_root, "logs"))
        self._profile = ""
        self._target_width = RVP_WIDTH
        self._target_height = RVP_HEIGHT
        self._target_fps = RVP_FPS
        self._sample_rate = RVP_SAMPLE_RATE
        self._format = "rvp"
        self._pcm_bytes_per_frame = RVP_PCM_BYTES_PER_FRAME
        self.set_profile(profile)

    @staticmethod
    def _normalize_profile(profile):
        normalized = str(profile or RVP_PROFILE_DEFAULT).strip().lower().replace("-", "_")
        if normalized in {
            "hi", "high", "quality", "best", "low", "small", "compact",
            "efficient", "mpeg", "mpeg2",
        }:
            normalized = "quality"
        elif normalized in {
            "balanced", "compact_raw", "compact_rvp", "small_raw",
        }:
            normalized = "compact_raw"
        elif normalized in {
            "native", "native_raw", "sharp", "sharp_raw", "recommended",
        }:
            normalized = "native_raw"
        elif normalized in {"raw", "rvp"}:
            normalized = "raw"
        return normalized if normalized in RVP_PROFILE_DEFINITIONS else RVP_PROFILE_DEFAULT

    @staticmethod
    def _coerce_positive_int(value, fallback):
        try:
            parsed = int(value)
            if parsed > 0:
                return parsed
        except (TypeError, ValueError):
            pass
        return fallback

    def set_profile(self, profile):
        self._profile = self._normalize_profile(profile)
        cfg = RVP_PROFILE_DEFINITIONS.get(self._profile, RVP_PROFILE_DEFINITIONS[RVP_PROFILE_DEFAULT])
        self._target_width = self._coerce_positive_int(cfg.get("max_width"), RVP_WIDTH)
        self._target_height = self._coerce_positive_int(cfg.get("max_height"), RVP_HEIGHT)
        self._target_fps = self._coerce_positive_int(cfg.get("fps"), RVP_FPS)
        self._sample_rate = self._coerce_positive_int(cfg.get("sample_rate"), RVP_SAMPLE_RATE)
        self._format = str(cfg.get("format") or "rvp")
        if self._target_fps > 0:
            self._pcm_bytes_per_frame = (
                self._sample_rate * RVP_CHANNELS * RVP_BYTES_PER_SAMPLE // self._target_fps
            )
        else:
            self._target_fps = RVP_FPS
            self._pcm_bytes_per_frame = RVP_PCM_BYTES_PER_FRAME
        return self._profile

    def ffmpeg_bin(self) -> str:
        explicit = os.path.abspath(self._ffmpeg_path) if self._ffmpeg_path else ""
        if explicit and os.path.isfile(explicit):
            return explicit
        if self._ffmpeg_path and shutil.which(self._ffmpeg_path):
            return shutil.which(self._ffmpeg_path) or ""
        return shutil.which("ffmpeg") or ""

    def is_available(self) -> bool:
        return bool(self.ffmpeg_bin())

    def prepare_track_for_sync(self, track_row, device_key: str):
        row = dict(track_row) if hasattr(track_row, "keys") else dict(track_row or {})
        source_path = os.path.abspath(str(row.get("file_path") or ""))
        ffmpeg_bin = self.ffmpeg_bin()
        if not ffmpeg_bin:
            raise RuntimeError("ffmpeg is required for iPod video conversion")
        if not os.path.isfile(source_path):
            raise RuntimeError("source video not found")

        if self._format == "mpeg2":
            return self._prepare_mpeg_track(
                row, source_path, ffmpeg_bin, device_key
            )

        source_width, source_height, source_duration = self._probe_video_info(
            ffmpeg_bin, source_path
        )
        video_width, video_height = self._fit_dimensions(
            source_width,
            source_height,
            self._target_width,
            self._target_height,
        )

        marker_path = self._cache_marker_path(source_path, row, device_key)
        yuv_path = os.path.splitext(marker_path)[0] + ".yuv"
        pcm_path = os.path.splitext(marker_path)[0] + ".pcm"
        with self._bundle_lock(marker_path):
            self._ensure_bundle(
                ffmpeg_bin,
                source_path,
                marker_path,
                yuv_path,
                pcm_path,
                row,
                video_width,
                video_height,
                source_duration=source_duration,
            )

            sizes = {
                "marker": os.path.getsize(marker_path),
                "yuv": os.path.getsize(yuv_path),
                "pcm": os.path.getsize(pcm_path),
            }
            segments = self._ensure_segments(marker_path, yuv_path, pcm_path, video_width, video_height)
            if segments:
                sizes = {"marker": os.path.getsize(marker_path)}
                for index, segment in enumerate(segments, start=1):
                    sizes[f"seg{index:02d}_yuv"] = os.path.getsize(segment["yuv"])
                    sizes[f"seg{index:02d}_pcm"] = os.path.getsize(segment["pcm"])

            hash_paths = [marker_path]
            if segments:
                for segment in segments:
                    hash_paths.extend([segment["yuv"], segment["pcm"]])
            else:
                hash_paths.extend([yuv_path, pcm_path])
            bundle_hash = self._bundle_hash_cached(marker_path, *hash_paths)
        row["sync_source_path"] = marker_path
        row["sync_output_ext"] = ".rvp"
        row["sync_transcoded"] = True
        row["sync_video_bundle_paths"] = {
            "rvp": marker_path,
            "yuv": yuv_path,
            "pcm": pcm_path,
        }
        row["sync_video_width"] = video_width
        row["sync_video_height"] = video_height
        row["sync_video_fps"] = self._target_fps
        row["sync_video_sample_rate"] = self._sample_rate
        row["sync_video_profile"] = self._profile
        if segments:
            row["sync_video_segments"] = segments
        row["sync_video_bundle_sizes"] = sizes
        row["file_size"] = int(sum(sizes.values()))
        row["file_hash"] = bundle_hash
        row["codec"] = "RVP"
        row["bitrate"] = 0
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
            row.get("media_type", ""),
            row.get("video_kind", ""),
            row.get("show_title", ""),
            row.get("season_number"),
            row.get("episode_number"),
        )
        return row, {"converted": True, "cache_path": marker_path, "reason": "rvp_video"}

    def _prepare_mpeg_track(
        self, row: dict, source_path: str, ffmpeg_bin: str, device_key: str
    ):
        media_info = self._probe_mpeg_info(ffmpeg_bin, source_path)
        # Structural codec compatibility is not enough for reliable Rockbox
        # controls. Rebuild even compatible inputs so the program stream has
        # bounded GOPs, predictable timestamps, and seek-friendly packetization.
        output_path = self._cache_mpeg_path(
            source_path, row, device_key
        )
        with self._bundle_lock(output_path):
            self._ensure_mpeg_file(
                ffmpeg_bin,
                source_path,
                output_path,
                row,
                media_info.get("duration"),
            )
        converted = True

        row["sync_source_path"] = output_path
        row["sync_output_ext"] = ".mpg"
        row["sync_transcoded"] = converted
        row["sync_video_width"] = RVP_WIDTH
        row["sync_video_height"] = RVP_HEIGHT
        row["sync_video_fps"] = RVP_FPS
        row["sync_video_sample_rate"] = RVP_SAMPLE_RATE
        row["sync_video_profile"] = self._profile
        row["file_size"] = os.path.getsize(output_path)
        if converted:
            row["file_hash"] = self._bundle_hash_cached(
                output_path, output_path
            )
        row["codec"] = "MPEG-2"
        try:
            row["bitrate"] = int(media_info.get("bit_rate") or 0) // 1000
        except (TypeError, ValueError):
            row["bitrate"] = 0
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
            row.get("media_type", ""),
            row.get("video_kind", ""),
            row.get("show_title", ""),
            row.get("season_number"),
            row.get("episode_number"),
        )
        reason = "seekable_mpeg2_video"
        return row, {
            "converted": converted,
            "cache_path": output_path,
            "reason": reason,
        }

    def _cache_mpeg_path(
        self, source_path: str, row: dict, device_key: str
    ) -> str:
        try:
            stat = os.stat(source_path)
            fingerprint = (
                f"{source_path}|{int(stat.st_mtime)}|{int(stat.st_size)}"
            )
        except OSError:
            fingerprint = source_path
        material = "|".join(
            [
                fingerprint,
                str(row.get("file_hash") or ""),
                str(device_key or "device"),
                RVP_MPEG_PROFILE_KEY,
            ]
        )
        digest = hashlib.sha256(material.encode("utf-8")).hexdigest()[:16]
        stem = self._safe_stem(Path(source_path).stem)
        device_dir = os.path.join(
            self._cache_root, str(device_key or "device")
        )
        return os.path.join(device_dir, f"{stem}-{digest}.mpg")

    def _probe_mpeg_info(
        self, ffmpeg_bin: str, source_path: str
    ) -> dict:
        ffprobe_bin = self._ffprobe_bin(ffmpeg_bin)
        if not ffprobe_bin:
            return {}
        command = [
            ffprobe_bin,
            "-v",
            "error",
            "-show_entries",
            (
                "stream=index,codec_type,codec_name,width,height,pix_fmt,"
                "r_frame_rate,has_b_frames,sample_rate,channels:"
                "format=duration,bit_rate"
            ),
            "-of",
            "json",
            source_path,
        ]
        try:
            result = self._command_runner.run(
                command, cwd=os.path.dirname(source_path) or os.getcwd()
            )
        except OSError:
            return {}
        if result.returncode != 0:
            return {}
        try:
            payload = json.loads(result.stdout or "")
        except (TypeError, json.JSONDecodeError):
            return {}

        info = dict(payload.get("format") or {})
        for stream in payload.get("streams") or []:
            stream_type = stream.get("codec_type")
            if stream_type == "video" and "video" not in info:
                info["video"] = dict(stream)
            elif stream_type == "audio" and "audio" not in info:
                info["audio"] = dict(stream)
        return info

    @staticmethod
    def _fps_value(value) -> float:
        text = str(value or "").strip()
        if not text:
            return 0.0
        try:
            if "/" in text:
                numerator, denominator = text.split("/", 1)
                denominator_value = float(denominator)
                if denominator_value == 0:
                    return 0.0
                return float(numerator) / denominator_value
            return float(text)
        except (TypeError, ValueError):
            return 0.0

    @classmethod
    def _is_ipod_mpeg_compatible(
        cls, source_path: str, media_info: dict
    ) -> bool:
        if Path(source_path).suffix.lower() not in {".mpg", ".mpeg"}:
            return False
        video = dict(media_info.get("video") or {})
        audio = dict(media_info.get("audio") or {})
        fps = cls._fps_value(video.get("r_frame_rate"))
        return (
            video.get("codec_name") in {"mpeg1video", "mpeg2video"}
            and int(video.get("width") or 0) == RVP_WIDTH
            and int(video.get("height") or 0) == RVP_HEIGHT
            and video.get("pix_fmt") == "yuv420p"
            and int(video.get("has_b_frames") or 0) == 0
            and 19.9 <= fps <= 20.1
            and audio.get("codec_name") in {"mp2", "mp3"}
            and int(audio.get("sample_rate") or 0) == RVP_SAMPLE_RATE
            and int(audio.get("channels") or 0) == RVP_CHANNELS
        )

    def _ensure_mpeg_file(
        self,
        ffmpeg_bin: str,
        source_path: str,
        output_path: str,
        row: dict,
        source_duration=None,
    ):
        if os.path.isfile(output_path) and os.path.getsize(output_path) > 0:
            return

        output_dir = os.path.dirname(output_path)
        os.makedirs(output_dir, exist_ok=True)
        tmp_path = output_path + ".tmp"
        try:
            os.remove(tmp_path)
        except FileNotFoundError:
            pass

        duration = self._positive_float(source_duration)
        if duration is None:
            duration = self._positive_float(row.get("duration"))
        if duration is not None:
            estimated_bytes = int(
                math.ceil(duration * (1_600_000 + 112_000) / 8)
            )
            required_bytes = estimated_bytes + RVP_CACHE_SPACE_RESERVE
            try:
                free_bytes = shutil.disk_usage(output_dir).free
            except OSError:
                free_bytes = required_bytes
            if free_bytes < required_bytes:
                gib = 1024.0 * 1024.0 * 1024.0
                raise RuntimeError(
                    "not enough local cache space for MPEG-2 video "
                    f"(about {required_bytes / gib:.1f} GiB needed, "
                    f"{free_bytes / gib:.1f} GiB free)"
                )

        try:
            self._run_ffmpeg(
                self._mpeg_command(ffmpeg_bin, source_path, tmp_path),
                output_path,
                "MPEG-2 video conversion failed",
            )
            if not os.path.isfile(tmp_path) or os.path.getsize(tmp_path) <= 0:
                raise RuntimeError(
                    "MPEG-2 video conversion produced an empty file"
                )
            os.replace(tmp_path, output_path)
        except Exception:
            try:
                os.remove(tmp_path)
            except FileNotFoundError:
                pass
            raise

    @staticmethod
    def _mpeg_command(
        ffmpeg_bin: str, source_path: str, target_path: str
    ):
        return [
            ffmpeg_bin,
            "-y",
            "-loglevel",
            "error",
            "-i",
            source_path,
            "-vf",
            VideoRvpTranscoder._video_filter(
                RVP_WIDTH, RVP_HEIGHT, fps=RVP_FPS
            ),
            "-map",
            "0:v:0",
            "-map",
            "0:a:0?",
            "-c:v",
            "mpeg2video",
            "-pix_fmt",
            "yuv420p",
            "-bf",
            "0",
            "-g",
            "12",
            "-flags",
            "+low_delay",
            "-sc_threshold",
            "0",
            "-q:v",
            "2",
            "-maxrate",
            "1600k",
            "-bufsize",
            "800k",
            "-c:a",
            "mp2",
            "-ar",
            str(RVP_SAMPLE_RATE),
            "-ac",
            str(RVP_CHANNELS),
            "-b:a",
            "112k",
            "-packetsize",
            "2048",
            "-f",
            "mpeg",
            target_path,
        ]

    @staticmethod
    @contextmanager
    def _bundle_lock(marker_path: str):
        """Serialize preparation of one deterministic cache bundle."""
        lock_path = marker_path + ".lock"
        os.makedirs(os.path.dirname(lock_path), exist_ok=True)
        with open(lock_path, "a+b") as lock_handle:
            fcntl.flock(lock_handle.fileno(), fcntl.LOCK_EX)
            try:
                yield
            finally:
                fcntl.flock(lock_handle.fileno(), fcntl.LOCK_UN)

    def _cache_marker_path(self, source_path: str, row: dict, device_key: str) -> str:
        try:
            stat = os.stat(source_path)
            fingerprint = f"{source_path}|{int(stat.st_mtime)}|{int(stat.st_size)}"
        except OSError:
            fingerprint = source_path
        material = "|".join(
            [
                fingerprint,
                str(row.get("file_hash") or ""),
                str(device_key or "device"),
                RVP_PROFILE_KEY,
                self._profile,
                str(self._target_width),
                str(self._target_height),
                str(self._target_fps),
                str(self._sample_rate),
            ]
        )
        digest = hashlib.sha256(material.encode("utf-8")).hexdigest()[:16]
        stem = self._safe_stem(Path(source_path).stem)
        device_dir = os.path.join(self._cache_root, str(device_key or "device"))
        return os.path.join(device_dir, f"{stem}-{digest}.rvp")

    @staticmethod
    def _safe_stem(value: str) -> str:
        text = "".join(ch if ch.isalnum() or ch in {" ", ".", "_", "-"} else "_" for ch in str(value or "video"))
        return (text.strip()[:64] or "video").rstrip(". ")

    @staticmethod
    def _to_even(value: int) -> int:
        if value % 2 == 1:
            value -= 1
        if value < 2:
            return 2
        return value

    @classmethod
    def _fit_dimensions(
        cls,
        source_width: int | None,
        source_height: int | None,
        max_width: int = RVP_WIDTH,
        max_height: int = RVP_HEIGHT,
    ) -> tuple[int, int]:
        if not source_width or not source_height or source_width <= 0 or source_height <= 0:
            return (max_width, max_height)

        if max_width <= 0:
            max_width = RVP_WIDTH
        if max_height <= 0:
            max_height = RVP_HEIGHT

        return (cls._to_even(max_width), cls._to_even(max_height))

    @staticmethod
    def _ffprobe_bin(ffmpeg_bin: str) -> str:
        ffprobe = ""
        ffmpeg_dir = os.path.dirname(ffmpeg_bin)
        if ffmpeg_dir:
            candidate = os.path.join(ffmpeg_dir, "ffprobe")
            if os.path.isfile(candidate):
                ffprobe = candidate
        if not ffprobe and shutil.which("ffprobe"):
            ffprobe = shutil.which("ffprobe") or ""
        return ffprobe

    def _probe_video_info(self, ffmpeg_bin: str, source_path: str):
        ffprobe_bin = self._ffprobe_bin(ffmpeg_bin)
        if not ffprobe_bin:
            return (None, None, None)

        command = [
            ffprobe_bin,
            "-v",
            "error",
            "-select_streams",
            "v:0",
            "-show_entries",
            "stream=width,height,duration:format=duration",
            "-of",
            "json",
            source_path,
        ]
        try:
            result = self._command_runner.run(command, cwd=os.path.dirname(source_path) or os.getcwd())
        except OSError:
            return (None, None, None)

        if result.returncode != 0:
            return (None, None, None)

        try:
            payload = json.loads(result.stdout or "")
            stream = payload.get("streams") or []
            if isinstance(stream, list) and stream:
                first = stream[0]
                duration = self._positive_float(
                    (payload.get("format") or {}).get("duration")
                )
                if duration is None:
                    duration = self._positive_float(first.get("duration"))
                return (
                    int(first.get("width")),
                    int(first.get("height")),
                    duration,
                )
        except Exception:
            return (None, None, None)
        return (None, None, None)

    def _probe_video_dimensions(self, ffmpeg_bin: str, source_path: str):
        width, height, _duration = self._probe_video_info(
            ffmpeg_bin, source_path
        )
        return width, height

    @staticmethod
    def _positive_float(value):
        try:
            parsed = float(value)
        except (TypeError, ValueError):
            return None
        return parsed if math.isfinite(parsed) and parsed > 0 else None

    def _estimated_bundle_bytes(
        self, duration: float, width: int, height: int
    ) -> int:
        frames = int(math.ceil(duration * self._target_fps))
        video_bytes = frames * width * height * 3 // 2
        audio_bytes = int(
            math.ceil(
                duration
                * self._sample_rate
                * RVP_CHANNELS
                * RVP_BYTES_PER_SAMPLE
            )
        )
        return video_bytes + audio_bytes

    def _check_cache_space(
        self,
        marker_path: str,
        row: dict,
        width: int,
        height: int,
        source_duration=None,
    ):
        duration = self._positive_float(source_duration)
        if duration is None:
            duration = self._positive_float(row.get("duration"))
        if duration is None:
            return

        estimated_bytes = self._estimated_bundle_bytes(
            duration, width, height
        )
        estimated_video_bytes = int(
            math.ceil(duration * self._target_fps)
        ) * width * height * 3 // 2
        estimated_audio_bytes = int(
            math.ceil(
                duration
                * self._sample_rate
                * RVP_CHANNELS
                * RVP_BYTES_PER_SAMPLE
            )
        )
        peak_bytes = estimated_bytes
        if (
            estimated_video_bytes > RVP_SEGMENT_YUV_LIMIT
            or estimated_audio_bytes > RVP_SEGMENT_PCM_LIMIT
        ):
            peak_bytes += estimated_bytes
        required_bytes = peak_bytes + RVP_CACHE_SPACE_RESERVE

        cache_dir = os.path.dirname(marker_path) or self._cache_root
        os.makedirs(cache_dir, exist_ok=True)
        try:
            free_bytes = shutil.disk_usage(cache_dir).free
        except OSError:
            return
        if free_bytes >= required_bytes:
            return

        gib = 1024.0 * 1024.0 * 1024.0
        raise RuntimeError(
            "not enough local cache space for the "
            f"{self._profile} video profile "
            f"(about {required_bytes / gib:.1f} GiB needed, "
            f"{free_bytes / gib:.1f} GiB free)"
        )

    def _ensure_bundle(
        self,
        ffmpeg_bin: str,
        source_path: str,
        marker_path: str,
        yuv_path: str,
        pcm_path: str,
        row: dict,
        video_width: int,
        video_height: int,
        source_duration=None,
    ):
        if self._is_cached_bundle_valid(marker_path, yuv_path, pcm_path, video_width, video_height):
            return

        os.makedirs(os.path.dirname(marker_path), exist_ok=True)
        tmp_yuv = yuv_path + ".tmp"
        tmp_pcm = pcm_path + ".tmp"
        tmp_marker = marker_path + ".tmp"
        for path in (tmp_yuv, tmp_pcm, tmp_marker):
            if os.path.exists(path):
                os.remove(path)

        self._check_cache_space(
            marker_path,
            row,
            video_width,
            video_height,
            source_duration=source_duration,
        )

        try:
            self._run_ffmpeg(
                self._video_command(
                    ffmpeg_bin,
                    source_path,
                    tmp_yuv,
                    width=video_width,
                    height=video_height,
                    fps=self._target_fps,
                ),
                marker_path,
                "video conversion failed",
            )
            audio_ok = self._run_ffmpeg(
                self._audio_command(
                    ffmpeg_bin,
                    source_path,
                    tmp_pcm,
                    sample_rate=self._sample_rate,
                ),
                marker_path,
                "audio conversion failed",
                raise_on_error=False,
            )
            if (
                not audio_ok
                or not os.path.isfile(tmp_pcm)
                or os.path.getsize(tmp_pcm) <= 0
            ):
                self._write_silence_pcm(
                    tmp_pcm,
                    row,
                    tmp_yuv,
                    width=video_width,
                    height=video_height,
                    fps=self._target_fps,
                    sample_rate=self._sample_rate,
                )

            with open(tmp_marker, "w", encoding="utf-8", newline="\n") as handle:
                handle.write(self.marker_text(
                    os.path.basename(marker_path),
                    width=video_width,
                    height=video_height,
                    fps=self._target_fps,
                    sample_rate=self._sample_rate,
                ))

            os.replace(tmp_yuv, yuv_path)
            os.replace(tmp_pcm, pcm_path)
            os.replace(tmp_marker, marker_path)
        except Exception:
            for path in (tmp_yuv, tmp_pcm, tmp_marker):
                try:
                    os.remove(path)
                except FileNotFoundError:
                    pass
            raise
        logger.info("Prepared iPod RVP video bundle: %s -> %s", source_path, marker_path)

    @staticmethod
    def _parse_int(value) -> int | None:
        try:
            parsed = int(str(value).strip())
        except (TypeError, ValueError):
            return None
        return parsed

    def _is_cached_bundle_valid(self, marker_path: str, yuv_path: str, pcm_path: str, width: int, height: int) -> bool:
        for path in (marker_path, yuv_path, pcm_path):
            if not os.path.isfile(path) or os.path.getsize(path) <= 0:
                return False

        meta = self._read_marker_meta(marker_path)
        if not meta:
            return False

        if self._parse_int(meta.get("width")) != int(width):
            return False
        if self._parse_int(meta.get("height")) != int(height):
            return False
        if self._parse_int(meta.get("fps")) != int(self._target_fps):
            return False
        if self._parse_int(meta.get("sample_rate")) != int(self._sample_rate):
            return False
        if self._parse_int(meta.get("channels")) != RVP_CHANNELS:
            return False
        return True

    @staticmethod
    def _read_marker_meta(marker_path: str) -> dict[str, str]:
        if not os.path.isfile(marker_path):
            return {}
        meta = {}
        with open(marker_path, "r", encoding="utf-8") as handle:
            for raw_line in handle:
                line = raw_line.strip()
                if not line or "=" not in line:
                    continue
                key, value = line.split("=", 1)
                meta[key.strip()] = value.strip()
        return meta

    def _ensure_segments(self, marker_path: str, yuv_path: str, pcm_path: str, width: int, height: int):
        yuv_size = os.path.getsize(yuv_path)
        pcm_size = os.path.getsize(pcm_path)
        if yuv_size <= RVP_SEGMENT_YUV_LIMIT and pcm_size <= RVP_SEGMENT_PCM_LIMIT:
            self._rewrite_single_marker(
                marker_path,
                width=width,
                height=height,
                fps=self._target_fps,
                sample_rate=self._sample_rate,
            )
            self._remove_stale_segments(marker_path)
            return []
        frame_size = max(1, width * height * 3 // 2)
        if yuv_size % frame_size:
            raise RuntimeError("raw video frame stream has an invalid size")

        segment_frames = max(1, self._target_fps * RVP_SEGMENT_SECONDS)
        frame_bytes = frame_size
        frames_total = yuv_size // frame_bytes
        base = os.path.splitext(marker_path)[0]
        part_count = (frames_total + segment_frames - 1) // segment_frames

        expected = []
        for index in range(1, part_count + 1):
            part_base = f"{base}.seg{index:02d}"
            expected.append({"yuv": part_base + ".yuv", "pcm": part_base + ".pcm"})
        expected_paths = {path for segment in expected for path in (segment["yuv"], segment["pcm"])}
        if all(os.path.isfile(p["yuv"]) and os.path.isfile(p["pcm"]) for p in expected):
            self._remove_stale_segments(marker_path, keep_paths=expected_paths)
            self._write_segmented_marker(
                marker_path,
                expected,
                width=width,
                height=height,
                fps=self._target_fps,
                sample_rate=self._sample_rate,
            )
            return expected

        segments = []
        with open(yuv_path, "rb") as yuv_in, open(pcm_path, "rb") as pcm_in:
            frame_start = 0
            for index, segment in enumerate(expected, start=1):
                part_frames = min(segment_frames, frames_total - frame_start)
                self._copy_exact(yuv_in, segment["yuv"], part_frames * frame_bytes)
                pcm_bytes = part_frames * self._pcm_bytes_per_frame
                self._copy_or_pad(pcm_in, segment["pcm"], pcm_bytes)
                segments.append(segment)
                frame_start += part_frames

        self._remove_stale_segments(marker_path, keep_paths=expected_paths)
        self._write_segmented_marker(
            marker_path,
            segments,
            width=width,
            height=height,
            fps=self._target_fps,
            sample_rate=self._sample_rate,
        )
        return segments

    @staticmethod
    def _rewrite_single_marker(
        marker_path: str,
        width: int = RVP_WIDTH,
        height: int = RVP_HEIGHT,
        fps: int = RVP_FPS,
        sample_rate: int = RVP_SAMPLE_RATE,
    ):
        content = VideoRvpTranscoder.marker_text(
            os.path.basename(marker_path),
            width=width,
            height=height,
            fps=fps,
            sample_rate=sample_rate,
        )
        VideoRvpTranscoder._write_text_if_changed(marker_path, content)

    @staticmethod
    def _write_segmented_marker(
        marker_path: str,
        segments: list[dict],
        width: int = RVP_WIDTH,
        height: int = RVP_HEIGHT,
        fps: int = RVP_FPS,
        sample_rate: int = RVP_SAMPLE_RATE,
    ):
        names = [(os.path.basename(s["yuv"]), os.path.basename(s["pcm"])) for s in segments]
        content = VideoRvpTranscoder.segmented_marker_text(
            parts=names,
            width=width,
            height=height,
            fps=fps,
            sample_rate=sample_rate,
        )
        VideoRvpTranscoder._write_text_if_changed(marker_path, content)

    @staticmethod
    def _write_text_if_changed(path: str, content: str):
        try:
            with open(path, "r", encoding="utf-8") as handle:
                if handle.read() == content:
                    return
        except OSError:
            pass
        tmp_path = path + ".tmp"
        with open(tmp_path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(content)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(tmp_path, path)

    @staticmethod
    def _remove_stale_segments(marker_path: str, keep_paths=None):
        keep = set(keep_paths or [])
        parent = os.path.dirname(marker_path) or "."
        prefix = os.path.splitext(os.path.basename(marker_path))[0] + ".seg"
        try:
            names = os.listdir(parent)
        except OSError:
            return
        for name in names:
            if not name.startswith(prefix) or not name.lower().endswith((".yuv", ".pcm")):
                continue
            path = os.path.join(parent, name)
            if path in keep:
                continue
            try:
                os.remove(path)
            except OSError:
                pass

    @staticmethod
    def _copy_exact(source, target_path: str, byte_count: int):
        tmp_path = target_path + ".tmp"
        with open(tmp_path, "wb") as out:
            remaining = byte_count
            while remaining > 0:
                chunk = source.read(min(4 * 1024 * 1024, remaining))
                if not chunk:
                    raise RuntimeError("short read while segmenting raw video")
                out.write(chunk)
                remaining -= len(chunk)
        os.replace(tmp_path, target_path)

    @staticmethod
    def _copy_or_pad(source, target_path: str, byte_count: int):
        """Copy a fixed PCM duration, padding a damaged/short stream silently."""
        tmp_path = target_path + ".tmp"
        with open(tmp_path, "wb") as out:
            remaining = byte_count
            while remaining > 0:
                chunk = source.read(min(4 * 1024 * 1024, remaining))
                if not chunk:
                    zero_chunk = b"\0" * min(1024 * 1024, remaining)
                    while remaining > 0:
                        written = min(len(zero_chunk), remaining)
                        out.write(zero_chunk[:written])
                        remaining -= written
                    break
                out.write(chunk)
                remaining -= len(chunk)
        os.replace(tmp_path, target_path)

    def _run_ffmpeg(self, command, marker_path: str, label: str, raise_on_error=True) -> bool:
        try:
            result = self._command_runner.run(command, cwd=os.path.dirname(marker_path))
        except OSError as exc:
            if raise_on_error:
                raise RuntimeError(f"ffmpeg launch failed: {exc}") from exc
            return False
        if result.returncode == 0:
            return True
        if not raise_on_error:
            return False
        message = (
            result.stderr
            or result.stdout
            or getattr(result, "failure_message", lambda: "")()
            or f"ffmpeg exited with {result.returncode}"
        ).strip()
        raise RuntimeError(f"{label}: {message}")

    @staticmethod
    def _video_filter(width: int, height: int, fps: int = RVP_FPS) -> str:
        width = VideoRvpTranscoder._to_even(int(width or RVP_WIDTH))
        height = VideoRvpTranscoder._to_even(int(height or RVP_HEIGHT))
        return (
            f"scale={width}:{height}:force_original_aspect_ratio=decrease,"
            f"pad={width}:{height}:(ow-iw)/2:(oh-ih)/2:black,"
            f"fps={int(fps)}"
        )

    @staticmethod
    def _video_command(
        ffmpeg_bin: str,
        source_path: str,
        target_path: str,
        width: int = RVP_WIDTH,
        height: int = RVP_HEIGHT,
        fps: int = RVP_FPS,
    ):
        return [
            ffmpeg_bin,
            "-y",
            "-loglevel",
            "error",
            "-i",
            source_path,
            "-vf",
            VideoRvpTranscoder._video_filter(width, height, fps=fps),
            "-an",
            "-pix_fmt",
            "yuv420p",
            "-f",
            "rawvideo",
            target_path,
        ]

    @staticmethod
    def _audio_command(ffmpeg_bin: str, source_path: str, target_path: str, sample_rate: int = RVP_SAMPLE_RATE):
        return [
            ffmpeg_bin,
            "-y",
            "-loglevel",
            "error",
            "-i",
            source_path,
            "-vn",
            "-map",
            "0:a:0?",
            "-ac",
            str(RVP_CHANNELS),
            "-ar",
            str(sample_rate),
            "-f",
            "s16le",
            target_path,
        ]

    @staticmethod
    def _write_silence_pcm(
        target_path: str,
        row: dict,
        yuv_path: str,
        width: int,
        height: int,
        fps: int,
        sample_rate: int | None = None,
    ):
        duration = VideoRvpTranscoder._duration_seconds(
            row,
            yuv_path,
            width=width,
            height=height,
            fps=fps,
        )
        sample_rate = sample_rate if sample_rate is not None else row.get("sync_video_sample_rate")
        sample_rate = int(sample_rate or RVP_SAMPLE_RATE)
        byte_count = int(duration * sample_rate * RVP_CHANNELS * RVP_BYTES_PER_SAMPLE)
        with open(target_path, "wb") as handle:
            min_bytes = sample_rate * RVP_CHANNELS * RVP_BYTES_PER_SAMPLE
            remaining = max(byte_count, min_bytes)
            chunk = b"\0" * min(remaining, 1024 * 1024)
            while remaining > 0:
                written = min(remaining, len(chunk))
                handle.write(chunk[:written])
                remaining -= written

    @staticmethod
    def _duration_seconds(row: dict, yuv_path: str, width: int = RVP_WIDTH, height: int = RVP_HEIGHT, fps: int = RVP_FPS) -> float:
        try:
            duration = float(row.get("duration") or 0.0)
        except (TypeError, ValueError):
            duration = 0.0
        if duration > 0:
            return duration
        try:
            frame_size = int(width) * int(height) * 3 // 2
            frames = os.path.getsize(yuv_path) // frame_size
            return max(float(frames) / float(fps or RVP_FPS), 1.0)
        except OSError:
            return 1.0

    @staticmethod
    def marker_text(
        marker_name: str,
        width: int = RVP_WIDTH,
        height: int = RVP_HEIGHT,
        fps: int = RVP_FPS,
        sample_rate: int = RVP_SAMPLE_RATE,
    ) -> str:
        stem = Path(marker_name).stem
        return (
            "ROCKPOD_RAW_VIDEO_V1\n"
            f"width={max(1, int(width))}\n"
            f"height={max(1, int(height))}\n"
            f"fps={int(fps)}\n"
            f"sample_rate={int(sample_rate)}\n"
            f"channels={RVP_CHANNELS}\n"
            "fit=contain\n"
            f"video={stem}.yuv\n"
            f"audio={stem}.pcm\n"
        )

    @staticmethod
    def segmented_marker_text(
        parts: list[tuple[str, str]],
        width: int = RVP_WIDTH,
        height: int = RVP_HEIGHT,
        fps: int = RVP_FPS,
        sample_rate: int = RVP_SAMPLE_RATE,
    ) -> str:
        lines = [
            "ROCKPOD_RAW_VIDEO_V1",
            f"width={max(1, int(width))}",
            f"height={max(1, int(height))}",
            f"fps={int(fps)}",
            f"sample_rate={int(sample_rate)}",
            f"channels={RVP_CHANNELS}",
            "fit=contain",
            f"segments={len(parts)}",
        ]
        for index, (video_name, audio_name) in enumerate(parts, start=1):
            lines.append(f"segment{index}_video={video_name}")
            lines.append(f"segment{index}_audio={audio_name}")
        return "\n".join(lines) + "\n"

    @staticmethod
    def _bundle_hash(*paths: str) -> str:
        h = hashlib.sha256()
        for path in paths:
            h.update(os.path.basename(path).encode("utf-8", errors="surrogateescape"))
            h.update(b"\0")
            with open(path, "rb") as handle:
                while True:
                    chunk = handle.read(1024 * 1024)
                    if not chunk:
                        break
                    h.update(chunk)
        return h.hexdigest()

    @classmethod
    def _bundle_hash_cached(cls, marker_path: str, *paths: str) -> str:
        """Reuse a bundle digest only while every component stat is unchanged."""
        digest_path = marker_path + ".digest.json"
        fingerprint = []
        for path in paths:
            stat = os.stat(path)
            fingerprint.append(
                {
                    "path": os.path.abspath(path),
                    "size": int(stat.st_size),
                    "mtime_ns": int(stat.st_mtime_ns),
                }
            )
        try:
            with open(digest_path, "r", encoding="utf-8") as handle:
                cached = json.load(handle)
            if cached.get("version") == 1 and cached.get("files") == fingerprint:
                digest = str(cached.get("sha256") or "")
                if len(digest) == 64:
                    return digest
        except (OSError, ValueError, TypeError):
            pass

        digest = cls._bundle_hash(*paths)
        payload = {
            "version": 1,
            "sha256": digest,
            "files": fingerprint,
        }
        tmp_path = digest_path + ".tmp"
        try:
            with open(tmp_path, "w", encoding="utf-8", newline="\n") as handle:
                json.dump(payload, handle, sort_keys=True, separators=(",", ":"))
                handle.write("\n")
                handle.flush()
                os.fsync(handle.fileno())
            os.replace(tmp_path, digest_path)
        except OSError:
            try:
                os.remove(tmp_path)
            except OSError:
                pass
        return digest
