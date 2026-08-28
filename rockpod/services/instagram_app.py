"""Incremental Instagram profile import and native iPod application sync."""

from __future__ import annotations

import hashlib
import html
import json
import os
import re
import shutil
import subprocess
import tempfile
import unicodedata
from pathlib import Path
from urllib.request import Request, urlopen
from urllib.parse import urlsplit

from PIL import Image, ImageDraw, ImageFont, ImageOps

from services.android_media import build_ffmpeg_command
from services.device_sync_index import DeviceSyncIndex
from services.file_safety import atomic_write_text
from services.path_safety import resolve_under_root, validate_device_root


INSTAGRAM_ROOT = ".rockbox/instagram"
INSTAGRAM_MEDIA_ROOT = f"{INSTAGRAM_ROOT}/media"
INSTAGRAM_THUMB_ROOT = f"{INSTAGRAM_ROOT}/thumbnails"
INSTAGRAM_FEED_ROOT = f"{INSTAGRAM_ROOT}/feed"
INSTAGRAM_DISPLAY_ROOT = f"{INSTAGRAM_ROOT}/display"
INSTAGRAM_ZOOM_ROOT = f"{INSTAGRAM_ROOT}/zoom"
INSTAGRAM_PROFILE_FEED_ROOT = f"{INSTAGRAM_ROOT}/profile-feed"
INSTAGRAM_ASSET_ROOT = f"{INSTAGRAM_ROOT}/assets"
INSTAGRAM_LIBRARY = f"{INSTAGRAM_ROOT}/library.tsv"
INSTAGRAM_PROFILES = f"{INSTAGRAM_ROOT}/profiles.tsv"
INSTAGRAM_PREVIEWS = f"{INSTAGRAM_ROOT}/previews.tsv"
PHOTO_SUFFIXES = {".jpg", ".jpeg", ".png", ".webp", ".bmp"}
VIDEO_SUFFIXES = {".mp4", ".m4v", ".mov", ".mkv", ".webm", ".avi"}
INSTAGRAM_VIEW_SIZE = (320, 200)
INSTAGRAM_ZOOM_SIZE = (480, 300)
INSTAGRAM_PROFILE_BG = (0xF7, 0xF5, 0xEF)
INSTAGRAM_PROFILE_FONT = "assets/ipodjs/sources/instagram/fonts/NotoSansMath-Regular.ttf"
INSTAGRAM_TULIP = "assets/ipodjs/sources/instagram/emoji/noto-tulip.64.png"


def _clean(value, limit=180):
    return " ".join(str(value or "").replace("\t", " ").split())[:limit]


def _ipod_text(value, limit=180):
    """Flatten styled Unicode and emoji into glyphs Rockbox's UI font has."""
    punctuation = str.maketrans({
        "’": "'", "‘": "'", "“": '"', "”": '"',
        "–": "-", "—": "-", "•": " - ", "…": "...",
    })
    normalized = unicodedata.normalize("NFKD", str(value or "").translate(punctuation))
    ascii_text = normalized.encode("ascii", "ignore").decode("ascii")
    return _clean(ascii_text, limit)


def _post_sort_key(item):
    return str(item.get("post_date") or ""), str(item.get("id") or "")


def _group_media_items(items):
    """Return newest-first carousel members grouped by matching descriptions."""
    groups = []
    by_description = {}
    for source in sorted(items, key=_post_sort_key, reverse=True):
        item = dict(source)
        description = _clean(item.get("caption"), 500)
        # Empty/generic descriptions do not prove that separate media belong
        # to one Instagram carousel.
        key = description.casefold() if description else ""
        if not key:
            groups.append([item])
            continue
        members = by_description.get(key)
        if members is None:
            members = []
            by_description[key] = members
            groups.append(members)
        members.append(item)

    flattened = []
    for members in groups:
        group_id = str(members[0].get("id") or "")
        count = len(members)
        for index, item in enumerate(members, 1):
            item["group_id"] = group_id
            item["group_index"] = index
            item["group_count"] = count
            flattened.append(item)
    return flattened


