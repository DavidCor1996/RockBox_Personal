"""Bounded, asset-free behavior traces for Maker Lite parity testing.

Traces intentionally contain only inputs and deterministic gameplay state.
Authentic traces made from a user's reference game remain private; the
repository may carry synthetic traces produced by the Maker Lite fixtures.
"""

from __future__ import annotations

import hashlib
import json
import re
from dataclasses import dataclass
from pathlib import Path

from services.maker_lite_runtime import MakerLiteRuntime


TRACE_FORMAT_VERSION = 1
TRACE_TICK_HZ = 60
MAX_TRACE_TICKS = 36_000
VALID_RULESETS = {"mario", "zelda", "sonic"}
VALID_KINDS = {"synthetic", "authentic"}
_HEX_8 = re.compile(r"^[0-9a-f]{8}$")
_HEX_64 = re.compile(r"^[0-9a-f]{64}$")


class MakerLiteTraceError(ValueError):
    pass


@dataclass(frozen=True)
class TraceReplay:
    ruleset: str
    ticks: int
    final_digest: int
    source_revision: str


def _source_digest(project: dict) -> str:
    encoded = json.dumps(
        project, sort_keys=True, separators=(",", ":"), ensure_ascii=True
    ).encode("ascii")
    return hashlib.sha256(encoded).hexdigest()


def _frame(snapshot: dict, digest: int) -> dict:
    return {
        "tick": int(snapshot["tick"]),
        "input": 0,
        "player": [
            int(snapshot["player_x"]),
            int(snapshot["player_y"]),
            int(snapshot["player_vx"]),
            int(snapshot["player_vy"]),
        ],
        "camera": [
            int(snapshot["camera_x"]),
            int(snapshot["camera_y"]),
        ],
        "state": [
            int(snapshot["action"]),
            int(snapshot["grounded"]),
            int(snapshot["health"]),
            int(snapshot["collectibles"]),
            int(snapshot["rings"]),
            int(snapshot["keys"]),
            int(snapshot["complete"]),
            int(snapshot["paused"]),
        ],
        "digest": f"{digest:08x}",
    }


def capture_trace(
    runtime: MakerLiteRuntime,
    project: dict,
    inputs: list[int],
    *,
    kind: str = "synthetic",
    source_revision: str = "maker-lite-synthetic-v1",
    source_sha256: str | None = None,
) -> dict:
    """Replay input masks and capture one canonical state record per tick."""

    if kind not in VALID_KINDS:
        raise MakerLiteTraceError("trace kind must be synthetic or authentic")
    if not source_revision or len(source_revision) > 96:
        raise MakerLiteTraceError("source_revision must contain 1-96 characters")
    if not isinstance(inputs, list) or not 1 <= len(inputs) <= MAX_TRACE_TICKS:
        raise MakerLiteTraceError(
            f"inputs must contain 1-{MAX_TRACE_TICKS} ticks"
        )
    if source_sha256 is None:
        source_sha256 = _source_digest(project)
    source_sha256 = str(source_sha256).lower()
    if not _HEX_64.fullmatch(source_sha256):
        raise MakerLiteTraceError("source_sha256 must be 64 lowercase hex digits")

    session = runtime.open_session(project)
    frames = []
    for index, raw_input in enumerate(inputs):
        if isinstance(raw_input, bool):
            raise MakerLiteTraceError(f"input {index} must be an integer mask")
        input_mask = int(raw_input)
        if not 0 <= input_mask <= 0x1FF:
            raise MakerLiteTraceError(f"input {index} is outside the 9-bit mask")
        session.tick(input_mask)
        frame = _frame(session.snapshot(), session.digest())
        frame["input"] = input_mask
        frames.append(frame)

    trace = {
        "format_version": TRACE_FORMAT_VERSION,
        "kind": kind,
        "ruleset": str(project.get("ruleset", "")),
        "project_id": str(project.get("project_id", "")),
        "source_revision": source_revision,
        "source_sha256": source_sha256,
        "tick_hz": TRACE_TICK_HZ,
        "frames": frames,
    }
    validate_trace(trace)
    return trace


def _bounded_integer(value, label: str, minimum: int, maximum: int) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise MakerLiteTraceError(f"{label} must be an integer")
    if not minimum <= value <= maximum:
        raise MakerLiteTraceError(
            f"{label} must be between {minimum} and {maximum}"
        )
    return value


