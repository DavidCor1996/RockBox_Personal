from pathlib import Path

from PIL import Image

from services.instagram_app import (
    InstagramAppService,
    _group_media_items,
    _ipod_text,
    _merge_retained_media,
    _parse_instagram_description,
    _post_sort_key,
    _profile_slug,
    _save_instagram_zoom_view,
    _sort_library_lines,
)


ROOT = Path(__file__).resolve().parents[2]


def test_instagram_profile_url_validation():
    assert _profile_slug("https://www.instagram.com/jasmine.in.dreamland/") == (
        "jasmine.in.dreamland"
    )


def test_instagram_public_description_maps_real_profile_counts():
    page = (
        '<meta name="description" content="597 Followers, 664 Following, '
        '1.2K Posts - Test (@example) on Instagram: &quot;Hello &amp; welcome&quot;">'
    )

    assert _parse_instagram_description(page) == {
        "follower_count": 597,
        "following_count": 664,
        "post_count": 1200,
        "bio": "Hello & welcome",
    }


def test_instagram_caption_fallback_is_safe_for_the_ipod_ui_font():
    assert _ipod_text("🌷𝓙𝓪𝓼𝓶𝓲𝓷𝓮 𝓜𝓲𝓻𝓪𝓷𝓭𝓪🌷") == "Jasmine Miranda"
    assert _ipod_text("𝓜𝓸𝓶 • 𝓒𝓸𝓾𝓷𝓼𝓮𝓵𝓵𝓸𝓻") == "Mom - Counsellor"


def test_instagram_post_sort_key_is_newest_first():
    rows = [
        {"id": "ig_1", "post_date": "2025-01-01 01:00:00"},
        {"id": "ig_3", "post_date": "2026-08-22 18:00:44"},
        {"id": "ig_2", "post_date": "2026-08-22 18:00:44"},
    ]
    assert [row["id"] for row in sorted(rows, key=_post_sort_key, reverse=True)] == [
        "ig_3", "ig_2", "ig_1",
    ]


def test_instagram_combined_home_feed_is_newest_first_and_keeps_group_order():
    header = "id\tusername\ttype\ttitle\tcaption\tmedia\tthumbnail\tdisplay\tlikes\tpost_date"
    rows = [
        "old\ta\tphoto\t\t\t\t\t\t0\t2025-01-01",
        "group-1\tb\tphoto\t\t\t\t\t\t0\t2026-08-22",
        "group-2\tb\tphoto\t\t\t\t\t\t0\t2026-08-22",
        "middle\ta\tphoto\t\t\t\t\t\t0\t2026-01-01",
    ]

    sorted_lines = _sort_library_lines([header, *rows])

    assert [line.split("\t")[0] for line in sorted_lines[1:]] == [
        "group-1", "group-2", "middle", "old",
    ]


def test_instagram_matching_descriptions_form_one_group_post():
    rows = [
        {"id": "ig_new", "caption": "A weekend away", "post_date": "2026-08-22"},
        {"id": "ig_other", "caption": "A different post", "post_date": "2026-08-21"},
        {"id": "ig_old", "caption": "A weekend away", "post_date": "2026-08-20"},
    ]

    grouped = _group_media_items(rows)

    assert [row["id"] for row in grouped] == ["ig_new", "ig_old", "ig_other"]
    assert [(row["group_id"], row["group_index"], row["group_count"])
            for row in grouped[:2]] == [
        ("ig_new", 1, 2), ("ig_new", 2, 2),
    ]


def test_instagram_refresh_retains_device_only_post_metadata():
    existing = [{
        "id": "ig_device_only",
        "type": "video",
        "source_path": "/deleted/from/laptop.mp4",
    }]

    assert _merge_retained_media([], existing) == existing


def test_instagram_sync_keeps_device_only_photo_and_video(config, mock_device):
    service = InstagramAppService(config, ROOT)
    service._save({
        "profiles": [{
            "username": "deviceonly",
            "display_name": "Device Only",
            "bio": "Already synced",
            "avatar_path": "/deleted/avatar.jpg",
            "media": [
                {
                    "id": "ig_photo",
                    "type": "photo",
                    "title": "Photo",
                    "caption": "Kept photo",
                    "source_path": "/deleted/photo.jpg",
                    "post_date": "2026-08-25",
                },
                {
                    "id": "ig_video",
                    "type": "video",
                    "title": "Video",
                    "caption": "Kept video",
                    "source_path": "/deleted/video.mp4",
                    "post_date": "2026-08-24",
                },
            ],
        }],
    })
    root = Path(mock_device) / ".rockbox/instagram"
    for directory in ("thumbnails", "feed", "display", "media"):
        (root / directory).mkdir(parents=True, exist_ok=True)
    for target in (
        root / "thumbnails/ig_photo.bmp",
        root / "feed/ig_photo.bmp",
        root / "display/ig_photo.bmp",
        root / "thumbnails/ig_video.bmp",
        root / "feed/ig_video.bmp",
    ):
        Image.new("RGB", (8, 8), "purple").save(target, "BMP")
    (root / "media/ig_video.mpg").write_bytes(b"device-video")

    report = service.sync(mock_device)

    assert report["photos"] == 1
    assert report["videos"] == 1
    library = (root / "library.tsv").read_text(encoding="utf-8")
    profile_feed = (root / "profile-feed/deviceonly.tsv").read_text(
        encoding="utf-8"
    )
    assert "ig_photo\tdeviceonly\tphoto" in library
    assert "ig_video\tdeviceonly\tvideo" in library
    assert "/.rockbox/instagram/media/ig_video.mpg" in library
    assert "ig_photo\tdeviceonly\tphoto" in profile_feed
    assert "ig_video\tdeviceonly\tvideo" in profile_feed


