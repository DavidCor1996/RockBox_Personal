#!/usr/bin/env python3
"""Prepare CBZ/CBR comics and manga for the Rockbox Comics plugin.

The device reader deliberately consumes a small, predictable format: one
issue manifest, a shelf cover, and baseline RGB JPEG pages — identical to
the Magazines plugin's format. CBZ/CBR extraction and any PDF fallback stay
entirely on the host; the device never opens an archive.
"""

from __future__ import annotations

import argparse
import hashlib
import html
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.parse
import urllib.request
import zipfile
import xml.etree.ElementTree as ElementTree


PAGE_WIDTH = 480
PAGE_HEIGHT = 640
JPEG_QUALITY = 86
PREPARATION_PROFILES = {
    "standard": {
        "width": PAGE_WIDTH,
        "height": PAGE_HEIGHT,
        "quality": JPEG_QUALITY,
        "crop_margins": False,
    },
    "fine-text": {
        "width": 720,
        "height": 960,
        "quality": 90,
        "crop_margins": True,
    },
}
SCHEMA = 1
IMAGE_SUFFIXES = {".jpg", ".jpeg", ".png", ".gif", ".webp", ".bmp"}
ARCHIVE_METADATA = "https://archive.org/metadata/{identifier}"
ARCHIVE_DOWNLOAD = "https://archive.org/download/{identifier}/{filename}"
SAFE_ID = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$")
_NATURAL_RE = re.compile(r"(\d+)")


class PrepareError(RuntimeError):
    """A user-facing preparation failure."""


def require_command(name: str) -> str:
    path = shutil.which(name)
    if path is None:
        raise PrepareError(f"required command not found: {name}")
    return path


def run(command: list[str], *, capture: bool = False) -> str:
    try:
        result = subprocess.run(
            command,
            check=True,
            text=True,
            stdout=subprocess.PIPE if capture else None,
            stderr=subprocess.PIPE if capture else None,
        )
    except subprocess.CalledProcessError as exc:
        detail = (exc.stderr or exc.stdout or "").strip()
        if detail:
            raise PrepareError(
                f"{Path(command[0]).name} failed: {detail}"
            ) from exc
        raise PrepareError(f"{Path(command[0]).name} failed") from exc
    return result.stdout if capture else ""


def fetch_json(url: str) -> dict:
    request = urllib.request.Request(
        url, headers={"User-Agent": "Rockbox-Comics/1"}
    )
    try:
        with urllib.request.urlopen(request, timeout=60) as response:
            return json.load(response)
    except (OSError, json.JSONDecodeError) as exc:
        raise PrepareError(f"could not fetch metadata: {exc}") from exc


def clean_text(value: object, fallback: str = "") -> str:
    if isinstance(value, list):
        value = value[0] if value else ""
    text = html.unescape(str(value or fallback))
    text = re.sub(r"<[^>]*>", "", text)
    return " ".join(text.split())


def safe_issue_id(value: str) -> str:
    value = value.strip()
    if not SAFE_ID.fullmatch(value) or value in {".", ".."}:
        raise PrepareError(
            "issue id must contain only letters, numbers, '.', '_' or '-'"
        )
    return value


def md5_file(path: Path) -> str:
    digest = hashlib.md5()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def download(url: str, destination: Path, expected_size: int = 0) -> None:
    request = urllib.request.Request(
        url, headers={"User-Agent": "Rockbox-Comics/1"}
    )
    try:
        with urllib.request.urlopen(request, timeout=120) as response:
            total = int(response.headers.get("Content-Length", 0))
            with destination.open("wb") as output:
                copied = 0
                while True:
                    block = response.read(1024 * 1024)
                    if not block:
                        break
                    output.write(block)
                    copied += len(block)
                    if total:
                        print(
                            f"\rDownloading archive: {copied * 100 // total:3d}%",
                            end="",
                            flush=True,
                        )
            if total:
                print()
    except OSError as exc:
        destination.unlink(missing_ok=True)
        raise PrepareError(f"archive download failed: {exc}") from exc
    if expected_size and destination.stat().st_size != expected_size:
        raise PrepareError(
            f"downloaded size {destination.stat().st_size} does not match "
            f"metadata size {expected_size}"
        )


def _natural_key(name: str):
    return [
        int(part) if part.isdigit() else part.lower()
        for part in _NATURAL_RE.split(name)
    ]


