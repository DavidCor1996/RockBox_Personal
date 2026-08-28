#!/usr/bin/env python3
"""Sync cached websites into Rockbox Offline Internet using archive snapshots."""

from __future__ import annotations

import argparse
import base64
import configparser
import gzip
import hashlib
import html
import json
import os
import re
import shutil
import socket
import sqlite3
import struct
import subprocess
import sys
import tempfile
import time
import traceback
import urllib.request
import zipfile
import zlib
from pathlib import Path
from urllib.parse import quote, unquote, urljoin, urlparse, urlsplit

ROOT = Path(__file__).resolve().parents[1]

try:
    from tools.offlineweb_import import import_archive  # type: ignore
except Exception:  # pragma: no cover
    import sys

    sys.path.insert(0, str(ROOT.parent))
    from tools.offlineweb_import import import_archive  # type: ignore


WAYBACK_AVAILABLE = "https://archive.org/wayback/available?url={url}"
USER_AGENT = "RockPodOfflineWebSync/1.0"
BROWSER_USER_AGENT = (
    "Mozilla/5.0 (X11; Linux x86_64; rv:128.0) "
    "Gecko/20100101 Firefox/128.0"
)
MAX_LIVE_ASSETS = 240
MAX_LIVE_PAGES = 32
MAX_LIVE_DEPTH = 1
MAX_FIREFOX_CACHE_IMAGES = 181
BROWSER_EXPORT_TIMEOUT = 60
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
    ".json",
    ".xml",
    ".webmanifest",
    ".mp3",
    ".m4a",
    ".mp4",
    ".mpg",
    ".mpeg",
    ".m2v",
    ".webm",
    ".avif",
    ".bin",
}
IMAGE_EXTS = {".gif", ".jpg", ".jpeg", ".png", ".bmp", ".ico", ".svg", ".webp", ".avif"}
DIRECT_DRAW_IMAGE_EXTS = {".jpg", ".jpeg", ".bmp"}
FIREFOX_CACHE_IMAGE_HOSTS = {
    "cdn2.onlyfans.com",
    "public.onlyfans.com",
    "thumbs.onlyfans.com",
    "static2.onlyfans.com",
}
ONLYFANS_LOGO_FALLBACK = (
    "https://static2.onlyfans.com/static/prod/f/202607081408-0174238f8e/"
    "images/of-logo-b.jpg"
)
OFFLINEWEB_CURSOR_SOURCE = (
    ROOT.parent
    / "apps"
    / "plugins"
    / "scummvm"
    / "upstream-1.9.0"
    / "gui"
    / "themes"
    / "scummmodern"
    / "cursor_small.bmp"
)
YOUTUBE_2006_LOGO_SOURCE = (
    ROOT.parent / "assets" / "offlineweb" / "youtube" / "youtube-logo-2006.bmp"
)
YOUTUBE_2007_STARS_SOURCE = (
    ROOT.parent / "assets" / "offlineweb" / "youtube"
    / "youtube-stars-5-2007.bmp"
)
_ACTIVE_CACHE_FILES = None


def _track_cache_file(path):
    if _ACTIVE_CACHE_FILES is not None and path:
        _ACTIVE_CACHE_FILES.add(str(Path(path).resolve()))


def _normalize_url(value):
    text = str(value or "").strip()
    if not text:
        return ""
    if "://" not in text:
        text = f"https://{text}"
    parsed = urlparse(text)
    if parsed.scheme not in {"http", "https"} or not parsed.netloc:
        return ""
    return parsed._replace(fragment="").geturl()


def _safe_name(value):
    return re.sub(r"[^A-Za-z0-9._-]+", "_", str(value or "").strip())


def _fetch_json(url):
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.loads(response.read().decode("utf-8", "replace"))


def _download_file(url, destination):
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=120) as response, open(destination, "wb") as handle:
        shutil.copyfileobj(response, handle)


def _browser_headers(url, cookie_header="", referer=""):
    parsed = urlsplit(url)
    headers = {
        "User-Agent": BROWSER_USER_AGENT,
        "Accept": (
            "text/html,application/xhtml+xml,application/xml;q=0.9,"
            "image/avif,image/webp,*/*;q=0.8"
        ),
        "Accept-Language": "en-US,en;q=0.9",
        "Connection": "keep-alive",
        "DNT": "1",
        "Host": parsed.netloc,
        "Sec-Fetch-Dest": "document",
        "Sec-Fetch-Mode": "navigate",
        "Sec-Fetch-Site": "none",
        "Sec-Fetch-User": "?1",
        "Upgrade-Insecure-Requests": "1",
    }
    if referer:
        headers["Referer"] = referer
        headers["Sec-Fetch-Site"] = "same-origin"
    if cookie_header:
        headers["Cookie"] = cookie_header
    return headers


def _list_warc_files(identifier):
    metadata = _fetch_json(f"https://archive.org/metadata/{identifier}")
    files = []
    for entry in metadata.get("files", []):
        name = str(entry.get("name") or "")
        if name.endswith(".warc.gz") and "meta.warc.gz" not in name:
            files.append(name)
    return files


def _extract_payload(http_payload):
    marker = b"\r\n\r\n"
    split_at = http_payload.find(marker)
    if split_at < 0:
        return b"", b""
    headers = http_payload[:split_at]
    body = http_payload[split_at + len(marker) :]
    return headers, body


def _parse_http_headers(header_bytes):
    text = header_bytes.decode("latin1", "ignore")
    lines = text.split("\r\n")
    status = lines[0] if lines else ""
    headers = {}
    for line in lines[1:]:
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        headers[key.strip().lower()] = value.strip()
    return status, headers


def _dechunk(payload):
    output = bytearray()
    offset = 0
    total = len(payload)
    while offset < total:
        line_end = payload.find(b"\r\n", offset)
        if line_end < 0:
            return bytes(output)
        size_token = payload[offset:line_end].split(b";", 1)[0].strip()
        try:
            chunk_size = int(size_token, 16)
        except ValueError:
            return bytes(output)
        offset = line_end + 2
        if chunk_size == 0:
            return bytes(output)
        output += payload[offset : offset + chunk_size]
        offset += chunk_size
        if payload[offset : offset + 2] == b"\r\n":
            offset += 2
    return bytes(output)


def _decode_body(body, headers):
    transfer = str(headers.get("transfer-encoding") or "").lower()
    if "chunked" in transfer:
        body = _dechunk(body)
    encoding = str(headers.get("content-encoding") or "").lower()
    if encoding == "gzip":
        try:
            body = gzip.decompress(body)
        except OSError:
            pass
    elif encoding == "deflate":
        try:
            body = zlib.decompress(body)
        except zlib.error:
            pass
    return body


def _candidate_web_paths(parts, force_html=False):
    path = parts.path or "/"
    query = parts.query or ""
    candidates = []
    if path.endswith("/"):
        candidates.append(path + "index.html")
    elif not Path(path).suffix:
        if force_html:
            candidates.append(path + ".html")
        candidates.append(path)
        if not force_html:
            candidates.append(path + ".html")
        candidates.append(path + "/index.html")
    else:
        candidates.append(path)
    if query:
        candidates.append(path + "?" + query)
    return candidates


def _extension_for_content(content_type):
    content_type = str(content_type or "").split(";", 1)[0].strip().lower()
    return {
        "text/html": ".html",
        "text/css": ".css",
        "application/javascript": ".js",
        "text/javascript": ".js",
        "image/jpeg": ".jpg",
        "image/png": ".png",
        "image/gif": ".gif",
        "image/bmp": ".bmp",
        "image/x-icon": ".ico",
        "image/vnd.microsoft.icon": ".ico",
        "image/svg+xml": ".svg",
        "image/webp": ".webp",
        "font/woff": ".woff",
        "font/woff2": ".woff2",
        "application/font-woff": ".woff",
        "application/font-woff2": ".woff2",
        "application/json": ".json",
        "application/manifest+json": ".webmanifest",
        "application/xml": ".xml",
        "text/xml": ".xml",
        "audio/mpeg": ".mp3",
        "audio/mp4": ".m4a",
        "video/mp4": ".mp4",
        "video/webm": ".webm",
        "image/avif": ".avif",
        "application/octet-stream": ".bin",
    }.get(content_type, "")


def _archive_relative_path_is_safe(host, relative):
    """Keep archive paths addressable on both the host and Rockbox target."""
    archive_relative = (Path(host) / relative).as_posix()
    encoded_parts = [
        part.encode("utf-8", "surrogatepass")
        for part in Path(archive_relative).parts
    ]
    # Leave room for image ".bmp" sidecars under the 255-byte component
    # ceiling and keep the complete Rockbox archive path below MAX_PATH.
    return (
        all(len(part) <= 240 for part in encoded_parts)
        and len(archive_relative.encode("utf-8", "surrogatepass")) <= 220
    )


def _write_url_record(root, record_url, body, content_type=""):
    parts = urlsplit(record_url)
    host = parts.netloc.lower()
    if not host:
        return ""
    html_content = str(content_type or "").lower().startswith("text/html")
    for candidate in _candidate_web_paths(parts, force_html=html_content):
        relative = unquote(candidate.lstrip("/"))
        if not relative:
            continue
        relative = relative.split("?", 1)[0]
        if parts.query:
            query_token = hashlib.sha1(parts.query.encode("utf-8")).hexdigest()[:10]
            relative_path = Path(relative)
            if relative_path.suffix:
                relative = str(
                    relative_path.with_name(
                        f"{relative_path.stem}__q_{query_token}{relative_path.suffix}"
                    )
                )
            else:
                relative = f"{relative}__q_{query_token}"
        ext = Path(relative).suffix.lower()
        inferred = _extension_for_content(content_type)
        if not ext:
            inferred = inferred or ".bin"
            relative = relative.rstrip("/") + inferred
            ext = inferred
        elif ext not in HTML_EXTS and ext not in ASSET_EXTS:
            inferred = inferred or ".bin"
            relative = str(Path(relative).with_suffix(inferred))
            ext = inferred
        if not _archive_relative_path_is_safe(host, relative):
            continue
        destination = root / host / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(body)
        _track_cache_file(destination)
        return str(destination)
    return ""


