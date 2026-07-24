#!/usr/bin/env python3

from __future__ import annotations

import plistlib
import struct
import tempfile
import unittest
import zipfile
from pathlib import Path

from tools.ipodgames.eapp_inspect import EappError, inspect_eapp
from tools.ipodgames.ipg_import import import_package
from tools.ipodgames.ipg_inspect import PackageError, inspect_package
from tools.ipodgames.retailos_frameworks import (
    inspect_retailos,
    match_eapp_imports,
)


def _manifest(executable: str = "Executables/Game.bin") -> dict:
    return {
        "BuildIdentifier": 100,
        "Files": [
            {
                "DRM": True,
                "Digest": "00" * 20,
                "Path": executable,
                "Size": 64,
                "Verify": True,
            }
        ],
        "GUID": "TEST01",
        "Name": "Synthetic Game",
        "Platforms": [
            {
                "BuildID": 100,
                "ExecutablePath": executable,
                "PlatformID": 2,
                "PlatformVersion": 1,
                "Size": 64,
            }
        ],
        "Version": "1.0",
    }


def _write_package(path: Path, executable: bytes, manifest: dict | None = None) -> None:
    with zipfile.ZipFile(path, "w", zipfile.ZIP_STORED) as archive:
        archive.writestr("Manifest.plist", plistlib.dumps(manifest or _manifest()))
        archive.writestr("Executables/Game.bin", executable)
        archive.writestr("iTunesArtwork", b"\xff\xd8\xff" + bytes(16))


def _synthetic_eapp(path: Path) -> None:
    data = bytearray(0xC0)
    data[:4] = b"eapp"
    struct.pack_into("<I", data, 0x04, 0x10001000)
    struct.pack_into("<I", data, 0x08, 5)
    struct.pack_into("<I", data, 0x0C, 0x28)
    struct.pack_into("<I", data, 0x10, 0x1800002C)
    data[0x2C : 0x2C + len(b"InputEvents")] = b"InputEvents"
    data[0x4C:0x5C] = bytes(range(16))
    struct.pack_into("<I", data, 0x5C, 2)
    struct.pack_into("<I", data, 0x60, 0x18000080)
    struct.pack_into("<II", data, 0x64, 0xE59FF000, 0xE59FF000)
    struct.pack_into("<II", data, 0x6C, 0, 0)
    struct.pack_into("<I", data, 0x74, 0xEBFFFFFB)
    data[0x80 : 0x80 + len(b"Terminator")] = b"Terminator"
    path.write_bytes(data)


def _synthetic_retailos(path: Path) -> None:
    data = bytearray(0x100)
    offset = 0x20
    data[offset : offset + len(b"InputEvents")] = b"InputEvents"
    struct.pack_into("<III", data, offset + 32, 1, 2, 0x88)
    struct.pack_into("<II", data, offset + 44, 0x1000, 0x2000)
    data[offset + 52 : offset + 68] = bytes(range(16))
    struct.pack_into("<I", data, offset + 68, 2)
    path.write_bytes(data)


