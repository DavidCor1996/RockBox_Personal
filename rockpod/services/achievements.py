"""Build the offline achievements database installed by RockPod.

The device database is deliberately network-free.  RockPod may enrich it
with official RetroAchievements data when the user supplies a Web API key,
but every launchable game receives a local baseline set either way.
"""

from __future__ import annotations

import csv
import hashlib
import io
import json
import os
import shutil
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path
from urllib.parse import urlencode
from urllib.request import Request, urlopen

from PIL import Image, ImageEnhance, ImageOps, UnidentifiedImageError

from services.file_safety import atomic_write_text
from services.xbox_avatar import XboxAvatarService


ACHIEVEMENTS_ROOT = ".rockbox/achievements"
CATALOG_COLUMNS = (
    "game_key", "title", "console", "set_kind", "cover_path",
    "achievement_file", "unlocked", "total", "earned_points",
    "total_points", "last_played", "launch_target", "ra_game_id",
    "ra_hash",
)
ACHIEVEMENT_COLUMNS = (
    "id", "title", "description", "points", "badge_unlocked",
    "badge_locked", "state", "measured", "unlock_time", "source",
    "memaddr",
)
BASELINE_ACHIEVEMENTS = (
    ("local-first-play", "First Play", "Launch this game for the first time.", 5),
    ("local-15-minutes", "Getting Started", "Play for a total of 15 minutes.", 10),
    ("local-60-minutes", "Settled In", "Play for a total of 60 minutes.", 20),
    ("local-10-sessions", "Regular Player", "Start this game 10 times.", 15),
)
CONSOLE_IDS = {
    ".md": 1, ".gen": 1, ".bin": 1, ".smd": 1,
    ".sfc": 3, ".smc": 3,
    ".gb": 4, ".gbc": 6,
    ".nes": 7,
    ".sms": 11, ".sg": 11,
    ".gg": 15,
    ".min": 24,
}
OFFLINE_RUNTIME_EXTENSIONS = frozenset(CONSOLE_IDS)
CONSOLE_NAMES = {
    1: "Genesis", 3: "Super Nintendo", 4: "Game Boy",
    6: "Game Boy Color", 7: "NES", 11: "Master System",
    15: "Game Gear", 24: "PokeMini",
}
SYSTEM_ALIASES = {
    "arduboy": "Arduboy", "doom": "DOOM", "genesis": "Genesis",
    "gwatch": "Game & Watch", "ipodgames": "iPod Games",
    "n64": "Nintendo 64", "native": "Rockbox", "nes": "NES",
    "pokemini": "PokeMini", "ps1": "PlayStation", "smsgg": "Sega 8-bit",
    "snes": "Super Nintendo",
}


def _clean(value):
    return " ".join(str(value or "").replace("\t", " ").splitlines()).strip()


def _runtime_definition(value):
    """Return a usable rcheevos rule, never an API definition hash."""
    value = _clean(value)
    return value if "0x" in value.casefold() else ""


def _has_complete_runtime_set(details):
    raw = (details or {}).get("Achievements") or {}
    items = list(raw.values()) if isinstance(raw, dict) else list(raw)
    return bool(items) and all(
        _runtime_definition(item.get("MemAddr")) for item in items
    )


def _device_path(root, value):
    value = str(value or "").strip()
    if not value:
        return ""
    return os.path.join(root, value.lstrip("/"))


def _device_rel(value):
    value = str(value or "").replace("\\", "/").strip()
    return "/" + value.lstrip("/") if value else ""


def _sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


@dataclass
class AchievementGame:
    key: str
    title: str
    console: str
    launch_target: str
    content_path: str
    cover_path: str
    content_abs: str
    cover_abs: str
    console_id: int = 0
    last_played: str = ""


