"""Tests for local-only iTunes asset discovery and import helpers."""

import json
import os
import tarfile

import main
import services.itunes_asset_tools as tools_module
import services.theme_assets as theme_assets_module
from services.itunes_asset_tools import (
    extract_itunes_assets,
    format_discovery_guide,
    import_itunes_assets,
    validate_personal_theme,
)
from services.theme_assets import ThemeAssetManager


def _prepare_theme_dirs(tmp_dir, monkeypatch):
    default_dir = os.path.join(tmp_dir, "theme_default")
    personal_dir = os.path.join(tmp_dir, "theme_itunes_personal")
    os.makedirs(os.path.join(default_dir, "branding"), exist_ok=True)
    os.makedirs(os.path.join(default_dir, "toolbar"), exist_ok=True)
    os.makedirs(os.path.join(default_dir, "sidebar"), exist_ok=True)
    os.makedirs(personal_dir, exist_ok=True)

    with open(os.path.join(default_dir, "theme.json"), "w", encoding="utf-8") as f:
        json.dump(
            {
                "name": "Default",
                "assets": {
                    "branding_title": "branding/title.png",
                    "toolbar_sync": "toolbar/sync.png",
                    "sidebar_music": "sidebar/music.png",
                },
            },
            f,
        )
    with open(os.path.join(default_dir, "branding", "title.png"), "wb") as f:
        f.write(b"default-title")
    with open(os.path.join(default_dir, "toolbar", "sync.png"), "wb") as f:
        f.write(b"default-sync")
    with open(os.path.join(default_dir, "sidebar", "music.png"), "wb") as f:
        f.write(b"default-music")

    monkeypatch.setattr(theme_assets_module, "DEFAULT_THEME_DIR", theme_assets_module.Path(default_dir))
    monkeypatch.setattr(theme_assets_module, "PERSONAL_THEME_DIR", theme_assets_module.Path(personal_dir))
    monkeypatch.setattr(tools_module, "PERSONAL_THEME_DIR", theme_assets_module.Path(personal_dir))
    return default_dir, personal_dir


def test_import_itunes_assets_maps_partial_theme(config, tmp_dir, monkeypatch):
    _prepare_theme_dirs(tmp_dir, monkeypatch)
    source_dir = os.path.join(tmp_dir, "extracted")
    os.makedirs(source_dir, exist_ok=True)
    with open(os.path.join(source_dir, "itunes_title_logo.png"), "wb") as f:
        f.write(b"personal-title")
    with open(os.path.join(source_dir, "sync_button.bmp"), "wb") as f:
        f.write(b"personal-sync")
    with open(os.path.join(source_dir, "music_source_icon.ico"), "wb") as f:
        f.write(b"personal-music")

    config.theme_mode = "personal"
    report = import_itunes_assets(source_dir, config)
    manager = ThemeAssetManager(config)

    assert report["copied_count"] == 3
    assert os.path.isfile(os.path.join(report["validation"]["manifest_path"]))
    assert manager.asset_path("branding_title").endswith("theme_itunes_personal/branding/title.png")
    assert manager.asset_path("toolbar_sync").endswith("theme_itunes_personal/toolbar/sync.bmp")
    assert manager.asset_path("sidebar_music").endswith("theme_itunes_personal/sidebar/music.ico")


def test_validate_personal_theme_reports_missing_assets(config, tmp_dir, monkeypatch):
    _prepare_theme_dirs(tmp_dir, monkeypatch)
    personal_dir = os.path.join(tmp_dir, "theme_itunes_personal")
    with open(os.path.join(personal_dir, "theme.json"), "w", encoding="utf-8") as f:
        json.dump({"name": "Personal"}, f)

    config.theme_mode = "personal"
    report = validate_personal_theme(config)

    assert report["requested"] == "personal"
    assert report["active"] == "personal"
    assert report["missing_count"] > 0
    assert "toolbar_refresh" in report["missing_assets"]


def test_find_assets_guide_contains_sources_and_versions():
    guide = format_discovery_guide()
    assert "OldVersion iTunes 7.0" in guide
    assert "7.0.2" in guide
    assert "Windows extraction:" in guide
    assert "macOS extraction:" in guide


def test_extract_itunes_assets_from_directory(tmp_dir):
    source_dir = os.path.join(tmp_dir, "itunes_app", "Contents", "Resources", "toolbar")
    os.makedirs(source_dir, exist_ok=True)
    image_path = os.path.join(source_dir, "sync.png")
    with open(image_path, "wb") as f:
        f.write(b"png")
    exe_path = os.path.join(tmp_dir, "itunes_app", "Contents", "Resources", "iTunes.exe")
    with open(exe_path, "wb") as f:
        f.write(b"exe")

    report = extract_itunes_assets(os.path.join(tmp_dir, "itunes_app"))

    assert report["copied_asset_count"] == 1
    assert report["embedded_resource_candidates"]
    assert os.path.isfile(report["summary_path"])
    assert os.path.isdir(report["collected_dir"])


def test_extract_itunes_assets_unpacks_pkg_payloads(tmp_dir):
    source_root = os.path.join(tmp_dir, "itunes_pkg_root")
    payload_root = os.path.join(source_root, "iTunesX.pkg", "Contents")
    os.makedirs(payload_root, exist_ok=True)

    staged = os.path.join(tmp_dir, "payload_stage", "Applications", "iTunes.app", "Contents", "Resources")
    os.makedirs(staged, exist_ok=True)
    image_path = os.path.join(staged, "iTunes.icns")
    with open(image_path, "wb") as f:
        f.write(b"icns")

    archive_path = os.path.join(payload_root, "Archive.pax.gz")
    with tarfile.open(archive_path, "w:gz") as tar:
        tar.add(os.path.join(tmp_dir, "payload_stage"), arcname=".")

    report = extract_itunes_assets(source_root)

    assert report["copied_asset_count"] >= 1
    collected = []
    for root, _, files in os.walk(report["collected_dir"]):
        for filename in files:
            collected.append(os.path.join(root, filename))
    assert any(path.endswith("iTunes.icns") for path in collected)


def test_parse_args_supports_asset_commands():
    args = main.parse_args(["extract-itunes-assets", "/tmp/iTunesSetup.exe", "--out", "/tmp/out"])
    assert args.command == "extract-itunes-assets"
    assert args.source_path == "/tmp/iTunesSetup.exe"
    assert args.out == "/tmp/out"

    args = main.parse_args(["import-itunes-assets", "/tmp/extracted", "--json"])
    assert args.command == "import-itunes-assets"
    assert args.source_dir == "/tmp/extracted"
    assert args.json is True

    args = main.parse_args(["validate-theme"])
    assert args.command == "validate-theme"

    args = main.parse_args(["find-itunes-assets"])
    assert args.command == "find-itunes-assets"
