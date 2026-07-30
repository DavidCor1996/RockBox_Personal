"""Personal offline Calm sound acquisition and scoped Rockbox sync."""

from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import uuid
from pathlib import Path

from services.file_safety import atomic_write_json, atomic_write_text


CALM_CATEGORIES = ("For You", "Sleep", "Meditate", "Music", "Sounds")
CALM_TARGET_DIR = ".rockbox/rockpod/calm"
CALM_MANIFEST_VERSION = "rockpod_calm_v1"
SUPPORTED_AUDIO = {".mp3", ".ogg", ".opus", ".flac", ".m4a", ".wav"}


def clean_field(value):
    return " ".join(str(value or "").replace("\t", " ").split())


def normalize_category(value):
    text = clean_field(value)
    return text if text in CALM_CATEGORIES else "Sounds"


class CalmService:
    """Own the host library, audio-only command, and Calm bundle."""

    def __init__(self, config, repo_root):
        self.config = config
        self.repo_root = Path(repo_root).resolve()
        self.root = Path(config.get("cache_dir")).expanduser().resolve() / "calm"
        self.library_root = self.root / "library"
        self.categories_path = self.root / "categories.json"
        self.library_root.mkdir(parents=True, exist_ok=True)

    def build_download_command(self, url, category="Sounds"):
        url = str(url or "").strip()
        if not re.match(r"^https?://", url):
            raise ValueError("Enter a full YouTube URL.")
        ytdlp = (
            self.config.get("youtube_calm_binary", "")
            or self.config.get("youtube_movie_binary", "")
            or shutil.which("yt-dlp")
            or "yt-dlp"
        )
        output = str(self.library_root / "%(id)s.%(ext)s")
        return [
            ytdlp,
            "--no-playlist",
            "--extract-audio",
            "--audio-format",
            "mp3",
            "--audio-quality",
            "5",
            "--write-info-json",
            "--no-write-thumbnail",
            "--restrict-filenames",
            "--output",
            output,
            "--",
            url,
        ], normalize_category(category)

    def run_download(self, url, category="Sounds", timeout=None):
        command, normalized_category = self.build_download_command(url, category)
        before = {path.name for path in self.library_root.glob("*.info.json")}
        completed = subprocess.run(
            command,
            check=False,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        if completed.returncode:
            detail = (completed.stderr or completed.stdout or "").strip()
            raise RuntimeError(detail[-800:] or "yt-dlp could not download the sound.")
        after = sorted(
            (path for path in self.library_root.glob("*.info.json")
             if path.name not in before),
            key=lambda path: path.stat().st_mtime,
        )
        if after:
            self.set_category(after[-1].name[:-10], normalized_category)
        return self.scan()

    def _category_map(self):
        try:
            with self.categories_path.open("r", encoding="utf-8") as handle:
                data = json.load(handle)
        except (OSError, ValueError, TypeError):
            return {}
        return data if isinstance(data, dict) else {}

    def set_category(self, media_id, category):
        categories = self._category_map()
        categories[clean_field(media_id)] = normalize_category(category)
        atomic_write_json(self.categories_path, categories)

    def scan(self):
        categories = self._category_map()
        items = []
        for audio_path in sorted(self.library_root.iterdir()):
            if not audio_path.is_file() or audio_path.suffix.lower() not in SUPPORTED_AUDIO:
                continue
            media_id = audio_path.stem
            info_path = self.library_root / f"{media_id}.info.json"
            try:
                with info_path.open("r", encoding="utf-8") as handle:
                    info = json.load(handle)
            except (OSError, ValueError, TypeError):
                info = {}
            title = clean_field(info.get("title") or media_id)
            source = clean_field(
                info.get("webpage_url")
                or info.get("original_url")
                or (f"https://www.youtube.com/watch?v={media_id}"
                    if media_id else "")
            )
            try:
                duration = max(0, int(float(info.get("duration") or 0)))
            except (TypeError, ValueError):
                duration = 0
            items.append(
                {
                    "id": media_id,
                    "title": title,
                    "category": normalize_category(categories.get(media_id)),
                    "duration": duration,
                    "source_url": source,
                    "audio_path": str(audio_path.resolve()),
                    "size": audio_path.stat().st_size,
                }
            )
        return items

    def target_root(self, profile, target_mode="device", simulator_target=None):
        if target_mode == "simulator":
            mount = (
                profile.get("simulator_simdisk_path")
                or (simulator_target or {}).get("simdisk_path")
                or ""
            )
        else:
            mount = profile.get("device_mount_path") or ""
        if not mount:
            return None
        return Path(mount).expanduser().resolve() / CALM_TARGET_DIR

    def _icon_source(self):
        return (
            self.repo_root
            / "assets"
            / "ipodjs"
            / "rockbox"
            / "calm"
            / "calm-icon.64x64x24.bmp"
        )

    def sync(self, profile, target_mode="device", simulator_target=None):
        target = self.target_root(profile, target_mode, simulator_target)
        if target is None:
            raise ValueError(f"No {target_mode} target is configured.")
        target.parent.mkdir(parents=True, exist_ok=True)
        stage = target.parent / f".calm-stage-{uuid.uuid4().hex}"
        old = target.parent / f".calm-old-{uuid.uuid4().hex}"
        items = self.scan()
        try:
            (stage / "sounds").mkdir(parents=True)
            (stage / "assets").mkdir(parents=True)
            atomic_write_text(stage / "database.ignore", "")
            rows = [CALM_MANIFEST_VERSION]
            for item in items:
                destination = stage / "sounds" / Path(item["audio_path"]).name
                shutil.copy2(item["audio_path"], destination)
                device_path = (
                    f"/{CALM_TARGET_DIR}/sounds/{destination.name}"
                )
                rows.append(
                    "\t".join(
                        (
                            clean_field(item["title"]),
                            normalize_category(item["category"]),
                            str(item["duration"]),
                            device_path,
                            clean_field(item["source_url"]),
                        )
                    )
                )
            atomic_write_text(stage / "library.tsv", "\n".join(rows) + "\n")
            icon = self._icon_source()
            if icon.is_file():
                shutil.copy2(icon, stage / "assets" / icon.name)
            if target.exists():
                os.replace(target, old)
            os.replace(stage, target)
            if old.exists():
                shutil.rmtree(old)
        except Exception:
            if stage.exists():
                shutil.rmtree(stage)
            if old.exists() and not target.exists():
                os.replace(old, target)
            raise
        return {"count": len(items), "target": str(target)}

    def remove(self, media_ids):
        removed = 0
        categories = self._category_map()
        for media_id in {clean_field(value) for value in media_ids}:
            for path in self.library_root.glob(f"{media_id}.*"):
                if path.is_file():
                    path.unlink()
                    removed += 1
            categories.pop(media_id, None)
        atomic_write_json(self.categories_path, categories)
        return removed
