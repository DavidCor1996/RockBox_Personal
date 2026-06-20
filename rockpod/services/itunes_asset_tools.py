"""Local-only discovery, import, and validation helpers for iTunes-era assets."""

from __future__ import annotations

import json
import os
import shutil
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

from services.command_runner import CommandRunner
from services.file_safety import atomic_write_json, atomic_write_text
from services.theme_assets import PERSONAL_THEME_DIR, ThemeAssetManager


DISCOVERY_SOURCES = [
    {
        "name": "Apple newsroom: iTunes 7 launch",
        "type": "reference",
        "recommended_versions": ["7.0"],
        "url": "https://www.apple.com/newsroom/2006/09/12Apple-Announces-iTunes-7-with-Amazing-New-Features/",
        "notes": "Best reference for the original September 12, 2006 iTunes 7 chrome, Cover Flow era visuals, and launch timing.",
    },
    {
        "name": "Apple newsroom: iTunes 7.4 era",
        "type": "reference",
        "recommended_versions": ["7.4"],
        "url": "https://www.apple.com/newsroom/2007/09/05Apple-Unveils-the-iTunes-Wi-Fi-Music-Store/",
        "notes": "Good reference for late-2007 iTunes 7.x window chrome and controls before the iTunes 8 redesign.",
    },
    {
        "name": "Apple Support legacy downloads index",
        "type": "download-index",
        "recommended_versions": ["9.2.1", "8.2", "8.1.1"],
        "url": "https://support.apple.com/en-us/docs/software/pl296",
        "notes": "Apple still hosts some later legacy Windows installers. Useful when you want an official source or to trace adjacent package layout.",
    },
    {
        "name": "OldVersion iTunes 7.0",
        "type": "archive",
        "recommended_versions": ["7.0"],
        "url": "https://www.oldversion.com/software/itunes/itunes-7-0/",
        "notes": "Closest to the first Leopard-era visual language. Best source for the original 2006 asset set.",
    },
    {
        "name": "OldVersion iTunes 7.0.2",
        "type": "archive",
        "recommended_versions": ["7.0.2"],
        "url": "https://www.oldversion.com/windows/itunes-7-0-2",
        "notes": "Good if you want the same early iTunes 7 visuals with some 2006 maintenance fixes.",
    },
    {
        "name": "OldVersion iTunes 7.6.2",
        "type": "archive",
        "recommended_versions": ["7.6.2"],
        "url": "https://www.oldversion.com/software/itunes/itunes-7-6-2/",
        "notes": "Useful if you want the last pre-iTunes-8 7.x package layout from early 2008.",
    },
    {
        "name": "The Apple Wiki: iTunes 7 icon",
        "type": "community-reference",
        "recommended_versions": ["7.x"],
        "url": "https://theapplewiki.com/wiki/File:ITunes_7_icon.png",
        "notes": "A community reference confirming at least one icon extracted from iTunes.app. Use as a visual cross-check, not as a bundled asset source.",
    },
]

VERSION_RECOMMENDATIONS = [
    {
        "version": "7.0",
        "when": "Best match for September 2006 iTunes 7 launch styling",
        "why": "Original iTunes 7 UI refresh, Cover Flow era chrome, and early Aqua/Leopard feel.",
    },
    {
        "version": "7.0.2",
        "when": "Good default recommendation",
        "why": "Still firmly in the 2006 look while being a slightly more mature build.",
    },
    {
        "version": "7.4",
        "when": "Best match for late-2007 iPhone / iPod touch era iTunes 7",
        "why": "Apple explicitly announced iTunes 7.4 on September 5, 2007, still pre-iTunes-8 visually.",
    },
    {
        "version": "7.6.2",
        "when": "Best if you want the last 7.x package before the 8.x redesign",
        "why": "Latest 7.x branch and often easiest to find intact in old-version archives.",
    },
]

