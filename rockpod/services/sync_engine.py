"""Sync engine — copies tracks to device with metadata change detection.

Handles:
  - Copying new/missing tracks to device
  - Re-copying tracks whose metadata or file content has changed
  - Building the device folder layout
  - Force-full-resync mode
  - Cancellation and progress reporting
"""

import csv
import hashlib
import logging
import os
import re
import shutil
import time
import unicodedata
from collections import defaultdict
from datetime import datetime
from pathlib import Path

from PySide6.QtCore import QObject, Signal, QThread, Slot, Qt

from app.database import Database
from services.audio_transcode import AudioSyncTranscoder
from services.rockbox_device import detect_rockbox_database_state
from services.metadata_reader import read_metadata, compute_file_hash, CODEC_MAP, VIDEO_EXTENSIONS
from services.path_safety import resolve_under_root
from services.reconciliation import summarize_unmatched_tracks
from services.smart_playlists import evaluate_playlist
from services.track_matcher import TrackMatcher
from services.device_inventory import device_music_roots, device_record_from_info, verify_device_inventory
from services.video_thumbnails import VideoThumbnailService
from services.video_rvp import VideoRvpTranscoder
from services.rockbox_wps_art import wps_album_art_sizes_for_config
from services.weather import build_weather_bundle
from models.track import compute_metadata_hash

logger = logging.getLogger(__name__)
COPY_CHUNK_SIZE = 4 * 1024 * 1024
LEGACY_COPY_MODE = os.environ.get("ROCKPOD_SYNC_LEGACY_COPY", "").strip().lower() in {"1", "true", "yes", "on"}
AUDIO_EXTENSIONS = {ext for ext in CODEC_MAP.keys() if ext not in VIDEO_EXTENSIONS}
ALBUM_LIST_DEVICE_DIR = os.path.join(".rockbox", "albumlist")
ALBUM_LIST_THUMB_DEVICE_DIR = os.path.join(ALBUM_LIST_DEVICE_DIR, "thumbs")
ALBUM_LIST_SLIDE_DEVICE_DIR = os.path.join(ALBUM_LIST_DEVICE_DIR, "slides")
VIDEO_LIST_DEVICE_DIR = os.path.join(".rockbox", "videolist")
VIDEO_LIST_THUMB_DEVICE_DIR = os.path.join(VIDEO_LIST_DEVICE_DIR, "thumbs")
VIDEO_LIST_PREVIEW_DEVICE_DIR = os.path.join(VIDEO_LIST_DEVICE_DIR, "previews")
VIDEO_LIST_NETFLIX_POSTER_DEVICE_DIR = os.path.join(VIDEO_LIST_DEVICE_DIR, "netflix")
VIDEO_LIST_NETFLIX_LANDING_DEVICE_DIR = os.path.join(VIDEO_LIST_DEVICE_DIR, "netflix-landing")
VIDEO_LIST_NETFLIX_DETAIL_DEVICE_DIR = os.path.join(VIDEO_LIST_DEVICE_DIR, "netflix-detail")
SYNC_TEMP_SUFFIX = ".rockpod_tmp"
MAX_AUTO_DUPLICATE_DELETE_COUNT = 50
AUDIO_TRANSCODE_CACHE_EXTENSIONS = {".mp3", ".m4a"}
VIDEO_SYNC_CACHE_EXTENSIONS = {
    ".rvp", ".yuv", ".pcm", ".mpg", ".json", ".tmp"
}
MANIFEST_ARTWORK_RELPATHS = {
    os.path.join(ALBUM_LIST_DEVICE_DIR, "index.tsv"),
    os.path.join(VIDEO_LIST_DEVICE_DIR, "index.tsv"),
    os.path.join(VIDEO_LIST_DEVICE_DIR, "locked.pin"),
}


class _SyncCancelled(Exception):
    """Internal control flow for cooperative sync cancellation."""


def _sanitize_filename(name, max_len=200):
    """Make a string safe for use as a filename on FAT32."""
    if not name:
        return "Unknown"
    # Normalize unicode
    name = unicodedata.normalize("NFC", name)
    # Remove or replace forbidden chars
    name = re.sub(r'[<>:"/\\|?*]', "_", name)
    # Collapse whitespace
    name = re.sub(r"\s+", " ", name).strip()
    # Trim length
    if len(name) > max_len:
        name = name[:max_len].rstrip()
    return name or "Unknown"


def _bump_reason(reason_counts, reason):
    if not reason:
        return
    reason_counts[reason] = reason_counts.get(reason, 0) + 1


def _looks_like_auto_duplicate_path(rel_path):
    """Return whether a device path looks like a RockPod-created duplicate copy."""
    name = os.path.basename(str(rel_path or ""))
    stem, _ext = os.path.splitext(name)
    suffix_match = re.search(r"\s\((\d+)\)$", stem)
    if suffix_match is not None and int(suffix_match.group(1)) >= 2:
        return True

    normalized = str(rel_path or "").replace("\\", "/").casefold()
    normalized = normalized.replace("\u2018", "'").replace("\u2019", "'").replace("\u02bc", "'")
    # Older Rani imports generated an album-level directory literal "Rani's playlist";
    # those entries are legacy duplicates that can be safely auto-cleaned.
    return bool(re.search(r"/rani's playlist(?=/|$)", normalized))


def _sync_update_reasons(row, device_row=None, old_dev_path="", new_rel_path=""):
    local = dict(row) if hasattr(row, "keys") else dict(row or {})
    device = dict(device_row) if hasattr(device_row, "keys") else dict(device_row or {})
    reasons = []

    if local.get("sync_transcoded"):
        reasons.append("conversion required")

    local_mh = str(local.get("metadata_hash") or "").strip()
    device_mh = str(device.get("metadata_hash") or "").strip()
    last_synced_mh = str(
        device.get("last_synced_metadata_hash")
        or device_mh
        or local.get("last_synced_metadata_hash")
        or ""
    ).strip()
    if (local_mh and device_mh and local_mh != device_mh) or (
        local_mh and last_synced_mh and local_mh != last_synced_mh
    ):
        reasons.append("metadata changed")

    local_fh = str(local.get("file_hash") or "").strip()
    device_fh = str(device.get("file_hash") or "").strip()
    last_synced_fh = str(
        device.get("last_synced_file_hash")
        or device_fh
        or local.get("last_synced_file_hash")
        or ""
    ).strip()
    if (local_fh and device_fh and local_fh != device_fh) or (
        local_fh and last_synced_fh and local_fh != last_synced_fh
    ):
        reasons.append("file changed")

    if old_dev_path and new_rel_path and old_dev_path != new_rel_path:
        reasons.append("path changed")

    return reasons


def _dedupe_normalize(value):
    if not value:
        return ""
    text = unicodedata.normalize("NFKC", str(value)).strip().casefold()
    text = re.sub(r"\s+", " ", text)
    return text


def _dedupe_artist_key(row):
    artist = _dedupe_normalize(row.get("artist", ""))
    album_artist = _dedupe_normalize(row.get("album_artist", ""))
    keys = sorted({item for item in (artist, album_artist) if item})
    return keys[0] if keys else ""


def _dedupe_duration(value):
    try:
        return round(float(value or 0))
    except (TypeError, ValueError):
        return 0


def _dedupe_group_key(row):
    item = dict(row) if hasattr(row, "keys") else row
    title = _dedupe_normalize(item.get("title", ""))
    artist = _dedupe_artist_key(item)
    album = _dedupe_normalize(item.get("album", ""))
    if title and artist and album:
        return ("identity", artist, album, title)
    return None


def _managed_playlist_media_track(row):
    item = dict(row) if hasattr(row, "keys") else row
    normalized = str(item.get("file_path") or "").replace("\\", "/").casefold()
    return "/playlists/rockpod media/" in normalized


def _sync_duplicate_key(row):
    item = dict(row) if hasattr(row, "keys") else row
    identity = _dedupe_group_key(item)
    if identity is None:
        return None
    try:
        track_number = int(item.get("track_number") or 0)
    except (TypeError, ValueError):
        track_number = 0
    try:
        disc_number = int(item.get("disc_number") or 1)
    except (TypeError, ValueError):
        disc_number = 1
    return identity + (
        track_number,
        disc_number,
        _dedupe_duration(item.get("duration")),
    )


def _collapse_managed_playlist_duplicates(rows):
    """Prefer regular library files over duplicate managed playlist imports."""
    grouped = defaultdict(list)
    passthrough = []
    for row in rows:
        key = _sync_duplicate_key(row)
        if key is None:
            passthrough.append(row)
        else:
            grouped[key].append(row)

    collapsed = list(passthrough)
    for group in grouped.values():
        regular = [row for row in group if not _managed_playlist_media_track(row)]
        collapsed.extend(regular if regular else group)
    return collapsed


def _audio_codec_rank(row):
    item = dict(row) if hasattr(row, "keys") else row
    codec = str(item.get("codec") or "").strip().casefold()
    return {
        "flac": 5,
        "alac": 5,
        "wav": 5,
        "aiff": 5,
        "ape": 5,
        "wv": 5,
        "opus": 4,
        "ogg": 3,
        "vorbis": 3,
        "aac": 2,
        "m4a": 2,
        "mp3": 1,
        "wma": 1,
    }.get(codec, 0)


def _video_episode_label(row):
    item = dict(row) if hasattr(row, "keys") else dict(row or {})
    title = _sanitize_filename(item.get("title") or Path(str(item.get("file_path") or "")).stem or "Episode")
    title = title.rstrip(" ._-") or "Episode"
    season = _to_video_int(item.get("season_number") or 0) or 0
    episode = _to_video_int(item.get("episode_number") or item.get("track_number") or 0) or 0
    if season > 0 and episode > 0:
        return f"S{season:02d}E{episode:02d} - {title}"
    if episode > 0:
        return f"{episode:02d} - {title}"
    return title


def _clean_inferred_show_title(value):
    text = Path(str(value or "")).stem.replace("_", " ")
    text = re.sub(r"\s+", " ", text).strip(" ._-")
    text = re.sub(r"\s+FULL\s+EPISODE\b.*$", "", text, flags=re.IGNORECASE).strip(" ._-")
    text = re.sub(r"\s+RETRO\s+RERUN\b.*$", "", text, flags=re.IGNORECASE).strip(" ._-")
    return text


def _infer_video_episode_metadata(row):
    item = dict(row) if hasattr(row, "keys") else dict(row or {})
    candidates = [
        item.get("title"),
        Path(str(item.get("file_path") or "")).stem,
    ]
    for candidate in candidates:
        text = str(candidate or "").replace("_", " ")
        text = re.sub(r"\s+", " ", text).strip()
        if not text:
            continue

        match = re.match(
            r"^(.+?)\s+S(\d{1,2})E(\d{1,3})(?:\s*[-:]\s*(.+))?$",
            text,
            flags=re.IGNORECASE,
        )
        if match:
            title = (match.group(4) or item.get("title") or text).strip()
            return {
                "show_title": _clean_inferred_show_title(match.group(1)),
                "season_number": int(match.group(2)),
                "episode_number": int(match.group(3)),
                "title": title,
            }

        match = re.match(
            r"^(.+?)\s*[-:]\s*(?:season|series)\s*(\d{1,2})\s*[-: ]+"
            r"(?:episode|ep)\s*(\d{1,3})(?:\s*[-:]\s*(.+))?$",
            text,
            flags=re.IGNORECASE,
        )
        if match:
            title = (match.group(4) or item.get("title") or text).strip()
            return {
                "show_title": _clean_inferred_show_title(match.group(1)),
                "season_number": int(match.group(2)),
                "episode_number": int(match.group(3)),
                "title": title,
            }

        match = re.match(
            r"^(?:episode|ep)\s*(\d{1,4})\s*[-:]\s*(.+)$",
            text,
            flags=re.IGNORECASE,
        )
        if match:
            show_title = _clean_inferred_show_title(match.group(2))
            if show_title:
                return {
                    "show_title": show_title,
                    "season_number": 1,
                    "episode_number": int(match.group(1)),
                }

        match = re.match(
            r"^(.+?)\s*[-–—:]\s*(.+)$",
            text,
            flags=re.IGNORECASE,
        )
        if match:
            left = _clean_inferred_show_title(match.group(1)).strip()
            right = _clean_inferred_show_title(match.group(2)).strip()
            left = re.sub(r"\s*\([^)]*\)\s*$", "", left).strip()
            right = re.sub(r"\s*\([^)]*\)\s*$", "", right).strip()
            if re.search(r"\bvs\.?\b", left, flags=re.IGNORECASE):
                show_title = left
                title = right
            elif re.search(r"\bvs\.?\b", right, flags=re.IGNORECASE):
                show_title = right
                title = left
            else:
                continue
            title = title or item.get("title") or text
            title = _clean_inferred_show_title(title)
            if show_title and re.search(r"\bvs\.?\b", show_title, flags=re.IGNORECASE):
                return {
                    "show_title": show_title,
                    "title": title,
                    "video_kind": "show",
                }

    return {}


def _video_manifest_group_key(row):
    item = _normalize_video_show_fields(dict(row) if hasattr(row, "keys") else dict(row or {}))
    if bool(item.get("video_locked")):
        return "locked"
    kind = _normalize_video_kind_value(item.get("video_kind")) or "movie"
    if kind == "show":
        show = _clean_inferred_show_title(item.get("show_title"))
        if show:
            return "show:" + re.sub(r"[^a-z0-9]+", "-", show.casefold()).strip("-")
    if kind == "home_video":
        return "home_video"
    if kind == "music_video":
        return "music_video"
    title = _clean_inferred_show_title(item.get("title") or Path(str(item.get("file_path") or "")).stem)
    if title:
        return "movie:" + re.sub(r"[^a-z0-9]+", "-", title.casefold()).strip("-")
    return kind


def _recompute_row_metadata_hash(row):
    item = dict(row) if hasattr(row, "keys") else dict(row or {})
    item["metadata_hash"] = compute_metadata_hash(
        item.get("title", ""),
        item.get("artist", ""),
        item.get("album", ""),
        item.get("album_artist", ""),
        item.get("track_number"),
        item.get("disc_number", 1),
        item.get("genre", ""),
        item.get("year"),
        item.get("composer", ""),
        item.get("duration", 0.0),
        item.get("bitrate", 0),
        item.get("codec", ""),
        item.get("media_type", ""),
        item.get("video_kind", ""),
        item.get("show_title", ""),
        item.get("season_number"),
        item.get("episode_number"),
    )
    return item


def build_device_path(track_row, dir_template, file_template):
    """Build the target path on the device for a track.

    Returns a relative path like 'Music/Artist/Album/01 - Title.mp3'
    """
    d = dict(track_row) if hasattr(track_row, "keys") else track_row
    if str(d.get("media_type") or "audio").lower() == "video":
        d = _normalize_video_show_fields(d)
        normalized_kind = _normalize_video_kind_value(d.get("video_kind"))
        if normalized_kind:
            d["video_kind"] = normalized_kind

    ext = d.get("sync_output_ext") or Path(d.get("file_path", "")).suffix or ".mp3"
    tn = d.get("track_number")
    dn = d.get("disc_number", 1) or 1
    media_type = str(d.get("media_type") or "audio").lower()
    source_path = str(d.get("file_path") or "")
    source_ext = Path(source_path).suffix.lower()
    album_artist = _sanitize_filename(d.get("album_artist") or d.get("artist") or "Unknown Artist")
    album = _sanitize_filename(d.get("album") or "Unknown Album")
    title = _sanitize_filename(d.get("title") or "Unknown")
    show_title_raw = str(d.get("show_title") or "").strip()
    show_title = _sanitize_filename(show_title_raw) if show_title_raw else ""
    season_number = _to_video_int(d.get("season_number") or 0) or 0
    source_parts = [part.casefold() for part in Path(source_path).parts]
    is_downloaded_video = (
        media_type == "video"
        and source_ext in {".mpg", ".mpeg", ".mpe"}
        and "youtube" in source_parts
    )
    if media_type == "video" and _is_generic_downloaded_video_title(title):
        stem_title = Path(source_path).stem
        if stem_title and not _is_generic_downloaded_video_title(stem_title):
            title = _sanitize_filename(stem_title)
    if is_downloaded_video and _is_generic_downloaded_video_title(title):
        title = _sanitize_filename(Path(source_path).stem or "Downloaded Video")
    video_kind_label = {
        "movie": "Movies",
        "show": "TV Shows",
        "music_video": "Music Videos",
        "home_video": "Home Videos",
    }.get(str(d.get("video_kind") or "movie"), "Movies")
    video_sync_category = str(d.get("video_sync_category") or "").strip()
    if (
        is_downloaded_video
        and not show_title
        and _normalize_video_kind_value(d.get("video_kind")) == "movie"
    ):
        video_sync_category = video_sync_category or "Downloaded"
    if video_sync_category:
        video_kind_label = video_sync_category

    if media_type == "video" and bool(d.get("video_locked")):
        return os.path.join("Videos", ".Locked", f"{title}{ext}")

    if media_type == "video" and str(dir_template or "").startswith("Music/"):
        if _normalize_video_kind_value(d.get("video_kind")) == "show" and show_title:
            season_label = "Specials" if season_number == 0 else f"Season {season_number:02d}"
            return os.path.join(
                "Videos",
                "TV Shows",
                show_title,
                _sanitize_filename(season_label),
                f"{_video_episode_label(d)}{ext}",
            )
        dir_template = "Videos/{video_kind}"
        file_template = "{title}{ext}"

    try:
        track_num = int(tn) if tn else 0
    except (ValueError, TypeError):
        track_num = 0

    try:
        dir_path = dir_template.format(
            album_artist=album_artist,
            artist=_sanitize_filename(d.get("artist") or "Unknown Artist"),
            album=album,
            genre=_sanitize_filename(d.get("genre") or "Unknown"),
            video_kind=_sanitize_filename(video_kind_label),
            year=d.get("year") or "0000",
        )
    except (KeyError, ValueError):
        dir_path = f"Music/{album_artist}/{album}"

    try:
        filename = file_template.format(
            track_number=track_num,
            disc_number=dn,
            title=title,
            artist=_sanitize_filename(d.get("artist") or ""),
            ext=ext,
        )
    except (KeyError, ValueError):
        filename = f"{track_num:02d} - {title}{ext}"

    return os.path.join(dir_path, filename)


