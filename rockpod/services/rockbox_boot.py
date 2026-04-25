"""Boot / branding asset validation, preview, and deploy bundle building."""

from __future__ import annotations

import os

from PySide6.QtCore import Qt
from PySide6.QtGui import QImage, QImageReader


BOOT_SPECS = {
    "320x240": {
        "name": "iPod Classic / Video Boot Splash",
        "width": 320,
        "height": 240,
        "default_candidates": [
            "apps/bitmaps/native/rockboxlogo.320x98x16.bmp",
        ],
    },
    "176x132": {
        "name": "iPod nano 2G Boot Splash",
        "width": 176,
        "height": 132,
        "default_candidates": [
            "apps/bitmaps/native/rockboxlogo.176x54x16.bmp",
            "wps/iPone_nano2g/BootLogo.bmp",
        ],
    },
    "160x128": {
        "name": "iPod 3G Boot Splash",
        "width": 160,
        "height": 128,
        "default_candidates": [
            "apps/bitmaps/native/rockboxlogo.160x53x2.bmp",
        ],
    },
}


class RockboxBootService:
    """Manage boot branding assets without writing target files directly."""

    def profile_spec(self, profile):
        resolution = str(profile.get("screen_resolution") or "").strip()
        return dict(BOOT_SPECS.get(resolution, BOOT_SPECS["320x240"]))

    def default_image_for_profile(self, profile):
        spec = self.profile_spec(profile)
        repo_root = os.path.abspath(profile["source_repo_path"])
        explicit = os.path.abspath(profile.get("boot_image_path") or "")
        if explicit and os.path.isfile(explicit):
            return explicit
        for rel in spec["default_candidates"]:
            candidate = os.path.join(repo_root, rel)
            if os.path.isfile(candidate):
                return candidate
        return ""

    def validate_image(self, image_path, profile):
        spec = self.profile_spec(profile)
        path = os.path.abspath(image_path or "")
        if not path:
            return self._invalid(spec, "No boot image selected")
        if not os.path.isfile(path):
            return self._invalid(spec, "Boot image file does not exist")
        reader = QImageReader(path)
        if not reader.canRead():
            return self._invalid(spec, "Unsupported or unreadable image")
        size = reader.size()
        if size.width() != spec["width"] or size.height() != spec["height"]:
            return {
                "valid": False,
                "message": f"Expected {spec['width']}x{spec['height']}, got {size.width()}x{size.height()}",
                "path": path,
                "width": size.width(),
                "height": size.height(),
                "expected_width": spec["width"],
                "expected_height": spec["height"],
            }
        return {
            "valid": True,
            "message": f"Ready for {spec['width']}x{spec['height']}",
            "path": path,
            "width": size.width(),
            "height": size.height(),
            "expected_width": spec["width"],
            "expected_height": spec["height"],
        }

    def generate_preview(self, image_path, profile, cache_root):
        validation = self.validate_image(image_path, profile)
        if not validation["valid"]:
            return {"success": False, "preview_path": "", "validation": validation}
        os.makedirs(cache_root, exist_ok=True)
        image = QImage(validation["path"])
        if image.isNull():
            return {"success": False, "preview_path": "", "validation": self._invalid(self.profile_spec(profile), "Preview load failed")}
        preview_path = os.path.join(cache_root, f"{profile['id']}-boot-preview.png")
        image.save(preview_path, "PNG")
        return {"success": True, "preview_path": preview_path, "validation": validation}

    def deploy_profile(self, profile, target_mode="device", simulator_target=None):
        mode = "simulator" if target_mode == "simulator" else "device"
        mount_path = profile.get("device_mount_path") or ""
        backup_root = os.path.join(profile["source_repo_path"], "rockpod", ".backups", "boot", mode, profile["id"])
        if mode == "simulator":
            mount_path = (
                profile.get("simulator_simdisk_path")
                or (simulator_target or {}).get("simdisk_path")
                or ""
            )
        return {
            "id": f"{profile['id']}-boot-{mode}",
            "name": f"{profile['name']} Boot ({mode})",
            "device_mount_path": os.path.abspath(mount_path) if mount_path else "",
            "target_device_model": profile.get("target_device_model", ""),
            "screen_resolution": profile.get("screen_resolution", ""),
            "source_repo_path": profile["source_repo_path"],
            "selected_theme": profile.get("selected_theme", ""),
            "backup_location": os.path.abspath(backup_root),
        }

    def build_bundle(self, profile, image_path, working_root):
        validation = self.validate_image(image_path, profile)
        if not validation["valid"]:
            raise ValueError(validation["message"])
        spec = self.profile_spec(profile)
        os.makedirs(working_root, exist_ok=True)
        normalized_path = os.path.join(working_root, f"{profile['id']}-boot-logo.bmp")
        image = QImage(validation["path"])
        if image.isNull():
            raise ValueError("Boot image load failed")
        image = image.convertToFormat(QImage.Format_RGB32)
        image.save(normalized_path, "BMP")
        return {
            "id": f"boot-{profile['id']}",
            "name": spec["name"],
            "assets": [
                {
                    "kind": "boot_logo",
                    "source_rel": os.path.basename(validation["path"]),
                    "source_abs": os.path.abspath(normalized_path),
                    "destination_rel": f".rockbox/rockpod/boot/branding/{profile['screen_resolution']}/boot-logo.bmp",
                    "exists": True,
                    "size": os.path.getsize(normalized_path),
                    "preview_path": validation["path"],
                }
            ],
        }

    def fit_preview(self, preview_path, max_width=320, max_height=240):
        image = QImage(preview_path)
        if image.isNull():
            return ""
        scaled = image.scaled(max_width, max_height, Qt.KeepAspectRatio, Qt.SmoothTransformation)
        temp_path = os.path.join(os.path.dirname(preview_path), f"scaled-{os.path.basename(preview_path)}")
        scaled.save(temp_path, "PNG")
        return temp_path

    @staticmethod
    def _invalid(spec, message):
        return {
            "valid": False,
            "message": message,
            "path": "",
            "width": 0,
            "height": 0,
            "expected_width": spec["width"],
            "expected_height": spec["height"],
        }
