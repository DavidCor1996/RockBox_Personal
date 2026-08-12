#!/usr/bin/env python3
"""Run the RockPod Store companion used by iOS 6 and iOS 7 clients."""

import argparse
import os
import sys

from aiohttp import web

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

from app.config import Config
from services.store_companion import StoreCompanion


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", help="RockPod config JSON path")
    parser.add_argument("--bind", default="0.0.0.0", help="Address to listen on")
    parser.add_argument("--port", type=int, default=8732, help="TCP port")
    parser.add_argument("--pair-code", help="Optional fixed six-digit code")
    args = parser.parse_args()
    companion = StoreCompanion(Config(args.config), pair_code=args.pair_code)
    print("RockPod Store: http://%s:%d" % (args.bind, args.port))
    print("Pairing code: %s (valid for 10 minutes)" % companion.pair_code)
    web.run_app(companion.app(), host=args.bind, port=args.port, print=None)


if __name__ == "__main__":
    main()
