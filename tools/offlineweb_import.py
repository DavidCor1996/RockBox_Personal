#!/usr/bin/env python3
"""Import archived early-web content for Rockbox Offline Internet.

The importer copies real local HTML and media files into .rockbox/offlineweb,
extracts lightweight metadata, and builds cache/pages.tsv for offline search.
It never downloads content; the source directory must already contain the
Wayback/GeoCities/Angelfire/Tripod/Yahoo/personal archive to preserve.
"""

from __future__ import annotations

import argparse
import html
import os
import re
import shutil
from html.parser import HTMLParser
from pathlib import Path


HTML_EXTS = {".html", ".htm", ".shtml", ".xhtml"}
ASSET_EXTS = {
    ".gif",
    ".jpg",
    ".jpeg",
    ".png",
    ".bmp",
    ".mid",
    ".midi",
    ".wav",
    ".mod",
    ".xm",
    ".s3m",
    ".it",
    ".css",
    ".js",
    ".ico",
    ".svg",
    ".webp",
    ".woff",
    ".woff2",
    ".ttf",
}
SOURCE_NAMES = ("geocities", "angelfire", "tripod", "yahoo", "myspace", "archive")
GEOCITIES_NEIGHBORHOODS = (
    "Area51",
    "Hollywood",
    "SiliconValley",
    "Tokyo",
    "Athens",
    "Heartland",
    "EnchantedForest",
    "RainForest",
)


class MetadataParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.in_title = False
        self.title_parts: list[str] = []
        self.keywords = ""
        self.author = ""
        self.links: list[str] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        attrs_dict = {k.lower(): v or "" for k, v in attrs}
        if tag.lower() == "title":
            self.in_title = True
        elif tag.lower() == "meta":
            name = attrs_dict.get("name", "").lower()
            content = attrs_dict.get("content", "")
            if name == "keywords" and not self.keywords:
                self.keywords = content
            elif name in {"author", "owner", "creator"} and not self.author:
                self.author = content
        elif tag.lower() in {"a", "area", "img", "embed", "bgsound"}:
            ref = attrs_dict.get("href") or attrs_dict.get("src")
            if ref:
                self.links.append(ref)

    def handle_endtag(self, tag: str) -> None:
        if tag.lower() == "title":
            self.in_title = False

    def handle_data(self, data: str) -> None:
        if self.in_title:
            self.title_parts.append(data)


def clean_field(value: str, fallback: str = "Unknown") -> str:
    value = html.unescape(value or "").replace("\t", " ").replace("\n", " ")
    value = re.sub(r"\s+", " ", value).strip()
    return value[:160] if value else fallback


def classify(path: Path) -> tuple[str, str]:
    parts_lower = [p.lower() for p in path.parts]
    source = "Archive"
    source_map = {
        "geocities": "GeoCities",
        "angelfire": "Angelfire",
        "tripod": "Tripod",
        "yahoo": "Yahoo",
        "myspace": "MySpace",
        "archive": "Archive",
    }
    for name in SOURCE_NAMES:
        if name in parts_lower or any(name in p for p in parts_lower):
            source = source_map.get(name, name.title())
            break

    neighborhood = "Unknown"
    joined = "/".join(path.parts)
    for name in GEOCITIES_NEIGHBORHOODS:
        if name.lower() in joined.lower():
            neighborhood = name
            break
    return source, neighborhood


def archive_date(path: Path) -> str:
    match = re.search(r"(19|20)\d{12}", "/".join(path.parts))
    if not match:
        return "Unknown"
    stamp = match.group(0)
    return f"{stamp[0:4]}-{stamp[4:6]}-{stamp[6:8]}"


def read_metadata(path: Path) -> MetadataParser:
    parser = MetadataParser()
    try:
        data = path.read_text(errors="ignore")[:512_000]
        parser.feed(data)
    except OSError:
        pass
    return parser


def safe_rel(path: Path, root: Path) -> Path:
    rel = path.relative_to(root)
    parts = [re.sub(r"[^A-Za-z0-9._ -]", "_", p) for p in rel.parts]
    return Path(*parts)


