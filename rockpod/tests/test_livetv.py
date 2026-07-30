"""Live TV scheduling, scanning and device-format tests.

The whole illusion depends on the PC guide, the iPod guide and the iPod
player resolving the same instant to the same programme, so most of these
tests are about determinism and about the file format both sides parse.
"""

import json
from pathlib import Path

import pytest

from services import livetv
from services.livetv import (
    LiveTvChannel,
    LiveTvLineup,
    LiveTvMedia,
    LiveTvScheduler,
    clean_title,
    max_two_day_slot_count,
    program_title,
    resolve_slot,
    series_name,
    write_channels_tsv,
    write_guide_tsv,
)

REPO = Path(__file__).parents[2]
GUIDE_C = REPO / "apps" / "plugins" / "mpegplayer" / "livetv_guide.c"
GUIDE_H = REPO / "apps" / "plugins" / "mpegplayer" / "livetv.h"
PLAYER_C = REPO / "apps" / "plugins" / "mpegplayer" / "mpegplayer.c"
VIDEO_OUT_C = (
    REPO / "apps" / "plugins" / "mpegplayer" / "video_out_rockbox.c"
)


def test_live_tv_volume_uses_owned_green_crt_overlay():
    source = PLAYER_C.read_text(encoding="utf-8")
    video_out = VIDEO_OUT_C.read_text(encoding="utf-8")

    assert "LIVETV_VOLUME_GREEN" in source
    assert "LIVETV_VOLUME_SEGMENTS 16" in source
    assert "LIVETV_VOLUME_BAR_W 112" in source
    assert "static void livetv_volume_show(void)" in source
    assert "mpegplayer_yuv_overlay_draw" in source
    assert "vo_draw_yuv_overlay" in video_out
    assert "int group_x = (width - group_w) / 2;" in source
    assert '"VOLUME", 2, 32, 255, 80' in source
    assert "percent >= 100 ? LIVETV_VOLUME_SEGMENTS" in source
    assert "global_settings->volume_limit" in source
    assert "int overlay_top = mpegplayer_yuv_overlay_y();" in video_out
    assert "livetv_volume_post_frame_callback" not in source
    volume_show = source.rindex("static void livetv_volume_show(void)")
    assert "lcd_update_rect" not in source[
        volume_show :
        source.index("static void livetv_overlay_show(bool mini)", volume_show)
    ]
    assert "livetv_volume_until" in source
    assert (
        "if (!mpegplayer_livetv_desktop)\n"
        "            livetv_volume_show();"
    ) in source


def test_weather_snow_insert_selection_uses_nearest_hour(tmp_path):
    from services.weather import WEATHER_REL_DIR

    class Config:
        cache_dir = str(tmp_path)

    forecast = tmp_path / "weather" / WEATHER_REL_DIR / "forecast.tsv"
    forecast.parent.mkdir(parents=True)
    now = livetv.datetime.now().replace(minute=0, second=0, microsecond=0)
    forecast.write_text(
        "rockpod_weather_v1\tTest\n"
        f"hourly\t{now.isoformat(timespec='minutes')}\tsnow\tSnow\n",
        encoding="utf-8",
    )

    assert livetv.weather_current_code(Config()) == "snow"
    assert livetv.weather_condition_is_snow("freezing_rain")
    assert livetv.weather_condition_is_snow("sleet")
    assert not livetv.weather_condition_is_snow("partly_cloudy")
    assert livetv.weather_interstitial_kinds(
        "clear", livetv.datetime(2026, 7, 15)) == ("summer", "general")
    assert livetv.weather_interstitial_kinds(
        "snow", livetv.datetime(2026, 7, 15)) == ("snow", "general")
    assert livetv.weather_interstitial_kinds(
        "clear", livetv.datetime(2026, 10, 15)) == ("general",)


def test_weather_rotation_drops_byte_identical_clips(tmp_path):
    first = tmp_path / "first.mp4"
    duplicate = tmp_path / "renamed-copy.mp4"
    distinct = tmp_path / "distinct.mp4"
    first.write_bytes(b"same clip")
    duplicate.write_bytes(b"same clip")
    distinct.write_bytes(b"different clip")

    unique, duplicates = livetv._weather_unique_media(
        [str(first), str(duplicate), str(distinct)])

    assert unique == [str(first), str(distinct)]
    assert duplicates == [str(duplicate)]


def _write_weather_selector_forecast(tmp_path, now, condition, temperature,
                                     high, precipitation):
    from services.weather import WEATHER_REL_DIR

    forecast = tmp_path / "cache" / "weather" / WEATHER_REL_DIR / "forecast.tsv"
    forecast.parent.mkdir(parents=True)
    forecast.write_text(
        "\t".join([
            "rockpod_weather_v1", "Test, NB", "0", "0", "Test/Local",
            "2026-07-29T00:00:00Z", now.date().isoformat(), "metric",
        ]) + "\n" +
        "\t".join([
            now.date().isoformat(), condition, condition.title(), "14",
            str(high), str(precipitation), "8", "60", "06:00", "20:00",
            "test",
        ]) + "\n" +
        "\t".join([
            "hourly", now.isoformat(timespec="minutes"), condition,
            condition.title(), str(temperature), str(precipitation), "8",
            "60", "1", "test",
        ]) + "\n",
        encoding="utf-8",
    )


