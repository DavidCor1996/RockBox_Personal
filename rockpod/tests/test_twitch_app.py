import os
import struct
import subprocess
from io import BytesIO
from pathlib import Path
from types import SimpleNamespace

import pytest
from PIL import Image

from services.twitch_app import (
    TwitchAppService,
    current_creator_programme,
    normalize_twitch_creator_url,
    twitch_vod_id,
)
from services.twitch_chat import (
    EMOJI_SIZE,
    chat_pack_sprite_indexes,
    download_chat_replay,
)


ROOT = Path(__file__).resolve().parents[2]
TEST_VOD_URL = "https://www.twitch.tv/videos/2860855364"


def test_twitch_urls_accept_the_requested_real_vod():
    assert twitch_vod_id(TEST_VOD_URL) == "2860855364"
    assert normalize_twitch_creator_url("Emma") == "https://www.twitch.tv/emma"
    assert normalize_twitch_creator_url(
        "https://www.twitch.tv/Emma/videos?filter=archives"
    ) == "https://www.twitch.tv/emma"


def test_twitch_creator_schedule_has_one_programme_at_boundaries():
    vods = [
        {"id": "tw_a", "published_date": "2026-01-01", "duration_ms": 10_000},
        {"id": "tw_b", "published_date": "2026-01-02", "duration_ms": 20_000},
        {"id": "tw_bad", "published_date": "2026-01-03", "duration_ms": 0},
    ]

    first = current_creator_programme(vods, cycle_epoch=100, now=109)
    second = current_creator_programme(vods, cycle_epoch=100, now=110)
    wrapped = current_creator_programme(vods, cycle_epoch=100, now=130)

    assert (first[0]["id"], first[1], first[2]) == ("tw_a", 9, 100)
    assert (second[0]["id"], second[1], second[2]) == ("tw_b", 0, 110)
    assert (wrapped[0]["id"], wrapped[1], wrapped[2]) == ("tw_a", 0, 130)


def test_twitch_chat_replay_packs_twitch_emotes_and_unicode_emoji(tmp_dir):
    pages = {
        None: {
            "data": {"video": {"comments": {
                "edges": [
                    {
                        "cursor": "next-page",
                        "node": {
                            "id": "message-2", "contentOffsetSeconds": 9.8,
                            "commenter": {"displayName": "Streamer"},
                            "message": {
                                "userColor": "#9147FF",
                                "fragments": [
                                    {"text": "Kappa", "emote": {"emoteID": "25"}},
                                    {"text": " hello 😀", "emote": None},
                                ],
                            },
                        },
                    },
                ],
                "pageInfo": {"hasNextPage": True},
            }}},
        },
        "next-page": {
            "data": {"video": {"comments": {
                "edges": [
                    {
                        "cursor": "done",
                        "node": {
                            "id": "message-1", "contentOffsetSeconds": 3.1,
                            "commenter": {"displayName": "Viewer"},
                            "message": {
                                "userColor": None,
                                "fragments": [{"text": "first ❤️", "emote": None}],
                            },
                        },
                    },
                ],
                "pageInfo": {"hasNextPage": False},
            }}},
        },
    }
    asset = BytesIO()
    Image.new("RGBA", (18, 12), (145, 70, 255, 255)).save(asset, "PNG")
    chat_path = Path(tmp_dir) / "vod.twc"
    emoji_path = Path(tmp_dir) / "vod.twe"

    report = download_chat_replay(
        "2860855364", chat_path, emoji_path,
        page_fetcher=lambda _video_id, cursor: pages[cursor],
        asset_fetcher=lambda _url: asset.getvalue(),
    )

    lines = chat_path.read_text(encoding="ascii").splitlines()
    emoji = emoji_path.read_bytes()
    count, width, height = struct.unpack("<HHH", emoji[4:10])
    indexes = chat_pack_sprite_indexes("\n".join(lines))
    assert lines[0] == "# rockpod-twitch-chat-v2"
    assert lines[1].startswith("3\tB8B8C0\tViewer\tfirst ")
    assert lines[2].startswith("9\t9147FF\tStreamer\t")
    assert report == {
        "messages": 2, "sprites": 3,
        "chat_path": str(chat_path), "emoji_path": str(emoji_path),
    }
    assert (count, width, height) == (3, EMOJI_SIZE, EMOJI_SIZE)
    assert len(emoji) == 10 + count * EMOJI_SIZE * EMOJI_SIZE * 4
    assert indexes and max(indexes) < count