def _is_generic_downloaded_video_title(title):
    return str(title or "").strip().casefold() in {
        "",
        "unknown",
        "youtube",
        "mpeg video",
        "youtube mpeg video",
    }


def _to_video_int(value):
    """Coerce common episode/season-style metadata values into an integer."""
    if value is None:
        return None
    if isinstance(value, bool):
        return int(value)
    if isinstance(value, (int, float)):
        try:
            return int(value)
        except (TypeError, ValueError):
            return None
    text = str(value).strip()
    if not text:
        return None
    match = re.search(r"\d+", text)
    if not match:
        return None
    try:
        return int(match.group(0))
    except (TypeError, ValueError):
        return None


def _normalize_video_kind_value(value):
    """Normalize mixed-case/legacy video-kind values to canonical forms."""
    text = str(value or "").strip().lower().replace("-", " ").replace("_", " ")
    text = re.sub(r"\s+", " ", text)
    if not text:
        return ""
    if re.search(r"\b(tv\s*show|tvseries|tvshow|series|show|episode|tv)\b", text):
        return "show"
    if re.search(r"\b(home video|home videos|home_video|homevideo|camcorder|family|personal)\b", text):
        return "home_video"
    if re.search(r"\b(music video|music videos|musicvideo)\b", text):
        return "music_video"
    if re.search(r"\b(movie|film)\b", text):
        return "movie"
    return text


def _pick_first_field(row, keys):
    for key in keys:
        value = row.get(key)
        if value is None:
            continue
        text = str(value).strip()
        if text:
            return text
    return ""


def _normalize_video_show_fields(row):
    if not isinstance(row, dict):
        return row

    normalized = dict(row)
    kind = _normalize_video_kind_value(
        normalized.get("video_kind") or normalized.get("type") or normalized.get("kind")
    )
    if kind:
        normalized["video_kind"] = kind

    if not normalized.get("show_title"):
        normalized["show_title"] = _pick_first_field(
            normalized,
            [
                "show_title",
                "show",
                "show_name",
                "series_name",
                "tv_show",
                "series",
                "tvshow",
                "tv series",
            ],
        )

    if not normalized.get("season_number"):
        parsed = _to_video_int(
            _pick_first_field(
                normalized,
                [
                    "season_number",
                    "season",
                    "season_num",
                    "tv_season",
                    "tvseason",
                    "season_number_text",
                    "series_num",
                    "series_number",
                ],
            )
        )
        if parsed is not None:
            normalized["season_number"] = parsed

    if not normalized.get("episode_number"):
        parsed = _to_video_int(
            _pick_first_field(
                normalized,
                [
                    "episode_number",
                    "episode",
                    "episode_num",
                    "tv_episode",
                    "ep",
                    "ep_number",
                    "episode_number_text",
                    "episode_id",
                    "episodeid",
                ],
            )
        )
        if parsed is not None:
            normalized["episode_number"] = parsed

    inferred = _infer_video_episode_metadata(normalized)
    if inferred:
        if inferred.get("show_title") and not str(normalized.get("show_title") or "").strip():
            normalized["show_title"] = inferred["show_title"]
        if inferred.get("season_number") is not None and not normalized.get("season_number"):
            normalized["season_number"] = inferred["season_number"]
        if inferred.get("episode_number") is not None and not normalized.get("episode_number"):
            normalized["episode_number"] = inferred["episode_number"]
        if inferred.get("title") and _is_generic_downloaded_video_title(normalized.get("title")):
            normalized["title"] = inferred["title"]
        inferred_kind = _normalize_video_kind_value(inferred.get("video_kind"))
        if inferred_kind == "show" and kind != "home_video":
            kind = inferred_kind
            normalized["video_kind"] = kind

    if (not normalized.get("track_number")) and normalized.get("episode_number") is not None:
        normalized["track_number"] = normalized.get("episode_number")

    if (
        normalized.get("artist") == ""
        and normalized.get("album_artist") == ""
        and normalized.get("show_title")
    ):
        normalized["artist"] = normalized["show_title"]
        normalized["album_artist"] = normalized["show_title"]

    inferred_season = _to_video_int(normalized.get("season_number"))
    inferred_episode = _to_video_int(normalized.get("episode_number"))
    final_kind = _normalize_video_kind_value(normalized.get("video_kind"))
    if normalized.get("show_title") and (
        final_kind == "show" or inferred_season is not None or inferred_episode is not None
    ):
        normalized["video_kind"] = "show"
        if inferred_season is None and (
            normalized.get("season_number") is None or normalized.get("season_number") == ""
        ):
            source_parts = [part.casefold() for part in Path(str(normalized.get("file_path") or "")).parts]
            if not any(part in {"special", "specials"} for part in source_parts):
                normalized["season_number"] = 1

    return normalized


_VIDEO_EPISODE_OVERRIDES = {
    "disney's recess - randall's friends": {
        "show_title": "Recess",
        "title": "Randall's Friends",
        "season_number": 4,
        "episode_number": 12,
    },
    "recess - randall's friends": {
        "show_title": "Recess",
        "title": "Randall's Friends",
        "season_number": 4,
        "episode_number": 12,
    },
    "randall's friends": {
        "show_title": "Recess",
        "title": "Randall's Friends",
        "season_number": 4,
        "episode_number": 12,
    },
    "disney's recess - space cadet": {
        "show_title": "Recess",
        "title": "Space Cadet",
        "season_number": 3,
        "episode_number": 7,
    },
    "recess - space cadet": {
        "show_title": "Recess",
        "title": "Space Cadet",
        "season_number": 3,
        "episode_number": 7,
    },
    "disney's recess - dodgeball city": {
        "show_title": "Recess",
        "title": "Dodgeball City",
        "season_number": 3,
        "episode_number": 3,
    },
    "recess - dodgeball city": {
        "show_title": "Recess",
        "title": "Dodgeball City",
        "season_number": 3,
        "episode_number": 3,
    },
    "disney's recess - lord of the nerds": {
        "show_title": "Recess",
        "title": "Lord of the Nerds",
        "season_number": 3,
        "episode_number": 10,
    },
    "recess - lord of the nerds": {
        "show_title": "Recess",
        "title": "Lord of the Nerds",
        "season_number": 3,
        "episode_number": 10,
    },
    "lord of the nerds": {
        "show_title": "Recess",
        "title": "Lord of the Nerds",
        "season_number": 3,
        "episode_number": 10,
    },
    "who can stay homeless the longest - kenny vs spenny (hd)": {
        "show_title": "Kenny vs. Spenny",
        "title": "Who Can Stay Homeless the Longest?",
        "season_number": 1,
        "episode_number": 22,
    },
    "who can stay homeless the longest_ - kenny vs. spenny (hd)": {
        "show_title": "Kenny vs. Spenny",
        "title": "Who Can Stay Homeless the Longest?",
        "season_number": 1,
        "episode_number": 22,
    },
    "who can stay homeless the longest": {
        "show_title": "Kenny vs. Spenny",
        "title": "Who Can Stay Homeless the Longest?",
        "season_number": 1,
        "episode_number": 22,
    },
    "triumph of the will - the french reconnect": {
        "show_title": "Triumph of the Will",
        "title": "The French ReConnection",
        "season_number": 1,
        "episode_number": 5,
    },
    "triumph of the will the french reconnect": {
        "show_title": "Triumph of the Will",
        "title": "The French ReConnection",
        "season_number": 1,
        "episode_number": 5,
    },
    "triumph of the will - the french reconnection": {
        "show_title": "Triumph of the Will",
        "title": "The French ReConnection",
        "season_number": 1,
        "episode_number": 5,
    },
    "triumph of the will the french reconnection": {
        "show_title": "Triumph of the Will",
        "title": "The French ReConnection",
        "season_number": 1,
        "episode_number": 5,
    },
    "the french reconnect": {
        "show_title": "Triumph of the Will",
        "title": "The French ReConnection",
        "season_number": 1,
        "episode_number": 5,
    },
}


def _video_override_key(value):
    text = Path(str(value or "")).stem
    text = text.replace("_", " ")
    text = re.sub(r"\s+", " ", text).strip().casefold()
    text = text.replace("?", "")
    return text


def _apply_video_episode_overrides(row):
    item = dict(row) if hasattr(row, "keys") else dict(row or {})
    if str(item.get("media_type") or "audio").lower() != "video":
        return item
    if item.get("metadata_locked") and (
        item.get("show_title")
        and item.get("season_number") is not None
        and item.get("episode_number") is not None
    ):
        return item
    candidates = [
        item.get("title"),
        Path(str(item.get("file_path") or "")).stem,
    ]
    for value in candidates:
        override = _VIDEO_EPISODE_OVERRIDES.get(_video_override_key(value))
        if not override:
            continue
        item.update(override)
        item["artist"] = item.get("artist") or override["show_title"]
        item["album_artist"] = item.get("album_artist") or override["show_title"]
        item["album"] = item.get("album") or (
            "Specials" if int(override.get("season_number") or 0) == 0
            else f"Season {int(override.get('season_number') or 0)}"
        )
        item["track_number"] = item.get("track_number") or override.get("episode_number")
        item["video_kind"] = "show"
        item["video_sync_category"] = ""
        return item
    return item


def _device_cache_title(row):
    title = str(row.get("title") or "").strip()
    file_path = str(row.get("file_path") or "")
    media_type = str(row.get("media_type") or "audio").lower()
    ext = Path(file_path).suffix.lower()
    source_parts = [part.casefold() for part in Path(file_path).parts]
    if (
        media_type == "video"
        and ext in {".mpg", ".mpeg", ".mpe"}
        and "youtube" in source_parts
        and _is_generic_downloaded_video_title(title)
    ):
        return Path(file_path).stem or "Downloaded Video"
    return title


def _remove_audio_with_sidecars(path):
    """Remove a device audio file and its plain-text lyric sidecar when present."""
    removed = []
    targets = [path]
    base, _ext = os.path.splitext(path)
    targets.append(base + ".lrc")

    for target in targets:
        if os.path.exists(target):
            os.remove(target)
            removed.append(target)
    return removed


def _is_video_bundle_row(row):
    item = dict(row) if hasattr(row, "keys") else dict(row or {})
    return (
        str(item.get("media_type") or "audio").lower() == "video"
        and str(item.get("sync_output_ext") or "").lower() == ".rvp"
        and bool(item.get("sync_video_bundle_paths"))
    )


def _remove_media_with_sidecars(path):
    """Remove a synced media file plus the sidecars RockPod owns for that format."""
    base, ext = os.path.splitext(path)
    if ext.lower() == ".rvp":
        removed = []
        targets = [path, base + ".yuv", base + ".pcm"]
        parent = os.path.dirname(path) or "."
        prefix = os.path.basename(base) + ".seg"
        try:
            for name in os.listdir(parent):
                if name.startswith(prefix) and name.lower().endswith((".yuv", ".pcm")):
                    targets.append(os.path.join(parent, name))
        except OSError:
            pass
        for target in targets:
            if os.path.exists(target):
                os.remove(target)
                removed.append(target)
        return removed
    return _remove_audio_with_sidecars(path)


def _current_video_bundle_dest_paths(row, dest):
    item = dict(row) if hasattr(row, "keys") else dict(row or {})
    base, _ext = os.path.splitext(dest)
    paths = {dest}
    segments = list(item.get("sync_video_segments") or [])
    if segments:
        for index, _segment in enumerate(segments, start=1):
            paths.add(f"{base}.seg{index:02d}.yuv")
            paths.add(f"{base}.seg{index:02d}.pcm")
    else:
        paths.add(base + ".yuv")
        paths.add(base + ".pcm")
    return paths


def _remove_stale_converted_video_outputs(row, dest):
    """Remove old source-format files and obsolete RVP sidecars after conversion."""
    if not _is_video_bundle_row(row):
        return []

    base, ext = os.path.splitext(dest)
    if ext.lower() != ".rvp":
        return []

    removed = []
    targets = []
    source_ext = Path(str(dict(row).get("file_path") or "")).suffix.lower()
    legacy_exts = [source_ext] if source_ext in VIDEO_EXTENSIONS else []
    for candidate_ext in (".mpg", ".mpeg", ".mpe", ".mp4", ".m4v", ".mov", ".mkv", ".avi", ".webm"):
        if candidate_ext not in legacy_exts:
            legacy_exts.append(candidate_ext)
    targets.extend(base + candidate_ext for candidate_ext in legacy_exts)

    keep_paths = _current_video_bundle_dest_paths(row, dest)
    parent = os.path.dirname(dest) or "."
    prefix = os.path.basename(base) + ".seg"
    targets.extend([base + ".yuv", base + ".pcm"])
    try:
        for name in os.listdir(parent):
            if name.startswith(prefix) and name.lower().endswith((".yuv", ".pcm")):
                targets.append(os.path.join(parent, name))
    except OSError:
        pass

    seen = set()
    for target in targets:
        if target in seen or target in keep_paths:
            continue
        seen.add(target)
        if os.path.exists(target):
            os.remove(target)
            removed.append(target)
    return removed


def _source_lyrics_sidecar_path(track_row):
    row = dict(track_row) if hasattr(track_row, "keys") else dict(track_row or {})
    source_path = str(row.get("file_path") or "").strip()
    if not source_path:
        return ""
    return str(Path(source_path).with_suffix(".lrc"))


def _clear_device_trash(device_mount):
    """Remove desktop trash folders from the device root so Rockbox does not index them."""
    removed = []
    try:
        names = os.listdir(device_mount)
    except OSError:
        return removed

    for name in names:
        if not str(name).startswith(".Trash-"):
            continue
        full = os.path.join(device_mount, name)
        if not os.path.isdir(full):
            continue
        shutil.rmtree(full, ignore_errors=True)
        if not os.path.exists(full):
            removed.append(full)
    return removed


def _read_existing_video_manifest_entries(device_mount):
    """Return only manifest rows whose media marker is still on this device."""
    try:
        manifest_path = resolve_under_root(
            device_mount, os.path.join(VIDEO_LIST_DEVICE_DIR, "index.tsv")
        )
        with open(manifest_path, "r", encoding="utf-8", newline="") as handle:
            first_line = handle.readline()
            if not first_line.startswith("# rockpod videolist"):
                return []
            reader = csv.DictReader(handle, delimiter="\t")
            entries = []
            for row in reader:
                rel_path = str(row.get("device_path") or "").strip()
                if not rel_path:
                    continue
                try:
                    media_path = resolve_under_root(device_mount, rel_path)
                except ValueError:
                    continue
                if os.path.isfile(media_path):
                    entries.append(dict(row))
            return entries
    except (OSError, csv.Error):
        return []


def _physical_rvp_paths(device_mount):
    try:
        videos_root = resolve_under_root(device_mount, "Videos")
    except ValueError:
        return {}
    if not os.path.isdir(videos_root):
        return {}

    paths = {}
    for root, dirs, files in os.walk(videos_root):
        dirs[:] = [
            name for name in dirs
            if not str(name).startswith(".Trash-")
        ]
        for name in files:
            if not str(name).lower().endswith(".rvp"):
                continue
            full_path = os.path.join(root, name)
            rel_path = _device_rel_path(device_mount, full_path)
            paths[rel_path.casefold()] = rel_path
    return paths


def _manifest_ordinal(value):
    """Normalise a season or episode number to a bare base-10 integer.

    The device groups seasons by comparing these strings, so "03" and "3"
    would otherwise split one season into two lists. Blank stays blank.
    """
    text = str(value if value is not None else "").strip()
    if not text:
        return ""
    match = re.search(r"\d+", text)
    if not match:
        return ""
    return str(int(match.group(0)))


def _fallback_video_manifest_entry(rel_path):
    """Build enough metadata to keep an unlinked physical RVP discoverable."""
    normalized = str(rel_path or "").replace("\\", "/").strip("/")
    parts = normalized.split("/")
    stem = Path(parts[-1] if parts else "Untitled Video").stem
    title = re.sub(r"^S\d{1,2}E\d{1,3}\s*-\s*", "", stem).strip() or stem
    video_id = hashlib.sha256(
        normalized.casefold().encode("utf-8", errors="surrogateescape")
    ).hexdigest()[:24]
    entry = {
        "video_id": video_id,
        "title": title,
        "kind": "movie",
        "group_key": "movie:" + re.sub(
            r"[^a-z0-9]+", "-", title.casefold()
        ).strip("-"),
        "device_path": normalized,
        "locked": "0",
    }

    if len(parts) >= 2 and parts[1].casefold() == ".locked":
        entry.update(kind="home_video", group_key="locked", locked="1")
    elif len(parts) >= 5 and parts[1].casefold() == "tv shows":
        show_title = parts[2]
        season_match = re.search(r"(\d+)", parts[3])
        episode_match = re.match(r"S(\d{1,2})E(\d{1,3})", stem, re.IGNORECASE)
        entry.update(
            kind="show",
            group_key="show:" + re.sub(
                r"[^a-z0-9]+", "-", show_title.casefold()
            ).strip("-"),
            show=show_title,
            season=_manifest_ordinal(
                season_match.group(1) if season_match else ""
            ),
            episode=_manifest_ordinal(
                episode_match.group(2) if episode_match else ""
            ),
        )
    elif len(parts) >= 2 and parts[1].casefold() == "music videos":
        entry.update(kind="music_video", group_key="music_video")
    elif len(parts) >= 2 and parts[1].casefold() == "home videos":
        entry.update(kind="home_video", group_key="home_video")
    return entry


