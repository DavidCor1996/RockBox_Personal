#!/usr/bin/env python3
"""Host-only tests for tools/nano3g_deploy.py."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock
import zipfile
import stat


SCRIPT = Path(__file__).with_name("nano3g_deploy.py")
SPEC = importlib.util.spec_from_file_location("nano3g_deploy", SCRIPT)
deploy = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules["nano3g_deploy"] = deploy
SPEC.loader.exec_module(deploy)


def firmware_image(body: bytes = b"native nano3g test body") -> bytes:
    checksum = (deploy.MODEL_NUMBER + sum(body)) & 0xFFFFFFFF
    return checksum.to_bytes(4, "big") + deploy.MODEL_TAG + body


class Nano3gDeployTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory(prefix="n3g-deploy-test-")
        self.root = Path(self.temp.name)
        self.mount = self.root / "nano"
        self.mount.mkdir()
        self.fixture = self.root / "lsblk.json"
        self.fixture.write_text(
            json.dumps(
                {
                    "blockdevices": [
                        {
                            "path": "/dev/sda",
                            "type": "disk",
                            "size": 512510136320,
                            "model": "iPod",
                            "serial": deploy.EXCLUDED_IPOD6G_SERIAL,
                            "tran": "usb",
                            "rm": True,
                            "children": [
                                {
                                    "path": "/dev/sda1",
                                    "type": "part",
                                    "fstype": "vfat",
                                    "mountpoints": [str(self.root / "classic")],
                                }
                            ],
                        },
                        {
                            "path": "/dev/sdb",
                            "type": "disk",
                            "size": 3892056064,
                            "model": "iPod",
                            "serial": deploy.EXPECTED_NANO_SERIAL,
                            "tran": "usb",
                            "rm": True,
                            "children": [
                                {
                                    "path": "/dev/sdb1",
                                    "type": "part",
                                    "fstype": "vfat",
                                    "mountpoints": [str(self.mount)],
                                }
                            ],
                        },
                    ]
                }
            ),
            encoding="utf-8",
        )
        self.firmware = self.root / "rockbox.ipod"
        self.firmware.write_bytes(firmware_image())
        self.package = self.root / "rockbox.zip"
        with zipfile.ZipFile(self.package, "w") as archive:
            archive.writestr(".rockbox/rockbox.ipod", self.firmware.read_bytes())
            archive.writestr(".rockbox/rocks/test.rock", b"plugin")
            archive.writestr(".rockbox/codecs/test.codec", b"codec")

    def tearDown(self) -> None:
        self.temp.cleanup()

    def arguments(self, *extra: str):
        return deploy.build_parser().parse_args(
            [
                "--firmware",
                str(self.firmware),
                "--package",
                str(self.package),
                "--mountpoint",
                str(self.mount),
                "--lsblk-json",
                str(self.fixture),
                *extra,
            ]
        )

    def test_identity_selects_nano_and_excludes_classic(self) -> None:
        selected = deploy.select_device(
            deploy.read_lsblk(self.fixture),
            deploy.EXPECTED_NANO_SERIAL,
            self.mount,
        )
        self.assertEqual(selected.disk, Path("/dev/sdb"))
        self.assertEqual(selected.serial, deploy.EXPECTED_NANO_SERIAL)

    def test_firmware_checksum_and_tag_are_required(self) -> None:
        _, digest, checksum = deploy.validate_firmware(self.firmware)
        self.assertEqual(len(digest), 64)
        self.assertEqual(checksum, int.from_bytes(self.firmware.read_bytes()[:4], "big"))
        broken = self.root / "broken.ipod"
        broken.write_bytes(b"\0\0\0\0nn3gbody")
        with self.assertRaises(deploy.DeployError):
            deploy.validate_firmware(broken)

    def test_default_is_read_only_plan(self) -> None:
        result = deploy.deploy(self.arguments())
        self.assertEqual(result["mode"], "plan")
        self.assertFalse((self.mount / "rockbox.ipod").exists())
        self.assertFalse((self.mount / ".rockbox").exists())

    def test_apply_installs_package_and_both_firmware_paths(self) -> None:
        old = firmware_image(b"old firmware")
        (self.mount / ".rockbox").mkdir()
        (self.mount / "rockbox.ipod").write_bytes(old)
        (self.mount / ".rockbox" / "rockbox.ipod").write_bytes(old)
        (self.mount / ".rockbox" / "user.cfg").write_text("keep", encoding="utf-8")
        backup = self.root / "backup"

        with mock.patch.object(deploy.os, "sync"):
            result = deploy.deploy(
                self.arguments("--apply", "--backup-dir", str(backup))
            )

        expected = self.firmware.read_bytes()
        self.assertEqual((self.mount / "rockbox.ipod").read_bytes(), expected)
        self.assertEqual(
            (self.mount / ".rockbox" / "rockbox.ipod").read_bytes(), expected
        )
        self.assertEqual(
            (self.mount / ".rockbox" / "rocks" / "test.rock").read_bytes(),
            b"plugin",
        )
        self.assertEqual(
            (self.mount / ".rockbox" / "codecs" / "test.codec").read_bytes(),
            b"codec",
        )
        self.assertEqual(
            (self.mount / ".rockbox" / "user.cfg").read_text(encoding="utf-8"),
            "keep",
        )
        self.assertEqual((backup / "root-rockbox.ipod").read_bytes(), old)
        self.assertEqual((backup / "dot-rockbox.ipod").read_bytes(), old)
        self.assertTrue((backup / "manifest.json").is_file())
        self.assertEqual(result["mode"], "apply")
        self.assertEqual(result["status"], "complete")
        self.assertEqual(len(result["installed_firmware"]), 2)

    def test_package_traversal_is_rejected(self) -> None:
        bad = self.root / "bad.zip"
        with zipfile.ZipFile(bad, "w") as archive:
            archive.writestr(".rockbox/../escape", b"bad")
        with self.assertRaises(deploy.DeployError):
            deploy.load_package(bad)

    def test_wrong_size_for_expected_serial_is_rejected(self) -> None:
        topology = deploy.read_lsblk(self.fixture)
        topology["blockdevices"][1]["size"] = 512510136320
        with self.assertRaises(deploy.DeployError):
            deploy.select_device(
                topology, deploy.EXPECTED_NANO_SERIAL, self.mount
            )

    def test_package_requires_matching_firmware(self) -> None:
        missing = self.root / "missing-firmware.zip"
        with zipfile.ZipFile(missing, "w") as archive:
            archive.writestr(".rockbox/rocks/test.rock", b"plugin")
        with self.assertRaisesRegex(deploy.DeployError, "missing"):
            deploy.deploy(self.arguments("--package", str(missing)))

    def test_zip_symlink_is_rejected(self) -> None:
        bad = self.root / "symlink.zip"
        info = zipfile.ZipInfo(".rockbox/link")
        info.create_system = 3
        info.external_attr = (stat.S_IFLNK | 0o777) << 16
        with zipfile.ZipFile(bad, "w") as archive:
            archive.writestr(info, "target")
        with self.assertRaisesRegex(deploy.DeployError, "special file"):
            deploy.load_package(bad)

    def test_fat_case_collision_is_rejected(self) -> None:
        bad = self.root / "case-collision.zip"
        with zipfile.ZipFile(bad, "w") as archive:
            archive.writestr(".rockbox/ROCKS/test.rock", b"one")
            archive.writestr(".rockbox/rocks/test.rock", b"two")
        with self.assertRaisesRegex(deploy.DeployError, "case aliases"):
            deploy.load_package(bad)

    def test_identical_fat_case_aliases_are_coalesced(self) -> None:
        aliases = self.root / "case-aliases.zip"
        with zipfile.ZipFile(aliases, "w") as archive:
            archive.writestr(".rockbox/Covers/Game.bmp", b"same")
            archive.writestr(".rockbox/covers/game.bmp", b"same")
        entries, _ = deploy.load_package(aliases)
        self.assertEqual(len(entries), 1)


if __name__ == "__main__":
    unittest.main()