def test_twitch_schema_creator_vods_and_incremental_sync(
    db, config, mock_device, tmp_dir, monkeypatch
):
    monkeypatch.setattr("services.twitch_app._validate_export",
                        lambda *args, **kwargs: None)
    service = TwitchAppService(db, config, ROOT)
    creator = service.add_creator("Emma", "Emma", keep_count=3, cycle_epoch=100)
    source = Path(tmp_dir) / "mma-stream.mpg"
    source.write_bytes(b"small-mpeg-twitch-fixture")
    thumbnail = Path(tmp_dir) / "mma-stream.jpg"
    Image.new("RGB", (320, 180), "#6441a5").save(thumbnail)
    vod = service.add_vod(
        source,
        creator_key=creator["creator_key"],
        twitch_id="2860855364",
        title="MMA STREAM",
        game="IRL",
        duration_ms=8_109_000,
        published_date="2026-08-30",
        view_count=2070,
        source_url=TEST_VOD_URL,
        source_type="twitch",
        thumbnail_path=thumbnail,
    )
    source.with_suffix(".twc").write_text(
        "# rockpod-twitch-chat-v2\n3\t9147FF\tEmma\tHello ~E0000~\n",
        encoding="ascii",
    )
    source.with_suffix(".twe").write_bytes(
        b"TWE1" + struct.pack("<HHH", 1, EMOJI_SIZE, EMOJI_SIZE)
        + bytes(EMOJI_SIZE * EMOJI_SIZE * 4)
    )
    device = SimpleNamespace(mount_path=mock_device, rockbox_target="ipod6g")

    first = service.sync(
        mock_device, device=device, refresh_creators=False,
        video_profile="quality",
    )
    second = service.sync(
        mock_device, device=device, refresh_creators=False,
        video_profile="quality",
    )

    assert first["media_updated"] == 1
    assert second["media_updated"] == 0
    assert second["media_unchanged"] == 1
    assert service.current_programme(creator["creator_key"], now=105)[0]["id"] == vod["id"]
    creators = Path(mock_device) / ".rockbox/twitch/creators.tsv"
    exported = Path(mock_device) / ".rockbox/twitch/vods.tsv"
    media = Path(mock_device) / "Twitch/videos" / f"{vod['id']}.mpg"
    metadata = Path(mock_device) / "Twitch/videos" / f"{vod['id']}.twm"
    chat = Path(mock_device) / "Twitch/videos" / f"{vod['id']}.twc"
    emoji = Path(mock_device) / "Twitch/videos" / f"{vod['id']}.twe"
    thumb = Path(mock_device) / ".rockbox/twitch/thumbnails" / f"{vod['id']}.bmp"
    assert "Emma" in creators.read_text()
    assert "MMA STREAM" in exported.read_text()
    assert media.read_bytes() == source.read_bytes()
    assert "creator=Emma" in metadata.read_text()
    assert "Hello ~E0000~" in chat.read_text()
    assert emoji.read_bytes().startswith(b"TWE1")
    assert first["chat_updated"] == 2
    assert second["chat_updated"] == 0
    assert Image.open(thumb).size == (96, 54)
    assert (
        Path(mock_device)
        / ".rockbox/ipodjs/twitch/twitch-wordmark-current-white.136x50.bmp"
    ).is_file()

    service.remove_vod(vod["id"])
    third = service.sync(
        mock_device, device=device, refresh_creators=False,
        video_profile="quality",
    )
    assert third["stale_files_removed"] == 5
    assert not media.exists()
    assert not metadata.exists()
    assert not chat.exists()
    assert not emoji.exists()
    assert not thumb.exists()