def test_weather_sync_keeps_carissa_and_only_activates_matching_solo_women(
        tmp_path):
    class Config:
        cache_dir = str(tmp_path / "cache")
        livetv_weather_assets_dir = str(tmp_path / "assets")

    now = livetv.datetime(2026, 7, 29, 14, 0)
    _write_weather_selector_forecast(
        tmp_path, now, "cloudy", 24, 29, 2)

    summer = (
        Path(Config.livetv_weather_assets_dir) / "interstitials" / "summer")
    summer.mkdir(parents=True)
    carissa = summer / "carissa-june-anchor-weather-read.mp4"
    carissa.write_bytes(b"carissa")

    conditional = (
        Path(Config.livetv_weather_assets_dir) /
        "interstitials" / "conditional")
    conditional.mkdir(parents=True)
    for name in ("mild.mp4", "rain.mp4", "hot.mp4", "male.mp4"):
        (conditional / name).write_bytes(name.encode("ascii"))
    (conditional / "clips.json").write_text(json.dumps({
        "version": 1,
        "clips": [
            {
                "file": "mild.mp4",
                "presenter": "Solo Woman",
                "solo_woman": True,
                "conditions": ["clear", "partly_cloudy", "cloudy"],
                "months": [6, 7, 8],
                "min_temp_c": 18,
                "max_temp_c": 27,
                "max_precipitation": 25,
                "priority": 90,
            },
            {
                "file": "rain.mp4",
                "presenter": "Rain Presenter",
                "solo_woman": True,
                "conditions": ["rain", "thunder"],
                "min_precipitation": 40,
                "priority": 100,
            },
            {
                "file": "hot.mp4",
                "presenter": "Hot Presenter",
                "solo_woman": True,
                "conditions": ["clear", "partly_cloudy"],
                "min_high_c": 30,
                "priority": 80,
            },
            {
                "file": "male.mp4",
                "presenter": "Mixed Segment",
                "solo_woman": False,
                "conditions": ["cloudy"],
                "priority": 999,
            },
        ],
    }), encoding="utf-8")

    selected, report = livetv.select_weather_interstitials(
        Config(), now=now)

    assert selected == [str(carissa), str(conditional / "mild.mp4")]
    assert report["forecast"]["condition_group"] == "cloudy"
    assert report["forecast"]["temperature_c"] == 24
    male = next(row for row in report["decisions"]
                if row["presenter"] == "Mixed Segment")
    assert not male["active"]
    assert "solo-woman" in male["reason"]


def test_weather_sync_swaps_dry_guest_for_rain_guest(tmp_path):
    class Config:
        cache_dir = str(tmp_path / "cache")
        livetv_weather_assets_dir = str(tmp_path / "assets")

    now = livetv.datetime(2026, 7, 29, 14, 0)
    _write_weather_selector_forecast(
        tmp_path, now, "rain", 21, 23, 75)

    summer = (
        Path(Config.livetv_weather_assets_dir) / "interstitials" / "summer")
    summer.mkdir(parents=True)
    rainy_carissa = summer / "carissa-august-rain-report.mp4"
    rainy_carissa.write_bytes(b"carissa")
    (summer / "carissa-june-anchor-weather-read.mp4").write_bytes(b"dry")

    conditional = (
        Path(Config.livetv_weather_assets_dir) /
        "interstitials" / "conditional")
    conditional.mkdir(parents=True)
    dry = conditional / "dry.mp4"
    rain = conditional / "rain.mp4"
    dry.write_bytes(b"dry")
    rain.write_bytes(b"rain")
    (conditional / "clips.json").write_text(json.dumps({
        "clips": [
            {
                "file": "dry.mp4",
                "presenter": "Dry Presenter",
                "solo_woman": True,
                "conditions": ["clear", "cloudy"],
                "max_precipitation": 25,
            },
            {
                "file": "rain.mp4",
                "presenter": "Rain Presenter",
                "solo_woman": True,
                "conditions": ["rain", "thunder"],
                "min_precipitation": 40,
            },
        ],
    }), encoding="utf-8")

    selected, _report = livetv.select_weather_interstitials(
        Config(), now=now)

    assert selected == [
        str(rainy_carissa),
        str(rain),
        str(summer / "carissa-june-anchor-weather-read.mp4"),
    ]
    assert str(dry) not in selected


def test_weather_clip_allows_brief_male_intro_to_woman_presenter():
    forecast = {
        "current": {
            "code": "rain",
            "temperature_c": 20,
            "precipitation": 75,
        },
        "today": {
            "code": "rain",
            "high_c": 23,
            "precipitation": 75,
        },
    }
    matched, _reason = livetv._weather_conditional_rule_matches({
        "solo_woman": False,
        "male_role": "introduction",
        "conditions": ["rain"],
        "min_precipitation": 60,
    }, forecast, livetv.datetime(2026, 7, 29))

    assert matched


@pytest.mark.parametrize(
    ("units", "temperature", "low", "high", "wind", "expected"),
    [
        ("metric", "19", "12", "23", "14", (19, 12, 23, 14)),
        ("imperial", "68", "50", "77", "10", (20, 10, 25, 16)),
    ],
)
def test_weather_presenter_forecast_is_always_celsius(
        tmp_path, units, temperature, low, high, wind, expected):
    from services.weather import WEATHER_REL_DIR

    class Config:
        cache_dir = str(tmp_path)

    forecast = tmp_path / "weather" / WEATHER_REL_DIR / "forecast.tsv"
    forecast.parent.mkdir(parents=True)
    now = livetv.datetime.now().replace(minute=0, second=0, microsecond=0)
    forecast.write_text(
        "\t".join([
            "rockpod_weather_v1", "Test, NB", "0", "0", "Test/Local",
            "2026-01-01T00:00:00Z", now.date().isoformat(), units,
        ]) + "\n" +
        "\t".join([
            now.date().isoformat(), "clear", "Clear", low, high, "25",
            wind, "90", "06:00", "20:00", "test",
        ]) + "\n" +
        "\t".join([
            "hourly", now.isoformat(timespec="minutes"), "clear", "Clear",
            temperature, "20", wind, "90", "1", "test",
        ]) + "\n",
        encoding="utf-8",
    )

    parsed = livetv.weather_forecast_for_presenter(Config(), now=now)
    current_c, low_c, high_c, wind_kmh = expected
    assert parsed["location"] == "Test, NB"
    assert parsed["current"]["temperature_c"] == current_c
    assert parsed["today"]["low_c"] == low_c
    assert parsed["today"]["high_c"] == high_c
    assert parsed["current"]["wind_kmh"] == wind_kmh
    assert parsed["current"]["wind_direction"] == "E"


