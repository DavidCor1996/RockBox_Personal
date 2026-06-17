"""Photo library indexing and scoped sync/remove workflows."""

from __future__ import annotations

import os

from PIL import Image, ImageOps, UnidentifiedImageError


SUPPORTED_PHOTO_EXTENSIONS = {".bmp", ".gif", ".jpg", ".jpe", ".jpeg", ".png", ".ppm"}
PHOTO_TARGET_DIR = "Photos"
PHOTO_THUMB_DIR = "Photos/.photo_thumbs"
DEVICE_PHOTO_MAX_SIZE = (800, 800)
DEVICE_PHOTO_JPEG_QUALITY = 85
THUMBNAIL_MAX_SIZE = (64, 48)


class RockboxPhotoService:
    """Manage user-supplied photos for the Rockbox Photos plugin."""

    def list_photos(self, profile, simulator_target=None):
        library_path = os.path.abspath(profile.get("photos_library_path") or "")
        device_root = self.photo_target_root(profile, "device")
        sim_root = self.photo_target_root(profile, "simulator", simulator_target)
        photos = []
        relpaths = set()

        if library_path and os.path.isdir(library_path):
            for source_path in self._scan_photo_files(library_path):
                relpath = self._relative_photo_path(library_path, source_path)
                stat = os.stat(source_path)
                photos.append(
                    {
                        "id": relpath,
                        "title": os.path.basename(source_path),
                        "filename": os.path.basename(source_path),
                        "relative_path": relpath,
                        "device_relative_path": self._device_photo_relpath(relpath),
                        "source_path": source_path,
                        "size": stat.st_size,
                        "modified_time": stat.st_mtime,
                        "on_device": os.path.isfile(os.path.join(device_root, self._device_photo_relpath(relpath))) if device_root else False,
                        "on_simulator": os.path.isfile(os.path.join(sim_root, self._device_photo_relpath(relpath))) if sim_root else False,
                        "missing_source": False,
                    }
                )
                relpaths.add(relpath)
                relpaths.add(self._device_photo_relpath(relpath))

        for root, key in ((device_root, "on_device"), (sim_root, "on_simulator")):
            if not root or not os.path.isdir(root):
                continue
            for target_path in self._scan_photo_files(root):
                relpath = self._relative_photo_path(root, target_path)
                if relpath in relpaths:
                    continue
                stat = os.stat(target_path)
                photos.append(
                    {
                        "id": relpath,
                        "title": os.path.basename(target_path),
                        "filename": os.path.basename(target_path),
                        "relative_path": relpath,
                        "device_relative_path": relpath,
                        "source_path": "",
                        "size": stat.st_size,
                        "modified_time": stat.st_mtime,
                        "on_device": key == "on_device",
                        "on_simulator": key == "on_simulator",
                        "missing_source": True,
                    }
                )
                relpaths.add(relpath)

        return sorted(photos, key=lambda item: item["relative_path"].lower())

    def mount_root(self, profile, target_mode="device", simulator_target=None):
        mount_root = profile.get("device_mount_path") or ""
        if target_mode == "simulator":
            mount_root = (
                profile.get("simulator_simdisk_path")
                or (simulator_target or {}).get("simdisk_path")
                or ""
            )
        return os.path.abspath(mount_root) if mount_root else ""

    def photo_target_root(self, profile, target_mode="device", simulator_target=None):
        mount_root = self.mount_root(profile, target_mode, simulator_target)
        if not mount_root:
            return ""
        return os.path.abspath(os.path.join(mount_root, self._photo_target_dir(profile, target_mode).lstrip("/")))

    def target_root(self, profile, target_mode="device", simulator_target=None):
        return self.photo_target_root(profile, target_mode, simulator_target)

    def deploy_profile(self, profile, target_mode="device", simulator_target=None):
        mount_root = self.mount_root(profile, target_mode, simulator_target)
        backup_root = os.path.join(profile["source_repo_path"], "rockpod", ".backups", "photos", profile["id"], target_mode)
        return {
            "id": f"{profile['id']}-photos-{target_mode}",
            "name": f"{profile['name']} Photos ({target_mode})",
            "device_mount_path": os.path.abspath(mount_root) if mount_root else "",
            "target_device_model": profile.get("target_device_model", ""),
            "screen_resolution": profile.get("screen_resolution", ""),
            "source_repo_path": profile["source_repo_path"],
            "selected_theme": profile.get("selected_theme", ""),
            "backup_location": os.path.abspath(backup_root),
        }

    def build_sync_bundle(self, profile, photos, target_mode="device"):
        target_dir = self._photo_target_dir(profile, target_mode).rstrip("/")
        thumb_dir = self._photo_thumb_dir(profile, target_mode).rstrip("/")
        assets = []
        for photo in photos:
            prepared = self._prepare_photo_for_device(profile, photo, target_mode)
            relpath = prepared["device_relative_path"].lstrip("/")
            assets.append(
                {
                    "kind": "photo",
                    "source_rel": photo["relative_path"].lstrip("/"),
                    "source_abs": prepared["photo_abs"],
                    "destination_rel": f"{target_dir}/{relpath}",
                    "exists": bool(prepared["photo_abs"] and os.path.isfile(prepared["photo_abs"])),
                    "size": prepared.get("photo_size", 0),
                    "preserve_metadata": False,
                }
            )
            assets.append(
                {
                    "kind": "photo_thumb",
                    "source_rel": photo["relative_path"].lstrip("/"),
                    "source_abs": prepared["thumb_abs"],
                    "destination_rel": f"{thumb_dir}/{relpath}.bmp",
                    "exists": bool(prepared["thumb_abs"] and os.path.isfile(prepared["thumb_abs"])),
                    "size": prepared.get("thumb_size", 0),
                    "preserve_metadata": False,
                }
            )
            original_relpath = photo["relative_path"].lstrip("/")
            if original_relpath and original_relpath != relpath:
                assets.append(
                    {
                        "kind": "photo_stale_original",
                        "source_rel": "",
                        "source_abs": "",
                        "destination_rel": f"{target_dir}/{original_relpath}",
                        "exists": False,
                        "action": "remove",
                    }
                )
                assets.append(
                    {
                        "kind": "photo_stale_thumb",
                        "source_rel": "",
                        "source_abs": "",
                        "destination_rel": f"{thumb_dir}/{original_relpath}.bmp",
                        "exists": False,
                        "action": "remove",
                    }
                )
        return {
            "id": f"photos-sync-{profile['id']}",
            "name": "Rockbox Photo Sync",
            "assets": assets,
        }

    def build_remove_bundle(self, profile, photos, target_mode="device"):
        target_dir = self._photo_target_dir(profile, target_mode).rstrip("/")
        thumb_dir = self._photo_thumb_dir(profile, target_mode).rstrip("/")
        assets = []
        for photo in photos:
            relpath = str(photo.get("device_relative_path") or self._device_photo_relpath(photo["relative_path"])).lstrip("/")
            assets.append(
                {
                    "kind": "photo",
                    "source_rel": "",
                    "source_abs": "",
                    "destination_rel": f"{target_dir}/{relpath}",
                    "exists": False,
                    "action": "remove",
                }
            )
            assets.append(
                {
                    "kind": "photo_thumb",
                    "source_rel": "",
                    "source_abs": "",
                    "destination_rel": f"{thumb_dir}/{relpath}.bmp",
                    "exists": False,
                    "action": "remove",
                }
            )
        return {
            "id": f"photos-remove-{profile['id']}",
            "name": "Remove Rockbox Photos",
            "assets": assets,
        }

    def _photo_target_dir(self, profile, target_mode="device"):
        if target_mode == "simulator":
            return str(profile.get("photos_simulator_target_dir") or PHOTO_TARGET_DIR).strip() or PHOTO_TARGET_DIR
        return str(profile.get("photos_device_target_dir") or PHOTO_TARGET_DIR).strip() or PHOTO_TARGET_DIR

    def _photo_thumb_dir(self, profile, target_mode="device"):
        target_dir = self._photo_target_dir(profile, target_mode).rstrip("/")
        return f"{target_dir}/.photo_thumbs"

    def _prepare_photo_for_device(self, profile, photo, target_mode="device"):
        source_path = str(photo.get("source_path") or "")
        relpath = str(photo.get("relative_path") or "").lstrip("/")
        device_relpath = self._device_photo_relpath(relpath)
        if not source_path or not os.path.isfile(source_path):
            return {
                "device_relative_path": device_relpath,
                "photo_abs": "",
                "thumb_abs": "",
                "photo_size": 0,
                "thumb_size": 0,
            }

        cache_root = self._photo_cache_root(profile, target_mode)
        photo_cache = os.path.join(cache_root, "photos", device_relpath)
        thumb_cache = os.path.join(cache_root, "thumbs", f"{device_relpath}.bmp")
        os.makedirs(os.path.dirname(photo_cache), exist_ok=True)
        os.makedirs(os.path.dirname(thumb_cache), exist_ok=True)

        try:
            with Image.open(source_path) as image:
                image = ImageOps.exif_transpose(image)
                image = self._rgb_image(image)

                device_image = image.copy()
                device_image.thumbnail(DEVICE_PHOTO_MAX_SIZE, Image.Resampling.LANCZOS)
                device_image.save(
                    photo_cache,
                    "JPEG",
                    quality=DEVICE_PHOTO_JPEG_QUALITY,
                    optimize=True,
                    progressive=False,
                )

                thumb = image.copy()
                thumb.thumbnail(self._thumbnail_size(profile), Image.Resampling.LANCZOS)
                thumb.save(thumb_cache, "BMP")
        except (OSError, UnidentifiedImageError):
            return {
                "device_relative_path": relpath,
                "photo_abs": source_path,
                "thumb_abs": "",
                "photo_size": os.path.getsize(source_path),
                "thumb_size": 0,
            }

        return {
            "device_relative_path": device_relpath,
            "photo_abs": photo_cache,
            "thumb_abs": thumb_cache,
            "photo_size": os.path.getsize(photo_cache),
            "thumb_size": os.path.getsize(thumb_cache),
        }

    def _photo_cache_root(self, profile, target_mode):
        source_repo = profile.get("source_repo_path") or os.getcwd()
        profile_id = self._safe_cache_component(profile.get("id") or "default")
        mode = self._safe_cache_component(target_mode or "device")
        return os.path.abspath(os.path.join(source_repo, "rockpod", ".generated", "photos", profile_id, mode))

    @staticmethod
    def _thumbnail_size(profile):
        resolution = str((profile or {}).get("screen_resolution") or "").strip().lower()
        try:
            width_text, height_text = resolution.split("x", 1)
            width = int(width_text)
            height = int(height_text)
        except (TypeError, ValueError):
            return THUMBNAIL_MAX_SIZE

        if width >= 300 and height >= 220:
            return (87, 70)
        if width >= 220 and height >= 170:
            return (80, 60)
        if width >= 176 and height >= 132:
            return (64, 48)
        return (56, 42)

    @staticmethod
    def _rgb_image(image):
        if image.mode in ("RGBA", "LA") or (image.mode == "P" and "transparency" in image.info):
            rgba = image.convert("RGBA")
            background = Image.new("RGBA", rgba.size, (255, 255, 255, 255))
            background.alpha_composite(rgba)
            return background.convert("RGB")
        return image.convert("RGB")

    @staticmethod
    def _device_photo_relpath(relpath):
        relpath = str(relpath or "").replace("\\", "/").lstrip("/")
        stem, ext = os.path.splitext(relpath)
        if ext.lower() in {".jpg", ".jpe", ".jpeg"}:
            return relpath
        return f"{stem}.jpg" if stem else "photo.jpg"

    @staticmethod
    def _safe_cache_component(value):
        text = str(value or "").strip().replace(os.sep, "_")
        return "".join(ch if ch.isalnum() or ch in "._-" else "_" for ch in text) or "default"

    def _scan_photo_files(self, root):
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames[:] = [
                name for name in dirnames
                if not name.startswith(".") and os.path.abspath(os.path.join(dirpath, name)) != os.path.abspath(os.path.join(root, ".photo_thumbs"))
            ]
            for filename in sorted(filenames):
                if filename.startswith("."):
                    continue
                if os.path.splitext(filename)[1].lower() not in SUPPORTED_PHOTO_EXTENSIONS:
                    continue
                yield os.path.abspath(os.path.join(dirpath, filename))

    @staticmethod
    def _relative_photo_path(root, path):
        relpath = os.path.relpath(os.path.abspath(path), os.path.abspath(root))
        return relpath.replace(os.sep, "/")
