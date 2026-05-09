"""Photo library indexing and scoped sync/remove workflows."""

from __future__ import annotations

import os


SUPPORTED_PHOTO_EXTENSIONS = {".bmp", ".gif", ".jpg", ".jpe", ".jpeg", ".png", ".ppm"}
PHOTO_TARGET_DIR = "Photos"
PHOTO_THUMB_DIR = "Photos/.photo_thumbs"


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
                        "source_path": source_path,
                        "size": stat.st_size,
                        "modified_time": stat.st_mtime,
                        "on_device": os.path.isfile(os.path.join(device_root, relpath)) if device_root else False,
                        "on_simulator": os.path.isfile(os.path.join(sim_root, relpath)) if sim_root else False,
                        "missing_source": False,
                    }
                )
                relpaths.add(relpath)

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
        assets = []
        for photo in photos:
            relpath = photo["relative_path"].lstrip("/")
            assets.append(
                {
                    "kind": "photo",
                    "source_rel": relpath,
                    "source_abs": photo["source_path"],
                    "destination_rel": f"{target_dir}/{relpath}",
                    "exists": bool(photo.get("source_path") and os.path.isfile(photo["source_path"])),
                    "size": photo.get("size", 0),
                }
            )
        return {
            "id": f"photos-sync-{profile['id']}",
            "name": "Rockbox Photo Sync",
            "assets": assets,
        }

    def build_remove_bundle(self, profile, photos, target_mode="device"):
        target_dir = self._photo_target_dir(profile, target_mode).rstrip("/")
        thumb_dir = PHOTO_THUMB_DIR.rstrip("/")
        assets = []
        for photo in photos:
            relpath = photo["relative_path"].lstrip("/")
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
