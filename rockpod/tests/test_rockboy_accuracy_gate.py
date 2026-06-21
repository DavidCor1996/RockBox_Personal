"""Tests for the Rockboy accuracy ROM simulator gate."""

import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools" / "rockboy_accuracy_gate.py"


def _load_gate():
    spec = importlib.util.spec_from_file_location("rockboy_accuracy_gate", SCRIPT)
    gate = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(gate)
    return gate


def test_validate_accepts_expected_serial_output(tmp_path):
    gate = _load_gate()
    simdisk = tmp_path / "simdisk"
    serial_log = simdisk / ".rockbox" / "rockboy" / "serial.log"
    serial_log.parent.mkdir(parents=True)
    serial_log.write_bytes(b"cpu_instrs\n\nPassed\n")

    class Args:
        validate = str(simdisk)
        expect = "Passed"

    gate.validate(Args)


def test_validate_rejects_missing_expected_serial_output(tmp_path):
    gate = _load_gate()
    simdisk = tmp_path / "simdisk"
    serial_log = simdisk / ".rockbox" / "rockboy" / "serial.log"
    serial_log.parent.mkdir(parents=True)
    serial_log.write_bytes(b"cpu_instrs\n\nFailed #02\n")

    class Args:
        validate = str(simdisk)
        expect = "Passed"

    try:
        gate.validate(Args)
    except SystemExit as exc:
        assert "expected 'Passed'" in str(exc)
    else:
        raise AssertionError("validate should reject serial output without the expected text")


def test_copy_rom_stages_accuracy_rom_under_gameboy(tmp_path):
    gate = _load_gate()
    simdisk = tmp_path / "simdisk"
    source_rom = tmp_path / "cpu_instrs.gb"
    source_rom.write_bytes(b"rom")

    staged = gate.copy_rom(simdisk, source_rom)

    assert staged == simdisk / "gameboy" / "accuracy" / "cpu_instrs.gb"
    assert staged.read_bytes() == b"rom"


def test_force_dmg_header_patches_staged_cgb_flag(tmp_path):
    gate = _load_gate()
    rom = tmp_path / "oam_bug.gb"
    data = bytearray(0x150)
    data[0x143] = 0x80
    rom.write_bytes(data)

    gate.force_dmg_header(rom)

    assert rom.read_bytes()[0x143] == 0x00
