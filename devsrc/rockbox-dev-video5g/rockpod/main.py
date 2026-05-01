#!/usr/bin/env python3
"""RockPod entrypoint and local maintenance commands."""

import argparse
import json
import logging
import os
import sys
import tempfile

# Ensure the rockpod package root is on the path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from app.config import Config
from app.database import Database
from services.itunes_asset_tools import (
    build_asset_review,
    extract_itunes_assets,
    format_discovery_guide,
    format_validation_report,
    import_itunes_assets,
    validate_personal_theme,
)


def setup_logging(verbose=False, cli_mode=False):
    if verbose:
        level = logging.DEBUG
    elif cli_mode:
        level = logging.WARNING
    else:
        level = logging.INFO
    logging.basicConfig(
        level=level,
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
        datefmt="%H:%M:%S",
    )


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description="RockPod — Rockbox iPod Music Manager")
    subparsers = parser.add_subparsers(dest="command")

    import_parser = subparsers.add_parser(
        "import-itunes-assets",
        help="Import manually extracted iTunes-era assets into the local-only personal theme",
    )
    import_parser.add_argument("source_dir", help="Directory containing manually extracted assets")
    import_parser.add_argument("--config", help="Override config file path")
    import_parser.add_argument("--json", action="store_true", help="Print machine-readable JSON")

    extract_parser = subparsers.add_parser(
        "extract-itunes-assets",
        help="Extract image assets from a local installer, app bundle, or extracted directory",
    )
    extract_parser.add_argument("source_path", help="Local installer, app bundle, or extracted directory")
    extract_parser.add_argument("--out", help="Directory to write extracted output into")
    extract_parser.add_argument("--json", action="store_true", help="Print machine-readable JSON")

    review_parser = subparsers.add_parser(
        "review-itunes-assets",
        help="Build a local HTML/contact-sheet review bundle for extracted assets",
    )
    review_parser.add_argument("source_dir", help="Extracted asset directory to review")
    review_parser.add_argument("--out", help="Directory to write the review bundle into")
    review_parser.add_argument("--json", action="store_true", help="Print machine-readable JSON")

    validate_parser = subparsers.add_parser(
        "validate-theme",
        help="Validate the currently configured theme and report missing personal assets",
    )
    validate_parser.add_argument("--config", help="Override config file path")
    validate_parser.add_argument("--json", action="store_true", help="Print machine-readable JSON")

    find_parser = subparsers.add_parser(
        "find-itunes-assets",
        help="Print suggested manual sources and extraction steps for iTunes-era assets",
    )
    find_parser.add_argument("--json", action="store_true", help="Print machine-readable JSON")

    parser.add_argument("--music-dir", help="Override music library directory")
    parser.add_argument("--mock", action="store_true", help="Enable mock device mode for testing")
    parser.add_argument("--mock-path", help="Path for mock device (implies --mock)")
    parser.add_argument("--db", help="Override database path")
    parser.add_argument("--config", help="Override config file path")
    parser.add_argument("-v", "--verbose", action="store_true", help="Enable debug logging")
    return parser.parse_args(argv)


def launch_ui(args):
    from PySide6.QtGui import QIcon
    from PySide6.QtWidgets import QApplication

    from services.theme_assets import ThemeAssetManager
    from ui.main_window import MainWindow
    from ui.styles import get_stylesheet

    logger = logging.getLogger("rockpod")
    logger.info("Starting RockPod")

    config = Config(args.config)

    if args.music_dir:
        config.music_dir = args.music_dir
    if args.db:
        config.db_path = args.db
    if args.mock or args.mock_path:
        config.mock_device_enabled = True
        if args.mock_path:
            config.mock_device_path = args.mock_path
        elif not config.mock_device_path:
            config.mock_device_path = os.path.join(
                os.path.expanduser("~"), ".rockpod", "mock_device"
            )
            from services.device_detector import create_mock_device
            if not os.path.isdir(config.mock_device_path):
                create_mock_device(config.mock_device_path)

    config.ensure_dirs()
    configured_db_path = config.db_path
    runtime_db_path = _resolve_runtime_db_path(config, logger)
    config.db_path = runtime_db_path
    Database.init_db_once(config.db_path)
    try:
        config.db_path = configured_db_path
        config.save()
    except OSError as exc:
        logger.warning("Could not save config at startup: %s", exc)
    finally:
        config.db_path = runtime_db_path

    app = QApplication(sys.argv)
    app.setApplicationName("RockPod")
    app.setApplicationVersion("0.1.0")
    if hasattr(app, "setDesktopFileName"):
        app.setDesktopFileName("RockPod")
    app.setStyleSheet(get_stylesheet())
    theme_assets = ThemeAssetManager(config)
    app_icon_path = theme_assets.asset_path("branding_app_icon")
    if app_icon_path:
        app.setWindowIcon(QIcon(app_icon_path))

    window = MainWindow(config)
    if app_icon_path:
        window.setWindowIcon(QIcon(app_icon_path))
    window.show()
    window.raise_()
    window.activateWindow()

    logger.info("RockPod UI ready")
    return app.exec()


