#!/usr/bin/env python3
"""One-click Maps location, fresh hotspot, and city imagery refresh."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "rockpod"))

DEFAULT_LOCATION = (
    "247 Dominion St, Moncton, NB",
    46.093520297636,
    -64.7934201321174,
)

from services.rockbox_maps import RockboxMapsService  # noqa: E402


def saved_location() -> tuple[str, float, float]:
    config_path = Path.home() / ".rockpod" / "config.json"
    try:
        config = json.loads(config_path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        config = {}
    return (
        str(
            config.get("maps_location_name")
            or config.get("weather_location_name")
            or DEFAULT_LOCATION[0]
        ),
        float(
            config.get("maps_latitude")
            or config.get("weather_latitude")
            or DEFAULT_LOCATION[1]
        ),
        float(
            config.get("maps_longitude")
            or config.get("weather_longitude")
            or DEFAULT_LOCATION[2]
        ),
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mount", type=Path, help="mounted iPod or simdisk")
    parser.add_argument("--name")
    parser.add_argument("--latitude", type=float)
    parser.add_argument("--longitude", type=float)
    parser.add_argument(
        "--reuse-hotspots", action="store_true",
        help="keep the last good POI set while refreshing imagery",
    )
    args = parser.parse_args()
    saved_name, saved_latitude, saved_longitude = saved_location()
    profile = {
        "device_mount_path": str(args.mount.resolve()),
        "source_repo_path": str(REPO_ROOT),
        "id": "current-location",
        "weather_location_name": args.name or saved_name,
        "weather_latitude": (
            args.latitude if args.latitude is not None else saved_latitude
        ),
        "weather_longitude": (
            args.longitude if args.longitude is not None else saved_longitude
        ),
    }
    service = RockboxMapsService()
    name, latitude, longitude = service.resolve_location(profile)
    print(f"location {name}: {latitude:.6f}, {longitude:.6f}", flush=True)
    hotspots = (
        service.cached_hotspots(profile) if args.reuse_hotspots
        else service.nearby_hotspots(latitude, longitude)
    )
    bundle = service.write_bundle(
        profile, name, latitude, longitude, hotspots=hotspots
    )
    atlas = service.sync_location_atlas(profile, latitude, longitude)
    print(
        f"synced {bundle['hotspots']} hotspots and {atlas['tiles']} "
        "z9-z18 satellite tiles",
        flush=True,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
