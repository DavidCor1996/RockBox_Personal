#!/usr/bin/python
"""Capture real Mac OS X 10.6 UI pixels for private Rockpod use.

Python 2.6 compatible: this is intended to run with Snow Leopard's system
Python and PyObjC frameworks.
"""

from __future__ import print_function

import hashlib
import json
import os
import plistlib
import shutil
import subprocess
import sys
import time


HERE = os.path.abspath(os.path.dirname(__file__))
OUTPUT = os.path.join(HERE, "SnowLeopardDesktopCapture")
SYSTEM_VERSION = "/System/Library/CoreServices/SystemVersion.plist"
NOTICE = "Personal-use Snow Leopard capture; do not redistribute."
BOOT_FRAME_COUNT = 12


BOOT_CAPTURES = (
    (
        "boot/background.png",
        "Open a lossless recording of this same Mac OS X 10.6 system's real "
        "gray Apple-logo boot screen. Pause after the logo appears but before "
        "the spinner appears, then select only the complete boot framebuffer "
        "with no player or host UI.",
    ),
) + tuple(
    (
        "boot/spinner-%02d.png" % index,
        "Open a lossless recording of this same Mac OS X 10.6 system's real "
        "gray Apple-logo boot screen. Pause on spinner phase %02d of %02d, "
        "then select the same tight square around only the spinner and its "
        "native gray background. Do not reconstruct or substitute the phase."
        % (index + 1, BOOT_FRAME_COUNT),
    )
    for index in range(BOOT_FRAME_COUNT)
)


CAPTURES = BOOT_CAPTURES + (
    (
        "desktop/menubar.png",
        "Select the native menu-bar strip from the left Apple menu through the "
        "right edge. Include only the menu bar.",
    ),
    (
        "chrome/apple-menu.png",
        "Open the Apple menu, then select the entire open menu including its "
        "native border and shadow.",
    ),
    (
        "chrome/finder-context-menu.png",
        "Right-click a normal Finder item and select a clean native context "
        "menu region large enough to include three adjacent actions.",
    ),
    (
        "chrome/finder-window.png",
        "Select a complete Snow Leopard Finder window in icon view, including "
        "title bar, toolbar, sidebar, content, and status bar.",
    ),
    (
        "chrome/itunes-window.png",
        "Select a complete Snow Leopard iTunes window with source list, browser "
        "or track list, and bottom controls visible.",
    ),
    (
        "chrome/preferences-window.png",
        "Select the complete Snow Leopard System Preferences main window.",
    ),
    (
        "chrome/get-info-sheet.png",
        "Select a Finder Get Info window showing General information.",
    ),
    (
        "chrome/selection.png",
        "Select one clean native blue Finder list-selection row. Exclude text "
        "and icons when possible so Rockpod can overlay real Lucida text.",
    ),
    (
        "chrome/tooltip.png",
        "Show a native Snow Leopard yellow tooltip and select its complete "
        "rounded rectangle without the pointer cursor.",
    ),
    (
        "chrome/scrollbar-thumb.png",
        "Select one complete native Snow Leopard Finder vertical scrollbar "
        "thumb in its active state, excluding the track and arrow buttons.",
    ),
)


