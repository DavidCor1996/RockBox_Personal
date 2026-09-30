import gzip
import json
import os
import sqlite3
import subprocess
import urllib.error
from pathlib import Path

from PIL import Image

import services.onlyfans_app as onlyfans_app_module
from services.onlyfans_app import (
    OnlyFansAppService,
    _clean,
    _display_shard,
    _metadata_enumeration_complete,
    _profile_slug,
)


ROOT = Path(__file__).resolve().parents[2]


class FakeConfig(dict):
    def get(self, key, default=None):
        return super().get(key, default)


def test_onlyfans_profile_url_validation():
    assert _profile_slug("https://onlyfans.com/kellycadigan") == "kellycadigan"
    try:
        _profile_slug("https://example.com/kellycadigan")
    except ValueError as exc:
        assert "OnlyFans profile URL" in str(exc)
    else:
        raise AssertionError("non-OnlyFans URL was accepted")


def test_onlyfans_sync_screen_is_mpeg_only():
    panel = (ROOT / "rockpod/ui/onlyfans_panel.py").read_text(
        encoding="utf-8"
    )
    assert 'QPushButton("Sync Selected Posts")' in panel
    assert 'QPushButton("Sync Creator")' in panel
    assert 'QPushButton("Sync All")' in panel
    assert 'QPushButton("Sync as H.264")' not in panel
    assert "sync_h264_button" not in panel


def test_ofscraper_completion_message_can_wrap_across_terminal_lines():
    assert _metadata_enumeration_complete([
        "[postcollection.get_media_for_metadata:147] Returning "
        "postcollection.py:147",
        "563 final media items for metadata.",
    ])
    assert not _metadata_enumeration_complete([
        "Returning 563 items",
        "metadata processing is still running",
    ])


def test_onlyfans_text_removes_html_and_decodes_entities():
    assert _clean("<p>Hello &amp; welcome<br />again</p>") == (
        "Hello & welcome again"
    )


def test_onlyfans_capture_never_uses_guessed_pointer_coordinates():
    source = (ROOT / "rockpod/services/onlyfans_app.py").read_text(
        encoding="utf-8"
    )

    assert "click_at" not in source
    assert "xdotool" not in source
    assert "wtype" not in source
    assert "/dev/uinput" not in source
    assert "media_grid=" not in source


def test_ofscraper_command_is_locked_to_normal_metadata(tmp_path):
    service = OnlyFansAppService(FakeConfig(cache_dir=str(tmp_path)), ROOT)

    command = service._ofscraper_command("damagedmissfit", tmp_path / "result.json")

    assert command[1] == "metadata"
    assert command[command.index("--metadata") + 1] == "check"
    assert "--action" not in command
    assert "--normal-only" in command
    assert "--protected-only" not in command
    assert "like" not in command
    assert "unlike" not in command
    assert command[command.index("--username") + 1] == "damagedmissfit"
    assert command[command.index("--quality") + 1] == "source"


def test_ofscraper_database_payload_requires_exact_creator(tmp_path):
    service = OnlyFansAppService(FakeConfig(cache_dir=str(tmp_path)), ROOT)
    database_root = (
        service.ofscraper_config_root / "main_profile/.data/123"
    )
    database_root.mkdir(parents=True)
    database = sqlite3.connect(database_root / "user_data.db")
    database.execute(
        "CREATE TABLE profiles (user_id INTEGER, username TEXT)"
    )
    database.execute("INSERT INTO profiles VALUES (123, 'damagedmissfit')")
    database.execute(
        "CREATE TABLE posts (post_id INTEGER, text TEXT, created_at TEXT, "
        "pinned BOOLEAN)"
    )
    database.execute(
        "INSERT INTO posts VALUES (5, 'caption', '2026-08-21', 1)"
    )
    database.execute(
        "CREATE TABLE medias (media_id INTEGER, post_id INTEGER, link TEXT, "
        "media_type TEXT, created_at TEXT, posted_at TEXT, unlocked BOOLEAN)"
    )
    database.execute(
        "INSERT INTO medias VALUES (9, 5, "
        "'https://cdn2.onlyfans.com/file.jpg', 'Images', '', '', 1)"
    )
    database.commit()
    database.close()

    payload = service._ofscraper_database_payload("damagedmissfit")

    assert payload["username"] == "damagedmissfit"
    assert payload["posts"][0]["isPinned"] is True
    assert payload["media"][0]["id"] == 9
    try:
        service._ofscraper_database_payload("someoneelse")
    except ValueError as exc:
        assert "no profile data" in str(exc)
    else:
        raise AssertionError("cross-profile database was accepted")


