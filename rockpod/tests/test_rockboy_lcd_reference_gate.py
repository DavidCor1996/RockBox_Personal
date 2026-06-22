"""Unit tests for the Rockboy LCD reference gate helpers."""

from __future__ import annotations

import importlib.util
import struct
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def _load_gate():
    path = REPO_ROOT / "tools" / "rockboy_lcd_reference_gate.py"
    spec = importlib.util.spec_from_file_location("rockboy_lcd_reference_gate", path)
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_write_lcd_options_preserves_keys_and_forces_deterministic_lcd_settings(tmp_path):
    gate = _load_gate()
    options_path = tmp_path / "options"
    original = list(range(gate.ROCKBOY_OPTION_INTS))
    options_path.write_bytes(struct.pack("<" + "i" * gate.ROCKBOY_OPTION_INTS, *original))

    gate.write_lcd_options(options_path, palette=1, performance_preset=2)

    values = list(struct.unpack(
        "<" + "i" * gate.ROCKBOY_OPTION_INTS,
        options_path.read_bytes(),
    ))
    assert values[:9] == original[:9]
    assert values[gate.OPTION_FRAMESKIP] == 0
    assert values[gate.OPTION_MAXSKIP] == 0
    assert values[gate.OPTION_SOUND] == 0
    assert values[gate.OPTION_SCALING] == 2
    assert values[gate.OPTION_SHOWSTATS] == 0
    assert values[gate.OPTION_ROTATE] == 0
    assert values[gate.OPTION_PAL] == 1
    assert values[gate.OPTION_DIRTY] == 1
    assert values[gate.OPTION_PERFORMANCE_PRESET] == 2
    assert values[gate.OPTION_PROFILE] == 0


def test_write_lcd_options_can_enable_sound_for_rom_repro_runs(tmp_path):
    gate = _load_gate()
    options_path = tmp_path / "options"

    gate.write_lcd_options(options_path, palette=1, performance_preset=2, sound=True)

    values = list(struct.unpack(
        "<" + "i" * gate.ROCKBOY_OPTION_INTS,
        options_path.read_bytes(),
    ))
    assert values[gate.OPTION_SOUND] == 1


def test_lcd_dump_completion_requires_full_raw_frame(tmp_path):
    gate = _load_gate()
    dump = tmp_path / "lcd-reference.ppm"

    dump.write_bytes(gate.LCD_PPM_HEADER + b"\0" * (gate.LCD_RGB_BYTES - 1))
    assert not gate.is_complete_ppm(dump)

    dump.write_bytes(gate.LCD_PPM_HEADER + b"\0" * gate.LCD_RGB_BYTES)
    assert gate.is_complete_ppm(dump)


def test_lcd_nonblank_validation_rejects_single_color_frame(tmp_path):
    gate = _load_gate()
    dump = tmp_path / "lcd-reference.ppm"
    dump.write_bytes(gate.LCD_PPM_HEADER + b"\0" * gate.LCD_RGB_BYTES)

    try:
        gate.validate_nonblank_dump(dump)
    except SystemExit as exc:
        assert "appears blank" in str(exc)
    else:
        raise AssertionError("blank LCD dump should be rejected")


def test_lcd_nonblank_validation_accepts_mixed_frame(tmp_path):
    gate = _load_gate()
    dump = tmp_path / "lcd-reference.ppm"
    body = bytearray(b"\0" * gate.LCD_RGB_BYTES)
    body[-3:] = b"\xff\xff\xff"
    dump.write_bytes(gate.LCD_PPM_HEADER + body)

    gate.validate_nonblank_dump(dump)


def test_lcd_gate_supports_dump_only_mode():
    gate_source = (REPO_ROOT / "tools" / "rockboy_lcd_reference_gate.py").read_text(encoding="utf-8")

    assert "--dump-only" in gate_source
    assert "captured LCD dump:" in gate_source
    assert "--reference is required with --run unless --dump-only is set" in gate_source


def test_lcd_gate_exposes_scripted_input_for_interactive_rom_repro_runs():
    gate_source = (REPO_ROOT / "tools" / "rockboy_lcd_reference_gate.py").read_text(encoding="utf-8")

    assert "--input-script" in gate_source
    assert "ROCKBOY_INPUT_SCRIPT" in gate_source


def test_lcd_gate_uses_direct_simulator_plugin_launch():
    gate_source = (REPO_ROOT / "tools" / "rockboy_lcd_reference_gate.py").read_text(encoding="utf-8")

    assert "ROCKBOX_SIM_PLUGIN" in gate_source
    assert "ROCKBOX_SIM_PLUGIN_PARAM" in gate_source


def test_lcd_gate_requires_nonblank_by_default():
    gate_source = (REPO_ROOT / "tools" / "rockboy_lcd_reference_gate.py").read_text(encoding="utf-8")

    assert "--require-nonblank" in gate_source
    assert "default=True" in gate_source