RESOURCE_CANDIDATES = {
    "desktop/aurora.jpg": (
        "/Library/Desktop Pictures/Nature/Aurora.jpg",
        "/Library/Desktop Pictures/Aurora.jpg",
    ),
    "fonts/LucidaGrande.ttc": (
        "/System/Library/Fonts/LucidaGrande.ttc",
    ),
    "desktop/dock.png": (
        "/System/Library/CoreServices/Dock.app/Contents/Resources/scurve-sm.png",
        "/System/Library/CoreServices/Dock.app/Contents/Resources/scurve-m.png",
        "/System/Library/CoreServices/Dock.app/Contents/Resources/scurve-l.png",
    ),
    "desktop/dock-indicator.png": (
        "/System/Library/CoreServices/Dock.app/Contents/Resources/indicator_small.png",
        "/System/Library/CoreServices/Dock.app/Contents/Resources/indicator_medium.png",
        "/System/Library/CoreServices/Dock.app/Contents/Resources/indicator_large.png",
    ),
    "icons/Finder.icns": (
        "/System/Library/CoreServices/Finder.app/Contents/Resources/Finder.icns",
    ),
    "icons/iTunes.icns": (
        "/Applications/iTunes.app/Contents/Resources/iTunes.icns",
    ),
    "icons/iPhoto.icns": (
        "/Applications/iPhoto.app/Contents/Resources/iPhoto.icns",
    ),
    "icons/Preview.icns": (
        "/Applications/Preview.app/Contents/Resources/Preview.icns",
    ),
    "icons/Edit.icns": (
        "/Applications/TextEdit.app/Contents/Resources/Edit.icns",
    ),
    "icons/Calculator.icns": (
        "/Applications/Calculator.app/Contents/Resources/Calculator.icns",
    ),
    "icons/PrefApp.icns": (
        "/Applications/System Preferences.app/Contents/Resources/PrefApp.icns",
    ),
    "icons/Dashboard.icns": (
        "/Applications/Dashboard.app/Contents/Resources/Dashboard.icns",
        "/System/Library/CoreServices/Dock.app/Contents/Resources/dashboard.icns",
    ),
    "icons/GenericFolderIcon.icns": (
        "/System/Library/CoreServices/CoreTypes.bundle/Contents/Resources/"
        "GenericFolderIcon.icns",
    ),
    "icons/GenericDocumentIcon.icns": (
        "/System/Library/CoreServices/CoreTypes.bundle/Contents/Resources/"
        "GenericDocumentIcon.icns",
    ),
    "icons/GenericHardDiskIcon.icns": (
        "/System/Library/CoreServices/CoreTypes.bundle/Contents/Resources/"
        "GenericHardDiskIcon.icns",
    ),
    "icons/TrashIcon.icns": (
        "/System/Library/CoreServices/CoreTypes.bundle/Contents/Resources/"
        "TrashIcon.icns",
    ),
    "icons/FullTrashIcon.icns": (
        "/System/Library/CoreServices/CoreTypes.bundle/Contents/Resources/"
        "FullTrashIcon.icns",
    ),
}


def _sha256(path):
    digest = hashlib.sha256()
    handle = open(path, "rb")
    try:
        while True:
            block = handle.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    finally:
        handle.close()
    return digest.hexdigest()


def _mkdir_parent(path):
    parent = os.path.dirname(path)
    if not os.path.isdir(parent):
        os.makedirs(parent)


def _read_system_version():
    if not os.path.isfile(SYSTEM_VERSION):
        raise RuntimeError("SystemVersion.plist is missing")
    info = plistlib.readPlist(SYSTEM_VERSION)
    version = str(info.get("ProductVersion") or "")
    if not version.startswith("10.6"):
        raise RuntimeError(
            "This helper only accepts Mac OS X 10.6; running system reports %s"
            % (version or "unknown")
        )
    return {
        "product_version": version,
        "product_build": str(info.get("ProductBuildVersion") or ""),
        "product_name": str(info.get("ProductName") or "Mac OS X"),
    }


def _find_first(candidates):
    for candidate in candidates:
        if os.path.isfile(candidate):
            return candidate
    return ""


def _copy_resources(records):
    for relative in sorted(RESOURCE_CANDIDATES):
        source = _find_first(RESOURCE_CANDIDATES[relative])
        if not source:
            raise RuntimeError(
                "Required installed Snow Leopard resource is missing for %s"
                % relative
            )
        destination = os.path.join(OUTPUT, relative)
        _mkdir_parent(destination)
        shutil.copy2(source, destination)
        records[relative] = {
            "method": "direct-copy",
            "source": source,
            "sha256": _sha256(destination),
        }


