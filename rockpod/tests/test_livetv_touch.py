from pathlib import Path
from types import SimpleNamespace

import pytest

from services.livetv_touch import (
    GUIDE_PALETTE, LiveTvTouchCatalog, LiveTvTouchError, LiveTvTouchJobs,
)


class ConfigStub:
    def __init__(self, root):
        self.cache_dir = str(root / "cache")
        self.ffmpeg_binary = "ffmpeg-test"


class FakeMedia:
    def __init__(self, path, relative, title="Local Forecast"):
        self.path = str(path)
        self.key = self.path
        self.title = title
        self._relative = relative

    def device_relative(self):
        return self._relative


class FakeLibrary:
    def __init__(self, config):
        self.config = config

    def scan(self, probe_durations=True):
        assert probe_durations
        return list(self.config.shows), list(self.config.ads)


class FakeLineup:
    def __init__(self, library):
        self.channels = list(library.config.channels)

    def autobuild(self, shows, ads):
        raise AssertionError("the fixture already has a lineup")

    def save(self):
        pass


class FakeSync:
    def __init__(self, library):
        self.config = library.config

    def reconcile_missing_sources(self, lineup, shows, ads):
        return shows, ads

    def ensure_mpeg(self, item):
        return item.path


class FakeScheduler:
    def __init__(self, lineup, shows, ads):
        self.config = lineup.channels[0]._config

    def build(self):
        return list(self.config.slots)


def fake_module(config):
    for channel in config.channels:
        channel._config = config
    return SimpleNamespace(
        LIVETV_WEATHER_CATEGORY="Weather",
        LiveTvLibrary=FakeLibrary,
        LiveTvLineup=FakeLineup,
        LiveTvSync=FakeSync,
        LiveTvScheduler=FakeScheduler,
        ensure_weather_channel=lambda sync, lineup, cfg, shows, ads: shows,
    )


def fixture(tmp_path):
    config = ConfigStub(tmp_path)
    weather_file = tmp_path / "weather.mpg"
    usa_file = tmp_path / "usa.mpg"
    ytv_file = tmp_path / "ytv.mpg"
    other_file = tmp_path / "other.mpg"
    weather_file.write_bytes(b"classic weather")
    usa_file.write_bytes(b"classic usa")
    ytv_file.write_bytes(b"classic ytv")
    other_file.write_bytes(b"classic other")
    config.shows = [
        FakeMedia(weather_file, "shows/Weather/weather.mpg"),
        FakeMedia(usa_file, "shows/USA/usa.mpg", "Monday Night Raw"),
        FakeMedia(ytv_file, "shows/YTV/ytv.mpg", "YTV Show"),
        FakeMedia(other_file, "shows/Other/other.mpg", "Other Show"),
    ]
    config.ads = []
    config.channels = [
        SimpleNamespace(number=900, callsign="WX", name="Weather",
                        category="Weather", favourite=True,
                        parental_locked=False),
        SimpleNamespace(number=103, callsign="USA", name="USA Network",
                        category="Series", favourite=True,
                        parental_locked=False),
        SimpleNamespace(number=108, callsign="YTV", name="YTV",
                        category="Series", favourite=True,
                        parental_locked=False),
        SimpleNamespace(number=100, callsign="TEST", name="Test",
                        category="Series", favourite=True,
                        parental_locked=False),
    ]
    config.slots = [
        SimpleNamespace(channel=900, day=0, start=0, duration=3600,
                        kind="S", title="Local Forecast", rating="TV-G",
                        description="Continuous local weather.",
                        block_start=0, block_duration=3600,
                        path="shows/Weather/weather.mpg"),
        SimpleNamespace(channel=103, day=0, start=0, duration=7200,
                        kind="S", title="Monday Night Raw", rating="TV-14",
                        description="USA Network programming.",
                        block_start=0, block_duration=7200,
                        path="shows/USA/usa.mpg"),
        SimpleNamespace(channel=108, day=0, start=0, duration=1800,
                        kind="S", title="YTV Show", rating="TV-Y7",
                        description="YTV programming.",
                        block_start=0, block_duration=1800,
                        path="shows/YTV/ytv.mpg"),
        SimpleNamespace(channel=100, day=0, start=0, duration=1800,
                        kind="S", title="Other Show", rating="TV-PG",
                        description="Not in the forecast test.",
                        block_start=0, block_duration=1800,
                        path="shows/Other/other.mpg"),
    ]
    return config