def test_weather_presenter_renderer_uses_clean_backplates(tmp_path):
    from PIL import Image
    from services.weather import WEATHER_REL_DIR

    class Config:
        cache_dir = str(tmp_path / "cache")
        livetv_weather_assets_dir = str(tmp_path / "assets")

    now = livetv.datetime.now().replace(minute=0, second=0, microsecond=0)
    forecast = (
        Path(Config.cache_dir) / "weather" / WEATHER_REL_DIR / "forecast.tsv")
    forecast.parent.mkdir(parents=True)
    forecast.write_text(
        "\t".join([
            "rockpod_weather_v1", "Test, NB", "0", "0", "Test/Local",
            "2026-01-01T00:00:00Z", now.date().isoformat(), "metric",
        ]) + "\n" +
        "\t".join([
            now.date().isoformat(), "cloudy", "Cloudy", "10", "21", "30",
            "12", "135", "06:00", "20:00", "test",
        ]) + "\n" +
        "\t".join([
            "hourly", now.isoformat(timespec="minutes"), "cloudy", "Cloudy",
            "18", "30", "12", "135", "1", "test",
        ]) + "\n",
        encoding="utf-8",
    )
    backplates = Path(Config.livetv_weather_assets_dir) / "presenter-backplates"
    backplates.mkdir(parents=True)
    Image.new("RGB", (1448, 1086), "#123456").save(
        backplates / "current-conditions.png")

    rendered = livetv.render_weather_presenters(
        Config(), str(tmp_path / "rendered"))

    assert len(rendered) == 1
    assert rendered[0].endswith("current-conditions-celsius.png")
    with Image.open(rendered[0]) as image:
        assert image.size == (1448, 1086)
        assert image.getpixel((100, 150)) != (18, 52, 86)

    transitions = livetv.render_weather_transition_slates(
        Config(), str(tmp_path / "transitions"))
    assert len(transitions) == 2
    with Image.open(transitions[0]) as image:
        assert image.size == (640, 480)
        assert image.getpixel((10, 10)) != image.getpixel((10, 450))


def test_weather_icons_use_dimensional_broadcast_atlas():
    from PIL import Image

    atlas = (
        REPO / "rockpod" / "assets" / "weather" / "source" /
        "broadcast-weather-atlas-v2.png")
    generator = (REPO / "tools" / "generate_weather_icons.py").read_text(
        encoding="utf-8")
    assert atlas.is_file()
    assert "broadcast-weather-atlas-v2.png" in generator
    assert "ImageDraw" not in generator

    kinds = (
        "clear_day", "clear_night", "partly_cloudy", "cloudy", "rain",
        "drizzle", "snow", "fog", "thunderstorm", "unknown",
    )
    images = set()
    for size in (40, 64):
        for kind in kinds:
            path = (
                REPO / "rockpod" / "assets" / "weather" / "icons" /
                f"{kind}.{size}x{size}x24.bmp")
            with Image.open(path) as image:
                assert image.size == (size, size)
                assert image.mode == "RGB"
                red, green, blue = image.getpixel((0, 0))
                assert red >= 220 and blue >= 220 and green < 60
                images.add(image.tobytes())
    assert len(images) == len(kinds) * 2


def test_weather_icon_install_replaces_same_size_old_art(tmp_path,
                                                         monkeypatch):
    source = tmp_path / "source"
    source.mkdir()
    (source / "clear_day.40x40x24.bmp").write_bytes(b"N" * 64)
    target = (
        tmp_path / "device" / ".rockbox" / "rockpod" / "weather" / "icons")
    target.mkdir(parents=True)
    installed = target / "clear_day.40x40x24.bmp"
    installed.write_bytes(b"O" * 64)
    assert installed.stat().st_size == (
        source / "clear_day.40x40x24.bmp").stat().st_size

    monkeypatch.setattr(livetv, "_weather_icons_source_dir",
                        lambda: str(source))
    copied = livetv.install_weather_icons(str(tmp_path / "device"))

    assert copied == 1
    assert installed.read_bytes() == b"N" * 64


def test_weather_channel_uses_only_condition_and_season_specific_ads(
        monkeypatch):
    class Config:
        pass

    ads = [
        LiveTvMedia(
            path="/video/Live/ADS/Weather/summer/heat.mp4",
            kind="ad", title="Heat", series="Commercials", duration=30),
        LiveTvMedia(
            path="/video/Live/ADS/Weather/general/warnings.mp4",
            kind="ad", title="Warnings", series="Commercials", duration=30),
        LiveTvMedia(
            path="/video/Live/ADS/Weather/snow/winter.mp4",
            kind="ad", title="Winter", series="Commercials", duration=30),
        LiveTvMedia(
            path="/video/Live/ADS/soda.mp4",
            kind="ad", title="Soda", series="Commercials", duration=30),
    ]
    monkeypatch.setattr(livetv, "weather_current_code", lambda _config: "clear")
    assert livetv._weather_ad_keys(
        Config(), ads, now=livetv.datetime(2026, 7, 15)) == [ads[0].key]
    assert livetv._weather_ad_keys(
        Config(), ads, now=livetv.datetime(2026, 10, 15)) == [ads[1].key]

    monkeypatch.setattr(livetv, "weather_current_code", lambda _config: "snow")
    assert livetv._weather_ad_keys(
        Config(), ads, now=livetv.datetime(2026, 7, 15)) == [ads[2].key]

    normal = LiveTvChannel(
        number=100, callsign="TV", name="Television", category="Series")
    weather = LiveTvChannel(
        number=102, callsign="WX", name="Weather", category="Weather",
        ads=[ads[0].key])
    scheduler = LiveTvScheduler(_Lineup([normal, weather]), [], ads)
    assert scheduler._channel_ads(normal) == [ads[3]]
    assert scheduler._channel_ads(weather) == [ads[0]]

    weather.ads = []
    assert scheduler._channel_ads(weather) == []


