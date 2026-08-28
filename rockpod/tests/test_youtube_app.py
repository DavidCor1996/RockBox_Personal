import os
from pathlib import Path
from types import SimpleNamespace

from PIL import Image

from services.youtube_app import (
    YoutubeAppService,
    _youtube_channel_url,
    _youtube_upload_date,
    _youtube_video_id,
)


ROOT = Path(__file__).resolve().parents[2]


def test_youtube_url_import_helpers():
    assert _youtube_video_id("https://www.youtube.com/watch?v=VQ2TWroCFgs") == "VQ2TWroCFgs"
    assert _youtube_video_id("https://youtu.be/VQ2TWroCFgs") == "VQ2TWroCFgs"
    assert _youtube_upload_date("20141118") == "2014-11-18"
    assert _youtube_channel_url("https://www.youtube.com/@memoriamatters/") == (
        "https://www.youtube.com/@memoriamatters"
    )


def test_youtube_channel_sync_keeps_only_three_latest_uploads(db, config, tmp_dir):
    service = YoutubeAppService(db, config, ROOT)
    channel_url = "https://www.youtube.com/@memoriamatters"
    service.add_channel_sync(channel_url)

    old_source = Path(tmp_dir) / "old.mpg"
    old_source.write_bytes(b"old")
    old = service.add_video(
        old_source,
        title="Old upload",
        channel_url=channel_url,
        source_type="youtube-channel",
    )
    manual_source = Path(tmp_dir) / "manual.mpg"
    manual_source.write_bytes(b"manual")
    manual = service.add_video(
        manual_source,
        title="Manual video",
        channel_url=channel_url,
        source_type="youtube",
    )
    latest_sources = []
    for number in range(3):
        source = Path(tmp_dir) / f"latest-{number}.mpg"
        source.write_bytes(f"latest-{number}".encode())
        latest_sources.append(source)

    service._channel_uploads = lambda _url, _limit: [
        ("ABCDEFGHI01", "Memoria Matters"),
        ("ABCDEFGHI02", "Memoria Matters"),
        ("ABCDEFGHI03", "Memoria Matters"),
    ]
    counter = iter(latest_sources)

    def import_upload(_url, destination, channel_url=""):
        return service.add_video(
            next(counter),
            title="Newest upload",
            show_profile=destination == "profile",
            channel_url=channel_url,
            source_type="youtube-channel",
        )

    service.import_url = import_upload
    report = service.sync_channel_uploads()

    rows = [
        row for row in service.list_videos()
        if row["channel_url"] == channel_url
        and row["source_type"] == "youtube-channel"
    ]
    assert report == {"channels": 1, "videos_added": 3, "videos_removed": 1}
    assert len(rows) == 3
    assert service.get_video(old["id"]) is None
    assert service.get_video(manual["id"]) is not None
    assert service.list_channel_syncs()[0]["channel_name"] == "Memoria Matters"


def test_youtube_schema_profile_and_stable_video_ids(db, config, tmp_dir):
    service = YoutubeAppService(db, config, ROOT)
    source = Path(tmp_dir) / "my clip.mpg"
    source.write_bytes(b"mpeg-test")
    first = service.add_video(
        source,
        title="My Clip",
        uploader="david2007",
        show_home=True,
        show_profile=False,
        rating_average=4.73,
    )
    second = service.add_video(source, title="My Clip Edited")

    assert first["id"] == second["id"]
    assert service.get_video(first["id"])["title"] == "My Clip Edited"
    profile = service.save_profile(
        {"username": "david2007", "about_me": "Old-school profile"}
    )
    assert profile["username"] == "david2007"
    assert profile["about_me"] == "Old-school profile"


