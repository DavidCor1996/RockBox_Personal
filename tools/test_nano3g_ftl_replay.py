#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).with_name("nano3g_ftl_replay.py")
SPEC = importlib.util.spec_from_file_location("nano3g_ftl_replay", SCRIPT)
replay = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules["nano3g_ftl_replay"] = replay
SPEC.loader.exec_module(replay)


def le16_put(buf: bytearray, off: int, value: int) -> None:
    buf[off] = value & 0xFF
    buf[off + 1] = (value >> 8) & 0xFF


def le32_put(buf: bytearray, off: int, value: int) -> None:
    buf[off] = value & 0xFF
    buf[off + 1] = (value >> 8) & 0xFF
    buf[off + 2] = (value >> 16) & 0xFF
    buf[off + 3] = (value >> 24) & 0xFF


def add_page(lines: list[str], page_id: int, kind: str, meta: str, body: bytes) -> None:
    lines.append(f"N3GD_BEGIN id={page_id} kind={kind} {meta}")
    for off in range(0, len(body), 16):
        lines.append(f"N3GD_BODY id={page_id} off={off:03x} {body[off:off + 16].hex()}")
    for off in range(0, 0x40, 16):
        lines.append(f"N3GD_OOB id={page_id} off={off:03x} {'00' * 16}")
    lines.append(f"N3GD_END id={page_id}")


def synthetic_dump(valid_boot: bool = True) -> str:
    lines: list[str] = [
        "N3GD_START profile=winpod_mbr_fat32 host=0000A07E target=0000A07E ppb=512"
    ]

    mbr = bytearray(0x800)
    part = 0x1BE
    mbr[part + 4] = 0x0C
    le32_put(mbr, part + 8, 0xA07E)
    le32_put(mbr, part + 12, 0xE7F81)
    le16_put(mbr, 0x1FE, 0xAA55)
    add_page(
        lines,
        1,
        "mbr",
        "j=982 v=056B po=0 sl=0 l0=00000000 host=0000A07E tgt=00000000 bank=0 pb=1 pp=0 phy=128 rc=0 t=40 l=00000000",
        mbr,
    )

    boot = bytearray(0x800)
    sector = 2 * 0x200
    if valid_boot:
        boot[sector:sector + 3] = b"\xEB\x58\x90"
        boot[sector + 3:sector + 11] = b"MSDOS5.0"
        le16_put(boot, sector + 0x0B, 512)
        boot[sector + 0x0D] = 8
        le16_put(boot, sector + 0x0E, 32)
        boot[sector + 0x10] = 2
        le32_put(boot, sector + 0x20, 0xE7F81)
        le32_put(boot, sector + 0x24, 123)
        le32_put(boot, sector + 0x2C, 2)
        boot[sector + 0x52:sector + 0x5A] = b"FAT32   "
        le16_put(boot, sector + 0x1FE, 0xAA55)
    else:
        le16_put(boot, sector + 0x1FE, 0xAA55)
        le16_put(boot, sector + 0x0B, 35)
        boot[sector + 0x52:sector + 0x5A] = b"FAT?bad "

    add_page(
        lines,
        2,
        "cand",
        "j=100 v=0123 po=0 sl=4294967295 l0=0000A07C host=0000A07E tgt=0000A07E bank=0 pb=2 pp=0 phy=256 rc=0 t=40 l=0000A07C",
        boot,
    )
    adjacent = bytearray(0x800)
    adjacent[0x000:0x008] = b"ADJACENT"
    add_page(
        lines,
        3,
        "cand",
        "j=100 v=0123 po=1 sl=4294967295 l0=0000A07C host=0000A080 tgt=0000A080 bank=0 pb=2 pp=1 phy=257 rc=0 t=40 l=0000A080",
        adjacent,
    )
    lines.append("N3GD_DONE pages=3")
    return "\n".join(lines) + "\n"


def synthetic_dump_covering_invalid_near_valid() -> str:
    lines = synthetic_dump(valid_boot=False).splitlines()[:-1]

    near = bytearray(0x800)
    near[0:3] = b"\xEB\x58\x90"
    near[3:11] = b"MSDOS5.0"
    le16_put(near, 0x0B, 512)
    near[0x0D] = 8
    le16_put(near, 0x0E, 32)
    near[0x10] = 2
    le32_put(near, 0x20, 0xE7F81)
    le32_put(near, 0x24, 123)
    le32_put(near, 0x2C, 2)
    near[0x52:0x5A] = b"FAT32   "
    le16_put(near, 0x1FE, 0xAA55)
    add_page(
        lines,
        4,
        "cand",
        "j=101 v=0124 po=0 sl=4294967295 l0=0000B000 host=0000A07E tgt=0000A07E bank=0 pb=3 pp=0 phy=384 rc=0 t=40 l=0000B000",
        near,
    )
    lines.append("N3GD_DONE pages=4")
    return "\n".join(lines) + "\n"


