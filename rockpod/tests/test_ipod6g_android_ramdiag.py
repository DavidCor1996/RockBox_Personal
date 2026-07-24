"""Fail-closed tests for the N25 storage-free qualification logic."""

from __future__ import annotations

import gzip
import importlib.util
import stat
import struct
from pathlib import Path

import pytest


MODULE_PATH = (
    Path(__file__).resolve().parents[2]
    / "tools"
    / "ipod6g_android"
    / "qualify_n25_ramdiag.py"
)
SPEC = importlib.util.spec_from_file_location("qualify_n25_ramdiag", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def test_config_gate_rejects_storage_capability(tmp_path):
    config = tmp_path / ".config"
    config.write_text(
        "CONFIG_ARCH_S5L87XX=y\nCONFIG_CPU_S5L8702=y\nCONFIG_ATA=y\n",
        encoding="utf-8",
    )

    with pytest.raises(MODULE.QualificationError, match="forbidden option"):
        MODULE.check_config(
            config,
            {"CONFIG_ARCH_S5L87XX": "y", "CONFIG_CPU_S5L8702": "y"},
            {"CONFIG_ATA"},
        )


def test_regular_rejects_symlink(tmp_path):
    artifact = tmp_path / "artifact"
    alias = tmp_path / "alias"
    artifact.write_bytes(b"safe")
    alias.symlink_to(artifact)

    with pytest.raises(MODULE.QualificationError, match="must not be a symlink"):
        MODULE.regular(str(alias))


def test_reproducible_initramfs_has_only_four_expected_entries(tmp_path):
    output = tmp_path / "initramfs.cpio.gz"
    builder = MODULE_PATH.with_name("build_ramdiag_initramfs.sh")
    MODULE.subprocess.run([str(builder), str(output)], check=True)

    entries = MODULE.parse_newc(gzip.decompress(output.read_bytes()))
    assert set(entries) == {"dev", "init", "proc", "sys"}
    mode, init = entries["init"]
    assert stat.S_ISREG(mode)
    assert mode & 0o111
    assert b"RAMDIAG HEARTBEAT" in init
    assert b"/dev/ttyGS0" in init
    assert not any(marker in init for marker in (b"/dev/sd", b"/dev/mmc", b"/dev/mtd"))


def test_dfu_gate_reserves_top_16k_of_iram_for_early_stack(tmp_path):
    uboot = tmp_path / "u-boot.bin"
    dfu = tmp_path / "u-boot.dfu"
    body = b"U" * (0x3C000 + 1)
    uboot.write_bytes(body)
    header = struct.pack(
        "<4s3sBIIIII32sHH16s",
        b"8702",
        b"1.0",
        2,
        0,
        len(body),
        len(body),
        len(body),
        0,
        b"\0" * 32,
        0,
        0,
        b"\0" * 16,
    )
    dfu.write_bytes(header + b"\0" * (0x800 - len(header)) + body)

    with pytest.raises(MODULE.QualificationError, match="top 16 KiB"):
        MODULE.check_dfu(dfu, uboot)


def test_uboot_patch_uses_full_iram_and_disables_memory_write_command():
    patch = MODULE_PATH.parent / "patches" / "uboot-s5l8702-n25-ramdiag.patch"
    text = patch.read_text(encoding="utf-8")

    assert "CFG_SYS_INIT_RAM_SIZE   0x00040000" in text
    assert "+CONFIG_TEXT_BASE=0x22000000" in text
    assert "+CONFIG_POSITION_INDEPENDENT=y" not in text
    assert "+CONFIG_BOOTDELAY=0" in text
    assert "+# CONFIG_CMD_MEMORY is not set" in text
    assert "+void n25_dram_cold_init(void)" in text
    assert "+int n25_dram_probe(void)" in text
    assert "+#define N25_MIU_BASE           0x38100000" in text
    assert "+    writel(0x6105d, N25_MIUAREF);" in text
    assert "+    writel(0x1fb621, N25_MIUSDPARA);" in text
    assert "+    n25_dram_cold_init();" in text


def test_ramdiag_has_bounded_soc_reset_recovery():
    tools = MODULE_PATH.parent
    config = (
        tools
        / "linux-overlay"
        / "arch"
        / "arm"
        / "configs"
        / "apple_n25_ramdiag_defconfig"
    ).read_text(encoding="utf-8")
    dts = (
        tools
        / "linux-overlay"
        / "arch"
        / "arm"
        / "boot"
        / "dts"
        / "samsung"
        / "s5l8702-n25-ramdiag.dts"
    ).read_text(encoding="utf-8")
    builder = (tools / "build_n25_ramdiag_bundle.sh").read_text(encoding="utf-8")

    assert "CONFIG_POWER_RESET_S5L8702=y" in config
    assert "CONFIG_PANIC_TIMEOUT=5" in config
    assert 'compatible = "apple,s5l8702-watchdog-reset"' in dts
    assert "apple,test-timeout-seconds = <60>" in dts
    assert "linux-s5l8702-n25-input-reset.patch" in builder


def test_compressed_stage0_is_fail_closed_and_instruction_qualified():
    tools = MODULE_PATH.parent
    builder = (tools / "build_dfu_stage0.sh").read_text(encoding="utf-8")
    emulator = (tools / "emulate_n25_dfu_stage0.py").read_text(
        encoding="utf-8"
    )

    assert "stage0_size > 114688" in builder
    assert 'body_len > 114688' in builder
    assert "emulate_n25_dfu_stage0.py" in builder
    assert "reconstructed_uboot_matches" in builder
    assert 'device_test_ready = checkpoint == "boot" and emulation is not None' in builder
    assert '"CONFIG_ENV_IS_NOWHERE": "y"' in builder
    assert '"CONFIG_DFU_RAM": "y"' in builder
    assert '"CONFIG_USB_GADGET_PRODUCT_NUM": "0x8007"' in builder
    assert '"CONFIG_USB_FUNCTION_MASS_STORAGE"' in builder
    assert 'name.startswith("CONFIG_ENV_IS_IN_")' in builder

    assert "UC_ARCH_ARM" in emulator
    assert "DRAM_STAGING" in emulator
    assert "RELOCATOR_ADDRESS" in emulator
    assert "stage zero never reached the reconstructed U-Boot entry" in emulator
    assert "reconstructed U-Boot differs from the qualified binary" in emulator
    assert "Rockbox-derived N25 DRAM initialization is incomplete" in emulator