WINDOWS_EXTRACTION_GUIDE = [
    "1. Download an iTunes 7.x installer manually from a source you trust.",
    "2. Copy the installer to a scratch directory so you do not modify the original.",
    "3. Try simple archive extraction first: `7z x iTunesSetup.exe -oitunes_extracted`.",
    "4. If the EXE contains an MSI, extract that too: `7z x iTunes.msi -oitunes_msi`.",
    "5. Inspect extracted files such as `iTunes.exe`, `iTunes.Resources`, `iTunes.rsrc`, bundled DLLs, ICO files, BMP files, PNG files, and any image-heavy resource folders.",
    "6. For EXE or DLL resources, use a local tool such as Resource Hacker or a similar resource viewer to export bitmap, PNG, ICO, GIF, dialog, and string resources.",
    "7. Put the exported images in a normal folder tree, then run `rockpod import-itunes-assets /path/to/extracted/assets`.",
]

MAC_EXTRACTION_GUIDE = [
    "1. Obtain a local copy of an iTunes 7.x-era `iTunes.app` or installer image manually.",
    "2. Open the app bundle and inspect `iTunes.app/Contents/Resources/`.",
    "3. Look for `*.icns`, `*.png`, nib resources, localized `.lproj` folders, and toolbar/sidebar artwork under `Resources`.",
    "4. On macOS, `Show Package Contents` is enough for manual browsing. On Linux, treat the `.app` bundle as a normal directory tree.",
    "5. Export or convert the assets you want into a normal working directory, then run the RockPod importer on that directory.",
]

EXPECTED_LOCATIONS = [
    "Windows installer archives: top-level MSI payload, `iTunes.exe`, resource DLLs, ICO/BMP/PNG resources, and embedded dialog/bitmap resources in PE files.",
    "macOS app bundle: `iTunes.app/Contents/Resources/`, localized `.lproj` directories, `.icns` app icons, and any toolbar/sidebar image resources packaged with the bundle.",
]


@dataclass(frozen=True)
class AssetRule:
    asset_name: str
    target_relpath: str
    aliases: tuple[str, ...]
    contexts: tuple[str, ...] = ()


ASSET_RULES = [
    AssetRule("branding_title", "branding/title", ("title", "logo", "itunes"), ("branding", "titlebar")),
    AssetRule("toolbar_sync", "toolbar/sync", ("sync",), ("toolbar",)),
    AssetRule("toolbar_refresh", "toolbar/refresh", ("refresh", "update"), ("toolbar",)),
    AssetRule("toolbar_new_playlist", "toolbar/new_playlist", ("playlist", "newplaylist", "addplaylist"), ("toolbar",)),
    AssetRule("playback_previous", "playback/previous", ("previous", "prev", "back"), ("playback", "transport")),
    AssetRule("playback_play", "playback/play", ("play",), ("playback", "transport")),
    AssetRule("playback_pause", "playback/pause", ("pause",), ("playback", "transport")),
    AssetRule("playback_next", "playback/next", ("next", "forward"), ("playback", "transport")),
    AssetRule("sidebar_music", "sidebar/music", ("music",), ("sidebar", "source")),
    AssetRule("sidebar_artists", "sidebar/artists", ("artists", "artist"), ("sidebar", "source")),
    AssetRule("sidebar_albums", "sidebar/albums", ("albums", "album"), ("sidebar", "source")),
    AssetRule("sidebar_genres", "sidebar/genres", ("genres", "genre"), ("sidebar", "source")),
    AssetRule("sidebar_playlist", "sidebar/playlist", ("playlist",), ("sidebar", "source")),
    AssetRule("sidebar_device", "sidebar/device", ("device", "ipod"), ("sidebar", "source")),
    AssetRule("chrome_texture", "chrome/toolbar_texture", ("toolbar", "chrome", "brushed", "metal"), ("chrome", "toolbar")),
    AssetRule("search_field_left", "toolbar/search_left", ("search", "left"), ("toolbar", "search")),
    AssetRule("search_field_middle", "toolbar/search_middle", ("search", "middle", "center"), ("toolbar", "search")),
    AssetRule("search_field_right", "toolbar/search_right", ("search", "right"), ("toolbar", "search")),
    AssetRule("table_header_texture", "table/header", ("header", "columnheader", "tableheader"), ("table", "header")),
    AssetRule("table_selection_texture", "table/selection", ("selection", "selectedrow"), ("table", "list")),
    AssetRule("sidebar_selection_texture", "sidebar/selection", ("selection", "highlight"), ("sidebar", "source")),
    AssetRule("statusbar_texture", "statusbar/background", ("statusbar", "status", "bottom"), ("statusbar",)),
    AssetRule("statusbar_divider", "statusbar/divider", ("divider", "separator"), ("statusbar",)),
    AssetRule("storagebar_frame", "statusbar/storage_frame", ("storage", "capacity", "meter", "frame"), ("statusbar", "device")),
    AssetRule("device_summary_header", "device/summary_header", ("summary", "header"), ("device", "summary")),
    AssetRule("device_summary_sidebar_icon", "device/ipod_icon", ("ipod", "device", "icon"), ("device", "sidebar")),
    AssetRule("album_placeholder", "artwork/album_placeholder", ("placeholder", "noart", "missingart"), ("artwork", "album")),
    AssetRule("album_frame", "artwork/album_frame", ("frame", "artframe"), ("artwork", "album")),
]

