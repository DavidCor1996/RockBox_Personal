"""Generate Rockbox-ready offline weather bundles."""

from __future__ import annotations

import json
import math
import os
import shutil
import time
import urllib.parse
import urllib.request
from concurrent.futures import ThreadPoolExecutor, wait
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path

from services.file_safety import atomic_write_json, atomic_write_text
from services.metadata_reader import compute_file_hash


WEATHER_REL_DIR = os.path.join(".rockbox", "rockpod", "weather")
BACKGROUND_NAMES = ("clear_day.bmp", "rain_day.bmp", "snow_day.bmp", "night.bmp")
ASSET_BACKGROUND_DIR = Path(__file__).resolve().parents[1] / "assets" / "weather" / "backgrounds"
ASSET_ICON_DIR = Path(__file__).resolve().parents[1] / "assets" / "weather" / "icons"
RADAR_FRAME_COUNT = 4
RADAR_ZOOM = 7
RADAR_WIDTH = 640
RADAR_HEIGHT = 480
WEATHER_HTTP_TIMEOUT_SECONDS = 8
RADAR_DOWNLOAD_WORKERS = 16
RADAR_BATCH_TIMEOUT_SECONDS = 15
IPODJS_WEATHER_PREVIEW_FRAMES = 4
IPODJS_WEATHER_PREVIEW_SIZE = (174, 240)


@dataclass
class WeatherOutput:
    files: list[tuple[str, str, str]]
    errors: list[str]
    output_root: str
    warnings: list[str] = field(default_factory=list)


def _config_value(config, key, default=None, device=None):
    if hasattr(config, "get_effective"):
        return config.get_effective(key, device=device, default=default)
    if hasattr(config, "get"):
        return config.get(key, default)
    return default


def _sanitize(value):
    return str(value or "").replace("\t", " ").replace("\r", " ").replace("\n", " ").strip()


def _weather_code(code):
    try:
        code = int(code)
    except (TypeError, ValueError):
        return "unknown", "Unknown"
    if code == 0:
        return "clear", "Clear"
    if code in {1, 2}:
        return "partly_cloudy", "Partly cloudy"
    if code == 3:
        return "cloudy", "Cloudy"
    if code in {45, 48}:
        return "fog", "Fog"
    if code in {51, 53, 55, 56, 57}:
        return "drizzle", "Drizzle"
    if code in {61, 63, 65, 66, 67, 80, 81, 82}:
        return "rain", "Rain"
    if code in {71, 73, 75, 77, 85, 86}:
        return "snow", "Snow"
    if code in {95, 96, 99}:
        return "thunderstorm", "Thunderstorm"
    return "unknown", "Weather"


def _fetch_open_meteo(latitude, longitude, units):
    temp_unit = "fahrenheit" if units == "imperial" else "celsius"
    wind_unit = "mph" if units == "imperial" else "kmh"
    forecast_days = 16
    query = urllib.parse.urlencode(
        {
            "latitude": f"{float(latitude):.4f}",
            "longitude": f"{float(longitude):.4f}",
            "hourly": ",".join(
                [
                    "temperature_2m",
                    "weather_code",
                    "precipitation_probability",
                    "wind_speed_10m",
                    "wind_direction_10m",
                    "is_day",
                ]
            ),
            "daily": ",".join(
                [
                    "weather_code",
                    "temperature_2m_max",
                    "temperature_2m_min",
                    "precipitation_probability_max",
                    "wind_speed_10m_max",
                    "wind_direction_10m_dominant",
                    "sunrise",
                    "sunset",
                ]
            ),
            "temperature_unit": temp_unit,
            "wind_speed_unit": wind_unit,
            "timezone": "auto",
            "forecast_days": str(forecast_days),
            "forecast_hours": str(forecast_days * 24),
        }
    )
    url = f"https://api.open-meteo.com/v1/forecast?{query}"
    request = urllib.request.Request(url, headers={"User-Agent": "RockPod weather sync"})
    with urllib.request.urlopen(
            request, timeout=WEATHER_HTTP_TIMEOUT_SECONDS) as response:
        return json.loads(response.read().decode("utf-8"))


