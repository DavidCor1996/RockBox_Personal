"""Authenticated Firefox capture and device sync for the native OnlyFans app."""

from __future__ import annotations

import hashlib
import html
import json
import os
import re
import selectors
import shutil
import sqlite3
import subprocess
import tempfile
import time
import urllib.error
import urllib.request
import xml.etree.ElementTree as ET
import zlib
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from urllib.parse import unquote, urljoin, urlsplit

from PIL import Image, ImageOps

from services.device_sync_index import DeviceSyncIndex
from services.device_manifest import present_manifest_ids
from services.file_safety import atomic_write_text
from services.path_safety import resolve_under_root, validate_device_root
from services.app_video_sync import (
    app_video_extension,
    app_video_profile,
    app_video_signature,
    device_video_target,
    remove_alternate_video,
    stage_app_video,
)
from scripts.offlineweb_sync import (
    _discover_firefox_profile,
    _firefox_cache_entries_root,
    _firefox_cache_image_records,
)


ONLYFANS_ROOT = ".rockbox/onlyfans"
ONLYFANS_MEDIA_ROOT = f"{ONLYFANS_ROOT}/media"
ONLYFANS_THUMB_ROOT = f"{ONLYFANS_ROOT}/thumbnails"
ONLYFANS_DISPLAY_ROOT = f"{ONLYFANS_ROOT}/display"
ONLYFANS_PROFILE_FEED_ROOT = f"{ONLYFANS_ROOT}/profile-feed"
ONLYFANS_ASSET_ROOT = f"{ONLYFANS_ROOT}/assets"
ONLYFANS_LIBRARY = f"{ONLYFANS_ROOT}/library.tsv"
ONLYFANS_PROFILES = f"{ONLYFANS_ROOT}/profiles.tsv"
ONLYFANS_PREVIEWS = f"{ONLYFANS_ROOT}/previews.tsv"
ONLYFANS_HIDDEN = f"{ONLYFANS_ROOT}/hidden"
ONLYFANS_CAPTURE_WAIT = 10
ONLYFANS_MAX_POSTS = 1024
ONLYFANS_PHOTO_SUFFIXES = {".jpg", ".jpeg", ".png", ".webp", ".bmp"}
ONLYFANS_VIDEO_SUFFIXES = {".mp4", ".m4v", ".mov", ".mkv", ".webm", ".avi"}
OFSCRAPER_VERSION = "3.14.7"


def _clean(value, limit=180):
    text = html.unescape(str(value or ""))
    text = re.sub(r"<br\s*/?>", " ", text, flags=re.IGNORECASE)
    text = re.sub(r"<[^>]+>", "", text)
    return re.sub(r"\s+", " ", text).strip()[:limit]


def _write_if_changed(path, text):
    try:
        if Path(path).read_text(encoding="utf-8") == text:
            return False
    except OSError:
        pass
    atomic_write_text(path, text)
    return True


def _copy_if_changed(source, target):
    source = Path(source)
    target = Path(target)
    if target.is_file() and hashlib.sha256(source.read_bytes()).digest() == hashlib.sha256(
        target.read_bytes()
    ).digest():
        return False
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = target.with_name(target.name + ".rockpod-tmp")
    shutil.copy2(source, temporary)
    os.replace(temporary, target)
    return True


def _profile_slug(url):
    parsed = urlsplit(str(url or "").strip())
    if parsed.scheme not in {"http", "https"} or not parsed.netloc.lower().endswith(
        "onlyfans.com"
    ):
        raise ValueError("Enter an OnlyFans profile URL")
    slug = parsed.path.strip("/").split("/", 1)[0]
    if not slug or not re.fullmatch(r"[A-Za-z0-9._-]+", slug):
        raise ValueError("Enter an OnlyFans profile URL")
    return slug


def _media_signature(path):
    stat = os.stat(path)
    return f"{stat.st_size}:{stat.st_mtime_ns}"


def _display_shard(item_id):
    """Keep long photo-view filenames out of one FAT directory."""
    checksum = zlib.crc32(str(item_id).encode("utf-8")) & 0xFFFFFFFF
    return f"{checksum:08x}"[0]


def _item_display_path(display_root, item_id):
    return Path(display_root) / _display_shard(item_id) / f"{item_id}.bmp"


def _migrate_legacy_display_files(display_root, item_ids):
    """Move flat item views into 16 shards without touching profile artwork."""
    root = Path(display_root)
    known_ids = {str(item_id) for item_id in item_ids if item_id}
    migrated = 0
    for legacy in list(root.iterdir()):
        if not legacy.is_file():
            continue
        name = legacy.name
        if ".zoom" in name:
            item_id = name.split(".zoom", 1)[0]
        elif name.endswith(".photo.source"):
            item_id = name[:-len(".photo.source")]
        elif name.endswith(".bmp"):
            item_id = name[:-len(".bmp")]
        else:
            continue
        if item_id not in known_ids:
            continue
        target_dir = root / _display_shard(item_id)
        target_dir.mkdir(exist_ok=True)
        target = target_dir / name
        if target.exists():
            # A prior interrupted migration may have produced both names.
            # Keep the larger artifact; valid BMPs always beat empty partials.
            if legacy.stat().st_size > target.stat().st_size:
                os.replace(legacy, target)
            else:
                legacy.unlink()
        else:
            os.replace(legacy, target)
        migrated += 1
    return migrated


def _probe_stream_types(path, ffprobe="ffprobe"):
    try:
        result = subprocess.run(
            [ffprobe, "-v", "error", "-show_entries", "stream=codec_type",
             "-of", "default=noprint_wrappers=1:nokey=1", str(path)],
            check=False, capture_output=True, text=True, timeout=30,
        )
        return set(result.stdout.split()) if result.returncode == 0 else set()
    except (OSError, subprocess.SubprocessError):
        return set()


def _media_decodes(path, ffmpeg="ffmpeg"):
    """Reject DRM/encrypted payloads whose headers merely resemble MP4 tracks."""
    try:
        result = subprocess.run(
            [ffmpeg, "-hide_banner", "-loglevel", "error", "-i", str(path),
             "-t", "2", "-map", "0:v:0", "-map", "0:a:0", "-f", "null", "-"],
            check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            timeout=60,
        )
        return result.returncode == 0
    except (OSError, subprocess.SubprocessError):
        return False


def _metadata_enumeration_complete(lines):
    """Recognize OF-Scraper's completion message even when Rich wraps it."""
    recent = " ".join(str(line or "").strip() for line in lines[-4:])
    return bool(re.search(
        r"Returning.{0,180}?\b\d+\s+final media items for metadata", recent,
        re.IGNORECASE,
    ))


def _firefox_cookie_header(profile_path, request_url):
    """Return only cookies applicable to this CDN request; never persist them."""
    source = Path(profile_path) / "cookies.sqlite"
    if not source.is_file():
        return ""
    parsed = urlsplit(request_url)
    host = parsed.netloc.lower().split(":", 1)[0]
    request_path = parsed.path or "/"
    now = int(time.time())
    try:
        with tempfile.TemporaryDirectory(prefix="rockpod-of-cookies-") as temp:
            copied = Path(temp) / "cookies.sqlite"
            shutil.copy2(source, copied)
            for suffix in ("-wal", "-shm"):
                sidecar = Path(str(source) + suffix)
                if sidecar.is_file():
                    shutil.copy2(sidecar, Path(str(copied) + suffix))
            connection = sqlite3.connect(copied)
            try:
                rows = connection.execute(
                    "SELECT host, path, name, value, expiry, isSecure "
                    "FROM moz_cookies"
                ).fetchall()
            finally:
                connection.close()
    except (OSError, sqlite3.Error):
        return ""
    cookies = []
    for cookie_host, cookie_path, name, value, expiry, secure in rows:
        domain = str(cookie_host or "").lower().lstrip(".")
        if not domain or not (host == domain or host.endswith("." + domain)):
            continue
        if not request_path.startswith(str(cookie_path or "/")):
            continue
        if int(expiry or 0) and int(expiry) < now:
            continue
        if secure and parsed.scheme != "https":
            continue
        cookies.append(f"{name}={value}")
    return "; ".join(cookies)