def _convert_image_sidecar(path):
    path = Path(path)
    ext = path.suffix.lower()
    if ext not in IMAGE_EXTS or ext == ".bmp":
        return False
    sidecar = Path(str(path) + ".bmp")
    try:
        if (
            sidecar.is_file()
            and sidecar.stat().st_mtime_ns >= path.stat().st_mtime_ns
        ):
            return False
    except OSError:
        pass
    try:
        result = subprocess.run(
            [
                "magick",
                str(path) + "[0]" if ext == ".gif" else str(path),
                "-auto-orient",
                "-resize",
                "314x202>",
                "BMP3:" + str(sidecar),
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=30,
            check=False,
        )
    except Exception:
        return False
    converted = result.returncode == 0 and sidecar.is_file()
    if converted:
        _track_cache_file(sidecar)
    return converted


def _firefox_cache_entries_root(profile_path):
    if not profile_path:
        return Path()
    profile_name = Path(profile_path).name
    cache_root = Path.home() / ".cache" / "mozilla" / "firefox" / profile_name
    entries = cache_root / "cache2" / "entries"
    return entries if entries.is_dir() else Path()


def _image_type_from_magic(data):
    if data.startswith(b"\xff\xd8\xff"):
        return ".jpg", "image/jpeg"
    if data.startswith(b"\x89PNG\r\n\x1a\n"):
        return ".png", "image/png"
    if data.startswith((b"GIF87a", b"GIF89a")):
        return ".gif", "image/gif"
    if data.startswith(b"BM"):
        return ".bmp", "image/bmp"
    if data.startswith(b"RIFF") and data[8:12] == b"WEBP":
        return ".webp", "image/webp"
    if data.lstrip()[:5].lower() == b"<svg " or data.lstrip()[:4].lower() == b"<svg":
        return ".svg", "image/svg+xml"
    return "", ""


def _extract_firefox_request_origin(data):
    match = re.search(rb"request-Origin\x00(https?://[^\x00\r\n\t <>'\"]+)", data)
    if not match:
        match = re.search(rb"(?im)^request-origin:\s*(https?://[^\r\n]+)", data)
    if not match:
        return ""
    return match.group(1).decode("utf-8", "ignore").strip()


def _host_related(host, root_host):
    host = str(host or "").lower()
    root_host = str(root_host or "").lower()
    if not host or not root_host:
        return False
    return host == root_host or host.endswith(f".{root_host}") or root_host.endswith(f".{host}")


def _extract_firefox_cache_url(data, page_host="", resource_hosts=None,
                               allow_any_host=False):
    urls = []
    resource_hosts = set(resource_hosts or [])
    pattern = rb"https?://[^\x00\r\n\t <>'\"]+"
    for match in re.finditer(pattern, data):
        value = match.group(0).decode("utf-8", "ignore")
        value = unquote(value).strip()
        parsed = urlsplit(value)
        ext = Path(parsed.path).suffix.lower()
        host = parsed.netloc.lower()
        if ext not in IMAGE_EXTS:
            continue
        if (
            allow_any_host
            or host in FIREFOX_CACHE_IMAGE_HOSTS
            or host in resource_hosts
            or _host_related(host, page_host)
        ):
            urls.append(parsed._replace(fragment="").geturl())
    if not urls:
        return ""
    urls.sort(
        key=lambda item: (
            0 if "/files/" in urlsplit(item).path else 1,
            len(item),
        )
    )
    return urls[0]


def _firefox_cache_payload(data, content_type):
    match = re.search(rb"(?im)^content-length:\s*(\d+)", data)
    if not match:
        return data
    try:
        size = int(match.group(1))
    except ValueError:
        return data
    if size <= 0 or size > len(data):
        return data
    payload = data[:size]
    _ext, magic_type = _image_type_from_magic(payload[:32])
    return payload if magic_type == content_type or magic_type else data


def _firefox_cache_image_records(profile_path, cache_entries_root=None,
                                 page_url="", snapshot=None,
                                 modified_since=0,
                                 max_records=MAX_FIREFOX_CACHE_IMAGES):
    entries_root = (
        Path(cache_entries_root)
        if cache_entries_root
        else _firefox_cache_entries_root(profile_path)
    )
    if not entries_root.is_dir():
        return []

    try:
        entries = sorted(
            (item for item in entries_root.iterdir() if item.is_file()),
            key=lambda item: item.stat().st_mtime,
            reverse=True,
        )
    except OSError:
        return []

    page_host = urlsplit(str(page_url or "")).netloc.lower()
    page_origin = ""
    if page_host:
        page_origin = f"{urlsplit(str(page_url)).scheme or 'https'}://{page_host}"
    resource_hosts = set()
    for value in ((snapshot or {}).get("images") or []) + ((snapshot or {}).get("resources") or []):
        host = urlsplit(str(value or "")).netloc.lower()
        if host:
            resource_hosts.add(host)

    records = []
    seen_urls = set()
    seen_hashes = set()
    for entry in entries:
        if len(records) >= max_records:
            break
        try:
            if modified_since and entry.stat().st_mtime < modified_since:
                break
            with open(entry, "rb") as handle:
                head = handle.read(32)
                ext, content_type = _image_type_from_magic(head)
                if not ext:
                    continue
                size = entry.stat().st_size
                if size < 12_000 and ext != ".svg":
                    continue
                handle.seek(0)
                data = handle.read()
        except OSError:
            continue

        origin = _extract_firefox_request_origin(data)
        origin_host = urlsplit(origin).netloc.lower()
        cached_url = _extract_firefox_cache_url(
            data,
            page_host,
            resource_hosts,
            allow_any_host=False,
        )
        cached_host = urlsplit(cached_url).netloc.lower()
        origin_matches_page = bool(page_host and origin_host and _host_related(origin_host, page_host))
        if page_host and origin_host and not _host_related(origin_host, page_host):
            continue
        if (
            page_host
            and not origin_host
            and page_origin.encode("utf-8") not in data
            and cached_host not in FIREFOX_CACHE_IMAGE_HOSTS
        ):
            continue

        url = cached_url or _extract_firefox_cache_url(
            data, page_host, resource_hosts,
            allow_any_host=origin_matches_page,
        )
        if not url:
            continue
        parsed = urlsplit(url)
        if (
            parsed.netloc.lower() == "cdn2.onlyfans.com"
            and "/files/" not in parsed.path
        ):
            continue
        payload = _firefox_cache_payload(data, content_type)
        digest = hashlib.sha256(payload).hexdigest()
        path_key = parsed._replace(query="", fragment="").geturl()
        if digest in seen_hashes or path_key in seen_urls:
            continue
        seen_hashes.add(digest)
        seen_urls.add(path_key)
        records.append(
            {
                "url": url,
                "body": payload,
                "content_type": content_type,
                "size": len(payload),
            }
        )
    return records


def _extract_warc_to_folder(warc_path, archive_root, allowed_hosts):
    data = gzip.open(warc_path, "rb").read()
    offset = 0
    saved = 0
    html_saved = 0
    while True:
        start = data.find(b"WARC/1.0", offset)
        if start < 0:
            break
        end_header = data.find(b"\r\n\r\n", start)
        if end_header < 0:
            break
        header_text = data[start:end_header].decode("utf-8", "ignore")
        length_match = re.search(r"(?im)^Content-Length:\s*(\d+)", header_text)
        content_length = int(length_match.group(1)) if length_match else 0
        type_match = re.search(r"(?im)^WARC-Type:\s*(\S+)", header_text)
        warc_type = (type_match.group(1).lower() if type_match else "")
        uri_match = re.search(r"(?im)^WARC-Target-URI:\s*(\S+)", header_text)
        record_url = uri_match.group(1).strip() if uri_match else ""
        payload_start = end_header + 4
        payload = data[payload_start : payload_start + content_length]
        offset = payload_start + content_length + 4

        if warc_type != "response" or not record_url:
            continue
        parsed = urlsplit(record_url)
        host = parsed.netloc.lower()
        if allowed_hosts and host not in allowed_hosts and not any(host.endswith(f".{item}") for item in allowed_hosts):
            continue

        http_header, http_body = _extract_payload(payload)
        if not http_header:
            continue
        status_line, headers = _parse_http_headers(http_header)
        status_match = re.match(r"^HTTP/[^ ]+\s+(\d+)", status_line)
        status = status_match.group(1) if status_match else ""
        if status != "200":
            continue
        content_type = str(headers.get("content-type") or "").split(";", 1)[0].strip().lower()

        decoded = _decode_body(http_body, headers)
        written = _write_url_record(archive_root, record_url, decoded, content_type=content_type)
        if not written:
            continue
        _convert_image_sidecar(written)
        saved += 1
        if Path(written).suffix.lower() in HTML_EXTS:
            html_saved += 1
    return {"saved": saved, "html_saved": html_saved}


def _seed_existing_archive(device_root, archive_root):
    existing = Path(device_root) / ".rockbox" / "offlineweb" / "archive"
    if existing.is_dir():
        shutil.copytree(existing, archive_root, dirs_exist_ok=True)


def _seed_builtin_archive(archive_root):
    seed_root = ROOT.parent / "apps" / "plugins" / "offlineweb_seed" / ".rockbox" / "offlineweb" / "archive"
    if seed_root.is_dir():
        shutil.copytree(seed_root, archive_root, dirs_exist_ok=True)


def _convert_archive_images(archive_root):
    converted = 0
    for path in Path(archive_root).rglob("*"):
        if not path.is_file():
            continue
        if _convert_image_sidecar(path):
            converted += 1
    return converted


def _install_runtime_assets(device_root):
    saved = 0
    assets_root = Path(device_root) / ".rockbox" / "offlineweb" / "assets"
    assets_root.mkdir(parents=True, exist_ok=True)
    if OFFLINEWEB_CURSOR_SOURCE.is_file():
        cursor_target = assets_root / "cursor.bmp"
        try:
            result = subprocess.run(
                [
                    "magick",
                    str(OFFLINEWEB_CURSOR_SOURCE),
                    "-alpha",
                    "off",
                    "-fill",
                    "black",
                    "+opaque",
                    "#ff00ff",
                    "BMP3:" + str(cursor_target),
                ],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=10,
                check=False,
            )
        except Exception:
            result = None
        if not result or result.returncode != 0 or not cursor_target.is_file():
            shutil.copy2(OFFLINEWEB_CURSOR_SOURCE, cursor_target)
        saved += 1
    if YOUTUBE_2006_LOGO_SOURCE.is_file():
        shutil.copy2(
            YOUTUBE_2006_LOGO_SOURCE,
            assets_root / "youtube-logo-2006.bmp",
        )
        saved += 1
    if YOUTUBE_2007_STARS_SOURCE.is_file():
        shutil.copy2(
            YOUTUBE_2007_STARS_SOURCE,
            assets_root / "youtube-stars-5-2007.bmp",
        )
        saved += 1
    return saved


def _is_bad_html_capture(html_text, url=""):
    text = re.sub(r"\s+", " ", str(html_text or "")).lower()
    if "just a moment" in text and "try your destination again" in text:
        return True
    if "enable javascript and cookies to continue" in text:
        return True
    if "checking your browser" in text and "cloudflare" in text:
        return True
    if "onlyfans.com" in str(url).lower():
        if "just a moment" in text or "try your destination again" in text:
            return True
    return False


def _remove_bad_html_captures(archive_root):
    removed = 0
    for path in Path(archive_root).rglob("*"):
        if not path.is_file() or path.suffix.lower() not in HTML_EXTS:
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="ignore")[:512_000]
        except OSError:
            continue
        if not _is_bad_html_capture(text, str(path)):
            continue
        try:
            path.unlink()
            removed += 1
        except OSError:
            pass
    return removed


def _purge_url_records(archive_root, url):
    parsed = urlsplit(url)
    host = parsed.netloc.lower()
    if not host:
        return 0
    removed = 0
    for candidate in _candidate_web_paths(parsed, force_html=True):
        relative = unquote(candidate.lstrip("/")).split("?", 1)[0]
        if parsed.query:
            query_token = hashlib.sha1(parsed.query.encode("utf-8")).hexdigest()[:10]
            relative_path = Path(relative)
            if relative_path.suffix:
                relative = str(
                    relative_path.with_name(
                        f"{relative_path.stem}__q_{query_token}{relative_path.suffix}"
                    )
                )
            else:
                relative = f"{relative}__q_{query_token}"
        if not relative:
            continue
        path = Path(archive_root) / host / relative
        for item in (path, Path(str(path) + ".bmp")):
            try:
                if item.is_file():
                    item.unlink()
                    removed += 1
            except OSError:
                pass
    return removed


def _discover_firefox_profile(preferred=""):
    def looks_like_profile(path):
        return path.is_dir() and (
            (path / "cookies.sqlite").is_file()
            or (path / "cookies.sqlite.bak").is_file()
            or (path / "prefs.js").is_file()
            or (path / "storage").is_dir()
        )

    preferred = str(preferred or "").strip()
    if preferred:
        candidate = Path(preferred).expanduser()
        if looks_like_profile(candidate):
            return candidate

    env_profile = os.environ.get("FIREFOX_PROFILE_PATH", "").strip()
    if env_profile:
        env_candidate = Path(env_profile).expanduser()
        if looks_like_profile(env_candidate):
            return env_candidate

    ini_candidates = [
        Path.home() / ".mozilla" / "firefox" / "profiles.ini",
        Path.home() / "AppData" / "Roaming" / "Mozilla" / "Firefox" / "profiles.ini",
    ]
    for ini_path in ini_candidates:
        if not ini_path.is_file():
            continue
        parser = configparser.ConfigParser()
        try:
            parser.read(ini_path, encoding="utf-8")
        except Exception:
            continue
        sections = [name for name in parser.sections() if name.lower().startswith("profile")]
        for section in sections:
            path_value = parser.get(section, "Path", fallback="")
            if not path_value:
                continue
            is_relative = parser.get(section, "IsRelative", fallback="1") == "1"
            profile_path = (ini_path.parent / path_value) if is_relative else Path(path_value)
            profile_path = profile_path.expanduser()
            if looks_like_profile(profile_path):
                if parser.get(section, "Default", fallback="0") == "1":
                    return profile_path
        for section in sections:
            path_value = parser.get(section, "Path", fallback="")
            if not path_value:
                continue
            is_relative = parser.get(section, "IsRelative", fallback="1") == "1"
            profile_path = (ini_path.parent / path_value) if is_relative else Path(path_value)
            profile_path = profile_path.expanduser()
            if looks_like_profile(profile_path):
                return profile_path
    return None


def _domain_matches(cookie_domain, host):
    cookie_domain = str(cookie_domain or "").lstrip(".").lower()
    host = str(host or "").lower()
    if not cookie_domain or not host:
        return False
    return host == cookie_domain or host.endswith(f".{cookie_domain}")


