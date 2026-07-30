"""Per-project Maker Lite controls stored independently from level packs."""

from __future__ import annotations

import json
import os
import tempfile
import zlib


ACTIONS = ("primary", "secondary", "previous", "next")
PHYSICAL_BUTTONS = ("select", "play", "previous", "next")
PRESETS = {
    "stock": {
        "select": "primary",
        "play": "secondary",
        "previous": "previous",
        "next": "next",
    },
    "left_handed": {
        "select": "secondary",
        "play": "primary",
        "previous": "previous",
        "next": "next",
    },
}


class MakerLiteSettingsError(ValueError):
    pass


def default_settings() -> dict:
    return {"preset": "stock", "mapping": dict(PRESETS["stock"])}


def validate_settings(settings: dict) -> dict:
    if not isinstance(settings, dict):
        raise MakerLiteSettingsError("control settings must be an object")
    preset = str(settings.get("preset", "stock"))
    if preset in PRESETS:
        mapping = dict(PRESETS[preset])
    elif preset == "custom":
        source = settings.get("mapping")
        if not isinstance(source, dict):
            raise MakerLiteSettingsError("custom controls need a button mapping")
        mapping = {button: str(source.get(button, "")) for button in PHYSICAL_BUTTONS}
        if set(mapping.values()) != set(ACTIONS):
            raise MakerLiteSettingsError(
                "custom controls must assign each action exactly once"
            )
    else:
        raise MakerLiteSettingsError("unknown control preset")
    return {"preset": preset, "mapping": mapping}


def _settings_path(private_root: str, project_id: str) -> str:
    safe_id = os.path.basename(project_id)
    if safe_id != project_id or not safe_id:
        raise MakerLiteSettingsError("invalid project ID")
    return os.path.join(os.path.abspath(private_root), "settings", safe_id + ".json")


def load_settings(private_root: str, project_id: str) -> dict:
    path = _settings_path(private_root, project_id)
    try:
        with open(path, "r", encoding="utf-8") as source:
            return validate_settings(json.load(source))
    except FileNotFoundError:
        return default_settings()


def save_settings(private_root: str, project_id: str, settings: dict) -> dict:
    normalized = validate_settings(settings)
    path = _settings_path(private_root, project_id)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=".maker-lite-", dir=os.path.dirname(path))
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as output:
            json.dump(normalized, output, indent=2, sort_keys=True)
            output.write("\n")
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
    except Exception:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise
    return normalized


def compile_device_settings(project_id: str, settings: dict) -> bytes:
    normalized = validate_settings(settings)
    encoded_id = project_id.encode("ascii")
    if not 1 <= len(encoded_id) <= 32:
        raise MakerLiteSettingsError("invalid project ID")
    data = bytearray(64)
    data[:4] = b"MLCT"
    data[4:6] = (1).to_bytes(2, "little")
    mapping = normalized["mapping"]
    for index, button in enumerate(PHYSICAL_BUTTONS):
        data[6 + index] = ACTIONS.index(mapping[button])
    data[12:12 + len(encoded_id)] = encoded_id
    data[60:64] = (zlib.crc32(data[:60]) & 0xFFFFFFFF).to_bytes(4, "little")
    return bytes(data)
