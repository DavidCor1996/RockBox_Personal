"""Website sync helper for Rockbox Offline Internet archives."""

from __future__ import annotations

import os
import json
import html
import re
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from urllib.parse import urlparse


class OfflineWebSyncError(RuntimeError):
    """Raised when website sync cannot be started."""


@dataclass(frozen=True)
class OfflineWebSyncRequest:
    command: list[str]
    env: dict[str, str]
    output_path: str
    log_path: str
    device_root: str


def _normalize_sync_url(value):
    text = str(value or "").strip()
    if not text:
        return ""
    if "://" not in text:
        text = f"https://{text}"
    parsed = urlparse(text)
    if parsed.scheme not in {"http", "https"} or not parsed.netloc:
        return ""
    cleaned = parsed._replace(fragment="")
    return cleaned.geturl()


def parse_sync_urls(value):
    if isinstance(value, (list, tuple, set)):
        tokens = []
        for item in value:
            tokens.extend(parse_sync_urls(item))
    else:
        text = str(value or "")
        tokens = [token for token in re.split(r"[\s,;]+", text) if token]

    normalized = []
    seen = set()
    for token in tokens:
        url = _normalize_sync_url(token)
        if not url or url in seen:
            continue
        seen.add(url)
        normalized.append(url)
    return normalized


def website_registry_path(device_root):
    return os.path.join(
        os.path.abspath(os.path.expanduser(str(device_root or ""))),
        ".rockbox",
        "offlineweb",
        "cache",
        "websites.json",
    )


def load_synced_websites(device_root):
    path = website_registry_path(device_root)
    try:
        with open(path, "r", encoding="utf-8") as handle:
            data = json.load(handle)
    except (OSError, ValueError, TypeError):
        data = []
    entries = [item for item in data if isinstance(item, dict) and item.get("url")]
    if entries:
        return entries

    root = os.path.abspath(os.path.expanduser(str(device_root or "")))
    pages_path = os.path.join(os.path.dirname(path), "pages.tsv")
    try:
        with open(pages_path, "r", encoding="utf-8", errors="ignore") as handle:
            rows = [line.rstrip("\r\n").split("\t") for line in handle]
    except OSError:
        return []

    by_path = {}
    for fields in rows:
        if not fields or fields[0].startswith("#") or len(fields) < 3:
            continue
        title, url, device_path = fields[:3]
        if not url or not device_path:
            continue
        parsed_url = urlparse(url)
        if parsed_url.netloc.lower().endswith("onlyfans.com") and parsed_url.path.endswith(".html"):
            url = parsed_url._replace(
                scheme="https", path=parsed_url.path[:-5]
            ).geturl()
        by_path[device_path] = {
            "title": title or urlparse(url).netloc,
            "url": url,
            "path": device_path,
            "preview_path": "",
            "source": "Legacy Website Sync",
            "synced_at": "Unknown",
        }

    for entry in list(by_path.values()):
        relative = str(entry.get("path") or "").lstrip("/")
        absolute = os.path.abspath(os.path.join(root, relative))
        archive_root = os.path.join(root, ".rockbox", "offlineweb", "archive")
        try:
            rel_archive = os.path.relpath(absolute, archive_root)
        except ValueError:
            continue
        rel_parts = Path(rel_archive).parts
        if len(rel_parts) != 2 or rel_parts[0] in {".", ".."}:
            continue
        parent = os.path.dirname(absolute)
        parsed = urlparse(str(entry.get("url") or ""))
        try:
            siblings = list(Path(parent).glob("*.htm*"))
        except OSError:
            siblings = []
        for sibling in siblings:
            if sibling.suffix.lower() not in {".html", ".htm", ".shtml", ".xhtml"}:
                continue
            if ".rockpod-preview" in sibling.name:
                continue
            sibling_device_path = "/" + sibling.relative_to(Path(root)).as_posix()
            if sibling_device_path in by_path:
                continue
            try:
                text = sibling.read_text(encoding="utf-8", errors="ignore")[:512_000]
            except OSError:
                text = ""
            match = re.search(r"(?is)<title\b[^>]*>(.*?)</title>", text)
            title = sibling.stem
            if match:
                title = re.sub(r"\s+", " ", html.unescape(match.group(1))).strip()
            sibling_url = parsed._replace(path="/" + sibling.name, query="", fragment="").geturl()
            by_path[sibling_device_path] = {
                "title": title or sibling.stem,
                "url": sibling_url,
                "path": sibling_device_path,
                "preview_path": "",
                "source": "Legacy Website Sync",
                "synced_at": "Unknown",
            }

    entries = list(by_path.values())
    if entries:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        try:
            with open(path, "w", encoding="utf-8") as handle:
                json.dump(entries, handle, indent=2)
            pages_path = os.path.join(os.path.dirname(path), "pages.tsv")
            with open(pages_path, "w", encoding="utf-8", newline="\n") as handle:
                handle.write(
                    "# title\turl\tpath\tsource\tneighborhood\t"
                    "author\tarchived\tkeywords\n"
                )
                for item in entries:
                    fields = (
                        item.get("title") or urlparse(item.get("url") or "").netloc,
                        item.get("url") or "",
                        item.get("path") or "",
                        "Website Sync",
                        "",
                        "",
                        str(item.get("synced_at") or "")[:10],
                        "",
                    )
                    handle.write(
                        "\t".join(
                            str(value).replace("\t", " ").replace("\n", " ")
                            for value in fields
                        )
                        + "\n"
                    )
        except OSError:
            pass
    return entries


