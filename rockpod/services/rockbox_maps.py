"""Offline Maps location, photo, and GPX route bundle for Rockbox."""

from __future__ import annotations

import os
import re
import tempfile
import concurrent.futures
import io
import json
import math
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET

from PIL import Image, ImageOps, ExifTags, UnidentifiedImageError


MAPS_RELATIVE_PATH = ".rockbox/maps/location.v1.tsv"
WORLD_ATLAS_RELATIVE_DIR = ".rockbox/maps/world"
WORLD_TILE_SIZE = (320, 160)
MAX_PHOTOS = 12
MAX_ROUTE_POINTS = 24
MAX_HOTSPOTS = 48
MAP_PHOTO_SIZE = (40, 30)
MAP_PHOTO_DIR = ".rockbox/maps/photos"
PHOTOS_LOCKS_RELATIVE_PATH = ".rockbox/rocks/apps/data/photos.locks"
NOMINATIM_SEARCH_URL = "https://nominatim.openstreetmap.org/search"
OVERPASS_URLS = (
    "https://overpass.kumi.systems/api/interpreter",
    "https://overpass.private.coffee/api/interpreter",
    "https://overpass-api.de/api/interpreter",
)
WORLD_IMAGERY_URL = (
    "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/"
    "MapServer/tile/{zoom}/{y}/{x}"
)
HTTP_HEADERS = {"User-Agent": "RockPod offline Maps sync/1.1"}


def _clean(value, limit=48):
    return " ".join(str(value or "").replace("\t", " ").replace("\n", " ").split())[:limit]


def _coordinate(value, minimum, maximum):
    value = float(value)
    if not minimum <= value <= maximum:
        raise ValueError(f"coordinate must be between {minimum} and {maximum}")
    return value


def _rational(value):
    try:
        return float(value)
    except (TypeError, ValueError):
        numerator = getattr(value, "numerator", None)
        denominator = getattr(value, "denominator", None)
        if numerator is None or not denominator:
            raise ValueError("invalid EXIF GPS value")
        return float(numerator) / float(denominator)


def _gps_from_photo(path):
    try:
        with Image.open(path) as image:
            exif = image.getexif()
            gps = exif.get_ifd(ExifTags.IFD.GPSInfo) if exif else {}
    except (OSError, UnidentifiedImageError, AttributeError):
        return None
    if not gps:
        return None
    tags = {ExifTags.GPSTAGS.get(key, key): value for key, value in gps.items()}
    try:
        lat = tags["GPSLatitude"]
        lon = tags["GPSLongitude"]
        latitude = _rational(lat[0]) + _rational(lat[1]) / 60 + _rational(lat[2]) / 3600
        longitude = _rational(lon[0]) + _rational(lon[1]) / 60 + _rational(lon[2]) / 3600
        if str(tags.get("GPSLatitudeRef", "N")).upper() == "S":
            latitude = -latitude
        if str(tags.get("GPSLongitudeRef", "E")).upper() == "W":
            longitude = -longitude
        return (_coordinate(latitude, -90, 90), _coordinate(longitude, -180, 180))
    except (KeyError, IndexError, TypeError, ValueError):
        return None


