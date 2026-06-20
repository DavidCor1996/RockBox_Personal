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