def _cookie_db_for_read(profile_path, temp_root):
    db_path = Path(profile_path) / "cookies.sqlite"
    if not db_path.is_file():
        backup = Path(profile_path) / "cookies.sqlite.bak"
        if backup.is_file():
            db_path = backup
    if not db_path.is_file():
        return Path()
    snapshot = Path(temp_root) / "cookies.sqlite"
    try:
        shutil.copy2(db_path, snapshot)
        for suffix in ("-wal", "-shm"):
            sidecar = Path(str(db_path) + suffix)
            if sidecar.is_file():
                shutil.copy2(sidecar, Path(str(snapshot) + suffix))
    except OSError:
        return db_path
    return snapshot


def _cookie_header_for_host(profile_path, host):
    if not profile_path or not host:
        return ""
    query = (
        "SELECT name, value, host, path, isSecure, expiry "
        "FROM moz_cookies "
        "WHERE expiry = 0 OR expiry > ?"
    )
    now = int(time.time())
    cookies = []
    try:
        with tempfile.TemporaryDirectory(prefix="rockpod-firefox-cookies-") as temp_dir:
            db_path = _cookie_db_for_read(profile_path, temp_dir)
            if not db_path.is_file():
                return ""
            connection = sqlite3.connect(f"file:{db_path}?mode=ro", uri=True)
            try:
                cursor = connection.cursor()
                for name, value, cookie_host, _path, _secure, expiry in cursor.execute(query, (now,)):
                    if not _domain_matches(cookie_host, host):
                        continue
                    if int(expiry or 0) and int(expiry) < now:
                        continue
                    if name and value is not None:
                        cookies.append(f"{name}={value}")
            finally:
                connection.close()
    except Exception:
        return ""
    return "; ".join(cookies)


def _normalize_resource_url(value, base_url):
    value = str(value or "").strip()
    if not value or value.startswith("#"):
        return ""
    if value.lower().startswith(("mailto:", "tel:", "javascript:", "data:")):
        return ""
    candidate = urljoin(base_url, value)
    parsed = urlsplit(candidate)
    if parsed.scheme not in {"http", "https"} or not parsed.netloc:
        return ""
    return parsed._replace(fragment="").geturl()


def _offline_reference_url(value):
    parsed = urlsplit(str(value or ""))
    if parsed.scheme in {"http", "https"} and parsed.netloc:
        return parsed._replace(query="", fragment="").geturl()
    return str(value or "").split("#", 1)[0].split("?", 1)[0]


def _append_unique(items, seen, value):
    if value and value not in seen:
        seen.add(value)
        items.append(value)


def _extract_srcset_links(value, base_url, out, seen):
    for item in str(value or "").split(","):
        url = item.strip().split(" ", 1)[0].strip()
        _append_unique(out, seen, _normalize_resource_url(url, base_url))


def _extract_html_links(html_text, base_url):
    assets = []
    pages = []
    seen_assets = set()
    seen_pages = set()

    for match in re.finditer(r"(?is)<(a|link|img|script|source|iframe|embed|object|video|audio)\b([^>]*)>", html_text):
        tag = match.group(1).lower()
        attrs = match.group(2) or ""
        values = {}
        for attr in re.finditer(
            r"([A-Za-z_:][-A-Za-z0-9_:.]*)\s*=\s*"
            r"(?:([\"'])(.*?)\2|([^\"'\s>]+))",
            attrs,
            flags=re.S,
        ):
            values[attr.group(1).lower()] = attr.group(3) or attr.group(4) or ""
        if "srcset" in values:
            _extract_srcset_links(values.get("srcset"), base_url, assets, seen_assets)
        ref = values.get("href") or values.get("src") or values.get("data")
        url = _normalize_resource_url(ref, base_url)
        if not url:
            continue
        parsed = urlsplit(url)
        ext = Path(parsed.path).suffix.lower()
        if tag == "a" or ext in HTML_EXTS or not ext:
            _append_unique(pages, seen_pages, url)
        elif ext in ASSET_EXTS:
            _append_unique(assets, seen_assets, url)

    for match in re.findall(r"(?is)<style\b[^>]*>(.*?)</style>", html_text):
        css_assets, css_imports = _extract_css_links(match, base_url)
        for item in css_assets:
            _append_unique(assets, seen_assets, item)
        for item in css_imports:
            _append_unique(assets, seen_assets, item)
    return pages, assets


def _extract_css_links(css_text, base_url):
    assets = []
    imports = []
    seen_assets = set()
    seen_imports = set()

    for match in re.findall(r"(?is)url\(\s*([\"']?)(.*?)\1\s*\)", css_text):
        url = _normalize_resource_url(match[1], base_url)
        if not url:
            continue
        ext = Path(urlsplit(url).path).suffix.lower()
        if not ext or ext in ASSET_EXTS:
            _append_unique(assets, seen_assets, url)

    for match in re.findall(r"(?is)@import\s+(?:url\(\s*)?[\"']?([^\"') ;]+)", css_text):
        url = _normalize_resource_url(match, base_url)
        if url:
            _append_unique(imports, seen_imports, url)
    return assets, imports


def _same_site(url, root_host):
    host = urlsplit(url).netloc.lower()
    return host == root_host or host.endswith(f".{root_host}")


def _cookie_for_url(url, profile_path, use_firefox_cookies):
    if not use_firefox_cookies or not profile_path:
        return ""
    return _cookie_header_for_host(profile_path, urlsplit(url).netloc.lower())


def _fetch_live_url(url, cookie_header="", referer=""):
    headers = _browser_headers(url, cookie_header, referer)
    request = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(request, timeout=40) as response:
        body = response.read()
        content_type = str(response.headers.get("Content-Type") or "").split(";", 1)[0].strip().lower()
        final_url = response.geturl()
        status = int(getattr(response, "status", 200) or 200)
        return {
            "url": final_url,
            "status": status,
            "content_type": content_type,
            "body": body,
        }


def _fetch_live_resource(url, cookie_header="", referer=""):
    try:
        return _fetch_live_url(url, cookie_header, referer)
    except TypeError:
        return _fetch_live_url(url, cookie_header)


def _free_tcp_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as handle:
        handle.bind(("127.0.0.1", 0))
        return handle.getsockname()[1]


class _BidiClient:
    def __init__(self, url):
        parsed = urlsplit(url)
        port = parsed.port or 80
        path = parsed.path or "/"
        if parsed.query:
            path += "?" + parsed.query
        self._socket = socket.create_connection((parsed.hostname, port), timeout=10)
        self._next_id = 0
        key = base64.b64encode(os.urandom(16)).decode("ascii")
        request = (
            f"GET {path} HTTP/1.1\r\n"
            f"Host: {parsed.hostname}:{port}\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n"
        )
        self._socket.sendall(request.encode("ascii"))
        response = b""
        while b"\r\n\r\n" not in response:
            chunk = self._socket.recv(4096)
            if not chunk:
                break
            response += chunk
        if b"101 Switching Protocols" not in response.split(b"\r\n", 1)[0]:
            raise RuntimeError("firefox-bidi-handshake-failed")

    def close(self):
        try:
            self._socket.close()
        except OSError:
            pass

    def _send_text(self, text):
        payload = text.encode("utf-8")
        size = len(payload)
        header = bytearray([0x81])
        if size < 126:
            header.append(0x80 | size)
        elif size < 65536:
            header += bytes([0x80 | 126]) + struct.pack("!H", size)
        else:
            header += bytes([0x80 | 127]) + struct.pack("!Q", size)
        mask = os.urandom(4)
        masked = bytes(value ^ mask[index % 4] for index, value in enumerate(payload))
        self._socket.sendall(header + mask + masked)

    def _recv_text(self):
        while True:
            header = self._socket.recv(2)
            if len(header) < 2:
                raise EOFError("firefox-bidi-closed")
            first, second = header
            opcode = first & 0x0F
            size = second & 0x7F
            if size == 126:
                size = struct.unpack("!H", self._socket.recv(2))[0]
            elif size == 127:
                size = struct.unpack("!Q", self._socket.recv(8))[0]
            mask = self._socket.recv(4) if second & 0x80 else b""
            payload = b""
            while len(payload) < size:
                payload += self._socket.recv(size - len(payload))
            if mask:
                payload = bytes(
                    value ^ mask[index % 4]
                    for index, value in enumerate(payload)
                )
            if opcode == 1:
                return payload.decode("utf-8", "replace")
            if opcode == 8:
                raise EOFError("firefox-bidi-closed")

    def command(self, method, params=None, timeout=30):
        self._next_id += 1
        command_id = self._next_id
        self._send_text(
            json.dumps(
                {
                    "id": command_id,
                    "method": method,
                    "params": params or {},
                }
            )
        )
        deadline = time.time() + timeout
        while time.time() < deadline:
            message = json.loads(self._recv_text())
            if message.get("id") != command_id:
                continue
            if message.get("type") == "error":
                raise RuntimeError(message.get("error") or method)
            return message
        raise TimeoutError(method)


def _wait_for_bidi(port):
    deadline = time.time() + 20
    last_error = None
    while time.time() < deadline:
        try:
            return _BidiClient(f"ws://127.0.0.1:{port}/session")
        except Exception as exc:
            last_error = exc
            time.sleep(0.5)
    raise RuntimeError(f"firefox-bidi-unavailable:{last_error}")


def _copy_firefox_profile(profile_path, destination):
    ignored = {
        "lock",
        ".parentlock",
        "cache2",
        "startupCache",
        "minidumps",
        "crashes",
        "saved-telemetry-pings",
    }
    shutil.copytree(
        profile_path,
        destination,
        dirs_exist_ok=True,
        ignore=lambda _src, names: ignored.intersection(names),
        symlinks=True,
    )
    for name in ("lock", ".parentlock"):
        try:
            (destination / name).unlink()
        except OSError:
            pass


