import importlib.util
import struct
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools" / "rockboy_profile_gate.py"


def _load_gate():
    spec = importlib.util.spec_from_file_location("rockboy_profile_gate", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_enable_profile_logging_sets_last_option_int(tmp_path):
    gate = _load_gate()
    options_path = tmp_path / ".rockbox" / "rockboy" / "options"
    options_path.parent.mkdir(parents=True)
    values = [0] * gate.ROCKBOY_OPTION_INTS
    values[-1] = gate.PROFILE_OFF
    options_path.write_bytes(struct.pack("<" + "i" * gate.ROCKBOY_OPTION_INTS, *values))

    gate.enable_profile_logging(options_path)

    patched = struct.unpack("<" + "i" * gate.ROCKBOY_OPTION_INTS, options_path.read_bytes())
    assert patched[-1] == gate.PROFILE_OVERLAY_AND_LOG


def test_validate_profile_log_requires_new_phase1_fields(tmp_path):
    gate = _load_gate()
    profile_log = tmp_path / "profile.log"
    fields = {field: "1" for field in gate.REQUIRED_PROFILE_FIELDS}
    fields["rom"] = "/gameboy/Tetris.gb"
    profile_log.write_text(" ".join(f"{key}={value}" for key, value in fields.items()) + "\n", encoding="utf-8")

    gate.validate_profile_log(profile_log)


def test_validate_profile_log_reports_missing_new_counter(tmp_path):
    gate = _load_gate()
    profile_log = tmp_path / "profile.log"
    profile_log.write_text("rom=/gameboy/Tetris.gb rendered_frames=1\n", encoding="utf-8")

    with pytest.raises(SystemExit) as excinfo:
        gate.validate_profile_log(profile_log)

    assert "cpu_ops" in str(excinfo.value)


def _cstring(data, offset, size):
    raw = data[offset : offset + size]
    return raw.split(b"\0", 1)[0].decode("utf-8")


def test_write_rockboy_direct_start_uses_launcher_rom_argument(tmp_path):
    gate = _load_gate()
    simdisk = tmp_path / "simdisk"
    rom = simdisk / "gameboy" / "Pokemon Red.gb"
    rom.parent.mkdir(parents=True)
    rom.write_bytes(b"rom")

    plugin_dat = gate.write_rockboy_direct_start(simdisk, rom)
    data = plugin_dat.read_bytes()

    assert len(data) == gate.OPEN_PLUGIN_ENTRY_SIZE
    assert struct.unpack_from("<IiI", data, 0) == (
        gate.START_SCREEN_HASH,
        gate.LANG_START_SCREEN,
        gate.OPEN_PLUGIN_CHECKSUM,
    )
    assert _cstring(data, gate.OPEN_PLUGIN_NAME_OFFSET, gate.OPEN_PLUGIN_NAME_SIZE) == "rockboy.rock"
    assert _cstring(data, gate.OPEN_PLUGIN_PATH_OFFSET, gate.OPEN_PLUGIN_PATH_SIZE) == gate.ROCKBOY_PLUGIN_PATH
    assert _cstring(data, gate.OPEN_PLUGIN_PARAM_OFFSET, gate.OPEN_PLUGIN_PARAM_SIZE) == "@gameboy/Pokemon Red.gb"
    config = (simdisk / ".rockbox" / "config.cfg").read_text(encoding="utf-8")
    assert "start in screen: plugin" in config
    assert '"@gameboy/Pokemon Red.gb"' in config


def test_write_rockboy_direct_start_preserves_existing_start_screen_metadata(tmp_path):
    gate = _load_gate()
    simdisk = tmp_path / "simdisk"
    plugin_dat = simdisk / ".rockbox" / "rocks" / "plugin.dat"
    plugin_dat.parent.mkdir(parents=True)
    existing = bytearray(gate.OPEN_PLUGIN_ENTRY_SIZE)
    struct.pack_into("<IiI", existing, 0, 0x11111111, gate.LANG_START_SCREEN, 0x22222222)
    plugin_dat.write_bytes(existing)

    rom = simdisk / "gameboy" / "Tetris.gb"
    rom.parent.mkdir(parents=True)
    rom.write_bytes(b"rom")

    gate.write_rockboy_direct_start(simdisk, rom)

    assert struct.unpack_from("<IiI", plugin_dat.read_bytes(), 0) == (
        0x11111111,
        gate.LANG_START_SCREEN,
        0x22222222,
    )


def test_rom_only_replaces_default_patterns(monkeypatch):
    gate = _load_gate()
    captured = {}

    def fake_prepare(args):
        captured["rom"] = args.rom

    monkeypatch.setattr(gate, "prepare", fake_prepare)

    assert gate.main(["--rom-only", "Pokemon*.gb", "--direct-start"]) == 0
    assert captured["rom"] == ["Pokemon*.gb"]