def extract_archive(source: Path, work: Path) -> tuple[Path, str]:
    """Extract a CBZ or CBR archive into ``work``. Returns (raw_dir, format)."""
    suffix = source.suffix.lower()
    raw_dir = work / "raw"
    raw_dir.mkdir()
    if suffix in {".cbz", ".zip"}:
        try:
            with zipfile.ZipFile(source) as archive:
                archive.extractall(raw_dir)
        except zipfile.BadZipFile as exc:
            raise PrepareError(f"not a valid CBZ/ZIP archive: {exc}") from exc
        return raw_dir, "cbz"
    if suffix in {".cbr", ".rar"}:
        tool = shutil.which("unrar") or shutil.which("7z")
        if tool is None:
            raise PrepareError(
                "extracting CBR requires 'unrar' or '7z' to be installed"
            )
        if Path(tool).name == "unrar":
            run([tool, "x", "-inul", "-y", str(source), str(raw_dir) + os.sep])
        else:
            run([tool, "x", f"-o{raw_dir}", "-y", str(source)])
        return raw_dir, "cbr"
    raise PrepareError(f"unsupported comic archive type: {suffix}")


def find_comic_info(raw_dir: Path) -> dict:
    """Best-effort read of a ComicInfo.xml sidecar, if the archive has one."""
    for candidate in raw_dir.rglob("ComicInfo.xml"):
        try:
            root = ElementTree.parse(candidate).getroot()
        except ElementTree.ParseError:
            continue
        values = {child.tag: (child.text or "").strip() for child in root}
        return {
            "title": values.get("Title") or values.get("Series") or "",
            "series": values.get("Series") or "",
            "creator": values.get("Writer") or values.get("Penciller") or "",
            "year": values.get("Year") or "",
            "manga": str(values.get("Manga") or "").lower()
            in {"yes", "yesandrighttoleft"},
        }
    return {}


def collect_pages(raw_dir: Path) -> list[Path]:
    pages = sorted(
        (
            path
            for path in raw_dir.rglob("*")
            if path.is_file() and path.suffix.lower() in IMAGE_SUFFIXES
        ),
        key=lambda path: _natural_key(path.name),
    )
    if not pages:
        raise PrepareError("archive contains no orderable page images")
    return pages


def render_pages(
    magick: str, sources: list[Path], pages_dir: Path, profile: dict
) -> list[Path]:
    pages_dir.mkdir()
    outputs = []
    total = len(sources)
    for index, source in enumerate(sources, 1):
        destination = pages_dir / f"{index:04d}.jpg"
        command = [magick, str(source), "-auto-orient"]
        if profile["crop_margins"]:
            command.extend(["-fuzz", "8%", "-trim", "+repage"])
        command.extend(
            [
                "-resize",
                f"{profile['width']}x{profile['height']}"
                + ("" if profile["crop_margins"] else ">"),
            ]
        )
        if profile["crop_margins"]:
            command.extend(
                [
                    "-gravity",
                    "center",
                    "-background",
                    "#fffdf7",
                    "-extent",
                    f"{profile['width']}x{profile['height']}",
                ]
            )
        command.extend(
            [
                "-background",
                "#fffdf7",
                "-alpha",
                "remove",
                "-alpha",
                "off",
                "-colorspace",
                "sRGB",
                "-strip",
                "-interlace",
                "none",
                "-sampling-factor",
                "4:2:0",
                "-quality",
                str(profile["quality"]),
                str(destination),
            ]
        )
        run(command)
        outputs.append(destination)
        print(f"\rPreparing pages: {index:4d}/{total}", end="", flush=True)
    print()
    return outputs


def make_cover(
    magick: str,
    first_page: Path,
    destination: Path,
    geometry: str = "68x88",
    quality: int = JPEG_QUALITY,
) -> None:
    run(
        [
            magick,
            str(first_page),
            "-resize",
            geometry + "^",
            "-gravity",
            "center",
            "-extent",
            geometry,
            "-strip",
            "-interlace",
            "none",
            "-sampling-factor",
            "4:2:0",
            "-quality",
            str(quality),
            str(destination),
        ]
    )


def jpeg_dimensions(magick: str, path: Path) -> tuple[int, int, str]:
    output = run(
        [
            magick,
            "identify",
            "-format",
            "%w %h %[colorspace] %[interlace]",
            str(path),
        ],
        capture=True,
    ).split()
    if len(output) < 4:
        raise PrepareError(f"could not validate image: {path.name}")
    return int(output[0]), int(output[1]), " ".join(output[2:])


def validate_pages(magick: str, pages: list[Path], profile: dict) -> None:
    for expected, path in enumerate(pages, 1):
        if path.name != f"{expected:04d}.jpg":
            raise PrepareError(f"unexpected page filename: {path.name}")
        width, height, description = jpeg_dimensions(magick, path)
        if width > profile["width"] or height > profile["height"]:
            raise PrepareError(
                f"{path.name} is {width}x{height}, over the device limit"
            )
        lowered = description.lower()
        if "progressive" in lowered or "cmyk" in lowered:
            raise PrepareError(
                f"{path.name} is not a baseline RGB-compatible JPEG"
            )


