"""Build, validate, and install private Snow Leopard Desktop Mode assets.

The repository intentionally contains no Mac OS X artwork.  This module only
converts files supplied by the user from a personally owned Mac OS X 10.6
system, installer, or a capture bundle produced on that system.  Missing
assets are fatal: there is no procedural or modern-macOS fallback.
"""

from __future__ import annotations

import hashlib
import json
import os
import plistlib
import shutil
import struct
import tempfile
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable

from PIL import IcnsImagePlugin, Image

from services import snow_leopard_chrome, snow_leopard_fonts
from services.file_safety import atomic_write_json, atomic_write_text
from services.path_safety import resolve_under_root, validate_device_root


PROJECT_ROOT = Path(__file__).resolve().parents[2]
PRIVATE_ROOT = PROJECT_ROOT / "rockpod" / ".snow_leopard_desktop"
DEFAULT_PACK_DIR = PRIVATE_ROOT / "packs" / "ipod-320x240"
DEVICE_PACK_RELATIVE = ".rockbox/rocks/apps/desktop_mode_snow_leopard"
SIMULATOR_PACK_RELATIVE = ".rockbox/rocks.data/desktop_mode_snow_leopard"
DEVICE_PACK_RELATIVES = (DEVICE_PACK_RELATIVE, SIMULATOR_PACK_RELATIVE)
LEGACY_XP_PACK_RELATIVES = (
    ".rockbox/rocks/apps/desktop_mode_xp",
    ".rockbox/rocks.data/desktop_mode_xp",
)
PACK_FORMAT = 2
PACK_ID = "snow-leopard-10.6-ipod-320x240"
PERSONAL_USE_NOTICE = (
    "Personal-use Snow Leopard asset pack; do not redistribute."
)
OFFICIAL_COMBO_SHA256 = (
    "4a193634a7ce5f147e4844e5d477ffee06491930e89242632ae5c7babd1a045e"
)
MENU_PREVIEW_ID = "ipodjs.menu_preview"
MENU_PREVIEW_RELATIVE = "ipodjs/menu-preview.174x220x16.bmp"
MENU_PREVIEW_SIZE = (174, 220)
MENU_PREVIEW_OPERATION = "compose-authentic-idle-desktop-nine-icon-dock-and-crop-1to1"
MENU_PREVIEW_CROP = (0, 10, 174, 230)
MENU_PREVIEW_SITEKICK_ID = "bundled.sitekick.desktop_icon"
MENU_PREVIEW_SITEKICK_PATH = (
    PROJECT_ROOT
    / "assets"
    / "ipodjs"
    / "rockbox"
    / "sitekick"
    / "desktop"
    / "icon.32x32.rga"
)
MENU_PREVIEW_DOCK_ICONS = (
    "finder",
    "itunes",
    "preview",
    "textedit",
    "calculator",
    "directv",
    "sitekick",
    "system_preferences",
    "trash_empty",
)
MENU_PREVIEW_PACK_SOURCES = (
    "320x240.desktop.aurora",
    "320x240.desktop.menubar",
    "320x240.desktop.dock_shelf",
    *(
        f"320x240.icon.{name}"
        for name in MENU_PREVIEW_DOCK_ICONS
        if name != "sitekick"
    ),
)
MENU_PREVIEW_SOURCES = MENU_PREVIEW_PACK_SOURCES + (
    MENU_PREVIEW_SITEKICK_ID,
)


class SnowLeopardAssetError(RuntimeError):
    """Raised when a private asset pack cannot be safely prepared."""


@dataclass(frozen=True)
class AssetSpec:
    asset_id: str
    output: str
    size: tuple[int, int] | None
    source_candidates: tuple[str, ...]
    kind: str = "image"
    recipe: str = ""
    recipe_args: tuple = ()
    icon_frame: int = 0
    font_face: int = 0
    font_size: int = 11
    runtime_resident: bool = True


def _capture(*names: str) -> tuple[str, ...]:
    roots = (
        "",
        "SnowLeopardDesktopCapture",
        "RockpodSnowLeopardCapture",
        "desktop_mode_snow_leopard",
    )
    return tuple(
        str(Path(root) / name) if root else name
        for root in roots
        for name in names
    )


BOOT_FRAME_COUNT = 12

_DESKTOP_CAPTURE = _capture(
    "captures/desktop.png", "10-6-Snow-Leopard-Desktop.png"
)
_FINDER_CAPTURE = _capture(
    "captures/finder-home.png", "10-6-Snow-Leopard-Finder-Home.png"
)
_MENU_CAPTURE = _capture(
    "captures/apple-menu.png", "10-6-Snow-Leopard-Finder-Apple-Menu.png"
)
_ITUNES_CAPTURE = _capture(
    "captures/itunes.png", "10-6-Snow-Leopard-iTunes-v9.png"
)
_LUCIDA = _capture(
    "fonts/LucidaGrande.ttc", "System/Library/Fonts/LucidaGrande.ttc"
)

_CORE_TYPES = "System/Library/CoreServices/CoreTypes.bundle/Contents/Resources"

# logical icon id -> (icns candidates, native frame size)
_ICON_SOURCES = (
    ("finder", ("icons/Finder.icns",
                "System/Library/CoreServices/Finder.app/Contents/Resources/Finder.icns"), 32),
    ("itunes", ("icons/iTunes.icns",
                "Applications/iTunes.app/Contents/Resources/iTunes.icns"), 32),
    ("preview", ("icons/Preview.icns",
                 "Applications/Preview.app/Contents/Resources/preview.icns",
                 "Applications/Preview.app/Contents/Resources/Preview.icns"), 32),
    ("textedit", ("icons/Edit.icns",
                  "Applications/TextEdit.app/Contents/Resources/Edit.icns"), 32),
    ("calculator", ("icons/Calculator.icns",
                    "Applications/Calculator.app/Contents/Resources/Calculator.icns"), 32),
    # DIRECTV is the user-owned Live TV application artwork, not Apple
    # chrome. It is kept in the same private source bundle so every generated
    # Dock size has the same checksum/provenance guarantees as the OS icons.
    ("directv", ("icons/DirectTV.png",), 32),
    ("system_preferences", ("icons/PrefApp.icns",
                            "Applications/System Preferences.app/Contents/Resources/PrefApp.icns"), 32),
    ("dashboard", ("icons/Dashboard.icns",
                   "Applications/Dashboard.app/Contents/Resources/Dashboard.icns"), 32),
    ("disk", ("icons/GenericHardDiskIcon.icns",
              f"{_CORE_TYPES}/GenericHardDiskIcon.icns"), 32),
    ("trash_empty", ("icons/TrashIcon.icns", f"{_CORE_TYPES}/TrashIcon.icns"), 32),
    ("folder_desktop", ("icons/GenericFolderIcon.icns",
                        f"{_CORE_TYPES}/GenericFolderIcon.icns"), 32),
    ("folder", ("icons/GenericFolderIcon.icns",
                f"{_CORE_TYPES}/GenericFolderIcon.icns"), 16),
    ("document", ("icons/GenericDocumentIcon.icns",
                  f"{_CORE_TYPES}/GenericDocumentIcon.icns"), 16),
)