IMAGE_EXTENSIONS = {".png", ".bmp", ".gif", ".jpg", ".jpeg", ".tif", ".tiff", ".ico", ".icns"}
EMBEDDED_RESOURCE_EXTENSIONS = {".exe", ".dll", ".mui", ".ocx", ".cpl"}
ARCHIVE_EXTENSIONS = {".exe", ".msi", ".zip", ".cab", ".dmg", ".pkg"}


def _tokenize(value: str) -> str:
    text = value.lower()
    for old in ("-", "_", ".", "(", ")", "[", "]"):
        text = text.replace(old, " ")
    return " ".join(text.split())


def _iter_candidate_files(source_dir: str | os.PathLike[str]) -> Iterable[Path]:
    root = Path(source_dir)
    for path in sorted(root.rglob("*")):
        if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS:
            yield path


def _score_rule(rule: AssetRule, root: Path, path: Path) -> int:
    try:
        relative = path.relative_to(root)
    except ValueError:
        relative = path
    haystack = _tokenize(str(relative))
    score = 0
    for alias in rule.aliases:
        if _tokenize(alias) in haystack:
            score += 4
    for context in rule.contexts:
        if _tokenize(context) in haystack:
            score += 2
    if rule.asset_name.endswith("play") and "pause" in haystack:
        score -= 3
    if rule.asset_name.endswith("pause") and "play" in haystack and "pause" not in haystack:
        score -= 3
    return score


def discover_asset_matches(source_dir: str | os.PathLike[str]):
    root = Path(source_dir)
    matches = {}
    candidates = list(_iter_candidate_files(root))
    used = set()
    for rule in ASSET_RULES:
        ranked = []
        for candidate in candidates:
            score = _score_rule(rule, root, candidate)
            if score > 0:
                ranked.append((score, candidate))
        ranked.sort(key=lambda item: (-item[0], len(str(item[1]))))
        selected = None
        for score, candidate in ranked:
            if str(candidate) in used:
                continue
            selected = (score, candidate)
            break
        if selected:
            score, candidate = selected
            used.add(str(candidate))
            matches[rule.asset_name] = {
                "source_path": str(candidate),
                "score": score,
                "target_relpath": rule.target_relpath,
            }
    return matches


def ensure_personal_theme_scaffold():
    PERSONAL_THEME_DIR.mkdir(parents=True, exist_ok=True)
    for folder in ("branding", "chrome", "toolbar", "sidebar", "playback", "table", "statusbar", "device", "icons", "artwork"):
        (PERSONAL_THEME_DIR / folder).mkdir(parents=True, exist_ok=True)


