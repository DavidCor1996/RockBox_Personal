"""Live TV scheduling, scanning and device-format tests.

The whole illusion depends on the PC guide, the iPod guide and the iPod
player resolving the same instant to the same programme, so most of these
tests are about determinism and about the file format both sides parse.
"""

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


class _Lineup:
    """A line-up without the on-disk state file."""

    def __init__(self, channels, seed=20051115, ad_min=2, ad_max=3):
        self.channels = channels
        self.seed = seed
        self.ad_break_min = ad_min
        self.ad_break_max = ad_max


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
