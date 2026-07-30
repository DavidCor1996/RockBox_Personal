#!/usr/bin/env python3
"""Install self-contained macOS/Windows Desktop Mode launchers on an iPod.

Native simulator plugins must not replace the ARM plugins used by the physical
iPod. Each platform bundle therefore carries a small system-root overlay while
the simulator's ordinary --root remains the mounted iPod for media and
database access.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import tempfile
from datetime import datetime, timezone
from pathlib import Path


PLATFORMS = {
    "windows-x86_64": "rockboxui.exe",
    "macos-x86_64": "rockboxui",
    "macos-arm64": "rockboxui",
}
REQUIRED_PLUGINS = (
    ".rockbox/rocks/apps/desktop_mode.rock",
    ".rockbox/rocks/apps/livetv.rock",
    ".rockbox/rocks/apps/sitekick.rock",
    ".rockbox/rocks/apps/netflix_desktop.rock",
    ".rockbox/rocks/viewers/mpegplayer.rock",
    ".rockbox/rocks/viewers/openh264_player.rock",
)
DEVICE_ASSET_RELATIVE = Path(
    ".rockbox/rocks/apps/desktop_mode_snow_leopard"
)
LAUNCHERS = (
    "Open Desktop Mode.command",
    "Open Desktop Mode.cmd",
    "README.txt",
)


class PortableDesktopError(RuntimeError):
    pass


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _bundle_system_root(bundle: Path) -> Path:
    for name in ("system-root", "simdisk"):
        candidate = bundle / name
        if candidate.is_dir():
            return candidate
    raise PortableDesktopError(
        f"{bundle}: missing system-root (or simulator simdisk)"
    )


def _validate_bundle(bundle: Path, platform: str) -> tuple[Path, Path]:
    runtime = bundle / PLATFORMS[platform]
    if not runtime.is_file():
        raise PortableDesktopError(f"{bundle}: missing {runtime.name}")
    system_root = _bundle_system_root(bundle)
    missing = [
        relative
        for relative in REQUIRED_PLUGINS
        if not (system_root / relative).is_file()
    ]
    if missing:
        raise PortableDesktopError(
            f"{bundle}: missing native plugins: {', '.join(missing)}"
        )
    return runtime, system_root


def _copy_bundle(bundle: Path, destination: Path, platform: str) -> None:
    _, source_system_root = _validate_bundle(bundle, platform)
    destination.mkdir(parents=True)
    for source in sorted(bundle.iterdir(), key=lambda item: item.name):
        if source == source_system_root:
            continue
        target = destination / source.name
        if source.is_dir():
            shutil.copytree(source, target)
        elif source.is_file():
            shutil.copy2(source, target)
    shutil.copytree(source_system_root, destination / "system-root")
    if platform.startswith("macos-"):
        (destination / "rockboxui").chmod(0o755)


def _install_private_assets(device_root: Path, platform_root: Path) -> None:
    source = device_root / DEVICE_ASSET_RELATIVE
    manifest = source / "manifest.json"
    if not source.is_dir() or not manifest.is_file():
        raise PortableDesktopError(
            "Desktop Mode assets are not installed on the iPod: "
            f"{DEVICE_ASSET_RELATIVE}"
        )

    destination = (
        platform_root / "system-root" / DEVICE_ASSET_RELATIVE
    )
    if destination.exists():
        shutil.rmtree(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(source, destination)


def _manifest(root: Path, platforms: list[str]) -> dict:
    files = {}
    for path in sorted(root.rglob("*")):
        if path.is_file():
            files[path.relative_to(root).as_posix()] = {
                "bytes": path.stat().st_size,
                "sha256": _sha256(path),
            }
    return {
        "format": 1,
        "created_at": datetime.now(timezone.utc)
        .replace(microsecond=0)
        .isoformat(),
        "platforms": platforms,
        "media_root": "mounted-iPod",
        "system_overlay": [
            "/.rockbox/rocks",
            "/.rockbox/rocks.data",
            "/.rockbox/fonts",
            "/.rockbox/langs",
            "/.rockbox/icons",
        ],
        "files": files,
    }


def install(
    device_root: Path,
    bundle_root: Path,
    launcher_root: Path,
    platforms: list[str],
) -> dict:
    device_root = device_root.expanduser().resolve()
    bundle_root = bundle_root.expanduser().resolve()
    launcher_root = launcher_root.expanduser().resolve()
    if not (device_root / ".rockbox").is_dir():
        raise PortableDesktopError(
            f"{device_root} is not a mounted or staged Rockbox root"
        )
    for name in LAUNCHERS:
        if not (launcher_root / name).is_file():
            raise PortableDesktopError(
                f"portable launcher source is missing: {name}"
            )
    unknown = sorted(set(platforms) - set(PLATFORMS))
    if unknown:
        raise PortableDesktopError(
            f"unknown portable platform(s): {', '.join(unknown)}"
        )
    if not platforms:
        raise PortableDesktopError("at least one portable platform is required")

    for platform in platforms:
        _validate_bundle(bundle_root / platform, platform)

    host_parent = device_root / ".rockbox"
    with tempfile.TemporaryDirectory(
        prefix=".desktop-host-stage-", dir=host_parent
    ) as temporary:
        stage = Path(temporary) / "desktop-host"
        stage.mkdir()
        for platform in platforms:
            platform_destination = stage / platform
            _copy_bundle(
                bundle_root / platform, platform_destination, platform
            )
            _install_private_assets(device_root, platform_destination)

        manifest = _manifest(stage, platforms)
        (stage / "manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )

        destination = host_parent / "desktop-host"
        backup = host_parent / ".desktop-host.previous"
        if backup.exists():
            shutil.rmtree(backup)
        if destination.exists():
            os.replace(destination, backup)
        try:
            os.replace(stage, destination)
        except Exception:
            if backup.exists() and not destination.exists():
                os.replace(backup, destination)
            raise
        if backup.exists():
            shutil.rmtree(backup)

    installed_launchers = []
    for name in LAUNCHERS:
        source = launcher_root / name
        destination = device_root / name
        shutil.copy2(source, destination)
        if destination.suffix == ".command":
            destination.chmod(0o755)
        installed_launchers.append(destination.name)

    return {
        "destination": str(device_root / ".rockbox/desktop-host"),
        "platforms": platforms,
        "launchers": installed_launchers,
        "file_count": len(manifest["files"]),
    }


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ipod-root", type=Path, required=True)
    parser.add_argument(
        "--bundle-root",
        type=Path,
        default=repo / ".rockpod-private/desktop-mode-portable",
        help="directory containing one prebuilt bundle per platform",
    )
    parser.add_argument(
        "--launcher-root",
        type=Path,
        default=repo / "packaging/desktop-mode-portable",
    )
    parser.add_argument(
        "--platform",
        action="append",
        choices=sorted(PLATFORMS),
        dest="platforms",
        help="install one platform (repeatable); defaults to all",
    )
    args = parser.parse_args()
    try:
        result = install(
            args.ipod_root,
            args.bundle_root,
            args.launcher_root,
            args.platforms or list(PLATFORMS),
        )
    except (OSError, PortableDesktopError) as exc:
        parser.error(str(exc))
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
