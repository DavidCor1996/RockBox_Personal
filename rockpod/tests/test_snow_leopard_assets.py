"""Tests for the private Snow Leopard Desktop Mode asset pipeline."""

import json
import os
from pathlib import Path

import pytest
from PIL import Image

import services.snow_leopard_assets as snow


def _write_image(path, size=(64, 64), color=(40, 90, 160, 255)):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.new("RGBA", size, color).save(path)


def _complete_capture(root):
    root = Path(root)
    root.mkdir(parents=True, exist_ok=True)
    marker = root / "capture-manifest.json"
    marker.write_text(
        json.dumps({"product_version": "10.6.8", "capture": "native AppKit"}),
        encoding="utf-8",
    )
    seen = set()
    for spec in snow.ASSET_SPECS:
        source = root / spec.source_candidates[0]
        if source in seen:
            continue
        seen.add(source)
        if spec.kind == "font_atlas":
            source.parent.mkdir(parents=True, exist_ok=True)
            source.write_bytes(b"fake-private-ttc")
        else:
            _write_image(source, spec.size)
    return root


def test_boot_contract_requires_real_background_and_twelve_captured_phases():
    boot = [
        spec
        for spec in snow.ASSET_SPECS
        if spec.asset_id.startswith("boot.")
    ]

    assert len(boot) == 13
    assert boot[0].asset_id == "boot.background"
    assert boot[0].size == (320, 240)
    assert [spec.asset_id for spec in boot[1:]] == [
        f"boot.spinner_{index:02d}"
        for index in range(snow.BOOT_FRAME_COUNT)
    ]
    assert all(spec.size == (24, 24) for spec in boot[1:])
    assert all(spec.runtime_resident is False for spec in boot)
    assert all(
        any("boot/" in candidate for candidate in spec.source_candidates)
        for spec in boot
    )


def test_discovery_fails_closed_when_real_assets_are_missing(tmp_dir):
    root = Path(tmp_dir)
    (root / "capture-manifest.json").write_text(
        json.dumps({"product_version": "10.6.8"}),
        encoding="utf-8",
    )
    _write_image(root / "desktop/aurora.png", (320, 240))

    report = snow.discover_assets([root])

    assert report["version"]["snow_leopard"] is True
    assert report["complete"] is False
    assert "cursor.arrow" in report["missing"]
    assert report["resolved"]["desktop.aurora"].endswith("desktop/aurora.png")


def test_discovery_rejects_modern_macos_source(tmp_dir):
    root = Path(tmp_dir)
    version = root / "System/Library/CoreServices/SystemVersion.plist"
    version.parent.mkdir(parents=True)
    with version.open("wb") as handle:
        import plistlib
        plistlib.dump({"ProductVersion": "14.5"}, handle)
    _write_image(root / "desktop/aurora.png", (320, 240))

    report = snow.discover_assets([root])

    assert report["version"]["snow_leopard"] is False
    assert report["complete"] is False


def test_build_validate_and_detect_checksum_drift(tmp_dir, monkeypatch):
    source = _complete_capture(Path(tmp_dir) / "owned")
    output = Path(tmp_dir) / "pack"

    def fake_font(source_path, destination, spec):
        destination.parent.mkdir(parents=True, exist_ok=True)
        snow._save_rgb565_bmp(Image.new("RGB", spec.size, (40, 90, 160)), destination)
        metrics = destination.with_suffix(".widths")
        metrics.write_bytes(bytes([7] * 95))
        return {
            "operation": "test-font-atlas",
            "font_size": spec.font_size,
            "font_face": spec.font_face,
            "metrics_path": metrics.name,
            "metrics_sha256": snow.sha256(metrics),
        }

    monkeypatch.setattr(snow, "_convert_font_atlas", fake_font)
    report = snow.build_pack([source], output)

    assert report["valid"] is True
    assert report["complete"] is True
    assert report["manifest"]["product_version"] == "10.6.8"
    assert report["manifest"]["personal_use_only"] is True
    assert (output / "PROVENANCE.txt").is_file()
    aurora = output / report["manifest"]["assets"]["desktop.aurora"]["path"]
    assert snow._bmp_header(aurora)["depth"] == 16
    assert snow._bmp_header(aurora)["compression"] == 3

    cursor = output / report["manifest"]["assets"]["cursor.arrow"]["path"]
    cursor.write_bytes(cursor.read_bytes() + b"drift")
    drift = snow.validate_pack(output)
    assert drift["valid"] is False
    assert any("cursor.arrow: checksum mismatch" in item for item in drift["errors"])


