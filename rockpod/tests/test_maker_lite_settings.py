import sys
import zlib
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "rockpod"))

from services.maker_lite_settings import (  # noqa: E402
    MakerLiteSettingsError,
    compile_device_settings,
    load_settings,
    save_settings,
)


def test_external_control_presets_roundtrip_and_compile(tmp_path):
    project_id = "controls-test"
    assert load_settings(str(tmp_path), project_id)["preset"] == "stock"
    settings = save_settings(
        str(tmp_path),
        project_id,
        {
            "preset": "custom",
            "mapping": {
                "select": "secondary",
                "play": "primary",
                "previous": "next",
                "next": "previous",
            },
        },
    )
    assert load_settings(str(tmp_path), project_id) == settings
    data = compile_device_settings(project_id, settings)
    assert len(data) == 64
    assert data[:6] == b"MLCT\x01\x00"
    assert data[6:10] == bytes([1, 0, 3, 2])
    assert int.from_bytes(data[60:64], "little") == (
        zlib.crc32(data[:60]) & 0xFFFFFFFF
    )


def test_custom_controls_must_be_bijective(tmp_path):
    with pytest.raises(MakerLiteSettingsError, match="exactly once"):
        save_settings(
            str(tmp_path),
            "bad-controls",
            {
                "preset": "custom",
                "mapping": {
                    "select": "primary",
                    "play": "primary",
                    "previous": "previous",
                    "next": "next",
                },
            },
        )