def _rows_from_payload(payload):
    daily = payload.get("daily") or {}
    times = daily.get("time") or []
    rows = []
    for idx, day in enumerate(times[:7]):
        code, text = _weather_code(_value(daily.get("weather_code"), idx))
        sunrise = str(_value(daily.get("sunrise"), idx) or "")
        sunset = str(_value(daily.get("sunset"), idx) or "")
        rows.append(
            [
                day,
                code,
                text,
                _num(daily.get("temperature_2m_min"), idx),
                _num(daily.get("temperature_2m_max"), idx),
                _num(daily.get("precipitation_probability_max"), idx),
                _num(daily.get("wind_speed_10m_max"), idx),
                _num(daily.get("wind_direction_10m_dominant"), idx),
                sunrise[-5:] if "T" in sunrise else sunrise,
                sunset[-5:] if "T" in sunset else sunset,
                "open-meteo",
            ]
        )
    return rows


def _hourly_rows_from_payload(payload):
    hourly = payload.get("hourly") or {}
    times = hourly.get("time") or []
    rows = []
    for idx, stamp in enumerate(times[:16 * 24]):
        code, text = _weather_code(_value(hourly.get("weather_code"), idx))
        rows.append(
            [
                "hourly",
                str(stamp or ""),
                code,
                text,
                _num(hourly.get("temperature_2m"), idx),
                _num(hourly.get("precipitation_probability"), idx),
                _num(hourly.get("wind_speed_10m"), idx),
                _num(hourly.get("wind_direction_10m"), idx),
                _num(hourly.get("is_day"), idx),
                "open-meteo",
            ]
        )
    return rows


def _value(values, idx):
    try:
        return values[idx]
    except (TypeError, IndexError):
        return None


def _num(values, idx):
    value = _value(values, idx)
    if value is None:
        return ""
    try:
        return str(int(round(float(value))))
    except (TypeError, ValueError):
        return ""


def _read_cached_forecast(path):
    """Return cached daily/hourly rows when the on-disk bundle is valid."""
    try:
        lines = [
            line.rstrip("\r\n").split("\t")
            for line in Path(path).read_text(encoding="utf-8").splitlines()
            if line.strip()
        ]
    except OSError:
        return [], []
    if not lines or not lines[0] or lines[0][0] != "rockpod_weather_v1":
        return [], []
    daily_rows = [row for row in lines[1:] if row and row[0] != "hourly"]
    hourly_rows = [row for row in lines[1:] if row and row[0] == "hourly"]
    return daily_rows, hourly_rows


def _cache_matches(path, location, latitude, longitude, units):
    """Never send a previous city's forecast or temperatures in old units."""
    try:
        with Path(path).open(encoding="utf-8") as handle:
            header = handle.readline().rstrip("\r\n").split("\t")
        return (len(header) >= 8 and header[1] == location
                and header[2] == f"{float(latitude):.4f}"
                and header[3] == f"{float(longitude):.4f}"
                and header[7] == units)
    except (OSError, ValueError):
        return False


def _cache_is_fresh(path, max_age_minutes):
    try:
        age_seconds = max(0.0, time.time() - Path(path).stat().st_mtime)
    except OSError:
        return False
    try:
        max_age_seconds = max(0.0, float(max_age_minutes)) * 60.0
    except (TypeError, ValueError):
        max_age_seconds = 3600.0
    return age_seconds <= max_age_seconds


def _cached_radar_files(bundle_dir):
    radar_dir = Path(bundle_dir) / "maps"
    frames = sorted(radar_dir.glob("radar-*.png"))
    metadata = radar_dir / "radar.json"
    if not frames or not metadata.is_file():
        return []
    return frames + [metadata]


def _copy_backgrounds(bundle_dir):
    copied = []
    bg_dir = bundle_dir / "backgrounds"
    bg_dir.mkdir(parents=True, exist_ok=True)
    for name in BACKGROUND_NAMES:
        src = ASSET_BACKGROUND_DIR / name
        dst = bg_dir / name
        if src.is_file():
            shutil.copy2(src, dst)
            copied.append(dst)
    return copied


def _copy_icons(bundle_dir):
    copied = []
    icon_dir = bundle_dir / "icons"
    icon_dir.mkdir(parents=True, exist_ok=True)
    if not ASSET_ICON_DIR.is_dir():
        return copied
    for src in sorted(ASSET_ICON_DIR.glob("*.bmp")):
        dst = icon_dir / src.name
        shutil.copy2(src, dst)
        copied.append(dst)
    return copied