def test_menu_preview_shows_idle_desktop_and_real_dock_icons(tmp_dir):
    root = Path(tmp_dir)
    sources = {
        "320x240.desktop.aurora": (
            "320x240/desktop/aurora.bmp",
            Image.new("RGB", (320, 240), (0, 0, 255)),
        ),
        "320x240.desktop.menubar": (
            "320x240/desktop/menubar.bmp",
            Image.new("RGB", (320, 21), (0, 255, 0)),
        ),
        "320x240.desktop.dock_shelf": (
            "320x240/desktop/dock-shelf.bmp",
            Image.new("RGB", (288, 26), (255, 255, 0)),
        ),
    }
    assets = {}
    for asset_id, (relative, image) in sources.items():
        path = root / relative
        snow._save_rgb565_bmp(image, path)
        assets[asset_id] = {
            "path": relative,
            "output_sha256": snow.sha256(path),
        }
    for name in snow.MENU_PREVIEW_DOCK_ICONS:
        if name == "sitekick":
            continue
        relative = f"320x240/icons/{name}.rga"
        path = root / relative
        snow._save_rga(Image.new("RGBA", (32, 32), (255, 0, 0, 255)), path)
        assets[f"320x240.icon.{name}"] = {
            "path": relative,
            "output_sha256": snow.sha256(path),
        }

    item = snow._build_menu_preview(root, assets)

    assert "320x240.chrome.window_sidebar" not in item["source_asset_ids"]
    assert set(item["source_asset_ids"]) == set(snow.MENU_PREVIEW_SOURCES)
    with Image.open(root / item["path"]) as preview:
        rgb = preview.convert("RGB")
        # The work area remains plain Aurora: no Finder window is composited.
        assert rgb.getpixel((80, 100))[2] > 240
        # Nine slots place Finder at x=8; sample inside its red fixture.
        finder = rgb.getpixel((24, 204))
        assert finder[0] > 240 and finder[1] < 16 and finder[2] < 16

    # A checksum-valid preview from the former window composition must still
    # be replaced when an existing private pack is reinstalled.
    stale = dict(item)
    stale["operation"] = "compose-authentic-runtime-and-crop-1to1"
    stale["source_asset_ids"] = [
        "320x240.desktop.aurora",
        "320x240.desktop.menubar",
        "320x240.chrome.window_sidebar",
        "320x240.desktop.dock_shelf",
    ]
    assets[snow.MENU_PREVIEW_ID] = stale
    (root / "manifest.json").write_text(
        json.dumps({"assets": assets}),
        encoding="utf-8",
    )

    snow._ensure_menu_preview(root)

    repaired = json.loads(
        (root / "manifest.json").read_text(encoding="utf-8")
    )["assets"][snow.MENU_PREVIEW_ID]
    assert repaired["operation"] == snow.MENU_PREVIEW_OPERATION
    assert repaired["source_asset_ids"] == list(snow.MENU_PREVIEW_SOURCES)


