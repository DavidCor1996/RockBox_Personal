#!/usr/bin/env python3
"""Run the Android 2.0 ARM native subset under QEMU user emulation."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import zipfile


PASS_LINES = [
    "IPOD6G_ECLAIR_PROBE:BEGIN",
    "IPOD6G_ECLAIR_PROBE:SYSCALLS_OK",
    "IPOD6G_ECLAIR_PROBE:MEMORY_OK",
    "IPOD6G_ECLAIR_PROBE:PASS",
]


class EmulationError(RuntimeError):
    """The emulated Android process violated its expected contract."""


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def run_process(
    command: list[str], timeout: int = 5, env: dict[str, str] | None = None
) -> subprocess.CompletedProcess:
    return subprocess.run(
        command,
        text=True,
        capture_output=True,
        timeout=timeout,
        check=False,
        env=env,
    )


def emulate(source: Path, qemu: Path) -> dict:
    product = source / "out/target/product/generic"
    static_report_path = product / "ipod6g-eclair-qualification.json"
    if not static_report_path.is_file() or static_report_path.is_symlink():
        raise EmulationError("static qualification report is missing")
    static_report = json.loads(static_report_path.read_text(encoding="utf-8"))
    if static_report.get("static_gate_passed") is not True:
        raise EmulationError("static qualification did not pass")
    if static_report.get("hardware_actions_enabled") is not False:
        raise EmulationError("static report does not retain hardware lock")
    if static_report.get("framework_library_built") is not True:
        raise EmulationError("static report lacks the Eclair framework library gate")
    if static_report.get("framework_resource_built") is not True:
        raise EmulationError("static report lacks the Eclair framework resource gate")
    if static_report.get("app_process_built") is not True:
        raise EmulationError("static report lacks the app_process gate")
    if static_report.get("zygote_native_runtime_built") is not True:
        raise EmulationError("static report lacks the native Zygote gate")
    if not qemu.is_file() or qemu.is_symlink():
        raise EmulationError("QEMU must be a regular executable")

    base = [str(qemu), "-cpu", "arm926", "-L", str(product)]
    runs = {}
    for kind in ("static", "dynamic"):
        binary = product / "system/bin" / f"ipod6g_eclair_probe_{kind}"
        result = run_process(base + [str(binary)])
        lines = result.stdout.splitlines()
        if result.returncode != 0:
            raise EmulationError(
                f"{kind} probe exited {result.returncode}: {result.stderr.strip()}"
            )
        if lines != PASS_LINES:
            raise EmulationError(f"{kind} probe returned an invalid transcript")
        if result.stderr:
            raise EmulationError(f"{kind} probe wrote unexpected stderr")
        runs[f"probe_{kind}"] = {
            "exit_code": result.returncode,
            "stdout": lines,
            "stderr": [],
        }

    service = product / "system/bin/servicemanager"
    result = run_process(base + [str(service)])
    expected_error = "binder: cannot open device (No such file or directory)"
    if result.returncode != 255 or expected_error not in result.stderr:
        raise EmulationError(
            "servicemanager did not fail cleanly at the absent Binder boundary"
        )
    if "uncaught target signal" in result.stderr or "Segmentation fault" in result.stderr:
        raise EmulationError("servicemanager crashed instead of rejecting absent Binder")
    runs["servicemanager_without_binder"] = {
        "exit_code": result.returncode,
        "stdout": result.stdout.splitlines(),
        "stderr": result.stderr.splitlines(),
        "expected_negative_test": True,
    }

    dexdump = product / "system/xbin/dexdump"
    dex_fixture = (
        source
        / "dalvik/libcore/support/src/test/java/tests/resources/"
        "cts_dalvikExecTest_classes.dex"
    )
    if not dex_fixture.is_file() or dex_fixture.is_symlink():
        raise EmulationError("official Dalvik DEX fixture is missing")
    result = run_process(base + [str(dexdump), "-f", str(dex_fixture)])
    dex_markers = (
        "DEX version '035'",
        "Class descriptor  : 'LdalvikExecTest/HelloWorld;'",
    )
    if result.returncode != 0 or any(
        marker not in result.stdout for marker in dex_markers
    ):
        raise EmulationError("Eclair libdex did not parse the official DEX fixture")
    if result.stderr:
        raise EmulationError("Eclair libdex wrote unexpected stderr")
    runs["dexdump_official_fixture"] = {
        "exit_code": 0,
        "fixture_sha256": sha256(dex_fixture),
        "required_markers": list(dex_markers),
    }

    dalvikvm = product / "system/bin/dalvikvm"
    core_jar = product / "system/framework/core.jar"
    if not dalvikvm.is_file() or dalvikvm.is_symlink():
        raise EmulationError("official Dalvik VM is missing")
    if not core_jar.is_file() or core_jar.is_symlink():
        raise EmulationError("official Eclair core.jar is missing")
    with tempfile.TemporaryDirectory(prefix="ipod6g-eclair-ashmem-") as tmpdir:
        dalvik_cache = Path(tmpdir) / "dalvik-cache"
        dalvik_cache.mkdir(mode=0o700)
        fixture_jar = Path(tmpdir) / "official-fixture.jar"
        with zipfile.ZipFile(fixture_jar, "w", zipfile.ZIP_STORED) as archive:
            archive.write(dex_fixture, "classes.dex")
        dalvik_env = os.environ.copy()
        dalvik_env["IPOD6G_ASHMEM_TMPDIR"] = tmpdir
        dalvik_env["ANDROID_DATA"] = tmpdir
        dalvik_env["ANDROID_ROOT"] = str(product / "system")
        result = run_process(
            base
            + [
                str(dalvikvm),
                "-Xverify:none",
                "-Xdexopt:none",
                f"-Xbootclasspath:{core_jar}",
                "-cp",
                str(fixture_jar),
                "dalvikExecTest.HelloWorld",
            ],
            timeout=15,
            env=dalvik_env,
        )
        ashmem_leftovers = list(Path(tmpdir).glob(".eclair-ashmem-*"))
        cache_files = [path for path in dalvik_cache.iterdir() if path.is_file()]
    if result.returncode != 0:
        raise EmulationError(
            f"Dalvik VM exited {result.returncode}: {result.stderr.strip()}"
        )
    if "Hello Android World!" not in result.stdout:
        raise EmulationError("Dalvik VM did not execute the official Java fixture")
    if ashmem_leftovers:
        raise EmulationError("Dalvik ashmem fallback left persistent files")
    runs["dalvikvm_official_fixture"] = {
        "exit_code": 0,
        "stdout": result.stdout.splitlines(),
        "stderr": result.stderr.splitlines(),
        "fixture_sha256": sha256(dex_fixture),
        "core_jar_sha256": sha256(core_jar),
        "ashmem_backing": "unlinked-host-tmpfs-test-file",
        "ashmem_files_remaining": 0,
        "ephemeral_dalvik_cache_files": len(cache_files),
    }

    ext_jar = product / "system/framework/ext.jar"
    framework_jar = product / "system/framework/framework.jar"
    policy_jar = (
        source
        / "out/target/common/obj/JAVA_LIBRARIES/"
        "android.policy_phone_intermediates/javalib.jar"
    )
    framework_fixture = product / "system/framework/ipod6g-framework-fixture.jar"
    for path in (ext_jar, framework_jar, policy_jar, framework_fixture):
        if not path.is_file() or path.is_symlink():
            raise EmulationError(f"framework runtime file is missing: {path.name}")
    with tempfile.TemporaryDirectory(prefix="ipod6g-eclair-framework-") as tmpdir:
        dalvik_cache = Path(tmpdir) / "dalvik-cache"
        dalvik_cache.mkdir(mode=0o700)
        dalvik_env = os.environ.copy()
        dalvik_env["IPOD6G_ASHMEM_TMPDIR"] = tmpdir
        dalvik_env["ANDROID_DATA"] = tmpdir
        dalvik_env["ANDROID_ROOT"] = str(product / "system")
        official_fixture = Path(tmpdir) / "official-fixture.jar"
        with zipfile.ZipFile(official_fixture, "w", zipfile.ZIP_STORED) as archive:
            archive.write(dex_fixture, "classes.dex")
        bootclasspath = ":".join(str(path) for path in (
            core_jar, ext_jar, framework_jar
        ))
        result = run_process(
            base
            + [
                str(dalvikvm),
                "-Xverify:none",
                "-Xdexopt:none",
                f"-Xbootclasspath:{bootclasspath}",
                "-cp",
                f"{framework_fixture}:{official_fixture}",
                "ipod6g.frameworktest.FrameworkHello",
            ],
            timeout=20,
            env=dalvik_env,
        )
        ashmem_leftovers = list(Path(tmpdir).glob(".eclair-ashmem-*"))
        cache_files = [path for path in dalvik_cache.iterdir() if path.is_file()]
    if result.returncode != 0:
        raise EmulationError(
            f"framework Java fixture exited {result.returncode}: {result.stderr.strip()}"
        )
    if result.stdout.splitlines() != [
        "Hello Android World!", "Android Framework Java OK!"
    ]:
        raise EmulationError("Dalvik did not execute the Eclair framework fixture")
    if ashmem_leftovers:
        raise EmulationError("framework Java test left ashmem backing files")
    runs["dalvikvm_framework_fixture"] = {
        "exit_code": 0,
        "stdout": result.stdout.splitlines(),
        "stderr": result.stderr.splitlines(),
        "core_jar_sha256": sha256(core_jar),
        "ext_jar_sha256": sha256(ext_jar),
        "framework_jar_sha256": sha256(framework_jar),
        "fixture_sha256": sha256(framework_fixture),
        "ashmem_backing": "unlinked-host-tmpfs-test-file",
        "ashmem_files_remaining": 0,
        "ephemeral_dalvik_cache_files": len(cache_files),
    }

    app_process = product / "system/bin/app_process"
    with tempfile.TemporaryDirectory(prefix="ipod6g-eclair-zygote-") as tmpdir:
        tmp = Path(tmpdir)
        dalvik_cache = tmp / "dalvik-cache"
        dalvik_cache.mkdir(mode=0o700)
        socket_path = tmp / "zygote.sock"
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as zygote_socket:
            zygote_socket.bind(str(socket_path))
            zygote_socket.listen(4)
            zygote_env = os.environ.copy()
            zygote_env["IPOD6G_ASHMEM_TMPDIR"] = tmpdir
            zygote_env["ANDROID_DATA"] = tmpdir
            zygote_env["ANDROID_ROOT"] = str(product / "system")
            zygote_env["BOOTCLASSPATH"] = ":".join(str(path) for path in (
                core_jar, ext_jar, framework_jar, policy_jar
            ))
            zygote_env["ANDROID_SOCKET_zygote"] = str(zygote_socket.fileno())
            process = subprocess.Popen(
                base + [
                    str(app_process),
                    "-Xzygote",
                    "-Xverify:none",
                    "-Xdexopt:none",
                    str(product / "system/bin"),
                    "--zygote",
                ],
                text=True,
                errors="replace",
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                env=zygote_env,
                pass_fds=(zygote_socket.fileno(),),
            )
            try:
                stdout, stderr = process.communicate(timeout=10)
                stayed_alive = False
            except subprocess.TimeoutExpired:
                stayed_alive = True
                process.terminate()
                try:
                    stdout, stderr = process.communicate(timeout=2)
                except subprocess.TimeoutExpired:
                    process.kill()
                    stdout, stderr = process.communicate(timeout=2)
        ashmem_leftovers = list(tmp.glob(".eclair-ashmem-*"))
        cache_files = [path for path in dalvik_cache.iterdir() if path.is_file()]
    transcript = stdout + stderr
    zygote_markers = (
        ">>>>>>>>>>>>>> AndroidRuntime START <<<<<<<<<<<<<<",
        "AndroidRuntime: --- registering native functions ---",
        "Zygote: Accepting command socket connections",
    )
    if not stayed_alive:
        raise EmulationError(
            f"app_process/Zygote exited unexpectedly: {transcript.strip()}"
        )
    if any(marker not in transcript for marker in zygote_markers):
        raise EmulationError("app_process did not reach the Zygote accept loop")
    for failure in ("Zygote died with exception", "Segmentation fault"):
        if failure in transcript:
            raise EmulationError(f"app_process transcript contains: {failure}")
    if ashmem_leftovers:
        raise EmulationError("Zygote test left ashmem backing files")
    runs["app_process_zygote"] = {
        "accept_loop_reached": True,
        "stayed_alive_until_test_termination": True,
        "required_markers": list(zygote_markers),
        "ashmem_files_remaining": 0,
        "ephemeral_dalvik_cache_files": len(cache_files),
    }

    version = run_process([str(qemu), "--version"])
    if version.returncode != 0:
        raise EmulationError("cannot read QEMU version")
    return {
        "schema": 1,
        "scope": "android-2.0-native-userspace-emulation",
        "board_cpu_model": "ARM926EJ-S",
        "qemu": {
            "path": str(qemu),
            "sha256": sha256(qemu),
            "version": version.stdout.splitlines()[0],
        },
        "runs": runs,
        "static_gate_passed": True,
        "emulation_gate_passed": True,
        "binder_positive_path_tested": False,
        "dalvik_dex_parser_tested": True,
        "dalvik_vm_tested": True,
        "core_library_tested": True,
        "framework_library_tested": True,
        "framework_resource_qualified": True,
        "policy_library_tested": True,
        "app_process_tested": True,
        "zygote_accept_loop_tested": True,
        "host_ephemeral_storage_used": True,
        "storage_accessed": False,
        "hardware_actions_enabled": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("--qemu", default="qemu-arm-static")
    args = parser.parse_args()
    qemu_arg = Path(args.qemu)
    if qemu_arg.parent == Path("."):
        located = shutil.which(args.qemu)
        if located is None:
            parser.exit(1, f"emulate_eclair_native: cannot find {args.qemu}\n")
        qemu_arg = Path(located)
    try:
        report = emulate(args.source.resolve(), qemu_arg.resolve())
    except (OSError, json.JSONDecodeError, EmulationError,
            subprocess.TimeoutExpired) as error:
        parser.exit(1, f"emulate_eclair_native: {error}\n")
    output = (
        args.source.resolve()
        / "out/target/product/generic/ipod6g-eclair-emulation.json"
    )
    output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                      encoding="utf-8")
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
