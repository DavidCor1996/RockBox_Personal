from pathlib import Path

from services.app_video_sync import app_video_profile, stage_app_video


ROOT = Path(__file__).resolve().parents[2]


def test_mpeg_only_apps_ignore_global_and_requested_h264():
    config = {"video_sync_profile": "h264_apple_exact"}

    for source_app in ("livetv", "tiktok", "onlyfans"):
        assert app_video_profile(config, source_app=source_app) == "quality"
        assert app_video_profile(
            config, "h264_apple_exact", source_app=source_app
        ) == "quality"


def test_netflix_youtube_and_twitch_keep_the_new_h264_profile():
    config = {"video_sync_profile": "h264_apple_exact"}

    assert (
        app_video_profile(config, source_app="netflix") == "h264_apple_exact"
    )
    assert (
        app_video_profile(config, source_app="youtube") == "h264_apple_exact"
    )
    assert (
        app_video_profile(config, source_app="twitch") == "h264_apple_exact"
    )
    assert app_video_profile(
        config, "quality", source_app="twitch"
    ) == "quality"


def test_native_apps_route_only_supported_sources_to_h264():
    tiktok = (ROOT / "apps/plugins/ipodtiktok.c").read_text(encoding="utf-8")
    onlyfans = (ROOT / "apps/plugins/onlyfans.c").read_text(encoding="utf-8")
    youtube = (ROOT / "apps/plugins/youtube.c").read_text(encoding="utf-8")
    netflix = (ROOT / "apps/root_menu.c").read_text(encoding="utf-8")
    netflix_desktop = (
        ROOT / "apps/plugins/netflix_desktop.c"
    ).read_text(encoding="utf-8")

    assert "plugin_open(IPODTIKTOK_PLAYER_PATH, launch_param)" in tiktok
    assert (
        '#define IPODTIKTOK_PLAYER_PATH   VIEWERS_DIR "/mpegplayer.rock"'
        in tiktok
    )
    assert "plugin_open(OF_PLAYER, launch)" in onlyfans
    assert '#define OF_PLAYER       VIEWERS_DIR "/mpegplayer.rock"' in onlyfans
    assert "plugin_video_player_for(path)" in youtube
    assert "#if defined(IPOD_6G) || defined(IPOD_VIDEO)" in netflix
    assert "#if defined(IPOD_6G) || defined(IPOD_VIDEO)" in netflix_desktop


def test_classic_livetv_device_paths_are_mpeg():
    source = (ROOT / "rockpod/services/livetv.py").read_text(encoding="utf-8")

    assert 'return f"ads/{stem}.mpg"' in source
    assert (
        'return f"shows/{_safe_name(self.series, \'series\')}/{stem}.mpg"'
        in source
    )
    assert "def ensure_mpeg(self, item: LiveTvMedia) -> str:" in source


def test_mpeg_staging_caches_completed_conversion(
    tmp_path, monkeypatch
):
    source = tmp_path / "source.mp4"
    first = tmp_path / "first.mpg"
    second = tmp_path / "second.mpg"
    source.write_bytes(b"source")
    calls = []

    def convert(command, check):
        assert check is True
        calls.append(command)
        Path(command[-1]).write_bytes(b"converted-mpeg")

    monkeypatch.setattr("services.app_video_sync.subprocess.run", convert)
    config = {"cache_dir": str(tmp_path / "cache"), "ffmpeg_binary": "ffmpeg"}
    kwargs = {
        "config": config,
        "profile": "quality",
        "cache_namespace": "twitch_video",
        "device_key": "device",
        "device_target": "ipod6g",
    }

    stage_app_video(source, first, **kwargs)
    stage_app_video(source, second, **kwargs)

    assert len(calls) == 1
    assert first.read_bytes() == b"converted-mpeg"
    assert second.read_bytes() == b"converted-mpeg"
