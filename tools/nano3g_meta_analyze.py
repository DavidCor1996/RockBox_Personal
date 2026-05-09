#!/usr/bin/env python3
"""Analyze captured Nano 3G Whimory metadata pages.

This is an offline helper for the DFU captures produced by
tools/nano3g_dfu_bpb_scan.py. It intentionally does not try to mount the FTL;
it summarizes the metadata bodies so the next live diagnostic can be narrow.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import re
import struct


META_RE = re.compile(
    r"meta-b(?P<block>\d+)-p(?P<page>\d+)-t(?P<type>[0-9a-fA-F]{2})-"
    r"i(?P<idx>[0-9a-fA-F]{4})-u(?P<usn>[0-9a-fA-F]{8})\.body\.bin$"
)


@dataclass(frozen=True)
class MetaCapture:
    block: int
    page: int
    oob_type: int
    idx: int
    usn: int
    body_path: Path

    @property
    def ref(self) -> int:
        return self.block * 512 + self.page


def le16(data: bytes, off: int) -> int:
    return struct.unpack_from("<H", data, off)[0]


def le32(data: bytes, off: int) -> int:
    return struct.unpack_from("<I", data, off)[0]


def load_captures(paths: list[Path]) -> list[MetaCapture]:
    captures: list[MetaCapture] = []
    for root in paths:
        files = [root] if root.is_file() else sorted(root.glob("*.body.bin"))
        for path in files:
            match = META_RE.match(path.name)
            if match is None:
                continue
            oob_type = int(match.group("type"), 16)
            idx = int(match.group("idx"), 16)
            usn = int(match.group("usn"), 16)
            oob_path = path.with_suffix("").with_suffix(".oob.bin")
            if oob_path.exists():
                oob = oob_path.read_bytes()
                if len(oob) >= 10:
                    usn = le32(oob, 0)
                    idx = le16(oob, 4)
                    oob_type = oob[9]
            captures.append(
                MetaCapture(
                    block=int(match.group("block")),
                    page=int(match.group("page")),
                    oob_type=oob_type,
                    idx=idx,
                    usn=usn,
                    body_path=path,
                )
            )
    return sorted(captures, key=lambda item: (item.block, item.page))


def half_stats(values: list[int]) -> tuple[int, int, int, int, int, bool]:
    valid = [(i, value) for i, value in enumerate(values) if value != 0xFFFF]
    if not valid:
        return 0, -1, -1, 0xFFFF, 0, False
    first = valid[0][0]
    last = valid[-1][0]
    min_value = min(value for _, value in valid)
    max_value = max(value for _, value in valid)
    sequential = all(value == n for n, (_, value) in enumerate(valid))
    return len(valid), first, last, min_value, max_value, sequential


def summarize_t45(capture: MetaCapture, body: bytes) -> None:
    words = [le16(body, off) for off in range(0, min(len(body), 0x800), 2)]
    for half in range(2):
        table = words[half * 512 : (half + 1) * 512]
        count, first, last, min_value, max_value, sequential = half_stats(table)
        if count == 0:
            continue
        # Observed OOB indexes are even and one 0x800 page stores two 0x400-byte
        # offset tables. Treat idx + half as the candidate log-table number.
        table_index = capture.idx + half
        print(
            "N3GM_T45_TABLE "
            f"b={capture.block} p={capture.page} idx={capture.idx:04X} "
            f"table={table_index:04X} valid={count} lp_first={first} lp_last={last} "
            f"po_min={min_value} po_max={max_value} seq={int(sequential)}"
        )


def summarize_t43(capture: MetaCapture, body: bytes, by_ref: dict[int, MetaCapture]) -> None:
    seen = 0
    for off in range(0, min(len(body), 0x800) - 3, 4):
        value = le32(body, off)
        target = by_ref.get(value)
        if target is None:
            continue
        seen += 1
        print(
            "N3GM_CTX_REF "
            f"off={off:03X} ref={value:08X} b={target.block} p={target.page} "
            f"t={target.oob_type:02X} idx={target.idx:04X} u={target.usn:08X}"
        )
    print(f"N3GM_CTX_REF_DONE b={capture.block} p={capture.page} refs={seen}")


def summarize_t49(capture: MetaCapture, body: bytes, target_host_lba: int, target_raw_lba: int) -> None:
    words = [le16(body, off) for off in range(0, min(len(body), 0x800), 2)]
    nonzero = [(index, value) for index, value in enumerate(words) if value not in (0, 0xFFFF)]
    exact_target = [index for index, value in nonzero if value == (target_raw_lba & 0xFFFF)]
    target_near = [
        (index, value)
        for index, value in nonzero
        if abs(value - (target_raw_lba & 0xFFFF)) <= 0x80
    ][:12]
    small = sum(1 for _, value in nonzero if value < 512)
    blocklike = sum(1 for _, value in nonzero if value < 8192)
    print(
        "N3GM_T49_SUM "
        f"b={capture.block} p={capture.page} idx={capture.idx:04X} "
        f"nonzero={len(nonzero)} small={small} blocklike={blocklike} "
        f"target_hits={len(exact_target)}"
    )
    if target_near:
        near = ",".join(f"{index}:{value:04X}" for index, value in target_near)
        print(f"N3GM_T49_NEAR target={target_raw_lba & 0xFFFF:04X} vals={near}")
    hints = {
        "host_lo": target_host_lba & 0xFFFF,
        "raw_lo": target_raw_lba & 0xFFFF,
        "host_512": (target_host_lba >> 9) & 0xFFFF,
        "raw_512": (target_raw_lba >> 9) & 0xFFFF,
        "host_256": (target_host_lba >> 8) & 0xFFFF,
        "raw_256": (target_raw_lba >> 8) & 0xFFFF,
    }
    for name, value in hints.items():
        hits = [index for index, word in nonzero if word == value]
        if hits:
            print(
                "N3GM_T49_HINT "
                f"name={name} value={value:04X} hits={','.join(str(hit) for hit in hits[:12])} "
                f"count={len(hits)}"
            )
    for stride in (16, 20, 24, 28, 32):
        plausible = 0
        first: list[str] = []
        for off in range(0, min(len(body), 0x800) - stride + 1, stride):
            vals = [le16(body, off + i) for i in range(0, stride, 2)]
            smalls = [value for value in vals if value not in (0, 0xFFFF) and value < 8192]
            if len(smalls) < 3:
                continue
            plausible += 1
            if len(first) < 4:
                first.append(f"{off:03X}:{'/'.join(f'{value:04X}' for value in vals[:6])}")
        print(
            "N3GM_T49_STRIDE "
            f"stride={stride} plausible={plausible} first={','.join(first) if first else '-'}"
        )


def analyze(paths: list[Path], target_host_lba: int, target_raw_lba: int) -> int:
    captures = load_captures(paths)
    by_ref = {capture.ref: capture for capture in captures}
    print(f"N3GM_START pages={len(captures)} host={target_host_lba:08X} raw={target_raw_lba:08X}")
    for capture in captures:
        body = capture.body_path.read_bytes()
        hwords = [le16(body, off) for off in range(0, min(len(body), 0x800), 2)]
        nonzero = sum(1 for value in hwords if value not in (0, 0xFFFF))
        ff = sum(1 for value in hwords if value == 0xFFFF)
        zero = sum(1 for value in hwords if value == 0)
        print(
            "N3GM_PAGE "
            f"b={capture.block} p={capture.page} t={capture.oob_type:02X} "
            f"idx={capture.idx:04X} u={capture.usn:08X} nz={nonzero} ff={ff} z={zero}"
        )
        if capture.oob_type == 0x45:
            summarize_t45(capture, body)
        elif capture.oob_type == 0x43:
            summarize_t43(capture, body, by_ref)
        elif capture.oob_type == 0x49:
            summarize_t49(capture, body, target_host_lba, target_raw_lba)
    print("N3GM_DONE")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="+", type=Path)
    parser.add_argument(
        "--target-lba",
        dest="target_host_lba",
        type=lambda value: int(value, 0),
        default=0xA07E,
        help="Host 512-byte disk LBA.",
    )
    parser.add_argument("--target-host-lba", dest="target_host_lba", type=lambda value: int(value, 0))
    parser.add_argument("--target-raw-lba", type=lambda value: int(value, 0))
    parser.add_argument("--host-to-raw-shift", type=int, default=1)
    args = parser.parse_args(argv)
    target_raw_lba = args.target_raw_lba
    if target_raw_lba is None:
        target_raw_lba = args.target_host_lba << args.host_to_raw_shift
    return analyze(args.paths, args.target_host_lba, target_raw_lba)


if __name__ == "__main__":
    raise SystemExit(main())
