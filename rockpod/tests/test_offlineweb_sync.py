import base64
import io
import json
import os
import sqlite3
import time
from pathlib import Path

from scripts import offlineweb_sync as sync_script
from services.offlineweb_sync import (
    OfflineWebSyncImporter,
    load_synced_websites,
    parse_sync_urls,
    remove_synced_website,
)


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
    monkeypatch.setattr(
        sync_script,
        "_sync_browser_export",
        lambda *args, **kwargs: {"success": False, "reason": "test"},
    )

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
    monkeypatch.setattr(
        sync_script,
        "_sync_browser_export",
        lambda *args, **kwargs: {"success": False, "reason": "test"},
    )
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
    assert "https://example.com/" in pages
    assert json.loads(output.read_text())["completed"] == 1


def test_query_urls_are_stored_as_distinct_pages(tmp_dir):
    root = Path(tmp_dir) / "archive"
    root.mkdir()

    first = sync_script._write_url_record(
        root,
        "https://example.com/watch?v=one",
        b"<html><title>One</title></html>",
        "text/html",
    )
    second = sync_script._write_url_record(
        root,
        "https://example.com/watch?v=two",
        b"<html><title>Two</title></html>",
        "text/html",
    )

    assert first != second
    assert Path(first).read_bytes() != Path(second).read_bytes()
    assert "__q_" in Path(first).name


def test_browser_page_title_uses_path_when_site_title_is_generic():
    assert sync_script._browser_page_title(
        {"title": "OnlyFans"}, "https://onlyfans.com/emmaontwitch"
    ) == "Emmaontwitch"


def test_browser_visual_capture_omits_asset_gallery_and_extracted_text():
    rendered = sync_script._browser_static_html(
        {
            "title": "Example",
            "text": "Random extracted website text",
            "images": ["https://cdn.example.com/random.jpg"],
            "links": [{"url": "https://example.com/noise", "text": "Noise"}],
        },
        "https://example.com/profile",
        extra_images=["https://cdn.example.com/also-random.jpg"],
        logo_url="https://example.com/logo.png",
        preview_images=[
            "https://example.com/profile.rockpod-preview-01.png",
            "https://example.com/profile.rockpod-preview-02.png",
        ],
    )

    assert rendered.count("<img ") == 2
    assert "rockpod-render-mode" in rendered
    assert "Random extracted website text" not in rendered
    assert "random.jpg" not in rendered
    assert "also-random.jpg" not in rendered
    assert "logo.png" not in rendered
    assert "Noise" not in rendered


def test_browser_visual_capture_still_caches_underlying_resources(tmp_dir, monkeypatch):
    preview = base64.b64encode(b"\x89PNG\r\n\x1a\npreview").decode("ascii")
    monkeypatch.setattr(
        sync_script,
        "_browser_snapshot",
        lambda *args, **kwargs: {
            "url": "https://example.com/profile",
            "title": "Example Profile",
            "text": "Rendered text",
            "html": "<html><body>Rendered text</body></html>",
            "preview_pngs": [preview],
            "resources": [
                "https://example.com/site.css",
                "https://example.com/app.js",
                "https://example.com/api.php",
            ],
            "images": [],
            "links": [],
        },
    )

    def fake_resource(url, cookie_header="", referer=""):
        if url.endswith(".css"):
            content_type = "text/css"
        elif url.endswith(".php"):
            content_type = "application/json"
        else:
            content_type = "application/javascript"
        return {"url": url, "status": 200, "content_type": content_type, "body": b"cached"}

    monkeypatch.setattr(sync_script, "_fetch_live_resource", fake_resource)
    monkeypatch.setattr(sync_script, "_convert_image_sidecar", lambda path: False)
    archive = Path(tmp_dir) / "archive"
    archive.mkdir()

    result = sync_script._sync_browser_export(
        "https://example.com/profile",
        Path(tmp_dir),
        archive,
        Path(),
        use_firefox_cookies=False,
    )

    assert result["success"] is True
    assert result["assets_saved"] == 3
    assert (archive / "example.com" / "site.css").read_bytes() == b"cached"
    assert (archive / "example.com" / "app.js").read_bytes() == b"cached"
    assert (archive / "example.com" / "api.json").read_bytes() == b"cached"
    page = (archive / "example.com" / "profile.html").read_text()
    assert "rockpod-render-mode" in page
    assert "site.css" not in page
    assert "app.js" not in page


