#!/usr/bin/env python3
"""Statically qualify the pinned Android 2.0 ARM native bring-up subset."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import zipfile

from prepare_eclair_native_source import MARKER_NAME, sha256_source_tree


VFP_MNEMONIC = re.compile(
    r"^(?:v(?:add|sub|mul|div|ldr|str|mov|cmp|cmpe|cvt|push|pop|neg|abs|sqrt)\w*|"
    r"vmrs|vmsr|fld\w*|fst\w*|fadd\w*|fsub\w*|fmul\w*|fdiv\w*)$",
    re.IGNORECASE,
)
LIBGCC_VFP_UNWIND = re.compile(
    r"^(?:__dl_)?__gnu_Unwind_(?:Restore|Save)_VFP(?:_D(?:_16_to_31)?)?$"
)

ARTIFACTS = {
    "init": "root/init",
    "linker": "system/bin/linker",
    "servicemanager": "system/bin/servicemanager",
    "installd": "system/bin/installd",
    "dexdump": "system/xbin/dexdump",
    "dalvikvm": "system/bin/dalvikvm",
    "dexopt": "system/bin/dexopt",
    "app_process": "system/bin/app_process",
    "zygote_gate": "system/bin/ipod6g_eclair_zygote_gate",
    "probe_dynamic": "system/bin/ipod6g_eclair_probe_dynamic",
    "probe_static": "system/bin/ipod6g_eclair_probe_static",
    "libc": "system/lib/libc.so",
    "libdl": "system/lib/libdl.so",
    "liblog": "system/lib/liblog.so",
    "libcutils": "system/lib/libcutils.so",
    "libstdcxx": "system/lib/libstdc++.so",
    "libm": "system/lib/libm.so",
    "libdvm": "system/lib/libdvm.so",
    "libnativehelper": "system/lib/libnativehelper.so",
    "libexpat": "system/lib/libexpat.so",
    "libcrypto": "system/lib/libcrypto.so",
    "libssl": "system/lib/libssl.so",
    "libutils": "system/lib/libutils.so",
    "libicudata": "system/lib/libicudata.so",
    "libicuuc": "system/lib/libicuuc.so",
    "libicui18n": "system/lib/libicui18n.so",
    "libsqlite": "system/lib/libsqlite.so",
    "libz": "system/lib/libz.so",
    "libbinder": "system/lib/libbinder.so",
    "libnetutils": "system/lib/libnetutils.so",
    "libhardware": "system/lib/libhardware.so",
    "libhardware_legacy": "system/lib/libhardware_legacy.so",
    "libwpa_client": "system/lib/libwpa_client.so",
    "libsonivox": "system/lib/libsonivox.so",
    "libemoji": "system/lib/libemoji.so",
    "libskia": "system/lib/libskia.so",
    "libEGL": "system/lib/libEGL.so",
    "libGLESv1_CM": "system/lib/libGLESv1_CM.so",
    "libpixelflinger": "system/lib/libpixelflinger.so",
    "libui": "system/lib/libui.so",
    "libmedia": "system/lib/libmedia.so",
    "libskiagl": "system/lib/libskiagl.so",
    "libandroid_runtime": "system/lib/libandroid_runtime.so",
    "libsurfaceflinger": "system/lib/libsurfaceflinger.so",
    "libsystem_server": "system/lib/libsystem_server.so",
    "libandroid_servers": "system/lib/libandroid_servers.so",
    "gralloc_default": "system/lib/hw/gralloc.default.so",
    "libGLES_android": "system/lib/egl/libGLES_android.so",
}

DYNAMIC_EXECUTABLES = {
    "servicemanager", "installd", "dexdump", "dalvikvm", "dexopt", "app_process",
    "zygote_gate", "probe_dynamic"
}
STATIC_EXECUTABLES = {"init", "linker", "probe_static"}
ALLOWED_NEEDED = {
    "libc.so",
    "libdl.so",
    "liblog.so",
    "libcutils.so",
    "libstdc++.so",
    "libm.so",
    "libz.so",
    "libdvm.so",
    "libnativehelper.so",
    "libexpat.so",
    "libcrypto.so",
    "libssl.so",
    "libutils.so",
    "libicudata.so",
    "libicuuc.so",
    "libicui18n.so",
    "libsqlite.so",
    "libbinder.so",
    "libnetutils.so",
    "libhardware.so",
    "libhardware_legacy.so",
    "libwpa_client.so",
    "libsonivox.so",
    "libemoji.so",
    "libskia.so",
    "libEGL.so",
    "libGLESv1_CM.so",
    "libpixelflinger.so",
    "libui.so",
    "libmedia.so",
    "libskiagl.so",
    "libandroid_runtime.so",
    "libsurfaceflinger.so",
    "libsystem_server.so",
    "libandroid_servers.so",
}


class QualificationError(RuntimeError):
    """An artifact did not satisfy a mandatory gate."""


def run(*command: str) -> str:
    result = subprocess.run(command, check=True, text=True, capture_output=True)
    return result.stdout


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def check_dex_archive(path: Path, label: str) -> tuple[bytes, dict]:
    if not path.is_file() or path.is_symlink():
        raise QualificationError(f"missing regular {label}")
    try:
        with zipfile.ZipFile(path) as archive:
            classes_dex = archive.read("classes.dex")
    except (KeyError, zipfile.BadZipFile) as error:
        raise QualificationError(f"{label} lacks a valid classes.dex") from error
    if not classes_dex.startswith(b"dex\n035\0"):
        raise QualificationError(f"{label} classes.dex is not DEX 035")
    return classes_dex, {
        "path": str(path),
        "size": path.stat().st_size,
        "sha256": sha256(path),
        "classes_dex_sha256": hashlib.sha256(classes_dex).hexdigest(),
        "dex_version": "035",
    }


def classify_vfp_instructions(disassembly: str) -> tuple[str | None, list[str]]:
    symbol = ""
    dormant_helpers: set[str] = set()
    for line in disassembly.splitlines():
        label = re.match(r"^[0-9a-f]+ <([^>]+)>:$", line)
        if label:
            symbol = label.group(1)
            continue
        fields = line.split("\t")
        if len(fields) < 3:
            continue
        instruction = fields[2].strip()
        if not instruction:
            continue
        mnemonic = instruction.split(maxsplit=1)[0]
        if VFP_MNEMONIC.fullmatch(mnemonic):
            if LIBGCC_VFP_UNWIND.fullmatch(symbol):
                dormant_helpers.add(symbol)
                continue
            return line.strip(), sorted(dormant_helpers)
    return None, sorted(dormant_helpers)


def check_elf(name: str, path: Path, readelf: str, objdump: str) -> dict:
    header = run(readelf, "-h", str(path))
    attributes = run(readelf, "-A", str(path))
    programs = run(readelf, "-l", str(path))
    dynamic = run(readelf, "-d", str(path))
    disassembly = run(objdump, "-d", str(path))

    if "Class:                             ELF32" not in header:
        raise QualificationError(f"{name}: not ELF32")
    if "Machine:                           ARM" not in header:
        raise QualificationError(f"{name}: not ARM")
    if "Tag_CPU_arch: v5TE" not in attributes:
        raise QualificationError(f"{name}: not tagged ARMv5TE")
    if re.search(r"Tag_CPU_arch: v(?:6|7|8)", attributes):
        raise QualificationError(f"{name}: incompatible CPU architecture")
    if "Tag_VFP_arch" in attributes:
        raise QualificationError(f"{name}: VFP attribute is forbidden")
    vfp_line, dormant_vfp_helpers = classify_vfp_instructions(disassembly)
    if vfp_line:
        raise QualificationError(f"{name}: VFP instruction: {vfp_line}")

    has_interp = "Requesting program interpreter" in programs
    if name in DYNAMIC_EXECUTABLES:
        if "Requesting program interpreter: /system/bin/linker" not in programs:
            raise QualificationError(f"{name}: wrong Android interpreter")
    elif name in STATIC_EXECUTABLES and has_interp:
        raise QualificationError(f"{name}: expected a static executable")

    needed = re.findall(r"Shared library: \[([^]]+)\]", dynamic)
    unexpected = sorted(set(needed) - ALLOWED_NEEDED)
    if unexpected:
        raise QualificationError(f"{name}: unexpected dependency {unexpected}")
    return {
        "path": str(path),
        "size": path.stat().st_size,
        "sha256": sha256(path),
        "needed": needed,
        "arm_arch": "v5TE",
        "application_vfp": False,
        "dormant_libgcc_vfp_unwind_helpers": dormant_vfp_helpers,
    }


def qualify(source: Path) -> dict:
    marker_path = source / MARKER_NAME
    if not marker_path.is_file() or marker_path.is_symlink():
        raise QualificationError("missing prepared-source marker")
    marker = json.loads(marker_path.read_text(encoding="utf-8"))
    if marker.get("aosp_tag") != "android-2.0_r1":
        raise QualificationError("source marker is not android-2.0_r1")
    if marker.get("network_used") is not False:
        raise QualificationError("source preparation network state is unsafe")
    if marker.get("hardware_accessed") is not False:
        raise QualificationError("source preparation touched hardware")
    actual_tree = sha256_source_tree(source)
    if actual_tree != marker.get("prepared_source_tree_sha256"):
        raise QualificationError("prepared source tree changed after verification")

    version_defaults = (source / "build/core/version_defaults.mk").read_text(
        encoding="utf-8"
    )
    for expected in (
        "PLATFORM_VERSION := 2.0",
        "PLATFORM_SDK_VERSION := 5",
        "PLATFORM_VERSION_CODENAME := REL",
    ):
        if expected not in version_defaults:
            raise QualificationError(f"missing AOSP identity: {expected}")

    tls_header = (source / "bionic/libc/private/bionic_tls.h").read_text(
        encoding="utf-8"
    )
    if "(((void* (*)(void)) 0xffff0fe0)())" not in tls_header:
        raise QualificationError("Bionic does not use __kuser_get_tls")
    service_source = (
        source / "frameworks/base/cmds/servicemanager/service_manager.c"
    ).read_text(encoding="utf-8")
    if "if (!bs)\n        return -1;" not in service_source:
        raise QualificationError("servicemanager lacks Binder-open failure guard")
    pixelflinger_makefile = (
        source / "system/core/libpixelflinger/Android.mk"
    ).read_text(encoding="utf-8")
    for expected in (
        "PIXELFLINGER_HAS_ARMV6 := false",
        "ifneq ($(filter armv6% armv7%,$(TARGET_ARCH_VARIANT)),)",
        "ifeq ($(PIXELFLINGER_HAS_ARMV6),true)",
    ):
        if expected not in pixelflinger_makefile:
            raise QualificationError(
                "PixelFlinger lacks the ARMv5TE/ARMv6 assembly exclusion"
            )

    product = source / "out/target/product/generic"
    toolchain_report_path = product / "ipod6g-eclair-host-toolchain.json"
    if not toolchain_report_path.is_file() or toolchain_report_path.is_symlink():
        raise QualificationError("missing host-toolchain verification report")
    toolchain_report = json.loads(
        toolchain_report_path.read_text(encoding="utf-8")
    )
    if toolchain_report.get("toolchain_gate_passed") is not True:
        raise QualificationError("host Java 6 toolchain gate did not pass")
    if toolchain_report.get("hardware_accessed") is not False:
        raise QualificationError("host toolchain report touched hardware")
    if toolchain_report.get("network_used") is not False:
        raise QualificationError("host toolchain report used the network")

    framework_dir = product / "system/framework"
    core_jar = framework_dir / "core.jar"
    core_dex, core_record = check_dex_archive(core_jar, "official Eclair core.jar")
    ext_dex, ext_record = check_dex_archive(
        framework_dir / "ext.jar", "official Eclair ext.jar"
    )
    framework_dex, framework_record = check_dex_archive(
        framework_dir / "framework.jar", "official Eclair framework.jar"
    )
    policy_dex, policy_record = check_dex_archive(
        source
        / "out/target/common/obj/JAVA_LIBRARIES/"
        "android.policy_phone_intermediates/javalib.jar",
        "official Eclair phone policy library",
    )
    services_dex, services_record = check_dex_archive(
        framework_dir / "services.jar", "official Eclair services.jar"
    )
    if b"Lcom/android/server/SystemServer;" not in services_dex:
        raise QualificationError("services.jar lacks the official SystemServer class")
    fixture_dex, fixture_record = check_dex_archive(
        framework_dir / "ipod6g-framework-fixture.jar", "framework fixture"
    )
    framework_res = framework_dir / "framework-res.apk"
    if not framework_res.is_file() or framework_res.is_symlink():
        raise QualificationError("missing regular official Eclair framework-res.apk")
    try:
        with zipfile.ZipFile(framework_res) as archive:
            resource_entries = set(archive.namelist())
    except zipfile.BadZipFile as error:
        raise QualificationError("framework-res.apk is not a valid ZIP") from error
    for required in ("AndroidManifest.xml", "resources.arsc"):
        if required not in resource_entries:
            raise QualificationError(f"framework-res.apk lacks {required}")
    tool_bin = source / "prebuilt/linux-x86/toolchain/arm-eabi-4.4.0/bin"
    readelf = str(tool_bin / "arm-eabi-readelf")
    objdump = str(tool_bin / "arm-eabi-objdump")
    records = {}
    for name, relative in ARTIFACTS.items():
        path = product / relative
        if not path.is_file() or path.is_symlink():
            raise QualificationError(f"missing regular artifact: {relative}")
        records[name] = check_elf(name, path, readelf, objdump)

    for probe_name in ("probe_dynamic", "probe_static"):
        probe = Path(records[probe_name]["path"])
        if b"IPOD6G_ECLAIR_PROBE:PASS" not in probe.read_bytes():
            raise QualificationError(f"{probe_name}: completion marker missing")
    if b"/dev/binder" not in Path(records["servicemanager"]["path"]).read_bytes():
        raise QualificationError("servicemanager does not target /dev/binder")
    if b"IPOD6G_ECLAIR_HEADLESS_GRALLOC:READY" not in Path(
        records["gralloc_default"]["path"]
    ).read_bytes():
        raise QualificationError("gralloc.default lacks the headless safety marker")
    surfaceflinger = Path(records["libsurfaceflinger"]["path"]).read_bytes()
    if b"IPOD6G_ECLAIR_SURFACEFLINGER_SERVICE:PUBLISHED" not in surfaceflinger:
        raise QualificationError("SurfaceFlinger lacks the publication marker")
    if b"IPOD6G_ECLAIR_SYSTEM_SERVER:READY" not in services_dex:
        raise QualificationError("services.jar lacks the minimal ready marker")
    if b"IPOD6G_ECLAIR_BATTERY:STUB_READY" not in services_dex:
        raise QualificationError("services.jar lacks the RAM-only battery marker")
    if b"IPOD6G_ECLAIR_ACCESSIBILITY:READY" not in services_dex:
        raise QualificationError("services.jar lacks the accessibility marker")
    settings_dex, settings_record = check_dex_archive(
        product / "system/app/SettingsProvider.apk", "SettingsProvider.apk"
    )
    if b"Lcom/android/providers/settings/SettingsProvider;" not in settings_dex:
        raise QualificationError("SettingsProvider.apk lacks its provider class")
    launcher_dex, launcher_record = check_dex_archive(
        product / "system/app/RockpodLauncher.apk", "RockpodLauncher.apk"
    )
    if b"IPOD6G_ECLAIR_LAUNCHER:ON_CREATE" not in launcher_dex:
        raise QualificationError("RockpodLauncher.apk lacks its boot marker")

    return {
        "schema": 1,
        "scope": "android-2.0-native-userspace",
        "board": "apple-n25-ipod6g",
        "aosp_tag": "android-2.0_r1",
        "platform_version": "2.0",
        "platform_sdk": 5,
        "build_id": "ESD20",
        "cpu": "ARMv5TE",
        "source_tree_sha256": actual_tree,
        "artifacts": records,
        "core_jar": core_record,
        "ext_jar": ext_record,
        "framework_jar": framework_record,
        "policy_jar": policy_record,
        "services_jar": services_record,
        "framework_fixture": fixture_record,
        "framework_res": {
            "path": str(framework_res),
            "size": framework_res.stat().st_size,
            "sha256": sha256(framework_res),
            "required_entries": ["AndroidManifest.xml", "resources.arsc"],
        },
        "settings_provider_apk": settings_record,
        "launcher_apk": launcher_record,
        "host_toolchain": toolchain_report,
        "dalvik_vm_built": True,
        "core_library_built": True,
        "framework_library_built": True,
        "framework_resource_built": True,
        "policy_library_built": True,
        "app_process_built": True,
        "zygote_native_runtime_built": True,
        "services_library_built": True,
        "system_server_native_runtime_built": True,
        "headless_gralloc_built": True,
        "software_renderer_built": True,
        "installd_built": True,
        "surfaceflinger_publication_instrumented": True,
        "settings_provider_built": True,
        "launcher_built": True,
        "static_gate_passed": True,
        "emulation_gate_passed": False,
        "storage_profile": "none",
        "hardware_actions_enabled": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    args = parser.parse_args()
    try:
        report = qualify(args.source.resolve())
    except (OSError, json.JSONDecodeError, QualificationError,
            subprocess.CalledProcessError) as error:
        parser.exit(1, f"qualify_eclair_native: {error}\n")
    output = (
        args.source.resolve()
        / "out/target/product/generic/ipod6g-eclair-qualification.json"
    )
    output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                      encoding="utf-8")
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
