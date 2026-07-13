"""Rockboy ROM indexing and scoped sync/remove workflows."""

from __future__ import annotations

import json
import hashlib
import os
import re
import shutil
import zipfile
from datetime import datetime
from urllib.error import HTTPError, URLError
from urllib.parse import quote
from urllib.request import Request, urlopen

from PIL import Image, UnidentifiedImageError

from services.file_safety import atomic_write_json, atomic_write_text
from services.online_game_metadata import OnlineGameMetadataLookup


SUPPORTED_ROM_EXTENSIONS = {".gb", ".gbc", ".nes", ".sms", ".gg", ".sg", ".mgw", ".gw", ".gwz", ".sfc", ".smc"}
SNES_ROM_EXTENSIONS = {".sfc", ".smc"}
SMSGG_ROM_EXTENSIONS = {".sms", ".gg", ".sg"}
GWATCH_PACKAGE_EXTENSIONS = {".mgw", ".gw", ".gwz"}
ROM_TARGET_DIR = "gameboy"
SAVE_TARGET_DIR = ".rockbox/rockboy"
SMSGG_ROM_TARGET_DIR = ".rockbox/games/smsgg/roms"
SMSGG_SAVE_TARGET_DIR = ".rockbox/games/smsgg/saves"
SMSGG_STATE_TARGET_DIR = ".rockbox/games/smsgg/states"
SMSGG_PLUGIN_PATH = ".rockbox/rocks/games/smsgg.rock"
GWATCH_ROM_TARGET_DIR = ".rockbox/games/gwatch/roms"
GWATCH_SAVE_TARGET_DIR = ".rockbox/games/gwatch/saves"
GWATCH_PLUGIN_PATH = ".rockbox/rocks/games/gwatch.rock"
SNES_ROM_TARGET_DIR = ".rockbox/roms/snes"
SNES_SAVE_TARGET_DIR = ".rockbox/saves/snes"
SNES_PLUGIN_PATH = ".rockbox/rocks/games/snes_lite.rock"
SNES_CONFIG_PATH = ".rockbox/config/snes_lite.cfg"
SNES_GAME_CONFIG_DIR = ".rockbox/config/snes_lite"
SYSTEMS_INDEX_PATH = ".rockbox/games/library/systems.tsv"
LAUNCHER_INDEX_RELATIVE_PATH = ".rockbox/rocks/games/rockboy_launcher/games.tsv"
LAUNCHER_CONFIG_RELATIVE_PATH = ".rockbox/rocks/games/rockboy_launcher/config.cfg"
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
    ".nes": "https://thumbnails.libretro.com/Nintendo%20-%20Nintendo%20Entertainment%20System/Named_Boxarts",
    ".gg": "https://thumbnails.libretro.com/Sega%20-%20Game%20Gear/Named_Boxarts",
    ".sms": "https://thumbnails.libretro.com/Sega%20-%20Master%20System%20-%20Mark%20III/Named_Boxarts",
    ".sg": "https://thumbnails.libretro.com/Sega%20-%20SG-1000/Named_Boxarts",
    ".sfc": "https://thumbnails.libretro.com/Nintendo%20-%20Super%20Nintendo%20Entertainment%20System/Named_Boxarts",
    ".smc": "https://thumbnails.libretro.com/Nintendo%20-%20Super%20Nintendo%20Entertainment%20System/Named_Boxarts",
}
ROM_PLATFORM_CACHE_DIRS = {
    ".gb": "gameboy",
    ".gbc": "gameboy-color",
    ".nes": "nes",
    ".gg": "game-gear",
    ".sms": "master-system",
    ".sg": "sg-1000",
    ".mgw": "game-watch",
    ".gw": "game-watch",
    ".gwz": "game-watch",
    ".sfc": "super-nintendo",
    ".smc": "super-nintendo",
}
PERF_THRESHOLDS = {
    "320x240": {"warn": 1024 * 1024, "critical": 2 * 1024 * 1024},
    "176x132": {"warn": 512 * 1024, "critical": 1024 * 1024},
}
LAUNCHER_INDEX_COLUMN_COUNT = 11
SYSTEM_MANIFEST_COLUMNS = 7
SYSTEM_MANIFEST_RELATIVE_PATHS = {
    "nes": ".rockbox/games/nes/games.tsv",
    "smsgg": ".rockbox/games/smsgg/games.tsv",
    "gwatch": ".rockbox/games/gwatch/games.tsv",
    "snes": ".rockbox/games/snes/games.tsv",
}
SYSTEM_MANIFEST_EXTENSIONS = {
    "nes": {".nes"},
    "smsgg": SMSGG_ROM_EXTENSIONS,
    "gwatch": GWATCH_PACKAGE_EXTENSIONS,
    "snes": SNES_ROM_EXTENSIONS,
}