def _merge_existing_video_manifest_entries(entries, plan, device_mount):
    """Preserve untouched physical videos and merge only planned changes."""
    obsolete_paths = {
        str(rel_path or "").strip()
        for rel_path in getattr(plan, "to_delete", [])
        if str(rel_path or "").strip()
    }
    replacement_paths = {
        str(rel_path or "").strip()
        for _row, rel_path in getattr(plan, "to_copy", [])
        if str(rel_path or "").strip()
    }
    replacement_paths.update(
        str(rel_path or "").strip()
        for _row, rel_path in getattr(plan, "preflight_linked", [])
        if str(rel_path or "").strip()
    )
    for _row, old_rel_path, new_rel_path in getattr(plan, "to_resync", []):
        old_rel_path = str(old_rel_path or "").strip()
        new_rel_path = str(new_rel_path or "").strip()
        if new_rel_path:
            replacement_paths.add(new_rel_path)
        if old_rel_path and old_rel_path != new_rel_path:
            obsolete_paths.add(old_rel_path)

    physical_paths = _physical_rvp_paths(device_mount)
    obsolete_keys = {path.casefold() for path in obsolete_paths}
    replacement_keys = {path.casefold() for path in replacement_paths}
    merged = {
        str(entry.get("device_path") or "").strip().casefold(): entry
        for entry in _read_existing_video_manifest_entries(device_mount)
        if str(entry.get("device_path") or "").strip().casefold()
        not in obsolete_keys
    }
    for entry in entries:
        rel_path = str(entry.get("device_path") or "").strip()
        if rel_path:
            key = rel_path.casefold()
            item = dict(entry)
            item["device_path"] = physical_paths.get(key, rel_path)
            if key not in merged or key in replacement_keys:
                merged[key] = item
    for key, rel_path in physical_paths.items():
        if key in obsolete_keys or key in merged:
            continue
        merged[key] = _fallback_video_manifest_entry(rel_path)

    # Rows carried over untouched from the device keep whatever season and
    # episode strings an older sync wrote, so normalise every row on the way
    # out. Otherwise a pre-existing "03" still splits a season away from the
    # freshly written "3".
    for entry in merged.values():
        entry["season"] = _manifest_ordinal(entry.get("season"))
        entry["episode"] = _manifest_ordinal(entry.get("episode"))
    return list(merged.values())


def _device_rel_path(device_mount, path):
    try:
        rel = os.path.relpath(path, device_mount)
    except (OSError, ValueError):
        return str(path)
    return rel.replace(os.sep, "/")


def _cleanup_stale_sync_temps(device_mount):
    """Remove temp files left by an interrupted RockPod sync."""
    removed = []
    errors = []
    if not device_mount or not os.path.isdir(device_mount):
        return removed, errors

    for root, dirs, files in os.walk(device_mount):
        dirs[:] = [name for name in dirs if not str(name).startswith(".Trash-")]
        for name in files:
            if not str(name).endswith(SYNC_TEMP_SUFFIX):
                continue
            full = os.path.join(root, name)
            try:
                os.remove(full)
                removed.append(_device_rel_path(device_mount, full))
            except OSError as exc:
                errors.append(f"Could not remove stale temp file {_device_rel_path(device_mount, full)}: {exc}")
    return removed, errors


def _sync_source_path(row):
    item = dict(row) if hasattr(row, "keys") else dict(row or {})
    return str(item.get("sync_source_path") or item.get("file_path") or "")


def _sync_row_transfer_size(row):
    item = dict(row) if hasattr(row, "keys") else dict(row or {})
    if _is_video_bundle_row(item):
        sizes = item.get("sync_video_bundle_sizes") or {}
        try:
            total = sum(int(value or 0) for value in sizes.values())
            if total > 0:
                return total
        except (TypeError, ValueError):
            pass
    source_path = _sync_source_path(item)
    if source_path:
        try:
            return os.path.getsize(source_path)
        except OSError:
            pass
    try:
        return int(item.get("file_size") or 0)
    except (TypeError, ValueError):
        return 0


def _device_file_matches_sync_source(row, device_path):
    if not os.path.isfile(device_path):
        return False
    if _is_video_bundle_row(row):
        base, _ext = os.path.splitext(device_path)
        sizes = dict((dict(row).get("sync_video_bundle_sizes") or {}))
        segments = list((dict(row).get("sync_video_segments") or []))
        if segments:
            required = {"rvp": device_path}
            for index, _segment in enumerate(segments, start=1):
                required[f"seg{index:02d}_yuv"] = f"{base}.seg{index:02d}.yuv"
                required[f"seg{index:02d}_pcm"] = f"{base}.seg{index:02d}.pcm"
        else:
            required = {
                "rvp": device_path,
                "yuv": base + ".yuv",
                "pcm": base + ".pcm",
            }
        for key, path in required.items():
            if not os.path.isfile(path):
                return False
            expected = int(sizes.get(key) or 0)
            if expected > 0 and os.path.getsize(path) != expected:
                return False
        return True
    try:
        return os.path.getsize(device_path) == _sync_row_transfer_size(row)
    except OSError:
        return False


def _video_bundle_file_is_current(row, device_path):
    """Validate a prepared RVP bundle against the on-device marker + sidecar state."""
    if not _is_video_bundle_row(row):
        return _device_file_matches_sync_source(row, device_path)

    if not _device_file_matches_sync_source(row, device_path):
        return False

    marker_path = device_path
    marker_meta = VideoRvpTranscoder._read_marker_meta(marker_path)
    if not marker_meta:
        return False

    expected_width = _coerce_int(row.get("sync_video_width"), 0)
    if expected_width > 0 and expected_width != _coerce_int(marker_meta.get("width"), 0):
        return False

    expected_height = _coerce_int(row.get("sync_video_height"), 0)
    if expected_height > 0 and expected_height != _coerce_int(marker_meta.get("height"), 0):
        return False

    expected_fps = _coerce_int(row.get("sync_video_fps"), 0)
    if expected_fps > 0 and expected_fps != _coerce_int(marker_meta.get("fps"), 0):
        return False

    expected_rate = _coerce_int(row.get("sync_video_sample_rate"), 0)
    if expected_rate > 0 and expected_rate != _coerce_int(marker_meta.get("sample_rate"), 0):
        return False

    expected_segments = list((dict(row).get("sync_video_segments") or []))
    marker_segments = _coerce_int(marker_meta.get("segments"), 0)
    if not expected_segments and marker_segments > 0:
        return False

    base = os.path.splitext(os.path.basename(marker_path))[0]
    if expected_segments:
        if marker_segments != len(expected_segments):
            return False
        for index in range(1, len(expected_segments) + 1):
            if marker_meta.get(f"segment{index}_video") != f"{base}.seg{index:02d}.yuv":
                return False
            if marker_meta.get(f"segment{index}_audio") != f"{base}.seg{index:02d}.pcm":
                return False
    else:
        if marker_meta.get("video") != f"{base}.yuv":
            return False
        if marker_meta.get("audio") != f"{base}.pcm":
            return False

    marker_width = _coerce_int(marker_meta.get("width"), 0)
    marker_height = _coerce_int(marker_meta.get("height"), 0)
    marker_fps = _coerce_int(marker_meta.get("fps"), 0)
    if (
        marker_width <= 0
        or marker_height <= 0
        or marker_width % 2
        or marker_height % 2
        or marker_fps <= 0
    ):
        return False

    return True


def _coerce_int(value, default=0):
    try:
        value_int = int(value)
    except (TypeError, ValueError):
        return default
    return value_int


class SyncPlan:
    """Describes what a sync operation will do before executing it."""

    def __init__(self):
        self.to_copy = []       # list of (track_row, device_rel_path)
        self.to_resync = []     # list of (track_row, existing_device_path, new_device_rel_path)
        self.artwork_to_copy = []  # list of (cache_cover_path, device_rel_path, album_key)
        self.generated_to_copy = []  # list of (source_path, device_rel_path, label)
        self.to_delete = []     # list of device_paths to remove (orphaned)
        self.up_to_date = []    # list of MatchResult already present on device
        self.total_bytes = 0
        self.transcode_count = 0
        self.errors = []
        self.preflight_linked = []   # list of (track_row, device_rel_path) repaired before copy
        self.preflight_removed = []  # list of stale temp relpaths removed before copy
        self.plan_profile = {}
        self.execution_profile = {}
        self.copy_reason_counts = {}
        self.update_reason_counts = {}
        self.warnings = []
        self.verified_duplicate_paths = set()

    @property
    def total_operations(self):
        return (
            len(self.to_copy)
            + len(self.to_resync)
            + len(self.artwork_to_copy)
            + len(self.generated_to_copy)
            + len(self.to_delete)
        )

    @property
    def copy_count(self):
        return len(self.to_copy)

    @property
    def resync_count(self):
        return len(self.to_resync)

    @property
    def up_to_date_count(self):
        return len(self.up_to_date)

    def summary(self):
        parts = []
        if self.to_copy:
            parts.append(f"{len(self.to_copy)} new tracks to copy")
        if self.to_resync:
            parts.append(f"{len(self.to_resync)} tracks to update")
        if self.artwork_to_copy:
            parts.append(f"{len(self.artwork_to_copy)} artwork files to update")
        if self.generated_to_copy:
            parts.append(f"{len(self.generated_to_copy)} app data files to update")
        if self.up_to_date:
            parts.append(f"{len(self.up_to_date)} tracks already up to date")
        if self.to_delete:
            parts.append(f"{len(self.to_delete)} duplicate device tracks to remove")
        if self.preflight_linked:
            parts.append(f"{len(self.preflight_linked)} interrupted sync files repaired")
        if self.preflight_removed:
            parts.append(f"{len(self.preflight_removed)} stale sync temp files removed")
        if self.transcode_count:
            parts.append(f"{self.transcode_count} tracks converted for device sync")
        mb = self.total_bytes / (1024 * 1024)
        if mb > 0:
            parts.append(f"{mb:.1f} MB to transfer")
        return "; ".join(parts) if parts else "Nothing to sync"