def test_instagram_zoom_migration_preserves_existing_views(tmp_path):
    source = Image.new("RGB", (900, 1200), "purple")
    display = tmp_path / "post.bmp"
    zoom = tmp_path / "zoom" / "post.zoom1.bmp"
    zoom.parent.mkdir()
    display.write_bytes(b"existing-view")

    _save_instagram_zoom_view(source, zoom)

    assert display.read_bytes() == b"existing-view"
    with Image.open(zoom) as rendered_zoom:
        assert rendered_zoom.size == (480, 300)


def test_instagram_photo_profile_sync(config, mock_device, monkeypatch):
    service = InstagramAppService(config, ROOT)
    source = Path(config.get("cache_dir")) / "instagram-source.jpg"
    Image.new("RGB", (900, 1200), "purple").save(source)
    service._save(
        {
            "profiles": [
                {
                    "url": "https://www.instagram.com/example/",
                    "username": "example",
                    "display_name": "🌷𝓙𝓪𝓼𝓶𝓲𝓷𝓮 𝓜𝓲𝓻𝓪𝓷𝓭𝓪🌷",
                    "bio": "𝓜𝓸𝓶 • 𝓒𝓸𝓾𝓷𝓼𝓮𝓵𝓵𝓸𝓻",
                    "avatar_path": str(source),
                    "follower_count": 10,
                    "following_count": 2,
                    "is_verified": True,
                    "media": [
                        {
                            "id": "ig_123",
                            "type": "photo",
                            "title": "Post",
                            "caption": "Caption",
                            "source_path": str(source),
                            "likes": 7,
                            "post_date": "2026-08-20",
                        }
                    ],
                }
            ]
        }
    )
    report = service.sync(mock_device)
    assert report["profiles"] == 1
    assert report["photos"] == 1
    root = Path(mock_device) / ".rockbox/instagram"
    assert (root / "library.tsv").is_file()
    assert (root / "profiles.tsv").is_file()
    assert (root / "profile-feed/example.tsv").is_file()
    assert (root / "display/ig_123.bmp").is_file()
    zoom = root / "zoom/ig_123.zoom1.bmp"
    assert zoom.is_file()
    with Image.open(zoom) as view:
        assert view.size == (480, 300)
    with Image.open(root / "thumbnails/ig_123.bmp") as thumb:
        assert thumb.size == (72, 72)
    with Image.open(root / "feed/ig_123.bmp") as feed:
        assert feed.size == (160, 160)
    with Image.open(root / "assets/instagram-launch-2010.bmp") as launch:
        assert launch.size == (320, 240)
    name_art_path = root / "display/example-name.bmp"
    bio_art_path = root / "display/example-bio.bmp"
    with Image.open(name_art_path) as name_art:
        assert name_art.size == (205, 18)
        assert name_art.getbbox() is not None
        assert name_art.getpixel((204, 17)) == (255, 0, 255)
    with Image.open(bio_art_path) as bio_art:
        assert bio_art.size == (304, 30)
    header, row = (root / "library.tsv").read_text(
        encoding="utf-8"
    ).splitlines()
    assert header.endswith("\tgroup_id\tgroup_index\tgroup_count\tautoplay")
    assert "/.rockbox/instagram/feed/ig_123.bmp\tig_123\t1\t1" in row
    profiles = (root / "profiles.tsv").read_text(encoding="utf-8")
    assert "🌷𝓙𝓪𝓼𝓶𝓲𝓷𝓮 𝓜𝓲𝓻𝓪𝓷𝓭𝓪🌷" in profiles
    assert "𝓜𝓸𝓶 • 𝓒𝓸𝓾𝓷𝓼𝓮𝓵𝓵𝓸𝓻" in profiles
    assert not (root / "assets/instagram-player-2010.bmp").exists()

    name_mtime = name_art_path.stat().st_mtime_ns
    bio_mtime = bio_art_path.stat().st_mtime_ns

    def unexpected_profile_render(*_args, **_kwargs):
        raise AssertionError("unchanged profile text should stay indexed")

    monkeypatch.setattr(
        "services.instagram_app._save_profile_text_art",
        unexpected_profile_render,
    )
    second_report = service.sync(mock_device)

    assert second_report["updated"] == 0
    assert name_art_path.stat().st_mtime_ns == name_mtime
    assert bio_art_path.stat().st_mtime_ns == bio_mtime