def test_ofscraper_download_reports_ip_bound_cdn_403(tmp_path, monkeypatch):
    service = OnlyFansAppService(FakeConfig(cache_dir=str(tmp_path)), ROOT)
    service._ensure_ofscraper_config()
    service.ofscraper_auth_path.write_text(
        json.dumps(
            {"auth_id": "1", "sess": "session", "x-bc": "token",
             "user_agent": "Mozilla/5.0"}
        ),
        encoding="utf-8",
    )

    def denied(*_args, **_kwargs):
        raise urllib.error.HTTPError("cdn", 403, "denied", {}, None)

    monkeypatch.setattr(onlyfans_app_module.urllib.request, "urlopen", denied)
    payload = {
        "media": [
            {
                "id": 9,
                "postId": 5,
                "media_type": "Images",
                "link": "https://cdn2.onlyfans.com/full-size.jpg?Policy=test",
            }
        ]
    }

    try:
        service._download_ofscraper_media("damagedmissfit", payload)
    except ValueError as exc:
        assert "IP-bound CDN URLs returned 403" in str(exc)
    else:
        raise AssertionError("CDN 403 was not surfaced")


def test_ofscraper_config_never_contains_cdm_credentials(tmp_path):
    service = OnlyFansAppService(
        FakeConfig(cache_dir=str(tmp_path), ffmpeg_binary="ffmpeg"), ROOT
    )

    path = service._ensure_ofscraper_config()
    payload = json.loads(path.read_text(encoding="utf-8"))

    assert payload["cdm_options"]["private-key"] is None
    assert payload["cdm_options"]["client-id"] is None
    assert payload["file_options"]["save_location"] == str(
        service.ofscraper_download_root
    )
    assert payload["file_options"]["file_format"].startswith("{media_id}_")


def test_ofscraper_auth_is_imported_from_firefox_without_browser_input(
    tmp_path, monkeypatch
):
    firefox = tmp_path / "firefox-profile"
    firefox.mkdir()
    cookies = sqlite3.connect(firefox / "cookies.sqlite")
    cookies.execute(
        "CREATE TABLE moz_cookies "
        "(name TEXT, value TEXT, host TEXT, expiry INTEGER)"
    )
    cookies.executemany(
        "INSERT INTO moz_cookies VALUES (?, ?, ?, ?)",
        [
            ("sess", "session-value", "onlyfans.com", 9999999999),
            ("auth_id", "123456789", "onlyfans.com", 9999999999),
        ],
    )
    cookies.commit()
    cookies.close()
    storage_root = firefox / "storage/default/https+++onlyfans.com/ls"
    storage_root.mkdir(parents=True)
    storage = sqlite3.connect(storage_root / "data.sqlite")
    storage.execute("CREATE TABLE data (key TEXT PRIMARY KEY, value BLOB)")
    storage.execute(
        "INSERT INTO data VALUES (?, ?)",
        ("bcTokenSha", b"abcdefghijklmnopqrstuvwxyz1234567890ABCD"),
    )
    storage.commit()
    storage.close()
    monkeypatch.setattr(
        onlyfans_app_module, "_discover_firefox_profile", lambda: firefox
    )
    monkeypatch.setattr(
        onlyfans_app_module.subprocess,
        "run",
        lambda *_args, **_kwargs: subprocess.CompletedProcess(
            [], 0, stdout="Mozilla Firefox 153.0.4\n", stderr=""
        ),
    )
    service = OnlyFansAppService(FakeConfig(cache_dir=str(tmp_path)), ROOT)

    auth_path = service.import_ofscraper_login_from_firefox()
    auth = json.loads(auth_path.read_text(encoding="utf-8"))

    assert auth["sess"] == "session-value"
    assert auth["auth_id"] == "123456789"
    assert auth["x-bc"] == "abcdefghijklmnopqrstuvwxyz1234567890ABCD"
    assert auth["user_agent"].endswith("Firefox/153.0")
    assert auth_path.stat().st_mode & 0o777 == 0o600


