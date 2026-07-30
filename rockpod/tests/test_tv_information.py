import json
from pathlib import Path

from PIL import Image

from services import livetv
from services import tv_information as tv


def test_feed_and_current_news_video_parsers():
    atom = b"""<?xml version="1.0"?>
    <feed xmlns="http://www.w3.org/2005/Atom">
      <entry><id>nb-1</id><title>New Brunswick update</title>
      <updated>2026-07-29T12:00:00Z</updated>
      <link href="https://example.test/news/1"/></entry>
    </feed>"""
    items = tv.parse_feed(atom, source="Canada News")
    assert [(item.title, item.identity) for item in items] == [
        ("New Brunswick update", "nb-1")]

    page = b"""
    <a href="/video/123/current-story/">Current NB story 02:15</a>
    <a href="/video/123/current-story/">Current NB story 02:15</a>
    <a href="/not-video/">Ignore this</a>
    """
    videos = tv.parse_news_video_page(
        page, "https://globalnews.ca/videos/new-brunswick/")
    assert len(videos) == 1
    assert videos[0].url == \
        "https://globalnews.ca/video/123/current-story/"


def test_information_clock_converts_utc_to_atlantic():
    assert tv._atlantic_stamp("2026-07-29T03:02:00Z") == "00:02 ATL"


def test_maritime_camera_parsers_ignore_site_branding():
    html = b"""
    <img src="/images/wordmark-en.png" alt="Government">
    <img src="/webcam/secure/images/rwis_cam/Amherst_1.jpg"
         alt="Amherst">
    """
    items = tv.parse_camera_page(
        html, "https://novascotia.ca/tran/cameras/all.asp",
        "Nova Scotia")
    assert [(item.title, item.source) for item in items] == [
        ("Amherst", "Nova Scotia")]

    payload = json.dumps({
        "item2": [{"itemId": "7", "location": [46.2, -63.1]}],
    })
    items = tv.camera_api_items(
        payload, "https://511.gov.pe.ca/en/cameras.html",
        "Prince Edward Island")
    assert items[0].enclosure == \
        "https://511.gov.pe.ca/map/Cctv/7"


def test_road_capture_frame_preserves_real_image_and_adds_credit(tmp_path):
    source = tmp_path / "capture.jpg"
    output = tmp_path / "frame.jpg"
    Image.new("RGB", (1200, 630), (23, 91, 147)).save(source)

    tv.render_road_capture_frame(
        source, output, "MacKay Bridge — Halifax bound",
        captured_at="2026-07-29T14:25:00Z")

    rendered = Image.open(output).convert("RGB")
    assert rendered.size == (640, 480)
    assert rendered.getpixel((40, 100))[2] > rendered.getpixel((40, 100))[0]
    assert rendered.getpixel((40, 450)) != rendered.getpixel((40, 100))


def test_road_capture_builds_chronological_video_source(
        tmp_path, monkeypatch):
    calls = []
    sleeps = []

    def download(url, target, **_kwargs):
        calls.append(url)
        Image.new(
            "RGB", (1200, 630), (30 * len(calls), 80, 140)).save(target)
        return str(target)

    def build(_ffmpeg, frames, target, seconds_per_frame=2.5):
        assert len(frames) == 3
        assert seconds_per_frame == 2.5
        Path(target).write_bytes(b"captured-video")
        return str(target)

    monkeypatch.setattr(tv, "_download_atomic", download)
    monkeypatch.setattr(tv, "_build_camera_capture", build)
    monkeypatch.setattr(tv.time, "sleep", sleeps.append)
    service = tv.TvInformationService({
        "cache_dir": str(tmp_path),
        "tv_information_road_capture_frames": 3,
        "tv_information_road_capture_interval": 1,
        "tv_information_road_capture_cameras": [{
            "title": "Test bridge",
            "url": "https://camera.test/live.jpg",
        }],
    })
    source_dir = tmp_path / "source"
    source_dir.mkdir()

    items, sources, warnings = service._road_capture_sources(source_dir)

    assert not warnings
    assert [item.title for item in items] == ["Test bridge"]
    assert Path(sources[0]).read_bytes() == b"captured-video"
    assert len(calls) == 3
    assert all("rockpod=" in url for url in calls)
    assert len(sleeps) == 2


def test_silent_capture_normalization_ends_with_source(
        tmp_path, monkeypatch):
    captured = {}

    monkeypatch.setattr(tv, "_probe", lambda *_args: (7.5, False))

    def run(command, target):
        captured["command"] = command
        return target

    monkeypatch.setattr(tv, "_run_ffmpeg", run)
    tv._normalize_video(
        "ffmpeg", "ffprobe", "capture.mkv", "bug.png",
        str(tmp_path / "normalized.mkv"))

    graph = captured["command"][
        captured["command"].index("-filter_complex") + 1]
    assert "overlay=W-w-12:10:format=auto:shortest=1" in graph


