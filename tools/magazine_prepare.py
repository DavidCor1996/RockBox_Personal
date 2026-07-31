#!/usr/bin/env python3
"""Prepare PDF magazines for the Rockbox Magazines plugin.

The device reader deliberately consumes a small, predictable format:
one issue manifest, a shelf cover, and baseline RGB JPEG pages. PDF and
Internet Archive handling stays on the host.
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
ARCHIVE_METADATA = "https://archive.org/metadata/{identifier}"
ARCHIVE_DOWNLOAD = "https://archive.org/download/{identifier}/{filename}"
SAFE_ID = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$")


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
        url, headers={"User-Agent": "Rockbox-Magazines/1"}
    )
    try:
        with urllib.request.urlopen(request, timeout=60) as response:
            return json.load(response)
    except (OSError, json.JSONDecodeError) as exc:
        raise PrepareError(f"could not fetch metadata: {exc}") from exc


def select_archive_pdf(record: dict) -> dict:
    candidates = []
    for entry in record.get("files", []):
        name = str(entry.get("name", ""))
        file_format = str(entry.get("format", "")).lower()
        if not name.lower().endswith(".pdf") and "pdf" not in file_format:
            continue
        score = 0
        if entry.get("source") == "original":
            score += 100
        if file_format == "text pdf":
            score += 20
        if name.lower().endswith(".pdf"):
            score += 10
        try:
            size = int(entry.get("size", 0))
        except (TypeError, ValueError):
            size = 0
        candidates.append((score, size, entry))
    if not candidates:
        raise PrepareError("Internet Archive item has no PDF file")
    candidates.sort(key=lambda item: (item[0], item[1]), reverse=True)
    return candidates[0][2]


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
        url, headers={"User-Agent": "Rockbox-Magazines/1"}
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
                            f"\rDownloading PDF: {copied * 100 // total:3d}%",
                            end="",
                            flush=True,
                        )
            if total:
                print()
    except OSError as exc:
        destination.unlink(missing_ok=True)
        raise PrepareError(f"PDF download failed: {exc}") from exc
    if expected_size and destination.stat().st_size != expected_size:
        raise PrepareError(
            f"downloaded size {destination.stat().st_size} does not match "
            f"metadata size {expected_size}"
        )


def pdf_page_count(pdfinfo: str, pdf: Path) -> int:
    output = run([pdfinfo, str(pdf)], capture=True)
    for line in output.splitlines():
        if line.startswith("Pages:"):
            try:
                count = int(line.split(":", 1)[1].strip())
            except ValueError as exc:
                raise PrepareError("pdfinfo returned an invalid page count") from exc
            if count < 1 or count > 9999:
                raise PrepareError(f"unsupported PDF page count: {count}")
            return count
    raise PrepareError("pdfinfo did not report a page count")


def render_pages(
    pdftoppm: str,
    magick: str,
    pdf: Path,
    pages_dir: Path,
    expected_pages: int,
    profile: dict,
) -> list[Path]:
    raw_dir = pages_dir.parent / "rendered"
    raw_dir.mkdir()
    prefix = raw_dir / "page"
    print(f"Rendering {expected_pages} pages...")
    run(
        [
            pdftoppm,
            "-jpeg",
            "-jpegopt",
            "quality=90,progressive=n,optimize=y",
            "-scale-to",
            str(profile["height"]),
            str(pdf),
            str(prefix),
        ]
    )
    rendered = sorted(raw_dir.glob("page-*.jpg"))
    if len(rendered) != expected_pages:
        raise PrepareError(
            f"renderer produced {len(rendered)} pages; expected {expected_pages}"
        )

    pages_dir.mkdir()
    outputs = []
    for index, source in enumerate(rendered, 1):
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
        print(
            f"\rPreparing pages: {index:4d}/{expected_pages}",
            end="",
            flush=True,
        )
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
        "# Rockbox Magazines catalog v1\n" + "\n".join(entries) + "\n",
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


def prepare(args: argparse.Namespace) -> Path:
    pdftoppm = require_command("pdftoppm")
    pdfinfo = require_command("pdfinfo")
    magick = require_command("magick")
    destination = args.output.resolve()
    library = destination.parent
    library.mkdir(parents=True, exist_ok=True)
    issue_id = safe_issue_id(args.issue_id or destination.name)
    profile = PREPARATION_PROFILES[args.profile]

    source_kind = "local_pdf"
    source_id = ""
    source_url = ""
    source_md5 = ""
    title = clean_text(args.title, destination.name)
    creator = clean_text(args.creator)
    year = clean_text(args.year)
    expected_size = 0
    source_pdf = args.pdf.resolve() if args.pdf else None

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
        selected = select_archive_pdf(record)
        title = clean_text(args.title or metadata.get("title"), source_id)
        creator = clean_text(args.creator or metadata.get("creator"))
        year = clean_text(args.year or metadata.get("date"))
        filename = str(selected["name"])
        source_md5 = str(selected.get("md5", "")).lower()
        expected_size = int(selected.get("size", 0) or 0)
        source_kind = "internet_archive"
        source_url = f"https://archive.org/details/{source_id}/"
        print(f"Source: {title}")
        print(f"PDF: {filename} ({expected_size:,} bytes)")
    elif source_pdf is None or not source_pdf.is_file():
        raise PrepareError(f"PDF not found: {source_pdf}")

    work = Path(
        tempfile.mkdtemp(prefix=f".{destination.name}.prepare-", dir=library)
    )
    staged = work / destination.name
    staged.mkdir()
    try:
        if args.archive_id:
            source_pdf = work / "source.pdf"
            url = ARCHIVE_DOWNLOAD.format(
                identifier=urllib.parse.quote(source_id, safe=""),
                filename=urllib.parse.quote(filename, safe=""),
            )
            download(url, source_pdf, expected_size)
        assert source_pdf is not None
        actual_md5 = md5_file(source_pdf)
        if source_md5 and actual_md5 != source_md5:
            raise PrepareError(
                f"PDF MD5 mismatch: got {actual_md5}, expected {source_md5}"
            )
        if not source_md5:
            source_md5 = actual_md5

        count = pdf_page_count(pdfinfo, source_pdf)
        pages = render_pages(
            pdftoppm, magick, source_pdf, staged / "pages", count, profile
        )
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
                profile_name=args.profile,
                category=args.category,
                locked=args.locked,
            ),
            encoding="utf-8",
        )
        shutil.rmtree(staged / "rendered")
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
        description="Prepare an offline magazine for Rockbox"
    )
    source = result.add_mutually_exclusive_group(required=True)
    source.add_argument("--pdf", type=Path, help="local PDF file")
    source.add_argument("--archive-id", help="Internet Archive identifier")
    result.add_argument("--output", type=Path, required=True)
    result.add_argument("--issue-id")
    result.add_argument(
        "--source-id",
        help="record an Internet Archive identifier for a local PDF",
    )
    result.add_argument("--title")
    result.add_argument("--creator")
    result.add_argument("--year")
    result.add_argument("--category", default="Uncategorized")
    result.add_argument("--locked", action="store_true")
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
    try:
        prepare(parser().parse_args())
    except PrepareError as exc:
        print(f"magazine_prepare: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
