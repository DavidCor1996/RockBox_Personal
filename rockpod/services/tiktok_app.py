"""RockPod library and rolling account sync for the native TikTok app."""

from __future__ import annotations

import hashlib
import html
import csv
import json
import os
import re
import shutil
import subprocess
from types import SimpleNamespace
from datetime import date
from pathlib import Path
from urllib.parse import urlparse
from urllib.request import Request, urlopen

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
from services.device_manifest import present_manifest_rows
from services.file_safety import atomic_write_text
from services.path_safety import resolve_under_root, validate_device_root


TIKTOK_DEVICE_ROOT = ".rockbox/tiktok"
TIKTOK_MEDIA_ROOT = "TikTok/videos"
TIKTOK_THUMB_ROOT = ".rockbox/tiktok/thumbnails"
TIKTOK_AVATAR_ROOT = ".rockbox/tiktok/avatars"
TIKTOK_PREVIEW_ROOT = ".rockbox/tiktok/previews"
TIKTOK_VIDEO_SUFFIXES = {
    ".avi", ".m4v", ".mkv", ".mov", ".mp4", ".mpe", ".mpeg", ".mpg", ".webm",
}
TIKTOK_FEED_PATH = ".rockbox/rocks/apps/.ipodtiktok_feed.tsv"
TIKTOK_MEDIA_PIPELINE_VERSION = "tiktok-320x240-30fps-v2"
TIKTOK_MENU_PREVIEW_LIMIT = 64
TIKTOK_MENU_PREVIEW_VERSION = "menu-pane-160x240-source-v2"
TIKTOK_ASSET_ROOT = ".rockbox/ipodjs/tiktok"
TIKTOK_FIELDS = {
    "title",
    "creator",
    "description",
    "duration_ms",
    "upload_date",
    "like_count",
    "comment_count",
    "source_url",
    "thumbnail_path",
    "pin_order",
}


def _clean(value, limit=240):
    return " ".join(str(value or "").replace("\t", " ").split())[:limit]


def _source_signature(path):
    stat = os.stat(path)
    return f"{stat.st_size}:{stat.st_mtime_ns}"


def _stable_id(source_path, remote_id=""):
    if remote_id:
        safe = re.sub(r"[^A-Za-z0-9_-]", "", str(remote_id))[:40]
        if safe:
            return f"tt_{safe}"
    canonical = os.path.realpath(os.path.abspath(source_path))
    return "tt_" + hashlib.sha1(canonical.encode("utf-8")).hexdigest()[:16]


def _tiktok_url(value, account=False):
    text = str(value or "").strip()
    parsed = urlparse(text)
    host = parsed.netloc.lower().split(":", 1)[0]
    if host not in {
        "tiktok.com",
        "www.tiktok.com",
        "m.tiktok.com",
        "vm.tiktok.com",
        "vt.tiktok.com",
    }:
        raise ValueError("Enter a valid TikTok URL")
    if account:
        match = re.search(r"/@([^/?#]+)", parsed.path)
        if not match:
            raise ValueError("Enter a TikTok account URL such as tiktok.com/@name")
        return f"https://www.tiktok.com/@{match.group(1)}"
    if not re.search(r"/video/\d+", parsed.path) and host not in {
        "vm.tiktok.com",
        "vt.tiktok.com",
    }:
        raise ValueError("Enter a TikTok video URL")
    return text


def _parse_tiktok_embed(html_text, video_id):
    """Return media URL + yt-dlp-shaped metadata from TikTok's official embed."""
    match = re.search(
        r'<script[^>]+id="__FRONTITY_CONNECT_STATE__"[^>]*>(.*?)</script>',
        html_text,
        re.DOTALL,
    )
    if not match:
        raise ValueError("TikTok embed did not return video metadata")
    state = json.loads(html.unescape(match.group(1)))
    records = state.get("source", {}).get("data", {})
    record = next(
        (
            value for value in records.values()
            if isinstance(value, dict) and value.get("videoData")
        ),
        {},
    )
    video_data = record.get("videoData") or {}
    item = video_data.get("itemInfos") or {}
    author = video_data.get("authorInfos") or {}
    stats = video_data.get("authorStats") or {}
    urls = (item.get("video") or {}).get("urls") or []
    covers = item.get("covers") or []
    if str(item.get("id") or "") != str(video_id) or not urls:
        raise ValueError("TikTok embed did not return a playable video")
    create_time = str(item.get("createTime") or "")
    upload_date = ""
    if create_time.isdigit():
        upload_date = date.fromtimestamp(int(create_time)).strftime("%Y%m%d")
    info = {
        "id": str(video_id),
        "title": item.get("text") or "TikTok",
        "description": item.get("text") or "",
        "duration": (item.get("video") or {}).get("videoMeta", {}).get("duration", 0),
        "upload_date": upload_date,
        "uploader": author.get("uniqueId") or "TikTok",
        "uploader_url": (
            f"https://www.tiktok.com/@{author.get('uniqueId')}"
            if author.get("uniqueId") else ""
        ),
        "channel": author.get("nickName") or author.get("uniqueId") or "TikTok",
        # Keep the stable account key beside the video's own ``id``.  The
        # profile archiver can recover from any known creator video when
        # TikTok stops exposing this value on the profile page itself.
        "channel_id": author.get("secUid") or author.get("userId") or "",
        "uploader_id": author.get("userId") or "",
        "bio": author.get("signature") or "",
        "avatar_url": next(iter(author.get("covers") or []), ""),
        "verified": bool(author.get("verified")),
        "channel_follower_count": stats.get("followerCount"),
        "following_count": stats.get("followingCount"),
        "profile_like_count": stats.get("heartCount"),
        "video_count": stats.get("videoCount"),
        "like_count": item.get("diggCount"),
        "comment_count": item.get("commentCount"),
        "view_count": item.get("playCount"),
        "webpage_url": f"https://www.tiktok.com/@{author.get('uniqueId')}/video/{video_id}",
        "vcodec": "h264",
    }
    return str(urls[0]), str(covers[0] if covers else ""), info


def _parse_tiktok_profile_page(html_text):
    """Read TikTok's server-rendered public profile payload."""
    match = re.search(
        r'<script[^>]+id="__UNIVERSAL_DATA_FOR_REHYDRATION__"[^>]*>'
        r'(.*?)</script>',
        html_text,
        re.DOTALL,
    )
    if not match:
        raise ValueError("TikTok profile page did not return public metadata")
    payload = json.loads(match.group(1))
    detail = payload.get("__DEFAULT_SCOPE__", {}).get("webapp.user-detail", {})
    user_info = detail.get("userInfo") or {}
    user = user_info.get("user") or {}
    stats = user_info.get("statsV2") or user_info.get("stats") or {}
    username = str(user.get("uniqueId") or "").strip()
    sec_uid = str(user.get("secUid") or "").strip()
    if detail.get("statusCode") or not username or not sec_uid:
        raise ValueError("TikTok profile page did not return a public account")
    return {
        # TikTokUserIE uses its playlist id for the secondary user id. Keep
        # that shape so account archive callers can use either extractor.
        "id": sec_uid,
        "channel_id": str(user.get("id") or ""),
        "uploader": username,
        "uploader_url": f"https://www.tiktok.com/@{username}",
        "channel": user.get("nickname") or username,
        "bio": user.get("signature") or "",
        "avatar_url": (
            user.get("avatarLarger") or user.get("avatarMedium")
            or user.get("avatarThumb") or ""
        ),
        "verified": bool(user.get("verified")),
        "channel_follower_count": stats.get("followerCount") or 0,
        "following_count": stats.get("followingCount") or 0,
        "profile_like_count": stats.get("heartCount") or stats.get("heart") or 0,
        "video_count": stats.get("videoCount") or 0,
    }


def _frontity_embed_record(html_text, route_prefix):
    """Return one record from TikTok's official server-rendered embed."""
    match = re.search(
        r'<script[^>]+id=[\'\"]__FRONTITY_CONNECT_STATE__[\'\"][^>]*>'
        r'(.*?)</script>',
        html_text or "",
        re.DOTALL,
    )
    if not match:
        raise ValueError("TikTok embed did not return public metadata")
    try:
        records = json.loads(html.unescape(match.group(1))).get(
            "source", {}
        ).get("data", {})
    except (TypeError, json.JSONDecodeError) as exc:
        raise ValueError("TikTok embed returned invalid public metadata") from exc
    record = next(
        (
            value for key, value in records.items()
            if str(key).startswith(route_prefix) and isinstance(value, dict)
        ),
        {},
    )
    if not record:
        raise ValueError("TikTok embed did not return a public account")
    return record


def _parse_tiktok_profile_embed_videos(html_text):
    """Read public account metadata and ordered post IDs from a profile embed."""
    record = _frontity_embed_record(html_text, "/embed/@")
    user = record.get("userInfo") or {}
    username = str(user.get("uniqueId") or "").strip()
    videos = record.get("videoList") or []
    video_ids = [
        str(item.get("id")) for item in videos
        if item and str(item.get("id") or "").isdigit()
    ]
    video_ids = list(dict.fromkeys(video_ids))
    if record.get("isError") or not username or not video_ids:
        raise ValueError("TikTok profile embed did not return public posts")
    profile = {
        "channel_id": str(user.get("id") or ""),
        "uploader": username,
        "uploader_url": f"https://www.tiktok.com/@{username}",
        "channel": user.get("nickname") or username,
        "bio": user.get("signature") or "",
        "avatar_url": user.get("avatarLargerUrl") or user.get("avatarThumbUrl") or "",
        "verified": bool(user.get("verified")),
        "channel_follower_count": user.get("followerCount") or 0,
        "following_count": user.get("followingCount") or 0,
        "profile_like_count": user.get("heartCount") or 0,
        "video_count": user.get("videoCount") or 0,
    }
    return profile, video_ids


