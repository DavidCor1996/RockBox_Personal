#!/usr/bin/env python3
"""Snapshot actual Rockbox storage as Music, Apps and Other.

Allocated file bytes include app binaries, assets and downloads, counted once.
Rockbox indexes resolve ownership; the retail iTunes database is never used.
OnlyFans and its indexed media always belong to Other, with shared firmware,
fonts, filesystem overhead and unassigned files. Run after media sync.
"""
import argparse
from collections import defaultdict
import csv
from functools import lru_cache
import os
from pathlib import Path
import sys
import tempfile

MAX_APPS = 256
AUDIO = frozenset("mp3 m4a m4b aac flac ogg oga opus wav aiff aif wv ape alac mpc wma".split())
VIDEO = frozenset("mp4 m4v mov avi mpg mpeg m2v mkv webm".split())
PHOTO = frozenset("jpg jpeg png bmp gif tif tiff webp".split())
NAMES = {
    "youtube": "YouTube", "twitch": "Twitch", "tiktok": "TikTok",
    "instagram": "Instagram", "reddit": "Reddit", "videolist": "Netflix",
    "netflix": "Netflix", "photos": "Photos", "comics": "Comics",
    "magazines": "Magazines", "desktop_mode": "Desktop Mode",
    "desktop-mode": "Desktop Mode", "minivmac": "Mini vMac",
    "spotify-wrapped": "Spotify Wrapped", "spotify_wrapped": "Spotify Wrapped",
    "achievements": "Achievements", "livetv": "DIRECTV", "directv": "DIRECTV",
    "sitekick": "Sitekick", "tamagotchi": "Tamagotchi", "maps": "Maps",
    "weather": "Weather", "calm": "Calm", "offlineweb": "Internet",
    "pocketsky": "Pocket Sky", "qrcode": "QR Codes", "qrcodes": "QR Codes",
    "pokedex": "Pokedex", "games": "Steam", "steam": "Steam",
    "clock": "Clock", "scummvm": "ScummVM", "doom": "Doom",
    "rockboy": "Game Boy", "infones": "NES", "pokemini": "PokeMini",
    "uxn": "Uxn", "palmos": "Palm OS", "zelda3": "Zelda 3",
    "ipodgames": "iPod Games", "flash": "Flash", "flashplayer": "Flash",
}
ROOT_APPS = {
    "videos": "Netflix", "youtube": "YouTube", "twitch": "Twitch",
    "tiktok": "TikTok", "hq tiktok": "TikTok", "instagram": "Instagram",
    "photos": "Photos", "comics": "Comics", "magazines": "Magazines",
    "scummvm": "ScummVM", "gameboy": "Game Boy", "nes": "NES",
    "pokemini": "PokeMini", "uxn": "Uxn", "qr codes": "QR Codes",
}


@lru_cache(maxsize=1024)
def plugin_name(stem):
    for key in sorted(NAMES, key=len, reverse=True):
        if stem == key or stem.startswith(key + "_") or stem.startswith(key + "-"):
            return NAMES[key]
    return stem.replace("_", " ").replace("-", " ")


