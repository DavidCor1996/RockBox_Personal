#!/usr/bin/env python3
"""Tests for tools/magazine_prepare.py."""

from __future__ import annotations

import argparse
from pathlib import Path
import tempfile
import unittest

from tools import magazine_prepare as prepare


class MagazinePrepareTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory(prefix="magazine-test-")
        self.root = Path(self.temp.name)

    def tearDown(self) -> None:
        self.temp.cleanup()

    def test_selects_original_text_pdf(self) -> None:
        selected = prepare.select_archive_pdf(
            {
                "files": [
                    {
                        "name": "derived.pdf",
                        "source": "derivative",
                        "format": "Text PDF",
                        "size": "900",
                    },
                    {
                        "name": "original.pdf",
                        "source": "original",
                        "format": "Text PDF",
                        "size": "800",
                    },
                    {
                        "name": "scan.epub",
                        "source": "derivative",
                        "format": "EPUB",
                        "size": "1000",
                    },
                ]
            }
        )
        self.assertEqual(selected["name"], "original.pdf")

    def test_rejects_archive_without_pdf(self) -> None:
        with self.assertRaisesRegex(prepare.PrepareError, "no PDF"):
            prepare.select_archive_pdf({"files": [{"name": "book.epub"}]})

    def test_safe_issue_id(self) -> None:
        self.assertEqual(
            prepare.safe_issue_id("wwf-divas-2002"), "wwf-divas-2002"
        )
        for value in ("../escape", "has space", "", ".."):
            with self.subTest(value=value):
                with self.assertRaises(prepare.PrepareError):
                    prepare.safe_issue_id(value)

    def test_manifest_contains_reader_contract(self) -> None:
        manifest = prepare.manifest_text(
            issue_id="sample",
            title="Sample Issue",
            creator="Publisher",
            year="2002",
            page_count=130,
            source_kind="internet_archive",
            source_id="sample-c",
            source_url="https://archive.org/details/sample-c/",
            source_md5="0123456789abcdef",
        )
        self.assertIn("schema=1\n", manifest)
        self.assertIn("page_count=130\n", manifest)
        self.assertIn("page_pattern=pages/%04d.jpg\n", manifest)
        self.assertIn("preview=cover-pane.jpg\n", manifest)
        self.assertIn("prepared_width=480\n", manifest)
        self.assertIn("prepared_height=640\n", manifest)
        self.assertIn("prepare_profile=standard\n", manifest)
        self.assertIn("crop_margins=0\n", manifest)
        self.assertIn("category=Uncategorized\n", manifest)
        self.assertIn("locked=0\n", manifest)

    def test_fine_text_manifest_contract(self) -> None:
        manifest = prepare.manifest_text(
            issue_id="sample",
            title="Sample Issue",
            creator="Publisher",
            year="2002",
            page_count=130,
            source_kind="local_pdf",
            source_id="",
            source_url="",
            source_md5="0123456789abcdef",
            profile_name="fine-text",
        )
        self.assertIn("prepare_profile=fine-text\n", manifest)
        self.assertIn("crop_margins=1\n", manifest)
        self.assertIn("prepared_width=720\n", manifest)
        self.assertIn("prepared_height=960\n", manifest)

    def test_catalog_update_is_sorted_and_unique(self) -> None:
        (self.root / "catalog.mgi").write_text(
            "# existing\nzeta\nAlpha\nzeta\n", encoding="utf-8"
        )
        prepare.update_catalog(self.root, "beta")
        self.assertEqual(
            (self.root / "catalog.mgi").read_text(encoding="utf-8"),
            "# Rockbox Magazines catalog v1\nAlpha\nbeta\nzeta\n",
        )

    def test_install_refuses_overwrite_without_force(self) -> None:
        destination = self.root / "issue"
        destination.mkdir()
        staged = self.root / "staged"
        staged.mkdir()
        with self.assertRaisesRegex(prepare.PrepareError, "--force"):
            prepare.install_staged(staged, destination, False)
        self.assertTrue(destination.is_dir())
        self.assertTrue(staged.is_dir())

    def test_install_force_replaces_complete_directory(self) -> None:
        destination = self.root / "issue"
        destination.mkdir()
        (destination / "old").write_text("old", encoding="utf-8")
        staged = self.root / "staged"
        staged.mkdir()
        (staged / "new").write_text("new", encoding="utf-8")
        prepare.install_staged(staged, destination, True)
        self.assertFalse((destination / "old").exists())
        self.assertEqual((destination / "new").read_text(), "new")
        self.assertFalse((self.root / "issue.previous").exists())

    def test_pdf_page_count_parser(self) -> None:
        original_run = prepare.run
        prepare.run = lambda _command, capture=False: (
            "Title: Sample\nPages:          130\n"
        )
        try:
            self.assertEqual(
                prepare.pdf_page_count("pdfinfo", self.root / "x.pdf"), 130
            )
        finally:
            prepare.run = original_run

    def test_argument_contract(self) -> None:
        arguments = prepare.parser().parse_args(
            [
                "--pdf",
                "x.pdf",
                "--source-id",
                "sample-c",
                "--output",
                "Magazines/x",
            ]
        )
        self.assertIsInstance(arguments, argparse.Namespace)
        self.assertEqual(arguments.pdf, Path("x.pdf"))
        self.assertEqual(arguments.source_id, "sample-c")
        self.assertEqual(arguments.profile, "standard")

    def test_fine_text_argument_contract(self) -> None:
        arguments = prepare.parser().parse_args(
            [
                "--pdf",
                "x.pdf",
                "--output",
                "Magazines/x",
                "--profile",
                "fine-text",
            ]
        )
        self.assertEqual(arguments.profile, "fine-text")

    def test_category_and_lock_manifest_contract(self) -> None:
        manifest = prepare.manifest_text(
            issue_id="sample",
            title="Sample Issue",
            creator="Publisher",
            year="2002",
            page_count=12,
            source_kind="local_pdf",
            source_id="",
            source_url="",
            source_md5="0123456789abcdef",
            category="Wrestling",
            locked=True,
        )
        self.assertIn("category=Wrestling\n", manifest)
        self.assertIn("locked=1\n", manifest)


if __name__ == "__main__":
    unittest.main()