def test_each_channel_has_distinct_guide_icon_and_bug(tmp_path):
    pixels = []
    for channel in tv.CHANNELS:
        logo, bug = tv.render_channel_identity(channel, tmp_path / channel.key)
        assert Image.open(logo).size == (240, 108)
        assert Image.open(bug).size == (120, 54)
        pixels.append(Image.open(logo).convert("RGB").getpixel((120, 90)))
    assert len(set(pixels)) == len(tv.CHANNELS)


def test_nasa_slot_is_replaced_when_current_asset_changes(
        tmp_path, monkeypatch):
    state = {"id": "mission-one"}

    def request(url, **_kwargs):
        if "images-api" in url:
            return json.dumps({
                "collection": {"items": [{
                    "href": "https://assets.test/collection.json",
                    "data": [{
                        "nasa_id": state["id"],
                        "title": state["id"],
                        "date_created": "2026-07-29T00:00:00Z",
                        "media_type": "video",
                    }],
                }]},
            }).encode()
        return json.dumps([
            f"https://assets.test/{state['id']}~mobile.mp4",
        ]).encode()

    def download(url, target, **_kwargs):
        Path(target).write_bytes(url.encode())
        return str(target)

    monkeypatch.setattr(tv, "_request_bytes", request)
    monkeypatch.setattr(tv, "_download_atomic", download)
    service = tv.TvInformationService({
        "cache_dir": str(tmp_path),
        "tv_information_video_limit": 1,
    })
    source_dir = tmp_path / "source"
    source_dir.mkdir()

    _items, first, _warnings = service._nasa_sources(source_dir)
    first_bytes = Path(first[0]).read_bytes()
    state["id"] = "mission-two"
    _items, second, _warnings = service._nasa_sources(source_dir)

    assert first == second
    assert Path(second[0]).read_bytes() != first_bytes
    assert (source_dir / "video-0.id").read_text().strip() == \
        tv.FeedItem(title="x", identity="mission-two").stable_id()


def test_information_channels_never_use_unrelated_commercials():
    channel = livetv.LiveTvChannel(
        number=100, callsign="NASA", name="NASA",
        category=tv.NASA_CATEGORY, shows=["show"])
    lineup = type("Lineup", (), {
        "channels": [channel],
        "seed": 1,
        "ad_break_min": 2,
        "ad_break_max": 3,
    })()
    show = livetv.LiveTvMedia(
        path="show", kind="show", title="Show", series="NASA", duration=1800)
    ad = livetv.LiveTvMedia(
        path="ad", kind="ad", title="Ad", series="Ads", duration=30)
    scheduler = livetv.LiveTvScheduler(lineup, [show], [ad])
    assert scheduler._channel_ads(channel) == []


def test_information_channels_are_added_with_icons(tmp_path, monkeypatch):
    media = []
    logos = {}
    for channel in tv.CHANNELS:
        programme = tmp_path / f"{channel.key}-current.mkv"
        programme.write_bytes(b"video")
        logo = tmp_path / f"{channel.key}-guide.png"
        Image.new("RGB", (240, 108), channel.colours[1]).save(logo)
        media.append(tv.TvInformationMedia(
            channel=channel, path=str(programme),
            generated_at="2026-07-29T12:00:00Z"))
        logos[channel.category] = str(logo)

    result = tv.TvInformationResult(media=media, logos=logos)
    monkeypatch.setattr(
        tv.TvInformationService, "refresh",
        lambda self, progress=None, force=False: result)

    class Lineup:
        def __init__(self):
            self.channels = []

        def next_free_number(self):
            return 100 + len(self.channels)

        def ensure_unique_numbers(self):
            return False

    lineup = Lineup()
    shows, refreshed = livetv.ensure_tv_information_channels(
        lineup, {"cache_dir": str(tmp_path)}, [])
    assert refreshed is result
    assert [item.series for item in shows] == [
        "NASA", "New Brunswick News", "Maritime RoadWatch"]
    assert [channel.callsign for channel in lineup.channels] == [
        "NASA", "NBT", "ROAD"]
    assert all(Path(channel.logo).is_file() for channel in lineup.channels)


def test_information_refresh_preserves_channel_ads(tmp_path, monkeypatch):
    channel_spec = tv.CHANNELS[0]
    programme = tmp_path / "nasa-current.mkv"
    programme.write_bytes(b"video")
    result = tv.TvInformationResult(media=[
        tv.TvInformationMedia(
            channel=channel_spec, path=str(programme),
            generated_at="2026-07-29T12:00:00Z")
    ])
    monkeypatch.setattr(
        tv.TvInformationService, "refresh",
        lambda self, progress=None, force=False: result)

    existing = livetv.LiveTvChannel(
        number=109,
        callsign="NASA",
        name="NASA",
        category=channel_spec.category,
        ads=["/ads/2002-bell.mp4"],
    )

    class Lineup:
        channels = [existing]

        def ensure_unique_numbers(self):
            return False

    livetv.ensure_tv_information_channels(
        Lineup(), {"cache_dir": str(tmp_path)}, [])

    assert existing.ads == ["/ads/2002-bell.mp4"]