def manifest_text(
    *,
    issue_id: str,
    title: str,
    creator: str,
    year: str,
    page_count: int,
    source_kind: str,
    source_id: str,
    source_url: str,
    source_md5: str,
    source_format: str,
    reading_direction: str,
    profile_name: str = "standard",
    category: str = "Uncategorized",
    locked: bool = False,
) -> str:
    profile = PREPARATION_PROFILES[profile_name]
    values = {
        "schema": str(SCHEMA),
        "id": issue_id,
        "title": title,
        "creator": creator,
        "year": year,
        "page_count": str(page_count),
        "cover": "cover.jpg",
        "preview": "cover-pane.jpg",
        "page_pattern": "pages/%04d.jpg",
        "source_kind": source_kind,
        "source_id": source_id,
        "source_url": source_url,
        "source_md5": source_md5,
        "source_format": source_format,
        "reading_direction": reading_direction,
        "prepare_profile": profile_name,
        "category": clean_text(category, "Uncategorized"),
        "locked": "1" if locked else "0",
        "crop_margins": "1" if profile["crop_margins"] else "0",
        "prepared_width": str(profile["width"]),
        "prepared_height": str(profile["height"]),
    }
    for key, value in values.items():
        if "\n" in value or "\r" in value:
            raise PrepareError(f"newline in manifest value: {key}")
    return "".join(f"{key}={value}\n" for key, value in values.items())


def update_catalog(library: Path, issue_name: str) -> None:
    catalog = library / "catalog.mgi"
    entries = []
    if catalog.exists():
        entries = [
            line.strip()
            for line in catalog.read_text(encoding="utf-8").splitlines()
            if line.strip() and not line.lstrip().startswith("#")
        ]
    entries = sorted(set(entries + [issue_name]), key=str.casefold)
    temporary = library / "catalog.mgi.tmp"
    temporary.write_text(
        "# Rockbox Comics catalog v1\n" + "\n".join(entries) + "\n",
        encoding="utf-8",
    )
    os.replace(temporary, catalog)


def install_staged(staged_issue: Path, destination: Path, force: bool) -> None:
    backup = destination.with_name(destination.name + ".previous")
    if destination.exists() and not force:
        raise PrepareError(
            f"destination already exists: {destination} (use --force)"
        )
    if backup.exists():
        raise PrepareError(f"recover or remove stale backup first: {backup}")
    if destination.exists():
        os.replace(destination, backup)
    try:
        os.replace(staged_issue, destination)
    except Exception:
        if backup.exists() and not destination.exists():
            os.replace(backup, destination)
        raise
    if backup.exists():
        shutil.rmtree(backup)


def _select_archive_file(record: dict) -> dict:
    """Prefer a CBZ derivative; fall back to the original archive or a PDF."""
    candidates = []
    for entry in record.get("files", []):
        name = str(entry.get("name", ""))
        lowered = name.lower()
        if lowered.endswith((".cbz", ".cbr", ".zip", ".rar", ".pdf")):
            score = 0
            if lowered.endswith(".cbz"):
                score += 100
            elif lowered.endswith(".cbr"):
                score += 90
            elif lowered.endswith(".pdf"):
                score += 50
            if entry.get("source") == "original":
                score += 10
            try:
                size = int(entry.get("size", 0))
            except (TypeError, ValueError):
                size = 0
            candidates.append((score, size, entry))
    if not candidates:
        raise PrepareError("Internet Archive item has no comic archive or PDF")
    candidates.sort(key=lambda item: (item[0], item[1]), reverse=True)
    return candidates[0][2]