def test_weather_program_clock_exposes_video_and_preserves_ads():
    guide = GUIDE_C.read_text(encoding="utf-8")
    player = PLAYER_C.read_text(encoding="utf-8")
    host = Path(livetv.__file__).read_text(encoding="utf-8")

    assert "return true;" in guide
    assert "slot->kind == LIVETV_KIND_SHOW" in guide
    assert livetv.LIVETV_WEATHER_FLOW_PROFILE == "broadcast-flow-v5"
    assert '"VIEWER COMMENTS"' in host
    assert "for cycle in range(57)" in host
    assert "regular_clocks[" in host
    assert (
        "[5:v]scale=320:240:force_original_aspect_ratio=increase,"
        in host
    )
    assert "crop=320:240,fps=20" in host
    assert "trim=duration=16,setpts=PTS-STARTPTS[v5base]" in host
    assert "apad=pad_dur=16,atrim=duration=16" in host
    assert "atrim=duration=48[apre]" in host
    assert "volume='if(lt(mod(t,64),2),mod(t,64)*0.5," in host
    assert "if(lt(mod(t,64),48)," in host
    assert "1-(mod(t,64)-46)*0.5,0)" in host
    assert "[music][report]amix=inputs=2:" in host
    assert "alimiter=limit=0.95[aout]" in host
    assert "ribbon_bg = LCD_RGBPACK(12, 68, 126)" in guide
    assert "full-height wipe" in guide
    assert "livetv_weather_program_active()" in player
    assert player.count(
        "livetv_update_weather_view(stream_get_time() / TS_SECOND, false)"
    ) == 1
    assert "uint32_t seconds = stream_get_time() / TS_SECOND;" in player
    assert (
        "livetv_update_weather_view(livetv_weather_program_seconds(), false)"
        not in player
    )
    video_out = VIDEO_OUT_C.read_text(encoding="utf-8")
    assert "mpegplayer_livetv_weather_hidden" in video_out
    assert "do not let fresh carrier frames overwrite" in video_out
    update = player.index("static void livetv_update_weather_view")
    loop = player.index("/* Run the guide without disturbing playback.", update)
    view_source = player[update:loop]
    assert "stream_open" not in view_source
    assert "osd_play" not in view_source
    assert "stream_seek" not in view_source
    assert "stream_vo_set_display_mode" not in view_source


class _Lineup:
    """A line-up without the on-disk state file."""

    def __init__(self, channels, seed=20051115, ad_min=2, ad_max=3):
        self.channels = channels
        self.seed = seed
        self.ad_break_min = ad_min
        self.ad_break_max = ad_max

    def ensure_unique_numbers(self):
        return False


def _media(path, kind, duration, title=None, series="Test Series"):
    return LiveTvMedia(
        path=path,
        kind=kind,
        title=title or Path(path).stem,
        series=series,
        duration=duration,
        rating="TV-PG",
        description="Series, Test Series. Episode.",
    )


def _fixture():
    shows = [
        _media("/live/show-a.mp4", "show", 1500, "Show A"),
        _media("/live/show-b.mp4", "show", 1800, "Show B"),
    ]
    ads = [
        _media("/live/ADS/ad-1.mp4", "ad", 30, "Ad One", "Commercials"),
        _media("/live/ADS/ad-2.mp4", "ad", 15, "Ad Two", "Commercials"),
        _media("/live/ADS/ad-3.mp4", "ad", 45, "Ad Three", "Commercials"),
    ]
    channel = LiveTvChannel(
        number=100, callsign="TEST", name="Test Channel",
        shows=[item.path for item in shows], ads=[],
    )
    return _Lineup([channel]), shows, ads


def test_every_channel_day_is_filled_end_to_end():
    """A channel must be on the air for all 86400 seconds of every day."""
    lineup, shows, ads = _fixture()
    slots = LiveTvScheduler(lineup, shows, ads).build()

    for day in range(7):
        day_slots = sorted((s for s in slots if s.day == day),
                           key=lambda s: s.start)
        assert day_slots, f"day {day} has no programming"
        assert day_slots[0].start == 0
        cursor = 0
        for slot in day_slots:
            assert slot.start == cursor, "a gap or overlap in the schedule"
            cursor = slot.end
        assert cursor == livetv.LIVETV_DAY_SECONDS


def test_schedule_is_deterministic_across_rebuilds():
    """The same inputs must always produce the same listings."""
    lineup, shows, ads = _fixture()
    first = LiveTvScheduler(lineup, shows, ads).build()

    lineup2, shows2, ads2 = _fixture()
    second = LiveTvScheduler(lineup2, shows2, ads2).build()

    key = lambda slots: [(s.channel, s.day, s.start, s.duration, s.kind,
                          s.path) for s in slots]
    assert key(first) == key(second)


def test_each_show_is_followed_by_two_or_three_commercials():
    lineup, shows, ads = _fixture()
    slots = sorted(LiveTvScheduler(lineup, shows, ads).build(),
                   key=lambda s: (s.day, s.start))

    breaks = []
    run = 0
    for slot in slots:
        if slot.kind == "A":
            run += 1
        else:
            if run:
                breaks.append(run)
            run = 0
    if run:
        breaks.append(run)

    assert breaks, "no commercial breaks were scheduled"
    # The final break of a day may be cut short by midnight.
    assert all(1 <= length <= 3 for length in breaks)
    assert any(length in (2, 3) for length in breaks[:-1] or breaks)


def test_a_commercial_never_repeats_back_to_back():
    lineup, shows, ads = _fixture()
    slots = sorted(LiveTvScheduler(lineup, shows, ads).build(),
                   key=lambda s: (s.day, s.start))

    previous_ad = None
    for slot in slots:
        if slot.kind != "A":
            previous_ad = None
            continue
        assert slot.path != previous_ad
        previous_ad = slot.path


def test_commercials_inherit_the_programme_block():
    """A guide keeps naming the show through its commercial break."""
    lineup, shows, ads = _fixture()
    slots = LiveTvScheduler(lineup, shows, ads).build()

    by_block = {}
    for slot in slots:
        by_block.setdefault((slot.day, slot.block_start), []).append(slot)

    for members in by_block.values():
        titles = {slot.title for slot in members}
        assert len(titles) == 1, "a block advertised more than one title"
        lengths = {slot.block_duration for slot in members}
        assert len(lengths) == 1
        span = max(s.end for s in members) - min(s.start for s in members)
        assert span == members[0].block_duration


def test_resolve_slot_matches_the_schedule_at_any_instant():
    """The lookup the device performs must agree with the generated table."""
    import time

    lineup, shows, ads = _fixture()
    slots = LiveTvScheduler(lineup, shows, ads).build()

    now = time.time()
    for offset in (0, 61, 907, 3600, 7200, 43200, 86399):
        moment = now + offset
        slot, into = resolve_slot(slots, 100, moment)
        assert slot is not None
        local = time.localtime(moment)
        secs = local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec
        day = (local.tm_wday + 1) % 7
        assert slot.day == day
        assert slot.start <= secs < slot.end
        assert into == secs - slot.start


