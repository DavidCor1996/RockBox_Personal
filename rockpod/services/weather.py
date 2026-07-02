"""Generate Rockbox-ready offline weather bundles."""

from __future__ import annotations

import json
import os
import shutil
import time
import urllib.parse
import urllib.request
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path

from services.file_safety import atomic_write_json, atomic_write_text
from services.metadata_reader import compute_file_hash


WEATHER_REL_DIR = os.path.join(".rockbox", "rockpod", "weather")
BACKGROUND_NAMES = ("clear_day.bmp", "rain_day.bmp", "snow_day.bmp", "night.bmp")
ASSET_BACKGROUND_DIR = Path(__file__).resolve().parents[1] / "assets" / "weather" / "backgrounds"
ASSET_ICON_DIR = Path(__file__).resolve().parents[1] / "assets" / "weather" / "icons"


@dataclass
class WeatherOutput:
    files: list[tuple[str, str, str]]
    errors: list[str]
    output_root: str


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
    with urllib.request.urlopen(request, timeout=20) as response:
        return json.loads(response.read().decode("utf-8"))


def _rows_from_payload(payload):
    daily = payload.get("daily") or {}
    times = daily.get("time") or []
    rows = []
    for idx, day in enumerate(times[:7]):
        code, text = _weather_code((daily.get("weather_code") or [None] * 7)[idx])
        sunrise = str((daily.get("sunrise") or [""] * 7)[idx] or "")
        sunset = str((daily.get("sunset") or [""] * 7)[idx] or "")
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


def build_weather_bundle(config, output_root=None, device=None, force=False):
    errors = []
    enabled = bool(_config_value(config, "weather_enabled", True, device=device))
    cache_root = Path(output_root or Path(config.cache_dir) / "weather")
    bundle_dir = cache_root / WEATHER_REL_DIR
    bundle_dir.mkdir(parents=True, exist_ok=True)
    if not enabled:
        return WeatherOutput([], [], str(cache_root))

    location = _sanitize(_config_value(config, "weather_location_name", "Moncton, NB", device=device)) or "Moncton, NB"
    latitude = _config_value(config, "weather_latitude", 46.0878, device=device)
    longitude = _config_value(config, "weather_longitude", -64.7782, device=device)
    units = str(_config_value(config, "weather_units", "metric", device=device) or "metric").strip().lower()
    if units not in {"metric", "imperial"}:
        units = "metric"

    forecast_path = bundle_dir / "forecast.tsv"
    manifest_path = bundle_dir / "manifest.json"
    payload = None
    try:
        payload = _fetch_open_meteo(latitude, longitude, units)
    except Exception as exc:
        errors.append(f"Weather forecast fetch failed: {exc}")

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
    elif not forecast_path.exists():
        generated = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        atomic_write_text(
            forecast_path,
            "\t".join(["rockpod_weather_v1", location, str(latitude), str(longitude), "", generated, "", units]) + "\n",
        )

    copied_backgrounds = _copy_backgrounds(bundle_dir)
    copied_icons = _copy_icons(bundle_dir)
    manifest = {
        "version": 1,
        "generated_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "provider": "open-meteo",
        "location_name": location,
        "latitude": latitude,
        "longitude": longitude,
        "units": units,
        "forecast_rows": len(rows),
        "hourly_rows": len(hourly_rows),
        "warnings": errors,
    }
    atomic_write_json(manifest_path, manifest)

    files = []
    for path in [forecast_path, manifest_path] + copied_backgrounds + copied_icons:
        if not path.is_file():
            continue
        rel = path.relative_to(cache_root).as_posix()
        files.append((str(path), rel, compute_file_hash(str(path))))
    return WeatherOutput(files, errors, str(cache_root))


def format_weather_report(report):
    lines = [f"Output: {report.output_root}", f"Files: {len(report.files)}"]
    lines.extend(f"Warning: {err}" for err in report.errors)
    return "\n".join(lines)