def import_itunes_assets(source_dir: str | os.PathLike[str], config=None):
    ensure_personal_theme_scaffold()
    manager = ThemeAssetManager(config) if config is not None else ThemeAssetManager(_ephemeral_config())
    matches = discover_asset_matches(source_dir)

    copied = []
    asset_overrides = {}
    for asset_name, data in matches.items():
        src = Path(data["source_path"])
        dest_base = PERSONAL_THEME_DIR / data["target_relpath"]
        dest = dest_base.with_suffix(src.suffix.lower())
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest)
        rel = dest.relative_to(PERSONAL_THEME_DIR).as_posix()
        asset_overrides[asset_name] = rel
        copied.append({"asset": asset_name, "source_path": str(src), "target_path": str(dest), "mapped_path": rel})

    manifest_path = manager.update_personal_manifest(
        asset_overrides=asset_overrides,
        metadata={
            "source_dir": str(Path(source_dir).resolve()),
            "imported_asset_count": len(copied),
        },
    )
    validation = manager.validate_active_theme()
    return {
        "source_dir": str(Path(source_dir).resolve()),
        "manifest_path": manifest_path,
        "copied_count": len(copied),
        "copied_assets": copied,
        "matched_assets": sorted(matches.keys()),
        "missing_assets": [rule.asset_name for rule in ASSET_RULES if rule.asset_name not in matches],
        "validation": validation,
    }