def test_twitch_sync_skips_one_vod_when_h264_and_mpeg_fail_and_keeps_going(
    db, config, mock_device, tmp_dir, monkeypatch
):
    service = TwitchAppService(db, config, ROOT)
    creator = service.add_creator("Emma", "Emma", cycle_epoch=100)
    bad = Path(tmp_dir) / "bad.mp4"
    good = Path(tmp_dir) / "good.mpg"
    bad.write_bytes(b"broken-input")
    good.write_bytes(b"working-mpeg")
    bad_vod = service.add_vod(
        bad, creator_key=creator["creator_key"], twitch_id="10001",
        title="Bad VOD", duration_ms=10_000, published_date="2026-01-01",
    )
    good_vod = service.add_vod(
        good, creator_key=creator["creator_key"], twitch_id="10002",
        title="Good VOD", duration_ms=10_000, published_date="2026-01-02",
    )

    from services import twitch_app

    def fail_one(source, target, **kwargs):
        if str(source) == str(bad):
            Path(target).write_bytes(b"partial")
            raise subprocess.CalledProcessError(1, ["ffmpeg"])
        Path(target).write_bytes(Path(source).read_bytes())
        return kwargs["profile"]

    monkeypatch.setattr(twitch_app, "_validate_export",
                        lambda *args, **kwargs: None)
    monkeypatch.setattr(twitch_app, "stage_app_video", fail_one)
    report = service.sync(
        mock_device,
        device=SimpleNamespace(mount_path=mock_device, rockbox_target="ipod6g"),
        refresh_creators=False,
        video_profile="h264_apple_exact",
    )

    exported = (Path(mock_device) / ".rockbox/twitch/vods.tsv").read_text()
    assert report["media_failed"] == 1
    assert report["media_updated"] == 1
    assert report["vods_exported"] == 1
    assert any("Bad VOD" in error for error in report["errors"])
    assert bad_vod["id"] not in exported
    assert good_vod["id"] in exported
    assert not (Path(mock_device) / ".rockbox/twitch/.staging" /
                f"{bad_vod['id']}.mpg").exists()
    assert not (Path(mock_device) / ".rockbox/twitch/.staging" /
                f"{bad_vod['id']}.m4v").exists()
    assert (Path(mock_device) / "Twitch/videos" /
            f"{good_vod['id']}.m4v").read_bytes() == b"working-mpeg"
    assert (Path(mock_device) / "Twitch/videos" /
            f"{good_vod['id']}.mpg").read_bytes() == b"working-mpeg"
    assert report["mpeg_backups_updated"] == 1


def test_twitch_sync_falls_back_to_clean_mpeg_after_h264_failure(
    db, config, mock_device, tmp_dir, monkeypatch
):
    service = TwitchAppService(db, config, ROOT)
    creator = service.add_creator("Emma", "Emma", cycle_epoch=100)
    source = Path(tmp_dir) / "emma.mp4"
    fallback = Path(tmp_dir) / "emma-clean.mpg"
    source.write_bytes(b"downloaded-h264")
    fallback.write_bytes(b"clean-full-mpeg")
    vod = service.add_vod(
        source, creator_key=creator["creator_key"], twitch_id="2860855364",
        title="Emma Stream", duration_ms=8_109_000,
    )

    from services import twitch_app

    def fail_h264(source_path, target, **kwargs):
        if kwargs["profile"] == "h264_apple_exact":
            Path(target).write_bytes(b"partial")
            raise subprocess.CalledProcessError(1, ["ffmpeg"])
        Path(target).write_bytes(Path(source_path).read_bytes())
        return kwargs["profile"]

    monkeypatch.setattr(twitch_app, "_validate_export",
                        lambda *args, **kwargs: None)
    monkeypatch.setattr(twitch_app, "stage_app_video", fail_h264)
    report = service.sync(
        mock_device,
        device=SimpleNamespace(mount_path=mock_device, rockbox_target="ipod6g"),
        refresh_creators=False,
        video_profile="h264_apple_exact",
    )

    exported = (Path(mock_device) / ".rockbox/twitch/vods.tsv").read_text()
    media = Path(mock_device) / "Twitch/videos" / f"{vod['id']}.mpg"
    assert report["mpeg_fallbacks"] == 1
    assert report["media_failed"] == 0
    assert report["vods_exported"] == 1
    assert report["errors"] == []
    assert report["fallback_details"] == [
        "Emma Stream: H.264 failed; MPEG succeeded"
    ]
    assert media.read_bytes() == b"clean-full-mpeg"
    assert f"/{media.relative_to(mock_device)}" in exported
    assert not (Path(mock_device) / ".rockbox/twitch/.staging" /
                f"{vod['id']}.m4v").exists()


