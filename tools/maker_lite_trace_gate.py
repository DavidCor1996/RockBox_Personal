#!/usr/bin/env python3
"""Replay asset-free Maker Lite behavior traces through the shared C core."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "rockpod"))

from services.maker_lite_runtime import MakerLiteRuntime  # noqa: E402
from services.maker_lite_trace import (  # noqa: E402
    MakerLiteTraceError,
    capture_trace,
    load_trace,
    replay_trace,
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "trace",
        type=Path,
        nargs="*",
        help="Trace files (default: committed synthetic traces)",
    )
    parser.add_argument(
        "--private-root",
        type=Path,
        default=Path("/tmp/maker-lite-trace-runtime"),
    )
    parser.add_argument(
        "--update-synthetic",
        action="store_true",
        help="recapture synthetic fixtures using their existing input masks",
    )
    args = parser.parse_args()
    traces = args.trace or sorted(
        (REPO / "testdata/maker_lite/traces").glob("*.json")
    )
    if not traces:
        raise SystemExit("no Maker Lite traces found")
    runtime = MakerLiteRuntime(str(REPO), str(args.private_root))
    for trace_path in traces:
        try:
            trace = load_trace(trace_path)
            project_path = (
                REPO / "testdata/maker_lite" /
                f"{trace['ruleset']}-test.json"
            )
            project = json.loads(project_path.read_text(encoding="utf-8"))
            if args.update_synthetic:
                if trace["kind"] != "synthetic":
                    raise MakerLiteTraceError(
                        "only synthetic traces may be updated"
                    )
                trace = capture_trace(
                    runtime,
                    project,
                    [frame["input"] for frame in trace["frames"]],
                    kind="synthetic",
                    source_revision=trace["source_revision"],
                )
                trace_path.write_text(
                    json.dumps(
                        trace,
                        sort_keys=True,
                        separators=(",", ":"),
                    )
                    + "\n",
                    encoding="utf-8",
                )
            result = replay_trace(runtime, project, trace)
        except (OSError, ValueError, MakerLiteTraceError) as error:
            raise SystemExit(f"{trace_path}: {error}") from error
        print(
            f"PASS {trace_path.name}: ruleset={result.ruleset} "
            f"ticks={result.ticks} digest={result.final_digest:08x} "
            f"source={result.source_revision}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