def _parse_tiktok_profile_embed(html_text):
    """Compatibility wrapper returning metadata and the first recent post."""
    profile, video_ids = _parse_tiktok_profile_embed_videos(html_text)
    return profile, video_ids[0]


def _parse_tiktok_video_author_embed(html_text):
    """Read the secondary account ID and authoritative stats from a video embed."""
    record = _frontity_embed_record(html_text, "/embed/v2/")
    video_data = record.get("videoData") or {}
    author = video_data.get("authorInfos") or {}
    stats = video_data.get("authorStats") or {}
    sec_uid = str(author.get("secUid") or "").strip()
    username = str(author.get("uniqueId") or "").strip()
    if not sec_uid or not username:
        raise ValueError("TikTok video embed did not return an account ID")
    return {
        "id": sec_uid,
        "channel_id": str(author.get("userId") or ""),
        "uploader": username,
        "uploader_url": f"https://www.tiktok.com/@{username}",
        "channel": author.get("nickName") or username,
        "bio": author.get("signature") or "",
        "avatar_url": next(iter(author.get("covers") or []), ""),
        "verified": bool(author.get("verified")),
        "channel_follower_count": stats.get("followerCount") or 0,
        "following_count": stats.get("followingCount") or 0,
        "profile_like_count": stats.get("heartCount") or 0,
        "video_count": stats.get("videoCount") or 0,
    }


def _upload_date(value):
    text = str(value or "")
    if len(text) == 8 and text.isdigit():
        return f"{text[:4]}-{text[4:6]}-{text[6:]}"
    return text


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


def _probe_stream_types(path, ffprobe="ffprobe"):
    """Return real container stream types; extractor metadata is not trusted."""
    try:
        result = subprocess.run(
            [
                ffprobe,
                "-v",
                "error",
                "-show_entries",
                "stream=codec_type",
                "-of",
                "default=noprint_wrappers=1:nokey=1",
                str(path),
            ],
            check=False,
            capture_output=True,
            text=True,
            timeout=30,
        )
        return set(result.stdout.split()) if result.returncode == 0 else set()
    except (OSError, subprocess.SubprocessError):
        return set()


def _has_audio_stream(path, ffprobe="ffprobe"):
    """Verify the downloaded container, not TikTok's unreliable format flags."""
    return "audio" in _probe_stream_types(path, ffprobe)


def _has_playable_streams(path, ffprobe="ffprobe"):
    streams = _probe_stream_types(path, ffprobe)
    return "video" in streams and "audio" in streams


def _stream_marker(path):
    return Path(path).with_name(Path(path).name + ".streams-ok")


def _unavailable_marker(path):
    return Path(path).with_name(Path(path).name + ".streams-unavailable")


def _mark_streams_verified(path):
    _stream_marker(path).write_text(_source_signature(path), encoding="ascii")
    _unavailable_marker(path).unlink(missing_ok=True)


def _streams_verified(path, ffprobe="ffprobe"):
    """Fast-path unchanged files while still probing every new/replaced source."""
    try:
        if _stream_marker(path).read_text(encoding="ascii") == _source_signature(path):
            return True
    except OSError:
        pass
    if not _has_playable_streams(path, ffprobe):
        return False
    _mark_streams_verified(path)
    return True


def _copy_if_changed(source, target):
    if os.path.isfile(target):
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


def _write_text_if_changed(path, text):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            if handle.read() == text:
                return False
    except OSError:
        pass
    atomic_write_text(path, text)
    return True


def _thumbnail_signature(row):
    thumbnail = str(row.get("thumbnail_path") or "")
    if thumbnail and os.path.isfile(thumbnail):
        return f"image:{_source_signature(thumbnail)}"
    return f"frame:{_source_signature(row['source_path'])}"


