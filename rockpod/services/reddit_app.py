"""Incremental Reddit subreddit import and native iPod application sync."""

from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from urllib.parse import urlsplit

from PIL import Image, ImageOps

from services.android_media import build_ffmpeg_command
from services.device_sync_index import DeviceSyncIndex
from services.file_safety import atomic_write_text
from services.path_safety import resolve_under_root, validate_device_root


REDDIT_ROOT = ".rockbox/reddit"
PHOTO_SUFFIXES = {".jpg", ".jpeg", ".png", ".webp", ".bmp"}
VIDEO_SUFFIXES = {".mp4", ".m4v", ".mov", ".mkv", ".webm"}


def _clean(value, limit=180):
    return " ".join(str(value or "").replace("\t", " ").split())[:limit]


def _subreddit_slug(url):
    parsed = urlsplit(str(url or "").strip())
    if parsed.scheme not in {"http", "https"} or parsed.netloc.lower() not in {
        "reddit.com", "www.reddit.com", "old.reddit.com",
    }:
        raise ValueError("Enter a Reddit subreddit URL")
    match = re.match(r"^/r/([^/]+)", parsed.path, re.IGNORECASE)
    if not match or not re.fullmatch(r"[A-Za-z0-9_]{2,21}", match.group(1)):
        raise ValueError("Enter a Reddit subreddit URL such as https://www.reddit.com/r/ipod/")
    return match.group(1).lower()


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
    if target.is_file() and hashlib.sha256(source.read_bytes()).digest() == hashlib.sha256(target.read_bytes()).digest():
        return False
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = target.with_name(target.name + ".rockpod-tmp")
    shutil.copy2(source, temporary)
    os.replace(temporary, target)
    return True


def _signature(path):
    stat = os.stat(path)
    return f"{stat.st_size}:{stat.st_mtime_ns}"


