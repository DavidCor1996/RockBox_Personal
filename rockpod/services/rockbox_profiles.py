"""Persistent Rockbox profile model for theme deployment."""

from __future__ import annotations

import copy
import os
import re


def _slug(value):
    text = re.sub(r"[^a-z0-9]+", "-", str(value or "").strip().lower())
    text = text.strip("-")
    return text or "profile"


def _normalize_games_target_dir(value, default_value="gameboy"):
    text = str(value or "").strip()
    legacy_values = {
        "",
        "Gameboy",
        "GameBoy",
        "gameboy",
        ".rockbox/rockpod/games/rockboy",
        ".rockbox/rocks/games/roms",
    }
    if text in legacy_values:
        return default_value
    return text


def _normalize_clock_position(value):
    text = str(value or "").strip().lower()
    if text == "left":
        return "left"
    return "center"


class RockboxProfileStore:
    """Persist Rockbox deployment profiles inside the app config."""

    def __init__(self, config, repo_root):
        self._config = config
        self._repo_root = os.path.abspath(repo_root)

    def profiles(self):
        stored = self._config.get("rockbox_profiles", [])
        if not stored:
            stored = self._default_profiles()
            self._config.set("rockbox_profiles", stored)
            self._config.set("rockbox_selected_profile_id", stored[0]["id"])
            self._config.save()
        normalized = [self._normalize_profile(item) for item in stored]
        selected = self.selected_profile_id()
        if not any(item["id"] == selected for item in normalized):
            self._config.set("rockbox_selected_profile_id", normalized[0]["id"])
            self._config.save()
        return normalized

    def selected_profile_id(self):
        profiles = self._config.get("rockbox_profiles", []) or self._default_profiles()
        default_id = profiles[0]["id"] if profiles else ""
        return self._config.get("rockbox_selected_profile_id", default_id) or default_id

    def current_profile(self):
        selected = self.selected_profile_id()
        for profile in self.profiles():
            if profile["id"] == selected:
                return profile
        profiles = self.profiles()
        return profiles[0] if profiles else None

    def set_selected_profile(self, profile_id):
        profiles = self.profiles()
        if any(item["id"] == profile_id for item in profiles):
            self._config.set("rockbox_selected_profile_id", profile_id)
            self._config.save()

    def save_profile(self, profile_data):
        profile = self._normalize_profile(profile_data)
        profiles = self.profiles()
        updated = False
        for index, existing in enumerate(profiles):
            if existing["id"] == profile["id"]:
                profiles[index] = profile
                updated = True
                break
        if not updated:
            profiles.append(profile)
        self._config.set("rockbox_profiles", profiles)
        self._config.set("rockbox_selected_profile_id", profile["id"])
        self._config.save()
        return profile

    def sync_with_device(self, device):
        if device is None:
            return self.current_profile()
        profiles = self.profiles()
        selected = self.current_profile() or profiles[0]
        selected["device_mount_path"] = getattr(device, "mount_path", "") or selected["device_mount_path"]
        selected["target_device_model"] = (
            getattr(device, "detected_model", "")
            or getattr(device, "name", "")
            or selected["target_device_model"]
        )
        detected_resolution = str(getattr(device, "screen_resolution", "") or "").strip()
        if detected_resolution:
            selected["screen_resolution"] = detected_resolution
        elif not selected.get("screen_resolution"):
            selected["screen_resolution"] = self._guess_resolution(selected["target_device_model"], selected["selected_theme"])
        return self.save_profile(selected)

    def _default_profiles(self):
        repo_root = self._repo_root
        backup_root = os.path.join(repo_root, ".backups")
        default_mount = self._config.get("device_mount_path", "") or ""
        return [
            {
                "id": "ipod_320x240",
                "name": "iPod Classic / Video",
                "device_mount_path": default_mount,
                "target_device_model": "iPod Classic / Video",
                "screen_resolution": "320x240",
                "source_repo_path": repo_root,
                "selected_theme": "iPone",
                "backup_location": os.path.join(backup_root, "ipod_320x240"),
                "lockscreen_clock_position": "center",
            },
            {
                "id": "ipod_nano2g",
                "name": "iPod nano 2G",
                "device_mount_path": "",
                "target_device_model": "iPod nano 2G",
                "screen_resolution": "176x132",
                "source_repo_path": repo_root,
                "selected_theme": "iPone_nano2g",
                "backup_location": os.path.join(backup_root, "ipod_nano2g"),
                "lockscreen_clock_position": "center",
            },
        ]

    def _normalize_profile(self, profile_data):
        item = copy.deepcopy(profile_data or {})
        name = str(item.get("name") or item.get("target_device_model") or "Rockbox Device").strip()
        profile_id = _slug(item.get("id") or name)
        source_repo_path = os.path.abspath(item.get("source_repo_path") or self._repo_root)
        backup_location = item.get("backup_location") or os.path.join(source_repo_path, ".backups", profile_id)
        screen_resolution = str(item.get("screen_resolution") or "").strip()
        selected_theme = str(item.get("selected_theme") or "iPone").strip() or "iPone"
        games_device_target_dir = _normalize_games_target_dir(
            item.get("games_device_target_dir"),
            _normalize_games_target_dir(self._config.get("games_device_target_dir"), "gameboy"),
        )
        games_simulator_target_dir = _normalize_games_target_dir(
            item.get("games_simulator_target_dir"),
            _normalize_games_target_dir(self._config.get("games_simulator_target_dir"), "gameboy"),
        )
        if not screen_resolution:
            screen_resolution = self._guess_resolution(item.get("target_device_model"), selected_theme)
        return {
            "id": profile_id,
            "name": name,
            "device_mount_path": str(item.get("device_mount_path") or "").strip(),
            "target_device_model": str(item.get("target_device_model") or name).strip(),
            "screen_resolution": screen_resolution,
            "source_repo_path": source_repo_path,
            "selected_theme": selected_theme,
            "backup_location": os.path.abspath(backup_location),
            "lockscreen_clock_position": _normalize_clock_position(item.get("lockscreen_clock_position")),
            "simulator_target": str(item.get("simulator_target") or "").strip(),
            "simulator_binary_path": str(item.get("simulator_binary_path") or "").strip(),
            "simulator_simdisk_path": str(item.get("simulator_simdisk_path") or "").strip(),
            "simulator_screenshot_dir": os.path.abspath(
                item.get("simulator_screenshot_dir") or os.path.join(source_repo_path, "simshots")
            ),
            "boot_image_path": str(item.get("boot_image_path") or "").strip(),
            "games_library_path": os.path.abspath(
                str(item.get("games_library_path") or self._config.get("games_library_path", "")).strip()
            ),
            "games_device_target_dir": games_device_target_dir,
            "games_simulator_target_dir": games_simulator_target_dir,
        }

    @staticmethod
    def _guess_resolution(device_model, selected_theme):
        model = str(device_model or "").lower()
        theme = str(selected_theme or "").lower()
        if "nano" in model or "nano2g" in theme:
            return "176x132"
        return "320x240"