class RockboxMapsService:
    """Writes a bounded, text-only bundle consumed by ``nb_maps.rock``."""

    @staticmethod
    def _locked_photo_paths(profile):
        mount = os.path.abspath(str(profile.get("device_mount_path") or ""))
        path = os.path.join(mount, PHOTOS_LOCKS_RELATIVE_PATH)
        locked = set()
        try:
            with open(path, "r", encoding="utf-8") as handle:
                for line in handle:
                    relative, separator, pin = line.rstrip("\n").rpartition("|")
                    if separator and len(pin) == 4 and pin.isdigit():
                        locked.add(relative.replace("\\", "/").lstrip("/").casefold())
        except OSError:
            pass
        return locked

    @staticmethod
    def _rgb565_thumbnail(source, target):
        with Image.open(source) as image:
            image = ImageOps.exif_transpose(image).convert("RGB")
            image.thumbnail(MAP_PHOTO_SIZE, Image.Resampling.LANCZOS)
            canvas = Image.new("RGB", MAP_PHOTO_SIZE, (255, 255, 255))
            x = (MAP_PHOTO_SIZE[0] - image.width) // 2
            y = (MAP_PHOTO_SIZE[1] - image.height) // 2
            canvas.paste(image, (x, y))
        payload = bytearray(MAP_PHOTO_SIZE[0] * MAP_PHOTO_SIZE[1] * 2)
        offset = 0
        for red, green, blue in canvas.getdata():
            value = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
            payload[offset] = value & 0xFF
            payload[offset + 1] = value >> 8
            offset += 2
        with open(target, "wb") as handle:
            handle.write(payload)

    @staticmethod
    def _request_json(url, params=None, data=None, timeout=45):
        if params:
            url = "%s?%s" % (url, urllib.parse.urlencode(params))
        request = urllib.request.Request(
            url,
            data=data.encode("utf-8") if isinstance(data, str) else data,
            headers=HTTP_HEADERS,
        )
        with urllib.request.urlopen(request, timeout=timeout) as response:
            return json.loads(response.read().decode("utf-8"))

    def resolve_location(self, profile):
        """Return the configured location, geocoding its name when needed."""
        name = _clean(profile.get("weather_location_name") or "Current Location")
        latitude = profile.get("weather_latitude")
        longitude = profile.get("weather_longitude")
        try:
            latitude = _coordinate(latitude, -90, 90)
            longitude = _coordinate(longitude, -180, 180)
            if latitude or longitude:
                return name, latitude, longitude
        except (TypeError, ValueError):
            pass
        if not name or name == "Current Location":
            raise ValueError("Set a city or postal code in Device weather settings.")
        results = self._request_json(
            NOMINATIM_SEARCH_URL,
            {"q": name, "format": "jsonv2", "limit": 1},
        )
        if not results:
            raise ValueError("The saved location name could not be found online.")
        return (
            name,
            _coordinate(results[0]["lat"], -90, 90),
            _coordinate(results[0]["lon"], -180, 180),
        )

    @staticmethod
    def _hotspot_kind(tags):
        amenity = str(tags.get("amenity") or "")
        tourism = str(tags.get("tourism") or "")
        if amenity in {"bar", "pub", "biergarten", "nightclub"}:
            return "d"
        if amenity in {"restaurant", "cafe", "fast_food", "food_court"}:
            return "f"
        if tourism in {"attraction", "museum", "viewpoint", "gallery",
                       "theme_park", "zoo"}:
            return "a"
        if tourism in {"hotel", "motel", "hostel", "guest_house"}:
            return "s"
        if amenity == "fuel":
            return "g"
        if amenity in {"hospital", "clinic", "pharmacy"}:
            return "h"
        return ""

    def nearby_hotspots(self, latitude, longitude, radius=8000):
        """Fetch a bounded, named POI set around the current location."""
        latitude = _coordinate(latitude, -90, 90)
        longitude = _coordinate(longitude, -180, 180)
        filters = (
            ("amenity", "restaurant|cafe|fast_food|food_court|bar|pub|"
                        "biergarten|nightclub|fuel|hospital|clinic|pharmacy"),
            ("tourism", "attraction|museum|viewpoint|gallery|theme_park|zoo|"
                        "hotel|motel|hostel|guest_house"),
        )
        elements = []
        last_error = None
        for key, values in filters:
            query = (
                "[out:json][timeout:25];nwr(around:%d,%.6f,%.6f)"
                "[%s~\"^(%s)$\"][name];out center tags;"
            ) % (radius, latitude, longitude, key, values)
            for endpoint in OVERPASS_URLS:
                try:
                    payload = self._request_json(
                        endpoint, data=query, timeout=25
                    )
                    elements.extend(payload.get("elements", []))
                    break
                except (OSError, ValueError) as error:
                    last_error = error
        if not elements and last_error:
            raise last_error
        records = []
        seen = set()
        for item in elements:
            tags = item.get("tags") or {}
            name = _clean(tags.get("name") or tags.get("brand"), 23)
            kind = self._hotspot_kind(tags)
            center = item.get("center") or item
            if not name or not kind or "lat" not in center or "lon" not in center:
                continue
            lat = _coordinate(center["lat"], -90, 90)
            lon = _coordinate(center["lon"], -180, 180)
            key = (name.casefold(), round(lat, 5), round(lon, 5))
            if key in seen:
                continue
            seen.add(key)
            distance = (lat - latitude) ** 2 + (
                (lon - longitude) * math.cos(math.radians(latitude))
            ) ** 2
            records.append((distance, name, lat, lon, kind))
        records.sort(key=lambda item: (item[0], item[1].casefold()))
        return [
            {"name": name, "latitude": lat, "longitude": lon, "kind": kind}
            for _distance, name, lat, lon, kind in records[:MAX_HOTSPOTS]
        ]

    def cached_hotspots(self, profile):
        """Read the last good bounded POI set without altering the bundle."""
        mount = os.path.abspath(str(profile.get("device_mount_path") or ""))
        path = os.path.join(mount, MAPS_RELATIVE_PATH)
        records = []
        try:
            with open(path, "r", encoding="utf-8") as handle:
                for line in handle:
                    fields = line.rstrip("\n").split("\t", 4)
                    if len(fields) != 5 or fields[0] != "poi":
                        continue
                    records.append({
                        "latitude": int(fields[1]) / 1000000.0,
                        "longitude": int(fields[2]) / 1000000.0,
                        "kind": fields[3],
                        "name": fields[4],
                    })
                    if len(records) >= MAX_HOTSPOTS:
                        break
        except (OSError, ValueError):
            return []
        return records

    def sync_location_atlas(self, profile, latitude, longitude):
        """Refresh native z9-z18 imagery around the synced location."""
        mount = os.path.abspath(str(profile.get("device_mount_path") or ""))
        if not os.path.isdir(mount):
            raise ValueError("The selected iPod mount path is not available.")
        destination = os.path.join(mount, WORLD_ATLAS_RELATIVE_DIR)
        jobs = set()
        for zoom, radius in (
            (9, 1), (10, 1), (11, 2), (12, 3), (13, 4), (14, 6),
            (15, 12), (16, 18),
            (17, 24), (18, 28),
        ):
            scale = 1 << zoom
            x = int((longitude + 180.0) / 360.0 * scale)
            radians = math.radians(max(-85.05112878, min(85.05112878, latitude)))
            y = int((1.0 - math.asinh(math.tan(radians)) / math.pi) * scale / 2.0)
            for dx in range(-radius, radius + 1):
                for dy in range(-radius, radius + 1):
                    jobs.add((zoom, (x + dx) % scale,
                              max(0, min(scale - 1, y + dy))))
        def sync_tile(job):
            zoom, x, y = job
            directory = os.path.join(destination, str(zoom))
            target = os.path.join(directory, f"{x}_{y}.r16")
            os.makedirs(directory, exist_ok=True)
            if os.path.isfile(target) and os.path.getsize(target) == 320 * 160 * 2:
                return 0
            request = urllib.request.Request(
                WORLD_IMAGERY_URL.format(zoom=zoom, x=x, y=y),
                headers=HTTP_HEADERS,
            )
            with urllib.request.urlopen(request, timeout=45) as response:
                with Image.open(io.BytesIO(response.read())) as source:
                    image = source.convert("RGB")
                payload = RockboxWorldAtlasService._rgb565(image)
            fd, temporary = tempfile.mkstemp(prefix="tile.", suffix=".tmp",
                                              dir=directory)
            try:
                with os.fdopen(fd, "wb") as handle:
                    handle.write(payload)
                os.replace(temporary, target)
            except Exception:
                try:
                    os.unlink(temporary)
                except OSError:
                    pass
                raise
            return 1

        written = 0
        with concurrent.futures.ThreadPoolExecutor(max_workers=12) as pool:
            for result in pool.map(sync_tile, sorted(jobs)):
                written += result
        return {"tiles": written, "path": destination}

    def geotagged_photos(self, profile):
        root_value = str(profile.get("photos_library_path") or "").strip()
        if not root_value:
            return []
        root = os.path.abspath(root_value)
        if not os.path.isdir(root):
            return []
        locked = self._locked_photo_paths(profile)
        records = []
        for current_root, _dirs, files in os.walk(root):
            for filename in sorted(files):
                if os.path.splitext(filename)[1].lower() not in {".jpg", ".jpeg", ".png", ".tif", ".tiff"}:
                    continue
                source = os.path.join(current_root, filename)
                relative = os.path.relpath(source, root).replace(os.sep, "/")
                device_relative = os.path.splitext(relative)[0] + ".jpg"
                lock_key = device_relative.casefold()
                if lock_key in locked or any(
                    lock_key.startswith(item.rstrip("/") + "/") for item in locked
                ):
                    continue
                point = _gps_from_photo(source)
                if not point:
                    continue
                records.append((os.path.basename(filename), relative, point[0], point[1]))
                if len(records) >= MAX_PHOTOS:
                    return records
        return records

    def parse_gpx(self, path):
        root = ET.parse(path).getroot()
        points = []
        for node in root.iter():
            if node.tag.rsplit("}", 1)[-1] not in {"trkpt", "rtept"}:
                continue
            try:
                points.append((_coordinate(node.attrib["lat"], -90, 90),
                               _coordinate(node.attrib["lon"], -180, 180)))
            except (KeyError, ValueError):
                continue
        if not points:
            raise ValueError("The GPX file has no usable track or route points.")
        stride = max(1, (len(points) + MAX_ROUTE_POINTS - 1) // MAX_ROUTE_POINTS)
        sampled = points[::stride][:MAX_ROUTE_POINTS]
        if sampled[-1] != points[-1] and len(sampled) < MAX_ROUTE_POINTS:
            sampled.append(points[-1])
        return sampled

    def write_bundle(self, profile, name, latitude, longitude, route_name="",
                     route_points=None, hotspots=None):
        mount = os.path.abspath(str(profile.get("device_mount_path") or ""))
        if not os.path.isdir(mount):
            raise ValueError("The selected iPod mount path is not available.")
        latitude = _coordinate(latitude, -90, 90)
        longitude = _coordinate(longitude, -180, 180)
        lines = ["rockpod-maps-v1", "location\t%d\t%d\t%s" % (
            round(latitude * 1000000), round(longitude * 1000000), _clean(name) or "Current Location")]
        # Categories are deliberate offline entry points.  They do not imply live POI search.
        for category in ("Nearby", "Food", "Fuel", "Health", "Photos"):
            lines.append("category\t%s" % category)
        for hotspot in list(hotspots or [])[:MAX_HOTSPOTS]:
            lines.append("poi\t%d\t%d\t%s\t%s" % (
                round(_coordinate(hotspot["latitude"], -90, 90) * 1000000),
                round(_coordinate(hotspot["longitude"], -180, 180) * 1000000),
                str(hotspot.get("kind") or "f")[:1],
                _clean(hotspot.get("name"), 23),
            ))
        photos = self.geotagged_photos(profile)
        cache_dir = os.path.join(
            str(profile.get("source_repo_path") or os.getcwd()), "rockpod",
            ".generated", "maps", str(profile.get("id") or "default"), "photos"
        )
        os.makedirs(cache_dir, exist_ok=True)
        photo_assets = []
        for title, relative, lat, lon in photos:
            index = len(photo_assets)
            thumb_name = f"photo_{index:02d}.r16"
            thumb_cache = os.path.join(cache_dir, thumb_name)
            source = os.path.join(str(profile.get("photos_library_path") or ""), relative)
            try:
                self._rgb565_thumbnail(source, thumb_cache)
            except (OSError, UnidentifiedImageError):
                continue
            lines.append("photo\t%d\t%d\t%s\t%s" % (
                round(lat * 1000000), round(lon * 1000000), _clean(title), thumb_name))
            photo_assets.append((thumb_cache, os.path.join(mount, MAP_PHOTO_DIR, thumb_name)))
        if route_points:
            lines.append("route\t%s" % (_clean(route_name) or "GPS Route"))
            for lat, lon in route_points[:MAX_ROUTE_POINTS]:
                lines.append("point\t%d\t%d" % (round(lat * 1000000), round(lon * 1000000)))
        target = os.path.join(mount, MAPS_RELATIVE_PATH)
        os.makedirs(os.path.dirname(target), exist_ok=True)
        fd, temporary = tempfile.mkstemp(prefix="location.", suffix=".tmp", dir=os.path.dirname(target), text=True)
        try:
            with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as handle:
                handle.write("\n".join(lines) + "\n")
            os.replace(temporary, target)
        except Exception:
            try:
                os.unlink(temporary)
            except OSError:
                pass
            raise
        os.makedirs(os.path.join(mount, MAP_PHOTO_DIR), exist_ok=True)
        for source, thumbnail_target in photo_assets:
            with open(source, "rb") as input_handle, open(thumbnail_target + ".tmp", "wb") as output_handle:
                output_handle.write(input_handle.read())
            os.replace(thumbnail_target + ".tmp", thumbnail_target)
        return {"path": target, "photos": len(photo_assets),
                "route_points": len(route_points or []),
                "hotspots": min(len(hotspots or []), MAX_HOTSPOTS)}


class RockboxWorldAtlasService:
    """Convert a standard ``z/x/y`` raster atlas to fixed iPod map frames.

    The iPod never resizes or decodes source imagery.  Each installed tile is
    an exact 320x160 RGB565 little-endian frame at
    ``.rockbox/maps/world/z/x_y.r16``.
    """

    _tile_path = re.compile(r"(?:^|/)(\d+)/(\d+)/(\d+)\.(?:jpg|jpeg|png|webp)$", re.I)

    @staticmethod
    def _rgb565(image):
        image = image.convert("RGB").resize(WORLD_TILE_SIZE, Image.Resampling.LANCZOS)
        data = bytearray(WORLD_TILE_SIZE[0] * WORLD_TILE_SIZE[1] * 2)
        offset = 0
        for red, green, blue in image.getdata():
            value = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
            data[offset] = value & 0xFF
            data[offset + 1] = value >> 8
            offset += 2
        return bytes(data)

    def sync_atlas(self, profile, source_root, maximum_zoom=8):
        mount = os.path.abspath(str(profile.get("device_mount_path") or ""))
        source_root = os.path.abspath(str(source_root or ""))
        if not os.path.isdir(mount):
            raise ValueError("The selected iPod mount path is not available.")
        if not os.path.isdir(source_root):
            raise ValueError("Choose a directory containing z/x/y satellite tiles.")
        destination_root = os.path.join(mount, WORLD_ATLAS_RELATIVE_DIR)
        converted = 0
        skipped = 0
        for current_root, _dirs, files in os.walk(source_root):
            for filename in files:
                source = os.path.join(current_root, filename)
                relative = os.path.relpath(source, source_root).replace(os.sep, "/")
                match = self._tile_path.search(relative)
                if not match:
                    continue
                zoom, x, y = (int(item) for item in match.groups())
                if zoom > int(maximum_zoom) or x >= (1 << zoom) or y >= (1 << zoom):
                    skipped += 1
                    continue
                try:
                    with Image.open(source) as image:
                        payload = self._rgb565(image)
                except (OSError, UnidentifiedImageError):
                    skipped += 1
                    continue
                destination_dir = os.path.join(destination_root, str(zoom))
                os.makedirs(destination_dir, exist_ok=True)
                target = os.path.join(destination_dir, f"{x}_{y}.r16")
                fd, temporary = tempfile.mkstemp(prefix="tile.", suffix=".tmp", dir=destination_dir)
                try:
                    with os.fdopen(fd, "wb") as handle:
                        handle.write(payload)
                    os.replace(temporary, target)
                except Exception:
                    try:
                        os.unlink(temporary)
                    except OSError:
                        pass
                    raise
                converted += 1
        return {"tiles": converted, "skipped": skipped, "path": destination_root}