def _write_nsimage(image, destination):
    try:
        from AppKit import NSBitmapImageRep, NSPNGFileType
    except ImportError:
        from Cocoa import NSBitmapImageRep, NSPNGFileType
    tiff = image.TIFFRepresentation()
    representation = NSBitmapImageRep.imageRepWithData_(tiff)
    data = representation.representationUsingType_properties_(
        NSPNGFileType, None
    )
    _mkdir_parent(destination)
    if not data.writeToFile_atomically_(destination, True):
        raise RuntimeError("Could not write native cursor image: %s" % destination)


def _capture_cursors(records):
    try:
        from AppKit import NSCursor
    except ImportError:
        from Cocoa import NSCursor
    cursor_specs = (
        ("cursor/arrow.png", NSCursor.arrowCursor()),
        ("cursor/pointing-hand.png", NSCursor.pointingHandCursor()),
    )
    for relative, cursor in cursor_specs:
        destination = os.path.join(OUTPUT, relative)
        _write_nsimage(cursor.image(), destination)
        records[relative] = {
            "method": "NSCursor-image-export",
            "source": "AppKit NSCursor on running 10.6 system",
            "sha256": _sha256(destination),
        }


def _answer(prompt):
    try:
        return raw_input(prompt)
    except NameError:
        return input(prompt)


def _capture_regions(records):
    tool = "/usr/sbin/screencapture"
    if not os.path.isfile(tool):
        tool = "/usr/bin/screencapture"
    if not os.path.isfile(tool):
        raise RuntimeError("Snow Leopard screencapture tool is missing")
    for relative, instructions in CAPTURES:
        destination = os.path.join(OUTPUT, relative)
        _mkdir_parent(destination)
        while True:
            print("")
            print(instructions)
            answer = _answer(
                "Press Return to select the native region, or q to stop: "
            ).strip().lower()
            if answer == "q":
                raise RuntimeError("Capture cancelled before bundle completion")
            if os.path.exists(destination):
                os.unlink(destination)
            result = subprocess.call([tool, "-i", destination])
            if result != 0 or not os.path.isfile(destination):
                print("No capture was saved; try again.")
                continue
            review = _answer(
                "Saved %s. Keep it? [Y/r] " % relative
            ).strip().lower()
            if review not in ("r", "retry"):
                break
        records[relative] = {
            "method": "interactive-native-screencapture",
            "source": "running Mac OS X 10.6 UI",
            "sha256": _sha256(destination),
        }


def _write_manifest(version, records):
    manifest = {
        "format": 1,
        "capture_tool": "Rockpod Snow Leopard native capture",
        "product_name": version["product_name"],
        "product_version": version["product_version"],
        "product_build": version["product_build"],
        "captured_at_unix": int(time.time()),
        "personal_use_only": True,
        "notice": NOTICE,
        "asset_policy": (
            "Direct copies, native AppKit exports, and native 10.6 screen "
            "captures only; no traced, hand-drawn, or substitute artwork."
        ),
        "assets": records,
    }
    destination = os.path.join(OUTPUT, "capture-manifest.json")
    handle = open(destination, "wb")
    try:
        payload = json.dumps(manifest, indent=2, sort_keys=True) + "\n"
        handle.write(payload.encode("utf-8"))
    finally:
        handle.close()


def main():
    try:
        version = _read_system_version()
        print(
            "Verified %s %s (%s)"
            % (
                version["product_name"],
                version["product_version"],
                version["product_build"],
            )
        )
        print(NOTICE)
        if os.path.isdir(OUTPUT):
            answer = _answer(
                "Existing capture folder will be replaced. Continue? [y/N] "
            ).strip().lower()
            if answer not in ("y", "yes"):
                return 1
            shutil.rmtree(OUTPUT)
        os.makedirs(OUTPUT)
        records = {}
        _copy_resources(records)
        _capture_cursors(records)
        _capture_regions(records)
        _write_manifest(version, records)
        print("")
        print("Complete verified capture: %s" % OUTPUT)
        return 0
    except Exception as exc:
        print("ERROR: %s" % exc, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
