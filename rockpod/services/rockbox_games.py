"""Rockboy ROM indexing and scoped sync/remove workflows."""

from __future__ import annotations

import json
import os
import re
import shutil
import zipfile
from datetime import datetime
from urllib.error import HTTPError, URLError
from urllib.parse import quote
from urllib.request import Request, urlopen

from PIL import Image, UnidentifiedImageError

from services.online_game_metadata import OnlineGameMetadataLookup


SUPPORTED_ROM_EXTENSIONS = {".gb", ".gbc"}
ROM_TARGET_DIR = "gameboy"
SAVE_TARGET_DIR = ".rockbox/rockboy"
LAUNCHER_INDEX_RELATIVE_PATH = ".rockbox/rocks/games/rockboy_launcher/games.tsv"
LAUNCHER_SCAN_MAX_DEPTH = 6
LAUNCHER_COVER_EXTENSIONS = (".bmp", ".jpg", ".jpeg")
LOCAL_COVER_EXTENSIONS = (".png", ".jpg", ".jpeg", ".webp")
SYNC_COVER_EXTENSIONS = (".jpg", ".jpeg", ".bmp")
SYNC_COVER_MAX_SIZE = {
    "320x240": (140, 124),
    "176x132": (96, 96),
}
LIBRETRO_BOXART_BASE_URLS = {
    ".gb": "https://thumbnails.libretro.com/Nintendo%20-%20Game%20Boy/Named_Boxarts",
    ".gbc": "https://thumbnails.libretro.com/Nintendo%20-%20Game%20Boy%20Color/Named_Boxarts",
}
PERF_THRESHOLDS = {
    "320x240": {"warn": 1024 * 1024, "critical": 2 * 1024 * 1024},
    "176x132": {"warn": 512 * 1024, "critical": 1024 * 1024},
}
LAUNCHER_INDEX_COLUMN_COUNT = 10