def test_ofscraper_hook_rejects_cross_profile_results(tmp_path):
    hook = ROOT / "rockpod/scripts/ofscraper_capture_hook.py"
    manifest = tmp_path / "manifest.json"
    environment = os.environ.copy()
    environment.update(
        {
            "ROCKPOD_OFSCRAPER_MANIFEST": str(manifest),
            "ROCKPOD_OFSCRAPER_USERNAME": "damagedmissfit",
        }
    )

    result = subprocess.run(
        [str(hook)],
        input=json.dumps({"username": "kellycadigan", "action": "download"}),
        text=True,
        env=environment,
        check=False,
    )

    assert result.returncode == 3
    assert not manifest.exists()


def test_ofscraper_ingest_preserves_full_size_and_creator(tmp_path, monkeypatch):
    service = OnlyFansAppService(FakeConfig(cache_dir=str(tmp_path)), ROOT)
    creator = service.ofscraper_download_root / "damagedmissfit/123/Images"
    creator.mkdir(parents=True)
    photo = creator / "456_source.jpg"
    Image.new("RGB", (2200, 3300), "purple").save(photo, quality=96)
    video_dir = service.ofscraper_download_root / "damagedmissfit/123/Videos"
    video_dir.mkdir(parents=True)
    video = video_dir / "789_source.mp4"
    video.write_bytes(b"test-video-with-audio")
    monkeypatch.setattr(
        onlyfans_app_module, "_probe_stream_types",
        lambda *_args, **_kwargs: {"video", "audio"},
    )
    monkeypatch.setattr(
        onlyfans_app_module, "_media_decodes", lambda *_args, **_kwargs: True
    )
    payload = {
        "username": "damagedmissfit",
        "action": "download",
        "userdata": {"name": "Damaged Missfit", "about": "Profile bio"},
        "posts": [{"id": 123, "text": "Newest post", "postedAt": "2026-08-21"}],
        "media": [
            {"id": 456, "postId": 123, "filepath": str(photo)},
            {"id": 789, "postId": 123, "filepath": str(video)},
        ],
    }

    report = service._ingest_ofscraper_result("damagedmissfit", payload)

    assert report["photos"] == 1
    assert report["videos"] == 1
    profile = report["profile"]
    assert profile["username"] == "damagedmissfit"
    assert profile["display_name"] == "Damaged Missfit"
    photo_item = next(item for item in profile["media"] if item["type"] == "photo")
    assert (photo_item["width"], photo_item["height"]) == (2200, 3300)
    with Image.open(photo_item["source_path"]) as imported:
        assert imported.size == (2200, 3300)
    assert all(
        Path(item["source_path"]).parent == tmp_path / "onlyfans/damagedmissfit/media"
        for item in profile["media"]
    )


def test_dash_audio_and_video_variants_are_grouped(tmp_path, monkeypatch):
    entries = tmp_path / "cache2" / "entries"
    entries.mkdir(parents=True)
    stem = "https://cdn3.onlyfans.com/dash/files/a/post/item"
    (entries / "video").write_bytes((stem + "_source.mp4?sig=test\0").encode())
    (entries / "audio").write_bytes((stem + "_audio.mp4?sig=test\0").encode())
    monkeypatch.setattr(
        onlyfans_app_module, "_firefox_cache_entries_root", lambda _profile: entries
    )

    groups = OnlyFansAppService._recent_video_groups(tmp_path)

    assert len(groups) == 1
    assert any("_source.mp4" in value for value in groups[0]["video"])
    assert any("_audio.mp4" in value for value in groups[0]["audio"])


