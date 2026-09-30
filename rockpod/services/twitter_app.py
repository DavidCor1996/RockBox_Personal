"""Import X posts into an external cache and sync an offline Twitter reader."""

from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import tempfile
import unicodedata
from pathlib import Path
from urllib.parse import urlsplit
from urllib.request import Request, urlopen

from PIL import Image, ImageOps

from services.app_video_sync import (
    app_video_extension, app_video_profile, stage_app_video,
)
from services.device_sync_index import DeviceSyncIndex
from services.file_safety import atomic_write_text
from services.path_safety import resolve_under_root, validate_device_root


TWITTER_ROOT = ".rockbox/twitter"
PHOTO_SUFFIXES = {".jpg", ".jpeg", ".png", ".webp", ".bmp"}
VIDEO_SUFFIXES = {".mp4", ".m4v", ".mov", ".webm", ".mkv"}
MAX_POSTS = 500
MAX_MEDIA_PER_POST = 4


def twitter_handle(value):
    """Accept an X/Twitter profile URL, ignoring mobile share parameters."""
    raw = str(value or "").strip()
    if raw.startswith("@"):
        raw = raw[1:]
    if re.fullmatch(r"[A-Za-z0-9_]{1,15}", raw):
        return raw.lower()
    if "://" not in raw:
        raw = "https://" + raw
    parsed = urlsplit(raw)
    if parsed.scheme not in {"http", "https"} or parsed.hostname not in {
        "x.com", "www.x.com", "twitter.com", "www.twitter.com",
        "mobile.twitter.com",
    }:
        raise ValueError("Enter an X/Twitter profile URL or @handle")
    match = re.fullmatch(r"/([A-Za-z0-9_]{1,15})/?", parsed.path)
    if not match or match.group(1).lower() in {"home", "explore", "i", "search"}:
        raise ValueError("Enter a profile URL such as https://x.com/username")
    return match.group(1).lower()


def _ipod_text(value, limit):
    translated = str(value or "").translate(str.maketrans({
        "’": "'", "‘": "'", "“": '"', "”": '"', "–": "-", "—": "-",
        "…": "...", "\t": " ", "\r": " ", "\n": " ",
    }))
    return " ".join(unicodedata.normalize("NFKD", translated)
                    .encode("ascii", "ignore").decode("ascii").split())[:limit]


def _source_signature(path):
    stat = Path(path).stat()
    return f"{stat.st_size}:{stat.st_mtime_ns}"


class _ExternalCacheConfig:
    """Redirect Twitter conversion and sync indexes to the external drive."""

    def __init__(self, config, root):
        self.config = config
        self.root = Path(root)

    def get(self, key, default=None):
        if key == "cache_dir":
            return str(self.root / "work")
        return self.config.get(key, default)