class PackageTests(unittest.TestCase):
    def test_encrypted_package_report(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            package = Path(directory) / "game.ipg"
            _write_package(package, bytes(range(64)))
            report = inspect_package(package)

            self.assertTrue(report["valid"])
            self.assertEqual(report["manifest"]["name"], "Synthetic Game")
            self.assertEqual(
                report["platforms"][0]["executable"]["state"], "encrypted"
            )

    def test_decrypted_eapp_detection(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            package = Path(directory) / "game.ipg"
            _write_package(package, b"eapp" + bytes(60))
            report = inspect_package(package)
            self.assertEqual(
                report["platforms"][0]["executable"]["state"],
                "decrypted-eapp",
            )

    def test_rejects_traversal_member(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            package = Path(directory) / "bad.ipg"
            with zipfile.ZipFile(package, "w") as archive:
                archive.writestr("../Manifest.plist", plistlib.dumps(_manifest()))
            with self.assertRaises(PackageError):
                inspect_package(package)

    def test_import_writes_sanitized_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            package = root / "game.ipg"
            _write_package(package, bytes(range(64)))
            metadata = import_package(package, root / "output")
            text = metadata.read_text(encoding="utf-8")

            self.assertTrue(text.startswith("IPODGAMES/1\n"))
            self.assertIn("name=Synthetic Game\n", text)
            self.assertIn("executable_state=encrypted\n", text)
            self.assertIn("cover=cover.jpg\n", text)
            self.assertNotIn(str(root), text)
            index = root / "output" / "games" / "ipodgames" / "games.tsv"
            self.assertIn("Synthetic Game", index.read_text(encoding="utf-8"))

    def test_import_extracts_decrypted_package_executable(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            package = root / "game.ipg"
            executable = b"eapp" + bytes(60)
            _write_package(package, executable)
            metadata = import_package(package, root / "output")
            imported = metadata.parent / "executable.eapp"

            self.assertEqual(imported.read_bytes(), executable)
            self.assertIn(
                "local_executable=executable.eapp\n",
                metadata.read_text(encoding="utf-8"),
            )

    def test_import_extracts_manifest_assets(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            package = root / "game.ipg"
            manifest = _manifest()
            manifest["Files"].append(
                {"Path": "data/texture.bin", "Size": 4, "Verify": True}
            )
            with zipfile.ZipFile(package, "w", zipfile.ZIP_STORED) as archive:
                archive.writestr("Manifest.plist", plistlib.dumps(manifest))
                archive.writestr("Executables/Game.bin", bytes(range(64)))
                archive.writestr("data/texture.bin", b"GAME")
            metadata = import_package(package, root / "output")

            self.assertEqual(
                (metadata.parent / "assets/data/texture.bin").read_bytes(),
                b"GAME",
            )
            self.assertIn(
                "asset_count=1\n", metadata.read_text(encoding="utf-8")
            )

    def test_allows_identical_duplicate_manifest_entry(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            package = Path(directory) / "game.ipg"
            manifest = _manifest()
            manifest["Files"].append(dict(manifest["Files"][0]))
            _write_package(package, bytes(range(64)), manifest)
            report = inspect_package(package)

            self.assertTrue(report["valid"])
            self.assertEqual(
                report["manifest"]["duplicate_file_paths"],
                ["Executables/Game.bin"],
            )


class EappTests(unittest.TestCase):
    def test_framework_table(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "game.eapp"
            _synthetic_eapp(path)
            report = inspect_eapp(path)

            self.assertEqual(report["header"]["inferred_load_base"], "0x18000000")
            self.assertEqual(report["framework_count"], 1)
            self.assertEqual(report["frameworks"][0]["name"], "InputEvents")
            self.assertEqual(report["frameworks"][0]["import_count"], 2)
            self.assertEqual(report["directly_referenced_imports"], 1)
            self.assertEqual(
                report["frameworks"][0]["direct_branch_imports"][0]["ordinal"],
                1,
            )

    def test_rejects_encrypted_input(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "encrypted.bin"
            path.write_bytes(bytes(range(64)))
            with self.assertRaises(EappError):
                inspect_eapp(path)


class RetailOsTests(unittest.TestCase):
    def test_matches_direct_import_to_export_address(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            eapp = root / "game.eapp"
            firmware = root / "retailos.bin"
            _synthetic_eapp(eapp)
            _synthetic_retailos(firmware)

            retailos_report = inspect_retailos(firmware)
            matches = match_eapp_imports(retailos_report, inspect_eapp(eapp))
            used = matches[0]["directly_referenced_exports"]

            self.assertEqual(matches[0]["status"], "matched")
            self.assertEqual(used[0]["ordinal"], 1)
            self.assertEqual(used[0]["implementation_offset"], "0x00002000")
            self.assertEqual(
                used[0]["implementation_runtime_address"], "0x10002000"
            )
            framework = retailos_report["frameworks"][0]
            self.assertEqual(framework["next_record_file_offset"], 0x88)


if __name__ == "__main__":
    unittest.main()