def test_channel_ad_pool_is_used_when_one_is_assigned():
    lineup, shows, ads = _fixture()
    lineup.channels[0].ads = [ads[0].path]
    slots = LiveTvScheduler(lineup, shows, ads).build()

    ad_paths = {slot.path for slot in slots if slot.kind == "A"}
    assert ad_paths == {ads[0].device_relative()}


def test_guide_tsv_layout_matches_what_the_device_parses(tmp_path):
    lineup, shows, ads = _fixture()
    slots = LiveTvScheduler(lineup, shows, ads).build()
    guide = tmp_path / "guide.tsv"
    write_guide_tsv(guide, slots)

    lines = [line for line in guide.read_text(encoding="utf-8").splitlines()
             if line and not line.startswith("#")]
    assert lines
    for line in lines:
        fields = line.split("\t")
        # livetv_guide.c reads eleven columns and rejects shorter rows.
        assert len(fields) == 11
        assert fields[4] in {"S", "A"}
        assert all(field != "" for field in fields)
        int(fields[0]); int(fields[1]); int(fields[2]); int(fields[3])
        int(fields[9]); int(fields[10])

    # Grouped by channel, then day, then start: the resolver relies on it.
    keys = [(int(f[0]), int(f[1]), int(f[2]))
            for f in (line.split("\t") for line in lines)]
    assert keys == sorted(keys)


def test_channels_tsv_layout_matches_what_the_device_parses(tmp_path):
    channels = [LiveTvChannel(number=100, callsign="TEST",
                              name="Test Channel", category="Series",
                              logo="", favourite=True)]
    path = tmp_path / "channels.tsv"
    write_channels_tsv(path, channels)

    rows = [line for line in path.read_text(encoding="utf-8").splitlines()
            if line and not line.startswith("#")]
    assert len(rows) == 1
    fields = rows[0].split("\t")
    assert len(fields) == 6
    assert fields[0] == "100" and fields[1] == "TEST"
    assert fields[5] in {"0", "1"}


def test_device_slot_cap_is_the_same_on_both_sides():
    header = GUIDE_H.read_text(encoding="utf-8")
    assert f"#define LIVETV_MAX_SLOTS        {livetv.LIVETV_DEVICE_MAX_SLOTS}" \
        in header


def test_two_day_slot_count_is_reported_for_the_device_budget():
    lineup, shows, ads = _fixture()
    slots = LiveTvScheduler(lineup, shows, ads).build()
    worst = max_two_day_slot_count(slots)
    per_day = len([s for s in slots if s.day == 0])
    assert worst >= per_day


def test_internet_archive_duplicates_are_collapsed(tmp_path):
    for name in ("clip.mp4", "clip.ia.mp4", "other.mp4"):
        (tmp_path / name).write_bytes(b"0" * 1024)
    paths = sorted(str(p) for p in tmp_path.glob("*.mp4"))
    kept = livetv._dedupe_internet_archive(paths)
    assert len(kept) == 2
    assert any(p.endswith("other.mp4") for p in kept)
    assert any(p.endswith("clip.mp4") for p in kept)
    assert not any(p.endswith("clip.ia.mp4") for p in kept)


@pytest.mark.parametrize("name", [
    "clip.mp4.part", "subs.vtt", "notes.txt", "thumb.jpg", "meta.json",
])
def test_sidecar_files_are_not_treated_as_media(name):
    assert not livetv._is_media_file(f"/live/{name}")


def test_series_and_titles_are_readable_in_a_guide():
    path = "/live/WWE_Monday_Night_Raw_2006_01_09_LQ.mp4"
    series = series_name(path, "/live")
    assert series == "WWE Monday Night Raw"
    title = program_title(path, series)
    assert title.startswith("WWE Monday Night Raw")
    assert "LQ" not in title
    assert clean_title("Show [1080p] x264") == "Show"


def test_a_title_never_repeats_its_own_series():
    """A file inside a folder of the same name printed the name twice."""
    series = "SpongeBob SquarePants Goes Prehistoric"
    title = program_title(
        f"/live/{series}/SpongeBob SquarePants Goes Prehistoric VHS.mp4",
        series)
    assert title.lower().count("spongebob") == 1, title
    assert len(title) <= 60


def test_a_folder_under_live_becomes_the_series():
    assert series_name("/live/ALF/s01e01.mkv", "/live") == "ALF"


def test_live_paths_are_excluded_from_the_ordinary_video_library():
    dirs = ["/home/u/Videos"]
    assert livetv.is_livetv_path("/home/u/Videos/Live/show.mp4", dirs)
    assert livetv.is_livetv_path("/home/u/Videos/Live/ADS/ad.mp4", dirs)
    assert not livetv.is_livetv_path("/home/u/Videos/Movie.mp4", dirs)
    assert not livetv.is_livetv_path("/home/u/Videos/Liverpool/x.mp4", dirs)


def test_library_scanner_skips_the_live_subtree():
    scanner = (REPO / "rockpod" / "services" / "library_scanner.py").read_text(
        encoding="utf-8")
    assert "LIVETV_SOURCE_DIR_NAME" in scanner
    assert "livetv_root" in scanner


def test_device_paths_stay_inside_the_livetv_folder():
    show = _media("/live/Some Show (1983).mp4", "show", 100)
    ad = _media("/live/ADS/An Ad!.mp4", "ad", 20, series="Commercials")
    assert show.device_relative().startswith("shows/")
    assert ad.device_relative().startswith("ads/")
    for relative in (show.device_relative(), ad.device_relative()):
        assert relative.endswith(".mpg")
        assert ".." not in relative
        assert not relative.startswith("/")