# Dock magnification variants: reduced from Apple's 128-pixel frame so the
# intermediate sizes keep real detail instead of being blown up from 32.
_DOCK_MAGNIFIED = ("finder", "itunes", "preview", "textedit", "calculator",
                   "directv",
                   "system_preferences", "trash_empty")

_ICON_CANDIDATES = {name: candidates for name, candidates, _ in _ICON_SOURCES}


# --------------------------------------------------------------------------
# Panel profiles
#
# The same 1:1 Snow Leopard cuts serve both panels.  At 320x240 the artwork
# has to be used in fragments; at 1920x1080 it is used at the size Apple drew
# it, so a window is a real window and the Dock carries real 64-pixel icons.
# Each profile lives in its own subdirectory of the pack so one install can
# feed the iPod and the 1080p host panel.
# --------------------------------------------------------------------------


@dataclass(frozen=True)
class PanelProfile:
    key: str
    width: int
    height: int
    menubar_h: int
    dock_shelf_w: int
    dock_scale: int
    dock_icon: int
    window_w: int
    window_h: int
    sidebar_w: int
    itunes_w: int
    itunes_h: int
    itunes_source_w: int
    list_selection_w: int
    row_h: int
    menu_w: int
    menu_h: int
    context_w: int
    context_h: int
    sheet_w: int
    sheet_h: int
    tooltip_w: int
    tooltip_h: int
    scroller_h: int
    thumb_h: int
    icon_frame: int


PANEL_PROFILES = (
    PanelProfile(
        key="320x240", width=320, height=240, menubar_h=21,
        dock_shelf_w=288, dock_scale=2, dock_icon=32,
        window_w=304, window_h=174, sidebar_w=86,
        itunes_w=304, itunes_h=174, itunes_source_w=86,
        list_selection_w=217, row_h=19,
        menu_w=152, menu_h=124, context_w=124, context_h=71,
        sheet_w=240, sheet_h=112, tooltip_w=104, tooltip_h=18,
        scroller_h=97, thumb_h=36, icon_frame=32,
    ),
    PanelProfile(
        key="1920x1080", width=1920, height=1080, menubar_h=22,
        dock_shelf_w=608, dock_scale=1, dock_icon=64,
        window_w=897, window_h=671, sidebar_w=135,
        itunes_w=1081, itunes_h=655, itunes_source_w=206,
        list_selection_w=761, row_h=19,
        menu_w=226, menu_h=279, context_w=200, context_h=90,
        sheet_w=520, sheet_h=260, tooltip_w=180, tooltip_h=22,
        scroller_h=430, thumb_h=80, icon_frame=128,
    ),
)


