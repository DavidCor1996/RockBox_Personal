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


def test_prepare_social_sync_passes_oldest_post_date(tmp_dir):
    device = os.path.join(tmp_dir, "ipod")
    os.makedirs(os.path.join(device, ".rockbox"), exist_ok=True)
    importer = OfflineWebSyncImporter(_Config(os.path.join(tmp_dir, "cache")))

    request = importer.prepare_sync(
        "https://www.instagram.com/example/",
        device,
        oldest_date="2024-06-15",
    )

    assert request.command[
        request.command.index("--oldest-date") + 1
    ] == "2024-06-15"


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


def test_live_capture_skips_oversized_instagram_cdn_asset(tmp_dir, monkeypatch):
    archive_root = Path(tmp_dir) / "archive"
    archive_root.mkdir()
    long_asset = (
        "https://static.cdninstagram.com/rsrc.php/v4/en_US/"
        + ("a" * 280)
        + ".js"
    )

    def fake_fetch(url, cookie_header="", referer=""):
        if url == "https://www.instagram.com/":
            return {
                "url": url,
                "status": 200,
                "content_type": "text/html",
                "body": (
                    f"<html><head><title>Instagram</title>"
                    f'<script src="{long_asset}"></script></head>'
                    "<body>Saved Instagram feed</body></html>"
                ).encode(),
            }
        assert url == long_asset
        return {
            "url": url,
            "status": 200,
            "content_type": "application/javascript",
            "body": b"window.__instagram = true;",
        }

    monkeypatch.setattr(sync_script, "_fetch_live_resource", fake_fetch)

    result = sync_script._sync_live_url(
        "https://www.instagram.com/",
        archive_root,
        use_firefox_cookies=False,
    )

    assert result["success"] is True
    assert result["title"] == "Instagram"
    assert result["assets_saved"] == 0
    assert (archive_root / "www.instagram.com" / "index.html").is_file()


def test_catalog_lists_only_managed_sites_when_registry_exists(tmp_dir):
    device = Path(tmp_dir) / "ipod"
    cache = device / ".rockbox" / "offlineweb" / "cache"
    archive = device / ".rockbox" / "offlineweb" / "archive"
    cache.mkdir(parents=True)
    instagram_main = archive / "www.instagram.com" / "index.html"
    instagram_main.parent.mkdir(parents=True)
    instagram_main.write_text("<html>Instagram</html>")
    pages = cache / "pages.tsv"
    pages.write_text(
        "# title\turl\tpath\tsource\tneighborhood\tauthor\tarchived\tkeywords\n"
        "Instagram aux\thttp://www.instagram.com/ajax/query.html\t"
        "/.rockbox/offlineweb/archive/www.instagram.com/ajax/query.html\tArchive\t\t\t\t\n"
        "Example\thttps://example.com/\t"
        "/.rockbox/offlineweb/archive/example.com/index.html\tArchive\t\t\t\t\n"
    )

    sync_script._merge_pages_catalog(
        pages,
        [
            {
                "title": "Instagram",
                "url": "https://www.instagram.com/",
                "path": "/.rockbox/offlineweb/archive/www.instagram.com/index.html",
                "synced_at": "2026-07-25 17:30:00",
            }
        ],
        device,
    )

    catalog = pages.read_text()
    assert "Instagram aux" not in catalog
    assert "\tWebsite Sync\t" in catalog
    assert "Example" not in catalog


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


def test_browser_stage_exception_falls_back_to_live_sync(tmp_dir, monkeypatch):
    device = Path(tmp_dir) / "ipod"
    (device / ".rockbox").mkdir(parents=True)

    def broken_browser(*args, **kwargs):
        raise RuntimeError("browser capture failed")

    def fake_fetch(url, cookie_header=""):
        return {
            "url": "https://example.com/",
            "status": 200,
            "content_type": "text/html",
            "body": b"<html><title>Fallback Page</title><body>Saved</body></html>",
        }

    monkeypatch.setattr(sync_script, "_sync_browser_export", broken_browser)
    monkeypatch.setattr(sync_script, "_fetch_live_url", fake_fetch)
    output = Path(tmp_dir) / "fallback.json"
    log = Path(tmp_dir) / "fallback.log"

    summary = sync_script.sync_websites(
        ["https://example.com/"],
        str(device),
        str(output),
        str(log),
        use_firefox_cookies=False,
    )

    assert summary["completed"] == 1
    assert "browser capture failed" in log.read_text()
    assert "Fallback Page" in (
        device / ".rockbox" / "offlineweb" / "cache" / "pages.tsv"
    ).read_text()


def test_failed_sync_preserves_existing_archive_catalog(tmp_dir, monkeypatch):
    device = Path(tmp_dir) / "ipod"
    archive = (
        device / ".rockbox" / "offlineweb" / "archive" /
        "legacy.example" / "index.html"
    )
    archive.parent.mkdir(parents=True)
    archive.write_text(
        "<html><title>Legacy Saved Site</title><body>Still here</body></html>"
    )

    monkeypatch.setattr(
        sync_script,
        "_sync_browser_export",
        lambda *args, **kwargs: (_ for _ in ()).throw(
            RuntimeError("browser unavailable")
        ),
    )
    monkeypatch.setattr(
        sync_script,
        "_sync_live_url",
        lambda *args, **kwargs: (_ for _ in ()).throw(
            RuntimeError("live unavailable")
        ),
    )
    monkeypatch.setattr(sync_script, "_resolve_snapshot_url", lambda url: "")
    monkeypatch.setattr(sync_script, "_fetch_json", lambda url: {})
    output = Path(tmp_dir) / "failed.json"
    log = Path(tmp_dir) / "failed.log"

    summary = sync_script.sync_websites(
        ["https://unavailable.example/"],
        str(device),
        str(output),
        str(log),
        use_firefox_cookies=False,
    )

    pages = (
        device / ".rockbox" / "offlineweb" / "cache" / "pages.tsv"
    ).read_text()
    assert summary["completed"] == 0
    assert summary["failed"] == 1
    assert "Legacy Saved Site" in pages
    assert archive.is_file()


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