def _merge_retained_media(discovered, existing, include_photos=True,
                          include_videos=True):
    """Keep imported posts until the user explicitly removes the profile.

    gallery-dl's archive deliberately does not download an already imported
    post again. Consequently, deleting its laptop source must not make a
    later refresh forget the metadata needed to address the converted copy on
    the iPod.
    """
    merged = [dict(item) for item in discovered if isinstance(item, dict)]
    known_ids = {str(item.get("id") or "") for item in merged}
    for old_item in existing or []:
        if not isinstance(old_item, dict):
            continue
        item_id = str(old_item.get("id") or "")
        media_type = str(old_item.get("type") or "")
        if (
            not item_id or item_id in known_ids
            or (media_type == "photo" and not include_photos)
            or (media_type == "video" and not include_videos)
            or media_type not in {"photo", "video"}
        ):
            continue
        merged.append(dict(old_item))
        known_ids.add(item_id)
    return merged


def _sort_library_lines(lines):
    """Sort the combined Home feed newest-first while preserving carousels."""
    if not lines:
        return []
    header, *rows = lines
    rows.sort(key=lambda row: row.split("\t")[9], reverse=True)
    return [header, *rows]


def _save_profile_text_art(font_path, tulip_path, text, target, size,
                           font_size, color, max_lines=1,
                           background=INSTAGRAM_PROFILE_BG):
    """Rasterize exact UTF-8 profile text with real Noto glyph assets."""
    canvas = Image.new("RGB", size, background)
    draw = ImageDraw.Draw(canvas)
    font = ImageFont.truetype(str(font_path), font_size)
    with Image.open(tulip_path) as source:
        tulip_size = max(12, font_size)
        tulip = source.convert("RGBA").resize(
            (tulip_size, tulip_size), Image.Resampling.LANCZOS,
        )

    def text_width(value):
        return sum(
            tulip_size if character == "🌷" else font.getlength(character)
            for character in value
        )

    def draw_line(value, y):
        x = 0.0
        for character in value:
            if character == "🌷":
                canvas.paste(tulip, (round(x), y + 1), tulip)
                x += tulip_size
            else:
                draw.text((round(x), y - 3), character, font=font, fill=color)
                x += font.getlength(character)

    words = str(text or "").split()
    lines = []
    current = ""
    for word in words:
        candidate = f"{current} {word}" if current else word
        if current and text_width(candidate) > size[0]:
            lines.append(current)
            current = word
            if len(lines) >= max_lines:
                break
        else:
            current = candidate
    if current and len(lines) < max_lines:
        lines.append(current)
    line_height = size[1] // max_lines
    for index, line in enumerate(lines[:max_lines]):
        draw_line(line, index * line_height)
    canvas.save(target, "BMP")


