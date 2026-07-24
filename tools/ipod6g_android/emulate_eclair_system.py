#!/usr/bin/env python3
"""Boot the storage-free Eclair native root in a generic ARM926 VM."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


class EmulationError(RuntimeError):
    """The full-system Android native-core gate did not pass."""


REQUIRED_CONFIG = {
    "CONFIG_ARCH_VERSATILE": "y",
    "CONFIG_CPU_ARM926T": "y",
    "CONFIG_ARM_THUMB": "y",
    "CONFIG_KUSER_HELPERS": "y",
    "CONFIG_BINFMT_ELF": "y",
    "CONFIG_FUTEX": "y",
    "CONFIG_FILE_LOCKING": "y",
    "CONFIG_COMPAT_32BIT_TIME": "y",
    "CONFIG_MULTIUSER": "y",
    "CONFIG_BLK_DEV_INITRD": "y",
    "CONFIG_SHMEM": "y",
    "CONFIG_NET": "y",
    "CONFIG_UNIX": "y",
    "CONFIG_TMPFS": "y",
    "CONFIG_ANDROID_BINDER_IPC": "y",
    "CONFIG_ANDROID_BINDER_DEVICES": '"binder"',
}
FORBIDDEN_CONFIG = {
    "CONFIG_BLOCK",
    "CONFIG_ATA",
    "CONFIG_SCSI",
    "CONFIG_MMC",
    "CONFIG_MTD",
    "CONFIG_INET",
    "CONFIG_PACKET",
    "CONFIG_NETDEVICES",
    "CONFIG_INPUT",
    "CONFIG_I2C",
    "CONFIG_SPI",
    "CONFIG_DRM",
    "CONFIG_FB",
    "CONFIG_SOUND",
    "CONFIG_HID",
    "CONFIG_IO_URING",
    "CONFIG_BPF_SYSCALL",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def regular(path: Path) -> Path:
    resolved = path.resolve(strict=True)
    if path.is_symlink() or not resolved.is_file():
        raise EmulationError(f"not a regular file: {path}")
    return resolved


def parse_config(path: Path) -> dict[str, str]:
    values = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        match = re.fullmatch(r"(CONFIG_[A-Z0-9_]+)=(.*)", line)
        if match:
            values[match.group(1)] = match.group(2)
    return values


def main(argv=None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", type=Path, required=True)
    parser.add_argument("--kernel-build", type=Path, required=True)
    parser.add_argument("--initramfs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--transcript-output", type=Path)
    parser.add_argument("--timeout", type=int, default=20)
    args = parser.parse_args(argv)

    qemu = regular(args.qemu)
    build = args.kernel_build.resolve(strict=True)
    kernel = regular(build / "arch/arm/boot/zImage")
    dtb = regular(build / "arch/arm/boot/dts/arm/versatile-pb.dtb")
    config_path = regular(build / ".config")
    initramfs = regular(args.initramfs)
    output = args.output.resolve()
    if output.exists() or output.is_symlink():
        raise EmulationError(f"refusing to overwrite report: {output}")
    transcript_output = (
        args.transcript_output.resolve() if args.transcript_output else None
    )
    if transcript_output is not None and (
        transcript_output.exists() or transcript_output.is_symlink()
    ):
        raise EmulationError(
            f"refusing to overwrite transcript: {transcript_output}"
        )

    config = parse_config(config_path)
    for name, expected in REQUIRED_CONFIG.items():
        if config.get(name) != expected:
            raise EmulationError(f"required kernel setting missing: {name}={expected}")
    for name in FORBIDDEN_CONFIG:
        if name in config:
            raise EmulationError(f"forbidden kernel setting enabled: {name}")

    version = subprocess.run(
        [str(qemu), "--version"], text=True, capture_output=True,
        timeout=5, check=True,
    ).stdout.splitlines()[0]
    command = [
        str(qemu),
        "-M", "versatilepb",
        "-cpu", "arm926",
        "-m", "64M",
        "-display", "none",
        "-monitor", "none",
        "-serial", "stdio",
        "-no-reboot",
        "-nic", "none",
        "-kernel", str(kernel),
        "-dtb", str(dtb),
        "-initrd", str(initramfs),
        "-append",
        "console=ttyAMA0,115200 rdinit=/init androidboot.hardware=versatile "
        "androidboot.console=ttyAMA0 loglevel=6 panic=-1",
    ]
    try:
        result = subprocess.run(
            command, text=True, capture_output=True, timeout=args.timeout,
            check=False,
        )
        timed_out = False
    except subprocess.TimeoutExpired as error:
        stdout = error.stdout or ""
        stderr = error.stderr or ""
        if isinstance(stdout, bytes):
            stdout = stdout.decode("utf-8", errors="replace")
        if isinstance(stderr, bytes):
            stderr = stderr.decode("utf-8", errors="replace")
        result = subprocess.CompletedProcess(command, 124, stdout, stderr)
        timed_out = True

    transcript = (result.stdout or "") + (result.stderr or "")
    if transcript_output is not None:
        transcript_output.parent.mkdir(parents=True, exist_ok=True)
        transcript_output.write_text(transcript, encoding="utf-8")
    required_markers = (
        "Linux version 6.14.0+",
        "process '/init' started with executable stack",
        "IPOD6G_ECLAIR_BINDER:CONTEXT_MANAGER_READY",
        "DEX version '035'",
        "Class descriptor  : 'LdalvikExecTest/HelloWorld;'",
        "Hello Android World!",
        "Android Framework Java OK!",
        "IPOD6G_ECLAIR_ZYGOTE_GATE:BEGIN",
        "IPOD6G_ECLAIR_ZYGOTE_GATE:FRAMEWORK_READY",
        ">>>>>>>>>>>>>> AndroidRuntime START <<<<<<<<<<<<<<",
        "AndroidRuntime: --- registering native functions ---",
        "dalvikvm: System server process",
        "sysproc: Entered system_init()",
        "SurfaceFlinger: SurfaceFlinger is starting",
        "IPOD6G_ECLAIR_SURFACEFLINGER_SERVICE:PUBLISHED",
        "IPOD6G_ECLAIR_GRALLOC:NO_FB_DEVICE_USING_HEADLESS",
        "IPOD6G_ECLAIR_HEADLESS_GRALLOC:READY 320x240 RGB565",
        "libEGL: loaded /system/lib/egl/libGLES_android.so",
        "Android PixelFlinger 1.1",
        "System server: starting Android runtime.",
        "System server: starting Android services.",
        "SystemServer: Entered the Android system server!",
        "IPOD6G_ECLAIR_SYSTEM_SERVER:MINIMAL_PROFILE",
        "IPOD6G_ECLAIR_BATTERY:STUB_READY",
        "IPOD6G_ECLAIR_ACCESSIBILITY:READY",
        "IPOD6G_ECLAIR_SYSTEM_SERVER:READY",
        "installd: DexInv: --- END '/system/app/RockpodLauncher.apk' (success) ---",
        "IPOD6G_ECLAIR_LAUNCHER:ON_CREATE",
        "Zygote: Accepting command socket connections",
        "IPOD6G_ECLAIR_PROBE:PASS",
    )
    for marker in required_markers:
        if marker not in transcript:
            raise EmulationError(f"system transcript missing marker: {marker}")
    forbidden_markers = (
        "Kernel panic",
        "Segmentation fault",
        "binder_write: ioctl failed",
        "FAILED BINDER TRANSACTION",
        "cannot become context manager",
        "binder: cannot open device",
        "IPOD6G_ECLAIR_ZYGOTE_GATE:FRAMEWORK_FAILED",
        "IPOD6G_ECLAIR_ZYGOTE_GATE:EXEC_FAILED",
        "IPOD6G_ECLAIR_SURFACEFLINGER_SERVICE:FAILED",
        "SurfaceFlinger not published, waiting",
        "Zygote died with exception",
        "Exit zygote because system server",
        "Couldn't get gralloc module",
        "*** EXCEPTION IN SYSTEM PROCESS",
        "FATAL EXCEPTION",
    )
    for marker in forbidden_markers:
        if marker in transcript:
            raise EmulationError(f"system transcript contains failure: {marker}")
    if not timed_out:
        raise EmulationError("QEMU exited instead of keeping Android init alive")

    report = {
        "schema": 1,
        "scope": "android-2.0-native-full-system-emulation",
        "machine": "versatilepb",
        "cpu": "ARM926EJ-S",
        "memory_mib": 64,
        "qemu": {"version": version, "sha256": sha256(qemu)},
        "artifacts": {
            "kernel": {"sha256": sha256(kernel), "size": kernel.stat().st_size},
            "dtb": {"sha256": sha256(dtb), "size": dtb.stat().st_size},
            "initramfs": {
                "sha256": sha256(initramfs), "size": initramfs.stat().st_size,
            },
            "config": {"sha256": sha256(config_path)},
        },
        "required_markers": list(required_markers),
        "binder_protocol": 7,
        "binder_positive_path_tested": True,
        "dalvik_dex_parser_tested": True,
        "dalvik_vm_tested": True,
        "core_library_tested": True,
        "framework_library_tested": True,
        "framework_resource_qualified": True,
        "policy_library_tested": True,
        "app_process_tested": True,
        "zygote_accept_loop_tested": True,
        "services_library_tested": True,
        "system_server_tested": True,
        "surfaceflinger_tested": True,
        "surfaceflinger_service_published": True,
        "headless_gralloc_tested": True,
        "software_renderer_tested": True,
        "system_server_ready": True,
        "launcher_tested": True,
        "launcher_dex_optimized": True,
        "android_init_stayed_alive": True,
        "system_boot_gate_passed": True,
        "persistent_storage_attached": False,
        "network_backend_attached": False,
        "hardware_actions_enabled": False,
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                      encoding="utf-8")
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, EmulationError, subprocess.SubprocessError) as error:
        print(json.dumps({"system_boot_gate_passed": False, "error": str(error)}))
        raise SystemExit(1)