def prepare(args: argparse.Namespace) -> Path:
    magick = require_command("magick")
    destination = args.output.resolve()
    library = destination.parent
    library.mkdir(parents=True, exist_ok=True)
    issue_id = safe_issue_id(args.issue_id or destination.name)
    profile = PREPARATION_PROFILES[args.profile]

    source_kind = "local_archive"
    source_id = ""
    source_url = ""
    source_md5 = ""
    title = clean_text(args.title, destination.name)
    creator = clean_text(args.creator)
    year = clean_text(args.year)
    reading_direction = "rtl" if args.manga else "ltr"
    expected_size = 0
    source_archive = args.archive.resolve() if args.archive else None
    remote_filename = ""

    if args.source_id:
        source_id = safe_issue_id(args.source_id)
        source_kind = "internet_archive"
        source_url = f"https://archive.org/details/{source_id}/"

    if args.archive_id:
        source_id = safe_issue_id(args.archive_id)
        record = fetch_json(
            ARCHIVE_METADATA.format(
                identifier=urllib.parse.quote(source_id, safe="")
            )
        )
        metadata = record.get("metadata", {})
        selected = _select_archive_file(record)
        title = clean_text(args.title or metadata.get("title"), source_id)
        creator = clean_text(args.creator or metadata.get("creator"))
        year = clean_text(args.year or metadata.get("date"))
        remote_filename = str(selected["name"])
        source_md5 = str(selected.get("md5", "")).lower()
        expected_size = int(selected.get("size", 0) or 0)
        source_kind = "internet_archive"
        source_url = f"https://archive.org/details/{source_id}/"
        print(f"Source: {title}")
        print(f"File: {remote_filename} ({expected_size:,} bytes)")
    elif source_archive is None or not source_archive.is_file():
        raise PrepareError(f"comic archive not found: {source_archive}")

    work = Path(
        tempfile.mkdtemp(prefix=f".{destination.name}.prepare-", dir=library)
    )
    staged = work / destination.name
    staged.mkdir()
    try:
        if args.archive_id:
            source_archive = work / ("source" + Path(remote_filename).suffix)
            url = ARCHIVE_DOWNLOAD.format(
                identifier=urllib.parse.quote(source_id, safe=""),
                filename=urllib.parse.quote(remote_filename, safe=""),
            )
            download(url, source_archive, expected_size)
        assert source_archive is not None
        actual_md5 = md5_file(source_archive)
        if source_md5 and actual_md5 != source_md5:
            raise PrepareError(
                f"archive MD5 mismatch: got {actual_md5}, expected {source_md5}"
            )
        if not source_md5:
            source_md5 = actual_md5

        if source_archive.suffix.lower() == ".pdf":
            pdftoppm = require_command("pdftoppm")
            pdfinfo = require_command("pdfinfo")
            import magazine_prepare as _magazines  # local import, same tools/ dir

            count = _magazines.pdf_page_count(pdfinfo, source_archive)
            pages = _magazines.render_pages(
                pdftoppm, magick, source_archive, staged / "pages", count, profile
            )
            source_format = "pdf"
            info = {}
        else:
            raw_dir, source_format = extract_archive(source_archive, work)
            info = find_comic_info(raw_dir)
            if args.manga is None and info.get("manga"):
                reading_direction = "rtl"
            sources = collect_pages(raw_dir)
            count = len(sources)
            print(f"Preparing {count} pages...")
            pages = render_pages(magick, sources, staged / "pages", profile)

        if not creator and info.get("creator"):
            creator = clean_text(info["creator"])
        if not year and info.get("year"):
            year = clean_text(info["year"])
        if title == destination.name and info.get("title"):
            title = clean_text(info["title"])

        make_cover(magick, pages[0], staged / "cover.jpg")
        make_cover(
            magick,
            pages[0],
            staged / "cover-pane.jpg",
            geometry="348x480",
            quality=92,
        )
        validate_pages(magick, pages, profile)
        (staged / "issue.mgi").write_text(
            manifest_text(
                issue_id=issue_id,
                title=title,
                creator=creator,
                year=year,
                page_count=count,
                source_kind=source_kind,
                source_id=source_id,
                source_url=source_url,
                source_md5=source_md5,
                source_format=source_format,
                reading_direction=reading_direction,
                profile_name=args.profile,
                category=args.category,
                locked=args.locked,
            ),
            encoding="utf-8",
        )
        total = sum(path.stat().st_size for path in staged.rglob("*") if path.is_file())
        print(f"Prepared {count} pages ({total:,} bytes)")
        install_staged(staged, destination, args.force)
        update_catalog(library, destination.name)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print(f"Installed: {destination}")
    return destination


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(
        description="Prepare an offline comic/manga issue for Rockbox"
    )
    source = result.add_mutually_exclusive_group(required=True)
    source.add_argument("--archive", type=Path, help="local CBZ/CBR/PDF file")
    source.add_argument("--archive-id", help="Internet Archive identifier")
    result.add_argument("--output", type=Path, required=True)
    result.add_argument("--issue-id")
    result.add_argument(
        "--source-id",
        help="record an Internet Archive identifier for a local archive",
    )
    result.add_argument("--title")
    result.add_argument("--creator")
    result.add_argument("--year")
    result.add_argument("--category", default="Uncategorized")
    result.add_argument("--locked", action="store_true")
    result.add_argument(
        "--manga",
        action="store_true",
        default=None,
        help="force right-to-left reading direction "
        "(auto-detected from ComicInfo.xml when omitted)",
    )
    result.add_argument(
        "--profile",
        choices=tuple(PREPARATION_PROFILES),
        default="standard",
        help="page preparation profile (default: standard)",
    )
    result.add_argument(
        "--force", action="store_true", help="replace an existing prepared issue"
    )
    return result


def main() -> int:
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    try:
        prepare(parser().parse_args())
    except PrepareError as exc:
        print(f"comics_prepare: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
