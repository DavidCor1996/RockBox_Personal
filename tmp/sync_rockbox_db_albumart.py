#!/usr/bin/env python3
from __future__ import annotations

import shutil
import struct
import subprocess
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path


TAG_COUNT = 23
INDEX_ENTRY_SIZE = (TAG_COUNT + 1) * 4
TAG_ALBUM = 1
TAG_ALBUMARTIST = 7
MAGIC = 0x54434810


@dataclass(frozen=True)
class Album:
    artist: str
    album: str


@dataclass(frozen=True)
class LocalCover:
    source: Path
    artist: str
    album: str


def norm(value: str) -> str:
    value = value.casefold().replace("&", "and")
    out = []
    last_space = True
    for ch in value:
        if ch.isalnum():
            out.append(ch)
            last_space = False
        elif not last_space:
            out.append(" ")
            last_space = True
    return "".join(out).strip()


def base_title(value: str) -> str:
    value = norm(value)
    words = value.split()
    stop = {
        "deluxe",
        "edition",
        "explicit",
        "version",
        "remaster",
        "remastered",
        "anniversary",
        "expanded",
        "special",
        "bonus",
        "mono",
        "stereo",
        "disc",
        "cd",
    }
    trimmed = []
    for word in words:
        if word in stop or word.isdigit():
            break
        trimmed.append(word)
    return " ".join(trimmed) if trimmed else value


def key(album: Album) -> tuple[str, str]:
    return norm(album.artist), norm(album.album)


def sanitize_path_part(value: str) -> str:
    invalid = set("*/:<>?\\|")
    return "".join("'" if ch == '"' else "_" if ch in invalid else ch for ch in value)


def art_filename(album: Album) -> str:
    return sanitize_path_part(f"{album.artist}-{album.album}") + ".bmp"


