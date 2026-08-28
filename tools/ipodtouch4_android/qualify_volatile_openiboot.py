#!/usr/bin/env python3
"""Fail-closed source and binary gate for the N81 volatile OpeniBoot loader."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

from flatten_openiboot import FlattenError, flatten


HERE = Path(__file__).resolve().parent
LOCK_PATH = HERE / "source-lock.json"
PATCH_PATH = HERE / "patches" / "openiboot-ipodtouch4g-volatile.patch"
MARKER = ".rockpod-source-lock.json"

REQUIRED_SYMBOLS = {
    "OpenIBootStart",
    "acm_start",
    "boot_linux",
    "cmd_setup_boot",
    "cmd_setup_initrd",
    "cmd_setup_kernel",
    "cmd_n81_state",
    "usb_setup",
}
FORBIDDEN_SYMBOL_PATTERNS = (
    r"^(?:h2fmi|nand|mtd|vfl|ftl|yaftl)_",
    r"^(?:framebuffer|displaypipe|lcd_|mipi_dsim|pmu_|i2c_|spi_|dma_|cdma_)",
    r"^cmd_(?:mwb|mws|mw|go|jump)$",
    r"^cmd_(?:install|uninstall|mtd_write|saveenv)$",
    r"^images_(?:install|uninstall)$",
)
FORBIDDEN_VOLATILE_SOURCES = {
    "h2fmi.c",
    "sdio.c",
    "mtd.c",
    "nand.c",
    "vfl.c",
    "ftl.c",
    "nvram.c",
    "images.c",
    "scripting.c",
    "syscfg.c",
    "mipi_dsim.c",
    "clcd.c",
    "cdma.c",
    "pmu.c",
    "spi.c",
    "i2c.c",
    "framebuffer.c",
}
EXPECTED_VOLATILE_BASE_SOURCES = {
    "device.c",
    "actions.c",
    "commands.c",
    "malloc.c",
    "openiboot.c",
    "printf.c",
    "tasks.c",
    "util.c",
}
EXPECTED_VOLATILE_A4_SOURCES = {
    "a4.c",
    "chipid.c",
    "clock.c",
    "event.c",
    "gpio.c",
    "interrupt.c",
    "miu.c",
    "mmu.c",
    "power.c",
    "timer.c",
    "uart.c",
    "usbphy.c",
}
EXPECTED_LAYOUT = {
    "GeneralStack": 0x8402E000,
    "HeapStart": 0x84020000,
    "HeapEnd": 0x8402B000,
    "KERNEL_LOAD": 0x4A000000,
    "INITRD_LOAD": 0x4B000000,
    "VOLATILE_UPLOAD": 0x4D000000,
    "VOLATILE_KERNEL_MAX": 0x00800000,
    "VOLATILE_INITRD_MAX": 0x01400000,
}
EXPECTED_LOAD_ADDRESS = 0x84000000


class QualificationError(RuntimeError):
    """Raised when the loader does not prove the volatile safety contract."""


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


def parse_defines(text: str) -> dict[str, int]:
    values: dict[str, int] = {}
    for name, raw in re.findall(r"^#define\s+(\w+)\s+(0x[0-9A-Fa-f]+|\d+)\s*$", text, re.M):
        values[name] = int(raw, 0)
    return values


def volatile_source_names(script: str, variable: str) -> set[str]:
    match = re.search(
        rf"{re.escape(variable)}\s*=.*?Localize\(\[(.*?)\]\)",
        script,
        re.S,
    )
    if not match:
        raise QualificationError(f"{variable} source list is missing")
    return set(re.findall(r"['\"]([^'\"]+\.c)['\"]", match.group(1)))


def qualify(source: Path, elf: Path, binary: Path, prefix: str) -> dict[str, object]:
    lock = json.loads(LOCK_PATH.read_text(encoding="utf-8"))
    try:
        marker = json.loads((source / MARKER).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise QualificationError(f"prepared-source marker is invalid: {error}") from error
    if marker.get("openiboot_commit") != lock["openiboot"]["commit"]:
        raise QualificationError("prepared source has the wrong OpeniBoot commit")
    if marker.get("archive_sha256") != lock["openiboot"]["archive_sha256"]:
        raise QualificationError("prepared source has the wrong archive checksum")
    if marker.get("patch_sha256") != sha256(PATCH_PATH):
        raise QualificationError("prepared source has the wrong port patch")
    if marker.get("profile") != "ipodtouch4g-volatile-no-storage":
        raise QualificationError("prepared source has the wrong safety profile")
    if not elf.is_file() or not binary.is_file():
        raise QualificationError("ELF and raw binary are both required")

    header = run_text([prefix + "readelf", "-h", str(elf)])
    if "Machine:                           ARM" not in header:
        raise QualificationError("loader ELF is not ARM")
    if f"Entry point address:               0x{EXPECTED_LOAD_ADDRESS:x}" not in header:
        raise QualificationError("loader does not execute in place at SHAtter load address")
    symbols_text = run_text([prefix + "nm", "-a", str(elf)])
    symbol_addresses = {
        parts[-1]: int(parts[0], 16)
        for line in symbols_text.splitlines()
        if len(parts := line.split()) >= 3 and re.fullmatch(r"[0-9a-fA-F]+", parts[0])
    }
    if symbol_addresses.get("_start") != EXPECTED_LOAD_ADDRESS:
        raise QualificationError("loader _start differs from SHAtter load address")
    symbols = {
        parts[-1]
        for line in symbols_text.splitlines()
        if len(parts := line.split()) >= 2
    }
    missing = REQUIRED_SYMBOLS - symbols
    if missing:
        raise QualificationError(f"loader lacks required symbols: {sorted(missing)}")
    forbidden = sorted(
        symbol
        for symbol in symbols
        if any(re.search(pattern, symbol) for pattern in FORBIDDEN_SYMBOL_PATTERNS)
    )
    if forbidden:
        raise QualificationError(f"loader contains forbidden symbols: {forbidden}")

    disassembly = run_text([prefix + "objdump", "-d", str(elf)])
    platform_match = re.search(
        r"<platform_init>:\n(.*?)(?=\n[0-9a-f]+ <[^>]+>:\n)",
        disassembly,
        re.S,
    )
    if not platform_match:
        raise QualificationError("loader lacks disassembled platform_init")
    if any(
        f"<{name}>" in platform_match.group(1)
        for name in ("arm_setup", "mmu_setup")
    ):
        raise QualificationError("volatile platform_init still calls cache/MMU setup")
    if not re.search(r"\bmcr\s+15,\s*0,\s*r1,\s*cr12,\s*cr0", disassembly):
        raise QualificationError("volatile entry does not set VBAR to in-place vectors")

    strings = binary.read_bytes()
    for marker_text in (
        b"iPod Touch 4G volatile",
        b"ram-upload",
        b"866562f",
        b"N81STATE sctlr=%08x ttbr0=%08x vbar=%08x miu=%08x",
    ):
        if marker_text not in strings:
            raise QualificationError(f"loader lacks marker {marker_text!r}")
    for forbidden_text in (
        b"mtd_write",
        b"install openiboot onto the device",
        b"uninstall openiboot from the device",
        b"saveenv",
    ):
        if forbidden_text in strings:
            raise QualificationError(f"loader contains {forbidden_text!r}")

    root_source = (source / "SConstruct").read_text(encoding="utf-8")
    a4_source = (source / "plat-a4" / "SConscript").read_text(encoding="utf-8")
    volatile_base_sources = volatile_source_names(root_source, "volatile_base_src")
    volatile_a4_sources = volatile_source_names(a4_source, "volatile_plat_a4_src")
    volatile_sources = volatile_base_sources | volatile_a4_sources
    bad_sources = volatile_sources & FORBIDDEN_VOLATILE_SOURCES
    if bad_sources:
        raise QualificationError(f"volatile target includes forbidden sources: {sorted(bad_sources)}")
    if volatile_base_sources != EXPECTED_VOLATILE_BASE_SOURCES:
        raise QualificationError("volatile base source allowlist differs")
    if volatile_a4_sources != EXPECTED_VOLATILE_A4_SOURCES:
        raise QualificationError("volatile A4 source allowlist differs")
    if {"acm", "usb-synopsys", "CONFIG_VOLATILE_NO_STORAGE"} - set(
        re.findall(r"[A-Za-z0-9_-]+", a4_source)
    ):
        raise QualificationError("volatile target lacks USB or safety configuration")

    actions_source = (source / "actions.c").read_text(encoding="utf-8")
    for contract in (
        "kernel = (void*)KERNEL_LOAD",
        "ramdisk = (void*)INITRD_LOAD",
        "size > VOLATILE_KERNEL_MAX",
        "size > VOLATILE_INITRD_MAX",
        "memcpy((void*)KERNEL_LOAD, (void*)VOLATILE_UPLOAD",
        "memcpy((void*)INITRD_LOAD, (void*)VOLATILE_UPLOAD",
        'COMMAND("n81state", "Report fixed volatile handoff state.", cmd_n81_state)',
    ):
        if contract not in actions_source:
            raise QualificationError(f"loader lacks fixed upload contract: {contract}")

    platform_source = (source / "plat-a4" / "a4.c").read_text(encoding="utf-8")
    volatile_branch = platform_source.split(
        "#ifdef CONFIG_VOLATILE_NO_STORAGE", 1
    )[1].split("#else", 1)[0]
    expected_bootstrap_calls = (
        "tasks_setup();",
        "clock_setup();",
        "interrupt_setup();",
        "event_setup();",
        "LeaveCriticalSection();",
    )
    if any(call not in volatile_branch for call in expected_bootstrap_calls):
        raise QualificationError("volatile startup lacks its minimal USB prerequisites")
    for forbidden_call in (
        "arm_setup();",
        "mmu_setup();",
        "miu_setup();",
        "power_setup();",
        "gpio_setup();",
        "timer_setup();",
        "uart_setup();",
        "i2c_setup();",
        "dma_setup();",
        "spi_setup();",
        "aes_setup();",
        "displaypipe_init()",
        "pmu_setup_gpio(",
    ):
        if forbidden_call in volatile_branch:
            raise QualificationError(
                f"volatile startup still performs {forbidden_call} before USB"
            )

    defines = parse_defines(
        (source / "plat-a4" / "includes" / "hardware" / "a4.h").read_text(
            encoding="utf-8"
        )
    )
    for name, expected in EXPECTED_LAYOUT.items():
        if defines.get(name) != expected:
            raise QualificationError(f"unexpected {name} memory contract")
    heap_start = defines["HeapStart"]
    image_end = symbol_addresses.get("_end")
    if not (
        isinstance(image_end, int)
        and image_end <= heap_start < defines["HeapEnd"]
        and defines["HeapEnd"] <= defines["GeneralStack"] - 0x3000
        and defines["GeneralStack"] < 0x8402F198
        and defines["KERNEL_LOAD"] + defines["VOLATILE_KERNEL_MAX"]
        <= defines["INITRD_LOAD"]
        and defines["INITRD_LOAD"] + defines["VOLATILE_INITRD_MAX"]
        <= defines["VOLATILE_UPLOAD"]
        and defines["VOLATILE_UPLOAD"] + defines["VOLATILE_INITRD_MAX"]
        < 0x4FFF8000
    ):
        raise QualificationError("volatile RAM regions overlap")

    tasks_source = (source / "includes" / "tasks.h").read_text(encoding="utf-8")
    if "#define TASK_DEFAULT_STACK_SIZE (8*1024)" not in tasks_source:
        raise QualificationError("volatile ACM task does not use the bounded bootstrap stack")

    with tempfile.TemporaryDirectory(prefix="n81-flatten-") as temporary:
        rebuilt = Path(temporary) / "loader.bin"
        try:
            rebuilt.write_bytes(flatten(elf.read_bytes()))
        except (OSError, FlattenError) as error:
            raise QualificationError(f"cannot flatten loader ELF: {error}") from error
        if rebuilt.read_bytes() != binary.read_bytes():
            raise QualificationError(
                "raw loader is not the exact canonical OpeniBoot ELF flattening"
            )

    return {
        "schema": 1,
        "artifact_gate_passed": True,
        "board": "apple-n81-ipod-touch-4g",
        "product_type": "iPod4,1",
        "hardware_model": "N81AP",
        "machine_id": 3564,
        "profile": "eclair-volatile-ram-only-no-storage",
        "openiboot_commit": lock["openiboot"]["commit"],
        "storage_controller_symbols_present": False,
        "storage_write_commands_present": False,
        "bounded_heap": True,
        "fixed_kernel_initrd_regions": True,
        "usb_ram_upload_compiled": True,
        "linux_atag_handoff_compiled": True,
        "load_address": EXPECTED_LOAD_ADDRESS,
        "in_place_shatter_execution": True,
        "in_place_exception_vectors": True,
        "hardware_actions_enabled": False,
        "device_test_ready": False,
        "physical_loader_tested": False,
        "physical_linux_tested": False,
        "physical_android_tested": False,
        "artifacts": {
            "elf": {"size": elf.stat().st_size, "sha256": sha256(elf)},
            "bin": {"size": binary.stat().st_size, "sha256": sha256(binary)},
        },
        "memory": {name: value for name, value in EXPECTED_LAYOUT.items()},
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("elf", type=Path)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--cross-prefix", default="arm-elf-eabi-")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        report = qualify(
            args.source.resolve(),
            args.elf.resolve(),
            args.binary.resolve(),
            args.cross_prefix,
        )
    except QualificationError as error:
        parser.error(str(error))
    rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
