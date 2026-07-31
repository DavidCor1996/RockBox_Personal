#!/usr/bin/env python3
"""Host tests for tools/animalcrossing_prepare_assets.py."""

from __future__ import annotations

from pathlib import Path
import struct
import tempfile
import unittest
import zlib

from tools import animalcrossing_prepare_assets as assets


def _yaz0_literals(data: bytes) -> bytes:
    result = bytearray(b"Yaz0" + len(data).to_bytes(4, "big") + bytes(8))
    for offset in range(0, len(data), 8):
        chunk = data[offset : offset + 8]
        result.append((0xFF << (8 - len(chunk))) & 0xFF)
        result.extend(chunk)
    return bytes(result)


def _disc_image(game_id: bytes = b"GAFE01", revision: int = 0) -> bytes:
    image = bytearray(0x8000)
    image[:6] = game_id
    image[7] = revision
    image[0x1C:0x20] = assets.GAMECUBE_MAGIC
    title = b"AnimalCrossing"
    image[0x20 : 0x20 + len(title)] = title

    dol_offset = 0x1000
    struct.pack_into(">I", image, 0x420, dol_offset)
    struct.pack_into(">I", image, dol_offset, 0x100)
    struct.pack_into(">I", image, dol_offset + 0x90, 4)
    image[dol_offset + 0x100 : dol_offset + 0x104] = b"DOL!"

    rel = _yaz0_literals(b"portable-rel-data")
    rel_offset = 0x3000
    image[rel_offset : rel_offset + len(rel)] = rel
    file_offset = 0x4000
    image[file_offset : file_offset + 4] = b"FILE"

    fst_offset = 0x2000
    names = b"foresta.rel.szs\0test.bin\0"
    fst_size = 3 * 12 + len(names)
    struct.pack_into(">II", image, 0x424, fst_offset, fst_size)
    struct.pack_into(">III", image, fst_offset, 0x01000000, 0, 3)
    struct.pack_into(
        ">III", image, fst_offset + 12, 0, rel_offset, len(rel)
    )
    struct.pack_into(
        ">III",
        image,
        fst_offset + 24,
        len(b"foresta.rel.szs\0"),
        file_offset,
        4,
    )
    image[fst_offset + 36 : fst_offset + 36 + len(names)] = names
    return bytes(image)


def _ciso(logical: bytes) -> bytes:
    header = bytearray(assets.CISO_HEADER_SIZE)
    header[:4] = b"CISO"
    struct.pack_into("<I", header, 4, len(logical))
    header[assets.CISO_MAP_OFFSET] = 1
    return bytes(header) + logical


def _pack_entries(path: Path) -> dict[str, tuple[bytes, int, int]]:
    data = path.read_bytes()
    (
        magic,
        version,
        count,
        entry_size,
        _flags,
        index_offset,
        _data_offset,
        identity,
        _reserved,
    ) = assets.PACK_HEADER.unpack_from(data)
    if magic != assets.PACK_MAGIC or version != assets.PACK_VERSION:
        raise AssertionError("bad pack header")
    if entry_size != assets.PACK_ENTRY.size:
        raise AssertionError("bad pack entry size")
    if identity[:7] != b"GAFE01\0":
        raise AssertionError("bad pack identity")

    result = {}
    for index in range(count):
        unpacked = assets.PACK_ENTRY.unpack_from(
            data, index_offset + index * entry_size
        )
        name = unpacked[0].split(b"\0", 1)[0].decode()
        offset, size, checksum, flags, _source = unpacked[1:]
        payload = data[offset : offset + size]
        if zlib.crc32(payload) & 0xFFFFFFFF != checksum:
            raise AssertionError("bad entry checksum")
        result[name] = (payload, flags, checksum)
    return result


class AnimalCrossingAssetTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory(prefix="ac-assets-test-")
        self.root = Path(self.temp.name)
        self.disc = self.root / "game.ciso"
        self.disc.write_bytes(_ciso(_disc_image()))

    def tearDown(self) -> None:
        self.temp.cleanup()

    def test_inspects_supported_ciso(self) -> None:
        report = assets.inspect_path(self.disc)
        self.assertTrue(report["valid"])
        self.assertEqual(report["game_id"], "GAFE01")
        self.assertEqual(report["revision"], 0)
        self.assertEqual(report["container"], "CISO")
        self.assertEqual(report["fst_files"], 2)
        self.assertEqual(report["rel_size"], len(b"portable-rel-data"))

    def test_builds_indexed_pack(self) -> None:
        output = self.root / "runtime.assets.pack"
        report = assets.build_pack(self.disc, output)
        entries = _pack_entries(output)

        self.assertEqual(report["pack_entries"], 4)
        self.assertEqual(entries["sys/main.dol"][0][-4:], b"DOL!")
        self.assertEqual(
            entries["sys/foresta.rel"][0], b"portable-rel-data"
        )
        self.assertEqual(entries["disc/test.bin"][0], b"FILE")
        self.assertTrue(
            entries["sys/foresta.rel"][1]
            & assets.ENTRY_YAZ0_DECOMPRESSED
        )

    def test_rejects_wrong_revision(self) -> None:
        self.disc.write_bytes(_ciso(_disc_image(revision=1)))
        with self.assertRaisesRegex(assets.AssetError, "unsupported disc"):
            assets.inspect_path(self.disc)

    def test_rejects_output_overwrite(self) -> None:
        output = self.root / "runtime.assets.pack"
        output.write_bytes(b"keep")
        with self.assertRaisesRegex(assets.AssetError, "already exists"):
            assets.build_pack(self.disc, output)
        self.assertEqual(output.read_bytes(), b"keep")

    def test_force_replaces_only_with_valid_input(self) -> None:
        output = self.root / "runtime.assets.pack"
        output.write_bytes(b"known-good")
        self.disc.write_bytes(b"bad")
        with self.assertRaises(assets.AssetError):
            assets.build_pack(self.disc, output, force=True)
        self.assertEqual(output.read_bytes(), b"known-good")

    def test_yaz0_rejects_invalid_back_reference(self) -> None:
        malformed = b"Yaz0" + (3).to_bytes(4, "big") + bytes(8) + bytes(4)
        with self.assertRaisesRegex(assets.AssetError, "precedes output"):
            assets.decode_yaz0(malformed)


if __name__ == "__main__":
    unittest.main()