def _browser_snapshot(url, work_root, profile_path, oldest_date=""):
    firefox = shutil.which("firefox")
    if not firefox:
        return {}

    profile_copy = Path(work_root) / "firefox-profile"
    if profile_path:
        _copy_firefox_profile(Path(profile_path), profile_copy)
    else:
        profile_copy.mkdir(parents=True, exist_ok=True)
    port = _free_tcp_port()
    process = subprocess.Popen(
        [
            firefox,
            "--headless",
            "--profile",
            str(profile_copy),
            "--remote-debugging-port",
            str(port),
            "--no-remote",
            url,
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    client = None
    try:
        client = _wait_for_bidi(port)
        client.command("session.new", {"capabilities": {"alwaysMatch": {}}})
        tree = client.command("browsingContext.getTree", {})
        contexts = (tree.get("result") or {}).get("contexts") or []
        if not contexts:
            return {}
        context = contexts[0].get("context")
        try:
            client.command(
                "browsingContext.setViewport",
                {
                    "context": context,
                    "viewport": {"width": 314, "height": 202},
                    "devicePixelRatio": 1,
                },
            )
        except Exception:
            pass
        expression = """
JSON.stringify({
  url: location.href,
  title: document.title,
  text: document.body ? document.body.innerText : "",
  html: document.documentElement ? document.documentElement.outerHTML : "",
  scrollHeight: Math.max(
    document.body ? document.body.scrollHeight : 0,
    document.documentElement ? document.documentElement.scrollHeight : 0
  ),
  images: Array.from(document.images || [])
    .map((item) => item.currentSrc || item.src)
    .filter(Boolean),
  resources: performance.getEntriesByType("resource")
    .map((item) => item.name)
    .filter(Boolean),
  links: Array.from(document.links || []).slice(0, 80).map((item) => ({
    url: item.href,
    text: (item.innerText || item.textContent || item.href || "").trim()
  })).filter((item) => item.url)
})
"""
        if oldest_date and any(
            host in urlsplit(url).netloc.lower()
            for host in ("instagram.com", "onlyfans.com")
        ):
            last_height = 0
            stagnant = 0
            for _index in range(240):
                depth_result = client.command(
                    "script.evaluate",
                    {
                        "expression": """
JSON.stringify({
  height: Math.max(
    document.body ? document.body.scrollHeight : 0,
    document.documentElement ? document.documentElement.scrollHeight : 0
  ),
  oldest: Array.from(document.querySelectorAll("time[datetime]"))
    .map((item) => {
      const parsed = new Date(item.getAttribute("datetime"));
      return Number.isNaN(parsed.getTime())
        ? "" : parsed.toISOString().slice(0, 10);
    }).filter(Boolean).sort()[0] || ""
})
""",
                        "target": {"context": context},
                        "awaitPromise": True,
                    },
                )
                remote = ((depth_result.get("result") or {}).get("result") or {})
                depth = json.loads(remote.get("value") or "{}")
                if depth.get("oldest") and depth["oldest"] <= oldest_date:
                    break
                height = int(depth.get("height") or 0)
                stagnant = stagnant + 1 if height <= last_height else 0
                last_height = max(last_height, height)
                if stagnant >= 8:
                    break
                client.command(
                    "script.evaluate",
                    {
                        "expression": "window.scrollTo(0, document.documentElement.scrollHeight); true",
                        "target": {"context": context},
                        "awaitPromise": False,
                    },
                )
                time.sleep(0.35)

        def with_preview(data):
            previews = []
            try:
                scroll_height = max(202, int(data.get("scrollHeight") or 202))
                for offset in range(0, min(scroll_height, 202 * 8), 202):
                    client.command(
                        "script.evaluate",
                        {
                            "expression": f"window.scrollTo(0, {offset}); true",
                            "target": {"context": context},
                            "awaitPromise": False,
                        },
                    )
                    time.sleep(0.12)
                    capture = client.command(
                        "browsingContext.captureScreenshot",
                        {"context": context, "origin": "viewport"},
                    )
                    value = str((capture.get("result") or {}).get("data") or "")
                    if value:
                        previews.append(value)
            except Exception:
                pass
            data["preview_pngs"] = previews
            data["preview_png"] = previews[0] if previews else ""
            return data

        best = {}
        deadline = time.time() + BROWSER_EXPORT_TIMEOUT
        while time.time() < deadline:
            time.sleep(2)
            result = client.command(
                "script.evaluate",
                {
                    "expression": expression,
                    "target": {"context": context},
                    "awaitPromise": True,
                },
            )
            remote = ((result.get("result") or {}).get("result") or {})
            data = json.loads(remote.get("value") or "{}")
            html_text = str(data.get("html") or "")
            body_text = str(data.get("text") or "")
            if len(html_text) > len(str(best.get("html") or "")):
                best = data
            if html_text and body_text and not _is_bad_html_capture(body_text, url):
                if len(body_text.strip()) > 80:
                    return with_preview(data)
        return with_preview(best) if best else best
    finally:
        if client:
            try:
                client.command("session.end", {}, timeout=5)
            except Exception:
                pass
            client.close()
        process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()


def _sync_browser_export(url, work_root, archive_root, profile_path,
                         use_firefox_cookies=True, oldest_date=""):
    try:
        snapshot = _browser_snapshot(
            url, work_root, profile_path, oldest_date=oldest_date
        )
    except Exception as exc:
        reason = str(exc).replace("\n", " ")
        return {
            "success": False,
            "reason": f"browser-export-failed:{reason[:240]}",
        }

    final_url = str(snapshot.get("url") or url)
    if not snapshot or not (
        snapshot.get("html") or snapshot.get("text") or snapshot.get("preview_png")
    ):
        return {"success": False, "reason": "browser-export-empty"}
    cached_image_urls = []
    cached_image_refs = []
    cache_assets_saved = 0
    preview_urls = []
    preview_paths = []
    preview_data_items = list(snapshot.get("preview_pngs") or [])
    if not preview_data_items and snapshot.get("preview_png"):
        preview_data_items = [snapshot.get("preview_png")]
    preview_token = hashlib.sha1(url.encode("utf-8")).hexdigest()[:10]
    for index, preview_data in enumerate(preview_data_items[:8]):
        preview_url = urlsplit(url)._replace(
            path=(urlsplit(url).path.rstrip("/") or "/index")
            + f".rockpod-preview-{preview_token}-{index + 1:02d}.png",
            query="",
            fragment="",
        ).geturl()
        try:
            preview_written = _write_url_record(
                archive_root,
                preview_url,
                base64.b64decode(preview_data),
                content_type="image/png",
            )
        except (ValueError, TypeError):
            preview_written = ""
        if preview_written:
            _convert_image_sidecar(preview_written)
            preview_urls.append(preview_url)
            preview_paths.append(preview_written)
    if profile_path:
        for record in _firefox_cache_image_records(
            profile_path,
            page_url=final_url,
            snapshot=snapshot,
        ):
            asset_written = _write_url_record(
                archive_root,
                record.get("url") or "",
                record.get("body") or b"",
                content_type=record.get("content_type") or "",
            )
            if not asset_written:
                continue
            cache_assets_saved += 1
            _convert_image_sidecar(asset_written)
            cached_image_urls.append(str(record.get("url") or ""))
            try:
                relative = Path(asset_written).relative_to(archive_root)
                cached_image_refs.append(
                    "/.rockbox/offlineweb/archive/" + relative.as_posix()
                )
            except ValueError:
                pass

    cached_image_refs = _compact_social_image_refs(
        archive_root, cached_image_refs, url
    )
    logo_url = _browser_logo_url(snapshot, final_url)
    html_text = _browser_static_html(
        snapshot,
        url,
        extra_images=cached_image_refs,
        logo_url=logo_url,
        preview_images=preview_urls,
    )
    body_text = str(snapshot.get("text") or "")
    if not html_text:
        return {"success": False, "reason": "browser-export-empty"}
    if _is_bad_html_capture(html_text + "\n" + body_text, url):
        return {"success": False, "reason": "browser-export-bad-html"}

    written = _write_url_record(
        archive_root,
        final_url,
        html_text.encode("utf-8"),
        content_type="text/html",
    )
    if not written:
        return {"success": False, "reason": "browser-export-write-failed"}
    _write_social_profile_pages(
        archive_root,
        url,
        _browser_page_title(snapshot, url),
        written,
        cached_image_refs,
        oldest_date,
    )
    if _social_site(url)[0]:
        return {
            "success": True,
            "source": "firefox-browser-export",
            "html_saved": 1,
            "assets_saved": cache_assets_saved,
            "cache_images_saved": cache_assets_saved,
            "title": _browser_page_title(snapshot, url),
            "entry_path": written,
            "preview_path": preview_paths[0] if preview_paths else "",
        }

    queued_assets = []
    seen_assets = set()
    cached_paths = {
        urlsplit(item)._replace(query="", fragment="").geturl()
        for item in cached_image_urls
    }
    if not preview_urls:
        _pages, html_assets = _extract_html_links(html_text, final_url)
        for asset_url in html_assets:
            ext = Path(urlsplit(asset_url).path).suffix.lower()
            path_key = urlsplit(asset_url)._replace(query="", fragment="").geturl()
            if ext in IMAGE_EXTS and path_key not in cached_paths:
                _append_unique(queued_assets, seen_assets, asset_url)
    preview_keys = {
        urlsplit(item)._replace(query="", fragment="").geturl()
        for item in preview_urls
    }
    for asset_url in (snapshot.get("images") or []) + (snapshot.get("resources") or []):
        parsed = urlsplit(str(asset_url))
        ext = Path(parsed.path).suffix.lower()
        path_key = parsed._replace(query="", fragment="").geturl()
        if path_key in preview_keys or path_key in cached_paths:
            continue
        if parsed.scheme in {"http", "https"} and parsed.netloc:
            _append_unique(queued_assets, seen_assets, str(asset_url))

    assets_saved = 0
    for asset_url in queued_assets:
        cookie_header = _cookie_for_url(asset_url, profile_path, use_firefox_cookies)
        try:
            asset = _fetch_live_resource(asset_url, cookie_header, final_url)
        except Exception:
            continue
        if int(asset.get("status") or 0) != 200:
            continue
        asset_written = _write_url_record(
            archive_root,
            asset.get("url") or asset_url,
            asset.get("body") or b"",
            content_type=asset.get("content_type") or "",
        )
        if not asset_written:
            continue
        assets_saved += 1
        _convert_image_sidecar(asset_written)

    return {
        "success": True,
        "source": "firefox-browser-export",
        "html_saved": 1,
        "assets_saved": assets_saved + cache_assets_saved,
        "cache_images_saved": cache_assets_saved,
        "title": _browser_page_title(snapshot, url),
        "entry_path": written,
        "preview_path": preview_paths[0] if preview_paths else "",
    }


def _browser_logo_url(snapshot, fallback_url):
    if "onlyfans.com" not in urlsplit(str(fallback_url or "")).netloc.lower():
        return ""
    html_text = str(snapshot.get("html") or "")
    pattern = r"https://static2\.onlyfans\.com/[^\"'<> ]*of-logo[^\"'<> ]+\.(?:jpg|jpeg|png|svg|webp)"
    for value in re.findall(pattern, html_text, flags=re.I):
        if Path(urlsplit(value).path).suffix.lower() in IMAGE_EXTS:
            return html.unescape(value)
    return ONLYFANS_LOGO_FALLBACK


def _browser_page_title(snapshot, fallback_url):
    title = str(snapshot.get("title") or "").strip()
    title = re.sub(r"^\(\d+\+?\)\s*", "", title)
    parsed = urlsplit(str(fallback_url or ""))
    host = parsed.netloc.lower().removeprefix("www.")
    host_label = host.split(".", 1)[0]
    if title.lower() not in {"", "home", host, host_label}:
        return title
    slug = Path(parsed.path.rstrip("/")).stem
    if slug and slug.lower() not in {"index", "home"}:
        label = re.sub(r"[-_]+", " ", slug).strip()
        if label:
            return label.title()
    return title or parsed.netloc or "Offline Page"


def _compact_social_image_refs(archive_root, image_refs, page_url):
    host = urlsplit(str(page_url or "")).netloc.lower()
    if "instagram.com" not in host and "onlyfans.com" not in host:
        return image_refs

    site = "instagram" if "instagram.com" in host else "onlyfans"
    profile_key = _safe_name(
        Path(urlsplit(str(page_url or "")).path.rstrip("/")).name or "home"
    )
    target_root = Path(archive_root) / host / "_rockpod_media" / profile_key
    target_root.mkdir(parents=True, exist_ok=True)
    compact_refs = []
    used_names = set()
    post_index = 1

    for ref in image_refs:
        prefix = "/.rockbox/offlineweb/archive/"
        if not str(ref).startswith(prefix):
            continue
        source = Path(archive_root) / str(ref)[len(prefix):]
        if not source.is_file():
            continue

        source_sidecar = Path(str(source) + ".bmp")
        if site == "instagram" and source_sidecar.is_file():
            header = source_sidecar.read_bytes()[:26]
            if len(header) == 26 and header[:2] == b"BM":
                width = int.from_bytes(header[18:22], "little", signed=True)
                height = abs(
                    int.from_bytes(header[22:26], "little", signed=True)
                )
                if width <= 150 and height <= 150:
                    if "avatar.jpg" not in used_names:
                        name = "avatar.jpg"
                    else:
                        continue
                else:
                    name = ""
            else:
                name = ""
        else:
            name = ""

        lower = str(source).lower()
        if name:
            pass
        elif site == "onlyfans" and lower.endswith("/header.jpg"):
            name = "cover.jpg"
        elif site == "onlyfans" and lower.endswith("/avatar.jpg"):
            name = "avatar.jpg"
        else:
            name = f"{post_index:02d}{source.suffix.lower() or '.jpg'}"
            post_index += 1
        if name in used_names:
            continue
        used_names.add(name)

        target = target_root / name
        target.write_bytes(source.read_bytes())
        _track_cache_file(target)
        if source_sidecar.is_file():
            target_sidecar = Path(str(target) + ".bmp")
            target_sidecar.write_bytes(source_sidecar.read_bytes())
            _track_cache_file(target_sidecar)
        compact_refs.append(
            "/.rockbox/offlineweb/archive/"
            + target.relative_to(archive_root).as_posix()
        )
        if len(compact_refs) >= 180:
            break

    return compact_refs or image_refs


def _social_site(url):
    host = urlsplit(str(url or "")).netloc.lower()
    if "instagram.com" in host:
        return "instagram", "Instagram"
    if "onlyfans.com" in host:
        return "onlyfans", "OnlyFans"
    return "", ""


def _write_social_profile_pages(
    archive_root, page_url, title, entry_path, image_refs, oldest_date=""
):
    site, site_title = _social_site(page_url)
    if not site:
        return
    host = urlsplit(page_url).netloc.lower()
    root = Path(archive_root) / host
    media_root = root / "_rockpod_media"
    manifest_path = media_root / "profiles.json"
    try:
        profiles = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, TypeError, ValueError):
        profiles = []
    if not isinstance(profiles, list):
        profiles = []
    handle = Path(urlsplit(page_url).path.rstrip("/")).name or title
    avatar = next(
        (ref for ref in image_refs if ref.lower().endswith("/avatar.jpg")),
        "",
    )
    page_ref = (
        "/.rockbox/offlineweb/archive/"
        + Path(entry_path).relative_to(archive_root).as_posix()
    )
    profile = {
        "url": page_url,
        "title": title or handle,
        "handle": handle,
        "page": page_ref,
        "avatar": avatar,
        "oldest_date": oldest_date,
    }
    profiles = [
        item for item in profiles
        if isinstance(item, dict) and item.get("url") != page_url
    ]
    profiles.append(profile)
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(profiles, indent=2), encoding="utf-8")
    _track_cache_file(manifest_path)

    rows = []
    for item in sorted(
        profiles, key=lambda value: str(value.get("title") or "").casefold()
    ):
        if item.get("avatar"):
            rows.append(
                '<img src="{src}" alt="" width="52" height="52" data-static="1">'.format(
                    src=html.escape(str(item["avatar"]), quote=True)
                )
            )
        rows.append(
            '<a href="{href}">{title}</a>'.format(
                href=html.escape(str(item.get("page") or ""), quote=True),
                title=html.escape(str(item.get("title") or item.get("handle") or "Profile")),
            )
        )
        detail = "@" + str(item.get("handle") or "")
        if item.get("oldest_date"):
            detail += "  Posts since " + str(item["oldest_date"])
        rows.append(f"<p>{html.escape(detail)}</p>")
    library_path = root / "profiles.html"
    library_path.write_text(
        "<!DOCTYPE html><html><head>"
        f"<title>{site_title}</title>"
        f'<meta name="rockpod-render-mode" content="native-{site}-profiles">'
        "</head><body>"
        f"<h1>{site_title} Profiles</h1>"
        + "\n".join(rows)
        + "</body></html>\n",
        encoding="utf-8",
    )
    _track_cache_file(library_path)