def remove_synced_website(device_root, url):
    root = os.path.abspath(os.path.expanduser(str(device_root or "")))
    normalized = _normalize_sync_url(url)
    entries = load_synced_websites(root)
    removed = [item for item in entries if item.get("url") == normalized]
    kept = [item for item in entries if item.get("url") != normalized]
    if not removed:
        return False

    kept_files = {
        str(path)
        for item in kept
        for path in (item.get("files") or [])
    }
    for item in removed:
        owned_files = set(item.get("files") or [])
        owned_files.update(
            str(item.get(key) or "") for key in ("path", "preview_path")
        )
        preview_path = str(item.get("preview_path") or "")
        if preview_path and not item.get("files"):
            owned_files.add(preview_path + ".bmp")
        for owned_path in owned_files - kept_files:
            relative = str(owned_path or "").lstrip("/")
            if not relative:
                continue
            candidate = os.path.abspath(os.path.join(root, relative))
            if os.path.commonpath([root, candidate]) != root:
                continue
            try:
                os.remove(candidate)
            except OSError:
                pass

    registry = website_registry_path(root)
    os.makedirs(os.path.dirname(registry), exist_ok=True)
    with open(registry, "w", encoding="utf-8") as handle:
        json.dump(kept, handle, indent=2)
    pages_path = os.path.join(os.path.dirname(registry), "pages.tsv")
    with open(pages_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(
            "# title\turl\tpath\tsource\tneighborhood\tauthor\tarchived\tkeywords\n"
        )
        for item in kept:
            fields = (
                item.get("title") or urlparse(item.get("url") or "").netloc,
                item.get("url") or "",
                item.get("path") or "",
                "Website Sync",
                "",
                "",
                str(item.get("synced_at") or "")[:10],
                "",
            )
            handle.write(
                "\t".join(
                    str(value).replace("\t", " ").replace("\n", " ")
                    for value in fields
                )
                + "\n"
            )
    return True


class OfflineWebSyncImporter:
    """Build process requests for syncing web pages into Offline Internet."""

    def __init__(self, config):
        self._config = config

    @property
    def state_dir(self):
        cache_dir = os.path.abspath(os.path.expanduser(self._config.cache_dir))
        return os.path.join(cache_dir, "offlineweb-sync")

    @property
    def output_dir(self):
        return os.path.join(self.state_dir, "results")

    @property
    def log_dir(self):
        return os.path.join(self.state_dir, "logs")

    def prepare_sync(self, urls, device_root):
        normalized_urls = parse_sync_urls(urls)
        if not normalized_urls:
            raise OfflineWebSyncError("Enter at least one website URL to cache.")

        mount_root = os.path.abspath(os.path.expanduser(str(device_root or "").strip()))
        if not mount_root:
            raise OfflineWebSyncError("No iPod mount path is configured for website sync.")
        if not os.path.isdir(mount_root):
            raise OfflineWebSyncError(f"Website sync target does not exist: {mount_root}")
        if not os.path.isdir(os.path.join(mount_root, ".rockbox")):
            raise OfflineWebSyncError("Website sync target must contain a .rockbox directory.")

        os.makedirs(self.output_dir, exist_ok=True)
        os.makedirs(self.log_dir, exist_ok=True)
        stamp = time.strftime("%Y%m%d-%H%M%S")
        output_path = os.path.join(self.output_dir, f"sync-{stamp}.json")
        log_path = os.path.join(self.log_dir, f"sync-{stamp}.log")
        script_path = Path(__file__).resolve().parents[1] / "scripts" / "offlineweb_sync.py"

        command = [
            sys.executable,
            str(script_path),
            "--device-root",
            mount_root,
            "--output",
            output_path,
            "--log-path",
            log_path,
            "--use-firefox-cookies",
        ]
        for url in normalized_urls:
            command.extend(["--url", url])

        return OfflineWebSyncRequest(
            command=command,
            env=os.environ.copy(),
            output_path=output_path,
            log_path=log_path,
            device_root=mount_root,
        )
