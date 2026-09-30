"""Creator-centric offline Twitch VOD library and iPod sync."""

from __future__ import annotations

import calendar
import hashlib
import json
import os
import re
import shutil
import subprocess
from datetime import datetime
from pathlib import Path
from urllib.parse import urlparse, urlunparse

from PIL import Image, ImageOps

from services.app_video_sync import (
    app_video_extension,
    app_video_profile,
    app_video_signature,
    device_video_target,
    remove_alternate_video,
    stage_app_video,
)
from services.device_sync_index import DeviceSyncIndex
from services.file_safety import atomic_write_text
from services.path_safety import resolve_under_root, validate_device_root
from services.twitch_chat import download_chat_replay


TWITCH_DEVICE_ROOT = ".rockbox/twitch"
TWITCH_MEDIA_ROOT = "Twitch/videos"
TWITCH_ASSET_ROOT = ".rockbox/ipodjs/twitch"
TWITCH_URL_RE = re.compile(r"(?:www\.)?twitch\.tv/videos/(\d+)", re.I)
TWITCH_LOGIN_RE = re.compile(r"^[A-Za-z0-9_]{2,25}$")


def _clean(value, limit=240):
    return " ".join(str(value or "").replace("\t", " ").split())[:limit]


def _local_epoch_now():
    """Return an epoch compatible with Rockbox's timezone-free RTC fields."""
    return int(calendar.timegm(datetime.now().timetuple()))


def _source_signature(path):
    stat = os.stat(path)
    return f"{stat.st_size}:{stat.st_mtime_ns}"


def _clean_mpeg_fallback_source(path):
    """Prefer a pre-cleaned MPEG fallback beside a downloaded Twitch VOD."""
    source = Path(path)
    for suffix in (".mpg", ".mpeg"):
        candidate = source.with_name(source.stem + "-clean" + suffix)
        if candidate.is_file():
            return str(candidate)
    return str(source)


def _creator_key(login):
    return hashlib.sha1(str(login).lower().encode("utf-8")).hexdigest()[:16]


def normalize_twitch_creator_url(url):
    """Validate a Twitch creator URL and discard page/query suffixes."""
    text = str(url or "").strip()
    if TWITCH_LOGIN_RE.fullmatch(text):
        login = text.lower()
        return f"https://www.twitch.tv/{login}"
    parsed = urlparse(text)
    host = parsed.netloc.lower().removeprefix("www.")
    parts = [part for part in parsed.path.split("/") if part]
    login = parts[0].lower() if parts else ""
    reserved = {"videos", "directory", "downloads", "jobs", "p", "settings"}
    if (
        parsed.scheme not in {"http", "https"}
        or host != "twitch.tv"
        or not TWITCH_LOGIN_RE.fullmatch(login)
        or login in reserved
    ):
        raise ValueError(
            "Enter a Twitch creator URL (for example https://www.twitch.tv/creator)"
        )
    return urlunparse(("https", "www.twitch.tv", f"/{login}", "", "", ""))


def twitch_vod_id(url):
    match = TWITCH_URL_RE.search(str(url or "").strip())
    if not match:
        raise ValueError("Enter a Twitch VOD URL such as https://www.twitch.tv/videos/123")
    return match.group(1)


