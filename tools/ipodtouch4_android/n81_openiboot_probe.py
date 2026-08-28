#!/usr/bin/env python3
"""Token-gated USB-only liveness probe for the volatile N81 OpeniBoot loader."""

from __future__ import annotations

import argparse
import json

from n81_ram_boot_transport import OpenIBootUSB, RamBootError
from transport_usb import ensure_usb_process_environment


PROBE_TOKEN = "N81_OPENIBOOT_VOLATILE_PROBE"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--execute-token", required=True)
    args = parser.parse_args()
    try:
        if args.execute_token != PROBE_TOKEN:
            raise RamBootError(f"physical probe requires exact token {PROBE_TOKEN}")
        ensure_usb_process_environment()
        channel = OpenIBootUSB.acquire()
        try:
            report = channel.probe_version()
            report["handoff_state"] = channel.probe_n81_state()
        finally:
            channel.close()
    except RamBootError as error:
        parser.error(str(error))
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