def _browser_static_html(
    snapshot,
    fallback_url,
    extra_images=None,
    logo_url="",
    preview_images=None,
):
    title = _browser_page_title(snapshot, fallback_url)
    preview_images = [str(value or "") for value in (preview_images or []) if value]
    body_text = str(snapshot.get("text") or "")
    cached_refs = [
        str(value or "").strip()
        for value in (extra_images or [])
        if str(value or "").strip()
    ]
    image_urls = []
    seen = set()

    for value in extra_images or []:
        value = str(value or "").strip()
        if not value or value in seen:
            continue
        if Path(urlsplit(value).path).suffix.lower() not in IMAGE_EXTS:
            continue
        seen.add(value)
        image_urls.append(value)
    if logo_url:
        _append_unique(image_urls, seen, logo_url)
    for value in snapshot.get("images") or []:
        value = str(value or "").strip()
        if not value or value in seen:
            continue
        if Path(urlsplit(value).path).suffix.lower() not in IMAGE_EXTS:
            continue
        seen.add(value)
        image_urls.append(value)

    lines = []
    for raw in body_text.splitlines():
        line = re.sub(r"\s+", " ", raw).strip()
        if len(line) < 2:
            continue
        if lines and lines[-1].casefold() == line.casefold():
            continue
        lines.append(line[:240])
        if len(lines) >= 48:
            break

    lead_html = ""
    if image_urls:
        lead_html = (
            '<img src="{src}" alt="{alt}" width="314" height="202">'
        ).format(
            src=html.escape(_offline_reference_url(image_urls[0]), quote=True),
            alt=html.escape(title, quote=True),
        )
    image_html = "\n".join(
        '<img src="{src}" alt="{alt}" width="314" height="202">'.format(
            src=html.escape(_offline_reference_url(src), quote=True),
            alt=html.escape(title, quote=True),
        )
        for src in image_urls[1:17]
    )
    text_html = "\n".join(
        f"<p>{html.escape(line)}</p>"
        for line in lines
    )
    if "instagram.com" in urlsplit(str(fallback_url or "")).netloc.lower():
        social_images = cached_refs or image_urls
        avatar_images = [
            src for src in social_images if src.lower().endswith("/avatar.jpg")
        ]
        feed_images = [
            src for src in social_images
            if not src.lower().endswith("/avatar.jpg")
        ][:180]
        handle = Path(urlsplit(str(fallback_url)).path.rstrip("/")).name
        avatar_html = (
            '<img src="{src}" alt="" width="56" height="56">'.format(
                src=html.escape(_offline_reference_url(avatar_images[0]), quote=True)
            )
            if avatar_images else ""
        )
        feed_html = "\n".join(
            '<img src="{src}" alt="" width="314" height="202">'.format(
                src=html.escape(_offline_reference_url(src), quote=True),
            )
            for src in feed_images
        )
        return (
            "<!DOCTYPE html>\n"
            "<html><head>"
            f"<title>{html.escape(title)}</title>"
            '<meta name="rockpod-render-mode" content="native-feed">'
            "</head><body>"
            f"{avatar_html}"
            f"<h1>{html.escape(title)}</h1>"
            f"<p>@{html.escape(handle)}</p>"
            f"{feed_html}"
            "</body></html>\n"
        )
    if "onlyfans.com" in urlsplit(str(fallback_url or "")).netloc.lower():
        creator = re.sub(r"\s+OnlyFans$", "", title, flags=re.I).strip()
        started = False
        profile_lines = []
        ignored = {
            "my profile", "collections", "settings", "dark mode", "english",
            "log out", "help and support", "home", "notifications",
            "messages", "bookmarks",
        }
        for line in lines:
            if not started:
                if line.casefold() == creator.casefold():
                    started = True
                else:
                    continue
            if line.casefold() in ignored:
                continue
            if re.fullmatch(r"[\d.,KkMm]+", line):
                continue
            if line.casefold() == creator.casefold() and profile_lines:
                continue
            profile_lines.append(line)
            if len(profile_lines) >= 28:
                break
        profile_html = "\n".join(
            f"<p>{html.escape(line)}</p>" for line in profile_lines
        )
        site_images = [
            src for src in (cached_refs or image_urls)
            if "of-logo" not in src.lower()
        ]
        cover_images = [
            src for src in site_images
            if urlsplit(src).path.lower().endswith("/header.jpg")
        ]
        avatar_images = [
            src for src in site_images
            if urlsplit(src).path.lower().endswith("/avatar.jpg")
        ]
        post_images = [
            src for src in site_images
            if src not in cover_images and src not in avatar_images
        ][:180]

        def onlyfans_image(src, alt, height=202):
            return '<img src="{src}" alt="{alt}" width="314" height="{height}">'.format(
                src=html.escape(_offline_reference_url(src), quote=True),
                alt=html.escape(alt, quote=True),
                height=height,
            )

        cover_html = onlyfans_image(cover_images[0], creator) if cover_images else ""
        avatar_html = (
            onlyfans_image(avatar_images[0], creator, 144)
            if avatar_images else ""
        )
        posts_html = "\n".join(
            onlyfans_image(src, creator) for src in post_images
        )
        return (
            "<!DOCTYPE html>\n"
            "<html><head>"
            f"<title>{html.escape(title)}</title>"
            '<meta name="rockpod-render-mode" content="native-profile">'
            "</head><body>"
            f"{cover_html}"
            f"<h1>{html.escape(creator or title)}</h1>"
            f"{avatar_html}"
            f"{profile_html}"
            f"{posts_html}"
            "</body></html>\n"
        )
    return (
        "<!DOCTYPE html>\n"
        "<html><head>"
        f"<title>{html.escape(title)}</title>"
        '<meta name="rockpod-render-mode" content="native-snapshot">'
        "</head><body>"
        f"{lead_html}"
        f"<h1>{html.escape(title)}</h1>"
        f"{'<h2>Saved page</h2>' if text_html else ''}"
        f"{text_html}"
        f"{'<h2>Saved media</h2>' if image_html else ''}"
        f"{image_html}"
        "</body></html>\n"
    )


def _sync_live_url(url, archive_root, profile_path=Path(), use_firefox_cookies=True):
    parsed = urlsplit(url)
    host = parsed.netloc.lower()
    cookie_header = _cookie_for_url(url, profile_path, use_firefox_cookies)
    cookie_count = cookie_header.count(";") + 1 if cookie_header else 0
    queued_pages = [(url, 0)]
    seen_pages = {url}
    queued_assets = []
    seen_assets = set()
    html_saved = 0
    assets_saved = 0
    entry_path = ""
    entry_title = ""

    while queued_pages and html_saved < MAX_LIVE_PAGES:
        page_url, depth = queued_pages.pop(0)
        page_cookie_header = _cookie_for_url(page_url, profile_path, use_firefox_cookies)
        try:
            page = _fetch_live_resource(
                page_url,
                page_cookie_header,
                url if page_url != url else "",
            )
        except Exception as exc:
            if page_url == url:
                return {
                    "success": False,
                    "reason": f"live-fetch-failed:{exc}",
                    "cookie_count": cookie_count,
                }
            continue

        if int(page.get("status") or 0) != 200:
            if page_url == url:
                return {
                    "success": False,
                    "reason": f"http-{page.get('status') or 0}",
                    "cookie_count": cookie_count,
                }
            continue

        content_type = str(page.get("content_type") or "")
        if content_type.startswith("text/html"):
            body_text = (page.get("body") or b"").decode("utf-8", "ignore")
            if _is_bad_html_capture(body_text, page.get("url") or page_url):
                if page_url == url:
                    return {
                        "success": False,
                        "reason": "bad-html-capture",
                        "cookie_count": cookie_count,
                    }
                continue

        written = _write_url_record(
            archive_root,
            page.get("url") or page_url,
            page.get("body") or b"",
            content_type=page.get("content_type") or "",
        )
        if not written:
            continue
        ext = Path(written).suffix.lower()
        if ext in HTML_EXTS:
            html_saved += 1
            if page_url == url:
                entry_path = written
                title_match = re.search(
                    r"(?is)<title\b[^>]*>(.*?)</title>",
                    body_text,
                )
                if title_match:
                    entry_title = re.sub(
                        r"\s+", " ", html.unescape(title_match.group(1))
                    ).strip()
        else:
            assets_saved += 1
            _convert_image_sidecar(written)
            continue

        if not content_type.startswith("text/html"):
            continue

        body = page.get("body") or b""
        html_text = body.decode("utf-8", "ignore")
        found_pages, found_assets = _extract_html_links(
            html_text,
            page.get("url") or page_url,
        )
        for asset_url in found_assets:
            if len(queued_assets) >= MAX_LIVE_ASSETS:
                break
            _append_unique(queued_assets, seen_assets, asset_url)
        if depth < MAX_LIVE_DEPTH:
            for next_url in found_pages:
                if len(seen_pages) >= MAX_LIVE_PAGES:
                    break
                if not _same_site(next_url, host):
                    continue
                if next_url not in seen_pages:
                    seen_pages.add(next_url)
                    queued_pages.append((next_url, depth + 1))

    while queued_assets and assets_saved < MAX_LIVE_ASSETS:
        asset_url = queued_assets.pop(0)
        asset_cookie_header = _cookie_for_url(
            asset_url,
            profile_path,
            use_firefox_cookies,
        )
        try:
            asset = _fetch_live_resource(asset_url, asset_cookie_header, url)
        except Exception:
            continue
        if int(asset.get("status") or 0) != 200:
            continue
        asset_written = _write_url_record(
            archive_root,
            asset.get("url") or asset_url,
            asset.get("body") or b"",
            content_type=asset.get("content_type") or "",
        )
        if not asset_written:
            continue
        ext = Path(asset_written).suffix.lower()
        if ext in HTML_EXTS:
            html_saved += 1
        else:
            assets_saved += 1
            _convert_image_sidecar(asset_written)
            if ext == ".css":
                try:
                    css_text = (asset.get("body") or b"").decode("utf-8", "ignore")
                except Exception:
                    css_text = ""
                css_assets, css_imports = _extract_css_links(
                    css_text,
                    asset.get("url") or asset_url,
                )
                for item in css_imports + css_assets:
                    if len(queued_assets) >= MAX_LIVE_ASSETS:
                        break
                    _append_unique(queued_assets, seen_assets, item)

    return {
        "success": html_saved > 0,
        "source": "live-firefox-session" if cookie_header else "live-public",
        "html_saved": html_saved,
        "assets_saved": assets_saved,
        "cookie_count": cookie_count,
        "entry_path": entry_path,
        "title": entry_title,
    }


def _resolve_snapshot_url(url):
    api_url = WAYBACK_AVAILABLE.format(url=quote(url, safe=""))
    data = _fetch_json(api_url)
    closest = (((data or {}).get("archived_snapshots") or {}).get("closest") or {})
    snapshot = str(closest.get("url") or "").strip()
    if snapshot:
        return snapshot.replace("http://", "https://", 1)
    return ""


def _try_snapshot_zip(snapshot_url, destination):
    if not snapshot_url:
        return ""
    parsed = urlparse(snapshot_url)
    path = parsed.path
    if "/web/" not in path:
        return ""
    prefix, rest = path.split("/web/", 1)
    if "/" not in rest:
        return ""
    stamp, original = rest.split("/", 1)
    if not stamp or not original:
        return ""
    archive_id = f"wayback-{stamp}-{_safe_name(urlparse(original).netloc)}"
    archive_url = f"https://archive.org/download/{archive_id}/{archive_id}.zip"
    try:
        _download_file(archive_url, destination)
    except Exception:
        return ""
    if not zipfile.is_zipfile(destination):
        return ""
    return archive_id


