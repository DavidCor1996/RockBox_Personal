"""Offline 2007 YouTube library, profile, export and incremental sync."""

from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import subprocess
import calendar
from datetime import date
from datetime import datetime
from pathlib import Path
from urllib.parse import urlparse, urlunparse

from PIL import Image, ImageOps

from services.android_media import build_ffmpeg_command
from services.device_sync_index import DeviceSyncIndex
from services.file_safety import atomic_write_text
from services.path_safety import resolve_under_root, validate_device_root
from services.app_video_sync import device_video_target


YOUTUBE_DEVICE_ROOT = ".rockbox/youtube"
YOUTUBE_MEDIA_ROOT = "YouTube/videos"
YOUTUBE_ASSET_ROOT = ".rockbox/ipodjs/youtube"
YOUTUBE_PREVIEW_ROOT = ".rockbox/youtube/previews"
YOUTUBE_URL_RE = re.compile(
    r"(?:youtube\.com/(?:watch\?(?:[^#]*&)?v=|shorts/|embed/)|youtu\.be/)"
    r"([A-Za-z0-9_-]{11})"
)
YOUTUBE_FIELDS = (
    "title",
    "uploader",
    "description",
    "duration_ms",
    "upload_date",
    "view_count",
    "rating_average",
    "rating_count",
    "category",
    "tags",
    "source_url",
    "source_type",
    "channel_url",
    "thumbnail_path",
    "show_home",
    "show_profile",
    "favorite",
    "home_section",
    "home_order",
    "my_rating",
    "is_live",
    "live_creator_key",
    "live_start_epoch",
)
PROFILE_FIELDS = (
    "username",
    "display_name",
    "profile_image",
    "banner_image",
    "banner_alignment",
    "banner_vertical_alignment",
    "about_me",
    "city",
    "country",
    "occupation",
    "interests",
    "movies",
    "music",
    "books",
    "website",
    "joined",
    "channel_views",
    "video_views",
    "subscribers",
    "friends",
    "background_color",
    "module_color",
    "text_color",
    "link_color",
    "show_about",
    "show_videos",
    "show_favorites",
    "show_on_main_menu",
)


def _clean(value, limit=240):
    return " ".join(str(value or "").replace("\t", " ").split())[:limit]


def _stable_id(source_path):
    canonical = os.path.realpath(os.path.abspath(source_path))
    digest = hashlib.sha1(canonical.encode("utf-8")).hexdigest()[:12]
    return f"yt_{digest}"


def _youtube_video_id(url):
    match = YOUTUBE_URL_RE.search(str(url or "").strip())
    if not match:
        raise ValueError("Enter a valid YouTube video URL")
    return match.group(1)


def _youtube_upload_date(value):
    text = str(value or "")
    if len(text) == 8 and text.isdigit():
        return f"{text[:4]}-{text[4:6]}-{text[6:]}"
    return text


def _youtube_channel_url(url):
    """Validate and normalize a YouTube channel or creator URL."""
    text = str(url or "").strip()
    parsed = urlparse(text)
    host = parsed.netloc.lower().removeprefix("www.")
    path = parsed.path.rstrip("/")
    valid_path = (
        path.startswith("/@")
        or path.startswith("/channel/")
        or path.startswith("/c/")
        or path.startswith("/user/")
    )
    if parsed.scheme not in {"http", "https"} or host != "youtube.com" or not valid_path:
        raise ValueError(
            "Enter a YouTube channel URL (for example https://www.youtube.com/@creator)"
        )
    return urlunparse(("https", "www.youtube.com", path, "", "", ""))


def _source_signature(path):
    stat = os.stat(path)
    return f"{stat.st_size}:{stat.st_mtime_ns}"


def _local_epoch_now():
    """Return Rockbox-compatible seconds for the machine's local wall clock.

    Rockbox's embedded ``mktime`` intentionally treats the RTC fields as a
    timezone-free epoch.  Treating the host's local fields as UTC produces the
    same integer and avoids a timezone-sized jump when the iPod joins live.
    """
    return int(calendar.timegm(datetime.now().timetuple()))


def _live_creator_key(channel_url, uploader):
    identity = _clean(channel_url, 500).lower() or _clean(uploader, 96).lower()
    identity = re.sub(r"\s+", "-", identity).strip("-") or "youtube-creator"
    return hashlib.sha1(identity.encode("utf-8")).hexdigest()[:16]