def test_repeated_sync_keeps_existing_websites_and_managed_shortcuts(tmp_dir, monkeypatch):
    device = Path(tmp_dir) / "ipod"
    (device / ".rockbox").mkdir(parents=True)

    def fake_fetch(url, cookie_header=""):
        title = "First" if "first" in url else "Second"
        return {
            "url": url,
            "status": 200,
            "content_type": "text/html",
            "body": f"<html><head><title>{title}</title></head><body>{title}</body></html>".encode(),
        }

    monkeypatch.setattr(sync_script, "_fetch_live_url", fake_fetch)
    monkeypatch.setattr(
        sync_script,
        "_sync_browser_export",
        lambda *args, **kwargs: {"success": False, "reason": "test"},
    )
    for name in ("first", "second"):
        sync_script.sync_websites(
            [f"https://example.com/{name}"],
            str(device),
            str(Path(tmp_dir) / f"{name}.json"),
            str(Path(tmp_dir) / f"{name}.log"),
            use_firefox_cookies=False,
        )

    registry = json.loads(
        (device / ".rockbox" / "offlineweb" / "cache" / "websites.json").read_text()
    )
    assert [item["url"] for item in registry] == [
        "https://example.com/first",
        "https://example.com/second",
    ]
    pages = (device / ".rockbox" / "offlineweb" / "cache" / "pages.tsv").read_text()
    assert "https://example.com/first" in pages
    assert "https://example.com/second" in pages
    assert (device / ".rockbox" / "offlineweb" / "archive" / "example.com" / "first.html").is_file()
    assert (device / ".rockbox" / "offlineweb" / "archive" / "example.com" / "second.html").is_file()

    assert remove_synced_website(str(device), "https://example.com/first") is True
    assert [item["url"] for item in load_synced_websites(str(device))] == [
        "https://example.com/second"
    ]
    pages = (device / ".rockbox" / "offlineweb" / "cache" / "pages.tsv").read_text()
    assert "https://example.com/first" not in pages
    assert "https://example.com/second" in pages
    assert not (device / ".rockbox" / "offlineweb" / "archive" / "example.com" / "first.html").exists()


def test_legacy_catalog_migrates_orphaned_sibling_websites(tmp_dir):
    device = Path(tmp_dir) / "ipod"
    cache = device / ".rockbox" / "offlineweb" / "cache"
    archive = device / ".rockbox" / "offlineweb" / "archive" / "example.com"
    cache.mkdir(parents=True)
    archive.mkdir(parents=True)
    (archive / "first.html").write_text("<title>First Site</title>")
    (archive / "second.html").write_text("<title>Second Site</title>")
    (cache / "pages.tsv").write_text(
        "# title\turl\tpath\tsource\tneighborhood\tauthor\tarchived\tkeywords\n"
        "First Site\thttps://example.com/first.html\t"
        "/.rockbox/offlineweb/archive/example.com/first.html\tArchive\t\t\t\t\n"
    )

    entries = load_synced_websites(str(device))

    assert {item["title"] for item in entries} == {"First Site", "Second Site"}
    assert (cache / "websites.json").is_file()
    migrated_pages = (cache / "pages.tsv").read_text()
    assert "First Site" in migrated_pages
    assert "Second Site" in migrated_pages


def test_remove_website_deletes_only_its_owned_cache_files(tmp_dir):
    device = Path(tmp_dir) / "ipod"
    cache = device / ".rockbox" / "offlineweb" / "cache"
    archive = device / ".rockbox" / "offlineweb" / "archive" / "example.com"
    cache.mkdir(parents=True)
    archive.mkdir(parents=True)
    shared = archive / "shared.css"
    first = archive / "first.html"
    second = archive / "second.html"
    for path in (shared, first, second):
        path.write_text(path.name)
    prefix = "/.rockbox/offlineweb/archive/example.com/"
    entries = [
        {
            "title": "First",
            "url": "https://example.com/first",
            "path": prefix + "first.html",
            "files": [prefix + "first.html", prefix + "shared.css"],
        },
        {
            "title": "Second",
            "url": "https://example.com/second",
            "path": prefix + "second.html",
            "files": [prefix + "second.html", prefix + "shared.css"],
        },
    ]
    (cache / "websites.json").write_text(json.dumps(entries))

    assert remove_synced_website(str(device), "https://example.com/first") is True
    assert not first.exists()
    assert second.exists()
    assert shared.exists()