def test_twitch_creator_refresh_skips_failed_download_without_pruning(
    db, config, tmp_dir, monkeypatch
):
    service = TwitchAppService(db, config, ROOT)
    creator = service.add_creator("Emma", "Emma", keep_count=2)
    old = Path(tmp_dir) / "old.mpg"
    fresh = Path(tmp_dir) / "fresh.mpg"
    old.write_bytes(b"old")
    fresh.write_bytes(b"fresh")
    old_vod = service.add_vod(
        old, creator_key=creator["creator_key"], twitch_id="90000",
        title="Old VOD", duration_ms=10_000, source_type="twitch",
    )
    monkeypatch.setattr(
        service, "_creator_vod_urls", lambda _creator, _limit: ["10001", "10002"]
    )

    def import_one(url, creator_key=""):
        if url.endswith("10001"):
            raise ValueError("network error")
        return service.add_vod(
            fresh, creator_key=creator_key, twitch_id="10002",
            title="Fresh VOD", duration_ms=10_000, source_type="twitch",
        )

    monkeypatch.setattr(service, "import_url", import_one)
    report = service.refresh_creators()

    assert report["download_failures"] == 1
    assert report["vods_added"] == 1
    assert service.get_vod(old_vod["id"]) is not None
    assert service.get_vod("tw_10002") is not None


