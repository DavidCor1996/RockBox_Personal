#!/usr/bin/env python3
import argparse
import json
import re
import subprocess
from pathlib import Path


AUDIO_EXTS = {
    ".flac",
    ".mp3",
    ".m4a",
    ".mp4",
    ".aac",
    ".ogg",
    ".opus",
    ".wav",
    ".aiff",
    ".aif",
}

IMAGE_NAMES = (
    "cover.jpg",
    "cover.jpeg",
    "cover.png",
    "cover.bmp",
    "folder.jpg",
    "folder.jpeg",
    "folder.png",
    "folder.bmp",
)


def fix_path_part(value: str) -> str:
    table = str.maketrans({'"': "'", "*": "_", "/": "_", ":": "_", "<": "_",
                           ">": "_", "?": "_", "\\": "_", "|": "_"})
    return value.translate(table)


def ffprobe_tags(path: Path) -> dict[str, str]:
    proc = subprocess.run(
        [
            "ffprobe",
            "-v",
            "error",
            "-show_entries",
            "format_tags",
            "-of",
            "json",
            str(path),
        ],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if proc.returncode != 0:
        return {}
    try:
        data = json.loads(proc.stdout)
    except json.JSONDecodeError:
        return {}
    raw = data.get("format", {}).get("tags", {})
    return {str(k).lower().replace(" ", "_"): str(v) for k, v in raw.items()}


def first_audio(album_dir: Path) -> Path | None:
    audio = sorted(p for p in album_dir.iterdir()
                   if p.is_file() and p.suffix.lower() in AUDIO_EXTS)
    return audio[0] if audio else None


def cover_image(album_dir: Path) -> Path | None:
    lower_files = {p.name.lower(): p for p in album_dir.iterdir() if p.is_file()}
    for name in IMAGE_NAMES:
        if name in lower_files:
            return lower_files[name]
    return None


def parse_wps_sizes(ipod_root: Path) -> list[tuple[int, int]]:
    cfg = ipod_root / ".rockbox" / "config.cfg"
    wps_path = None
    for line in cfg.read_text(errors="replace").splitlines():
        if line.startswith("wps:"):
            value = line.split(":", 1)[1].strip()
            wps_path = ipod_root / value.lstrip("/")
            break
    if not wps_path or not wps_path.exists():
        return [(138, 138)]

    sizes: set[tuple[int, int]] = set()
    text = wps_path.read_text(errors="replace")
    for match in re.finditer(r"%Cl\([^,]+,[^,]+,(\d+),(\d+)", text):
        width = int(match.group(1))
        height = int(match.group(2))
        if width > 0 and height > 0:
            sizes.add((width, height))
    return sorted(sizes) or [(138, 138)]


def scan_albums(music_root: Path) -> list[dict[str, str]]:
    albums = []
    seen_keys: set[str] = set()
    for album_dir in sorted(p for p in music_root.iterdir() if p.is_dir()):
        image = cover_image(album_dir)
        if not image:
            continue
        audio = first_audio(album_dir)
        if not audio:
            continue
        tags = ffprobe_tags(audio)
        album = tags.get("album")
        artist = (
            tags.get("albumartist")
            or tags.get("album_artist")
            or tags.get("album__artist")
            or tags.get("artist")
        )
        if not album or not artist:
            continue
        stem = fix_path_part(f"{artist}-{album}")
        if stem in seen_keys:
            continue
        seen_keys.add(stem)
        albums.append({
            "artist": artist,
            "album": album,
            "stem": stem,
            "image": str(image),
            "audio": str(audio),
        })
    return albums


def scan_needed_ipod_albums(
    ipod_music_root: Path,
    local_by_stem: dict[str, dict[str, str]],
) -> list[dict[str, str]]:
    albums = []
    seen_keys: set[str] = set()
    for album_dir in sorted(p for p in ipod_music_root.iterdir() if p.is_dir()):
        audio = first_audio(album_dir)
        if not audio:
            continue
        tags = ffprobe_tags(audio)
        album = tags.get("album")
        artist = (
            tags.get("albumartist")
            or tags.get("album_artist")
            or tags.get("album__artist")
            or tags.get("artist")
        )
        if not album or not artist:
            continue
        stem = fix_path_part(f"{artist}-{album}")
        if stem in seen_keys:
            continue
        seen_keys.add(stem)
        local = local_by_stem.get(stem)
        if local:
            albums.append(local)
    return albums


def identify(path: Path) -> str:
    proc = subprocess.run(
        ["identify", "-format", "%wx%h %m", str(path)],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return proc.stdout.strip() if proc.returncode == 0 else "unreadable"


def generate(image: Path, target: Path, width: int, height: int) -> bool:
    target.parent.mkdir(parents=True, exist_ok=True)
    proc = subprocess.run(
        [
            "magick",
            str(image),
            "-auto-orient",
            "-resize",
            f"{width}x{height}^",
            "-gravity",
            "center",
            "-extent",
            f"{width}x{height}",
            f"BMP3:{target}",
        ],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if proc.returncode != 0:
        print(f"FAILED\t{target}\t{proc.stderr.strip()}")
        return False
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--music-root", default="/home/david/Music")
    parser.add_argument("--ipod-root", default="/run/media/david/DAVID_S IPO")
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()

    music_root = Path(args.music_root)
    ipod_root = Path(args.ipod_root)
    albumart_dir = ipod_root / ".rockbox" / "albumart"
    sizes = parse_wps_sizes(ipod_root)
    local_albums = scan_albums(music_root)
    local_by_stem = {album["stem"]: album for album in local_albums}
    albums = scan_needed_ipod_albums(ipod_root / "Music", local_by_stem)

    missing: list[tuple[dict[str, str], int, int, Path]] = []
    present = 0
    for album in albums:
        for width, height in sizes:
            target = albumart_dir / f"{album['stem']}.{width}x{height}.bmp"
            if target.exists() and identify(target) == f"{width}x{height} BMP":
                present += 1
            else:
                missing.append((album, width, height, target))

    print(f"WPS sizes: {', '.join(f'{w}x{h}' for w, h in sizes)}")
    print(f"Albums with local cover art: {len(local_albums)}")
    print(f"iPod albums with matching local cover art: {len(albums)}")
    print(f"Already valid WPS-sized files: {present}")
    print(f"Missing or invalid WPS-sized files: {len(missing)}")
    for album, width, height, target in missing:
        print(f"MISSING\t{width}x{height}\t{album['artist']}\t{album['album']}\t{target.name}")

    if not args.write:
        return 0

    made = 0
    for album, width, height, target in missing:
        if generate(Path(album["image"]), target, width, height):
            made += 1
            print(f"WROTE\t{width}x{height}\t{album['artist']}\t{album['album']}\t{target.name}")

    print(f"Generated files: {made}")
    return 0 if made == len(missing) else 1


if __name__ == "__main__":
    raise SystemExit(main())