class Nano3gReplayTest(unittest.TestCase):
    def test_mbr_and_boot_sector_resolve(self) -> None:
        pages = replay.parse_dump(synthetic_dump().splitlines())
        mbr_page, mbr_slice, parts = replay.select_mbr(pages)
        self.assertIsNotNone(mbr_page)
        self.assertEqual(mbr_page.meta["j"], 982)
        self.assertEqual(mbr_page.meta["v"], 0x056B)
        self.assertEqual(mbr_page.meta["po"], 0)
        self.assertEqual(mbr_slice, 0)
        self.assertEqual(parts[0], (0x0C, 0xA07E, 0xE7F81))

        candidates = replay.candidate_pages_for_lba(pages, 0xA07E, 512)
        self.assertEqual(len(candidates), 1)
        checks = [
            replay.bpb_reason(sec, 0xE7F81)
            for _, _, _, sec in replay.iter_slices_and_shifts(candidates[0])
        ]
        self.assertTrue(any(check.valid for check in checks))

    def test_winpod_profile_uses_mbr_fat32_lba_rules(self) -> None:
        self.assertEqual(replay.WINPOD_MBR_FAT32.partition_scheme, "mbr")
        self.assertIn(0x0C, replay.WINPOD_MBR_FAT32.fat_types)
        self.assertIn(0x0B, replay.WINPOD_MBR_FAT32.fat_types)
        self.assertEqual(replay.WINPOD_MBR_FAT32.host_to_raw_shift, 0)

    def test_find_entry_covering_lba_uses_sector_delta(self) -> None:
        pages = replay.parse_dump(synthetic_dump().splitlines())

        base = replay.find_entry_covering_lba(pages, 0xA07C, 512)
        self.assertIsNotNone(base)
        assert base is not None
        self.assertEqual(base.page.meta["j"], 100)
        self.assertEqual(base.sector_delta, 0)
        self.assertEqual(base.page_offset, 0)
        self.assertEqual(base.slice_index, 0)

        boot = replay.find_entry_covering_lba(pages, 0xA07E, 512)
        self.assertIsNotNone(boot)
        assert boot is not None
        self.assertEqual(boot.page.meta["j"], 100)
        self.assertEqual(boot.sector_delta, 2)
        self.assertEqual(boot.page_offset, 0)
        self.assertEqual(boot.slice_index, 2)

        adjacent = replay.find_entry_covering_lba(pages, 0xA080, 512)
        self.assertIsNotNone(adjacent)
        assert adjacent is not None
        self.assertEqual(adjacent.page.meta["po"], 1)
        self.assertEqual(adjacent.sector_delta, 4)
        self.assertEqual(adjacent.page_offset, 1)
        self.assertEqual(adjacent.slice_index, 0)

    def test_read_lba_512_uses_covering_entry_slice(self) -> None:
        pages = replay.parse_dump(synthetic_dump().splitlines())
        mbr = replay.read_lba_512(pages, 0, 512)
        self.assertIsNotNone(mbr)
        assert mbr is not None
        self.assertEqual(replay.le16(mbr, 0x1FE), 0xAA55)
        self.assertEqual(mbr[0x1BE + 4], 0x0C)
        self.assertEqual(replay.le32(mbr, 0x1BE + 8), 0xA07E)

        boot = replay.read_lba_512(pages, 0xA07E, 512)
        self.assertIsNotNone(boot)
        assert boot is not None
        bpb = replay.bpb_reason(boot, 0xE7F81)
        self.assertTrue(bpb.valid)
        self.assertEqual(bpb.bps, 512)

        adjacent = replay.read_lba_512(pages, 0xA080, 512)
        self.assertIsNotNone(adjacent)
        assert adjacent is not None
        self.assertEqual(adjacent[:8], b"ADJACENT")

    def test_invalid_bps_is_rejected(self) -> None:
        pages = replay.parse_dump(synthetic_dump(valid_boot=False).splitlines())
        candidate = replay.candidate_pages_for_lba(pages, 0xA07E, 512)[0]
        checks = [
            replay.bpb_reason(sec, 0xE7F81)
            for _, _, _, sec in replay.iter_slices_and_shifts(candidate)
        ]
        self.assertFalse(any(check.valid for check in checks))
        self.assertTrue(any(check.sig == 0xAA55 and check.bps == 35 for check in checks))

    def test_cli_replay_accepts_synthetic_dump(self) -> None:
        with tempfile.NamedTemporaryFile("w", encoding="utf-8", delete=False) as handle:
            handle.write(synthetic_dump())
            path = handle.name
        self.assertEqual(replay.replay(path, 0xA07E, 512), 0)

    def test_replay_continues_to_near_pages_after_covering_rejects(self) -> None:
        with tempfile.NamedTemporaryFile("w", encoding="utf-8", delete=False) as handle:
            handle.write(synthetic_dump_covering_invalid_near_valid())
            path = handle.name
        self.assertEqual(replay.replay(path, 0xA07E, 512), 0)


if __name__ == "__main__":
    unittest.main()