class SyncWorker(QObject):
    """Background worker that executes a sync plan.

    Creates its own SQLite connection so all DB writes happen in the
    worker thread — SQLite connections cannot be shared across threads.
    """

    progress = Signal(int, int, str)    # current, total, description
    file_copied = Signal(str, str)      # source, dest
    file_error = Signal(str, str)       # path, error message
    finished = Signal(int, int, int)    # copied, failed, skipped
    cancelled = Signal()

    def __init__(self, plan, device_mount, db_path):
        super().__init__()
        self._plan = plan
        self._device_mount = device_mount
        self._db_path = db_path
        self._cancelled = False

    def cancel(self):
        self._cancelled = True

    def run(self):
        copied = 0
        failed = 0
        skipped = 0
        total = self._plan.total_operations
        db = None
        created_dirs = set()
        synced_updates = []
        stage_times = {
            "track_copy_seconds": 0.0,
            "artwork_copy_seconds": 0.0,
            "local_file_read_seconds": 0.0,
            "device_write_seconds": 0.0,
            "mkdir_seconds": 0.0,
            "open_seconds": 0.0,
            "flush_sync_seconds": 0.0,
            "replace_seconds": 0.0,
            "db_finalize_seconds": 0.0,
            "artwork_generation_seconds": float(self._plan.plan_profile.get("artwork_generation_seconds", 0.0) or 0.0),
        }
        t_sync_start = time.perf_counter()

        try:
            db = Database(self._db_path, initialize=False)

            # Process copies (new tracks)
            for i, (track_row, rel_path) in enumerate(self._plan.to_copy):
                if self._cancelled:
                    self.cancelled.emit()
                    return

                row = dict(track_row) if hasattr(track_row, "keys") else track_row
                src = row.get("sync_source_path") or row.get("file_path", "")
                dest = os.path.join(self._device_mount, rel_path)
                desc = f"Copying tracks: {os.path.basename(row.get('file_path', '') or src)}"
                self.progress.emit(i + 1, total, desc)

                copy_started = time.perf_counter()
                ok, copy_stats = self._copy_track_media(row, src, dest, created_dirs)
                stage_times["track_copy_seconds"] += time.perf_counter() - copy_started
                stage_times["local_file_read_seconds"] += copy_stats.get("read_seconds", 0.0)
                stage_times["device_write_seconds"] += copy_stats.get("write_seconds", 0.0)
                stage_times["mkdir_seconds"] += copy_stats.get("mkdir_seconds", 0.0)
                stage_times["open_seconds"] += copy_stats.get("open_seconds", 0.0)
                stage_times["flush_sync_seconds"] += copy_stats.get("flush_sync_seconds", 0.0)
                stage_times["replace_seconds"] += copy_stats.get("replace_seconds", 0.0)
                if ok:
                    copied += 1
                    self.file_copied.emit(src, dest)
                    if not _is_video_bundle_row(row):
                        self._sync_lyrics_sidecar(row, dest, created_dirs)
                    else:
                        self._remove_stale_converted_video_outputs(row, dest)
                    tid = row.get("id")
                    if tid:
                        mh = row.get("metadata_hash", "")
                        fh = row.get("file_hash", "")
                        synced_updates.append((tid, rel_path, mh, fh))
                else:
                    failed += 1

            # Process resyncs (metadata/content changed)
            offset = len(self._plan.to_copy)
            for i, (track_row, old_dev_path, new_rel_path) in enumerate(self._plan.to_resync):
                if self._cancelled:
                    self.cancelled.emit()
                    return

                row = dict(track_row) if hasattr(track_row, "keys") else track_row
                src = row.get("sync_source_path") or row.get("file_path", "")
                desc = f"Copying tracks: {os.path.basename(row.get('file_path', '') or src)}"
                self.progress.emit(offset + i + 1, total, desc)

                dest = os.path.join(self._device_mount, new_rel_path)
                copy_started = time.perf_counter()
                ok, copy_stats = self._copy_track_media(row, src, dest, created_dirs)
                stage_times["track_copy_seconds"] += time.perf_counter() - copy_started
                stage_times["local_file_read_seconds"] += copy_stats.get("read_seconds", 0.0)
                stage_times["device_write_seconds"] += copy_stats.get("write_seconds", 0.0)
                stage_times["mkdir_seconds"] += copy_stats.get("mkdir_seconds", 0.0)
                stage_times["open_seconds"] += copy_stats.get("open_seconds", 0.0)
                stage_times["flush_sync_seconds"] += copy_stats.get("flush_sync_seconds", 0.0)
                stage_times["replace_seconds"] += copy_stats.get("replace_seconds", 0.0)
                if ok:
                    copied += 1
                    self.file_copied.emit(src, dest)
                    if not _is_video_bundle_row(row):
                        self._sync_lyrics_sidecar(row, dest, created_dirs)
                    else:
                        self._remove_stale_converted_video_outputs(row, dest)
                    if old_dev_path and old_dev_path != new_rel_path:
                        old_full = os.path.join(self._device_mount, old_dev_path)
                        try:
                            removed = _remove_media_with_sidecars(old_full)
                            if removed:
                                logger.debug("Removed old device file(s): %s", ", ".join(removed))
                        except OSError as e:
                            logger.warning("Could not remove old file %s: %s", old_full, e)
                    tid = row.get("id")
                    if tid:
                        mh = row.get("metadata_hash", "")
                        fh = row.get("file_hash", "")
                        synced_updates.append((tid, new_rel_path, mh, fh))
                else:
                    failed += 1

            artwork_offset = len(self._plan.to_copy) + len(self._plan.to_resync)
            for i, (src_cover, rel_path, _album_key) in enumerate(self._plan.artwork_to_copy):
                if self._cancelled:
                    self.cancelled.emit()
                    return
                desc = f"Syncing artwork: {os.path.basename(os.path.dirname(rel_path))}"
                self.progress.emit(artwork_offset + i + 1, total, desc)
                if failed and rel_path in MANIFEST_ARTWORK_RELPATHS:
                    skipped += 1
                    logger.warning("Skipping %s because media copy failures occurred", rel_path)
                    continue
                dest = os.path.join(self._device_mount, rel_path)
                copy_started = time.perf_counter()
                ok, copy_stats = self._copy_file(src_cover, dest, created_dirs)
                stage_times["artwork_copy_seconds"] += time.perf_counter() - copy_started
                stage_times["local_file_read_seconds"] += copy_stats.get("read_seconds", 0.0)
                stage_times["device_write_seconds"] += copy_stats.get("write_seconds", 0.0)
                stage_times["mkdir_seconds"] += copy_stats.get("mkdir_seconds", 0.0)
                stage_times["open_seconds"] += copy_stats.get("open_seconds", 0.0)
                stage_times["flush_sync_seconds"] += copy_stats.get("flush_sync_seconds", 0.0)
                stage_times["replace_seconds"] += copy_stats.get("replace_seconds", 0.0)
                if ok:
                    copied += 1
                    self.file_copied.emit(src_cover, dest)
                else:
                    failed += 1

            generated_offset = (
                len(self._plan.to_copy)
                + len(self._plan.to_resync)
                + len(self._plan.artwork_to_copy)
            )
            for i, (src, rel_path, label) in enumerate(self._plan.generated_to_copy):
                if self._cancelled:
                    self.cancelled.emit()
                    return
                desc = f"Syncing {label}: {os.path.basename(rel_path)}"
                self.progress.emit(generated_offset + i + 1, total, desc)
                dest = os.path.join(self._device_mount, rel_path)
                copy_started = time.perf_counter()
                ok, copy_stats = self._copy_file(src, dest, created_dirs)
                stage_times["artwork_copy_seconds"] += time.perf_counter() - copy_started
                stage_times["local_file_read_seconds"] += copy_stats.get("read_seconds", 0.0)
                stage_times["device_write_seconds"] += copy_stats.get("write_seconds", 0.0)
                stage_times["mkdir_seconds"] += copy_stats.get("mkdir_seconds", 0.0)
                stage_times["open_seconds"] += copy_stats.get("open_seconds", 0.0)
                stage_times["flush_sync_seconds"] += copy_stats.get("flush_sync_seconds", 0.0)
                stage_times["replace_seconds"] += copy_stats.get("replace_seconds", 0.0)
                if ok:
                    copied += 1
                    self.file_copied.emit(src, dest)
                else:
                    failed += 1

            delete_offset = (
                len(self._plan.to_copy)
                + len(self._plan.to_resync)
                + len(self._plan.artwork_to_copy)
                + len(self._plan.generated_to_copy)
            )
            for i, rel_path in enumerate(self._plan.to_delete):
                if self._cancelled:
                    self.cancelled.emit()
                    return
                desc = f"Removing duplicate: {os.path.basename(rel_path)}"
                self.progress.emit(delete_offset + i + 1, total, desc)
                dest = os.path.join(self._device_mount, rel_path)
                try:
                    removed = _remove_media_with_sidecars(dest)
                    if removed:
                        logger.debug("Removed duplicate device file(s): %s", ", ".join(removed))
                except OSError as exc:
                    failed += 1
                    self.file_error.emit(dest, str(exc))
                    logger.error("Failed to delete duplicate device file %s: %s", dest, exc)

            if self._cancelled:
                raise _SyncCancelled()
            cleanup_index = total + 1 if total else 1
            self.progress.emit(cleanup_index, total + 2, "Clearing device trash")
            removed_trash = _clear_device_trash(self._device_mount)
            if removed_trash:
                logger.info("Cleared device trash folders: %s", ", ".join(removed_trash))

            finalize_index = total + 2 if total else 2
            self.progress.emit(finalize_index, total + 2, "Finalizing")
            finalize_started = time.perf_counter()
            if synced_updates:
                with db.transaction():
                    db.mark_synced_many(synced_updates)
            stage_times["db_finalize_seconds"] += time.perf_counter() - finalize_started
            self._plan.execution_profile = {
                **stage_times,
                "copied": copied,
                "failed": failed,
                "skipped": skipped,
                "total_seconds": time.perf_counter() - t_sync_start,
            }
            logger.info(
                "Sync timing summary: plan=%.3fs track_copy=%.3fs artwork_gen=%.3fs artwork_copy=%.3fs "
                "read=%.3fs write=%.3fs mkdir=%.3fs open=%.3fs flush_sync=%.3fs replace=%.3fs db_finalize=%.3fs total=%.3fs",
                float(self._plan.plan_profile.get("build_seconds", 0.0) or 0.0),
                stage_times["track_copy_seconds"],
                stage_times["artwork_generation_seconds"],
                stage_times["artwork_copy_seconds"],
                stage_times["local_file_read_seconds"],
                stage_times["device_write_seconds"],
                stage_times["mkdir_seconds"],
                stage_times["open_seconds"],
                stage_times["flush_sync_seconds"],
                stage_times["replace_seconds"],
                stage_times["db_finalize_seconds"],
                self._plan.execution_profile["total_seconds"],
            )
            self.finished.emit(copied, failed, skipped)

        except _SyncCancelled:
            logger.info("Sync cancelled during file copy")
            self.cancelled.emit()
        except Exception as e:
            logger.exception("Sync execution failed")
            self.finished.emit(copied, failed, skipped)
        finally:
            if db:
                db.close()

    def _copy_file(self, src, dest, created_dirs=None):
        """Copy a file safely with parent directory creation."""
        stats = {
            "read_seconds": 0.0,
            "write_seconds": 0.0,
            "mkdir_seconds": 0.0,
            "open_seconds": 0.0,
            "flush_sync_seconds": 0.0,
            "replace_seconds": 0.0,
        }
        try:
            if not os.path.isfile(src):
                self.file_error.emit(src, "Source file not found")
                return False, stats

            dest_dir = os.path.dirname(dest)
            if created_dirs is not None:
                if dest_dir not in created_dirs:
                    t_mkdir = time.perf_counter()
                    os.makedirs(dest_dir, exist_ok=True)
                    stats["mkdir_seconds"] += time.perf_counter() - t_mkdir
                    created_dirs.add(dest_dir)
            else:
                t_mkdir = time.perf_counter()
                os.makedirs(dest_dir, exist_ok=True)
                stats["mkdir_seconds"] += time.perf_counter() - t_mkdir

            # Copy to a temp file first, then replace the destination.
            # Rockbox does not need source timestamps preserved on device files,
            # so avoid extra metadata syscalls on slow removable storage.
            tmp = dest + SYNC_TEMP_SUFFIX
            try:
                t_open = time.perf_counter()
                with open(src, "rb") as src_handle, open(tmp, "wb") as tmp_handle:
                    stats["open_seconds"] += time.perf_counter() - t_open
                    chunk_size = 1024 * 1024 if LEGACY_COPY_MODE else COPY_CHUNK_SIZE
                    while True:
                        if self._cancelled:
                            raise _SyncCancelled()
                        t_read = time.perf_counter()
                        chunk = src_handle.read(chunk_size)
                        stats["read_seconds"] += time.perf_counter() - t_read
                        if not chunk:
                            break
                        t_write = time.perf_counter()
                        tmp_handle.write(chunk)
                        stats["write_seconds"] += time.perf_counter() - t_write
                    t_flush = time.perf_counter()
                    tmp_handle.flush()
                    sync_fn = os.fsync if LEGACY_COPY_MODE or not hasattr(os, "fdatasync") else os.fdatasync
                    sync_fn(tmp_handle.fileno())
                    stats["flush_sync_seconds"] += time.perf_counter() - t_flush
                    if self._cancelled:
                        raise _SyncCancelled()
                if LEGACY_COPY_MODE:
                    try:
                        shutil.copystat(src, tmp, follow_symlinks=True)
                    except OSError:
                        pass
                    if os.path.exists(dest):
                        os.remove(dest)
                    t_replace = time.perf_counter()
                    os.rename(tmp, dest)
                    stats["replace_seconds"] += time.perf_counter() - t_replace
                else:
                    t_replace = time.perf_counter()
                    os.replace(tmp, dest)
                    stats["replace_seconds"] += time.perf_counter() - t_replace
                return True, stats
            except Exception:
                # Clean up temp file on failure
                if os.path.exists(tmp):
                    try:
                        os.remove(tmp)
                    except OSError:
                        pass
                raise

        except _SyncCancelled:
            raise
        except Exception as e:
            self.file_error.emit(src, str(e))
            logger.error("Failed to copy %s -> %s: %s", src, dest, e)
            return False, stats

    def _copy_track_media(self, row, src, dest, created_dirs=None):
        if not _is_video_bundle_row(row):
            return self._copy_file(src, dest, created_dirs)

        stats = self._empty_copy_stats()
        bundle = dict(row.get("sync_video_bundle_paths") or {})
        segments = list(row.get("sync_video_segments") or [])
        source_yuv = bundle.get("yuv")
        source_pcm = bundle.get("pcm")
        if not segments and (not source_yuv or not source_pcm):
            self.file_error.emit(src, "RVP video bundle is missing sidecar paths")
            return False, stats

        base, _ext = os.path.splitext(dest)
        copied_paths = []
        copy_pairs = []
        if segments:
            for index, segment in enumerate(segments, start=1):
                copy_pairs.append((segment.get("yuv"), f"{base}.seg{index:02d}.yuv"))
                copy_pairs.append((segment.get("pcm"), f"{base}.seg{index:02d}.pcm"))
        else:
            copy_pairs = [(source_yuv, base + ".yuv"), (source_pcm, base + ".pcm")]

        for source_path, dest_path in copy_pairs:
            if not source_path:
                self.file_error.emit(src, "RVP video segment is missing a source sidecar")
                self._remove_partial_video_marker(dest)
                return False, stats
            if self._files_have_same_content(source_path, dest_path):
                copied_paths.append(dest_path)
                continue
            ok, copy_stats = self._copy_file(source_path, dest_path, created_dirs)
            self._add_copy_stats(stats, copy_stats)
            if not ok:
                self._remove_partial_video_marker(dest)
                return False, stats
            copied_paths.append(dest_path)

        ok, marker_stats = self._write_rvp_marker(dest, row, created_dirs)
        self._add_copy_stats(stats, marker_stats)
        if not ok:
            self._remove_partial_video_marker(dest)
            return False, stats
        return True, stats

    def _files_have_same_content(self, source_path, dest_path):
        """Return whether a completed bundle sidecar can be reused safely."""
        try:
            if not os.path.isfile(source_path) or not os.path.isfile(dest_path):
                return False
            if os.path.getsize(source_path) != os.path.getsize(dest_path):
                return False
            with open(source_path, "rb") as source_handle, open(dest_path, "rb") as dest_handle:
                while True:
                    if self._cancelled:
                        raise _SyncCancelled()
                    source_chunk = source_handle.read(COPY_CHUNK_SIZE)
                    dest_chunk = dest_handle.read(COPY_CHUNK_SIZE)
                    if source_chunk != dest_chunk:
                        return False
                    if not source_chunk:
                        return True
        except _SyncCancelled:
            raise
        except OSError as exc:
            logger.warning(
                "Could not verify reusable RVP sidecar %s against %s: %s",
                dest_path,
                source_path,
                exc,
            )
            return False

    @staticmethod
    def _empty_copy_stats():
        return {
            "read_seconds": 0.0,
            "write_seconds": 0.0,
            "mkdir_seconds": 0.0,
            "open_seconds": 0.0,
            "flush_sync_seconds": 0.0,
            "replace_seconds": 0.0,
        }

    @staticmethod
    def _add_copy_stats(target, source):
        for key in target:
            target[key] += float((source or {}).get(key, 0.0) or 0.0)

    def _write_rvp_marker(self, dest, row=None, created_dirs=None):
        stats = self._empty_copy_stats()
        try:
            from services.video_rvp import VideoRvpTranscoder
            width = int((row or {}).get("sync_video_width", 320) or 320)
            height = int((row or {}).get("sync_video_height", 240) or 240)
            fps = int((row or {}).get("sync_video_fps", 20) or 20)
            sample_rate = int((row or {}).get("sync_video_sample_rate", 44100) or 44100)

            dest_dir = os.path.dirname(dest)
            if created_dirs is not None:
                if dest_dir not in created_dirs:
                    t_mkdir = time.perf_counter()
                    os.makedirs(dest_dir, exist_ok=True)
                    stats["mkdir_seconds"] += time.perf_counter() - t_mkdir
                    created_dirs.add(dest_dir)
            else:
                t_mkdir = time.perf_counter()
                os.makedirs(dest_dir, exist_ok=True)
                stats["mkdir_seconds"] += time.perf_counter() - t_mkdir

            tmp = dest + SYNC_TEMP_SUFFIX
            t_open = time.perf_counter()
            with open(tmp, "w", encoding="utf-8", newline="\n") as handle:
                stats["open_seconds"] += time.perf_counter() - t_open
                t_write = time.perf_counter()
                segments = list((row or {}).get("sync_video_segments") or [])
                if segments:
                    base = os.path.splitext(os.path.basename(dest))[0]
                    parts = [
                        (f"{base}.seg{index:02d}.yuv", f"{base}.seg{index:02d}.pcm")
                        for index, _segment in enumerate(segments, start=1)
                    ]
                    handle.write(
                        VideoRvpTranscoder.segmented_marker_text(
                            parts,
                            width=width,
                            height=height,
                            fps=fps,
                            sample_rate=sample_rate,
                        )
                    )
                else:
                    handle.write(
                        VideoRvpTranscoder.marker_text(
                            os.path.basename(dest),
                            width=width,
                            height=height,
                            fps=fps,
                            sample_rate=sample_rate,
                        )
                    )
                stats["write_seconds"] += time.perf_counter() - t_write
                t_flush = time.perf_counter()
                handle.flush()
                sync_fn = os.fsync if LEGACY_COPY_MODE or not hasattr(os, "fdatasync") else os.fdatasync
                sync_fn(handle.fileno())
                stats["flush_sync_seconds"] += time.perf_counter() - t_flush
            t_replace = time.perf_counter()
            os.replace(tmp, dest)
            stats["replace_seconds"] += time.perf_counter() - t_replace
            return True, stats
        except Exception as exc:
            tmp = dest + SYNC_TEMP_SUFFIX
            if os.path.exists(tmp):
                try:
                    os.remove(tmp)
                except OSError:
                    pass
            self.file_error.emit(dest, str(exc))
            logger.error("Failed to write RVP marker %s: %s", dest, exc)
            return False, stats

    @staticmethod
    def _remove_partial_video_bundle(dest, copied_paths):
        for path in list(copied_paths) + [dest, dest + SYNC_TEMP_SUFFIX]:
            if os.path.exists(path):
                try:
                    os.remove(path)
                except OSError:
                    pass

    @staticmethod
    def _remove_partial_video_marker(dest):
        """Hide an incomplete RVP bundle while retaining resumable sidecars."""
        for path in (dest, dest + SYNC_TEMP_SUFFIX):
            if os.path.exists(path):
                try:
                    os.remove(path)
                except OSError:
                    pass

    @staticmethod
    def _remove_stale_converted_video_outputs(row, dest):
        try:
            removed = _remove_stale_converted_video_outputs(row, dest)
            if removed:
                logger.debug("Removed stale converted video output(s): %s", ", ".join(removed))
        except OSError as exc:
            logger.warning("Could not remove stale converted video outputs for %s: %s", dest, exc)

    def _sync_lyrics_sidecar(self, track_row, audio_dest, created_dirs=None):
        """Mirror a local .lrc sidecar to the device next to the synced audio file."""
        source_sidecar = _source_lyrics_sidecar_path(track_row)
        dest_sidecar = os.path.splitext(audio_dest)[0] + ".lrc"
        if source_sidecar and os.path.isfile(source_sidecar):
            self._copy_file(source_sidecar, dest_sidecar, created_dirs)
            return
        if os.path.exists(dest_sidecar):
            try:
                os.remove(dest_sidecar)
            except OSError as exc:
                self.file_error.emit(dest_sidecar, str(exc))
                logger.warning("Failed to remove stale lyrics sidecar %s: %s", dest_sidecar, exc)