class AchievementSyncService:
    """Discover games and build an atomic, fully offline generation."""

    def __init__(self, repo_root, api_key="", username="", opener=None,
                 avatar_profile=None):
        self.repo_root = os.path.abspath(repo_root)
        self.api_key = str(api_key or os.environ.get(
            "ROCKPOD_RA_WEB_API_KEY", "")).strip()
        self.username = str(username or os.environ.get(
            "ROCKPOD_RA_USERNAME", "")).strip()
        self._opener = opener or urlopen
        self.avatar_profile = dict(avatar_profile or {})
        self.cache_root = os.path.join(
            self.repo_root, "rockpod", ".cache", "retroachievements")

    def discover_games(self, mount_root, add_games=None):
        mount_root = os.path.abspath(mount_root)
        records = []
        main_manifest = os.path.join(
            mount_root, ".rockbox", "rocks", "games",
            "rockboy_launcher", "games.tsv")
        if os.path.isfile(main_manifest):
            records.extend(self._read_launcher_manifest(mount_root, main_manifest))

        games_root = os.path.join(mount_root, ".rockbox", "games")
        for manifest in sorted(Path(games_root).glob("*/games.tsv")):
            records.extend(self._read_system_manifest(
                mount_root, str(manifest), manifest.parent.name))
        records.extend(self._discover_native_games(mount_root))
        records.extend(self._discover_special_games(mount_root))

        for game in add_games or []:
            filename = str(game.get("filename") or "")
            source = os.path.abspath(str(game.get("source_path") or ""))
            if not filename or not os.path.isfile(source):
                continue
            content_path = self._new_game_destination(game)
            cover_abs = os.path.abspath(str(game.get("cover_path") or ""))
            ext = os.path.splitext(filename)[1].lower()
            console_id = CONSOLE_IDS.get(ext, 0)
            records.append(self._make_game(
                title=game.get("title") or os.path.splitext(filename)[0],
                system=CONSOLE_NAMES.get(console_id, "Games"),
                launch_target=content_path,
                content_path=content_path,
                cover_path="",
                content_abs=source,
                cover_abs=cover_abs if os.path.isfile(cover_abs) else "",
            ))

        unique = {}
        for game in records:
            identity = game.content_path.casefold() or game.launch_target.casefold()
            if not identity or "[bios]" in game.title.casefold():
                continue
            existing = unique.get(identity)
            if existing is None or (not existing.cover_abs and game.cover_abs):
                unique[identity] = game
        return sorted(unique.values(), key=lambda item: (
            item.console.casefold(), item.title.casefold(), item.key))

    def _discover_native_games(self, mount_root):
        records = []
        cover_root = os.path.join(
            mount_root, ".rockbox", "games", "library", "covers", "native")
        for cover in sorted(Path(cover_root).glob("*.bmp")):
            stem = cover.stem
            plugin_path = f"/.rockbox/rocks/games/{stem}.rock"
            plugin_abs = _device_path(mount_root, plugin_path)
            if not os.path.isfile(plugin_abs):
                continue
            title = stem.replace("_", " ").replace("-", " ")
            title = " ".join(word.upper() if word.casefold() in {
                "2048", "doom", "sudoku"} else word.title()
                for word in title.split())
            records.append(self._make_game(
                title, "native", plugin_path, plugin_path,
                "/" + cover.relative_to(mount_root).as_posix(),
                plugin_abs, str(cover)))
        return records

    def _discover_special_games(self, mount_root):
        special = (
            ("Super Mario 64", "n64", "/.rockbox/rocks/games/sm64.rock",
             "/.rockbox/games/library/covers/n64/Super Mario 64 (USA).bmp"),
            ("Stick RPG", "flash", "/.rockbox/flash/stickrpg/stickrpg.swf",
             "/.rockbox/ipodjs/steam/covers/Stick RPG.bmp"),
        )
        records = []
        for title, system, content, cover in special:
            content_abs = _device_path(mount_root, content)
            if not os.path.isfile(content_abs):
                continue
            records.append(self._make_game(
                title, system, content, content, cover, content_abs,
                _device_path(mount_root, cover)))
        return records

    def _read_launcher_manifest(self, mount_root, path):
        records = []
        with open(path, "r", encoding="utf-8-sig", errors="replace") as handle:
            for line in handle:
                if not line.strip() or line.startswith("#"):
                    continue
                row = line.rstrip("\r\n").split("\t")
                if len(row) < 2:
                    continue
                row += [""] * (11 - len(row))
                launch_target = row[10] or row[1]
                content_path = row[10] or row[1]
                content_abs = _device_path(mount_root, content_path)
                if not os.path.isfile(content_abs):
                    continue
                records.append(self._make_game(
                    row[0], "", launch_target, content_path, row[2],
                    content_abs, _device_path(mount_root, row[2])))
        return records

    def _read_system_manifest(self, mount_root, path, system):
        records = []
        with open(path, "r", encoding="utf-8-sig", errors="replace") as handle:
            reader = csv.DictReader(
                (line for line in handle if line.strip() and not line.startswith("#")),
                delimiter="\t")
            for row in reader:
                content_path = row.get("file", "")
                content_abs = _device_path(mount_root, content_path)
                if not os.path.isfile(content_abs):
                    continue
                records.append(self._make_game(
                    row.get("title") or row.get("id"), system, content_path,
                    content_path, row.get("cover", ""), content_abs,
                    _device_path(mount_root, row.get("cover", "")),
                    row.get("last_played", "")))
        return records

    def _make_game(self, title, system, launch_target, content_path,
                   cover_path, content_abs, cover_abs, last_played=""):
        ext = os.path.splitext(content_path)[1].lower()
        console_id = CONSOLE_IDS.get(ext, 0)
        console = CONSOLE_NAMES.get(console_id) or SYSTEM_ALIASES.get(
            system, system.replace("_", " ").title() or "Games")
        identity = f"{console}\0{_device_rel(content_path).casefold()}"
        key = hashlib.sha256(identity.encode("utf-8")).hexdigest()[:20]
        return AchievementGame(
            key=key, title=_clean(title), console=console,
            launch_target=_device_rel(launch_target),
            content_path=_device_rel(content_path),
            cover_path=_device_rel(cover_path), content_abs=content_abs,
            cover_abs=cover_abs if os.path.isfile(cover_abs) else "",
            console_id=console_id, last_played=_clean(last_played),
        )

    @staticmethod
    def _new_game_destination(game):
        ext = os.path.splitext(str(game.get("filename") or ""))[1].lower()
        folders = {
            ".sfc": "/.rockbox/roms/snes", ".smc": "/.rockbox/roms/snes",
            ".gg": "/.rockbox/games/smsgg/roms",
            ".sms": "/.rockbox/games/smsgg/roms",
            ".sg": "/.rockbox/games/smsgg/roms",
            ".mgw": "/.rockbox/games/gwatch/roms",
            ".gw": "/.rockbox/games/gwatch/roms",
            ".gwz": "/.rockbox/games/gwatch/roms",
            ".md": "/.rockbox/games/genesis/roms",
            ".gen": "/.rockbox/games/genesis/roms",
            ".smd": "/.rockbox/games/genesis/roms",
        }
        folder = folders.get(ext, "/gameboy")
        return f"{folder}/{os.path.basename(str(game.get('filename') or ''))}"

    def build_sync_assets(self, mount_root, stage_root, add_games=None):
        games = self.discover_games(mount_root, add_games)
        if not games:
            return [], {"games": 0, "official": 0, "baseline": 0}
        stage_root = os.path.abspath(stage_root)
        work_root = os.path.join(stage_root, "achievements")
        shutil.rmtree(work_root, ignore_errors=True)
        os.makedirs(work_root, exist_ok=True)

        prepared = []
        official_count = 0
        for game in games:
            has_cached_map = os.path.isfile(os.path.join(
                self.cache_root, f"console-{game.console_id}.json"
            ))
            official = self._official_set(game) \
                if self.api_key or has_cached_map else None
            if official and not _has_complete_runtime_set(official):
                official = None
            if official:
                official_count += 1
            prepared.append((game, official))

        generation_hash = hashlib.sha256()
        for game, official in prepared:
            generation_hash.update(json.dumps({
                "key": game.key,
                "title": game.title,
                "console": game.console,
                "launch_target": game.launch_target,
                "last_played": game.last_played,
                "cover_sha256": _sha256(self._art_source(mount_root, game)),
                "official": official,
            }, sort_keys=True, separators=(",", ":")).encode("utf-8"))
        generation = generation_hash.hexdigest()[:16]
        generation_root = os.path.join(work_root, "generations", generation)
        os.makedirs(generation_root, exist_ok=True)

        catalog_rows = []
        provenance_rows = []
        for game, official in prepared:
            row, provenance = self._write_game(
                mount_root, generation_root, generation, game, official)
            catalog_rows.append(row)
            provenance_rows.extend(provenance)

        self._write_tsv(os.path.join(generation_root, "catalog.tsv"),
                        CATALOG_COLUMNS, catalog_rows)
        self._write_tsv(
            os.path.join(generation_root, "provenance.tsv"),
            ("game_key", "asset", "source", "source_sha256", "output_sha256",
             "license_or_terms", "transform"), provenance_rows)
        coverage = {
            "schema": 1,
            "generation": generation,
            "games": len(games),
            "official": official_count,
            "baseline": len(games) - official_count,
            "uncovered": 0,
            "generated_at": int(time.time()),
            "notifications": "deferred",
            "achievement_mode": "ipod-hardcore",
            "verification": "local-device",
            "official_ra_hardcore_mastery": False,
        }
        atomic_write_text(
            os.path.join(generation_root, "coverage.json"),
            json.dumps(coverage, indent=2, sort_keys=True) + "\n")
        atomic_write_text(os.path.join(work_root, "current"), generation + "\n")

        assets = []
        for path in sorted(Path(generation_root).rglob("*")):
            if path.is_file():
                rel = path.relative_to(work_root).as_posix()
                assets.append(self._asset(path, f"{ACHIEVEMENTS_ROOT}/{rel}"))
        assets.extend(self._state_seed_assets(work_root, mount_root))
        assets.append(self._asset(
            os.path.join(work_root, "current"), f"{ACHIEVEMENTS_ROOT}/current"))
        avatar_assets, avatar_coverage = XboxAvatarService(
            self.repo_root, self.avatar_profile
        ).build_sync_assets(
            mount_root,
            stage_root,
            totals={
                "games": len(catalog_rows),
                "unlocked": sum(int(row["unlocked"]) for row in catalog_rows),
                "achievements": sum(int(row["total"]) for row in catalog_rows),
                "gamerscore": sum(
                    int(row["earned_points"]) for row in catalog_rows
                ),
            },
        )
        assets.extend(avatar_assets)
        coverage["avatar"] = avatar_coverage
        atomic_write_text(
            os.path.join(generation_root, "coverage.json"),
            json.dumps(coverage, indent=2, sort_keys=True) + "\n",
        )
        for asset in assets:
            if asset["destination_rel"].endswith("/coverage.json"):
                asset["size"] = os.path.getsize(asset["source_abs"])
                break
        return assets, coverage

    def _write_game(self, mount_root, generation_root, generation, game,
                    official):
        game_root = os.path.join(generation_root, "games", game.key)
        os.makedirs(game_root, exist_ok=True)
        provenance = []
        achievements = []
        ra_game_id = ""
        ra_hash = ""
        set_kind = "local-baseline"
        art_source = self._art_source(mount_root, game)
        cover_output = os.path.join(game_root, "cover.bmp")
        self._make_cover(art_source, cover_output)
        provenance.append({
            "game_key": game.key, "asset": "cover.bmp",
            "source": game.cover_path if game.cover_abs else
                      self._source_ref(art_source),
            "source_sha256": _sha256(art_source),
            "output_sha256": _sha256(cover_output),
            "license_or_terms": "Personal-use source artwork; no redistribution",
            "transform": "aspect-fit; 96x96 RGB BMP",
        })

        if official:
            set_kind = "retroachievements"
            ra_game_id = str(official.get("ID") or official.get("GameID") or "")
            ra_hash = str(official.get("_hash") or "")
            achievements, official_provenance = self._official_achievements(
                game_root, generation, game, official)
            provenance.extend(official_provenance)

        if not achievements:
            set_kind = "local-baseline"
            source = art_source
            unlocked = os.path.join(game_root, "badge.bmp")
            locked = os.path.join(game_root, "badge_lock.bmp")
            self._make_badges(source, unlocked, locked)
            source_ref = game.cover_path if game.cover_abs else self._source_ref(source)
            source_hash = _sha256(source)
            for output, transform in ((unlocked, "center-crop; 48x48 RGB"),
                                      (locked, "center-crop; grayscale; darken; 48x48 RGB")):
                provenance.append({
                    "game_key": game.key, "asset": os.path.basename(output),
                    "source": source_ref, "source_sha256": source_hash,
                    "output_sha256": _sha256(output),
                    "license_or_terms": "Personal-use source artwork; no redistribution",
                    "transform": transform,
                })
            for achievement_id, title, description, points in BASELINE_ACHIEVEMENTS:
                achievements.append({
                    "id": achievement_id, "title": title,
                    "description": description, "points": points,
                    "badge_unlocked": self._generation_path(
                        generation, game.key, "badge.bmp"),
                    "badge_locked": self._generation_path(
                        generation, game.key, "badge_lock.bmp"),
                    "state": "locked", "measured": "0", "unlock_time": "",
                    "source": "rockpod-local", "memaddr": "",
                })

        achievement_file = self._generation_path(
            generation, game.key, "achievements.tsv")
        self._write_tsv(os.path.join(game_root, "achievements.tsv"),
                        ACHIEVEMENT_COLUMNS, achievements)
        total_points = sum(int(item.get("points") or 0) for item in achievements)
        unlocked_items = [item for item in achievements
                          if item.get("state") == "unlocked"]
        earned_points = sum(int(item.get("points") or 0)
                            for item in unlocked_items)
        catalog = {
            "game_key": game.key, "title": game.title,
            "console": game.console, "set_kind": set_kind,
            "cover_path": self._generation_path(
                generation, game.key, "cover.bmp"),
            "achievement_file": achievement_file,
            "unlocked": len(unlocked_items), "total": len(achievements),
            "earned_points": earned_points,
            "total_points": total_points, "last_played": game.last_played,
            "launch_target": game.launch_target, "ra_game_id": ra_game_id,
            "ra_hash": ra_hash,
        }
        return catalog, provenance

    def _art_source(self, mount_root, game):
        if game.cover_abs and os.path.isfile(game.cover_abs):
            return game.cover_abs
        system = game.console.casefold().replace(" ", "")
        aliases = {
            "supernintendo": "snes", "gameboy": "gameboy",
            "gameboycolor": "gameboy", "game&watch": "gwatch",
            "master system": "smsgg", "mastersystem": "smsgg",
            "gamegear": "smsgg", "nintendo64": "n64",
            "playstation": "native", "rockbox": "native",
        }
        stem = aliases.get(system, system)
        systems_root = os.path.join(
            mount_root, ".rockbox", "games", "library", "covers", "systems")
        for extension in (".bmp", ".png", ".jpg", ".jpeg"):
            candidate = os.path.join(systems_root, stem + extension)
            if os.path.isfile(candidate):
                return candidate
        repositories = (
            self.repo_root,
            os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")),
        )
        for repository in repositories:
            fallback = os.path.join(
                repository, "assets", "ipodjs", "sources", "xbox360",
                "xbox360-sphere-official.jpg")
            if os.path.isfile(fallback):
                return fallback
        raise FileNotFoundError(f"No real source artwork for {game.title}")

    def _source_ref(self, source):
        try:
            return os.path.relpath(source, self.repo_root)
        except ValueError:
            return source

    @staticmethod
    def _make_badges(source, unlocked, locked):
        try:
            with Image.open(source) as image:
                image = ImageOps.exif_transpose(image).convert("RGB")
                side = min(image.size)
                left = (image.width - side) // 2
                top = (image.height - side) // 2
                badge = image.crop((left, top, left + side, top + side))
                badge = badge.resize((48, 48), Image.Resampling.LANCZOS)
                badge.save(unlocked, "BMP")
                gray = ImageOps.grayscale(badge).convert("RGB")
                ImageEnhance.Brightness(gray).enhance(0.45).save(locked, "BMP")
        except (OSError, UnidentifiedImageError) as exc:
            raise ValueError(f"Unable to derive badge from {source}: {exc}") from exc

    @staticmethod
    def _make_cover(source, output):
        try:
            with Image.open(source) as image:
                image = ImageOps.exif_transpose(image).convert("RGB")
                image.thumbnail((96, 96), Image.Resampling.LANCZOS)
                canvas = Image.new("RGB", (96, 96), (205, 208, 210))
                canvas.paste(image, ((96 - image.width) // 2,
                                     (96 - image.height) // 2))
                canvas.save(output, "BMP")
        except (OSError, UnidentifiedImageError) as exc:
            raise ValueError(f"Unable to prepare cover from {source}: {exc}") from exc

    def _official_set(self, game):
        extension = os.path.splitext(game.content_path)[1].lower()
        if (extension not in OFFLINE_RUNTIME_EXTENSIONS or
                not game.console_id or not os.path.isfile(game.content_abs)):
            return None
        rom_hash = self._rhash(game.console_id, game.content_abs)
        if not rom_hash:
            return None
        mapping = self._cached_api(
            f"console-{game.console_id}", "API_GetGameList.php",
            {"i": game.console_id, "h": 1, "f": 1})
        game_id = ""
        if isinstance(mapping, dict):
            game_id = mapping.get(rom_hash) or mapping.get(rom_hash.upper()) or ""
            if not game_id:
                mapping = (mapping.get("Results") or mapping.get("results") or
                           mapping.get("Items") or mapping.get("items") or [])
        if isinstance(mapping, list):
            for item in mapping:
                hashes = item.get("Hashes") or item.get("hashes") or []
                if any(str(value).casefold() == rom_hash for value in hashes):
                    game_id = (item.get("ID") or item.get("id") or
                               item.get("GameID") or item.get("gameId"))
                    break
        if not game_id:
            return None
        details = self._cached_api(
            f"game-{game_id}", "API_GetGameExtended.php", {"i": game_id})
        if not isinstance(details, dict):
            return None
        details["_hash"] = rom_hash
        if self.username:
            progress = self._cached_api(
                f"progress-{self.username}-{game_id}",
                "API_GetGameInfoAndUserProgress.php",
                {"g": game_id, "u": self.username}, max_age=3600)
            details["_progress"] = progress
        return details

    def _rhash(self, console_id, content_path):
        binary = os.path.join(self.cache_root, "bin", "rockpod_rhash")
        if not os.path.isfile(binary):
            os.makedirs(os.path.dirname(binary), exist_ok=True)
            subprocess.run([
                os.path.join(self.repo_root, "tools", "build_rockpod_rhash.sh"),
                binary,
            ], check=True, stdout=subprocess.DEVNULL)
        result = subprocess.run(
            [binary, str(console_id), content_path], check=False,
            capture_output=True, text=True, timeout=30)
        value = result.stdout.strip().lower()
        return value if result.returncode == 0 and len(value) == 32 else ""

    def _cached_api(self, cache_key, endpoint, params, max_age=86400):
        os.makedirs(self.cache_root, exist_ok=True)
        safe_key = "".join(char if char.isalnum() or char in "-_" else "_"
                           for char in cache_key)
        cache_path = os.path.join(self.cache_root, safe_key + ".json")
        try:
            if (not self.api_key or
                    time.time() - os.path.getmtime(cache_path) <= max_age):
                with open(cache_path, "r", encoding="utf-8") as handle:
                    return json.load(handle)
        except (OSError, ValueError):
            pass
        if not self.api_key:
            return None
        query = dict(params)
        query["y"] = self.api_key
        if self.username:
            query["z"] = self.username
        url = "https://retroachievements.org/API/" + endpoint + "?" + urlencode(query)
        request = Request(url, headers={"User-Agent": "RockPod/1.0"})
        with self._opener(request, timeout=30) as response:
            payload = json.load(response)
        atomic_write_text(cache_path, json.dumps(payload, sort_keys=True) + "\n")
        return payload

    def _official_achievements(self, game_root, generation, game, details):
        raw = details.get("Achievements") or {}
        items = list(raw.values()) if isinstance(raw, dict) else list(raw)
        expected_count = int(
            details.get("NumAchievements") or
            details.get("numAchievements") or len(items)
        )
        if expected_count != len(items):
            raise ValueError(
                f"RetroAchievements returned a partial set for {game.title}: "
                f"expected {expected_count}, received {len(items)}"
            )
        progress = details.get("_progress") or {}
        progress_items = progress.get("Achievements") or {}
        if isinstance(progress_items, list):
            progress_items = {str(item.get("ID")): item for item in progress_items}
        achievements = []
        provenance = []
        for item in items:
            achievement_id = str(item.get("ID") or "")
            badge_name = str(item.get("BadgeName") or "")
            if not achievement_id or not badge_name:
                continue
            unlocked_path = os.path.join(game_root, f"{achievement_id}.bmp")
            locked_path = os.path.join(game_root, f"{achievement_id}_lock.bmp")
            urls = (
                (f"https://media.retroachievements.org/Badge/{badge_name}.png",
                 unlocked_path),
                (f"https://media.retroachievements.org/Badge/{badge_name}_lock.png",
                 locked_path),
            )
            for url, output in urls:
                source_data = self._download(url)
                self._convert_badge(source_data, output)
                provenance.append({
                    "game_key": game.key, "asset": os.path.basename(output),
                    "source": url,
                    "source_sha256": hashlib.sha256(source_data).hexdigest(),
                    "output_sha256": _sha256(output),
                    "license_or_terms": "RetroAchievements media; personal use",
                    "transform": "official badge; 48x48 RGB BMP conversion",
                })
            earned = progress_items.get(achievement_id, {})
            unlock_time = earned.get("DateEarnedHardcore") or earned.get(
                "DateEarned") or ""
            achievements.append({
                "id": achievement_id, "title": _clean(item.get("Title")),
                "description": _clean(item.get("Description")),
                "points": int(item.get("Points") or 0),
                "badge_unlocked": self._generation_path(
                    generation, game.key, f"{achievement_id}.bmp"),
                "badge_locked": self._generation_path(
                    generation, game.key, f"{achievement_id}_lock.bmp"),
                "state": "unlocked" if unlock_time else "locked",
                "measured": "0", "unlock_time": _clean(unlock_time),
                "source": "retroachievements",
                "memaddr": _runtime_definition(item.get("MemAddr")),
            })
        if len(achievements) != expected_count:
            raise ValueError(
                f"RetroAchievements set for {game.title} is incomplete: "
                f"expected {expected_count}, prepared {len(achievements)}"
            )
        return achievements, provenance

    def _download(self, url):
        key = hashlib.sha256(url.encode("utf-8")).hexdigest()
        path = os.path.join(self.cache_root, "media", key + ".bin")
        if os.path.isfile(path):
            with open(path, "rb") as handle:
                return handle.read()
        request = Request(url, headers={"User-Agent": "RockPod/1.0"})
        with self._opener(request, timeout=30) as response:
            data = response.read()
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as handle:
            handle.write(data)
        return data

    @staticmethod
    def _convert_badge(source_data, output):
        with Image.open(io.BytesIO(source_data)) as image:
            image.convert("RGB").resize(
                (48, 48), Image.Resampling.LANCZOS).save(output, "BMP")

    @staticmethod
    def _generation_path(generation, game_key, filename):
        return (f"/{ACHIEVEMENTS_ROOT}/generations/{generation}/games/"
                f"{game_key}/{filename}")

    @staticmethod
    def _write_tsv(path, columns, rows):
        output = io.StringIO(newline="")
        writer = csv.DictWriter(
            output, fieldnames=columns, delimiter="\t", lineterminator="\n",
            extrasaction="ignore")
        writer.writeheader()
        for row in rows:
            writer.writerow({key: _clean(row.get(key, "")) for key in columns})
        atomic_write_text(path, output.getvalue())

    @staticmethod
    def _asset(source, destination):
        source = os.path.abspath(source)
        return {
            "kind": "achievements",
            "source_rel": os.path.basename(source),
            "source_abs": source,
            "destination_rel": destination,
            "exists": True,
            "size": os.path.getsize(source),
        }

    @staticmethod
    def _state_seed_assets(work_root, mount_root):
        state_root = os.path.join(work_root, "state")
        os.makedirs(state_root, exist_ok=True)
        assets = []
        seeds = {
            "sessions.v1.tsv": "launch_target\tsessions\tseconds\tlast_played\n",
            "unlocks.v1.tsv": "game_key\tachievement_id\tunlock_time\tsource\n",
            "events.v1.tsv": (
                "sequence\tgame_key\tachievement_id\tevent_time\tmode\tclient\n"
            ),
        }
        for name, content in seeds.items():
            installed = os.path.join(
                mount_root, ACHIEVEMENTS_ROOT, "state", name)
            if os.path.isfile(installed):
                continue
            path = os.path.join(state_root, name)
            atomic_write_text(path, content)
            assets.append(AchievementSyncService._asset(
                path, f"{ACHIEVEMENTS_ROOT}/state/{name}"))
        return assets