def test_gzipped_dash_manifest_discovers_video_and_audio(tmp_path, monkeypatch):
    entries = tmp_path / "cache2" / "entries"
    entries.mkdir(parents=True)
    manifest = b"""<?xml version="1.0"?>
    <MPD xmlns="urn:mpeg:dash:schema:mpd:2011">
      <Period>
        <AdaptationSet contentType="video">
          <Representation><BaseURL>clip_240p.mp4</BaseURL></Representation>
        </AdaptationSet>
        <AdaptationSet mimeType="audio/mp4">
          <Representation><BaseURL>clip_audio.mp4</BaseURL></Representation>
        </AdaptationSet>
      </Period>
    </MPD>"""
    metadata = b"\0https://cdn3.onlyfans.com/dash/post/manifest.mpd?Policy=test\0"
    (entries / "manifest").write_bytes(gzip.compress(manifest) + metadata)
    monkeypatch.setattr(
        onlyfans_app_module, "_firefox_cache_entries_root", lambda _profile: entries
    )

    groups = OnlyFansAppService._recent_video_groups(tmp_path)

    assert len(groups) == 1
    assert groups[0]["protected"] is False
    assert groups[0]["video"] == [
        "https://cdn3.onlyfans.com/dash/post/clip_240p.mp4?Policy=test"
    ]
    assert groups[0]["audio"] == [
        "https://cdn3.onlyfans.com/dash/post/clip_audio.mp4?Policy=test"
    ]


def test_protected_dash_manifest_is_reported_not_downloaded(tmp_path, monkeypatch):
    entries = tmp_path / "cache2" / "entries"
    entries.mkdir(parents=True)
    manifest = b"""<MPD xmlns="urn:mpeg:dash:schema:mpd:2011">
      <Period><AdaptationSet contentType="video">
        <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011" />
        <Representation><BaseURL>clip_source.mp4</BaseURL></Representation>
      </AdaptationSet></Period>
    </MPD>"""
    metadata = b"\0https://cdn3.onlyfans.com/dash/post/manifest.mpd\0"
    (entries / "manifest").write_bytes(gzip.compress(manifest) + metadata)
    monkeypatch.setattr(
        onlyfans_app_module, "_firefox_cache_entries_root", lambda _profile: entries
    )

    groups = OnlyFansAppService._recent_video_groups(tmp_path)

    assert len(groups) == 1
    assert groups[0]["protected"] is True


def test_photo_sync_writes_native_library_and_exact_bmp_sizes(tmp_path):
    cache = tmp_path / "cache"
    source = tmp_path / "source.jpg"
    device = tmp_path / "mounted-ipod"
    device.mkdir()
    Image.new("RGB", (900, 1200), (0, 175, 240)).save(source)
    service = OnlyFansAppService(
        FakeConfig(cache_dir=str(cache), ffmpeg_binary="ffmpeg"), ROOT
    )
    service._save(
        {
            "profiles": [
                {
                    "url": "https://onlyfans.com/example",
                    "username": "example",
                    "display_name": "Example",
                    "bio": "Offline profile",
                    "avatar_path": str(source),
                    "cover_path": str(source),
                    "media": [
                        {
                            "id": "of_test",
                            "type": "photo",
                            "title": "Post 1",
                            "caption": "",
                            "source_path": str(source),
                            "width": 900,
                            "height": 1200,
                        }
                    ],
                }
            ]
        }
    )

    progress = []
    report = service.sync(device, progress=progress.append)

    assert report["profiles"] == 1
    assert report["photos"] == 1
    assert progress[0] == "Preparing OnlyFans sync · 0/1"
    assert any(message.startswith("Preparing photo 1/1") for message in progress)
    assert progress[-1] == "OnlyFans sync complete · 1/1"
    assert (device / ".rockbox/onlyfans/library.tsv").is_file()
    assert (device / ".rockbox/onlyfans/profile-feed/example.tsv").is_file()
    preview_index = device / ".rockbox/onlyfans/previews.tsv"
    assert preview_index.is_file()
    preview_path = device / ".rockbox/onlyfans/display/example-profile-preview.bmp"
    with Image.open(preview_path) as image:
        assert image.size == (320, 240)
        assert image.getpixel((0, 120))[1] > 150
        assert image.getpixel((319, 120))[1] > 150
    assert preview_index.read_text(encoding="utf-8").splitlines() == [
        "path",
        "/.rockbox/onlyfans/display/example-profile-preview.bmp",
    ]
    assert (device / ".rockbox/onlyfans/assets/onlyfans-logo.bmp").is_file()
    with Image.open(device / ".rockbox/onlyfans/thumbnails/of_test.bmp") as image:
        assert image.size == (96, 72)
    shard = _display_shard("of_test")
    display = device / f".rockbox/onlyfans/display/{shard}"
    with Image.open(display / "of_test.bmp") as image:
        assert image.size == (320, 200)
    for zoom in range(1, 4):
        with Image.open(display / f"of_test.zoom{zoom}.bmp") as image:
            assert image.size == (320, 200)
        directions = (
            ("up", "down") if zoom < 3
            else ("up", "right", "down", "left")
        )
        for direction in directions:
            with Image.open(
                display / f"of_test.zoom{zoom}.{direction}.bmp"
            ) as image:
                assert image.size == (320, 200)
    library = (device / ".rockbox/onlyfans/library.tsv").read_text(
        encoding="utf-8"
    )
    assert f"/.rockbox/onlyfans/display/{shard}/of_test.bmp" in library
    profile_fields = (
        device / ".rockbox/onlyfans/profiles.tsv"
    ).read_text(encoding="utf-8").splitlines()[1].split("\t")
    assert len(profile_fields) == 9

    second_progress = []
    second_report = service.sync(device, progress=second_progress.append)
    assert second_report["updated"] == 0
    assert second_report["unchanged"] == 1
    assert any(message.startswith("Up to date 1/1") for message in second_progress)