class SyncEngine(QObject):
    """High-level sync coordinator.

    This is a QObject so that signals from the worker thread are delivered
    to slots on this object via Qt's queued connection mechanism, ensuring
    all callbacks run on the main thread where the UI and main-thread DB
    connection live.
    """

    # Re-emitted on the main thread for UI consumers
    sync_progress = Signal(int, int, str)    # current, total, description
    sync_file_copied = Signal(str, str)      # source, dest
    sync_file_error = Signal(str, str)       # path, error message
    sync_finished = Signal(int, int, int)    # copied, failed, skipped
    sync_cancelled = Signal()
    sync_error = Signal(str)

    def __init__(self, db, config, device_detector, artwork_manager=None, parent=None):
        super().__init__(parent)
        self._db = db
        self._config = config
        self._device_detector = device_detector
        if artwork_manager is None:
            try:
                from services.artwork_manager import ArtworkManager
                artwork_manager = ArtworkManager(
                    getattr(self._config, "artwork_cache_dir", "")
                    or os.path.join(getattr(self._config, "cache_dir", ""), "artwork"),
                    self._config,
                )
            except Exception:
                artwork_manager = None
        self._artwork_manager = artwork_manager
        cache_root = getattr(self._config, "artwork_cache_dir", "") or os.path.join(
            getattr(self._config, "cache_dir", ""),
            "artwork",
        )
        self._video_thumbnails = VideoThumbnailService(
            cache_root,
            self._config,
            self._artwork_manager,
        )
        self._audio_transcoder = AudioSyncTranscoder(
            os.path.join(self._config.cache_dir, "device_transcodes")
        )
        self._video_transcoder = VideoRvpTranscoder(
            os.path.join(self._config.cache_dir, "device_video_rvp"),
            ffmpeg_path=self._config.get("ffmpeg_binary", ""),
        )
        self._thread = None
        self._worker = None
        self._device_tracks = []
        self._device_file_state = {}
        self._current_device_key = ""

    @property
    def is_syncing(self):
        return self._thread is not None and self._thread.isRunning()

    @property
    def current_device_key(self):
        return self._current_device_key

    def _config_value(self, key, default=None):
        device = self._device_detector.current_device
        if device is not None:
            return self._config.get_effective(key, device=device, default=default)
        if self._current_device_key:
            return self._config.get_effective(
                key,
                stable_device_key=self._current_device_key,
                default=default,
            )
        return self._config.get(key, default)

    def _match_settings(self):
        return (
            self._config_value("duplicate_strictness", "metadata_and_hash"),
            self._config_value("duration_match_tolerance_seconds", 2.0),
        )

    def _matcher(self):
        duplicate_strictness, duration_tolerance = self._match_settings()
        return TrackMatcher(duplicate_strictness, duration_tolerance)

    def rockbox_playlist_track_ids(self, include_smart=True):
        """Return local track IDs required by playlists selected for Rockbox sync."""
        if not bool(self._config_value("sync_playlists_to_device", True)):
            return []
        device_id = self._ensure_current_device_key()
        seen = set()
        track_ids = []
        for playlist_row in self._db.get_all_playlists():
            playlist = dict(playlist_row) if hasattr(playlist_row, "keys") else dict(playlist_row or {})
            if not playlist.get("sync_to_rockbox", 1):
                continue
            if playlist.get("is_smart") and not include_smart:
                continue
            try:
                rows = (
                    evaluate_playlist(self._db, playlist, device_id=device_id)
                    if playlist.get("is_smart")
                    else self._db.get_playlist_tracks(playlist["id"])
                )
            except Exception:
                logger.exception("Could not evaluate Rockbox playlist %s for sync planning", playlist.get("name"))
                continue
            for row in rows:
                item = dict(row) if hasattr(row, "keys") else dict(row or {})
                if str(item.get("media_type") or "audio").lower() != "audio":
                    continue
                try:
                    track_id = int(item.get("id"))
                except (TypeError, ValueError):
                    continue
                if track_id in seen:
                    continue
                seen.add(track_id)
                track_ids.append(track_id)
        return track_ids

    def _ensure_current_device_key(self):
        if self._current_device_key:
            return self._current_device_key
        device = self._device_detector.current_device
        if not device:
            return ""
        return self.set_current_device(device)

    def _device_inventory_needs_scan(self):
        """True when the current device has never been verified into the cache."""
        if not self._current_device_key:
            return False
        device = self._device_detector.current_device
        if not device:
            return False
        device_row = self._db.get_device_by_key(self._current_device_key)
        if not device_row:
            return True
        row = dict(device_row) if hasattr(device_row, "keys") else device_row
        if self._rockbox_database_newer_than_scan(device, row):
            return True
        if row.get("last_scan_at"):
            return False
        if self._device_tracks:
            return self._device_has_indexable_media_on_disk(device)
        return self._device_has_indexable_media_on_disk(device)

    @staticmethod
    def _rockbox_database_newer_than_scan(device, device_row):
        state = detect_rockbox_database_state(device, device_row)
        latest_mtime = str(state.get("database_latest_mtime") or "").strip()
        if not latest_mtime:
            return False
        last_scan = str((device_row or {}).get("last_scan_at") or "").strip()
        if not last_scan:
            return bool(state.get("database_present"))
        try:
            latest_dt = datetime.fromisoformat(latest_mtime)
            scan_dt = datetime.fromisoformat(last_scan.replace(" ", "T"))
        except ValueError:
            return False
        return latest_dt > scan_dt

    def _device_mount_unavailable(self):
        device = self._device_detector.current_device
        if not device:
            return False
        mount_path = str(getattr(device, "mount_path", "") or "").strip()
        return not mount_path or not os.path.isdir(mount_path)

    def _device_mount_read_only(self):
        device = self._device_detector.current_device
        if not device:
            return False
        mount_path = str(getattr(device, "mount_path", "") or "").strip()
        if not mount_path or not os.path.isdir(mount_path):
            return False
        try:
            readonly_flag = getattr(os, "ST_RDONLY", 1)
            return bool(os.statvfs(mount_path).f_flag & readonly_flag)
        except OSError:
            return not os.access(mount_path, os.W_OK)

    @staticmethod
    def _device_has_indexable_media_on_disk(device):
        roots = device_music_roots(device)
        if not roots:
            return False
        for scan_root in roots:
            for root, dirs, files in os.walk(scan_root):
                dirs[:] = [name for name in dirs if not name.startswith(".")]
                for name in files:
                    ext = os.path.splitext(name)[1].lower()
                    if ext in AUDIO_EXTENSIONS:
                        return True
        return False

    def build_sync_plan(
        self,
        track_ids=None,
        force_full=False,
        status_callback=None,
        media_type="audio",
        video_profile=None,
    ):
        """Analyze what needs to be synced and return a SyncPlan.

        Args:
            track_ids: optional set of track IDs to sync (None = all missing)
            force_full: if True, re-copy everything regardless of match state
            media_type: optional media filter ("audio", "video", or None for all)
            video_profile: optional per-action RVP/MPEG conversion profile
        """
        def emit_status(message):
            if status_callback:
                status_callback(str(message or "").strip())

        t0 = time.perf_counter()
        device = self._device_detector.current_device
        if not device:
            plan = SyncPlan()
            plan.errors.append("No device connected")
            plan.plan_profile = {"build_seconds": time.perf_counter() - t0}
            return plan
        if self._device_mount_unavailable():
            plan = SyncPlan()
            plan.errors.append("Device mount path is unavailable")
            plan.plan_profile = {"build_seconds": time.perf_counter() - t0}
            return plan
        if self._device_mount_read_only():
            plan = SyncPlan()
            plan.errors.append(
                "Device filesystem is mounted read-only; repair and remount "
                "the iPod before syncing"
            )
            plan.plan_profile = {"build_seconds": time.perf_counter() - t0}
            return plan

        if self._device_inventory_needs_scan():
            logger.info("Device inventory missing for %s; scanning before sync planning", self._current_device_key)
            emit_status("Scanning device contents...")
            if status_callback:
                self.scan_device(force_full=False, status_callback=status_callback)
            else:
                self.scan_device(force_full=False)

        plan = SyncPlan()
        self._ensure_current_device_key()
        dir_template = self._config_value("device_music_template", "Music/{album_artist}/{album}")
        file_template = self._config_value("device_file_template", "{track_number:02d} - {title}{ext}")
        resync_meta = bool(self._config_value("resync_metadata_changes", True))
        force = force_full or bool(self._config_value("force_full_resync", False))
        removed_temps, temp_errors = _cleanup_stale_sync_temps(device.mount_path)
        plan.preflight_removed.extend(removed_temps)
        plan.errors.extend(temp_errors)

        stage_times = {
            "local_fetch_seconds": 0.0,
            "device_fetch_seconds": 0.0,
            "match_seconds": 0.0,
            "audio_prepare_seconds": 0.0,
            "artwork_generation_seconds": 0.0,
        }

        # Get local and device tracks
        stage_start = time.perf_counter()
        emit_status("Loading library tracks...")
        if track_ids:
            local_tracks = self._db.get_tracks_by_ids(
                track_ids, media_type=media_type
            )
        else:
            local_tracks = self._db.get_all_tracks(media_type=media_type)
            playlist_track_ids = self.rockbox_playlist_track_ids()
            if playlist_track_ids:
                existing_ids = {
                    int(row["id"])
                    for row in local_tracks
                    if row["id"] is not None
                }
                missing_playlist_ids = [
                    track_id for track_id in playlist_track_ids
                    if track_id not in existing_ids
                ]
                if missing_playlist_ids:
                    local_tracks = list(local_tracks) + self._db.get_tracks_by_ids(
                        missing_playlist_ids, media_type=media_type
                    )
        stage_times["local_fetch_seconds"] = time.perf_counter() - stage_start
        all_local_tracks = local_tracks if not track_ids else self._db.get_all_tracks(
            media_type=media_type
        )
        if not track_ids:
            local_tracks = _collapse_managed_playlist_duplicates(local_tracks)
            all_local_tracks = local_tracks

        stage_start = time.perf_counter()
        emit_status("Preparing tracks for device sync...")
        local_tracks, transcode_errors, transcode_count = self._prepare_tracks_for_sync(
            local_tracks, video_profile=video_profile
        )
        local_tracks = [self._normalize_video_row_for_sync(row) for row in local_tracks]
        stage_times["audio_prepare_seconds"] = time.perf_counter() - stage_start
        plan.errors.extend(transcode_errors)
        plan.transcode_count = transcode_count

        stage_start = time.perf_counter()
        emit_status("Loading cached iPod inventory...")
        device_tracks = self.get_device_tracks()
        stage_times["device_fetch_seconds"] = time.perf_counter() - stage_start
        copy_reason_counts = {}
        update_reason_counts = {}

        matched = []
        resync_list = []
        if force:
            # Force mode: re-copy every selected local track
            for lt in local_tracks:
                row = dict(lt)
                row = self._normalize_video_row_for_sync(row)
                rel_path = build_device_path(row, dir_template, file_template)
                old_dev = row.get("device_path", "")
                if old_dev:
                    plan.to_resync.append((row, old_dev, rel_path))
                    for reason in _sync_update_reasons(row, old_dev_path=old_dev, new_rel_path=rel_path):
                        _bump_reason(update_reason_counts, reason)
                    _bump_reason(update_reason_counts, "force full resync")
                else:
                    plan.to_copy.append((row, rel_path))
                    _bump_reason(copy_reason_counts, "force full resync")
                    if row.get("sync_transcoded"):
                        _bump_reason(copy_reason_counts, "conversion required")
                plan.total_bytes += row.get("file_size", 0)
        else:
            # Normal mode: use matcher
            stage_start = time.perf_counter()
            emit_status("Matching library against iPod...")
            matcher = self._matcher()
            matched, unmatched, orphaned, resync_list = matcher.match_all(
                local_tracks, device_tracks
            )
            stage_times["match_seconds"] = time.perf_counter() - stage_start

            used_paths = {dict(dt).get("device_path", "") for dt in device_tracks}
            device_by_path = {
                dict(dt).get("device_path", ""): dict(dt)
                for dt in device_tracks
                if dict(dt).get("device_path", "")
            }
            matched_paths = {
                dict(result.device_track).get("device_path", "")
                for result in matched
                if getattr(result, "device_track", None) is not None
            }
            unmatched_reasons = matcher.explain_unmatched_many(
                [result.local_track for result in unmatched],
                device_tracks,
            )

            # New tracks to copy
            for result, unmatched_reason in zip(unmatched, unmatched_reasons):
                row = dict(result.local_track) if hasattr(result.local_track, "keys") else result.local_track
                row = self._normalize_video_row_for_sync(row)
                desired_rel_path = build_device_path(row, dir_template, file_template)
                existing_row = device_by_path.get(desired_rel_path)
                if (
                    existing_row
                    and desired_rel_path not in matched_paths
                    and not existing_row.get("local_track_id")
                ):
                    used_paths.add(desired_rel_path)
                    plan.to_resync.append((row, desired_rel_path, desired_rel_path))
                    for reason in _sync_update_reasons(
                        row,
                        device_row=existing_row,
                        old_dev_path=desired_rel_path,
                        new_rel_path=desired_rel_path,
                    ):
                        _bump_reason(update_reason_counts, reason)
                else:
                    desired_full_path = os.path.join(device.mount_path, desired_rel_path)
                    if desired_rel_path not in used_paths and os.path.isfile(desired_full_path):
                        used_paths.add(desired_rel_path)
                        if _video_bundle_file_is_current(row, desired_full_path):
                            plan.preflight_linked.append((row, desired_rel_path))
                            continue
                        plan.to_resync.append((row, desired_rel_path, desired_rel_path))
                        _bump_reason(update_reason_counts, "interrupted sync repair overwrite")
                        if row.get("sync_transcoded"):
                            _bump_reason(update_reason_counts, "conversion required")
                        plan.total_bytes += _sync_row_transfer_size(row)
                        continue

                    rel_path = self._unique_device_path(
                        desired_rel_path,
                        used_paths,
                    )
                    used_paths.add(rel_path)
                    plan.to_copy.append((row, rel_path))
                    _bump_reason(
                        copy_reason_counts,
                        unmatched_reason,
                    )
                    if row.get("sync_transcoded"):
                        _bump_reason(copy_reason_counts, "conversion required")
                plan.total_bytes += row.get("file_size", 0) if isinstance(row, dict) else 0

            # Tracks needing resync due to metadata/content changes
            resync_ids = {id(result) for result in resync_list} if resync_meta else set()
            if not resync_meta:
                for result in matched:
                    local = dict(result.local_track) if hasattr(result.local_track, "keys") else result.local_track
                    if not _is_video_bundle_row(local):
                        continue
                    device_row = dict(result.device_track) if hasattr(result.device_track, "keys") else result.device_track
                    device_rel = (device_row or {}).get("device_path", "") if isinstance(device_row, dict) else ""
                    if not device_rel:
                        continue
                    if not _video_bundle_file_is_current(
                        local,
                        os.path.join(device.mount_path, device_rel),
                    ):
                        resync_ids.add(id(result))

            legacy_path_resync_ids = set()
            for result in matched:
                local = dict(result.local_track) if hasattr(result.local_track, "keys") else result.local_track
                local = self._normalize_video_row_for_sync(local)
                device_row = dict(result.device_track) if hasattr(result.device_track, "keys") else result.device_track
                old_dev = (
                    (device_row or {}).get("device_path", "")
                    if isinstance(device_row, dict)
                    else ""
                ) or (local or {}).get("device_path", "")
                expected = self._unique_device_path(
                    build_device_path(local, dir_template, file_template),
                    used_paths,
                    allow_existing=old_dev,
                )
                video_path_changed = (
                    str(local.get("media_type") or "audio").lower() == "video"
                    and bool(old_dev)
                    and bool(expected)
                    and old_dev != expected
                )
                if (
                    (id(result) in resync_ids or video_path_changed)
                    and (old_dev or expected)
                ):
                    if old_dev != expected:
                        used_paths.add(expected)
                    plan.to_resync.append((local, old_dev, expected))
                    for reason in _sync_update_reasons(
                        local,
                        device_row=device_row,
                        old_dev_path=old_dev,
                        new_rel_path=expected,
                    ):
                        _bump_reason(update_reason_counts, reason)
                    if (
                        _is_video_bundle_row(local)
                        and old_dev
                        and not _video_bundle_file_is_current(local, os.path.join(device.mount_path, old_dev))
                    ):
                        _bump_reason(update_reason_counts, "video bundle mismatch")
                    plan.total_bytes += local.get("file_size", 0) if isinstance(local, dict) else 0
                    continue
                if not resync_meta:
                    if (
                        old_dev
                        and expected
                        and old_dev != expected
                        and _looks_like_auto_duplicate_path(old_dev)
                    ):
                        legacy_path_resync_ids.add(id(result))
                        used_paths.add(expected)
                        plan.to_resync.append((local, old_dev, expected))
                        for reason in _sync_update_reasons(
                            local,
                            device_row=device_row,
                            old_dev_path=old_dev,
                            new_rel_path=expected,
                        ):
                            _bump_reason(update_reason_counts, reason)

            plan.up_to_date = [
                result for result in matched
                if id(result) not in resync_ids
                and id(result) not in legacy_path_resync_ids
            ]

            if plan.preflight_linked:
                self._apply_preflight_links(plan.preflight_linked)

            # Also check for artwork changes (if track matched but artwork_hash differs)
            if resync_meta:
                for result in matched:
                    if result in resync_list:
                        continue
                    row = dict(result.local_track) if hasattr(result.local_track, "keys") else result.local_track
                    ah = row.get("artwork_hash", "")
                    lsah = row.get("last_synced_metadata_hash", "")
                    # Artwork changes are caught by metadata_hash already since
                    # the artwork_hash is separate. Check explicitly.
                    # For now metadata_hash covers tag changes; artwork_hash
                    # requires its own comparison if we want artwork-only resync.
                    # This is a future enhancement — log for now.
                    pass

        if not track_ids:
            duplicate_groups = self._find_duplicate_groups(all_local_tracks, device_tracks)
            for group in duplicate_groups:
                for row in group["remove"]:
                    rel_path = row.get("device_path", "")
                    if rel_path:
                        plan.to_delete.append(rel_path)
                        if any(
                            _audio_codec_rank(keep) > _audio_codec_rank(row)
                            for keep in group["keep"]
                        ):
                            plan.verified_duplicate_paths.add(rel_path)
            self._apply_duplicate_delete_safety(plan)

        stage_start = time.perf_counter()
        emit_status("Planning artwork sync...")
        self._populate_artwork_sync_plan(plan, matched, local_tracks)
        self._populate_weather_sync_plan(plan, device.mount_path)
        stage_times["artwork_generation_seconds"] = time.perf_counter() - stage_start
        plan.plan_profile = {
            **stage_times,
            "build_seconds": time.perf_counter() - t0,
        }
        plan.copy_reason_counts = copy_reason_counts
        plan.update_reason_counts = update_reason_counts
        logger.info(
            "Sync plan timing: total=%.3fs local_fetch=%.3fs audio_prepare=%.3fs device_fetch=%.3fs match=%.3fs artwork=%.3fs "
            "ops(copy=%d,resync=%d,artwork=%d,delete=%d,up_to_date=%d)",
            plan.plan_profile["build_seconds"],
            stage_times["local_fetch_seconds"],
            stage_times["audio_prepare_seconds"],
            stage_times["device_fetch_seconds"],
            stage_times["match_seconds"],
            stage_times["artwork_generation_seconds"],
            len(plan.to_copy),
            len(plan.to_resync),
            len(plan.artwork_to_copy),
            len(plan.to_delete),
            len(plan.up_to_date),
        )
        return plan

    def build_weather_sync_plan(self, status_callback=None):
        """Build a sync plan that updates only weather app assets."""
        def emit_status(message):
            if status_callback:
                status_callback(str(message or "").strip())

        t0 = time.perf_counter()
        device = self._device_detector.current_device
        if not device:
            plan = SyncPlan()
            plan.errors.append("No device connected")
            plan.plan_profile = {"build_seconds": time.perf_counter() - t0}
            return plan
        if self._device_mount_unavailable():
            plan = SyncPlan()
            plan.errors.append("Device mount path is unavailable")
            plan.plan_profile = {"build_seconds": time.perf_counter() - t0}
            return plan
        if self._device_mount_read_only():
            plan = SyncPlan()
            plan.errors.append(
                "Device filesystem is mounted read-only; repair and remount "
                "the iPod before syncing"
            )
            plan.plan_profile = {"build_seconds": time.perf_counter() - t0}
            return plan

        emit_status("Checking weather data...")
        plan = SyncPlan()
        self._ensure_current_device_key()
        self._populate_weather_sync_plan(plan, device.mount_path)
        plan.plan_profile = {"build_seconds": time.perf_counter() - t0}
        if len(plan.generated_to_copy) == 0 and not plan.errors:
            emit_status("Weather forecast is up to date")
        logger.info(
            "Weather sync plan timing: total=%.3fs files=%d",
            plan.plan_profile["build_seconds"],
            len(plan.generated_to_copy),
        )
        return plan

    def _populate_weather_sync_plan(self, plan, device_mount):
        if not bool(self._config_value("weather_enabled", True)):
            return
        try:
            output = build_weather_bundle(self._config, device=self._device_detector.current_device)
        except Exception as exc:
            logger.warning("Could not build weather bundle: %s", exc)
            plan.errors.append(f"Weather update failed: {exc}")
            return
        plan.errors.extend(output.errors)
        existing_hashes = {}
        for src, rel_path, expected_hash in output.files:
            device_path = os.path.join(device_mount, rel_path)
            if os.path.exists(device_path):
                cached_hash = existing_hashes.get(device_path)
                if cached_hash is None:
                    try:
                        cached_hash = compute_file_hash(device_path)
                    except OSError:
                        cached_hash = ""
                    existing_hashes[device_path] = cached_hash
                if cached_hash and cached_hash == expected_hash:
                    continue
            plan.generated_to_copy.append((src, rel_path, "weather"))

        # Weather-only sync also refreshes the three fixed WX carrier paths.
        # Their paths are already referenced by the Live TV guide, so
        # replacing them updates the active recorded-report set without
        # rebuilding or taking ownership of the rest of the user's lineup.
        try:
            from services.livetv import (
                LIVETV_DEVICE_DIR,
                LIVETV_WEATHER_BLOCK_SECONDS,
                LIVETV_WEATHER_CATEGORY,
                LiveTvLibrary,
                LiveTvMedia,
                LiveTvSync,
                weather_music_files,
            )

            library = LiveTvLibrary(self._config)
            weather_sync = LiveTvSync(library)
            bumpers = weather_sync.ensure_weather_sources(
                weather_music_files(self._config), self._config)
            for source in bumpers:
                media = LiveTvMedia(
                    path=source,
                    kind="show",
                    title="Local Forecast",
                    series=LIVETV_WEATHER_CATEGORY,
                    duration=LIVETV_WEATHER_BLOCK_SECONDS,
                    rating="TV-G",
                    description="Continuous local weather and music.",
                )
                encoded = weather_sync.ensure_mpeg(media)
                rel_path = os.path.join(
                    LIVETV_DEVICE_DIR, media.device_relative())
                device_path = os.path.join(device_mount, rel_path)
                if (os.path.isfile(device_path) and
                        compute_file_hash(device_path) ==
                        compute_file_hash(encoded)):
                    continue
                plan.generated_to_copy.append(
                    (encoded, rel_path, "weather channel"))

            active_report = os.path.join(
                weather_sync.weather_state_dir(), "active-clips.json")
            if os.path.isfile(active_report):
                report_relative = os.path.join(
                    ".rockbox", "rockpod", "weather",
                    "active-clips.json")
                device_report = os.path.join(device_mount, report_relative)
                if (not os.path.isfile(device_report) or
                        compute_file_hash(device_report) !=
                        compute_file_hash(active_report)):
                    plan.generated_to_copy.append(
                        (active_report, report_relative, "weather clips"))
        except Exception as exc:
            logger.warning("Could not refresh Weather channel: %s", exc)
            plan.errors.append(f"Weather channel update failed: {exc}")

    def get_device_tracks(self):
        """Return the current device index, preferring the in-memory cache."""
        self._ensure_current_device_key()
        if self._device_tracks:
            return list(self._device_tracks)
        if not self._current_device_key:
            return []
        return self._db.get_all_device_tracks(self._current_device_key)

    def _apply_duplicate_delete_safety(self, plan):
        """Block risky automatic device deletes before a sync can execute."""
        deletes = list(getattr(plan, "to_delete", []) or [])
        if not deletes:
            return
        deletes = sorted(deletes)
        plan.to_delete = deletes

        unsafe = [
            rel_path for rel_path in deletes
            if (
                not _looks_like_auto_duplicate_path(rel_path)
                and rel_path not in plan.verified_duplicate_paths
            )
        ]
        if unsafe:
            plan.to_delete = [
                rel_path for rel_path in deletes
                if (
                    _looks_like_auto_duplicate_path(rel_path)
                    or rel_path in plan.verified_duplicate_paths
                )
            ]
            plan.warnings.append(
                "Skipped automatic duplicate cleanup because delete candidates did not look like RockPod "
                f"duplicate files: {', '.join(unsafe[:5])}"
            )
            logger.warning("Skipped unsafe duplicate cleanup candidates: %s", unsafe)
            if not plan.to_delete:
                return

        max_deletes = int(self._config_value("max_auto_duplicate_deletes_per_sync", MAX_AUTO_DUPLICATE_DELETE_COUNT) or 0)
        if max_deletes < 0:
            max_deletes = 0
        if max_deletes == 0:
            plan.to_delete = []
            logger.info("Skipped automatic duplicate cleanup because the safety limit is 0")
            return
        if len(plan.to_delete) > max_deletes:
            plan.to_delete = []
            plan.errors.append(
                f"Skipped automatic duplicate cleanup because it wanted to remove {len(plan.to_delete)} iPod files. "
                f"The safety limit is {max_deletes}. Use duplicate cleanup manually after reviewing the device."
            )
            logger.warning(
                "Blocked duplicate cleanup count %d above safety limit %d: %s",
                len(plan.to_delete),
                max_deletes,
                plan.to_delete,
            )

    def device_inventory_has_local_links(self):
        """Return whether the cached device inventory can answer match-based counts cheaply."""
        if self._device_inventory_needs_scan():
            return False
        device_row = self._db.get_device_by_key(self._current_device_key) if self._current_device_key else None
        device_tracks = self._device_tracks
        if not device_tracks:
            if not device_row:
                return False
            return bool(dict(device_row).get("last_scan_at"))
        return all(dict(track).get("local_track_id") for track in device_tracks)

    def device_inventory_is_verified(self):
        """Return whether the current device inventory has been scanned into the cache."""
        if not self._ensure_current_device_key():
            return False
        return not self._device_inventory_needs_scan()

    def set_current_device(self, device):
        """Select a connected device and load its cached inventory from SQLite."""
        device_row = self._db.upsert_device(device_record_from_info(device))
        self._db.commit()
        self._current_device_key = device_row["stable_device_key"]
        self.load_cached_device_inventory()
        return self._current_device_key

    def load_cached_device_inventory(self, device_key=None):
        if device_key is not None:
            self._current_device_key = device_key
        self._device_tracks = [
            dict(row) for row in self._db.get_all_device_tracks(self._current_device_key)
        ] if self._current_device_key else []
        self._device_file_state = {
            row.get("device_path", ""): {"track": dict(row), "file_size": row.get("file_size")}
            for row in self._device_tracks
        }
        return list(self._device_tracks)

    def clear_device_index(self, forget_current=False):
        if forget_current:
            self._device_tracks = []
            self._device_file_state = {}
            self._current_device_key = ""
            return

        if self._current_device_key:
            self.load_cached_device_inventory(self._current_device_key)
        else:
            self._device_tracks = []
            self._device_file_state = {}

    def get_not_on_device_tracks(self, track_ids=None):
        """Return local tracks missing from the current device index."""
        if not self._ensure_current_device_key():
            return []
        if self._device_mount_unavailable():
            return []
        if self._device_inventory_needs_scan():
            return []
        device_tracks = self.get_device_tracks()
        if not device_tracks:
            return self._db.get_tracks_not_on_device(self._current_device_key, track_ids=track_ids)
        if all(dict(track).get("local_track_id") for track in device_tracks):
            return self._db.get_tracks_not_on_device(self._current_device_key, track_ids=track_ids)

        if track_ids:
            local_tracks = self._db.get_tracks_by_ids(track_ids)
        else:
            local_tracks = self._db.get_all_tracks()

        matcher = self._matcher()
        _matched, unmatched, _orphaned, _resync = matcher.match_all(
            local_tracks, device_tracks
        )
        return [result.local_track for result in unmatched]

    def get_not_on_device_reason_counts(self, track_ids=None):
        """Return grouped reasons explaining why local tracks are not on the device."""
        if not self._ensure_current_device_key():
            return {}
        if self._device_mount_unavailable():
            return {}
        if self._device_inventory_needs_scan():
            return {}

        missing_tracks = self.get_not_on_device_tracks(track_ids=track_ids)
        if not missing_tracks:
            return {}

        summary = summarize_unmatched_tracks(
            missing_tracks,
            self.get_device_tracks(),
            matcher=self._matcher(),
            sample_limit=0,
        )
        return summary["reason_counts"]

    def get_sync_status_counts(self):
        if not self._ensure_current_device_key():
            return {"missing": 0, "resync": 0}
        if self._device_mount_unavailable():
            return {"missing": 0, "resync": 0}
        if self._device_inventory_needs_scan():
            return {"missing": 0, "resync": 0}
        if self._audio_conversion_enabled():
            plan = self.build_sync_plan()
            return {"missing": plan.copy_count, "resync": plan.resync_count}
        device_tracks = self.get_device_tracks()
        if not device_tracks:
            return self._db.get_sync_status_counts(
                self._current_device_key,
                include_resync=bool(self._config_value("resync_metadata_changes", True)),
            )

        if all(dict(track).get("local_track_id") for track in device_tracks):
            return self._db.get_sync_status_counts(
                self._current_device_key,
                include_resync=bool(self._config_value("resync_metadata_changes", True)),
            )

        missing = len(self.get_not_on_device_tracks())
        resync = 0
        if self._config_value("resync_metadata_changes", True):
            resync = self._db.get_sync_status_counts(
                self._current_device_key,
                include_resync=True,
            )["resync"]
        return {"missing": missing, "resync": resync}

    def _audio_conversion_enabled(self):
        return bool(self._config_value("convert_audio_for_device", False))

    def _audio_conversion_settings(self):
        return {
            "enabled": self._audio_conversion_enabled(),
            "mode": self._config_value("audio_conversion_mode", "unsupported_or_lossless"),
            "target_codec": self._config_value("audio_conversion_codec", "mp3"),
            "target_bitrate_kbps": self._config_value("audio_conversion_bitrate_kbps", 160),
        }

    def cleanup_local_sync_cache(self, device_key=None, dry_run=False):
        """Remove stale local transcode/cache files, keeping only current device tracks."""
        if device_key is None:
            if not self._ensure_current_device_key():
                return {
                    "device_key": "",
                    "audio_cache_dir": "",
                    "video_cache_dir": "",
                    "kept": {"audio": 0, "video": 0},
                    "removed": [],
                    "bytes_freed": 0,
                    "errors": ["No current device"],
                }
            device_key = self._current_device_key

        cache_root = getattr(self._config, "cache_dir", "") or ""
        audio_dir = os.path.join(cache_root, "device_transcodes", str(device_key))
        video_dir = os.path.join(cache_root, "device_video_rvp", str(device_key))
        video_profile = self._config_value("video_sync_profile", "quality")
        self._video_transcoder.set_profile(video_profile)
        settings = self._audio_conversion_settings()
        target_codec = self._audio_transcoder._normalize_codec(settings.get("target_codec", "mp3"))
        target_bitrate = self._audio_transcoder._normalize_bitrate(settings.get("target_bitrate_kbps", 160))

        device_tracks = self._db.get_all_device_tracks(device_key)
        local_ids = {
            int(dict(row).get("local_track_id"))
            for row in device_tracks
            if dict(row).get("local_track_id")
        }
        local_by_id = {
            int(row["id"]): dict(row)
            for row in self._db.get_tracks_by_ids(local_ids, media_type=None)
        }
        keep_audio = set()
        keep_video = set()

        for dt_row in device_tracks:
            local_id = dict(dt_row).get("local_track_id")
            if not local_id:
                continue
            local_track = local_by_id.get(int(local_id))
            if not local_track:
                continue
            local_track = dict(local_track) if hasattr(local_track, "keys") else dict(local_track or {})
            media_type = str(local_track.get("media_type") or "audio").lower()
            if media_type == "video":
                keep_video.update(self._collect_video_cache_paths(local_track, device_key))
            elif media_type == "audio":
                keep_audio.update(self._collect_audio_cache_paths(
                    local_track,
                    device_key,
                    target_codec,
                    target_bitrate,
                    settings,
                ))

        result = {
            "device_key": device_key,
            "audio_cache_dir": audio_dir,
            "video_cache_dir": video_dir,
            "kept": {
                "audio": len(keep_audio),
                "video": len(keep_video),
            },
            "removed": [],
            "bytes_freed": 0,
            "errors": [],
        }

        audio_removed = self._cleanup_cache_dir(audio_dir, keep_audio, AUDIO_TRANSCODE_CACHE_EXTENSIONS, dry_run=dry_run)
        video_removed = self._cleanup_cache_dir(
            video_dir,
            keep_video,
            VIDEO_SYNC_CACHE_EXTENSIONS,
            dry_run=dry_run,
        )

        for path, bytes_freed in audio_removed + video_removed:
            result["removed"].append(path)
            result["bytes_freed"] += bytes_freed

        if result["removed"]:
            logger.info(
                "Cleaned local sync cache for %s (audio kept=%s, video kept=%s, removed=%d files)",
                device_key,
                result["kept"]["audio"],
                result["kept"]["video"],
                len(result["removed"]),
            )
        return result

    def _collect_audio_cache_paths(self, local_track, device_key, target_codec, target_bitrate, settings):
        item = dict(local_track) if hasattr(local_track, "keys") else dict(local_track)
        source_path = os.path.abspath(str(item.get("file_path") or ""))
        if not os.path.isfile(source_path):
            return set()
        source_ext = Path(source_path).suffix.lower()
        should_transcode = False
        try:
            should_transcode = self._audio_transcoder._should_transcode(
                item,
                source_ext,
                settings,
            )
        except Exception:
            # Conservative fallback: if we cannot inspect the same decision path, err on the
            # side of keeping fewer files in sync-related cache directories.
            return set()
        if not should_transcode:
            return set()
        try:
            cache_path = self._audio_transcoder._cache_path(
                source_path,
                item,
                device_key,
                target_codec,
                target_bitrate,
            )
            return {cache_path}
        except Exception:
            return set()

    def _collect_video_cache_paths(self, local_track, device_key):
        item = dict(local_track) if hasattr(local_track, "keys") else dict(local_track or {})
        source_path = os.path.abspath(str(item.get("file_path") or ""))
        if not source_path:
            return set()
        if getattr(self._video_transcoder, "_format", "rvp") == "mpeg2":
            if Path(source_path).suffix.lower() in {".mpg", ".mpeg"}:
                return set()
            cache_path = self._video_transcoder._cache_mpeg_path(
                source_path, item, device_key
            )
            keep = {cache_path}
            digest_path = cache_path + ".digest.json"
            if os.path.isfile(digest_path):
                keep.add(digest_path)
            return keep
        base_keep = set()
        cache_root = self._video_transcoder._cache_root
        marker_path = self._video_transcoder._cache_marker_path(source_path, item, device_key)
        cache_dir = os.path.dirname(marker_path)
        if not os.path.isdir(cache_root) and not os.path.isdir(cache_dir):
            return set()
        base_keep.add(marker_path)
        digest_path = marker_path + ".digest.json"
        if os.path.isfile(digest_path):
            base_keep.add(digest_path)
        marker_meta = {}
        if os.path.isfile(marker_path):
            marker_meta = self._video_transcoder._read_marker_meta(marker_path)
        segments = self._collect_cached_video_segment_paths(marker_meta, marker_path)
        if segments:
            base_keep.update(segments)
            return base_keep
        base, _ext = os.path.splitext(marker_path)
        base_keep.add(base + ".yuv")
        base_keep.add(base + ".pcm")
        base_keep.update(self._collect_existing_segment_paths(marker_path))
        return base_keep

    @staticmethod
    def _collect_cached_video_segment_paths(marker_meta, marker_path):
        if not marker_meta:
            return set()
        segment_count = 0
        try:
            segment_count = int(str(marker_meta.get("segments") or "0").strip())
        except (TypeError, ValueError):
            segment_count = 0

        if segment_count <= 0:
            return set()
        base_dir = os.path.dirname(marker_path)
        tracks = set()
        for index in range(1, segment_count + 1):
            video_name = marker_meta.get(f"segment{index}_video", "").strip()
            audio_name = marker_meta.get(f"segment{index}_audio", "").strip()
            if video_name:
                tracks.add(os.path.join(base_dir, video_name))
            if audio_name:
                tracks.add(os.path.join(base_dir, audio_name))
        return tracks

    @staticmethod
    def _collect_existing_segment_paths(marker_path):
        base = os.path.splitext(os.path.basename(marker_path))[0]
        base_dir = os.path.dirname(marker_path)
        if not base_dir:
            base_dir = "."
        segment_paths = set()
        try:
            for name in os.listdir(base_dir):
                if name.startswith(base + ".seg") and name.lower().endswith((".yuv", ".pcm")):
                    segment_paths.add(os.path.join(base_dir, name))
        except OSError:
            return segment_paths
        return segment_paths

    @staticmethod
    def _cleanup_cache_dir(cache_dir, keep_paths, extensions, dry_run=False):
        if not os.path.isdir(cache_dir):
            return []
        removed = []
        keep = {os.path.normpath(path) for path in keep_paths}

        for root, dirs, files in os.walk(cache_dir):
            if os.path.basename(root) == "logs":
                continue
            for name in files:
                full_path = os.path.join(root, name)
                if os.path.normpath(full_path) in keep:
                    continue
                _, ext = os.path.splitext(name)
                if ext.lower() in extensions or name.lower().endswith(".rockpod_tmp"):
                    file_size = os.path.getsize(full_path) if os.path.isfile(full_path) else 0
                    try:
                        if not dry_run:
                            os.remove(full_path)
                        removed.append((full_path, file_size))
                    except OSError as exc:
                        logger.debug("Could not remove stale cache file %s: %s", full_path, exc)
        return removed

    def _prepare_tracks_for_sync(self, rows, video_profile=None):
        settings = self._audio_conversion_settings()
        selected_video_profile = (
            video_profile
            if video_profile is not None
            else self._config_value("video_sync_profile", "quality")
        )
        self._video_transcoder.set_profile(selected_video_profile)
        prepared = []
        errors = []
        transcode_count = 0
        for row in rows:
            item = dict(row) if hasattr(row, "keys") else dict(row)
            media_type = str(item.get("media_type") or "audio").lower()
            if media_type == "video":
                item = self._normalize_video_row_for_sync(item)
                try:
                    sync_row, info = self._video_transcoder.prepare_track_for_sync(
                        item,
                        self._current_device_key or "device",
                    )
                except RuntimeError as exc:
                    title = item.get("title") or os.path.basename(str(item.get("file_path") or "video"))
                    message = f"{title}: {exc}"
                    errors.append(message)
                    logger.warning("Skipping video sync for %s", message)
                    continue
                if info.get("converted"):
                    transcode_count += 1
                prepared.append(sync_row)
                continue
            if media_type != "audio" or not settings["enabled"]:
                prepared.append(item)
                continue
            try:
                sync_row, info = self._audio_transcoder.prepare_track_for_sync(
                    item,
                    self._current_device_key or "device",
                    settings,
                )
            except RuntimeError as exc:
                title = item.get("title") or os.path.basename(str(item.get("file_path") or "track"))
                message = f"{title}: {exc}"
                errors.append(message)
                logger.warning("Skipping sync for %s", message)
                continue
            if info.get("converted"):
                transcode_count += 1
            prepared.append(sync_row)
        return prepared, errors, transcode_count

    @staticmethod
    def _normalize_video_row_for_sync(row):
        item = dict(row) if hasattr(row, "keys") else dict(row or {})
        if str(item.get("media_type") or "audio").lower() != "video":
            return item
        item = _normalize_video_show_fields(item)
        item = _apply_video_episode_overrides(item)
        item = _normalize_video_show_fields(item)
        source_path = str(item.get("file_path") or "")
        stem_title = Path(source_path).stem.strip()
        if stem_title and _is_generic_downloaded_video_title(item.get("title")):
            item["title"] = stem_title
        if stem_title and _is_generic_downloaded_video_title(item.get("album")) and not item.get("season_number"):
            item["album"] = item.get("title") or stem_title
        if _normalize_video_kind_value(item.get("video_kind")) == "show" and item.get("show_title"):
            item["video_kind"] = "show"
            if not str(item.get("album") or "").strip() and item.get("season_number") is not None:
                season_number = _to_video_int(item.get("season_number") or 0) or 0
                item["album"] = "Specials" if season_number == 0 else f"Season {season_number:02d}"
            if not str(item.get("artist") or "").strip() and item.get("show_title"):
                item["artist"] = item["show_title"]
            if not str(item.get("album_artist") or "").strip() and item.get("show_title"):
                item["album_artist"] = item["show_title"]
            item["video_sync_category"] = ""
        elif not str(item.get("video_kind") or "").strip():
            item["video_kind"] = "movie"
        return _recompute_row_metadata_hash(item)

    def _unique_device_path(self, rel_path, used_paths, allow_existing=""):
        """Avoid path collisions while preserving the configured folder layout."""
        if rel_path == allow_existing or rel_path not in used_paths:
            return rel_path

        root, ext = os.path.splitext(rel_path)
        counter = 2
        candidate = f"{root} ({counter}){ext}"
        while candidate in used_paths and candidate != allow_existing:
            counter += 1
            candidate = f"{root} ({counter}){ext}"
        return candidate

    def scan_device(self, force_full=False, status_callback=None):
        """Verify the current device inventory and refresh the cached index."""
        device = self._device_detector.current_device
        if not device:
            return 0
        self._ensure_current_device_key()

        summary = verify_device_inventory(
            self._db,
            device,
            self._config_value("duplicate_strictness", "metadata_and_hash"),
            force_full,
            read_metadata,
            compute_file_hash,
            self._config_value("duration_match_tolerance_seconds", 2.0),
            status_callback=status_callback,
        )
        self._current_device_key = summary.get("device_key", self._current_device_key)
        self.load_cached_device_inventory()

        logger.info("Device scan complete: %d tracks found", summary["scanned"])
        return summary["scanned"]

    def _populate_artwork_sync_plan(self, plan, matched, local_tracks):
        if not self._artwork_manager:
            return
        if not self._config_value("copy_artwork_to_device", True):
            return
        export_device_covers = self._config_value("export_device_cover_jpg", True)
        export_wps_covers = self._config_value("export_wps_sized_covers", True)
        export_album_list_thumbnails = self._config_value("export_album_list_thumbnails", True)
        if not export_device_covers and not export_wps_covers and not export_album_list_thumbnails:
            return
        device = self._device_detector.current_device
        if not device:
            return

        album_targets = {}

        def add_target(row, device_rel_path):
            if str(row.get("media_type") or "audio").lower() != "audio":
                return
            rel_dir = os.path.dirname(device_rel_path)
            if not rel_dir:
                return
            info = {
                "group_key": row.get("album_group_key") or f"{row.get('album_artist') or row.get('artist') or 'Unknown Artist'}\0{row.get('album') or 'Unknown Album'}",
                "album": row.get("album", "") or "Unknown Album",
                "artist": row.get("album_artist", "") or row.get("artist", "") or "Unknown Artist",
                "tracks": [],
            }
            target = album_targets.setdefault(info["group_key"], {**info, "device_dirs": set()})
            target["device_dirs"].add(rel_dir)
            target["tracks"].append(row)

        for row, rel_path in plan.to_copy:
            add_target(dict(row), rel_path)
        for row, _old_path, rel_path in plan.to_resync:
            add_target(dict(row), rel_path)
        for row, rel_path in plan.preflight_linked:
            add_target(dict(row), rel_path)
        for result in matched:
            row = dict(result.local_track) if hasattr(result.local_track, "keys") else dict(result.local_track)
            device_row = dict(result.device_track) if hasattr(result.device_track, "keys") else dict(result.device_track)
            rel_path = device_row.get("device_path", "")
            if rel_path:
                add_target(row, rel_path)

        seen = set()
        existing_cover_hashes = {}
        existing_albumlist_hashes = {}
        wps_sizes = wps_album_art_sizes_for_config(self._config, device.mount_path) if export_wps_covers else []
        fit_mode = self._config_value("wps_cover_fit_mode", "contain")
        for album_key, album_info in album_targets.items():
            if export_device_covers:
                cover_src, cover_hash = self._artwork_manager.export_device_cover(album_info)
                if cover_src:
                    for rel_dir in sorted(album_info["device_dirs"]):
                        cover_rel = os.path.join(rel_dir, "cover.jpg")
                        if cover_rel in seen:
                            continue
                        seen.add(cover_rel)
                        device_cover = os.path.join(device.mount_path, cover_rel)
                        if os.path.exists(device_cover):
                            cached_hash = existing_cover_hashes.get(device_cover)
                            if cached_hash is None:
                                try:
                                    cached_hash = compute_file_hash(device_cover)
                                except OSError:
                                    cached_hash = ""
                                existing_cover_hashes[device_cover] = cached_hash
                            if cached_hash and cached_hash == cover_hash:
                                continue
                        plan.artwork_to_copy.append((cover_src, cover_rel, album_key))
            for width, height in wps_sizes:
                wps_src, wps_hash = self._artwork_manager.export_rockbox_wps_cover(
                    album_info,
                    (width, height),
                    fit_mode=fit_mode,
                )
                if not wps_src or not wps_hash:
                    continue
                for rel_dir in sorted(album_info["device_dirs"]):
                    wps_rel = os.path.join(rel_dir, f"cover.{width}x{height}.bmp")
                    if wps_rel in seen:
                        continue
                    seen.add(wps_rel)
                    self._append_artwork_copy_if_changed(
                        plan,
                        wps_src,
                        wps_rel,
                        album_key,
                        wps_hash,
                        device.mount_path,
                        existing_cover_hashes,
                    )

        if export_album_list_thumbnails:
            if album_targets:
                self._populate_album_list_artwork_sync_plan(
                    plan,
                    album_targets,
                    device.mount_path,
                    existing_albumlist_hashes,
                )
            self._populate_video_list_artwork_sync_plan(
                plan,
                matched,
                device.mount_path,
                existing_albumlist_hashes,
            )

    def _populate_album_list_artwork_sync_plan(self, plan, album_targets, device_mount, existing_hashes):
        manifest_entries = []
        seen = set()
        for album_key, album_info in sorted(album_targets.items(), key=lambda item: str(item[0]).casefold()):
            thumb_src, thumb_hash, device_name, album_id = self._artwork_manager.export_album_list_thumbnail(album_info)
            thumb_rel = ""
            if thumb_src and thumb_hash and device_name:
                thumb_rel = os.path.join(ALBUM_LIST_THUMB_DEVICE_DIR, device_name)
                if thumb_rel not in seen:
                    seen.add(thumb_rel)
                    self._append_artwork_copy_if_changed(
                        plan,
                        thumb_src,
                        thumb_rel,
                        album_key,
                        thumb_hash,
                        device_mount,
                        existing_hashes,
                    )
            elif not album_id:
                album_id = self._artwork_manager.album_list_id(album_key)

            slide_src, slide_hash, slide_device_name, slide_album_id = (
                self._artwork_manager.export_album_list_slide(album_info)
            )
            slide_rel = ""
            if not album_id and slide_album_id:
                album_id = slide_album_id
            if slide_src and slide_hash and slide_device_name:
                slide_rel = os.path.join(ALBUM_LIST_SLIDE_DEVICE_DIR, slide_device_name)
                if slide_rel not in seen:
                    seen.add(slide_rel)
                    self._append_artwork_copy_if_changed(
                        plan,
                        slide_src,
                        slide_rel,
                        album_key,
                        slide_hash,
                        device_mount,
                        existing_hashes,
                    )

            manifest_entries.append(
                {
                    "album_id": album_id,
                    "thumb": os.path.join("thumbs", device_name) if thumb_rel else "",
                    "slide": os.path.join("slides", slide_device_name) if slide_rel else "",
                    "artist": album_info.get("artist", ""),
                    "album": album_info.get("album", ""),
                    "group_key": album_key,
                    "device_dirs": "|".join(sorted(album_info.get("device_dirs") or [])),
                }
            )

        manifest_src, manifest_hash = self._artwork_manager.export_album_list_manifest(manifest_entries)
        if manifest_src and manifest_hash:
            self._append_artwork_copy_if_changed(
                plan,
                manifest_src,
                os.path.join(ALBUM_LIST_DEVICE_DIR, "index.tsv"),
                "albumlist_manifest",
                manifest_hash,
                device_mount,
                existing_hashes,
            )

    def _populate_video_list_artwork_sync_plan(self, plan, matched, device_mount, existing_hashes):
        if not self._video_thumbnails:
            return

        video_targets = {}

        def add_video(row, device_rel_path):
            item = self._normalize_video_row_for_sync(row)
            if str(item.get("media_type") or "audio").lower() != "video":
                return
            rel_path = str(device_rel_path or "").strip()
            if not rel_path:
                return
            key = str(item.get("file_path") or rel_path)
            video_targets[key] = {"row": item, "device_path": rel_path}

        # A selective video sync must retain every currently synced video in
        # the device manifest.  Building this list only from the selected
        # plan/matches collapses index.tsv to the last synced selection.
        local_videos = list(self._db.get_all_tracks(media_type="video"))
        local_videos_by_id = {
            int(row["id"]): row
            for row in local_videos
            if row["id"] is not None
        }
        for row in local_videos:
            item = dict(row) if hasattr(row, "keys") else dict(row)
            rel_path = str(item.get("device_path") or "").strip()
            if rel_path and os.path.isfile(os.path.join(device_mount, rel_path)):
                add_video(item, rel_path)

        # Inventory reconciliation can temporarily clear the legacy local
        # device_path while the current per-device row still links the same
        # local video.  Keep that physically present video in the manifest.
        for device_row in self.get_device_tracks():
            device_item = (
                dict(device_row)
                if hasattr(device_row, "keys") else dict(device_row)
            )
            try:
                local_id = int(device_item.get("local_track_id") or 0)
            except (TypeError, ValueError):
                continue
            local_row = local_videos_by_id.get(local_id)
            rel_path = str(device_item.get("device_path") or "").strip()
            if (
                local_row is not None
                and rel_path
                and os.path.isfile(os.path.join(device_mount, rel_path))
            ):
                add_video(local_row, rel_path)

        for row, rel_path in plan.to_copy:
            add_video(row, rel_path)
        for row, _old_path, rel_path in plan.to_resync:
            add_video(row, rel_path)
        for row, rel_path in plan.preflight_linked:
            add_video(row, rel_path)
        for result in matched:
            row = dict(result.local_track) if hasattr(result.local_track, "keys") else dict(result.local_track)
            device_row = dict(result.device_track) if hasattr(result.device_track, "keys") else dict(result.device_track)
            add_video(row, device_row.get("device_path", ""))

        if not video_targets:
            return

        manifest_entries = []
        seen = set()

        def export_hierarchy_art(row, scope, artwork_key):
            artwork_id = self._video_thumbnails.video_hierarchy_art_id(
                row, scope
            )
            copied = False
            variants = (
                ({}, VIDEO_LIST_NETFLIX_POSTER_DEVICE_DIR),
                ({"detail": True}, VIDEO_LIST_NETFLIX_DETAIL_DEVICE_DIR),
                ({"landing": True}, VIDEO_LIST_NETFLIX_LANDING_DEVICE_DIR),
            )
            for options, device_dir in variants:
                source, source_hash, device_name, _ = (
                    self._video_thumbnails.export_video_list_hierarchy_poster(
                        row, scope, **options
                    )
                )
                if not source or not source_hash or not device_name:
                    continue
                rel_path = os.path.join(device_dir, device_name)
                copied = True
                if rel_path in seen:
                    continue
                seen.add(rel_path)
                self._append_artwork_copy_if_changed(
                    plan,
                    source,
                    rel_path,
                    artwork_key,
                    source_hash,
                    device_mount,
                    existing_hashes,
                )
            return artwork_id if copied else ""

        for key, target in sorted(video_targets.items(), key=lambda item: str(item[0]).casefold()):
            row = target["row"]
            show_art_id = ""
            season_art_id = ""
            if (
                str(row.get("video_kind") or "") == "show"
                and not bool(row.get("video_locked"))
            ):
                show_art_id = export_hierarchy_art(
                    row, "show", f"{key}:show-art"
                )
                if int(row.get("season_number") or 0):
                    season_art_id = export_hierarchy_art(
                        row, "season", f"{key}:season-art"
                    )
            thumb_src, thumb_hash, device_name, video_id = self._video_thumbnails.export_video_list_thumbnail(row)
            thumb_rel = ""
            if thumb_src and thumb_hash and device_name:
                thumb_rel = os.path.join(VIDEO_LIST_THUMB_DEVICE_DIR, device_name)
                if thumb_rel not in seen:
                    seen.add(thumb_rel)
                    self._append_artwork_copy_if_changed(
                        plan,
                        thumb_src,
                        thumb_rel,
                        key,
                        thumb_hash,
                        device_mount,
                        existing_hashes,
                    )
            elif not video_id:
                video_id = self._video_thumbnails.video_list_id(row)

            preview_src, preview_hash, preview_name, preview_id = self._video_thumbnails.export_video_list_preview(row)
            if not video_id and preview_id:
                video_id = preview_id
            preview_rel = ""
            if preview_src and preview_hash and preview_name:
                preview_rel = os.path.join(VIDEO_LIST_PREVIEW_DEVICE_DIR, preview_name)
                if preview_rel not in seen:
                    seen.add(preview_rel)
                    self._append_artwork_copy_if_changed(
                        plan,
                        preview_src,
                        preview_rel,
                        key,
                        preview_hash,
                        device_mount,
                        existing_hashes,
                    )

            netflix_poster_src, netflix_poster_hash, netflix_poster_name, _ = (
                self._video_thumbnails.export_video_list_netflix_poster(row)
            )
            netflix_poster_rel = ""
            if netflix_poster_src and netflix_poster_hash and netflix_poster_name:
                netflix_poster_rel = os.path.join(
                    VIDEO_LIST_NETFLIX_POSTER_DEVICE_DIR, netflix_poster_name
                )
                if netflix_poster_rel not in seen:
                    seen.add(netflix_poster_rel)
                    self._append_artwork_copy_if_changed(
                        plan,
                        netflix_poster_src,
                        netflix_poster_rel,
                        key,
                        netflix_poster_hash,
                        device_mount,
                        existing_hashes,
                    )

            netflix_detail_src, netflix_detail_hash, netflix_detail_name, _ = (
                self._video_thumbnails.export_video_list_netflix_poster(
                    row, detail=True
                )
            )
            netflix_detail_rel = ""
            if netflix_detail_src and netflix_detail_hash and netflix_detail_name:
                netflix_detail_rel = os.path.join(
                    VIDEO_LIST_NETFLIX_DETAIL_DEVICE_DIR, netflix_detail_name
                )
                if netflix_detail_rel not in seen:
                    seen.add(netflix_detail_rel)
                    self._append_artwork_copy_if_changed(
                        plan,
                        netflix_detail_src,
                        netflix_detail_rel,
                        key,
                        netflix_detail_hash,
                        device_mount,
                        existing_hashes,
                    )

            netflix_landing_src, netflix_landing_hash, netflix_landing_name, _ = (
                self._video_thumbnails.export_video_list_netflix_poster(
                    row, landing=True
                )
            )
            netflix_landing_rel = ""
            if netflix_landing_src and netflix_landing_hash and netflix_landing_name:
                netflix_landing_rel = os.path.join(
                    VIDEO_LIST_NETFLIX_LANDING_DEVICE_DIR, netflix_landing_name
                )
                if netflix_landing_rel not in seen:
                    seen.add(netflix_landing_rel)
                    self._append_artwork_copy_if_changed(
                        plan,
                        netflix_landing_src,
                        netflix_landing_rel,
                        key,
                        netflix_landing_hash,
                        device_mount,
                        existing_hashes,
                    )

            title = str(row.get("title") or Path(str(row.get("file_path") or "")).stem or "Untitled Video")
            manifest_entries.append(
                {
                    "video_id": video_id,
                    "thumb": os.path.join("thumbs", device_name) if thumb_rel else "",
                    "preview": os.path.join("previews", preview_name) if preview_rel else "",
                    "title": title,
                    "kind": row.get("video_kind") or "movie",
                    "group_key": _video_manifest_group_key(row),
                    "device_path": target["device_path"],
                    "show": row.get("show_title") or "",
                    "season": _manifest_ordinal(row.get("season_number")),
                    "episode": _manifest_ordinal(row.get("episode_number")),
                    "duration": str(int(row.get("duration") or 0)),
                    "locked": "1" if bool(row.get("video_locked")) else "0",
                    "year": row.get("year") or "",
                    "genre": str(row.get("genre") or "")[:48],
                    "rating": row.get("rating") or "",
                    "plot_short": str(row.get("plot_short") or "")[:120],
                    "plot_long": str(row.get("plot_long") or "")[:180],
                    "content_rating": str(row.get("content_rating") or "")[:15],
                    "netflix_poster": os.path.join("netflix", netflix_poster_name)
                    if netflix_poster_rel else "",
                    "netflix_detail": os.path.join("netflix-detail", netflix_detail_name)
                    if netflix_detail_rel else "",
                    "show_art_id": show_art_id,
                    "season_art_id": season_art_id,
                    "show_plot": str(
                        row.get("show_plot")
                        or (row.get("plot_long") if
                            str(row.get("video_kind") or "") == "movie"
                            else "")
                        or ""
                    )[:180],
                }
            )

        manifest_entries = _merge_existing_video_manifest_entries(
            manifest_entries, plan, device_mount
        )
        manifest_src, manifest_hash = self._video_thumbnails.export_video_list_manifest(manifest_entries)
        if manifest_src and manifest_hash:
            self._append_artwork_copy_if_changed(
                plan,
                manifest_src,
                os.path.join(VIDEO_LIST_DEVICE_DIR, "index.tsv"),
                "videolist_manifest",
                manifest_hash,
                device_mount,
                existing_hashes,
            )

        if any(entry.get("locked") == "1" for entry in manifest_entries):
            pin = str(self._config_value("video_locked_pin", "") or "").strip()
            if len(pin) != 4 or not pin.isdigit():
                existing_pin_path = os.path.join(
                    device_mount, VIDEO_LIST_DEVICE_DIR, "locked.pin"
                )
                try:
                    with open(existing_pin_path, "r", encoding="ascii") as handle:
                        existing_pin = handle.read(16).strip()
                except OSError:
                    existing_pin = ""
                if len(existing_pin) == 4 and existing_pin.isdigit():
                    return
                plan.errors.append(
                    "Locked videos require a 4-digit PIN before they can be synced"
                )
                return
            pin_src, pin_hash = self._video_thumbnails.export_locked_video_pin(pin)
            self._append_artwork_copy_if_changed(
                plan,
                pin_src,
                os.path.join(VIDEO_LIST_DEVICE_DIR, "locked.pin"),
                "videolist_locked_pin",
                pin_hash,
                device_mount,
                existing_hashes,
            )

    def _append_artwork_copy_if_changed(self, plan, src, rel_path, album_key, expected_hash, device_mount, existing_hashes):
        device_path = os.path.join(device_mount, rel_path)
        if os.path.exists(device_path):
            cached_hash = existing_hashes.get(device_path)
            if cached_hash is None:
                try:
                    if len(str(expected_hash or "")) == 64:
                        digest = hashlib.sha256()
                        with open(device_path, "rb") as handle:
                            for chunk in iter(
                                lambda: handle.read(1024 * 1024), b""
                            ):
                                digest.update(chunk)
                        cached_hash = digest.hexdigest()
                    else:
                        cached_hash = compute_file_hash(device_path)
                except OSError:
                    cached_hash = ""
                existing_hashes[device_path] = cached_hash
            if cached_hash and cached_hash == expected_hash:
                return
        plan.artwork_to_copy.append((src, rel_path, album_key))

    def _apply_preflight_links(self, links):
        """Persist completed files found during planning as synced device rows."""
        device_key = self._current_device_key
        if not device_key or not links:
            return
        synced_at = datetime.now().isoformat(timespec="seconds")
        with self._db.transaction():
            synced_updates = []
            for row, rel_path in links:
                item = dict(row) if hasattr(row, "keys") else dict(row or {})
                self._upsert_synced_device_row(device_key, item, rel_path, synced_at)
                local_id = item.get("id")
                if local_id:
                    synced_updates.append((
                        local_id,
                        rel_path,
                        item.get("metadata_hash", ""),
                        item.get("file_hash", ""),
                    ))
            self._db.mark_synced_many(synced_updates)
            self._db.mark_device_synced(device_key)
        self.load_cached_device_inventory(device_key)

    def apply_successful_sync_to_cache(self, plan, synced_at=None):
        """Update cached DB device state from a completed sync plan."""
        device_key = self._current_device_key
        if not device_key:
            return
        if synced_at is None:
            synced_at = time.strftime("%Y-%m-%dT%H:%M:%S")

        with self._db.transaction():
            preflight_updates = []
            for row, rel_path in getattr(plan, "preflight_linked", []):
                item = dict(row) if hasattr(row, "keys") else dict(row or {})
                self._upsert_synced_device_row(device_key, item, rel_path, synced_at)
                local_id = item.get("id")
                if local_id:
                    preflight_updates.append((
                        local_id,
                        rel_path,
                        item.get("metadata_hash", ""),
                        item.get("file_hash", ""),
                    ))
            self._db.mark_synced_many(preflight_updates)
            for row, rel_path in plan.to_copy:
                self._upsert_synced_device_row(device_key, row, rel_path, synced_at)
            for row, old_rel_path, rel_path in plan.to_resync:
                if old_rel_path and old_rel_path != rel_path:
                    self._db.execute(
                        "UPDATE device_tracks SET present_on_device = 0 "
                        "WHERE device_id = ? AND device_path = ?",
                        (device_key, old_rel_path),
                    )
                self._upsert_synced_device_row(device_key, row, rel_path, synced_at)
            for rel_path in plan.to_delete:
                self._db.execute(
                    "DELETE FROM device_tracks WHERE device_id = ? AND device_path = ?",
                    (device_key, rel_path),
                )
            self._db.mark_device_synced(device_key)
        if plan.to_delete:
            self._link_device_to_local()
        self.load_cached_device_inventory(device_key)

    def _link_device_to_local(self, media_type=None):
        """Try to set local_track_id on device tracks by matching."""
        device_tracks = self._db.get_all_device_tracks(self._current_device_key)
        local_tracks = self._db.get_all_tracks(media_type=media_type)

        matcher = self._matcher()
        matched, unmatched, _, _ = matcher.match_all(local_tracks, device_tracks)

        synced_updates = []
        for result in matched:
            lt = dict(result.local_track) if hasattr(result.local_track, "keys") else result.local_track
            dt = dict(result.device_track) if hasattr(result.device_track, "keys") else result.device_track
            local_id = lt.get("id")
            dev_id = dt.get("id")
            dev_path = dt.get("device_path", "")
            if local_id and dev_id:
                self._db.execute(
                    "UPDATE device_tracks SET local_track_id = ? WHERE id = ?",
                    (local_id, dev_id),
                )
                # Also mark local track as synced
                mh = dt.get("metadata_hash", "")
                fh = dt.get("file_hash", "")
                synced_updates.append((local_id, dev_path, mh, fh))

        self._db.mark_synced_many(synced_updates)

        for result in unmatched:
            lt = dict(result.local_track) if hasattr(result.local_track, "keys") else result.local_track
            local_id = lt.get("id")
            if local_id:
                linked_elsewhere = self._db.execute(
                    "SELECT 1 FROM device_tracks "
                    "WHERE local_track_id = ? AND present_on_device = 1 LIMIT 1",
                    (local_id,),
                ).fetchone()
                if linked_elsewhere:
                    continue
                self._db.execute(
                    "UPDATE tracks SET synced_to_device = 0, device_path = NULL, "
                    "last_synced_metadata_hash = '', last_synced_file_hash = '' "
                    "WHERE id = ?",
                    (local_id,),
                )

        self._db.commit()

    def _upsert_synced_device_row(self, device_key, row, rel_path, synced_at):
        self._db.upsert_device_track(
            {
                "device_id": device_key,
                "device_path": rel_path,
                "local_track_id": row.get("id"),
                "title": _device_cache_title(row),
                "artist": row.get("artist", ""),
                "album": row.get("album", ""),
                "album_artist": row.get("album_artist", ""),
                "genre": row.get("genre", ""),
                "year": row.get("year"),
                "track_number": row.get("track_number"),
                "disc_number": row.get("disc_number", 1),
                "duration": row.get("duration", 0.0),
                "bitrate": row.get("bitrate", 0),
                "codec": row.get("codec", ""),
                "file_size": row.get("file_size", 0),
                "metadata_hash": row.get("metadata_hash", ""),
                "file_hash": row.get("file_hash", ""),
                "last_synced_metadata_hash": row.get("metadata_hash", ""),
                "last_synced_file_hash": row.get("file_hash", ""),
                "present_on_device": 1,
                "last_synced_at": synced_at,
            }
        )

    def execute_sync(self, plan):
        """Execute a sync plan on a background thread.

        Connect to sync_progress, sync_finished, sync_cancelled, and
        sync_error *before* calling this method.
        """
        device = self._device_detector.current_device
        if not device:
            self.sync_error.emit("No device connected")
            return

        if self.is_syncing:
            self.sync_error.emit("Sync already in progress")
            return

        db_path = self._config.db_path

        self._thread = QThread()
        self._worker = SyncWorker(plan, device.mount_path, db_path)
        self._worker.moveToThread(self._thread)

        self._thread.started.connect(self._worker.run)

        # Worker lives in worker thread; SyncEngine lives in main thread.
        # Qt uses queued connections automatically across threads between
        # QObjects, so these slots execute on the main thread.
        self._worker.progress.connect(
            self._on_worker_progress, Qt.QueuedConnection)
        self._worker.file_copied.connect(
            self._on_worker_file_copied, Qt.QueuedConnection)
        self._worker.file_error.connect(
            self._on_worker_file_error, Qt.QueuedConnection)
        self._worker.finished.connect(
            self._on_worker_finished, Qt.QueuedConnection)
        self._worker.cancelled.connect(
            self._on_worker_cancelled, Qt.QueuedConnection)

        self._thread.start()

    def cancel_sync(self):
        if self._worker:
            self._worker.cancel()

    def shutdown(self):
        if self._worker:
            self._worker.cancel()
        if self._thread and self._thread.isRunning():
            self._thread.quit()
            if not self._thread.wait(15000):
                logger.warning(
                    "Waiting for sync thread to reach a safe cancellation point"
                )
                self._thread.wait()
        self._thread = None
        self._worker = None

    @Slot(int, int, str)
    def _on_worker_progress(self, current, total, desc):
        """Re-emit progress on the main thread."""
        self.sync_progress.emit(current, total, desc)

    @Slot(str, str)
    def _on_worker_file_copied(self, src, dest):
        """Re-emit file_copied on the main thread."""
        self.sync_file_copied.emit(src, dest)

    @Slot(str, str)
    def _on_worker_file_error(self, path, msg):
        """Re-emit file_error on the main thread."""
        self.sync_file_error.emit(path, msg)

    @Slot(int, int, int)
    def _on_worker_finished(self, copied, failed, skipped):
        """Handle sync completion on the main thread."""
        self._device_detector.refresh_device()
        self._cleanup_thread()
        self.sync_finished.emit(copied, failed, skipped)

    @Slot()
    def _on_worker_cancelled(self):
        """Handle sync cancellation on the main thread."""
        self._cleanup_thread()
        self.sync_cancelled.emit()

    def _cleanup_thread(self):
        if self._thread:
            self._thread.quit()
            self._thread.wait(5000)
            self._thread = None
            self._worker = None

    def delete_device_track(self, device_track_row):
        """Delete a single track from the device filesystem and DB."""
        device = self._device_detector.current_device
        if not device:
            return False, "No device connected"

        d = dict(device_track_row) if hasattr(device_track_row, "keys") else device_track_row
        rel = d.get("device_path", "")
        full = os.path.join(device.mount_path, rel)

        if os.path.isfile(full):
            try:
                removed = _remove_media_with_sidecars(full)
                logger.info("Deleted device file(s): %s", ", ".join(removed) if removed else full)
            except OSError as e:
                return False, str(e)

        did = d.get("id")
        if did:
            self._db.delete_device_track(did)

        # Clear synced status on the local track if linked
        lid = d.get("local_track_id")
        if lid:
            self._db.execute(
                "UPDATE tracks SET synced_to_device = 0, device_path = NULL, "
                "last_synced_metadata_hash = '', last_synced_file_hash = '' "
                "WHERE id = ?",
                (lid,),
            )

        self._db.commit()
        return True, "Deleted"

    def find_duplicate_device_tracks(self):
        """Return duplicate device groups relative to the local library."""
        device_tracks = [dict(row) for row in self.get_device_tracks()]
        local_tracks = [dict(row) for row in self._db.get_all_tracks()]
        return self._find_duplicate_groups(local_tracks, device_tracks)

    def remove_duplicate_device_tracks(self):
        """Delete extra duplicate device tracks while keeping one canonical copy."""
        device = self._device_detector.current_device
        if not device:
            return {"success": False, "message": "No device connected", "duplicate_groups": [], "deleted": [], "failures": []}

        duplicates = self.find_duplicate_device_tracks()
        if not duplicates:
            return {"success": True, "message": "No duplicate device tracks found", "duplicate_groups": [], "deleted": [], "failures": []}

        deleted = []
        failures = []
        for group in duplicates:
            for row in group["remove"]:
                rel = row.get("device_path", "")
                if not _looks_like_auto_duplicate_path(rel):
                    failures.append(f"{rel}: skipped; not an automatic duplicate-suffix path")
                    continue
                full = os.path.join(device.mount_path, rel)
                try:
                    _remove_media_with_sidecars(full)
                    did = row.get("id")
                    if did:
                        self._db.delete_device_track(did)
                    deleted.append(rel)
                except OSError as exc:
                    failures.append(f"{rel}: {exc}")

        self._link_device_to_local()
        self._db.commit()
        if self._current_device_key:
            self.load_cached_device_inventory(self._current_device_key)
        return {
            "success": not failures,
            "message": "Duplicate cleanup completed" if not failures else "Duplicate cleanup completed with errors",
            "duplicate_groups": duplicates,
            "deleted": deleted,
            "failures": failures,
        }

    def _find_duplicate_groups(self, local_tracks, device_tracks):
        if not device_tracks:
            return []

        local_group_counts = defaultdict(int)
        for row in local_tracks:
            key = _dedupe_group_key(row)
            if key is not None:
                local_group_counts[key] += 1

        grouped = defaultdict(list)
        for row in device_tracks:
            key = _dedupe_group_key(row)
            if key is not None:
                grouped[key].append(dict(row) if hasattr(row, "keys") else row)

        duplicates = []
        for key, rows in grouped.items():
            expected_count = max(1, local_group_counts.get(key, 0))
            if len(rows) <= expected_count:
                continue
            ordered = sorted(rows, key=self._duplicate_rank, reverse=True)
            keep = ordered[:expected_count]
            remove = ordered[expected_count:]
            duplicates.append(
                {
                    "group_key": key,
                    "expected_count": expected_count,
                    "device_count": len(rows),
                    "keep": keep,
                    "remove": remove,
                }
            )
        duplicates.sort(
            key=lambda item: (
                item["keep"][0].get("artist", ""),
                item["keep"][0].get("album", ""),
                item["keep"][0].get("title", ""),
            )
        )
        return duplicates

    @staticmethod
    def _duplicate_rank(row):
        item = dict(row) if hasattr(row, "keys") else row
        linked = 1 if item.get("local_track_id") else 0
        synced_path = 1 if linked and item.get("device_path") else 0
        file_hash = 1 if item.get("file_hash") else 0
        metadata_hash = 1 if item.get("metadata_hash") else 0
        path = item.get("device_path", "")
        canonical_path = 0 if _looks_like_auto_duplicate_path(path) else 1
        return (
            _audio_codec_rank(item),
            canonical_path,
            linked,
            synced_path,
            file_hash,
            metadata_hash,
            -len(path),
            path,
        )