def extract_itunes_assets(
    source_path: str | os.PathLike[str],
    out_dir: str | os.PathLike[str] | None = None,
    command_runner=None,
):
    source = Path(source_path).resolve()
    if out_dir is None:
        out_dir = source.parent / f"{source.stem}_rockpod_assets"
    output_root = Path(out_dir).resolve()
    raw_dir = output_root / "raw"
    collected_dir = output_root / "collected"
    output_root.mkdir(parents=True, exist_ok=True)
    raw_dir.mkdir(parents=True, exist_ok=True)
    collected_dir.mkdir(parents=True, exist_ok=True)
    runner = command_runner or CommandRunner(log_dir=str(output_root / "logs"))

    extraction_mode = "directory"
    extraction_notes = []
    extracted_root = source
    if source.is_file():
        extraction_mode = "archive"
        if source.suffix.lower() not in ARCHIVE_EXTENSIONS:
            raise ValueError(f"Unsupported installer/archive type: {source.suffix}")
        seven_zip = shutil.which("7z") or shutil.which("7za")
        if not seven_zip:
            raise RuntimeError("7z is required for local installer extraction but was not found in PATH.")
        result = _run_tool([seven_zip, "x", str(source), f"-o{raw_dir}", "-y"], runner)
        if _extraction_failed(result, raw_dir):
            raise RuntimeError(_command_failure_detail(result, "7z extraction failed"))
        extracted_root = raw_dir
        extraction_notes.append("Archive extracted with 7z.")
    elif source.is_dir():
        if source.suffix.lower() == ".app" and (source / "Contents" / "Resources").is_dir():
            extracted_root = source / "Contents" / "Resources"
            extraction_notes.append("Scanning macOS app bundle resources.")
        else:
            extraction_notes.append("Scanning existing extracted directory.")
    else:
        raise FileNotFoundError(f"Source path not found: {source}")

    copied_assets = []
    copied_assets.extend(_collect_images(extracted_root, collected_dir))

    nested_notes = []
    if source.is_file() and source.suffix.lower() == ".exe":
        nested_msi = _first_existing(raw_dir / "iTunes.msi", raw_dir / "QuickTime.msi")
        if nested_msi:
            nested_dir = output_root / "itunes_msi"
            _extract_archive(nested_msi, nested_dir, runner)
            copied_assets.extend(_collect_images(nested_dir, collected_dir))
            nested_notes.append(f"Nested MSI extracted: {nested_msi.name}")

            nested_cab = _first_existing(nested_dir / "iTunes.cab")
            if nested_cab:
                cab_dir = output_root / "itunes_cab"
                _extract_archive(nested_cab, cab_dir, runner)
                copied_assets.extend(_collect_images(cab_dir, collected_dir))
                nested_notes.append("Nested iTunes.cab extracted.")
                copied_assets.extend(_extract_qtr_images(cab_dir, output_root, collected_dir, runner))
                copied_assets.extend(_extract_pe_icons(cab_dir, output_root, collected_dir, runner))
    elif source.is_file() and source.suffix.lower() == ".msi":
        nested_cab = _first_existing(raw_dir / "iTunes.cab")
        if nested_cab:
            cab_dir = output_root / "itunes_cab"
            _extract_archive(nested_cab, cab_dir, runner)
            copied_assets.extend(_collect_images(cab_dir, collected_dir))
            nested_notes.append("Nested iTunes.cab extracted.")
            copied_assets.extend(_extract_qtr_images(cab_dir, output_root, collected_dir, runner))
            copied_assets.extend(_extract_pe_icons(cab_dir, output_root, collected_dir, runner))
    elif source.is_dir():
        copied_assets.extend(_extract_qtr_images(extracted_root, output_root, collected_dir, runner))
        copied_assets.extend(_extract_pe_icons(extracted_root, output_root, collected_dir, runner))

    copied_assets.extend(_extract_pkg_payloads(extracted_root, output_root, collected_dir, runner))

    embedded_candidates = []
    for path in sorted(extracted_root.rglob("*")):
        if path.is_file() and path.suffix.lower() in EMBEDDED_RESOURCE_EXTENSIONS:
            embedded_candidates.append(str(path))

    summary_path = output_root / "summary.json"
    summary = {
        "source_path": str(source),
        "extraction_mode": extraction_mode,
        "extracted_root": str(extracted_root),
        "output_root": str(output_root),
        "collected_dir": str(collected_dir),
        "copied_asset_count": len(copied_assets),
        "copied_assets": copied_assets,
        "embedded_resource_candidates": embedded_candidates,
        "notes": extraction_notes + nested_notes,
        "next_step": f"Run `rockpod import-itunes-assets {collected_dir}`",
    }
    atomic_write_json(summary_path, summary, sort_keys=False)
    summary["summary_path"] = str(summary_path)
    return summary


def _extract_archive(source: Path, out_dir: Path, command_runner=None):
    seven_zip = shutil.which("7z") or shutil.which("7za")
    if not seven_zip:
        raise RuntimeError("7z is required for local installer extraction but was not found in PATH.")
    out_dir.mkdir(parents=True, exist_ok=True)
    result = _run_tool([seven_zip, "x", str(source), f"-o{out_dir}", "-y"], command_runner)
    if _extraction_failed(result, out_dir):
        raise RuntimeError(_command_failure_detail(result, f"Failed to extract {source}"))


def _extract_pkg_payloads(root: Path, output_root: Path, collected_dir: Path, command_runner=None):
    copied = []
    payload_root = output_root / "pkg_payloads"
    for archive in sorted(root.rglob("Archive.pax.gz")):
        try:
            relative = archive.relative_to(root)
        except ValueError:
            relative = Path(archive.name)
        stem = _safe_stem(relative.parent.parent.as_posix())
        dest = payload_root / stem
        shutil.rmtree(dest, ignore_errors=True)
        dest.mkdir(parents=True, exist_ok=True)
        if not _extract_payload_archive(archive, dest, command_runner):
            continue
        copied.extend(_collect_images(dest, collected_dir))
        copied.extend(_extract_qtr_images(dest, output_root, collected_dir, command_runner))
        copied.extend(_extract_pe_icons(dest, output_root, collected_dir, command_runner))
    return copied