def test_instagram_plugin_has_persistent_likes_favorites_and_no_news_tab():
    source = (ROOT / "apps/plugins/instagram.c").read_text(encoding="utf-8")

    assert '#define IG_LIKES        IG_ROOT "/likes.tsv"' in source
    assert '#define IG_FAVORITES    IG_ROOT "/favorites.tsv"' in source
    assert "static bool ig_toggle_current_like(void)" in source
    assert "IG_ACTION_LIKE" in source
    assert "group_count" in source
    assert "ig_group_member_index" in source
    assert "instagram_heart_unliked" in source
    assert '"pluginbitmaps/instagram_verified.h"' in source
    assert "ig_draw_verified_badge" in source
    assert '"VERIFIED"' not in source
    assert '"[v]"' not in source
    assert '#define IG_PROFILE_FEED IG_ROOT "/profile-feed"' in source
    assert '#define IG_STATE        IG_ROOT "/state.cfg"' in source
    assert '#define IG_LAUNCH       IG_ROOT "/assets/instagram-launch-2010.bmp"' in source
    assert "static long ig_show_launch(void)" in source
    assert "launch_bm, viewer_pixels" in source
    assert "bool cold_launch = parameter == NULL" in source
    assert "if (cold_launch)\n    {\n        rb->lcd_set_background(IG_CREAM);" in source
    assert '#define IG_FEED_PREFIX  "instagram-feed:"' in source
    assert "ig_restore_home_state" in source
    assert "ig_save_home_state" in source
    assert "ig_toggle_profile_favorite" in source
    assert "IG_SCREEN_PROFILES" in source
    assert "static void ig_draw_profiles(void)" in source
    assert "static void ig_cache_list_names" in source
    assert "list_name_pixels" in source
    assert "lcd_bitmap_transparent" in source
    assert "profile_return_screen" in source
    assert "char autoplay[96]" in source
    assert "video = post->media;" in source
    assert "mpegplayer now scales this original into the feed card" in (
        ROOT / "rockpod/services/instagram_app.py"
    ).read_text(encoding="utf-8")
    assert "while (post_count < IG_MAX_POSTS" in source
    assert "profile_art_index" in source
    assert "rb->lcd_fillrect(172, 60, 32, 32)" not in source
    tabbar = source.split("static void ig_tabbar", 1)[1].split(
        "static void ig_cache_thumbnails", 1
    )[0]
    assert '"NEWS"' not in tabbar
    assert '"SEARCH"' not in tabbar
    assert '"FAVORITES"' in tabbar


def test_instagram_player_uses_real_2013_marker_and_persistent_video_likes():
    player = (ROOT / "apps/plugins/mpegplayer/mpegplayer.c").read_text(
        encoding="utf-8"
    )

    assert 'MPEGPLAYER_INSTAGRAM_FEED_PREFIX "instagram-feed:"' in player
    assert '"pluginbitmaps/instagram_video_play.h"' in player
    assert '"/instagram/assets/instagram-player-2010.bmp"' not in player
    assert "instagram_toggle_like" in player
    assert "INSTAGRAM_LIKES_PATH" in player
    assert "MPEG_INSTAGRAM_VIDEO_W 160" in player
    assert "MPEG_INSTAGRAM_VIDEO_H 158" in player
    assert "MPEG_INSTAGRAM_PLAY_MARGIN 6" in player
    assert "instagram_video_play" in player
    assert "INSTAGRAM_LIBRARY_PATH" in player
    assert "same fixed 2010 feed card" in player
    assert "instagram_heart_unliked" in player
    assert "mpegplayer_instagram_return_direction" in player
    assert "if (mpegplayer_instagram_app_launch)" in player
    assert "if (TIME_AFTER(tick, osd.hide_tick))" in player
    assert "!mpegplayer_livetv_desktop && !mpegplayer_instagram_feed_launch" in player
    assert "!feed.active &&\n                    !mpegplayer_instagram_feed_launch" in player
    assert "Authentic play artwork" in player
    assert "Inline Instagram owns a permanent feed surface" in player
    assert "instagram_player_bmp" not in player
    assert "@mikeyk" not in player

    marker = (
        ROOT / "apps/plugins/bitmaps/native"
        / "instagram_video_play.20x20x24.bmp"
    )
    with Image.open(marker) as image:
        assert image.size == (20, 20)
        assert image.mode == "RGB"


def test_instagram_sync_reuses_existing_bounded_video_previews():
    source = (ROOT / "rockpod/services/instagram_app.py").read_text(
        encoding="utf-8"
    )

    video_preview = source.split('"video-previews-v2"', 1)[1].split(
        "):", 1
    )[0]
    assert "trust_existing=True" in video_preview