def _resolve_runtime_db_path(config, logger):
    configured = config.db_path
    try:
        Database.init_db_once(configured)
        return configured
    except OSError as exc:
        logger.warning("Configured database path is unusable: %s (%s)", configured, exc)
    except Exception as exc:
        logger.warning("Configured database path is unusable: %s (%s)", configured, exc)

    fallback_root = os.path.join(tempfile.gettempdir(), "rockpod-runtime")
    os.makedirs(fallback_root, exist_ok=True)
    fallback = os.path.join(fallback_root, "library.db")
    logger.warning("Falling back to runtime database: %s", fallback)
    return fallback


def run_import_assets(args):
    config = Config(args.config)
    report = import_itunes_assets(args.source_dir, config)
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print(f"Imported {report['copied_count']} assets from {report['source_dir']}")
        print(f"Manifest updated: {report['manifest_path']}")
        if report["copied_assets"]:
            print("Mapped assets:")
            for item in report["copied_assets"]:
                print(f"  - {item['asset']}: {item['source_path']} -> {item['mapped_path']}")
        if report["missing_assets"]:
            print("Still missing:")
            for name in report["missing_assets"]:
                print(f"  - {name}")
    return 0


def run_extract_assets(args):
    report = extract_itunes_assets(args.source_path, args.out)
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print(f"Source: {report['source_path']}")
        print(f"Extracted root: {report['extracted_root']}")
        print(f"Collected assets: {report['copied_asset_count']}")
        print(f"Collected directory: {report['collected_dir']}")
        if report["embedded_resource_candidates"]:
            print("Embedded resource candidates:")
            for path in report["embedded_resource_candidates"][:20]:
                print(f"  - {path}")
            if len(report["embedded_resource_candidates"]) > 20:
                print(f"  ... and {len(report['embedded_resource_candidates']) - 20} more")
        print(report["next_step"])
    return 0


def run_review_assets(args):
    report = build_asset_review(args.source_dir, args.out)
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print(f"Review bundle: {report['review_dir']}")
        print(f"Asset manifest: {report['manifest_path']}")
        print(f"HTML review: {report['html_path']}")
        for category, count in sorted(report["categories"].items()):
            print(f"  - {category}: {count}")
    return 0


def run_validate_theme(args):
    config = Config(args.config)
    report = validate_personal_theme(config)
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print(format_validation_report(report))
    return 0


def run_find_assets(args):
    if args.json:
        from services.itunes_asset_tools import (
            DISCOVERY_SOURCES,
            EXPECTED_LOCATIONS,
            MAC_EXTRACTION_GUIDE,
            VERSION_RECOMMENDATIONS,
            WINDOWS_EXTRACTION_GUIDE,
        )
        print(json.dumps({
            "sources": DISCOVERY_SOURCES,
            "recommended_versions": VERSION_RECOMMENDATIONS,
            "windows_extraction": WINDOWS_EXTRACTION_GUIDE,
            "mac_extraction": MAC_EXTRACTION_GUIDE,
            "expected_locations": EXPECTED_LOCATIONS,
        }, indent=2))
    else:
        print(format_discovery_guide())
    return 0


def main(argv=None):
    args = parse_args(argv)
    setup_logging(args.verbose, cli_mode=bool(args.command))

    if args.command == "extract-itunes-assets":
        return run_extract_assets(args)
    if args.command == "review-itunes-assets":
        return run_review_assets(args)
    if args.command == "import-itunes-assets":
        return run_import_assets(args)
    if args.command == "validate-theme":
        return run_validate_theme(args)
    if args.command == "find-itunes-assets":
        return run_find_assets(args)

    return launch_ui(args)


if __name__ == "__main__":
    sys.exit(main())