def test_sync_keeps_device_only_photo_and_video_when_laptop_sources_are_gone(
        tmp_path):
    cache = tmp_path / "cache"
    device = tmp_path / "mounted-ipod"
    display = device / ".rockbox/onlyfans/display"
    thumbs = device / ".rockbox/onlyfans/thumbnails"
    media = device / ".rockbox/onlyfans/media"
    display.mkdir(parents=True)
    thumbs.mkdir(parents=True)
    media.mkdir(parents=True)
    # Legacy flat files are migrated in place and remain usable when the
    # laptop source has already been removed.
    Image.new("RGB", (320, 200), "red").save(display / "of_keep.bmp")
    Image.new("RGB", (96, 72), "red").save(thumbs / "of_keep.bmp")
    Image.new("RGB", (96, 72), "blue").save(thumbs / "ofv_keep.bmp")
    (media / "ofv_keep.mpg").write_bytes(b"already-on-ipod")

    service = OnlyFansAppService(
        FakeConfig(cache_dir=str(cache), ffmpeg_binary="ffmpeg"), ROOT
    )
    service._save({
        "profiles": [{
            "url": "https://onlyfans.com/example",
            "username": "example",
            "display_name": "Example",
            "bio": "Device-only profile",
            "media": [
                {
                    "id": "of_keep", "type": "photo", "title": "Photo",
                    "caption": "", "source_path": str(tmp_path / "gone.jpg"),
                    "width": 900, "height": 1200,
                },
                {
                    "id": "ofv_keep", "type": "video", "title": "Video",
                    "caption": "", "source_path": str(tmp_path / "gone.mp4"),
                    "width": 0, "height": 0,
                },
            ],
        }]
    })

    report = service.sync(device)

    library = (device / ".rockbox/onlyfans/library.tsv").read_text(
        encoding="utf-8"
    )
    profile_feed = (
        device / ".rockbox/onlyfans/profile-feed/example.tsv"
    ).read_text(encoding="utf-8")
    assert "of_keep\texample\tphoto" in library
    assert "ofv_keep\texample\tvideo" in library
    assert "of_keep\texample\tphoto" in profile_feed
    assert "ofv_keep\texample\tvideo" in profile_feed
    shard = _display_shard("of_keep")
    assert (display / shard / "of_keep.bmp").is_file()
    assert not (display / "of_keep.bmp").exists()
    assert f"/.rockbox/onlyfans/display/{shard}/of_keep.bmp" in library
    assert (media / "ofv_keep.mpg").is_file()
    assert report["photos"] == 1
    assert report["videos"] == 1
    assert report["unchanged"] == 2