def inventory(root: Path) -> str:
    root = root.resolve(strict=True)
    if not (root / ".rockbox").is_dir():
        raise ValueError("not a Rockbox volume/runtime")
    forced, music_paths = {}, set()
    library_count = None
    installed = set()
    for bucket in ("apps", "games", "viewers", "demos"):
        directory = root / ".rockbox/rocks" / bucket
        if directory.is_dir():
            installed.update(path.stem.lower() for path in directory.iterdir()
                             if path.suffix == ".rock" and path.is_file())

    owner_keys = sorted(set(NAMES) | installed, key=len, reverse=True)

    @lru_cache(maxsize=1024)
    def installed_owner(name):
        stem = Path(name).stem
        for key in owner_keys:
            if stem == key or any(stem.startswith(key + separator) for separator in "_-."):
                return plugin_name(key)
        return None

    def resolve(value, required=False):
        path = root / value.lstrip("/")
        resolved = path.resolve(strict=required)
        if not resolved.is_relative_to(root):
            raise ValueError("media index points outside the device")
        if path.is_symlink():
            return None
        if required and not path.is_file():
            raise FileNotFoundError(path)
        return resolved

    if (root / ".rockbox/database_idx.tcd").is_file():
        sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "rockpod"))
        from services.rockbox_tagcache import read_rockbox_tagcache_tracks
        tracks = read_rockbox_tagcache_tracks(str(root))
        library_count = len({track["device_path"] for track in tracks})
        for track in tracks:
            path = resolve(track["device_path"], True)
            if path is not None:
                music_paths.add(path)

    # OnlyFans is last: its Other classification wins for shared references.
    indexes = (
        ("videolist/index.tsv", "Netflix", ("device_path",)),
        ("youtube/library.tsv", "YouTube", ("video_path", "thumb_path")),
        ("twitch/vods.tsv", "Twitch", ("video_path", "thumb_path")),
        ("tiktok/library.tsv", "TikTok", ("path", "thumbnail", "menu_preview")),
        ("instagram/library.tsv", "Instagram", ("media", "thumbnail", "display")),
        ("onlyfans/library.tsv", None, ("media", "thumbnail", "display")),
    )
    for relative, app, fields in indexes:
        index = root / ".rockbox" / relative
        if not index.is_file():
            continue
        with index.open(encoding="utf-8") as stream:
            rows = csv.DictReader((line for line in stream if not line.startswith("#")),
                                  delimiter="\t")
            for row in rows:
                for field in fields:
                    value = row.get(field)
                    if value and "://" not in value:
                        path = resolve(value)
                        if path is not None:
                            forced[path] = app

    def owner(path):
        parts = tuple(p.lower() for p in path.relative_to(root).parts)
        if any("onlyfans" in part for part in parts):
            return None
        if path in forced:
            return forced[path]
        if path in music_paths or parts[0] in ("music", "recordings", "playlists"):
            return "Music"
        if parts[:2] in ((".rockbox", "albumlist"), (".rockbox", "lyrics")):
            return "Music"
        if parts[0] in ROOT_APPS:
            return ROOT_APPS[parts[0]]
        if parts[0] == "apps" and len(parts) > 1:
            return installed_owner(parts[1])
        if parts[0] == ".rockbox" and len(parts) > 1:
            if parts[1] in NAMES:
                return NAMES[parts[1]]
            if parts[1] == "ipodjs" and len(parts) > 2:
                return NAMES.get(parts[2])
            if len(parts) > 3 and parts[1] == "rocks" and parts[2] in (
                    "apps", "games", "viewers", "demos", "data"):
                return installed_owner(parts[3])
        if path.suffix.lower().lstrip(".") in AUDIO and parts[0] != ".rockbox":
            return "Music"
        return None

    sizes, counts, seen = defaultdict(int), [0, 0, 0], set()
    def fail(error):
        raise error
    for directory, dirs, files in os.walk(root, followlinks=False, onerror=fail):
        dirs[:] = sorted(d for d in dirs if not (Path(directory) / d).is_symlink())
        for name in files:
            path = Path(directory) / name
            if path.is_symlink() or not path.is_file():
                continue
            stat = path.stat()
            identity = (stat.st_dev, stat.st_ino)
            if identity in seen:
                continue
            seen.add(identity)
            app = owner(path)
            # Actual allocation includes FAT cluster rounding.
            sizes[app] += stat.st_blocks * 512
            suffix = path.suffix.lower().lstrip(".")
            if app == "Music" and suffix in AUDIO:
                counts[0] += 1
            if app is not None and suffix in VIDEO:
                counts[1] += 1
            if app == "Photos" and suffix in PHOTO and not any(
                    p.startswith(".") for p in path.relative_to(root).parts):
                counts[2] += 1
    if library_count is not None:
        counts[0] = library_count
    apps = [(name, (size + 1023) // 1024) for name, size in sizes.items()
            if name not in (None, "Music")]
    apps.sort(key=lambda item: (-item[1], item[0].casefold()))
    if len(apps) > MAX_APPS or any(len(name.encode("utf-8")) >= 64 for name, _ in apps):
        raise ValueError(f"app inventory exceeds bounds: {len(apps)} apps, "
                         f"longest name {max(len(name.encode('utf-8')) for name, _ in apps)} bytes")
    rows = ["ipodjs-about-v2"]
    rows += [f"count{index}\t{count}" for index, count in zip((0, 1, 4), counts)]
    rows += [f"kib0\t{(sizes['Music'] + 1023) // 1024}",
             f"kib1\t{sum(size for _, size in apps)}"]
    vfs = os.statvfs(root) if os.path.ismount(root) else None
    rows += [f"disk0\t{vfs.f_blocks * vfs.f_frsize // 1024 if vfs else 0}",
             f"disk1\t{vfs.f_bfree * vfs.f_frsize // 1024 if vfs else 0}"]
    rows += [f"app\t{name}\t{size}" for name, size in apps]
    return "\n".join(rows) + "\n"


def write_inventory(root: Path, output: Path) -> None:
    content = inventory(root)
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8",
                                         dir=output.parent, delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, output)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    write_inventory(args.root, args.output)