def _is_youtube_url(url):
    parsed = urlsplit(str(url or ""))
    host = parsed.netloc.lower().split(":", 1)[0]
    if host in {"youtu.be", "youtube.com", "www.youtube.com", "m.youtube.com"}:
        return host == "youtu.be" or parsed.path == "/watch" or parsed.path.startswith(
            ("/shorts/", "/live/", "/embed/")
        )
    return False


def _youtube_duration(value):
    try:
        seconds = max(0, int(float(value or 0)))
    except (TypeError, ValueError):
        return ""
    hours, remainder = divmod(seconds, 3600)
    minutes, seconds = divmod(remainder, 60)
    return (
        f"{hours}:{minutes:02d}:{seconds:02d}"
        if hours else f"{minutes}:{seconds:02d}"
    )


def _youtube_archive_ref(archive_root, path):
    try:
        relative = Path(path).relative_to(archive_root)
    except (TypeError, ValueError):
        return ""
    return "/.rockbox/offlineweb/archive/" + relative.as_posix()


def _youtube_channel_key(metadata):
    value = str(
        metadata.get("channel_id")
        or metadata.get("uploader_id")
        or metadata.get("channel")
        or metadata.get("uploader")
        or "youtube-member"
    )
    value = re.sub(r"[^A-Za-z0-9_-]+", "-", value).strip("-")
    return value[:48] or "youtube-member"


def _youtube_channel_avatar(metadata, work_root, media_root, ffmpeg):
    channel_key = _youtube_channel_key(metadata)
    avatar_path = Path(media_root) / f"avatar-{channel_key}.jpg"
    candidates = [
        str(metadata.get("channel_thumbnail") or "").strip(),
        str(metadata.get("uploader_avatar") or "").strip(),
    ]
    channel_url = str(metadata.get("channel_url") or "").strip()
    if channel_url:
        try:
            page = _fetch_live_resource(channel_url, "", "")
            page_text = (page.get("body") or b"").decode("utf-8", "ignore")
        except Exception:
            page_text = ""
        for pattern in (
            r'"avatar":\{"thumbnails":\[\{"url":"([^"]+)"',
            r'"channelThumbnail":\{"thumbnails":\[\{"url":"([^"]+)"',
        ):
            match = re.search(pattern, page_text)
            if not match:
                continue
            try:
                candidates.append(json.loads(f'"{match.group(1)}"'))
            except (TypeError, ValueError):
                candidates.append(
                    match.group(1).replace("\\u0026", "&").replace("\\/", "/")
                )
            break

    avatar_url = next(
        (value for value in candidates if value.startswith(("http://", "https://"))),
        "",
    )
    if not avatar_url:
        return None
    try:
        avatar = _fetch_live_resource(avatar_url, "", channel_url)
    except Exception:
        return None
    if int(avatar.get("status") or 0) != 200 or not avatar.get("body"):
        return None

    source_path = Path(work_root) / f"avatar-{channel_key}.source"
    source_path.write_bytes(avatar["body"])
    result = subprocess.run(
        [
            ffmpeg,
            "-hide_banner",
            "-loglevel",
            "error",
            "-y",
            "-i",
            str(source_path),
            "-vf",
            (
                "scale=64:64:force_original_aspect_ratio=increase,"
                "crop=64:64"
            ),
            "-frames:v",
            "1",
            str(avatar_path),
        ],
        text=True,
        capture_output=True,
        timeout=120,
        check=False,
    )
    if result.returncode != 0 or not avatar_path.is_file():
        return None
    _convert_image_sidecar(avatar_path)
    return avatar_path


def _youtube_page_header(title):
    return (
        "<!DOCTYPE html>\n"
        "<html><head>"
        f"<title>{html.escape(title)}</title>"
        '<meta name="rockpod-render-mode" content="youtube-2007">'
        "<style>"
        "body{background:#fff;color:#000;font-family:Arial,sans-serif}"
        "a{color:#03c}.nav{border-bottom:1px solid #ccc;color:#03c}"
        "h1{color:#333;font-size:18px;border-bottom:1px solid #ccc}"
        ".channel{background:#e6f1fa;border:1px solid #b8d4e8}"
        ".video{border-bottom:1px solid #ddd}"
        "</style></head><body>\n"
        '<img src="/.rockbox/offlineweb/assets/youtube-logo-2006.bmp" '
        'alt="YouTube" width="128" height="52" data-static="1">\n'
        '<p class="nav">Videos | Categories | Channels | Community</p>\n'
    )