def _profile_specs(profile: PanelProfile) -> tuple:
    """Every panel-dependent asset, named under the profile's directory."""
    root = profile.key
    icon = profile.icon_frame

    def image(asset_id, name, size, sources, **kwargs):
        return AssetSpec(
            f"{root}.{asset_id}", f"{root}/{name}", size, sources, **kwargs
        )

    specs = (
        image("boot.background",
              f"boot/background.{profile.width}x{profile.height}x16.bmp",
              (profile.width, profile.height),
              _capture("boot/background.png"),
              runtime_resident=False),
        image("desktop.aurora",
              f"desktop/aurora.{profile.width}x{profile.height}x16.bmp",
              (profile.width, profile.height),
              _capture("Library/Desktop Pictures/Nature/Aurora.jpg",
                       "Library/Desktop Pictures/Aurora.jpg",
                       "desktop/aurora.png"),
              kind="derived", recipe="compose_wallpaper",
              recipe_args=(profile.width, profile.height)),
        image("desktop.menubar",
              f"desktop/menubar.{profile.width}x{profile.menubar_h}x16.bmp",
              (profile.width, profile.menubar_h), _DESKTOP_CAPTURE,
              kind="derived", recipe="compose_menubar",
              recipe_args=(profile.width, profile.menubar_h)),
        image("desktop.apple_highlight",
              f"desktop/apple-highlight.22x{profile.menubar_h}x16.bmp",
              (22, profile.menubar_h), _MENU_CAPTURE,
              kind="derived", recipe="compose_apple_highlight",
              recipe_args=(profile.menubar_h,)),
        image("desktop.dock_shelf",
              f"desktop/dock-shelf.{profile.dock_shelf_w}x"
              f"{52 // profile.dock_scale}x16.bmp",
              (profile.dock_shelf_w, 52 // profile.dock_scale),
              _DESKTOP_CAPTURE, kind="derived", recipe="compose_dock",
              recipe_args=(profile.dock_shelf_w, profile.dock_scale)),
        image("chrome.window_sidebar",
              f"chrome/window-sidebar.{profile.window_w}x{profile.window_h}"
              "x16.bmp",
              (profile.window_w, profile.window_h), _FINDER_CAPTURE,
              kind="derived", recipe="compose_window",
              recipe_args=(True, profile.window_w, profile.window_h,
                           profile.sidebar_w)),
        image("chrome.window_plain",
              f"chrome/window-plain.{profile.window_w}x{profile.window_h}"
              "x16.bmp",
              (profile.window_w, profile.window_h), _FINDER_CAPTURE,
              kind="derived", recipe="compose_window",
              recipe_args=(False, profile.window_w, profile.window_h,
                           profile.sidebar_w)),
        image("chrome.title_bar",
              f"chrome/title-bar.{profile.window_w}x24x16.bmp",
              (profile.window_w, 24), _FINDER_CAPTURE,
              kind="derived", recipe="compose_title_bar",
              recipe_args=(profile.window_w,)),
        image("chrome.itunes_window",
              f"chrome/itunes-window.{profile.itunes_w}x{profile.itunes_h}"
              "x16.bmp",
              (profile.itunes_w, profile.itunes_h), _ITUNES_CAPTURE,
              kind="derived", recipe="compose_itunes_window",
              recipe_args=(profile.itunes_w, profile.itunes_h,
                           profile.itunes_source_w)),
        image("chrome.itunes_selection",
              f"chrome/itunes-selection.{profile.itunes_w}x17x16.bmp",
              (profile.itunes_w, 17), _ITUNES_CAPTURE, kind="derived",
              recipe="compose_itunes_selection",
              recipe_args=(profile.itunes_w,)),
        image("chrome.list_selection",
              f"chrome/list-selection.{profile.list_selection_w}x19x16.bmp",
              (profile.list_selection_w, 19), _FINDER_CAPTURE,
              kind="derived", recipe="compose_selection",
              recipe_args=(profile.list_selection_w,)),
        image("chrome.sidebar_selection",
              f"chrome/sidebar-selection.{profile.sidebar_w}x19x16.bmp",
              (profile.sidebar_w, 19), _FINDER_CAPTURE, kind="derived",
              recipe="compose_selection", recipe_args=(profile.sidebar_w,)),
        image("chrome.menu_panel",
              f"chrome/menu-panel.{profile.menu_w}x{profile.menu_h}x16.bmp",
              (profile.menu_w, profile.menu_h), _MENU_CAPTURE,
              kind="derived", recipe="compose_menu_panel",
              recipe_args=(profile.menu_w, profile.menu_h)),
        image("chrome.menu_selection",
              f"chrome/menu-selection.{profile.menu_w - 2}x19x16.bmp",
              (profile.menu_w - 2, 19), _MENU_CAPTURE, kind="derived",
              recipe="compose_menu_selection",
              recipe_args=(profile.menu_w - 2,)),
        image("chrome.context_panel",
              f"chrome/context-panel.{profile.context_w}x{profile.context_h}"
              "x16.bmp",
              (profile.context_w, profile.context_h), _MENU_CAPTURE,
              kind="derived", recipe="compose_menu_panel",
              recipe_args=(profile.context_w, profile.context_h)),
        image("chrome.sheet",
              f"chrome/sheet.{profile.sheet_w}x{profile.sheet_h}x16.bmp",
              (profile.sheet_w, profile.sheet_h), _MENU_CAPTURE,
              kind="derived", recipe="compose_menu_panel",
              recipe_args=(profile.sheet_w, profile.sheet_h)),
        image("chrome.tooltip",
              f"chrome/tooltip.{profile.tooltip_w}x{profile.tooltip_h}"
              "x16.bmp",
              (profile.tooltip_w, profile.tooltip_h), _MENU_CAPTURE,
              kind="derived", recipe="compose_menu_panel",
              recipe_args=(profile.tooltip_w, profile.tooltip_h, 4)),
        image("chrome.scroller_track",
              f"chrome/scroller-track.16x{profile.scroller_h}x16.bmp",
              (16, profile.scroller_h), _ITUNES_CAPTURE, kind="derived",
              recipe="compose_scroller_track",
              recipe_args=(profile.scroller_h,)),
        image("chrome.scroller_thumb",
              f"chrome/scroller-thumb.16x{profile.thumb_h}x16.bmp",
              (16, profile.thumb_h), _ITUNES_CAPTURE, kind="derived",
              recipe="compose_scroller_thumb",
              recipe_args=(profile.thumb_h,)),
    )
    specs += tuple(
        AssetSpec(
            f"{root}.icon.{name}",
            f"{root}/icons/{name.replace('_', '-')}."
            f"{profile.dock_icon}x{profile.dock_icon}.rga",
            (profile.dock_icon, profile.dock_icon),
            _capture(*candidates), kind="icon", icon_frame=icon,
        )
        for name, candidates, _ in _ICON_SOURCES
        if name not in {"folder", "document"}
    )
    # Finder list rows are 19 pixels with 16-pixel icons at 1:1 on either
    # panel: that is the real metric, not a fraction of the Dock size.
    small = 16
    specs += tuple(
        AssetSpec(
            f"{root}.icon.{name}",
            f"{root}/icons/{name}.{small}x{small}.rga",
            (small, small),
            _capture(*_ICON_CANDIDATES[name]),
            kind="icon", icon_frame=max(32, small),
        )
        for name in ("folder", "document")
    )
    specs += tuple(
        AssetSpec(
            f"{root}.icon.{name}_dock_{size}",
            f"{root}/icons/{name.replace('_', '-')}-dock-{size}."
            f"{size}x{size}.rga",
            (size, size), _capture(*_ICON_CANDIDATES[name]),
            kind="icon", icon_frame=128,
        )
        for name in _DOCK_MAGNIFIED
        for size in (profile.dock_icon + 2, profile.dock_icon + 6)
    )
    return specs


# Panel-independent assets: the boot sequence, the cursors, the Dock's running
# indicator, and the Lucida Grande atlases, which are the same real UI sizes on
# either panel.
ASSET_SPECS = tuple(
    AssetSpec(
        f"boot.spinner_{index:02d}",
        f"boot/spinner-{index:02d}.24x24x16.bmp",
        (24, 24),
        _capture(f"boot/spinner-{index:02d}.png"),
        runtime_resident=False,
    )
    for index in range(BOOT_FRAME_COUNT)
) + (
    AssetSpec(
        "desktop.dock_indicator",
        "desktop/dock-indicator.14x8.rga",
        (14, 8),
        _capture(
            "desktop/dock-indicator.png",
            "System/Library/CoreServices/Dock.app/Contents/Resources/indicator_small.png",
        ),
        kind="alpha_image",
    ),
    AssetSpec(
        "cursor.arrow",
        "cursor/arrow.14x20.rga",
        (14, 20),
        _capture("cursor/arrow.png", "cursor/arrow.tiff"),
        kind="alpha_image",
    ),
    AssetSpec(
        "cursor.pointing_hand",
        "cursor/pointing-hand.16x18.rga",
        (16, 18),
        _capture("cursor/pointing-hand.png", "cursor/pointing-hand.tiff"),
        kind="alpha_image",
    ),
    AssetSpec(
        "font.lucida_11",
        "fonts/lucida-grande-11.alpha",
        None,
        _LUCIDA,
        kind="font",
        font_face=0,
        font_size=11,
    ),
    AssetSpec(
        "font.lucida_bold_11",
        "fonts/lucida-grande-bold-11.alpha",
        None,
        _LUCIDA,
        kind="font",
        font_face=1,
        font_size=11,
    ),
    AssetSpec(
        "font.lucida_9",
        "fonts/lucida-grande-9.alpha",
        None,
        _LUCIDA,
        kind="font",
        font_face=0,
        font_size=9,
    ),
)

for _profile in PANEL_PROFILES:
    ASSET_SPECS += _profile_specs(_profile)


def sha256(path: str | os.PathLike[str]) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def _normalize_sources(source_roots: Iterable[str | os.PathLike[str]]) -> list[Path]:
    result = []
    for raw in source_roots:
        path = Path(raw).expanduser().resolve()
        if not path.exists():
            raise SnowLeopardAssetError(f"Snow Leopard source does not exist: {path}")
        if path not in result:
            result.append(path)
    if not result:
        raise SnowLeopardAssetError("At least one owned Snow Leopard source is required")
    return result


def detect_source_version(source_roots: Iterable[str | os.PathLike[str]]) -> dict:
    roots = _normalize_sources(source_roots)
    versions = []
    for root in roots:
        candidates = (
            root / "System/Library/CoreServices/SystemVersion.plist",
            root / "SystemVersion.plist",
            root / "version.plist",
        )
        for candidate in candidates:
            if not candidate.is_file():
                continue
            try:
                with candidate.open("rb") as handle:
                    data = plistlib.load(handle)
            except (OSError, plistlib.InvalidFileException):
                continue
            version = str(data.get("ProductVersion") or data.get("CFBundleShortVersionString") or "")
            if version:
                versions.append(
                    {
                        "root": str(root),
                        "version": version,
                        "path": str(candidate.relative_to(root)),
                    }
                )
                break
    is_snow_leopard = any(item["version"].startswith("10.6") for item in versions)
    capture_markers = [
        root / "SnowLeopardDesktopCapture/capture-manifest.json"
        for root in roots
    ] + [
        root / "capture-manifest.json"
        for root in roots
    ]
    capture_version = ""
    for marker in capture_markers:
        if not marker.is_file():
            continue
        try:
            data = json.loads(marker.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        capture_version = str(data.get("product_version") or "")
        if capture_version.startswith("10.6"):
            is_snow_leopard = True
            break
    official_combo = []
    for root in roots:
        if root.is_file() and root.suffix.lower() == ".dmg":
            digest = sha256(root)
            if digest == OFFICIAL_COMBO_SHA256:
                official_combo.append(str(root))
    return {
        "snow_leopard": is_snow_leopard,
        "versions": versions,
        "capture_version": capture_version,
        "official_combo_sources": official_combo,
        "roots": [str(root) for root in roots],
    }


def _find_candidate(roots: list[Path], candidates: tuple[str, ...]) -> Path | None:
    for root in roots:
        if root.is_file():
            continue
        for relative in candidates:
            candidate = root / relative
            if candidate.is_file():
                return candidate
    return None


def _capture_manifests(roots: list[Path]) -> list[tuple[Path, dict]]:
    result = []
    for root in roots:
        if root.is_file():
            continue
        for capture_root in (root, root / "SnowLeopardDesktopCapture"):
            marker = capture_root / "capture-manifest.json"
            if not marker.is_file():
                continue
            try:
                data = json.loads(marker.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                continue
            result.append((capture_root.resolve(), data))
    return result


def _capture_integrity_errors(
    roots: list[Path],
    resolved: dict[str, str],
    version: dict,
) -> list[str]:
    errors = []
    manifests = _capture_manifests(roots)
    proven_roots = [
        Path(item["root"]).resolve()
        for item in version.get("versions") or []
        if str(item.get("version") or "").startswith("10.6")
    ]
    proven_roots.extend(
        capture_root
        for capture_root, manifest in manifests
        if str(manifest.get("product_version") or "").startswith("10.6")
    )
    for asset_id, raw_path in resolved.items():
        source = Path(raw_path).resolve()
        if not any(
            source == proven or source.is_relative_to(proven)
            for proven in proven_roots
        ):
            errors.append(
                f"{asset_id}: source is outside every proven Mac OS X 10.6 root"
            )
    for capture_root, manifest in manifests:
        records = manifest.get("assets")
        if not isinstance(records, dict):
            continue
        for asset_id, raw_path in resolved.items():
            source = Path(raw_path).resolve()
            try:
                relative = str(source.relative_to(capture_root))
            except ValueError:
                continue
            record = records.get(relative)
            if not isinstance(record, dict):
                errors.append(
                    f"{asset_id}: capture manifest has no record for {relative}"
                )
                continue
            expected = str(record.get("sha256") or "")
            if not expected or sha256(source) != expected:
                errors.append(
                    f"{asset_id}: capture checksum mismatch for {relative}"
                )
    return sorted(set(errors))


def discover_assets(source_roots: Iterable[str | os.PathLike[str]]) -> dict:
    roots = _normalize_sources(source_roots)
    version = detect_source_version(roots)
    resolved = {}
    missing = []
    for spec in ASSET_SPECS:
        source = _find_candidate(roots, spec.source_candidates)
        if source is None:
            missing.append(spec.asset_id)
        else:
            resolved[spec.asset_id] = str(source)
    integrity_errors = _capture_integrity_errors(roots, resolved, version)
    return {
        "version": version,
        "resolved": resolved,
        "missing": missing,
        "complete": (
            not missing
            and not integrity_errors
            and bool(version["snow_leopard"])
        ),
        "integrity_errors": integrity_errors,
        "required_count": len(ASSET_SPECS),
        "resolved_count": len(resolved),
    }


def _icns_frame(source: Path, frame: int) -> Image.Image:
    """Read one exact frame out of a real .icns.

    Pillow's ordinary image interface only ever exposes an .icns file's largest
    frame, so asking it for a 32x32 icon silently reduced Apple's 512x512
    artwork instead of using the 32x32 variant Apple hand-tuned.  Reading the
    icns directory directly gives the real small icon.
    """
    if source.suffix.lower() != ".icns":
        with Image.open(source) as opened:
            image = opened.convert("RGBA")
        if image.size != (frame, frame):
            image = image.resize((frame, frame), Image.Resampling.LANCZOS)
        return image
    with source.open("rb") as handle:
        icns = IcnsImagePlugin.IcnsFile(handle)
        sizes = sorted({size[0] for size in icns.itersizes()})
        exact = frame if frame in sizes else None
        chosen = exact or min(
            (size for size in sizes if size >= frame), default=max(sizes)
        )
        image = icns.getimage((chosen, chosen, 1)).convert("RGBA")
    if image.size != (frame, frame):
        image = image.resize((frame, frame), Image.Resampling.LANCZOS)
    return image


def _save_rga(image: Image.Image, destination: Path) -> dict:
    """Write RGB565 colour plus 8-bit coverage.

    Desktop Mode composites icons and cursors over real chrome, so it needs the
    coverage Apple drew rather than a one-bit magenta key, whose hard edge is
    what made the old Dock icons look cut out.
    """
    rgba = image.convert("RGBA")
    width, height = rgba.size
    pixels = rgba.load()
    payload = bytearray(struct.pack("<4sHH", b"RGA1", width, height))
    for y in range(height):
        for x in range(width):
            red, green, blue, alpha = pixels[x, y]
            value = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
            payload.extend(struct.pack("<HB", value, alpha))
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(bytes(payload))
    return {
        "operation": "rgb565-plus-coverage",
        "output_size": [width, height],
        "output_depth": 16,
        "alpha": "8-bit-coverage",
    }


def _load_rga(path: Path) -> Image.Image:
    """Read the runtime's RGB565-plus-coverage format for derived assets."""
    try:
        payload = path.read_bytes()
    except OSError as exc:
        raise SnowLeopardAssetError(
            f"Could not read authentic Desktop Mode coverage image: {exc}"
        ) from exc
    if len(payload) < 8:
        raise SnowLeopardAssetError(
            f"Truncated Desktop Mode coverage image: {path}"
        )
    magic, width, height = struct.unpack_from("<4sHH", payload)
    expected = 8 + width * height * 3
    if magic != b"RGA1" or width <= 0 or height <= 0 or len(payload) != expected:
        raise SnowLeopardAssetError(
            f"Invalid Desktop Mode coverage image: {path}"
        )

    pixels = []
    offset = 8
    for _ in range(width * height):
        value, alpha = struct.unpack_from("<HB", payload, offset)
        offset += 3
        pixels.append(
            (
                ((value >> 11) & 0x1F) * 255 // 31,
                ((value >> 5) & 0x3F) * 255 // 63,
                (value & 0x1F) * 255 // 31,
                alpha,
            )
        )
    image = Image.new("RGBA", (width, height))
    image.putdata(pixels)
    return image


def _flatten_rgba(image: Image.Image) -> Image.Image:
    rgba = image.convert("RGBA")
    output = Image.new("RGB", rgba.size, (255, 0, 255))
    opaque = rgba.getchannel("A").point(lambda alpha: 255 if alpha >= 128 else 0)
    output.paste(rgba.convert("RGB"), mask=opaque)
    return output


def _save_rgb565_bmp(image: Image.Image, destination: Path) -> None:
    """Write an uncompressed 16-bit RGB565 BMP accepted by Rockbox's decoder."""
    rgb = image.convert("RGB")
    width, height = rgb.size
    row_bytes = ((width * 2 + 3) // 4) * 4
    image_bytes = row_bytes * height
    pixel_offset = 14 + 40 + 12
    file_bytes = pixel_offset + image_bytes
    header = struct.pack(
        "<2sIHHI",
        b"BM",
        file_bytes,
        0,
        0,
        pixel_offset,
    )
    dib = struct.pack(
        "<IiiHHIIiiII",
        40,
        width,
        height,
        1,
        16,
        3,  # BI_BITFIELDS
        image_bytes,
        3780,
        3780,
        3,
        3,
    )
    masks = struct.pack("<III", 0xF800, 0x07E0, 0x001F)
    padding = b"\0" * (row_bytes - width * 2)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("wb") as handle:
        handle.write(header)
        handle.write(dib)
        handle.write(masks)
        pixels = rgb.load()
        for y in range(height - 1, -1, -1):
            row = bytearray()
            for x in range(width):
                red, green, blue = pixels[x, y]
                value = (
                    ((red & 0xF8) << 8)
                    | ((green & 0xFC) << 3)
                    | (blue >> 3)
                )
                row.extend(struct.pack("<H", value))
            handle.write(row)
            handle.write(padding)


def _build_menu_preview(root: Path, assets: dict) -> dict:
    """Compose an idle 1:1 desktop pane from verified Apple pixels."""
    paths = {}
    source_hashes = []
    for asset_id in MENU_PREVIEW_PACK_SOURCES:
        item = assets.get(asset_id)
        if not isinstance(item, dict):
            raise SnowLeopardAssetError(
                f"Cannot build Desktop Mode menu preview without {asset_id}"
            )
        path = (root / str(item.get("path") or "")).resolve()
        try:
            path.relative_to(root)
        except ValueError as exc:
            raise SnowLeopardAssetError(
                f"Unsafe Desktop Mode menu preview source: {path}"
            ) from exc
        paths[asset_id] = path
        source_hashes.append(str(item.get("output_sha256") or sha256(path)))

    if not MENU_PREVIEW_SITEKICK_PATH.is_file():
        raise SnowLeopardAssetError(
            "Cannot build Desktop Mode menu preview without the bundled "
            "Sitekick Dock icon"
        )
    source_hashes.append(sha256(MENU_PREVIEW_SITEKICK_PATH))

    try:
        with Image.open(paths["320x240.desktop.aurora"]) as opened:
            canvas = opened.convert("RGB")
        with Image.open(paths["320x240.desktop.menubar"]) as opened:
            menubar = opened.convert("RGB")
        with Image.open(paths["320x240.desktop.dock_shelf"]) as opened:
            dock = opened.convert("RGB")
    except OSError as exc:
        raise SnowLeopardAssetError(
            f"Could not open authentic Desktop Mode menu-preview source: {exc}"
        ) from exc

    if canvas.size != (320, 240):
        raise SnowLeopardAssetError(
            "Desktop Mode menu preview requires the verified 320x240 panel"
        )

    # Show the idle desktop rather than a clipped Finder window. Match
    # dm_draw_dock() exactly: a centered shelf, nine 32-pixel icons, and a
    # 34-pixel step. No synthetic icon, text, gradient, or frame is introduced.
    canvas.paste(menubar, (0, 0))
    canvas.paste(dock, ((320 - dock.width) // 2, 240 - dock.height))
    dock_step = 34
    dock_left = (
        (320 - len(MENU_PREVIEW_DOCK_ICONS) * dock_step) // 2
        + (dock_step - 32) // 2
    )
    dock_y = 240 - dock.height - 16
    for index, name in enumerate(MENU_PREVIEW_DOCK_ICONS):
        if name == "sitekick":
            icon = _load_rga(MENU_PREVIEW_SITEKICK_PATH)
        else:
            icon = _load_rga(paths[f"320x240.icon.{name}"])
        if icon.size != (32, 32):
            raise SnowLeopardAssetError(
                f"Desktop Mode menu preview icon has wrong size: {name}"
            )
        canvas.paste(icon, (dock_left + index * dock_step, dock_y), icon)

    preview = canvas.crop(MENU_PREVIEW_CROP)
    destination = root / MENU_PREVIEW_RELATIVE
    _save_rgb565_bmp(preview, destination)

    source_digest = hashlib.sha256(
        "\n".join(source_hashes).encode("ascii")
    ).hexdigest()
    return {
        "path": MENU_PREVIEW_RELATIVE,
        "source_root": "verified-private-pack",
        "source_path": ",".join(MENU_PREVIEW_SOURCES),
        "source_sha256": source_digest,
        "source_asset_ids": list(MENU_PREVIEW_SOURCES),
        "output_sha256": sha256(destination),
        "output_bytes": destination.stat().st_size,
        "operation": MENU_PREVIEW_OPERATION,
        "output_size": list(MENU_PREVIEW_SIZE),
        "output_depth": 16,
        "output_compression": "BI_BITFIELDS-RGB565",
        "alpha": "opaque",
    }


def _ensure_menu_preview(root: Path) -> None:
    """Add or repair the optional private preview in an existing valid pack."""
    manifest_path = root / "manifest.json"
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise SnowLeopardAssetError(
            f"Cannot prepare Desktop Mode menu preview: {exc}"
        ) from exc
    assets = manifest.get("assets")
    if not isinstance(assets, dict):
        raise SnowLeopardAssetError(
            "Cannot prepare Desktop Mode menu preview without pack assets"
        )

    item = assets.get(MENU_PREVIEW_ID)
    candidate = root / MENU_PREVIEW_RELATIVE
    if (
        isinstance(item, dict)
        and candidate.is_file()
        and sha256(candidate) == item.get("output_sha256")
        and item.get("operation") == MENU_PREVIEW_OPERATION
        and item.get("source_asset_ids") == list(MENU_PREVIEW_SOURCES)
    ):
        return

    assets[MENU_PREVIEW_ID] = _build_menu_preview(root, assets)
    atomic_write_json(manifest_path, manifest)
    provenance = root / "PROVENANCE.txt"
    line = (
        f"{MENU_PREVIEW_ID}\t{MENU_PREVIEW_RELATIVE}\t"
        f"{assets[MENU_PREVIEW_ID]['output_sha256']}\t"
        f"{assets[MENU_PREVIEW_ID]['source_path']}\n"
    )
    try:
        existing = provenance.read_text(encoding="utf-8")
    except OSError:
        existing = ""
    if MENU_PREVIEW_ID not in existing:
        atomic_write_text(provenance, existing.rstrip() + "\n" + line)


def _bmp_header(path: Path) -> dict:
    with path.open("rb") as handle:
        header = handle.read(54)
    if len(header) != 54 or header[:2] != b"BM":
        raise SnowLeopardAssetError(f"Not a Windows BMP: {path}")
    width, height = struct.unpack_from("<ii", header, 18)
    depth = struct.unpack_from("<H", header, 28)[0]
    compression = struct.unpack_from("<I", header, 30)[0]
    return {
        "width": width,
        "height": abs(height),
        "depth": depth,
        "compression": compression,
    }


def _convert_image(source: Path, destination: Path, size: tuple[int, int]) -> dict:
    try:
        with Image.open(source) as opened:
            image = opened.convert("RGBA")
            source_size = [image.width, image.height]
            if image.size != size:
                image = image.resize(size, Image.Resampling.LANCZOS)
            _save_rgb565_bmp(_flatten_rgba(image), destination)
    except (OSError, ValueError) as exc:
        raise SnowLeopardAssetError(f"Could not convert real asset {source}: {exc}") from exc
    return {
        "operation": "format-convert" if source_size == list(size) else "lanczos-resize-and-format-convert",
        "source_size": source_size,
        "output_size": list(size),
        "output_depth": 16,
        "output_compression": "BI_BITFIELDS-RGB565",
        "alpha": "opaque",
    }


def _convert_alpha_image(source: Path, destination: Path, size: tuple[int, int]) -> dict:
    try:
        with Image.open(source) as opened:
            image = opened.convert("RGBA")
            source_size = [image.width, image.height]
            if image.size != size:
                image = image.resize(size, Image.Resampling.LANCZOS)
    except (OSError, ValueError) as exc:
        raise SnowLeopardAssetError(f"Could not convert real asset {source}: {exc}") from exc
    record = _save_rga(image, destination)
    record["source_size"] = source_size
    return record


def _convert_icon(source: Path, destination: Path, spec: AssetSpec) -> dict:
    if not spec.size:
        raise SnowLeopardAssetError(f"Icon geometry is missing for {spec.asset_id}")
    try:
        image = _icns_frame(source, spec.icon_frame)
    except (OSError, ValueError, SyntaxError) as exc:
        raise SnowLeopardAssetError(f"Could not read real icon {source}: {exc}") from exc
    if image.size != spec.size:
        image = image.resize(spec.size, Image.Resampling.LANCZOS)
    record = _save_rga(image, destination)
    record["operation"] = "icns-native-frame"
    record["icns_frame"] = spec.icon_frame
    return record


def _convert_derived(source: Path, destination: Path, spec: AssetSpec) -> dict:
    recipe = getattr(snow_leopard_chrome, spec.recipe, None)
    if recipe is None:
        raise SnowLeopardAssetError(f"Unknown chrome recipe {spec.recipe!r}")
    try:
        with Image.open(source) as opened:
            capture = opened.convert("RGB" if spec.kind == "derived" else "RGBA")
            source_size = [capture.width, capture.height]
            image = recipe(capture, *spec.recipe_args)
    except (OSError, ValueError) as exc:
        raise SnowLeopardAssetError(
            f"Could not compose {spec.asset_id} from {source}: {exc}"
        ) from exc
    if spec.size and image.size != spec.size:
        raise SnowLeopardAssetError(
            f"{spec.asset_id}: recipe produced {image.width}x{image.height}, "
            f"expected {spec.size[0]}x{spec.size[1]}"
        )
    if spec.kind == "derived_alpha":
        record = _save_rga(image, destination)
    else:
        _save_rgb565_bmp(image, destination)
        record = {
            "output_size": list(image.size),
            "output_depth": 16,
            "output_compression": "BI_BITFIELDS-RGB565",
            "alpha": "opaque",
        }
    record["operation"] = f"compose-1to1:{spec.recipe}"
    record["recipe_args"] = list(spec.recipe_args)
    record["source_size"] = source_size
    return record


def _convert_font(source: Path, destination: Path, spec: AssetSpec) -> dict:
    try:
        atlas = snow_leopard_fonts.build_atlas(
            source, size=spec.font_size, face=spec.font_face
        )
    except OSError as exc:
        raise SnowLeopardAssetError(
            f"Could not open Snow Leopard Lucida Grande face {spec.font_face}: {exc}"
        ) from exc
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(atlas.coverage)
    metrics = destination.with_suffix(".metrics")
    metrics.write_bytes(atlas.metrics)
    return {
        "operation": "lucida-grande-baseline-atlas",
        "font_size": spec.font_size,
        "font_face": spec.font_face,
        "cell_size": [atlas.cell_w, atlas.cell_h],
        "ascent": atlas.ascent,
        "output_size": [atlas.width, atlas.height],
        "output_depth": 8,
        "alpha": "8-bit-coverage",
        "metrics_path": str(metrics.name),
        "metrics_sha256": sha256(metrics),
        "metrics_bytes": len(atlas.metrics),
    }


def _convert(source: Path, destination: Path, spec: AssetSpec) -> dict:
    if spec.kind == "font":
        return _convert_font(source, destination, spec)
    if spec.kind == "icon":
        return _convert_icon(source, destination, spec)
    if spec.kind in {"derived", "derived_alpha"}:
        return _convert_derived(source, destination, spec)
    if spec.kind == "alpha_image":
        return _convert_alpha_image(source, destination, spec.size)
    return _convert_image(source, destination, spec.size)


def build_pack(
    source_roots: Iterable[str | os.PathLike[str]],
    output_dir: str | os.PathLike[str] = DEFAULT_PACK_DIR,
) -> dict:
    roots = _normalize_sources(source_roots)
    discovery = discover_assets(roots)
    if not discovery["version"]["snow_leopard"]:
        raise SnowLeopardAssetError(
            "No source proves Mac OS X 10.6 provenance; refusing modern or unknown assets"
        )
    if discovery["missing"]:
        raise SnowLeopardAssetError(
            "Owned Snow Leopard source is incomplete; missing real assets: "
            + ", ".join(discovery["missing"])
        )
    if discovery["integrity_errors"]:
        raise SnowLeopardAssetError(
            "Snow Leopard capture integrity check failed: "
            + "; ".join(discovery["integrity_errors"])
        )

    output = Path(output_dir).expanduser().resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="snow-leopard-pack-", dir=output.parent) as temporary:
        stage = Path(temporary) / "pack"
        assets = {}
        for spec in ASSET_SPECS:
            source = Path(discovery["resolved"][spec.asset_id])
            destination = stage / spec.output
            conversion = _convert(source, destination, spec)
            assets[spec.asset_id] = {
                "path": spec.output,
                "source_root": next(
                    str(root)
                    for root in roots
                    if not root.is_file() and source.is_relative_to(root)
                ),
                "source_path": next(
                    str(source.relative_to(root))
                    for root in roots
                    if not root.is_file() and source.is_relative_to(root)
                ),
                "source_sha256": sha256(source),
                "output_sha256": sha256(destination),
                "output_bytes": destination.stat().st_size,
                **conversion,
            }

        assets[MENU_PREVIEW_ID] = _build_menu_preview(stage, assets)
        manifest = {
            "format": PACK_FORMAT,
            "pack_id": PACK_ID,
            "product": "Mac OS X Snow Leopard Desktop Mode",
            "product_version": (
                discovery["version"]["capture_version"]
                or next(
                    (
                        item["version"]
                        for item in discovery["version"]["versions"]
                        if item["version"].startswith("10.6")
                    ),
                    "10.6",
                )
            ),
            "target": "ipod-320x240",
            "built_at": _utc_now(),
            "personal_use_only": True,
            "notice": PERSONAL_USE_NOTICE,
            "sources": discovery["version"]["roots"],
            "assets": assets,
        }
        atomic_write_json(stage / "manifest.json", manifest)
        provenance_lines = [
            "Private Mac OS X Snow Leopard Desktop Mode asset extraction",
            "",
            PERSONAL_USE_NOTICE,
            "",
            f"Pack: {PACK_ID}",
            f"Product version: {manifest['product_version']}",
            "No substitute, traced, hand-drawn, or procedurally imitated artwork.",
            "Images are direct crops/resizes/conversions of user-owned Apple pixels.",
            "",
            "Generated assets:",
        ]
        for asset_id, item in assets.items():
            provenance_lines.append(
                f"{asset_id}\t{item['path']}\t{item['output_sha256']}\t"
                f"{item['source_path']}"
            )
        provenance_lines.append("")
        atomic_write_text(stage / "PROVENANCE.txt", "\n".join(provenance_lines))

        validation = validate_pack(stage)
        if not validation["valid"]:
            raise SnowLeopardAssetError(
                "Generated pack failed validation: " + "; ".join(validation["errors"])
            )
        if output.exists():
            shutil.rmtree(output)
        os.replace(stage, output)
    return validate_pack(output)


def validate_pack(
    pack_dir: str | os.PathLike[str],
    *,
    require_complete: bool = True,
) -> dict:
    root = Path(pack_dir).expanduser().resolve()
    errors = []
    manifest_path = root / "manifest.json"
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        return {
            "valid": False,
            "complete": False,
            "root": str(root),
            "errors": [f"Invalid or missing manifest.json: {exc}"],
            "missing": [spec.asset_id for spec in ASSET_SPECS],
            "manifest": {},
        }
    if manifest.get("format") != PACK_FORMAT:
        errors.append(f"Unsupported pack format: {manifest.get('format')!r}")
    if manifest.get("pack_id") != PACK_ID:
        errors.append(f"Wrong pack id: {manifest.get('pack_id')!r}")
    if not str(manifest.get("product_version") or "").startswith("10.6"):
        errors.append("Pack is not proven to be Mac OS X 10.6")
    if manifest.get("personal_use_only") is not True:
        errors.append("Personal-use marker is missing")

    assets = manifest.get("assets") if isinstance(manifest.get("assets"), dict) else {}
    missing = []
    checked = []
    for spec in ASSET_SPECS:
        item = assets.get(spec.asset_id)
        if not isinstance(item, dict):
            missing.append(spec.asset_id)
            continue
        rel = str(item.get("path") or "")
        candidate = (root / rel).resolve()
        try:
            candidate.relative_to(root)
        except ValueError:
            errors.append(f"{spec.asset_id}: path escapes pack")
            continue
        if not candidate.is_file():
            missing.append(spec.asset_id)
            continue
        actual_hash = sha256(candidate)
        if actual_hash != item.get("output_sha256"):
            errors.append(f"{spec.asset_id}: checksum mismatch")
        if spec.kind in {"image", "derived"}:
            try:
                with Image.open(candidate) as image:
                    actual_size = image.size
                header = _bmp_header(candidate)
            except (OSError, SnowLeopardAssetError) as exc:
                errors.append(f"{spec.asset_id}: unreadable image: {exc}")
            else:
                if actual_size != spec.size:
                    errors.append(
                        f"{spec.asset_id}: expected {spec.size[0]}x{spec.size[1]}, "
                        f"got {actual_size[0]}x{actual_size[1]}"
                    )
                if header["depth"] != 16:
                    errors.append(
                        f"{spec.asset_id}: expected 16-bit BMP, "
                        f"got {header['depth']}-bit"
                    )
                if header["compression"] != 3:
                    errors.append(
                        f"{spec.asset_id}: expected BI_BITFIELDS compression"
                    )
        if spec.kind in {"icon", "alpha_image", "derived_alpha"}:
            try:
                header = candidate.open("rb").read(8)
            except OSError as exc:
                errors.append(f"{spec.asset_id}: unreadable coverage image: {exc}")
            else:
                magic, width, height = struct.unpack("<4sHH", header)
                if magic != b"RGA1":
                    errors.append(f"{spec.asset_id}: not an RGA1 coverage image")
                elif (width, height) != spec.size:
                    errors.append(
                        f"{spec.asset_id}: expected {spec.size[0]}x{spec.size[1]}, "
                        f"got {width}x{height}"
                    )
                elif candidate.stat().st_size != 8 + width * height * 3:
                    errors.append(f"{spec.asset_id}: truncated coverage image")
        if spec.kind == "font":
            expected_bytes = int(item.get("output_size", [0, 0])[0]) * int(
                item.get("output_size", [0, 0])[1]
            )
            if expected_bytes and candidate.stat().st_size != expected_bytes:
                errors.append(f"{spec.asset_id}: coverage atlas size mismatch")
            metrics_name = str(item.get("metrics_path") or "")
            if not metrics_name or Path(metrics_name).name != metrics_name:
                errors.append(f"{spec.asset_id}: unsafe metrics path")
                checked.append(spec.asset_id)
                continue
            metrics = candidate.with_name(metrics_name).resolve()
            try:
                metrics.relative_to(root)
            except ValueError:
                errors.append(f"{spec.asset_id}: metrics path escapes pack")
                checked.append(spec.asset_id)
                continue
            if not metrics.is_file():
                errors.append(f"{spec.asset_id}: metrics file is missing")
            elif metrics.stat().st_size != snow_leopard_fonts.METRICS_BYTES:
                errors.append(
                    f"{spec.asset_id}: metrics file must be "
                    f"{snow_leopard_fonts.METRICS_BYTES} bytes"
                )
            elif sha256(metrics) != item.get("metrics_sha256"):
                errors.append(f"{spec.asset_id}: metrics checksum mismatch")
        checked.append(spec.asset_id)
    if missing and require_complete:
        errors.append("Missing required assets: " + ", ".join(missing))
    if not (root / "PROVENANCE.txt").is_file():
        errors.append("PROVENANCE.txt is missing")
    preview = assets.get(MENU_PREVIEW_ID)
    if isinstance(preview, dict):
        candidate = (root / str(preview.get("path") or "")).resolve()
        try:
            candidate.relative_to(root)
        except ValueError:
            errors.append(f"{MENU_PREVIEW_ID}: path escapes pack")
        else:
            if not candidate.is_file():
                errors.append(f"{MENU_PREVIEW_ID}: file is missing")
            elif sha256(candidate) != preview.get("output_sha256"):
                errors.append(f"{MENU_PREVIEW_ID}: checksum mismatch")
            else:
                try:
                    header = _bmp_header(candidate)
                    with Image.open(candidate) as image:
                        actual_size = image.size
                except (OSError, SnowLeopardAssetError) as exc:
                    errors.append(
                        f"{MENU_PREVIEW_ID}: unreadable image: {exc}"
                    )
                else:
                    if actual_size != MENU_PREVIEW_SIZE:
                        errors.append(
                            f"{MENU_PREVIEW_ID}: expected "
                            f"{MENU_PREVIEW_SIZE[0]}x{MENU_PREVIEW_SIZE[1]}"
                        )
                    if header["depth"] != 16 or header["compression"] != 3:
                        errors.append(
                            f"{MENU_PREVIEW_ID}: expected RGB565 BMP"
                        )
    complete = not missing
    return {
        "valid": not errors and (complete or not require_complete),
        "complete": complete,
        "root": str(root),
        "errors": errors,
        "missing": missing,
        "checked": checked,
        "manifest": manifest,
    }


def install_pack(
    pack_dir: str | os.PathLike[str],
    device_root: str | os.PathLike[str],
) -> dict:
    validation = validate_pack(pack_dir)
    if not validation["valid"]:
        raise SnowLeopardAssetError(
            "Refusing to install invalid Snow Leopard pack: "
            + "; ".join(validation["errors"])
        )
    root = Path(validate_device_root(device_root))
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    for relative in LEGACY_XP_PACK_RELATIVES:
        legacy = Path(resolve_under_root(root, relative))
        if legacy.exists() and (legacy.is_symlink() or not legacy.is_dir()):
            raise SnowLeopardAssetError(
                f"Refusing unsafe legacy Desktop Mode path: {legacy}"
            )
    destinations = []
    backups = []
    for index, relative in enumerate(DEVICE_PACK_RELATIVES):
        destination = Path(resolve_under_root(root, relative))
        destination.parent.mkdir(parents=True, exist_ok=True)
        backup = ""
        if destination.exists():
            backup_root = (
                root
                / ".rockbox/rockpod/backups/desktop_mode_snow_leopard"
                / stamp
            )
            backup_root.mkdir(parents=True, exist_ok=True)
            backup_path = backup_root / (
                "hardware-apps" if index == 0 else "hosted-rocks-data"
            )
            shutil.copytree(destination, backup_path)
            backup = str(backup_path)

        with tempfile.TemporaryDirectory(
            prefix="desktop-mode-install-",
            dir=destination.parent,
        ) as temporary:
            stage = Path(temporary) / destination.name
            shutil.copytree(validation["root"], stage)
            _ensure_menu_preview(stage)
            staged_validation = validate_pack(stage)
            if not staged_validation["valid"]:
                raise SnowLeopardAssetError(
                    "Staged device pack failed verification: "
                    + "; ".join(staged_validation["errors"])
                )
            if destination.exists():
                shutil.rmtree(destination)
            os.replace(stage, destination)
        installed = validate_pack(destination)
        if not installed["valid"]:
            raise SnowLeopardAssetError(
                "Installed device pack failed verification: "
                + "; ".join(installed["errors"])
            )
        destinations.append(str(destination))
        backups.append(backup)
    replaced_xp = []
    for index, relative in enumerate(LEGACY_XP_PACK_RELATIVES):
        legacy = Path(resolve_under_root(root, relative))
        if not legacy.exists():
            continue
        backup_root = (
            root
            / ".rockbox/rockpod/backups/desktop_mode_xp"
            / stamp
        )
        backup_root.mkdir(parents=True, exist_ok=True)
        backup_path = backup_root / (
            "hardware-apps" if index == 0 else "hosted-rocks-data"
        )
        shutil.copytree(legacy, backup_path)
        shutil.rmtree(legacy)
        replaced_xp.append(
            {
                "removed": str(legacy),
                "backup": str(backup_path),
            }
        )
    return {
        "success": True,
        "destination": destinations[0],
        "destinations": destinations,
        "backup": backups[0],
        "backups": backups,
        "pack_id": validation["manifest"]["pack_id"],
        "asset_count": len(validation["checked"]),
        "replaced_xp": replaced_xp,
    }