def test_youtube_sync_is_incremental_and_keeps_local_state(db, config, mock_device, tmp_dir):
    service = YoutubeAppService(db, config, ROOT)
    source = Path(tmp_dir) / "clip.mpg"
    source.write_bytes(b"small-mpeg-fixture")
    thumbnail = Path(tmp_dir) / "thumb.png"
    Image.new("RGB", (320, 180), "red").save(thumbnail)
    banner = Path(tmp_dir) / "banner.png"
    banner_image = Image.new("RGB", (936, 40), "black")
    banner_image.paste("red", (0, 0, 312, 40))
    banner_image.paste("green", (312, 0, 624, 40))
    banner_image.paste("blue", (624, 0, 936, 40))
    banner_image.save(banner)
    row = service.add_video(
        source,
        title="Chocolate Rain",
        uploader="TayZonday",
        description="A personal offline test entry",
        upload_date="2007-04-22",
        view_count=12_481,
        rating_average=4.82,
        rating_count=18432,
        thumbnail_path=thumbnail,
        show_home=True,
        show_profile=True,
    )
    service.save_profile(
        {
            "username": "david2007",
            "banner_image": str(banner),
            "banner_alignment": "right",
            "banner_vertical_alignment": "bottom",
            "show_on_main_menu": False,
        }
    )
    device = SimpleNamespace(mount_path=mock_device)
    first = service.sync(mock_device, device=device)

    assert first["media_updated"] == 1
    library = Path(mock_device) / ".rockbox/youtube/library.tsv"
    assert "Chocolate Rain" in library.read_text()
    assert "TayZonday" in library.read_text()
    assert library.read_text().splitlines()[0].endswith(
        "source_type\tmenu_preview"
    )
    assert (Path(mock_device) / "YouTube/videos" / f"{row['id']}.mpg").read_bytes() == source.read_bytes()
    assert (Path(mock_device) / ".rockbox/youtube/thumbnails" / f"{row['id']}.bmp").is_file()
    preview = Path(mock_device) / ".rockbox/youtube/previews" / f"{row['id']}.bmp"
    assert Image.open(preview).size == (320, 240)
    assert (Path(mock_device) / ".rockbox/ipodjs/youtube/youtube-logo-2006.bmp").is_file()
    assert (
        Path(mock_device)
        / ".rockbox/ipodjs/youtube/youtube-stars-active-2007.bmp"
    ).is_file()
    exported_banner = Path(mock_device) / ".rockbox/youtube/banner.bmp"
    assert Image.open(exported_banner).size == (312, 56)
    assert Image.open(exported_banner).getpixel((156, 28))[2] > 200
    profile_cfg = (Path(mock_device) / ".rockbox/youtube/profile.cfg").read_text()
    assert "banner_image=/.rockbox/youtube/banner.bmp" in profile_cfg
    assert "banner_alignment=right" in profile_cfg
    assert "banner_vertical_alignment=bottom" in profile_cfg
    assert (
        Path(mock_device)
        / ".rockbox/ipodjs/youtube/youtube-player-seek-knob-2007.bmp"
    ).is_file()
    assert (
        Path(mock_device)
        / ".rockbox/ipodjs/youtube/youtube-player-volume-knob-2007.bmp"
    ).is_file()

    state = Path(mock_device) / ".rockbox/youtube/state.tsv"
    state.write_text(f"id\tmy_rating\tfavorite\n{row['id']}\t5\t1\n")
    service.update_video(row["id"], {"description": "Metadata only"})
    progress = []
    second = service.sync(
        mock_device,
        device=device,
        progress_callback=lambda done, total, label: progress.append(
            (done, total, label)
        ),
    )

    assert second["media_updated"] == 0
    assert second["media_unchanged"] == 1
    assert second["thumbnails_updated"] == 0
    assert second["previews_updated"] == 0
    assert second["previews_unchanged"] == 1
    assert second["local_state_merged"] == 1
    assert any("Preparing video 1/1" in label for _, _, label in progress)
    assert progress[-1][0] == progress[-1][1]
    assert progress[-1][2] == "YouTube sync complete"
    updated = service.get_video(row["id"])
    assert updated["my_rating"] == 5
    assert updated["favorite"] == 1

    service.remove_video(row["id"])
    third = service.sync(mock_device, device=device)
    assert third["stale_files_removed"] == 5
    assert not (Path(mock_device) / "YouTube/videos" / f"{row['id']}.mpg").exists()
    assert not (Path(mock_device) / "YouTube/videos" / f"{row['id']}.ytm").exists()
    assert not (Path(mock_device) / ".rockbox/youtube/thumbnails" / f"{row['id']}.bmp").exists()
    assert not preview.exists()


def test_youtube_is_standalone_and_player_returns_to_detail():
    plugin = (ROOT / "apps/plugins/youtube.c").read_text()
    player = (ROOT / "apps/plugins/mpegplayer/mpegplayer.c").read_text()
    menu = (ROOT / "apps/root_menu.c").read_text()
    offlineweb = (ROOT / "apps/plugins/offlineweb.c").read_text()

    assert '#define YT_ROOT             ROCKBOX_DIR "/youtube"' in plugin
    assert 'YT_PLAYER_PREFIX    "youtube-app:"' in plugin
    assert '"return:%s"' in player
    assert 'PLUGIN_APPS_DIR "/youtube.rock"' in player
    assert '{ "youtube", &youtube_item }' in menu
    assert '&desktop_mode_item, &youtube_item' in menu
    assert 'if (item == &youtube_item)\n        return "YouTube";' in menu
    assert "IPODJS_PREVIEW_YOUTUBE" in menu
    assert "root_menu_video_preview_load_youtube_paths" in menu
    assert "fields[19][0] ? fields[19] : fields[13]" in menu
    assert 'ROCKBOX_DIR "/youtube/library.tsv"' in menu
    assert "root_menu_video_youtube_order" in menu
    assert 'ROCKBOX_DIR "/youtube"' not in offlineweb