def _write_youtube_subscription_pages(
    archive_root, metadata, entry_path, poster_path, avatar_path
):
    youtube_root = Path(archive_root) / "www.youtube.com"
    media_root = youtube_root / "_rockpod_media"
    manifest_path = media_root / "subscriptions.json"
    try:
        entries = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, TypeError, ValueError):
        entries = []
    if not isinstance(entries, list):
        entries = []

    source_url = str(metadata.get("webpage_url") or "").strip()
    channel_key = _youtube_channel_key(metadata)
    channel_name = str(
        metadata.get("channel") or metadata.get("uploader") or "YouTube Member"
    ).strip()[:64]
    item = {
        "url": source_url,
        "title": str(metadata.get("title") or "YouTube Video").strip()[:96],
        "channel_key": channel_key,
        "channel": channel_name,
        "watch": _youtube_archive_ref(archive_root, entry_path),
        "poster": _youtube_archive_ref(archive_root, poster_path),
        "avatar": _youtube_archive_ref(archive_root, avatar_path),
        "duration": _youtube_duration(metadata.get("duration")),
        "upload_date": str(metadata.get("upload_date") or ""),
    }
    entries = [
        value for value in entries
        if isinstance(value, dict)
        and str(value.get("url") or "") != source_url
    ]
    entries.append(item)
    entries.sort(
        key=lambda value: (
            str(value.get("channel") or "").casefold(),
            str(value.get("upload_date") or ""),
            str(value.get("title") or "").casefold(),
        )
    )
    manifest_path.write_text(
        json.dumps(entries, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )

    channels = {}
    for value in entries:
        channels.setdefault(str(value.get("channel_key") or ""), []).append(value)

    subscription_lines = [
        _youtube_page_header("Subscriptions"),
        "<h1>Subscriptions</h1>\n",
        "<p>Channels saved on this iPod</p>\n",
    ]
    generated = [manifest_path]
    for key, videos in sorted(
        channels.items(),
        key=lambda pair: str(pair[1][0].get("channel") or "").casefold(),
    ):
        first = videos[0]
        channel_file = youtube_root / f"channel-{key}.html"
        channel_ref = _youtube_archive_ref(archive_root, channel_file)
        avatar_ref = next(
            (str(video.get("avatar") or "") for video in videos
             if video.get("avatar")),
            "",
        )
        subscription_lines.append(
            '<youtubelink href="{href}" label="{name}"></youtubelink>\n'.format(
                href=html.escape(channel_ref, quote=True),
                name=html.escape(
                    str(first.get("channel") or "YouTube Member"), quote=True
                ),
            )
        )
        if avatar_ref:
            subscription_lines.append(
                '<img src="{src}" alt="{alt}" width="64" height="64" '
                'data-static="1">\n'.format(
                    src=html.escape(avatar_ref, quote=True),
                    alt=html.escape(str(first.get("channel") or ""), quote=True),
                )
            )
        subscription_lines.append(
            f"<p>{len(videos)} synced video{'s' if len(videos) != 1 else ''}</p>\n"
        )

        channel_lines = [
            _youtube_page_header(str(first.get("channel") or "Channel")),
        ]
        if avatar_ref:
            channel_lines.append(
                '<img src="{src}" alt="{alt}" width="64" height="64" '
                'data-static="1">\n'.format(
                    src=html.escape(avatar_ref, quote=True),
                    alt=html.escape(str(first.get("channel") or ""), quote=True),
                )
            )
        channel_lines.append(
            f"<h1>{html.escape(str(first.get('channel') or 'YouTube Member'))}</h1>\n"
        )
        channel_lines.append("<h2>Videos</h2>\n")
        for video in reversed(videos):
            poster_ref = str(video.get("poster") or "")
            if poster_ref:
                channel_lines.append(
                    '<img src="{src}" alt="{alt}" width="120" height="90" '
                    'data-static="1">\n'.format(
                        src=html.escape(poster_ref, quote=True),
                        alt=html.escape(str(video.get("title") or ""), quote=True),
                    )
                )
            channel_lines.append(
                '<youtubelink href="{href}" label="{title}{duration}">'
                "</youtubelink>\n".format(
                    href=html.escape(str(video.get("watch") or ""), quote=True),
                    title=html.escape(
                        str(video.get("title") or "YouTube Video"), quote=True
                    ),
                    duration=(
                        " (" +
                        html.escape(str(video.get("duration")), quote=True) +
                        ")"
                        if video.get("duration") else ""
                    ),
                )
            )
        channel_lines.append("</body></html>\n")
        channel_file.write_text("".join(channel_lines), encoding="utf-8")
        generated.append(channel_file)

    subscriptions_path = youtube_root / "subscriptions.html"
    subscription_lines.append("</body></html>\n")
    subscriptions_path.write_text(
        "".join(subscription_lines),
        encoding="utf-8",
    )
    generated.append(subscriptions_path)
    for path in generated:
        _track_cache_file(path)


def _youtube_watch_html(metadata, video_ref, frame_file, frame_count):
    title = str(metadata.get("title") or "YouTube Video").strip()[:72]
    uploader = str(
        metadata.get("uploader") or metadata.get("channel") or "YouTube Member"
    ).strip()[:48]
    upload_date = str(
        metadata.get("upload_date") or metadata.get("release_date") or ""
    )
    if len(upload_date) == 8 and upload_date.isdigit():
        upload_date = (
            f"{upload_date[4:6]}/{upload_date[6:8]}/{upload_date[:4]}"
        )
    duration = _youtube_duration(metadata.get("duration"))
    view_count = metadata.get("view_count")
    try:
        views = f"{int(view_count):,} views"
    except (TypeError, ValueError):
        views = ""
    return (
        "<!DOCTYPE html>\n"
        "<html><head>"
        f"<title>{html.escape(title)}</title>"
        '<meta name="rockpod-render-mode" content="youtube-2007">'
        "<style>"
        "body{background:#fff;color:#000;font-family:Arial,sans-serif}"
        ".topnav{color:#03c;border-bottom:1px solid #ccc}"
        ".watch-meta{background:#e6f1fa;color:#333}"
        "h1{font-size:18px}"
        "</style></head><body>\n"
        '<youtube2007 '
        f'video="{html.escape(video_ref, quote=True)}" '
        f'frame-file="{html.escape(frame_file, quote=True)}" '
        f'frame-count="{int(frame_count)}" '
        f'title="{html.escape(title, quote=True)}" '
        f'uploader="{html.escape(uploader, quote=True)}" '
        f'added="{html.escape(upload_date, quote=True)}" '
        f'duration="{html.escape(duration, quote=True)}" '
        f'views="{html.escape(views, quote=True)}">'
        "</youtube2007>\n"
        "</body></html>\n"
    )


def _sync_youtube_url(url, work_root, archive_root):
    yt_dlp = shutil.which("yt-dlp")
    ffmpeg = shutil.which("ffmpeg")
    if not yt_dlp:
        return {"success": False, "reason": "yt-dlp-not-found"}
    if not ffmpeg:
        return {"success": False, "reason": "ffmpeg-not-found"}

    metadata_result = subprocess.run(
        [
            yt_dlp,
            "--no-playlist",
            "--dump-single-json",
            "--skip-download",
            url,
        ],
        text=True,
        capture_output=True,
        timeout=120,
        check=False,
    )
    if metadata_result.returncode != 0:
        reason = (metadata_result.stderr or metadata_result.stdout).strip()
        return {
            "success": False,
            "reason": f"youtube-metadata-failed:{reason[-240:]}",
        }
    try:
        metadata = json.loads(metadata_result.stdout)
    except (TypeError, ValueError):
        return {"success": False, "reason": "youtube-metadata-invalid"}

    video_id = re.sub(r"[^A-Za-z0-9_-]+", "", str(metadata.get("id") or ""))
    video_id = video_id[:24] or hashlib.sha1(url.encode("utf-8")).hexdigest()[:12]
    download_root = Path(work_root) / "youtube-download"
    download_root.mkdir(parents=True, exist_ok=True)
    source_template = download_root / f"{video_id}.%(ext)s"
    download_result = subprocess.run(
        [
            yt_dlp,
            "--no-playlist",
            "--merge-output-format",
            "mp4",
            "--write-thumbnail",
            "--convert-thumbnails",
            "jpg",
            "-f",
            "bv*[height<=480]+ba/b[height<=480]/best",
            "-o",
            str(source_template),
            url,
        ],
        text=True,
        capture_output=True,
        timeout=1800,
        check=False,
    )
    if download_result.returncode != 0:
        reason = (download_result.stderr or download_result.stdout).strip()
        return {
            "success": False,
            "reason": f"youtube-download-failed:{reason[-240:]}",
        }

    source_videos = [
        path for path in download_root.glob(f"{video_id}.*")
        if path.suffix.lower() in {".mp4", ".mkv", ".webm", ".mov", ".m4v"}
    ]
    source_posters = [
        path for path in download_root.glob(f"{video_id}.*")
        if path.suffix.lower() in {".jpg", ".jpeg", ".png", ".webp"}
    ]
    if not source_videos:
        return {"success": False, "reason": "youtube-video-missing"}
    if not source_posters:
        return {"success": False, "reason": "youtube-thumbnail-missing"}

    media_root = Path(archive_root) / "www.youtube.com" / "_rockpod_media"
    media_root.mkdir(parents=True, exist_ok=True)
    video_path = media_root / f"video-{video_id}.mpg"
    poster_path = media_root / f"poster-{video_id}.jpg"
    scale = (
        "scale=320:240:force_original_aspect_ratio=decrease,"
        "pad=320:240:(ow-iw)/2:(oh-ih)/2:black,fps=20"
    )
    transcode_result = subprocess.run(
        [
            ffmpeg,
            "-hide_banner",
            "-loglevel",
            "error",
            "-y",
            "-i",
            str(source_videos[-1]),
            "-vf",
            scale,
            "-map",
            "0:v:0",
            "-map",
            "0:a:0?",
            "-c:v",
            "mpeg2video",
            "-pix_fmt",
            "yuv420p",
            "-bf",
            "0",
            "-g",
            "12",
            "-flags",
            "+low_delay",
            "-q:v",
            "8",
            "-maxrate",
            "900k",
            "-bufsize",
            "512k",
            "-c:a",
            "mp2",
            "-ar",
            "44100",
            "-ac",
            "2",
            "-b:a",
            "96k",
            str(video_path),
        ],
        text=True,
        capture_output=True,
        timeout=1800,
        check=False,
    )
    if transcode_result.returncode != 0 or not video_path.is_file():
        reason = (transcode_result.stderr or transcode_result.stdout).strip()
        return {
            "success": False,
            "reason": f"youtube-transcode-failed:{reason[-240:]}",
        }

    shutil.copy2(source_posters[-1], poster_path)
    _convert_image_sidecar(poster_path)
    frame_path = media_root / f"inline-{video_id}.bin"
    frame_result = subprocess.run(
        [
            ffmpeg,
            "-hide_banner",
            "-loglevel",
            "error",
            "-y",
            "-i",
            str(video_path),
            "-vf",
            (
                "fps=24,scale=210:158:force_original_aspect_ratio=decrease,"
                "pad=210:158:(ow-iw)/2:(oh-ih)/2:black"
            ),
            "-pix_fmt",
            "rgb565le",
            "-f",
            "rawvideo",
            str(frame_path),
        ],
        text=True,
        capture_output=True,
        timeout=180,
        check=False,
    )
    frame_size = 210 * 158 * 2
    frame_count = (
        frame_path.stat().st_size // frame_size
        if frame_path.is_file() else 0
    )
    if frame_result.returncode != 0 or frame_count <= 0:
        reason = (frame_result.stderr or frame_result.stdout).strip()
        return {
            "success": False,
            "reason": f"youtube-frames-failed:{reason[-240:]}",
        }
    video_ref = (
        "/.rockbox/offlineweb/archive/www.youtube.com/_rockpod_media/"
        + video_path.name
    )
    poster_ref = (
        "/.rockbox/offlineweb/archive/www.youtube.com/_rockpod_media/"
        + poster_path.name
    )
    frame_ref = (
        "/.rockbox/offlineweb/archive/www.youtube.com/_rockpod_media/"
        + frame_path.name
    )
    metadata_path = media_root / f"video-{video_id}.ytm"
    uploader = str(
        metadata.get("uploader") or metadata.get("channel") or "YouTube Member"
    ).strip()
    upload_date = str(
        metadata.get("upload_date") or metadata.get("release_date") or ""
    )
    if len(upload_date) == 8 and upload_date.isdigit():
        upload_date = (
            f"{upload_date[4:6]}/{upload_date[6:8]}/{upload_date[:4]}"
        )
    try:
        views = f"{int(metadata.get('view_count')):,} views"
    except (TypeError, ValueError):
        views = ""
    metadata_path.write_text(
        "\n".join(
            (
                "title=" + str(metadata.get("title") or "YouTube Video")[:63],
                "uploader=" + uploader[:39],
                "added=" + upload_date[:23],
                "views=" + views[:31],
                "duration=Time " + _youtube_duration(metadata.get("duration")),
            )
        ) + "\n",
        encoding="utf-8",
    )
    html_text = _youtube_watch_html(
        metadata, video_ref, frame_ref, frame_count
    )
    entry_path = _write_url_record(
        archive_root,
        url,
        html_text.encode("utf-8"),
        content_type="text/html",
    )
    if not entry_path:
        return {"success": False, "reason": "youtube-page-write-failed"}
    avatar_path = _youtube_channel_avatar(
        metadata, work_root, media_root, ffmpeg
    )
    _write_youtube_subscription_pages(
        archive_root,
        metadata,
        entry_path,
        poster_path,
        avatar_path,
    )
    _track_cache_file(video_path)
    _track_cache_file(poster_path)
    _track_cache_file(Path(str(poster_path) + ".bmp"))
    if avatar_path:
        _track_cache_file(avatar_path)
        _track_cache_file(Path(str(avatar_path) + ".bmp"))
    _track_cache_file(frame_path)
    _track_cache_file(metadata_path)
    return {
        "success": True,
        "source": "youtube-url",
        "html_saved": 1,
        "assets_saved": 7 if avatar_path else 5,
        "title": str(metadata.get("title") or "YouTube Video"),
        "entry_path": str(entry_path),
        "preview_path": str(poster_path),
    }


def _sync_single_url(url, work_root, archive_root, log_handle,
                     firefox_profile=Path(), use_firefox_cookies=True,
                     oldest_date=""):
    normalized = _normalize_url(url)
    if not normalized:
        return {"url": str(url or ""), "success": False, "reason": "invalid-url"}

    parsed = urlsplit(normalized)
    _purge_url_records(archive_root, normalized)
    if _is_youtube_url(normalized):
        return {
            "url": normalized,
            **_sync_youtube_url(normalized, work_root, archive_root),
        }
    hosts = {parsed.netloc.lower()}
    snapshot_url = ""
    archive_id = ""
    warc_used = ""
    html_saved = 0
    assets_saved = 0

    if shutil.which("firefox"):
        try:
            browser_result = _sync_browser_export(
                normalized,
                work_root,
                archive_root,
                firefox_profile,
                use_firefox_cookies=use_firefox_cookies,
                oldest_date=oldest_date,
            )
        except Exception as exc:
            browser_result = {"success": False, "reason": f"exception:{exc}"}
            log_handle.write(
                f"browser export failed for {normalized}: {exc}\n"
            )
            traceback.print_exc(file=log_handle)
        if browser_result.get("success"):
            return {
                "url": normalized,
                "success": True,
                "source": browser_result.get("source") or "firefox-browser-export",
                "firefox_profile": str(firefox_profile),
                "html_saved": int(browser_result.get("html_saved") or 0),
                "assets_saved": int(browser_result.get("assets_saved") or 0),
                "cache_images_saved": int(browser_result.get("cache_images_saved") or 0),
                "title": browser_result.get("title") or "",
                "entry_path": browser_result.get("entry_path") or "",
                "preview_path": browser_result.get("preview_path") or "",
                "oldest_date": oldest_date,
            }
        log_handle.write(
            "browser export skipped for "
            f"{normalized}: {browser_result.get('reason', 'unknown')}\n"
        )

    try:
        live_result = _sync_live_url(
            normalized,
            archive_root,
            firefox_profile,
            use_firefox_cookies=use_firefox_cookies,
        )
    except Exception as exc:
        live_result = {
            "success": False,
            "reason": f"exception:{exc}",
            "cookie_count": 0,
        }
        log_handle.write(f"live fetch failed for {normalized}: {exc}\n")
        traceback.print_exc(file=log_handle)
    if live_result.get("success"):
        return {
            "url": normalized,
            "success": True,
            "source": live_result.get("source") or "live-public",
            "firefox_profile": str(firefox_profile)
            if live_result.get("cookie_count") and firefox_profile else "",
            "cookie_count": int(live_result.get("cookie_count") or 0),
            "html_saved": int(live_result.get("html_saved") or 0),
            "assets_saved": int(live_result.get("assets_saved") or 0),
            "entry_path": live_result.get("entry_path") or "",
            "title": live_result.get("title") or "",
        }
    log_handle.write(
        f"live fetch skipped for {normalized}: {live_result.get('reason', 'unknown')}\n"
    )

    if use_firefox_cookies and firefox_profile and not live_result.get("cookie_count"):
        try:
            live_result = _sync_live_url(
                normalized,
                archive_root,
                firefox_profile,
                use_firefox_cookies=False,
            )
        except Exception as exc:
            live_result = {
                "success": False,
                "reason": f"exception:{exc}",
                "cookie_count": 0,
            }
            log_handle.write(
                f"public live fetch failed for {normalized}: {exc}\n"
            )
            traceback.print_exc(file=log_handle)
        if live_result.get("success"):
            return {
                "url": normalized,
                "success": True,
                "source": live_result.get("source") or "live-public",
                "firefox_profile": "",
                "cookie_count": 0,
                "html_saved": int(live_result.get("html_saved") or 0),
                "assets_saved": int(live_result.get("assets_saved") or 0),
                "entry_path": live_result.get("entry_path") or "",
                "title": live_result.get("title") or "",
            }
        log_handle.write(
            f"public live fetch skipped for {normalized}: {live_result.get('reason', 'unknown')}\n"
        )

    try:
        snapshot_url = _resolve_snapshot_url(normalized)
        if snapshot_url:
            snap_host = urlsplit(snapshot_url).netloc.lower()
            if snap_host:
                hosts.add(snap_host)
    except Exception as exc:
        log_handle.write(f"snapshot lookup failed for {normalized}: {exc}\n")

    zip_path = work_root / f"{_safe_name(parsed.netloc)}.zip"
    if snapshot_url:
        try:
            archive_id = _try_snapshot_zip(snapshot_url, str(zip_path))
        except Exception as exc:
            log_handle.write(f"snapshot zip fetch failed for {normalized}: {exc}\n")

    if archive_id and zip_path.exists() and zipfile.is_zipfile(zip_path):
        with zipfile.ZipFile(zip_path, "r") as bundle:
            bundle.extractall(archive_root)
        assets_saved += _convert_archive_images(archive_root)
        for path in archive_root.rglob("*"):
            if not path.is_file():
                continue
            ext = path.suffix.lower()
            if ext in HTML_EXTS:
                html_saved += 1
            elif ext in ASSET_EXTS:
                assets_saved += 1
        return {
            "url": normalized,
            "success": html_saved > 0,
            "source": "wayback-zip",
            "archive_id": archive_id,
            "snapshot_url": snapshot_url,
            "html_saved": html_saved,
            "assets_saved": assets_saved,
        }

    host_token = _safe_name(parsed.netloc).replace(".", "_")
    if host_token:
        search_url = (
            "https://archive.org/advancedsearch.php?"
            f"q=identifier:(warc-*{host_token}*)&"
            "fl[]=identifier&rows=1&page=1&output=json"
        )
        try:
            docs = ((_fetch_json(search_url) or {}).get("response") or {}).get("docs") or []
            if docs:
                archive_id = str(docs[0].get("identifier") or "").strip()
        except Exception as exc:
            log_handle.write(f"warc lookup failed for {normalized}: {exc}\n")

    if not archive_id:
        return {
            "url": normalized,
            "success": False,
            "source": "none",
            "reason": "no-archive-id",
        }

    try:
        warc_files = _list_warc_files(archive_id)
    except Exception as exc:
        return {
            "url": normalized,
            "success": False,
            "source": "warc",
            "archive_id": archive_id,
            "reason": f"metadata-error:{exc}",
        }

    for name in warc_files[:2]:
        warc_path = work_root / name
        warc_url = f"https://archive.org/download/{archive_id}/{name}"
        try:
            _download_file(warc_url, str(warc_path))
            extracted = _extract_warc_to_folder(str(warc_path), archive_root, hosts)
            if extracted["saved"]:
                warc_used = name
                html_saved += extracted["html_saved"]
                assets_saved += extracted["saved"] - extracted["html_saved"]
                break
        except Exception as exc:
            log_handle.write(f"warc fetch/extract failed {warc_url}: {exc}\n")

    return {
        "url": normalized,
        "success": html_saved > 0,
        "source": "warc",
        "archive_id": archive_id,
        "snapshot_url": snapshot_url,
        "warc_file": warc_used,
        "html_saved": html_saved,
        "assets_saved": assets_saved,
    }


def _merge_pages_catalog(pages_path, managed_entries, device_root):
    header = (
        "# title\turl\tpath\tsource\tneighborhood\t"
        "author\tarchived\tkeywords\n"
    )
    rows = {}
    order = []

    try:
        existing_lines = Path(pages_path).read_text(
            encoding="utf-8", errors="ignore"
        ).splitlines()
    except OSError:
        existing_lines = []

    for line in existing_lines:
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) < 3 or not fields[2]:
            continue
        fields.extend([""] * (8 - len(fields)))
        path = fields[2]
        if path not in rows:
            order.append(path)
        rows[path] = fields[:8]

    if managed_entries:
        rows = {}
        order = []

    managed_paths = {
        str(item.get("path") or "")
        for item in managed_entries
        if item.get("path")
    }
    managed_hosts = {
        urlsplit(str(item.get("url") or "")).netloc.lower().removeprefix("www.")
        for item in managed_entries
        if item.get("url")
    }
    for path, fields in list(rows.items()):
        row_host = urlsplit(str(fields[1] or "")).netloc.lower().removeprefix(
            "www."
        )
        if row_host in managed_hosts and path not in managed_paths:
            rows.pop(path, None)

    youtube_entries = [
        item for item in managed_entries
        if _is_youtube_url(str(item.get("url") or ""))
    ]
    social_entries = {
        site: [
            item for item in managed_entries
            if _social_site(str(item.get("url") or ""))[0] == site
        ]
        for site in ("instagram", "onlyfans")
    }
    for item in managed_entries:
        if (
            _is_youtube_url(str(item.get("url") or ""))
            or _social_site(str(item.get("url") or ""))[0]
        ):
            continue
        path = str(item.get("path") or "")
        if not path:
            continue
        absolute = Path(device_root) / path.lstrip("/")
        if not absolute.is_file():
            continue
        fields = [
            str(item.get("title") or urlsplit(item.get("url") or "").netloc),
            str(item.get("url") or ""),
            path,
            "Website Sync",
            "",
            "",
            str(item.get("synced_at") or "")[:10],
            "",
        ]
        if path not in rows:
            order.append(path)
        rows[path] = fields

    if youtube_entries:
        path = (
            "/.rockbox/offlineweb/archive/www.youtube.com/"
            "subscriptions.html"
        )
        absolute = Path(device_root) / path.lstrip("/")
        if absolute.is_file():
            fields = [
                "YouTube",
                "https://www.youtube.com/feed/subscriptions",
                path,
                "Website Sync",
                "Subscriptions",
                "",
                max(
                    str(item.get("synced_at") or "")[:10]
                    for item in youtube_entries
                ),
                "youtube,video,channels",
            ]
            if path not in rows:
                order.append(path)
            rows[path] = fields

    for site, entries in social_entries.items():
        if not entries:
            continue
        first_url = str(entries[0].get("url") or "")
        host = urlsplit(first_url).netloc.lower()
        path = (
            f"/.rockbox/offlineweb/archive/{host}/profiles.html"
        )
        absolute = Path(device_root) / path.lstrip("/")
        if not absolute.is_file():
            path = str(entries[0].get("path") or "")
            absolute = Path(device_root) / path.lstrip("/")
            if not path or not absolute.is_file():
                continue
        title = "Instagram" if site == "instagram" else "OnlyFans"
        fields = [
            title,
            f"https://{host}/profiles",
            path,
            "Website Sync",
            "Profiles",
            "",
            max(
                str(item.get("synced_at") or "")[:10]
                for item in entries
            ),
            f"{site},profiles,photos",
        ]
        if path not in rows:
            order.append(path)
        rows[path] = fields

    Path(pages_path).parent.mkdir(parents=True, exist_ok=True)
    with Path(pages_path).open("w", encoding="utf-8", newline="\n") as out:
        out.write(header)
        for path in order:
            fields = rows.get(path)
            if not fields:
                continue
            out.write(
                "\t".join(
                    str(value).replace("\t", " ").replace("\n", " ")
                    for value in fields
                )
                + "\n"
            )