class RockboxGameService:
    """Manage user-supplied Game Boy / Game Boy Color ROMs."""

    def launcher_library(self, profile, target_mode="device", simulator_target=None):
        mount_root = self.mount_root(profile, target_mode, simulator_target)
        if not mount_root:
            return []
        index_path = os.path.join(mount_root, LAUNCHER_INDEX_RELATIVE_PATH.lstrip("/"))
        entries = self.parse_launcher_index(index_path)
        if entries:
            return entries
        return self.scan_launcher_roms(self.rom_target_root(profile, target_mode, simulator_target))

    def parse_launcher_index(self, index_path):
        index_path = os.path.abspath(index_path or "")
        if not index_path or not os.path.isfile(index_path):
            return []
        index_dir = os.path.dirname(index_path)
        mount_root = self._launcher_mount_root(index_path)
        entries = []
        with open(index_path, "r", encoding="utf-8") as handle:
            for raw_line in handle:
                line = raw_line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = [part.strip() for part in raw_line.rstrip("\r\n").split("\t")]
                parts.extend([""] * (LAUNCHER_INDEX_COLUMN_COUNT - len(parts)))
                title, rom_path, cover_path, favorite, save_hint, year, genre, publisher, developer, description = parts[:LAUNCHER_INDEX_COLUMN_COUNT]
                resolved_rom = self._launcher_resolve_path(index_dir, rom_path, mount_root)
                if os.path.splitext(resolved_rom)[1].lower() not in SUPPORTED_ROM_EXTENSIONS:
                    continue
                if not os.path.isfile(resolved_rom):
                    continue
                entries.append(
                    {
                        "title": title or self._launcher_title_from_path(resolved_rom),
                        "rom_path": resolved_rom,
                        "cover_path": self._launcher_resolve_path(index_dir, cover_path, mount_root),
                        "favorite": self._launcher_parse_bool(favorite),
                        "save_hint": save_hint,
                        "year": year,
                        "genre": genre,
                        "publisher": publisher,
                        "developer": developer,
                        "description": description,
                    }
                )
        return self._sort_launcher_entries(entries)

    def scan_launcher_roms(self, scan_root):
        scan_root = os.path.abspath(scan_root or "")
        if not scan_root or not os.path.isdir(scan_root):
            return []
        entries = []
        self._scan_launcher_dir(scan_root, 0, entries)
        return self._sort_launcher_entries(entries)

    def list_games(self, profile, simulator_target=None, config=None):
        library_path = os.path.abspath(profile.get("games_library_path") or "")
        device_root = self.rom_target_root(profile, "device")
        sim_root = self.rom_target_root(profile, "simulator", simulator_target)
        device_save_root = self.save_target_root(profile, "device")
        sim_save_root = self.save_target_root(profile, "simulator", simulator_target)
        device_entries = self._launcher_entries_by_filename(self.launcher_library(profile, "device"))
        sim_entries = self._launcher_entries_by_filename(self.launcher_library(profile, "simulator", simulator_target))
        games = []
        filenames = set()
        if library_path and os.path.isdir(library_path):
            for name in sorted(os.listdir(library_path)):
                full = os.path.join(library_path, name)
                if not os.path.isfile(full):
                    continue
                ext = os.path.splitext(name)[1].lower()
                if ext not in SUPPORTED_ROM_EXTENSIONS:
                    continue
                stat = os.stat(full)
                metadata = self.cached_metadata_for_game(profile, {"filename": name, "source_path": full})
                games.append(
                    {
                        "id": name,
                        "title": os.path.splitext(name)[0].replace("_", " "),
                        "filename": name,
                        "source_path": full,
                        "size": stat.st_size,
                        "modified_time": stat.st_mtime,
                        "on_device": os.path.isfile(os.path.join(device_root, name)) if device_root else False,
                        "on_simulator": os.path.isfile(os.path.join(sim_root, name)) if sim_root else False,
                        "device_save_exists": self._save_exists(device_save_root, {"filename": name, "source_path": full}),
                        "simulator_save_exists": self._save_exists(sim_save_root, {"filename": name, "source_path": full}),
                        "cover_path": self.cover_path_for_game(
                            {
                                "filename": name,
                                "source_path": full,
                            },
                            config=config,
                        ),
                        "year": metadata.get("year", ""),
                        "genre": metadata.get("genre", ""),
                        "publisher": metadata.get("publisher", ""),
                        "developer": metadata.get("developer", ""),
                        "description": metadata.get("description", ""),
                        "missing_source": False,
                    }
                )
                filenames.add(name)
        for name in sorted(set(device_entries) | set(sim_entries)):
            if name in filenames:
                continue
            device_entry = device_entries.get(name) or {}
            sim_entry = sim_entries.get(name) or {}
            merged_entry = {}
            merged_entry.update(device_entry)
            merged_entry.update(sim_entry)
            device_path = str(device_entry.get("rom_path") or "").strip()
            sim_path = str(sim_entry.get("rom_path") or "").strip()
            existing_path = ""
            for candidate in (device_path, sim_path):
                if candidate and os.path.isfile(candidate):
                    existing_path = candidate
                    break
            stat = os.stat(existing_path) if existing_path else None
            games.append(
                {
                    "id": name,
                    "title": merged_entry.get("title") or self._launcher_title_from_path(name),
                    "filename": name,
                    "source_path": "",
                    "size": stat.st_size if stat else 0,
                    "modified_time": stat.st_mtime if stat else 0,
                    "on_device": bool(device_path and os.path.isfile(device_path)),
                    "on_simulator": bool(sim_path and os.path.isfile(sim_path)),
                    "device_save_exists": self._save_exists(device_save_root, {"filename": name, "source_path": ""}),
                    "simulator_save_exists": self._save_exists(sim_save_root, {"filename": name, "source_path": ""}),
                    "cover_path": merged_entry.get("cover_path") or "",
                    "year": merged_entry.get("year", ""),
                    "genre": merged_entry.get("genre", ""),
                    "publisher": merged_entry.get("publisher", ""),
                    "developer": merged_entry.get("developer", ""),
                    "description": merged_entry.get("description", ""),
                    "missing_source": True,
                }
            )
        for game in games:
            game["performance"] = self.performance_report(game, profile)
        return games

    def mount_root(self, profile, target_mode="device", simulator_target=None):
        mount_root = profile.get("device_mount_path") or ""
        if target_mode == "simulator":
            mount_root = (
                profile.get("simulator_simdisk_path")
                or (simulator_target or {}).get("simdisk_path")
                or ""
            )
        return os.path.abspath(mount_root) if mount_root else ""

    def rom_target_root(self, profile, target_mode="device", simulator_target=None):
        mount_root = self.mount_root(profile, target_mode, simulator_target)
        if not mount_root:
            return ""
        return os.path.abspath(os.path.join(mount_root, self._rom_target_dir(profile, target_mode).lstrip("/")))

    def save_target_root(self, profile, target_mode="device", simulator_target=None):
        mount_root = self.mount_root(profile, target_mode, simulator_target)
        if not mount_root:
            return ""
        return os.path.abspath(os.path.join(mount_root, SAVE_TARGET_DIR.lstrip("/")))

    def target_root(self, profile, target_mode="device", simulator_target=None):
        return self.rom_target_root(profile, target_mode, simulator_target)

    def _rom_target_dir(self, profile, target_mode="device"):
        if target_mode == "simulator":
            return str(profile.get("games_simulator_target_dir") or ROM_TARGET_DIR).strip() or ROM_TARGET_DIR
        return str(profile.get("games_device_target_dir") or ROM_TARGET_DIR).strip() or ROM_TARGET_DIR

    def deploy_profile(self, profile, target_mode="device", simulator_target=None):
        mount_root = self.mount_root(profile, target_mode, simulator_target)
        backup_root = os.path.join(profile["source_repo_path"], "rockpod", ".backups", "games", profile["id"], target_mode)
        return {
            "id": f"{profile['id']}-games-{target_mode}",
            "name": f"{profile['name']} Games ({target_mode})",
            "device_mount_path": os.path.abspath(mount_root) if mount_root else "",
            "target_device_model": profile.get("target_device_model", ""),
            "screen_resolution": profile.get("screen_resolution", ""),
            "source_repo_path": profile["source_repo_path"],
            "selected_theme": profile.get("selected_theme", ""),
            "backup_location": os.path.abspath(backup_root),
        }

    def build_sync_bundle(self, profile, games, target_mode="device", simulator_target=None):
        assets = []
        rom_target_dir = self._rom_target_dir(profile, target_mode).rstrip("/")
        stage_root = self._sync_stage_root(profile, target_mode)
        shutil.rmtree(stage_root, ignore_errors=True)
        os.makedirs(stage_root, exist_ok=True)
        cover_assets = {}
        for game in games:
            assets.append(
                {
                    "kind": "rom",
                    "source_rel": game["filename"],
                    "source_abs": game["source_path"],
                    "destination_rel": f"{rom_target_dir}/{game['filename']}",
                    "exists": True,
                    "size": game["size"],
                }
            )
            cover_asset = self._build_cover_sync_asset(profile, game, rom_target_dir, stage_root)
            if cover_asset:
                cover_assets[game["filename"]] = cover_asset
                assets.append(cover_asset)
        index_asset = self._build_launcher_index_asset(
            profile,
            target_mode,
            simulator_target,
            stage_root,
            add_games=games,
            include_missing=True,
            synced_cover_assets=cover_assets,
        )
        if index_asset:
            assets.append(index_asset)
        return {
            "id": f"games-sync-{profile['id']}",
            "name": "Rockboy Game Sync",
            "assets": assets,
        }

    def build_remove_bundle(self, profile, games, target_mode="device", simulator_target=None):
        assets = []
        rom_target_dir = self._rom_target_dir(profile, target_mode).rstrip("/")
        for game in games:
            assets.append(
                {
                    "kind": "rom",
                    "source_rel": "",
                    "source_abs": "",
                    "destination_rel": f"{rom_target_dir}/{game['filename']}",
                    "exists": False,
                    "action": "remove",
                }
            )
            stem = os.path.splitext(game["filename"])[0]
            for extension in SYNC_COVER_EXTENSIONS:
                assets.append(
                    {
                        "kind": "cover",
                        "source_rel": "",
                        "source_abs": "",
                        "destination_rel": f"{rom_target_dir}/{stem}{extension}",
                        "exists": False,
                        "action": "remove",
                    }
                )
        remove_filenames = {game["filename"] for game in games}
        index_asset = self._build_launcher_index_asset(
            profile,
            target_mode,
            simulator_target,
            self._sync_stage_root(profile, target_mode),
            remove_filenames=remove_filenames,
        )
        if index_asset:
            assets.append(index_asset)
        return {
            "id": f"games-remove-{profile['id']}",
            "name": "Remove Rockboy Games",
            "assets": assets,
        }

    def build_index_bundle(self, profile, target_mode="device", simulator_target=None, games=None, include_missing=False):
        stage_root = self._sync_stage_root(profile, target_mode)
        os.makedirs(stage_root, exist_ok=True)
        asset = self._build_launcher_index_asset(
            profile,
            target_mode,
            simulator_target,
            stage_root,
            add_games=(games or []),
            include_missing=include_missing,
        )
        assets = [asset] if asset else []
        return {
            "id": f"games-index-{profile['id']}-{target_mode}",
            "name": "Rockboy Launcher Index",
            "assets": assets,
        }

    def backup_saves(self, profile, games, target_mode="device", simulator_target=None):
        target_root = self.save_target_root(profile, target_mode, simulator_target)
        backup_root = os.path.join(
            profile["source_repo_path"],
            "rockpod",
            ".backups",
            "game-saves",
            profile["id"],
            target_mode,
            datetime.now().strftime("%Y%m%d-%H%M%S"),
        )
        copied = []
        if not target_root or not os.path.isdir(target_root):
            return {"success": False, "backup_dir": backup_root, "copied": copied, "message": "Target game directory does not exist"}
        for game in games:
            for source in self._save_candidate_paths(target_root, game):
                if not os.path.isfile(source):
                    continue
                dest = os.path.join(backup_root, os.path.basename(source))
                os.makedirs(os.path.dirname(dest), exist_ok=True)
                shutil.copy2(source, dest)
                copied.append(dest)
        return {
            "success": True,
            "backup_dir": backup_root,
            "copied": copied,
            "message": f"Backed up {len(copied)} save files" if copied else "No save files found",
        }

    def restore_saves(self, profile, games, target_mode="device", simulator_target=None, backup_dir=None):
        save_root = self.save_target_root(profile, target_mode, simulator_target)
        if not save_root:
            return {"success": False, "restored": [], "message": "Target save directory does not exist", "backup_dir": backup_dir or ""}
        backup_dir = backup_dir or self.latest_save_backup_dir(profile, target_mode)
        if not backup_dir or not os.path.isdir(backup_dir):
            return {"success": False, "restored": [], "message": "No save backup found", "backup_dir": backup_dir or ""}
        restored = []
        for game in games:
            for source, dest in self._backup_restore_pairs(backup_dir, save_root, game):
                if not os.path.isfile(source):
                    continue
                os.makedirs(os.path.dirname(dest), exist_ok=True)
                shutil.copy2(source, dest)
                restored.append(dest)
        return {
            "success": True,
            "restored": restored,
            "message": f"Restored {len(restored)} save files" if restored else "No matching save files in backup",
            "backup_dir": backup_dir,
        }

    def export_save_bundle(self, profile, games, target_mode="device", simulator_target=None, archive_path=""):
        save_root = self.save_target_root(profile, target_mode, simulator_target)
        if not save_root:
            return {"success": False, "message": "Target save directory does not exist", "archive_path": archive_path}
        archive_path = os.path.abspath(archive_path)
        os.makedirs(os.path.dirname(archive_path), exist_ok=True)
        written = 0
        with zipfile.ZipFile(archive_path, "w", compression=zipfile.ZIP_DEFLATED) as bundle:
            for game in games:
                for source in self._save_candidate_paths(save_root, game):
                    if not os.path.isfile(source):
                        continue
                    bundle.write(source, arcname=os.path.basename(source))
                    written += 1
        return {
            "success": True,
            "message": f"Exported {written} save files" if written else "No save files found to export",
            "archive_path": archive_path,
            "count": written,
        }

    def import_save_bundle(self, profile, target_mode="device", simulator_target=None, archive_path=""):
        save_root = self.save_target_root(profile, target_mode, simulator_target)
        if not save_root:
            return {"success": False, "message": "Target save directory does not exist", "imported": []}
        if not archive_path or not os.path.isfile(archive_path):
            return {"success": False, "message": "Save bundle not found", "imported": []}
        imported = []
        os.makedirs(save_root, exist_ok=True)
        with zipfile.ZipFile(archive_path, "r") as bundle:
            for member in bundle.infolist():
                if member.is_dir():
                    continue
                filename = os.path.basename(member.filename)
                if not filename.lower().endswith((".sav", ".rtc", ".sn")):
                    continue
                target = os.path.join(save_root, filename)
                with bundle.open(member, "r") as src, open(target, "wb") as dest:
                    dest.write(src.read())
                imported.append(target)
        return {
            "success": True,
            "message": f"Imported {len(imported)} save files" if imported else "No .sav files found in bundle",
            "imported": imported,
            "archive_path": archive_path,
        }

    def latest_save_backup_dir(self, profile, target_mode="device"):
        backup_root = os.path.join(
            profile["source_repo_path"],
            "rockpod",
            ".backups",
            "game-saves",
            profile["id"],
            target_mode,
        )
        if not os.path.isdir(backup_root):
            return ""
        candidates = [os.path.join(backup_root, name) for name in sorted(os.listdir(backup_root), reverse=True)]
        for path in candidates:
            if os.path.isdir(path):
                return path
        return ""

    def performance_report(self, game, profile):
        resolution = profile.get("screen_resolution") or "320x240"
        thresholds = PERF_THRESHOLDS.get(resolution, PERF_THRESHOLDS["320x240"])
        notes = []
        level = "ok"
        if game["size"] >= thresholds["critical"]:
            level = "critical"
            notes.append(f"Large ROM for {resolution}; expect slower loads on 5G hardware.")
        elif game["size"] >= thresholds["warn"]:
            level = "warn"
            notes.append(f"ROM is above the recommended size threshold for {resolution}.")
        cover_report = self.cover_optimization_report(game)
        if cover_report["oversized"]:
            notes.append(cover_report["message"])
            if level == "ok":
                level = "warn"
        return {
            "level": level,
            "notes": notes,
            "warn_threshold": thresholds["warn"],
            "critical_threshold": thresholds["critical"],
            "cover": cover_report,
        }

    def cover_optimization_report(self, game):
        cover_path = game.get("cover_path") or ""
        if not cover_path or not os.path.isfile(cover_path):
            return {"oversized": False, "message": "", "path": ""}
        try:
            with Image.open(cover_path) as image:
                width, height = image.size
        except (OSError, UnidentifiedImageError):
            return {"oversized": False, "message": "", "path": cover_path}
        stat = os.stat(cover_path)
        oversized = width > 320 or height > 240 or stat.st_size > 128 * 1024
        message = ""
        if oversized:
            message = f"Cover image is oversized ({width}x{height}, {stat.st_size // 1024} KiB). Downscale for faster previews."
        return {
            "oversized": oversized,
            "message": message,
            "path": cover_path,
            "width": width,
            "height": height,
            "size": stat.st_size,
        }

    def optimize_cover_asset(self, game):
        report = self.cover_optimization_report(game)
        cover_path = report.get("path") or ""
        if not report["oversized"] or not cover_path:
            return {"success": False, "message": "No oversized cover asset to optimize", "path": cover_path}
        try:
            with Image.open(cover_path) as image:
                optimized = image.convert("RGB")
                optimized.thumbnail(SYNC_COVER_MAX_SIZE["320x240"], self._resample_filter())
                optimized.save(cover_path, format="PNG", optimize=True)
        except (OSError, UnidentifiedImageError) as exc:
            return {"success": False, "message": f"Cover optimization failed: {exc}", "path": cover_path}
        return {"success": True, "message": "Cover optimized for Rockboy launcher previews", "path": cover_path}

    def fetch_metadata_for_game(self, profile, game, lookup_client=None, opener=urlopen):
        title = str(game.get("title") or game.get("filename") or "").strip()
        if not title:
            return {"success": False, "message": "No game title available", "metadata": {}}
        lookup = lookup_client or OnlineGameMetadataLookup()
        metadata = lookup.fetch_game_metadata(
            title,
            platform_hint=os.path.splitext(game.get("filename") or "")[1].lower(),
            opener=opener,
        )
        if not metadata:
            return {"success": False, "message": f"No metadata match found for {title}", "metadata": {}}
        cache_path = self._write_cached_metadata(profile, game, metadata)
        return {
            "success": True,
            "message": f"Fetched metadata for {title}",
            "metadata": metadata,
            "path": cache_path,
        }

    def settings_guidance(self, profile):
        resolution = profile.get("screen_resolution") or "320x240"
        notes = [
            "Frame skip: use 1 on heavier GBC titles if audio crackles.",
            "Scaling: prefer unscaled or integer-friendly layouts for steadier performance.",
            "Sound: disable sound first when chasing frame drops on 5G hardware.",
        ]
        if resolution == "176x132":
            notes.append("Use smaller viewport modes first; full scaling is expensive on nano-class targets.")
        return notes

    def cover_path_for_game(self, game, config=None):
        source_path = os.path.abspath(game.get("source_path") or "")
        if source_path:
            stem, _ext = os.path.splitext(source_path)
            for ext in LOCAL_COVER_EXTENSIONS:
                candidate = stem + ext
                if os.path.isfile(candidate):
                    return candidate
        cache_path = self._cached_cover_path(game, config)
        if cache_path and os.path.isfile(cache_path):
            return cache_path
        return ""

    def fetch_cover_for_game(self, game, config, opener=urlopen, timeout=10):
        cache_path = self._cached_cover_path(game, config)
        if not cache_path:
            return {"success": False, "message": "No cache directory configured", "path": "", "url": ""}
        existing = self.cover_path_for_game(game, config=config)
        if existing and os.path.isfile(existing):
            return {"success": True, "message": "Cover already available", "path": existing, "url": ""}
        ext = os.path.splitext(game.get("filename") or "")[1].lower()
        base_url = LIBRETRO_BOXART_BASE_URLS.get(ext)
        if not base_url:
            return {"success": False, "message": "Unsupported ROM platform for cover lookup", "path": "", "url": ""}
        os.makedirs(os.path.dirname(cache_path), exist_ok=True)
        last_error = None
        for candidate in self._cover_name_candidates(game):
            url = f"{base_url}/{quote(candidate)}.png"
            request = Request(url, headers={"User-Agent": "RockPod/0.1"})
            try:
                with opener(request, timeout=timeout) as response:
                    data = response.read()
            except HTTPError as exc:
                last_error = exc
                if exc.code == 404:
                    continue
                return {"success": False, "message": f"Cover lookup failed: HTTP {exc.code}", "path": "", "url": url}
            except URLError as exc:
                return {"success": False, "message": f"Cover lookup failed: {exc.reason}", "path": "", "url": url}
            if not data:
                continue
            with open(cache_path, "wb") as handle:
                handle.write(data)
            return {"success": True, "message": f"Fetched cover for {game.get('title') or game.get('filename')}", "path": cache_path, "url": url}
        if last_error is not None and getattr(last_error, "code", None) != 404:
            return {"success": False, "message": f"Cover lookup failed: HTTP {last_error.code}", "path": "", "url": ""}
        return {"success": False, "message": f"No cover match found for {game.get('title') or game.get('filename')}", "path": "", "url": ""}

    def _save_exists(self, target_root, game):
        if not target_root:
            return False
        return any(os.path.isfile(path) for path in self._save_candidate_paths(target_root, game))

    def _build_cover_sync_asset(self, profile, game, rom_target_dir, stage_root):
        cover_source = os.path.abspath(game.get("cover_path") or "")
        if not cover_source or not os.path.isfile(cover_source):
            return None
        stem = os.path.splitext(game.get("filename") or "game")[0]
        destination_ext = ".bmp"
        staged_abs = os.path.join(stage_root, f"{stem}{destination_ext}")
        if not self._stage_sync_cover(cover_source, staged_abs, profile):
            return None
        return {
            "kind": "cover",
            "source_rel": os.path.basename(staged_abs),
            "source_abs": staged_abs,
            "destination_rel": f"{rom_target_dir}/{stem}{destination_ext}",
            "exists": True,
            "size": os.stat(staged_abs).st_size,
        }

    def _stage_sync_cover(self, source_path, dest_path, profile):
        try:
            with Image.open(source_path) as image:
                rendered = image.convert("RGB")
                target_size = self._sync_cover_size(profile)
                rendered.thumbnail(target_size, self._resample_filter())
                canvas = Image.new("RGB", target_size, color="black")
                paste_x = max(0, (target_size[0] - rendered.width) // 2)
                paste_y = max(0, (target_size[1] - rendered.height) // 2)
                canvas.paste(rendered, (paste_x, paste_y))
                os.makedirs(os.path.dirname(dest_path), exist_ok=True)
                canvas.save(dest_path, format="BMP")
        except (OSError, UnidentifiedImageError):
            return False
        return True

    def _scan_launcher_dir(self, dir_path, depth, entries):
        if depth > LAUNCHER_SCAN_MAX_DEPTH or not os.path.isdir(dir_path):
            return
        for name in sorted(os.listdir(dir_path)):
            child = os.path.join(dir_path, name)
            if os.path.isdir(child):
                self._scan_launcher_dir(child, depth + 1, entries)
                continue
            if os.path.splitext(name)[1].lower() not in SUPPORTED_ROM_EXTENSIONS:
                continue
            entries.append(
                {
                    "title": self._launcher_title_from_path(child),
                    "rom_path": child,
                    "cover_path": self._launcher_detect_sidecar_cover(child),
                    "favorite": False,
                    "save_hint": "",
                    "year": "",
                    "genre": "",
                    "publisher": "",
                    "developer": "",
                    "description": "",
                }
            )

    def _backup_restore_pairs(self, backup_root, save_root, game):
        stem = self._save_stem_for_game(game)
        for extension in (".sav", ".rtc", ".sn"):
            filename = stem + extension
            yield os.path.join(backup_root, filename), os.path.join(save_root, filename)

    def _save_candidate_paths(self, target_root, game):
        stem = self._save_stem_for_game(game)
        return [os.path.join(target_root, stem + extension) for extension in (".sav", ".rtc", ".sn")]

    @staticmethod
    def _save_stem_for_game(game):
        source_path = os.path.abspath(game.get("source_path") or "")
        if source_path and os.path.isfile(source_path):
            try:
                with open(source_path, "rb") as handle:
                    header = handle.read(0x150)
                if len(header) >= 0x144:
                    raw = bytearray(header[0x134:0x144])
                    if raw[14] & 0x80:
                        raw[14] = 0
                    if raw[15] & 0x80:
                        raw[15] = 0
                    title = bytes(raw).split(b"\x00", 1)[0].decode("latin-1", "ignore").strip()
                    if title:
                        return title
            except OSError:
                pass
        return os.path.splitext(game.get("filename") or "game")[0]

    @staticmethod
    def _cover_name_candidates(game):
        filename = game.get("filename") or ""
        stem = os.path.splitext(filename)[0]
        values = []
        for text in (
            stem,
            stem.replace("_", " "),
            stem.replace(".", " "),
        ):
            text = " ".join(str(text).split())
            if text and text not in values:
                values.append(text)
        return values

    def _build_launcher_index_asset(
        self,
        profile,
        target_mode,
        simulator_target,
        stage_root,
        add_games=None,
        remove_filenames=None,
        include_missing=False,
        synced_cover_assets=None,
    ):
        os.makedirs(stage_root, exist_ok=True)
        mount_root = self.mount_root(profile, target_mode, simulator_target)
        entries = self._merged_target_index_entries(
            profile,
            target_mode,
            simulator_target,
            add_games=add_games,
            remove_filenames=remove_filenames,
            include_missing=include_missing,
            synced_cover_assets=synced_cover_assets,
        )
        if not entries:
            return {
                "kind": "index",
                "source_rel": "",
                "source_abs": "",
                "destination_rel": LAUNCHER_INDEX_RELATIVE_PATH,
                "exists": False,
                "action": "remove",
            } if mount_root else None

        staged_abs = os.path.join(stage_root, "games.tsv")
        self._write_launcher_index(staged_abs, mount_root, entries)
        return {
            "kind": "index",
            "source_rel": "games.tsv",
            "source_abs": staged_abs,
            "destination_rel": LAUNCHER_INDEX_RELATIVE_PATH,
            "exists": True,
            "size": os.stat(staged_abs).st_size,
        }

    def _merged_target_index_entries(
        self,
        profile,
        target_mode,
        simulator_target,
        add_games=None,
        remove_filenames=None,
        include_missing=False,
        synced_cover_assets=None,
    ):
        mount_root = self.mount_root(profile, target_mode, simulator_target)
        target_root = self.rom_target_root(profile, target_mode, simulator_target)
        current_entries = self.scan_launcher_roms(target_root)
        indexed_entries = []
        if mount_root:
            indexed_entries = self.parse_launcher_index(os.path.join(mount_root, LAUNCHER_INDEX_RELATIVE_PATH.lstrip("/")))
        entries_by_filename = {}
        for entry in current_entries:
            filename = os.path.basename(entry.get("rom_path") or "")
            if filename:
                entries_by_filename[filename] = dict(entry)
        for entry in indexed_entries:
            filename = os.path.basename(entry.get("rom_path") or "")
            if not filename:
                continue
            merged = dict(entries_by_filename.get(filename) or {})
            merged.update({key: value for key, value in dict(entry).items() if value not in ("", None)})
            entries_by_filename[filename] = merged

        remove_filenames = {str(name or "").strip() for name in (remove_filenames or set()) if str(name or "").strip()}
        for filename in remove_filenames:
            entries_by_filename.pop(filename, None)

        synced_cover_assets = synced_cover_assets or {}
        for game in add_games or []:
            filename = str(game.get("filename") or "").strip()
            if not filename:
                continue
            if not include_missing and filename not in entries_by_filename:
                continue
            current = entries_by_filename.get(filename, {})
            entry = self._target_entry_for_local_game(
                profile,
                game,
                target_mode,
                current_entry=current,
                synced_cover_asset=synced_cover_assets.get(filename),
            )
            entries_by_filename[filename] = entry

        return self._sort_launcher_entries(list(entries_by_filename.values()))

    def _target_entry_for_local_game(self, profile, game, target_mode, current_entry=None, synced_cover_asset=None):
        current_entry = dict(current_entry or {})
        rom_target_dir = self._rom_target_dir(profile, target_mode).rstrip("/")
        filename = game.get("filename") or ""
        stem = os.path.splitext(filename)[0]
        metadata = self.cached_metadata_for_game(profile, game)
        return {
            "title": game.get("title") or current_entry.get("title") or self._launcher_title_from_path(filename),
            "rom_path": f"/{rom_target_dir}/{filename}",
            "cover_path": (
                f"/{rom_target_dir}/{stem}.bmp"
                if synced_cover_asset
                else current_entry.get("cover_path", "")
            ),
            "favorite": bool(current_entry.get("favorite")),
            "save_hint": current_entry.get("save_hint", ""),
            "year": metadata.get("year") or current_entry.get("year", ""),
            "genre": metadata.get("genre") or current_entry.get("genre", ""),
            "publisher": metadata.get("publisher") or current_entry.get("publisher", ""),
            "developer": metadata.get("developer") or current_entry.get("developer", ""),
            "description": metadata.get("description") or current_entry.get("description", ""),
        }

    def _write_launcher_index(self, index_path, mount_root, entries):
        os.makedirs(os.path.dirname(index_path), exist_ok=True)
        lines = [
            "# Rockboy launcher index",
            "# Format:",
            "# title\\trom_path\\tcover_path\\tfavorite\\tsave_hint\\tyear\\tgenre\\tpublisher\\tdeveloper\\tdescription",
        ]
        for entry in entries:
            lines.append(
                "\t".join(
                    [
                        self._sanitize_index_field(entry.get("title")),
                        self._index_path(entry.get("rom_path"), mount_root),
                        self._index_path(entry.get("cover_path"), mount_root),
                        "1" if entry.get("favorite") else "0",
                        self._sanitize_index_field(entry.get("save_hint")),
                        self._sanitize_index_field(entry.get("year")),
                        self._sanitize_index_field(entry.get("genre")),
                        self._sanitize_index_field(entry.get("publisher")),
                        self._sanitize_index_field(entry.get("developer")),
                        self._sanitize_index_field(entry.get("description")),
                    ]
                )
            )
        with open(index_path, "w", encoding="utf-8") as handle:
            handle.write("\n".join(lines) + "\n")

    @staticmethod
    def _sanitize_index_field(value):
        return str(value or "").replace("\t", " ").replace("\r", " ").replace("\n", " ").strip()

    @staticmethod
    def _index_path(path, mount_root):
        path = str(path or "").strip()
        mount_root = os.path.abspath(mount_root or "")
        if not path:
            return ""
        abs_path = os.path.abspath(path) if os.path.isabs(path) else path
        if mount_root and abs_path.startswith(mount_root + os.sep):
            return "/" + os.path.relpath(abs_path, mount_root).replace(os.sep, "/")
        return path.replace(os.sep, "/")

    def cached_metadata_for_game(self, profile, game):
        cache_path = self._cached_metadata_path(profile, game)
        if not cache_path or not os.path.isfile(cache_path):
            return {}
        try:
            with open(cache_path, "r", encoding="utf-8") as handle:
                metadata = json.load(handle)
        except (OSError, json.JSONDecodeError):
            return {}
        return {
            "year": str(metadata.get("year") or "").strip(),
            "genre": str(metadata.get("genre") or "").strip(),
            "publisher": str(metadata.get("publisher") or "").strip(),
            "developer": str(metadata.get("developer") or "").strip(),
            "description": str(metadata.get("description") or "").strip(),
            "platform": str(metadata.get("platform") or "").strip(),
            "source": str(metadata.get("source") or "").strip(),
            "source_url": str(metadata.get("source_url") or "").strip(),
            "entity_id": str(metadata.get("entity_id") or "").strip(),
        }

    def _write_cached_metadata(self, profile, game, metadata):
        cache_path = self._cached_metadata_path(profile, game)
        os.makedirs(os.path.dirname(cache_path), exist_ok=True)
        payload = dict(self.cached_metadata_for_game(profile, game))
        payload.update(
            {
                "year": str(metadata.get("year") or "").strip(),
                "genre": str(metadata.get("genre") or "").strip(),
                "publisher": str(metadata.get("publisher") or "").strip(),
                "developer": str(metadata.get("developer") or "").strip(),
                "description": str(metadata.get("description") or "").strip(),
                "platform": str(metadata.get("platform") or "").strip(),
                "source": str(metadata.get("source") or "").strip(),
                "source_url": str(metadata.get("source_url") or "").strip(),
                "entity_id": str(metadata.get("entity_id") or "").strip(),
                "updated_at": datetime.now().isoformat(timespec="seconds"),
            }
        )
        with open(cache_path, "w", encoding="utf-8") as handle:
            json.dump(payload, handle, indent=2, sort_keys=True)
            handle.write("\n")
        return cache_path

    @staticmethod
    def _cached_metadata_path(profile, game):
        repo_root = os.path.abspath(profile.get("source_repo_path") or "")
        filename = str(game.get("filename") or "").strip()
        if not repo_root or not filename:
            return ""
        safe_name = re.sub(r"[^A-Za-z0-9._ -]+", "_", filename).strip() or "game"
        return os.path.join(repo_root, "rockpod", ".generated", "game_metadata", profile["id"], f"{safe_name}.json")

    @staticmethod
    def _cached_cover_path(game, config):
        if config is None:
            return ""
        cache_root = os.path.join(config.get("cache_dir", ""), "game_covers")
        if not cache_root:
            return ""
        ext = os.path.splitext(game.get("filename") or "")[1].lower()
        platform = "gameboy-color" if ext == ".gbc" else "gameboy"
        stem = os.path.splitext(game.get("filename") or "game")[0]
        safe_name = "".join(ch if ch.isalnum() or ch in ("-", "_", ".", " ") else "_" for ch in stem).strip() or "game"
        return os.path.join(cache_root, platform, f"{safe_name}.png")

    @staticmethod
    def _launcher_parse_bool(value):
        return str(value or "").strip().lower() in {"1", "true", "yes", "y", "favorite"}

    @staticmethod
    def _launcher_resolve_path(base_dir, value, mount_root=""):
        value = str(value or "").strip()
        if not value:
            return ""
        mount_root = os.path.abspath(mount_root or "")
        if mount_root and value.startswith("/"):
            return os.path.abspath(os.path.join(mount_root, value.lstrip("/")))
        if os.path.isabs(value):
            return os.path.abspath(value)
        return os.path.abspath(os.path.join(base_dir, value))

    @staticmethod
    def _launcher_title_from_path(path):
        stem = os.path.splitext(os.path.basename(path or ""))[0]
        return stem.replace("_", " ").replace("-", " ")

    @staticmethod
    def _launcher_detect_sidecar_cover(rom_path):
        stem, _ext = os.path.splitext(os.path.abspath(rom_path))
        for extension in LAUNCHER_COVER_EXTENSIONS:
            candidate = stem + extension
            if os.path.isfile(candidate):
                return candidate
        return ""

    @staticmethod
    def _launcher_mount_root(index_path):
        index_abs = os.path.abspath(index_path or "")
        relative_index = os.path.normpath(LAUNCHER_INDEX_RELATIVE_PATH.lstrip("/"))
        if not index_abs.endswith(relative_index):
            return ""
        mount_root = index_abs
        for _part in relative_index.split(os.sep):
            mount_root = os.path.dirname(mount_root)
        return mount_root

    @staticmethod
    def _launcher_entries_by_filename(entries):
        by_filename = {}
        for entry in entries or []:
            filename = os.path.basename(str(entry.get("rom_path") or "").strip())
            if filename:
                by_filename[filename] = dict(entry)
        return by_filename

    @staticmethod
    def _sort_launcher_entries(entries):
        return sorted(entries, key=lambda entry: (entry.get("title") or "").lower())

    @staticmethod
    def _sync_stage_root(profile, target_mode):
        repo_root = os.path.abspath(profile.get("source_repo_path") or "")
        return os.path.join(repo_root, "rockpod", ".generated", "games", profile["id"], target_mode)

    @staticmethod
    def _resample_filter():
        resampling = getattr(Image, "Resampling", None)
        if resampling is not None:
            return resampling.LANCZOS
        return Image.LANCZOS

    @staticmethod
    def _sync_cover_size(profile):
        resolution = profile.get("screen_resolution") or "320x240"
        return SYNC_COVER_MAX_SIZE.get(resolution, SYNC_COVER_MAX_SIZE["320x240"])