class _PlanningDeviceDetector:
    def __init__(self, device):
        self.current_device = device

    @property
    def is_connected(self):
        return self.current_device is not None


class SyncPlanWorker(QObject):
    finished = Signal(object)
    error = Signal(str)
    status = Signal(str)

    def __init__(
        self,
        db_path,
        config,
        device,
        artwork_cache_dir,
        track_ids=None,
        force_full=False,
        weather_only=False,
        media_type="audio",
        video_profile=None,
    ):
        super().__init__()
        self._db_path = db_path
        self._config = config
        self._device = device
        self._artwork_cache_dir = artwork_cache_dir
        self._track_ids = set(track_ids) if track_ids else None
        self._force_full = force_full
        self._weather_only = bool(weather_only)
        self._media_type = media_type
        self._video_profile = video_profile

    @Slot()
    def run(self):
        db = None
        artwork = None
        try:
            from services.artwork_manager import ArtworkManager

            db = Database(self._db_path, initialize=False)
            artwork = ArtworkManager(self._artwork_cache_dir, self._config)
            detector = _PlanningDeviceDetector(self._device)
            engine = SyncEngine(db, self._config, detector, artwork)
            if self._weather_only:
                plan = engine.build_weather_sync_plan(status_callback=self.status.emit)
            else:
                plan = engine.build_sync_plan(
                    track_ids=self._track_ids,
                    force_full=self._force_full,
                    media_type=self._media_type,
                    video_profile=self._video_profile,
                    status_callback=self.status.emit,
                )
            self.finished.emit(plan)
        except Exception as exc:
            logger.exception("Sync planning failed")
            self.error.emit(str(exc))
        finally:
            if artwork:
                artwork.shutdown()
            if db:
                db.close()


