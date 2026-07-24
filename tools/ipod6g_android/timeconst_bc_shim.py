#!/usr/bin/env python3
"""Narrow `bc` command shim for Linux's timeconst.bc build invocation only."""

from __future__ import annotations

import sys
from pathlib import Path


def main(argv=None, stdin=None, stdout=None):
    arguments = list(sys.argv[1:] if argv is None else argv)
    input_stream = sys.stdin if stdin is None else stdin
    output_stream = sys.stdout if stdout is None else stdout

    sources = [argument for argument in arguments if not argument.startswith("-")]
    if len(sources) != 1 or Path(sources[0]).name != "timeconst.bc":
        print(
            "timeconst_bc_shim.py only supports: bc -q kernel/time/timeconst.bc",
            file=sys.stderr,
        )
        return 2

    try:
        hz = int(input_stream.read().strip())
    except ValueError:
        print("timeconst_bc_shim.py expected an integer HZ on stdin", file=sys.stderr)
        return 2

    helper_dir = Path(__file__).resolve().parent
    sys.path.insert(0, str(helper_dir))
    from generate_timeconst import generate

    output_stream.write(generate(hz))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

