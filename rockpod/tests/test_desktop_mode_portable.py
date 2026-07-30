"""Portable mounted-iPod Desktop Mode package tests."""

import json
from pathlib import Path

from tools.install_desktop_mode_portable import (
    PLATFORMS,
    REQUIRED_PLUGINS,
    install,
)


def _bundle(root: Path, platform: str) -> None:
    bundle = root / platform
    runtime = bundle / PLATFORMS[platform]
    runtime.parent.mkdir(parents=True)
    runtime.write_bytes(f"{platform}-runtime".encode())
    if runtime.name == "rockboxui":
        runtime.chmod(0o755)
    system = bundle / "simdisk"
    for relative in REQUIRED_PLUGINS:
        path = system / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(f"{platform}:{relative}".encode())


def test_portable_install_keeps_native_plugins_isolated(tmp_dir):
    root = Path(tmp_dir)
    device = root / "IPOD"
    hardware_plugin = device / ".rockbox/rocks/apps/desktop_mode.rock"
    hardware_plugin.parent.mkdir(parents=True)
    hardware_plugin.write_bytes(b"arm-plugin")
    assets = (
        device
        / ".rockbox/rocks/apps/desktop_mode_snow_leopard"
    )
    assets.mkdir()
    (assets / "manifest.json").write_text("{}\n", encoding="utf-8")
    (assets / "private.bmp").write_bytes(b"private-asset")

    bundles = root / "bundles"
    for platform in PLATFORMS:
        _bundle(bundles, platform)

    launchers = (
        Path(__file__).resolve().parents[2]
        / "packaging/desktop-mode-portable"
    )
    result = install(device, bundles, launchers, list(PLATFORMS))

    assert result["platforms"] == list(PLATFORMS)
    assert hardware_plugin.read_bytes() == b"arm-plugin"
    for platform, runtime in PLATFORMS.items():
        platform_root = device / ".rockbox/desktop-host" / platform
        assert (platform_root / runtime).is_file()
        assert (
            platform_root
            / "system-root/.rockbox/rocks/apps/desktop_mode.rock"
        ).read_bytes().startswith(platform.encode())
        assert (
            platform_root
            / "system-root/.rockbox/rocks/apps/sitekick.rock"
        ).read_bytes().startswith(platform.encode())
        assert (
            platform_root
            / "system-root/.rockbox/rocks/apps/netflix_desktop.rock"
        ).read_bytes().startswith(platform.encode())
        assert (
            platform_root
            / "system-root/.rockbox/rocks/viewers/openh264_player.rock"
        ).read_bytes().startswith(platform.encode())
        assert (
            platform_root
            / "system-root/.rockbox/rocks/apps/"
            "desktop_mode_snow_leopard/private.bmp"
        ).read_bytes() == b"private-asset"

    assert (device / "Open Desktop Mode.command").is_file()
    assert (device / "Open Desktop Mode.cmd").is_file()
    manifest = json.loads(
        (device / ".rockbox/desktop-host/manifest.json").read_text(
            encoding="utf-8"
        )
    )
    assert manifest["platforms"] == list(PLATFORMS)
    assert manifest["media_root"] == "mounted-iPod"


def test_portable_launchers_use_device_media_and_system_overlay():
    root = Path(__file__).resolve().parents[2]
    mac = (
        root / "packaging/desktop-mode-portable/Open Desktop Mode.command"
    ).read_text(encoding="utf-8")
    windows = (
        root / "packaging/desktop-mode-portable/Open Desktop Mode.cmd"
    ).read_text(encoding="utf-8")
    filesystem = (
        root / "uisimulator/common/filesystem-sim.c"
    ).read_text(encoding="utf-8")

    for launcher in (mac, windows):
        assert "ROCKBOX_SIM_SYSTEM_ROOT" in launcher
        assert "ROCKBOX_SIM_PLUGIN" in launcher
        assert "--root" in launcher
    assert 'getenv("ROCKBOX_SIM_SYSTEM_ROOT")' in filesystem
    assert '"/.rockbox/rocks"' in filesystem
    assert "sim_root_dir" in filesystem