def url_for(path: Path, root: Path, source: str) -> str:
    rel_path = path.relative_to(root)
    rel = rel_path.as_posix()
    host_segment = rel_path.parts[0] if rel_path.parts else ""
    if re.fullmatch(r"[A-Za-z0-9.-]+\.[A-Za-z]{2,}", host_segment):
        tail = "/".join(rel_path.parts[1:])
        return f"http://{host_segment}/{tail}" if tail else f"http://{host_segment}/"
    host = {
        "GeoCities": "www.geocities.com",
        "Angelfire": "www.angelfire.com",
        "Tripod": "members.tripod.com",
        "Yahoo": "www.yahoo.com",
        "MySpace": "myspace.com",
    }.get(source, "offline.archive")
    return f"http://{host}/{rel}"


def copy_tree(source: Path, target: Path) -> list[Path]:
    copied_html: list[Path] = []
    for root, dirs, files in os.walk(source):
        dirs[:] = [d for d in dirs if d not in {".git", "__MACOSX"}]
        root_path = Path(root)
        for filename in files:
            src = root_path / filename
            ext = src.suffix.lower()
            if ext not in HTML_EXTS and ext not in ASSET_EXTS:
                continue
            dst = target / safe_rel(src, source)
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
            if ext in HTML_EXTS:
                copied_html.append(dst)
    return copied_html


def write_home(root: Path, rows: list[tuple[str, str, str, str, str, str, str, str]]) -> None:
    counts: dict[str, int] = {}
    for _, _, _, source, _, _, _, _ in rows:
        counts[source] = counts.get(source, 0) + 1
    links = "\n".join(
        f'<li><a href="{Path(path).as_posix()}">{html.escape(title)}</a></li>'
        for title, _, path, *_ in rows[:50]
    )
    source_list = "\n".join(
        f"<li>{html.escape(source)}: {count} pages</li>"
        for source, count in sorted(counts.items())
    )
    (root / "index.html").write_text(
        "<html><head><title>RockSearch Offline Internet</title></head>\n"
        "<body bgcolor=\"#ffffff\" text=\"#000000\" link=\"#0000ee\">\n"
        "<center><h1>INTERNET</h1><h2>RockSearch</h2></center>\n"
        "<hr><h3>Local Collections</h3><ul>"
        f"{source_list}</ul><h3>Start Surfing</h3><ol>{links}</ol>\n"
        "<hr><p>Use the Rockbox Internet icon for search, random surf, "
        "favorites, history, and GeoCities neighborhoods.</p></body></html>\n",
        encoding="utf-8",
    )


def import_archive(source: Path, ipod_root: Path) -> None:
    offline = ipod_root / ".rockbox" / "offlineweb"
    cache = offline / "cache"
    cache.mkdir(parents=True, exist_ok=True)
    for name in SOURCE_NAMES + ("images", "gifs", "midi"):
        (offline / name).mkdir(parents=True, exist_ok=True)

    html_files = copy_tree(source, offline / "archive")
    rows: list[tuple[str, str, str, str, str, str, str, str]] = []
    seen_urls: set[str] = set()
    seen_paths: set[str] = set()
    for path in sorted(html_files):
        rel_archive = path.relative_to(offline / "archive").as_posix()
        if ".rockbox/offlineweb/archive/" in rel_archive:
            continue
        meta = read_metadata(path)
        source_name, neighborhood = classify(path)
        title = clean_field(" ".join(meta.title_parts), path.stem)
        author = clean_field(meta.author)
        keywords = clean_field(meta.keywords, "")
        archived = archive_date(path)
        url = url_for(path, offline / "archive", source_name)
        device_path = "/" + path.relative_to(ipod_root).as_posix()
        url_key = url.lower()
        path_key = device_path.lower()
        if url_key in seen_urls or path_key in seen_paths:
            continue
        seen_urls.add(url_key)
        seen_paths.add(path_key)
        rows.append((title, url, device_path, source_name, neighborhood, author, archived, keywords))

    with (cache / "pages.tsv").open("w", encoding="utf-8", newline="\n") as out:
        out.write("# title\turl\tpath\tsource\tneighborhood\tauthor\tarchived\tkeywords\n")
        for row in rows:
            out.write("\t".join(clean_field(item, "") for item in row) + "\n")

    write_home(offline, rows)
    print(f"Imported {len(rows)} HTML pages into {offline}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Import Offline Internet archive")
    parser.add_argument("archive_folder", type=Path)
    parser.add_argument("ipod_root", type=Path, help="mounted iPod root or simulator simdisk")
    args = parser.parse_args()
    import_archive(args.archive_folder.resolve(), args.ipod_root.resolve())


if __name__ == "__main__":
    main()
