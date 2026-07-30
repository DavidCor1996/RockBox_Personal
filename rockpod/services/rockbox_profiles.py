"""Persistent Rockbox profile model for theme deployment."""

from __future__ import annotations

import copy
import os
import re


# Mirrors services.xbox_avatar so a stored profile always round-trips through
# the same set of real wardrobe parts and colour swatches the creator offers.
_AVATAR_STYLE_VALUES = {
    "hair_style": {"original", "boy-short", "girl-bob", "shaved"},
    "top_style": {"original", "crew-tee", "scoop-tee", "none"},
    "bottom_style": {"original", "jeans", "shorts"},
    "shoes_style": {"original", "sneakers", "flats", "heels", "barefoot"},
}
_AVATAR_COLOUR_VALUES = {
    "skin_colour": {"original", "porcelain", "light", "medium", "tan", "deep",
                    "rich"},
    "hair_colour": {"original", "black", "brown", "chestnut", "blond",
                    "auburn", "red", "silver"},
    "top_colour": {"original", "xbox-green", "blue", "navy", "red", "black",
                   "white", "purple", "orange"},
    "bottom_colour": {"original", "denim", "black", "gray", "khaki", "white"},
    "shoes_colour": {"original", "white", "black", "brown", "red", "blue"},
    "eye_colour": {"original", "brown", "blue", "green", "hazel", "grey"},
    "brow_colour": {"original", "black", "brown", "blond", "auburn"},
    "lip_colour": {"original", "natural", "rose", "red", "berry"},
}
_AVATAR_DECAL_VALUES = {
    "original", "none", "rockbox", "rockbox-icon", "apple", "album-art",
    "ipod-play", "ipod-volume",
}
_AVATAR_DESIGN_VALUES = {
    "none", "solid", "stripes", "pinstripe", "diagonal", "chevron", "checks",
    "plaid", "tartan", "dots", "halftone", "rings", "rays", "waves", "argyle",
    "houndstooth", "camo", "marble", "fade", "grid", "triangles", "static",
}
_AVATAR_SCALE_VALUES = {"fine", "small", "medium", "large", "huge"}
_AVATAR_ACCENT_VALUES = {
    "white", "black", "xbox-green", "blue", "red", "gold", "purple", "orange",
}
_AVATAR_APPEARANCE_VALUES = {
    **_AVATAR_STYLE_VALUES,
    **_AVATAR_COLOUR_VALUES,
    **{f"{slot}_design": _AVATAR_DESIGN_VALUES
       for slot in ("top", "bottom", "shoes")},
    **{f"{slot}_design_scale": _AVATAR_SCALE_VALUES
       for slot in ("top", "bottom", "shoes")},
    "accent_colour": _AVATAR_ACCENT_VALUES,
    "decal": _AVATAR_DECAL_VALUES,
}
# Imported Marketplace item names come from the installed pack, so they are
# sanitised here and validated by the renderer rather than being enumerated.
_AVATAR_MARKETPLACE_FIELDS = ("costume", "marketplace_top", "headwear", "prop")
_AVATAR_APPEARANCE_DEFAULTS = {
    **{f"{slot}_design": "none" for slot in ("top", "bottom", "shoes")},
    **{f"{slot}_design_scale": "medium"
       for slot in ("top", "bottom", "shoes")},
    "accent_colour": "black",
}

# Profiles written before the real rig landed stored one palette name per
# clothing slot; those names are still valid colours.
_AVATAR_LEGACY_COLOURS = {
    "skin": "skin_colour",
    "hair": "hair_colour",
    "top": "top_colour",
    "bottom": "bottom_colour",
    "shoes": "shoes_colour",
}


def _normalize_avatar_appearance(field, value):
    default = _AVATAR_APPEARANCE_DEFAULTS.get(field, "original")
    value = str(value or default)
    return value if value in _AVATAR_APPEARANCE_VALUES[field] else default


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
    if text in {"top", "center", "lower", "custom", "left"}:
        return text
    return "center"


def _normalize_hex_color(value, default_value):
    text = str(value or "").strip().lstrip("#").upper()
    if re.fullmatch(r"[0-9A-F]{6}", text):
        return text
    return default_value


