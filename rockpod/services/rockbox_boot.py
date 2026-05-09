"""Boot / branding asset validation, preview, and deploy bundle building."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import base64
import tempfile

from PySide6.QtCore import Qt
from PySide6.QtGui import QImage, QImageReader
from PIL import Image, ImageOps, UnidentifiedImageError


BOOT_SPECS = {
    "320x240": {
        "name": "iPod Classic / Video Boot Splash",
        "width": 320,
        "height": 240,
        "configure_target": "ipod6g",
        "build_dirs": ["build-hw-ipod6g", "build-hw-ipodvideo-5g", "build-hw-ipodvideo"],
        "default_candidates": [
            "apps/bitmaps/native/rockboxlogo.320x98x16.bmp",
        ],
    },
    "176x132": {
        "name": "iPod nano 2G Boot Splash",
        "width": 176,
        "height": 132,
        "configure_target": "ipodnano2g",
        "build_dirs": ["build-hw-ipodnano2g"],
        "default_candidates": [
            "apps/bitmaps/native/rockboxlogo.176x54x16.bmp",
            "wps/iPone_nano2g/BootLogo.bmp",
        ],
        "bootloader_required_for_device_branding": True,
        "bootloader_install_supported": False,
    },
    "160x128": {
        "name": "iPod 3G Boot Splash",
        "width": 160,
        "height": 128,
        "configure_target": "ipod3g",
        "build_dirs": ["build-hw-ipod3g", "build-hw-ipodvideo", "build-hw-ipodvideo-5g"],
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
        requires_resize = size.width() != spec["width"] or size.height() != spec["height"]
        return {
            "valid": True,
            "message": (
                f"Will scale {size.width()}x{size.height()} to {spec['width']}x{spec['height']}"
                if requires_resize
                else f"Ready for {spec['width']}x{spec['height']}"
            ),
            "path": path,
            "width": size.width(),
            "height": size.height(),
            "expected_width": spec["width"],
            "expected_height": spec["height"],
            "requires_resize": requires_resize,
        }

    def generate_preview(self, image_path, profile, cache_root):
        validation = self.validate_image(image_path, profile)
        if not validation["valid"]:
            return {"success": False, "preview_path": "", "validation": validation}
        os.makedirs(cache_root, exist_ok=True)
        spec = self.profile_spec(profile)
        try:
            image = self._render_image(validation["path"], spec["width"], spec["height"])
        except ValueError:
            return {"success": False, "preview_path": "", "validation": self._invalid(self.profile_spec(profile), "Preview load failed")}
        preview_path = os.path.join(
            cache_root,
            f"{profile['id']}-boot-preview-{self._preview_cache_key(validation['path'])}.png",
        )
        if image.save(preview_path, "PNG") and os.path.isfile(preview_path):
            return {"success": True, "preview_path": preview_path, "validation": validation}
        # Fall back to the validated source image so the UI can still render a preview.
        return {"success": True, "preview_path": validation["path"], "validation": validation}

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

    def source_profile(self, profile):
        repo_root = os.path.abspath(profile["source_repo_path"])
        backup_root = os.path.join(repo_root, "rockpod", ".backups", "boot", "source", profile["id"])
        return {
            "id": f"{profile['id']}-boot-source",
            "name": f"{profile['name']} Boot Sources",
            "device_mount_path": repo_root,
            "target_device_model": profile.get("target_device_model", ""),
            "screen_resolution": profile.get("screen_resolution", ""),
            "source_repo_path": repo_root,
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
        image = self._render_image(validation["path"], spec["width"], spec["height"])
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

    def build_source_bundle(self, profile, image_path, working_root):
        validation = self.validate_image(image_path, profile)
        if not validation["valid"]:
            raise ValueError(validation["message"])
        os.makedirs(working_root, exist_ok=True)
        assets = []
        for rel_path in self.profile_spec(profile)["default_candidates"]:
            rendered_path = os.path.join(
                working_root,
                f"{profile['id']}-{os.path.basename(rel_path)}",
            )
            width, height = self._target_dimensions(profile, rel_path)
            image = self._render_image(validation["path"], width, height)
            image.save(rendered_path, "BMP")
            assets.append(
                {
                    "kind": "boot_logo_source",
                    "source_rel": os.path.basename(validation["path"]),
                    "source_abs": os.path.abspath(rendered_path),
                    "destination_rel": rel_path,
                    "exists": True,
                    "size": os.path.getsize(rendered_path),
                    "preview_path": validation["path"],
                    "preserve_metadata": False,
                }
            )
        return {
            "id": f"boot-source-{profile['id']}",
            "name": self.profile_spec(profile)["name"],
            "assets": assets,
        }

    def bootloader_requirements(self, profile):
        spec = self.profile_spec(profile)
        required = bool(spec.get("bootloader_required_for_device_branding"))
        install_supported = bool(spec.get("bootloader_install_supported", True))
        output_name = self.bootloader_output_name(profile)
        install_name = output_name + "x" if output_name.endswith(".ipod") else output_name
        message = ""
        if required and not install_supported:
            message = (
                "Cold-boot branding on this target uses an encrypted bootloader image "
                f"({install_name}), not only mounted rockbox.ipod."
            )
        return {
            "required": required,
            "install_supported": install_supported,
            "output_name": output_name,
            "install_name": install_name,
            "message": message,
        }

    def firmware_build_dir(self, profile):
        repo_root = os.path.abspath(profile["source_repo_path"])
        for name in self._preferred_build_dirs(profile):
            candidate = os.path.join(repo_root, name)
            if os.path.isdir(candidate):
                return candidate
        return ""

    def firmware_output_path(self, profile):
        build_dir = self.firmware_build_dir(profile)
        if not build_dir:
            return ""
        return os.path.join(build_dir, "rockbox.ipod")

    def bootloader_build_dir(self, profile):
        spec = self.profile_spec(profile)
        repo_root = os.path.abspath(profile["source_repo_path"])
        configure_target = str(spec.get("configure_target") or "").strip()
        if not configure_target:
            return ""
        return os.path.join(repo_root, f"build-bootloader-{configure_target}")

    def bootloader_output_name(self, profile):
        configure_target = str(self.profile_spec(profile).get("configure_target") or "").strip()
        if not configure_target:
            return ""
        return f"bootloader-{configure_target}.ipod"

    def bootloader_output_path(self, profile):
        build_dir = self.bootloader_build_dir(profile)
        output_name = self.bootloader_output_name(profile)
        if not build_dir or not output_name:
            return ""
        return os.path.join(build_dir, output_name)

    def rebuild_firmware(self, profile, runner=None):
        build_dir = self.firmware_build_dir(profile)
        if not build_dir:
            return {
                "success": False,
                "artifact_path": "",
                "build_dir": "",
                "stdout": "",
                "stderr": "",
                "message": "No Rockbox build directory found for this profile",
            }
        artifact_path = os.path.join(build_dir, "rockbox.ipod")
        command = ["make", "-C", build_dir, "-j4", artifact_path]
        result = (runner or subprocess.run)(
            command,
            check=False,
            capture_output=True,
            text=True,
        )
        success = result.returncode == 0 and os.path.isfile(artifact_path)
        return {
            "success": success,
            "artifact_path": artifact_path if success else "",
            "build_dir": build_dir,
            "stdout": result.stdout,
            "stderr": result.stderr,
            "message": "" if success else "Firmware rebuild failed",
        }

    def full_install_firmware(self, profile, runner=None, progress_callback=None):
        build_dir = self.firmware_build_dir(profile)
        mount_path = os.path.abspath(profile.get("device_mount_path") or "")
        if not build_dir:
            return {
                "success": False,
                "artifact_path": "",
                "build_dir": "",
                "stdout": "",
                "stderr": "",
                "message": "No Rockbox build directory found for this profile",
            }
        if not mount_path or not os.path.isdir(mount_path):
            return {
                "success": False,
                "artifact_path": "",
                "build_dir": build_dir,
                "stdout": "",
                "stderr": "",
                "message": "Mounted device path does not exist",
            }

        artifact_path = os.path.join(build_dir, "rockbox.ipod")
        build_command = ["make", "-C", build_dir, "-j4", artifact_path]
        install_command = ["make", "-C", build_dir, f"PREFIX={mount_path}", "fullinstall"]

        build = self._run_progress_command(
            build_command,
            runner=runner,
            progress_callback=progress_callback,
            progress_current=1,
            progress_total=2,
            progress_label="Building rockbox.ipod",
        )
        if build.returncode != 0 or not os.path.isfile(artifact_path):
            return {
                "success": False,
                "artifact_path": "",
                "build_dir": build_dir,
                "stdout": build.stdout,
                "stderr": build.stderr,
                "message": "Firmware rebuild failed",
            }

        install = self._run_progress_command(
            install_command,
            runner=runner,
            progress_callback=progress_callback,
            progress_current=2,
            progress_total=2,
            progress_label="Full installing Rockbox to iPod",
        )
        success = install.returncode == 0
        stdout = "\n".join(part for part in (build.stdout, install.stdout) if part.strip())
        stderr = "\n".join(part for part in (build.stderr, install.stderr) if part.strip())
        return {
            "success": success,
            "artifact_path": artifact_path if success else "",
            "build_dir": build_dir,
            "stdout": stdout,
            "stderr": stderr,
            "message": "" if success else "Rockbox full install failed",
        }

    def rebuild_bootloader(self, profile, runner=None):
        spec = self.profile_spec(profile)
        repo_root = os.path.abspath(profile["source_repo_path"])
        configure_target = str(spec.get("configure_target") or "").strip()
        build_dir = self.bootloader_build_dir(profile)
        artifact_name = self.bootloader_output_name(profile)
        artifact_path = self.bootloader_output_path(profile)
        if not configure_target or not build_dir or not artifact_name or not artifact_path:
            return {
                "success": False,
                "artifact_path": "",
                "build_dir": "",
                "stdout": "",
                "stderr": "",
                "message": "No bootloader build target found for this profile",
            }

        os.makedirs(build_dir, exist_ok=True)
        configure_path = os.path.join(repo_root, "tools", "configure")
        configure_stdout = ""
        configure_stderr = ""
        if not os.path.isfile(os.path.join(build_dir, "Makefile")):
            configure_command = [configure_path, f"--target={configure_target}", "--type=b"]
            configure_result = (runner or subprocess.run)(
                configure_command,
                check=False,
                capture_output=True,
                text=True,
                cwd=build_dir,
            )
            configure_stdout = configure_result.stdout
            configure_stderr = configure_result.stderr
            if configure_result.returncode != 0:
                return {
                    "success": False,
                    "artifact_path": "",
                    "build_dir": build_dir,
                    "stdout": configure_stdout,
                    "stderr": configure_stderr,
                    "message": "Bootloader configure failed",
                }

        # Bootloader build trees don't consistently expose the final artifact name
        # as an addressable make target, even though the default target produces it.
        build_command = ["make", "-C", build_dir, "-j4"]
        result = (runner or subprocess.run)(
            build_command,
            check=False,
            capture_output=True,
            text=True,
        )
        success = result.returncode == 0 and os.path.isfile(artifact_path)
        stdout = "\n".join(part for part in (configure_stdout, result.stdout) if part.strip())
        stderr = "\n".join(part for part in (configure_stderr, result.stderr) if part.strip())
        return {
            "success": success,
            "artifact_path": artifact_path if success else "",
            "build_dir": build_dir,
            "stdout": stdout,
            "stderr": stderr,
            "message": "" if success else "Bootloader rebuild failed",
        }

    def build_firmware_bundle(self, artifact_path):
        path = os.path.abspath(artifact_path or "")
        if not path or not os.path.isfile(path):
            raise ValueError("Firmware artifact does not exist")
        return {
            "id": "boot-firmware",
            "name": "Rockbox Firmware",
            "assets": [
                {
                    "kind": "firmware_image",
                    "source_rel": os.path.basename(path),
                    "source_abs": path,
                    "destination_rel": "rockbox.ipod",
                    "exists": True,
                    "size": os.path.getsize(path),
                }
            ],
        }

    def invalidate_firmware_boot_assets(self, profile):
        build_dir = self.firmware_build_dir(profile)
        if not build_dir:
            return []

        removed = []
        for rel_path in self.profile_spec(profile).get("default_candidates", []):
            rel_without_ext, _ext = os.path.splitext(rel_path)
            for suffix in (".c", ".o"):
                candidate = os.path.join(build_dir, rel_without_ext + suffix)
                if self._remove_file(candidate):
                    removed.append(candidate)

        for header in ("bitmaps/rockboxlogo.h",):
            candidate = os.path.join(build_dir, header)
            if self._remove_file(candidate):
                removed.append(candidate)

        artifact = self.firmware_output_path(profile)
        if self._remove_file(artifact):
            removed.append(artifact)
        return removed

    def crypto_plugin_artifact_path(self, profile):
        build_dir = self.firmware_build_dir(profile)
        if not build_dir:
            return ""
        return os.path.join(build_dir, "apps", "plugins", "crypt_firmware.rock")

    def rebuild_crypto_plugin(self, profile, runner=None):
        build_dir = self.firmware_build_dir(profile)
        artifact_path = self.crypto_plugin_artifact_path(profile)
        if not build_dir or not artifact_path:
            return {
                "success": False,
                "artifact_path": "",
                "build_dir": "",
                "stdout": "",
                "stderr": "",
                "message": "No crypto plugin build target found for this profile",
            }
        command = ["make", "-C", build_dir, "-j4", artifact_path]
        result = (runner or subprocess.run)(
            command,
            check=False,
            capture_output=True,
            text=True,
        )
        success = result.returncode == 0 and os.path.isfile(artifact_path)
        return {
            "success": success,
            "artifact_path": artifact_path if success else "",
            "build_dir": build_dir,
            "stdout": result.stdout,
            "stderr": result.stderr,
            "message": "" if success else "Crypto plugin rebuild failed",
        }

    def bootloader_stage_paths(self, profile):
        requirements = self.bootloader_requirements(profile)
        if not requirements["required"]:
            return {
                "input_rel": "",
                "input_abs": "",
                "output_rel": "",
                "output_abs": "",
                "plugin_rel": "",
                "plugin_abs": "",
                "plugin_dat_rel": "",
                "plugin_dat_abs": "",
                "marker_rel": "",
                "marker_abs": "",
            }
        mount_path = os.path.abspath(profile.get("device_mount_path") or "")
        input_rel = ".rockbox/b.ipod"
        output_rel = ".rockbox/b.ipodx"
        plugin_rel = ".rockbox/rocks/viewers/crypt_firmware.rock"
        plugin_dat_rel = ".rockbox/rocks/plugin.dat"
        marker_rel = ".rockbox/rockpod/boot/nano2g-encrypt-pending"
        return {
            "input_rel": input_rel,
            "input_abs": os.path.join(mount_path, input_rel) if mount_path else "",
            "output_rel": output_rel,
            "output_abs": os.path.join(mount_path, output_rel) if mount_path else "",
            "plugin_rel": plugin_rel,
            "plugin_abs": os.path.join(mount_path, plugin_rel) if mount_path else "",
            "plugin_dat_rel": plugin_dat_rel,
            "plugin_dat_abs": os.path.join(mount_path, plugin_dat_rel) if mount_path else "",
            "marker_rel": marker_rel,
            "marker_abs": os.path.join(mount_path, marker_rel) if mount_path else "",
        }

    def bootloader_stage_metadata_path(self, profile):
        repo_root = os.path.abspath(profile["source_repo_path"])
        return os.path.join(
            repo_root,
            "rockpod",
            ".backups",
            "boot",
            "device",
            profile["id"],
            "nano2g-bootloader-stage.json",
        )

    def load_bootloader_stage_metadata(self, profile):
        path = self.bootloader_stage_metadata_path(profile)
        if not os.path.isfile(path):
            return {}
        try:
            with open(path, "r", encoding="utf-8") as handle:
                data = json.load(handle)
        except (OSError, json.JSONDecodeError):
            return {}
        return data if isinstance(data, dict) else {}

    def save_bootloader_stage_metadata(self, profile, metadata):
        path = self.bootloader_stage_metadata_path(profile)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        payload = dict(metadata or {})
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(payload, handle, indent=2, sort_keys=True)
            handle.write("\n")
        return path

    def clear_bootloader_stage_metadata(self, profile):
        path = self.bootloader_stage_metadata_path(profile)
        try:
            os.remove(path)
        except FileNotFoundError:
            return

    def bootloader_stage_status(self, profile, artifact_path=""):
        metadata = self.load_bootloader_stage_metadata(profile)
        paths = self.bootloader_stage_paths(profile)
        current_hash = self._hash_file(artifact_path) if artifact_path else ""
        metadata_hash = str(metadata.get("artifact_sha256") or "")
        staged_input_hash = self._hash_file(paths["input_abs"]) if paths["input_abs"] else ""
        plain_staged = os.path.isfile(paths["input_abs"]) if paths["input_abs"] else False
        encrypted_ready = os.path.isfile(paths["output_abs"]) if paths["output_abs"] else False
        marker_present = os.path.isfile(paths["marker_abs"]) if paths["marker_abs"] else False
        return {
            "metadata_path": self.bootloader_stage_metadata_path(profile),
            "metadata_present": bool(metadata),
            "metadata": metadata,
            "plain_staged": plain_staged,
            "encrypted_ready": encrypted_ready,
            "marker_present": marker_present,
            "stage_pending": bool(plain_staged and marker_present and not encrypted_ready),
            "plugin_present_on_device": os.path.isfile(paths["plugin_abs"]) if paths["plugin_abs"] else False,
            "staged_input_matches_metadata": bool(
                staged_input_hash and metadata_hash and staged_input_hash == metadata_hash
            ),
            "current_artifact_matches": bool(current_hash and metadata_hash and current_hash == metadata_hash),
            "paths": paths,
        }

    def bootloader_stage_ready_for_install(self, profile, artifact_path=""):
        status = self.bootloader_stage_status(profile, artifact_path)
        return bool(
            status["encrypted_ready"]
            and (status["staged_input_matches_metadata"] or status["current_artifact_matches"])
        )

    def build_bootloader_stage_bundle(self, profile, artifact_path, working_root):
        path = os.path.abspath(artifact_path or "")
        if not path or not os.path.isfile(path):
            raise ValueError("Bootloader artifact does not exist")
        metadata = self.load_bootloader_stage_metadata(profile)
        paths = self.bootloader_stage_paths(profile)
        os.makedirs(working_root, exist_ok=True)
        assets = self._build_legacy_bootloader_restore_assets(profile, metadata, working_root)
        staged_marker_path = os.path.join(working_root, f"{profile['id']}-nano2g-encrypt-pending")
        with open(staged_marker_path, "w", encoding="utf-8") as handle:
            handle.write("pending\n")
        assets.extend(
            [
                {
                    "kind": "bootloader_stage_input",
                    "source_rel": os.path.basename(path),
                    "source_abs": path,
                    "destination_rel": paths["input_rel"],
                    "exists": True,
                    "size": os.path.getsize(path),
                },
                {
                    "kind": "bootloader_stage_marker",
                    "source_rel": os.path.basename(staged_marker_path),
                    "source_abs": os.path.abspath(staged_marker_path),
                    "destination_rel": paths["marker_rel"],
                    "exists": True,
                    "size": os.path.getsize(staged_marker_path),
                },
            ]
        )
        plugin_artifact = self.crypto_plugin_artifact_path(profile)
        if os.path.isfile(plugin_artifact):
            assets.append(
                {
                    "kind": "bootloader_stage_plugin",
                    "source_rel": os.path.basename(plugin_artifact),
                    "source_abs": plugin_artifact,
                    "destination_rel": paths["plugin_rel"],
                    "exists": True,
                    "size": os.path.getsize(plugin_artifact),
                }
            )
        metadata_payload = {
            "profile_id": profile["id"],
            "input_rel": paths["input_rel"],
            "output_rel": paths["output_rel"],
            "marker_rel": paths["marker_rel"],
            "plugin_rel": paths["plugin_rel"],
            "artifact_sha256": self._hash_file(path),
        }
        for key in ("original_config", "original_plugin_dat_exists", "original_plugin_dat_base64"):
            if key in metadata:
                metadata_payload[key] = metadata[key]
        return {
            "id": f"bootloader-stage-{profile['id']}",
            "name": "nano2g Bootloader Encryption Stage",
            "assets": assets,
        }, metadata_payload

    def _build_legacy_bootloader_restore_assets(self, profile, metadata, working_root):
        original_config = metadata.get("original_config")
        original_plugin_dat_base64 = metadata.get("original_plugin_dat_base64")
        if not isinstance(original_config, str):
            return []
        os.makedirs(working_root, exist_ok=True)
        assets = []
        restored_config_path = os.path.join(working_root, f"{profile['id']}-bootloader-config-restore.cfg")
        with open(restored_config_path, "w", encoding="utf-8") as handle:
            handle.write(original_config)
        assets.append(
            {
                "kind": "bootloader_restore_config",
                "source_rel": "config.cfg",
                "source_abs": os.path.abspath(restored_config_path),
                "destination_rel": ".rockbox/config.cfg",
                "exists": True,
                "size": os.path.getsize(restored_config_path),
            }
        )
        if metadata.get("original_plugin_dat_exists") and isinstance(original_plugin_dat_base64, str):
            restored_plugin_dat_path = os.path.join(working_root, f"{profile['id']}-plugin-restore.dat")
            with open(restored_plugin_dat_path, "wb") as handle:
                handle.write(base64.b64decode(original_plugin_dat_base64.encode("ascii")))
            assets.append(
                {
                    "kind": "bootloader_restore_plugin_dat",
                    "source_rel": "plugin.dat",
                    "source_abs": os.path.abspath(restored_plugin_dat_path),
                    "destination_rel": ".rockbox/rocks/plugin.dat",
                    "exists": True,
                    "size": os.path.getsize(restored_plugin_dat_path),
                }
            )
        elif "original_plugin_dat_exists" in metadata:
            assets.append(
                {
                    "kind": "bootloader_restore_plugin_dat",
                    "source_rel": "",
                    "source_abs": "",
                    "destination_rel": ".rockbox/rocks/plugin.dat",
                    "exists": False,
                    "action": "remove",
                }
            )
        return assets

    def build_bootloader_restore_config_bundle(self, profile, working_root):
        metadata = self.load_bootloader_stage_metadata(profile)
        original_config = metadata.get("original_config")
        if not isinstance(original_config, str):
            raise ValueError("No staged bootloader config backup exists")
        original_plugin_dat_base64 = metadata.get("original_plugin_dat_base64")
        os.makedirs(working_root, exist_ok=True)
        paths = self.bootloader_stage_paths(profile)
        restored_config_path = os.path.join(working_root, f"{profile['id']}-bootloader-config-restore.cfg")
        with open(restored_config_path, "w", encoding="utf-8") as handle:
            handle.write(original_config)
        assets = [
            {
                "kind": "bootloader_restore_config",
                "source_rel": "config.cfg",
                "source_abs": os.path.abspath(restored_config_path),
                "destination_rel": ".rockbox/config.cfg",
                "exists": True,
                "size": os.path.getsize(restored_config_path),
            }
        ]
        if metadata.get("original_plugin_dat_exists") and isinstance(original_plugin_dat_base64, str):
            restored_plugin_dat_path = os.path.join(working_root, f"{profile['id']}-plugin-restore.dat")
            with open(restored_plugin_dat_path, "wb") as handle:
                handle.write(base64.b64decode(original_plugin_dat_base64.encode("ascii")))
            assets.append(
                {
                    "kind": "bootloader_restore_plugin_dat",
                    "source_rel": "plugin.dat",
                    "source_abs": os.path.abspath(restored_plugin_dat_path),
                    "destination_rel": paths["plugin_dat_rel"],
                    "exists": True,
                    "size": os.path.getsize(restored_plugin_dat_path),
                }
            )
        elif "original_plugin_dat_exists" in metadata:
            assets.append(
                {
                    "kind": "bootloader_restore_plugin_dat",
                    "source_rel": "",
                    "source_abs": "",
                    "destination_rel": paths["plugin_dat_rel"],
                    "exists": False,
                    "action": "remove",
                }
            )
        return {
            "id": f"bootloader-restore-config-{profile['id']}",
            "name": "nano2g Bootloader Config Restore",
            "assets": assets,
        }

    def build_bootloader_cleanup_bundle(self, profile):
        paths = self.bootloader_stage_paths(profile)
        return {
            "id": f"bootloader-cleanup-{profile['id']}",
            "name": "nano2g Bootloader Stage Cleanup",
            "assets": [
                {
                    "kind": "bootloader_stage_input",
                    "source_rel": "",
                    "source_abs": "",
                    "destination_rel": paths["input_rel"],
                    "exists": False,
                    "action": "remove",
                },
                {
                    "kind": "bootloader_stage_output",
                    "source_rel": "",
                    "source_abs": "",
                    "destination_rel": paths["output_rel"],
                    "exists": False,
                    "action": "remove",
                },
                {
                    "kind": "bootloader_stage_marker",
                    "source_rel": "",
                    "source_abs": "",
                    "destination_rel": paths["marker_rel"],
                    "exists": False,
                    "action": "remove",
                },
            ],
        }

    def copy_encrypted_bootloader_to_host(self, profile, working_root):
        paths = self.bootloader_stage_paths(profile)
        source_path = paths["output_abs"]
        if not source_path or not os.path.isfile(source_path):
            raise ValueError("Encrypted bootloader image is not present on the mounted device")
        os.makedirs(working_root, exist_ok=True)
        host_path = os.path.join(working_root, f"{profile['id']}-bootloader-ipodnano2g.ipodx")
        shutil.copy2(source_path, host_path)
        return host_path

    def resolve_disk_nodes(self, profile, runner=None):
        mount_path = os.path.abspath(profile.get("device_mount_path") or "")
        if not mount_path or not os.path.isdir(mount_path):
            return {
                "success": False,
                "partition_path": "",
                "disk_path": "",
                "stdout": "",
                "stderr": "",
                "message": "Mounted device path does not exist",
            }
        run = runner or subprocess.run
        findmnt = run(
            ["findmnt", "-no", "SOURCE", mount_path],
            check=False,
            capture_output=True,
            text=True,
        )
        partition_path = findmnt.stdout.strip()
        if findmnt.returncode != 0 or not partition_path:
            return {
                "success": False,
                "partition_path": "",
                "disk_path": "",
                "stdout": findmnt.stdout,
                "stderr": findmnt.stderr,
                "message": "Unable to resolve mounted partition for this device",
            }
        lsblk = run(
            ["lsblk", "-no", "PKNAME", partition_path],
            check=False,
            capture_output=True,
            text=True,
        )
        parent = lsblk.stdout.strip()
        disk_path = partition_path if not parent else os.path.join("/dev", parent)
        return {
            "success": True,
            "partition_path": partition_path,
            "disk_path": disk_path,
            "stdout": "\n".join(part for part in (findmnt.stdout, lsblk.stdout) if part.strip()),
            "stderr": "\n".join(part for part in (findmnt.stderr, lsblk.stderr) if part.strip()),
            "message": "",
        }

    def ensure_ipodpatcher(self, profile, runner=None):
        repo_root = os.path.abspath(profile["source_repo_path"])
        tool_dir = os.path.join(repo_root, "utils", "ipodpatcher")
        binary_path = os.path.join(tool_dir, "ipodpatcher")
        if os.path.isfile(binary_path) and os.access(binary_path, os.X_OK):
            return {
                "success": True,
                "binary_path": binary_path,
                "stdout": "",
                "stderr": "",
                "message": "",
            }
        result = (runner or subprocess.run)(
            ["make", "-C", tool_dir],
            check=False,
            capture_output=True,
            text=True,
        )
        success = result.returncode == 0 and os.path.isfile(binary_path) and os.access(binary_path, os.X_OK)
        return {
            "success": success,
            "binary_path": binary_path if success else "",
            "stdout": result.stdout,
            "stderr": result.stderr,
            "message": "" if success else "Failed to build ipodpatcher",
        }

    def install_encrypted_bootloader(self, profile, encrypted_host_path, runner=None):
        encrypted_path = os.path.abspath(encrypted_host_path or "")
        if not encrypted_path or not os.path.isfile(encrypted_path):
            return {
                "success": False,
                "stdout": "",
                "stderr": "",
                "message": "Encrypted bootloader image does not exist",
            }
        nodes = self.resolve_disk_nodes(profile, runner=runner)
        if not nodes["success"]:
            return {
                "success": False,
                "stdout": nodes.get("stdout", ""),
                "stderr": nodes.get("stderr", ""),
                "message": nodes["message"],
            }
        ipodpatcher = self.ensure_ipodpatcher(profile, runner=runner)
        if not ipodpatcher["success"]:
            return {
                "success": False,
                "stdout": ipodpatcher.get("stdout", ""),
                "stderr": ipodpatcher.get("stderr", ""),
                "message": ipodpatcher["message"],
            }

        run = runner or subprocess.run
        stdout_parts = []
        stderr_parts = []
        cleanup_path = ""
        mount_path = os.path.abspath(profile.get("device_mount_path") or "")
        if mount_path and os.path.commonpath([mount_path, encrypted_path]) == mount_path:
            fd, cleanup_path = tempfile.mkstemp(
                prefix=f"{profile.get('id', 'rockpod')}-bootloader-",
                suffix=os.path.splitext(encrypted_path)[1] or ".ipodx",
            )
            os.close(fd)
            shutil.copy2(encrypted_path, cleanup_path)
            encrypted_path = cleanup_path

        def _run(command):
            result = run(command, check=False, capture_output=True, text=True)
            if result.stdout.strip():
                stdout_parts.append(result.stdout)
            if result.stderr.strip():
                stderr_parts.append(result.stderr)
            return result

        unmount = _run(["udisksctl", "unmount", "-b", nodes["partition_path"]])
        if unmount.returncode != 0:
            return {
                "success": False,
                "stdout": "\n".join(stdout_parts),
                "stderr": "\n".join(stderr_parts),
                "message": "Failed to unmount iPod partition before bootloader install",
            }

        install = _run(["pkexec", ipodpatcher["binary_path"], nodes["disk_path"], "-a", encrypted_path])
        remount = _run(["udisksctl", "mount", "-b", nodes["partition_path"]])
        install_failed = install.returncode != 0 or "[ERR]" in (install.stderr or "")
        success = not install_failed and remount.returncode == 0
        message = ""
        if install_failed:
            message = "ipodpatcher failed to install the encrypted bootloader"
        elif remount.returncode != 0:
            message = "Bootloader installed, but remount failed"
        if cleanup_path:
            try:
                os.remove(cleanup_path)
            except OSError:
                pass
        return {
            "success": success,
            "stdout": "\n".join(stdout_parts),
            "stderr": "\n".join(stderr_parts),
            "message": message,
            "disk_path": nodes["disk_path"],
            "partition_path": nodes["partition_path"],
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
    def _preview_cache_key(path):
        resolved = os.path.abspath(path)
        try:
            stat = os.stat(resolved)
            fingerprint = f"{resolved}:{stat.st_mtime_ns}:{stat.st_size}"
        except OSError:
            fingerprint = resolved
        return hashlib.sha1(fingerprint.encode("utf-8")).hexdigest()[:12]

    def _render_image(self, image_path, width, height):
        try:
            with Image.open(image_path) as img:
                rendered = ImageOps.fit(
                    img.convert("RGB"),
                    (width, height),
                    Image.Resampling.LANCZOS,
                )
        except (OSError, UnidentifiedImageError) as exc:
            raise ValueError(f"Unreadable boot image: {image_path}") from exc
        data = rendered.tobytes("raw", "RGB")
        image = QImage(data, width, height, width * 3, QImage.Format_RGB888).copy()
        return image

    def _target_dimensions(self, profile, rel_path):
        spec = self.profile_spec(profile)
        candidate = os.path.join(os.path.abspath(profile["source_repo_path"]), rel_path)
        reader = QImageReader(candidate)
        if reader.canRead():
            size = reader.size()
            if size.width() > 0 and size.height() > 0:
                return size.width(), size.height()
        return spec["width"], spec["height"]

    def _preferred_build_dirs(self, profile):
        model = str(profile.get("target_device_model") or "").strip().lower()
        resolution = str(profile.get("screen_resolution") or "").strip()
        if resolution == "320x240":
            if "6g" in model or ("classic" in model and "video" not in model):
                return ["build-hw-ipod6g", "build-hw-ipodvideo-5g", "build-hw-ipodvideo"]
            return ["build-hw-ipodvideo-5g", "build-hw-ipodvideo", "build-hw-ipod6g"]
        return list(self.profile_spec(profile).get("build_dirs", []))

    @staticmethod
    def _hash_file(path):
        if not path or not os.path.isfile(path):
            return ""
        digest = hashlib.sha256()
        with open(path, "rb") as handle:
            while True:
                chunk = handle.read(1024 * 1024)
                if not chunk:
                    break
                digest.update(chunk)
        return digest.hexdigest()

    @staticmethod
    def _remove_file(path):
        if not path:
            return False
        try:
            os.remove(path)
            return True
        except OSError:
            return False

    @staticmethod
    def _run_progress_command(command, runner=None, progress_callback=None, progress_current=0, progress_total=0, progress_label=""):
        if progress_callback:
            progress_callback(progress_current, progress_total, progress_label)
        if runner is not None:
            result = runner(
                command,
                check=False,
                capture_output=True,
                text=True,
            )
            if progress_callback:
                progress_callback(progress_current, progress_total, progress_label)
            return result

        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        stdout_parts = []
        if process.stdout is not None:
            for line in process.stdout:
                stdout_parts.append(line)
                label = line.strip() or progress_label
                if progress_callback:
                    progress_callback(progress_current, progress_total, label[:160])
        returncode = process.wait()
        return subprocess.CompletedProcess(
            command,
            returncode,
            stdout="".join(stdout_parts),
            stderr="",
        )

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
            "requires_resize": False,
        }
