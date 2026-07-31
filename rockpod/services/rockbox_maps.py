"""Offline Maps location, photo, and GPX route bundle for Rockbox."""

from __future__ import annotations

import os
import re
import tempfile
import xml.etree.ElementTree as ET

from PIL import Image, ImageOps, ExifTags, UnidentifiedImageError


MAPS_RELATIVE_PATH = ".rockbox/maps/location.v1.tsv"
WORLD_ATLAS_RELATIVE_DIR = ".rockbox/maps/world"
WORLD_TILE_SIZE = (320, 160)
MAX_PHOTOS = 12
MAX_ROUTE_POINTS = 24
MAP_PHOTO_SIZE = (40, 30)
MAP_PHOTO_DIR = ".rockbox/maps/photos"
PHOTOS_LOCKS_RELATIVE_PATH = ".rockbox/rocks/apps/data/photos.locks"


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

    def geotagged_photos(self, profile):
        root = os.path.abspath(str(profile.get("photos_library_path") or ""))
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

    def write_bundle(self, profile, name, latitude, longitude, route_name="", route_points=None):
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
        return {"path": target, "photos": len(photo_assets), "route_points": len(route_points or [])}


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