class SyncPlanBuilder(QObject):
    finished = Signal(object)
    error = Signal(str)
    status = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._thread = None
        self._worker = None

    @property
    def is_running(self):
        return self._thread is not None and self._thread.isRunning()

    def start(
        self,
        db_path,
        config,
        device,
        artwork_cache_dir,
        track_ids=None,
        force_full=False,
        weather_only=False,
        media_type="audio",
        video_profile=None,
    ):
        if self.is_running:
            return False
        self._thread = QThread()
        self._worker = SyncPlanWorker(
            db_path,
            config,
            device,
            artwork_cache_dir,
            track_ids=track_ids,
            force_full=force_full,
            weather_only=weather_only,
            media_type=media_type,
            video_profile=video_profile,
        )
        self._worker.moveToThread(self._thread)
        self._thread.started.connect(self._worker.run)
        self._worker.finished.connect(self._on_finished, Qt.QueuedConnection)
        self._worker.error.connect(self._on_error, Qt.QueuedConnection)
        self._worker.status.connect(self.status, Qt.QueuedConnection)
        self._thread.start()
        return True

    @Slot(object)
    def _on_finished(self, plan):
        self._cleanup()
        self.finished.emit(plan)

    @Slot(str)
    def _on_error(self, message):
        self._cleanup()
        self.error.emit(message)

    def _cleanup(self):
        if self._thread:
            self._thread.quit()
            self._thread.wait(3000)
        if self._worker:
            try:
                self._worker.deleteLater()
            except RuntimeError:
                pass
        if self._thread:
            try:
                self._thread.deleteLater()
            except RuntimeError:
                pass
        self._worker = None
        self._thread = None
