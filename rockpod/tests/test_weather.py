import json
import os
import threading
import time
import urllib.parse

from services import weather


class _Config:
    cache_dir = ""

    def __init__(self, cache_dir, **overrides):
        self.cache_dir = cache_dir
        self._overrides = overrides

    def get_effective(self, key, device=None, default=None):
        values = {
            "weather_enabled": True,
            "weather_location_name": "Moncton, NB",
            "weather_latitude": 46.0878,
            "weather_longitude": -64.7782,
            "weather_units": "metric",
            "weather_maps_enabled": False,
            "weather_cache_max_age_minutes": 60,
            "weather_sync_stale_cache": True,
        }
        values.update(self._overrides)
        return values.get(key, default)


def _payload():
    return {
        "timezone": "America/Halifax",
        "daily": {
            "time": ["2026-07-02"],
            "weather_code": [61],
            "temperature_2m_min": [18.2],
            "temperature_2m_max": [25.1],
            "precipitation_probability_max": [70],
            "wind_speed_10m_max": [21],
            "wind_direction_10m_dominant": [180],
            "sunrise": ["2026-07-02T05:30"],
            "sunset": ["2026-07-02T21:10"],
        },
        "hourly": {
            "time": ["2026-07-02T12:00", "2026-07-02T13:00"],
            "weather_code": [3, 61],
            "temperature_2m": [22.1, 23.6],
            "precipitation_probability": [20, 55],
            "wind_speed_10m": [12, 15],
            "wind_direction_10m": [160, 170],
            "is_day": [1, 1],
        },
    }


def test_open_meteo_request_includes_16_day_hourly(monkeypatch):
    captured = {}

    class _Response:
        def __enter__(self):
            return self

        def __exit__(self, exc_type, exc, tb):
            return False

        def read(self):
            return json.dumps(_payload()).encode("utf-8")

    def fake_urlopen(request, timeout):
        captured["url"] = request.full_url
        captured["timeout"] = timeout
        return _Response()

    monkeypatch.setattr(weather.urllib.request, "urlopen", fake_urlopen)

    weather._fetch_open_meteo(46.0878, -64.7782, "metric")

    query = urllib.parse.parse_qs(urllib.parse.urlparse(captured["url"]).query)
    assert captured["timeout"] == weather.WEATHER_HTTP_TIMEOUT_SECONDS
    assert captured["timeout"] <= 10
    assert query["forecast_days"] == ["16"]
    assert query["forecast_hours"] == [str(16 * 24)]
    assert "temperature_2m" in query["hourly"][0]
    assert "weather_code" in query["hourly"][0]


def test_weather_bundle_writes_hourly_rows(tmp_dir, monkeypatch):
    monkeypatch.setattr(weather, "_fetch_open_meteo", lambda *_args: _payload())

    report = weather.build_weather_bundle(_Config(tmp_dir), output_root=tmp_dir)
    forecast = os.path.join(tmp_dir, ".rockbox", "rockpod", "weather", "forecast.tsv")
    manifest = os.path.join(tmp_dir, ".rockbox", "rockpod", "weather", "manifest.json")

    with open(forecast, "r", encoding="utf-8") as handle:
        lines = [line.strip() for line in handle if line.strip()]

    assert lines[0].startswith("rockpod_weather_v1\tMoncton, NB")
    assert lines[1].startswith("2026-07-02\train\tRain\t18\t25")
    assert lines[2].startswith("hourly\t2026-07-02T12:00\tcloudy\tCloudy\t22")
    assert lines[3].startswith("hourly\t2026-07-02T13:00\train\tRain\t24")
    with open(manifest, "r", encoding="utf-8") as handle:
        data = json.load(handle)
    assert data["forecast_rows"] == 1
    assert data["hourly_rows"] == 2
    assert any(rel == ".rockbox/rockpod/weather/forecast.tsv" for _, rel, _ in report.files)


def _write_cached_forecast(root):
    forecast = (
        root / ".rockbox" / "rockpod" / "weather" / "forecast.tsv"
    )
    forecast.parent.mkdir(parents=True, exist_ok=True)
    forecast.write_text(
        "rockpod_weather_v1\tMoncton, NB\t46.0878\t-64.7782\t"
        "America/Halifax\t2026-08-13T12:00:00Z\t2026-08-13\tmetric\n"
        "2026-08-13\tclear\tClear\t16\t25\t10\t12\t180\t06:10\t20:35\topen-meteo\n"
        "hourly\t2026-08-13T12:00\tclear\tClear\t24\t10\t8\t170\t1\topen-meteo\n",
        encoding="utf-8",
    )
    return forecast


def test_weather_bundle_reuses_fresh_cache_without_network(tmp_path, monkeypatch):
    forecast = _write_cached_forecast(tmp_path)
    manifest = forecast.parent / "manifest.json"
    manifest.write_text('{"generated_at":"cached"}\n', encoding="utf-8")

    def unexpected_fetch(*_args):
        raise AssertionError("fresh weather cache should avoid a network fetch")

    monkeypatch.setattr(weather, "_fetch_open_meteo", unexpected_fetch)
    monkeypatch.setattr(
        weather,
        "_render_ipodjs_weather_previews",
        lambda *_args: (_ for _ in ()).throw(
            AssertionError("cache-only sync must not regenerate previews")
        ),
    )

    report = weather.build_weather_bundle(
        _Config(str(tmp_path)), output_root=tmp_path, allow_network=False
    )

    assert report.errors == []
    assert report.warnings == []
    assert any(path == str(forecast) for path, _rel, _hash in report.files)
    assert manifest.read_text(encoding="utf-8") == '{"generated_at":"cached"}\n'


