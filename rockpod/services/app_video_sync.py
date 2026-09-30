"""Shared MPEG/Apple-exact staging for RockPod app video libraries."""

from __future__ import annotations

import os
import subprocess
from pathlib import Path

from services.video_rvp import VideoRvpTranscoder
from services.video_capabilities import find_device_mount


MPEG_PROFILE = "quality"
H264_PROFILE = "h264_apple_exact"
MPEG_ONLY_APPS = frozenset({"livetv", "tiktok", "onlyfans"})


def app_video_profile(config, requested=None, *, source_app=""):
    if str(source_app or "").strip().lower() in MPEG_ONLY_APPS:
        return MPEG_PROFILE
    value = requested
    if value is None:
        value = config.get("video_sync_profile", MPEG_PROFILE)
    normalized = VideoRvpTranscoder._normalize_profile(value)
    return H264_PROFILE if normalized == H264_PROFILE else MPEG_PROFILE


def app_video_extension(profile):
    return ".m4v" if app_video_profile({}, profile) == H264_PROFILE else ".mpg"


def device_video_target(mount_path):
    """Read the Rockbox target without importing the Qt device detector."""
    info_path = Path(mount_path) / ".rockbox" / "rockbox-info.txt"
    try:
        lines = info_path.read_text(
            encoding="utf-8", errors="replace"
        ).splitlines()
    except OSError:
        return ""
    for line in lines:
        label, separator, value = line.partition(":")
        if separator and label.strip().lower() == "target":
            return value.strip().lower()
    return ""


def app_video_signature(
    profile, source_signature, pipeline, device_target=""
):
    target = str(device_target or "").strip().lower() or "generic"
    return (
        f"{pipeline}:{app_video_profile({}, profile)}:{target}:"
        f"{source_signature}"
    )


def stage_app_video(
    source_path,
    target_path,
    *,
    config,
    profile,
    cache_namespace,
    device_key,
    title="",
    artist="",
    duration=0.0,
    mpeg_filter="",
    device_target="",
    require_audio=False,
    quality="tv",
    cache_root=None,
):
    """Write one complete staged file in the explicitly selected format."""
    normalized = app_video_profile(config, profile)
    source = os.path.abspath(str(source_path))
    target = os.path.abspath(str(target_path))
    from services.video_pipeline import profile_string
    from services.video_sync_transaction import verified_copy
    cache_root = cache_root or os.path.join(
        str(config.get("cache_dir") or Path.home() / ".rockpod" / "cache"),
        cache_namespace,
    )
    if quality not in {"tv", "space"}:
        raise ValueError("Choose Standard or Space saver video quality")
    transcoder = VideoRvpTranscoder(cache_root,
        ffmpeg_path=config.get("ffmpeg_binary", "ffmpeg"),
        profile=profile_string({'format': 'h264' if normalized == H264_PROFILE else 'mpeg',
                                'quality': quality}))
    prepared, info = transcoder.prepare_track_for_sync(
        {'file_path': source, 'title': title or Path(source).stem,
         'artist': artist, 'duration': float(duration or 0), 'media_type': 'video'},
        str(device_key), device_target=str(device_target or ''),
        device_mount=find_device_mount(target) or None)
    if require_audio and not info['validation'].get('audio'):
        raise RuntimeError('The selected app video has no playable audio track.')
    # App-specific framing belongs to the player. The shared rendition stays
    # unpadded and preserves source aspect for either TV Screen setting.
    verified_copy(prepared['sync_source_path'], target, prepared['file_hash'])
    return normalized


def _has_audio_stream(path, ffprobe="ffprobe"):
    try:
        result = subprocess.run(
            [
                str(ffprobe), "-v", "error", "-select_streams", "a:0",
                "-show_entries", "stream=codec_type",
                "-of", "default=noprint_wrappers=1:nokey=1", str(path),
            ],
            check=False,
            capture_output=True,
            text=True,
            timeout=30,
        )
        return result.returncode == 0 and "audio" in result.stdout.split()
    except (OSError, subprocess.SubprocessError):
        return False


def remove_alternate_video(target_path):
    target = Path(target_path)
    alternate = target.with_suffix(".mpg" if target.suffix.lower() == ".m4v" else ".m4v")
    for candidate in (
        alternate,
        Path(str(alternate) + ".source"),
        alternate.with_suffix(".source"),
    ):
        try:
            candidate.unlink()
        except FileNotFoundError:
            pass