def _extract_payload_archive(source: Path, out_dir: Path, command_runner=None):
    tar = shutil.which("tar")
    if tar:
        result = _run_tool([tar, "-xzf", str(source), "-C", str(out_dir)], command_runner)
        if result.returncode == 0:
            return True
    seven_zip = shutil.which("7z") or shutil.which("7za")
    if not seven_zip:
        return False
    result = _run_tool([seven_zip, "x", str(source), f"-o{out_dir}", "-y"], command_runner)
    if result.returncode != 0:
        return False

    pax_files = sorted(out_dir.glob("*.pax"))
    for pax in pax_files:
        pax_out = out_dir / f"{pax.stem}_contents"
        pax_out.mkdir(parents=True, exist_ok=True)
        if tar:
            pax_result = _run_tool([tar, "-xf", str(pax), "-C", str(pax_out)], command_runner)
            if pax_result.returncode == 0:
                _merge_tree(pax_out, out_dir)
                shutil.rmtree(pax_out, ignore_errors=True)
                continue
        pax_result = _run_tool([seven_zip, "x", str(pax), f"-o{pax_out}", "-y"], command_runner)
        if pax_result.returncode == 0:
            _merge_tree(pax_out, out_dir)
        shutil.rmtree(pax_out, ignore_errors=True)
    return True


def _safe_stem(value: str):
    text = value.replace("\\", "/")
    for old in ("/", " ", ".", "(", ")", "[", "]"):
        text = text.replace(old, "_")
    return "_".join(part for part in text.split("_") if part) or "payload"


def _run_tool(command, command_runner=None, cwd=""):
    runner = command_runner or CommandRunner()
    return runner.run([str(part) for part in command], cwd=str(cwd or ""))


def _command_failure_detail(result, fallback):
    message = (getattr(result, "stderr", "") or getattr(result, "stdout", "") or "").strip()
    if not message and hasattr(result, "failure_message"):
        message = result.failure_message().strip()
    if not message:
        message = fallback
    log_path = getattr(result, "log_path", "")
    if log_path and log_path not in message:
        message = f"{message}\nLog: {log_path}"
    return message


def _extraction_failed(result, out_dir: Path):
    if result.returncode == 0:
        return False
    message = f"{result.stderr}\n{result.stdout}".lower()
    has_output = out_dir.exists() and any(out_dir.rglob("*"))
    if has_output and "dangerous link path was ignored" in message:
        return False
    return True


def _merge_tree(source: Path, dest: Path):
    for path in sorted(source.rglob("*")):
        if not path.is_file():
            continue
        relative = path.relative_to(source)
        target = dest / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)


def _collect_images(root: Path, collected_dir: Path):
    copied = []
    for candidate in _iter_candidate_files(root):
        try:
            relative = candidate.relative_to(root)
        except ValueError:
            relative = Path(candidate.name)
        dest = collected_dir / relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(candidate, dest)
        copied.append(str(dest))
    return copied


def _extract_qtr_images(root: Path, output_root: Path, collected_dir: Path, command_runner=None):
    binwalk = shutil.which("binwalk")
    if not binwalk:
        return []
    copied = []
    for qtr in sorted(root.rglob("*.qtr")):
        qtr_out = output_root / f"{qtr.stem}_binwalk"
        shutil.rmtree(qtr_out, ignore_errors=True)
        qtr_out.mkdir(parents=True, exist_ok=True)
        result = _run_tool([binwalk, "-e", "-C", str(qtr_out), str(qtr)], command_runner)
        if result.returncode != 0:
            continue
        for image in sorted(qtr_out.rglob("image.png")):
            rel_parent = image.parent.name
            dest = collected_dir / "qtr" / f"{qtr.stem}_{rel_parent}.png"
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(image, dest)
            copied.append(str(dest))
    return copied