class RockboxGameService:
    """Index and sync supported handheld and console ROM libraries."""

    def __init__(self):
        self._snes_inspection_cache = {}

    @staticmethod
    def recommend_snes_controls(title, genre=""):
        """Choose a click-wheel-first layout from title and local metadata."""
        text = f"{title or ''} {genre or ''}".casefold()
        rules = (
            ("fighting", 3, "Fighting", (
                "killer instinct", "street fighter", "mortal kombat",
                "fatal fury", "samurai shodown", "fighting", "fighter",
            )),
            ("racing", 4, "Racing", (
                "mario kart", "f-zero", "top gear", "racing", "race",
            )),
            ("sports", 5, "Sports", (
                "nhl ", "nba ", "nfl ", "fifa", "madden", "baseball",
                "soccer", "hockey", "football", "basketball", "sports",
            )),
            ("action", 2, "Action", (
                "secret of mana", "zelda", "metroid", "mega man",
                "megaman", "contra", "shoot", "action",
            )),
            ("rpg", 1, "RPG", (
                "chrono trigger", "final fantasy",
                "earthbound", "dragon quest", "role-playing", "rpg",
                "adventure",
            )),
        )
        for game_type, profile, profile_name, keywords in rules:
            if any(keyword in text for keyword in keywords):
                return {
                    "game_type": game_type,
                    "input_profile": profile,
                    "input_profile_name": profile_name,
                }
        return {
            "game_type": "platformer",
            "input_profile": 0,
            "input_profile_name": "Platformer",
        }

    def launcher_library(self, profile, target_mode="device", simulator_target=None):
        mount_root = self.mount_root(profile, target_mode, simulator_target)
        if not mount_root:
            return []
        index_path = os.path.join(mount_root, LAUNCHER_INDEX_RELATIVE_PATH.lstrip("/"))
        entries = self.parse_launcher_index(index_path)
        if entries:
            return entries
        entries = self.scan_launcher_roms(self.rom_target_root(profile, target_mode, simulator_target))
        smsgg_root = self.smsgg_rom_target_root(profile, target_mode, simulator_target)
        if smsgg_root != self.rom_target_root(profile, target_mode, simulator_target):
            entries.extend(self.scan_launcher_roms(smsgg_root))
        gwatch_root = self.gwatch_rom_target_root(profile, target_mode, simulator_target)
        if gwatch_root not in {
            self.rom_target_root(profile, target_mode, simulator_target),
            smsgg_root,
        }:
            entries.extend(self.scan_launcher_roms(gwatch_root))
        return self._sort_launcher_entries(entries)

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
                title, rom_path, cover_path, favorite, save_hint, year, genre, publisher, developer, description, plugin_param = parts[:LAUNCHER_INDEX_COLUMN_COUNT]
                resolved_rom = self._launcher_resolve_path(index_dir, rom_path, mount_root)
                resolved_plugin_param = self._launcher_resolve_path(index_dir, plugin_param, mount_root)
                effective_rom = resolved_plugin_param or resolved_rom
                resolved_ext = os.path.splitext(effective_rom)[1].lower()
                if resolved_ext not in SUPPORTED_ROM_EXTENSIONS:
                    continue
                if not os.path.isfile(resolved_rom):
                    continue
                if resolved_plugin_param and not os.path.isfile(resolved_plugin_param):
                    continue
                entries.append(
                    {
                        "title": title or self._launcher_title_from_path(effective_rom),
                        "rom_path": resolved_rom,
                        "plugin_param": resolved_plugin_param,
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
        device_save_inventory = self._save_inventory(profile, "device", simulator_target)
        sim_save_inventory = self._save_inventory(profile, "simulator", simulator_target)
        device_entries = self._launcher_entries_by_filename(self.launcher_library(profile, "device"))
        sim_entries = self._launcher_entries_by_filename(self.launcher_library(profile, "simulator", simulator_target))
        named_save_extensions = (
            SNES_ROM_EXTENSIONS | SMSGG_ROM_EXTENSIONS | GWATCH_PACKAGE_EXTENSIONS
        )
        games = []
        filenames = set()
        if library_path and os.path.isdir(library_path):
            try:
                library_entries = sorted(
                    os.scandir(library_path),
                    key=lambda entry: entry.name.casefold(),
                )
            except OSError:
                library_entries = []
            for entry in library_entries:
                name = entry.name
                try:
                    if not entry.is_file():
                        continue
                except OSError:
                    continue
                ext = os.path.splitext(name)[1].lower()
                if ext not in SUPPORTED_ROM_EXTENSIONS:
                    continue
                full = entry.path
                try:
                    stat = entry.stat()
                except OSError:
                    continue
                metadata = self.cached_metadata_for_game(profile, {"filename": name, "source_path": full})
                snes_metadata = self.inspect_snes_rom(full) if ext in SNES_ROM_EXTENSIONS else {}
                snes_controls = self.recommend_snes_controls(
                    snes_metadata.get("title") or os.path.splitext(name)[0],
                    metadata.get("genre", ""),
                ) if ext in SNES_ROM_EXTENSIONS else {}
                game_ref = {"filename": name, "source_path": full}
                cover_path = self.cover_path_for_game(game_ref, config=config)
                if not cover_path and ext in SNES_ROM_EXTENSIONS:
                    fallback_cover = os.path.join(
                        profile.get("source_repo_path") or "",
                        "rockpod",
                        "assets",
                        "snes_lite_fallback.png",
                    )
                    if os.path.isfile(fallback_cover):
                        cover_path = fallback_cover
                save_stem = (
                    "" if ext in named_save_extensions
                    else self._save_stem_for_game(game_ref)
                )
                games.append(
                    {
                        "id": name,
                        "title": os.path.splitext(name)[0].replace("_", " "),
                        "filename": name,
                        "source_path": full,
                        "size": stat.st_size,
                        "modified_time": stat.st_mtime,
                        "added_time": self._file_added_time(stat),
                        "platform": self._platform_label(ext),
                        "on_device": bool(device_root and name in device_entries),
                        "on_simulator": bool(sim_root and name in sim_entries),
                        "device_save_exists": self._save_exists_in_inventory(
                            game_ref, device_save_inventory, save_stem
                        ),
                        "simulator_save_exists": self._save_exists_in_inventory(
                            game_ref, sim_save_inventory, save_stem
                        ),
                        "cover_path": cover_path,
                        "year": metadata.get("year", ""),
                        "genre": metadata.get("genre", ""),
                        "publisher": metadata.get("publisher", ""),
                        "developer": metadata.get("developer", ""),
                        "description": metadata.get("description", ""),
                        "console": "Super Nintendo" if ext in SNES_ROM_EXTENSIONS else self._platform_label(ext),
                        "rom_hash": snes_metadata.get("rom_hash", ""),
                        "internal_title": snes_metadata.get("title", ""),
                        "region": snes_metadata.get("region", ""),
                        "mapper": snes_metadata.get("mapper", ""),
                        "special_chip": snes_metadata.get("special_chip", ""),
                        "compatibility": snes_metadata.get("compatibility", "Untested"),
                        "game_type": snes_controls.get("game_type", ""),
                        "input_profile": snes_controls.get("input_profile", 0),
                        "input_profile_name": snes_controls.get("input_profile_name", ""),
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
            device_path = str(device_entry.get("plugin_param") or device_entry.get("rom_path") or "").strip()
            sim_path = str(sim_entry.get("plugin_param") or sim_entry.get("rom_path") or "").strip()
            existing_path = ""
            for candidate in (device_path, sim_path):
                if candidate and os.path.isfile(candidate):
                    existing_path = candidate
                    break
            stat = os.stat(existing_path) if existing_path else None
            game_ref = {"filename": name, "source_path": ""}
            ext = os.path.splitext(name)[1].lower()
            snes_metadata = self.inspect_snes_rom(existing_path) if (
                ext in SNES_ROM_EXTENSIONS and existing_path
            ) else {}
            snes_controls = self.recommend_snes_controls(
                snes_metadata.get("title") or
                merged_entry.get("title") or os.path.splitext(name)[0],
                merged_entry.get("genre", ""),
            ) if ext in SNES_ROM_EXTENSIONS else {}
            save_stem = (
                "" if ext in named_save_extensions
                else self._save_stem_for_game(game_ref)
            )
            games.append(
                {
                    "id": name,
                    "title": merged_entry.get("title") or self._launcher_title_from_path(name),
                    "filename": name,
                    "source_path": "",
                    "size": stat.st_size if stat else 0,
                    "modified_time": stat.st_mtime if stat else 0,
                    "added_time": self._file_added_time(stat) if stat else 0,
                    "platform": self._platform_label(os.path.splitext(name)[1].lower()),
                    "on_device": bool(device_path and os.path.isfile(device_path)),
                    "on_simulator": bool(sim_path and os.path.isfile(sim_path)),
                    "device_save_exists": self._save_exists_in_inventory(
                        game_ref, device_save_inventory, save_stem
                    ),
                    "simulator_save_exists": self._save_exists_in_inventory(
                        game_ref, sim_save_inventory, save_stem
                    ),
                    "cover_path": merged_entry.get("cover_path") or "",
                    "year": merged_entry.get("year", ""),
                    "genre": merged_entry.get("genre", ""),
                    "publisher": merged_entry.get("publisher", ""),
                    "developer": merged_entry.get("developer", ""),
                    "description": merged_entry.get("description", ""),
                    "console": "Super Nintendo" if ext in SNES_ROM_EXTENSIONS else self._platform_label(ext),
                    "rom_hash": snes_metadata.get("rom_hash", ""),
                    "internal_title": snes_metadata.get("title", ""),
                    "region": snes_metadata.get("region", ""),
                    "mapper": snes_metadata.get("mapper", ""),
                    "special_chip": snes_metadata.get("special_chip", ""),
                    "compatibility": snes_metadata.get("compatibility", "Untested"),
                    "game_type": snes_controls.get("game_type", ""),
                    "input_profile": snes_controls.get("input_profile", 0),
                    "input_profile_name": snes_controls.get("input_profile_name", ""),
                    "missing_source": True,
                }
            )
        for game in games:
            game["performance"] = self.performance_report(game, profile)
        return sorted(
            games,
            key=lambda game: (
                -float(game.get("added_time") or 0),
                str(game.get("title") or game.get("filename") or "").casefold(),
            ),
        )

    @staticmethod
    def _file_added_time(stat_result):
        if stat_result is None:
            return 0
        # Creation time is available on Windows/macOS. On Linux, ctime records
        # when a ROM entered or changed within this library, which is a closer
        # approximation of "added" than an archival ROM's embedded mtime.
        return float(
            getattr(stat_result, "st_birthtime", 0)
            or getattr(stat_result, "st_ctime", 0)
            or getattr(stat_result, "st_mtime", 0)
            or 0
        )

    @staticmethod
    def _platform_label(extension):
        return {
            ".gb": "Game Boy",
            ".gbc": "Game Boy Color",
            ".nes": "NES",
            ".sms": "Master System",
            ".gg": "Game Gear",
            ".sg": "SG-1000",
            ".mgw": "Game & Watch",
            ".gw": "Game & Watch",
            ".gwz": "Game & Watch",
            ".sfc": "Super Nintendo",
            ".smc": "Super Nintendo",
        }.get(str(extension or "").lower(), "Game")

    def inspect_snes_rom(self, path):
        path = os.path.abspath(path or "")
        try:
            stat = os.stat(path)
            cache_key = (path, stat.st_size, stat.st_mtime_ns)
            cached = self._snes_inspection_cache.get(cache_key)
            if cached is not None:
                return dict(cached)
            with open(path, "rb") as handle:
                digest = hashlib.sha256()
                while True:
                    chunk = handle.read(1024 * 1024)
                    if not chunk:
                        break
                    digest.update(chunk)
        except OSError:
            return {}
        if stat.st_size < 0x8000:
            return {"compatibility": "Invalid ROM"}
        copier = 512 if stat.st_size % 1024 == 512 else 0

        def read_header(offset):
            if offset < 0 or offset + 0x40 > stat.st_size:
                return b""
            try:
                with open(path, "rb") as handle:
                    handle.seek(offset)
                    return handle.read(0x40)
            except OSError:
                return b""

        def score(header):
            if len(header) < 0x40:
                return -1
            checksum = int.from_bytes(header[0x1E:0x20], "little")
            inverse = int.from_bytes(header[0x1C:0x1E], "little")
            return (4 if checksum ^ inverse == 0xFFFF else 0) + (2 if header[0x3D] & 0x80 else 0)

        lo_offset = copier + 0x7FC0
        hi_offset = copier + 0xFFC0
        lo_header = read_header(lo_offset)
        hi_header = read_header(hi_offset)
        hirom = score(hi_header) > score(lo_header)
        header = hi_header if hirom and len(hi_header) == 0x40 else lo_header
        if len(header) < 0x40:
            return {"compatibility": "Invalid ROM"}
        map_mode = header[0x15]
        cartridge_type = header[0x16]
        special_chip = ""
        if (map_mode & 0x2F) == 0x23 or cartridge_type in {0x34, 0x35}:
            special_chip = "SA-1"
        elif (map_mode & 0x3F) == 0x32 or cartridge_type in {0x43, 0x45}:
            special_chip = "S-DD1"
        elif 0x13 <= cartridge_type <= 0x1A:
            special_chip = "SuperFX"
        title = header[:21].decode("ascii", "replace").strip(" \x00")
        if (cartridge_type & 0xF0) == 0xF0 and title.upper().startswith(
            ("MEGAMAN X", "ROCKMAN X")
        ):
            special_chip = "C4"
        result = {
            "title": title,
            "rom_hash": digest.hexdigest(),
            "region": "NTSC" if header[0x19] in {0, 1, 13} else "PAL/Other",
            "mapper": "HiROM" if hirom else "LoROM",
            "map_mode": f"0x{map_mode:02x}",
            "cartridge_type": f"0x{cartridge_type:02x}",
            "special_chip": special_chip,
            "compatibility": "Unsupported special chip" if special_chip else "Untested",
        }
        if len(self._snes_inspection_cache) >= 256:
            self._snes_inspection_cache.clear()
        self._snes_inspection_cache[cache_key] = dict(result)
        return result

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

    def smsgg_rom_target_root(self, profile, target_mode="device", simulator_target=None):
        mount_root = self.mount_root(profile, target_mode, simulator_target)
        if not mount_root:
            return ""
        return os.path.abspath(os.path.join(mount_root, SMSGG_ROM_TARGET_DIR))

    def smsgg_save_target_root(self, profile, target_mode="device", simulator_target=None):
        mount_root = self.mount_root(profile, target_mode, simulator_target)
        if not mount_root:
            return ""
        return os.path.abspath(os.path.join(mount_root, SMSGG_SAVE_TARGET_DIR))

    def gwatch_rom_target_root(self, profile, target_mode="device", simulator_target=None):
        mount_root = self.mount_root(profile, target_mode, simulator_target)
        if not mount_root:
            return ""
        return os.path.abspath(os.path.join(mount_root, GWATCH_ROM_TARGET_DIR))

    def gwatch_save_target_root(self, profile, target_mode="device", simulator_target=None):
        mount_root = self.mount_root(profile, target_mode, simulator_target)
        if not mount_root:
            return ""
        return os.path.abspath(os.path.join(mount_root, GWATCH_SAVE_TARGET_DIR))

    def snes_save_target_root(self, profile, target_mode="device", simulator_target=None):
        mount_root = self.mount_root(profile, target_mode, simulator_target)
        if not mount_root:
            return ""
        return os.path.abspath(os.path.join(mount_root, SNES_SAVE_TARGET_DIR))

    def target_root(self, profile, target_mode="device", simulator_target=None):
        return self.rom_target_root(profile, target_mode, simulator_target)

    def rom_abs_path_for_game(self, profile, game, target_mode="device", simulator_target=None):
        return self._rom_abs_path(profile, game, target_mode, simulator_target)

    def _rom_target_dir(self, profile, target_mode="device"):
        if target_mode == "simulator":
            return str(profile.get("games_simulator_target_dir") or ROM_TARGET_DIR).strip() or ROM_TARGET_DIR
        return str(profile.get("games_device_target_dir") or ROM_TARGET_DIR).strip() or ROM_TARGET_DIR

    def _rom_target_dir_for_game(self, profile, game, target_mode="device"):
        ext = os.path.splitext(str(game.get("filename") or ""))[1].lower()
        if ext in SNES_ROM_EXTENSIONS:
            return SNES_ROM_TARGET_DIR
        if ext in SMSGG_ROM_EXTENSIONS:
            return SMSGG_ROM_TARGET_DIR
        if ext in GWATCH_PACKAGE_EXTENSIONS:
            return GWATCH_ROM_TARGET_DIR
        return self._rom_target_dir(profile, target_mode)

    def _rom_destination_rel(self, profile, game, target_mode="device"):
        filename = str(game.get("filename") or "").strip()
        return f"{self._rom_target_dir_for_game(profile, game, target_mode).rstrip('/')}/{filename}"

    def _rom_abs_path(self, profile, game, target_mode="device", simulator_target=None):
        mount_root = self.mount_root(profile, target_mode, simulator_target)
        if not mount_root:
            return ""
        return os.path.abspath(os.path.join(mount_root, self._rom_destination_rel(profile, game, target_mode).lstrip("/")))

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
        stage_root = self._sync_stage_root(profile, target_mode)
        shutil.rmtree(stage_root, ignore_errors=True)
        os.makedirs(stage_root, exist_ok=True)
        snes_games = [
            game for game in games
            if os.path.splitext(game.get("filename") or "")[1].lower() in SNES_ROM_EXTENSIONS
        ]
        if snes_games:
            assets.extend(self._build_snes_runtime_assets(
                profile, target_mode, stage_root, snes_games
            ))
            systems_asset = self._build_snes_systems_index_asset(
                profile, target_mode, stage_root
            )
            if systems_asset:
                assets.append(systems_asset)
        cover_assets = {}
        for game in games:
            assets.append(
                {
                    "kind": "rom",
                    "source_rel": game["filename"],
                    "source_abs": game["source_path"],
                    "destination_rel": self._rom_destination_rel(profile, game, target_mode),
                    "exists": True,
                    "size": game["size"],
                }
            )
            cover_asset = self._build_cover_sync_asset(profile, game, stage_root, target_mode)
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
        assets.extend(
            self._build_system_manifest_assets(
                profile,
                target_mode,
                simulator_target,
                stage_root,
                add_games=games,
                include_missing=True,
                synced_cover_assets=cover_assets,
            )
        )
        config_asset = self._build_launcher_config_asset(profile, stage_root)
        if config_asset:
            assets.append(config_asset)
        return {
            "id": f"games-sync-{profile['id']}",
            "name": "RockPod Game Sync",
            "assets": assets,
        }

    def _build_snes_runtime_assets(self, profile, target_mode, stage_root,
                                   snes_games=None):
        repo_root = os.path.abspath(profile.get("source_repo_path") or "")
        build_dir = "build-sim-ipod6g" if target_mode == "simulator" else "build-hw-ipod6g"
        plugin_source = os.path.join(
            repo_root, build_dir, "apps", "plugins", "snes_lite", "snes_lite.rock"
        )
        assets = []
        if os.path.isfile(plugin_source):
            assets.append(
                {
                    "kind": "emulator_plugin",
                    "source_rel": os.path.relpath(plugin_source, repo_root),
                    "source_abs": plugin_source,
                    "destination_rel": SNES_PLUGIN_PATH,
                    "exists": True,
                    "size": os.path.getsize(plugin_source),
                }
            )
        launcher_source = os.path.join(
            repo_root, build_dir, "apps", "plugins", "rockboy_launcher.rock"
        )
        if os.path.isfile(launcher_source):
            assets.append(
                {
                    "kind": "games_launcher",
                    "source_rel": os.path.relpath(launcher_source, repo_root),
                    "source_abs": launcher_source,
                    "destination_rel": ".rockbox/rocks/games/rockboy_launcher.rock",
                    "exists": True,
                    "size": os.path.getsize(launcher_source),
                }
            )
        config_source = os.path.join(stage_root, "snes_lite.cfg")
        atomic_write_text(
            config_source,
            "# SNES Lite experimental defaults\n"
            "frameskip=auto\n"
            "audio=auto\n"
            "input_profile=0\n"
            "show_fps=0\n"
            "performance_mode=1\n"
            "video_mode=0\n"
            "performance_preset=0\n",
        )
        assets.append(
            {
                "kind": "emulator_config",
                "source_rel": "snes_lite.cfg",
                "source_abs": config_source,
                "destination_rel": SNES_CONFIG_PATH,
                "exists": True,
                "size": os.path.getsize(config_source),
            }
        )
        config_stage = os.path.join(stage_root, "snes_lite")
        os.makedirs(config_stage, exist_ok=True)
        for game in snes_games or []:
            filename = os.path.basename(str(game.get("filename") or ""))
            if not filename:
                continue
            stem = os.path.splitext(filename)[0]
            recommendation = self.recommend_snes_controls(
                game.get("internal_title") or game.get("title") or stem,
                game.get("genre") or "",
            )
            profile_id = int(game.get("input_profile",
                                      recommendation["input_profile"]))
            per_game_source = os.path.join(config_stage, f"{stem}.cfg")
            atomic_write_text(
                per_game_source,
                "# Generated by RockPod for this SNES game\n"
                f"# type={recommendation['game_type']}\n"
                "frameskip=auto\n"
                "audio=auto\n"
                f"input_profile={max(0, min(5, profile_id))}\n"
                "show_fps=0\n"
                "performance_mode=1\n"
                "video_mode=0\n"
                "performance_preset=0\n",
            )
            assets.append(
                {
                    "kind": "game_config",
                    "source_rel": os.path.relpath(per_game_source, stage_root),
                    "source_abs": per_game_source,
                    "destination_rel": f"{SNES_GAME_CONFIG_DIR}/{stem}.cfg",
                    "exists": True,
                    "size": os.path.getsize(per_game_source),
                    "game_type": recommendation["game_type"],
                    "input_profile": profile_id,
                }
            )
        for cache_path in (".rockbox/rocks/plugin.dat", ".rockbox/rocks/rb_plugins.dat"):
            assets.append(
                {
                    "kind": "plugin_cache",
                    "source_rel": "",
                    "source_abs": "",
                    "destination_rel": cache_path,
                    "exists": False,
                    "action": "remove",
                }
            )
        return assets

    def _build_snes_systems_index_asset(self, profile, target_mode, stage_root):
        mount_root = self.mount_root(profile, target_mode)
        current_path = os.path.join(mount_root, SYSTEMS_INDEX_PATH) if mount_root else ""
        header = "id\ttitle\tsubtitle\tplugin\tpath\tcover\tenabled\tsort"
        entries = []
        if current_path and os.path.isfile(current_path):
            with open(current_path, "r", encoding="utf-8", errors="replace") as handle:
                entries = [
                    line.rstrip("\r\n") for line in handle
                    if line.strip() and not line.startswith("id\t")
                ]
        entries = [line for line in entries if line.split("\t", 1)[0] != "snes"]
        entries.append(
            "snes\tSuper Nintendo\tSNES Lite experimental\t"
            "/.rockbox/rocks/games/snes_lite.rock\t/.rockbox/roms/snes\t"
            "/.rockbox/games/library/covers/systems/snes.bmp\t1\t12"
        )

        def sort_key(line):
            parts = line.split("\t")
            try:
                order = int(parts[7])
            except (IndexError, ValueError):
                order = 9999
            return order, parts[1].casefold() if len(parts) > 1 else line.casefold()

        staged = os.path.join(stage_root, "systems.tsv")
        atomic_write_text(staged, header + "\n" + "\n".join(sorted(entries, key=sort_key)) + "\n")
        return {
            "kind": "systems_index",
            "source_rel": "systems.tsv",
            "source_abs": staged,
            "destination_rel": SYSTEMS_INDEX_PATH,
            "exists": True,
            "size": os.path.getsize(staged),
        }

    def build_remove_bundle(self, profile, games, target_mode="device", simulator_target=None):
        assets = []
        for game in games:
            assets.append(
                {
                    "kind": "rom",
                    "source_rel": "",
                    "source_abs": "",
                    "destination_rel": self._rom_destination_rel(profile, game, target_mode),
                    "exists": False,
                    "action": "remove",
                }
            )
            stem = os.path.splitext(game["filename"])[0]
            cover_target_dir = self._rom_target_dir_for_game(profile, game, target_mode).rstrip("/")
            for extension in SYNC_COVER_EXTENSIONS:
                assets.append(
                    {
                        "kind": "cover",
                        "source_rel": "",
                        "source_abs": "",
                        "destination_rel": f"{cover_target_dir}/{stem}{extension}",
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
        assets.extend(
            self._build_system_manifest_assets(
                profile,
                target_mode,
                simulator_target,
                self._sync_stage_root(profile, target_mode),
                remove_filenames=remove_filenames,
            )
        )
        config_asset = self._build_launcher_config_asset(profile, self._sync_stage_root(profile, target_mode))
        if config_asset:
            assets.append(config_asset)
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

    def build_system_manifest_bundle(self, profile, target_mode="device", simulator_target=None, games=None, include_missing=False):
        stage_root = self._sync_stage_root(profile, target_mode)
        os.makedirs(stage_root, exist_ok=True)
        assets = self._build_system_manifest_assets(
            profile,
            target_mode,
            simulator_target,
            stage_root,
            add_games=(games or []),
            include_missing=include_missing,
        )
        return {
            "id": f"games-system-manifests-{profile['id']}-{target_mode}",
            "name": "Game System Manifests",
            "assets": assets,
        }

    def build_launcher_config_bundle(self, profile, target_mode="device"):
        stage_root = self._sync_stage_root(profile, target_mode)
        os.makedirs(stage_root, exist_ok=True)
        asset = self._build_launcher_config_asset(profile, stage_root, force=True)
        return {
            "id": f"games-launcher-config-{profile['id']}-{target_mode}",
            "name": "Game Cover Flow Options",
            "assets": [asset] if asset else [],
        }

    def backup_saves(self, profile, games, target_mode="device", simulator_target=None):
        target_root = self.save_target_root(profile, target_mode, simulator_target)
        mount_root = self.mount_root(profile, target_mode, simulator_target)
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
        if not target_root and not mount_root:
            return {"success": False, "backup_dir": backup_root, "copied": copied, "message": "Target game directory does not exist"}
        for game in games:
            for source in self._save_candidate_paths_for_target(profile, game, target_mode, simulator_target):
                if not os.path.isfile(source):
                    continue
                dest = os.path.join(backup_root, self._backup_rel_for_save_source(source))
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
        if not save_root and not self.mount_root(profile, target_mode, simulator_target):
            return {"success": False, "restored": [], "message": "Target save directory does not exist", "backup_dir": backup_dir or ""}
        backup_dir = backup_dir or self.latest_save_backup_dir(profile, target_mode)
        if not backup_dir or not os.path.isdir(backup_dir):
            return {"success": False, "restored": [], "message": "No save backup found", "backup_dir": backup_dir or ""}
        restored = []
        for game in games:
            for source, dest in self._backup_restore_pairs_for_target(profile, backup_dir, game, target_mode, simulator_target):
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
        if not save_root and not self.mount_root(profile, target_mode, simulator_target):
            return {"success": False, "message": "Target save directory does not exist", "archive_path": archive_path}
        archive_path = os.path.abspath(archive_path)
        os.makedirs(os.path.dirname(archive_path), exist_ok=True)
        written = 0
        with zipfile.ZipFile(archive_path, "w", compression=zipfile.ZIP_DEFLATED) as bundle:
            for game in games:
                for source in self._save_candidate_paths_for_target(profile, game, target_mode, simulator_target):
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
                if not filename.lower().endswith((".sav", ".rtc", ".sn", ".srm")):
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

    def _save_exists_for_target(self, profile, game, target_mode="device", simulator_target=None):
        ext = os.path.splitext(str(game.get("filename") or ""))[1].lower()
        if ext in SNES_ROM_EXTENSIONS:
            return any(
                os.path.isfile(path)
                for path in self._snes_save_candidate_paths(
                    self.snes_save_target_root(profile, target_mode, simulator_target),
                    game,
                )
            )
        if ext in SMSGG_ROM_EXTENSIONS:
            save_root = self.smsgg_save_target_root(profile, target_mode, simulator_target)
            state_root = os.path.abspath(
                os.path.join(
                    self.mount_root(profile, target_mode, simulator_target),
                    SMSGG_STATE_TARGET_DIR,
                )
            ) if self.mount_root(profile, target_mode, simulator_target) else ""
            return (
                any(os.path.isfile(path) for path in self._smsgg_save_candidate_paths(save_root, game)) or
                any(os.path.isfile(path) for path in self._smsgg_save_candidate_paths(state_root, game))
            )
        if ext in GWATCH_PACKAGE_EXTENSIONS:
            return any(
                os.path.isfile(path)
                for path in self._gwatch_save_candidate_paths(
                    self.gwatch_save_target_root(profile, target_mode, simulator_target),
                    game,
                )
            )
        return self._save_exists(self.save_target_root(profile, target_mode, simulator_target), game)

    def _save_inventory(self, profile, target_mode="device", simulator_target=None):
        mount_root = self.mount_root(profile, target_mode, simulator_target)

        def names(path):
            if not path or not os.path.isdir(path):
                return set()
            try:
                return set(os.listdir(path))
            except OSError:
                return set()

        smsgg_names = names(
            self.smsgg_save_target_root(profile, target_mode, simulator_target)
        )
        if mount_root:
            smsgg_names.update(names(os.path.join(mount_root, SMSGG_STATE_TARGET_DIR)))
        return {
            "rockboy": names(
                self.save_target_root(profile, target_mode, simulator_target)
            ),
            "smsgg": smsgg_names,
            "gwatch": names(
                self.gwatch_save_target_root(profile, target_mode, simulator_target)
            ),
            "snes": names(
                self.snes_save_target_root(profile, target_mode, simulator_target)
            ),
        }

    @staticmethod
    def _save_exists_in_inventory(game, inventory, save_stem=""):
        ext = os.path.splitext(str(game.get("filename") or ""))[1].lower()
        filename_stem = os.path.splitext(game.get("filename") or "game")[0]
        if ext in SNES_ROM_EXTENSIONS:
            return filename_stem + ".srm" in inventory.get("snes", set())
        if ext in SMSGG_ROM_EXTENSIONS:
            safe_stem = re.sub(
                r"[^A-Za-z0-9._ -]+", "_", filename_stem
            ).strip() or "game"
            return any(
                name.startswith(safe_stem + "-")
                and name.lower().endswith((".sav", ".state"))
                for name in inventory.get("smsgg", set())
            )
        if ext in GWATCH_PACKAGE_EXTENSIONS:
            safe_stem = re.sub(
                r"[^A-Za-z0-9._ -]+", "_", filename_stem
            ).strip() or "game"
            return any(
                name.startswith(safe_stem)
                and name.lower().endswith((".state", ".sav"))
                for name in inventory.get("gwatch", set())
            )
        stem = save_stem or filename_stem
        return any(
            stem + extension in inventory.get("rockboy", set())
            for extension in (".sav", ".rtc", ".sn")
        )

    def _build_cover_sync_asset(self, profile, game, stage_root, target_mode):
        cover_source = os.path.abspath(game.get("cover_path") or "")
        if not cover_source or not os.path.isfile(cover_source):
            return None
        stem = os.path.splitext(game.get("filename") or "game")[0]
        rom_target_dir = self._rom_target_dir_for_game(profile, game, target_mode).rstrip("/")
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
                    "plugin_param": "",
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

    def _backup_restore_pairs_for_target(self, profile, backup_root, game, target_mode="device", simulator_target=None):
        ext = os.path.splitext(str(game.get("filename") or ""))[1].lower()
        if ext in SNES_ROM_EXTENSIONS:
            for source in self._save_candidate_paths_for_target(profile, game, target_mode, simulator_target):
                yield os.path.join(backup_root, "snes", os.path.basename(source)), source
            return
        if ext in SMSGG_ROM_EXTENSIONS:
            for source in self._save_candidate_paths_for_target(profile, game, target_mode, simulator_target):
                rel_dir = os.path.basename(os.path.dirname(source))
                yield (
                    os.path.join(backup_root, rel_dir, os.path.basename(source)),
                    source,
            )
            return
        if ext in GWATCH_PACKAGE_EXTENSIONS:
            for source in self._save_candidate_paths_for_target(profile, game, target_mode, simulator_target):
                yield (
                    os.path.join(backup_root, "saves", os.path.basename(source)),
                    source,
                )
            return
        yield from self._backup_restore_pairs(
            backup_root,
            self.save_target_root(profile, target_mode, simulator_target),
            game,
        )

    def _save_candidate_paths(self, target_root, game):
        stem = self._save_stem_for_game(game)
        return [os.path.join(target_root, stem + extension) for extension in (".sav", ".rtc", ".sn")]

    @staticmethod
    def _backup_rel_for_save_source(source):
        parent = os.path.basename(os.path.dirname(source))
        if parent in {"saves", "states", "snes"}:
            return os.path.join(parent, os.path.basename(source))
        return os.path.basename(source)

    def _save_candidate_paths_for_target(self, profile, game, target_mode="device", simulator_target=None):
        ext = os.path.splitext(str(game.get("filename") or ""))[1].lower()
        if ext in SNES_ROM_EXTENSIONS:
            return self._snes_save_candidate_paths(
                self.snes_save_target_root(profile, target_mode, simulator_target),
                game,
            )
        if ext in SMSGG_ROM_EXTENSIONS:
            mount_root = self.mount_root(profile, target_mode, simulator_target)
            if not mount_root:
                return []
            save_root = os.path.join(mount_root, SMSGG_SAVE_TARGET_DIR)
            state_root = os.path.join(mount_root, SMSGG_STATE_TARGET_DIR)
            return (
                self._smsgg_save_candidate_paths(save_root, game) +
                self._smsgg_save_candidate_paths(state_root, game)
            )
        if ext in GWATCH_PACKAGE_EXTENSIONS:
            return self._gwatch_save_candidate_paths(
                self.gwatch_save_target_root(profile, target_mode, simulator_target),
                game,
            )
        return self._save_candidate_paths(
            self.save_target_root(profile, target_mode, simulator_target),
            game,
        )

    @staticmethod
    def _smsgg_save_candidate_paths(target_root, game):
        if not target_root or not os.path.isdir(target_root):
            return []
        safe_stem = re.sub(r"[^A-Za-z0-9._ -]+", "_", os.path.splitext(game.get("filename") or "game")[0]).strip() or "game"
        candidates = []
        for name in os.listdir(target_root):
            if name.startswith(safe_stem + "-") and name.lower().endswith((".sav", ".state")):
                candidates.append(os.path.join(target_root, name))
        return candidates

    @staticmethod
    def _gwatch_save_candidate_paths(target_root, game):
        if not target_root or not os.path.isdir(target_root):
            return []
        stem = os.path.splitext(game.get("filename") or "game")[0]
        safe_stem = re.sub(r"[^A-Za-z0-9._ -]+", "_", stem).strip() or "game"
        candidates = []
        for name in os.listdir(target_root):
            lower = name.lower()
            if name.startswith(safe_stem) and lower.endswith((".state", ".sav")):
                candidates.append(os.path.join(target_root, name))
        return candidates

    @staticmethod
    def _snes_save_candidate_paths(target_root, game):
        if not target_root:
            return []
        stem = os.path.splitext(game.get("filename") or "game")[0]
        return [os.path.join(target_root, stem + ".srm")]

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
            filename = os.path.basename(entry.get("plugin_param") or entry.get("rom_path") or "")
            if filename:
                entries_by_filename[filename] = dict(entry)
        for entry in indexed_entries:
            filename = os.path.basename(entry.get("plugin_param") or entry.get("rom_path") or "")
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
        filename = game.get("filename") or ""
        stem = os.path.splitext(filename)[0]
        ext = os.path.splitext(filename)[1].lower()
        rom_path = f"/{self._rom_target_dir_for_game(profile, game, target_mode).rstrip('/')}/{filename}"
        if ext in GWATCH_PACKAGE_EXTENSIONS:
            plugin_path = f"/{GWATCH_PLUGIN_PATH}"
        elif ext in SNES_ROM_EXTENSIONS:
            plugin_path = f"/{SNES_PLUGIN_PATH}"
        else:
            plugin_path = f"/{SMSGG_PLUGIN_PATH}"
        uses_plugin_launcher = (
            ext in SMSGG_ROM_EXTENSIONS
            or ext in GWATCH_PACKAGE_EXTENSIONS
            or ext in SNES_ROM_EXTENSIONS
        )
        metadata = self.cached_metadata_for_game(profile, game)
        return {
            "title": game.get("title") or current_entry.get("title") or self._launcher_title_from_path(filename),
            "rom_path": plugin_path if uses_plugin_launcher else rom_path,
            "plugin_param": rom_path if uses_plugin_launcher else "",
            "cover_path": (
                f"/{self._rom_target_dir_for_game(profile, game, target_mode).rstrip('/')}/{stem}.bmp"
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
            "# title\\trom_path\\tcover_path\\tfavorite\\tsave_hint\\tyear\\tgenre\\tpublisher\\tdeveloper\\tdescription\\tplugin_param",
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
                        self._index_path(entry.get("plugin_param"), mount_root),
                    ]
                )
            )
        atomic_write_text(index_path, "\n".join(lines) + "\n")

    def _build_launcher_config_asset(self, profile, stage_root, force=False):
        show_doom = bool(profile.get("games_show_builtin_doom", True))
        show_stickrpg = bool(profile.get("games_show_builtin_stickrpg", True))
        show_runescape = bool(profile.get("games_show_builtin_runescape", True))
        if not force and show_doom and show_stickrpg and show_runescape:
            return None
        os.makedirs(stage_root, exist_ok=True)
        staged_abs = os.path.join(stage_root, "rockboy_launcher.cfg")
        lines = [
            "# Game Cover Flow options",
            f"show_builtin_doom={1 if show_doom else 0}",
            f"show_builtin_stickrpg={1 if show_stickrpg else 0}",
            f"show_builtin_runescape={1 if show_runescape else 0}",
        ]
        atomic_write_text(staged_abs, "\n".join(lines) + "\n")
        return {
            "kind": "config",
            "source_rel": "rockboy_launcher.cfg",
            "source_abs": staged_abs,
            "destination_rel": LAUNCHER_CONFIG_RELATIVE_PATH,
            "exists": True,
            "size": os.stat(staged_abs).st_size,
        }

    def _build_system_manifest_assets(
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
        system_ids = set()
        for game in add_games or []:
            system_id = self._system_manifest_id_for_game(game)
            if system_id:
                system_ids.add(system_id)
        for filename in remove_filenames or set():
            system_id = self._system_manifest_id_for_filename(filename)
            if system_id:
                system_ids.add(system_id)

        assets = []
        for system_id in sorted(system_ids):
            asset = self._build_system_manifest_asset(
                profile,
                target_mode,
                simulator_target,
                stage_root,
                system_id,
                add_games=add_games,
                remove_filenames=remove_filenames,
                include_missing=include_missing,
                synced_cover_assets=synced_cover_assets,
            )
            if asset:
                assets.append(asset)
        return assets

    def _build_system_manifest_asset(
        self,
        profile,
        target_mode,
        simulator_target,
        stage_root,
        system_id,
        add_games=None,
        remove_filenames=None,
        include_missing=False,
        synced_cover_assets=None,
    ):
        mount_root = self.mount_root(profile, target_mode, simulator_target)
        destination_rel = SYSTEM_MANIFEST_RELATIVE_PATHS.get(system_id)
        if not destination_rel:
            return None

        entries = self._merged_system_manifest_entries(
            profile,
            target_mode,
            simulator_target,
            system_id,
            add_games=add_games,
            remove_filenames=remove_filenames,
            include_missing=include_missing,
            synced_cover_assets=synced_cover_assets,
        )
        if not entries:
            return {
                "kind": "system_manifest",
                "source_rel": "",
                "source_abs": "",
                "destination_rel": destination_rel,
                "exists": False,
                "action": "remove",
            } if mount_root else None

        os.makedirs(stage_root, exist_ok=True)
        staged_abs = os.path.join(stage_root, f"{system_id}-games.tsv")
        self._write_system_manifest(staged_abs, mount_root, entries)
        return {
            "kind": "system_manifest",
            "source_rel": os.path.basename(staged_abs),
            "source_abs": staged_abs,
            "destination_rel": destination_rel,
            "exists": True,
            "size": os.stat(staged_abs).st_size,
        }

    def _merged_system_manifest_entries(
        self,
        profile,
        target_mode,
        simulator_target,
        system_id,
        add_games=None,
        remove_filenames=None,
        include_missing=False,
        synced_cover_assets=None,
    ):
        mount_root = self.mount_root(profile, target_mode, simulator_target)
        entries_by_filename = {}
        for entry in self._scan_system_manifest_files(profile, target_mode, simulator_target, system_id):
            filename = os.path.basename(entry.get("file_path") or "")
            if filename:
                entries_by_filename[filename] = dict(entry)

        manifest_rel = SYSTEM_MANIFEST_RELATIVE_PATHS.get(system_id, "")
        if mount_root and manifest_rel:
            manifest_path = os.path.join(mount_root, manifest_rel)
            for entry in self.parse_system_manifest(manifest_path):
                filename = os.path.basename(entry.get("file_path") or "")
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
            if self._system_manifest_id_for_game(game) != system_id:
                continue
            filename = str(game.get("filename") or "").strip()
            if not filename:
                continue
            if not include_missing and filename not in entries_by_filename:
                continue
            current = entries_by_filename.get(filename, {})
            entries_by_filename[filename] = self._system_manifest_entry_for_local_game(
                profile,
                game,
                target_mode,
                current_entry=current,
                synced_cover_asset=synced_cover_assets.get(filename),
            )

        return self._sort_launcher_entries(list(entries_by_filename.values()))

    def parse_system_manifest(self, manifest_path):
        manifest_path = os.path.abspath(manifest_path or "")
        if not manifest_path or not os.path.isfile(manifest_path):
            return []
        index_dir = os.path.dirname(manifest_path)
        mount_root = self._system_manifest_mount_root(manifest_path)
        entries = []
        with open(manifest_path, "r", encoding="utf-8") as handle:
            for raw_line in handle:
                line = raw_line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = [part.strip() for part in raw_line.rstrip("\r\n").split("\t")]
                parts.extend([""] * (SYSTEM_MANIFEST_COLUMNS - len(parts)))
                entry_id, title, file_path, cover_path, favorite, last_played, haptic_profile = parts[:SYSTEM_MANIFEST_COLUMNS]
                if entry_id.lower() == "id":
                    continue
                resolved_file = self._launcher_resolve_path(index_dir, file_path, mount_root)
                if not resolved_file or not os.path.isfile(resolved_file):
                    continue
                entries.append(
                    {
                        "id": entry_id,
                        "title": title or self._launcher_title_from_path(resolved_file),
                        "file_path": resolved_file,
                        "cover_path": self._launcher_resolve_path(index_dir, cover_path, mount_root),
                        "favorite": self._launcher_parse_bool(favorite),
                        "last_played": last_played,
                        "haptic_profile": haptic_profile,
                    }
                )
        return self._sort_launcher_entries(entries)

    def _scan_system_manifest_files(self, profile, target_mode, simulator_target, system_id):
        root = self._system_manifest_scan_root(profile, target_mode, simulator_target, system_id)
        extensions = SYSTEM_MANIFEST_EXTENSIONS.get(system_id, set())
        entries = []
        for entry in self.scan_launcher_roms(root):
            file_path = entry.get("rom_path") or ""
            if os.path.splitext(file_path)[1].lower() not in extensions:
                continue
            entries.append(
                {
                    "id": self._system_manifest_entry_id(file_path),
                    "title": entry.get("title") or self._launcher_title_from_path(file_path),
                    "file_path": file_path,
                    "cover_path": entry.get("cover_path") or "",
                    "favorite": bool(entry.get("favorite")),
                    "last_played": "",
                    "haptic_profile": "",
                }
            )
        return entries

    def _system_manifest_entry_for_local_game(self, profile, game, target_mode, current_entry=None, synced_cover_asset=None):
        current_entry = dict(current_entry or {})
        filename = str(game.get("filename") or "").strip()
        stem = os.path.splitext(filename)[0]
        file_path = f"/{self._rom_target_dir_for_game(profile, game, target_mode).rstrip('/')}/{filename}"
        cover_path = ""
        if synced_cover_asset:
            cover_path = f"/{self._rom_target_dir_for_game(profile, game, target_mode).rstrip('/')}/{stem}.bmp"
        else:
            cover_path = current_entry.get("cover_path", "")
        return {
            "id": current_entry.get("id") or self._system_manifest_entry_id(filename),
            "title": game.get("title") or current_entry.get("title") or self._launcher_title_from_path(filename),
            "file_path": file_path,
            "cover_path": cover_path,
            "favorite": bool(current_entry.get("favorite")),
            "last_played": current_entry.get("last_played", ""),
            "haptic_profile": current_entry.get("haptic_profile", ""),
        }

    def _write_system_manifest(self, manifest_path, mount_root, entries):
        os.makedirs(os.path.dirname(manifest_path), exist_ok=True)
        lines = [
            "# RockPod game system manifest",
            "id\ttitle\tfile\tcover\tfavorite\tlast_played\thaptic_profile",
        ]
        for entry in entries:
            lines.append(
                "\t".join(
                    [
                        self._sanitize_index_field(entry.get("id") or self._system_manifest_entry_id(entry.get("file_path"))),
                        self._sanitize_index_field(entry.get("title")),
                        self._index_path(entry.get("file_path"), mount_root),
                        self._index_path(entry.get("cover_path"), mount_root),
                        "1" if entry.get("favorite") else "0",
                        self._sanitize_index_field(entry.get("last_played")),
                        self._sanitize_index_field(entry.get("haptic_profile")),
                    ]
                )
            )
        atomic_write_text(manifest_path, "\n".join(lines) + "\n")

    def _system_manifest_scan_root(self, profile, target_mode, simulator_target, system_id):
        mount_root = self.mount_root(profile, target_mode, simulator_target)
        if not mount_root:
            return ""
        if system_id == "smsgg":
            return os.path.abspath(os.path.join(mount_root, SMSGG_ROM_TARGET_DIR))
        if system_id == "gwatch":
            return os.path.abspath(os.path.join(mount_root, GWATCH_ROM_TARGET_DIR))
        if system_id == "snes":
            return os.path.abspath(os.path.join(mount_root, SNES_ROM_TARGET_DIR))
        if system_id == "nes":
            return self.rom_target_root(profile, target_mode, simulator_target)
        return ""

    @staticmethod
    def _system_manifest_id_for_filename(filename):
        ext = os.path.splitext(str(filename or ""))[1].lower()
        if ext == ".nes":
            return "nes"
        if ext in SMSGG_ROM_EXTENSIONS:
            return "smsgg"
        if ext in GWATCH_PACKAGE_EXTENSIONS:
            return "gwatch"
        if ext in SNES_ROM_EXTENSIONS:
            return "snes"
        return ""

    def _system_manifest_id_for_game(self, game):
        return self._system_manifest_id_for_filename(game.get("filename"))

    @staticmethod
    def _system_manifest_entry_id(path):
        stem = os.path.splitext(os.path.basename(str(path or "")))[0]
        return re.sub(r"[^A-Za-z0-9._-]+", "-", stem).strip("-._") or "game"

    @staticmethod
    def _system_manifest_mount_root(manifest_path):
        manifest_abs = os.path.abspath(manifest_path or "")
        for relative_path in SYSTEM_MANIFEST_RELATIVE_PATHS.values():
            relative_manifest = os.path.normpath(relative_path)
            if not manifest_abs.endswith(relative_manifest):
                continue
            mount_root = manifest_abs
            for _part in relative_manifest.split(os.sep):
                mount_root = os.path.dirname(mount_root)
            return mount_root
        return ""

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
        atomic_write_json(cache_path, payload)
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
        platform = ROM_PLATFORM_CACHE_DIRS.get(ext, "gameboy")
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
            filename = os.path.basename(str(entry.get("plugin_param") or entry.get("rom_path") or "").strip())
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