def _download_rgba(url):
    from io import BytesIO

    from PIL import Image

    request = urllib.request.Request(
        url, headers={"User-Agent": "RockPod Weather personal display"})
    with urllib.request.urlopen(
            request, timeout=WEATHER_HTTP_TIMEOUT_SECONDS) as response:
        return Image.open(BytesIO(response.read())).convert("RGBA")


def _download_rgba_batch(urls):
    """Download a bounded radar tile batch without serial network stalls."""
    urls = list(urls)
    if not urls:
        return []

    executor = ThreadPoolExecutor(
        max_workers=min(RADAR_DOWNLOAD_WORKERS, len(urls)),
        thread_name_prefix="rockpod-weather",
    )
    futures = [executor.submit(_download_rgba, url) for url in urls]
    done, pending = wait(futures, timeout=RADAR_BATCH_TIMEOUT_SECONDS)
    if pending:
        for future in pending:
            future.cancel()
        executor.shutdown(wait=False, cancel_futures=True)
        raise TimeoutError(
            f"radar tile download exceeded {RADAR_BATCH_TIMEOUT_SECONDS}s"
        )

    executor.shutdown(wait=True)
    return [future.result() for future in futures]


def _slippy_pixel(latitude, longitude, zoom):
    latitude = max(-85.05112878, min(85.05112878, float(latitude)))
    scale = 256.0 * (2 ** int(zoom))
    x = (float(longitude) + 180.0) / 360.0 * scale
    lat_rad = math.radians(latitude)
    y = (
        1.0 - math.asinh(math.tan(lat_rad)) / math.pi
    ) / 2.0 * scale
    return x, y


