import copy
import json
import sys
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "rockpod"))

from services.maker_lite_runtime import MakerLiteRuntime  # noqa: E402
from services.maker_lite_trace import (  # noqa: E402
    MakerLiteTraceError,
    capture_trace,
    replay_trace,
    validate_trace,
)


def _project(ruleset):
    return json.loads(
        (ROOT / "testdata/maker_lite" / f"{ruleset}-test.json").read_text(
            encoding="utf-8"
        )
    )


@pytest.mark.parametrize(
    ("ruleset", "inputs"),
    [
        ("mario", [34] * 20 + [50] * 12 + [34] * 28),
        ("zelda", [2] * 15 + [18] + [2] * 10 + [32] + [8] * 20),
        ("sonic", [2] * 25 + [10] * 8 + [26] + [2] * 26),
    ],
)
def test_capture_and_replay_every_tick(tmp_path, ruleset, inputs):
    runtime = MakerLiteRuntime(str(ROOT), str(tmp_path / "private"))
    project = _project(ruleset)
    trace = capture_trace(runtime, project, inputs)
    result = replay_trace(runtime, project, trace)
    assert result.ticks == len(inputs)
    assert result.final_digest == int(trace["frames"][-1]["digest"], 16)

    corrupted = copy.deepcopy(trace)
    corrupted["frames"][9]["player"][0] += 1
    with pytest.raises(MakerLiteTraceError, match="tick 10"):
        replay_trace(runtime, project, corrupted)


def test_trace_contract_rejects_assets_and_malformed_timing(tmp_path):
    runtime = MakerLiteRuntime(str(ROOT), str(tmp_path / "private"))
    trace = capture_trace(runtime, _project("mario"), [0, 2, 2])
    trace["frames"][1]["tick"] = 3
    with pytest.raises(MakerLiteTraceError, match="noncanonical tick"):
        validate_trace(trace)

    trace = capture_trace(runtime, _project("mario"), [0])
    trace["image"] = "not part of the bounded contract"
    with pytest.raises(MakerLiteTraceError, match="unknown trace field"):
        validate_trace(trace)
