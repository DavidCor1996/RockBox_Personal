"""Artwork extraction, caching, online lookup, and device cover export."""

import hashlib
import io
import json
import logging
import os
import re
import threading
import time
import unicodedata
from collections import deque
from pathlib import Path

from PIL import Image, ImageOps
from PySide6.QtCore import QObject, Signal

from app.config import ARTWORK_FILENAMES, ARTWORK_THUMB_SIZE, ARTWORK_DISPLAY_SIZE
from services.metadata_reader import extract_artwork_data
from services.imdb_video_artwork import IMDbVideoArtworkCatalog
from services.online_artwork import (
    ITunesArtworkLookup,
    OnlineArtworkForbiddenError,
    OnlineArtworkNetworkError,
    OnlineArtworkRateLimitError,
)
from services.file_safety import atomic_write_json, atomic_write_text

logger = logging.getLogger(__name__)
_DESKTOP_MIN_SOURCE_DIMENSION = max(ARTWORK_DISPLAY_SIZE) * 2
_MIN_EFFECTIVE_LOOKUP_INTERVAL_SECONDS = 60.0
_VIDEO_POSTER_THUMB_SIZE = (180, 270)
_VIDEO_POSTER_DISPLAY_SIZE = (360, 540)
_ALBUM_LIST_THUMB_SIZE = (40, 40)
_ALBUM_LIST_SLIDE_SIZE = (384, 384)
_VIDEO_POSTER_FILENAMES = (
    "poster.jpg", "poster.png", "Poster.jpg", "Poster.png",
    "movie.jpg", "movie.png", "Movie.jpg", "Movie.png",
    "show.jpg", "show.png", "Show.jpg", "Show.png",
)
_QUERY_PUNCT_TRANSLATION = str.maketrans(
    {
        "\u2018": "'",
        "\u2019": "'",
        "\u201a": "'",
        "\u201b": "'",
        "\u2032": "'",
        "\u201c": '"',
        "\u201d": '"',
        "\u201e": '"',
        "\u2033": '"',
        "\u2010": "-",
        "\u2011": "-",
        "\u2012": "-",
        "\u2013": "-",
        "\u2014": "-",
        "\u2212": "-",
        "\u00a0": " ",
    }
)