class TwitterAppService:
    """Own the external original-media cache and device-ready Twitter catalog."""

    def __init__(self, config, repo_root):
        self.config = config
        self.repo_root = Path(repo_root)

    @property
    def cache_root(self):
        configured = str(self.config.get("twitter_cache_dir") or "").strip()
        if configured:
            return Path(configured).expanduser().resolve()
        username = os.environ.get("USER") or Path.home().name
        media_root = Path("/run/media") / username
        for mount in sorted(media_root.glob("*")):
            if mount.name.upper() == "IPOD" or not mount.is_mount():
                continue
            if (mount / "RockPod").is_dir():
                return mount / "RockPod" / "twitter"
        raise ValueError("Choose an external Twitter cache folder in RockPod")

    def set_cache_root(self, path):
        root = Path(path).expanduser().resolve()
        if not root.parent.is_dir():
            raise ValueError("The selected external drive is unavailable")
        self.config.set("twitter_cache_dir", str(root))
        self.config.save()

    def _require_root(self):
        root = self.cache_root
        if not root.parent.is_dir():
            raise ValueError("The external Twitter cache drive is unavailable")
        root.mkdir(parents=True, exist_ok=True)
        return root

    @property
    def gallery_dl(self):
        selected = str(self.config.get("gallery_dl_binary") or "").strip()
        return Path(selected).expanduser() if selected else self.repo_root / "rockpod/.venv/bin/gallery-dl"

    def _load(self):
        try:
            payload = json.loads((self.cache_root / "library.json").read_text(encoding="utf-8"))
        except (OSError, ValueError, TypeError):
            return {"accounts": []}
        if not isinstance(payload, dict) or not isinstance(payload.get("accounts"), list):
            return {"accounts": []}
        return payload

    def list_accounts(self):
        return sorted(self._load()["accounts"], key=lambda row: row.get("handle", ""))

    def remove_account(self, handle):
        root = self._require_root()
        payload = self._load()
        payload["accounts"] = [row for row in payload["accounts"]
                               if row.get("handle") != twitter_handle(handle)]
        atomic_write_text(root / "library.json", json.dumps(payload, indent=2, ensure_ascii=False) + "\n")

    @staticmethod
    def _parse_messages(messages, limit):
        if not isinstance(messages, list):
            raise ValueError("The X extractor returned an invalid response")
        posts = {}
        for message in messages:
            if not isinstance(message, list) or not message:
                continue
            if message[0] == -1:
                details = message[1] if len(message) > 1 else {}
                if isinstance(details, dict) and details.get("error") == "AuthRequired":
                    raise ValueError("X requires a signed-in Firefox session that can view this profile")
                raise ValueError("X import failed: " + str(details)[:240])
            if message[0] not in (1, 2, 3):
                continue
            data = message[2] if len(message) > 2 and isinstance(message[2], dict) else (
                message[1] if len(message) > 1 and isinstance(message[1], dict) else None
            )
            if not data:
                continue
            post_id = str(data.get("tweet_id") or "")
            if not post_id.isdigit():
                continue
            author = data.get("author") or data.get("user") or {}
            if not isinstance(author, dict):
                author = {}
            row = posts.setdefault(post_id, {
                "id": post_id,
                "handle": str(author.get("name") or ""),
                "display_name": str(author.get("nick") or author.get("name") or ""),
                "avatar_url": str(author.get("profile_image") or ""),
                "text": str(data.get("content") or ""),
                "date": str(data.get("date") or "")[:19],
                "likes": int(data.get("favorite_count") or 0),
                "replies": int(data.get("reply_count") or 0),
                "retweets": int(data.get("retweet_count") or 0),
                "reply_id": str(data.get("reply_id") or ""),
                "quote_id": str(data.get("quoted_id") or ""),
                "media_count": int(data.get("count") or 0),
                "source_media": [],
            })
            if message[0] == 3 and len(message) > 1 and isinstance(message[1], str):
                row["source_media"].append(message[1])
        ordered = sorted(posts.values(), key=lambda row: int(row["id"]), reverse=True)
        return ordered[:limit]

    def _run_dump(self, handle, limit):
        command = [
            str(self.gallery_dl), "--cookies-from-browser", "firefox",
            "-o", f"cache.file={self.cache_root / 'gallery-dl-cache.sqlite3'}",
            "-o", "extractor.twitter.text-tweets=true",
            "-o", "extractor.twitter.retweets=true",
            "-o", "extractor.twitter.ratelimit=abort",
            "--post-range", f"1-{limit}",
            "--dump-json", f"https://x.com/{handle}/tweets",
        ]
        try:
            result = subprocess.run(command, capture_output=True, text=True,
                                    timeout=300, check=False)
        except subprocess.TimeoutExpired as exc:
            raise ValueError("X took too long to return this profile. Your saved library is unchanged; try again shortly.") from exc
        except (OSError, subprocess.SubprocessError) as exc:
            raise ValueError(f"Could not start X import: {exc}") from exc
        try:
            posts = self._parse_messages(json.loads(result.stdout), limit)
        except (ValueError, json.JSONDecodeError) as exc:
            if "signed-in Firefox" in str(exc):
                raise
            if "rate limit" in (str(exc) + result.stderr).lower():
                raise ValueError("X temporarily limited profile requests. Wait a few minutes, then retry; your saved library is unchanged.") from exc
            detail = (result.stderr or str(exc)).strip()[-320:]
            raise ValueError(f"Could not read X posts: {detail}") from exc
        if result.returncode or not posts:
            detail = result.stderr.strip()[-240:] or "No accessible posts were returned"
            raise ValueError(f"Could not read X posts: {detail}")
        return posts

    def _download_avatar(self, account_root, url):
        path = account_root / "avatar.jpg"
        if not url or path.is_file():
            return str(path) if path.is_file() else ""
        try:
            with urlopen(Request(url, headers={"User-Agent": "RockPod/1.0"}), timeout=20) as response:
                content = response.read(2 * 1024 * 1024 + 1)
            if len(content) > 2 * 1024 * 1024:
                return ""
            temporary = path.with_suffix(".tmp")
            temporary.write_bytes(content)
            with Image.open(temporary) as image:
                image.verify()
            os.replace(temporary, path)
        except (OSError, ValueError):
            return ""
        return str(path)

    def import_profile(self, url, limit=50, include_media=True, progress=None):
        handle = twitter_handle(url)
        limit = max(1, min(MAX_POSTS, int(limit)))
        if not self.gallery_dl.is_file():
            raise ValueError("gallery-dl is not installed in RockPod's environment")
        root = self._require_root()
        progress = progress or (lambda _message: None)
        progress(f"Reading the latest {limit} posts from @{handle}…")
        posts = self._run_dump(handle, limit)
        account_root = root / handle
        media_root = account_root / "media"
        media_root.mkdir(parents=True, exist_ok=True)
        failures = []
        if include_media:
            for number, post in enumerate(posts, 1):
                if not post["source_media"] and not post["media_count"]:
                    continue
                cached = [path for path in media_root.glob(post["id"] + "_*")
                          if path.is_file() and path.stat().st_size and
                          path.suffix.lower() in PHOTO_SUFFIXES | VIDEO_SUFFIXES]
                if len(cached) >= max(1, post["media_count"]):
                    continue
                post_url = f"https://x.com/{handle}/status/{post['id']}"
                command = [
                    str(self.gallery_dl), "--cookies-from-browser", "firefox",
                    "-o", f"cache.file={root / 'gallery-dl-cache.sqlite3'}",
                    "-o", "extractor.twitter.ratelimit=abort",
                    "--download-archive", str(account_root / "gallery-dl.sqlite3"),
                    "--no-mtime", "--directory", str(media_root),
                    "--filename", "{tweet_id}_{num}.{extension}", post_url,
                ]
                progress(f"Media {number}/{len(posts)} · @{handle}")
                try:
                    result = subprocess.run(command, capture_output=True, text=True,
                                            timeout=240, check=False)
                except (OSError, subprocess.SubprocessError):
                    failures.append(post["id"])
                    continue
                if result.returncode:
                    failures.append(post["id"])
        for post in posts:
            candidates = sorted(media_root.glob(post["id"] + "_*"))
            post["media"] = [str(path) for path in candidates
                             if path.is_file() and path.suffix.lower() in
                             PHOTO_SUFFIXES | VIDEO_SUFFIXES][:MAX_MEDIA_PER_POST]
            del post["source_media"]
        first = posts[0]
        avatar = self._download_avatar(account_root, first.get("avatar_url"))
        account = {
            "handle": handle, "display_name": first.get("display_name") or handle,
            "avatar_path": avatar, "url": f"https://x.com/{handle}",
            "requested_posts": limit, "posts": posts,
        }
        payload = self._load()
        payload["accounts"] = [row for row in payload["accounts"]
                               if row.get("handle") != handle] + [account]
        atomic_write_text(root / "library.json", json.dumps(payload, indent=2, ensure_ascii=False) + "\n")
        return {"accounts": 1, "posts": len(posts),
                "media": sum(len(post["media"]) for post in posts),
                "media_errors": failures, "cache": str(root)}

    def sync(self, mount_path, progress=None, video_profile=None):
        mount = validate_device_root(mount_path)
        root = self._require_root()
        accounts = self.list_accounts()
        if not accounts:
            raise ValueError("Import an X profile before syncing Twitter")
        cfg = _ExternalCacheConfig(self.config, root)
        selected_profile = app_video_profile(cfg, video_profile)
        device_root = Path(resolve_under_root(mount, TWITTER_ROOT))
        for name in ("media", "thumbs", "avatars", "assets"):
            (device_root / name).mkdir(parents=True, exist_ok=True)
        staging = Path(tempfile.mkdtemp(prefix=".staging-", dir=device_root))
        index = DeviceSyncIndex(cfg, mount)
        report = {"accounts": len(accounts), "posts": 0, "media": 0,
                  "updated": 0, "unchanged": 0, "missing_media": 0}
        account_lines = ["handle\tdisplay_name\tposts\tavatar"]
        post_lines = ["id\thandle\tname\tdate\ttext\treplies\tretweets\tlikes\tavatar\tthumb\tmedia1\tmedia2\tmedia3\tmedia4\treply_id\tquote_id\tauthor_handle"]
        preview_lines = ["path"]
        valid_ids = set()
        try:
            for account in accounts:
                handle = twitter_handle(account.get("handle"))
                valid_ids.add(handle)
                avatar_device = ""
                source_avatar = account.get("avatar_path") or ""
                if source_avatar and Path(source_avatar).is_file():
                    target = device_root / "avatars" / f"{handle}.bmp"
                    signature = _source_signature(source_avatar) + ":twitter-avatar-40-v1"
                    if not index.current("twitter", handle, "avatar", signature, [target]) or not target.is_file():
                        with Image.open(source_avatar) as image:
                            ImageOps.fit(image.convert("RGB"), (40, 40), Image.Resampling.LANCZOS).save(staging / target.name, "BMP")
                        os.replace(staging / target.name, target)
                        index.mark("twitter", handle, "avatar", signature, [target])
                    avatar_device = f"/{TWITTER_ROOT}/avatars/{target.name}"
                posts = account.get("posts") or []
                account_lines.append("\t".join([handle,
                    _ipod_text(account.get("display_name"), 70), str(len(posts)), avatar_device]))
                for number, post in enumerate(posts, 1):
                    post_id = str(post.get("id") or "")
                    if not post_id.isdigit():
                        continue
                    valid_ids.add(post_id)
                    if progress:
                        progress(f"Twitter @{handle} · {number}/{len(posts)}")
                    media_paths = []
                    thumbnail = ""
                    for media_number, source in enumerate((post.get("media") or [])[:MAX_MEDIA_PER_POST], 1):
                        if not Path(source).is_file():
                            report["missing_media"] += 1
                            continue
                        source_signature = _source_signature(source)
                        key = f"{post_id}-{media_number}"
                        valid_ids.add(key)
                        if Path(source).suffix.lower() in PHOTO_SUFFIXES:
                            target = device_root / "media" / f"{key}.bmp"
                            signature = source_signature + ":twitter-photo-320x180-v1"
                            if not index.current("twitter", key, "photo", signature, [target]) or not target.is_file():
                                with Image.open(source) as image:
                                    canvas = Image.new("RGB", (320, 180), "white")
                                    fitted = ImageOps.contain(ImageOps.exif_transpose(image).convert("RGB"),
                                                              (320, 180), Image.Resampling.LANCZOS)
                                    canvas.paste(fitted, ((320-fitted.width)//2, (180-fitted.height)//2))
                                    canvas.save(staging / target.name, "BMP")
                                os.replace(staging / target.name, target)
                                index.mark("twitter", key, "photo", signature, [target])
                                report["updated"] += 1
                            else:
                                report["unchanged"] += 1
                            media_paths.append(f"/{TWITTER_ROOT}/media/{target.name}")
                            preview_lines.append(media_paths[-1])
                        else:
                            extension = app_video_extension(selected_profile)
                            target = device_root / "media" / f"{key}{extension}"
                            signature = source_signature + ":twitter-video-v1:" + selected_profile
                            if not index.current("twitter", key, "video", signature, [target]) or not target.is_file():
                                staged = staging / target.name
                                stage_app_video(source, staged, config=cfg, profile=selected_profile,
                                                cache_namespace="twitter-video", device_key=key,
                                                title=post.get("text") or key, artist="@" + handle)
                                os.replace(staged, target)
                                index.mark("twitter", key, "video", signature, [target])
                                report["updated"] += 1
                            else:
                                report["unchanged"] += 1
                            media_paths.append(f"/{TWITTER_ROOT}/media/{target.name}")
                        if not thumbnail:
                            thumb = device_root / "thumbs" / f"{post_id}.bmp"
                            thumb_signature = source_signature + ":twitter-feed-296x64-v2"
                            if not index.current("twitter", post_id, "thumb", thumb_signature, [thumb]) or not thumb.is_file():
                                preview_source = Path(source)
                                if preview_source.suffix.lower() in VIDEO_SUFFIXES:
                                    preview_root = root / "work" / "twitter-previews"
                                    preview_root.mkdir(parents=True, exist_ok=True)
                                    preview_source = preview_root / f"{key}.png"
                                    subprocess.run([
                                        str(cfg.get("ffmpeg_binary", "ffmpeg")),
                                        "-v", "error", "-y", "-i", str(source),
                                        "-frames:v", "1", "-vf", "scale=296:64:force_original_aspect_ratio=increase,crop=296:64",
                                        str(preview_source),
                                    ], check=True, capture_output=True, timeout=60)
                                with Image.open(preview_source) as image:
                                    ImageOps.fit(ImageOps.exif_transpose(image).convert("RGB"),
                                                 (296, 64), Image.Resampling.LANCZOS).save(staging / thumb.name, "BMP")
                                os.replace(staging / thumb.name, thumb)
                                index.mark("twitter", post_id, "thumb", thumb_signature, [thumb])
                            thumbnail = f"/{TWITTER_ROOT}/thumbs/{thumb.name}"
                        report["media"] += 1
                    fields = [post_id, handle, post.get("display_name") or account.get("display_name") or handle,
                              post.get("date"), post.get("text"), post.get("replies"),
                              post.get("retweets"), post.get("likes"), avatar_device, thumbnail,
                              *(media_paths + [""] * (MAX_MEDIA_PER_POST-len(media_paths))),
                              post.get("reply_id"), post.get("quote_id"), post.get("handle") or handle]
                    post_lines.append("\t".join(_ipod_text(value, 2047 if i == 4 else 510) for i, value in enumerate(fields)))
                    report["posts"] += 1
            atomic_write_text(device_root / "accounts.tsv", "\n".join(account_lines) + "\n")
            atomic_write_text(device_root / "library.tsv", "\n".join(post_lines) + "\n")
            atomic_write_text(device_root / "previews.tsv", "\n".join(preview_lines) + "\n")
            logo_source = self.repo_root / "assets/ipodjs/rockbox/twitter/twitter-logo.90x16.bmp"
            if logo_source.is_file():
                target = device_root / "assets" / "twitter-logo.bmp"
                if not target.is_file() or target.read_bytes() != logo_source.read_bytes():
                    shutil.copy2(logo_source, staging / target.name)
                    os.replace(staging / target.name, target)
            icon_source = self.repo_root / "assets/ipodjs/rockbox/applications/twitter.46x46x24.bmp"
            if icon_source.is_file():
                icon_target = Path(resolve_under_root(
                    mount, ".rockbox/ipodjs/applications/twitter.46x46x24.bmp"))
                icon_target.parent.mkdir(parents=True, exist_ok=True)
                if not icon_target.is_file() or icon_target.read_bytes() != icon_source.read_bytes():
                    shutil.copy2(icon_source, staging / icon_target.name)
                    os.replace(staging / icon_target.name, icon_target)
            index.prune("twitter", valid_ids)
        finally:
            index.close()
            shutil.rmtree(staging, ignore_errors=True)
        return report
