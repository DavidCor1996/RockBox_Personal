import io
import json
import os
import sqlite3
import time
from pathlib import Path

from scripts import offlineweb_sync as sync_script
from services.offlineweb_sync import OfflineWebSyncImporter, parse_sync_urls


class _Config:
    def __init__(self, cache_dir):
        self.cache_dir = cache_dir


def _create_firefox_profile(root, cookies):
    profile = Path(root) / "firefox-profile"
    profile.mkdir(parents=True, exist_ok=True)
    db_path = profile / "cookies.sqlite"
    connection = sqlite3.connect(db_path)
    try:
        connection.execute(
            "CREATE TABLE moz_cookies ("
            "name TEXT, value TEXT, host TEXT, path TEXT, isSecure INTEGER, expiry INTEGER)"
        )
        connection.executemany(
            "INSERT INTO moz_cookies VALUES (?, ?, ?, ?, ?, ?)",
            cookies,
        )
        connection.commit()
    finally:
        connection.close()
    return profile


def test_parse_sync_urls_normalizes_and_deduplicates():
    assert parse_sync_urls("example.com, https://example.com/path#frag; ftp://bad") == [
        "https://example.com",
        "https://example.com/path",
    ]


def test_prepare_sync_enables_firefox_cookies(tmp_dir):
    device = os.path.join(tmp_dir, "ipod")
    os.makedirs(os.path.join(device, ".rockbox"), exist_ok=True)
    importer = OfflineWebSyncImporter(_Config(os.path.join(tmp_dir, "cache")))

    request = importer.prepare_sync("example.com", device)

    assert "--use-firefox-cookies" in request.command
    assert request.command[-2:] == ["--url", "https://example.com"]
    assert request.device_root == os.path.abspath(device)


def test_cookie_header_for_host_reads_firefox_session_cookies(tmp_dir):
    now = int(time.time())
    profile = _create_firefox_profile(
        tmp_dir,
        [
            ("sid", "abc", ".example.com", "/", 1, now + 3600),
            ("theme", "dark", "example.com", "/", 0, now + 3600),
            ("old", "gone", ".example.com", "/", 0, now - 10),
            ("other", "nope", ".other.test", "/", 0, now + 3600),
        ],
    )

    header = sync_script._cookie_header_for_host(profile, "www.example.com")

    assert "sid=abc" in header
    assert "theme=dark" in header
    assert "old=gone" not in header
    assert "other=nope" not in header


def test_firefox_cache_image_records_extract_onlyfans_payload(tmp_dir):
    cache_root = Path(tmp_dir) / "cache2" / "entries"
    cache_root.mkdir(parents=True)
    payload = b"\xff\xd8\xff\xe0" + b"\x00" * 13000
    url = (
        "https://cdn2.onlyfans.com/files/a/ab/abcdef/"
        "1242x2208_photo.jpg?Tag=2&u=401120053"
    )
    metadata = (
        b"\x00request-method\x00GET\x00response-head\x00HTTP/2 200\r\n"
        b"content-type: image/jpeg\r\n"
        + f"content-length: {len(payload)}\r\n".encode("ascii")
        + b"\x00"
        + url.encode("ascii")
        + b"\x00"
    )
    (cache_root / "ENTRY").write_bytes(payload + metadata)

    records = sync_script._firefox_cache_image_records(Path(tmp_dir), cache_root)

    assert len(records) == 1
    assert records[0]["url"] == url
    assert records[0]["content_type"] == "image/jpeg"
    assert records[0]["body"] == payload


def test_firefox_cache_image_records_uses_request_origin_for_cdn(tmp_dir):
    cache_root = Path(tmp_dir) / "cache2" / "entries"
    cache_root.mkdir(parents=True)
    payload = b"\xff\xd8\xff\xe0" + b"\x00" * 13000
    url = "https://media.cdn.test/images/private-photo.jpg"
    metadata = (
        b"\x00request-Origin\x00https://members.example.test\x00"
        b"response-head\x00HTTP/2 200\r\n"
        b"content-type: image/jpeg\r\n"
        + f"content-length: {len(payload)}\r\n".encode("ascii")
        + b"\x00"
        + url.encode("ascii")
        + b"\x00"
    )
    (cache_root / "CDNENTRY").write_bytes(payload + metadata)

    records = sync_script._firefox_cache_image_records(
        Path(tmp_dir),
        cache_root,
        page_url="https://members.example.test/profile",
    )

    assert len(records) == 1
    assert records[0]["url"] == url
    assert records[0]["body"] == payload


def test_sync_single_url_uses_firefox_cookies_for_live_capture(tmp_dir, monkeypatch):
    profile = _create_firefox_profile(
        tmp_dir,
        [("session", "secret", ".example.com", "/", 1, int(time.time()) + 3600)],
    )
    archive_root = Path(tmp_dir) / "archive"
    archive_root.mkdir()
    seen_headers = []

    def fake_fetch(url, cookie_header=""):
        seen_headers.append((url, cookie_header))
        if url.endswith("style.css"):
            return {
                "url": url,
                "status": 200,
                "content_type": "text/css",
                "body": b"body { color: black; }",
            }
        return {
            "url": url,
            "status": 200,
            "content_type": "text/html",
            "body": b'<html><head><link href="/style.css"></head><body>Private</body></html>',
        }

    monkeypatch.setattr(sync_script, "_fetch_live_url", fake_fetch)

    result = sync_script._sync_single_url(
        "https://www.example.com/private",
        Path(tmp_dir),
        archive_root,
        io.StringIO(),
        firefox_profile=profile,
        use_firefox_cookies=True,
    )

    assert result["success"] is True
    assert result["source"] == "live-firefox-session"
    assert result["cookie_count"] == 1
    assert seen_headers[0] == ("https://www.example.com/private", "session=secret")
    assert seen_headers[1] == ("https://www.example.com/style.css", "session=secret")
    assert (archive_root / "www.example.com" / "private.html").is_file()
    assert (archive_root / "www.example.com" / "style.css").is_file()


def test_sync_websites_writes_live_site_to_offlineweb_archive(tmp_dir, monkeypatch):
    device = Path(tmp_dir) / "ipod"
    (device / ".rockbox").mkdir(parents=True)

    def fake_fetch(url, cookie_header=""):
        return {
            "url": "https://example.com/",
            "status": 200,
            "content_type": "text/html",
            "body": b"<html><head><title>Example Home</title></head><body>Hello</body></html>",
        }

    monkeypatch.setattr(sync_script, "_fetch_live_url", fake_fetch)
    output = Path(tmp_dir) / "result.json"
    log = Path(tmp_dir) / "sync.log"

    summary = sync_script.sync_websites(
        ["https://example.com/"],
        str(device),
        str(output),
        str(log),
        use_firefox_cookies=False,
    )

    assert summary["completed"] == 1
    assert (device / ".rockbox" / "offlineweb" / "archive" / "example.com" / "index.html").is_file()
    assert (device / ".rockbox" / "offlineweb" / "assets" / "cursor.bmp").is_file()
    pages = (device / ".rockbox" / "offlineweb" / "cache" / "pages.tsv").read_text()
    assert "Example Home" in pages
    assert "http://example.com/index.html" in pages
    assert json.loads(output.read_text())["completed"] == 1