class OnlyFansAppService:
    """Owns captured profiles without exposing Firefox cookies or signed URLs."""

    def __init__(self, config, repo_root):
        self.config = config
        self.repo_root = Path(repo_root)
        self.root = Path(config.get("cache_dir")) / "onlyfans"
        self.index_path = self.root / "library.json"
        self.root.mkdir(parents=True, exist_ok=True)

    @property
    def ofscraper_binary(self):
        configured = str(self.config.get("ofscraper_binary", "") or "")
        if configured:
            return Path(configured).expanduser()
        return self.repo_root / "rockpod/.ofscraper-venv/bin/ofscraper"

    @property
    def ofscraper_config_root(self):
        return self.root / "ofscraper"

    @property
    def ofscraper_download_root(self):
        return self.root / "ofscraper-downloads"

    @property
    def ofscraper_auth_path(self):
        return self.ofscraper_config_root / "main_profile/auth.json"

    def ofscraper_ready(self):
        return self.ofscraper_binary.is_file()

    def ofscraper_authenticated(self):
        try:
            payload = json.loads(self.ofscraper_auth_path.read_text(encoding="utf-8"))
        except (OSError, ValueError, TypeError):
            return False
        if isinstance(payload.get("auth"), dict):
            payload = payload["auth"]
        return all(
            str(payload.get(key) or "").strip()
            for key in ("sess", "auth_id", "user_agent", "x-bc")
        )

    def _ensure_ofscraper_config(self):
        """Create a private, deterministic download-only workspace."""
        self.ofscraper_config_root.mkdir(parents=True, exist_ok=True)
        self.ofscraper_download_root.mkdir(parents=True, exist_ok=True)
        (self.ofscraper_config_root / "main_profile").mkdir(
            parents=True, exist_ok=True
        )
        path = self.ofscraper_config_root / "config.json"
        try:
            current = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError, TypeError):
            current = {}
        current["main_profile"] = "main_profile"
        # OF-Scraper 3.14.7 has an upstream fallback bug when this otherwise
        # optional key is absent, so always write it explicitly.
        current["discord"] = ""
        file_options = current.setdefault("file_options", {})
        file_options.update(
            {
                "save_location": str(self.ofscraper_download_root),
                "dir_format": "{model_username}/{post_id}/{media_type}/",
                "file_format": "{media_id}_{filename}.{ext}",
                "textlength": 0,
                "space_replacer": "_",
                "date": "YYYY-MM-DD",
                "text_type_default": "letter",
                "truncation_default": True,
            }
        )
        download_options = current.setdefault("download_options", {})
        download_options.update(
            {
                "filter": ["Images", "Videos"],
                "auto_resume": True,
                "system_free_min": 0,
                "max_post_count": 0,
                "verify_all_integrity": False,
            }
        )
        current["binary_options"] = {
            "ffmpeg": str(self.config.get("ffmpeg_binary", "ffmpeg"))
        }
        advanced = current.setdefault("advanced_options", {})
        advanced.update(
            {
                "downloadbars": True,
                "incremental_downloads": True,
                "skip_unavailable_content": True,
            }
        )
        # RockPod never supplies CDM credentials or a remote key service. The
        # command also applies --normal-only before every run.
        current["cdm_options"] = {
            "private-key": None,
            "client-id": None,
            "key-mode-default": "manual",
        }
        scripts = current.setdefault("script_options", {})
        scripts.update(
            {
                "post_script": None,
                "naming_script": None,
                "after_download_script": None,
                "skip_download_script": None,
            }
        )
        atomic_write_text(path, json.dumps(current, indent=2) + "\n")
        return path

    def _ofscraper_command(self, username, manifest_path=None):
        command = [
            str(self.ofscraper_binary),
            "metadata",
            "--config", str(self.ofscraper_config_root),
            "--username", username,
            "--username-individual-search",
            "--posts", "all",
            "--metadata", "check",
            "--normal-only",
            "--quality", "source",
            "--mediatype", "images,videos",
            "--output", "normal",
            "--log", "off",
            "--no-rich",
            "--auth-fail",
            "--no-api-cache",
            "--update-profile",
        ]
        return command

    def launch_ofscraper_login(self, profile_url):
        """Open the requested OnlyFans page in the Firefox profile we read."""
        username = _profile_slug(profile_url)
        firefox = shutil.which("firefox")
        if not firefox:
            raise ValueError(
                "Firefox was not found. Open Firefox and sign in to OnlyFans, "
                "then click Set Up / Refresh Login again."
            )
        subprocess.Popen(
            [firefox, "--new-window", f"https://onlyfans.com/{username}"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            start_new_session=True,
        )
        return self.ofscraper_auth_path

    def import_ofscraper_login_from_firefox(self):
        """Build OF-Scraper auth from Firefox storage without browser input."""
        profile = _discover_firefox_profile()
        if not profile:
            raise ValueError("Firefox profile was not found")
        profile = Path(profile)
        cookie_source = profile / "cookies.sqlite"
        storage_source = (
            profile
            / "storage/default/https+++onlyfans.com/ls/data.sqlite"
        )
        if not cookie_source.is_file() or not storage_source.is_file():
            raise ValueError(
                "Sign in to OnlyFans in Firefox once, then refresh the login."
            )
        with tempfile.TemporaryDirectory(prefix="rockpod-of-auth-") as temp:
            temp = Path(temp)

            def snapshot(source, name):
                target = temp / name
                shutil.copy2(source, target)
                for suffix in ("-wal", "-shm"):
                    sidecar = Path(str(source) + suffix)
                    if sidecar.is_file():
                        shutil.copy2(sidecar, Path(str(target) + suffix))
                return target

            cookie_copy = snapshot(cookie_source, "cookies.sqlite")
            storage_copy = snapshot(storage_source, "storage.sqlite")
            try:
                connection = sqlite3.connect(cookie_copy)
                try:
                    rows = connection.execute(
                        "SELECT name, value, host, expiry FROM moz_cookies "
                        "WHERE host IN ('onlyfans.com', '.onlyfans.com') "
                        "AND name IN ('sess', 'auth_id') "
                        "ORDER BY (host='onlyfans.com') DESC, expiry DESC"
                    ).fetchall()
                finally:
                    connection.close()
                cookies = {}
                for name, value, _host, _expiry in rows:
                    if name not in cookies and value:
                        cookies[str(name)] = str(value)
                connection = sqlite3.connect(storage_copy)
                try:
                    row = connection.execute(
                        "SELECT value FROM data WHERE key='bcTokenSha'"
                    ).fetchone()
                finally:
                    connection.close()
            except sqlite3.Error as exc:
                raise ValueError("Firefox OnlyFans login storage could not be read") from exc
        token = bytes(row[0]).decode("ascii") if row and row[0] else ""
        if not re.fullmatch(r"[A-Za-z0-9_-]{20,200}", token):
            raise ValueError(
                "Firefox does not contain a current OnlyFans browser token."
            )
        if not cookies.get("sess") or not cookies.get("auth_id"):
            raise ValueError("Firefox does not contain a signed-in OnlyFans session.")
        try:
            version_result = subprocess.run(
                ["firefox", "--version"],
                check=False,
                capture_output=True,
                text=True,
                timeout=10,
            )
            match = re.search(r"(\d+(?:\.\d+)*)", version_result.stdout)
            product_version = match.group(1) if match else "128.0"
            # Firefox's package version can include patch components, while
            # navigator.userAgent exposes only the major Gecko version.
            version = product_version.split(".", 1)[0] + ".0"
        except (OSError, subprocess.SubprocessError):
            version = "128.0"
        auth = {
            "sess": cookies["sess"],
            "auth_id": cookies["auth_id"],
            "auth_uid": "",
            "user_agent": (
                "Mozilla/5.0 (X11; Linux x86_64; rv:"
                f"{version}) Gecko/20100101 Firefox/{version}"
            ),
            "x-bc": token,
        }
        self._ensure_ofscraper_config()
        atomic_write_text(
            self.ofscraper_auth_path,
            json.dumps(auth, indent=2) + "\n",
        )
        os.chmod(self.ofscraper_auth_path, 0o600)
        return self.ofscraper_auth_path

    def _load(self):
        try:
            payload = json.loads(self.index_path.read_text(encoding="utf-8"))
        except (OSError, ValueError, TypeError):
            payload = {"profiles": []}
        if not isinstance(payload, dict) or not isinstance(payload.get("profiles"), list):
            return {"profiles": []}
        return payload

    def _save(self, payload):
        atomic_write_text(self.index_path, json.dumps(payload, indent=2) + "\n")

    def _repair_owner_media_in_payload(self, payload):
        """Recover permanent owner posts from disk and migrate legacy records."""
        changed = False
        for profile in payload.get("profiles") or []:
            if not profile.get("is_my_profile"):
                continue
            username = str(profile.get("username") or "")
            media_root = self.root / username / "media"
            if not media_root.is_dir():
                continue
            existing = {
                str(item.get("id") or Path(item.get("source_path") or "").stem):
                dict(item)
                for item in (profile.get("media") or [])
            }
            recovered = []
            for source in media_root.iterdir():
                if not source.is_file() or not source.name.startswith(("of_", "ofv_")):
                    continue
                suffix = source.suffix.lower()
                if suffix in ONLYFANS_PHOTO_SUFFIXES:
                    media_type = "photo"
                    try:
                        with Image.open(source) as image:
                            width, height = image.size
                    except OSError:
                        continue
                elif suffix in ONLYFANS_VIDEO_SUFFIXES:
                    media_type = "video"
                    width, height = 0, 0
                else:
                    continue
                item_id = source.stem
                item = existing.get(item_id, {})
                item.update(
                    {
                        "id": item_id,
                        "type": media_type,
                        "title": _clean(
                            item.get("title") or
                            ("Recovered Photo Post" if media_type == "photo"
                             else "Recovered Video Post"),
                            80,
                        ),
                        "caption": _clean(item.get("caption"), 180),
                        "source_path": str(source),
                        "width": int(item.get("width") or width),
                        "height": int(item.get("height") or height),
                        "manual_import": True,
                        "imported_at_ns": int(
                            item.get("imported_at_ns") or source.stat().st_mtime_ns
                        ),
                        "imported_at": item.get("imported_at") or time.strftime(
                            "%Y-%m-%d %H:%M:%S", time.localtime(source.stat().st_mtime)
                        ),
                    }
                )
                recovered.append(item)
            recovered_ids = {
                str(item.get("id") or "") for item in recovered
            }
            # A missing laptop file is not an instruction to erase a post
            # that has already become device-only.  Keep its metadata until
            # the user explicitly removes the post/profile.
            for item_id, item in existing.items():
                if item_id not in recovered_ids:
                    recovered.append(dict(item))
            recovered.sort(
                key=lambda item: (
                    int(item.get("imported_at_ns") or 0),
                    str(item.get("id") or ""),
                ),
                reverse=True,
            )
            recovered = recovered[:ONLYFANS_MAX_POSTS]
            if recovered != (profile.get("media") or []):
                profile["media"] = recovered
                changed = True
        return changed

    def list_profiles(self):
        payload = self._load()
        if self._repair_owner_media_in_payload(payload):
            self._save(payload)
        return sorted(
            payload["profiles"],
            key=lambda profile: (
                not bool(profile.get("is_my_profile")),
                str(profile.get("username") or "").lower(),
            ),
        )

    def get_my_profile(self):
        return next(
            (
                profile for profile in self.list_profiles()
                if profile.get("is_my_profile")
            ),
            None,
        )

    def save_my_profile(
        self, url, display_name="", bio="", avatar_path="", cover_path=""
    ):
        """Create or edit the owner's profile, including a valid zero-post one."""
        username = _profile_slug(url)
        canonical_url = f"https://onlyfans.com/{username}"
        payload = self._load()
        existing = next(
            (
                profile for profile in payload["profiles"]
                if str(profile.get("username") or "").lower() == username.lower()
            ),
            {},
        )
        self._repair_owner_media_in_payload(payload)
        existing = next(
            (
                profile for profile in payload["profiles"]
                if str(profile.get("username") or "").lower() == username.lower()
            ),
            existing,
        )
        profile_root = self.root / username
        profile_root.mkdir(parents=True, exist_ok=True)

        def store_art(source, filename, fallback):
            source = os.path.abspath(os.path.expanduser(str(source or "")))
            if not source:
                return fallback or ""
            if not os.path.isfile(source):
                raise ValueError(f"Missing profile image: {source}")
            target = profile_root / filename
            try:
                with Image.open(source) as image:
                    image.verify()
            except OSError as exc:
                raise ValueError(f"Could not read profile image: {source}") from exc
            if os.path.realpath(source) != os.path.realpath(target):
                if Path(source).suffix.lower() in {".jpg", ".jpeg"}:
                    temporary = target.with_name(target.name + ".rockpod-tmp")
                    shutil.copy2(source, temporary)
                    os.replace(temporary, target)
                else:
                    with Image.open(source) as image:
                        ImageOps.exif_transpose(image).convert("RGB").save(
                            target, "JPEG", quality=95
                        )
            return str(target)

        profile = {
            "url": canonical_url,
            "username": username,
            "display_name": _clean(display_name or username, 80),
            "bio": _clean(bio, 180),
            "avatar_path": store_art(
                avatar_path, "avatar.jpg", existing.get("avatar_path")
            ),
            "cover_path": store_art(
                cover_path, "header.jpg", existing.get("cover_path")
            ),
            # The owner profile is edited locally. The owner-only media folder
            # is authoritative, including legacy RockPod imports that predate
            # the manual_import flag. Generic Firefox captures never write here.
            "media": [
                item for item in (existing.get("media") or [])
                if item.get("manual_import")
            ],
            "captured_at": existing.get("captured_at") or time.strftime(
                "%Y-%m-%d %H:%M:%S"
            ),
            "is_my_profile": True,
        }
        payload["profiles"] = [
            item for item in payload["profiles"]
            if str(item.get("username") or "").lower() != username.lower()
        ] + [profile]
        self._save(payload)
        return profile

    def add_my_profile_post(self, source_path, title="", caption=""):
        """Import a local photo/video as a permanent owner-created post."""
        profile = self.get_my_profile()
        if not profile:
            raise ValueError("Save My Profile before adding a post")
        source = Path(os.path.abspath(os.path.expanduser(str(source_path or ""))))
        if not source.is_file():
            raise ValueError("Choose an existing photo or video")
        suffix = source.suffix.lower()
        if suffix in ONLYFANS_PHOTO_SUFFIXES:
            media_type = "photo"
            try:
                with Image.open(source) as image:
                    width, height = image.size
            except OSError as exc:
                raise ValueError("The selected photo could not be read") from exc
        elif suffix in ONLYFANS_VIDEO_SUFFIXES:
            media_type = "video"
            width, height = 0, 0
            if "video" not in _probe_stream_types(
                source, self.config.get("ffprobe_binary", "ffprobe")
            ):
                raise ValueError("The selected file does not contain playable video")
        else:
            raise ValueError("Choose a JPG, PNG, WebP, BMP, MP4, MOV, MKV, WebM or AVI file")

        digest = hashlib.sha256()
        with source.open("rb") as handle:
            for block in iter(lambda: handle.read(1024 * 1024), b""):
                digest.update(block)
        item_id = ("of_" if media_type == "photo" else "ofv_") + digest.hexdigest()[:20]
        media_root = self.root / profile["username"] / "media"
        media_root.mkdir(parents=True, exist_ok=True)
        target = media_root / f"{item_id}{suffix}"
        if not target.is_file():
            temporary = target.with_name(target.name + ".rockpod-tmp")
            shutil.copy2(source, temporary)
            os.replace(temporary, target)
        item = {
            "id": item_id,
            "type": media_type,
            "title": _clean(title or source.stem, 80),
            "caption": _clean(caption, 180),
            "source_path": str(target),
            "width": width,
            "height": height,
            "manual_import": True,
            "imported_at_ns": time.time_ns(),
            "imported_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        }
        payload = self._load()
        for saved_profile in payload["profiles"]:
            if saved_profile.get("is_my_profile"):
                media = list(saved_profile.get("media") or [])
                media = [saved for saved in media if saved.get("id") != item_id]
                saved_profile["media"] = [item] + media
                break
        self._save(payload)
        return item

    def list_media(self):
        rows = []
        for profile in self.list_profiles():
            for item in profile.get("media") or []:
                row = dict(item)
                row["username"] = profile.get("username") or ""
                row["profile_url"] = profile.get("url") or ""
                rows.append(row)
        return rows

    @staticmethod
    def _recent_video_urls(profile_path, modified_since=0):
        root = _firefox_cache_entries_root(profile_path)
        if not root.is_dir():
            return []
        urls = []
        seen = set()
        cutoff = max(time.time() - 3600, float(modified_since or 0))
        for entry in sorted(root.iterdir(), key=lambda value: value.stat().st_mtime, reverse=True):
            try:
                if entry.stat().st_mtime < cutoff:
                    break
                data = entry.read_bytes()
            except OSError:
                continue
            for match in re.finditer(rb"https?[^\x00\r\n\t <>'\"]+", data):
                value = unquote(match.group(0).decode("utf-8", "ignore"))
                value = value.replace("\\/", "/")
                # Firefox prefixes partitioned cache keys with an encoded
                # origin (`https%2Conlyfans.com),:`). After unquoting, retain
                # the final real request URL rather than parsing that prefix.
                positions = [
                    value.rfind("https://"), value.rfind("http://")
                ]
                start = max(positions)
                if start > 0:
                    value = value[start:]
                try:
                    parsed = urlsplit(value)
                except ValueError:
                    continue
                if parsed.scheme not in {"http", "https"}:
                    continue
                if not parsed.netloc.lower().endswith("onlyfans.com"):
                    continue
                if parsed.netloc.lower() not in {"cdn2.onlyfans.com", "cdn3.onlyfans.com"}:
                    continue
                if Path(parsed.path).suffix.lower() not in {
                    ".mp4", ".m4v", ".mov", ".webm"
                }:
                    continue
                if value in seen:
                    continue
                seen.add(value)
                urls.append(value)
                if len(urls) >= 24:
                    return urls
        return urls

    @staticmethod
    def _recent_dash_groups(profile_path, modified_since=0):
        """Resolve unprotected DASH variants cached by the current capture.

        Firefox cache entries place the response body before their metadata
        trailer. OnlyFans currently gzip-compresses MPD responses, so looking
        only for literal MP4 URLs misses every video. Protected manifests are
        reported to the caller but are never downloaded or decrypted.
        """
        root = _firefox_cache_entries_root(profile_path)
        if not root.is_dir():
            return []
        cutoff = max(time.time() - 3600, float(modified_since or 0))
        groups = []
        seen = set()
        url_pattern = re.compile(rb"https?[^\x00\r\n\t <>'\"]+")
        for entry in sorted(
            root.iterdir(), key=lambda value: value.stat().st_mtime,
            reverse=True,
        ):
            try:
                if entry.stat().st_mtime < cutoff:
                    break
                raw = entry.read_bytes()
            except OSError:
                continue
            manifest_url = ""
            for match in url_pattern.finditer(raw):
                value = unquote(match.group(0).decode("utf-8", "ignore"))
                value = value.replace("\\/", "/")
                start = max(value.rfind("https://"), value.rfind("http://"))
                if start > 0:
                    value = value[start:]
                try:
                    parsed = urlsplit(value)
                except ValueError:
                    continue
                if (
                    parsed.scheme in {"http", "https"}
                    and parsed.netloc.lower() in {
                        "cdn2.onlyfans.com", "cdn3.onlyfans.com"
                    }
                    and Path(parsed.path).suffix.lower() == ".mpd"
                ):
                    manifest_url = value
                    break
            if not manifest_url or manifest_url in seen:
                continue
            seen.add(manifest_url)
            try:
                body = (
                    zlib.decompressobj(16 + zlib.MAX_WBITS).decompress(raw)
                    if raw.startswith(b"\x1f\x8b") else raw
                )
            except zlib.error:
                continue
            start = body.find(b"<MPD")
            end = body.rfind(b"</MPD>")
            if start < 0 or end < start:
                continue
            try:
                document = ET.fromstring(body[start:end + len(b"</MPD>")])
            except ET.ParseError:
                continue

            protected = False
            for element in document.iter():
                local_name = element.tag.rsplit("}", 1)[-1]
                if local_name == "ContentProtection" or any(
                    key.rsplit("}", 1)[-1] == "default_KID"
                    for key in element.attrib
                ):
                    protected = True
                    break

            group = {
                "combined": [], "video": [], "audio": [],
                "protected": protected, "manifest": manifest_url,
            }
            manifest_query = urlsplit(manifest_url).query
            for adaptation in document.iter():
                if adaptation.tag.rsplit("}", 1)[-1] != "AdaptationSet":
                    continue
                media_type = (
                    adaptation.attrib.get("contentType")
                    or adaptation.attrib.get("mimeType", "").split("/", 1)[0]
                ).lower()
                for element in adaptation.iter():
                    if element.tag.rsplit("}", 1)[-1] != "BaseURL":
                        continue
                    relative = (element.text or "").strip()
                    if not relative:
                        continue
                    resolved = urljoin(manifest_url, relative)
                    parsed = urlsplit(resolved)
                    if parsed.netloc.lower() not in {
                        "cdn2.onlyfans.com", "cdn3.onlyfans.com"
                    } or Path(parsed.path).suffix.lower() not in {
                        ".mp4", ".m4v", ".mov", ".webm"
                    }:
                        continue
                    if manifest_query and not parsed.query:
                        resolved += "?" + manifest_query
                    bucket = media_type if media_type in {"video", "audio"} else "combined"
                    if resolved not in group[bucket]:
                        group[bucket].append(resolved)
            if group["video"] or group["audio"] or group["combined"]:
                groups.append(group)
        return groups

    @classmethod
    def _recent_video_groups(cls, profile_path, modified_since=0):
        """Group OnlyFans DASH audio/video variants belonging to one post."""
        groups = {}
        for value in cls._recent_video_urls(profile_path, modified_since):
            parsed = urlsplit(value)
            match = re.match(r"^(.*)_(audio|source|\d+p)(\.[^.]+)$", parsed.path)
            if match:
                key = f"{parsed.netloc}{match.group(1)}"
                variant = match.group(2)
                base_path, suffix = match.group(1), match.group(3)
            else:
                key = f"{parsed.netloc}{parsed.path}"
                variant = "combined"
                base_path, suffix = parsed.path.rsplit(".", 1)[0], Path(parsed.path).suffix
            group = groups.setdefault(
                key,
                {"combined": [], "video": [], "audio": [],
                 "protected": False, "manifest": ""},
            )
            if variant == "audio":
                group["audio"].append(value)
            elif variant == "combined":
                group["combined"].append(value)
            else:
                group["video"].append(value)
            if match:
                # Signed CDN policies commonly cover the sibling DASH files.
                # Trying these fills in audio when Firefox cached only muted autoplay.
                query = f"?{parsed.query}" if parsed.query else ""
                origin = f"{parsed.scheme}://{parsed.netloc}"
                # The iPod output is 320x240; prefer compact DASH renditions
                # and retain source as a compatibility fallback.
                group["video"].extend(
                    f"{origin}{base_path}_{quality}{suffix}{query}"
                    for quality in ("480p", "240p", "source")
                )
                group["audio"].append(f"{origin}{base_path}_audio{suffix}{query}")
        direct = list(groups.values())
        manifests = cls._recent_dash_groups(profile_path, modified_since)
        return manifests + direct

    @staticmethod
    def _scroll_firefox_profile(progress=None, profile_path=None, modified_since=0):
        """Never synthesize input into an authenticated browser window."""
        del profile_path, modified_since
        if progress:
            progress(
                "Automatic page interaction disabled · reading media Firefox "
                "already loaded"
            )

    @staticmethod
    def _recent_post_urls(profile_path, username, modified_since=0):
        """Read post routes created by this capture without touching cookies."""
        source = Path(profile_path) / "places.sqlite"
        if not source.is_file():
            return []
        try:
            with tempfile.TemporaryDirectory(prefix="rockpod-of-history-") as temp:
                copied = Path(temp) / "places.sqlite"
                shutil.copy2(source, copied)
                for suffix in ("-wal", "-shm"):
                    sidecar = Path(str(source) + suffix)
                    if sidecar.is_file():
                        shutil.copy2(sidecar, Path(str(copied) + suffix))
                connection = sqlite3.connect(copied)
                try:
                    rows = connection.execute(
                        "SELECT DISTINCT p.url FROM moz_places p "
                        "JOIN moz_historyvisits v ON v.place_id=p.id "
                        "WHERE p.url LIKE ? AND v.visit_date>=? "
                        "ORDER BY v.visit_date DESC LIMIT 24",
                        (
                            f"https://onlyfans.com/%/{username}",
                            int(float(modified_since or 0) * 1_000_000),
                        ),
                    ).fetchall()
                finally:
                    connection.close()
        except (OSError, sqlite3.Error):
            return []
        pattern = re.compile(
            rf"^https://onlyfans\.com/\d+/{re.escape(username)}(?:[/?#]|$)",
            re.IGNORECASE,
        )
        return [str(row[0]) for row in rows if row and pattern.match(str(row[0]))]

    @staticmethod
    def _play_post_urls(urls, progress=None):
        """Never guess click coordinates on an authenticated account page."""
        if urls and progress:
            progress(
                "Video posts found · no controls clicked without DOM identity"
            )

    def _download_video_group(self, group, target, referer, firefox_profile=None):
        ffprobe = self.config.get("ffprobe_binary", "ffprobe")
        ffmpeg = self.config.get("ffmpeg_binary", "ffmpeg")
        temporary = []

        def fetch(candidates, label):
            for index, remote_url in enumerate(dict.fromkeys(candidates)):
                path = target.with_name(f".{target.stem}-{label}-{index}.mp4")
                try:
                    request = urllib.request.Request(
                        remote_url,
                        headers={
                            "User-Agent": "Mozilla/5.0",
                            "Referer": referer,
                            "Cookie": _firefox_cookie_header(
                                firefox_profile, remote_url
                            ) if firefox_profile else "",
                        },
                    )
                    with urllib.request.urlopen(request, timeout=120) as response, open(path, "wb") as handle:
                        shutil.copyfileobj(response, handle, 1024 * 1024)
                    temporary.append(path)
                    if path.stat().st_size >= 32 * 1024:
                        return path
                except Exception:
                    path.unlink(missing_ok=True)
            return None

        try:
            combined = fetch(group.get("combined") or [], "combined")
            if (
                combined
                and {"video", "audio"} <= _probe_stream_types(combined, ffprobe)
                and _media_decodes(combined, ffmpeg)
            ):
                os.replace(combined, target)
                temporary.remove(combined)
                return True
            video = combined if combined and "video" in _probe_stream_types(combined, ffprobe) else fetch(group.get("video") or [], "video")
            audio = combined if combined and "audio" in _probe_stream_types(combined, ffprobe) else fetch(group.get("audio") or [], "audio")
            if not video or "video" not in _probe_stream_types(video, ffprobe):
                return False
            if not audio or "audio" not in _probe_stream_types(audio, ffprobe):
                return False
            staged = target.with_name(f".{target.stem}-mux.mp4")
            temporary.append(staged)
            subprocess.run(
                [ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-i", str(video),
                 "-i", str(audio), "-map", "0:v:0", "-map", "1:a:0", "-c", "copy", str(staged)],
                check=True, timeout=600,
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            )
            if (
                {"video", "audio"} <= _probe_stream_types(staged, ffprobe)
                and _media_decodes(staged, ffmpeg)
            ):
                os.replace(staged, target)
                temporary.remove(staged)
                return True
            return False
        finally:
            for path in temporary:
                path.unlink(missing_ok=True)

    def _capture_profile_from_firefox(self, url, progress=None):
        username = _profile_slug(url)
        url = f"https://onlyfans.com/{username}"
        capture_url = f"{url}/media"
        existing = next(
            (
                profile for profile in self._load()["profiles"]
                if str(profile.get("username") or "").lower()
                == username.lower()
            ),
            None,
        )
        if existing and existing.get("is_my_profile"):
            if progress:
                progress("My Profile posts are managed with Add Photo/Video Post.")
            media = existing.get("media") or []
            return {
                "profile": existing,
                "photos": sum(item.get("type") == "photo" for item in media),
                "videos": sum(item.get("type") == "video" for item in media),
            }
        capture_started = time.time()
        if progress:
            progress("Opening the profile in Firefox…")
        subprocess.Popen(
            ["firefox", capture_url],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        for remaining in range(ONLYFANS_CAPTURE_WAIT, 4, -1):
            if progress and remaining in {10, 6, 3}:
                progress(f"Loading authenticated media… {remaining}s")
            time.sleep(1)
        firefox_profile = _discover_firefox_profile()
        if not firefox_profile:
            raise ValueError("Firefox profile was not found")
        self._scroll_firefox_profile(
            progress, firefox_profile, capture_started - 2,
        )
        post_urls = self._recent_post_urls(firefox_profile, username, 0)
        self._play_post_urls(post_urls, progress)
        if post_urls:
            subprocess.Popen(
                ["firefox", url],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            time.sleep(2)
        time.sleep(2)
        # Only accept cache entries touched by this navigation. CDN URLs do
        # not contain the creator handle, so a host-only cache scan can mix
        # the previously opened profile into the new one.
        records = _firefox_cache_image_records(
            firefox_profile,
            page_url=capture_url,
            modified_since=capture_started - 2,
            max_records=3000,
        )
        profile_root = self.root / username
        media_root = profile_root / "media"
        media_root.mkdir(parents=True, exist_ok=True)
        if progress:
            progress(f"Filtering {len(records)} authenticated Firefox assets…")

        candidates = []
        avatar_candidates = []
        poster_ids = set()
        for record in records:
            parsed = urlsplit(record.get("url") or "")
            if parsed.netloc.lower() != "cdn2.onlyfans.com" or "/files/" not in parsed.path:
                continue
            basename = Path(parsed.path).name.lower()
            is_profile_art = basename in {
                "avatar.jpg", "header.jpg", "header_image.jpg"
            }
            suffix = Path(parsed.path).suffix.lower()
            if suffix not in {".jpg", ".jpeg", ".png", ".webp"}:
                continue
            with tempfile.NamedTemporaryFile(suffix=suffix) as temporary:
                temporary.write(record.get("body") or b"")
                temporary.flush()
                try:
                    with Image.open(temporary.name) as image:
                        width, height = image.size
                except OSError:
                    continue
            if "_frame_" in parsed.path.lower():
                poster_ids.add("of_" + hashlib.sha256(record["body"]).hexdigest()[:20])
            if (
                basename == "avatar.jpg"
                and 220 <= width <= 420 and 220 <= height <= 420
            ):
                avatar_candidates.append((record, width, height))
            if is_profile_art:
                continue
            if width >= 500 and height >= 500 and "_frame_" not in parsed.path.lower():
                candidates.append((record, width, height))

        existing_profiles = self.list_profiles()
        existing = next(
            (item for item in existing_profiles if item.get("username") == username),
            {},
        )
        owned_by_other = {
            item.get("id")
            for profile in existing_profiles
            if profile.get("username") != username
            for item in profile.get("media") or []
            if item.get("id")
        }
        old_by_id = {item.get("id"): item for item in existing.get("media") or []}
        items = []
        seen = set()
        for record, width, height in candidates[:ONLYFANS_MAX_POSTS]:
            digest = hashlib.sha256(record["body"]).hexdigest()
            if digest in seen:
                continue
            seen.add(digest)
            item_id = "of_" + digest[:20]
            if item_id in owned_by_other:
                continue
            suffix = ".jpg" if record["content_type"] == "image/jpeg" else Path(
                urlsplit(record["url"]).path
            ).suffix.lower() or ".img"
            target = media_root / f"{item_id}{suffix}"
            if not target.is_file():
                target.write_bytes(record["body"])
            items.append(
                {
                    "id": item_id,
                    "type": "photo",
                    "title": f"Post {len(items) + 1}",
                    "caption": old_by_id.get(item_id, {}).get("caption", ""),
                    "source_path": str(target),
                    "width": width,
                    "height": height,
                }
            )

        # OnlyFans DASH exposes separate signed video and audio variants.
        # Group and mux them into one verified file before adding the post.
        protected_videos = 0
        video_groups = self._recent_video_groups(
            firefox_profile, modified_since=capture_started - 2
        )
        if progress:
            progress(f"Processing {len(video_groups)} captured video manifests…")
        for group_index, group in enumerate(video_groups, 1):
            if group.get("protected"):
                protected_videos += 1
                if progress:
                    progress(
                        "Protected OnlyFans video found · cannot sync encrypted media"
                    )
                continue
            identity = "|".join(sorted(
                value.split("?", 1)[0]
                for key in ("combined", "video", "audio")
                for value in group.get(key, [])
            ))
            video_id = "ofv_" + hashlib.sha256(identity.encode()).hexdigest()[:20]
            if video_id in owned_by_other:
                continue
            target = media_root / f"{video_id}.mp4"
            if not target.is_file() and not self._download_video_group(
                group, target, capture_url, firefox_profile
            ):
                if progress:
                    progress(
                        f"Video {group_index}/{len(video_groups)} could not be "
                        "downloaded with its audio"
                    )
                continue
            if (
                target.stat().st_size < 64 * 1024
                or not {"video", "audio"} <= _probe_stream_types(
                    target, self.config.get("ffprobe_binary", "ffprobe")
                )
                or not _media_decodes(
                    target, self.config.get("ffmpeg_binary", "ffmpeg")
                )
            ):
                target.unlink(missing_ok=True)
                if progress:
                    progress(
                        f"Video {group_index}/{len(video_groups)} failed "
                        "audio/video verification"
                    )
                continue
            items.append(
                {
                    "id": video_id,
                    "type": "video",
                    "title": f"Video {sum(1 for item in items if item['type'] == 'video') + 1}",
                    "caption": old_by_id.get(video_id, {}).get("caption", ""),
                    "source_path": str(target),
                    "width": 0,
                    "height": 0,
                }
            )
            if progress:
                progress(
                    f"Captured video {group_index}/{len(video_groups)} with audio"
                )

        # A refresh is additive. A short browser cache window must never erase
        # older, already-attributed posts that are still present locally.
        captured_ids = {item.get("id") for item in items}
        for old_item in existing.get("media") or []:
            item_id = old_item.get("id")
            source_path = old_item.get("source_path") or ""
            if (
                not item_id or item_id in captured_ids or item_id in owned_by_other
                or item_id in poster_ids or not os.path.isfile(source_path)
            ):
                continue
            items.append(dict(old_item))
            captured_ids.add(item_id)

        if not items:
            if protected_videos:
                raise ValueError(
                    f"Found {protected_videos} protected OnlyFans video"
                    f"{'s' if protected_videos != 1 else ''}, but encrypted media "
                    "cannot be converted for iPod sync."
                )
            raise ValueError(
                "No profile media was found. Open the profile in Firefox, scroll its posts, "
                "then capture again. Play a video once to make it available for sync."
            )
        avatar_source = ""
        if avatar_candidates:
            record = avatar_candidates[0][0]
            avatar_source = str(profile_root / "avatar.jpg")
            Path(avatar_source).write_bytes(record["body"])
        elif items:
            avatar_source = items[0]["source_path"]
        cover_source = next(
            (item["source_path"] for item in items if item["type"] == "photo" and item["width"] > item["height"]),
            items[0]["source_path"],
        )
        profile = {
            "url": url,
            "username": username,
            "display_name": existing.get("display_name") or username,
            "bio": existing.get("bio") or "",
            "avatar_path": avatar_source,
            "cover_path": cover_source,
            "media": items,
            "captured_at": time.strftime("%Y-%m-%d %H:%M:%S"),
            "is_my_profile": bool(existing.get("is_my_profile")),
        }
        payload = self._load()
        payload["profiles"] = [
            item for item in payload["profiles"] if item.get("username") != username
        ] + [profile]
        self._save(payload)
        return {
            "profile": profile,
            "photos": sum(i["type"] == "photo" for i in items),
            "videos": sum(i["type"] == "video" for i in items),
            "protected_videos": protected_videos,
        }

    @staticmethod
    def _manifest_post_map(payload):
        return {
            str(post.get("id") or ""): post
            for post in (payload.get("posts") or [])
            if isinstance(post, dict) and post.get("id") is not None
        }

    def _ofscraper_database(self, username):
        """Find the database whose profile row exactly matches *username*."""
        data_root = self.ofscraper_config_root / "main_profile/.data"
        matches = []
        for candidate in data_root.glob("*/user_data.db"):
            try:
                connection = sqlite3.connect(
                    f"file:{candidate}?immutable=1", uri=True
                )
                try:
                    row = connection.execute(
                        "SELECT username FROM profiles LIMIT 1"
                    ).fetchone()
                finally:
                    connection.close()
            except sqlite3.Error:
                continue
            if row and str(row[0]).lower() == username.lower():
                matches.append(candidate)
        if not matches:
            raise ValueError(
                f"OF-Scraper returned no profile data for @{username}. "
                "Refresh the login and confirm this account can access the "
                "creator, then retry."
            )
        if len(matches) > 1:
            raise ValueError(
                "OF-Scraper did not create an unambiguous database for "
                f"@{username}."
            )
        return matches[0]

    def _ofscraper_database_payload(self, username):
        """Read normal-media metadata without exposing signed links to the UI."""
        database = self._ofscraper_database(username)
        connection = sqlite3.connect(f"file:{database}?immutable=1", uri=True)
        connection.row_factory = sqlite3.Row
        try:
            profile = connection.execute(
                "SELECT user_id, username FROM profiles LIMIT 1"
            ).fetchone()
            posts = [
                {
                    "id": row["post_id"],
                    "text": row["text"] or "",
                    "createdAt": row["created_at"] or "",
                    "isPinned": bool(row["pinned"]),
                }
                for row in connection.execute(
                    "SELECT post_id, text, created_at, pinned FROM posts"
                )
            ]
            media = [dict(row) for row in connection.execute(
                "SELECT media_id AS id, post_id AS postId, link, media_type, "
                "created_at AS createdAt, posted_at AS postedAt "
                "FROM medias WHERE link IS NOT NULL AND unlocked = 1 "
                "AND media_type IN ('Images', 'Videos')"
            )]
        finally:
            connection.close()
        return {
            "username": str(profile["username"]),
            "action": "metadata",
            "userdata": {"id": profile["user_id"]},
            "posts": posts,
            "media": media,
        }

    @staticmethod
    def _ofscraper_download_suffix(media):
        parsed = urlsplit(str(media.get("link") or ""))
        suffix = Path(parsed.path).suffix.lower()
        allowed = (
            ONLYFANS_PHOTO_SUFFIXES
            if media.get("media_type") == "Images"
            else ONLYFANS_VIDEO_SUFFIXES
        )
        return suffix if suffix in allowed else ""

    def _download_ofscraper_media(self, username, payload, progress=None,
                                  cancel_event=None):
        """Download signed, normal CDN URLs atomically and resume by media ID."""
        creator_root = self.ofscraper_download_root / username
        creator_root.mkdir(parents=True, exist_ok=True)
        rows = []
        for media in payload.get("media") or []:
            link = str(media.get("link") or "")
            parsed = urlsplit(link)
            suffix = self._ofscraper_download_suffix(media)
            if (
                parsed.scheme != "https"
                or not parsed.hostname
                or not (
                    parsed.hostname == "onlyfans.com"
                    or parsed.hostname.endswith(".onlyfans.com")
                )
                or not suffix
            ):
                continue
            media_id = str(media.get("id") or "")
            post_id = str(media.get("postId") or "unknown")
            if not media_id.isdigit() or not post_id.isdigit():
                continue
            kind = "Images" if media.get("media_type") == "Images" else "Videos"
            target = creator_root / post_id / kind / f"{media_id}_source{suffix}"
            media["filepath"] = str(target)
            rows.append((media, target))

        pending = [row for row in rows if not row[1].is_file() or row[1].stat().st_size == 0]
        if progress:
            progress(
                f"Found {len(rows)} normal media item(s) · "
                f"{len(rows) - len(pending)} already complete"
            )

        auth = {}
        try:
            auth = json.loads(self.ofscraper_auth_path.read_text(encoding="utf-8"))
        except (OSError, ValueError, TypeError):
            pass

        def download(row):
            media, target = row
            if cancel_event is not None and cancel_event.is_set():
                return "cancelled", target
            request = urllib.request.Request(
                media["link"],
                headers={
                    "User-Agent": str(auth.get("user_agent") or "Mozilla/5.0"),
                    "Accept": "application/json, text/plain, */*",
                    "Referer": "https://onlyfans.com",
                    "user-id": str(auth.get("auth_id") or ""),
                    "x-bc": str(auth.get("x-bc") or ""),
                    "Cookie": (
                        f"auth_id={auth.get('auth_id', '')};"
                        f"sess={auth.get('sess', '')};"
                        f"auth_uid_={auth.get('auth_id', '')}"
                    ),
                },
            )
            target.parent.mkdir(parents=True, exist_ok=True)
            temporary = target.with_name(target.name + ".rockpod-part")
            try:
                with urllib.request.urlopen(request, timeout=45) as response, open(
                    temporary, "wb"
                ) as output:
                    while True:
                        if cancel_event is not None and cancel_event.is_set():
                            temporary.unlink(missing_ok=True)
                            return "cancelled", target
                        chunk = response.read(1024 * 1024)
                        if not chunk:
                            break
                        output.write(chunk)
                if temporary.stat().st_size == 0:
                    raise OSError("empty response")
                os.replace(temporary, target)
                return "downloaded", target
            except urllib.error.HTTPError as exc:
                temporary.unlink(missing_ok=True)
                return ("access_denied" if exc.code == 403 else "failed"), target
            except Exception:
                temporary.unlink(missing_ok=True)
                return "failed", target

        complete = len(rows) - len(pending)
        failed = 0
        access_denied = 0
        if pending:
            with ThreadPoolExecutor(max_workers=4) as executor:
                futures = [executor.submit(download, row) for row in pending]
                for future in as_completed(futures):
                    state, _target = future.result()
                    if state == "cancelled":
                        continue
                    complete += state == "downloaded"
                    failed += state in {"failed", "access_denied"}
                    access_denied += state == "access_denied"
                    processed = complete + failed
                    if progress and (processed % 10 == 0 or processed == len(rows)):
                        progress(
                            f"Downloaded {complete}/{len(rows)} · "
                            f"{failed} failed"
                        )
        if cancel_event is not None and cancel_event.is_set():
            raise ValueError("OnlyFans import cancelled")
        if access_denied and complete == 0:
            warp = Path("/sys/class/net/CloudflareWARP").exists()
            raise ValueError(
                "OnlyFans indexed the full profile, but its IP-bound CDN URLs "
                "returned 403. "
                + (
                    "Cloudflare WARP is active and is using different public "
                    "IPs for OnlyFans and its Amazon CDN. Pause WARP, refresh "
                    "the OnlyFans login, then run Import Full Profile again."
                    if warp else
                    "Make sure the same VPN/proxy route is used for both "
                    "OnlyFans and its CDN, refresh the login, then retry."
                )
            )
        if failed and complete == 0:
            raise ValueError(
                f"Downloaded {complete}/{len(rows)} media item(s); {failed} "
                "signed URL(s) failed. Run the import again to resume."
            )
        payload["download_failures"] = failed
        return payload

    def _ofscraper_files(self, username, payload):
        """Return only files attributed to this exact creator and staging root."""
        creator_root = self.ofscraper_download_root / username
        root_real = os.path.realpath(self.ofscraper_download_root)
        creator_real = os.path.realpath(creator_root)
        if os.path.commonpath([root_real, creator_real]) != root_real:
            raise ValueError("Invalid OF-Scraper creator path")
        rows = []
        seen = set()
        for media in payload.get("media") or []:
            if not isinstance(media, dict):
                continue
            path = media.get("filepath") or media.get("final_path")
            if not path:
                continue
            source = Path(path).expanduser()
            if not source.is_absolute():
                source = self.ofscraper_download_root / source
            source_real = os.path.realpath(source)
            if (
                os.path.commonpath([creator_real, source_real]) != creator_real
                or not os.path.isfile(source_real)
            ):
                continue
            rows.append((Path(source_real), media))
            seen.add(source_real)
        if creator_root.is_dir():
            for source in creator_root.rglob("*"):
                source_real = os.path.realpath(source)
                if not source.is_file() or source_real in seen:
                    continue
                if source.suffix.lower() not in (
                    ONLYFANS_PHOTO_SUFFIXES | ONLYFANS_VIDEO_SUFFIXES
                ):
                    continue
                rows.append((source, {}))
        return rows

    def _ingest_ofscraper_result(self, username, payload, progress=None):
        if str(payload.get("username") or "").lower() != username.lower():
            raise ValueError("OF-Scraper returned media for a different profile")
        posts = self._manifest_post_map(payload)
        userdata = payload.get("userdata") or {}
        existing_profiles = self.list_profiles()
        existing = next(
            (
                item for item in existing_profiles
                if str(item.get("username") or "").lower() == username.lower()
            ),
            {},
        )
        old_by_id = {
            str(item.get("id") or ""): item
            for item in (existing.get("media") or [])
        }
        profile_root = self.root / username
        media_root = profile_root / "media"
        media_root.mkdir(parents=True, exist_ok=True)
        items = []
        skipped_video = 0
        rows = self._ofscraper_files(username, payload)
        if progress:
            progress(f"Verifying {len(rows)} downloaded file(s) for @{username}…")
        for index, (source, metadata) in enumerate(rows, 1):
            suffix = source.suffix.lower()
            media_id = str(metadata.get("id") or metadata.get("media_id") or "")
            if not media_id:
                match = re.match(r"(\d+)_", source.name)
                media_id = match.group(1) if match else ""
            if not media_id:
                media_id = hashlib.sha256(source.read_bytes()).hexdigest()[:20]
            post_id = str(
                metadata.get("postId") or metadata.get("post_id") or ""
            )
            post = posts.get(post_id, {})
            caption = _clean(
                post.get("text") or metadata.get("text") or
                old_by_id.get("of_" + media_id, {}).get("caption") or
                old_by_id.get("ofv_" + media_id, {}).get("caption"),
                180,
            )
            posted = str(
                metadata.get("createdAt") or metadata.get("postedAt") or
                post.get("postedAt") or post.get("createdAt") or ""
            )
            pinned = bool(post.get("isPinned") or post.get("is_pinned"))
            if suffix in ONLYFANS_PHOTO_SUFFIXES:
                try:
                    with Image.open(source) as image:
                        image.verify()
                    with Image.open(source) as image:
                        width, height = image.size
                except OSError:
                    continue
                item_id = "of_" + media_id
                media_type = "photo"
            elif suffix in ONLYFANS_VIDEO_SUFFIXES:
                streams = _probe_stream_types(
                    source, self.config.get("ffprobe_binary", "ffprobe")
                )
                if (
                    not {"video", "audio"} <= streams
                    or not _media_decodes(
                        source, self.config.get("ffmpeg_binary", "ffmpeg")
                    )
                ):
                    skipped_video += 1
                    if progress:
                        progress(
                            f"Skipped video {index}/{len(rows)} · missing or "
                            "unplayable audio"
                        )
                    continue
                item_id = "ofv_" + media_id
                media_type = "video"
                width = height = 0
            else:
                continue
            target = media_root / f"{item_id}{suffix}"
            _copy_if_changed(source, target)
            items.append(
                {
                    "id": item_id,
                    "type": media_type,
                    "title": _clean(
                        ("Pinned · " if pinned else "")
                        + (caption or ("Photo" if media_type == "photo" else "Video")),
                        80,
                    ),
                    "caption": caption,
                    "source_path": str(target),
                    "width": width,
                    "height": height,
                    "post_id": post_id,
                    "posted_at": posted,
                    "pinned": pinned,
                }
            )
        captured_ids = {item["id"] for item in items}
        for item in existing.get("media") or []:
            if item.get("id") not in captured_ids:
                items.append(dict(item))
        items.sort(
            key=lambda item: (
                bool(item.get("pinned")),
                str(item.get("posted_at") or ""),
                str(item.get("id") or ""),
            ),
            reverse=True,
        )
        if not items:
            raise ValueError(
                "OF-Scraper found no downloadable, unprotected media for this "
                "subscribed profile."
            )
        avatar = existing.get("avatar_path") or ""
        cover = existing.get("cover_path") or ""
        photos = [item for item in items if item["type"] == "photo"]
        if not avatar and photos:
            avatar = photos[0]["source_path"]
        if not cover and photos:
            cover = next(
                (
                    item["source_path"] for item in photos
                    if int(item.get("width") or 0) > int(item.get("height") or 0)
                ),
                photos[0]["source_path"],
            )
        profile = {
            "url": f"https://onlyfans.com/{username}",
            "username": username,
            "display_name": _clean(
                userdata.get("name") or userdata.get("displayName") or
                existing.get("display_name") or username,
                80,
            ),
            "bio": _clean(
                userdata.get("about") or userdata.get("bio") or
                existing.get("bio"),
                180,
            ),
            "avatar_path": avatar,
            "cover_path": cover,
            "media": items[:ONLYFANS_MAX_POSTS],
            "captured_at": time.strftime("%Y-%m-%d %H:%M:%S"),
            "is_my_profile": bool(existing.get("is_my_profile")),
        }
        saved = self._load()
        saved["profiles"] = [
            item for item in saved["profiles"]
            if str(item.get("username") or "").lower() != username.lower()
        ] + [profile]
        self._save(saved)
        return {
            "profile": profile,
            "photos": sum(item["type"] == "photo" for item in items),
            "videos": sum(item["type"] == "video" for item in items),
            "skipped_videos": skipped_video,
        }

    def capture_profile(self, url, progress=None, cancel_event=None):
        """Download one exact creator through OF-Scraper's read-only path."""
        username = _profile_slug(url)
        existing = next(
            (
                profile for profile in self._load()["profiles"]
                if str(profile.get("username") or "").lower() == username.lower()
            ),
            None,
        )
        if existing and existing.get("is_my_profile"):
            media = existing.get("media") or []
            return {
                "profile": existing,
                "photos": sum(item.get("type") == "photo" for item in media),
                "videos": sum(item.get("type") == "video" for item in media),
            }
        self._ensure_ofscraper_config()
        if not self.ofscraper_ready():
            raise ValueError(
                "OF-Scraper is not installed. Run rockpod/scripts/"
                "install_ofscraper.sh first."
            )
        if not self.ofscraper_authenticated():
            raise ValueError(
                "Set up the OnlyFans login once, then run the import again."
            )
        command = self._ofscraper_command(username)
        if progress:
            progress(f"Scanning every post for @{username}…")
        started_at = time.monotonic()
        last_elapsed_status = -1
        enumeration_complete = False
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        recent = []
        assert process.stdout is not None
        output_selector = selectors.DefaultSelector()
        output_selector.register(process.stdout, selectors.EVENT_READ)
        try:
            while process.poll() is None:
                if cancel_event is not None and cancel_event.is_set():
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                    raise ValueError("OnlyFans import cancelled")
                events = output_selector.select(timeout=0.25)
                elapsed = int(time.monotonic() - started_at)
                if (
                    progress and not events and elapsed >= 5
                    and elapsed // 5 != last_elapsed_status
                ):
                    last_elapsed_status = elapsed // 5
                    progress(
                        f"Scanning @{username}… {elapsed}s · existing files "
                        "will be skipped"
                    )
                for key, _mask in events:
                    line = key.fileobj.readline()
                    clean = re.sub(
                        r"\x1b\[[0-?]*[ -/]*[@-~]", "", line
                    ).strip()
                    if not clean:
                        continue
                    recent.append(clean)
                    recent = recent[-8:]
                    if _metadata_enumeration_complete(recent):
                        # OF-Scraper has already committed every post/media row.
                        # Its metadata command next performs one remote probe per
                        # item, which RockPod deliberately replaces below.
                        enumeration_complete = True
                        process.terminate()
                        if progress:
                            progress(
                                f"Finished post index for @{username} · "
                                "starting resumable downloads"
                            )
                        break
                    if progress and any(
                        word in clean.lower()
                        for word in (
                            "download", "media", "posts", "processing", "found"
                        )
                    ):
                        progress(_clean(clean, 150))
                if enumeration_complete:
                    break
            return_code = process.wait()
        finally:
            output_selector.close()
            if process.poll() is None:
                process.terminate()
        if any("auth failed" in line.lower() for line in recent):
            raise ValueError(
                "OnlyFans rejected the saved login. Sign in to OnlyFans in "
                "Firefox, refresh the login in RockPod, then retry."
            )
        if return_code != 0 and not enumeration_complete:
            detail = next((line for line in reversed(recent) if line), "")
            raise ValueError(
                "OF-Scraper could not read this subscribed profile"
                + (f": {detail}" if detail else "")
            )
        payload = self._ofscraper_database_payload(username)
        payload = self._download_ofscraper_media(
            username, payload, progress, cancel_event
        )
        report = self._ingest_ofscraper_result(username, payload, progress)
        report["download_failures"] = int(payload.get("download_failures") or 0)
        return report

    def repair_cross_profile_media(self):
        """Remove media hashes already owned by an earlier capture.

        OnlyFans CDN file URLs are creator-opaque. Older RockPod builds could
        consequently append still-warm Firefox assets from profile A to a
        later profile B. Content-derived IDs let us repair that contamination
        without guessing from filenames.
        """
        payload = self._load()
        profiles = payload["profiles"]
        order = sorted(
            range(len(profiles)),
            key=lambda index: (profiles[index].get("captured_at") or "", index),
        )
        owners = {}
        removed = {}
        changed = False
        for index in order:
            profile = profiles[index]
            kept = []
            removed_paths = set()
            for item in profile.get("media") or []:
                item_id = item.get("id")
                if item_id and item_id in owners:
                    removed_paths.add(item.get("source_path") or "")
                    removed[profile.get("username") or ""] = (
                        removed.get(profile.get("username") or "", 0) + 1
                    )
                    changed = True
                    continue
                if item_id:
                    owners[item_id] = profile.get("username") or ""
                kept.append(item)
            profile["media"] = kept
            if kept:
                if profile.get("avatar_path") in removed_paths:
                    profile["avatar_path"] = kept[0].get("source_path") or ""
                if profile.get("cover_path") in removed_paths:
                    cover = next(
                        (
                            item.get("source_path") or ""
                            for item in kept
                            if item.get("type") == "photo"
                            and int(item.get("width") or 0)
                            > int(item.get("height") or 0)
                        ),
                        kept[0].get("source_path") or "",
                    )
                    profile["cover_path"] = cover
        if changed:
            self._save(payload)
        return removed

    def set_device_visibility(self, mount_path, visible):
        mount = validate_device_root(mount_path)
        root = resolve_under_root(mount, ONLYFANS_ROOT)
        os.makedirs(root, exist_ok=True)
        hidden_path = resolve_under_root(mount, ONLYFANS_HIDDEN)
        if visible:
            try:
                os.unlink(hidden_path)
            except FileNotFoundError:
                pass
        else:
            _write_if_changed(hidden_path, "hidden\n")

    def sync(self, mount_path, progress=None, video_profile=None,
             item_ids=None, usernames=None, post_ids=None,
             video_quality="tv"):
        mount = validate_device_root(mount_path)
        if video_quality not in {"tv", "space"}:
            raise ValueError("Choose Standard or Space saver video quality")
        video_target = device_video_target(mount)
        video_profile = app_video_profile(
            self.config, video_profile, source_app="onlyfans"
        )
        video_extension = app_video_extension(video_profile)
        self.repair_cross_profile_media()
        profiles = self.list_profiles()
        if not profiles:
            raise ValueError("Capture an OnlyFans profile before syncing")
        if item_ids is not None or usernames is not None or post_ids is not None:
            selected_ids = {str(value) for value in (item_ids or ())}
            selected_users = {str(value).lower() for value in (usernames or ())}
            selected_posts = {str(value) for value in (post_ids or ())}
            known_ids = {
                str(item.get("id")) for profile in profiles
                for item in (profile.get("media") or []) if item.get("id")
            }
            known_posts = {
                str(item.get("post_id")) for profile in profiles
                for item in (profile.get("media") or []) if item.get("post_id")
            }
            if selected_posts - known_posts:
                raise ValueError("Select an imported OnlyFans post to sync")
            selected_ids.update(
                str(item["id"]) for profile in profiles
                for item in (profile.get("media") or [])
                if str(item.get("post_id")) in selected_posts
            )
            known_users = {str(profile.get("username") or "").lower() for profile in profiles}
            if (not selected_ids and not selected_users
                    or selected_ids - known_ids or selected_users - known_users):
                raise ValueError("Select an imported post or creator to sync")
            existing = present_manifest_ids(
                mount, ONLYFANS_LIBRARY, ("media", "display")
            )
            profiles = [
                {**profile, "media": [
                    (item if (str(item.get("id")) in selected_ids or
                              str(profile.get("username") or "").lower() in selected_users)
                     else {**item, "source_path": ""})
                    for item in (profile.get("media") or [])
                    if (str(item.get("id")) in existing | selected_ids
                        or str(profile.get("username") or "").lower() in selected_users)
                ]}
                for profile in profiles
            ]
            profiles = [profile for profile in profiles if profile["media"] or
                        str(profile.get("username") or "").lower() in selected_users]
        roots = {
            name: resolve_under_root(mount, path)
            for name, path in {
                "root": ONLYFANS_ROOT,
                "media": ONLYFANS_MEDIA_ROOT,
                "thumb": ONLYFANS_THUMB_ROOT,
                "display": ONLYFANS_DISPLAY_ROOT,
                "profile_feed": ONLYFANS_PROFILE_FEED_ROOT,
                "assets": ONLYFANS_ASSET_ROOT,
            }.items()
        }
        for path in roots.values():
            os.makedirs(path, exist_ok=True)
        show_on_ipod = bool(self.config.get("onlyfans_show_on_ipod", True))
        self.set_device_visibility(mount, show_on_ipod)
        report = {"profiles": len(profiles), "photos": 0, "videos": 0, "updated": 0, "unchanged": 0}
        lines = ["id\tusername\ttype\ttitle\tcaption\tmedia\tthumbnail\tdisplay"]
        profile_lines = [
            "username\tdisplay_name\tbio\tavatar\tcover\tposts\tphotos\tvideos\tis_my_profile"
        ]
        preview_lines = ["path"]
        staging = Path(tempfile.mkdtemp(
            prefix=".staging-", dir=roots["root"]
        ))
        sync_index = DeviceSyncIndex(self.config, mount)
        valid_item_ids = set()
        item_total = sum(
            len(profile.get("media") or []) for profile in profiles
        )
        item_ids = {
            str(item.get("id") or "")
            for profile in profiles
            for item in (profile.get("media") or [])
            if item.get("id")
        }
        item_done = 0
        if progress:
            progress(f"Preparing OnlyFans sync · 0/{max(1, item_total)}")
        migrated = _migrate_legacy_display_files(roots["display"], item_ids)
        if progress and migrated:
            progress(
                f"Organized {migrated} existing photo files for FAT storage · "
                f"0/{max(1, item_total)}"
            )
        for profile_index, profile in enumerate(profiles, 1):
            if progress:
                progress(
                    f"Preparing @{profile['username']} · "
                    f"{item_done}/{max(1, item_total)} · profile "
                    f"{profile_index} of {len(profiles)}"
                )
            avatar_device = ""
            cover_device = ""
            preview_source = profile.get("avatar_path") or ""
            for label, source, size in (
                ("avatar", profile.get("avatar_path"), (40, 40)),
                ("cover", profile.get("cover_path"), (320, 72)),
            ):
                target = Path(roots["display"]) / f"{profile['username']}-{label}.bmp"
                profile_id = f"profile:{profile['username']}"
                if source and os.path.isfile(source):
                    valid_item_ids.add(profile_id)
                    signature = (
                        f"onlyfans-{label}-{size[0]}x{size[1]}-v1:"
                        + _media_signature(source)
                    )
                    if not sync_index.current_or_seed(
                        "onlyfans", profile_id, f"{label}-v1", signature,
                        [target], trust_existing=True,
                    ):
                        with Image.open(source) as image:
                            ImageOps.fit(
                                image.convert("RGB"), size,
                                Image.Resampling.LANCZOS,
                            ).save(target, "BMP")
                        sync_index.mark(
                            "onlyfans", profile_id, f"{label}-v1",
                            signature, [target],
                        )
                elif target.is_file():
                    valid_item_ids.add(profile_id)
                else:
                    continue
                device_path = f"/{ONLYFANS_DISPLAY_ROOT}/{target.name}"
                if label == "avatar":
                    avatar_device = device_path
                else:
                    cover_device = device_path
            preview = Path(roots["display"]) / (
                f"{profile['username']}-profile-preview.bmp"
            )
            if preview_source and os.path.isfile(preview_source):
                profile_id = f"profile:{profile['username']}"
                valid_item_ids.add(profile_id)
                preview_signature = (
                    "onlyfans-profile-preview-320x240-v1:"
                    + _media_signature(preview_source)
                )
                if not sync_index.current_or_seed(
                    "onlyfans", profile_id, "profile-preview-v1",
                    preview_signature, [preview], trust_existing=True,
                ):
                    with Image.open(preview_source) as image:
                        ImageOps.fit(
                            image.convert("RGB"), (320, 240),
                            Image.Resampling.LANCZOS,
                        ).save(preview, "BMP")
                    sync_index.mark(
                        "onlyfans", profile_id, "profile-preview-v1",
                        preview_signature, [preview],
                    )
                preview_lines.append(
                    f"/{ONLYFANS_DISPLAY_ROOT}/{preview.name}"
                )
            elif preview.is_file():
                valid_item_ids.add(f"profile:{profile['username']}")
                preview_lines.append(
                    f"/{ONLYFANS_DISPLAY_ROOT}/{preview.name}"
                )
            photo_count = 0
            video_count = 0
            profile_media_lines = [lines[0]]
            for item in profile.get("media") or []:
                current_item = item_done + 1
                source = item.get("source_path") or ""
                item_id = str(item.get("id") or "")
                if not item_id:
                    item_done = current_item
                    continue
                thumb = Path(roots["thumb"]) / f"{item_id}.bmp"
                display = _item_display_path(roots["display"], item_id)
                display.parent.mkdir(exist_ok=True)
                device_display = (
                    f"/{ONLYFANS_DISPLAY_ROOT}/{_display_shard(item_id)}/"
                    f"{item_id}.bmp"
                )
                if not os.path.isfile(source):
                    device_media = ""
                    if item.get("type") == "photo":
                        ready = thumb.is_file() and display.is_file()
                        if ready:
                            photo_count += 1
                            report["photos"] += 1
                    else:
                        selected_target = (
                            Path(roots["media"]) / f"{item_id}{video_extension}"
                        )
                        alternate_target = selected_target.with_suffix(
                            ".mpg" if video_extension == ".m4v" else ".m4v"
                        )
                        target = (
                            selected_target if selected_target.is_file()
                            else alternate_target
                        )
                        ready = target.is_file()
                        if ready:
                            device_media = f"/{ONLYFANS_MEDIA_ROOT}/{target.name}"
                            device_display = ""
                            video_count += 1
                            report["videos"] += 1
                    if not ready:
                        item_done = current_item
                        continue
                    valid_item_ids.add(item_id)
                    report["unchanged"] += 1
                    if progress:
                        progress(
                            f"Keeping device-only {item.get('type') or 'post'} "
                            f"{current_item}/{max(1, item_total)} · "
                            f"@{profile['username']} · "
                            f"{_clean(item.get('title') or item_id, 60)}"
                        )
                    row = "\t".join([
                        item_id, _clean(profile["username"], 64),
                        str(item.get("type") or "photo"),
                        _clean(item.get("title"), 80),
                        _clean(item.get("caption"), 180), device_media,
                        f"/{ONLYFANS_THUMB_ROOT}/{item_id}.bmp",
                        device_display,
                    ])
                    lines.append(row)
                    profile_media_lines.append(row)
                    item_done = current_item
                    continue
                valid_item_ids.add(item_id)
                device_media = ""
                if item["type"] == "photo":
                    photo_signature_path = display.with_suffix(
                        ".photo.source"
                    )
                    photo_signature = (
                        "onlyfans-photo-views-v2:"
                        + _media_signature(source)
                    )
                    photo_outputs = [thumb, display] + [
                        display.with_name(f"{display.stem}.zoom{zoom}.bmp")
                        for zoom in range(1, 4)
                    ]
                    photo_outputs_ready = sync_index.current_or_seed(
                        "onlyfans", item_id, "photo-views-v2",
                        photo_signature, photo_outputs,
                        legacy_marker=photo_signature_path,
                        legacy_signature=photo_signature,
                    )
                    if progress:
                        action = "Up to date" if photo_outputs_ready else "Preparing photo"
                        progress(
                            f"{action} {current_item}/{max(1, item_total)} · "
                            f"@{profile['username']} · "
                            f"{_clean(item.get('title') or item_id, 60)}"
                        )
                    if not photo_outputs_ready:
                        with Image.open(source) as image:
                            rgb = image.convert("RGB")
                            ImageOps.fit(
                                rgb, (96, 72), Image.Resampling.LANCZOS
                            ).save(thumb, "BMP")
                            fitted = ImageOps.contain(
                                rgb, (320, 200), Image.Resampling.LANCZOS
                            )
                            # Keep each viewer level at exactly one 320x200
                            # decode buffer on-device. RockPod does the
                            # expensive resize once during sync; wheel input
                            # loads a prepared level into the same buffer.
                            for zoom, numerator in enumerate((2, 3, 4, 6)):
                                width = max(1, fitted.width * numerator // 2)
                                height = max(1, fitted.height * numerator // 2)
                                scaled = rgb.resize(
                                    (width, height), Image.Resampling.LANCZOS
                                )
                                canvas = Image.new(
                                    "RGB", (320, 200), "black"
                                )
                                canvas.paste(
                                    scaled,
                                    ((320 - width) // 2,
                                     (200 - height) // 2),
                                )
                                zoom_display = (
                                    display if zoom == 0
                                    else display.with_name(
                                        f"{display.stem}.zoom{zoom}.bmp"
                                    )
                                )
                                canvas.save(zoom_display, "BMP")
                                if zoom == 0:
                                    continue
                                centered_x = (320 - width) // 2
                                centered_y = (200 - height) // 2
                                positions = {
                                    "up": (
                                        centered_x,
                                        0 if height > 200 else centered_y,
                                    ),
                                    "right": (
                                        320 - width
                                        if width > 320 else centered_x,
                                        centered_y,
                                    ),
                                    "down": (
                                        centered_x,
                                        200 - height
                                        if height > 200 else centered_y,
                                    ),
                                    "left": (
                                        0 if width > 320 else centered_x,
                                        centered_y,
                                    ),
                                }
                                for direction, position in positions.items():
                                    if position == (centered_x, centered_y):
                                        continue
                                    pan_canvas = Image.new(
                                        "RGB", (320, 200), "black"
                                    )
                                    pan_canvas.paste(scaled, position)
                                    pan_canvas.save(
                                        display.with_name(
                                            f"{display.stem}.zoom{zoom}."
                                            f"{direction}.bmp"
                                        ),
                                        "BMP",
                                    )
                        atomic_write_text(
                            photo_signature_path, photo_signature
                        )
                        sync_index.mark(
                            "onlyfans", item_id, "photo-views-v2",
                            photo_signature, photo_outputs,
                        )
                        report["updated"] += 1
                    else:
                        report["unchanged"] += 1
                    photo_count += 1
                    report["photos"] += 1
                else:
                    target = (
                        Path(roots["media"]) / f"{item_id}{video_extension}"
                    )
                    signature_path = Path(str(target) + ".source")
                    signature = app_video_signature(
                        video_profile, _media_signature(source),
                        "onlyfans-video-320x240-30-v2" +
                        (":space" if video_quality == "space" else ""),
                        video_target,
                    )
                    media_current = sync_index.current_or_seed(
                        "onlyfans", item_id, "video-media-v2", signature,
                        [target], legacy_marker=signature_path,
                        legacy_signature=signature,
                    )
                    thumb_signature = (
                        "onlyfans-video-thumb-96x72-v1:"
                        + _media_signature(source)
                    )
                    thumb_current = sync_index.current_or_seed(
                        "onlyfans", item_id, "video-thumbnail-v1",
                        thumb_signature, [thumb], trust_existing=media_current,
                    )
                    if progress:
                        action = (
                            "Up to date" if media_current and thumb_current
                            else ("Converting video" if not media_current
                                  else "Preparing video thumbnail")
                        )
                        progress(
                            f"{action} {current_item}/{max(1, item_total)} · "
                            f"@{profile['username']} · "
                            f"{_clean(item.get('title') or item_id, 60)}"
                        )
                    if not media_current:
                        staged = staging / target.name
                        stage_app_video(
                            source, staged,
                            config=self.config,
                            profile=video_profile,
                            cache_namespace="onlyfans-video",
                            device_key=item_id,
                            device_target=video_target,
                            title=item.get("title") or item_id,
                            artist=f"@{profile['username']}",
                            mpeg_filter=(
                                "scale=320:240:force_original_aspect_ratio=decrease,"
                                "pad=320:240:(ow-iw)/2:(oh-ih)/2:black,fps=30"
                            ),
                            quality=video_quality,
                        )
                        os.replace(staged, target)
                        atomic_write_text(signature_path, signature)
                        remove_alternate_video(target)
                        sync_index.mark(
                            "onlyfans", item_id, "video-media-v2",
                            signature, [target],
                        )
                        report["updated"] += 1
                    else:
                        report["unchanged"] += 1
                    if not thumb_current:
                        subprocess.run(
                            [self.config.get("ffmpeg_binary", "ffmpeg"), "-hide_banner", "-loglevel", "error", "-y", "-ss", "1", "-i", source, "-frames:v", "1", str(staging / f"{item_id}.jpg")],
                            check=False,
                        )
                        frame = staging / f"{item_id}.jpg"
                        if frame.is_file():
                            with Image.open(frame) as image:
                                ImageOps.fit(image.convert("RGB"), (96, 72), Image.Resampling.LANCZOS).save(thumb, "BMP")
                            frame.unlink()
                            sync_index.mark(
                                "onlyfans", item_id, "video-thumbnail-v1",
                                thumb_signature, [thumb],
                            )
                    device_media = f"/{ONLYFANS_MEDIA_ROOT}/{target.name}"
                    device_display = ""
                    video_count += 1
                    report["videos"] += 1
                row = "\t".join([
                    item_id, _clean(profile["username"], 64), item["type"],
                    _clean(item.get("title"), 80),
                    _clean(item.get("caption"), 180), device_media,
                    f"/{ONLYFANS_THUMB_ROOT}/{item_id}.bmp", device_display,
                ])
                lines.append(row)
                profile_media_lines.append(row)
                item_done = current_item
            profile_lines.append("\t".join([_clean(profile["username"], 64), _clean(profile.get("display_name"), 80), _clean(profile.get("bio"), 180), avatar_device, cover_device, str(photo_count + video_count), str(photo_count), str(video_count), "1" if profile.get("is_my_profile") else "0"]))
            _write_if_changed(
                Path(roots["profile_feed"]) / f"{profile['username']}.tsv",
                "\n".join(profile_media_lines) + "\n",
            )
        _write_if_changed(resolve_under_root(mount, ONLYFANS_LIBRARY), "\n".join(lines) + "\n")
        _write_if_changed(resolve_under_root(mount, ONLYFANS_PROFILES), "\n".join(profile_lines) + "\n")
        _write_if_changed(
            resolve_under_root(mount, ONLYFANS_PREVIEWS),
            "\n".join(preview_lines) + "\n",
        )
        logo = self.repo_root / "assets/ipodjs/rockbox/onlyfans/onlyfans-logo-official.40x40.bmp"
        if _copy_if_changed(logo, Path(roots["assets"]) / "onlyfans-logo.bmp"):
            report["updated"] += 1
        sync_index.prune("onlyfans", valid_item_ids)
        sync_index.close()
        if progress:
            progress(
                f"OnlyFans sync complete · {max(1, item_total)}/"
                f"{max(1, item_total)}"
            )
        shutil.rmtree(staging, ignore_errors=True)
        return report