def test_install_pack_backs_up_only_owned_pack(tmp_dir, monkeypatch):
    source = _complete_capture(Path(tmp_dir) / "owned")
    output = Path(tmp_dir) / "pack"

    def fake_font(source_path, destination, spec):
        destination.parent.mkdir(parents=True, exist_ok=True)
        snow._save_rgb565_bmp(Image.new("RGB", spec.size, (40, 90, 160)), destination)
        metrics = destination.with_suffix(".widths")
        metrics.write_bytes(bytes([7] * 95))
        return {
            "operation": "test-font-atlas",
            "metrics_path": metrics.name,
            "metrics_sha256": snow.sha256(metrics),
        }

    monkeypatch.setattr(snow, "_convert_font_atlas", fake_font)
    snow.build_pack([source], output)

    device = Path(tmp_dir) / "device"
    (device / ".rockbox").mkdir(parents=True)
    unrelated = device / ".rockbox/config.cfg"
    unrelated.write_text("theme: keep\n", encoding="utf-8")
    legacy_apps = device / snow.LEGACY_XP_PACK_RELATIVES[0]
    legacy_data = device / snow.LEGACY_XP_PACK_RELATIVES[1]
    legacy_apps.mkdir(parents=True)
    legacy_data.mkdir(parents=True)
    (legacy_apps / "bliss.bmp").write_bytes(b"legacy-apps")
    (legacy_data / "bliss.bmp").write_bytes(b"legacy-hosted")
    first = snow.install_pack(output, device)
    assert first["backup"] == ""
    assert len(first["destinations"]) == 2
    assert all(snow.validate_pack(path)["valid"] for path in first["destinations"])
    assert len(first["replaced_xp"]) == 2
    assert not legacy_apps.exists()
    assert not legacy_data.exists()
    assert {
        (Path(item["backup"]) / "bliss.bmp").read_bytes()
        for item in first["replaced_xp"]
    } == {b"legacy-apps", b"legacy-hosted"}
    installed_marker = Path(first["destination"]) / "personal-note.txt"
    installed_marker.write_text("old", encoding="utf-8")

    second = snow.install_pack(output, device)
    assert second["backup"]
    assert (Path(second["backup"]) / "personal-note.txt").read_text() == "old"
    assert unrelated.read_text(encoding="utf-8") == "theme: keep\n"
    assert snow.validate_pack(second["destination"])["valid"] is True


def test_official_combo_hash_is_recognized_without_claiming_complete(tmp_dir):
    combo = Path(tmp_dir) / "MacOSXUpdCombo10.6.8.dmg"
    combo.write_bytes(b"not-the-official-payload")

    report = snow.detect_source_version([combo])

    assert report["snow_leopard"] is False
    assert report["official_combo_sources"] == []


def test_capture_manifest_checksum_drift_fails_discovery(tmp_dir):
    root = Path(tmp_dir)
    _write_image(root / "desktop/aurora.png", (320, 240))
    (root / "capture-manifest.json").write_text(
        json.dumps(
            {
                "product_version": "10.6.8",
                "assets": {
                    "desktop/aurora.png": {
                        "sha256": "0" * 64,
                    }
                },
            }
        ),
        encoding="utf-8",
    )

    report = snow.discover_assets([root])

    assert report["complete"] is False
    assert any(
        "capture checksum mismatch" in error
        for error in report["integrity_errors"]
    )


def test_valid_nested_capture_cannot_prove_unrelated_parent_asset(tmp_dir):
    parent = Path(tmp_dir)
    capture = parent / "SnowLeopardDesktopCapture"
    capture.mkdir()
    (capture / "capture-manifest.json").write_text(
        json.dumps({"product_version": "10.6.8"}),
        encoding="utf-8",
    )
    _write_image(parent / "desktop/aurora.png", (320, 240))

    report = snow.discover_assets([parent])

    assert report["complete"] is False
    assert any(
        "outside every proven Mac OS X 10.6 root" in error
        for error in report["integrity_errors"]
    )


def test_validate_pack_rejects_manifest_path_escape(tmp_dir):
    pack = Path(tmp_dir) / "pack"
    pack.mkdir()
    (pack / "PROVENANCE.txt").write_text("private", encoding="utf-8")
    outside = Path(tmp_dir) / "outside.bmp"
    _write_image(outside, (320, 240))
    manifest = {
        "format": snow.PACK_FORMAT,
        "pack_id": snow.PACK_ID,
        "product_version": "10.6.8",
        "personal_use_only": True,
        "assets": {
            "desktop.aurora": {
                "path": "../outside.bmp",
                "output_sha256": snow.sha256(outside),
            }
        },
    }
    (pack / "manifest.json").write_text(
        json.dumps(manifest),
        encoding="utf-8",
    )

    report = snow.validate_pack(pack)

    assert report["valid"] is False
    assert any("path escapes pack" in error for error in report["errors"])