def test_forecast_manifest_reuses_scheduler_and_opaque_cached_media(tmp_path):
    config = fixture(tmp_path)
    commands = []

    def encode(command, **kwargs):
        commands.append(command)
        Path(command[-1]).write_bytes(b"touch mp4")
        return SimpleNamespace(returncode=0, stdout="", stderr="")

    catalog = LiveTvTouchCatalog(
        config, livetv_module=fake_module(config), command_runner=encode
    )
    first = catalog.prepare("forecast")
    second = catalog.prepare("forecast")

    assert [channel["callsign"] for channel in first["channels"]] == ["WX"]
    assert {slot["channel"] for slot in first["slots"]} == {900}
    assert len(first["media"]) == 1
    assert len(first["media"][0]["id"]) == 32
    assert "weather.mpg" not in str(first)
    assert first["media"][0]["download_url"].startswith("/v1/livetv/media/")
    assert first["palette"] == GUIDE_PALETTE
    assert first["revision"] == second["revision"]
    assert len(commands) == 1, "the second sync must reuse the touch cache"
    assert "baseline" in commands[0] and "+faststart" in commands[0]
    assert catalog.media_path(first["media"][0]["id"])
    assert catalog.media_path("../../etc/passwd") is None


def test_all_scope_publishes_both_channels(tmp_path):
    config = fixture(tmp_path)

    def encode(command, **kwargs):
        Path(command[-1]).write_bytes(b"touch mp4")
        return SimpleNamespace(returncode=0, stdout="", stderr="")

    manifest = LiveTvTouchCatalog(
        config, livetv_module=fake_module(config), command_runner=encode
    ).prepare("all")
    assert {channel["number"] for channel in manifest["channels"]} == {100, 103, 108, 900}
    assert {slot["channel"] for slot in manifest["slots"]} == {100, 103, 108, 900}
    assert len(manifest["media"]) == 4


def test_featured_scope_publishes_weather_usa_and_ytv(tmp_path):
    config = fixture(tmp_path)

    def encode(command, **kwargs):
        Path(command[-1]).write_bytes(b"touch mp4")
        return SimpleNamespace(returncode=0, stdout="", stderr="")

    manifest = LiveTvTouchCatalog(
        config, livetv_module=fake_module(config), command_runner=encode
    ).prepare("featured")
    assert [channel["callsign"] for channel in manifest["channels"]] == [
        "WX", "USA", "YTV"
    ]
    assert {slot["channel"] for slot in manifest["slots"]} == {103, 108, 900}
    assert len(manifest["media"]) == 3


def test_failed_transcode_is_not_published(tmp_path):
    config = fixture(tmp_path)

    def fail(command, **kwargs):
        return SimpleNamespace(returncode=1, stdout="", stderr="encoder broke")

    catalog = LiveTvTouchCatalog(
        config, livetv_module=fake_module(config), command_runner=fail
    )
    with pytest.raises(LiveTvTouchError, match="could not prepare weather.mpg"):
        catalog.prepare("forecast")
    assert catalog.latest_manifest("forecast") is None


def test_jobs_return_existing_manifest_without_repreparing():
    manifest = {"scope": "featured", "media": [{"id": "one"}, {"id": "two"}]}

    class Catalog:
        def latest_manifest(self, scope):
            assert scope == "featured"
            return manifest

    jobs = LiveTvTouchJobs(Catalog())
    job, created = jobs.create("featured")
    assert created is False
    assert job["state"] == "ready"
    assert job["done"] == job["total"] == 2
    assert job["manifest"] is manifest
    assert jobs.pending.empty()