def read_header(path: Path, size: int = 12) -> tuple[int, ...]:
    raw = path.read_bytes()[:size]
    vals = struct.unpack("<" + "i" * (len(raw) // 4), raw)
    if vals[0] != MAGIC:
        vals_be = struct.unpack(">" + "i" * (len(raw) // 4), raw)
        if vals_be[0] == MAGIC:
            raise RuntimeError(f"{path}: big-endian database not supported by this helper")
        raise RuntimeError(f"{path}: bad Rockbox DB magic {vals[0]:08x}")
    return vals


def read_tag_at(data: bytes, seek: int) -> str:
    if seek < 0 or seek + 8 > len(data):
        return ""
    tag_len, _idx_id = struct.unpack_from("<ii", data, seek)
    if tag_len <= 0 or seek + 8 + tag_len > len(data):
        return ""
    raw = data[seek + 8 : seek + 8 + tag_len]
    raw = raw.split(b"\0", 1)[0]
    return raw.decode("utf-8", "replace")


def parse_rockbox_albums(db_dir: Path) -> tuple[list[Album], int]:
    idx = db_dir / "database_idx.tcd"
    album_data = (db_dir / "database_1.tcd").read_bytes()
    albumartist_data = (db_dir / "database_7.tcd").read_bytes()
    mh = read_header(idx, 24)
    entry_count = mh[2]
    raw = idx.read_bytes()
    albums: dict[tuple[str, str], Album] = {}
    offset = 24
    for _ in range(entry_count):
        if offset + INDEX_ENTRY_SIZE > len(raw):
            break
        vals = struct.unpack_from("<" + "i" * (TAG_COUNT + 1), raw, offset)
        offset += INDEX_ENTRY_SIZE
        album = read_tag_at(album_data, vals[TAG_ALBUM])
        artist = read_tag_at(albumartist_data, vals[TAG_ALBUMARTIST])
        if not album:
            continue
        if not artist:
            artist = "<Untagged>"
        albums.setdefault(key(Album(artist, album)), Album(artist, album))
    return sorted(albums.values(), key=lambda a: (a.artist.casefold(), a.album.casefold())), entry_count


def read_local_manifest(path: Path) -> list[LocalCover]:
    covers = []
    for line in path.read_text(encoding="utf-8").splitlines():
        parts = line.split("\t")
        if len(parts) < 4:
            continue
        src = Path(parts[0])
        artist = parts[2]
        album = parts[3]
        if src.exists() and artist and album:
            covers.append(LocalCover(src, artist, album))
    return covers


def discover_cover_files(root: Path) -> list[Path]:
    names = {"cover.jpg", "folder.jpg", "cover.jpeg", "folder.jpeg", "cover.png", "folder.png"}
    return [p for p in root.rglob("*") if p.is_file() and p.name.casefold() in names]


def folder_score(album: Album, cover: Path) -> int:
    parts = [norm(p) for p in cover.parts[-5:]]
    joined = " ".join(parts)
    album_norm = norm(album.album)
    artist_norm = norm(album.artist)
    album_base = base_title(album.album)
    artist_ok = artist_norm and artist_norm in joined
    album_ok = album_norm and album_norm in joined
    base_ok = album_base and album_base in joined
    score = 0
    if artist_ok:
        score += 4
    if album_ok:
        score += 6
    elif base_ok:
        score += 3
    return score


def convert_cover(magick: str, source: Path, target: Path) -> bool:
    target.parent.mkdir(parents=True, exist_ok=True)
    proc = subprocess.run(
        [
            magick,
            str(source),
            "-auto-orient",
            "-resize",
            "100x100^",
            "-gravity",
            "center",
            "-extent",
            "100x100",
            "-alpha",
            "off",
            "-colorspace",
            "sRGB",
            "-type",
            "TrueColor",
            f"BMP3:{target}",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    return proc.returncode == 0 and target.exists() and target.stat().st_size > 0


def main() -> int:
    if len(sys.argv) not in (5, 6):
        print("usage: sync_rockbox_db_albumart.py DB_DIR LOCAL_MANIFEST OUTDIR REPORT [MUSIC_DIR]", file=sys.stderr)
        return 2
    db_dir = Path(sys.argv[1])
    local_manifest = Path(sys.argv[2])
    outdir = Path(sys.argv[3])
    report = Path(sys.argv[4])
    music_dir = Path(sys.argv[5]) if len(sys.argv) == 6 else Path("/home/david/Music")
    magick = shutil.which("magick")
    if not magick:
        print("magick not found", file=sys.stderr)
        return 1

    db_albums, track_count = parse_rockbox_albums(db_dir)
    local = read_local_manifest(local_manifest)
    local_by_key: dict[tuple[str, str], LocalCover] = {}
    local_by_artist_base: dict[tuple[str, str], LocalCover] = {}
    title_to_local: defaultdict[str, list[LocalCover]] = defaultdict(list)
    base_title_to_local: defaultdict[str, list[LocalCover]] = defaultdict(list)
    for item in local:
        local_by_key.setdefault((norm(item.artist), norm(item.album)), item)
        local_by_artist_base.setdefault((norm(item.artist), base_title(item.album)), item)
        title_to_local[norm(item.album)].append(item)
        base_title_to_local[base_title(item.album)].append(item)

    title_counts = Counter(norm(item.album) for item in local)
    base_title_counts = Counter(base_title(item.album) for item in local)
    cover_files = discover_cover_files(music_dir)
    matches: list[tuple[Album, LocalCover, str]] = []
    missing_cover: list[Album] = []

    outdir.mkdir(parents=True, exist_ok=True)
    for old in outdir.glob("*.bmp"):
        old.unlink()

    for album in db_albums:
        cover = local_by_key.get(key(album))
        reason = "artist+album"
        if cover is None:
            cover = local_by_artist_base.get((norm(album.artist), base_title(album.album)))
            reason = "artist+base album"
        if cover is None and title_counts[norm(album.album)] == 1:
            cover = title_to_local[norm(album.album)][0]
            reason = "unique album title"
        if cover is None and base_title_counts[base_title(album.album)] == 1:
            cover = base_title_to_local[base_title(album.album)][0]
            reason = "unique base album title"
        if cover is None:
            candidates = [(folder_score(album, path), path) for path in cover_files]
            candidates = [(score, path) for score, path in candidates if score >= 7]
            candidates.sort(reverse=True, key=lambda x: x[0])
            if candidates and (len(candidates) == 1 or candidates[0][0] > candidates[1][0]):
                cover = LocalCover(candidates[0][1], album.artist, album.album)
                reason = "cover folder"
        if cover is None:
            missing_cover.append(album)
            continue
        target = outdir / art_filename(album)
        if convert_cover(magick, cover.source, target):
            matches.append((album, cover, reason))
        else:
            missing_cover.append(album)

    matched_keys = {key(album) for album, _cover, _reason in matches}
    db_keys = {key(album) for album in db_albums}
    local_not_in_db = sorted(
        [item for item in local if (norm(item.artist), norm(item.album)) not in db_keys],
        key=lambda x: (x.artist.casefold(), x.album.casefold()),
    )

    report.parent.mkdir(parents=True, exist_ok=True)
    with report.open("w", encoding="utf-8") as f:
        f.write("iPod 5G Rockbox DB artwork and album differences\n")
        f.write("Generated from Rockbox database_idx.tcd + database_1.tcd + database_7.tcd.\n\n")
        f.write(f"Rockbox DB tracks: {track_count}\n")
        f.write(f"Rockbox DB albums: {len(db_albums)}\n")
        f.write(f"Local cover albums in manifest: {len(local)}\n")
        f.write(f"Local cover files discovered: {len(cover_files)}\n")
        f.write(f"Artwork generated for DB albums: {len(matches)}\n")
        f.write(f"DB albums without local cover match: {len(missing_cover)}\n")
        f.write(f"Local cover albums not in this Rockbox DB: {len(local_not_in_db)}\n\n")

        f.write("DB albums without local cover match:\n")
        if missing_cover:
            for album in sorted(missing_cover, key=lambda a: (a.artist.casefold(), a.album.casefold())):
                f.write(f"  {album.artist} - {album.album}\n")
        else:
            f.write("  none\n")

        f.write("\nLocal cover albums not in this Rockbox DB:\n")
        if local_not_in_db:
            for item in local_not_in_db:
                f.write(f"  {item.artist} - {item.album}\n")
        else:
            f.write("  none\n")

        f.write("\nArtwork generated:\n")
        for album, cover, reason in sorted(matches, key=lambda x: (x[0].artist.casefold(), x[0].album.casefold())):
            f.write(f"  {album.artist} - {album.album} [{reason}] <- {cover.source}\n")

    print(
        f"tracks={track_count} db_albums={len(db_albums)} generated={len(matches)} "
        f"missing_cover={len(missing_cover)} local_not_in_db={len(local_not_in_db)} "
        f"outdir={outdir} report={report}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