def validate_trace(trace: dict) -> None:
    if not isinstance(trace, dict):
        raise MakerLiteTraceError("trace root must be an object")
    allowed_root = {
        "format_version", "kind", "ruleset", "project_id",
        "source_revision", "source_sha256", "tick_hz", "frames",
    }
    unknown_root = set(trace) - allowed_root
    if unknown_root:
        raise MakerLiteTraceError(
            f"unknown trace field: {sorted(unknown_root)[0]}"
        )
    if trace.get("format_version") != TRACE_FORMAT_VERSION:
        raise MakerLiteTraceError("unsupported behavior trace version")
    if trace.get("kind") not in VALID_KINDS:
        raise MakerLiteTraceError("trace kind must be synthetic or authentic")
    if trace.get("ruleset") not in VALID_RULESETS:
        raise MakerLiteTraceError("unknown trace ruleset")
    project_id = trace.get("project_id")
    if not isinstance(project_id, str) or not 1 <= len(project_id) <= 32:
        raise MakerLiteTraceError("trace project_id must contain 1-32 characters")
    revision = trace.get("source_revision")
    if not isinstance(revision, str) or not 1 <= len(revision) <= 96:
        raise MakerLiteTraceError(
            "trace source_revision must contain 1-96 characters"
        )
    if not _HEX_64.fullmatch(str(trace.get("source_sha256", ""))):
        raise MakerLiteTraceError("trace source_sha256 is invalid")
    if trace.get("tick_hz") != TRACE_TICK_HZ:
        raise MakerLiteTraceError("Maker Lite traces must run at exactly 60 Hz")
    frames = trace.get("frames")
    if not isinstance(frames, list) or not 1 <= len(frames) <= MAX_TRACE_TICKS:
        raise MakerLiteTraceError(
            f"trace must contain 1-{MAX_TRACE_TICKS} frames"
        )

    for index, frame in enumerate(frames, 1):
        if not isinstance(frame, dict):
            raise MakerLiteTraceError(f"frame {index} must be an object")
        unknown_frame = set(frame) - {
            "tick", "input", "player", "camera", "state", "digest",
        }
        if unknown_frame:
            raise MakerLiteTraceError(
                f"unknown frame field: {sorted(unknown_frame)[0]}"
            )
        if frame.get("tick") != index:
            raise MakerLiteTraceError(f"frame {index} has a noncanonical tick")
        _bounded_integer(frame.get("input"), f"frame {index} input", 0, 0x1FF)
        player = frame.get("player")
        camera = frame.get("camera")
        state = frame.get("state")
        if not isinstance(player, list) or len(player) != 4:
            raise MakerLiteTraceError(f"frame {index} player must have 4 fields")
        if not isinstance(camera, list) or len(camera) != 2:
            raise MakerLiteTraceError(f"frame {index} camera must have 2 fields")
        if not isinstance(state, list) or len(state) != 8:
            raise MakerLiteTraceError(f"frame {index} state must have 8 fields")
        for field, value in enumerate(player):
            _bounded_integer(
                value, f"frame {index} player[{field}]", -(1 << 31), (1 << 31) - 1
            )
        for field, value in enumerate(camera):
            _bounded_integer(
                value, f"frame {index} camera[{field}]", -(1 << 31), (1 << 31) - 1
            )
        for field, value in enumerate(state):
            _bounded_integer(
                value, f"frame {index} state[{field}]", -32768, 32767
            )
        if not _HEX_8.fullmatch(str(frame.get("digest", ""))):
            raise MakerLiteTraceError(f"frame {index} digest is invalid")


def load_trace(path: str | Path) -> dict:
    with open(path, "r", encoding="utf-8") as source:
        trace = json.load(source)
    validate_trace(trace)
    return trace


def replay_trace(
    runtime: MakerLiteRuntime, project: dict, trace: dict
) -> TraceReplay:
    """Fail at the first tick whose complete canonical state diverges."""

    validate_trace(trace)
    if trace["ruleset"] != project.get("ruleset"):
        raise MakerLiteTraceError("trace and project rulesets differ")
    if trace["project_id"] != project.get("project_id"):
        raise MakerLiteTraceError("trace and project IDs differ")
    if trace["kind"] == "synthetic" and trace["source_sha256"] != _source_digest(project):
        raise MakerLiteTraceError("synthetic trace does not match this project source")

    session = runtime.open_session(project)
    final_digest = 0
    for expected in trace["frames"]:
        session.tick(expected["input"])
        snapshot = session.snapshot()
        actual = _frame(snapshot, session.digest())
        actual["input"] = expected["input"]
        for field in ("player", "camera", "state", "digest"):
            if actual[field] != expected[field]:
                raise MakerLiteTraceError(
                    f"trace diverged at tick {expected['tick']} in {field}: "
                    f"expected {expected[field]!r}, got {actual[field]!r}"
                )
        final_digest = session.digest()
    return TraceReplay(
        ruleset=trace["ruleset"],
        ticks=len(trace["frames"]),
        final_digest=final_digest,
        source_revision=trace["source_revision"],
    )
