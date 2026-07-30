#!/usr/bin/env python3
"""Print validated Maker Lite pack metadata."""

import argparse
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "rockpod"))

from services.maker_lite_pack import parse_pack  # noqa: E402


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("pack", type=Path)
    args = parser.parse_args()
    print(json.dumps(parse_pack(args.pack.read_bytes()).__dict__, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