def sync_websites(urls, device_root, output_path, log_path,
                  use_firefox_cookies=True, firefox_profile_path="",
                  oldest_date=""):
    global _ACTIVE_CACHE_FILES

    device_root = os.path.abspath(os.path.expanduser(str(device_root or "").strip()))
    if not os.path.isdir(os.path.join(device_root, ".rockbox")):
        raise RuntimeError("Device root is missing .rockbox")

    output_file = Path(output_path)
    output_file.parent.mkdir(parents=True, exist_ok=True)
    log_file = Path(log_path)
    log_file.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="rockpod-offlineweb-") as temp_dir:
        temp_root = Path(temp_dir)
        source_root = temp_root / "source"
        archive_root = source_root / "archive"
        archive_root.mkdir(parents=True, exist_ok=True)
        _seed_existing_archive(device_root, archive_root)
        _seed_builtin_archive(archive_root)
        removed_bad = _remove_bad_html_captures(archive_root)
        firefox_profile = _discover_firefox_profile(firefox_profile_path) if use_firefox_cookies else Path()

        results = []
        with open(log_file, "w", encoding="utf-8") as log_handle:
            log_handle.write("RockPod Offline Web Sync log\n")
            log_handle.write(f"device_root={device_root}\n")
            if use_firefox_cookies:
                log_handle.write(f"firefox_profile={firefox_profile or '(not found)'}\n")
            if removed_bad:
                log_handle.write(f"removed_bad_html_captures={removed_bad}\n")
            for raw in urls:
                log_handle.write(f"sync_url={raw}\n")
                _ACTIVE_CACHE_FILES = set()
                try:
                    result = _sync_single_url(
                        raw,
                        temp_root,
                        archive_root,
                        log_handle,
                        firefox_profile=firefox_profile,
                        use_firefox_cookies=use_firefox_cookies,
                        oldest_date=oldest_date,
                    )
                    cached_files = []
                    for cached_path in sorted(_ACTIVE_CACHE_FILES):
                        try:
                            relative = Path(cached_path).relative_to(archive_root)
                        except ValueError:
                            continue
                        cached_files.append(
                            "/.rockbox/offlineweb/archive/" + relative.as_posix()
                        )
                    result["files"] = cached_files
                except Exception as exc:
                    normalized = _normalize_url(raw)
                    result = {
                        "url": normalized or str(raw or ""),
                        "success": False,
                        "source": "error",
                        "reason": f"{type(exc).__name__}: {exc}",
                        "files": [],
                    }
                    log_handle.write(
                        f"sync failed for {normalized or raw}: {exc}\n"
                    )
                    traceback.print_exc(file=log_handle)
                finally:
                    _ACTIVE_CACHE_FILES = None
                results.append(result)
                log_handle.write(json.dumps(result, ensure_ascii=False) + "\n")

        main_page = archive_root / "editthis.info" / "myspace_af" / "Main_Page"
        if main_page.is_file():
            main_page_html = main_page.with_suffix(".html")
            if not main_page_html.exists():
                shutil.copy2(main_page, main_page_html)

        sidecars_saved = _convert_archive_images(archive_root)

        import_archive(archive_root, Path(device_root))
        cache_root = Path(device_root) / ".rockbox" / "offlineweb" / "cache"
        registry_path = cache_root / "websites.json"
        try:
            registry = json.loads(registry_path.read_text(encoding="utf-8"))
        except (OSError, ValueError, TypeError):
            registry = []
        by_url = {
            str(item.get("url") or ""): item
            for item in registry
            if isinstance(item, dict) and item.get("url")
        }
        for item in results:
            if not item.get("success"):
                continue
            previous = by_url.get(item["url"], {})
            new_files = {
                str(path)
                for path in (item.get("files") or [])
                if str(path).startswith("/.rockbox/offlineweb/archive/")
            }
            other_files = {
                str(path)
                for other_url, other in by_url.items()
                if other_url != item["url"]
                for path in (other.get("files") or [])
            }
            stale_files = set(previous.get("files") or []) - new_files - other_files
            for stale_path in stale_files:
                relative = str(stale_path).lstrip("/")
                device_path = os.path.abspath(os.path.join(device_root, relative))
                if os.path.commonpath([device_root, device_path]) != device_root:
                    continue
                try:
                    os.remove(device_path)
                except OSError:
                    pass
            entry_path = str(item.get("entry_path") or "")
            preview_path = str(item.get("preview_path") or "")
            try:
                entry_path = "/" + Path(entry_path).relative_to(source_root).as_posix()
            except (ValueError, TypeError):
                entry_path = ""
            try:
                preview_path = "/" + Path(preview_path).relative_to(source_root).as_posix()
            except (ValueError, TypeError):
                preview_path = ""
            captured_title = str(item.get("title") or "").strip()
            item_url = urlsplit(item["url"])
            host = item_url.netloc.lower().removeprefix("www.")
            slug_title = re.sub(
                r"[-_]+", " ", Path(item_url.path.rstrip("/")).stem
            ).strip().title()
            if (
                previous.get("title")
                and captured_title.lower() in {
                    "",
                    host,
                    host.split(".", 1)[0],
                    slug_title.lower(),
                }
            ):
                captured_title = str(previous.get("title") or "")
            by_url[item["url"]] = {
                "url": item["url"],
                "title": captured_title or urlsplit(item["url"]).netloc,
                "path": f"/.rockbox/offlineweb{entry_path}" if entry_path else "",
                "preview_path": (
                    f"/.rockbox/offlineweb{preview_path}" if preview_path else ""
                ),
                "source": item.get("source") or "",
                "synced_at": time.strftime("%Y-%m-%d %H:%M:%S"),
                "oldest_date": str(item.get("oldest_date") or ""),
                "files": sorted(new_files),
            }
        cache_root.mkdir(parents=True, exist_ok=True)
        registry_path.write_text(
            json.dumps(list(by_url.values()), indent=2),
            encoding="utf-8",
        )
        _merge_pages_catalog(
            cache_root / "pages.tsv",
            list(by_url.values()),
            device_root,
        )
        runtime_assets_saved = _install_runtime_assets(device_root)
        removed_device_bad = _remove_bad_html_captures(
            Path(device_root) / ".rockbox" / "offlineweb" / "archive"
        )

        html_total = sum(int(item.get("html_saved") or 0) for item in results)
        asset_total = sum(int(item.get("assets_saved") or 0) for item in results)
        summary = {
            "requested": len(urls),
            "completed": sum(1 for item in results if item.get("success")),
            "failed": sum(1 for item in results if not item.get("success")),
            "html_saved": html_total,
            "assets_saved": asset_total + sidecars_saved,
            "image_sidecars_saved": sidecars_saved,
            "runtime_assets_saved": runtime_assets_saved,
            "removed_device_bad_html_captures": removed_device_bad,
            "offlineweb_root": os.path.join(device_root, ".rockbox", "offlineweb"),
            "firefox_profile": str(firefox_profile) if use_firefox_cookies and firefox_profile else "",
            "results": results,
            "log_path": str(log_file),
        }
        output_file.write_text(json.dumps(summary, indent=2), encoding="utf-8")
        return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", action="append", default=[])
    parser.add_argument("--device-root", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--log-path", required=True)
    parser.add_argument("--use-firefox-cookies", action="store_true")
    parser.add_argument("--firefox-profile", default="")
    parser.add_argument("--oldest-date", default="")
    args = parser.parse_args()

    summary = sync_websites(
        args.url,
        args.device_root,
        args.output,
        args.log_path,
        use_firefox_cookies=args.use_firefox_cookies,
        firefox_profile_path=args.firefox_profile,
        oldest_date=args.oldest_date,
    )
    print(
        "Synced {completed}/{requested} websites (html={html_saved}, assets={assets_saved})".format(
            **summary,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
