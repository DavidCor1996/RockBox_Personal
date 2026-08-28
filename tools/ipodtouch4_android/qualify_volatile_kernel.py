#!/usr/bin/env python3
"""Fail-closed source, config, and binary gate for the N81 RAM-only kernel."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


HERE = Path(__file__).resolve().parent
LOCK_PATH = HERE / "source-lock.json"
PATCH_PATH = HERE / "patches" / "idroid-kernel-n81-volatile.patch"
MARKER = ".rockpod-kernel-source-lock.json"
ZIMAGE_MAGIC_OFFSET = 0x24
ZIMAGE_MAGIC = bytes.fromhex("18286f01")
ZIMAGE_MAX = 8 * 1024 * 1024

REQUIRED_CONFIG = {
    "CONFIG_ARM": "y",
    "CONFIG_ARCH_S5L": "y",
    "CONFIG_PLAT_S5L": "y",
    "CONFIG_CPU_S5L8930": "y",
    "CONFIG_MACH_IPOD_TOUCH_4G": "y",
    "CONFIG_BLK_DEV_INITRD": "y",
    "CONFIG_RD_GZIP": "y",
    "CONFIG_DEVTMPFS": "y",
    "CONFIG_DEVTMPFS_MOUNT": "y",
    "CONFIG_TMPFS": "y",
    "CONFIG_FB": "y",
    "CONFIG_FB_S5L_CLCD": "y",
    "CONFIG_FRAMEBUFFER_CONSOLE": "y",
    "CONFIG_SERIAL_SAMSUNG": "y",
    "CONFIG_SERIAL_SAMSUNG_CONSOLE": "y",
    "CONFIG_SERIAL_S5L8900": "y",
    "CONFIG_STAGING": "y",
    "CONFIG_ANDROID": "y",
    "CONFIG_ANDROID_BINDER_IPC": "y",
    "CONFIG_ANDROID_LOGGER": "y",
    "CONFIG_ANDROID_LOW_MEMORY_KILLER": "y",
    "CONFIG_MODULES": "n",
    "CONFIG_BLOCK": "n",
    "CONFIG_MTD": "n",
    "CONFIG_MMC": "n",
    "CONFIG_SCSI": "n",
    "CONFIG_ATA": "n",
    "CONFIG_IDE": "n",
    "CONFIG_MD": "n",
    "CONFIG_S3C_DEV_HSMMC": "n",
    "CONFIG_DEVMEM": "n",
    "CONFIG_DEVKMEM": "n",
    "CONFIG_SWAP": "n",
    "CONFIG_PANIC_TIMEOUT": "5",
    "CONFIG_LOCALVERSION": '"-rockpod-n81-volatile"',
}
FORBIDDEN_CONFIG_PREFIXES = (
    "CONFIG_BLK_DEV_APPLE",
    "CONFIG_BLK_DEV_H2FMI",
    "CONFIG_BLK_DEV_S5L8900",
    "CONFIG_MTD_",
    "CONFIG_MMC_",
    "CONFIG_ATA_",
    "CONFIG_IDE_",
    "CONFIG_MD_",
    "CONFIG_EXT2_",
    "CONFIG_EXT3_",
    "CONFIG_EXT4_",
    "CONFIG_FAT_",
    "CONFIG_HFS_",
    "CONFIG_HFSPLUS_",
)
REQUIRED_SYMBOLS = {
    "start_kernel",
    "__mach_desc_IPOD_TOUCH_4G",
    "s5l8930_init",
    "s5l8930_map_io",
    "s5l8930_init_irq",
    "s5l8930_register_mipi_dsim",
    "s5l8930_register_clcd",
    "binder_init",
    "logger_init",
}
FORBIDDEN_SYMBOL_PATTERNS = (
    r"(?:^|_)h2fmi(?:_|$)",
    r"(?:^|_)(?:yaftl|vsvfl|legacy_vfl|apple_vfl|apple_ftl)(?:_|$)",
    r"(?:^|_)(?:nand|mtd|sdhci|mmc)(?:_|$)",
)


class QualificationError(RuntimeError):
    """Raised when the kernel cannot prove the volatile safety contract."""


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def run_text(command: list[str]) -> str:
    try:
        return subprocess.run(
            command, text=True, capture_output=True, check=True
        ).stdout
    except (OSError, subprocess.CalledProcessError) as error:
        raise QualificationError(f"command failed: {' '.join(command)}: {error}") from error


def parse_config(text: str) -> dict[str, str]:
    config: dict[str, str] = {}
    for line in text.splitlines():
        enabled = re.fullmatch(r"(CONFIG_[A-Z0-9_]+)=(.*)", line)
        disabled = re.fullmatch(r"# (CONFIG_[A-Z0-9_]+) is not set", line)
        if enabled:
            config[enabled.group(1)] = enabled.group(2)
        elif disabled:
            config[disabled.group(1)] = "n"
    return config


def check_prepared_source(source: Path) -> dict[str, object]:
    lock = json.loads(LOCK_PATH.read_text(encoding="utf-8"))
    try:
        marker = json.loads((source / MARKER).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise QualificationError(f"prepared-source marker is invalid: {error}") from error
    expected = {
        "kernel_commit": lock["idroid_kernel"]["commit"],
        "archive_sha256": lock["idroid_kernel"]["archive_sha256"],
        "patch_sha256": sha256(PATCH_PATH),
        "profile": "ipodtouch4g-n81-volatile-no-storage",
    }
    for name, value in expected.items():
        if marker.get(name) != value:
            raise QualificationError(f"prepared source has unexpected {name}")

    board = (source / "arch/arm/mach-s5l8930/mach-ipodtouch4g.c").read_text(
        encoding="utf-8"
    )
    makefile = (source / "arch/arm/mach-s5l8930/Makefile").read_text(
        encoding="utf-8"
    )
    kconfig = (source / "arch/arm/mach-s5l8930/Kconfig").read_text(
        encoding="utf-8"
    )
    cpu = (source / "arch/arm/mach-s5l8930/cpu.c").read_text(encoding="utf-8")
    forbidden_source = {
        "N81 board NAND registration": "s5l8930_register_h2fmi" in board,
        "NAND object selection": "dev-h2fmi.o" in makefile,
        "SD/MMC Kconfig selection": "select S3C_DEV_HSMMC" in kconfig,
        "SD/MMC platform registration": "s3c_sdhci0_set_platdata" in cpu,
    }
    present = [name for name, found in forbidden_source.items() if found]
    if present:
        raise QualificationError(f"prepared source enables storage: {present}")
    for required in (
        "MACHINE_START(IPOD_TOUCH_4G",
        "s5l8930_register_mipi_dsim",
        "s5l8930_register_clcd(&video_mode, 32, &clcd_info)",
    ):
        if required not in board:
            raise QualificationError(f"N81 board support lacks {required}")
    return lock


def qualify(
    source: Path,
    zimage: Path,
    vmlinux: Path,
    config_path: Path,
    prefix: str,
) -> dict[str, object]:
    lock = check_prepared_source(source)
    for path, label in (
        (zimage, "zImage"),
        (vmlinux, "vmlinux"),
        (config_path, "kernel config"),
    ):
        if path.is_symlink() or not path.is_file():
            raise QualificationError(f"{label} must be a regular, non-symlink file")

    config = parse_config(config_path.read_text(encoding="utf-8"))
    for name, expected in REQUIRED_CONFIG.items():
        if config.get(name, "n") != expected:
            raise QualificationError(f"kernel config has unexpected {name}")
    forbidden_enabled = sorted(
        name
        for name, value in config.items()
        if value in {"y", "m"}
        and any(name.startswith(prefix) for prefix in FORBIDDEN_CONFIG_PREFIXES)
    )
    if forbidden_enabled:
        raise QualificationError(
            f"kernel config enables storage interfaces: {forbidden_enabled}"
        )

    zimage_bytes = zimage.read_bytes()
    magic_end = ZIMAGE_MAGIC_OFFSET + len(ZIMAGE_MAGIC)
    if not zimage_bytes or len(zimage_bytes) > ZIMAGE_MAX:
        raise QualificationError("zImage is empty or exceeds the 8 MiB loader region")
    if zimage_bytes[ZIMAGE_MAGIC_OFFSET:magic_end] != ZIMAGE_MAGIC:
        raise QualificationError("kernel artifact is not an ARM zImage")

    header = run_text([prefix + "readelf", "-h", str(vmlinux)])
    if "Machine:                           ARM" not in header:
        raise QualificationError("vmlinux is not ARM")
    symbols_text = run_text([prefix + "nm", "-a", str(vmlinux)])
    symbols = {
        parts[-1]
        for line in symbols_text.splitlines()
        if len(parts := line.split()) >= 2
    }
    missing = REQUIRED_SYMBOLS - symbols
    if missing:
        raise QualificationError(f"kernel lacks required N81 symbols: {sorted(missing)}")
    forbidden = sorted(
        symbol
        for symbol in symbols
        if any(re.search(pattern, symbol, re.I) for pattern in FORBIDDEN_SYMBOL_PATTERNS)
    )
    if forbidden:
        raise QualificationError(f"kernel contains storage symbols: {forbidden}")
    image_strings = run_text(["strings", str(vmlinux)])
    for expected in ("Apple iPod Touch 4G", "3.0.8-rockpod-n81-volatile"):
        if expected not in image_strings:
            raise QualificationError(f"kernel lacks build marker {expected!r}")

    return {
        "schema": 1,
        "artifact_gate_passed": True,
        "board": "apple-n81-ipod-touch-4g",
        "product_type": "iPod4,1",
        "hardware_model": "N81AP",
        "machine_id": 3564,
        "profile": "eclair-volatile-ram-only-no-storage",
        "kernel_repository": lock["idroid_kernel"]["repository"],
        "kernel_commit": lock["idroid_kernel"]["commit"],
        "kernel_version": lock["idroid_kernel"]["version"],
        "storage_subsystems_configured": False,
        "storage_controller_symbols_present": False,
        "n81_display_compiled": True,
        "n81_serial_console_compiled": True,
        "initramfs_support_compiled": True,
        "android_binder_compiled": True,
        "android_logger_compiled": True,
        "android_lowmemorykiller_compiled": True,
        "hardware_actions_enabled": False,
        "device_test_ready": False,
        "physical_kernel_tested": False,
        "physical_android_tested": False,
        "artifacts": {
            "zimage": {"size": zimage.stat().st_size, "sha256": sha256(zimage)},
            "vmlinux": {"size": vmlinux.stat().st_size, "sha256": sha256(vmlinux)},
            "config": {
                "size": config_path.stat().st_size,
                "sha256": sha256(config_path),
            },
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("zimage", type=Path)
    parser.add_argument("vmlinux", type=Path)
    parser.add_argument("config", type=Path)
    parser.add_argument("--cross-prefix", default="arm-elf-eabi-")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        report = qualify(
            args.source.resolve(),
            args.zimage.resolve(),
            args.vmlinux.resolve(),
            args.config.resolve(),
            args.cross_prefix,
        )
    except (OSError, QualificationError) as error:
        parser.error(str(error))
    rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