class TikTokAppService:
    """Source of truth for manual clips and followed-account rolling feeds."""

    def __init__(self, database, config, repo_root):
        self.db = database
        self.config = config
        self.repo_root = Path(repo_root)
        self._account_profiles = {}
        self._account_embed_uploads = {}
        self.backfill_profiles()

    def _yt_dlp_binary(self):
        """Prefer RockPod's dependency-complete yt-dlp for TikTok."""
        configured = self.config.get("yt_dlp_binary", "yt-dlp")
        bundled = self.repo_root / "rockpod" / ".venv" / "bin" / "yt-dlp"
        if configured == "yt-dlp" and bundled.is_file():
            return str(bundled)
        return configured

    def _account_profile_from_web(self, account_url):
        """Resolve profile stats and secUid without TikTokUserIE."""
        request = Request(
            account_url,
            headers={
                "User-Agent": (
                    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                    "Chrome/131.0.0.0 Safari/537.36"
                ),
                "Accept-Language": "en-US,en;q=0.9",
            },
        )
        with urlopen(request, timeout=45) as response:
            page = response.read(4 * 1024 * 1024).decode("utf-8", "replace")
        profile = _parse_tiktok_profile_page(page)
        expected = account_url.rsplit("/@", 1)[-1].lower()
        if str(profile.get("uploader") or "").lower() != expected:
            raise ValueError("TikTok returned a different public account")
        self._account_profiles[account_url] = profile
        return profile

    def _account_profile_from_embed(self, account_url):
        """Resolve profiles when TikTok's extractor and main page both fail."""
        headers = {
            "User-Agent": (
                "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                "Chrome/131.0.0.0 Safari/537.36"
            ),
            "Accept-Language": "en-US,en;q=0.9",
        }

        def fetch(url):
            with urlopen(Request(url, headers=headers), timeout=45) as response:
                return response.read(4 * 1024 * 1024).decode("utf-8", "replace")

        username = account_url.rsplit("/@", 1)[-1]
        profile, video_ids = _parse_tiktok_profile_embed_videos(
            fetch(f"https://www.tiktok.com/embed/@{username}")
        )
        if str(profile.get("uploader") or "").lower() != username.lower():
            raise ValueError("TikTok returned a different public account")
        try:
            author = _parse_tiktok_video_author_embed(
                fetch(f"https://www.tiktok.com/embed/v2/{video_ids[0]}")
            )
            if str(author.get("uploader") or "").lower() != username.lower():
                raise ValueError("TikTok returned a different public account")
            # Video embeds contain secUid and authoritative stats. Following
            # can still use the profile list if that second request is blocked.
            profile.update({key: value for key, value in author.items() if value})
        except (OSError, ValueError, json.JSONDecodeError):
            pass
        self._account_embed_uploads[account_url] = [
            f"{account_url}/video/{video_id}" for video_id in video_ids
        ]
        self._account_profiles[account_url] = profile
        return profile

    def _account_profile_from_video(self, video_url, account_url):
        """Resolve a stable account key from one official video embed."""
        match = re.search(r"/video/(\d+)", str(video_url or ""))
        if not match:
            raise ValueError("TikTok account recovery needs a public video URL")
        request = Request(
            f"https://www.tiktok.com/embed/v2/{match.group(1)}",
            headers={
                "User-Agent": (
                    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                    "Chrome/131.0.0.0 Safari/537.36"
                ),
                "Accept-Language": "en-US,en;q=0.9",
            },
        )
        with urlopen(request, timeout=45) as response:
            page = response.read(4 * 1024 * 1024).decode("utf-8", "replace")
        profile = _parse_tiktok_video_author_embed(page)
        expected = account_url.rsplit("/@", 1)[-1].lower()
        if str(profile.get("uploader") or "").lower() != expected:
            raise ValueError("TikTok returned a different video's account")
        return profile

    @staticmethod
    def _archive_account_key(profile):
        """Return TikTok's stable secUid from any supported metadata shape."""
        for field in ("sec_uid", "channel_id", "id"):
            value = str((profile or {}).get(field) or "").strip()
            if value.startswith("MS4wLjABAAAA"):
                return value
        return ""

    def list_videos(self):
        return [
            dict(row)
            for row in self.db.fetchall(
                "SELECT * FROM tiktok_videos ORDER BY "
                "CASE source_type WHEN 'manual' THEN 0 ELSE 1 END, "
                "CASE WHEN source_type='archive' THEN pin_order ELSE 0 END DESC, "
                "upload_date DESC, date_added DESC"
            )
        ]

    def recover_from_device(self, mount_path):
        """Restore RockPod metadata from a previously synced iPod library."""
        mount = Path(validate_device_root(mount_path))
        root = mount / TIKTOK_DEVICE_ROOT
        library_path = root / "library.tsv"
        profiles_path = root / "profiles.tsv"
        feed_path = mount / TIKTOK_FEED_PATH
        if not library_path.is_file() or not profiles_path.is_file():
            raise ValueError("The connected iPod has no TikTok library index")

        def read_rows(path):
            with path.open("r", encoding="utf-8", newline="") as handle:
                return list(csv.DictReader(handle, delimiter="\t"))

        library_rows = read_rows(library_path)
        profile_rows = read_rows(profiles_path)
        feed_rows = {
            row.get("id", ""): row for row in read_rows(feed_path)
        } if feed_path.is_file() else {}
        archived_accounts = {
            row.get("account_url", "") for row in library_rows
            if row.get("source_type") == "archive"
        }

        with self.db.transaction() as conn:
            for row in profile_rows:
                account_url = _clean(row.get("account_url"), 500)
                if not account_url:
                    continue
                avatar = str(row.get("avatar") or "")
                avatar_path = str(mount / avatar.lstrip("/")) if avatar else ""
                conn.execute(
                    "INSERT INTO tiktok_profiles (account_url, username, "
                    "display_name, bio, avatar_path, follower_count, "
                    "following_count, likes_count, video_count, verified) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
                    "ON CONFLICT(account_url) DO UPDATE SET "
                    "username=excluded.username, "
                    "display_name=excluded.display_name, bio=excluded.bio, "
                    "avatar_path=CASE WHEN excluded.avatar_path<>'' THEN "
                    "excluded.avatar_path ELSE avatar_path END, "
                    "follower_count=excluded.follower_count, "
                    "following_count=excluded.following_count, "
                    "likes_count=excluded.likes_count, "
                    "video_count=excluded.video_count, "
                    "verified=excluded.verified, date_modified=datetime('now')",
                    (
                        account_url,
                        _clean(row.get("username"), 96),
                        _clean(row.get("display_name"), 96),
                        _clean(row.get("bio"), 500),
                        avatar_path,
                        int(row.get("followers") or 0),
                        int(row.get("following") or 0),
                        int(row.get("likes") or 0),
                        int(row.get("videos") or 0),
                        1 if row.get("verified") == "1" else 0,
                    ),
                )
                if row.get("followed") == "1":
                    sync_mode = (
                        "archive" if account_url in archived_accounts
                        else "rolling"
                    )
                    conn.execute(
                        "INSERT INTO tiktok_account_syncs "
                        "(account_url, account_name, keep_count, sync_mode) "
                        "VALUES (?, ?, ?, ?) "
                        "ON CONFLICT(account_url) DO UPDATE SET "
                        "account_name=excluded.account_name, "
                        "keep_count=excluded.keep_count, "
                        "sync_mode=excluded.sync_mode, "
                        "date_modified=datetime('now')",
                        (
                            account_url,
                            "@" + _clean(row.get("username"), 96),
                            0 if sync_mode == "archive" else 10,
                            sync_mode,
                        ),
                    )

            for row in library_rows:
                video_id = _clean(row.get("id"), 48)
                if not video_id:
                    continue
                details = feed_rows.get(video_id, {})
                source_path = mount / str(row.get("path") or "").lstrip("/")
                thumbnail = mount / str(
                    row.get("thumbnail") or ""
                ).lstrip("/")
                if not source_path.is_file():
                    continue
                source_hash = _source_signature(source_path)
                remote_id = video_id[3:] if video_id.startswith("tt_") else ""
                account_url = _clean(row.get("account_url"), 500)
                source_url = (
                    f"{account_url}/video/{remote_id}"
                    if account_url and remote_id else ""
                )
                source_type = row.get("source_type")
                if source_type not in {"manual", "following", "archive"}:
                    source_type = "manual"
                conn.execute(
                    "INSERT INTO tiktok_videos (id, source_path, title, "
                    "creator, description, upload_date, like_count, "
                    "comment_count, source_url, source_type, account_url, "
                    "thumbnail_path, source_hash, last_synced_source_hash, "
                    "pin_order) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
                    "?, ?, ?) ON CONFLICT(id) DO UPDATE SET "
                    "title=excluded.title, creator=excluded.creator, "
                    "description=excluded.description, "
                    "upload_date=excluded.upload_date, "
                    "like_count=excluded.like_count, "
                    "comment_count=excluded.comment_count, "
                    "source_url=CASE WHEN excluded.source_url<>'' THEN "
                    "excluded.source_url ELSE source_url END, "
                    "source_type=excluded.source_type, "
                    "account_url=excluded.account_url, "
                    "pin_order=excluded.pin_order, date_modified=datetime('now')",
                    (
                        video_id,
                        str(source_path),
                        _clean(row.get("title"), 80),
                        _clean(row.get("creator"), 48),
                        _clean(details.get("description"), 500),
                        _clean(row.get("upload_date"), 20),
                        int(details.get("likes") or 0),
                        int(details.get("comments") or 0),
                        source_url,
                        source_type,
                        account_url,
                        str(thumbnail) if thumbnail.is_file() else "",
                        source_hash,
                        f"{TIKTOK_MEDIA_PIPELINE_VERSION}:{source_hash}",
                        int(details.get("pin_order") or 0),
                    ),
                )

        return {
            "videos": len(library_rows),
            "profiles": len(profile_rows),
            "accounts": sum(
                row.get("followed") == "1" for row in profile_rows
            ),
        }

    def get_video(self, video_id):
        row = self.db.fetchone(
            "SELECT * FROM tiktok_videos WHERE id = ?", (video_id,)
        )
        return dict(row) if row else None

    def add_video(self, source_path, **metadata):
        source = os.path.abspath(os.path.expanduser(str(source_path or "")))
        if not os.path.isfile(source):
            raise ValueError("Choose an existing local video file")
        video_id = _stable_id(source, metadata.get("remote_id"))
        values = {
            "title": _clean(metadata.get("title") or Path(source).stem, 80),
            "creator": _clean(metadata.get("creator") or "@you", 48),
            "description": _clean(metadata.get("description"), 500),
            "duration_ms": int(
                metadata.get("duration_ms")
                or _probe_duration_ms(
                    source, self.config.get("ffprobe_binary", "ffprobe")
                )
            ),
            "upload_date": _clean(
                metadata.get("upload_date") or date.today().isoformat(), 20
            ),
            "like_count": max(0, int(metadata.get("like_count") or 0)),
            "comment_count": max(0, int(metadata.get("comment_count") or 0)),
            "source_url": _clean(metadata.get("source_url"), 500),
            "source_type": (
                metadata.get("source_type")
                if metadata.get("source_type") in {"following", "archive"}
                else "manual"
            ),
            "account_url": _clean(metadata.get("account_url"), 500),
            "thumbnail_path": os.path.abspath(
                os.path.expanduser(str(metadata.get("thumbnail_path") or ""))
            )
            if metadata.get("thumbnail_path")
            else "",
            "pin_order": max(0, min(3, int(metadata.get("pin_order") or 0))),
        }
        columns = ", ".join(values)
        placeholders = ", ".join("?" for _ in values)
        updates = ", ".join(f"{column}=excluded.{column}" for column in values)
        self.db.execute(
            f"INSERT INTO tiktok_videos "
            f"(id, source_path, source_hash, {columns}) "
            f"VALUES (?, ?, ?, {placeholders}) "
            f"ON CONFLICT(id) DO UPDATE SET {updates}, "
            "source_path=excluded.source_path, source_hash=excluded.source_hash, "
            "date_modified=datetime('now')",
            (video_id, source, _source_signature(source), *values.values()),
        )
        return self.get_video(video_id)

    def update_video(self, video_id, updates):
        allowed = {
            key: value
            for key, value in dict(updates).items()
            if key in TIKTOK_FIELDS
        }
        if not allowed:
            return self.get_video(video_id)
        for key in ("duration_ms", "like_count", "comment_count", "pin_order"):
            if key in allowed:
                allowed[key] = max(0, int(allowed[key] or 0))
        for key in allowed:
            if key not in {
                "duration_ms", "like_count", "comment_count", "pin_order"
            }:
                allowed[key] = _clean(allowed[key], 500)
        assignment = ", ".join(f"{key} = ?" for key in allowed)
        self.db.execute(
            f"UPDATE tiktok_videos SET {assignment}, "
            "date_modified=datetime('now') WHERE id = ?",
            (*allowed.values(), video_id),
        )
        return self.get_video(video_id)

    def remove_video(self, video_id):
        row = self.get_video(video_id)
        if not row:
            return
        self.db.execute("DELETE FROM tiktok_videos WHERE id = ?", (video_id,))
        if row.get("source_type") == "following":
            self._remove_managed_following_files(row)

    def list_account_syncs(self):
        return [
            dict(row)
            for row in self.db.fetchall(
                "SELECT a.*, p.username, p.display_name, p.bio, p.avatar_path, "
                "p.follower_count, p.following_count, p.likes_count, "
                "p.video_count FROM tiktok_account_syncs a "
                "LEFT JOIN tiktok_profiles p ON p.account_url=a.account_url "
                "ORDER BY a.account_name, a.account_url"
            )
        ]

    def list_profiles(self):
        self.backfill_profiles()
        return [
            dict(row)
            for row in self.db.fetchall(
                "SELECT p.*, EXISTS(SELECT 1 FROM tiktok_account_syncs a "
                "WHERE a.account_url=p.account_url) AS followed "
                "FROM tiktok_profiles p ORDER BY p.username, p.display_name"
            )
        ]

    def backfill_profiles(self):
        """Recover profiles for URL imports created by an older RockPod run."""
        updated = 0
        for row in self.list_videos():
            source = Path(row.get("source_path") or "")
            info_path = source.with_suffix(".info.json")
            if not info_path.is_file():
                continue
            try:
                with info_path.open("r", encoding="utf-8") as handle:
                    info = json.load(handle)
                profile = self._upsert_profile(info, row.get("account_url") or "")
            except (OSError, ValueError, json.JSONDecodeError):
                continue
            if not profile:
                continue
            creator = _clean(info.get("uploader"), 48)
            if creator and not creator.startswith("@"):
                creator = "@" + creator
            self.db.execute(
                "UPDATE tiktok_videos SET account_url=?, "
                "creator=CASE WHEN ?<>'' THEN ? ELSE creator END, "
                "date_modified=datetime('now') WHERE id=?",
                (profile["account_url"], creator, creator, row["id"]),
            )
            updated += 1
        return updated

    def _upsert_profile(self, info, account_url=""):
        info = dict(info or {})
        username = _clean(info.get("uploader") or info.get("username"), 96)
        if username.startswith("@"):
            username = username[1:]
        account_url = _clean(
            account_url
            or info.get("uploader_url")
            or (f"https://www.tiktok.com/@{username}" if username else ""),
            500,
        )
        if not account_url:
            return None
        existing_row = self.db.fetchone(
            "SELECT * FROM tiktok_profiles WHERE account_url=?", (account_url,)
        )
        existing = dict(existing_row) if existing_row else {}

        def count_value(keys, existing_key):
            for key in keys:
                if info.get(key) is not None:
                    return max(0, int(info.get(key) or 0))
            return max(0, int(existing.get(existing_key) or 0))

        values = (
            account_url,
            username,
            _clean(info.get("channel") or info.get("display_name"), 96),
            _clean(info.get("bio") or info.get("playlist_description"), 500),
            _clean(info.get("avatar_url") or info.get("uploader_avatar"), 1000),
            _clean(info.get("avatar_path"), 500),
            count_value(("channel_follower_count", "follower_count"), "follower_count"),
            count_value(("following_count",), "following_count"),
            count_value(("profile_like_count", "likes_count"), "likes_count"),
            count_value(("video_count",), "video_count"),
            1 if info.get("verified", existing.get("verified", 0)) else 0,
        )
        self.db.execute(
            "INSERT INTO tiktok_profiles (account_url, username, display_name, "
            "bio, avatar_url, avatar_path, follower_count, following_count, "
            "likes_count, video_count, verified) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(account_url) DO UPDATE SET "
            "username=CASE WHEN excluded.username<>'' THEN excluded.username ELSE username END, "
            "display_name=CASE WHEN excluded.display_name<>'' THEN excluded.display_name ELSE display_name END, "
            "bio=CASE WHEN excluded.bio<>'' THEN excluded.bio ELSE bio END, "
            "avatar_url=CASE WHEN excluded.avatar_url<>'' THEN excluded.avatar_url ELSE avatar_url END, "
            "avatar_path=CASE WHEN excluded.avatar_path<>'' THEN excluded.avatar_path ELSE avatar_path END, "
            "follower_count=excluded.follower_count, following_count=excluded.following_count, "
            "likes_count=excluded.likes_count, video_count=excluded.video_count, "
            "verified=excluded.verified, "
            "date_modified=datetime('now')",
            values,
        )
        row = self.db.fetchone(
            "SELECT * FROM tiktok_profiles WHERE account_url=?", (account_url,)
        )
        return dict(row)

    def _ensure_profile_counts(self, profile):
        """Backfill authoritative account stats when video metadata is sparse."""
        if not profile or any(
            int(profile.get(key) or 0)
            for key in (
                "follower_count", "following_count", "likes_count", "video_count"
            )
        ):
            return profile
        account_url = profile.get("account_url") or ""
        if not account_url:
            return profile
        try:
            refreshed = self.refresh_account_profile(account_url)
        except (OSError, ValueError, subprocess.SubprocessError):
            return profile
        return refreshed.get("profile") or profile

    def add_account_sync(self, account_url, sync_mode="rolling", account_key=""):
        account_url = _tiktok_url(account_url, account=True)
        sync_mode = "archive" if sync_mode == "archive" else "rolling"
        name = "@" + account_url.rsplit("/@", 1)[-1]
        if self.db.fetchone(
            "SELECT 1 FROM tiktok_profiles WHERE account_url=?", (account_url,)
        ) is None:
            self._upsert_profile({"uploader": name}, account_url)
        self.db.execute(
            "INSERT INTO tiktok_account_syncs "
            "(account_url, account_name, keep_count, sync_mode, account_key) "
            "VALUES (?, ?, ?, ?, ?) "
            "ON CONFLICT(account_url) DO UPDATE SET "
            "account_name=excluded.account_name, keep_count=excluded.keep_count, "
            "sync_mode=excluded.sync_mode, account_key=CASE WHEN "
            "excluded.account_key<>'' THEN excluded.account_key ELSE account_key END, "
            "date_modified=datetime('now')",
            (
                account_url,
                name,
                0 if sync_mode == "archive" else 10,
                sync_mode,
                _clean(account_key, 1000),
            ),
        )
        row = self.db.fetchone(
            "SELECT * FROM tiktok_account_syncs WHERE account_url = ?",
            (account_url,),
        )
        return dict(row)

    @staticmethod
    def _archive_source(account_url, account_key=""):
        key = str(account_key or "").strip()
        return f"tiktokuser:{key}" if key else account_url

    def _register_downloaded(
        self, media, info, thumbnail, account_url, source_type="following"
    ):
        creator = info.get("uploader") or info.get("creator") or "TikTok"
        if creator and not str(creator).startswith("@"):
            creator = "@" + str(creator)
        profile = self._upsert_profile(info, account_url)
        profile = self._ensure_profile_counts(profile)
        return self.add_video(
            media,
            remote_id=info.get("id"),
            title=info.get("title") or info.get("description") or "TikTok",
            creator=creator,
            description=info.get("description") or info.get("title") or "",
            duration_ms=int(float(info.get("duration") or 0) * 1000),
            upload_date=_upload_date(info.get("upload_date")),
            like_count=int(info.get("like_count") or 0),
            comment_count=int(info.get("comment_count") or 0),
            pin_order=int(info.get("pin_order") or 0),
            source_url=info.get("webpage_url") or "",
            source_type=("archive" if source_type == "archive" else "following"),
            account_url=(profile or {}).get("account_url") or account_url,
            thumbnail_path=str(thumbnail) if thumbnail.is_file() else "",
        )

    def archive_storage_estimate(self, account_url):
        """Return resumable archive counts and a conservative local estimate."""
        account_url = _tiktok_url(account_url, account=True)
        username = account_url.rsplit("/@", 1)[-1].lower()
        managed = Path.home() / ".rockpod" / "tiktok" / "following" / username
        media = [
            path for path in managed.glob("*")
            if path.is_file() and path.suffix.lower() in TIKTOK_VIDEO_SUFFIXES
        ] if managed.is_dir() else []
        downloaded_bytes = sum(path.stat().st_size for path in media)
        profile = self.db.fetchone(
            "SELECT video_count FROM tiktok_profiles WHERE lower(username)=?",
            (username,),
        )
        total = int(profile["video_count"] or 0) if profile else 0
        archived = len(media)
        remaining = max(0, total - archived)
        average = downloaded_bytes // archived if archived else 5 * 1024 * 1024
        estimated_remaining = remaining * average
        free = shutil.disk_usage(managed.parent if managed.parent.exists() else Path.home()).free
        return {
            "archived": archived,
            "total": total,
            "remaining": remaining,
            "downloaded_bytes": downloaded_bytes,
            "estimated_remaining_bytes": estimated_remaining,
            "free_bytes": free,
        }

    def prepare_account_archive(
        self, account_url, account_key="", progress_callback=None
    ):
        """Download and register every currently public post without pruning."""
        supplied_url = str(account_url or "").strip()
        video_hint = supplied_url if re.search(r"/video/\d+", supplied_url) else ""
        account_url = _tiktok_url(supplied_url, account=True)
        progress = progress_callback or (lambda _message: None)
        username = account_url.rsplit("/@", 1)[-1].lower()
        managed = Path.home() / ".rockpod" / "tiktok" / "following" / username
        if not account_key:
            progress("[archive] Resolving account and current statistics…")
            profile_info = {}
            profile_path = managed / ".profile.json"
            if profile_path.is_file():
                try:
                    profile_info = json.loads(
                        profile_path.read_text(encoding="utf-8")
                    )
                    account_key = self._archive_account_key(profile_info)
                    if account_key:
                        progress("[archive] Reusing the saved stable account key…")
                except (OSError, ValueError, json.JSONDecodeError):
                    profile_info = {}
            if not account_key and video_hint:
                progress("[archive] Recovering the account key from that video…")
                profile_info = self._account_profile_from_video(
                    video_hint, account_url
                )
                account_key = self._archive_account_key(profile_info)
            if not account_key:
                try:
                    self._account_uploads(account_url, 1)
                    profile_info = self._account_profiles.pop(account_url, {})
                except (OSError, ValueError, subprocess.SubprocessError):
                    # Some valid profiles no longer expose the secondary user ID
                    # through TikTokUserIE. TikTok's own rendered profile payload
                    # still provides that ID and authoritative account stats.
                    progress("[archive] Trying TikTok's public profile metadata…")
                    try:
                        profile_info = self._account_profile_from_web(account_url)
                    except (OSError, ValueError):
                        progress("[archive] Reading TikTok's official profile embed…")
                        try:
                            profile_info = self._account_profile_from_embed(account_url)
                        except (OSError, ValueError, json.JSONDecodeError) as exc:
                            raise ValueError(
                                "TikTok hid this profile's account key. Paste any "
                                "public video URL from the same creator into the "
                                "Archive field once; RockPod will save and reuse it."
                            ) from exc
                    self._account_profiles.pop(account_url, None)
                account_key = self._archive_account_key(profile_info)
            self._upsert_profile(profile_info, account_url)
        account = self.add_account_sync(account_url, "archive", account_key)
        managed.mkdir(parents=True, exist_ok=True)
        yt_dlp = self._yt_dlp_binary()
        bundled_yt_dlp = self.repo_root / "rockpod" / ".venv" / "bin" / "yt-dlp"
        if yt_dlp == "yt-dlp" and bundled_yt_dlp.is_file():
            yt_dlp = str(bundled_yt_dlp)
        archive_script = self.repo_root / "rockpod" / "scripts" / "tiktok_profile_archive.py"
        python = bundled_yt_dlp.parent / "python"
        if account_key and archive_script.is_file() and python.is_file():
            command = [
                str(python), str(archive_script),
                "--sec-uid", str(account_key),
                "--username", username,
                "--output", str(managed),
            ]
        else:
            command = [
                yt_dlp, "--ignore-errors", "--continue", "--no-overwrites",
                "--extractor-args",
                "tiktok:app_info=/musical_ly/35.1.3/2023501030/1233",
                "-f", "best[height<=720]/best", "--write-info-json",
                "--write-thumbnail", "--convert-thumbnails", "jpg",
                "-o", str(managed / "%(id)s.%(ext)s"),
                self._archive_source(account_url, account_key),
            ]
        progress(f"[archive] Starting resumable archive for @{username}…")
        if progress_callback is None:
            result = subprocess.run(command, check=False, timeout=24 * 60 * 60)
        else:
            process = subprocess.Popen(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1,
            )
            assert process.stdout is not None
            for line in process.stdout:
                progress_callback(line.rstrip())
            result = SimpleNamespace(returncode=process.wait())
        profile_path = managed / ".profile.json"
        if profile_path.is_file():
            try:
                with profile_path.open("r", encoding="utf-8") as handle:
                    self._upsert_profile(json.load(handle), account_url)
            except (OSError, json.JSONDecodeError):
                pass
        registered = self.register_account_archive(account_url, managed)
        if registered == 0:
            raise ValueError("The account archive did not download any TikToks")
        self.db.execute(
            "UPDATE tiktok_account_syncs SET account_name=?, keep_count=0, "
            "sync_mode='archive', date_modified=datetime('now') "
            "WHERE account_url=?",
            (account.get("account_name") or f"@{username}", account_url),
        )
        return {"videos": registered, "returncode": result.returncode}

    def register_account_archive(self, account_url, managed=None):
        """Register an already-downloaded full profile without downloading again."""
        account_url = _tiktok_url(account_url, account=True)
        username = account_url.rsplit("/@", 1)[-1].lower()
        managed = Path(
            managed
            or Path.home() / ".rockpod" / "tiktok" / "following" / username
        )
        profile_path = managed / ".profile.json"
        if profile_path.is_file():
            try:
                with profile_path.open("r", encoding="utf-8") as handle:
                    self._upsert_profile(json.load(handle), account_url)
            except (OSError, json.JSONDecodeError):
                pass
        registered = 0
        for info_path in sorted(managed.glob("*.info.json"), reverse=True):
            try:
                with info_path.open("r", encoding="utf-8") as handle:
                    info = json.load(handle)
            except (OSError, json.JSONDecodeError):
                continue
            if str(info.get("vcodec") or "").lower() == "none":
                continue
            media = next(
                (
                    path
                    for path in managed.glob(f"{info_path.name[:-10]}.*")
                    if path.is_file()
                    and path.suffix.lower() in {
                        ".avi", ".m4v", ".mkv", ".mov", ".mp4",
                        ".mpeg", ".mpg", ".webm",
                    }
                ),
                None,
            )
            if media is None:
                continue
            thumbnail = next(
                iter(managed.glob(f"{info_path.name[:-10]}*.jpg")),
                managed / f"{info_path.name[:-10]}.jpg",
            )
            self._register_downloaded(
                media, info, thumbnail, account_url, source_type="archive"
            )
            registered += 1
        return registered

    def remove_account_sync(self, account_url):
        self.db.execute(
            "DELETE FROM tiktok_account_syncs WHERE account_url = ?",
            (account_url,),
        )

    def _download(self, url, managed):
        managed.mkdir(parents=True, exist_ok=True)
        remote_match = re.search(r"/video/(\d+)", str(url))
        remote_id = remote_match.group(1) if remote_match else ""

        def cached_result():
            if not remote_id:
                return None
            info_path = managed / f"{remote_id}.info.json"
            cached_media = next(
                (
                    candidate
                    for candidate in managed.glob(f"{remote_id}.*")
                    if candidate.is_file()
                    and candidate.suffix.lower() in TIKTOK_VIDEO_SUFFIXES
                ),
                None,
            )
            if (
                cached_media is not None
                and info_path.is_file()
                and _streams_verified(
                    cached_media, self.config.get("ffprobe_binary", "ffprobe")
                )
            ):
                try:
                    with info_path.open("r", encoding="utf-8") as handle:
                        info = json.load(handle)
                except (OSError, json.JSONDecodeError):
                    return None
                thumbnail = next(
                    (
                        candidate
                        for candidate in managed.glob(f"{remote_id}.*")
                        if candidate.suffix.lower() in {".jpg", ".jpeg"}
                    ),
                    managed / f"{remote_id}.jpg",
                )
                return cached_media, info, thumbnail
            return None

        cached = cached_result()
        if cached is not None:
            return cached

        command = [
            self._yt_dlp_binary(),
            "--no-playlist",
            "-f",
            "best[height<=720]/best",
            "--write-info-json",
            "--write-thumbnail",
            "--convert-thumbnails",
            "jpg",
            "--print",
            "after_move:filepath",
            "-o",
            str(managed / "%(id)s.%(ext)s"),
            str(url),
        ]
        api_client = [
            "--extractor-args",
            "tiktok:app_info=/musical_ly/35.1.3/2023501030/1233",
        ]
        impersonate = ["--impersonate", "chrome"]

        def run(args):
            return subprocess.run(
                args, check=True, capture_output=True, text=True, timeout=60 * 60
            )

        def official_embed_result():
            if not remote_id:
                raise ValueError("TikTok embed fallback needs a video ID")
            embed_url = f"https://www.tiktok.com/embed/v2/{remote_id}"
            headers = {
                "User-Agent": (
                    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
                    "AppleWebKit/537.36 (KHTML, like Gecko) "
                    "Chrome/133.0.0.0 Safari/537.36"
                ),
                "Accept-Language": "en-US,en;q=0.9",
            }

            def fetch(remote_url):
                request = Request(remote_url, headers=headers)
                with urlopen(request, timeout=60) as response:
                    return response.read()

            def fetch_to(remote_url, destination):
                request = Request(remote_url, headers=headers)
                with urlopen(request, timeout=60) as response:
                    with destination.open("wb") as output:
                        shutil.copyfileobj(response, output, 1024 * 1024)

            embed_html = fetch(embed_url).decode("utf-8", "replace")
            media_url, thumbnail_url, info = _parse_tiktok_embed(
                embed_html, remote_id
            )
            media = managed / f"{remote_id}.mp4"
            media_temp = managed / f".{remote_id}.mp4.download"
            fetch_to(media_url, media_temp)
            if media_temp.stat().st_size < 1024:
                media_temp.unlink(missing_ok=True)
                raise ValueError("TikTok embed returned an empty video")
            if not _has_playable_streams(
                media_temp, self.config.get("ffprobe_binary", "ffprobe")
            ):
                media_temp.unlink(missing_ok=True)
                raise ValueError("TikTok embed returned incomplete audio/video media")
            os.replace(media_temp, media)
            _mark_streams_verified(media)
            info_path = managed / f"{remote_id}.info.json"
            info_path.write_text(
                json.dumps(info, ensure_ascii=False, indent=2), encoding="utf-8"
            )
            thumbnail = managed / f"{remote_id}.jpg"
            if thumbnail_url:
                raw_thumbnail = managed / f".{remote_id}.image.download"
                raw_thumbnail.write_bytes(fetch(thumbnail_url))
                with Image.open(raw_thumbnail) as image:
                    image.convert("RGB").save(thumbnail, "JPEG", quality=92)
                raw_thumbnail.unlink(missing_ok=True)
            return media, info, thumbnail

        # TikTok's extractor currently labels several video-only CDN variants
        # as AAC. Prefer the official progressive embed URL, whose container
        # carries both H.264 video and AAC audio, and verify the bytes before
        # replacing any cached file. Keep yt-dlp as the compatibility fallback.
        try:
            return official_embed_result()
        except (OSError, ValueError, json.JSONDecodeError) as embed_error:
            last_error = embed_error

        attempts = [
            command,
            command[:1] + api_client + command[1:],
            command[:1] + impersonate + command[1:],
        ]
        firefox = Path.home() / ".mozilla" / "firefox"
        if any(firefox.glob("*/cookies.sqlite")):
            attempts.append(
                command[:1]
                + ["--cookies-from-browser", "firefox"]
                + impersonate
                + api_client
                + command[1:]
            )
        for attempt in attempts:
            try:
                result = run(attempt)
                break
            except (OSError, subprocess.SubprocessError) as error:
                last_error = error
                # yt-dlp can finish media and metadata, then return an error
                # when a TikTok webpage or thumbnail follow-up is blocked.
                cached = cached_result()
                if cached is not None:
                    return cached
        else:
            try:
                return official_embed_result()
            except (OSError, ValueError, json.JSONDecodeError) as embed_error:
                detail = getattr(last_error, "stderr", "") or str(last_error)
                raise ValueError(
                    f"TikTok download failed after API, browser-impersonated, "
                    f"signed-in, and official-embed attempts: "
                    f"{_clean(detail, 220)}; embed: {_clean(embed_error, 120)}"
                ) from embed_error

        paths = [Path(line.strip()) for line in result.stdout.splitlines() if line.strip()]
        media = next(
            (
                path for path in reversed(paths)
                if path.is_file() and path.suffix.lower() in TIKTOK_VIDEO_SUFFIXES
            ),
            None,
        )
        if media is None:
            raise ValueError("TikTok download finished without a video file")
        if not _has_playable_streams(
            media, self.config.get("ffprobe_binary", "ffprobe")
        ):
            raise ValueError("TikTok download finished without complete audio/video streams")
        _mark_streams_verified(media)
        info_path = media.with_suffix(".info.json")
        if not info_path.is_file():
            candidates = list(managed.glob(f"{media.stem}*.info.json"))
            info_path = candidates[0] if candidates else info_path
        if not info_path.is_file():
            raise ValueError("TikTok download finished without metadata")
        with info_path.open("r", encoding="utf-8") as handle:
            info = json.load(handle)
        if str(info.get("vcodec") or "").lower() == "none":
            raise ValueError("TikTok returned audio only, without a video stream")
        thumbnail = media.with_suffix(".jpg")
        if not thumbnail.is_file():
            candidates = list(managed.glob(f"{media.stem}*.jpg"))
            thumbnail = candidates[0] if candidates else thumbnail
        return media, info, thumbnail

    def import_url(self, url, source_type="manual", account_url=""):
        url = _tiktok_url(url)
        following = source_type == "following"
        managed = Path.home() / ".rockpod" / "tiktok" / (
            "following" if following else "manual"
        )
        media, info, thumbnail = self._download(url, managed)
        if following:
            return self._register_downloaded(media, info, thumbnail, account_url)
        creator = info.get("uploader") or info.get("creator") or "TikTok"
        if creator and not str(creator).startswith("@"):
            creator = "@" + str(creator)
        profile = self._upsert_profile(info, account_url)
        profile = self._ensure_profile_counts(profile)
        return self.add_video(
            media, remote_id=info.get("id"),
            title=info.get("title") or info.get("description") or "TikTok",
            creator=creator,
            description=info.get("description") or info.get("title") or "",
            duration_ms=int(float(info.get("duration") or 0) * 1000),
            upload_date=_upload_date(info.get("upload_date")),
            like_count=int(info.get("like_count") or 0),
            comment_count=int(info.get("comment_count") or 0),
            source_url=url, source_type="manual",
            account_url=(profile or {}).get("account_url") or account_url,
            thumbnail_path=str(thumbnail) if thumbnail.is_file() else "",
        )

    def _account_uploads(self, account_url, limit=10):
        command = [
            self._yt_dlp_binary(),
            "--extractor-args",
            "tiktok:app_info=/musical_ly/35.1.3/2023501030/1233",
            "--flat-playlist",
            "--playlist-end",
            str(limit),
            "--dump-single-json",
            "--no-warnings",
            account_url,
        ]
        try:
            result = subprocess.run(
                command, check=True, capture_output=True, text=True, timeout=180
            )
            payload = json.loads(result.stdout)
        except (OSError, json.JSONDecodeError, subprocess.SubprocessError) as exc:
            detail = getattr(exc, "stderr", "") or str(exc)
            try:
                profile_info = self._account_profile_from_embed(account_url)
                uploads = self._account_embed_uploads.pop(account_url, [])
            except (OSError, ValueError, json.JSONDecodeError) as embed_exc:
                raise ValueError(
                    f"Could not read TikTok account: {_clean(detail, 220)}; "
                    f"official embed: {_clean(embed_exc, 100)}"
                ) from exc
            if not uploads:
                raise ValueError(
                    "TikTok's official profile embed returned no downloadable posts"
                ) from exc
            self._account_profiles[account_url] = profile_info
            return uploads[:limit]
        uploads = []
        first_entry = next((entry for entry in payload.get("entries") or [] if entry), {})
        profile_info = dict(payload)
        for key in (
            "uploader", "uploader_url", "channel", "channel_follower_count",
            "following_count", "profile_like_count", "avatar_url",
            "uploader_avatar", "verified",
        ):
            if first_entry.get(key) is not None:
                profile_info[key] = first_entry.get(key)
        if not profile_info.get("uploader"):
            profile_info["uploader"] = account_url.rsplit("/@", 1)[-1]
        self._account_profiles[account_url] = profile_info
        for entry in payload.get("entries") or []:
            entry = entry or {}
            video_id = str(entry.get("id") or "")
            url = entry.get("webpage_url") or entry.get("url") or ""
            if video_id.isdigit():
                url = f"{account_url}/video/{video_id}"
            if url:
                uploads.append(str(url))
        if not uploads:
            raise ValueError("The account did not return downloadable TikToks")
        return uploads[:limit]

    def refresh_account_profile(self, account_url):
        """Fetch profile metadata without blocking on ten media downloads."""
        uploads = self._account_uploads(account_url, 1)
        profile_info = self._account_profiles.pop(account_url, {})
        sec_uid = str(profile_info.get("id") or "")
        script = self.repo_root / "rockpod" / "scripts" / "tiktok_profile_archive.py"
        python = self.repo_root / "rockpod" / ".venv" / "bin" / "python"
        username = account_url.rsplit("/@", 1)[-1].lower()
        profile_dir = Path.home() / ".rockpod" / "tiktok" / "profiles" / username
        if sec_uid.startswith("MS4wLjABAAAA") and script.is_file() and python.is_file():
            profile_dir.mkdir(parents=True, exist_ok=True)
            subprocess.run(
                [str(python), str(script), "--sec-uid", sec_uid,
                 "--username", username, "--output", str(profile_dir),
                 "--profile-only"],
                check=False, timeout=180,
            )
            profile_path = profile_dir / ".profile.json"
            if profile_path.is_file():
                try:
                    with profile_path.open("r", encoding="utf-8") as handle:
                        profile_info.update(json.load(handle))
                except (OSError, json.JSONDecodeError):
                    pass
        profile = self._upsert_profile(profile_info, account_url)
        return {"profile": profile, "latest_url": uploads[0]}

    def sync_account_uploads(self, progress_callback=None):
        report = {
            "accounts": 0, "videos_added": 0, "videos_removed": 0,
            "errors": 0,
        }
        accounts = [
            account for account in self.list_account_syncs()
            if account.get("sync_mode") != "archive"
        ]
        for account_index, account in enumerate(accounts, 1):
            account_url = account["account_url"]
            account_name = (
                account.get("account_name") or account_url.rsplit("/@", 1)[-1]
            )
            if progress_callback is not None:
                progress_callback(
                    0,
                    0,
                    f"Checking Following account {account_index}/{len(accounts)}: "
                    f"@{str(account_name).lstrip('@')}…",
                )
            try:
                uploads = self._account_uploads(account_url, 20)
            except (OSError, ValueError, subprocess.SubprocessError):
                report["errors"] += 1
                continue
            profile = self._upsert_profile(
                self._account_profiles.pop(account_url, {}), account_url
            )
            self._ensure_profile_counts(profile)
            ids = []
            account_name = account.get("account_name") or ""
            for upload_index, url in enumerate(uploads, 1):
                if len(ids) >= 10:
                    break
                if progress_callback is not None:
                    progress_callback(
                        min(len(ids), 9),
                        10,
                        f"Downloading newest posts from "
                        f"@{str(account_name).lstrip('@')}: "
                        f"{min(upload_index, 10)}/10",
                    )
                try:
                    row = self.import_url(url, "following", account_url)
                except (OSError, ValueError, subprocess.SubprocessError):
                    report["errors"] += 1
                    continue
                ids.append(row["id"])
                if not account_name:
                    account_name = row.get("creator") or ""
            if not ids:
                report["errors"] += 1
                continue
            placeholders = ", ".join("?" for _ in ids)
            stale = self.db.fetchall(
                "SELECT id FROM tiktok_videos WHERE source_type='following' "
                f"AND account_url=? AND id NOT IN ({placeholders})",
                (account_url, *ids),
            )
            for row in stale:
                self.remove_video(row["id"])
            self.db.execute(
                "UPDATE tiktok_account_syncs SET account_name=?, keep_count=10, "
                "date_modified=datetime('now') WHERE account_url=?",
                (_clean(account_name, 96), account_url),
            )
            report["accounts"] += 1
            report["videos_added"] += len(ids)
            report["videos_removed"] += len(stale)
            if progress_callback is not None:
                progress_callback(
                    10,
                    10,
                    f"Following account {account_index}/{len(accounts)} refreshed",
                )
        return report

    @staticmethod
    def _remove_managed_following_files(row):
        root = (Path.home() / ".rockpod" / "tiktok" / "following").resolve()
        source = Path(row.get("source_path") or "")
        try:
            resolved = source.resolve()
            resolved.relative_to(root)
        except (OSError, ValueError):
            return
        for path in root.glob(f"{source.stem}*"):
            if path.is_file():
                path.unlink()

    def _render_thumbnail(self, row, target, staging):
        source_image = row.get("thumbnail_path") or ""
        extracted = ""
        if not os.path.isfile(source_image):
            extracted = os.path.join(staging, f"{row['id']}-frame.jpg")
            subprocess.run(
                [
                    self.config.get("ffmpeg_binary", "ffmpeg"),
                    "-hide_banner",
                    "-loglevel",
                    "error",
                    "-y",
                    "-ss",
                    "1",
                    "-i",
                    row["source_path"],
                    "-frames:v",
                    "1",
                    extracted,
                ],
                check=False,
                timeout=120,
            )
            source_image = extracted
        canvas = Image.new("RGB", (96, 72), "black")
        if os.path.isfile(source_image):
            with Image.open(source_image) as image:
                fitted = ImageOps.contain(
                    image.convert("RGB"), (96, 72), Image.Resampling.LANCZOS
                )
                canvas.paste(
                    fitted,
                    ((96 - fitted.width) // 2, (72 - fitted.height) // 2),
                )
        generated = os.path.join(staging, f"{row['id']}-thumb.bmp")
        canvas.save(generated, "BMP")
        changed = _copy_if_changed(generated, target)
        os.unlink(generated)
        if extracted and os.path.isfile(extracted):
            os.unlink(extracted)
        return changed

    def _render_menu_preview(self, row, target, staging):
        """Render a source-video frame at the exact 160x240 menu-pane aspect."""
        fallback_image = row.get("thumbnail_path") or ""
        extracted = os.path.join(staging, f"{row['id']}-preview-frame.jpg")
        source_image = ""
        if os.path.isfile(row.get("source_path") or ""):
            subprocess.run(
                [
                    self.config.get("ffmpeg_binary", "ffmpeg"),
                    "-hide_banner", "-loglevel", "error", "-y",
                    "-ss", "1", "-i", row["source_path"],
                    "-frames:v", "1", extracted,
                ],
                check=False,
                timeout=120,
            )
            if os.path.isfile(extracted):
                source_image = extracted
        if not source_image and os.path.isfile(fallback_image):
            source_image = fallback_image
        if os.path.isfile(source_image):
            with Image.open(source_image) as image:
                canvas = ImageOps.fit(
                    image.convert("RGB"), (160, 240), Image.Resampling.LANCZOS
                )
        else:
            canvas = Image.new("RGB", (160, 240), "black")
        generated = os.path.join(staging, f"{row['id']}-preview.bmp")
        canvas.save(generated, "BMP")
        changed = _copy_if_changed(generated, target)
        os.unlink(generated)
        if os.path.isfile(extracted):
            os.unlink(extracted)
        return changed

    def _sync_media(
        self, row, mount, staging, sync_index, video_profile,
        device_target="", video_quality="tv",
    ):
        video_extension = app_video_extension(video_profile)
        destination = resolve_under_root(
            mount, f"{TIKTOK_MEDIA_ROOT}/{row['id']}{video_extension}"
        )
        # Include the output contract so existing 20 fps device copies are
        # rebuilt after an encoder/profile upgrade even when source is unchanged.
        signature = app_video_signature(
            video_profile, _source_signature(row["source_path"]),
            TIKTOK_MEDIA_PIPELINE_VERSION + (":space" if video_quality == "space" else ""),
            device_target,
        )
        if sync_index.current_or_seed(
            "tiktok", row["id"], "video-media-v2", signature,
            [destination],
            trust_existing=row.get("last_synced_source_hash") == signature,
        ):
            return False, signature
        staged = os.path.join(staging, f"{row['id']}{video_extension}")
        stage_app_video(
            row["source_path"], staged,
            config=self.config,
            profile=video_profile,
            cache_namespace="tiktok-video",
            device_key=row["id"],
            device_target=device_target,
            title=row.get("title") or row["id"],
            artist=row.get("creator") or "",
            duration=float(row.get("duration_ms") or 0) / 1000.0,
            mpeg_filter=(
                "scale=136:240:force_original_aspect_ratio=decrease,"
                "pad=320:240:(ow-iw)/2:(oh-ih)/2:black,fps=30"
            ),
            quality=video_quality,
        )
        os.replace(staged, destination)
        remove_alternate_video(destination)
        sync_index.mark(
            "tiktok", row["id"], "video-media-v2", signature, [destination]
        )
        return True, signature

    def _ensure_sync_source(self, row, video_profile, device_target="", video_quality="tv"):
        """Repair a stale/broken URL import before it reaches the transcoder."""
        source = Path(row["source_path"])
        if source.suffix.lower() in {".mpg", ".mpeg", ".mpe"}:
            return row
        ffprobe = self.config.get("ffprobe_binary", "ffprobe")
        proven_signature = app_video_signature(
            video_profile, _source_signature(source),
            TIKTOK_MEDIA_PIPELINE_VERSION + (":space" if video_quality == "space" else ""),
            device_target,
        )
        if row.get("last_synced_source_hash") == proven_signature:
            # A previous ffmpeg run used mandatory 0:v:0 and 0:a:0 maps, so a
            # completed transcode is already authoritative stream validation.
            _mark_streams_verified(source)
            return row
        if _streams_verified(source, ffprobe):
            return row
        source_url = str(row.get("source_url") or "")
        if not source_url or "/video/" not in source_url:
            raise ValueError("Source media is missing a real audio or video stream")
        unavailable = _unavailable_marker(source)
        try:
            if unavailable.read_text(encoding="ascii") == _source_signature(source):
                raise ValueError("TikTok source is unavailable for automatic repair")
        except OSError:
            pass
        try:
            repaired, _info, _thumbnail = self._download(source_url, source.parent)
        except (OSError, ValueError, subprocess.SubprocessError):
            unavailable.write_text(_source_signature(source), encoding="ascii")
            raise
        if not _streams_verified(repaired, ffprobe):
            raise ValueError("TikTok repair did not return complete audio/video media")
        if repaired != source:
            self.db.execute(
                "UPDATE tiktok_videos SET source_path=?, source_hash=?, "
                "date_modified=datetime('now') WHERE id=?",
                (str(repaired), _source_signature(repaired), row["id"]),
            )
        repaired_row = self.get_video(row["id"])
        return repaired_row or {**row, "source_path": str(repaired)}

    @staticmethod
    def _remove_stale(directory, expected, suffix):
        removed = 0
        for name in os.listdir(directory):
            stem, ext = os.path.splitext(name)
            if stem.startswith("tt_") and ext.lower() == suffix and stem not in expected:
                os.unlink(os.path.join(directory, name))
                removed += 1
                sidecar = os.path.join(directory, name + ".source")
                if os.path.isfile(sidecar):
                    os.unlink(sidecar)
                    removed += 1
        return removed

    def sync(
        self, mount_path, device=None, refresh_accounts=True,
        progress_callback=None, video_profile=None, video_ids=None,
        video_quality="tv",
    ):
        progress = progress_callback or (lambda _done, _total, _message: None)
        progress(0, 0, "Checking the connected iPod…")
        mount = validate_device_root(mount_path)
        device_target = (
            str(getattr(device, "rockbox_target", "") or "").strip().lower()
            or device_video_target(mount)
        )
        video_profile = app_video_profile(
            self.config, video_profile, source_app="tiktok"
        )
        video_extension = app_video_extension(video_profile)
        if video_quality not in {"tv", "space"}:
            raise ValueError("Choose Standard or Space saver video quality")
        if video_ids is not None:
            refresh_accounts = False
        if refresh_accounts and self.list_account_syncs():
            progress(0, 0, "Refreshing the newest Following videos…")
            following = self.sync_account_uploads(progress_callback=progress)
        else:
            following = {"accounts": 0, "videos_added": 0, "videos_removed": 0}
        progress(0, 0, "Reading the TikTok library…")
        videos = self.list_videos()
        retained_library = {}
        retained_feed = {}
        if video_ids is not None:
            requested = {str(value) for value in video_ids}
            known = {str(row["id"]) for row in videos}
            if not requested or requested - known:
                raise ValueError("Select one or more available TikToks to sync")
            existing = present_manifest_rows(
                mount, f"{TIKTOK_DEVICE_ROOT}/library.tsv", ("path",)
            )
            feed_rows = present_manifest_rows(
                mount, TIKTOK_FEED_PATH, ("path",)
            )
            retained_ids = existing.keys() & feed_rows.keys()
            retained_library = {item_id: existing[item_id] for item_id in retained_ids}
            retained_feed = {item_id: feed_rows[item_id] for item_id in retained_ids}
            videos = [row for row in videos if str(row["id"]) in requested]
        missing_sources = [
            row for row in videos if not os.path.isfile(row["source_path"])
        ]
        videos = [row for row in videos if os.path.isfile(row["source_path"])]

        root = resolve_under_root(mount, TIKTOK_DEVICE_ROOT)
        media_root = resolve_under_root(mount, TIKTOK_MEDIA_ROOT)
        thumb_root = resolve_under_root(mount, TIKTOK_THUMB_ROOT)
        avatar_root = resolve_under_root(mount, TIKTOK_AVATAR_ROOT)
        preview_root = resolve_under_root(mount, TIKTOK_PREVIEW_ROOT)
        feed_path = resolve_under_root(mount, TIKTOK_FEED_PATH)
        staging = resolve_under_root(mount, f"{TIKTOK_DEVICE_ROOT}/.staging")
        for directory in (
            root, media_root, thumb_root, avatar_root, preview_root,
            os.path.dirname(feed_path), staging,
        ):
            os.makedirs(directory, exist_ok=True)
        sync_index = DeviceSyncIndex(self.config, mount)

        # Leave enough room for Rockbox state and avoid a half-written feed.
        # MPEG output is capped by duration/target bitrate, so this estimate is
        # much closer than summing potentially huge source files.
        projected = 0
        for row in videos:
            destination = resolve_under_root(
                mount, f"{TIKTOK_MEDIA_ROOT}/{row['id']}{video_extension}"
            )
            media_signature = app_video_signature(
                video_profile, _source_signature(row["source_path"]),
                TIKTOK_MEDIA_PIPELINE_VERSION + (":space" if video_quality == "space" else ""),
                device_target,
            )
            if not sync_index.current_or_seed(
                "tiktok", row["id"], "video-media-v2", media_signature,
                [destination],
                trust_existing=(
                    row.get("last_synced_source_hash") == media_signature
                ),
            ):
                projected += max(
                    2 * 1024 * 1024,
                    int(row.get("duration_ms") or 0)
                    * (80 if video_quality == "space" else 220),
                )
        free = shutil.disk_usage(mount).free
        reserve = 64 * 1024 * 1024
        if projected + reserve > free:
            needed_mib = (projected + reserve + (1 << 20) - 1) // (1 << 20)
            free_mib = free // (1 << 20)
            raise ValueError(
                f"Not enough free space for TikTok sync: need about "
                f"{needed_mib} MiB including reserve, {free_mib} MiB available"
            )

        report = {
            "videos": len(videos),
            "media_updated": 0,
            "media_unchanged": 0,
            "thumbnails_updated": 0,
            "thumbnails_unchanged": 0,
            "previews_updated": 0,
            "previews_unchanged": 0,
            "stale_files_removed": 0,
            "assets_updated": 0,
            "state_files_updated": 0,
            "videos_skipped": len(missing_sources),
            "following": following,
        }
        lines = [
            "id\ttitle\tpath\tsource_type\tcreator\tdescription\tthumbnail"
            "\tlikes\tcomments\tpin_order"
        ]
        library = [
            "id\ttitle\tcreator\tsource_type\taccount_url\tupload_date\tpath"
            "\tthumbnail\tmenu_preview"
        ]
        profiles = [
            "account_url\tusername\tdisplay_name\tbio\tavatar\tfollowers"
            "\tfollowing\tlikes\tvideos\tfollowed\tverified"
        ]
        synced_ids = set()
        total_videos = len(videos)
        progress(0, total_videos, f"Preparing {total_videos} TikTok clips…")
        for video_index, row in enumerate(videos, 1):
            title = _clean(row.get("title") or row.get("creator") or "TikTok", 48)
            progress(
                video_index - 1,
                total_videos,
                f"Checking clip {video_index}/{total_videos}: {title}",
            )
            try:
                row = self._ensure_sync_source(
                    row, video_profile, device_target, video_quality
                )
                progress(
                    video_index - 1,
                    total_videos,
                    f"Syncing clip {video_index}/{total_videos}: {title}",
                )
                updated, signature = self._sync_media(
                    row, mount, staging, sync_index, video_profile,
                    device_target, video_quality,
                )
            except (OSError, ValueError, subprocess.CalledProcessError):
                report["videos_skipped"] += 1
                progress(
                    video_index,
                    total_videos,
                    f"Skipped unavailable clip {video_index}/{total_videos}",
                )
                continue
            report["media_updated" if updated else "media_unchanged"] += 1
            thumb_path = resolve_under_root(
                mount, f"{TIKTOK_THUMB_ROOT}/{row['id']}.bmp"
            )
            thumbnail_signature = _thumbnail_signature(row)
            thumbnail_current = sync_index.current_or_seed(
                "tiktok", row["id"], "thumbnail-v1", thumbnail_signature,
                [thumb_path],
                trust_existing=(
                    row.get("last_synced_thumbnail_hash") == thumbnail_signature
                    or (
                        not row.get("last_synced_thumbnail_hash")
                        and row.get("last_synced_source_hash") == signature
                    )
                ),
            )
            if thumbnail_current:
                # Schema-v21 libraries already proved the media/device pair
                # current. Seed the new thumbnail signature without rendering
                # thousands of identical BMPs during the v22 migration.
                report["thumbnails_unchanged"] += 1
            else:
                if self._render_thumbnail(row, thumb_path, staging):
                    report["thumbnails_updated"] += 1
                else:
                    report["thumbnails_unchanged"] += 1
                sync_index.mark(
                    "tiktok", row["id"], "thumbnail-v1",
                    thumbnail_signature, [thumb_path],
                )
            self.db.execute(
                "UPDATE tiktok_videos SET last_synced_source_hash=?, "
                "last_synced_thumbnail_hash=? WHERE id=?",
                (signature, thumbnail_signature, row["id"]),
            )
            device_video = (
                f"/{TIKTOK_MEDIA_ROOT}/{row['id']}{video_extension}"
            )
            player_metadata = os.path.join(
                media_root, f"{row['id']}.ttm"
            )
            report["state_files_updated"] += _write_text_if_changed(
                player_metadata,
                "\n".join([
                    f"id={_clean(row['id'], 96)}",
                    f"title={_clean(row.get('title'), 80)}",
                    f"creator={_clean(row.get('creator'), 48)}",
                    f"description={_clean(row.get('description'), 180)}",
                    f"likes={int(row.get('like_count') or 0)}",
                    f"comments={int(row.get('comment_count') or 0)}",
                    f"source_type={_clean(row.get('source_type'), 24)}",
                ]) + "\n",
            )
            device_thumb = f"/{TIKTOK_THUMB_ROOT}/{row['id']}.bmp"
            device_preview = ""
            if video_index <= TIKTOK_MENU_PREVIEW_LIMIT:
                preview_path = os.path.join(preview_root, f"{row['id']}.bmp")
                preview_signature = (
                    f"{TIKTOK_MENU_PREVIEW_VERSION}:"
                    f"{_source_signature(row['source_path'])}:"
                    f"{thumbnail_signature}"
                )
                preview_signature_path = preview_path + ".source"
                if sync_index.current_or_seed(
                    "tiktok", row["id"], "menu-preview-v2",
                    preview_signature, [preview_path],
                    legacy_marker=preview_signature_path,
                    legacy_signature=preview_signature,
                ):
                    report["previews_unchanged"] += 1
                else:
                    self._render_menu_preview(row, preview_path, staging)
                    atomic_write_text(preview_signature_path, preview_signature)
                    sync_index.mark(
                        "tiktok", row["id"], "menu-preview-v2",
                        preview_signature, [preview_path],
                    )
                    report["previews_updated"] += 1
                device_preview = f"/{TIKTOK_PREVIEW_ROOT}/{row['id']}.bmp"
            lines.append(
                "\t".join(
                    [
                        row["id"],
                        _clean(row["title"], 80),
                        device_video,
                        row["source_type"],
                        _clean(row["creator"], 48),
                        _clean(row["description"], 180),
                        device_thumb,
                        str(int(row["like_count"] or 0)),
                        str(int(row["comment_count"] or 0)),
                        str(int(row.get("pin_order") or 0)),
                    ]
                )
            )
            library.append(
                "\t".join(
                    [
                        row["id"],
                        _clean(row["title"], 80),
                        _clean(row["creator"], 48),
                        row["source_type"],
                        _clean(row["account_url"], 300),
                        _clean(row["upload_date"], 20),
                        device_video,
                        device_thumb,
                        device_preview,
                    ]
                )
            )
            synced_ids.add(row["id"])
            result = "Synced" if updated else "Already current"
            progress(
                video_index,
                total_videos,
                f"{result} · clip {video_index}/{total_videos}: {title}",
            )

        for item_id in sorted(retained_library):
            if item_id in synced_ids:
                continue
            feed_row = retained_feed[item_id]
            library_row = retained_library[item_id]
            lines.append("\t".join(str(feed_row.get(key) or "") for key in (
                "id", "title", "path", "source_type", "creator", "description",
                "thumbnail", "likes", "comments", "pin_order",
            )))
            library.append("\t".join(str(library_row.get(key) or "") for key in (
                "id", "title", "creator", "source_type", "account_url",
                "upload_date", "path", "thumbnail", "menu_preview",
            )))
            synced_ids.add(item_id)
        expected = synced_ids
        progress(total_videos, total_videos, "Writing feeds and profile data…")
        report["videos"] = len(synced_ids)
        legacy_reconcile = sync_index.needs_legacy_reconcile("tiktok")
        if legacy_reconcile:
            report["stale_files_removed"] += self._remove_stale(
                media_root, expected, ".mpg"
            )
            report["stale_files_removed"] += self._remove_stale(
                media_root, expected, ".m4v"
            )
            report["stale_files_removed"] += self._remove_stale(
                media_root, expected, ".ttm"
            )
            report["stale_files_removed"] += self._remove_stale(
                thumb_root, expected, ".bmp"
            )
            report["stale_files_removed"] += self._remove_stale(
                preview_root, expected, ".bmp"
            )
            sync_index.mark_legacy_reconciled("tiktok")
        report["state_files_updated"] += _write_text_if_changed(
            feed_path, "\n".join(lines) + "\n"
        )
        report["state_files_updated"] += _write_text_if_changed(
            os.path.join(root, "library.tsv"), "\n".join(library) + "\n"
        )
        followed_urls = {row["account_url"] for row in self.list_account_syncs()}
        for profile in self.list_profiles():
            avatar_device = ""
            avatar_source = str(profile.get("avatar_path") or "")
            if avatar_source and os.path.isfile(avatar_source):
                avatar_name = "profile_" + hashlib.sha1(
                    str(profile["account_url"]).encode("utf-8")
                ).hexdigest()[:16] + ".bmp"
                avatar_target = os.path.join(avatar_root, avatar_name)
                avatar_signature = _source_signature(avatar_source) + ":48-v1"
                signature_path = avatar_target + ".source"
                profile_id = "profile:" + hashlib.sha1(
                    str(profile["account_url"]).encode("utf-8")
                ).hexdigest()[:16]
                synced_ids.add(profile_id)
                if not sync_index.current_or_seed(
                    "tiktok", profile_id, "avatar-v1", avatar_signature,
                    [avatar_target], legacy_marker=signature_path,
                    legacy_signature=avatar_signature,
                ):
                    with Image.open(avatar_source) as image:
                        ImageOps.fit(
                            image.convert("RGB"), (48, 48),
                            method=Image.Resampling.LANCZOS,
                        ).save(avatar_target, "BMP")
                    atomic_write_text(signature_path, avatar_signature)
                    sync_index.mark(
                        "tiktok", profile_id, "avatar-v1",
                        avatar_signature, [avatar_target],
                    )
                    report["assets_updated"] += 1
                avatar_device = f"/{TIKTOK_AVATAR_ROOT}/{avatar_name}"
            profiles.append(
                "\t".join(
                    [
                        _clean(profile["account_url"], 300),
                        _clean(profile["username"], 96),
                        _clean(profile["display_name"], 96),
                        _clean(profile["bio"], 300),
                        avatar_device,
                        str(int(profile["follower_count"] or 0)),
                        str(int(profile["following_count"] or 0)),
                        str(int(profile["likes_count"] or 0)),
                        str(int(profile["video_count"] or 0)),
                        "1" if profile["account_url"] in followed_urls else "0",
                        "1" if profile.get("verified") else "0",
                    ]
                )
            )
        report["state_files_updated"] += _write_text_if_changed(
            os.path.join(root, "profiles.tsv"), "\n".join(profiles) + "\n"
        )

        asset_source = self.repo_root / "assets" / "ipodjs" / "rockbox" / "tiktok"
        asset_target = resolve_under_root(mount, TIKTOK_ASSET_ROOT)
        os.makedirs(asset_target, exist_ok=True)
        for name in (
            "tiktok-header-official.320x40.bmp",
            "tiktok-heart-liked.32x32.bmp",
            "tiktok-heart-outline.32x32.bmp",
            "tiktok-check-official.24x24.bmp",
            "tiktok-verified-official.14x14.bmp",
        ):
            if _copy_if_changed(str(asset_source / name), os.path.join(asset_target, name)):
                report["assets_updated"] += 1

        if device is not None:
            from services.rockbox_device import set_rockbox_tiktok_main_menu

            set_rockbox_tiktok_main_menu(device, True)
        stale_ids = sync_index.prune("tiktok", synced_ids)
        if not legacy_reconcile:
            for stale_id in stale_ids:
                if stale_id.startswith("profile:"):
                    continue
                for path in (
                    os.path.join(media_root, stale_id + ".mpg"),
                    os.path.join(media_root, stale_id + ".mpg.source"),
                    os.path.join(media_root, stale_id + ".m4v"),
                    os.path.join(media_root, stale_id + ".m4v.source"),
                    os.path.join(media_root, stale_id + ".ttm"),
                    os.path.join(thumb_root, stale_id + ".bmp"),
                    os.path.join(preview_root, stale_id + ".bmp"),
                    os.path.join(preview_root, stale_id + ".bmp.source"),
                ):
                    try:
                        os.unlink(path)
                        report["stale_files_removed"] += 1
                    except FileNotFoundError:
                        pass
        sync_index.close()
        progress(total_videos, total_videos, "TikTok sync complete")
        try:
            os.rmdir(staging)
        except OSError:
            pass
        return report
