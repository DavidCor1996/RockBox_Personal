#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
from pathlib import Path
import struct
import unittest


MODULE_PATH = Path(__file__).with_name("nano3g_mkdfu.py")
SPEC = importlib.util.spec_from_file_location("nano3g_mkdfu", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
mkdfu = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(mkdfu)


class Nano3gMkdfuTests(unittest.TestCase):
    def test_round_trip(self) -> None:
        body = bytes(range(256)) * 4
        wrapper = mkdfu.build_wrapper(body)
        report = mkdfu.inspect_wrapper(wrapper, body)

        self.assertEqual(wrapper[:8], b"87021.0\x02")
        self.assertEqual(wrapper[mkdfu.HEADER_SIZE:], body)
        self.assertEqual(report["body_bytes"], len(body))
        self.assertFalse(report["persistent"])

    def test_rejects_empty_body(self) -> None:
        with self.assertRaisesRegex(mkdfu.DfuError, "empty"):
            mkdfu.build_wrapper(b"")

    def test_rejects_oversized_body(self) -> None:
        body = b"x" * (mkdfu.MAX_BODY_SIZE + 1)
        with self.assertRaisesRegex(mkdfu.DfuError, "IRAM limit"):
            mkdfu.build_wrapper(body)

    def test_rejects_persistent_installer_format(self) -> None:
        wrapper = bytearray(mkdfu.build_wrapper(b"test"))
        wrapper[7] = 3
        with self.assertRaisesRegex(mkdfu.DfuError, "volatile type 2"):
            mkdfu.inspect_wrapper(bytes(wrapper))

    def test_rejects_inconsistent_lengths(self) -> None:
        wrapper = bytearray(mkdfu.build_wrapper(b"test"))
        struct.pack_into("<I", wrapper, mkdfu.LENGTH_OFFSETS[1], 3)
        with self.assertRaisesRegex(mkdfu.DfuError, "inconsistent"):
            mkdfu.inspect_wrapper(bytes(wrapper))

    def test_rejects_reserved_header_data(self) -> None:
        wrapper = bytearray(mkdfu.build_wrapper(b"test"))
        wrapper[0x20] = 1
        with self.assertRaisesRegex(mkdfu.DfuError, "reserved"):
            mkdfu.inspect_wrapper(bytes(wrapper))

        wrapper = bytearray(mkdfu.build_wrapper(b"test"))
        wrapper[0x08] = 1
        with self.assertRaisesRegex(mkdfu.DfuError, "reserved"):
            mkdfu.inspect_wrapper(bytes(wrapper))

    def test_requires_expected_body_match(self) -> None:
        wrapper = mkdfu.build_wrapper(b"test")
        with self.assertRaisesRegex(mkdfu.DfuError, "does not match"):
            mkdfu.inspect_wrapper(wrapper, b"nope")


if __name__ == "__main__":
    unittest.main()