class ArtworkManager(QObject):
    """Manages artwork extraction and a disk-backed thumbnail cache."""

    _gc_lock = threading.Lock()

    album_artwork_updated = Signal(str, str)   # album_key, artwork_path
    artwork_lookup_failed = Signal(str, str)   # album_key, reason
    artwork_lookup_status = Signal(str)

    def __init__(self, cache_dir, config=None, parent=None, lookup_client=None):
        super().__init__(parent)
        self._cache_dir = cache_dir
        self._config = config
        os.makedirs(cache_dir, exist_ok=True)
        self._thumb_dir = os.path.join(cache_dir, "thumbs")
        self._display_dir = os.path.join(cache_dir, "display")
        self._album_dir = os.path.join(cache_dir, "albums")
        self._original_dir = os.path.join(self._album_dir, "originals")
        self._meta_dir = os.path.join(self._album_dir, "meta")
        self._device_dir = os.path.join(self._album_dir, "device")
        self._albumlist_dir = os.path.join(self._album_dir, "albumlist")
        self._albumlist_slide_dir = os.path.join(self._albumlist_dir, "slides")
        self._wps_dir = os.path.join(self._album_dir, "wps")
        os.makedirs(self._thumb_dir, exist_ok=True)
        os.makedirs(self._display_dir, exist_ok=True)
        os.makedirs(self._original_dir, exist_ok=True)
        os.makedirs(self._meta_dir, exist_ok=True)
        os.makedirs(self._device_dir, exist_ok=True)
        os.makedirs(self._albumlist_dir, exist_ok=True)
        os.makedirs(self._albumlist_slide_dir, exist_ok=True)
        os.makedirs(self._wps_dir, exist_ok=True)

        storefront = getattr(config, "online_artwork_storefront", "us") if config else "us"
        interval = self._effective_artwork_interval(config)
        self._lookup = lookup_client or ITunesArtworkLookup(storefront=storefront, min_interval_seconds=interval)
        self._imdb_artwork = IMDbVideoArtworkCatalog()
        self._fetch_lock = threading.Lock()
        self._queued_fetches = set()
        self._high_queue = deque()
        self._medium_queue = deque()
        self._low_queue = deque()
        self._overflow_high = deque()
        self._overflow_medium = deque()
        self._overflow_low = deque()
        self._queue_event = threading.Event()
        self._stop_event = threading.Event()
        self._cooldown_until = 0.0
        self._last_request_finished_at = 0.0
        self._manual_threads = set()
        self._maybe_cleanup_cache()
        self._worker = threading.Thread(target=self._queue_worker, name="rockpod-artwork-queue", daemon=True)
        self._worker.start()
        self._artwork_diagnostics = {
            "albums_without_artwork": [],
            "artwork_failures": [],
            "album_keys": {},
        }

    def shutdown(self):
        self._stop_event.set()
        self._queue_event.set()
        if self._worker.is_alive():
            self._worker.join(timeout=2.0)
        for thread in list(self._manual_threads):
            if thread.is_alive():
                thread.join(timeout=2.0)

    def set_config(self, config):
        self._config = config

    def _maybe_cleanup_cache(self):
        max_mb = self._config_value("artwork_cache_max_mb", 1024)
        interval_hours = self._config_value("artwork_cache_cleanup_interval_hours", 24)
        try:
            max_bytes = max(0, int(float(max_mb) * 1024 * 1024))
            interval_seconds = max(0.0, float(interval_hours) * 3600.0)
        except (TypeError, ValueError):
            return
        if max_bytes <= 0:
            return

        stamp_path = os.path.join(self._cache_dir, ".rockpod-artwork-gc.json")
        try:
            with open(stamp_path, "r", encoding="utf-8") as handle:
                last_run = float((json.load(handle) or {}).get("last_run", 0.0) or 0.0)
        except (OSError, ValueError, TypeError):
            last_run = 0.0
        if interval_seconds and time.time() - last_run < interval_seconds:
            return

        with self._gc_lock:
            result = self.cleanup_cache(max_bytes=max_bytes)
            try:
                atomic_write_json(
                    stamp_path,
                    {
                        "last_run": time.time(),
                        "max_bytes": max_bytes,
                        "bytes_after": result["bytes_after"],
                        "removed_count": len(result["removed"]),
                    },
                )
            except OSError as exc:
                logger.debug("Could not update artwork cache cleanup stamp: %s", exc)

    def cleanup_cache(self, max_bytes=None, dry_run=False):
        """Evict only unreferenced, regenerable files from managed cache dirs."""
        if max_bytes is None:
            try:
                max_bytes = int(float(self._config_value("artwork_cache_max_mb", 1024)) * 1024 * 1024)
            except (TypeError, ValueError):
                max_bytes = 0
        max_bytes = max(0, int(max_bytes or 0))
        referenced = self._artwork_cache_references()
        managed_roots = {
            self._thumb_dir,
            self._display_dir,
            self._original_dir,
            self._device_dir,
            self._albumlist_dir,
            self._albumlist_slide_dir,
            self._wps_dir,
        }
        all_files = {}
        candidates = {}
        for root, _dirs, files in os.walk(self._cache_dir):
            for name in files:
                path = os.path.abspath(os.path.join(root, name))
                try:
                    stat = os.stat(path)
                except OSError:
                    continue
                all_files[path] = int(stat.st_size)
                if not any(self._is_within(path, managed) for managed in managed_roots):
                    continue
                if path in referenced or name.startswith("placeholder_") or name == "index.tsv":
                    continue
                candidates[path] = (int(stat.st_mtime_ns), int(stat.st_size))

        bytes_before = sum(all_files.values())
        bytes_after = bytes_before
        removed = []
        errors = []
        if max_bytes and bytes_after > max_bytes:
            for path, (_mtime_ns, size) in sorted(candidates.items(), key=lambda item: item[1][0]):
                if bytes_after <= max_bytes:
                    break
                if not dry_run:
                    try:
                        os.remove(path)
                    except OSError as exc:
                        errors.append({"path": path, "error": str(exc)})
                        continue
                removed.append(path)
                bytes_after -= size
        if removed:
            logger.info(
                "Artwork cache cleanup removed %d unreferenced files (%d -> %d bytes)",
                len(removed),
                bytes_before,
                bytes_after,
            )
        return {
            "max_bytes": max_bytes,
            "bytes_before": bytes_before,
            "bytes_after": bytes_after,
            "removed": removed,
            "errors": errors,
            "dry_run": bool(dry_run),
        }

    def _artwork_cache_references(self):
        referenced = set()

        def visit(value):
            if isinstance(value, dict):
                for nested in value.values():
                    visit(nested)
            elif isinstance(value, (list, tuple)):
                for nested in value:
                    visit(nested)
            elif isinstance(value, str) and value:
                path = os.path.abspath(value)
                if self._is_within(path, self._cache_dir):
                    referenced.add(path)

        try:
            names = os.listdir(self._meta_dir)
        except OSError:
            names = []
        for name in names:
            if not name.endswith(".json"):
                continue
            try:
                with open(os.path.join(self._meta_dir, name), "r", encoding="utf-8") as handle:
                    visit(json.load(handle))
            except (OSError, ValueError, TypeError):
                continue
        return referenced

    @staticmethod
    def _is_within(path, root):
        try:
            return os.path.commonpath((os.path.abspath(path), os.path.abspath(root))) == os.path.abspath(root)
        except ValueError:
            return False

    def get_artwork_path(self, track_row, size="thumb"):
        """Return path to desktop artwork for a track, preferring album-level hi-res art."""
        if self._get_value(track_row, "media_type", "audio") == "video":
            poster = self.get_video_poster(
                self._video_info_for_track(track_row),
                size=size,
                allow_online=False,
            )
            return poster or self._placeholder_path(size)

        album_info = self._album_info([track_row])
        source_path = self._ensure_album_source(album_info, allow_online=False)
        if self._config_value("enable_online_artwork_lookup", False):
            self._queue_if_low_quality(album_info, priority="medium")
        if source_path:
            rendered = self._render_album_variant(album_info["group_key"], source_path, size)
            if rendered:
                return rendered

        file_path = self._get_value(track_row, "file_path", "")
        if not file_path:
            return self._placeholder_path(size)

        cache_key = hashlib.md5(file_path.encode()).hexdigest()
        target_dir = self._thumb_dir if size == "thumb" else self._display_dir
        cached = os.path.join(target_dir, f"{cache_key}.jpg")
        if os.path.exists(cached):
            return cached

        img_data = self._get_raw_artwork(track_row)
        if img_data is None:
            return self._placeholder_path(size)
        rendered = self._render_image_bytes(img_data, cached, size)
        return rendered or self._placeholder_path(size)

    def _video_info_for_track(self, track_row):
        video_kind = str(self._get_value(track_row, "video_kind", "") or "movie")
        if video_kind == "show":
            show_title = (
                self._get_value(track_row, "show_title", "")
                or self._get_value(track_row, "artist", "")
                or self._get_value(track_row, "album_artist", "")
                or "Unknown Show"
            )
            season_number = int(
                self._get_value(track_row, "season_number", 0) or 0
            )
            scope = str(
                self._get_value(track_row, "_video_artwork_scope", "")
                or ("season" if season_number else "show")
            )
            identity = str(
                self._get_value(track_row, "imdb_id", "")
                or show_title
            ).strip().casefold()
            group_key = (
                f"show:{identity}:season:{season_number}"
                if scope == "season" and season_number
                else f"show:{identity}"
            )
            return {
                "group_key": group_key,
                "album": str(show_title),
                "artist": f"Season {season_number}" if season_number else "",
                "tracks": [track_row],
                "media_type": "video",
                "video_kind": "show",
                "video_scope": scope,
            }

        title = (
            self._get_value(track_row, "title", "")
            or self._get_value(track_row, "album", "")
            or "Untitled Video"
        )
        return {
            "group_key": str(
                self._get_value(track_row, "video_group_key", "")
                or self._get_value(track_row, "file_path", "")
                or title
            ),
            "album": str(title),
            "artist": str(
                self._get_value(track_row, "artist", "")
                or self._get_value(track_row, "album_artist", "")
            ),
            "tracks": [track_row],
            "media_type": "video",
            "video_kind": video_kind,
            "video_scope": video_kind,
        }

    def get_artwork_for_album(self, album_or_tracks, size="display", allow_online=None):
        """Get artwork for an album using cache, local sources, then optional online lookup."""
        album_info = self._album_info(album_or_tracks)
        album_key = album_info["group_key"]
        self._record_album_identity(album_info)

        if allow_online is None:
            allow_online = self._config_value("enable_online_artwork_lookup", False)
        source_path = self._ensure_album_source(album_info, allow_online=False)
        if allow_online:
            self._queue_if_low_quality(album_info, priority="high")

        if source_path:
            rendered = self._render_album_variant(album_key, source_path, size)
            if rendered:
                return rendered

        if allow_online:
            self.queue_online_lookup(album_info, priority="high")

        self._record_album_without_artwork(album_key, album_info["tracks"])
        return self._placeholder_path(size)

    def get_video_poster(self, video_or_tracks, size="thumb", allow_online=None):
        """Return a cached/local/online poster path for a video group when available."""
        video_info = self._album_info(video_or_tracks)
        video_key = video_info["group_key"]
        self._record_album_identity(video_info)

        if allow_online is None:
            # Video poster lookups should be explicit per item, not silently
            # triggered by rendering the video grid.
            allow_online = False
        source_path = self._ensure_video_source(video_info, allow_online=False)
        if allow_online:
            self._queue_if_low_quality(video_info, priority="medium")

        if source_path:
            if self._video_variant_meta_is_stale(video_key):
                self._render_video_variant(video_key, source_path, "thumb")
                self._render_video_variant(video_key, source_path, "display")
            rendered = self._render_video_variant(video_key, source_path, size)
            if rendered:
                return rendered

        if allow_online:
            self.queue_online_lookup(video_info, priority="medium")
        return ""

    def set_manual_video_poster(self, video_key, url_or_bytes):
        """Manages manually assigning/downloading a poster for a video_key."""
        meta = self._load_album_meta(video_key)
        desktop_path = os.path.join(self._original_dir, f"{self._slug(video_key)}_manual.jpg")
        try:
            if isinstance(url_or_bytes, bytes):
                data = url_or_bytes
            else:
                data, mime = self._lookup.download_image(url_or_bytes)
            if data:
                with open(desktop_path, "wb") as handle:
                    handle.write(data)
                resolution = self._image_resolution_from_bytes(data)
                meta.update({
                    "desktop_source_art_path": desktop_path,
                    "desktop_source_type": "manual",
                    "desktop_source_resolution": resolution,
                    "source": "manual",
                    "online_download_succeeded": True
                })
                self._save_album_meta(video_key, meta)
                # Force render variants
                self._render_video_variant(video_key, desktop_path, "thumb")
                self._render_video_variant(video_key, desktop_path, "display")
                return desktop_path
        except Exception as e:
            logger.error(f"Failed to set manual video poster: {str(e)}")
        return ""

    def _video_variant_meta_is_stale(self, video_key):
        meta = self._load_album_meta(video_key)
        for key in ("desktop_thumb_path", "desktop_display_path"):
            path = str(meta.get(key) or "")
            if not path:
                continue
            name = os.path.basename(path)
            if name.startswith("album_"):
                return True
        return False

    def refresh_missing_artwork(self, albums, force=False):
        albums = list(albums or [])
        if not self._config_value("enable_online_artwork_lookup", False):
            logger.info("Artwork refresh skipped: online artwork lookup is disabled")
            self.artwork_lookup_status.emit("Online artwork lookup is disabled")
            return {"queued": 0, "skipped": len(albums), "reason": "disabled", "skip_reasons": {"disabled": len(albums)}}
        if not self._config_value("background_artwork_lookup_enabled", True) and not force:
            logger.info("Artwork refresh skipped: background artwork lookup is disabled")
            self.artwork_lookup_status.emit("Background artwork lookup is disabled")
            return {"queued": 0, "skipped": len(albums), "reason": "background_disabled", "skip_reasons": {"background_disabled": len(albums)}}

        queued = 0
        skipped = 0
        skip_reasons = {}
        for album in albums:
            accepted, queue_reason = self.queue_online_lookup(self._album_info(album), force=force, priority="low", with_reason=True)
            if accepted:
                queued += 1
            else:
                skipped += 1
                skip_reasons[queue_reason] = skip_reasons.get(queue_reason, 0) + 1
        reason_summary = " ".join(f"{key}={value}" for key, value in sorted(skip_reasons.items()))
        if reason_summary:
            logger.info("Artwork refresh queued=%d skipped=%d %s", queued, skipped, reason_summary)
        else:
            logger.info("Artwork refresh queued=%d skipped=%d", queued, skipped)
        if queued:
            self.artwork_lookup_status.emit(f"Fetching artwork... ({queued} albums queued)")
        else:
            self.artwork_lookup_status.emit("No artwork lookups were queued")
        return {"queued": queued, "skipped": skipped, "reason": "ok", "skip_reasons": skip_reasons}

    def fetch_online_artwork_now(self, album_or_tracks, force=False):
        album_info = self._album_info(album_or_tracks)
        return self._perform_lookup(album_info, force=force)

    def start_online_lookup_now(self, album_or_tracks, force=False, with_reason=False):
        album_info = self._album_info(album_or_tracks)
        album_key = album_info["group_key"]
        meta = self._load_album_meta(album_key)
        if (
            not force
            and meta.get("online_status") == "success"
            and meta.get("desktop_source_type") == "online"
            and os.path.exists(meta.get("desktop_source_art_path", ""))
        ):
            return (False, "already_cached") if with_reason else False

        with self._fetch_lock:
            if album_key in self._queued_fetches:
                payload = self._remove_queued_payload_locked(album_key)
                if payload is None:
                    return (True, "already_running") if with_reason else True
            else:
                self._queued_fetches.add(album_key)
            thread = threading.Thread(
                target=self._run_manual_lookup,
                args=(album_info, force),
                name=f"rockpod-artwork-now-{self._slug(album_key)[:8]}",
                daemon=True,
            )
            self._manual_threads.add(thread)
            thread.start()
        return (True, "started") if with_reason else True

    def _run_manual_lookup(self, album_info, force):
        thread = threading.current_thread()
        try:
            self._perform_lookup(album_info, force=force)
        finally:
            with self._fetch_lock:
                self._manual_threads.discard(thread)

    def export_device_cover(self, album_or_tracks, force=False):
        album_info = self._album_info(album_or_tracks)
        source_path = self._ensure_album_source(album_info, allow_online=False)
        if not source_path:
            return "", ""

        album_key = album_info["group_key"]
        meta = self._load_album_meta(album_key)
        source_hash = self._file_hash(source_path)
        cover_path = os.path.join(self._device_dir, f"{self._slug(album_key)}.jpg")
        if (
            not force
            and meta.get("device_cover_export_path") == cover_path
            and meta.get("device_cover_hash") == source_hash
            and os.path.exists(cover_path)
        ):
            return cover_path, source_hash

        try:
            size = int(self._config_value("device_cover_art_size", 320) or 320)
            quality = int(self._config_value("device_cover_art_quality", 85) or 85)
            with Image.open(source_path) as img:
                source_resolution = [int(img.width), int(img.height)]
                img = img.convert("RGB")
                img.thumbnail((size, size), Image.LANCZOS)
                exported_resolution = [int(img.width), int(img.height)]
                img.save(cover_path, "JPEG", quality=quality, progressive=False, optimize=False)
        except Exception as e:
            self._record_artwork_failure(source_path, f"device cover export failure: {e}")
            return "", ""

        meta["device_cover_export_path"] = cover_path
        meta["device_cover_hash"] = source_hash
        meta["device_cover_resolution"] = exported_resolution
        meta["device_cover_from_high_res"] = (
            source_resolution[0] > exported_resolution[0] or source_resolution[1] > exported_resolution[1]
        )
        self._save_album_meta(album_key, meta)
        return cover_path, source_hash

    def export_rockbox_wps_cover(self, album_or_tracks, size, force=False, fit_mode=None):
        album_info = self._album_info(album_or_tracks)
        source_path = self._ensure_album_source(album_info, allow_online=False)
        if not source_path:
            return "", ""

        target_size = self._normalize_wps_cover_size(size)
        if not target_size:
            return "", ""
        fit = str(fit_mode or self._config_value("wps_cover_fit_mode", "contain") or "contain").strip().lower()
        if fit not in {"contain", "cover"}:
            fit = "contain"

        album_key = album_info["group_key"]
        meta = self._load_album_meta(album_key)
        source_hash = self._file_hash(source_path)
        width, height = target_size
        cover_name = f"{self._slug(album_key)}_{width}x{height}_{fit}.bmp"
        cover_path = os.path.join(self._wps_dir, cover_name)
        meta_key = f"{width}x{height}:{fit}"
        exports = dict(meta.get("wps_cover_exports") or {})
        cached = dict(exports.get(meta_key) or {})
        if (
            not force
            and cached.get("path") == cover_path
            and cached.get("source_hash") == source_hash
            and os.path.exists(cover_path)
        ):
            return cover_path, cached.get("output_hash") or self._file_hash(cover_path)

        try:
            with Image.open(source_path) as img:
                img = img.convert("RGB")
                if fit == "cover":
                    rendered = ImageOps.fit(img, target_size, Image.LANCZOS)
                else:
                    img.thumbnail(target_size, Image.LANCZOS)
                    rendered = Image.new("RGB", target_size, "#000000")
                    left = (width - img.width) // 2
                    top = (height - img.height) // 2
                    rendered.paste(img, (left, top))
                rendered.save(cover_path, "BMP")
        except Exception as e:
            self._record_artwork_failure(source_path, f"WPS cover export failure: {e}")
            return "", ""

        output_hash = self._file_hash(cover_path)
        exports[meta_key] = {
            "path": cover_path,
            "source_hash": source_hash,
            "output_hash": output_hash,
            "dimensions": [width, height],
            "fit_mode": fit,
        }
        meta["wps_cover_exports"] = exports
        self._save_album_meta(album_key, meta)
        return cover_path, output_hash

    def export_album_list_thumbnail(self, album_or_tracks, force=False, size=None):
        album_info = self._album_info(album_or_tracks)
        source_path = self._ensure_album_source(album_info, allow_online=False)
        if not source_path:
            return "", "", "", ""

        target_size = self._album_list_thumb_size(size)
        album_key = album_info["group_key"]
        album_id = self.album_list_id(album_key)
        meta = self._load_album_meta(album_key)
        source_hash = self._file_hash(source_path)
        thumb_name = f"{album_id}_{target_size[0]}x{target_size[1]}_{source_hash[:12]}.bmp"
        thumb_path = os.path.join(self._albumlist_dir, thumb_name)
        if (
            not force
            and meta.get("album_list_thumb_path") == thumb_path
            and meta.get("album_list_thumb_source_hash") == source_hash
            and os.path.exists(thumb_path)
        ):
            return thumb_path, self._file_hash(thumb_path), f"{album_id}.bmp", album_id

        try:
            with Image.open(source_path) as img:
                img = img.convert("RGB")
                img.thumbnail(target_size, Image.LANCZOS)
                canvas = Image.new("RGB", target_size, "#000000")
                left = (target_size[0] - img.width) // 2
                top = (target_size[1] - img.height) // 2
                canvas.paste(img, (left, top))
                canvas.save(thumb_path, "BMP")
        except Exception as e:
            self._record_artwork_failure(source_path, f"album list thumbnail export failure: {e}")
            return "", "", "", album_id

        meta["album_list_thumb_path"] = thumb_path
        meta["album_list_thumb_source_hash"] = source_hash
        meta["album_list_thumb_resolution"] = [target_size[0], target_size[1]]
        self._save_album_meta(album_key, meta)
        return thumb_path, self._file_hash(thumb_path), f"{album_id}.bmp", album_id

    def export_album_list_slide(self, album_or_tracks, force=False, size=None):
        album_info = self._album_info(album_or_tracks)
        source_path = self._ensure_album_source(album_info, allow_online=False)
        if not source_path:
            return "", "", "", ""

        target_size = self._album_list_slide_size(size)
        album_key = album_info["group_key"]
        album_id = self.album_list_id(album_key)
        meta = self._load_album_meta(album_key)
        source_hash = self._file_hash(source_path)
        slide_name = f"{album_id}_{target_size[0]}x{target_size[1]}_{source_hash[:12]}.bmp"
        slide_path = os.path.join(self._albumlist_slide_dir, slide_name)
        if (
            not force
            and meta.get("album_list_slide_path") == slide_path
            and meta.get("album_list_slide_source_hash") == source_hash
            and os.path.exists(slide_path)
        ):
            return slide_path, self._file_hash(slide_path), f"{album_id}.bmp", album_id

        try:
            with Image.open(source_path) as img:
                img = img.convert("RGB")
                rendered = ImageOps.fit(img, target_size, Image.LANCZOS)
                rendered.save(slide_path, "BMP")
        except Exception as e:
            self._record_artwork_failure(source_path, f"album list slide export failure: {e}")
            return "", "", "", album_id

        meta["album_list_slide_path"] = slide_path
        meta["album_list_slide_source_hash"] = source_hash
        meta["album_list_slide_resolution"] = [target_size[0], target_size[1]]
        self._save_album_meta(album_key, meta)
        return slide_path, self._file_hash(slide_path), f"{album_id}.bmp", album_id

    def export_album_list_manifest(self, entries):
        rows = []
        for entry in entries:
            album_id = str(entry.get("album_id") or "").strip()
            if not album_id:
                continue
            rows.append(
                {
                    "album_id": album_id,
                    "thumb": self._manifest_field(entry.get("thumb", "")),
                    "slide": self._manifest_field(entry.get("slide", "")),
                    "artist": self._manifest_field(entry.get("artist", "")),
                    "album": self._manifest_field(entry.get("album", "")),
                    "group_key": self._manifest_field(entry.get("group_key", "")),
                    "device_dirs": self._manifest_field(entry.get("device_dirs", "")),
                }
            )
        rows.sort(key=lambda item: (item["artist"].casefold(), item["album"].casefold(), item["album_id"]))
        lines = [
            "# rockpod albumlist v2",
            "album_id\tthumb\tslide\tartist\talbum\tgroup_key\tdevice_dirs",
        ]
        lines.extend(
            "\t".join(
                (
                    row["album_id"],
                    row["thumb"],
                    row["slide"],
                    row["artist"],
                    row["album"],
                    row["group_key"],
                    row["device_dirs"],
                )
            )
            for row in rows
        )
        data = "\n".join(lines) + "\n"
        manifest_path = os.path.join(self._albumlist_dir, "index.tsv")
        try:
            if os.path.exists(manifest_path):
                with open(manifest_path, "r", encoding="utf-8") as handle:
                    if handle.read() == data:
                        return manifest_path, self._file_hash(manifest_path)
            atomic_write_text(manifest_path, data)
            return manifest_path, self._file_hash(manifest_path)
        except OSError as e:
            logger.warning("Failed to write album list manifest: %s", e)
            return "", ""

    def album_artwork_diagnostics(self):
        return {
            "albums_without_artwork": list(self._artwork_diagnostics["albums_without_artwork"]),
            "artwork_failures": list(self._artwork_diagnostics["artwork_failures"]),
            "album_keys": dict(self._artwork_diagnostics["album_keys"]),
        }

    def inspect_artwork(self, album_or_tracks):
        album_info = self._album_info(album_or_tracks)
        album_key = album_info["group_key"]
        meta = self._load_album_meta(album_key)
        self._record_album_identity(album_info)
        info = dict(self._artwork_diagnostics["album_keys"].get(album_key, {}))
        info.update(
            {
                "desktop_source_art_path": meta.get("desktop_source_art_path", ""),
                "desktop_source_type": meta.get("desktop_source_type", ""),
                "desktop_source_resolution": meta.get("desktop_source_resolution", []),
                "desktop_thumb_path": meta.get("desktop_thumb_path", ""),
                "desktop_thumb_resolution": meta.get("desktop_thumb_resolution", []),
                "desktop_display_path": meta.get("desktop_display_path", ""),
                "desktop_display_resolution": meta.get("desktop_display_resolution", []),
                "device_cover_export_path": meta.get("device_cover_export_path", ""),
                "device_cover_resolution": meta.get("device_cover_resolution", []),
                "device_cover_from_high_res": bool(meta.get("device_cover_from_high_res", False)),
                "online_lookup_attempted": bool(meta.get("online_lookup_attempted", False)),
                "online_query": meta.get("online_query", ""),
                "online_match_found": bool(meta.get("online_match_found", False)),
                "online_selected_url": meta.get("online_selected_url", ""),
                "online_selected_size": meta.get("online_selected_size", ""),
                "online_download_succeeded": bool(meta.get("online_download_succeeded", False)),
                "online_cached_path": meta.get("online_cached_path", ""),
                "online_status": meta.get("online_status", ""),
                "online_error": meta.get("online_error", ""),
                "display_is_hi_res_thumb": self._is_rendered_from_higher_res(
                    meta.get("desktop_source_resolution", []),
                    meta.get("desktop_display_resolution", []),
                ),
            }
        )
        return info

    def queue_online_lookup(self, album_or_tracks, force=False, priority="high", with_reason=False, manual=False):
        if not manual and not self._config_value("enable_online_artwork_lookup", False):
            return (False, "disabled") if with_reason else False
        if (
            not manual
            and not force
            and priority == "low"
            and not self._config_value("background_artwork_lookup_enabled", True)
        ):
            return (False, "background_disabled") if with_reason else False
        album_info = self._album_info(album_or_tracks)
        album_key = album_info["group_key"]
        meta = self._load_album_meta(album_key)
        if (
            not force
            and meta.get("online_status") == "success"
            and meta.get("desktop_source_type") == "online"
            and os.path.exists(meta.get("desktop_source_art_path", ""))
        ):
            return (False, "already_cached") if with_reason else False
        if not force and not self._should_retry_online_fetch(meta):
            return (False, "cooldown") if with_reason else False
        with self._fetch_lock:
            if album_key in self._queued_fetches:
                if manual and self._promote_queued_lookup_locked(album_key, album_info, force):
                    queue_size = self._queue_size_locked(include_overflow=True)
                    logger.info("Artwork queue promote album_key=%s priority=high queue_size=%d", album_key, queue_size)
                    self._queue_event.set()
                    return (True, "promoted") if with_reason else True
                return (False, "already_queued") if with_reason else False
            self._queued_fetches.add(album_key)
            if priority == "low":
                self._enqueue_locked(self._low_queue, self._overflow_low, (album_info, force))
            elif priority == "medium":
                self._enqueue_locked(self._medium_queue, self._overflow_medium, (album_info, force))
            else:
                self._enqueue_locked(self._high_queue, self._overflow_high, (album_info, force))
            queue_size = self._queue_size_locked(include_overflow=True)
        logger.info("Artwork queue add album_key=%s priority=%s queue_size=%d", album_key, priority, queue_size)
        self._queue_event.set()
        return (True, "queued") if with_reason else True

    def _promote_queued_lookup_locked(self, album_key, album_info, force):
        payload = self._remove_queued_payload_locked(album_key)
        if payload is None:
            return False
        existing_info, existing_force = payload
        self._high_queue.appendleft((album_info or existing_info, force or existing_force))
        return True

    @staticmethod
    def _payload_album_key(payload):
        if not payload:
            return ""
        info = payload[0] if isinstance(payload, tuple) and payload else {}
        if hasattr(info, "get"):
            return str(info.get("group_key") or "")
        return ""

    def _remove_queued_payload_locked(self, album_key):
        for queue in (
            self._high_queue,
            self._medium_queue,
            self._low_queue,
            self._overflow_high,
            self._overflow_medium,
            self._overflow_low,
        ):
            for payload in list(queue):
                if self._payload_album_key(payload) == album_key:
                    queue.remove(payload)
                    return payload
        return None

    def _queue_worker(self):
        while not self._stop_event.is_set():
            self._queue_event.wait(timeout=0.5)
            if self._stop_event.is_set():
                break
            payload = None
            with self._fetch_lock:
                payload = self._dequeue_locked()
                if payload is None:
                    self._queue_event.clear()
            if payload is None:
                continue

            now = time.time()
            if self._cooldown_until > now:
                pause = self._cooldown_until - now
                logger.warning("Artwork lookup paused due to rate limiting for %.1fs queue_size=%d", pause, self._queue_size())
                self.artwork_lookup_status.emit("Artwork lookup paused due to rate limiting")
                if self._stop_event.wait(timeout=pause):
                    break

            min_interval = self._effective_artwork_interval(self._config)
            since_last = time.time() - self._last_request_finished_at
            if since_last < min_interval:
                if self._stop_event.wait(timeout=min_interval - since_last):
                    break

            album_info, force = payload
            self.artwork_lookup_status.emit("Fetching artwork...")
            self._perform_lookup(album_info, force=force)
            self._last_request_finished_at = time.time()

    def _perform_lookup(self, album_info, force=False):
        album_key = album_info["group_key"]
        try:
            is_video = album_info.get("media_type") == "video"
            if is_video:
                variants = self._video_lookup_variants(album_info)
            else:
                variants = self._lookup_variants(album_info["album"], album_info["artist"])
            effective_interval = self._effective_artwork_interval(self._config)
            requests_per_second = 1.0 / max(0.001, effective_interval)
            logger.info(
                "Artwork lookup start album=%s artist=%s variants=%d queue_size=%d requests_per_second=%.2f",
                album_info["album"],
                album_info["artist"],
                len(variants),
                self._queue_size(),
                requests_per_second,
            )
            result = None
            query = ""
            query_album = ""
            query_artist = ""
            for query_album, query_artist in variants:
                query = f"{query_album} {query_artist}".strip()
                logger.info("Artwork lookup try album_key=%s query=%s", album_key, query)
                if is_video:
                    if hasattr(self._lookup, "search_video_art_from_info"):
                        result = self._lookup.search_video_art_from_info(album_info, limit=8, relaxed=False)
                    else:
                        result = self._lookup.search_video_art(
                            query_album,
                            video_kind=album_info.get("video_kind", "movie"),
                            season_label=query_artist,
                        )
                    if not result and force and hasattr(self._lookup, "search_video_art_relaxed"):
                        if hasattr(self._lookup, "search_video_art_from_info"):
                            result = self._lookup.search_video_art_from_info(album_info, limit=20, relaxed=True)
                        else:
                            result = self._lookup.search_video_art_relaxed(
                                query_album,
                                video_kind=album_info.get("video_kind", "movie"),
                                season_label=query_artist,
                            )
                else:
                    result = self._lookup.search_album_art(query_album, query_artist)
                if result:
                    break
            meta = self._load_album_meta(album_key)
            meta["last_attempt_at"] = self._timestamp()
            meta["online_lookup_attempted"] = True
            meta["online_query"] = query
            if not result:
                meta["online_status"] = "failed"
                meta["online_error"] = "no confident match"
                meta["online_match_found"] = False
                meta["online_selected_url"] = ""
                meta["online_selected_size"] = ""
                meta["online_download_succeeded"] = False
                meta["next_retry_at"] = ""
                self._save_album_meta(album_key, meta)
                logger.info("Artwork lookup miss album_key=%s query=%s", album_key, query)
                self.artwork_lookup_failed.emit(album_key, "no confident match")
                return ""

            download_url = result["artwork_url"] if self._config_value("fetch_hires_online_artwork", True) else result["preview_url"]
            raw, content_type = self._lookup.download_image(download_url)
            ext = ".jpg" if "png" not in content_type else ".png"
            original_path = os.path.join(self._original_dir, f"{self._slug(album_key)}{ext}")
            with open(original_path, "wb") as handle:
                handle.write(raw)
            source_resolution = self._image_resolution(original_path)

            meta.update(
                {
                    "desktop_source_art_path": original_path,
                    "desktop_source_type": "online",
                    "desktop_source_resolution": source_resolution,
                    "source": "online",
                    "source_provenance": result["source"],
                    "online_status": "success",
                    "online_error": "",
                    "online_match_found": True,
                    "online_selected_url": download_url,
                    "online_selected_size": f"{source_resolution[0]}x{source_resolution[1]}" if source_resolution else "",
                    "online_download_succeeded": True,
                    "online_cached_path": original_path,
                    "next_retry_at": "",
                    "retry_count": 0,
                    "last_success_at": self._timestamp(),
                    "last_attempt_at": self._timestamp(),
                }
            )
            self._save_album_meta(album_key, meta)
            logger.info(
                "Artwork lookup success album_key=%s url=%s cached=%s resolution=%s queue_size=%d",
                album_key,
                download_url,
                original_path,
                source_resolution,
                self._queue_size(),
            )
            if is_video:
                display_path = self._render_video_variant(album_key, original_path, "display")
                self._render_video_variant(album_key, original_path, "thumb")
            else:
                display_path = self._render_album_variant(album_key, original_path, "display")
                self._render_album_variant(album_key, original_path, "thumb")
            if is_video:
                self.artwork_lookup_status.emit(f"Fetched poster for {album_info['album']}")
            else:
                self.artwork_lookup_status.emit(f"Fetched artwork for {album_info['artist']} - {album_info['album']}")
            self.album_artwork_updated.emit(album_key, display_path or original_path)
            return original_path
        except OnlineArtworkRateLimitError as e:
            self._handle_lookup_backoff(album_info, "rate_limited", str(e), force=force)
            return ""
        except OnlineArtworkForbiddenError as e:
            self._handle_lookup_backoff(album_info, "forbidden", str(e), force=force)
            return ""
        except OnlineArtworkNetworkError as e:
            self._handle_lookup_backoff(album_info, "network_error", str(e), force=force)
            return ""
        except Exception as e:
            meta = self._load_album_meta(album_key)
            meta["last_attempt_at"] = self._timestamp()
            meta["online_status"] = "failed"
            meta["online_error"] = str(e)
            meta["online_lookup_attempted"] = True
            meta["online_download_succeeded"] = False
            self._save_album_meta(album_key, meta)
            self._record_artwork_failure(album_info.get("album", ""), f"online lookup failure: {e}")
            self.artwork_lookup_failed.emit(album_key, str(e))
            return ""
        finally:
            with self._fetch_lock:
                self._queued_fetches.discard(album_key)

    def _ensure_album_source(self, album_info, allow_online=False):
        album_key = album_info["group_key"]
        meta = self._load_album_meta(album_key)
        self._record_album_identity(album_info)

        img_data, source_path, source_kind, source_resolution = self._get_best_local_album_artwork(album_info["tracks"])
        if img_data:
            source_hash = hashlib.md5(img_data).hexdigest()
            cached_desktop = meta.get("desktop_source_art_path", "")
            cached_resolution = meta.get("desktop_source_resolution", [])
            if (
                cached_desktop
                and os.path.exists(cached_desktop)
                and meta.get("source_provenance") == source_path
                and meta.get("desktop_source_hash") == source_hash
                and (not allow_online or not self._is_low_quality_resolution(cached_resolution))
            ):
                return cached_desktop

            ext = self._ext_for_source(source_path)
            desktop_path = os.path.join(self._original_dir, f"{self._slug(album_key)}_local{ext}")
            try:
                with open(desktop_path, "wb") as handle:
                    handle.write(img_data)
                meta.update(
                    {
                        "desktop_source_art_path": desktop_path,
                        "desktop_source_hash": source_hash,
                        "desktop_source_type": source_kind,
                        "desktop_source_resolution": source_resolution,
                        "source": source_kind,
                        "source_provenance": source_path,
                    }
                )
                self._save_album_meta(album_key, meta)
                if allow_online and self._config_value("enable_online_artwork_lookup", False) and self._is_low_quality_resolution(source_resolution):
                    self.queue_online_lookup(album_info)
                return desktop_path
            except OSError as e:
                self._record_artwork_failure(source_path, f"cache write failure: {e}")

        cached_desktop = meta.get("desktop_source_art_path", "")
        cached_resolution = meta.get("desktop_source_resolution", [])
        if cached_desktop and os.path.exists(cached_desktop):
            if not allow_online or not self._is_low_quality_resolution(cached_resolution):
                return cached_desktop

        if allow_online:
            self.queue_online_lookup(album_info)
        return ""

    def _ensure_video_source(self, video_info, allow_online=False):
        video_key = video_info["group_key"]
        meta = self._load_album_meta(video_key)
        self._record_album_identity(video_info)

        cached_desktop = meta.get("desktop_source_art_path", "")
        cached_resolution = meta.get("desktop_source_resolution", [])
        if (
            meta.get("desktop_source_type") == "manual"
            and cached_desktop
            and os.path.exists(cached_desktop)
        ):
            return cached_desktop

        img_data, source_path, source_kind, source_resolution = self._get_best_local_video_artwork(video_info)
        if img_data:
            ext = self._ext_for_source(source_path)
            desktop_path = os.path.join(self._original_dir, f"{self._slug(video_key)}_local{ext}")
            try:
                with open(desktop_path, "wb") as handle:
                    handle.write(img_data)
                meta.update(
                    {
                        "desktop_source_art_path": desktop_path,
                        "desktop_source_type": source_kind,
                        "desktop_source_resolution": source_resolution,
                        "source": source_kind,
                        "source_provenance": source_path,
                    }
                )
                self._save_album_meta(video_key, meta)
                if allow_online and self._config_value("enable_online_artwork_lookup", False) and self._is_low_quality_resolution(source_resolution):
                    self.queue_online_lookup(video_info)
                return desktop_path
            except OSError as e:
                self._record_artwork_failure(source_path, f"cache write failure: {e}")

        if cached_desktop and os.path.exists(cached_desktop):
            return cached_desktop

        if allow_online:
            self.queue_online_lookup(video_info)
        return ""

    def _get_best_local_album_artwork(self, tracks):
        best = None
        for track in tracks:
            img_data, source_path = self._get_album_artwork_bytes(track)
            if img_data:
                kind = "folder" if source_path.lower().endswith((".jpg", ".jpeg", ".png")) and source_path != self._get_value(track, "file_path", "") else "embedded"
                resolution = self._image_resolution_from_bytes(img_data)
                candidate = (img_data, source_path, kind, resolution)
                if best is None or self._resolution_area(resolution) > self._resolution_area(best[3]):
                    best = candidate
        if best is not None:
            return best
        return None, "", "", []

    def _get_best_local_video_artwork(self, video_info):
        best = None
        scope = str(video_info.get("video_scope") or "")

        # An explicit library assignment or verified IMDb catalog match is
        # authoritative.  Do not let a larger embedded frame or stale online
        # cache replace a deliberately identified title/season poster.
        for track in video_info.get("tracks") or []:
            explicit = str(self._get_value(track, "artwork_path", "") or "")
            catalog_path, catalog_entry = self._imdb_artwork.resolve(
                track, scope=scope
            )
            catalog_first = (
                scope in {"show", "season"}
                or str(self._get_value(track, "metadata_source", "") or "")
                .casefold() == "imdb"
            )
            candidates = (
                ((catalog_path, "imdb_catalog"), (explicit, "explicit"))
                if catalog_first else
                ((explicit, "explicit"), (catalog_path, "imdb_catalog"))
            )
            for source_path, source_kind in candidates:
                if not source_path or not os.path.isfile(source_path):
                    continue
                try:
                    with open(source_path, "rb") as handle:
                        data = handle.read()
                except OSError as e:
                    self._record_artwork_failure(
                        source_path, f"video artwork read failure: {e}"
                    )
                    continue
                return (
                    data,
                    source_path,
                    source_kind,
                    self._image_resolution_from_bytes(data),
                )

        # Check embedded artwork first (Apple/iTunes embedded artwork is extremely common in store-downloaded videos!)
        for track in video_info.get("tracks") or []:
            if self._get_value(track, "has_embedded_artwork", 0):
                file_path = self._get_value(track, "file_path", "")
                if file_path and os.path.isfile(file_path):
                    try:
                        data, mime = extract_artwork_data(file_path)
                        if data:
                            resolution = self._image_resolution_from_bytes(data)
                            current = (data, file_path, "embedded", resolution)
                            if best is None or self._resolution_area(resolution) > self._resolution_area(best[3]):
                                best = current
                    except Exception as e:
                        logger.debug("Failed to extract embedded artwork from video %s: %s", file_path, e)

        for candidate in self._video_artwork_candidates(video_info):
            if not os.path.isfile(candidate):
                continue
            try:
                with open(candidate, "rb") as handle:
                    data = handle.read()
            except OSError as e:
                self._record_artwork_failure(candidate, f"video artwork read failure: {e}")
                continue
            resolution = self._image_resolution_from_bytes(data)
            current = (data, candidate, "folder", resolution)
            if best is None or self._resolution_area(resolution) > self._resolution_area(best[3]):
                best = current
        if best is not None:
            return best
        return None, "", "", []

    def _render_album_variant(self, album_key, source_path, size):
        target_dir = self._thumb_dir if size == "thumb" else self._display_dir
        meta = self._load_album_meta(album_key)
        source_hash = self._file_hash(source_path)
        cached = os.path.join(target_dir, f"album_{self._slug(album_key)}_{source_hash[:12]}.jpg")
        meta_key = "desktop_thumb_path" if size == "thumb" else "desktop_display_path"
        resolution_key = "desktop_thumb_resolution" if size == "thumb" else "desktop_display_resolution"
        if os.path.exists(cached):
            meta[meta_key] = cached
            meta[resolution_key] = self._image_resolution(cached)
            self._save_album_meta(album_key, meta)
            return cached
        try:
            target_size = ARTWORK_THUMB_SIZE if size == "thumb" else ARTWORK_DISPLAY_SIZE
            with Image.open(source_path) as img:
                img = img.convert("RGB")
                img.thumbnail(target_size, Image.LANCZOS)
                rendered_resolution = [int(img.width), int(img.height)]
                img.save(cached, "JPEG", quality=85, progressive=False, optimize=False)
            meta[meta_key] = cached
            meta[resolution_key] = rendered_resolution
            self._save_album_meta(album_key, meta)
            return cached
        except Exception as e:
            self._record_artwork_failure(source_path, f"thumbnail generation failure: {e}")
            return ""

    def _render_video_variant(self, video_key, source_path, size):
        target_dir = self._thumb_dir if size == "thumb" else self._display_dir
        meta = self._load_album_meta(video_key)
        source_hash = self._file_hash(source_path)
        cached = os.path.join(target_dir, f"video_{self._slug(video_key)}_{source_hash[:12]}.jpg")
        meta_key = "desktop_thumb_path" if size == "thumb" else "desktop_display_path"
        resolution_key = "desktop_thumb_resolution" if size == "thumb" else "desktop_display_resolution"
        if os.path.exists(cached):
            meta[meta_key] = cached
            meta[resolution_key] = self._image_resolution(cached)
            self._save_album_meta(video_key, meta)
            return cached
        try:
            target_size = _VIDEO_POSTER_THUMB_SIZE if size == "thumb" else _VIDEO_POSTER_DISPLAY_SIZE
            with Image.open(source_path) as img:
                img = img.convert("RGB")
                img = self._crop_to_aspect(img, target_size)
                rendered_resolution = [int(img.width), int(img.height)]
                img.save(cached, "JPEG", quality=85, progressive=False, optimize=False)
            meta[meta_key] = cached
            meta[resolution_key] = rendered_resolution
            self._save_album_meta(video_key, meta)
            return cached
        except Exception as e:
            self._record_artwork_failure(source_path, f"video poster generation failure: {e}")
            return ""

    def _render_image_bytes(self, img_data, cached, size):
        try:
            target_size = ARTWORK_THUMB_SIZE if size == "thumb" else ARTWORK_DISPLAY_SIZE
            img = Image.open(io.BytesIO(img_data))
            img = img.convert("RGB")
            img.thumbnail(target_size, Image.LANCZOS)
            img.save(cached, "JPEG", quality=85, progressive=False, optimize=False)
            return cached
        except Exception as e:
            logger.debug("Failed to cache artwork at %s: %s", cached, e)
            return ""

    @staticmethod
    def _crop_to_aspect(img, target_size):
        target_width, target_height = target_size
        source_ratio = img.width / max(img.height, 1)
        target_ratio = target_width / max(target_height, 1)
        if source_ratio > target_ratio:
            scaled_height = target_height
            scaled_width = int(target_height * source_ratio)
        else:
            scaled_width = target_width
            scaled_height = int(target_width / max(source_ratio, 0.01))
        img = img.resize((scaled_width, scaled_height), Image.LANCZOS)
        left = max((scaled_width - target_width) // 2, 0)
        top = max((scaled_height - target_height) // 2, 0)
        return img.crop((left, top, left + target_width, top + target_height))

    def _get_raw_artwork(self, track_row):
        file_path = self._get_value(track_row, "file_path", "")
        if not file_path:
            return None
        has_embedded = self._get_value(track_row, "has_embedded_artwork", 0)

        if has_embedded:
            data, _ = extract_artwork_data(file_path)
            if data:
                return data
            self._record_artwork_failure(file_path, "embedded artwork missing or extraction failed")

        folder = os.path.dirname(file_path)
        for name in ARTWORK_FILENAMES:
            art_path = os.path.join(folder, name)
            if os.path.isfile(art_path):
                try:
                    with open(art_path, "rb") as f:
                        return f.read()
                except OSError:
                    continue
        return None

    def _get_album_artwork_bytes(self, track_row):
        file_path = self._get_value(track_row, "file_path", "")
        if not file_path:
            return None, ""

        explicit_artwork = self._get_value(track_row, "artwork_path", None)
        if explicit_artwork and os.path.exists(explicit_artwork):
            try:
                with open(explicit_artwork, "rb") as f:
                    return f.read(), explicit_artwork
            except OSError as e:
                self._record_artwork_failure(explicit_artwork, f"explicit artwork read failure: {e}")

        folder = os.path.dirname(file_path)
        for name in ARTWORK_FILENAMES:
            art_path = os.path.join(folder, name)
            if os.path.isfile(art_path):
                try:
                    with open(art_path, "rb") as f:
                        return f.read(), art_path
                except OSError as e:
                    self._record_artwork_failure(art_path, f"folder artwork read failure: {e}")

        has_embedded = self._get_value(track_row, "has_embedded_artwork", 0)
        if has_embedded:
            data, _mime = extract_artwork_data(file_path)
            if data:
                return data, file_path
            self._record_artwork_failure(file_path, "embedded artwork missing or extraction failed")

        return None, file_path

    def _album_info(self, album_or_tracks):
        if isinstance(album_or_tracks, dict) and "tracks" in album_or_tracks:
            tracks = list(album_or_tracks.get("tracks") or [])
            group_key = str(album_or_tracks.get("group_key") or self._album_cache_key(tracks))
            if "album" in album_or_tracks and album_or_tracks.get("album") is not None:
                album = album_or_tracks.get("album")
            else:
                album = self._get_value(tracks[0] if tracks else {}, "album", "Unknown Album")
            if "artist" in album_or_tracks and album_or_tracks.get("artist") is not None:
                artist = album_or_tracks.get("artist")
            else:
                artist = (
                    self._get_value(tracks[0] if tracks else {}, "album_artist", "")
                    or self._get_value(tracks[0] if tracks else {}, "artist", "Unknown Artist")
                )
            media_type = album_or_tracks.get("media_type") or self._get_value(tracks[0] if tracks else {}, "media_type", "audio")
            video_kind = album_or_tracks.get("video_kind") or self._get_value(tracks[0] if tracks else {}, "video_kind", "")
            video_scope = album_or_tracks.get("video_scope") or self._get_value(tracks[0] if tracks else {}, "_video_scope", "")
            return {
                "group_key": group_key,
                "album": album or "Unknown Album",
                "artist": artist if artist is not None else "Unknown Artist",
                "tracks": tracks,
                "media_type": media_type or "audio",
                "video_kind": video_kind or "",
                "video_scope": video_scope or "",
            }
        tracks = list(album_or_tracks or [])
        first = tracks[0] if tracks else {}
        return {
            "group_key": self._album_cache_key(tracks),
            "album": self._get_value(first, "album", "Unknown Album") or "Unknown Album",
            "artist": self._get_value(first, "album_artist", "") or self._get_value(first, "artist", "Unknown Artist") or "Unknown Artist",
            "tracks": tracks,
            "media_type": self._get_value(first, "media_type", "audio") or "audio",
            "video_kind": self._get_value(first, "video_kind", "") or "",
            "video_scope": self._get_value(first, "_video_scope", "") or "",
        }

    def _record_album_identity(self, album_info):
        album_key = album_info["group_key"]
        self._artwork_diagnostics["album_keys"][album_key] = {
            "album_key": album_key,
            "album": album_info.get("album", ""),
            "artist": album_info.get("artist", ""),
            "track_count": len(album_info["tracks"]),
        }

    def _queue_if_low_quality(self, album_info, priority):
        album_key = album_info["group_key"]
        meta = self._load_album_meta(album_key)
        path = meta.get("desktop_source_art_path", "")
        resolution = meta.get("desktop_source_resolution", [])
        if not path or not os.path.exists(path) or self._is_low_quality_resolution(resolution):
            self.queue_online_lookup(album_info, priority=priority)

    def _normalized_lookup_terms(self, album_title, artist_name):
        album = self._clean_lookup_value(album_title)
        artist = self._clean_lookup_value(artist_name)
        album = self._trim_trailing_artist_suffix(album, artist)
        return album or "Unknown Album", artist

    def _lookup_variants(self, album_title, artist_name):
        raw_album = self._clean_lookup_value(album_title, preserve_punctuation=True)
        raw_artist = self._clean_lookup_value(artist_name, preserve_punctuation=False)
        cleaned_album, cleaned_artist = self._normalized_lookup_terms(album_title, artist_name)
        variants = []
        candidates = [
            (raw_album, raw_artist),
            (cleaned_album, cleaned_artist),
            (self._strip_bracket_suffix(cleaned_album), cleaned_artist),
            (self._strip_leading_artist_prefix(cleaned_album, cleaned_artist), cleaned_artist),
        ]
        candidates.extend(self._combined_lookup_candidates(raw_album, raw_artist))
        candidates.extend(self._combined_lookup_candidates(cleaned_album, cleaned_artist))
        seen = set()
        for album, artist in candidates:
            album = (album or "").strip()
            artist = (artist or "").strip()
            if not album:
                continue
            key = (album.casefold(), artist.casefold())
            if key in seen:
                continue
            seen.add(key)
            variants.append((album, artist))
        return variants or [("Unknown Album", "")]

    def _combined_lookup_candidates(self, album, artist):
        if (artist or "").strip():
            return []
        text = (album or "").strip()
        if not text:
            return []
        variants = []
        for pattern in (r"\s+[-–—]\s+", r"\s*:\s*", r"\s+\|\s+"):
            parts = re.split(pattern, text, maxsplit=1)
            if len(parts) != 2:
                continue
            candidate_artist = self._clean_lookup_value(parts[0], preserve_punctuation=True)
            candidate_album = self._clean_lookup_value(parts[1], preserve_punctuation=True)
            if not candidate_album or not candidate_artist:
                continue
            variants.append((candidate_album, candidate_artist))
        return variants

    def _video_lookup_variants(self, video_info):
        title = self._clean_lookup_value(video_info.get("album"), preserve_punctuation=True)
        subtitle = self._clean_lookup_value(video_info.get("artist"), preserve_punctuation=True)
        stripped = self._strip_bracket_suffix(title)
        variants = []
        seen = set()
        for candidate_title, candidate_subtitle in (
            (title, subtitle),
            (title, ""),
            (stripped, subtitle),
            (stripped, ""),
        ):
            candidate_title = (candidate_title or "").strip()
            candidate_subtitle = (candidate_subtitle or "").strip()
            if not candidate_title:
                continue
            key = (candidate_title.casefold(), candidate_subtitle.casefold())
            if key in seen:
                continue
            seen.add(key)
            variants.append((candidate_title, candidate_subtitle))
        return variants or [("Unknown Video", "")]

    def _trim_trailing_artist_suffix(self, album, artist):
        if not album:
            return ""
        parts = re.split(r"\s*[-–—]\s*", album)
        if len(parts) < 2:
            return album
        suffix = self._clean_lookup_value(parts[-1])
        artist_clean = self._clean_lookup_value(artist)
        suffix_words = [word for word in suffix.split() if word]
        edition_words = {"edition", "deluxe", "remaster", "remastered", "expanded", "bonus", "mono", "stereo", "live", "version"}
        if suffix and artist_clean and (suffix == artist_clean or suffix in artist_clean or artist_clean in suffix):
            return " - ".join(parts[:-1]).strip()
        if (
            2 <= len(suffix_words) <= 4
            and all(word.isalpha() for word in suffix_words)
            and not any(word.lower() in edition_words for word in suffix_words)
        ):
            return " - ".join(parts[:-1]).strip()
        return album

    def _clean_lookup_value(self, value, preserve_punctuation=False):
        if not value:
            return ""
        value = unicodedata.normalize("NFKC", str(value)).translate(_QUERY_PUNCT_TRANSLATION)
        value = value.replace("_", " ")
        value = re.sub(r"\b(feat|ft|featuring)\.?\b.*$", "", value, flags=re.IGNORECASE)
        value = re.sub(r"[\"`]+", " ", value)
        if preserve_punctuation:
            value = re.sub(r"[^\w\s&'(),?!-]", " ", value)
        else:
            value = re.sub(r"[^\w\s&-]", " ", value)
        value = re.sub(r"\s*&\s*", " and ", value)
        value = re.sub(r"\s+", " ", value)
        return value.strip()

    def _strip_bracket_suffix(self, album):
        if not album:
            return ""
        stripped = re.sub(r"\s*[\[(].*?[\])]\s*$", "", album).strip()
        return stripped or album

    def _strip_leading_artist_prefix(self, album, artist):
        if not album or not artist:
            return album
        album_clean = album.casefold()
        artist_clean = artist.casefold()
        if album_clean.startswith(artist_clean + " "):
            return album[len(artist):].strip(" -:")
        return album

    def _video_artwork_candidates(self, video_info):
        tracks = list(video_info.get("tracks") or [])
        scope = str(video_info.get("video_scope") or "")
        seen = set()
        candidates = []
        for track in tracks:
            file_path_text = self._get_value(track, "file_path", "")
            if not file_path_text:
                continue
            file_path = Path(file_path_text)
            stem_variants = [file_path.with_name(f"{file_path.stem}.jpg"), file_path.with_name(f"{file_path.stem}.png")]
            for candidate in stem_variants:
                key = str(candidate)
                if key not in seen:
                    seen.add(key)
                    candidates.append(key)

            directories = [file_path.parent]
            parent_dir = file_path.parent.parent if file_path.parent.parent != file_path.parent else file_path.parent
            if scope == "show":
                if parent_dir not in directories:
                    directories.append(parent_dir)
            for directory in directories:
                season_number = int(self._get_value(track, "season_number", 0) or 0)
                if season_number:
                    for name in (
                        f"season{season_number:02d}-poster.jpg",
                        f"season{season_number:02d}-poster.png",
                        f"season{season_number}-poster.jpg",
                        f"season{season_number}-poster.png",
                    ):
                        candidate = str(directory / name)
                        if candidate not in seen:
                            seen.add(candidate)
                            candidates.append(candidate)
                for name in list(_VIDEO_POSTER_FILENAMES) + list(ARTWORK_FILENAMES):
                    candidate = str(directory / name)
                    if candidate not in seen:
                        seen.add(candidate)
                        candidates.append(candidate)
        return candidates

    @staticmethod
    def _effective_artwork_interval(config):
        if config is None:
            return _MIN_EFFECTIVE_LOOKUP_INTERVAL_SECONDS
        try:
            configured = float(getattr(config, "online_artwork_min_interval_seconds", _MIN_EFFECTIVE_LOOKUP_INTERVAL_SECONDS) or _MIN_EFFECTIVE_LOOKUP_INTERVAL_SECONDS)
        except (TypeError, ValueError):
            configured = _MIN_EFFECTIVE_LOOKUP_INTERVAL_SECONDS
        return max(_MIN_EFFECTIVE_LOOKUP_INTERVAL_SECONDS, configured)

    def _album_cache_key(self, tracks):
        if not tracks:
            return "unknown"
        first = tracks[0]
        if hasattr(first, "get"):
            key = first.get("album_group_key") or first.get("group_key")
            if key:
                return str(key)
        album = self._get_value(first, "album", "Unknown Album") or "Unknown Album"
        artist = self._get_value(first, "album_artist", "") or self._get_value(first, "artist", "") or "Unknown Artist"
        return f"{artist}\0{album}"

    def _should_retry_online_fetch(self, meta):
        next_retry_at = meta.get("next_retry_at")
        if next_retry_at:
            try:
                if self._timestamp_float() < float(next_retry_at):
                    return False
            except (TypeError, ValueError):
                pass
        if not meta.get("last_attempt_at"):
            return True
        retry_seconds = float(self._config_value("online_artwork_failure_retry_seconds", 3600) or 3600)
        try:
            last = float(meta["last_attempt_at"])
        except (TypeError, ValueError):
            return True
        return (self._timestamp_float() - last) >= retry_seconds

    def _handle_lookup_backoff(self, album_info, status, reason, force=False):
        album_key = album_info["group_key"]
        meta = self._load_album_meta(album_key)
        retry_count = int(meta.get("retry_count", 0) or 0) + 1
        delay = self._retry_delay_seconds(status, retry_count)
        next_retry = self._timestamp_float() + delay
        meta.update(
            {
                "last_attempt_at": self._timestamp(),
                "online_status": status,
                "online_error": reason,
                "online_lookup_attempted": True,
                "online_download_succeeded": False,
                "retry_count": retry_count,
                "next_retry_at": str(next_retry),
            }
        )
        self._save_album_meta(album_key, meta)
        self._record_artwork_failure(album_info.get("album", ""), f"online lookup {status}: {reason}")
        logger.warning(
            "Artwork lookup backoff album_key=%s status=%s retry=%d delay=%.1fs queue_size=%d",
            album_key,
            status,
            retry_count,
            delay,
            self._queue_size(),
        )
        if status == "rate_limited":
            self._cooldown_until = max(self._cooldown_until, next_retry)
            self.artwork_lookup_status.emit("Artwork lookup paused due to rate limiting")
        elif status == "forbidden":
            self._cooldown_until = max(self._cooldown_until, next_retry)
            self.artwork_lookup_status.emit("Artwork lookup paused due to rate limiting")
        self.artwork_lookup_failed.emit(album_key, reason)
        if retry_count < 4 or force:
            with self._fetch_lock:
                self._queued_fetches.discard(album_key)
            self.queue_online_lookup(album_info, force=True, priority="low")

    @staticmethod
    def _retry_delay_seconds(status, retry_count):
        retry_count = max(1, int(retry_count))
        if status == "rate_limited":
            delays = [15.0, 60.0, 300.0]
            return delays[min(retry_count - 1, len(delays) - 1)]
        if status == "forbidden":
            return max(300.0, min(3600.0, 300.0 * retry_count))
        delays = [30.0, 120.0, 300.0]
        return delays[min(retry_count - 1, len(delays) - 1)]

    def _queue_size(self):
        with self._fetch_lock:
            return self._queue_size_locked(include_overflow=True)

    def _queue_size_locked(self, include_overflow=False):
        active = len(self._high_queue) + len(self._medium_queue) + len(self._low_queue)
        if not include_overflow:
            return active
        return active + len(self._overflow_high) + len(self._overflow_medium) + len(self._overflow_low)

    def _active_queue_limit(self):
        return max(1, int(self._config_value("online_artwork_max_queue_size", 50) or 50))

    def _enqueue_locked(self, active_queue, overflow_queue, payload):
        if self._queue_size_locked(include_overflow=False) < self._active_queue_limit():
            active_queue.append(payload)
        else:
            overflow_queue.append(payload)

    def _refill_queues_locked(self):
        while self._queue_size_locked(include_overflow=False) < self._active_queue_limit():
            if self._overflow_high:
                self._high_queue.append(self._overflow_high.popleft())
            elif self._overflow_medium:
                self._medium_queue.append(self._overflow_medium.popleft())
            elif self._overflow_low:
                self._low_queue.append(self._overflow_low.popleft())
            else:
                break

    def _dequeue_locked(self):
        self._refill_queues_locked()
        if self._high_queue:
            return self._high_queue.popleft()
        if self._medium_queue:
            return self._medium_queue.popleft()
        if self._low_queue:
            return self._low_queue.popleft()
        return None

    def _load_album_meta(self, album_key):
        path = self._meta_path(album_key)
        if not os.path.exists(path):
            return {}
        try:
            with open(path, "r", encoding="utf-8") as handle:
                return json.load(handle)
        except (OSError, json.JSONDecodeError):
            return {}

    def _save_album_meta(self, album_key, meta):
        path = self._meta_path(album_key)
        data = dict(meta)
        data["album_key"] = album_key
        atomic_write_json(path, data)

    def _meta_path(self, album_key):
        return os.path.join(self._meta_dir, f"{self._slug(album_key)}.json")

    def _placeholder_path(self, size):
        target_dir = self._thumb_dir if size == "thumb" else self._display_dir
        target_size = ARTWORK_THUMB_SIZE if size == "thumb" else ARTWORK_DISPLAY_SIZE
        path = os.path.join(target_dir, f"placeholder_{target_size[0]}x{target_size[1]}.jpg")
        if os.path.exists(path):
            return path

        try:
            img = Image.new("RGB", target_size, "#d8d8d8")
            img.save(path, "JPEG", quality=85, progressive=False, optimize=False)
            return path
        except Exception as e:
            logger.debug("Failed to create placeholder artwork: %s", e)
            return ""

    def _record_album_without_artwork(self, album_key, tracks, limit=200):
        if len(self._artwork_diagnostics["albums_without_artwork"]) >= limit:
            return
        first = tracks[0] if tracks else {}
        item = {
            "album_key": album_key,
            "album": self._get_value(first, "album", ""),
            "artist": self._get_value(first, "album_artist", "") or self._get_value(first, "artist", ""),
        }
        if item not in self._artwork_diagnostics["albums_without_artwork"]:
            self._artwork_diagnostics["albums_without_artwork"].append(item)

    def _record_artwork_failure(self, path, reason, limit=500):
        if len(self._artwork_diagnostics["artwork_failures"]) >= limit:
            return
        self._artwork_diagnostics["artwork_failures"].append({"path": path, "reason": reason})

    @staticmethod
    def album_list_id(album_key):
        text = unicodedata.normalize("NFKC", str(album_key or "")).strip().casefold()
        text = re.sub(r"\s+", " ", text)
        return hashlib.sha1(text.encode("utf-8")).hexdigest()[:24]

    @staticmethod
    def _album_list_thumb_size(size):
        if size is None:
            return _ALBUM_LIST_THUMB_SIZE
        try:
            value = int(size)
        except (TypeError, ValueError):
            return _ALBUM_LIST_THUMB_SIZE
        value = max(16, min(96, value))
        return value, value

    @staticmethod
    def _album_list_slide_size(size):
        if size is None:
            return _ALBUM_LIST_SLIDE_SIZE
        try:
            value = int(size)
        except (TypeError, ValueError):
            return _ALBUM_LIST_SLIDE_SIZE
        value = max(160, min(384, value))
        return value, value

    @staticmethod
    def _normalize_wps_cover_size(size):
        if isinstance(size, str):
            parts = re.split(r"[xX, ]+", size.strip())
        else:
            parts = list(size or [])
        if len(parts) < 2:
            return None
        try:
            width = int(parts[0])
            height = int(parts[1])
        except (TypeError, ValueError):
            return None
        if width <= 0 or height <= 0 or width > 800 or height > 800:
            return None
        return width, height

    @staticmethod
    def _manifest_field(value):
        text = unicodedata.normalize("NFC", str(value or ""))
        return re.sub(r"[\x00\t\r\n]+", " ", text).strip()

    def _config_value(self, key, default=None):
        if self._config is None:
            return default
        getter = getattr(self._config, "get", None)
        if callable(getter):
            return getter(key, default)
        return getattr(self._config, key, default)

    @staticmethod
    def _get_value(track_row, key, default=None):
        if track_row is None:
            return default
        if hasattr(track_row, "get"):
            return track_row.get(key, default)
        if hasattr(track_row, "__getitem__") and hasattr(track_row, "keys"):
            return track_row[key] if key in track_row.keys() else default
        return getattr(track_row, key, default)

    @staticmethod
    def _slug(value):
        return hashlib.md5(str(value).encode("utf-8")).hexdigest()

    @staticmethod
    def _timestamp():
        return str(ArtworkManager._timestamp_float())

    @staticmethod
    def _timestamp_float():
        import time
        return time.time()

    @staticmethod
    def _file_hash(path):
        digest = hashlib.md5()
        with open(path, "rb") as handle:
            while True:
                chunk = handle.read(65536)
                if not chunk:
                    break
                digest.update(chunk)
        return digest.hexdigest()

    @staticmethod
    def _ext_for_source(source_path):
        ext = Path(source_path or "").suffix.lower()
        if ext in (".png",):
            return ".png"
        return ".jpg"

    @staticmethod
    def _image_resolution(path):
        if not path or not os.path.exists(path):
            return []
        try:
            with Image.open(path) as img:
                return [int(img.width), int(img.height)]
        except Exception:
            return []

    @staticmethod
    def _image_resolution_from_bytes(img_data):
        if not img_data:
            return []
        try:
            with Image.open(io.BytesIO(img_data)) as img:
                return [int(img.width), int(img.height)]
        except Exception:
            return []

    @staticmethod
    def _resolution_area(resolution):
        if not resolution or len(resolution) != 2:
            return 0
        return int(resolution[0]) * int(resolution[1])

    @staticmethod
    def _is_rendered_from_higher_res(source_resolution, rendered_resolution):
        if not source_resolution or not rendered_resolution:
            return False
        return (
            int(source_resolution[0]) > int(rendered_resolution[0])
            or int(source_resolution[1]) > int(rendered_resolution[1])
        )

    @staticmethod
    def _is_low_quality_resolution(resolution):
        if not resolution or len(resolution) != 2:
            return True
        return min(int(resolution[0]), int(resolution[1])) < _DESKTOP_MIN_SOURCE_DIMENSION