def _save_instagram_photo_views(image, display, zoom):
    """Build fixed real-photo frames so device zoom/pan needs no large buffer."""
    display = Path(display)
    rgb = image.convert("RGB")
    canvas = Image.new("RGB", INSTAGRAM_VIEW_SIZE, "black")
    fitted = ImageOps.contain(rgb, INSTAGRAM_VIEW_SIZE, Image.Resampling.LANCZOS)
    canvas.paste(
        fitted,
        ((INSTAGRAM_VIEW_SIZE[0] - fitted.width) // 2,
         (INSTAGRAM_VIEW_SIZE[1] - fitted.height) // 2),
    )
    canvas.save(display, "BMP")

    ImageOps.fit(rgb, INSTAGRAM_ZOOM_SIZE,
                 Image.Resampling.LANCZOS).save(zoom, "BMP")


def _save_instagram_zoom_view(image, target):
    """Atomically add the v3 zoom view while preserving proven v1/v2 views."""
    target = Path(target)
    temporary = target.with_name(target.name + ".rockpod-tmp")
    try:
        ImageOps.fit(
            image.convert("RGB"), INSTAGRAM_ZOOM_SIZE,
            Image.Resampling.LANCZOS,
        ).save(temporary, "BMP")
        os.replace(temporary, target)
    finally:
        try:
            temporary.unlink()
        except FileNotFoundError:
            pass


def _profile_slug(url):
    parsed = urlsplit(str(url or "").strip())
    if parsed.scheme not in {"http", "https"} or parsed.netloc.lower() not in {
        "instagram.com", "www.instagram.com",
    }:
        raise ValueError("Enter an Instagram profile URL")
    slug = parsed.path.strip("/").split("/", 1)[0]
    if not slug or slug in {"p", "reel", "stories", "explore"}:
        raise ValueError("Enter an Instagram profile URL")
    return slug.lower()


def _write_if_changed(path, text):
    path = Path(path)
    try:
        if path.read_text(encoding="utf-8") == text:
            return False
    except OSError:
        pass
    atomic_write_text(path, text)
    return True


def _copy_if_changed(source, target):
    source, target = Path(source), Path(target)
    if target.is_file() and hashlib.sha256(source.read_bytes()).digest() == hashlib.sha256(
        target.read_bytes()
    ).digest():
        return False
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = target.with_name(target.name + ".rockpod-tmp")
    shutil.copy2(source, temporary)
    os.replace(temporary, target)
    return True


def _signature(path):
    stat = os.stat(path)
    return f"{stat.st_size}:{stat.st_mtime_ns}"


def _instagram_count(value):
    text = str(value or "").strip().upper().replace(",", "")
    multiplier = 1
    if text.endswith("K"):
        multiplier, text = 1_000, text[:-1]
    elif text.endswith("M"):
        multiplier, text = 1_000_000, text[:-1]
    elif text.endswith("B"):
        multiplier, text = 1_000_000_000, text[:-1]
    try:
        return int(float(text) * multiplier)
    except ValueError:
        return 0


def _parse_instagram_description(page):
    """Parse Instagram's locale-pinned public SEO profile summary."""
    text = str(page or "")
    tag = re.search(
        r"<meta\b[^>]*\bname=(?:\"description\"|'description')[^>]*>",
        text,
        re.IGNORECASE,
    )
    if not tag:
        return {}
    match = re.search(r'\bcontent="([^"]*)"', tag.group(0), re.IGNORECASE)
    if not match:
        match = re.search(r"\bcontent='([^']*)'", tag.group(0), re.IGNORECASE)
    if not match:
        return {}
    description = html.unescape(match.group(1))
    counts = re.search(
        r"([\d.,]+[KMB]?)\s+Followers,\s*"
        r"([\d.,]+[KMB]?)\s+Following,\s*"
        r"([\d.,]+[KMB]?)\s+Posts",
        description,
        re.IGNORECASE,
    )
    if not counts:
        return {}
    result = {
        "follower_count": _instagram_count(counts.group(1)),
        "following_count": _instagram_count(counts.group(2)),
        "post_count": _instagram_count(counts.group(3)),
    }
    bio = re.search(
        r"\bon Instagram:\s*[\"“](.*?)[\"”]\s*$",
        description,
        re.IGNORECASE,
    )
    if bio:
        result["bio"] = _clean(bio.group(1), 500)
    return result


class InstagramAppService:
    """Use gallery-dl's maintained Instagram extractor; never click the site."""

    def __init__(self, config, repo_root):
        self.config = config
        self.repo_root = Path(repo_root)
        self.root = Path(config.get("cache_dir")) / "instagram"
        self.index_path = self.root / "library.json"
        self.root.mkdir(parents=True, exist_ok=True)

    @property
    def gallery_dl(self):
        configured = str(self.config.get("gallery_dl_binary", "") or "")
        if configured:
            return Path(configured).expanduser()
        return self.repo_root / "rockpod/.venv/bin/gallery-dl"

    def _load(self):
        try:
            payload = json.loads(self.index_path.read_text(encoding="utf-8"))
        except (OSError, ValueError, TypeError):
            payload = {"profiles": []}
        if not isinstance(payload, dict) or not isinstance(payload.get("profiles"), list):
            return {"profiles": []}
        return payload

    def _save(self, payload):
        atomic_write_text(self.index_path, json.dumps(payload, indent=2, ensure_ascii=False) + "\n")

    def list_profiles(self):
        return sorted(self._load()["profiles"], key=lambda row: row.get("username", ""))

    def remove_profile(self, username):
        payload = self._load()
        payload["profiles"] = [
            row for row in payload["profiles"]
            if row.get("username", "").lower() != str(username).lower()
        ]
        self._save(payload)

    def _run_json(self, url):
        command = [
            str(self.gallery_dl), "--cookies-from-browser", "firefox",
            "--dump-json", url,
        ]
        try:
            result = subprocess.run(
                command, check=True, capture_output=True, text=True, timeout=180,
            )
            messages = json.loads(result.stdout)
        except (OSError, ValueError, subprocess.SubprocessError) as exc:
            detail = getattr(exc, "stderr", "") or str(exc)
            raise ValueError(f"Could not read Instagram profile: {_clean(detail, 300)}") from exc
        return [row[1] for row in messages if row and row[0] == 2 and isinstance(row[1], dict)]

    def _profile_page_metadata(self, username):
        """Read real public counts from Instagram's own SEO metadata."""
        try:
            from gallery_dl.cookies import load_cookies

            jar = load_cookies(("firefox",))
            cookie = "; ".join(
                f"{item.name}={item.value}" for item in jar
                if str(item.domain or "").endswith("instagram.com")
            )
            request = Request(
                f"https://www.instagram.com/{username}/?hl=en",
                headers={
                    # Instagram's SEO response (used by search crawlers and
                    # link previews) carries the public counts. A generic
                    # browser UA reliably receives that representation.
                    "User-Agent": "Mozilla/5.0",
                    "Accept-Language": "en-US,en;q=0.9",
                    "Cookie": cookie,
                },
            )
            with urlopen(request, timeout=30) as response:
                page = response.read(2 * 1024 * 1024).decode("utf-8", "replace")
            return _parse_instagram_description(page)
        except (OSError, ValueError, ImportError):
            return {}

    def import_profile(self, url, include_photos=True, include_videos=True, progress=None):
        username = _profile_slug(url)
        if not self.gallery_dl.is_file():
            raise ValueError("gallery-dl is not installed in RockPod's environment")
        progress = progress or (lambda _message: None)
        progress(f"Reading @{username} profile metadata…")
        info_rows = self._run_json(f"https://www.instagram.com/{username}/info/")
        info = info_rows[0] if info_rows else {}
        public_counts = self._profile_page_metadata(username)
        profile_root = self.root / username
        media_root = profile_root / "media"
        media_root.mkdir(parents=True, exist_ok=True)
        archive = profile_root / "gallery-dl.sqlite3"
        command = [
            str(self.gallery_dl), "--cookies-from-browser", "firefox",
            "--download-archive", str(archive), "--write-metadata",
            "--no-mtime", "--directory", str(media_root),
            "--filename", "{post_id}_{num:>02}_{media_id}.{extension}",
        ]
        filters = []
        if not include_photos:
            filters.append("extension not in ('jpg','jpeg','png','webp')")
        if not include_videos:
            filters.append("extension not in ('mp4','mov','m4v','webm')")
        if filters:
            command += ["--filter", " and ".join(filters)]
        targets = [f"https://www.instagram.com/{username}/posts/"]
        if include_videos:
            targets.append(f"https://www.instagram.com/{username}/reels/")
        command += targets
        progress(f"Downloading new posts from @{username} · existing media will be skipped…")
        process = subprocess.Popen(
            command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, bufsize=1,
        )
        assert process.stdout is not None
        for line in process.stdout:
            line = _clean(line, 180)
            if line:
                progress(line)
        if process.wait() != 0:
            raise ValueError("Instagram download failed; refresh the Firefox login and retry")

        existing = next((p for p in self.list_profiles() if p.get("username") == username), {})
        items = []
        for media in sorted(media_root.iterdir(), key=lambda path: path.stat().st_mtime_ns, reverse=True):
            if not media.is_file() or media.suffix.lower() not in PHOTO_SUFFIXES | VIDEO_SUFFIXES:
                continue
            metadata_path = media.with_name(media.name + ".json")
            if not metadata_path.is_file():
                metadata_path = media.with_suffix(media.suffix + ".json")
            try:
                metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
            except (OSError, ValueError):
                metadata = {}
            media_type = "video" if media.suffix.lower() in VIDEO_SUFFIXES else "photo"
            if media_type == "photo" and not include_photos:
                continue
            if media_type == "video" and not include_videos:
                continue
            item_id = _clean(metadata.get("media_id") or media.stem, 80)
            items.append({
                "id": "ig_" + item_id,
                "type": media_type,
                "title": _clean(metadata.get("description") or "Instagram Post", 80),
                "caption": _clean(metadata.get("description"), 500),
                "source_path": str(media),
                "post_url": _clean(metadata.get("post_url"), 500),
                "post_date": _clean(metadata.get("post_date") or metadata.get("date"), 32),
                "likes": int(metadata.get("likes") or 0),
            })
        items = _merge_retained_media(
            items, existing.get("media") or [],
            include_photos=include_photos,
            include_videos=include_videos,
        )
        if not items:
            raise ValueError("Instagram returned no photos or videos for this profile")

        avatar = profile_root / "avatar.jpg"
        avatar_url = str(info.get("profile_pic_url") or "")
        if avatar_url and not avatar.is_file():
            avatar_command = [
                str(self.gallery_dl), "--cookies-from-browser", "firefox",
                "--directory", str(profile_root),
                "--filename", "avatar.{extension}",
                f"https://www.instagram.com/{username}/avatar/",
            ]
            subprocess.run(avatar_command, check=False, timeout=180)
            found = next(profile_root.glob("avatar.*"), None)
            if found and found != avatar:
                shutil.copy2(found, avatar)
        items.sort(key=_post_sort_key, reverse=True)
        profile = {
            "url": f"https://www.instagram.com/{username}/",
            "username": username,
            "display_name": _clean(info.get("full_name") or existing.get("display_name") or username, 100),
            "bio": _clean(
                public_counts.get("bio") or info.get("biography")
                or existing.get("bio"),
                500,
            ),
            "avatar_path": (
                str(avatar) if avatar.is_file()
                else existing.get("avatar_path") or items[0]["source_path"]
            ),
            "is_private": bool(info.get("is_private")),
            "is_verified": bool(info.get("is_verified")),
            "follower_count": int(
                public_counts.get("follower_count")
                or info.get("follower_count") or info.get("count_followed") or 0
            ),
            "following_count": int(
                public_counts.get("following_count")
                or info.get("following_count") or info.get("count_follow") or 0
            ),
            "post_count": int(public_counts.get("post_count") or len(items)),
            "media": items,
        }
        payload = self._load()
        payload["profiles"] = [p for p in payload["profiles"] if p.get("username") != username] + [profile]
        self._save(payload)
        return {"profile": profile, "photos": sum(i["type"] == "photo" for i in items), "videos": sum(i["type"] == "video" for i in items)}

    def sync(self, mount_path, progress=None):
        mount = validate_device_root(mount_path)
        profiles = self.list_profiles()
        if not profiles:
            raise ValueError("Import an Instagram profile before syncing")
        roots = {
            key: resolve_under_root(mount, value) for key, value in {
                "root": INSTAGRAM_ROOT, "media": INSTAGRAM_MEDIA_ROOT,
                "thumb": INSTAGRAM_THUMB_ROOT, "display": INSTAGRAM_DISPLAY_ROOT,
                "zoom": INSTAGRAM_ZOOM_ROOT, "feed": INSTAGRAM_FEED_ROOT,
                "profile_feed": INSTAGRAM_PROFILE_FEED_ROOT,
                "assets": INSTAGRAM_ASSET_ROOT,
            }.items()
        }
        for path in roots.values():
            os.makedirs(path, exist_ok=True)
        staging = Path(tempfile.mkdtemp(prefix=".staging-", dir=roots["root"]))
        sync_index = DeviceSyncIndex(self.config, mount)
        report = {"profiles": len(profiles), "photos": 0, "videos": 0, "updated": 0, "unchanged": 0}
        lines = [
            "id\tusername\ttype\ttitle\tcaption\tmedia\tthumbnail\t"
            "display\tlikes\tpost_date\tfeed\tgroup_id\tgroup_index\tgroup_count\t"
            "autoplay"
        ]
        profile_lines = [
            "username\tdisplay_name\tbio\tavatar\tcover\tposts\tphotos\tvideos\t"
            "is_my_profile\tfollowers\tfollowing\tverified\tname_art\tbio_art\t"
            "synced_posts"
        ]
        previews = ["path"]
        total = sum(len(p.get("media") or []) for p in profiles)
        done = 0
        valid_item_ids = set()
        profile_font = self.repo_root / INSTAGRAM_PROFILE_FONT
        tulip_art = self.repo_root / INSTAGRAM_TULIP
        for profile in profiles:
            profile_rows = [lines[0]]
            profile_id = f"profile:{profile['username']}"
            valid_item_ids.add(profile_id)
            avatar_device = ""
            avatar = profile.get("avatar_path") or ""
            avatar_target = Path(roots["display"]) / (
                f"{profile['username']}-avatar.bmp"
            )
            preview = Path(roots["display"]) / (
                f"{profile['username']}-profile-preview.bmp"
            )
            if os.path.isfile(avatar):
                avatar_signature = _signature(avatar) + ":instagram-avatar-40-v1"
                if not sync_index.current_or_seed(
                    "instagram", profile_id, "avatar-v1", avatar_signature,
                    [avatar_target], trust_existing=True,
                ):
                    with Image.open(avatar) as image:
                        ImageOps.fit(image.convert("RGB"), (40, 40), Image.Resampling.LANCZOS).save(avatar_target, "BMP")
                    sync_index.mark("instagram", profile_id, "avatar-v1", avatar_signature, [avatar_target])
                preview_signature = _signature(avatar) + ":instagram-profile-preview-320-v1"
                if not sync_index.current_or_seed(
                    "instagram", profile_id, "profile-preview-v1", preview_signature,
                    [preview], trust_existing=True,
                ):
                    with Image.open(avatar) as image:
                        ImageOps.fit(image.convert("RGB"), (320, 240), Image.Resampling.LANCZOS).save(preview, "BMP")
                    sync_index.mark("instagram", profile_id, "profile-preview-v1", preview_signature, [preview])
            if avatar_target.is_file():
                avatar_device = f"/{INSTAGRAM_DISPLAY_ROOT}/{avatar_target.name}"
            if preview.is_file():
                previews.append(f"/{INSTAGRAM_DISPLAY_ROOT}/{preview.name}")
            photos = videos = 0
            media_items = _group_media_items(profile.get("media") or [])
            for item in media_items:
                done += 1
                item_id = str(item.get("id") or "")
                if not item_id:
                    continue
                source = item.get("source_path") or ""
                thumb = Path(roots["thumb"]) / f"{item_id}.bmp"
                feed = Path(roots["feed"]) / f"{item_id}.bmp"
                display = Path(roots["display"]) / f"{item_id}.bmp"
                zoom = Path(roots["zoom"]) / f"{item_id}.zoom1.bmp"
                media_device = ""
                autoplay_device = ""
                display_device = ""
                feed_device = ""
                if not os.path.isfile(source):
                    if item.get("type") == "photo":
                        ready = (
                            thumb.is_file() and feed.is_file()
                            and display.is_file()
                        )
                        if ready:
                            display_device = (
                                f"/{INSTAGRAM_DISPLAY_ROOT}/{display.name}"
                            )
                            feed_device = f"/{INSTAGRAM_FEED_ROOT}/{feed.name}"
                            photos += 1
                            report["photos"] += 1
                    else:
                        target = Path(roots["media"]) / f"{item_id}.mpg"
                        ready = target.is_file()
                        if ready:
                            media_device = (
                                f"/{INSTAGRAM_MEDIA_ROOT}/{target.name}"
                            )
                            autoplay_device = media_device
                            if feed.is_file():
                                feed_device = (
                                    f"/{INSTAGRAM_FEED_ROOT}/{feed.name}"
                                )
                            metadata = "\n".join([
                                f"group_id={_ipod_text(item.get('group_id') or item_id, 32)}",
                                f"username={_ipod_text(profile['username'], 64)}",
                                f"title={_ipod_text(item.get('title'), 80)}",
                                f"likes={int(item.get('likes') or 0)}",
                            ]) + "\n"
                            _write_if_changed(
                                target.with_suffix(".igm"), metadata,
                            )
                            videos += 1
                            report["videos"] += 1
                    if not ready:
                        continue
                    valid_item_ids.add(item_id)
                    report["unchanged"] += 1
                    if progress:
                        progress(
                            f"Keeping device-only {item.get('type') or 'post'} "
                            f"{done}/{max(1, total)} · "
                            f"@{profile['username']} · "
                            f"{_clean(item.get('title') or item_id, 50)}"
                        )
                    row_line = "\t".join([
                        item_id, _ipod_text(profile["username"], 64),
                        str(item.get("type") or "photo"),
                        _ipod_text(item.get("title"), 80),
                        _ipod_text(item.get("caption"), 180), media_device,
                        f"/{INSTAGRAM_THUMB_ROOT}/{thumb.name}",
                        display_device, str(item.get("likes") or 0),
                        _ipod_text(item.get("post_date"), 32), feed_device,
                        _ipod_text(item.get("group_id") or item_id, 32),
                        str(item.get("group_index") or 1),
                        str(item.get("group_count") or 1),
                        autoplay_device,
                    ])
                    lines.append(row_line)
                    profile_rows.append(row_line)
                    continue
                valid_item_ids.add(item_id)
                if progress:
                    progress(
                        f"Instagram {done}/{max(1, total)} · "
                        f"@{profile['username']} · "
                        f"{_clean(item.get('title'), 50)}"
                    )
                signature = _signature(source)
                if item["type"] == "photo":
                    marker = zoom.with_suffix(".photo.source")
                    legacy_marker = display.with_suffix(".photo.source")
                    legacy_zoom = display.with_name(f"{display.stem}.zoom1.bmp")
                    legacy_temporary = legacy_zoom.with_name(
                        legacy_zoom.name + ".rockpod-tmp"
                    )
                    if not zoom.is_file():
                        for candidate in (legacy_zoom, legacy_temporary):
                            if candidate.is_file():
                                os.replace(candidate, zoom)
                                break
                    elif legacy_temporary.is_file():
                        legacy_temporary.unlink()
                    proof_marker = marker if marker.is_file() else legacy_marker
                    photo_targets = [display, thumb, feed, zoom]
                    photo_signature = signature + ":instagram-photo-views-v3"
                    current = sync_index.current_or_seed(
                        "instagram", item_id, "photo-views-v3", photo_signature,
                        photo_targets, legacy_marker=proof_marker,
                        legacy_signature=photo_signature,
                    )
                    if not current:
                        with Image.open(source) as image:
                            rgb = image.convert("RGB")
                            if display.is_file() and thumb.is_file() and feed.is_file():
                                _save_instagram_zoom_view(rgb, zoom)
                            else:
                                # Instagram 1.0 was built around square photos.
                                # Keep that unmistakable 2010 feed geometry on iPod.
                                ImageOps.fit(rgb, (72, 72), Image.Resampling.LANCZOS).save(thumb, "BMP")
                                ImageOps.fit(rgb, (160, 160), Image.Resampling.LANCZOS).save(feed, "BMP")
                                _save_instagram_photo_views(rgb, display, zoom)
                        atomic_write_text(marker, photo_signature)
                        sync_index.mark(
                            "instagram", item_id, "photo-views-v3",
                            photo_signature, photo_targets,
                        )
                        report["updated"] += 1
                    else:
                        report["unchanged"] += 1
                    display_device = f"/{INSTAGRAM_DISPLAY_ROOT}/{display.name}"
                    feed_device = f"/{INSTAGRAM_FEED_ROOT}/{feed.name}"
                    photos += 1; report["photos"] += 1
                else:
                    target = Path(roots["media"]) / f"{item_id}.mpg"
                    marker = target.with_suffix(".mpg.source")
                    media_signature = signature + ":instagram-mpeg2-320x240-30-v1"
                    current = sync_index.current_or_seed(
                        "instagram", item_id, "video-media-v1", media_signature,
                        [target], legacy_marker=marker, legacy_signature=signature,
                    )
                    if not current:
                        staged = staging / target.name
                        command = build_ffmpeg_command(source, str(staged), "video", ffmpeg_path=self.config.get("ffmpeg_binary", "ffmpeg"))
                        command[command.index("-vf") + 1] = "scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2:black,fps=30"
                        subprocess.run(command, check=True)
                        os.replace(staged, target)
                        atomic_write_text(marker, signature)
                        sync_index.mark(
                            "instagram", item_id, "video-media-v1",
                            media_signature, [target],
                        )
                        report["updated"] += 1
                    else:
                        report["unchanged"] += 1
                    thumb_signature = signature + ":instagram-video-previews-v2"
                    if not sync_index.current_or_seed(
                        "instagram", item_id, "video-previews-v2",
                        thumb_signature, [thumb, feed], trust_existing=True,
                    ):
                        frame = staging / f"{item_id}.jpg"
                        subprocess.run([self.config.get("ffmpeg_binary", "ffmpeg"), "-hide_banner", "-loglevel", "error", "-y", "-ss", "1", "-i", source, "-frames:v", "1", str(frame)], check=False)
                        if frame.is_file():
                            with Image.open(frame) as image:
                                rgb = image.convert("RGB")
                                ImageOps.fit(rgb, (72, 72), Image.Resampling.LANCZOS).save(thumb, "BMP")
                                ImageOps.fit(rgb, (160, 160), Image.Resampling.LANCZOS).save(feed, "BMP")
                            sync_index.mark(
                                "instagram", item_id, "video-previews-v2",
                                thumb_signature, [thumb, feed],
                            )
                    media_device = f"/{INSTAGRAM_MEDIA_ROOT}/{target.name}"
                    # mpegplayer now scales this original into the feed card,
                    # so no second autoplay video is stored or converted.
                    autoplay_device = media_device
                    feed_device = f"/{INSTAGRAM_FEED_ROOT}/{feed.name}"
                    metadata = "\n".join([
                        f"group_id={_ipod_text(item.get('group_id') or item_id, 32)}",
                        f"username={_ipod_text(profile['username'], 64)}",
                        f"title={_ipod_text(item.get('title'), 80)}",
                        f"likes={int(item.get('likes') or 0)}",
                    ]) + "\n"
                    _write_if_changed(target.with_suffix(".igm"), metadata)
                    videos += 1; report["videos"] += 1
                row_line = "\t".join([
                    item_id, _ipod_text(profile["username"], 64), item["type"],
                    _ipod_text(item.get("title"), 80),
                    _ipod_text(item.get("caption"), 180), media_device,
                    f"/{INSTAGRAM_THUMB_ROOT}/{thumb.name}", display_device,
                    str(item.get("likes") or 0),
                    _ipod_text(item.get("post_date"), 32), feed_device,
                    _ipod_text(item.get("group_id") or item_id, 32),
                    str(item.get("group_index") or 1),
                    str(item.get("group_count") or 1),
                    autoplay_device,
                ])
                lines.append(row_line)
                profile_rows.append(row_line)
            profile_rows = _sort_library_lines(profile_rows)
            _write_if_changed(
                Path(roots["profile_feed"]) / f"{profile['username']}.tsv",
                "\n".join(profile_rows) + "\n",
            )
            name_target = Path(roots["display"]) / f"{profile['username']}-name.bmp"
            bio_target = Path(roots["display"]) / f"{profile['username']}-bio.bmp"
            staged_name = staging / name_target.name
            staged_bio = staging / bio_target.name
            text_asset_signature = (
                f"{_signature(profile_font)}:{_signature(tulip_art)}"
            )
            display_name = str(
                profile.get("display_name") or profile["username"]
            )
            name_signature = hashlib.sha256(
                (
                    "instagram-profile-name-205x18-14-transparent-v2\0"
                    + text_asset_signature + "\0" + display_name
                ).encode("utf-8")
            ).hexdigest()
            if not sync_index.current_or_seed(
                "instagram", profile_id, "name-art-v1", name_signature,
                [name_target], trust_existing=False,
            ):
                _save_profile_text_art(
                    profile_font, tulip_art, display_name,
                    staged_name, (205, 18), 14, (0, 0, 0),
                    background=(255, 0, 255),
                )
                os.replace(staged_name, name_target)
                sync_index.mark(
                    "instagram", profile_id, "name-art-v1",
                    name_signature, [name_target],
                )
                report["updated"] += 1
            else:
                report["unchanged"] += 1
            bio = str(profile.get("bio") or "")
            bio_signature = hashlib.sha256(
                (
                    "instagram-profile-bio-304x30-11-v1\0"
                    + text_asset_signature + "\0" + bio
                ).encode("utf-8")
            ).hexdigest()
            if not sync_index.current_or_seed(
                "instagram", profile_id, "bio-art-v1", bio_signature,
                [bio_target], trust_existing=False,
            ):
                _save_profile_text_art(
                    profile_font, tulip_art, bio,
                    staged_bio, (304, 30), 11,
                    (0x76, 0x76, 0x76), max_lines=2,
                )
                os.replace(staged_bio, bio_target)
                sync_index.mark(
                    "instagram", profile_id, "bio-art-v1",
                    bio_signature, [bio_target],
                )
                report["updated"] += 1
            else:
                report["unchanged"] += 1
            synced_posts = sum(
                int(row.split("\t")[12]) == 1 for row in profile_rows[1:]
            )
            profile_lines.append("\t".join([
                _ipod_text(profile["username"], 64),
                _clean(profile.get("display_name"), 80),
                _clean(profile.get("bio"), 180), avatar_device, "",
                str(profile.get("post_count") or photos + videos), str(photos),
                str(videos), "0", str(profile.get("follower_count") or 0),
                str(profile.get("following_count") or 0),
                "1" if profile.get("is_verified") else "0",
                f"/{INSTAGRAM_DISPLAY_ROOT}/{name_target.name}",
                f"/{INSTAGRAM_DISPLAY_ROOT}/{bio_target.name}",
                str(synced_posts),
            ]))
        lines = _sort_library_lines(lines)
        _write_if_changed(resolve_under_root(mount, INSTAGRAM_LIBRARY), "\n".join(lines) + "\n")
        _write_if_changed(resolve_under_root(mount, INSTAGRAM_PROFILES), "\n".join(profile_lines) + "\n")
        _write_if_changed(resolve_under_root(mount, INSTAGRAM_PREVIEWS), "\n".join(previews) + "\n")
        logo = self.repo_root / "assets/ipodjs/rockbox/instagram/instagram-logo-official.40x40.bmp"
        if _copy_if_changed(logo, Path(roots["assets"]) / "instagram-logo.bmp"):
            report["updated"] += 1
        launch = self.repo_root / "assets/ipodjs/rockbox/instagram/instagram-launch-2010.bmp"
        if _copy_if_changed(launch, Path(roots["assets"]) / "instagram-launch-2010.bmp"):
            report["updated"] += 1
        sync_index.prune("instagram", valid_item_ids)
        sync_index.close()
        shutil.rmtree(staging, ignore_errors=True)
        return report
