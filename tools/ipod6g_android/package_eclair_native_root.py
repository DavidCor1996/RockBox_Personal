#!/usr/bin/env python3
"""Create and emulate a deterministic, storage-free Eclair native root."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import struct
import subprocess
import tempfile
import zipfile


SCRIPT_DIR = Path(__file__).resolve().parent
INIT_RC = SCRIPT_DIR / "eclair/root/init.rc"
CLICKWHEEL_KEY_LAYOUT = (
    SCRIPT_DIR / "eclair/root/iPod_Classic_Click_Wheel.kl"
)
RUNTIME_FILES = {
    "init": "root/init",
    "system/bin/linker": "system/bin/linker",
    "system/bin/servicemanager": "system/bin/servicemanager",
    "system/bin/installd": "system/bin/installd",
    "system/bin/dalvikvm": "system/bin/dalvikvm",
    "system/bin/dexopt": "system/bin/dexopt",
    "system/bin/app_process": "system/bin/app_process",
    "system/bin/ipod6g_eclair_zygote_gate":
        "system/bin/ipod6g_eclair_zygote_gate",
    "system/bin/ipod6g_eclair_probe_dynamic":
        "system/bin/ipod6g_eclair_probe_dynamic",
    "system/bin/ipod6g_eclair_probe_static":
        "system/bin/ipod6g_eclair_probe_static",
    "system/xbin/dexdump": "system/xbin/dexdump",
    "system/lib/libc.so": "system/lib/libc.so",
    "system/lib/libdl.so": "system/lib/libdl.so",
    "system/lib/liblog.so": "system/lib/liblog.so",
    "system/lib/libcutils.so": "system/lib/libcutils.so",
    "system/lib/libstdc++.so": "system/lib/libstdc++.so",
    "system/lib/libm.so": "system/lib/libm.so",
    "system/lib/libz.so": "system/lib/libz.so",
    "system/lib/libdvm.so": "system/lib/libdvm.so",
    "system/lib/libnativehelper.so": "system/lib/libnativehelper.so",
    "system/lib/libexpat.so": "system/lib/libexpat.so",
    "system/lib/libcrypto.so": "system/lib/libcrypto.so",
    "system/lib/libssl.so": "system/lib/libssl.so",
    "system/lib/libutils.so": "system/lib/libutils.so",
    "system/lib/libicudata.so": "system/lib/libicudata.so",
    "system/lib/libicuuc.so": "system/lib/libicuuc.so",
    "system/lib/libicui18n.so": "system/lib/libicui18n.so",
    "system/lib/libsqlite.so": "system/lib/libsqlite.so",
    "system/lib/libbinder.so": "system/lib/libbinder.so",
    "system/lib/libnetutils.so": "system/lib/libnetutils.so",
    "system/lib/libhardware.so": "system/lib/libhardware.so",
    "system/lib/libhardware_legacy.so": "system/lib/libhardware_legacy.so",
    "system/lib/libwpa_client.so": "system/lib/libwpa_client.so",
    "system/lib/libsonivox.so": "system/lib/libsonivox.so",
    "system/lib/libemoji.so": "system/lib/libemoji.so",
    "system/lib/libskia.so": "system/lib/libskia.so",
    "system/lib/libEGL.so": "system/lib/libEGL.so",
    "system/lib/libGLESv1_CM.so": "system/lib/libGLESv1_CM.so",
    "system/lib/libpixelflinger.so": "system/lib/libpixelflinger.so",
    "system/lib/libui.so": "system/lib/libui.so",
    "system/lib/libmedia.so": "system/lib/libmedia.so",
    "system/lib/libskiagl.so": "system/lib/libskiagl.so",
    "system/lib/libandroid_runtime.so": "system/lib/libandroid_runtime.so",
    "system/lib/libsurfaceflinger.so": "system/lib/libsurfaceflinger.so",
    "system/lib/libsystem_server.so": "system/lib/libsystem_server.so",
    "system/lib/libandroid_servers.so": "system/lib/libandroid_servers.so",
    "system/lib/hw/gralloc.default.so": "system/lib/hw/gralloc.default.so",
    "system/lib/egl/libGLES_android.so": "system/lib/egl/libGLES_android.so",
}
DATA_FILES = {
    "system/framework/ipod6g-dalvik-fixture.dex":
        "dalvik/libcore/support/src/test/java/tests/resources/"
        "cts_dalvikExecTest_classes.dex",
    "system/framework/ipod6g-dalvik-fixture.jar":
        "dalvik/libcore/support/src/test/java/tests/resources/"
        "cts_dalvikExecTest_classes.dex",
    "system/framework/core.jar":
        "out/target/product/generic/system/framework/core.jar",
    "system/framework/ext.jar":
        "out/target/product/generic/system/framework/ext.jar",
    "system/framework/framework.jar":
        "out/target/product/generic/system/framework/framework.jar",
    "system/framework/framework-res.apk":
        "out/target/product/generic/system/framework/framework-res.apk",
    "system/framework/android.policy.jar":
        "out/target/common/obj/JAVA_LIBRARIES/"
        "android.policy_phone_intermediates/javalib.jar",
    "system/framework/services.jar":
        "out/target/product/generic/system/framework/services.jar",
    "system/lib/egl/egl.cfg": "ipod6g_probe/egl.cfg",
    "system/framework/ipod6g-framework-fixture.jar":
        "out/target/product/generic/system/framework/ipod6g-framework-fixture.jar",
    "system/app/SettingsProvider.apk":
        "out/target/product/generic/system/app/SettingsProvider.apk",
    "system/app/RockpodLauncher.apk":
        "out/target/product/generic/system/app/RockpodLauncher.apk",
    "system/fonts/DroidSans.ttf":
        "out/target/product/generic/system/fonts/DroidSans.ttf",
    "system/fonts/DroidSans-Bold.ttf":
        "out/target/product/generic/system/fonts/DroidSans-Bold.ttf",
    "system/fonts/DroidSerif-Regular.ttf":
        "out/target/product/generic/system/fonts/DroidSerif-Regular.ttf",
    "system/fonts/DroidSerif-Bold.ttf":
        "out/target/product/generic/system/fonts/DroidSerif-Bold.ttf",
    "system/fonts/DroidSerif-Italic.ttf":
        "out/target/product/generic/system/fonts/DroidSerif-Italic.ttf",
    "system/fonts/DroidSerif-BoldItalic.ttf":
        "out/target/product/generic/system/fonts/DroidSerif-BoldItalic.ttf",
    "system/fonts/DroidSansMono.ttf":
        "out/target/product/generic/system/fonts/DroidSansMono.ttf",
    "system/fonts/DroidSansFallback.ttf":
        "out/target/product/generic/system/fonts/DroidSansFallback.ttf",
}
LOCAL_FILES = {
    "system/usr/keylayout/iPod_Classic_Click_Wheel.kl":
        CLICKWHEEL_KEY_LAYOUT,
}
DIRECTORIES = {
    "dev": 0o755,
    "proc": 0o555,
    "sys": 0o555,
    "system": 0o755,
    "system/bin": 0o755,
    "system/xbin": 0o755,
    "system/lib": 0o755,
    "system/lib/hw": 0o755,
    "system/lib/egl": 0o755,
    "system/framework": 0o755,
    "system/app": 0o755,
    "system/etc": 0o755,
    "system/etc/permissions": 0o755,
    "system/fonts": 0o755,
    "system/usr": 0o755,
    "system/usr/keylayout": 0o755,
    "data": 0o771,
    "cache": 0o770,
    "metadata": 0o700,
}
PASS_LINES = [
    "IPOD6G_ECLAIR_PROBE:BEGIN",
    "IPOD6G_ECLAIR_PROBE:SYSCALLS_OK",
    "IPOD6G_ECLAIR_PROBE:MEMORY_OK",
    "IPOD6G_ECLAIR_PROBE:PASS",
]
FORBIDDEN_INIT_RC = (
    "mtd",
    "yaffs",
    "ext2",
    "ext3",
    "ext4",
    "vfat",
    "sdcard",
    "storage",
    "/dev/block",
)


class PackageError(RuntimeError):
    """The root package failed a source, content, or emulation gate."""


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def copy_deterministic_zip(source: Path, target: Path) -> None:
    """Copy a build ZIP/JAR while removing host time and entry-order variance."""
    entries: dict[str, bytes] = {}
    with zipfile.ZipFile(source) as archive:
        for info in archive.infolist():
            name = info.filename
            if (
                not name
                or name.startswith("/")
                or "\\" in name
                or ".." in Path(name).parts
                or name in entries
            ):
                raise PackageError(f"unsafe or duplicate ZIP entry: {name!r}")
            entries[name] = b"" if info.is_dir() else archive.read(info)

    with zipfile.ZipFile(
        target, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9
    ) as archive:
        for name in sorted(entries):
            is_directory = name.endswith("/")
            info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            info.create_system = 3
            info.compress_type = (
                zipfile.ZIP_STORED if is_directory else zipfile.ZIP_DEFLATED
            )
            info.external_attr = (
                (stat.S_IFDIR | 0o755) if is_directory
                else (stat.S_IFREG | 0o644)
            ) << 16
            archive.writestr(info, entries[name])


def pad4(data: bytearray) -> None:
    data.extend(b"\0" * ((-len(data)) & 3))


def add_newc_entry(output: bytearray, inode: int, name: str, mode: int,
                   body: bytes, nlink: int) -> None:
    encoded_name = name.encode("utf-8") + b"\0"
    fields = (
        inode,
        mode,
        0,
        0,
        nlink,
        0,
        len(body),
        0,
        0,
        0,
        0,
        len(encoded_name),
        0,
    )
    output.extend(b"070701")
    output.extend("".join(f"{value:08x}" for value in fields).encode("ascii"))
    output.extend(encoded_name)
    pad4(output)
    output.extend(body)
    pad4(output)


def build_newc(root: Path) -> bytes:
    output = bytearray()
    inode = 1
    for relative in sorted(DIRECTORIES):
        add_newc_entry(
            output,
            inode,
            relative,
            stat.S_IFDIR | DIRECTORIES[relative],
            b"",
            2,
        )
        inode += 1
    for path in sorted(root.rglob("*"), key=lambda item: item.as_posix()):
        if path.is_dir():
            continue
        relative = path.relative_to(root).as_posix()
        if relative not in {
            "init.rc", *RUNTIME_FILES, *DATA_FILES, *LOCAL_FILES
        }:
            raise PackageError(f"unexpected root entry: {relative}")
        if path.is_symlink() or not path.is_file():
            raise PackageError(f"root entry is not a regular file: {relative}")
        mode = 0o755 if relative == "init" or relative.startswith(
            ("system/bin/", "system/xbin/")
        ) else 0o644
        add_newc_entry(output, inode, relative, stat.S_IFREG | mode,
                       path.read_bytes(), 1)
        inode += 1
    add_newc_entry(output, inode, "TRAILER!!!", 0, b"", 1)
    return bytes(output)


def run_qemu(qemu: Path, root: Path, relative: str,
             *arguments: str, env: dict[str, str] | None = None,
             timeout: int = 5) -> subprocess.CompletedProcess:
    return subprocess.run(
        [str(qemu), "-cpu", "arm926", "-L", str(root), str(root / relative),
         *arguments],
        text=True,
        capture_output=True,
        timeout=timeout,
        check=False,
        env=env,
    )


def package(source: Path, destination: Path, qemu: Path) -> dict:
    product = source / "out/target/product/generic"
    qualification_path = product / "ipod6g-eclair-qualification.json"
    emulation_path = product / "ipod6g-eclair-emulation.json"
    for report_path in (qualification_path, emulation_path):
        if not report_path.is_file() or report_path.is_symlink():
            raise PackageError(f"missing regular report: {report_path.name}")
    qualification = json.loads(qualification_path.read_text(encoding="utf-8"))
    emulation = json.loads(emulation_path.read_text(encoding="utf-8"))
    if qualification.get("static_gate_passed") is not True:
        raise PackageError("static source gate did not pass")
    if emulation.get("emulation_gate_passed") is not True:
        raise PackageError("source emulation gate did not pass")
    if qualification.get("framework_library_built") is not True:
        raise PackageError("source qualification lacks the framework library gate")
    if qualification.get("framework_resource_built") is not True:
        raise PackageError("source qualification lacks the framework resource gate")
    if qualification.get("policy_library_built") is not True:
        raise PackageError("source qualification lacks the phone policy gate")
    if qualification.get("app_process_built") is not True:
        raise PackageError("source qualification lacks the app_process gate")
    if qualification.get("zygote_native_runtime_built") is not True:
        raise PackageError("source qualification lacks the native Zygote gate")
    if qualification.get("services_library_built") is not True:
        raise PackageError("source qualification lacks the services library gate")
    if qualification.get("system_server_native_runtime_built") is not True:
        raise PackageError("source qualification lacks the system_server runtime gate")
    if qualification.get("headless_gralloc_built") is not True:
        raise PackageError("source qualification lacks the headless gralloc gate")
    if qualification.get("software_renderer_built") is not True:
        raise PackageError("source qualification lacks the software renderer gate")
    if qualification.get("installd_built") is not True:
        raise PackageError("source qualification lacks the RAM-only installd gate")
    if qualification.get("surfaceflinger_publication_instrumented") is not True:
        raise PackageError("source qualification lacks the SurfaceFlinger publication gate")
    if qualification.get("settings_provider_built") is not True:
        raise PackageError("source qualification lacks the Settings provider gate")
    if qualification.get("launcher_built") is not True:
        raise PackageError("source qualification lacks the Launcher gate")
    if emulation.get("framework_library_tested") is not True:
        raise PackageError("source emulation lacks the framework Java gate")
    if emulation.get("framework_resource_qualified") is not True:
        raise PackageError("source emulation lacks the framework resource gate")
    if emulation.get("app_process_tested") is not True:
        raise PackageError("source emulation lacks the app_process gate")
    if emulation.get("zygote_accept_loop_tested") is not True:
        raise PackageError("source emulation lacks the Zygote accept-loop gate")
    if emulation.get("policy_library_tested") is not True:
        raise PackageError("source emulation lacks the phone policy gate")
    if qualification.get("hardware_actions_enabled") is not False:
        raise PackageError("source qualification does not retain hardware lock")
    if emulation.get("hardware_actions_enabled") is not False:
        raise PackageError("source emulation does not retain hardware lock")
    if destination.exists() and any(destination.iterdir()):
        raise PackageError("package destination must be absent or empty")
    if destination.resolve() in (Path("/"), Path.home().resolve()):
        raise PackageError("refusing broad package destination")
    if not qemu.is_file() or qemu.is_symlink():
        raise PackageError("QEMU must be a regular executable")

    init_rc_text = INIT_RC.read_text(encoding="utf-8")
    lowered = init_rc_text.lower()
    for term in FORBIDDEN_INIT_RC:
        if term in lowered:
            raise PackageError(f"init.rc contains storage term: {term}")
    if lowered.count("mount tmpfs") != 3:
        raise PackageError("init.rc must mount data, cache, and metadata as tmpfs")
    if "mount rootfs rootfs / ro remount" not in lowered:
        raise PackageError("init.rc does not remount the packaged root read-only")

    root = destination / "root"
    root.mkdir(parents=True, exist_ok=True)
    for relative, mode in DIRECTORIES.items():
        path = root / relative
        path.mkdir(parents=True, exist_ok=True)
        os.chmod(path, mode)
    shutil.copyfile(INIT_RC, root / "init.rc")
    os.chmod(root / "init.rc", 0o644)
    for target_relative, source_relative in RUNTIME_FILES.items():
        source_path = product / source_relative
        if not source_path.is_file() or source_path.is_symlink():
            raise PackageError(f"missing runtime artifact: {source_relative}")
        target = root / target_relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source_path, target)
        os.chmod(target, 0o755 if target_relative == "init" or
                 target_relative.startswith(("system/bin/", "system/xbin/"))
                 else 0o644)
    for target_relative, source_relative in DATA_FILES.items():
        source_path = source / source_relative
        if not source_path.is_file() or source_path.is_symlink():
            raise PackageError(f"missing data artifact: {source_relative}")
        target = root / target_relative
        target.parent.mkdir(parents=True, exist_ok=True)
        if target_relative == "system/framework/ipod6g-dalvik-fixture.jar":
            info = zipfile.ZipInfo("classes.dex", (1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_STORED
            info.external_attr = 0o100644 << 16
            with zipfile.ZipFile(target, "w") as archive:
                archive.writestr(info, source_path.read_bytes())
        elif target_relative.endswith(".jar"):
            copy_deterministic_zip(source_path, target)
        else:
            shutil.copyfile(source_path, target)
        os.chmod(target, 0o644)
    for target_relative, source_path in LOCAL_FILES.items():
        if not source_path.is_file() or source_path.is_symlink():
            raise PackageError(f"missing local runtime file: {source_path}")
        target = root / target_relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source_path, target)
        os.chmod(target, 0o644)

    strip = (
        source
        / "prebuilt/linux-x86/toolchain/arm-eabi-4.4.0/bin/arm-eabi-strip"
    )
    if not strip.is_file() or strip.is_symlink():
        raise PackageError("pinned strip tool is missing")
    for relative in RUNTIME_FILES:
        subprocess.run([str(strip), "--strip-unneeded", str(root / relative)],
                       check=True)

    qemu_runs = {}
    for kind in ("static", "dynamic"):
        relative = f"system/bin/ipod6g_eclair_probe_{kind}"
        result = run_qemu(qemu, root, relative)
        if result.returncode != 0 or result.stdout.splitlines() != PASS_LINES:
            raise PackageError(f"stripped {kind} probe failed emulation")
        if result.stderr:
            raise PackageError(f"stripped {kind} probe wrote stderr")
        qemu_runs[kind] = {"exit_code": 0, "stdout": PASS_LINES}
    result = run_qemu(qemu, root, "system/bin/servicemanager")
    if result.returncode != 255 or "binder: cannot open device" not in result.stderr:
        raise PackageError("stripped servicemanager failed its Binder boundary test")
    if "Segmentation fault" in result.stderr or "uncaught target signal" in result.stderr:
        raise PackageError("stripped servicemanager crashed")
    qemu_runs["servicemanager_without_binder"] = {
        "exit_code": result.returncode,
        "stderr": result.stderr.splitlines(),
        "expected_negative_test": True,
    }
    dex_fixture = "system/framework/ipod6g-dalvik-fixture.dex"
    result = run_qemu(
        qemu, root, "system/xbin/dexdump", "-f", str(root / dex_fixture)
    )
    dex_markers = (
        "DEX version '035'",
        "Class descriptor  : 'LdalvikExecTest/HelloWorld;'",
    )
    if result.returncode != 0 or any(
        marker not in result.stdout for marker in dex_markers
    ):
        raise PackageError("stripped dexdump failed the official DEX fixture")
    if result.stderr:
        raise PackageError("stripped dexdump wrote unexpected stderr")
    qemu_runs["dexdump_official_fixture"] = {
        "exit_code": 0,
        "required_markers": list(dex_markers),
    }

    with tempfile.TemporaryDirectory(prefix="ipod6g-eclair-package-") as tmpdir:
        data = Path(tmpdir) / "dalvik-cache"
        data.mkdir(mode=0o700)
        dalvik_env = os.environ.copy()
        dalvik_env["ANDROID_DATA"] = tmpdir
        dalvik_env["ANDROID_ROOT"] = str(root / "system")
        dalvik_env["IPOD6G_ASHMEM_TMPDIR"] = tmpdir
        result = run_qemu(
            qemu,
            root,
            "system/bin/dalvikvm",
            "-Xverify:none",
            "-Xdexopt:none",
            f"-Xbootclasspath:{root / 'system/framework/core.jar'}",
            "-cp",
            str(root / "system/framework/ipod6g-dalvik-fixture.jar"),
            "dalvikExecTest.HelloWorld",
            env=dalvik_env,
            timeout=15,
        )
        ashmem_leftovers = list(Path(tmpdir).glob(".eclair-ashmem-*"))
    if result.returncode != 0 or "Hello Android World!" not in result.stdout:
        raise PackageError("stripped Dalvik VM failed the official Java fixture")
    if ashmem_leftovers:
        raise PackageError("stripped Dalvik VM left ashmem backing files")
    qemu_runs["dalvikvm_official_fixture"] = {
        "exit_code": 0,
        "stdout": result.stdout.splitlines(),
        "stderr": result.stderr.splitlines(),
        "ashmem_files_remaining": 0,
    }

    with tempfile.TemporaryDirectory(prefix="ipod6g-eclair-framework-package-") as tmpdir:
        data = Path(tmpdir) / "dalvik-cache"
        data.mkdir(mode=0o700)
        dalvik_env = os.environ.copy()
        dalvik_env["ANDROID_DATA"] = tmpdir
        dalvik_env["ANDROID_ROOT"] = str(root / "system")
        dalvik_env["IPOD6G_ASHMEM_TMPDIR"] = tmpdir
        bootclasspath = ":".join(str(root / relative) for relative in (
            "system/framework/core.jar",
            "system/framework/ext.jar",
            "system/framework/framework.jar",
        ))
        result = run_qemu(
            qemu,
            root,
            "system/bin/dalvikvm",
            "-Xverify:none",
            "-Xdexopt:none",
            f"-Xbootclasspath:{bootclasspath}",
            "-cp",
            ":".join(str(root / relative) for relative in (
                "system/framework/ipod6g-framework-fixture.jar",
                "system/framework/ipod6g-dalvik-fixture.jar",
            )),
            "ipod6g.frameworktest.FrameworkHello",
            env=dalvik_env,
            timeout=20,
        )
        ashmem_leftovers = list(Path(tmpdir).glob(".eclair-ashmem-*"))
    if result.returncode != 0 or result.stdout.splitlines() != [
        "Hello Android World!", "Android Framework Java OK!"
    ]:
        raise PackageError("stripped Dalvik VM failed the framework Java fixture")
    if ashmem_leftovers:
        raise PackageError("framework Java fixture left ashmem backing files")
    qemu_runs["dalvikvm_framework_fixture"] = {
        "exit_code": 0,
        "stdout": result.stdout.splitlines(),
        "stderr": result.stderr.splitlines(),
        "ashmem_files_remaining": 0,
    }

    archive = build_newc(root)
    cpio_path = destination / "eclair-native-initramfs.cpio.gz"
    with cpio_path.open("wb") as raw:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw,
                           compresslevel=9, mtime=0) as compressed:
            compressed.write(archive)

    runtime = {}
    for relative in sorted({
        "init.rc", *RUNTIME_FILES, *DATA_FILES, *LOCAL_FILES
    }):
        path = root / relative
        runtime[relative] = {"size": path.stat().st_size, "sha256": sha256(path)}
    report = {
        "schema": 1,
        "scope": "android-2.0-native-storage-free-root",
        "aosp_tag": "android-2.0_r1",
        "cpu": "ARMv5TE",
        "runtime_files": runtime,
        "initramfs": {
            "file": cpio_path.name,
            "uncompressed_size": len(archive),
            "compressed_size": cpio_path.stat().st_size,
            "sha256": sha256(cpio_path),
        },
        "qemu_runs": qemu_runs,
        "root_read_only": True,
        "data_cache_metadata": "tmpfs",
        "persistent_storage_nodes": False,
        "storage_tools": False,
        "static_gate_passed": True,
        "emulation_gate_passed": True,
        "binder_positive_path_tested": False,
        "dalvik_dex_parser_tested": True,
        "dalvik_vm_tested": True,
        "core_library_tested": True,
        "framework_library_tested": True,
        "framework_resource_qualified": True,
        "policy_library_packaged": True,
        "app_process_packaged": True,
        "zygote_native_runtime_packaged": True,
        "services_library_packaged": True,
        "system_server_native_runtime_packaged": True,
        "headless_gralloc_packaged": True,
        "software_renderer_packaged": True,
        "installd_packaged": True,
        "settings_provider_packaged": True,
        "launcher_packaged": True,
        "n25_clickwheel_keylayout_packaged": True,
        "hardware_actions_enabled": False,
    }
    report_path = destination / "qualification.json"
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8")
    checksums = [
        f"{sha256(cpio_path)}  {cpio_path.name}",
        f"{sha256(report_path)}  {report_path.name}",
    ]
    (destination / "SHA256SUMS").write_text("\n".join(checksums) + "\n",
                                             encoding="utf-8")
    return report


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--qemu", type=Path, required=True)
    args = parser.parse_args()
    try:
        report = package(args.source.resolve(), args.destination.resolve(),
                         args.qemu.resolve())
    except (OSError, json.JSONDecodeError, PackageError,
            subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f"package_eclair_native_root: {error}\n")
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
