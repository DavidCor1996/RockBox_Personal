#!/usr/bin/env python3
"""Host-only tests for tools/nano3g_release_audit.py."""

from __future__ import annotations

from pathlib import Path
import tempfile
import unittest
import zipfile

from tools import nano3g_deploy as deploy
from tools import nano3g_release_audit as audit


def firmware_image(body: bytes = b"release body") -> bytes:
    checksum = (deploy.MODEL_NUMBER + sum(body)) & 0xFFFFFFFF
    return checksum.to_bytes(4, "big") + deploy.MODEL_TAG + body


class Nano3gReleaseAuditTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory(prefix="n3g-audit-test-")
        self.root = Path(self.temp.name)
        self.firmware = self.root / "rockbox.ipod"
        self.firmware.write_bytes(firmware_image())
        self.package = self.root / "rockbox.zip"
        with zipfile.ZipFile(self.package, "w") as archive:
            archive.writestr(".rockbox/rockbox.ipod", self.firmware.read_bytes())
            archive.writestr(".rockbox/rocks/test.rock", b"plugin")
            archive.writestr(".rockbox/codecs/test.codec", b"codec")

    def tearDown(self) -> None:
        self.temp.cleanup()

    def test_artifact_report_and_manifests(self) -> None:
        report, manifests = audit.inspect_artifacts(self.firmware, self.package)
        self.assertTrue(report["package"]["firmware_matches"])
        self.assertEqual(report["package"]["plugins"], 1)
        self.assertEqual(report["package"]["codecs"], 1)
        self.assertIn(".rockbox/rocks/test.rock", manifests["plugins.sha256"])
        self.assertIn(".rockbox/codecs/test.codec", manifests["codecs.sha256"])

    def test_mismatched_firmware_is_rejected(self) -> None:
        self.firmware.write_bytes(firmware_image(b"different"))
        with self.assertRaisesRegex(audit.AuditError, "does not match"):
            audit.inspect_artifacts(self.firmware, self.package)

    def test_release_record_is_written_atomically(self) -> None:
        report, manifests = audit.inspect_artifacts(self.firmware, self.package)
        output = self.root / "record"
        audit.write_release_record(output, report, manifests)
        self.assertTrue((output / "release-audit.json").is_file())
        self.assertTrue((output / "package-files.sha256").is_file())
        self.assertTrue((output / "plugins.sha256").is_file())
        self.assertTrue((output / "codecs.sha256").is_file())


if __name__ == "__main__":
    unittest.main()
