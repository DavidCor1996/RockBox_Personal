#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
from tempfile import TemporaryDirectory
import unittest


SCRIPT = Path(__file__).with_name("nano3g_wmount_replay.py")
SPEC = importlib.util.spec_from_file_location("nano3g_wmount_replay", SCRIPT)
wm = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules["nano3g_wmount_replay"] = wm
SPEC.loader.exec_module(wm)

FTL_SCRIPT = Path(__file__).with_name("test_nano3g_ftl_replay.py")
FTL_SPEC = importlib.util.spec_from_file_location("test_nano3g_ftl_replay", FTL_SCRIPT)
ftl_test = importlib.util.module_from_spec(FTL_SPEC)
assert FTL_SPEC.loader is not None
sys.modules["test_nano3g_ftl_replay"] = ftl_test
FTL_SPEC.loader.exec_module(ftl_test)


class Nano3gWmountReplayTest(unittest.TestCase):
    def setUp(self) -> None:
        self.pages = wm.ftl.parse_dump(ftl_test.synthetic_dump().splitlines())
        self.entries = wm.merge_entries([], wm.entries_from_pages(self.pages, 512 * 4))
        self.mbr_page, _, self.parts = wm.select_mbr(self.pages)
        self.mbr_identity = wm.mbr_entry_identity(self.mbr_page)

    def test_lba0_reads_mbr(self) -> None:
        result = wm.lookup_lba(self.pages, self.entries, 0, self.mbr_identity)
        sec = wm.read_result_sector(result)
        self.assertIsNotNone(sec)
        assert sec is not None
        self.assertEqual(wm.ftl.le16(sec, 0x1FE), 0xAA55)
        self.assertEqual(sec[0x1BE + 4], 0x0C)
        self.assertEqual(wm.ftl.le32(sec, 0x1BE + 8), 0xA07E)
        self.assertEqual(wm.ftl.le32(sec, 0x1BE + 12), 0xE7F81)

    def test_nonzero_lba_never_reuses_mbr_entry(self) -> None:
        entries = self.entries + [wm.WmountEntry(982, 0x056B, 0xA000, 2048, 0x40, 0)]
        for lba in (1, 0x3F, 0xA07C, 0xA07E, 0xA080):
            result = wm.lookup_lba(self.pages, entries, lba, self.mbr_identity)
            if result.entry is not None:
                self.assertNotEqual((result.entry.j, result.entry.v), self.mbr_identity)

    def test_partition_start_lookup_uses_range_delta(self) -> None:
        result = wm.lookup_lba(self.pages, self.entries, 0xA07E, self.mbr_identity)
        self.assertIsNotNone(result.entry)
        assert result.entry is not None
        self.assertEqual(result.entry.l0, 0xA07C)
        self.assertEqual(result.delta, 2)
        self.assertEqual(result.po_final, 0)
        self.assertEqual(result.sector_slice, 2)
        self.assertEqual(result.reason, "ok")

    def test_lookup_adds_entry_page_base_to_delta_page(self) -> None:
        entry = wm.WmountEntry(332, 0x025A, 0x1B372, 2048, 0x40, 440)
        result = wm.lookup_lba([], [entry], 0x1B6DC, None)
        self.assertIsNotNone(result.entry)
        self.assertEqual(result.delta, 874)
        self.assertEqual(result.po_final, 658)
        self.assertEqual(result.sector_slice, 2)
        self.assertEqual(result.reason, "nopage")

    def test_partition_boot_sector_reads_sig_aa55_bps_512(self) -> None:
        result = wm.lookup_lba(self.pages, self.entries, 0xA07E, self.mbr_identity)
        sec = wm.read_result_sector(result)
        self.assertIsNotNone(sec)
        assert sec is not None
        bpb = wm.ftl.bpb_reason(sec, 0xE7F81)
        self.assertTrue(bpb.valid)
        self.assertEqual(bpb.sig, 0xAA55)
        self.assertEqual(bpb.bps, 512)

    def test_manifest_values_parse_as_firmware_hex(self) -> None:
        lines = [
            "N3GM_ENTRY mapb=6166 mapp=0 j=61 v=0194 "
            "l0=0000A07C span=2048 t=40 po=0 tgt=0000A07E d=2"
        ]
        entries = wm.parse_manifest_entries(lines, 2048)
        self.assertEqual(len(entries), 1)
        self.assertEqual(entries[0].j, 61)
        self.assertEqual(entries[0].v, 0x0194)
        self.assertEqual(entries[0].l0, 0xA07C)
        self.assertEqual(entries[0].span, 2048)
        self.assertEqual(entries[0].entry_type, 0x40)
        self.assertEqual(entries[0].po0, 0)

    def test_binary_dump_directory_loads_named_files(self) -> None:
        with TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            body = bytearray(0x800)
            body[0x1FE:0x200] = b"\x55\xaa"
            body[0x1BE + 4] = 0x0C
            body[0x1BE + 8:0x1BE + 12] = (0xA07E).to_bytes(4, "little")
            body[0x1BE + 12:0x1BE + 16] = (0xE7F81).to_bytes(4, "little")
            (root / "page_j982_v056B.bin").write_bytes(body)
            (root / "page_j982_v056B.oob").write_bytes(b"\x00" * 64)
            (root / "n3g_wmount_manifest.txt").write_text(
                "\n".join(
                    [
                        "N3GD_FILE target=page_j982_v056B kind=page "
                        "j=982 v=056B po=0 sl=0 l0=00000000 "
                        "bank=0 pb=0 pp=0 phy=0 rc=0 t=40 l=00000000 "
                        "bin=page_j982_v056B.bin oob=page_j982_v056B.oob",
                        "N3GM_ENTRY mapb=0 mapp=0 j=982 v=056B "
                        "l0=00000000 span=2048 t=40 po=0 tgt=0000A07E d=0",
                    ]
                )
                + "\n",
                encoding="utf-8",
            )
            pages, entries = wm.load_capture(str(root), 512)
            self.assertEqual(len(pages), 1)
            self.assertEqual(pages[0].meta["target"], "page_j982_v056B")
            self.assertEqual(len(entries), 1)
            self.assertEqual(entries[0].v, 0x056B)

    def test_reference_sector_finds_exact_dumped_slice(self) -> None:
        with TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            sector = bytearray(0x200)
            sector[0:3] = b"\xebX\x90"
            sector[0x0B:0x0D] = (512).to_bytes(2, "little")
            sector[0x0D] = 8
            sector[0x0E:0x10] = (32).to_bytes(2, "little")
            sector[0x10] = 2
            sector[0x20:0x24] = (0xE7F81).to_bytes(4, "little")
            sector[0x24:0x28] = (123).to_bytes(4, "little")
            sector[0x2C:0x30] = (2).to_bytes(4, "little")
            sector[0x52:0x5A] = b"FAT32   "
            sector[0x1FE:0x200] = b"\x55\xaa"

            body = bytearray(0x800)
            body[0x200:0x400] = sector
            (root / "page_j10_v1234.bin").write_bytes(body)
            (root / "page_j10_v1234.oob").write_bytes(b"\x00" * 64)
            (root / "bootref.bin").write_bytes(sector)
            (root / "n3g_wmount_manifest.txt").write_text(
                "N3GD_FILE target=page_j10_v1234 kind=page "
                "j=10 v=1234 po=1 sl=1 l0=0000A07E "
                "bank=0 pb=1 pp=1 phy=1 rc=0 t=40 l=0000A07E "
                "bin=page_j10_v1234.bin oob=page_j10_v1234.oob\n",
                encoding="utf-8",
            )

            rc = wm.scan_reference_sector(str(root), root / "bootref.bin", 512, 4)
            self.assertEqual(rc, 0)

    def test_current_winpod_transform_uses_oob_base_plus_double_lba(self) -> None:
        with TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)

            lba0 = bytearray(0x200)
            lba0[0x1BE + 4] = 0x0B
            lba0[0x1BE + 8:0x1BE + 12] = (0x3F).to_bytes(4, "little")
            lba0[0x1BE + 12:0x1BE + 16] = (0xE7F81).to_bytes(4, "little")
            lba0[0x1FE:0x200] = b"\x55\xaa"

            boot = bytearray(0x200)
            boot[0:3] = b"\xeb<\x90"
            boot[0x0B:0x0D] = (4096).to_bytes(2, "little")
            boot[0x0D] = 1
            boot[0x0E:0x10] = (32).to_bytes(2, "little")
            boot[0x10] = 2
            boot[0x20:0x24] = (0xE7F81).to_bytes(4, "little")
            boot[0x24:0x28] = (927).to_bytes(4, "little")
            boot[0x2C:0x30] = (2).to_bytes(4, "little")
            boot[0x52:0x5A] = b"FAT32   "
            boot[0x1FE:0x200] = b"\x55\xaa"

            (root / "page_b1524_p0.bin").write_bytes(lba0 + b"\0" * 0x600)
            (root / "page_b1524_p0.oob").write_bytes(
                bytes.fromhex("7e40010094de0000ff40") + b"\0" * 54
            )
            (root / "scan_v044B_po128_l000140FC.bin").write_bytes(boot + b"\0" * 0x600)
            (root / "scan_v044B_po128_l000140FC.oob").write_bytes(
                bytes.fromhex("fc40010094de0000ff40") + b"\0" * 54
            )
            (root / "lba0.bin").write_bytes(lba0)
            (root / "boot.bin").write_bytes(boot)
            (root / "n3g_wmount_manifest.txt").write_text("", encoding="utf-8")

            rc = wm.replay_current_winpod(root, root / "lba0.bin", root / "boot.bin")
            self.assertEqual(rc, 0)

    def test_synthetic_fsinfo_matches_rockbox_fat_mount_signature(self) -> None:
        sector = wm.synthetic_fsinfo_sector()
        self.assertTrue(wm.fsinfo_signature_valid(sector))
        self.assertEqual(wm.ftl.le32(sector, 0), 0x41615252)
        self.assertEqual(wm.ftl.le32(sector, 0x1E4), 0x61417272)
        self.assertEqual(wm.ftl.le32(sector, 0x1E8), 0)
        self.assertEqual(wm.ftl.le32(sector, 0x1EC), 2)
        self.assertEqual(wm.ftl.le32(sector, 0x1FC), 0xAA550000)

    def test_current_winpod_prefers_active_covering_page_over_stale_exact_page(self) -> None:
        with TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)

            lba0 = bytearray(0x800)
            lba0[0x1BE + 4] = 0x0B
            lba0[0x1BE + 8:0x1BE + 12] = (0x3F).to_bytes(4, "little")
            lba0[0x1BE + 12:0x1BE + 16] = (0xE7F81).to_bytes(4, "little")
            lba0[0x1FE:0x200] = b"\x55\xaa"
            (root / "page_b1524_p0.bin").write_bytes(lba0)
            (root / "page_b1524_p0.oob").write_bytes(
                bytes.fromhex("7e40010094de0000ff40") + b"\0" * 54
            )

            active = bytearray(0x800)
            active[0x200:0x204] = b"GOOD"
            (root / "scan_v044B_po64_l000140FE.bin").write_bytes(active)
            (root / "scan_v044B_po64_l000140FE.oob").write_bytes(
                bytes.fromhex("fe40010094de0000ff40") + b"\0" * 54
            )

            stale = bytearray(0x800)
            stale[0:4] = b"BAD!"
            (root / "scan_v044C_po64_l00014100.bin").write_bytes(stale)
            (root / "scan_v044C_po64_l00014100.oob").write_bytes(
                bytes.fromhex("0041010094de0000ff40") + b"\0" * 54
            )
            (root / "n3g_wmount_manifest.txt").write_text("", encoding="utf-8")

            pages, _ = wm.load_capture(str(root), 512)
            sector, page, raw_l0, slice_index = wm.read_current_winpod_sector(
                pages, 0x1407E, 0x41, 0x044B
            )
            self.assertIsNotNone(sector)
            self.assertIsNotNone(page)
            self.assertEqual(raw_l0, 0x14100)
            self.assertEqual(slice_index, 1)
            self.assertEqual(page.meta["target"], "scan_v044B_po64_l000140FE")
            self.assertEqual(sector[:4], b"GOOD")


if __name__ == "__main__":
    unittest.main()
