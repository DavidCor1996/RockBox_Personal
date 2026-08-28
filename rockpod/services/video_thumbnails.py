"""Video thumbnail extraction and disk cache management."""

import hashlib
import json
import math
import os
import shutil
from pathlib import Path

from PIL import Image

from services.command_runner import CommandRunner
from services.file_safety import atomic_write_text
from services.imdb_video_artwork import IMDbVideoArtworkCatalog


_VIDEO_POSTER_FILENAMES = (
    "poster.jpg", "poster.png", "Poster.jpg", "Poster.png",
    "movie.jpg", "movie.png", "Movie.jpg", "Movie.png",
    "season-poster.jpg", "season-poster.png",
    "season01-poster.jpg", "season01-poster.png",
    "season1-poster.jpg", "season1-poster.png",
    "show.jpg", "show.png",
)
_VIDEO_LIST_THUMB_SIZE = (32, 32)
_VIDEO_LIST_PREVIEW_SIZE = (174, 240)
_VIDEO_LIST_NETFLIX_POSTER_SIZE = (28, 42)
_VIDEO_LIST_NETFLIX_LANDING_SIZE = (72, 108)
_VIDEO_LIST_NETFLIX_DETAIL_SIZE = (96, 144)


class VideoThumbnailService:
    """Generate and cache poster thumbnails for local video files."""

    def __init__(self, cache_root, config=None, artwork_manager=None, command_runner=None):
        self._cache_dir = os.path.join(cache_root, "video_thumbs")
        self._video_list_dir = os.path.join(self._cache_dir, "list")
        self._marker_cache_dir = os.path.join(
            self._cache_dir, "playback_markers"
        )
        self._config = config
        self._artwork = artwork_manager
        self._imdb_artwork = IMDbVideoArtworkCatalog()
        self._command_runner = command_runner or CommandRunner(log_dir=os.path.join(self._cache_dir, "logs"))
        os.makedirs(self._cache_dir, exist_ok=True)
        os.makedirs(self._video_list_dir, exist_ok=True)
        os.makedirs(self._marker_cache_dir, exist_ok=True)
        self._ffmpeg = shutil.which("ffmpeg") or ""
        self._ffprobe = shutil.which("ffprobe") or ""

    @property
    def is_available(self):
        return bool(self._ffmpeg)

    def thumbnail_path(self, track, size=232, allow_online=False):
        path = str((track or {}).get("file_path") or "")
        if not path or not os.path.isfile(path):
            return ""

        try:
            stat = os.stat(path)
        except OSError:
            return ""

        duration = float((track or {}).get("duration") or 0.0)
        width = max(int(size), 96)
        height = int(width * 1.5)
        artwork_scope = str((track or {}).get("_video_artwork_scope") or "")
        explicit_poster = str((track or {}).get("artwork_path") or "")
        if not os.path.isfile(explicit_poster):
            explicit_poster = ""
        imdb_poster, _imdb_entry = self._imdb_artwork.resolve(
            track, scope=artwork_scope
        )
        imdb_authoritative = (
            artwork_scope in {"show", "season"}
            or str((track or {}).get("metadata_source") or "").casefold()
            == "imdb"
        )
        local_poster = (
            (imdb_poster or explicit_poster)
            if imdb_authoritative else
            (explicit_poster or imdb_poster)
        ) or self._find_local_poster(track)
        if local_poster:
            poster_mtime = os.stat(local_poster).st_mtime
        else:
            poster_mtime = 0

        online_poster = self._online_poster_path(track, allow_online=allow_online)
        if online_poster:
            poster_mtime = os.stat(online_poster).st_mtime

        cache_key = hashlib.md5(
            f"{path}|{stat.st_mtime}|{stat.st_size}|{width}|{height}|{local_poster}|{online_poster}|{poster_mtime}".encode("utf-8")
        ).hexdigest()
        target = os.path.join(self._cache_dir, f"{cache_key}.jpg")
        if os.path.exists(target):
            return target

        if local_poster:
            self._render_poster(local_poster, target, width, height)
            return target if os.path.exists(target) else ""

        if online_poster:
            self._render_poster(online_poster, target, width, height)
            return target if os.path.exists(target) else ""

        if not self._ffmpeg:
            return ""

        temp_frame = os.path.join(self._cache_dir, f"{cache_key}.src.jpg")
        if not self._extract_frame(path, temp_frame, duration):
            return ""
        try:
            self._render_poster(temp_frame, target, width, height)
        finally:
            try:
                os.remove(temp_frame)
            except OSError:
                pass
        return target if os.path.exists(target) else ""

    def _online_poster_path(self, track, allow_online=False):
        if not self._artwork:
            return ""
        video_info = self._video_info(track)
        poster = self._artwork.get_video_poster(video_info, size="thumb", allow_online=False)
        if poster or not allow_online:
            return poster
        if not self._config_value("enable_online_artwork_lookup", False):
            return ""
        fetched = self._artwork.fetch_online_artwork_now(video_info, force=False)
        if fetched:
            return self._artwork.get_video_poster(video_info, size="thumb", allow_online=False) or fetched
        return ""

    def video_list_id(self, track):
        video_info = self._video_info(track)
        identity = str(
            (track or {}).get("device_path")
            or (track or {}).get("file_path")
            or video_info.get("group_key")
            or video_info.get("album")
            or "video"
        )
        return hashlib.sha1(identity.encode("utf-8", "replace")).hexdigest()[:24]

    def video_hierarchy_art_id(self, track, scope):
        track = dict(track or {})
        scope = str(scope or "show").strip().casefold()
        title = str(track.get("show_title") or track.get("title") or "video")
        imdb_id = str(track.get("imdb_id") or "").strip().casefold()
        identity = imdb_id or title.strip().casefold()
        if scope == "season":
            identity = f"{identity}|{int(track.get('season_number') or 0)}"
        return hashlib.sha1(
            f"{scope}|{identity}".encode("utf-8", "replace")
        ).hexdigest()[:24]

    def video_catalog_metadata(self, track):
        """Return verified catalog fields that are safe sync fallbacks."""
        _poster, entry = self._imdb_artwork.resolve(track, scope="show")
        if not entry:
            return {}
        return {
            "imdb_id": str(entry.get("imdb_id") or ""),
            "tmdb_id": str(entry.get("tmdb_id") or ""),
            "show_title": str(entry.get("title") or ""),
            "year": self._positive_int(entry.get("year")),
            "genre": str(entry.get("genre") or ""),
            "content_rating": str(entry.get("content_rating") or ""),
            "show_plot": str(entry.get("show_plot") or ""),
        }

    def video_playback_markers(self, track):
        """Resolve item-specific intro and credits markers.

        Values are seconds. Explicit library values win, followed by an exact
        catalog season/episode record and named chapters embedded in this
        file. A series-wide timing guess is deliberately never used: showing
        no button is preferable to jumping over programme content.
        """
        track = dict(track or {})
        _poster, entry = self._imdb_artwork.resolve(track, scope="show")
        duration = self._positive_int(track.get("duration"))
        season_number = self._positive_int(track.get("season_number"))
        episode_number = self._positive_int(track.get("episode_number"))
        intro_start = self._positive_int(track.get("intro_start"))
        intro_end = self._positive_int(track.get("intro_end"))
        credits_start = self._positive_int(track.get("credits_start"))
        credits_duration = 0

        exact = {}
        if entry and season_number > 0 and episode_number > 0:
            exact = dict(
                (entry.get("episodes") or {}).get(
                    f"{season_number}x{episode_number:02d}"
                )
                or {}
            )
        needs_embedded_intro = (
            not intro_start and not intro_end and not exact.get("intro_end")
        )
        needs_embedded_credits = (
            not credits_start
            and not exact.get("credits_start")
            and not exact.get("credits_duration")
        )
        embedded = (
            self._embedded_playback_markers(track)
            if needs_embedded_intro or needs_embedded_credits
            else {}
        )

        if not intro_start and not intro_end:
            source = exact if exact.get("intro_end") else embedded
            intro_start = self._positive_int(source.get("intro_start"))
            intro_end = self._positive_int(source.get("intro_end"))
        if not credits_start:
            if exact.get("credits_start"):
                credits_start = self._positive_int(
                    exact.get("credits_start")
                )
            elif exact.get("credits_duration"):
                credits_duration = self._positive_int(
                    exact.get("credits_duration")
                )
            elif embedded.get("credits_start"):
                credits_start = self._positive_int(
                    embedded.get("credits_start")
                )
            if credits_duration and duration > credits_duration:
                credits_start = duration - credits_duration

        if intro_end <= intro_start:
            intro_start = 0
            intro_end = 0
        if credits_start >= duration and duration:
            credits_start = 0
        return {
            "intro_start": intro_start,
            "intro_end": intro_end,
            "credits_start": credits_start,
            "credits_duration": credits_duration,
        }

    def _embedded_playback_markers(self, track):
        """Read only explicitly named intro/credits chapters from one file."""
        path = str((track or {}).get("file_path") or "")
        ffprobe = str(getattr(self, "_ffprobe", "") or "")
        runner = getattr(self, "_command_runner", None)
        cache_dir = str(getattr(self, "_marker_cache_dir", "") or "")
        if not path or not os.path.isfile(path) or not ffprobe or runner is None:
            return {}
        try:
            stat = os.stat(path)
        except OSError:
            return {}
        cache_key = hashlib.sha256(
            f"chapters-v1|{path}|{stat.st_mtime_ns}|{stat.st_size}".encode(
                "utf-8", "replace"
            )
        ).hexdigest()
        cache_path = os.path.join(cache_dir, f"{cache_key}.json")
        try:
            with open(cache_path, "r", encoding="utf-8") as handle:
                cached = json.load(handle)
            return dict(cached or {})
        except (OSError, ValueError, TypeError):
            pass

        command = [
            ffprobe, "-v", "error", "-show_chapters", "-of", "json", path,
        ]
        markers = {}
        try:
            result = runner.run(
                command, cwd=os.path.dirname(path) or os.getcwd()
            )
            payload = json.loads(result.stdout or "{}")
        except (OSError, ValueError, TypeError, AttributeError):
            payload = {}
        for chapter in payload.get("chapters") or []:
            tags = chapter.get("tags") or {}
            title = str(tags.get("title") or tags.get("TITLE") or "")
            normalized = " ".join(title.casefold().replace("_", " ").split())
            start = self._nonnegative_float(chapter.get("start_time"))
            end = self._nonnegative_float(chapter.get("end_time"))
            if start is None or end is None or end <= start:
                continue
            if normalized in {"intro", "opening credits", "opening theme",
                              "title sequence"}:
                markers.setdefault("intro_start", int(math.ceil(start)))
                markers.setdefault("intro_end", int(math.floor(end)))
            if normalized in {"credits", "end credits", "closing credits",
                              "closing titles"}:
                # Rounding forward is intentionally conservative: the seek
                # button cannot appear before the authored chapter boundary.
                markers.setdefault("credits_start", int(math.ceil(start)))
        if cache_dir:
            try:
                os.makedirs(cache_dir, exist_ok=True)
                atomic_write_text(
                    cache_path,
                    json.dumps(markers, sort_keys=True) + "\n",
                )
            except OSError:
                pass
        return markers

    @staticmethod
    def _nonnegative_float(value):
        try:
            parsed = float(value)
        except (TypeError, ValueError):
            return None
        return parsed if math.isfinite(parsed) and parsed >= 0 else None

    @staticmethod
    def _positive_int(value):
        try:
            return max(0, int(float(value or 0)))
        except (TypeError, ValueError):
            return 0

    def export_video_list_thumbnail(self, track, force=False, size=None, allow_online=None):
        target_size = size or _VIDEO_LIST_THUMB_SIZE
        width = max(int(target_size[0]), 1)
        height = max(int(target_size[1]), 1)
        video_id = self.video_list_id(track)
        if allow_online is None:
            allow_online = self._config_value("enable_online_artwork_lookup", False)
        source = self.thumbnail_path(track, size=max(width, height) * 4, allow_online=allow_online)
        if not source:
            return "", "", "", video_id

        source_hash = self._file_hash(source)[:12]
        cache_target = os.path.join(
            self._video_list_dir,
            f"{video_id}_{width}x{height}_{source_hash}.bmp",
        )
        if not os.path.exists(cache_target) or force:
            self._render_list_thumbnail(source, cache_target, width, height)
        if not os.path.exists(cache_target):
            return "", "", "", video_id
        return cache_target, self._file_hash(cache_target), f"{video_id}.bmp", video_id

    def export_video_list_preview(self, track, force=False, size=None, allow_online=None):
        target_size = size or _VIDEO_LIST_PREVIEW_SIZE
        width = max(int(target_size[0]), 1)
        height = max(int(target_size[1]), 1)
        video_id = self.video_list_id(track)
        if allow_online is None:
            allow_online = self._config_value("enable_online_artwork_lookup", False)
        source = self.thumbnail_path(track, size=max(width, height), allow_online=allow_online)
        if not source:
            return "", "", "", video_id

        source_hash = self._file_hash(source)[:12]
        cache_target = os.path.join(
            self._video_list_dir,
            f"{video_id}_{width}x{height}_preview_{source_hash}.bmp",
        )
        if not os.path.exists(cache_target) or force:
            self._render_fill_thumbnail(source, cache_target, width, height)
        if not os.path.exists(cache_target):
            return "", "", "", video_id
        return cache_target, self._file_hash(cache_target), f"{video_id}.bmp", video_id

    def export_video_list_netflix_poster(self, track, force=False,
                                         detail=False, landing=False,
                                         allow_online=None,
                                         artwork_scope="", artwork_id=""):
        if detail:
            target_size = _VIDEO_LIST_NETFLIX_DETAIL_SIZE
        elif landing:
            target_size = _VIDEO_LIST_NETFLIX_LANDING_SIZE
        else:
            target_size = _VIDEO_LIST_NETFLIX_POSTER_SIZE
        width, height = target_size
        video_id = artwork_id or self.video_list_id(track)
        if allow_online is None:
            allow_online = self._config_value("enable_online_artwork_lookup", False)
        scoped_track = dict(track or {})
        if artwork_scope:
            scoped_track["_video_artwork_scope"] = artwork_scope
        source = self.thumbnail_path(
            scoped_track, size=max(width, height) * 3,
            allow_online=allow_online
        )
        if not source:
            return "", "", "", video_id

        source_hash = self._file_hash(source)[:12]
        role = (
            "netflix_detail" if detail else
            "netflix_landing" if landing else
            "netflix_list"
        )
        cache_target = os.path.join(
            self._video_list_dir,
            f"{video_id}_{width}x{height}_{role}_{source_hash}.bmp",
        )
        if not os.path.exists(cache_target) or force:
            self._render_list_thumbnail(
                source, cache_target, width, height
            )
        if not os.path.exists(cache_target):
            return "", "", "", video_id
        return cache_target, self._file_hash(cache_target), f"{video_id}.bmp", video_id

    def export_video_list_hierarchy_poster(self, track, scope, force=False,
                                           detail=False, landing=False,
                                           allow_online=None):
        artwork_id = self.video_hierarchy_art_id(track, scope)
        return self.export_video_list_netflix_poster(
            track,
            force=force,
            detail=detail,
            landing=landing,
            allow_online=allow_online,
            artwork_scope=scope,
            artwork_id=artwork_id,
        )

    # Manifest column order. The device parser reads these positionally and
    # tolerates a longer row but not a reordered one, so new columns are only
    # ever appended. The header line and every row are generated from this one
    # tuple so the two cannot drift apart.
    _MANIFEST_COLUMNS = (
        "video_id", "thumb", "preview", "title", "kind", "group_key",
        "device_path", "show", "season", "episode", "duration", "locked",
        "year", "genre", "rating", "plot_short", "plot_long",
        "content_rating", "netflix_poster", "netflix_detail", "show_art_id",
        "season_art_id", "show_plot", "intro_start", "intro_end",
        "credits_start", "credits_duration",
    )

    # The device reads a manifest row into a fixed VIDEO_LIST_MANIFEST_LINE_MAX
    # buffer (1024) and splits it in place. A longer row is truncated, loses
    # its trailing tabs, fails the split and is dropped from the browser
    # completely - a missing title, not merely a missing description. Keep a
    # little headroom below that and spend the overflow on the prose fields,
    # which are the only expendable ones.
    _MANIFEST_LINE_BUDGET = 1000
    _MANIFEST_TRIMMABLE = ("show_plot", "plot_long", "plot_short")

    def _manifest_cell(self, entry, name):
        if name == "locked":
            flag = str(entry.get("locked") or "0").strip().lower()
            return "1" if flag in {"1", "true", "yes"} else "0"
        return self._manifest_field(entry.get(name))

    def _fit_manifest_row(self, entry):
        entry = dict(entry or {})

        def measure():
            return sum(
                len(self._manifest_cell(entry, name).encode("utf-8")) + 1
                for name in self._MANIFEST_COLUMNS
            )

        for name in self._MANIFEST_TRIMMABLE:
            overflow = measure() - self._MANIFEST_LINE_BUDGET
            if overflow <= 0:
                break
            text = self._manifest_field(entry.get(name))
            if not text:
                continue
            entry[name] = text[:max(0, len(text) - overflow)].rstrip()
        return entry

    def export_video_list_manifest(self, entries):
        manifest_path = os.path.join(self._video_list_dir, "index.tsv")
        # v7 appends playback markers. Appending is the only safe way to grow this
        # file: the device parser reads columns positionally and tolerates a
        # longer row, but not a reordered one.
        lines = [
            "# rockpod videolist v7",
            "\t".join(self._MANIFEST_COLUMNS),
        ]
        def sort_key(item):
            return (
                str(item.get("title") or "").casefold(),
                str(item.get("device_path") or ""),
            )

        for entry in sorted(entries, key=sort_key):
            entry = self._fit_manifest_row(entry)
            lines.append(
                "\t".join(
                    self._manifest_cell(entry, name)
                    for name in self._MANIFEST_COLUMNS
                )
            )
        atomic_write_text(manifest_path, "\n".join(lines) + "\n")
        return manifest_path, self._file_hash(manifest_path)

    def export_locked_video_pin(self, pin):
        pin = str(pin or "").strip()
        if len(pin) != 4 or not pin.isdigit():
            raise ValueError("Locked Videos PIN must contain exactly 4 digits")
        pin_path = os.path.join(self._video_list_dir, "locked.pin")
        atomic_write_text(pin_path, pin + "\n")
        return pin_path, self._file_hash(pin_path)

    @staticmethod
    def _video_info(track):
        track = dict(track or {})
        tracks = list(track.get("_video_tracks") or [track])
        first = tracks[0] if tracks else track
        video_kind = str(first.get("video_kind") or "movie")
        if video_kind == "show":
            album = str(first.get("show_title") or first.get("artist") or "Unknown Show")
            season_number = int(first.get("season_number") or 0)
            artist = str(
                f"Season {season_number}" if season_number else
                first.get("album") or ""
            )
            scope = str(first.get("_video_artwork_scope") or (
                "season" if season_number else "show"
            ))
            identity = str(first.get("imdb_id") or album).strip().casefold()
            group_key = (
                f"show:{identity}:season:{season_number}"
                if scope == "season" and season_number
                else f"show:{identity}"
            )
        else:
            album = str(first.get("title") or first.get("album") or "Untitled Video")
            artist = str(first.get("artist") or first.get("album_artist") or "")
            scope = video_kind
            group_key = str(
                first.get("video_group_key") or first.get("file_path") or album
            )
        return {
            "group_key": group_key,
            "album": album,
            "artist": artist,
            "tracks": tracks,
            "media_type": "video",
            "video_kind": video_kind,
            "video_scope": scope,
        }

    def _find_local_poster(self, track):
        path_text = str((track or {}).get("file_path") or "")
        if not path_text:
            return ""
        path = Path(path_text)
        scope = str((track or {}).get("_video_scope") or "")
        if not scope and str((track or {}).get("video_kind") or "") == "show":
            scope = "show"
        season_number = int((track or {}).get("season_number") or 0)
        show_title = str((track or {}).get("show_title") or "").strip()

        candidates = []
        stem_variants = (
            f"{path.stem}.jpg",
            f"{path.stem}.png",
        )
        current_dir = path.parent
        parent_dir = current_dir.parent if current_dir.parent != current_dir else current_dir
        search_dirs = [current_dir]
        if scope == "show":
            if season_number and parent_dir not in search_dirs:
                search_dirs.append(parent_dir)
            grandparent = parent_dir.parent if parent_dir.parent != parent_dir else parent_dir
            if show_title and grandparent not in search_dirs:
                search_dirs.append(grandparent)
        else:
            if season_number and parent_dir not in search_dirs:
                search_dirs.append(parent_dir)

        season_specific = []
        if season_number:
            season_specific.extend(
                [
                    f"season{season_number:02d}-poster.jpg",
                    f"season{season_number:02d}-poster.png",
                    f"season{season_number}-poster.jpg",
                    f"season{season_number}-poster.png",
                ]
            )

        for directory in search_dirs:
            for name in season_specific + list(_VIDEO_POSTER_FILENAMES):
                candidates.append(directory / name)
            if show_title:
                safe_show = show_title.replace("/", " ").replace("\\", " ").strip()
                candidates.extend(
                    [
                        directory / f"{safe_show}.jpg",
                        directory / f"{safe_show}.png",
                        directory / f"{safe_show} poster.jpg",
                        directory / f"{safe_show} poster.png",
                    ]
                )

        if scope != "show":
            for name in stem_variants:
                candidates.append(path.with_name(name))

        seen = set()
        for candidate in candidates:
            text = str(candidate)
            if text in seen:
                continue
            seen.add(text)
            if candidate.is_file():
                return text
        return ""

    def _extract_frame(self, source_path, target_path, duration):
        seek_seconds = max(1.0, min(duration * 0.1 if duration > 0 else 5.0, 30.0))
        command = [
            self._ffmpeg,
            "-hide_banner",
            "-loglevel",
            "error",
            "-y",
            "-ss",
            f"{seek_seconds:.2f}",
            "-i",
            source_path,
            "-frames:v",
            "1",
            target_path,
        ]
        try:
            result = self._command_runner.run(command, cwd=os.path.dirname(target_path))
        except OSError:
            return False
        return result.returncode == 0 and os.path.exists(target_path)

    @staticmethod
    def _render_poster(source_path, target_path, width, height):
        with Image.open(source_path) as frame:
            frame = frame.convert("RGB")
            source_ratio = frame.width / max(frame.height, 1)
            target_ratio = width / max(height, 1)
            if source_ratio > target_ratio:
                scaled_height = height
                scaled_width = int(height * source_ratio)
            else:
                scaled_width = width
                scaled_height = int(width / max(source_ratio, 0.01))
            frame = frame.resize((scaled_width, scaled_height), Image.LANCZOS)
            left = max((scaled_width - width) // 2, 0)
            top = max((scaled_height - height) // 2, 0)
            canvas = frame.crop((left, top, left + width, top + height))
            canvas.save(target_path, "JPEG", quality=88)

    @staticmethod
    def _render_list_thumbnail(source_path, target_path, width, height):
        with Image.open(source_path) as frame:
            frame = frame.convert("RGB")
            frame.thumbnail((width, height), Image.LANCZOS)
            canvas = Image.new("RGB", (width, height), "black")
            left = max((width - frame.width) // 2, 0)
            top = max((height - frame.height) // 2, 0)
            canvas.paste(frame, (left, top))
            canvas.save(target_path, "BMP")

    @staticmethod
    def _render_fill_thumbnail(source_path, target_path, width, height):
        with Image.open(source_path) as frame:
            frame = frame.convert("RGB")
            frame = VideoThumbnailService._trim_dark_border(frame)
            source_ratio = frame.width / max(frame.height, 1)
            target_ratio = width / max(height, 1)
            if source_ratio > target_ratio:
                scaled_height = height
                scaled_width = int(height * source_ratio)
            else:
                scaled_width = width
                scaled_height = int(width / max(source_ratio, 0.01))
            frame = frame.resize((scaled_width, scaled_height), Image.LANCZOS)
            left = max((scaled_width - width) // 2, 0)
            top = max((scaled_height - height) // 2, 0)
            canvas = frame.crop((left, top, left + width, top + height))
            canvas.save(target_path, "BMP")

    @staticmethod
    def _trim_dark_border(frame):
        mask = frame.convert("L").point(lambda value: 255 if value > 18 else 0)
        bbox = mask.getbbox()
        if not bbox:
            return frame
        left, top, right, bottom = bbox
        border_x = left + (frame.width - right)
        border_y = top + (frame.height - bottom)
        if border_x < 4 and border_y < 4:
            return VideoThumbnailService._trim_dark_letterbox_rows(frame)
        if right - left < frame.width // 2 or bottom - top < frame.height // 2:
            return VideoThumbnailService._trim_dark_letterbox_rows(frame)
        frame = frame.crop(bbox)
        return VideoThumbnailService._trim_dark_letterbox_rows(frame)

    @staticmethod
    def _trim_dark_letterbox_rows(frame):
        if frame.height < 8 or frame.width < 8:
            return frame
        pixels = frame.load()
        limit = max(frame.height // 4, 1)
        threshold = frame.width // 3

        def dark_count(y):
            return sum(1 for x in range(frame.width)
                       if max(pixels[x, y]) < 24)

        top = 0
        while top < limit and dark_count(top) > threshold:
            top += 1

        bottom = frame.height
        while bottom - 1 > frame.height - limit and dark_count(bottom - 1) > threshold:
            bottom -= 1

        if top == 0 and bottom == frame.height:
            return frame
        if bottom - top < frame.height // 2:
            return frame
        return frame.crop((0, top, frame.width, bottom))

    @staticmethod
    def _file_hash(path):
        h = hashlib.sha256()
        with open(path, "rb") as handle:
            for chunk in iter(lambda: handle.read(1024 * 1024), b""):
                h.update(chunk)
        return h.hexdigest()

    @staticmethod
    def _manifest_field(value):
        return str(value or "").replace("\t", " ").replace("\r", " ").replace("\n", " ").strip()

    def _config_value(self, key, default=None):
        config = self._config
        if config is None:
            return default
        getter = getattr(config, "get", None)
        if callable(getter):
            return getter(key, default)
        return getattr(config, key, default)