def test_onlyfans_h264_request_still_syncs_mpeg(
    config, mock_device, monkeypatch,
):
    service = OnlyFansAppService(config, ROOT)
    source = Path(config.get("cache_dir")) / "onlyfans-video.mp4"
    source.write_bytes(b"source")
    service._save({"profiles": [{
        "username": "creator", "display_name": "Creator", "media": [{
            "id": "ofv_switch", "type": "video", "title": "Video",
            "source_path": str(source),
        }],
    }]})

    def fake_stage(_source, target, **kwargs):
        Path(target).write_bytes(kwargs["profile"].encode("ascii"))

    monkeypatch.setattr(onlyfans_app_module, "stage_app_video", fake_stage)
    monkeypatch.setattr(
        onlyfans_app_module.subprocess, "run", lambda *a, **k: None,
    )
    media = Path(mock_device) / ".rockbox/onlyfans/media"

    service.sync(mock_device, video_profile="h264_apple_exact")

    assert (media / "ofv_switch.mpg").read_bytes() == b"quality"
    assert not (media / "ofv_switch.m4v").exists()
    assert "/.rockbox/onlyfans/media/ofv_switch.mpg" in (
        Path(mock_device) / ".rockbox/onlyfans/library.tsv"
    ).read_text(encoding="utf-8")


def test_owner_profile_repair_retains_missing_device_only_post_metadata(
        tmp_path):
    service = OnlyFansAppService(
        FakeConfig(cache_dir=str(tmp_path / "cache")), ROOT
    )
    service._save({
        "profiles": [{
            "username": "owner",
            "is_my_profile": True,
            "media": [{
                "id": "ofv_device_only",
                "type": "video",
                "title": "Keep Me",
                "source_path": str(tmp_path / "removed.mp4"),
                "manual_import": True,
                "imported_at_ns": 1,
            }],
        }]
    })
    (service.root / "owner/media").mkdir(parents=True)

    profiles = service.list_profiles()

    assert [item["id"] for item in profiles[0]["media"]] == [
        "ofv_device_only"
    ]


def test_native_app_is_standalone_and_in_applications_menu():
    plugin = (ROOT / "apps/plugins/onlyfans.c").read_text(encoding="utf-8")
    root_menu = (ROOT / "apps/root_menu.c").read_text(encoding="utf-8")
    categories = (ROOT / "apps/plugins/CATEGORIES").read_text(encoding="utf-8")
    assert 'OF_ROOT         ROCKBOX_DIR "/onlyfans"' in plugin
    assert 'PLUGIN_APPS_DIR "/onlyfans.rock"' in root_menu
    assert '{ "OnlyFans", launch_onlyfans_plugin,' in root_menu
    assert 'ROCKBOX_DIR "/onlyfans/hidden"' in root_menu
    assert "onlyfans_item_callback" in root_menu
    assert "root_menu_video_application_item_visible" in root_menu
    assert "root_menu_video_application_item_at(" in root_menu
    assert "selected)->function(NULL)" in root_menu
    assert "onlyfans,apps" in categories
    assert "OF_SCREEN_PROFILES" in plugin
    assert "visible_posts[OF_MAX_POSTS]" in plugin
    assert "of_select_profile(profile_selection)" in plugin
    assert "of_step_viewer_photo(-1)" in plugin
    assert "of_step_viewer_photo(1)" in plugin
    assert "of_load_profile_page(index, 0)" in plugin
    assert "profile_post_offset + ordinal + 1" in plugin
    assert "of_change_profile_page(1, false)" in plugin
    assert "of_load_viewer_level(viewer_zoom + 1, OF_PAN_CENTER)" in plugin
    assert "rb->wheel_status()" in plugin
    assert "rb->wheel_send_events(false)" in plugin
    assert '".zoom%d.%s.bmp"' in plugin
    assert '"1x", "1.5x", "2x", "3x"' in plugin
    assert "profile->cover" in plugin
    assert "cover_bm.data" in plugin
    assert "of_find_my_profile" in plugin
    assert '"Touch: Pan  Select: Zoom' in plugin
    assert "profile->is_my_profile" in plugin
    assert 'OF_PROFILE_FEED OF_ROOT "/profile-feed"' in plugin
    assert "of_profile_feed_path" in plugin
    assert "profile_art_index" in plugin