def _extract_pe_icons(root: Path, output_root: Path, collected_dir: Path, command_runner=None):
    wrestool = shutil.which("wrestool")
    icotool = shutil.which("icotool")
    if not wrestool or not icotool:
        return []
    copied = []
    for pe in sorted(root.rglob("*")):
        if not pe.is_file() or pe.suffix.lower() not in EMBEDDED_RESOURCE_EXTENSIONS:
            continue
        icon_dir = output_root / f"{pe.name}_icons"
        shutil.rmtree(icon_dir, ignore_errors=True)
        icon_dir.mkdir(parents=True, exist_ok=True)
        extract = _run_tool([wrestool, "-x", "--output", str(icon_dir), "-t14", str(pe)], command_runner)
        if extract.returncode != 0:
            continue
        for ico in sorted(icon_dir.glob("*.ico")):
            png_dir = output_root / f"{ico.stem}_png"
            png_dir.mkdir(parents=True, exist_ok=True)
            _run_tool([icotool, "-x", "-o", str(png_dir), str(ico)], command_runner)
            for png in sorted(png_dir.glob("*.png")):
                dest = collected_dir / "icons" / png.name
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(png, dest)
                copied.append(str(dest))
    return copied


def _first_existing(*paths: Path):
    for path in paths:
        if path.is_file():
            return path
    return None


def validate_personal_theme(config=None):
    manager = ThemeAssetManager(_personal_theme_config(config))
    report = manager.validate_active_theme()
    report["personal_theme_dir"] = str(PERSONAL_THEME_DIR)
    report["personal_manifest_exists"] = (PERSONAL_THEME_DIR / "theme.json").is_file()
    return report


def build_asset_review(
    source_dir: str | os.PathLike[str],
    out_dir: str | os.PathLike[str] | None = None,
    command_runner=None,
):
    source = Path(source_dir).resolve()
    if out_dir is None:
        out_dir = source / "_review"
    review_dir = Path(out_dir).resolve()
    review_dir.mkdir(parents=True, exist_ok=True)
    runner = command_runner or CommandRunner(log_dir=str(review_dir / "logs"))

    manifest = []
    grouped = {}
    for image in sorted(_iter_candidate_files(source)):
        category = image.parent.name if image.parent != source else "root"
        dims = _image_dimensions(image, runner)
        rel = image.relative_to(source).as_posix()
        item = {
            "category": category,
            "relative_path": rel,
            "filename": image.name,
            "width": dims[0],
            "height": dims[1],
            "absolute_path": str(image),
        }
        manifest.append(item)
        grouped.setdefault(category, []).append(item)

    sheets = {}
    montage = shutil.which("montage")
    for category, items in grouped.items():
        if not items or not montage:
            continue
        output_path = review_dir / f"{category}_contact_sheet.png"
        cmd = [
            montage,
            *[str(source / item["relative_path"]) for item in items],
            "-label", "%f",
            "-tile", "8x",
            "-geometry", "96x96+8+20",
            str(output_path),
        ]
        result = _run_tool(cmd, runner)
        if result.returncode == 0 and output_path.is_file():
            sheets[category] = str(output_path)

    manifest_path = review_dir / "asset_manifest.json"
    atomic_write_json(manifest_path, manifest, sort_keys=False)

    html_path = review_dir / "index.html"
    atomic_write_text(html_path, _review_html(grouped, sheets))

    return {
        "source_dir": str(source),
        "review_dir": str(review_dir),
        "manifest_path": str(manifest_path),
        "html_path": str(html_path),
        "contact_sheets": sheets,
        "asset_count": len(manifest),
        "categories": {key: len(value) for key, value in grouped.items()},
    }


def format_validation_report(report: dict) -> str:
    lines = [
        f"Requested theme: {report['requested']}",
        f"Active theme: {report['active']}",
        f"Theme name: {report['theme_name']}",
        f"Manifest: {report['manifest_path'] or '(none)'}",
        f"Found assets: {report['found_count']}/{report['asset_count']} ({report['completeness_percent']}%)",
        f"Personal overrides in use: {report['personal_count']}",
    ]
    if report["missing_assets"]:
        lines.append("Missing assets:")
        lines.extend(f"  - {name}" for name in report["missing_assets"])
    else:
        lines.append("Missing assets: none")
    return "\n".join(lines)