def _duration_text(milliseconds):
    seconds = max(0, int(milliseconds or 0) // 1000)
    hours, remainder = divmod(seconds, 3600)
    minutes, seconds = divmod(remainder, 60)
    if hours:
        return f"{hours}:{minutes:02d}:{seconds:02d}"
    return f"{minutes}:{seconds:02d}"


def _probe_duration_ms(path, ffprobe="ffprobe"):
    try:
        result = subprocess.run(
            [
                ffprobe,
                "-v",
                "error",
                "-show_entries",
                "format=duration",
                "-of",
                "default=noprint_wrappers=1:nokey=1",
                path,
            ],
            check=False,
            capture_output=True,
            text=True,
            timeout=30,
        )
        return max(0, int(float(result.stdout.strip()) * 1000))
    except (OSError, ValueError, subprocess.SubprocessError):
        return 0


def _copy_if_changed(source, target):
    if os.path.isfile(target):
        source_stat = os.stat(source)
        target_stat = os.stat(target)
        if source_stat.st_size == target_stat.st_size:
            with open(source, "rb") as left, open(target, "rb") as right:
                if hashlib.sha256(left.read()).digest() == hashlib.sha256(
                    right.read()
                ).digest():
                    return False
    os.makedirs(os.path.dirname(target), exist_ok=True)
    temp = f"{target}.rockpod-tmp"
    shutil.copy2(source, temp)
    os.replace(temp, target)
    return True


class YoutubeAppService:
    """RockPod-side source of truth for the standalone YouTube app."""

    def __init__(self, database, config, repo_root):
        self.db = database
        self.config = config
        self.repo_root = Path(repo_root)
        self.db.execute("INSERT OR IGNORE INTO youtube_profile (id) VALUES (1)")

    def list_videos(self):
        return [
            dict(row)
            for row in self.db.fetchall(
                "SELECT * FROM youtube_videos "
                "ORDER BY show_home DESC, home_order, date_added, title"
            )
        ]

    def list_live_videos(self):
        return [
            dict(row)
            for row in self.db.fetchall(
                "SELECT * FROM youtube_videos WHERE is_live = 1 "
                "ORDER BY uploader, date_modified DESC"
            )
        ]

    def get_video(self, video_id):
        row = self.db.fetchone(
            "SELECT * FROM youtube_videos WHERE id = ?", (video_id,)
        )
        return dict(row) if row else None

    def list_channel_syncs(self):
        return [
            dict(row)
            for row in self.db.fetchall(
                "SELECT * FROM youtube_channel_syncs ORDER BY channel_name, channel_url"
            )
        ]

    def add_channel_sync(self, channel_url, channel_name=""):
        channel_url = _youtube_channel_url(channel_url)
        self.db.execute(
            "INSERT INTO youtube_channel_syncs "
            "(channel_url, channel_name, keep_count) VALUES (?, ?, 3) "
            "ON CONFLICT(channel_url) DO UPDATE SET "
            "channel_name=excluded.channel_name, "
            "keep_count=3, date_modified=datetime('now')",
            (channel_url, _clean(channel_name, 96)),
        )
        row = self.db.fetchone(
            "SELECT * FROM youtube_channel_syncs WHERE channel_url = ?",
            (channel_url,),
        )
        return dict(row)

    def remove_channel_sync(self, channel_url):
        self.db.execute(
            "DELETE FROM youtube_channel_syncs WHERE channel_url = ?", (channel_url,)
        )

    def _channel_uploads(self, channel_url, limit=3):
        """Return the current newest uploads without downloading media yet."""
        command = [
            self.config.get("yt_dlp_binary", "yt-dlp"),
            "--flat-playlist",
            "--playlist-end", str(limit),
            "--dump-single-json",
            "--no-warnings",
            f"{channel_url}/videos",
        ]
        try:
            result = subprocess.run(
                command, check=True, capture_output=True, text=True, timeout=120
            )
            payload = json.loads(result.stdout)
        except (OSError, json.JSONDecodeError, subprocess.SubprocessError) as exc:
            detail = getattr(exc, "stderr", "") or str(exc)
            raise ValueError(
                f"Could not read channel uploads: {_clean(detail, 300)}"
            ) from exc
        entries = payload.get("entries") or []
        uploads = []
        for entry in entries:
            video_id = str((entry or {}).get("id") or "")
            if re.fullmatch(r"[A-Za-z0-9_-]{11}", video_id):
                uploads.append(
                    (video_id, str(entry.get("channel") or entry.get("uploader") or ""))
                )
        if not uploads:
            raise ValueError("The channel did not return any downloadable uploads")
        return uploads[:limit]

    def sync_channel_uploads(self, progress_callback=None):
        """Refresh selected channels and retain exactly their newest three videos.

        Existing iPod media is removed by the ordinary export's stable-ID stale
        cleanup after the database rows below are pruned.  Pruning occurs only
        after every selected upload has downloaded successfully.
        """
        progress = progress_callback or (lambda _done, _total, _label: None)
        channels = self.list_channel_syncs()
        report = {"channels": 0, "videos_added": 0, "videos_removed": 0}
        for channel_index, channel in enumerate(channels, 1):
            channel_url = channel["channel_url"]
            progress(
                0,
                0,
                f"Checking YouTube channel {channel_index}/{len(channels)}: "
                f"{channel.get('channel_name') or channel_url}",
            )
            uploads = self._channel_uploads(channel_url, channel["keep_count"])
            ids = []
            channel_name = channel.get("channel_name") or uploads[0][1]
            for upload_index, (video_id, discovered_name) in enumerate(uploads, 1):
                progress(
                    upload_index - 1,
                    len(uploads),
                    f"Downloading channel video {upload_index}/{len(uploads)} "
                    f"from {channel_name or channel_url}…",
                )
                row = self.import_url(
                    f"https://www.youtube.com/watch?v={video_id}",
                    "profile",
                    channel_url=channel_url,
                )
                ids.append(row["id"])
                if not channel_name:
                    channel_name = discovered_name
                progress(
                    upload_index,
                    len(uploads),
                    f"Downloaded channel video {upload_index}/{len(uploads)}",
                )
            placeholders = ", ".join("?" for _ in ids)
            stale_rows = self.db.fetchall(
                "SELECT id FROM youtube_videos WHERE source_type = "
                "'youtube-channel' AND channel_url = ? "
                f"AND id NOT IN ({placeholders})",
                (channel_url, *ids),
            )
            for stale in stale_rows:
                self.remove_video(stale["id"])
            self.db.execute(
                "UPDATE youtube_channel_syncs SET channel_name = ?, "
                "date_modified=datetime('now') WHERE channel_url = ?",
                (_clean(channel_name, 96), channel_url),
            )
            report["channels"] += 1
            report["videos_added"] += len(ids)
            report["videos_removed"] += len(stale_rows)
        return report

    def add_video(self, source_path, **metadata):
        source = os.path.abspath(os.path.expanduser(str(source_path or "")))
        if not os.path.isfile(source):
            raise ValueError("Choose an existing local video file")
        video_id = _stable_id(source)
        profile = self.get_profile()
        title = _clean(metadata.get("title") or Path(source).stem, 80)
        uploader = _clean(
            metadata.get("uploader") or profile.get("username") or "You", 48
        )
        channel_url = _clean(metadata.get("channel_url"), 500)
        is_live = int(bool(metadata.get("is_live", False)))
        creator_key = _clean(metadata.get("live_creator_key"), 64)
        if is_live and not creator_key:
            creator_key = _live_creator_key(channel_url, uploader)
        live_start_epoch = int(metadata.get("live_start_epoch") or 0)
        if is_live and live_start_epoch <= 0:
            live_start_epoch = _local_epoch_now()
        duration_ms = int(
            metadata.get("duration_ms") or _probe_duration_ms(
                source, self.config.get("ffprobe_binary", "ffprobe")
            )
        )
        values = {
            "title": title,
            "uploader": uploader,
            "description": _clean(metadata.get("description"), 1000),
            "duration_ms": duration_ms,
            "upload_date": _clean(
                metadata.get("upload_date") or date.today().isoformat(), 20
            ),
            "view_count": max(0, int(metadata.get("view_count") or 0)),
            "rating_average": min(
                5.0, max(0.0, float(metadata.get("rating_average") or 0))
            ),
            "rating_count": max(0, int(metadata.get("rating_count") or 0)),
            "category": _clean(
                metadata.get("category") or "People & Blogs", 32
            ),
            "tags": _clean(metadata.get("tags"), 300),
            "source_url": _clean(metadata.get("source_url"), 500),
            "source_type": _clean(
                metadata.get("source_type") or "personal", 24
            ),
            "channel_url": channel_url,
            "thumbnail_path": os.path.abspath(
                os.path.expanduser(str(metadata.get("thumbnail_path") or ""))
            )
            if metadata.get("thumbnail_path")
            else "",
            "show_home": int(bool(metadata.get("show_home", False))),
            "show_profile": int(bool(metadata.get("show_profile", True))),
            "favorite": int(bool(metadata.get("favorite", False))),
            "home_section": _clean(
                metadata.get("home_section") or "featured", 24
            ),
            "home_order": int(metadata.get("home_order") or 0),
            "my_rating": min(5, max(0, int(metadata.get("my_rating") or 0))),
            "is_live": is_live,
            "live_creator_key": creator_key if is_live else "",
            "live_start_epoch": live_start_epoch if is_live else 0,
        }
        columns = ", ".join(values)
        placeholders = ", ".join("?" for _ in values)
        updates = ", ".join(
            f"{column}=excluded.{column}" for column in values
        )
        self.db.execute(
            f"INSERT INTO youtube_videos "
            f"(id, source_path, source_hash, {columns}) "
            f"VALUES (?, ?, ?, {placeholders}) "
            f"ON CONFLICT(id) DO UPDATE SET {updates}, "
            "source_path=excluded.source_path, source_hash=excluded.source_hash, "
            "date_modified=datetime('now')",
            (video_id, source, _source_signature(source), *values.values()),
        )
        if is_live:
            self.db.execute(
                "UPDATE youtube_videos SET is_live = 0, live_start_epoch = 0 "
                "WHERE live_creator_key = ? AND id <> ?",
                (creator_key, video_id),
            )
        return self.get_video(video_id)

    def import_url(
        self, url, destination="home", channel_url="", *,
        is_live=False, live_start_epoch=0,
    ):
        """Download one YouTube URL into RockPod's managed offline library."""
        youtube_id = _youtube_video_id(url)
        destinations = {
            "library": (False, False),
            "home": (True, False),
            "profile": (False, True),
            "both": (True, True),
        }
        if destination not in destinations:
            raise ValueError("Choose Library, Home, Profile, or Home + Profile")

        managed = Path.home() / ".rockpod" / "youtube"
        managed.mkdir(parents=True, exist_ok=True)
        output = str(managed / "%(id)s.%(ext)s")
        downloader = self.config.get("yt_dlp_binary", "yt-dlp")
        base = [
            downloader,
            "--no-playlist",
            "-f",
            "18/best[height<=480]/best",
            "--write-info-json",
            "--write-thumbnail",
            "--convert-thumbnails",
            "jpg",
            "-o",
            output,
            str(url).strip(),
        ]

        def download(command):
            return subprocess.run(
                command,
                check=True,
                capture_output=True,
                text=True,
                timeout=60 * 60,
            )

        try:
            download(base)
        except (OSError, subprocess.CalledProcessError) as first_error:
            firefox = Path.home() / ".mozilla" / "firefox"
            if not any(firefox.glob("*/cookies.sqlite")):
                detail = getattr(first_error, "stderr", "") or str(first_error)
                raise ValueError(f"YouTube download failed: {_clean(detail, 300)}") from first_error
            try:
                download(base[:1] + ["--cookies-from-browser", "firefox"] + base[1:])
            except (OSError, subprocess.CalledProcessError) as retry_error:
                detail = getattr(retry_error, "stderr", "") or str(retry_error)
                raise ValueError(f"YouTube download failed: {_clean(detail, 300)}") from retry_error

        info_path = managed / f"{youtube_id}.info.json"
        if not info_path.is_file():
            raise ValueError("YouTube download finished without metadata")
        with info_path.open("r", encoding="utf-8") as handle:
            info = json.load(handle)
        media_candidates = [
            path
            for path in managed.glob(f"{youtube_id}.*")
            if path.suffix.lower()
            not in {".json", ".jpg", ".jpeg", ".webp", ".part", ".ytdl"}
        ]
        if not media_candidates:
            raise ValueError("YouTube download finished without a video file")
        media = max(media_candidates, key=lambda path: path.stat().st_size)
        thumbnail = managed / f"{youtube_id}.jpg"
        show_home, show_profile = destinations[destination]
        next_home = 0
        if show_home:
            next_home = max(
                (int(row.get("home_order") or 0) for row in self.list_videos()),
                default=0,
            ) + 1
        return self.add_video(
            media,
            title=info.get("title") or youtube_id,
            uploader=info.get("uploader") or info.get("channel") or "YouTube",
            description=info.get("description") or "",
            duration_ms=int(float(info.get("duration") or 0) * 1000),
            upload_date=_youtube_upload_date(info.get("upload_date")),
            view_count=int(info.get("view_count") or 0),
            rating_average=float(info.get("average_rating") or 0),
            rating_count=0,
            category=(info.get("categories") or ["People & Blogs"])[0],
            tags=", ".join(info.get("tags") or []),
            source_url=str(url).strip(),
            source_type=(
                "youtube-live" if is_live else
                "youtube-channel" if channel_url else "youtube"
            ),
            channel_url=channel_url,
            is_live=is_live,
            live_creator_key=_live_creator_key(
                channel_url or info.get("channel_url") or info.get("uploader_url"),
                info.get("uploader") or info.get("channel") or "YouTube",
            ) if is_live else "",
            live_start_epoch=live_start_epoch or (
                _local_epoch_now() if is_live else 0
            ),
            thumbnail_path=str(thumbnail) if thumbnail.is_file() else "",
            show_home=show_home,
            show_profile=show_profile,
            home_order=next_home,
        )

    def import_live_url(self, url):
        """Download a URL and make it the creator's sole current live item."""
        return self.import_url(
            url, "library", is_live=True, live_start_epoch=_local_epoch_now()
        )

    def set_live_video(self, video_id, enabled=True, start_epoch=0):
        row = self.get_video(video_id)
        if not row:
            raise ValueError("Choose an existing YouTube video")
        if not enabled:
            return self.update_video(
                video_id,
                {"is_live": 0, "live_creator_key": "", "live_start_epoch": 0},
            )
        creator_key = _live_creator_key(row.get("channel_url"), row.get("uploader"))
        self.db.execute(
            "UPDATE youtube_videos SET is_live = 0, live_start_epoch = 0 "
            "WHERE live_creator_key = ? AND id <> ?",
            (creator_key, video_id),
        )
        return self.update_video(
            video_id,
            {
                "is_live": 1,
                "live_creator_key": creator_key,
                "live_start_epoch": int(start_epoch or _local_epoch_now()),
                "source_type": "youtube-live",
            },
        )

    def update_video(self, video_id, updates):
        allowed = {key: value for key, value in dict(updates).items()
                   if key in YOUTUBE_FIELDS}
        if not allowed:
            return self.get_video(video_id)
        for key in (
            "show_home",
            "show_profile",
            "favorite",
            "is_live",
        ):
            if key in allowed:
                allowed[key] = int(bool(allowed[key]))
        for key in (
            "duration_ms",
            "view_count",
            "rating_count",
            "home_order",
            "my_rating",
            "live_start_epoch",
        ):
            if key in allowed:
                allowed[key] = int(allowed[key] or 0)
        if "rating_average" in allowed:
            allowed["rating_average"] = min(
                5.0, max(0.0, float(allowed["rating_average"] or 0))
            )
        for key in allowed:
            if key not in {
                "duration_ms",
                "view_count",
                "rating_average",
                "rating_count",
                "show_home",
                "show_profile",
                "favorite",
                "home_order",
                "my_rating",
                "is_live",
                "live_start_epoch",
            }:
                allowed[key] = _clean(allowed[key], 1000)
        assignment = ", ".join(f"{key} = ?" for key in allowed)
        self.db.execute(
            f"UPDATE youtube_videos SET {assignment}, "
            "date_modified=datetime('now') WHERE id = ?",
            (*allowed.values(), video_id),
        )
        return self.get_video(video_id)

    def remove_video(self, video_id):
        self.db.execute("DELETE FROM youtube_videos WHERE id = ?", (video_id,))

    def get_profile(self):
        row = self.db.fetchone("SELECT * FROM youtube_profile WHERE id = 1")
        return dict(row) if row else {}

    def save_profile(self, updates):
        allowed = {key: value for key, value in dict(updates).items()
                   if key in PROFILE_FIELDS}
        if not allowed:
            return self.get_profile()
        if "banner_alignment" in allowed:
            alignment = str(allowed["banner_alignment"] or "center").lower()
            allowed["banner_alignment"] = (
                alignment if alignment in {"left", "center", "right"} else "center"
            )
        if "banner_vertical_alignment" in allowed:
            alignment = str(
                allowed["banner_vertical_alignment"] or "center"
            ).lower()
            allowed["banner_vertical_alignment"] = (
                alignment if alignment in {"top", "center", "bottom"} else "center"
            )
        for key in (
            "channel_views",
            "video_views",
            "subscribers",
            "friends",
            "show_about",
            "show_videos",
            "show_favorites",
            "show_on_main_menu",
        ):
            if key in allowed:
                allowed[key] = max(0, int(allowed[key] or 0))
        for key in allowed:
            if key not in {
                "channel_views",
                "video_views",
                "subscribers",
                "friends",
                "show_about",
                "show_videos",
                "show_favorites",
                "show_on_main_menu",
            }:
                allowed[key] = _clean(allowed[key], 1000)
        assignment = ", ".join(f"{key} = ?" for key in allowed)
        self.db.execute(
            f"UPDATE youtube_profile SET {assignment}, "
            "date_modified=datetime('now') WHERE id = 1",
            tuple(allowed.values()),
        )
        return self.get_profile()

    def _merge_device_state(self, mount):
        state_path = resolve_under_root(mount, f"{YOUTUBE_DEVICE_ROOT}/state.tsv")
        if not os.path.isfile(state_path):
            return 0
        merged = 0
        with open(state_path, "r", encoding="utf-8", errors="replace") as handle:
            for raw in handle:
                fields = raw.rstrip("\r\n").split("\t")
                if len(fields) < 3 or fields[0] == "id":
                    continue
                row = self.get_video(fields[0])
                if not row:
                    continue
                rating = min(5, max(0, int(fields[1] or 0)))
                favorite = int(fields[2] == "1")
                if rating != row["my_rating"] or favorite != row["favorite"]:
                    self.update_video(
                        fields[0], {"my_rating": rating, "favorite": favorite}
                    )
                    merged += 1
        return merged

    def _render_thumbnail(self, row, target, staging):
        source_image = row.get("thumbnail_path") or ""
        extracted = ""
        if not os.path.isfile(source_image):
            extracted = os.path.join(staging, f"{row['id']}-frame.jpg")
            command = [
                self.config.get("ffmpeg_binary", "ffmpeg"),
                "-hide_banner",
                "-loglevel",
                "error",
                "-y",
                "-ss",
                "3",
                "-i",
                row["source_path"],
                "-frames:v",
                "1",
                extracted,
            ]
            subprocess.run(command, check=False, timeout=120)
            source_image = extracted
        if os.path.isfile(source_image):
            with Image.open(source_image) as image:
                fitted = ImageOps.fit(
                    image.convert("RGB"), (96, 72), Image.Resampling.LANCZOS
                )
                generated = os.path.join(staging, f"{row['id']}-thumb.bmp")
                fitted.save(generated, "BMP")
                changed = _copy_if_changed(generated, target)
                os.unlink(generated)
                if extracted and os.path.isfile(extracted):
                    os.unlink(extracted)
                return changed
        image = Image.new("RGB", (96, 72), "#eeeeee")
        generated = os.path.join(staging, f"{row['id']}-thumb.bmp")
        image.save(generated, "BMP")
        changed = _copy_if_changed(generated, target)
        os.unlink(generated)
        if extracted and os.path.isfile(extracted):
            os.unlink(extracted)
        return changed

    def _render_menu_preview(self, row, target, staging):
        source_image = row.get("thumbnail_path") or ""
        extracted = ""
        if not os.path.isfile(source_image):
            extracted = os.path.join(staging, f"{row['id']}-preview-frame.jpg")
            subprocess.run(
                [
                    self.config.get("ffmpeg_binary", "ffmpeg"),
                    "-hide_banner", "-loglevel", "error", "-y",
                    "-ss", "3", "-i", row["source_path"],
                    "-frames:v", "1", extracted,
                ],
                check=False,
                timeout=120,
            )
            source_image = extracted
        if os.path.isfile(source_image):
            with Image.open(source_image) as image:
                fitted = ImageOps.fit(
                    image.convert("RGB"), (320, 240), Image.Resampling.LANCZOS
                )
        else:
            fitted = Image.new("RGB", (320, 240), "#eeeeee")
        generated = os.path.join(staging, f"{row['id']}-preview.bmp")
        fitted.save(generated, "BMP")
        changed = _copy_if_changed(generated, target)
        os.unlink(generated)
        if extracted and os.path.isfile(extracted):
            os.unlink(extracted)
        return changed

    @staticmethod
    def _remove_stale_exports(media_root, thumb_root, preview_root, video_ids):
        """Remove only files owned by this app's stable-ID export namespace."""
        removed = 0
        expected = set(video_ids)
        for directory, suffixes in (
            (media_root, {".mpg", ".m4v", ".ytm"}),
            (thumb_root, {".bmp"}),
            (preview_root, {".bmp"}),
        ):
            for name in os.listdir(directory):
                stem, suffix = os.path.splitext(name)
                if (
                    stem.startswith("yt_")
                    and suffix.lower() in suffixes
                    and stem not in expected
                ):
                    os.unlink(os.path.join(directory, name))
                    removed += 1
                    sidecar = os.path.join(directory, name + ".source")
                    if os.path.isfile(sidecar):
                        os.unlink(sidecar)
                        removed += 1
        return removed

    def _sync_media(
        self, row, mount, staging, sync_index, device_target=""
    ):
        profile = str(self.config.get("video_sync_profile", "quality")).strip().lower()
        source_signature = _source_signature(row["source_path"])
        apple_exact = profile == "h264_apple_exact"

        def media_signature(for_profile, selected_device_target):
            return (
                f"youtube-media-v3:{for_profile}:{selected_device_target or 'generic'}:"
                f"{source_signature}"
            )

        extension = ".m4v" if apple_exact else ".mpg"
        destination = resolve_under_root(
            mount, f"{YOUTUBE_MEDIA_ROOT}/{row['id']}{extension}"
        )
        signature = media_signature(profile, device_target)
        if sync_index.current_or_seed(
            "youtube", row["id"], "video-media-v2", signature,
            [destination],
            trust_existing=row.get("last_synced_source_hash") == signature,
        ):
            return False, destination, signature
        os.makedirs(os.path.dirname(destination), exist_ok=True)
        staged = os.path.join(staging, f"{row['id']}{extension}")
        if apple_exact:
            try:
                from services.video_rvp import VideoRvpTranscoder

                cache_root = os.path.join(
                    str(self.config.get("cache_dir") or Path.home() / ".rockpod" / "cache"),
                    "youtube_video",
                )
                transcoder = VideoRvpTranscoder(
                    cache_root,
                    ffmpeg_path=self.config.get("ffmpeg_binary", "ffmpeg"),
                    profile="h264_apple_exact",
                )
                prepared, _info = transcoder.prepare_track_for_sync(
                    {
                        "file_path": row["source_path"],
                        "title": row.get("title") or "YouTube",
                        "artist": row.get("uploader") or "YouTube",
                        "duration": float(row.get("duration_ms") or 0) / 1000.0,
                        "media_type": "Video",
                    },
                    f"youtube-{hashlib.sha1(str(mount).encode()).hexdigest()[:12]}",
                    device_target=device_target,
                )
                shutil.copy2(prepared["sync_source_path"], staged)
            except Exception:
                # H.264 conversion failed for one video; keep syncing by
                # converting this item as MPEG instead of failing the whole run.
                profile = "quality"
                extension = ".mpg"
                destination = resolve_under_root(
                    mount, f"{YOUTUBE_MEDIA_ROOT}/{row['id']}{extension}"
                )
                signature = media_signature(profile, device_target)
                staged = os.path.join(staging, f"{row['id']}{extension}")
                current = sync_index.current_or_seed(
                    "youtube", row["id"], "video-media-v2", signature,
                    [destination],
                    trust_existing=row.get("last_synced_source_hash") == signature,
                )
                if not current:
                    if Path(row["source_path"]).suffix.lower() in {".mpg", ".mpeg", ".mpe"}:
                        shutil.copy2(row["source_path"], staged)
                    else:
                        command = build_ffmpeg_command(
                            row["source_path"],
                            staged,
                            "video",
                            ffmpeg_path=self.config.get("ffmpeg_binary", "ffmpeg"),
                        )
                        subprocess.run(command, check=True)
                    os.replace(staged, destination)
                    sync_index.mark(
                        "youtube", row["id"], "video-media-v2", signature, [destination]
                    )
                    updated = True
                else:
                    updated = False
                alternate = os.path.splitext(destination)[0] + ".m4v"
                try:
                    os.unlink(alternate)
                except FileNotFoundError:
                    pass
                return updated, destination, signature
        elif Path(row["source_path"]).suffix.lower() in {".mpg", ".mpeg", ".mpe"}:
            shutil.copy2(row["source_path"], staged)
        else:
            command = build_ffmpeg_command(
                row["source_path"],
                staged,
                "video",
                ffmpeg_path=self.config.get("ffmpeg_binary", "ffmpeg"),
            )
            subprocess.run(command, check=True)
        os.replace(staged, destination)
        alternate = os.path.splitext(destination)[0] + (
            ".mpg" if apple_exact else ".m4v"
        )
        try:
            os.unlink(alternate)
        except FileNotFoundError:
            pass
        sync_index.mark(
            "youtube", row["id"], "video-media-v2", signature, [destination]
        )
        return True, destination, signature

    def _profile_lines(self, profile, mount, staging):
        profile_image = ""
        source = profile.get("profile_image") or ""
        if os.path.isfile(source):
            target = resolve_under_root(mount, f"{YOUTUBE_DEVICE_ROOT}/profile.bmp")
            with Image.open(source) as image:
                fitted = ImageOps.fit(
                    image.convert("RGB"), (64, 64), Image.Resampling.LANCZOS
                )
                temp = os.path.join(staging, "profile.bmp")
                fitted.save(temp, "BMP")
                os.replace(temp, target)
            profile_image = "/.rockbox/youtube/profile.bmp"
        banner_image = ""
        banner_source = profile.get("banner_image") or ""
        banner_alignment = str(
            profile.get("banner_alignment") or "center"
        ).lower()
        banner_vertical_alignment = str(
            profile.get("banner_vertical_alignment") or "center"
        ).lower()
        if banner_alignment not in {"left", "center", "right"}:
            banner_alignment = "center"
        if banner_vertical_alignment not in {"top", "center", "bottom"}:
            banner_vertical_alignment = "center"
        if os.path.isfile(banner_source):
            target = resolve_under_root(mount, f"{YOUTUBE_DEVICE_ROOT}/banner.bmp")
            horizontal = {"left": 0.0, "center": 0.5, "right": 1.0}[
                banner_alignment
            ]
            vertical = {"top": 0.0, "center": 0.5, "bottom": 1.0}[
                banner_vertical_alignment
            ]
            with Image.open(banner_source) as image:
                fitted = ImageOps.fit(
                    image.convert("RGB"),
                    (312, 56),
                    Image.Resampling.LANCZOS,
                    centering=(horizontal, vertical),
                )
                temp = os.path.join(staging, "banner.bmp")
                fitted.save(temp, "BMP")
                os.replace(temp, target)
            banner_image = "/.rockbox/youtube/banner.bmp"
        values = dict(profile)
        values["profile_image"] = profile_image
        values["banner_image"] = banner_image
        values["banner_alignment"] = banner_alignment
        values["banner_vertical_alignment"] = banner_vertical_alignment
        keys = [
            "username",
            "display_name",
            "profile_image",
            "banner_image",
            "banner_alignment",
            "banner_vertical_alignment",
            "about_me",
            "city",
            "country",
            "occupation",
            "interests",
            "movies",
            "music",
            "books",
            "website",
            "joined",
            "channel_views",
            "video_views",
            "subscribers",
            "friends",
            "background_color",
            "module_color",
            "text_color",
            "link_color",
            "show_about",
            "show_videos",
            "show_favorites",
        ]
        return "\n".join(f"{key}={_clean(values.get(key), 1000)}" for key in keys) + "\n"

    def sync(
        self, mount_path, device=None, refresh_channels=True,
        progress_callback=None, live_only=False,
    ):
        progress = progress_callback or (lambda _done, _total, _label: None)
        progress(0, 0, "Checking the mounted iPod…")
        mount = validate_device_root(mount_path)
        video_target = (
            str(getattr(device, "rockbox_target", "") or "").strip().lower()
            or device_video_target(mount)
        )
        root = resolve_under_root(mount, YOUTUBE_DEVICE_ROOT)
        existing_library_rows = {}
        existing_library_path = os.path.join(root, "library.tsv")
        if live_only and os.path.isfile(existing_library_path):
            with open(
                existing_library_path, "r", encoding="utf-8", errors="replace"
            ) as handle:
                for raw in handle:
                    line = raw.rstrip("\r\n")
                    fields = line.split("\t")
                    if fields and fields[0] != "id":
                        existing_library_rows[fields[0]] = line
        channel_report = (
            self.sync_channel_uploads(progress_callback=progress)
            if refresh_channels and not live_only
            else {"channels": 0, "videos_added": 0, "videos_removed": 0}
        )
        all_videos = self.list_videos()
        videos = (
            [row for row in all_videos if row.get("is_live")]
            if live_only else all_videos
        )
        export_total = len(videos) + 3
        progress(0, export_total, "Validating YouTube videos and thumbnails…")
        profile = self.get_profile()
        for row in videos:
            if not os.path.isfile(row["source_path"]):
                raise ValueError(f"Missing YouTube source video: {row['title']}")
            thumbnail = row.get("thumbnail_path") or ""
            if thumbnail and not os.path.isfile(thumbnail):
                raise ValueError(f"Missing YouTube thumbnail: {row['title']}")
        progress(0, export_total, "Importing iPod ratings and favorites…")
        merged = self._merge_device_state(mount)
        all_videos = self.list_videos()
        videos = (
            [row for row in all_videos if row.get("is_live")]
            if live_only else all_videos
        )
        media_root = resolve_under_root(mount, YOUTUBE_MEDIA_ROOT)
        thumb_root = resolve_under_root(mount, f"{YOUTUBE_DEVICE_ROOT}/thumbnails")
        preview_root = resolve_under_root(mount, YOUTUBE_PREVIEW_ROOT)
        staging = resolve_under_root(mount, f"{YOUTUBE_DEVICE_ROOT}/.staging")
        os.makedirs(root, exist_ok=True)
        os.makedirs(media_root, exist_ok=True)
        os.makedirs(thumb_root, exist_ok=True)
        os.makedirs(preview_root, exist_ok=True)
        os.makedirs(staging, exist_ok=True)
        sync_index = DeviceSyncIndex(self.config, mount)
        report = {
            "videos": len(videos),
            "live_videos": sum(1 for row in all_videos if row.get("is_live")),
            "live_only": bool(live_only),
            "media_updated": 0,
            "media_unchanged": 0,
            "thumbnails_updated": 0,
            "previews_updated": 0,
            "previews_unchanged": 0,
            "metadata_updated": 1,
            "profile_updated": 1,
            "local_state_merged": merged,
            "assets_updated": 0,
            "stale_files_removed": 0,
            "channels_refreshed": channel_report["channels"],
            "channel_videos_removed": channel_report["videos_removed"],
        }
        library_rows = [
            "id\ttitle\tuploader\tduration\tupload_date\tviews\t"
            "rating_x100\trating_count\tcategory\tshow_home\tshow_profile\t"
            "home_order\tvideo_path\tthumb_path\tdescription\ttags\t"
            "favorite\tmy_rating\tsource_type\tmenu_preview\tis_live\t"
            "live_start_epoch\tlive_creator_key"
        ]
        state_rows = ["id\tmy_rating\tfavorite"]
        for video_index, row in enumerate(videos, 1):
            progress(
                video_index - 1,
                export_total,
                f"Preparing video {video_index}/{len(videos)}: {row['title']}",
            )
            updated, destination, signature = self._sync_media(
                row, mount, staging, sync_index, video_target
            )
            report["media_updated" if updated else "media_unchanged"] += 1
            self.db.execute(
                "UPDATE youtube_videos SET source_hash = ?, "
                "last_synced_source_hash = ? WHERE id = ?",
                (signature, signature, row["id"]),
            )
            thumb_target = os.path.join(thumb_root, f"{row['id']}.bmp")
            thumbnail_source = row.get("thumbnail_path") or row["source_path"]
            thumbnail_signature = (
                "youtube-thumb-100x75-v1:" + _source_signature(thumbnail_source)
            )
            if not sync_index.current_or_seed(
                "youtube", row["id"], "thumbnail-v1", thumbnail_signature,
                [thumb_target], trust_existing=not updated,
            ):
                if self._render_thumbnail(row, thumb_target, staging):
                    report["thumbnails_updated"] += 1
                sync_index.mark(
                    "youtube", row["id"], "thumbnail-v1",
                    thumbnail_signature, [thumb_target],
                )
            preview_target = os.path.join(preview_root, f"{row['id']}.bmp")
            preview_source = row.get("thumbnail_path") or row["source_path"]
            preview_signature = "menu-320x240-v1:" + _source_signature(preview_source)
            preview_signature_path = preview_target + ".source"
            if sync_index.current_or_seed(
                "youtube", row["id"], "menu-preview-v1",
                preview_signature, [preview_target],
                legacy_marker=preview_signature_path,
                legacy_signature=preview_signature,
            ):
                report["previews_unchanged"] += 1
            else:
                self._render_menu_preview(row, preview_target, staging)
                atomic_write_text(preview_signature_path, preview_signature)
                sync_index.mark(
                    "youtube", row["id"], "menu-preview-v1",
                    preview_signature, [preview_target],
                )
                report["previews_updated"] += 1
            ytm_target = os.path.join(media_root, f"{row['id']}.ytm")
            atomic_write_text(
                ytm_target,
                "\n".join(
                    [
                        f"title={_clean(row['title'], 80)}",
                        f"uploader={_clean(row['uploader'], 48)}",
                        f"added={_clean(row['upload_date'], 20)}",
                        f"views={int(row['view_count'] or 0):,} views",
                        f"duration={_duration_text(row['duration_ms'])}",
                        f"is_live={1 if row.get('is_live') else 0}",
                        f"live_start_epoch={int(row.get('live_start_epoch') or 0)}",
                    ]
                )
                + "\n",
            )
            values = [
                row["id"],
                _clean(row["title"], 80),
                _clean(row["uploader"], 48),
                _duration_text(row["duration_ms"]),
                _clean(row["upload_date"], 20),
                f"{int(row['view_count'] or 0):,} views",
                str(round(float(row["rating_average"] or 0) * 100)),
                str(int(row["rating_count"] or 0)),
                _clean(row["category"], 32),
                "1" if row["show_home"] else "0",
                "1" if row["show_profile"] else "0",
                str(int(row["home_order"] or 0)),
                "/" + os.path.relpath(destination, mount).replace(os.sep, "/"),
                f"/.rockbox/youtube/thumbnails/{row['id']}.bmp",
                _clean(row["description"], 220),
                _clean(row["tags"], 110),
                "1" if row["favorite"] else "0",
                str(int(row["my_rating"] or 0)),
                _clean(row["source_type"], 24),
                f"/{YOUTUBE_PREVIEW_ROOT}/{row['id']}.bmp",
                "1" if row.get("is_live") else "0",
                str(int(row.get("live_start_epoch") or 0)),
                _clean(row.get("live_creator_key"), 64),
            ]
            library_rows.append("\t".join(values))
            state_rows.append(
                f"{row['id']}\t{int(row['my_rating'] or 0)}\t"
                f"{'1' if row['favorite'] else '0'}"
            )
            progress(
                video_index,
                export_total,
                f"Finished video {video_index}/{len(videos)}: {row['title']}",
            )
        if live_only:
            synced_ids = {row["id"] for row in videos}
            for row in all_videos:
                if row["id"] in synced_ids:
                    continue
                existing = existing_library_rows.get(row["id"])
                if not existing:
                    continue
                fields = existing.split("\t")
                if len(fields) > 22 and not row.get("is_live"):
                    fields[18] = _clean(row.get("source_type"), 24)
                    fields[20] = "0"
                    fields[21] = "0"
                    fields[22] = ""
                library_rows.append("\t".join(fields))
            state_rows = ["id\tmy_rating\tfavorite"] + [
                f"{row['id']}\t{int(row['my_rating'] or 0)}\t"
                f"{'1' if row['favorite'] else '0'}"
                for row in all_videos
            ]
        progress(
            len(videos) + 1,
            export_total,
            "Removing obsolete YouTube exports…",
        )
        valid_video_ids = {row["id"] for row in all_videos}
        legacy_reconcile = sync_index.needs_legacy_reconcile("youtube")
        if legacy_reconcile:
            report["stale_files_removed"] = self._remove_stale_exports(
                media_root, thumb_root, preview_root, valid_video_ids
            )
            sync_index.mark_legacy_reconciled("youtube")
        stale_ids = sync_index.prune("youtube", valid_video_ids)
        if not legacy_reconcile:
            for stale_id in stale_ids:
                for path in (
                    os.path.join(media_root, stale_id + ".mpg"),
                    os.path.join(media_root, stale_id + ".m4v"),
                    os.path.join(media_root, stale_id + ".ytm"),
                    os.path.join(thumb_root, stale_id + ".bmp"),
                    os.path.join(preview_root, stale_id + ".bmp"),
                    os.path.join(preview_root, stale_id + ".bmp.source"),
                ):
                    try:
                        os.unlink(path)
                        report["stale_files_removed"] += 1
                    except FileNotFoundError:
                        pass
        progress(
            len(videos) + 2,
            export_total,
            "Writing YouTube library and profile metadata…",
        )
        atomic_write_text(os.path.join(root, "library.tsv"), "\n".join(library_rows) + "\n")
        atomic_write_text(os.path.join(root, "state.tsv"), "\n".join(state_rows) + "\n")
        atomic_write_text(
            os.path.join(root, "profile.cfg"),
            self._profile_lines(profile, mount, staging),
        )
        asset_source = self.repo_root / "assets" / "ipodjs" / "rockbox" / "youtube"
        asset_target = resolve_under_root(mount, YOUTUBE_ASSET_ROOT)
        os.makedirs(asset_target, exist_ok=True)
        progress(
            len(videos) + 3,
            export_total,
            "Updating authentic 2007 YouTube assets and menu settings…",
        )
        for name in (
            "youtube-logo-2006.bmp",
            "youtube-stars-5-2007.bmp",
            "youtube-stars-active-2007.bmp",
            "youtube-player-2007.bmp",
            "youtube-player-seek-knob-2007.bmp",
            "youtube-player-volume-knob-2007.bmp",
        ):
            if _copy_if_changed(str(asset_source / name), os.path.join(asset_target, name)):
                report["assets_updated"] += 1
        if device is not None:
            from services.rockbox_device import set_rockbox_youtube_main_menu

            set_rockbox_youtube_main_menu(
                device, bool(profile.get("show_on_main_menu"))
            )
        sync_index.close()
        try:
            os.rmdir(staging)
        except OSError:
            pass
        progress(export_total, export_total, "YouTube sync complete")
        return report
