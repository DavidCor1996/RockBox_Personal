#!/usr/bin/env python3
"""Qualify the N25 storage-free kernel and Eclair native RAM root bundle."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import re
import stat
import subprocess
import tempfile

from package_eclair_native_root import (
    CLICKWHEEL_KEY_LAYOUT,
    DATA_FILES,
    DIRECTORIES,
    FORBIDDEN_INIT_RC,
    LOCAL_FILES,
    RUNTIME_FILES,
)
from qualify_n25_ramdiag import (
    LINUX_FORBIDDEN,
    LINUX_REQUIRED,
    UBOOT_FORBIDDEN,
    UBOOT_REQUIRED,
    FIT_STAGING_LOAD,
    QualificationError,
    check_config,
    check_dfu,
    check_elf_cpu,
    check_memory_layout,
    check_n25_dram_guard,
    check_symbols,
    check_uboot_load_contract,
    decompile_dtb,
    parse_newc,
    regular,
    run,
    sha256,
)


ECLAIR_LINUX_REQUIRED = {
    **LINUX_REQUIRED,
    "CONFIG_ARM_THUMB": "y",
    "CONFIG_KUSER_HELPERS": "y",
    "CONFIG_MULTIUSER": "y",
    "CONFIG_FILE_LOCKING": "y",
    "CONFIG_COMPAT_32BIT_TIME": "y",
    "CONFIG_NET": "y",
    "CONFIG_UNIX": "y",
    "CONFIG_SYSVIPC": "y",
    "CONFIG_POSIX_MQUEUE": "y",
    "CONFIG_TMPFS": "y",
    "CONFIG_TMPFS_XATTR": "y",
    "CONFIG_ANDROID_BINDER_IPC": "y",
    "CONFIG_ANDROID_BINDER_DEVICES": '"binder"',
    "CONFIG_FB": "y",
    "CONFIG_FB_S5L8702": "y",
    "CONFIG_I2C": "y",
    "CONFIG_I2C_S5L8702": "y",
    "CONFIG_INPUT": "y",
    "CONFIG_INPUT_EVDEV": "y",
    "CONFIG_INPUT_MISC": "y",
    "CONFIG_INPUT_S5L8702_N25_CLICKWHEEL": "y",
    "CONFIG_POWER_RESET": "y",
    "CONFIG_POWER_RESET_S5L8702": "y",
    "CONFIG_PANIC_TIMEOUT": "5",
}
ECLAIR_LINUX_FORBIDDEN = {
    *(LINUX_FORBIDDEN - {"CONFIG_FB", "CONFIG_I2C"}),
    "CONFIG_ANDROID_BINDERFS",
    "CONFIG_ANDROID_BINDER_IPC_SELFTEST",
    "CONFIG_INET",
    "CONFIG_PACKET",
    "CONFIG_NETDEVICES",
    "CONFIG_NETWORK_FILESYSTEMS",
    "CONFIG_NFS_FS",
    "CONFIG_HID",
    "CONFIG_MEDIA_SUPPORT",
    "CONFIG_IO_URING",
    "CONFIG_BPF_SYSCALL",
}
DT_STORAGE_NODE = re.compile(
    r"(?:ata|ceata|ide|sata|scsi|mmc|nand|flash|spi|eeprom|storage)@",
    re.IGNORECASE,
)


def check_eclair_dtb(path: Path, dtc: str) -> None:
    source = decompile_dtb(path, dtc)
    required = (
        'compatible = "apple,n25", "samsung,s5l8702"',
        'rockpod,safety-profile = "eclair-native-ram-only-no-storage"',
        "reg = <0x8000000 0x4000000>",
        'dr_mode = "peripheral"',
        "androidboot.hardware=n25",
        'compatible = "apple,n25-lcd"',
        'reg-names = "lcd", "panel-strap"',
        'compatible = "samsung,s5l8702-i2c"',
        "i2c@3c600000",
        "reg = <0x3c600000 0x100>",
        "interrupts = <0x15>",
        "pmu-i2c = <",
        'compatible = "apple,n25-clickwheel"',
        'reg-names = "wheel", "pcon14", "pwrcon1"',
        "interrupts = <0x17>",
        'compatible = "apple,s5l8702-watchdog-reset"',
        "apple,test-timeout-seconds = <0xb4>",
        "panic=5",
        "linux,initrd-start = <0x9c00000>",
        "linux,initrd-end = <0xa933780>",
    )
    for marker in required:
        if marker not in source:
            raise QualificationError(f"{path}: missing device-tree marker: {marker}")
    if DT_STORAGE_NODE.search(source):
        raise QualificationError(f"{path}: persistent-bus node found")
    if source.count('compatible = "apple,s5l8702-vic"') != 2:
        raise QualificationError(f"{path}: expected exactly two N25 VIC nodes")
    if 'compatible = "arm,pl192-vic"' in source:
        raise QualificationError(f"{path}: generic PL192 VIC path is forbidden")
    if len(re.findall(r"\bi2c@[0-9a-f]+", source, re.IGNORECASE)) != 1:
        raise QualificationError(f"{path}: unexpected I2C controller count")
    i2c_node = re.search(
        r"i2c@3c600000\s*\{(?P<body>.*?)\n\s*\};",
        source,
        re.DOTALL,
    )
    if i2c_node is None or re.search(
        r"^\s*[a-zA-Z_][\w,-]*@[0-9a-f]+\s*\{",
        i2c_node.group("body"),
        re.MULTILINE | re.IGNORECASE,
    ):
        raise QualificationError(f"{path}: I2C child device is forbidden")


def check_rockbox_wrapper(wrapper: Path, payload: Path) -> None:
    body = payload.read_bytes()
    body += b"\0" * ((-len(body)) % 4)
    wrapped = wrapper.read_bytes()
    if len(wrapped) != len(body) + 8:
        raise QualificationError(f"{wrapper}: wrong wrapped size")
    if wrapped[4:8] != b"ip6g":
        raise QualificationError(f"{wrapper}: wrong Rockbox model tag")
    expected_sum = (71 + sum(body)) & 0xFFFFFFFF
    if int.from_bytes(wrapped[:4], "big") != expected_sum:
        raise QualificationError(f"{wrapper}: wrong Rockbox checksum")
    if wrapped[8:] != body:
        raise QualificationError(f"{wrapper}: wrapped payload mismatch")


def check_menu_play_bootloader(wrapper: Path, payload: Path, elf: Path,
                               readelf: str, objdump: str, nm: str) -> None:
    check_rockbox_wrapper(wrapper, payload)
    body = payload.read_bytes()
    for marker in (
        b"Android 2.0 RAM boot",
        b"/.rockbox/android/n25-eclair-kernel.ipod",
        b"/.rockbox/android/n25-eclair-initramfs.ipod",
        b"/.rockbox/android/n25-eclair-dtb.ipod",
        b"Menu+Play / no storage in Linux",
    ):
        if marker not in body:
            raise QualificationError(
                f"{wrapper}: bootloader marker missing: {marker.decode()}"
            )
    symbols = run(nm, str(elf))
    for symbol in ("n25_android_boot", "n25_android_linux_jump"):
        if symbol not in symbols:
            raise QualificationError(f"{elf}: missing {symbol}")
    sections = run(readelf, "-SW", str(elf))
    match = re.search(
        r"\]\s+\.bss\s+NOBITS\s+([0-9a-fA-F]+)\s+\S+\s+([0-9a-fA-F]+)",
        sections,
    )
    if match is None or int(match.group(1), 16) + int(match.group(2), 16) >= 0x09C00000:
        raise QualificationError(f"{elf}: bootloader BSS overlaps Android RAM root")
    disassembly = run(objdump, "-d", str(elf))
    jump = re.search(
        r"<n25_android_linux_jump>:(?P<body>.*?)(?:\n\n|$)",
        disassembly,
        re.DOTALL,
    )
    if jump is None:
        raise QualificationError(f"{elf}: Linux jump routine missing")
    for opcode in ("msr", "mrc", "mcr", "bx"):
        if opcode not in jump.group("body"):
            raise QualificationError(f"{elf}: Linux jump lacks {opcode}")


def check_android_elf(body: bytes, name: str, readelf: str) -> None:
    with tempfile.NamedTemporaryFile(prefix="eclair-root-", suffix=".elf") as handle:
        handle.write(body)
        handle.flush()
        attributes = run(readelf, "-A", handle.name)
        header = run(readelf, "-h", handle.name)
        programs = run(readelf, "-l", handle.name)
    if "Machine:                           ARM" not in header:
        raise QualificationError(f"initramfs {name}: not ARM ELF")
    if "Tag_CPU_arch: v5TE" not in attributes:
        raise QualificationError(f"initramfs {name}: not ARMv5TE")
    if "Tag_VFP_arch" in attributes:
        raise QualificationError(f"initramfs {name}: VFP ABI is forbidden")
    if name in (
        "system/bin/servicemanager",
        "system/bin/dalvikvm",
        "system/bin/dexopt",
        "system/bin/app_process",
        "system/bin/ipod6g_eclair_zygote_gate",
        "system/bin/ipod6g_eclair_probe_dynamic",
        "system/xbin/dexdump",
    ):
        if "Requesting program interpreter: /system/bin/linker" not in programs:
            raise QualificationError(f"initramfs {name}: wrong interpreter")
    elif name in ("init", "system/bin/linker", "system/bin/ipod6g_eclair_probe_static"):
        if "Requesting program interpreter" in programs:
            raise QualificationError(f"initramfs {name}: expected static ELF")


def check_eclair_initramfs(path: Path, root_report: Path, readelf: str) -> None:
    compressed = path.read_bytes()
    if len(compressed) < 10 or compressed[:3] != b"\x1f\x8b\x08":
        raise QualificationError(f"{path}: not gzip")
    if compressed[4:8] != b"\0\0\0\0":
        raise QualificationError(f"{path}: gzip timestamp is not deterministic")
    entries = parse_newc(gzip.decompress(compressed))
    expected = {
        "init.rc", *DIRECTORIES, *RUNTIME_FILES, *DATA_FILES, *LOCAL_FILES
    }
    if set(entries) != expected:
        raise QualificationError(
            f"{path}: unexpected Eclair root entries: {sorted(set(entries) ^ expected)}"
        )
    for name in DIRECTORIES:
        mode, body = entries[name]
        if not stat.S_ISDIR(mode) or body:
            raise QualificationError(f"{path}: {name} is not an empty directory")
    for name in {"init.rc", *RUNTIME_FILES, *DATA_FILES, *LOCAL_FILES}:
        mode, body = entries[name]
        if not stat.S_ISREG(mode):
            raise QualificationError(f"{path}: {name} is not a regular file")
        executable = name == "init" or name.startswith(
            ("system/bin/", "system/xbin/")
        )
        if executable != bool(mode & 0o111):
            raise QualificationError(f"{path}: incorrect executable mode for {name}")
        if name in RUNTIME_FILES:
            if not body.startswith(b"\x7fELF"):
                raise QualificationError(f"{path}: {name} is not ELF")
            check_android_elf(body, name, readelf)

    init_rc = entries["init.rc"][1].decode("utf-8")
    lowered = init_rc.lower()
    if lowered.count("mount tmpfs") != 3:
        raise QualificationError(f"{path}: mutable roots are not all tmpfs")
    if "mount rootfs rootfs / ro remount" not in lowered:
        raise QualificationError(f"{path}: rootfs is not remounted read-only")
    for term in FORBIDDEN_INIT_RC:
        if term in lowered:
            raise QualificationError(f"{path}: storage term in init.rc: {term}")
    if b"IPOD6G_ECLAIR_PROBE:PASS" not in entries[
        "system/bin/ipod6g_eclair_probe_dynamic"
    ][1]:
        raise QualificationError(f"{path}: probe completion marker missing")
    if b"/dev/binder" not in entries["system/bin/servicemanager"][1]:
        raise QualificationError(f"{path}: servicemanager Binder path missing")
    gralloc = entries["system/lib/hw/gralloc.default.so"][1]
    for marker in (
        b"IPOD6G_ECLAIR_HEADLESS_GRALLOC:READY",
        b"IPOD6G_ECLAIR_GRALLOC:NO_FB_DEVICE_USING_HEADLESS",
        b"IPOD6G_ECLAIR_GRALLOC:USING_LINUX_FRAMEBUFFER",
    ):
        if marker not in gralloc:
            raise QualificationError(
                f"{path}: gralloc path missing: {marker.decode()}"
            )
    if b"IPOD6G_ECLAIR_ZYGOTE_GATE:FRAMEWORK_READY" not in entries[
        "system/bin/ipod6g_eclair_zygote_gate"
    ][1]:
        raise QualificationError(f"{path}: Zygote sequencer marker missing")
    for marker in (
        "service zygotegate /system/bin/ipod6g_eclair_zygote_gate",
        "service installd /system/bin/installd",
        "socket installd stream 600 system system",
        "socket zygote stream 0666",
        "export IPOD6G_ASHMEM_TMPDIR /dev/ashmem-fallback",
        "mkdir /dev/ashmem-fallback 0777 root root",
    ):
        if marker.lower() not in lowered:
            raise QualificationError(f"{path}: Zygote init marker missing: {marker}")
    dex_fixture = entries["system/framework/ipod6g-dalvik-fixture.dex"][1]
    if not dex_fixture.startswith(b"dex\n035\0"):
        raise QualificationError(f"{path}: official DEX fixture header missing")
    if b"LdalvikExecTest/HelloWorld;" not in dex_fixture:
        raise QualificationError(f"{path}: official DEX fixture class missing")
    keylayout = entries[
        "system/usr/keylayout/iPod_Classic_Click_Wheel.kl"
    ][1]
    if keylayout != CLICKWHEEL_KEY_LAYOUT.read_bytes():
        raise QualificationError(f"{path}: Classic key layout differs from source")
    for marker in (
        b"key 28    DPAD_CENTER",
        b"key 103   DPAD_UP",
        b"key 108   DPAD_DOWN",
        b"key 158   BACK",
        b"key 164   MEDIA_PLAY_PAUSE",
    ):
        if marker not in keylayout:
            raise QualificationError(
                f"{path}: Classic key layout marker missing: {marker.decode()}"
            )

    report = json.loads(root_report.read_text(encoding="utf-8"))
    gates = (
        report.get("static_gate_passed") is True,
        report.get("emulation_gate_passed") is True,
        report.get("hardware_actions_enabled") is False,
        report.get("persistent_storage_nodes") is False,
        report.get("storage_tools") is False,
        report.get("root_read_only") is True,
        report.get("data_cache_metadata") == "tmpfs",
        report.get("dalvik_dex_parser_tested") is True,
        report.get("dalvik_vm_tested") is True,
        report.get("core_library_tested") is True,
        report.get("framework_library_tested") is True,
        report.get("framework_resource_qualified") is True,
        report.get("app_process_packaged") is True,
        report.get("zygote_native_runtime_packaged") is True,
        report.get("policy_library_packaged") is True,
        report.get("services_library_packaged") is True,
        report.get("system_server_native_runtime_packaged") is True,
        report.get("headless_gralloc_packaged") is True,
        report.get("software_renderer_packaged") is True,
        report.get("installd_packaged") is True,
        report.get("settings_provider_packaged") is True,
        report.get("launcher_packaged") is True,
        report.get("n25_clickwheel_keylayout_packaged") is True,
    )
    if not all(gates):
        raise QualificationError(f"{root_report}: root qualification gates failed")
    if report.get("initramfs", {}).get("sha256") != sha256(path):
        raise QualificationError(f"{root_report}: initramfs checksum mismatch")
    for name in {"init.rc", *RUNTIME_FILES, *DATA_FILES, *LOCAL_FILES}:
        record = report.get("runtime_files", {}).get(name, {})
        body = entries[name][1]
        if record.get("sha256") != hashlib.sha256(body).hexdigest():
            raise QualificationError(f"{root_report}: runtime checksum mismatch: {name}")


def check_eclair_fit(path: Path, mkimage: str) -> None:
    listing = run(mkimage, "-l", str(path))
    markers = (
        "Rockpod N25 storage-free Android 2.0 native diagnostic",
        "Default Configuration: 'n25-eclair-native'",
        "Load Address: 0x08008000",
        "Load Address: 0x09c00000",
        "Load Address: 0x0ad00000",
    )
    for marker in markers:
        if marker not in listing:
            raise QualificationError(f"{path}: FIT marker missing: {marker}")
    if listing.count("Hash algo:    sha256") != 3:
        raise QualificationError(f"{path}: all FIT images require SHA-256")


def check_n25_framebuffer_linked(path: Path) -> None:
    body = path.read_bytes()
    required = (
        b"apple,n25-lcd",
        b"N25 panel strap type",
        b"applying Rockbox Classic init",
        b"display PMU write",
        b"apple,n25-clickwheel",
        b"Rockbox-derived N25 click wheel ready",
        b"PCF50635 Hold read failed",
        b"Menu+Select held for 8 seconds",
        b"apple,s5l8702-watchdog-reset",
        b"Rockbox-derived S5L8702 restart handler ready",
        b"volatile test window expired after",
    )
    for marker in required:
        if marker not in body:
            raise QualificationError(
                f"{path}: N25 framebuffer is not linked: {marker.decode()}"
            )


def check_system_report(path: Path, initramfs: Path) -> dict:
    report = json.loads(path.read_text(encoding="utf-8"))
    required = {
        "scope": "android-2.0-native-full-system-emulation",
        "cpu": "ARM926EJ-S",
        "memory_mib": 64,
        "binder_protocol": 7,
        "binder_positive_path_tested": True,
        "android_init_stayed_alive": True,
        "system_boot_gate_passed": True,
        "persistent_storage_attached": False,
        "network_backend_attached": False,
        "hardware_actions_enabled": False,
        "dalvik_vm_tested": True,
        "core_library_tested": True,
        "framework_library_tested": True,
        "framework_resource_qualified": True,
        "app_process_tested": True,
        "zygote_accept_loop_tested": True,
        "policy_library_tested": True,
        "services_library_tested": True,
        "system_server_tested": True,
        "surfaceflinger_tested": True,
        "surfaceflinger_service_published": True,
        "headless_gralloc_tested": True,
        "software_renderer_tested": True,
        "system_server_ready": True,
        "launcher_tested": True,
        "launcher_dex_optimized": True,
    }
    for name, expected in required.items():
        if report.get(name) != expected:
            raise QualificationError(f"{path}: system-emulation gate failed: {name}")
    if report.get("artifacts", {}).get("initramfs", {}).get("sha256") != sha256(
        initramfs
    ):
        raise QualificationError(f"{path}: emulated initramfs checksum mismatch")
    return report


def check_input_report(path: Path) -> dict:
    report = json.loads(path.read_text(encoding="utf-8"))
    if report.get("scope") != "n25-clickwheel-host-model":
        raise QualificationError(f"{path}: wrong input-model scope")
    if report.get("wheel_positions") != 96 or report.get(
        "wheel_sensitivity"
    ) != 4:
        raise QualificationError(f"{path}: wrong wheel geometry")
    required_cases = {
        "clockwise_dpad_down",
        "clockwise_wrap",
        "counterclockwise_wrap",
        "hold_releases_and_suppresses",
        "initial_ack_button_format",
        "menu_select_eight_second_reset",
        "select_press_release",
        "three_pmu_errors_fail_closed",
    }
    cases = report.get("cases", {})
    if set(cases) != required_cases or not all(cases.values()):
        raise QualificationError(f"{path}: input-model case failed")
    if report.get("model_gate_passed") is not True:
        raise QualificationError(f"{path}: input-model gate failed")
    if report.get("persistent_storage_attached") is not False:
        raise QualificationError(f"{path}: input model attached storage")
    if report.get("hardware_actions_enabled") is not False:
        raise QualificationError(f"{path}: input model enabled hardware")
    return report


def main(argv=None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--uboot-dir", required=True)
    parser.add_argument("--linux-dir", required=True)
    parser.add_argument("--initramfs", required=True)
    parser.add_argument("--root-report", required=True)
    parser.add_argument("--system-report", required=True)
    parser.add_argument("--input-report", required=True)
    parser.add_argument("--fit", required=True)
    parser.add_argument("--dfu", required=True)
    parser.add_argument("--kernel-ipod", required=True)
    parser.add_argument("--initramfs-ipod", required=True)
    parser.add_argument("--dtb-ipod", required=True)
    parser.add_argument("--rockbox-bootloader", required=True)
    parser.add_argument("--rockbox-bootloader-dfu", required=True)
    parser.add_argument("--rockbox-bootloader-bin", required=True)
    parser.add_argument("--rockbox-bootloader-elf", required=True)
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
        "linux_dtb": regular(str(
            linux / "arch/arm/boot/dts/samsung/s5l8702-n25-eclair-native.dtb"
        )),
        "initramfs": regular(args.initramfs),
        "root_report": regular(args.root_report),
        "system_report": regular(args.system_report),
        "input_report": regular(args.input_report),
        "fit": regular(args.fit),
        "dfu": regular(args.dfu),
        "kernel_ipod": regular(args.kernel_ipod),
        "initramfs_ipod": regular(args.initramfs_ipod),
        "dtb_ipod": regular(args.dtb_ipod),
        "rockbox_bootloader": regular(args.rockbox_bootloader),
        "rockbox_bootloader_dfu": regular(args.rockbox_bootloader_dfu),
        "rockbox_bootloader_bin": regular(args.rockbox_bootloader_bin),
        "rockbox_bootloader_elf": regular(args.rockbox_bootloader_elf),
    }
    mkimage = args.mkimage or str(uboot / "tools/mkimage")

    check_config(uboot / ".config", UBOOT_REQUIRED, UBOOT_FORBIDDEN)
    check_config(linux / ".config", ECLAIR_LINUX_REQUIRED, ECLAIR_LINUX_FORBIDDEN)
    check_elf_cpu(
        artifacts["uboot_elf"], args.readelf, args.objdump, expected_entry=0x22000000
    )
    check_uboot_load_contract(artifacts["uboot_elf"])
    check_n25_dram_guard(artifacts["uboot_elf"], args.objdump)
    check_elf_cpu(artifacts["linux_elf"], args.readelf, args.objdump)
    check_symbols(artifacts["uboot_elf"], args.nm)
    check_symbols(artifacts["linux_elf"], args.nm)
    check_n25_framebuffer_linked(artifacts["linux_elf"])
    from qualify_n25_ramdiag import check_dtb

    check_dtb(artifacts["uboot_dtb"], args.dtc, True)
    check_eclair_dtb(artifacts["linux_dtb"], args.dtc)
    check_eclair_initramfs(
        artifacts["initramfs"], artifacts["root_report"], args.readelf
    )
    check_system_report(artifacts["system_report"], artifacts["initramfs"])
    check_input_report(artifacts["input_report"])
    check_eclair_fit(artifacts["fit"], mkimage)
    check_dfu(artifacts["dfu"], artifacts["uboot_bin"])
    check_rockbox_wrapper(artifacts["kernel_ipod"], artifacts["zimage"])
    check_rockbox_wrapper(artifacts["initramfs_ipod"], artifacts["initramfs"])
    check_rockbox_wrapper(artifacts["dtb_ipod"], artifacts["linux_dtb"])
    check_menu_play_bootloader(
        artifacts["rockbox_bootloader"], artifacts["rockbox_bootloader_bin"],
        artifacts["rockbox_bootloader_elf"], args.readelf, args.objdump, args.nm
    )
    check_dfu(artifacts["rockbox_bootloader_dfu"],
              artifacts["rockbox_bootloader_bin"])
    check_memory_layout(
        artifacts["fit"], artifacts["zimage"], artifacts["linux_image"],
        artifacts["initramfs"], artifacts["linux_dtb"],
        dtb_load=0x0AD00000, initramfs_load=0x09C00000
    )

    report = {
        "artifact_gate_passed": True,
        "android_native_gate_passed": True,
        "board": "apple-n25-ipod-classic-6g",
        "profile": "eclair-native-ram-only-no-storage",
        "aosp_tag": "android-2.0_r1",
        "binder_kernel_enabled": True,
        "binder_positive_path_tested": True,
        "dalvik_dex_parser_tested": True,
        "dalvik_vm_tested": True,
        "core_library_tested": True,
        "framework_library_tested": True,
        "framework_resource_qualified": True,
        "app_process_tested": True,
        "zygote_accept_loop_tested": True,
        "policy_library_tested": True,
        "services_library_tested": True,
        "system_server_tested": True,
        "surfaceflinger_tested": True,
        "surfaceflinger_service_published": True,
        "headless_gralloc_tested": True,
        "n25_framebuffer_compiled": True,
        "n25_framebuffer_handoff_only": False,
        "n25_panel_init_compiled": True,
        "n25_pcf50635_display_power_compiled": True,
        "n25_clickwheel_compiled": True,
        "n25_hold_switch_compiled": True,
        "n25_reset_chord_compiled": True,
        "n25_watchdog_restart_compiled": True,
        "n25_clickwheel_keylayout_packaged": True,
        "volatile_test_timeout_seconds": 180,
        "linux_framebuffer_gralloc_compiled": True,
        "physical_display_tested": False,
        "physical_input_tested": False,
        "physical_reset_tested": False,
        "software_renderer_tested": True,
        "system_server_ready": True,
        "launcher_tested": True,
        "launcher_dex_optimized": True,
        "fit_staging_load": f"0x{FIT_STAGING_LOAD:08x}",
        "full_system_emulation_gate_passed": True,
        "persistent_storage_available": False,
        "hardware_actions_enabled": False,
        "rockbox_menu_play_boot_packaged": True,
        "rockbox_boot_components_checksum_wrapped": True,
        "rockbox_menu_play_bootloader_volatile_dfu": True,
        "artifacts": {
            name: {
                "path": path.name,
                "size": path.stat().st_size,
                "sha256": sha256(path),
            }
            for name, path in artifacts.items()
            if not name.endswith("_elf")
        },
    }
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (QualificationError, json.JSONDecodeError,
            subprocess.CalledProcessError) as error:
        print(json.dumps({"artifact_gate_passed": False, "error": str(error)}))
        raise SystemExit(1)