def test_rockpod_onlyfans_panel_has_determinate_progress_feedback():
    panel = (ROOT / "rockpod/ui/onlyfans_panel.py").read_text(encoding="utf-8")
    assert "QProgressBar" in panel
    assert "def _set_progress_message" in panel
    assert 're.findall(r"(\\d+)\\s*/\\s*(\\d+)"' in panel
    assert "self._job.signals.progress.connect(self._set_progress_message)" in panel


def test_twemoji_fallback_is_native_to_shared_rockbox_text_renderer():
    font = (ROOT / "firmware/font.c").read_text(encoding="utf-8")
    lcd = (ROOT / "firmware/drivers/lcd-bitmap-common.c").read_text(
        encoding="utf-8"
    )
    emoji = (ROOT / "firmware/export/social_emoji.h").read_text(encoding="utf-8")
    data = (ROOT / "firmware/export/social_emoji_data.h").read_text(
        encoding="utf-8"
    )
    lookup = (ROOT / "firmware/social_emoji.c").read_text(encoding="utf-8")
    assert "social_emoji_lookup(emoji_text, NULL)" in font
    assert "lcd_bitmap_transparent_part" in lcd
    assert "twemoji_atlas" in lcd
    assert "SOCIAL_EMOJI_MAX_SEQUENCE  10" in emoji
    assert "longest-first" in lookup
    assert "text[text_index] == 0xfe0f" in lookup
    assert "#define SOCIAL_EMOJI_ASSET_COUNT 4009" in data
    assert (
        ROOT / "apps/bitmaps/native/twemoji_atlas.224x3514x16.bmp"
    ).is_file()


def test_my_profile_supports_zero_posts_and_local_import(tmp_path):
    cache = tmp_path / "cache"
    avatar = tmp_path / "avatar.jpg"
    header = tmp_path / "header.jpg"
    post = tmp_path / "post.jpg"
    second_post = tmp_path / "second-post.png"
    Image.new("RGB", (144, 144), "red").save(avatar)
    Image.new("RGB", (760, 666), "blue").save(header)
    Image.new("RGB", (900, 1200), "green").save(post)
    Image.new("RGB", (1200, 900), "yellow").save(second_post)
    service = OnlyFansAppService(FakeConfig(cache_dir=str(cache)), ROOT)

    profile = service.save_my_profile(
        "https://onlyfans.com/cum2play",
        "Cum 2 Play",
        "Old account",
        avatar,
        header,
    )

    assert profile["username"] == "cum2play"
    assert profile["is_my_profile"] is True
    assert profile["media"] == []
    assert Path(profile["avatar_path"]).is_file()
    assert Path(profile["cover_path"]).is_file()
    assert Path(profile["avatar_path"]).read_bytes() == avatar.read_bytes()
    assert Path(profile["cover_path"]).read_bytes() == header.read_bytes()

    item = service.add_my_profile_post(post, "My post", "Local caption")
    assert item["type"] == "photo"
    assert item["manual_import"] is True
    assert item["title"] == "My post"
    assert Path(item["source_path"]).is_file()
    assert service.get_my_profile()["media"][0]["caption"] == "Local caption"

    second = service.add_my_profile_post(second_post, "Second post", "Still here")
    service.save_my_profile(
        "https://onlyfans.com/cum2play",
        "Cum 2 Play",
        "Updated account",
        avatar,
        header,
    )
    saved = service.get_my_profile()["media"]
    assert [entry["id"] for entry in saved] == [second["id"], item["id"]]
    assert all(entry["manual_import"] for entry in saved)