def _fetch_radar_maps(bundle_dir, latitude, longitude):
    """Cache four real radar/map frames centred on the synced location.

    OpenStreetMap supplies the geographic base and RainViewer supplies the
    time-stamped radar overlay. Fixed filenames keep the device manifest
    stable while every Weather Sync replaces their contents with the latest
    available frames.
    """
    from PIL import Image

    map_dir = bundle_dir / "maps"
    map_dir.mkdir(parents=True, exist_ok=True)
    metadata_path = map_dir / "radar.json"
    base_path = map_dir / "base-map.png"
    request = urllib.request.Request(
        "https://api.rainviewer.com/public/weather-maps.json",
        headers={"User-Agent": "RockPod Weather personal display"})
    with urllib.request.urlopen(
            request, timeout=WEATHER_HTTP_TIMEOUT_SECONDS) as response:
        radar_payload = json.loads(response.read().decode("utf-8"))

    host = str(radar_payload.get("host") or "").rstrip("/")
    frames = list((radar_payload.get("radar") or {}).get("past") or [])
    frames = frames[-RADAR_FRAME_COUNT:]
    if not host or not frames:
        raise ValueError("radar service returned no current frames")

    center_x, center_y = _slippy_pixel(latitude, longitude, RADAR_ZOOM)
    left = int(round(center_x - RADAR_WIDTH / 2))
    top = int(round(center_y - RADAR_HEIGHT / 2))
    first_x = math.floor(left / 256)
    last_x = math.floor((left + RADAR_WIDTH - 1) / 256)
    first_y = math.floor(top / 256)
    last_y = math.floor((top + RADAR_HEIGHT - 1) / 256)
    tile_count = 2 ** RADAR_ZOOM

    tile_positions = []
    for tile_y in range(first_y, last_y + 1):
        if tile_y < 0 or tile_y >= tile_count:
            continue
        for tile_x in range(first_x, last_x + 1):
            wrapped_x = tile_x % tile_count
            paste_x = tile_x * 256 - left
            paste_y = tile_y * 256 - top
            tile_positions.append((wrapped_x, tile_y, paste_x, paste_y))

    base = None
    try:
        previous = json.loads(metadata_path.read_text(encoding="utf-8"))
        same_map = (
            abs(float(previous.get("latitude")) - float(latitude)) < 0.0001
            and abs(float(previous.get("longitude")) -
                    float(longitude)) < 0.0001
            and int(previous.get("zoom")) == RADAR_ZOOM
        )
        if same_map and base_path.is_file():
            base = Image.open(base_path).convert("RGBA")
    except (OSError, TypeError, ValueError):
        base = None
    if base is None or base.size != (RADAR_WIDTH, RADAR_HEIGHT):
        base = Image.new(
            "RGBA", (RADAR_WIDTH, RADAR_HEIGHT), (17, 34, 49, 255))
        base_urls = [
            (
                "https://tile.openstreetmap.org/"
                f"{RADAR_ZOOM}/{tile_x}/{tile_y}.png"
            )
            for tile_x, tile_y, _paste_x, _paste_y in tile_positions
        ]
        for position, tile in zip(
                tile_positions, _download_rgba_batch(base_urls)):
            _tile_x, _tile_y, paste_x, paste_y = position
            base.alpha_composite(tile, (paste_x, paste_y))
        temporary = base_path.with_suffix(".png.tmp")
        base.convert("RGB").save(temporary, "PNG", optimize=True)
        os.replace(temporary, base_path)

    frame_specs = []
    radar_urls = []
    for index, frame in enumerate(frames):
        path = str(frame.get("path") or "")
        stamp = int(frame.get("time") or 0)
        if not path or stamp <= 0:
            continue
        frame_specs.append((index, stamp))
        radar_urls.extend(
            f"{host}{path}/256/{RADAR_ZOOM}/{tile_x}/{tile_y}/2/1_1.png"
            for tile_x, tile_y, _paste_x, _paste_y in tile_positions
        )

    downloaded_radar = iter(_download_rgba_batch(radar_urls))
    outputs = []
    metadata_frames = []
    for index, stamp in frame_specs:
        composite = base.copy()
        for tile_x, tile_y, paste_x, paste_y in tile_positions:
            radar = next(downloaded_radar)
            composite.alpha_composite(radar, (paste_x, paste_y))
        target = map_dir / f"radar-{index}.png"
        temporary = target.with_suffix(".png.tmp")
        composite.convert("RGB").save(temporary, "PNG", optimize=True)
        os.replace(temporary, target)
        outputs.append(target)
        metadata_frames.append({
            "file": target.name,
            "time": stamp,
            "generated_utc": datetime.fromtimestamp(
                stamp, timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        })

    if not outputs:
        raise ValueError("radar service returned no usable frames")
    atomic_write_json(metadata_path, {
        "version": 1,
        "provider": "RainViewer",
        "base_map": "OpenStreetMap",
        "latitude": float(latitude),
        "longitude": float(longitude),
        "zoom": RADAR_ZOOM,
        "frames": metadata_frames,
    })
    return outputs + [metadata_path]


def _weather_preview_font(size, bold=False):
    from PIL import ImageFont

    candidates = (
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
        if bold else
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf"
        if bold else
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
    )
    for candidate in candidates:
        try:
            return ImageFont.truetype(candidate, size)
        except OSError:
            continue
    return ImageFont.load_default()


def _render_ipodjs_weather_previews(
        cache_root, radar_files, location, units, rows, hourly_rows):
    """Render the current real radar loop as the iPodJS forecast pane.

    These are sync-time assets. Firmware only decodes one prefetched 174x240
    BMP at an idle service point and paints cached pixels during animation.
    """
    from PIL import Image, ImageDraw

    sources = [
        Path(path) for path in radar_files
        if Path(path).suffix.lower() == ".png" and Path(path).is_file()
    ]
    output_dir = (
        Path(cache_root) / ".rockbox" / "ipodjs" / "previews" /
        "weather-loop"
    )
    output_dir.mkdir(parents=True, exist_ok=True)
    if not sources:
        return sorted(output_dir.glob("frame-*.bmp"))

    hourly = hourly_rows[0] if hourly_rows else []
    daily = rows[0] if rows else []
    condition = str(
        (hourly[3] if len(hourly) > 3 else "") or
        (daily[2] if len(daily) > 2 else "") or "Forecast"
    )
    temperature = str(
        hourly[4] if len(hourly) > 4 else
        (daily[4] if len(daily) > 4 else "")
    )
    precipitation = str(
        hourly[5] if len(hourly) > 5 else
        (daily[5] if len(daily) > 5 else "")
    )
    high = str(daily[4] if len(daily) > 4 else "")
    low = str(daily[3] if len(daily) > 3 else "")
    unit = "F" if units == "imperial" else "C"
    generated = []
    title_font = _weather_preview_font(13, bold=True)
    location_font = _weather_preview_font(10)
    temp_font = _weather_preview_font(27, bold=True)
    detail_font = _weather_preview_font(11, bold=True)
    small_font = _weather_preview_font(9)

    for index in range(IPODJS_WEATHER_PREVIEW_FRAMES):
        with Image.open(sources[index % len(sources)]) as source:
            image = source.convert("RGB")
        src_w, src_h = image.size
        crop_w = max(1, int(round(src_h * 174 / 240)))
        left = max(0, (src_w - crop_w) // 2)
        image = image.crop((left, 0, min(src_w, left + crop_w), src_h))
        image = image.resize(
            IPODJS_WEATHER_PREVIEW_SIZE, Image.Resampling.LANCZOS)
        image = image.convert("RGBA")
        overlay = Image.new(
            "RGBA", IPODJS_WEATHER_PREVIEW_SIZE, (0, 0, 0, 0))
        draw = ImageDraw.Draw(overlay)
        draw.rectangle((0, 0, 173, 49), fill=(4, 28, 51, 232))
        draw.rectangle((0, 159, 173, 239), fill=(3, 23, 42, 238))
        draw.rectangle((0, 48, 173, 51), fill=(32, 181, 226, 255))
        draw.rectangle((0, 158, 173, 161), fill=(32, 181, 226, 255))
        draw.text((10, 6), "LOCAL FORECAST", font=title_font,
                  fill=(242, 249, 255, 255))
        draw.text((10, 27), location[:25], font=location_font,
                  fill=(164, 215, 240, 255))
        temp_text = f"{temperature}°{unit}" if temperature else f"°{unit}"
        draw.text((10, 166), temp_text, font=temp_font,
                  fill=(255, 255, 255, 255))
        draw.text((10, 200), condition[:23], font=detail_font,
                  fill=(205, 235, 249, 255))
        details = (
            f"H {high}°  L {low}°  RAIN {precipitation}%"
            if high or low or precipitation else "CURRENT RADAR"
        )
        draw.text((10, 221), details, font=small_font,
                  fill=(137, 199, 229, 255))
        draw.ellipse((80, 96, 94, 110), fill=(255, 255, 255, 255),
                     outline=(12, 80, 120, 255), width=2)
        draw.line((87, 86, 87, 120), fill=(255, 255, 255, 225), width=1)
        draw.line((77, 103, 97, 103), fill=(255, 255, 255, 225), width=1)
        image = Image.alpha_composite(image, overlay).convert("RGB")

        target = output_dir / f"frame-{index:02d}.bmp"
        temporary = target.with_suffix(".bmp.tmp")
        image.save(temporary, "BMP")
        os.replace(temporary, target)
        generated.append(target)

    return generated


def build_weather_bundle(
        config, output_root=None, device=None, force=False,
        allow_network=True):
    errors = []
    warnings = []
    enabled = bool(_config_value(config, "weather_enabled", True, device=device))
    cache_root = Path(output_root or Path(config.cache_dir) / "weather")
    bundle_dir = cache_root / WEATHER_REL_DIR
    bundle_dir.mkdir(parents=True, exist_ok=True)
    if not enabled:
        return WeatherOutput([], [], str(cache_root), [])

    location = _sanitize(_config_value(config, "weather_location_name", "Moncton, NB", device=device)) or "Moncton, NB"
    latitude = _config_value(config, "weather_latitude", 46.0878, device=device)
    longitude = _config_value(config, "weather_longitude", -64.7782, device=device)
    units = str(_config_value(config, "weather_units", "metric", device=device) or "metric").strip().lower()
    if units not in {"metric", "imperial"}:
        units = "metric"

    forecast_path = bundle_dir / "forecast.tsv"
    manifest_path = bundle_dir / "manifest.json"
    max_age_minutes = _config_value(
        config, "weather_cache_max_age_minutes", 60, device=device)
    allow_stale_cache = bool(_config_value(
        config, "weather_sync_stale_cache", True, device=device))
    cached_rows, cached_hourly_rows = _read_cached_forecast(forecast_path)
    if not _cache_matches(forecast_path, location, latitude, longitude, units):
        cached_rows, cached_hourly_rows = [], []
    cached_forecast_available = bool(cached_rows)
    cached_forecast_fresh = (
        cached_forecast_available
        and _cache_is_fresh(forecast_path, max_age_minutes)
    )
    payload = None
    if allow_network and (force or not cached_forecast_fresh):
        try:
            payload = _fetch_open_meteo(latitude, longitude, units)
        except Exception as exc:
            message = f"Weather forecast fetch failed: {exc}"
            if cached_forecast_available and allow_stale_cache:
                warnings.append(f"{message}; using cached forecast")
            else:
                errors.append(message)
    elif not cached_forecast_available:
        warnings.append(
            "Weather cache is empty; use Device > Sync Weather to refresh it"
        )

    rows = _rows_from_payload(payload or {}) if payload else []
    hourly_rows = _hourly_rows_from_payload(payload or {}) if payload else []
    if rows:
        generated = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        timezone_name = _sanitize((payload or {}).get("timezone") or "")
        header = [
            "rockpod_weather_v1",
            location,
            f"{float(latitude):.4f}",
            f"{float(longitude):.4f}",
            timezone_name,
            generated,
            rows[0][0],
            units,
        ]
        lines = ["\t".join(_sanitize(part) for part in header)]
        lines.extend("\t".join(_sanitize(part) for part in row) for row in rows)
        lines.extend("\t".join(_sanitize(part) for part in row) for row in hourly_rows)
        atomic_write_text(forecast_path, "\n".join(lines) + "\n")
    elif cached_forecast_available and (
            cached_forecast_fresh or allow_stale_cache):
        rows = cached_rows
        hourly_rows = cached_hourly_rows

    forecast_available = bool(rows or hourly_rows)
    if not allow_network:
        if not forecast_available:
            return WeatherOutput([], errors, str(cache_root), warnings)
        cached_paths = [forecast_path]
        if manifest_path.is_file():
            cached_paths.append(manifest_path)
        cached_paths.extend(
            path for path in (bundle_dir / "backgrounds").glob("*.bmp")
            if path.is_file()
        )
        cached_paths.extend(
            path for path in (bundle_dir / "icons").glob("*.bmp")
            if path.is_file()
        )
        cached_paths.extend(_cached_radar_files(bundle_dir))
        cached_paths.extend(
            path for path in (
                cache_root / ".rockbox" / "ipodjs" / "previews" /
                "weather-loop"
            ).glob("frame-*.bmp")
            if path.is_file()
        )
        files = []
        for path in dict.fromkeys(cached_paths):
            rel = path.relative_to(cache_root).as_posix()
            files.append((str(path), rel, compute_file_hash(str(path))))
        return WeatherOutput(files, errors, str(cache_root), warnings)

    copied_backgrounds = _copy_backgrounds(bundle_dir)
    copied_icons = _copy_icons(bundle_dir)
    radar_files = []
    if bool(_config_value(
            config, "weather_maps_enabled", True, device=device)):
        cached_radar = _cached_radar_files(bundle_dir)
        cached_radar_fresh = bool(cached_radar) and _cache_is_fresh(
            bundle_dir / "maps" / "radar.json", max_age_minutes)
        if cached_radar_fresh and not force:
            radar_files = cached_radar
        elif allow_network:
            try:
                radar_files = _fetch_radar_maps(
                    bundle_dir, latitude, longitude)
            except Exception as exc:
                warnings.append(f"Weather radar update failed: {exc}")
                if cached_radar and allow_stale_cache:
                    radar_files = cached_radar
        elif cached_radar and allow_stale_cache:
            radar_files = cached_radar
    preview_files = _render_ipodjs_weather_previews(
        cache_root, radar_files, location, units, rows, hourly_rows)
    manifest = {
        "version": 2,
        "generated_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "provider": "open-meteo",
        "location_name": location,
        "latitude": latitude,
        "longitude": longitude,
        "units": units,
        "forecast_rows": len(rows),
        "hourly_rows": len(hourly_rows),
        "radar_frames": len([
            path for path in radar_files if path.suffix.lower() == ".png"
        ]),
        "right_pane_frames": len(preview_files),
        "warnings": warnings,
        "errors": errors,
    }
    atomic_write_json(manifest_path, manifest)

    files = []
    forecast_files = [forecast_path] if forecast_available else []
    for path in (
            forecast_files + [manifest_path] + copied_backgrounds +
            copied_icons + radar_files + preview_files):
        if not path.is_file():
            continue
        rel = path.relative_to(cache_root).as_posix()
        files.append((str(path), rel, compute_file_hash(str(path))))
    return WeatherOutput(files, errors, str(cache_root), warnings)


def format_weather_report(report):
    lines = [f"Output: {report.output_root}", f"Files: {len(report.files)}"]
    lines.extend(f"Warning: {warning}" for warning in report.warnings)
    lines.extend(f"Warning: {err}" for err in report.errors)
    return "\n".join(lines)