def current_creator_programme(vods, cycle_epoch, now=None):
    """Resolve the one VOD on a creator's wall-clock channel right now.

    Returns ``(row, offset_seconds, programme_start_epoch)``. Rows with no
    positive duration cannot form a live schedule and are ignored.
    """
    now = int(_local_epoch_now() if now is None else now)
    ordered = sorted(
        (dict(row) for row in vods if int(row.get("duration_ms") or 0) >= 1000),
        key=lambda row: (str(row.get("published_date") or ""), str(row.get("id") or "")),
    )
    if not ordered:
        return None
    durations = [max(1, int(row["duration_ms"]) // 1000) for row in ordered]
    total = sum(durations)
    epoch = int(cycle_epoch or now)
    elapsed = max(0, now - epoch) % total
    cursor = 0
    for row, duration in zip(ordered, durations):
        if elapsed < cursor + duration:
            offset = elapsed - cursor
            return row, offset, now - offset
        cursor += duration
    return ordered[0], 0, now


def _probe_duration_ms(path, ffprobe="ffprobe"):
    try:
        result = subprocess.run(
            [
                ffprobe, "-v", "error", "-show_entries", "format=duration",
                "-of", "default=noprint_wrappers=1:nokey=1", str(path),
            ],
            check=False,
            capture_output=True,
            text=True,
            timeout=30,
        )
        return max(0, int(float(result.stdout.strip()) * 1000))
    except (OSError, ValueError, subprocess.SubprocessError):
        return 0


def _probe_stream_types(path, ffprobe="ffprobe"):
    try:
        result = subprocess.run(
            [
                ffprobe, "-v", "error", "-show_entries", "stream=codec_type",
                "-of", "default=noprint_wrappers=1:nokey=1", str(path),
            ],
            check=False,
            capture_output=True,
            text=True,
            timeout=30,
        )
        return set(result.stdout.split()) if result.returncode == 0 else set()
    except (OSError, subprocess.SubprocessError):
        return set()


def _validate_export(path, *, config, profile, require_audio, device_target):
    """Check staged, cached and preserved VODs before catalogue publication."""
    h264 = profile == "h264_apple_exact"
    # The updated 6G H.264 reader supports the full FAT range. MPEG and
    # other target readers still use signed 32-bit file offsets.
    limit = 0xffffffff if h264 and device_target == "ipod6g" else 0x7fffffff
    size = os.path.getsize(path)
    if not 0 < size <= limit:
        raise ValueError(f"VOD size {size} exceeds the player limit {limit}")
    result = subprocess.run(
        [str(config.get("ffprobe_binary", "ffprobe")), "-v", "error",
         "-show_entries",
         "stream=codec_type,codec_name,profile,sample_rate,channels,nb_frames",
         "-of", "json", str(path)],
        check=True, capture_output=True, text=True, timeout=60,
    )
    streams = json.loads(result.stdout).get("streams") or []
    video = next((s for s in streams if s.get("codec_type") == "video"), {})
    audio = next((s for s in streams if s.get("codec_type") == "audio"), {})
    if video.get("codec_name") not in (
        {"h264"} if h264 else {"mpeg1video", "mpeg2video"}
    ):
        raise ValueError("VOD has no supported video track")
    if require_audio and not audio:
        raise ValueError("VOD is missing its required audio track")
    if audio:
        if h264:
            if (audio.get("codec_name") != "aac"
                    or audio.get("profile") != "LC"
                    or int(audio.get("sample_rate") or 0) != 44100
                    or int(audio.get("channels") or 0) != 2):
                raise ValueError("VOD audio must be stereo AAC-LC at 44.1 kHz")
        elif audio.get("codec_name") not in {"mp2", "mp3"}:
            raise ValueError("MPEG VOD has unsupported audio")
    if h264 and device_target == "ipod6g":
        # Keep aligned with VIDEO_MAX_{AUDIO_,}SAMPLES in video_playback.c.
        for stream, maximum, label in ((video, 400000, "video"),
                                        (audio, 600000, "audio")):
            if not stream:
                continue
            try:
                count = int(stream.get("nb_frames") or 0)
            except (TypeError, ValueError):
                count = 0
            if not 0 < count <= maximum:
                raise ValueError(
                    f"VOD {label} sample count is unknown or exceeds {maximum}"
                )


def _copy_if_changed(source, target):
    source = str(source)
    target = str(target)
    if os.path.isfile(target):
        left = os.stat(source)
        right = os.stat(target)
        if left.st_size == right.st_size:
            with open(source, "rb") as src, open(target, "rb") as dst:
                if hashlib.sha256(src.read()).digest() == hashlib.sha256(dst.read()).digest():
                    return False
    os.makedirs(os.path.dirname(target), exist_ok=True)
    temporary = target + ".rockpod-tmp"
    shutil.copy2(source, temporary)
    os.replace(temporary, target)
    return True


class TwitchAppService:
    """RockPod-side source of truth for the Twitch iPod application."""

    def __init__(self, database, config, repo_root):
        self.db = database
        self.config = config
        self.repo_root = Path(repo_root)

    def list_creators(self):
        rows = self.db.fetchall(
            "SELECT c.*, COUNT(v.id) AS vod_count FROM twitch_creators c "
            "LEFT JOIN twitch_vods v ON v.creator_key = c.creator_key "
            "GROUP BY c.creator_key ORDER BY LOWER(c.display_name), c.login"
        )
        return [dict(row) for row in rows]

    def get_creator(self, creator_key):
        row = self.db.fetchone(
            "SELECT * FROM twitch_creators WHERE creator_key = ?", (creator_key,)
        )
        return dict(row) if row else None

    def add_creator(
        self, channel_url, display_name="", keep_count=3,
        cycle_epoch=0, avatar_path="",
    ):
        normalized = normalize_twitch_creator_url(channel_url)
        login = normalized.rstrip("/").rsplit("/", 1)[-1]
        key = _creator_key(login)
        keep = min(20, max(1, int(keep_count or 3)))
        epoch = int(cycle_epoch or _local_epoch_now())
        avatar = os.path.abspath(os.path.expanduser(str(avatar_path))) if avatar_path else ""
        self.db.execute(
            "INSERT INTO twitch_creators "
            "(creator_key, channel_url, login, display_name, keep_count, "
            "cycle_epoch, avatar_path) VALUES (?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(creator_key) DO UPDATE SET "
            "channel_url=excluded.channel_url, login=excluded.login, "
            "display_name=excluded.display_name, keep_count=excluded.keep_count, "
            "avatar_path=CASE WHEN excluded.avatar_path <> '' "
            "THEN excluded.avatar_path ELSE twitch_creators.avatar_path END, "
            "date_modified=datetime('now')",
            (key, normalized, login, _clean(display_name, 64), keep, epoch, avatar),
        )
        return self.get_creator(key)

    def update_creator(self, creator_key, **updates):
        allowed = {
            key: value for key, value in updates.items()
            if key in {"display_name", "keep_count", "cycle_epoch", "avatar_path"}
        }
        if not allowed:
            return self.get_creator(creator_key)
        if "display_name" in allowed:
            allowed["display_name"] = _clean(allowed["display_name"], 64)
        if "keep_count" in allowed:
            allowed["keep_count"] = min(20, max(1, int(allowed["keep_count"] or 3)))
        if "cycle_epoch" in allowed:
            allowed["cycle_epoch"] = int(allowed["cycle_epoch"] or _local_epoch_now())
        if "avatar_path" in allowed:
            allowed["avatar_path"] = os.path.abspath(
                os.path.expanduser(str(allowed["avatar_path"] or ""))
            ) if allowed["avatar_path"] else ""
        assignment = ", ".join(f"{key} = ?" for key in allowed)
        self.db.execute(
            f"UPDATE twitch_creators SET {assignment}, "
            "date_modified=datetime('now') WHERE creator_key = ?",
            (*allowed.values(), creator_key),
        )
        return self.get_creator(creator_key)

    def remove_creator(self, creator_key):
        self.db.execute("DELETE FROM twitch_creators WHERE creator_key = ?", (creator_key,))

    def list_vods(self, creator_key=""):
        if creator_key:
            rows = self.db.fetchall(
                "SELECT v.*, c.display_name, c.login, c.channel_url, c.cycle_epoch "
                "FROM twitch_vods v JOIN twitch_creators c USING (creator_key) "
                "WHERE v.creator_key = ? "
                "ORDER BY v.published_date DESC, v.id DESC",
                (creator_key,),
            )
        else:
            rows = self.db.fetchall(
                "SELECT v.*, c.display_name, c.login, c.channel_url, c.cycle_epoch "
                "FROM twitch_vods v JOIN twitch_creators c USING (creator_key) "
                "ORDER BY LOWER(c.display_name), v.published_date DESC, v.id DESC"
            )
        return [dict(row) for row in rows]

    def get_vod(self, vod_id):
        row = self.db.fetchone(
            "SELECT v.*, c.display_name, c.login, c.channel_url, c.cycle_epoch "
            "FROM twitch_vods v JOIN twitch_creators c USING (creator_key) "
            "WHERE v.id = ?", (vod_id,),
        )
        return dict(row) if row else None

    @staticmethod
    def chat_source_paths(row):
        source = Path(str((row or {}).get("source_path") or ""))
        if not source.name:
            return Path(), Path()
        return source.with_suffix(".twc"), source.with_suffix(".twe")

    def refresh_vod_chat(self, vod_id, progress_callback=None):
        row = self.get_vod(vod_id)
        if row is None or not row.get("twitch_id"):
            raise ValueError("This item has no Twitch VOD ID for chat replay")
        chat_path, emoji_path = self.chat_source_paths(row)
        try:
            return download_chat_replay(
                row["twitch_id"], chat_path, emoji_path,
                progress_callback=progress_callback,
            )
        except Exception as exc:
            raise ValueError(
                f"Twitch chat replay download failed: {_clean(exc, 300)}"
            ) from exc

    def add_vod(self, source_path, creator_key="", creator_url="", **metadata):
        source = os.path.abspath(os.path.expanduser(str(source_path or "")))
        if not os.path.isfile(source):
            raise ValueError("Choose an existing local Twitch VOD file")
        creator = self.get_creator(creator_key) if creator_key else None
        if creator is None:
            creator = self.add_creator(
                creator_url or metadata.get("channel_url") or metadata.get("login"),
                metadata.get("display_name") or metadata.get("creator") or "",
            )
        twitch_id = _clean(metadata.get("twitch_id"), 32)
        vod_id = f"tw_{twitch_id}" if twitch_id else "tw_" + hashlib.sha1(
            os.path.realpath(source).encode("utf-8")
        ).hexdigest()[:12]
        duration_ms = int(metadata.get("duration_ms") or _probe_duration_ms(
            source, self.config.get("ffprobe_binary", "ffprobe")
        ))
        values = {
            "twitch_id": twitch_id,
            "creator_key": creator["creator_key"],
            "source_path": source,
            "title": _clean(metadata.get("title") or Path(source).stem, 96),
            "game": _clean(metadata.get("game") or "Just Chatting", 48),
            "description": _clean(metadata.get("description"), 500),
            "duration_ms": max(0, duration_ms),
            "published_date": _clean(
                metadata.get("published_date") or datetime.now().strftime("%Y-%m-%d"), 24
            ),
            "view_count": max(0, int(metadata.get("view_count") or 0)),
            "source_url": _clean(metadata.get("source_url"), 500),
            "source_type": _clean(metadata.get("source_type") or "manual", 24),
            "thumbnail_path": os.path.abspath(
                os.path.expanduser(str(metadata.get("thumbnail_path") or ""))
            ) if metadata.get("thumbnail_path") else "",
            "source_hash": _source_signature(source),
        }
        columns = ", ".join(values)
        placeholders = ", ".join("?" for _ in values)
        updates = ", ".join(
            f"{column}=excluded.{column}" for column in values
            if column != "creator_key"
        )
        self.db.execute(
            f"INSERT INTO twitch_vods (id, {columns}) VALUES (?, {placeholders}) "
            f"ON CONFLICT(id) DO UPDATE SET {updates}, "
            "creator_key=excluded.creator_key, date_modified=datetime('now')",
            (vod_id, *values.values()),
        )
        return self.get_vod(vod_id)

    def remove_vod(self, vod_id):
        self.db.execute("DELETE FROM twitch_vods WHERE id = ?", (vod_id,))

    def import_url(self, url, creator_key=""):
        twitch_id = twitch_vod_id(url)
        managed = Path.home() / ".rockpod" / "twitch"
        managed.mkdir(parents=True, exist_ok=True)
        output = str(managed / "%(id)s.%(ext)s")
        command = [
            self.config.get("yt_dlp_binary", "yt-dlp"),
            "--no-playlist", "--force-overwrites",
            "-f", (
                "best[height<=480][vcodec!=none][acodec!=none]/"
                "bestvideo[height<=480]+bestaudio/best"
            ),
            "--merge-output-format", "mp4",
            "--write-info-json", "--write-thumbnail", "--convert-thumbnails", "jpg",
            "-o", output, str(url).strip(),
        ]
        try:
            subprocess.run(
                command, check=True, capture_output=True, text=True, timeout=60 * 60
            )
        except (OSError, subprocess.CalledProcessError) as exc:
            detail = getattr(exc, "stderr", "") or str(exc)
            raise ValueError(f"Twitch VOD download failed: {_clean(detail, 300)}") from exc
        candidates = [
            managed / f"{twitch_id}.info.json",
            managed / f"v{twitch_id}.info.json",
        ]
        info_path = next((path for path in candidates if path.is_file()), None)
        if info_path is None:
            raise ValueError("Twitch download finished without metadata")
        with info_path.open("r", encoding="utf-8") as handle:
            info = json.load(handle)
        media = []
        for prefix in (twitch_id, "v" + twitch_id):
            media.extend(
                path for path in managed.glob(f"{prefix}.*")
                if path.suffix.lower()
                not in {".json", ".jpg", ".jpeg", ".webp", ".part", ".ytdl"}
            )
        if not media:
            raise ValueError("Twitch download finished without a video file")
        complete_media = [
            path for path in media
            if {"video", "audio"} <= _probe_stream_types(
                path, self.config.get("ffprobe_binary", "ffprobe")
            )
        ]
        if not complete_media:
            raise ValueError(
                "Twitch download finished without complete video and audio tracks"
            )
        source = max(complete_media, key=lambda path: path.stat().st_size)
        creator_url = (
            info.get("uploader_url") or info.get("channel_url") or
            (self.get_creator(creator_key) or {}).get("channel_url") or ""
        )
        if not creator_url:
            login = info.get("uploader_id") or info.get("channel_id") or info.get("uploader")
            creator_url = str(login or "")
        creator = self.get_creator(creator_key) if creator_key else None
        if creator is None:
            creator = self.add_creator(
                creator_url,
                info.get("uploader") or info.get("channel") or "Twitch Creator",
            )
        thumbnail = next(
            (
                path for path in (
                    managed / f"{twitch_id}.jpg",
                    managed / f"v{twitch_id}.jpg",
                ) if path.is_file()
            ),
            managed / f"{twitch_id}.jpg",
        )
        timestamp = info.get("timestamp")
        published = (
            datetime.fromtimestamp(timestamp).strftime("%Y-%m-%d %H:%M")
            if timestamp else str(info.get("upload_date") or "")
        )
        if len(published) == 8 and published.isdigit():
            published = f"{published[:4]}-{published[4:6]}-{published[6:]}"
        categories = info.get("categories") or []
        row = self.add_vod(
            source,
            creator_key=creator["creator_key"],
            twitch_id=twitch_id,
            title=info.get("title") or f"Twitch VOD {twitch_id}",
            game=info.get("game") or info.get("category") or (categories[0] if categories else ""),
            description=info.get("description") or "",
            duration_ms=int(float(info.get("duration") or 0) * 1000),
            published_date=published,
            view_count=info.get("view_count") or 0,
            source_url=str(url).strip(),
            source_type="twitch",
            thumbnail_path=str(thumbnail) if thumbnail.is_file() else "",
        )
        # Chat replay is optional. A removed or chat-disabled VOD must not
        # invalidate an otherwise complete video import.
        try:
            self.refresh_vod_chat(row["id"])
        except ValueError:
            pass
        return self.get_vod(row["id"])

    def _creator_vod_urls(self, creator, limit):
        url = creator["channel_url"] + "/videos?filter=archives&sort=time"
        command = [
            self.config.get("yt_dlp_binary", "yt-dlp"),
            "--flat-playlist", "--playlist-end", str(limit),
            "--dump-single-json", "--no-warnings", url,
        ]
        try:
            result = subprocess.run(
                command, check=True, capture_output=True, text=True, timeout=120
            )
            payload = json.loads(result.stdout)
        except (OSError, json.JSONDecodeError, subprocess.SubprocessError) as exc:
            detail = getattr(exc, "stderr", "") or str(exc)
            raise ValueError(
                f"Could not read {creator['display_name'] or creator['login']} VODs: "
                f"{_clean(detail, 300)}"
            ) from exc
        found = []
        for entry in payload.get("entries") or []:
            video_id = str((entry or {}).get("id") or "")
            webpage = str((entry or {}).get("url") or "")
            match = re.search(r"(\d{5,})", video_id) or TWITCH_URL_RE.search(webpage)
            if match:
                found.append(match.group(1))
        if not found:
            raise ValueError("The creator did not return any downloadable Twitch VODs")
        return found[:limit]

    def refresh_creators(self, progress_callback=None):
        progress = progress_callback or (lambda _done, _total, _label: None)
        creators = self.list_creators()
        report = {
            "creators": 0,
            "vods_added": 0,
            "vods_removed": 0,
            "creator_failures": 0,
            "download_failures": 0,
            "errors": [],
        }
        for creator_index, creator in enumerate(creators, 1):
            name = creator.get("display_name") or creator["login"]
            try:
                ids = self._creator_vod_urls(creator, creator["keep_count"])
            except Exception as exc:
                report["creator_failures"] += 1
                report["errors"].append(
                    f"{name}: could not read VOD list: {_clean(exc, 300)}"
                )
                progress(
                    creator_index, len(creators),
                    f"Skipped {name}: VOD list unavailable",
                )
                continue
            retained = []
            download_failed = False
            for vod_index, twitch_id in enumerate(ids, 1):
                progress(
                    vod_index - 1, len(ids),
                    f"Downloading {name} VOD {vod_index}/{len(ids)}…",
                )
                try:
                    existing = self.db.fetchone(
                        "SELECT id, source_path FROM twitch_vods "
                        "WHERE creator_key = ? AND twitch_id = ?",
                        (creator["creator_key"], twitch_id),
                    )
                    if (
                        existing
                        and os.path.isfile(existing["source_path"])
                        and {"video", "audio"} <= _probe_stream_types(
                            existing["source_path"],
                            self.config.get("ffprobe_binary", "ffprobe"),
                        )
                    ):
                        retained.append(existing["id"])
                    else:
                        row = self.import_url(
                            f"https://www.twitch.tv/videos/{twitch_id}",
                            creator["creator_key"],
                        )
                        retained.append(row["id"])
                        report["vods_added"] += 1
                except Exception as exc:
                    download_failed = True
                    report["download_failures"] += 1
                    report["errors"].append(
                        f"{name} VOD {twitch_id}: download failed: "
                        f"{_clean(exc, 300)}"
                    )
                    progress(
                        vod_index, len(ids),
                        f"Skipped failed {name} VOD {vod_index}/{len(ids)}",
                    )
                    continue
                progress(vod_index, len(ids), f"Ready: {name} VOD {vod_index}/{len(ids)}")
            stale = []
            # A partial network refresh is not authoritative. Keep older VODs
            # rather than deleting working local/device media because one new
            # Twitch download failed.
            if not download_failed:
                placeholders = ", ".join("?" for _ in retained)
                stale = self.db.fetchall(
                    "SELECT id FROM twitch_vods WHERE creator_key = ? "
                    "AND source_type = 'twitch' "
                    f"AND id NOT IN ({placeholders})",
                    (creator["creator_key"], *retained),
                )
            for row in stale:
                self.remove_vod(row["id"])
            report["creators"] += 1
            report["vods_removed"] += len(stale)
            progress(creator_index, len(creators), f"Refreshed {name}")
        return report

    def current_programme(self, creator_key, now=None):
        creator = self.get_creator(creator_key)
        if not creator:
            return None
        return current_creator_programme(
            self.list_vods(creator_key), creator["cycle_epoch"], now
        )

    def _render_thumbnail(self, row, target, staging):
        source = row.get("thumbnail_path") or ""
        extracted = ""
        if not os.path.isfile(source):
            extracted = os.path.join(staging, row["id"] + "-frame.jpg")
            subprocess.run(
                [
                    self.config.get("ffmpeg_binary", "ffmpeg"),
                    "-hide_banner", "-loglevel", "error", "-y", "-ss", "3",
                    "-i", row["source_path"], "-frames:v", "1", extracted,
                ],
                check=False,
                timeout=120,
            )
            source = extracted
        if os.path.isfile(source):
            with Image.open(source) as image:
                fitted = ImageOps.fit(
                    image.convert("RGB"), (96, 54), Image.Resampling.LANCZOS
                )
        else:
            fitted = Image.new("RGB", (96, 54), "#17141f")
        generated = os.path.join(staging, row["id"] + "-thumb.bmp")
        fitted.save(generated, "BMP")
        changed = _copy_if_changed(generated, target)
        os.unlink(generated)
        if extracted and os.path.isfile(extracted):
            os.unlink(extracted)
        return changed

    @staticmethod
    def _remove_stale_exports(media_root, thumb_root, valid_ids):
        removed = 0
        for directory, suffixes in (
            (media_root, {".mpg", ".m4v", ".twm", ".twc", ".twe"}),
            (thumb_root, {".bmp"}),
        ):
            for name in os.listdir(directory):
                stem, suffix = os.path.splitext(name)
                if stem.startswith("tw_") and suffix.lower() in suffixes and stem not in valid_ids:
                    os.unlink(os.path.join(directory, name))
                    removed += 1
        return removed

    def sync(
        self, mount_path, device=None, refresh_creators=True,
        progress_callback=None, video_profile=None,
    ):
        progress = progress_callback or (lambda _done, _total, _label: None)
        progress(0, 0, "Checking the mounted iPod…")
        mount = validate_device_root(mount_path)
        refresh = self.refresh_creators(progress) if refresh_creators else {
            "creators": 0, "vods_added": 0, "vods_removed": 0,
            "creator_failures": 0, "download_failures": 0, "errors": [],
        }
        creators = self.list_creators()
        vods = self.list_vods()
        export_vods = sorted(
            vods,
            key=lambda row: (
                str(row.get("creator_key") or ""),
                str(row.get("published_date") or ""),
                str(row.get("id") or ""),
            ),
        )
        chat_downloaded = 0
        if refresh_creators:
            for row in vods:
                source_chat, source_emoji = self.chat_source_paths(row)
                if (
                    not row.get("twitch_id")
                    or (source_chat.is_file() and source_emoji.is_file())
                ):
                    continue
                progress(0, 0, f"Downloading chat replay: {row['title']}…")
                try:
                    self.refresh_vod_chat(row["id"])
                    chat_downloaded += 1
                except ValueError:
                    # Chat may be disabled or no longer retained while the VOD
                    # itself remains valid and playable.
                    pass
        profile = app_video_profile(
            self.config, video_profile, source_app="twitch"
        )
        target_name = (
            str(getattr(device, "rockbox_target", "") or "").strip().lower()
            if device is not None else ""
        ) or device_video_target(mount)
        root = resolve_under_root(mount, TWITCH_DEVICE_ROOT)
        media_root = resolve_under_root(mount, TWITCH_MEDIA_ROOT)
        thumb_root = resolve_under_root(mount, f"{TWITCH_DEVICE_ROOT}/thumbnails")
        staging = resolve_under_root(mount, f"{TWITCH_DEVICE_ROOT}/.staging")
        for directory in (root, media_root, thumb_root, staging):
            os.makedirs(directory, exist_ok=True)
        sync_index = DeviceSyncIndex(self.config, mount)
        report = {
            "creators": len(creators), "vods": len(vods),
            "media_updated": 0, "media_unchanged": 0,
            "thumbnails_updated": 0, "assets_updated": 0,
            "stale_files_removed": 0,
            "creators_refreshed": refresh["creators"],
            "vods_downloaded": refresh["vods_added"],
            "vods_removed": refresh["vods_removed"],
            "chat_updated": 0,
            "chat_downloaded": chat_downloaded,
            "creator_failures": refresh.get("creator_failures", 0),
            "download_failures": refresh.get("download_failures", 0),
            "media_failed": 0,
            "media_preserved": 0,
            "mpeg_fallbacks": 0,
            "mpeg_backups_updated": 0,
            "mpeg_backups_unchanged": 0,
            "mpeg_backup_failures": 0,
            "fallback_details": [],
            "vods_exported": 0,
            "errors": list(refresh.get("errors") or []),
        }
        creator_rows = [
            "creator_key\tlogin\tdisplay_name\tcycle_epoch\tfollower_count\tavatar_path"
        ]
        vod_rows = [
            "id\tcreator_key\ttitle\tgame\tduration_seconds\tpublished_date\t"
            "views\tvideo_path\tthumb_path\tdescription"
        ]
        try:
            for index, row in enumerate(export_vods, 1):
                progress(index - 1, len(vods) + 2, f"Preparing Twitch VOD {index}/{len(vods)}: {row['title']}")
                prepared = False
                destination = ""
                signature = ""
                source_signature = ""
                attempt_errors = []
                source_has_audio = "audio" in _probe_stream_types(
                    row["source_path"],
                    self.config.get("ffprobe_binary", "ffprobe"),
                )
                attempt_profiles = [profile]
                if profile == "h264_apple_exact":
                    attempt_profiles.append("quality")
                for attempt_profile in attempt_profiles:
                    extension = app_video_extension(attempt_profile)
                    candidate_destination = resolve_under_root(
                        mount, f"{TWITCH_MEDIA_ROOT}/{row['id']}{extension}"
                    )
                    staged = os.path.join(staging, row["id"] + extension)
                    attempt_source = row["source_path"]
                    if attempt_profile == "quality" and profile == "h264_apple_exact":
                        attempt_source = _clean_mpeg_fallback_source(attempt_source)
                    try:
                        attempt_source_signature = _source_signature(attempt_source)
                        attempt_signature = app_video_signature(
                            attempt_profile, attempt_source_signature,
                            "twitch-media-v3", target_name,
                        )
                        current = sync_index.current_or_seed(
                            "twitch", row["id"], "video-media-v3",
                            attempt_signature, [candidate_destination],
                            trust_existing=(
                                row.get("last_synced_source_hash")
                                == attempt_signature
                            ),
                        ) and os.path.isfile(candidate_destination)
                        if current:
                            _validate_export(
                                candidate_destination, config=self.config,
                                profile=attempt_profile,
                                require_audio=source_has_audio,
                                device_target=target_name,
                            )
                            report["media_unchanged"] += 1
                        else:
                            stage_app_video(
                                attempt_source, staged,
                                config=self.config, profile=attempt_profile,
                                cache_namespace="twitch_video",
                                device_key=(
                                    "twitch-" + hashlib.sha1(
                                        str(mount).encode()
                                    ).hexdigest()[:12]
                                ),
                                title=row["title"],
                                artist=(
                                    row.get("display_name") or row.get("login")
                                ),
                                duration=float(row["duration_ms"] or 0) / 1000.0,
                                device_target=target_name,
                                require_audio=source_has_audio,
                            )
                            _validate_export(
                                staged, config=self.config,
                                profile=attempt_profile,
                                require_audio=source_has_audio,
                                device_target=target_name,
                            )
                            os.replace(staged, candidate_destination)
                            if not (
                                profile == "h264_apple_exact"
                                and attempt_profile == "h264_apple_exact"
                            ):
                                remove_alternate_video(candidate_destination)
                            sync_index.mark(
                                "twitch", row["id"], "video-media-v3",
                                attempt_signature, [candidate_destination],
                            )
                            report["media_updated"] += 1
                        prepared = True
                        destination = candidate_destination
                        signature = attempt_signature
                        source_signature = _source_signature(row["source_path"])
                        if attempt_profile == "quality" and profile == "h264_apple_exact":
                            report["mpeg_fallbacks"] += 1
                            report["fallback_details"].append(
                                f"{row['title']}: H.264 failed; MPEG succeeded"
                            )
                        break
                    except Exception as exc:
                        try:
                            os.unlink(staged)
                        except FileNotFoundError:
                            pass
                        label = (
                            "H.264" if attempt_profile == "h264_apple_exact"
                            else "MPEG"
                        )
                        attempt_errors.append(f"{label}: {_clean(exc, 300)}")
                if prepared:
                    self.db.execute(
                        "UPDATE twitch_vods SET source_hash = ?, "
                        "last_synced_source_hash = ? WHERE id = ?",
                        (source_signature, signature, row["id"]),
                    )

                    # Keep an MPEG sibling ready for device-side recovery.
                    # H.264 remains the exported/default path; the native
                    # player opens this only after VPU or AAC failure.
                    if (
                        profile == "h264_apple_exact"
                        and destination.endswith(".m4v")
                    ):
                        backup_source = _clean_mpeg_fallback_source(
                            row["source_path"]
                        )
                        backup_destination = resolve_under_root(
                            mount, f"{TWITCH_MEDIA_ROOT}/{row['id']}.mpg"
                        )
                        backup_staged = os.path.join(
                            staging, row["id"] + ".backup.mpg"
                        )
                        try:
                            backup_signature = app_video_signature(
                                "quality", _source_signature(backup_source),
                                "twitch-mpeg-backup-v1", target_name,
                            )
                            backup_current = sync_index.current_or_seed(
                                "twitch", row["id"], "mpeg-backup-v1",
                                backup_signature, [backup_destination],
                            ) and os.path.isfile(backup_destination)
                            if backup_current:
                                _validate_export(
                                    backup_destination, config=self.config,
                                    profile="quality",
                                    require_audio=source_has_audio,
                                    device_target=target_name,
                                )
                                report["mpeg_backups_unchanged"] += 1
                            else:
                                stage_app_video(
                                    backup_source, backup_staged,
                                    config=self.config, profile="quality",
                                    cache_namespace="twitch_video",
                                    device_key=(
                                        "twitch-" + hashlib.sha1(
                                            str(mount).encode()
                                        ).hexdigest()[:12]
                                    ),
                                    title=row["title"],
                                    artist=(
                                        row.get("display_name")
                                        or row.get("login")
                                    ),
                                    duration=(
                                        float(row["duration_ms"] or 0) / 1000.0
                                    ),
                                    device_target=target_name,
                                    require_audio=source_has_audio,
                                )
                                _validate_export(
                                    backup_staged, config=self.config,
                                    profile="quality",
                                    require_audio=source_has_audio,
                                    device_target=target_name,
                                )
                                os.replace(backup_staged, backup_destination)
                                sync_index.mark(
                                    "twitch", row["id"], "mpeg-backup-v1",
                                    backup_signature, [backup_destination],
                                )
                                report["mpeg_backups_updated"] += 1
                        except Exception as exc:
                            try:
                                os.unlink(backup_staged)
                            except FileNotFoundError:
                                pass
                            report["mpeg_backup_failures"] += 1
                            report["errors"].append(
                                f"{row['title']}: MPEG backup failed: "
                                f"{_clean(exc, 300)}"
                            )
                else:
                    report["media_failed"] += 1
                    report["errors"].append(
                        f"{row['title']}: " + "; ".join(attempt_errors)
                    )
                    # Preserve one previously playable output when every new
                    # preparation attempt fails. The exported path remains
                    # accurate and later VODs continue syncing.
                    preserve_extensions = (
                        (".m4v", ".mpg")
                        if profile == "h264_apple_exact" else (".mpg", ".m4v")
                    )
                    for preserve_extension in preserve_extensions:
                        candidate = resolve_under_root(
                            mount,
                            f"{TWITCH_MEDIA_ROOT}/{row['id']}{preserve_extension}",
                        )
                        if os.path.isfile(candidate):
                            try:
                                _validate_export(
                                    candidate, config=self.config,
                                    profile=("h264_apple_exact"
                                             if preserve_extension == ".m4v"
                                             else "quality"),
                                    require_audio=source_has_audio,
                                    device_target=target_name,
                                )
                            except Exception as exc:
                                report["errors"].append(
                                    f"{row['title']}: saved VOD invalid: "
                                    f"{_clean(exc, 300)}"
                                )
                                continue
                            destination = candidate
                            prepared = True
                            report["media_preserved"] += 1
                            report["media_unchanged"] += 1
                            break
                if not prepared:
                    progress(
                        index, len(vods) + 2,
                        f"Skipped failed VOD: {row['title']}",
                    )
                    continue
                thumb = os.path.join(thumb_root, row["id"] + ".bmp")
                thumb_source = row.get("thumbnail_path") or row["source_path"]
                try:
                    thumb_signature = (
                        "twitch-thumb-96x54-v1:"
                        + _source_signature(thumb_source)
                    )
                    if not sync_index.current_or_seed(
                        "twitch", row["id"], "thumbnail-v1", thumb_signature,
                        [thumb],
                    ):
                        if self._render_thumbnail(row, thumb, staging):
                            report["thumbnails_updated"] += 1
                        sync_index.mark(
                            "twitch", row["id"], "thumbnail-v1",
                            thumb_signature, [thumb],
                        )
                except Exception as exc:
                    report["errors"].append(
                        f"{row['title']}: thumbnail skipped: {_clean(exc, 300)}"
                    )
                metadata = os.path.join(media_root, row["id"] + ".twm")
                atomic_write_text(
                    metadata,
                    "\n".join([
                        "title=" + _clean(row["title"], 80),
                        "creator=" + _clean(row.get("display_name") or row.get("login"), 48),
                        "game=" + _clean(row.get("game"), 48),
                        f"views={int(row.get('view_count') or 0):,} views",
                    ]) + "\n",
                )
                source_chat, source_emoji = self.chat_source_paths(row)
                destination_chat = os.path.join(media_root, row["id"] + ".twc")
                destination_emoji = os.path.join(media_root, row["id"] + ".twe")
                if source_chat.is_file() and source_emoji.is_file():
                    if _copy_if_changed(source_chat, destination_chat):
                        report["chat_updated"] += 1
                    if _copy_if_changed(source_emoji, destination_emoji):
                        report["chat_updated"] += 1
                else:
                    for stale_chat in (destination_chat, destination_emoji):
                        try:
                            os.unlink(stale_chat)
                            report["stale_files_removed"] += 1
                        except FileNotFoundError:
                            pass
                vod_rows.append("\t".join([
                    row["id"], row["creator_key"], _clean(row["title"], 96),
                    _clean(row.get("game"), 48), str(max(0, int(row["duration_ms"]) // 1000)),
                    _clean(row.get("published_date"), 24),
                    str(int(row.get("view_count") or 0)),
                    "/" + os.path.relpath(destination, mount).replace(os.sep, "/"),
                    f"/{TWITCH_DEVICE_ROOT}/thumbnails/{row['id']}.bmp",
                    _clean(row.get("description"), 220),
                ]))
                report["vods_exported"] += 1
                progress(index, len(vods) + 2, f"Ready: {row['title']}")

            for creator in creators:
                creator_rows.append("\t".join([
                    creator["creator_key"], _clean(creator["login"], 25),
                    _clean(creator.get("display_name") or creator["login"], 64),
                    str(int(creator.get("cycle_epoch") or 0)),
                    str(int(creator.get("follower_count") or 0)), "",
                ]))
            atomic_write_text(os.path.join(root, "creators.tsv"), "\n".join(creator_rows) + "\n")
            atomic_write_text(os.path.join(root, "vods.tsv"), "\n".join(vod_rows) + "\n")
            progress(len(vods) + 1, len(vods) + 2, "Updating authentic Twitch assets…")
            asset_source = self.repo_root / "assets" / "ipodjs" / "rockbox" / "twitch"
            asset_target = resolve_under_root(mount, TWITCH_ASSET_ROOT)
            os.makedirs(asset_target, exist_ok=True)
            for name in (
                "twitch-wordmark-current-white.136x50.bmp",
                "twitch-glitch-current.18x20.bmp",
            ):
                if _copy_if_changed(asset_source / name, os.path.join(asset_target, name)):
                    report["assets_updated"] += 1
            valid_ids = {row["id"] for row in vods}
            report["stale_files_removed"] += self._remove_stale_exports(
                media_root, thumb_root, valid_ids
            )
            if sync_index.needs_legacy_reconcile("twitch"):
                sync_index.mark_legacy_reconciled("twitch")
            sync_index.prune("twitch", valid_ids)
            progress(len(vods) + 2, len(vods) + 2, "Twitch sync complete")
            return report
        finally:
            sync_index.close()
            try:
                os.rmdir(staging)
            except OSError:
                pass