def test_youtube_browser_and_player_use_stock_live_controls():
    plugin = (ROOT / "apps/plugins/youtube.c").read_text()
    player = (ROOT / "apps/plugins/mpegplayer/mpegplayer.c").read_text()

    assert "#define YT_LIST_VISIBLE     2" in plugin
    assert "yt_draw_video_list" in plugin
    assert "yt_draw_scrollbar" in plugin
    assert "selection[current_page] = MAX(0, MIN(next, count - 1));" in plugin
    assert "{ PLA_SCROLL_BACK,        BUTTON_SCROLL_BACK" in plugin
    assert "{ PLA_SCROLL_FWD,         BUTTON_SCROLL_FWD" in plugin
    assert "{ PLA_CANCEL,             BUTTON_MENU" in plugin
    assert '"MENU Back"' in plugin
    assert '"MENU Exit"' in plugin
    assert "YT_FILTER_MOST_VIEWED" in plugin
    assert "YT_FILTER_TOP_RATED" in plugin
    assert "YT_FILTER_FAVORITES" in plugin
    assert "YT_FILTER_RELATED" in plugin
    assert "YT_PAGE_SUBSCRIPTIONS" in plugin
    assert "Subscription Uploads" in plugin
    assert '"youtube-channel"' in plugin
    assert '"Related"' in plugin
    assert "profile_focus" in plugin
    assert "yt_draw_profile_field" in plugin
    assert "profile.movies" in plugin
    assert "profile.music" in plugin
    assert "profile.books" in plugin
    assert "profile.website" in plugin
    assert "lcd_bitmap_transparent_part" in plugin
    assert "yt_detail_delta" in plugin
    assert "PLA_SELECT_REPEAT" in plugin
    assert "select_held" in plugin
    assert '"HOLD SEL Sort"' in plugin
    assert "result = yt_play_video();" in plugin
    assert '#define YT_MPEG_CONFIG      VIEWERS_DIR "/mpegplayer.cfg"' in plugin
    assert "yt_load_resume_positions();" in plugin
    assert '"Resume %lu:%02lu  %s"' in plugin
    assert 'video->resume_seconds > 0 ? "Resume" : "Watch"' in plugin
    assert "thumb_next_slot" in plugin
    assert "static fb_data logo_data" in plugin
    assert "static fb_data thumb_data" in plugin
    assert "static fb_data banner_data" in plugin
    assert "banner_valid" in plugin
    assert "lcd_bitmap_part" in plugin
    assert plugin.count("CACHEALIGN_ATTR;") >= 5
    assert "if (redraw)" in plugin
    assert "mpeg_yuv_youtube_sprite" in player
    assert "YOUTUBE_SEEK_KNOB_PATH" in player
    assert "YOUTUBE_VOLUME_KNOB_PATH" in player
    assert "volume_percent * MPEG_YOUTUBE_VOLUME_W / 100" in player
    assert "osd_show(OSD_SHOW);" in player
    assert "if (mpegplayer_youtube_app_launch)" in player
    assert "settings.resume_options = MPEG_RESUME_ALWAYS;" in player
    assert "result = mpeg_start_menu(stream_get_duration());" in player
    assert "stock early/95%-complete thresholds" in player


def test_youtube_assets_have_historical_source_records():
    assets = ROOT / "assets/ipodjs/rockbox/youtube"
    sources = (assets / "SOURCES.md").read_text()
    references = (ROOT / "docs/youtube-2007-visual-references.md").read_text()

    assert (assets / "youtube-logo-2006.bmp").is_file()
    assert (assets / "youtube-stars-5-2007.bmp").is_file()
    assert (assets / "youtube-stars-active-2007.bmp").is_file()
    assert (assets / "youtube-player-2007.bmp").is_file()
    assert (assets / "youtube-player-seek-knob-2007.bmp").is_file()
    assert (assets / "youtube-player-volume-knob-2007.bmp").is_file()
    assert "web.archive.org/web/20070601000000im_" in sources
    assert "/img/star.gif" in sources
    assert "YouTube_video_player_history.png" in sources
    assert "Official YouTube Blog, January 2007" in references


def test_rockpod_youtube_sync_is_non_blocking_and_reports_progress():
    panel = (ROOT / "rockpod/ui/youtube_panel.py").read_text(encoding="utf-8")

    assert "class YoutubeSyncJob(QRunnable)" in panel
    assert "QThreadPool.globalInstance().start(self._sync_job)" in panel
    assert "QProgressDialog" in panel
    assert "def _sync_status(self, done, total, message):" in panel
    assert "self._sync_progress.setLabelText(label)" in panel
    assert "self.sync_button.setEnabled(False)" in panel
    assert "self.sync_button.setEnabled(True)" in panel