def test_guide_palette_is_the_sampled_directv_one():
    """The colours must stay the ones sampled from the receiver artwork."""
    source = GUIDE_C.read_text(encoding="utf-8")
    for name, rgb in (
        ("LIVETV_DESC_BG", "(2, 111, 175)"),
        ("LIVETV_HDR_BG", "(18, 37, 73)"),
        ("LIVETV_ROW_BG", "(9, 72, 113)"),
        ("LIVETV_SEL_BG", "(254, 196, 37)"),
        ("LIVETV_HINT_BG", "(15, 86, 137)"),
    ):
        assert f"#define {name}" in source
        assert rgb in source, f"{name} no longer uses the sampled colour"


def test_guide_never_touches_tagcache_or_core_alloc():
    """Decoration must not disturb playback memory or the database."""
    source = GUIDE_C.read_text(encoding="utf-8")
    for forbidden in ("core_alloc", "tagcache", "audio_stop", "plugin_get_audio_buffer"):
        assert forbidden not in source


class _StubLibrary:
    def __init__(self, directory):
        self._directory = str(directory)

    def state_dir(self):
        return self._directory


def _show(path, series):
    return LiveTvMedia(path=path, kind="show", title=path, series=series,
                       duration=600)


def test_building_channels_does_not_disturb_manual_edits(tmp_path):
    """Adding, renaming and renumbering channels must survive a rebuild."""
    lineup = LiveTvLineup(_StubLibrary(tmp_path))
    shows = [_show("/live/A/1.mp4", "Alpha"), _show("/live/B/1.mp4", "Beta")]
    lineup.autobuild(shows)

    lineup.channels.append(LiveTvChannel(number=500, callsign="MINE",
                                         name="My Channel"))
    alpha = next(c for c in lineup.channels if c.name == "Alpha")
    alpha.number, alpha.callsign, alpha.name = 200, "RALP", "Renamed Alpha"
    lineup.save()

    shows.append(_show("/live/C/1.mp4", "Gamma"))
    shows.append(_show("/live/A/2.mp4", "Alpha"))

    reloaded = LiveTvLineup(_StubLibrary(tmp_path))
    reloaded.autobuild(shows)
    names = {channel.name for channel in reloaded.channels}

    assert "My Channel" in names, "a hand-made channel was dropped"
    assert "Renamed Alpha" in names, "a rename was undone"
    assert "Alpha" not in names, "a renamed channel came back as a duplicate"
    assert "Gamma" in names, "a new series did not get a channel"

    renamed = next(c for c in reloaded.channels if c.name == "Renamed Alpha")
    assert renamed.number == 200 and renamed.callsign == "RALP"
    assert len(renamed.shows) == 2, "a new episode missed its channel"
    assert sorted(c.number for c in reloaded.channels) == \
        [c.number for c in reloaded.channels]


def test_duplicate_channel_numbers_are_repaired_on_load(tmp_path):
    """The device takes the first match, so numbers must be unique."""
    import json

    (tmp_path / "lineup.json").write_text(json.dumps({
        "channels": [
            {"number": 100, "callsign": "AAA", "name": "First", "shows": []},
            {"number": 100, "callsign": "BBB", "name": "Second", "shows": []},
            {"number": 101, "callsign": "CCC", "name": "Third", "shows": []},
        ],
    }), encoding="utf-8")

    lineup = LiveTvLineup(_StubLibrary(tmp_path))
    numbers = [channel.number for channel in lineup.channels]
    assert len(numbers) == len(set(numbers)), numbers
    assert len(lineup.channels) == 3
    # The repair is written back, so it does not recur.
    reloaded = LiveTvLineup(_StubLibrary(tmp_path))
    assert [c.number for c in reloaded.channels] == numbers


def test_new_channels_take_the_lowest_free_number(tmp_path):
    lineup = LiveTvLineup(_StubLibrary(tmp_path))
    lineup.channels = [
        LiveTvChannel(number=100, callsign="A", name="A"),
        LiveTvChannel(number=102, callsign="C", name="C"),
    ]
    assert lineup.next_free_number() == 101


def test_a_show_is_never_assigned_to_two_channels(tmp_path):
    lineup = LiveTvLineup(_StubLibrary(tmp_path))
    shows = [_show("/live/A/1.mp4", "Alpha"), _show("/live/A/2.mp4", "Alpha")]
    lineup.autobuild(shows)
    lineup.autobuild(shows)

    seen = [path for channel in lineup.channels for path in channel.shows]
    assert len(seen) == len(set(seen))
    assert set(seen) == {item.path for item in shows}


def _library_over(tmp_path, monkeypatch, media_dir):
    class _Config:
        video_dirs = [str(media_dir)]
        video_dir = str(media_dir)
        ffmpeg_binary = ""

    library = livetv.LiveTvLibrary(_Config())
    monkeypatch.setattr(library, "state_dir", lambda: str(tmp_path))
    library._overrides_path = str(tmp_path / "titles.json")
    library._overrides = {}
    return library


def test_a_show_can_be_renamed_and_the_rename_survives_a_rescan(
        tmp_path, monkeypatch):
    """Filenames are often air dates, so guide titles must be editable."""
    media = tmp_path / "media" / "Live"
    media.mkdir(parents=True)
    clip = media / "WWE_Monday_Night_Raw_2006_01_09_LQ.mp4"
    clip.write_bytes(b"0" * 2048)

    library = _library_over(tmp_path, monkeypatch, tmp_path / "media")
    shows, _ads = library.scan(probe_durations=False)
    assert len(shows) == 1
    original = shows[0].title
    assert not shows[0].renamed

    library.set_title(str(clip), "Monday Night Raw",
                      "Sports. WWE from January 2006.")

    reloaded = _library_over(tmp_path, monkeypatch, tmp_path / "media")
    reloaded._load_overrides()
    shows, _ads = reloaded.scan(probe_durations=False)
    assert shows[0].title == "Monday Night Raw"
    assert shows[0].description == "Sports. WWE from January 2006."
    assert shows[0].renamed is True

    # Clearing the override goes back to the derived title.
    reloaded.clear_title(str(clip))
    again = _library_over(tmp_path, monkeypatch, tmp_path / "media")
    again._load_overrides()
    shows, _ads = again.scan(probe_durations=False)
    assert shows[0].title == original
    assert shows[0].renamed is False