def test_weather_bundle_cache_only_mode_never_fetches(tmp_path, monkeypatch):
    def unexpected_fetch(*_args):
        raise AssertionError("cache-only weather sync must not use the network")

    monkeypatch.setattr(weather, "_fetch_open_meteo", unexpected_fetch)

    report = weather.build_weather_bundle(
        _Config(str(tmp_path)),
        output_root=tmp_path,
        allow_network=False,
    )

    assert report.errors == []
    assert report.warnings == [
        "Weather cache is empty; use Device > Sync Weather to refresh it"
    ]


def test_weather_bundle_uses_stale_cache_when_provider_is_offline(
        tmp_path, monkeypatch):
    forecast = _write_cached_forecast(tmp_path)
    os.utime(forecast, (1, 1))
    monkeypatch.setattr(
        weather,
        "_fetch_open_meteo",
        lambda *_args: (_ for _ in ()).throw(OSError("offline")),
    )

    report = weather.build_weather_bundle(
        _Config(str(tmp_path)), output_root=tmp_path
    )

    assert report.errors == []
    assert report.warnings == [
        "Weather forecast fetch failed: offline; using cached forecast"
    ]
    manifest = json.loads(
        (tmp_path / ".rockbox" / "rockpod" / "weather" /
         "manifest.json").read_text(encoding="utf-8")
    )
    assert manifest["forecast_rows"] == 1
    assert manifest["hourly_rows"] == 1


def test_weather_bundle_reports_missing_forecast_when_offline(
        tmp_path, monkeypatch):
    monkeypatch.setattr(
        weather,
        "_fetch_open_meteo",
        lambda *_args: (_ for _ in ()).throw(OSError("offline")),
    )

    report = weather.build_weather_bundle(
        _Config(str(tmp_path)), output_root=tmp_path
    )

    assert report.errors == ["Weather forecast fetch failed: offline"]
    assert not any(
        rel == ".rockbox/rockpod/weather/forecast.tsv"
        for _path, rel, _hash in report.files
    )


def test_radar_maps_are_real_tiled_frames_with_stable_names(
        tmp_path, monkeypatch):
    from PIL import Image

    frames = [
        {"time": 1785411000 + index * 600,
         "path": f"/v2/radar/frame-{index}"}
        for index in range(4)
    ]

    class _Response:
        def __enter__(self):
            return self

        def __exit__(self, exc_type, exc, tb):
            return False

        def read(self):
            return json.dumps({
                "host": "https://radar.invalid",
                "radar": {"past": frames},
            }).encode("utf-8")

    monkeypatch.setattr(
        weather.urllib.request, "urlopen",
        lambda *_args, **_kwargs: _Response())

    def fake_image(url):
        color = (30, 120, 210, 105) if "radar.invalid" in url else (
            80, 95, 70, 255)
        return Image.new("RGBA", (256, 256), color)

    monkeypatch.setattr(weather, "_download_rgba", fake_image)
    bundle = tmp_path / ".rockbox" / "rockpod" / "weather"
    outputs = weather._fetch_radar_maps(bundle, 46.0878, -64.7782)

    pngs = [path for path in outputs if path.suffix == ".png"]
    assert [path.name for path in pngs] == [
        "radar-0.png", "radar-1.png", "radar-2.png", "radar-3.png"]
    with Image.open(pngs[-1]) as image:
        assert image.size == (weather.RADAR_WIDTH, weather.RADAR_HEIGHT)
        assert image.getpixel((320, 240)) != (80, 95, 70)
    metadata = json.loads((bundle / "maps" / "radar.json").read_text())
    assert metadata["provider"] == "RainViewer"
    assert metadata["base_map"] == "OpenStreetMap"


def test_radar_tiles_download_in_parallel_and_preserve_order(monkeypatch):
    active = 0
    maximum_active = 0
    lock = threading.Lock()

    def fake_download(url):
        nonlocal active, maximum_active
        with lock:
            active += 1
            maximum_active = max(maximum_active, active)
        time.sleep(0.02)
        with lock:
            active -= 1
        return f"image:{url}"

    monkeypatch.setattr(weather, "_download_rgba", fake_download)
    urls = [f"https://tiles.invalid/{index}" for index in range(12)]

    assert weather._download_rgba_batch(urls) == [
        f"image:{url}" for url in urls
    ]
    assert maximum_active > 1


def test_ipodjs_weather_preview_uses_real_radar_and_forecast(tmp_path):
    from PIL import Image

    radar = tmp_path / "radar.png"
    Image.new("RGB", (640, 480), (34, 116, 72)).save(radar)
    daily = weather._rows_from_payload(_payload())
    hourly = weather._hourly_rows_from_payload(_payload())

    outputs = weather._render_ipodjs_weather_previews(
        tmp_path, [radar], "Moncton, NB", "metric", daily, hourly)

    assert len(outputs) == weather.IPODJS_WEATHER_PREVIEW_FRAMES
    assert [path.name for path in outputs] == [
        "frame-00.bmp", "frame-01.bmp", "frame-02.bmp", "frame-03.bmp"]
    with Image.open(outputs[0]) as pane:
        assert pane.size == weather.IPODJS_WEATHER_PREVIEW_SIZE
        assert pane.mode == "RGB"
        assert pane.getpixel((87, 103)) != (34, 116, 72)
