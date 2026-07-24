#!/usr/bin/env python3
"""Fail-closed host qualification for the N25 volatile diagnostic bundle."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import re
import struct
import subprocess
import tempfile
from pathlib import Path


UBOOT_REQUIRED = {
    "CONFIG_TARGET_N25_RAMDIAG": "y",
    "CONFIG_DFU_RAM": "y",
    "CONFIG_ENV_IS_NOWHERE": "y",
    "CONFIG_NO_NET": "y",
    "CONFIG_BOOTDELAY": "0",
    "CONFIG_CMD_BOOTM": "y",
    "CONFIG_CMD_DFU": "y",
    "CONFIG_TEXT_BASE": "0x22000000",
    "CONFIG_SYS_LOAD_ADDR": "0x08800000",
    "CONFIG_USB_GADGET_VENDOR_NUM": "0x05ac",
    "CONFIG_USB_GADGET_PRODUCT_NUM": "0x8007",
    "CONFIG_SYS_DFU_DATA_BUF_SIZE": "0x01800000",
}
FIT_STAGING_LOAD = 0x08800000
UBOOT_FORBIDDEN = {
    "CONFIG_ATA",
    "CONFIG_IDE",
    "CONFIG_SATA",
    "CONFIG_SCSI",
    "CONFIG_MMC",
    "CONFIG_MTD",
    "CONFIG_NAND",
    "CONFIG_SPI_FLASH",
    "CONFIG_USB_STORAGE",
    "CONFIG_USB_MASS_STORAGE",
    "CONFIG_DFU_MMC",
    "CONFIG_DFU_SCSI",
    "CONFIG_CMD_IDE",
    "CONFIG_CMD_MMC",
    "CONFIG_CMD_NAND",
    "CONFIG_CMD_SATA",
    "CONFIG_CMD_SF",
    "CONFIG_CMD_PART",
    "CONFIG_CMD_FAT",
    "CONFIG_CMD_EXT2",
    "CONFIG_CMD_EXT4",
    "CONFIG_CMD_FS_GENERIC",
    "CONFIG_CMD_MEMORY",
    "CONFIG_CMD_GO",
    "CONFIG_CMD_LOADB",
    "CONFIG_CMD_LOADS",
    "CONFIG_CMD_SOURCE",
    "CONFIG_CMD_SAVEENV",
    "CONFIG_CMD_BLOCK_CACHE",
}
LINUX_REQUIRED = {
    "CONFIG_ARCH_S5L87XX": "y",
    "CONFIG_CPU_S5L8702": "y",
    "CONFIG_CPU_ARM926T": "y",
    "CONFIG_USB_DWC2": "y",
    "CONFIG_USB_DWC2_PERIPHERAL": "y",
    "CONFIG_USB_GADGET": "y",
    "CONFIG_USB_G_SERIAL": "y",
    "CONFIG_BLK_DEV_INITRD": "y",
    "CONFIG_DEVTMPFS_MOUNT": "y",
    "CONFIG_PANIC_TIMEOUT": "5",
    "CONFIG_POWER_RESET": "y",
    "CONFIG_POWER_RESET_S5L8702": "y",
}
LINUX_FORBIDDEN = {
    "CONFIG_ARCH_MULTI_V7",
    "CONFIG_BLOCK",
    "CONFIG_ATA",
    "CONFIG_SCSI",
    "CONFIG_MMC",
    "CONFIG_MTD",
    "CONFIG_I2C",
    "CONFIG_SPI",
    "CONFIG_FB",
    "CONFIG_SOUND",
    "CONFIG_USB_MASS_STORAGE",
    "CONFIG_DEVMEM",
}
STORAGE_SYMBOL = re.compile(
    r"(?:^|_)(?:ata|ceata|ide|sata|scsi|mmc|mtd|nand|spi_nor|"
    r"usb_stor|mass_storage)(?:_|$)",
    re.IGNORECASE,
)
VFP_INSTRUCTION = re.compile(
    r"^\s*[0-9a-f]+:\s+[0-9a-f ]+\s+"
    r"(?:v(?:add|sub|mul|div|ldr|str|mov|cmp|cvt|push|pop|neg|abs|sqrt)\w*|"
    r"vmrs|vmsr|fld\w*|fst\w*|fadd\w*|fsub\w*|fmul\w*|fdiv\w*)\b",
    re.IGNORECASE | re.MULTILINE,
)
DT_STORAGE_NODE = re.compile(
    r"(?:ata|ceata|ide|sata|scsi|mmc|nand|flash|spi|i2c|storage)@",
    re.IGNORECASE,
)


class QualificationError(RuntimeError):
    pass


def run(*command: str) -> str:
    result = subprocess.run(command, check=True, text=True, capture_output=True)
    return result.stdout


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_config(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("CONFIG_") and "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
        elif line.startswith("# CONFIG_") and line.endswith(" is not set"):
            values[line[2 : -len(" is not set")]] = "n"
    return values


def check_config(
    path: Path,
    required: dict[str, str],
    forbidden: set[str],
    persistent_env: bool = False,
) -> None:
    config = parse_config(path)
    for key, expected in required.items():
        if config.get(key) != expected:
            raise QualificationError(f"{path}: {key} must equal {expected}")
    for key in forbidden:
        if config.get(key) not in (None, "n"):
            raise QualificationError(f"{path}: forbidden option enabled: {key}")
    if not persistent_env:
        for key, value in config.items():
            if key.startswith("CONFIG_ENV_IS_IN_") and value == "y":
                raise QualificationError(
                    f"{path}: persistent environment backend enabled: {key}"
                )


def check_elf_cpu(
    path: Path, readelf: str, objdump: str, expected_entry: int | None = None
) -> None:
    attributes = run(readelf, "-A", str(path))
    if "Tag_CPU_arch: v5TE" not in attributes:
        raise QualificationError(f"{path}: artifact is not tagged ARMv5TE")
    if re.search(r"Tag_CPU_arch: v(?:6|7|8)", attributes):
        raise QualificationError(f"{path}: incompatible ARM architecture tag")
    disassembly = run(objdump, "-d", str(path))
    match = VFP_INSTRUCTION.search(disassembly)
    if match:
        raise QualificationError(f"{path}: VFP instruction found: {match.group(0)}")
    if expected_entry is not None:
        header = run(readelf, "-h", str(path))
        entry = re.search(r"Entry point address:\s+(0x[0-9a-fA-F]+)", header)
        if entry is None or int(entry.group(1), 16) != expected_entry:
            actual = entry.group(1) if entry else "missing"
            raise QualificationError(
                f"{path}: entry point {actual} does not match DFU load address "
                f"0x{expected_entry:08x}"
            )


def check_symbols(path: Path, nm: str) -> None:
    symbols = run(nm, "--defined-only", str(path))
    names = set()
    for line in symbols.splitlines():
        name = line.rsplit(maxsplit=1)[-1] if line.split() else ""
        names.add(name)
        if STORAGE_SYMBOL.search(name):
            raise QualificationError(f"{path}: storage-capable symbol found: {name}")
    required = {"n25_dram_cold_init", "n25_dram_probe"}
    if path.name == "u-boot" and not required.issubset(names):
        missing = ", ".join(sorted(required - names))
        raise QualificationError(f"{path}: missing pre-relocation DRAM guard: {missing}")


def check_n25_dram_guard(path: Path, objdump: str) -> None:
    expected = {
        "n25_dram_cold_init": (
            "38100000",
            "0006105d",
            "001fb621",
            "0790682b",
            "00008040",
        ),
        "n25_dram_probe": (
            "0x8000000",
            "0xa000000",
            "55aa00ff",
            "aa55ff00",
            "0ff033cc",
            "f00fcc33",
        ),
        "board_early_init_f": (
            "<n25_dram_cold_init>",
            "<n25_dram_probe>",
        ),
    }
    for symbol, markers in expected.items():
        disassembly = run(objdump, "-d", f"--disassemble={symbol}", str(path))
        for marker in markers:
            if marker not in disassembly:
                raise QualificationError(
                    f"{path}: {symbol} is missing DRAM guard marker {marker}"
                )


def decompile_dtb(path: Path, dtc: str) -> str:
    return run(dtc, "-q", "-I", "dtb", "-O", "dts", str(path))


def check_dtb(path: Path, dtc: str, uboot: bool) -> None:
    source = decompile_dtb(path, dtc)
    required = (
        'compatible = "apple,n25", "samsung,s5l8702"',
        'rockpod,safety-profile = "ram-only-no-storage"',
        "reg = <0x8000000 0x4000000>",
    )
    for marker in required:
        if marker not in source:
            raise QualificationError(f"{path}: missing device-tree marker: {marker}")
    if DT_STORAGE_NODE.search(source):
        raise QualificationError(f"{path}: persistent-bus node found")
    if not uboot:
        if source.count('compatible = "apple,s5l8702-vic"') != 2:
            raise QualificationError(f"{path}: expected exactly two N25 VIC nodes")
        if 'compatible = "arm,pl192-vic"' in source:
            raise QualificationError(f"{path}: generic PL192 VIC path is forbidden")
    if not uboot and 'dr_mode = "peripheral"' not in source:
        raise QualificationError(f"{path}: Linux USB must be peripheral-only")
    if not uboot:
        for marker in (
            'compatible = "apple,s5l8702-watchdog-reset"',
            "apple,test-timeout-seconds = <0x3c>",
            "panic=5",
        ):
            if marker not in source:
                raise QualificationError(
                    f"{path}: missing volatile recovery marker: {marker}"
                )


def check_linux_reset(path: Path, nm: str) -> None:
    symbols = run(nm, "--defined-only", str(path))
    for marker in ("s5l8702_restart_probe", "s5l8702_restart_handler"):
        if marker not in symbols:
            raise QualificationError(
                f"{path}: mandatory reset implementation is not linked: {marker}"
            )


def parse_newc(data: bytes) -> dict[str, tuple[int, bytes]]:
    offset = 0
    entries: dict[str, tuple[int, bytes]] = {}
    while True:
        if data[offset : offset + 6] != b"070701":
            raise QualificationError("initramfs is not a valid newc archive")
        header = data[offset + 6 : offset + 110]
        if len(header) != 104:
            raise QualificationError("truncated newc header")
        fields = [int(header[index : index + 8], 16) for index in range(0, 104, 8)]
        mode = fields[1]
        size = fields[6]
        name_size = fields[11]
        offset += 110
        raw_name = data[offset : offset + name_size]
        if len(raw_name) != name_size or not raw_name.endswith(b"\0"):
            raise QualificationError("invalid newc pathname")
        name = raw_name[:-1].decode("utf-8")
        offset = (offset + name_size + 3) & ~3
        body = data[offset : offset + size]
        if len(body) != size:
            raise QualificationError("truncated newc body")
        offset = (offset + size + 3) & ~3
        if name == "TRAILER!!!":
            break
        entries[name.removeprefix("./")] = (mode, body)
    return entries


def check_initramfs(path: Path, readelf: str, objdump: str) -> None:
    entries = parse_newc(gzip.decompress(path.read_bytes()))
    if set(entries) != {"dev", "init", "proc", "sys"}:
        raise QualificationError(f"{path}: unexpected initramfs entries: {sorted(entries)}")
    mode, init = entries["init"]
    if mode & 0o170000 != 0o100000 or mode & 0o111 == 0:
        raise QualificationError(f"{path}: /init is not an executable regular file")
    if not init.startswith(b"\x7fELF"):
        raise QualificationError(f"{path}: /init is not ELF")
    if b"RAMDIAG HEARTBEAT" not in init or b"/dev/ttyGS0" not in init:
        raise QualificationError(f"{path}: heartbeat contract missing")
    for marker in (b"/dev/sd", b"/dev/mmc", b"/dev/mtd", b"/dev/hd"):
        if marker in init:
            raise QualificationError(f"{path}: storage path embedded in /init")
    with tempfile.NamedTemporaryFile(prefix="n25-init-", suffix=".elf") as handle:
        handle.write(init)
        handle.flush()
        check_elf_cpu(Path(handle.name), readelf, objdump)


def check_fit(path: Path, mkimage: str) -> None:
    listing = run(mkimage, "-l", str(path))
    markers = (
        "Rockpod N25 storage-free volatile diagnostic",
        "Default Configuration: 'n25-ramdiag'",
        "Load Address: 0x08008000",
        "Load Address: 0x0a800000",
        "Load Address: 0x0a900000",
    )
    for marker in markers:
        if marker not in listing:
            raise QualificationError(f"{path}: FIT marker missing: {marker}")
    if listing.count("Hash algo:    sha256") != 3:
        raise QualificationError(f"{path}: all three FIT images require SHA-256")


def check_dfu(path: Path, uboot_bin: Path) -> None:
    data = path.read_bytes()
    if len(data) < 0x800:
        raise QualificationError(f"{path}: truncated IMG1")
    header = struct.unpack("<4s3sBIIIII32sHH16s", data[:80])
    magic, version, image_format = header[:3]
    entrypoint, body_len, data_len, cert_offset, cert_len = header[3:8]
    if (magic, version, image_format) != (b"8702", b"1.0", 2):
        raise QualificationError(f"{path}: not an unsigned S5L8702 IMG1 v1 image")
    if entrypoint != 0 or data_len != body_len or cert_offset != body_len or cert_len:
        raise QualificationError(f"{path}: unexpected IMG1 execution or footer fields")
    if body_len > 0x3C000 or len(data) != 0x800 + body_len:
        raise QualificationError(
            f"{path}: image enters the top 16 KiB reserved for early stack/global data"
        )
    original = uboot_bin.read_bytes()
    body = data[0x800:]
    if body[: len(original)] != original or any(body[len(original) :]):
        raise QualificationError(f"{path}: IMG1 body does not match U-Boot")


def check_uboot_load_contract(path: Path) -> None:
    marker = b"dfu_alt_info=diagnostic ram 0x08800000 0x01800000\0"
    if marker not in path.read_bytes():
        raise QualificationError(
            f"{path}: DFU RAM destination does not match qualified FIT staging"
        )


def check_memory_layout(
    fit: Path, zimage: Path, image: Path, initramfs: Path, linux_dtb: Path,
    dtb_load: int = 0x0A900000,
    initramfs_load: int = 0x0A800000,
    fit_load: int = FIT_STAGING_LOAD,
) -> None:
    ranges = {
        "uncompressed kernel": (0x08008000, 0x08008000 + image.stat().st_size),
        "FIT staging": (fit_load, fit_load + fit.stat().st_size),
        "initramfs": (
            initramfs_load, initramfs_load + initramfs.stat().st_size
        ),
        "Linux DTB": (dtb_load, dtb_load + linux_dtb.stat().st_size),
    }
    if zimage.stat().st_size >= 0x01000000:
        raise QualificationError("compressed kernel exceeds its 16 MiB load window")
    ordered = sorted(ranges.items(), key=lambda item: item[1][0])
    for (name, (start, end)), (next_name, (next_start, _)) in zip(ordered, ordered[1:]):
        if end > next_start:
            raise QualificationError(f"RAM overlap: {name} reaches {next_name}")
    if ranges["Linux DTB"][1] >= 0x0B000000:
        raise QualificationError("payload enters the top 16 MiB U-Boot reserve")


def regular(path: str) -> Path:
    candidate = Path(path)
    if candidate.is_symlink():
        raise QualificationError(f"artifact must not be a symlink: {candidate}")
    resolved = candidate.resolve(strict=True)
    if not resolved.is_file():
        raise QualificationError(f"not a regular artifact: {resolved}")
    return resolved


def main(argv=None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--uboot-dir", required=True)
    parser.add_argument("--linux-dir", required=True)
    parser.add_argument("--initramfs", required=True)
    parser.add_argument("--fit", required=True)
    parser.add_argument("--dfu", required=True)
    parser.add_argument("--dtc", default="dtc")
    parser.add_argument("--readelf", default="arm-none-eabi-readelf")
    parser.add_argument("--objdump", default="arm-none-eabi-objdump")
    parser.add_argument("--nm", default="arm-none-eabi-nm")
    parser.add_argument("--mkimage")
    args = parser.parse_args(argv)

    uboot = Path(args.uboot_dir).resolve(strict=True)
    linux = Path(args.linux_dir).resolve(strict=True)
    artifacts = {
        "uboot_elf": regular(str(uboot / "u-boot")),
        "uboot_bin": regular(str(uboot / "u-boot.bin")),
        "uboot_dtb": regular(str(uboot / "arch/arm/dts/s5l8702-n25-ramdiag.dtb")),
        "linux_elf": regular(str(linux / "vmlinux")),
        "zimage": regular(str(linux / "arch/arm/boot/zImage")),
        "linux_image": regular(str(linux / "arch/arm/boot/Image")),
        "linux_dtb": regular(
            str(linux / "arch/arm/boot/dts/samsung/s5l8702-n25-ramdiag.dtb")
        ),
        "initramfs": regular(args.initramfs),
        "fit": regular(args.fit),
        "dfu": regular(args.dfu),
    }
    mkimage = args.mkimage or str(uboot / "tools/mkimage")

    check_config(uboot / ".config", UBOOT_REQUIRED, UBOOT_FORBIDDEN)
    check_config(linux / ".config", LINUX_REQUIRED, LINUX_FORBIDDEN)
    check_elf_cpu(
        artifacts["uboot_elf"], args.readelf, args.objdump, expected_entry=0x22000000
    )
    check_uboot_load_contract(artifacts["uboot_elf"])
    check_n25_dram_guard(artifacts["uboot_elf"], args.objdump)
    check_elf_cpu(artifacts["linux_elf"], args.readelf, args.objdump)
    check_symbols(artifacts["uboot_elf"], args.nm)
    check_symbols(artifacts["linux_elf"], args.nm)
    check_linux_reset(artifacts["linux_elf"], args.nm)
    check_dtb(artifacts["uboot_dtb"], args.dtc, True)
    check_dtb(artifacts["linux_dtb"], args.dtc, False)
    check_initramfs(artifacts["initramfs"], args.readelf, args.objdump)
    check_fit(artifacts["fit"], mkimage)
    check_dfu(artifacts["dfu"], artifacts["uboot_bin"])
    check_memory_layout(
        artifacts["fit"],
        artifacts["zimage"],
        artifacts["linux_image"],
        artifacts["initramfs"],
        artifacts["linux_dtb"],
    )

    report = {
        "artifact_gate_passed": True,
        "board": "apple-n25-ipod-classic-6g",
        "profile": "ram-only-no-storage",
        "hardware_actions_enabled": False,
        "automatic_recovery_seconds": 60,
        "watchdog_restart_compiled": True,
        "artifacts": {
            name: {
                "path": str(path),
                "size": path.stat().st_size,
                "sha256": sha256(path),
            }
            for name, path in artifacts.items()
        },
    }
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (QualificationError, subprocess.CalledProcessError) as error:
        print(json.dumps({"artifact_gate_passed": False, "error": str(error)}))
        raise SystemExit(1)