def format_discovery_guide() -> str:
    lines = ["Recommended sources for manual iTunes-era asset discovery:", ""]
    for source in DISCOVERY_SOURCES:
        versions = ", ".join(source["recommended_versions"])
        lines.append(f"- {source['name']} ({versions})")
        lines.append(f"  {source['url']}")
        lines.append(f"  {source['notes']}")
    lines.append("")
    lines.append("Recommended iTunes 7.x targets:")
    for item in VERSION_RECOMMENDATIONS:
        lines.append(f"- {item['version']}: {item['when']}. {item['why']}")
    lines.append("")
    lines.append("Windows extraction:")
    lines.extend(f"- {step}" for step in WINDOWS_EXTRACTION_GUIDE)
    lines.append("")
    lines.append("macOS extraction:")
    lines.extend(f"- {step}" for step in MAC_EXTRACTION_GUIDE)
    lines.append("")
    lines.append("Look in these locations after extraction:")
    lines.extend(f"- {item}" for item in EXPECTED_LOCATIONS)
    return "\n".join(lines)


def _image_dimensions(path: Path, command_runner=None):
    identify = shutil.which("identify")
    if identify:
        result = _run_tool([identify, "-format", "%w %h", str(path)], command_runner)
        if result.returncode == 0 and result.stdout.strip():
            parts = result.stdout.strip().split()
            if len(parts) == 2 and all(part.isdigit() for part in parts):
                return int(parts[0]), int(parts[1])
    return 0, 0


def _review_html(grouped: dict, sheets: dict):
    parts = [
        "<!doctype html>",
        "<html><head><meta charset='utf-8'><title>RockPod Asset Review</title>",
        "<style>body{font-family:sans-serif;margin:24px;background:#f2f2f2;color:#222}h1,h2{margin:0 0 12px}section{margin:0 0 32px}table{border-collapse:collapse;width:100%;background:#fff}th,td{border:1px solid #ccc;padding:6px 8px;font-size:12px;text-align:left}img.sheet{max-width:100%;background:#fff;border:1px solid #bbb}code{font-size:12px}</style>",
        "</head><body>",
        "<h1>RockPod Asset Review</h1>",
    ]
    for category in sorted(grouped):
        parts.append(f"<section><h2>{category}</h2>")
        if category in sheets:
            sheet_name = Path(sheets[category]).name
            parts.append(f"<p><img class='sheet' src='{sheet_name}' alt='{category} contact sheet'></p>")
        parts.append("<table><thead><tr><th>File</th><th>Size</th><th>Path</th></tr></thead><tbody>")
        for item in grouped[category]:
            parts.append(
                "<tr>"
                f"<td>{item['filename']}</td>"
                f"<td>{item['width']}x{item['height']}</td>"
                f"<td><code>{item['relative_path']}</code></td>"
                "</tr>"
            )
        parts.append("</tbody></table></section>")
    parts.append("</body></html>")
    return "".join(parts)


def _ephemeral_config():
    class _Config:
        def __init__(self):
            self.theme_mode = "personal"

        def get(self, key, default=None):
            if key == "theme_mode":
                return self.theme_mode
            return default

    return _Config()


def _personal_theme_config(config):
    base = config or _ephemeral_config()

    class _ConfigProxy:
        def __init__(self, wrapped):
            self._wrapped = wrapped
            self.theme_mode = "personal"

        def get(self, key, default=None):
            if key == "theme_mode":
                return "personal"
            getter = getattr(self._wrapped, "get", None)
            if getter is not None:
                return getter(key, default)
            return getattr(self._wrapped, key, default)

    return _ConfigProxy(base)