class RedditAppService:
    """Offline subreddit reader backed by gallery-dl's maintained Reddit extractor."""

    def __init__(self, config, repo_root):
        self.config = config
        self.repo_root = Path(repo_root)
        self.root = Path(config.get("cache_dir")) / "reddit"
        self.index_path = self.root / "library.json"
        self.root.mkdir(parents=True, exist_ok=True)

    @property
    def gallery_dl(self):
        configured = str(self.config.get("gallery_dl_binary", "") or "")
        return Path(configured).expanduser() if configured else self.repo_root / "rockpod/.venv/bin/gallery-dl"

    def _load(self):
        try:
            payload = json.loads(self.index_path.read_text(encoding="utf-8"))
        except (OSError, ValueError, TypeError):
            payload = {"subreddits": []}
        if not isinstance(payload, dict) or not isinstance(payload.get("subreddits"), list):
            return {"subreddits": []}
        return payload

    def _save(self, payload):
        atomic_write_text(self.index_path, json.dumps(payload, indent=2, ensure_ascii=False) + "\n")

    def list_subreddits(self):
        return sorted(self._load()["subreddits"], key=lambda row: row.get("name", ""))

    def remove_subreddit(self, name):
        payload = self._load()
        payload["subreddits"] = [row for row in payload["subreddits"] if row.get("name", "").lower() != str(name).lower()]
        self._save(payload)

    def _run_json(self, url, limit):
        command = [
            str(self.gallery_dl), "--cookies-from-browser", "firefox",
            "--range", f"1-{int(limit)}", "--dump-json", url,
        ]
        try:
            result = subprocess.run(command, check=True, capture_output=True, text=True, timeout=240)
            messages = json.loads(result.stdout)
        except (OSError, ValueError, subprocess.SubprocessError) as exc:
            detail = getattr(exc, "stderr", "") or str(exc)
            raise ValueError(f"Could not read Reddit: {_clean(detail, 320)}") from exc
        posts = {}
        for row in messages:
            if not row or row[0] not in {2, 3}:
                continue
            data = row[1] if row[0] == 2 else (row[2] if len(row) > 2 else {})
            if isinstance(data, dict) and data.get("id"):
                posts[str(data["id"])] = data
        return list(posts.values())

    def import_subreddit(self, url, limit=25, include_media=True, progress=None):
        name = _subreddit_slug(url)
        limit = max(5, min(100, int(limit)))
        if not self.gallery_dl.is_file():
            raise ValueError("gallery-dl is not installed in RockPod's environment")
        progress = progress or (lambda _message: None)
        canonical = f"https://www.reddit.com/r/{name}/"
        progress(f"Reading r/{name} through your Firefox Reddit session…")
        rows = self._run_json(canonical, limit)
        if not rows:
            raise ValueError(f"Reddit returned no posts for r/{name}")

        community_root = self.root / name
        media_root = community_root / "media"
        media_root.mkdir(parents=True, exist_ok=True)
        if include_media:
            command = [
                str(self.gallery_dl), "--cookies-from-browser", "firefox",
                "--range", f"1-{limit}", "--download-archive", str(community_root / "gallery-dl.sqlite3"),
                "--write-metadata", "--no-mtime", "--directory", str(media_root),
                "--filename", "{id}_{num:>02}.{extension}", canonical,
            ]
            progress(f"Downloading new r/{name} media · cached files are skipped…")
            process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
            assert process.stdout is not None
            for line in process.stdout:
                message = _clean(line, 180)
                if message:
                    progress(message)
            if process.wait() != 0:
                raise ValueError("Reddit media download failed; refresh the Firefox login and retry")

        existing = next((s for s in self.list_subreddits() if s.get("name") == name), {})
        old_posts = {p.get("id"): p for p in existing.get("posts") or []}
        posts = []
        for data in rows:
            post_id = str(data.get("id") or "")
            candidates = [p for p in media_root.glob(f"{post_id}_*") if p.suffix.lower() in PHOTO_SUFFIXES | VIDEO_SUFFIXES]
            source = str(max(candidates, key=lambda p: p.stat().st_mtime_ns)) if candidates else ""
            if source and Path(source).suffix.lower() in VIDEO_SUFFIXES:
                post_type = "video"
            elif source:
                post_type = "photo"
            elif data.get("is_self"):
                post_type = "text"
            else:
                post_type = "link"
            created = float(data.get("created_utc") or 0)
            posts.append({
                "id": "rd_" + post_id,
                "type": post_type,
                "title": _clean(data.get("title") or "Reddit post", 100),
                "body": _clean(data.get("selftext"), 1000),
                "author": _clean(data.get("author") or "[deleted]", 64),
                "flair": _clean(data.get("link_flair_text"), 48),
                "source_path": source,
                "url": _clean("https://www.reddit.com" + str(data.get("permalink") or ""), 500),
                "score": int(data.get("score") or data.get("ups") or 0),
                "comments": int(data.get("num_comments") or 0),
                "ratio": int(round(float(data.get("upvote_ratio") or 0) * 100)),
                "created": datetime.fromtimestamp(created, timezone.utc).strftime("%Y-%m-%d") if created else "",
                "stickied": bool(data.get("stickied")),
            })
        # Preserve cached posts that fell off the current listing, then show Reddit order:
        # stickies first, followed by the listing's newest/current ranking.
        seen = {p["id"] for p in posts}
        posts.extend(p for key, p in old_posts.items() if key not in seen)
        subreddit = {
            "url": canonical, "name": name,
            "display_name": "r/" + str(rows[0].get("subreddit") or name),
            "subscribers": int(rows[0].get("subreddit_subscribers") or existing.get("subscribers") or 0),
            "description": _clean(existing.get("description") or f"Offline posts from r/{name}", 300),
            "posts": posts,
        }
        payload = self._load()
        payload["subreddits"] = [s for s in payload["subreddits"] if s.get("name") != name] + [subreddit]
        self._save(payload)
        return {"subreddits": 1, "posts": len(posts), "media": sum(bool(p.get("source_path")) for p in posts)}

    def sync(self, mount_path, progress=None):
        mount = validate_device_root(mount_path)
        subreddits = self.list_subreddits()
        if not subreddits:
            raise ValueError("Import a subreddit before syncing")
        root = Path(resolve_under_root(mount, REDDIT_ROOT))
        paths = {name: root / name for name in ("media", "thumbnails", "display", "assets")}
        for path in (root, *paths.values()):
            Path(path).mkdir(parents=True, exist_ok=True)
        staging = Path(tempfile.mkdtemp(prefix=".staging-", dir=root))
        sync_index = DeviceSyncIndex(self.config, mount)
        report = {"subreddits": len(subreddits), "posts": 0, "media": 0, "updated": 0, "unchanged": 0}
        lines = ["id\tsubreddit\ttype\ttitle\tbody\tauthor\tmedia\tthumbnail\tdisplay\tscore\tcomments\tratio\tcreated\tflair\tstickied"]
        community_lines = ["name\tdisplay_name\tdescription\tsubscribers\tposts"]
        previews = ["path"]
        total = sum(len(s.get("posts") or []) for s in subreddits)
        done = 0
        valid_item_ids = set()
        for community in subreddits:
            posts = community.get("posts") or []
            community_lines.append("\t".join([_clean(community["name"], 32), _clean(community.get("display_name"), 48), _clean(community.get("description"), 180), str(community.get("subscribers") or 0), str(len(posts))]))
            for post in posts:
                done += 1
                valid_item_ids.add(post["id"])
                if progress:
                    progress(f"Reddit {done}/{max(1,total)} · r/{community['name']} · {_clean(post.get('title'), 48)}")
                source = post.get("source_path") or ""
                thumb_device = display_device = media_device = ""
                if source and os.path.isfile(source):
                    report["media"] += 1
                    signature = _signature(source)
                    thumb = paths["thumbnails"] / f"{post['id']}.bmp"
                    if post["type"] == "photo":
                        display = paths["display"] / f"{post['id']}.bmp"
                        marker = display.with_suffix(".source")
                        photo_signature = signature + ":reddit-2010-square-pane-v1"
                        pane = display.with_suffix(".pane.bmp")
                        current = sync_index.current_or_seed(
                            "reddit", post["id"], "photo-views-v1", photo_signature,
                            [display, thumb, pane], legacy_marker=marker,
                            legacy_signature=photo_signature,
                        )
                        if not current:
                            with Image.open(source) as image:
                                rgb = image.convert("RGB")
                                ImageOps.fit(rgb, (72, 72), Image.Resampling.LANCZOS).save(thumb, "BMP")
                                canvas = Image.new("RGB", (320, 200), "white")
                                fitted = ImageOps.contain(rgb, (320, 200), Image.Resampling.LANCZOS)
                                canvas.paste(fitted, ((320-fitted.width)//2, (200-fitted.height)//2))
                                canvas.save(display, "BMP")
                                ImageOps.fit(rgb, (320, 240), Image.Resampling.LANCZOS).save(pane, "BMP")
                            atomic_write_text(marker, photo_signature)
                            sync_index.mark(
                                "reddit", post["id"], "photo-views-v1",
                                photo_signature, [display, thumb, pane],
                            )
                            report["updated"] += 1
                        else:
                            report["unchanged"] += 1
                        display_device = f"/{REDDIT_ROOT}/display/{display.name}"
                        previews.append(display_device)
                    else:
                        target = paths["media"] / f"{post['id']}.mpg"
                        marker = target.with_suffix(".source")
                        media_signature = signature + ":reddit-mpeg2-320x240-30-v1"
                        current = sync_index.current_or_seed(
                            "reddit", post["id"], "video-media-v1", media_signature,
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
                                "reddit", post["id"], "video-media-v1",
                                media_signature, [target],
                            )
                            report["updated"] += 1
                        else:
                            report["unchanged"] += 1
                        thumb_signature = signature + ":reddit-video-thumb-72-v1"
                        thumb_ready = sync_index.current_or_seed(
                            "reddit", post["id"], "video-thumbnail-v1",
                            thumb_signature, [thumb], trust_existing=current,
                        )
                        if not thumb_ready:
                            frame = staging / f"{post['id']}.jpg"
                            subprocess.run([self.config.get("ffmpeg_binary", "ffmpeg"), "-hide_banner", "-loglevel", "error", "-y", "-ss", "1", "-i", source, "-frames:v", "1", str(frame)], check=False)
                            if frame.is_file():
                                with Image.open(frame) as image:
                                    ImageOps.fit(image.convert("RGB"), (72, 72), Image.Resampling.LANCZOS).save(thumb, "BMP")
                                sync_index.mark(
                                    "reddit", post["id"], "video-thumbnail-v1",
                                    thumb_signature, [thumb],
                                )
                                thumb_ready = True
                        media_device = f"/{REDDIT_ROOT}/media/{target.name}"
                    if post["type"] == "photo" or thumb_ready:
                        thumb_device = f"/{REDDIT_ROOT}/thumbnails/{thumb.name}"
                fields = [post["id"], community["name"], post["type"], post.get("title"), post.get("body"), post.get("author"), media_device, thumb_device, display_device, post.get("score"), post.get("comments"), post.get("ratio"), post.get("created"), post.get("flair"), "1" if post.get("stickied") else "0"]
                lines.append("\t".join(_clean(value, 180) for value in fields))
                report["posts"] += 1
        _write_if_changed(root / "library.tsv", "\n".join(lines) + "\n")
        _write_if_changed(root / "subreddits.tsv", "\n".join(community_lines) + "\n")
        _write_if_changed(root / "previews.tsv", "\n".join(previews) + "\n")
        logo = self.repo_root / "assets/ipodjs/rockbox/reddit/reddit-logo-official.40x40.bmp"
        if _copy_if_changed(logo, paths["assets"] / "reddit-logo.bmp"):
            report["updated"] += 1
        sync_index.prune("reddit", valid_item_ids)
        sync_index.close()
        shutil.rmtree(staging, ignore_errors=True)
        return report