def test_browser_visual_capture_builds_native_readable_snapshot():
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

    assert rendered.count("<img ") == 3
    assert "rockpod-render-mode" in rendered
    assert "native-snapshot" in rendered
    assert "Random extracted website text" in rendered
    assert "profile.rockpod-preview-01.png" not in rendered
    assert "profile.rockpod-preview-02.png" not in rendered
    assert "random.jpg" in rendered
    assert "also-random.jpg" in rendered
    assert "logo.png" in rendered
    assert "Noise" not in rendered


def test_instagram_capture_is_media_feed_without_username_dump():
    rendered = sync_script._browser_static_html(
        {
            "title": "Instagram",
            "text": "Search\nfirst_username\nsecond_username",
            "images": [],
            "links": [],
        },
        "https://www.instagram.com/",
        extra_images=[
            "/.rockbox/offlineweb/archive/cdn.instagram.test/post-one.jpg",
            "/.rockbox/offlineweb/archive/cdn.instagram.test/post-two.jpg",
        ],
        preview_images=[
            "https://www.instagram.com/index.rockpod-preview.png",
        ],
    )

    assert "native-feed" in rendered
    assert rendered.count("<img ") == 2
    assert "first_username" not in rendered
    assert "second_username" not in rendered
    assert "rockpod-preview" not in rendered


def test_social_media_refs_are_compacted_for_device_safe_rendering(tmp_dir):
    tmp_dir = Path(tmp_dir)
    source = tmp_dir / "cdn.example" / "very" / "long" / "post.jpg"
    source.parent.mkdir(parents=True)
    source.write_bytes(b"jpeg")
    Path(str(source) + ".bmp").write_bytes(b"bitmap")

    refs = sync_script._compact_social_image_refs(
        tmp_dir,
        ["/.rockbox/offlineweb/archive/cdn.example/very/long/post.jpg"],
        "https://www.instagram.com/",
    )

    assert refs == [
        "/.rockbox/offlineweb/archive/www.instagram.com/"
        "_rockpod_media/home/01.jpg"
    ]
    compact = (
        tmp_dir / "www.instagram.com" / "_rockpod_media" / "home" / "01.jpg"
    )
    assert compact.read_bytes() == b"jpeg"
    assert Path(str(compact) + ".bmp").read_bytes() == b"bitmap"


def test_onlyfans_capture_is_native_profile_without_account_menu_dump():
    rendered = sync_script._browser_static_html(
        {
            "title": "Maddy TS OnlyFans",
            "text": (
                "Da\nDavid\nMy profile\nCollections\nSettings\n"
                "Maddy TS\n@babymaddyxo\nCreator bio\n128\nMessages"
            ),
            "images": [],
            "links": [],
        },
        "https://onlyfans.com/babymaddyxo",
        extra_images=[
            "/.rockbox/offlineweb/archive/cdn.onlyfans.test/profile.jpg",
            "/.rockbox/offlineweb/archive/cdn.onlyfans.test/post.jpg",
        ],
        preview_images=[
            "https://onlyfans.com/babymaddyxo.rockpod-preview.png",
        ],
    )

    assert "native-profile" in rendered
    assert "<h1>Maddy TS</h1>" in rendered
    assert "@babymaddyxo" in rendered
    assert "Creator bio" in rendered
    assert "My profile" not in rendered
    assert "Collections" not in rendered
    assert "Messages" not in rendered
    assert "rockpod-preview" not in rendered
    assert rendered.count("<img ") == 2


def test_youtube_urls_are_routed_as_native_watch_pages():
    assert sync_script._is_youtube_url(
        "https://www.youtube.com/watch?v=dQw4w9WgXcQ"
    )
    assert sync_script._is_youtube_url("https://youtu.be/dQw4w9WgXcQ")
    assert not sync_script._is_youtube_url("https://example.com/video")
    assert {".mpg", ".mpeg", ".m2v"} <= sync_script.ASSET_EXTS


def test_youtube_watch_page_uses_real_assets_and_linked_video_poster():
    rendered = sync_script._youtube_watch_html(
        {
            "title": "Example Upload",
            "uploader": "original_creator",
            "upload_date": "20070519",
            "duration": 95,
            "view_count": 1234,
            "description": "Original description",
        },
        "/.rockbox/offlineweb/archive/www.youtube.com/_rockpod_media/video-id.mpg",
        "/.rockbox/offlineweb/archive/www.youtube.com/_rockpod_media/inline-id.bin",
        144,
    )

    assert "youtube-2007" in rendered
    assert 'video="/.rockbox/offlineweb/archive/www.youtube.com/_rockpod_media/video-id.mpg"' in rendered
    assert 'frame-file="/.rockbox/offlineweb/archive/www.youtube.com/_rockpod_media/inline-id.bin"' in rendered
    assert 'frame-count="144"' in rendered
    assert "05/19/2007" in rendered
    assert "1,234 views" in rendered
    assert "click to play" not in rendered.lower()
    assert "select to play" not in rendered.lower()


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