def _normalize_lockscreen_customization(value):
    item = copy.deepcopy(value) if isinstance(value, dict) else {}
    clock = item.get("clock") if isinstance(item.get("clock"), dict) else {}
    date = item.get("date") if isinstance(item.get("date"), dict) else {}
    readability = item.get("readability") if isinstance(item.get("readability"), dict) else {}
    mini_player = item.get("mini_player") if isinstance(item.get("mini_player"), dict) else {}
    position = _normalize_clock_position(clock.get("position") or item.get("clock_position"))
    align = str(clock.get("align") or "center").strip().lower()
    if align not in {"left", "center", "right"}:
        align = "center"
    style = str(clock.get("style") or "solid").strip().lower()
    if style not in {"solid", "soft shadow", "outline", "glass", "glass tinted"}:
        style = "solid"
    glass_strength = str(clock.get("glass_strength") or "off").strip().lower()
    if glass_strength not in {"off", "low", "medium", "high"}:
        glass_strength = "off"
    date_mode = str(date.get("mode") or "below").strip().lower()
    if date_mode not in {"follow", "above", "below", "hidden"}:
        date_mode = "below"
    blur_strength = str(mini_player.get("blur_strength") or "medium").strip().lower()
    if blur_strength not in {"low", "medium", "high"}:
        blur_strength = "medium"
    return {
        "wallpaper_id": str(item.get("wallpaper_id") or "").strip(),
        "clock": {
            "position": position,
            "x": int(clock.get("x", 0) or 0),
            "y": int(clock.get("y", 32) or 32),
            "width": int(clock.get("width", 320) or 320),
            "height": int(clock.get("height", 55) or 55),
            "align": align,
            "font": str(clock.get("font") or "35-Adobe-Helvetica-Bold.fnt").strip(),
            "style": style,
            "color": _normalize_hex_color(clock.get("color"), "FFFFFF"),
            "shadow": "soft" if str(clock.get("shadow") or "soft").strip().lower() != "off" else "off",
            "glass_strength": glass_strength,
            "opacity": int(clock.get("opacity", 82) or 82),
        },
        "date": {
            "mode": date_mode,
            "y": int(date.get("y", 101) or 101),
            "font": str(date.get("font") or "16-Adobe-Helvetica-Bold.fnt").strip(),
            "color": _normalize_hex_color(date.get("color"), "FFFFFF"),
        },
        "readability": {
            "auto_contrast": bool(readability.get("auto_contrast", True)),
            "min_contrast": float(readability.get("min_contrast", 4.5) or 4.5),
            "sample_region": str(readability.get("sample_region") or "clock_box").strip(),
        },
        "mini_player": {
            "style": str(mini_player.get("style") or "matched_blur").strip(),
            "blur_strength": blur_strength,
            "tint_source": str(mini_player.get("tint_source") or "wallpaper").strip(),
            "tint_color": _normalize_hex_color(mini_player.get("tint_color"), "2D2936"),
            "text_color": _normalize_hex_color(mini_player.get("text_color"), "FFFFFF"),
            "secondary_text_color": _normalize_hex_color(mini_player.get("secondary_text_color"), "C8BED7"),
        },
    }


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
                "lockscreen_customization": _normalize_lockscreen_customization({}),
            },
            {
                "id": "ipod_3g",
                "name": "iPod 3G",
                "device_mount_path": "",
                "target_device_model": "iPod 3G",
                "screen_resolution": "160x128",
                "source_repo_path": repo_root,
                "selected_theme": "CoverPod_3g",
                "backup_location": os.path.join(backup_root, "ipod_3g"),
                "lockscreen_clock_position": "center",
                "lockscreen_customization": _normalize_lockscreen_customization({}),
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
                "lockscreen_customization": _normalize_lockscreen_customization({}),
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
            "lockscreen_customization": _normalize_lockscreen_customization(item.get("lockscreen_customization")),
            "simulator_target": str(item.get("simulator_target") or "").strip(),
            "simulator_binary_path": str(item.get("simulator_binary_path") or "").strip(),
            "simulator_simdisk_path": str(item.get("simulator_simdisk_path") or "").strip(),
            "simulator_screenshot_dir": os.path.abspath(
                item.get("simulator_screenshot_dir") or os.path.join(source_repo_path, "simshots")
            ),
            "boot_image_path": str(item.get("boot_image_path") or "").strip(),
            "photos_library_path": os.path.abspath(
                str(item.get("photos_library_path") or self._config.get("photos_library_path", "")).strip()
            ),
            "photos_device_target_dir": str(
                item.get("photos_device_target_dir")
                or self._config.get("photos_device_target_dir", "Photos")
            ).strip() or "Photos",
            "photos_simulator_target_dir": str(
                item.get("photos_simulator_target_dir")
                or self._config.get("photos_simulator_target_dir", "Photos")
            ).strip() or "Photos",
            "games_library_path": os.path.abspath(
                str(item.get("games_library_path") or self._config.get("games_library_path", "")).strip()
            ),
            "games_genesis_library_path": os.path.abspath(
                str(
                    item.get("games_genesis_library_path")
                    or self._config.get("games_genesis_library_path", "")
                ).strip()
            ) if str(
                item.get("games_genesis_library_path")
                or self._config.get("games_genesis_library_path", "")
            ).strip() else "",
            "games_device_target_dir": games_device_target_dir,
            "games_simulator_target_dir": games_simulator_target_dir,
            "games_show_builtin_doom": bool(
                item.get("games_show_builtin_doom", self._config.get("games_show_builtin_doom", True))
            ),
            "games_show_builtin_stickrpg": bool(
                item.get("games_show_builtin_stickrpg", self._config.get("games_show_builtin_stickrpg", True))
            ),
            "games_show_builtin_runescape": bool(
                item.get("games_show_builtin_runescape", self._config.get("games_show_builtin_runescape", True))
            ),
            "retroachievements_username": str(
                item.get("retroachievements_username")
                or self._config.get("retroachievements_username", "")
            ).strip(),
            "retroachievements_web_api_key": str(
                item.get("retroachievements_web_api_key")
                or self._config.get("retroachievements_web_api_key", "")
            ).strip(),
            "xbox_avatar_display_name": str(
                item.get("xbox_avatar_display_name")
                or self._config.get("xbox_avatar_display_name", "OFFLINE PLAYER")
            ).strip()[:15] or "OFFLINE PLAYER",
            "xbox_avatar_body": (
                str(item.get("xbox_avatar_body")
                    or self._config.get("xbox_avatar_body", "xna-boy"))
                if str(item.get("xbox_avatar_body")
                       or self._config.get("xbox_avatar_body", "xna-boy"))
                    in {"xna-boy", "xna-girl", "xna-girl-heels"}
                    else "xna-boy"
            ),
            "xbox_avatar_favorite_clip": (
                str(item.get("xbox_avatar_favorite_clip")
                    or self._config.get("xbox_avatar_favorite_clip", "jump"))
                if str(item.get("xbox_avatar_favorite_clip")
                       or self._config.get("xbox_avatar_favorite_clip", "jump"))
                in {
                    "jump", "throw", "faint", "sit-idle",
                    "punch", "kick", "walk",
                } else "jump"
            ),
            **{
                f"xbox_avatar_{field}": re.sub(
                    r"[^a-z0-9-]", "",
                    str(item.get(f"xbox_avatar_{field}")
                        or self._config.get(f"xbox_avatar_{field}")
                        or "none").lower()
                ) or "none"
                for field in _AVATAR_MARKETPLACE_FIELDS
            },
            **{
                f"xbox_avatar_{field}": _normalize_avatar_appearance(
                    field,
                    item.get(f"xbox_avatar_{field}")
                    or self._config.get(f"xbox_avatar_{field}")
                    or (item.get(f"xbox_avatar_{legacy}")
                        or self._config.get(f"xbox_avatar_{legacy}")
                        if (legacy := next(
                            (old for old, new in _AVATAR_LEGACY_COLOURS.items()
                             if new == field), None)) else None)
                    or None
                )
                for field in _AVATAR_APPEARANCE_VALUES
            },
        }

    @staticmethod
    def _guess_resolution(device_model, selected_theme):
        model = str(device_model or "").lower()
        theme = str(selected_theme or "").lower()
        if "ipod 3g" in model or theme in {"ipone_3g", "galaxy"}:
            return "160x128"
        if "nano" in model or "nano2g" in theme:
            return "176x132"
        return "320x240"