def test_my_profile_recovers_legacy_posts_left_in_owner_media_folder(tmp_path):
    cache = tmp_path / "cache"
    art = tmp_path / "art.jpg"
    Image.new("RGB", (144, 144), "red").save(art)
    service = OnlyFansAppService(FakeConfig(cache_dir=str(cache)), ROOT)
    service.save_my_profile(
        "https://onlyfans.com/cum2play", "Owner", "Bio", art, art
    )
    media_root = cache / "onlyfans/cum2play/media"
    media_root.mkdir(parents=True)
    orphan = media_root / "of_0123456789abcdef0123.jpg"
    Image.new("RGB", (640, 960), "purple").save(orphan)

    recovered = service.get_my_profile()["media"]

    assert len(recovered) == 1
    assert recovered[0]["id"] == orphan.stem
    assert recovered[0]["manual_import"] is True
    assert recovered[0]["width"] == 640
    assert recovered[0]["height"] == 960


def test_my_profile_never_captures_unrelated_firefox_media(tmp_path, monkeypatch):
    cache = tmp_path / "cache"
    avatar = tmp_path / "avatar.jpg"
    Image.new("RGB", (144, 144), "red").save(avatar)
    service = OnlyFansAppService(FakeConfig(cache_dir=str(cache)), ROOT)
    service.save_my_profile(
        "https://onlyfans.com/cum2play", "My Profile", "Owner bio", avatar, avatar
    )
    monkeypatch.setattr(
        onlyfans_app_module.subprocess,
        "Popen",
        lambda *args, **kwargs: (_ for _ in ()).throw(
            AssertionError("Firefox must not open for My Profile")
        ),
    )

    report = service.capture_profile("https://onlyfans.com/cum2play")

    assert report["photos"] == 0
    assert report["videos"] == 0


def test_sync_can_hide_onlyfans_application(tmp_path):
    cache = tmp_path / "cache"
    source = tmp_path / "source.jpg"
    device = tmp_path / "mounted-ipod"
    device.mkdir()
    Image.new("RGB", (900, 1200), "blue").save(source)
    service = OnlyFansAppService(
        FakeConfig(
            cache_dir=str(cache),
            ffmpeg_binary="ffmpeg",
            onlyfans_show_on_ipod=False,
        ),
        ROOT,
    )
    service._save(
        {
            "profiles": [
                {
                    "username": "private",
                    "media": [
                        {
                            "id": "of_private",
                            "type": "photo",
                            "source_path": str(source),
                            "width": 900,
                            "height": 1200,
                        }
                    ],
                }
            ]
        }
    )

    service.sync(device)

    assert (device / ".rockbox/onlyfans/hidden").read_text() == "hidden\n"


def test_cross_profile_cache_contamination_is_repaired(tmp_path):
    cache = tmp_path / "cache"
    source_a = tmp_path / "a.jpg"
    source_b = tmp_path / "b.jpg"
    Image.new("RGB", (900, 1200), "red").save(source_a)
    Image.new("RGB", (900, 1200), "blue").save(source_b)
    service = OnlyFansAppService(FakeConfig(cache_dir=str(cache)), ROOT)
    shared = {
        "id": "of_shared",
        "type": "photo",
        "source_path": str(source_a),
        "width": 900,
        "height": 1200,
    }
    service._save(
        {
            "profiles": [
                {
                    "username": "first",
                    "captured_at": "2026-08-20 10:00:00",
                    "media": [dict(shared)],
                },
                {
                    "username": "later",
                    "captured_at": "2026-08-20 11:00:00",
                    "avatar_path": str(source_a),
                    "cover_path": str(source_a),
                    "media": [
                        dict(shared),
                        {
                            "id": "of_later",
                            "type": "photo",
                            "source_path": str(source_b),
                            "width": 900,
                            "height": 1200,
                        },
                    ],
                },
            ]
        }
    )

    assert service.repair_cross_profile_media() == {"later": 1}
    later = service.list_profiles()[1]
    assert [item["id"] for item in later["media"]] == ["of_later"]
    assert later["avatar_path"] == str(source_b)
    assert later["cover_path"] == str(source_b)