def test_a_renamed_title_reaches_the_generated_guide(tmp_path):
    lineup, shows, ads = _fixture()
    shows[0].title = "Monday Night Raw"
    slots = LiveTvScheduler(lineup, shows, ads).build()

    titles = {slot.title for slot in slots}
    assert "Monday Night Raw" in titles
    # Commercials inside that programme carry the renamed title too. A block
    # is identified by its day as well as its start; the same second of the
    # day belongs to a different programme on a different day.
    show_blocks = {(slot.day, slot.block_start) for slot in slots
                   if slot.title == "Monday Night Raw" and slot.kind == "S"}
    ad_titles = {slot.title for slot in slots
                 if slot.kind == "A"
                 and (slot.day, slot.block_start) in show_blocks}
    assert ad_titles == {"Monday Night Raw"}


def test_cutting_a_section_shortens_the_programme():
    item = LiveTvMedia(path="/live/tape.mp4", kind="show", title="Tape",
                       series="Tape", duration=1800, segment="part1",
                       start=0, end=1800, cuts=[[600, 700]])
    assert item.keep_ranges() == [(0, 600), (700, 1800)]
    assert item.edited_duration() == 1700


def test_overlapping_and_touching_cuts_are_merged():
    item = LiveTvMedia(path="/live/tape.mp4", kind="show", title="Tape",
                       series="Tape", segment="part1", start=0, end=1000,
                       cuts=[[100, 300], [200, 400], [900, 1200]])
    assert item.keep_ranges() == [(0, 100), (400, 900)]
    assert item.edited_duration() == 600


def test_splitting_gives_each_programme_its_own_device_file(tmp_path):
    whole = LiveTvMedia(path="/live/Double Bill.mp4", kind="show",
                        title="Double Bill", series="Double Bill",
                        duration=3600)
    first = LiveTvMedia(path=whole.path, kind="show", title="Part One",
                        series=whole.series, segment="ep1", start=0, end=1800)
    second = LiveTvMedia(path=whole.path, kind="show", title="Part Two",
                         series=whole.series, segment="ep2", start=1800,
                         end=3600)

    assert first.key != second.key
    assert first.device_relative() != second.device_relative()
    for media in (first, second):
        assert media.device_relative().endswith(".mpg")
        assert ".." not in media.device_relative()
    assert first.edited_duration() == 1800


def test_edits_persist_and_expand_into_separate_programmes(
        tmp_path, monkeypatch):
    media = tmp_path / "media" / "Live"
    media.mkdir(parents=True)
    clip = media / "Evening Tape.mp4"
    clip.write_bytes(b"0" * 4096)

    library = _library_over(tmp_path, monkeypatch, tmp_path / "media")
    library._edits_path = str(tmp_path / "edits.json")
    library._edits = {}
    monkeypatch.setattr(library, "duration_for", lambda path: 3600)

    shows, _ads = library.scan(probe_durations=True)
    assert len(shows) == 1 and not shows[0].is_segment

    library.set_edits(str(clip), [
        {"id": "ep1", "title": "First Show", "start": 0, "end": 1800,
         "cuts": [[300, 360]]},
        {"id": "ep2", "title": "Second Show", "start": 1800, "end": 3600,
         "cuts": []},
    ])

    reloaded = _library_over(tmp_path, monkeypatch, tmp_path / "media")
    reloaded._edits_path = str(tmp_path / "edits.json")
    reloaded._load_edits()
    monkeypatch.setattr(reloaded, "duration_for", lambda path: 3600)

    shows, _ads = reloaded.scan(probe_durations=True)
    assert [media.title for media in shows] == ["First Show", "Second Show"]
    assert shows[0].duration == 1740, "the cut was not taken off the length"
    assert shows[1].duration == 1800
    assert len({media.key for media in shows}) == 2

    # Clearing the edits restores the single recording.
    reloaded.set_edits(str(clip), [])
    shows, _ads = reloaded.scan(probe_durations=True)
    assert len(shows) == 1 and not shows[0].is_segment


def test_the_encode_trims_to_the_kept_ranges_only():
    graph = livetv.LiveTvSync.edit_filter_complex([(0, 600), (700, 1800)])

    assert "trim=start=0:end=600" in graph
    assert "trim=start=700:end=1800" in graph
    assert "atrim=start=0:end=600" in graph
    assert "concat=n=2:v=1:a=1" in graph
    # The screen-filling scale still runs after the pieces are joined.
    assert livetv.LiveTvSync.video_filter() in graph
    assert graph.endswith("[vout]")


def test_a_split_recording_is_never_deleted_from_staging():
    """Removing the source after the first part would strand the rest."""
    source = (REPO / "rockpod" / "services" / "livetv.py").read_text(
        encoding="utf-8")
    assert "not item.is_segment and" in source
    assert "item.path.startswith(staging + os.sep)" in source


def test_videos_are_encoded_to_fill_the_ipod_screen():
    """Letterboxing a 16:9 rip into 4:3 leaves a small, banded picture."""
    video_filter = livetv.LiveTvSync.video_filter()

    assert "scale=320:240:force_original_aspect_ratio=increase" in video_filter
    assert "crop=320:240" in video_filter
    assert "pad=" not in video_filter, "padding would reintroduce black bars"
    # Anamorphic 720x480 rips must be squared before scaling.
    assert video_filter.startswith("scale=iw*sar:ih")
    assert "setsar=1" in video_filter
    assert "fps=20" in video_filter


def test_changing_the_encode_profile_invalidates_cached_clips(tmp_path):
    class _Library:
        def cache_dir(self):
            return str(tmp_path)

    sync = livetv.LiveTvSync(_Library())
    item = _media("/live/show.mp4", "show", 100)
    cached = sync.cached_mpeg_path(item)

    assert livetv.LIVETV_MPEG_PROFILE in cached
    assert cached.endswith(item.device_relative())

    # A clip left behind by an older profile is cleared out.
    stale = tmp_path / "mpeg2-320x240-old"
    stale.mkdir()
    (stale / "clip.mpg").write_bytes(b"0")
    current = tmp_path / livetv.LIVETV_MPEG_PROFILE
    current.mkdir()
    (current / "keep.mpg").write_bytes(b"0")

    assert sync.prune_stale_cache() == 1
    assert not stale.exists()
    assert (current / "keep.mpg").is_file()


