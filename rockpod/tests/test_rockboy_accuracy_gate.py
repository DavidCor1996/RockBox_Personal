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


def test_validate_accepts_hosted_serial_output(tmp_path):
    gate = _load_gate()
    simdisk = tmp_path / "simdisk"
    serial_log = simdisk / ".config" / "rockbox.org" / "rockboy" / "serial.log"
    serial_log.parent.mkdir(parents=True)
    serial_log.write_bytes(b"oam_bug\n\nPassed\n")

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


def test_mirror_hosted_config_copies_direct_start_files(tmp_path):
    gate = _load_gate()
    simdisk = tmp_path / "simdisk"
    config = simdisk / ".rockbox" / "config.cfg"
    plugin_dat = simdisk / ".rockbox" / "rocks" / "plugin.dat"
    rockboy = simdisk / gate.ROCKBOY_PLUGIN_PATH.lstrip("/")
    config.parent.mkdir(parents=True)
    plugin_dat.parent.mkdir(parents=True)
    rockboy.parent.mkdir(parents=True)
    config.write_text("start in screen: plugin\n", encoding="utf-8")
    plugin_dat.write_bytes(b"plugin-dat")
    rockboy.write_bytes(b"rockboy")

    gate.mirror_hosted_config(simdisk)

    assert (simdisk / ".config" / "rockbox.org" / "config.cfg").read_text(encoding="utf-8") == "start in screen: plugin\n"
    assert (simdisk / ".config" / "rockbox.org" / "rocks" / "plugin.dat").read_bytes() == b"plugin-dat"
    assert (simdisk / ".config" / "rockbox.org" / "rocks" / "viewers" / "rockboy.rock").read_bytes() == b"rockboy"


def test_minimal_simdisk_ignore_skips_stale_screen_dumps(tmp_path):
    gate = _load_gate()
    simdisk = tmp_path / "simdisk"
    names = [".rockbox", "config.cfg", "dump 260621-160316.bmp", "dump-extra.bmp", "music"]

    ignored = gate.minimal_simdisk_ignore(str(simdisk), names)

    assert ".rockbox" not in ignored
    assert "config.cfg" not in ignored
    assert "dump 260621-160316.bmp" in ignored
    assert "dump-extra.bmp" in ignored
    assert "music" in ignored


def test_minimal_simdisk_ignore_skips_stale_rockboy_runtime_state(tmp_path):
    gate = _load_gate()
    rockbox = tmp_path / "simdisk" / ".rockbox"
    names = ["rocks", "rockboy", "config.cfg"]

    ignored = gate.minimal_simdisk_ignore(str(rockbox), names)

    assert "rockboy" in ignored
    assert "rocks" not in ignored
    assert "config.cfg" not in ignored


def test_run_launches_simulator_with_accuracy_logging_and_stops(monkeypatch, tmp_path):
    gate = _load_gate()
    simdisk = tmp_path / "simdisk"
    serial_log = simdisk / ".rockbox" / "rockboy" / "serial.log"
    serial_log.parent.mkdir(parents=True)
    serial_log.write_text("4-scanline_timing\n\nPassed\n", encoding="ascii")
    captured = {}

    class FakeProcess:
        def __init__(self):
            self.terminated = False

        def poll(self):
            return None

        def terminate(self):
            self.terminated = True

        def wait(self, timeout=None):
            return 0

        def kill(self):
            raise AssertionError("process should terminate cleanly")

    fake_process = FakeProcess()

    def fake_prepare(args):
        return simdisk

    def fake_popen(command, cwd, env, text):
        captured["command"] = command
        captured["cwd"] = cwd
        captured["env"] = env
        captured["text"] = text
        return fake_process

    monkeypatch.setattr(gate, "prepare", fake_prepare)
    monkeypatch.setattr(gate.subprocess, "Popen", fake_popen)

    class Args:
        build_dir = "build-sim-video-5g"
        rom = "/tmp/test.gb"
        expect = "Passed"
        force_dmg = True
        timeout = 1.0
        poll_interval = 0.01

    gate.run(Args)

    assert captured["command"][0].endswith("build-sim-video-5g/rockboxui")
    assert captured["env"]["RBROOT"] == str(simdisk)
    assert captured["env"]["ROCKBOY_SERIAL_LOG"] == "1"
    assert captured["env"]["ROCKBOY_ACCURACY_LOG"] == "1"
    assert captured["env"]["ROCKBOY_ACCURACY_FAST"] == "1"
    assert captured["env"]["ROCKBOX_SIM_PLUGIN"] == gate.ROCKBOY_PLUGIN_PATH
    assert captured["env"]["ROCKBOX_SIM_PLUGIN_PARAM"] == "/gameboy/accuracy/test.gb"
    assert fake_process.terminated