def test_twitch_app_menu_assets_and_6g_player_contract():
    plugin = (ROOT / "apps/plugins/twitch.c").read_text()
    mpeg = (ROOT / "apps/plugins/mpegplayer/mpegplayer.c").read_text()
    mpeg_video = (
        ROOT / "apps/plugins/mpegplayer/video_out_rockbox.c"
    ).read_text()
    openh264 = (ROOT / "apps/plugins/openh264_player.c").read_text()
    hardware_6g = (ROOT / "apps/video_playback.c").read_text()
    menu = (ROOT / "apps/root_menu.c").read_text()
    categories = (ROOT / "apps/plugins/CATEGORIES").read_text().splitlines()
    sources = (ROOT / "assets/ipodjs/sources/twitch/SOURCES.md").read_text()
    icon_path = ROOT / "assets/ipodjs/rockbox/applications/twitch.46x46x24.bmp"

    assert 'TW_LIVE_PREFIX      "twitch-live:"' in plugin
    assert "tw_current_vod" in plugin
    assert "button_queue_count() != 0" in plugin
    assert 'PLUGIN_APPS_DIR "/twitch.rock"' in menu
    assert "twitch,apps" in categories
    assert '{ "Twitch", launch_twitch_plugin, "twitch.46x46x24.bmp" }' in menu
    assert '&youtube_item, &twitch_item' in menu
    assert 'MPEGPLAYER_TWITCH_LIVE_PREFIX "twitch-live:"' in mpeg
    assert '"continue:%s", mpegplayer_twitch_creator' in mpeg
    assert '"twitch-live:", 12' in openh264
    assert "VIDEO_STYLE_TWITCH_LIVE" in hardware_6g
    assert "video_twitch_icon_valid" in hardware_6g
    assert "Wikimedia Commons" in sources
    assert icon_path.is_file()
    with Image.open(icon_path).convert("RGB") as icon:
        colors = {color for _count, color in icon.getcolors(46 * 46)}
        assert icon.getpixel((0, 0)) == (255, 0, 255)
        assert icon.getpixel((23, 23)) == (255, 255, 255)
        assert (255, 255, 255) in colors
        assert (0, 0, 0) not in colors
    assert " · " not in plugin
    assert "TW_PURPLE_DARK" in plugin
    assert "lcd_set_drawmode(DRMODE_FG)" in plugin
    assert "twitch-glitch-current.18x20.bmp" in mpeg
    assert "twitch-glitch-current.18x20.bmp" in hardware_6g
    assert "width, height, 60, 17, width - 68, 4" in mpeg
    assert "59, LCD_HEIGHT - 22" in hardware_6g
    assert '".twc"' in mpeg
    assert '".twe"' in mpeg
    assert "twitch_chat.visible = !twitch_chat.visible" in mpeg
    assert "twitch_chat_yuv_draw" in mpeg
    assert "TWITCH_EMOJI_CACHE 32" in mpeg
    assert "TWITCH_CHAT_ANIMATION_TICKS" in mpeg
    assert "TWITCH_CHAT_EASE_SCALE" in mpeg
    assert "input_wait = MAX(1, HZ / 50)" in mpeg
    assert "mpegplayer_twitch_chat_video_rect" in mpeg
    assert "mpegplayer_twitch_chat_video_rect" in mpeg_video
    assert "chat->visible = !chat->visible" in hardware_6g
    assert "video_twitch_chat_draw" in hardware_6g
    assert "VIDEO_TWITCH_EMOJI_CACHE 32" in hardware_6g
    assert "VIDEO_TWITCH_CHAT_ANIMATION_TICKS" in hardware_6g
    assert "VIDEO_TWITCH_CHAT_EASE_SCALE" in hardware_6g
    assert "content_width = MAX(2, LCD_WIDTH - chat->panel_width)" in hardware_6g
    assert 'VIEWERS_DIR "/mpegplayer.rock", fallback_launch' in openh264
    assert 'rb->splash(HZ, "Using MPEG fallback")' in openh264
    assert 'splash(HZ * 2, "H.264 audio decode failed")' in hardware_6g


@pytest.mark.skipif(
    not os.environ.get("ROCKPOD_TWITCH_REAL_VOD_FIXTURE"),
    reason="set ROCKPOD_TWITCH_REAL_VOD_FIXTURE for a real Twitch excerpt",
)
def test_real_twitch_vod_excerpt_exports_for_6g(
    db, config, mock_device
):
    source = Path(os.environ["ROCKPOD_TWITCH_REAL_VOD_FIXTURE"])
    service = TwitchAppService(db, config, ROOT)
    creator = service.add_creator("Emma", "Emma", cycle_epoch=100)
    vod = service.add_vod(
        source,
        creator_key=creator["creator_key"],
        twitch_id="2860855364",
        title="MMA STREAM",
        game="IRL",
        duration_ms=15_000,
        published_date="2026-08-30",
        source_url=TEST_VOD_URL,
        source_type="twitch",
    )
    report = service.sync(
        mock_device,
        device=SimpleNamespace(mount_path=mock_device, rockbox_target="ipod6g"),
        refresh_creators=False,
        video_profile="quality",
    )
    output = Path(mock_device) / "Twitch/videos" / f"{vod['id']}.mpg"
    probe = subprocess.run(
        [
            config.get("ffprobe_binary", "ffprobe"), "-v", "error",
            "-show_entries", "stream=codec_name,profile,width,height",
            "-of", "default=noprint_wrappers=1", str(output),
        ],
        check=True,
        capture_output=True,
        text=True,
    ).stdout

    assert report["media_updated"] == 1
    assert output.is_file()
    assert "codec_name=mpeg2video" in probe
    assert "codec_name=mp2" in probe
    assert "width=320" in probe