def test_ad_break_bounds_are_configurable_and_default_to_two_or_three():
    assert livetv.DEFAULT_AD_BREAK_MIN == 2
    assert livetv.DEFAULT_AD_BREAK_MAX == 3
    lineup, shows, ads = _fixture()
    lineup.ad_break_min = 3
    lineup.ad_break_max = 3
    slots = sorted(LiveTvScheduler(lineup, shows, ads).build(),
                   key=lambda s: (s.day, s.start))
    runs = []
    run = 0
    for slot in slots:
        if slot.kind == "A":
            run += 1
        elif run:
            runs.append(run)
            run = 0
    assert runs and all(length == 3 for length in runs)


class _StubLibraryForSync:
    """Just enough of LiveTvLibrary for LiveTvSync's reconciliation and
    device-write paths, without touching ffmpeg, the network or state
    files on disk."""

    def __init__(self, cache_root, roots):
        self._cache_root = cache_root
        self._roots = [str(r) for r in roots]

    def cache_dir(self):
        return str(self._cache_root)

    def staging_dir(self):
        return str(self._cache_root / "staging")

    def source_roots(self):
        return self._roots

    def _apply_override(self, item):
        return item

    def edits_for(self, path):
        return []

    def override_for(self, key):
        return {}

    def _probe_duration(self, path):
        return 42


def test_prune_orphaned_cache_removes_only_what_the_schedule_no_longer_needs(
        tmp_path):
    library = _StubLibraryForSync(tmp_path / "cache", [tmp_path / "live"])
    sync = livetv.LiveTvSync(library)

    profile_root = Path(library.cache_dir()) / livetv.LIVETV_MPEG_PROFILE
    kept = profile_root / "Kept_Show" / "episode.mpg"
    orphan = profile_root / "Old_Series_Name" / "episode.mpg"
    kept.parent.mkdir(parents=True)
    orphan.parent.mkdir(parents=True)
    kept.write_bytes(b"x")
    orphan.write_bytes(b"x")

    removed = sync.prune_orphaned_cache({"Kept_Show/episode.mpg"})

    assert removed == 1
    assert kept.is_file()
    assert not orphan.exists()
    assert not orphan.parent.exists()  # emptied directory is cleaned up too


def test_a_show_missing_locally_is_rebuilt_from_its_cached_copy(tmp_path):
    """Once a show has reached the iPod, deleting it from the laptop must
    not drop its channel from the next sync."""
    root = tmp_path / "live"
    library = _StubLibraryForSync(tmp_path / "cache", [root])
    sync = livetv.LiveTvSync(library)

    path = str(root / "WWE_Monday_Night_Raw_2006_01_02_SHD.mp4")
    series = series_name(path, str(root))
    probe_item = LiveTvMedia(path=path, kind="show", title="x", series=series)
    cached = Path(sync.cached_mpeg_path(probe_item))
    cached.parent.mkdir(parents=True)
    cached.write_bytes(b"already converted")

    channel = LiveTvChannel(number=103, callsign="USA", name="USA Network",
                            shows=[path], ads=[])
    lineup = _Lineup([channel])

    # The file is not on disk: scan() would not have found it.
    assert not Path(path).exists()

    shows, ads = sync.reconcile_missing_sources(lineup, [], [])

    assert not ads
    assert len(shows) == 1
    stand_in = shows[0]
    assert stand_in.path == path
    assert stand_in.duration == 42
    assert stand_in.series == series_name(path, str(root))
    # And it schedules like any other show.
    slots = LiveTvScheduler(lineup, shows, []).build()
    assert slots and all(slot.channel == 103 for slot in slots)


def test_reconcile_drops_a_path_with_no_cached_copy_either(tmp_path):
    """A show that is neither on disk nor ever synced stays dropped."""
    root = tmp_path / "live"
    library = _StubLibraryForSync(tmp_path / "cache", [root])
    sync = livetv.LiveTvSync(library)

    channel = LiveTvChannel(number=100, callsign="CH", name="Channel",
                            shows=[str(root / "never-synced.mp4")], ads=[])
    lineup = _Lineup([channel])

    shows, ads = sync.reconcile_missing_sources(lineup, [], [])
    assert shows == [] and ads == []


def test_sync_never_writes_a_guide_row_for_a_slot_that_failed_to_copy(
        tmp_path, monkeypatch):
    """A programme whose conversion or copy failed must not be scheduled -
    otherwise the device shows "Channel unavailable" at its air time."""
    root = tmp_path / "live"
    library = _StubLibraryForSync(tmp_path / "cache", [root])
    sync = livetv.LiveTvSync(library)
    monkeypatch.setattr(sync, "install_logos", lambda *a, **k: None)

    good = _media(str(root / "good.mp4"), "show", 1000, "Good Show",
                  "Good Show")
    bad = _media(str(root / "bad.mp4"), "show", 1000, "Bad Show", "Bad Show")
    good_chan = LiveTvChannel(number=100, callsign="GOOD", name="Good",
                              shows=[good.path], ads=[])
    bad_chan = LiveTvChannel(number=101, callsign="BAD", name="Bad",
                             shows=[bad.path], ads=[])
    lineup = _Lineup([good_chan, bad_chan])

    def fake_ensure_mpeg(item):
        if item.path == bad.path:
            raise RuntimeError("conversion failed")
        target = Path(sync.cached_mpeg_path(item))
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(b"x" * 10)
        return str(target)

    monkeypatch.setattr(sync, "ensure_mpeg", fake_ensure_mpeg)

    mount = tmp_path / "ipod"
    summary = sync.sync(str(mount), lineup, [good, bad], [])

    assert summary["errors"]
    assert summary["warnings"]

    guide_path = sync.device_root(str(mount)) + "/guide.tsv"
    guide_text = Path(guide_path).read_text(encoding="utf-8")
    assert "Good Show" in guide_text
    assert "Bad Show" not in guide_text

    # A channel with no playable content must not be published to the device.
    channels_text = Path(
        sync.device_root(str(mount)) + "/channels.tsv").read_text(
        encoding="utf-8")
    assert "\tGOOD\t" in channels_text
    assert "\tBAD\t" not in channels_text
    assert summary["channels"] == 1
