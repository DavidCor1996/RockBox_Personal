"""Tests for Rockpod's locked image-only Android helper adapter."""

from __future__ import annotations

import json
import os
import hashlib

import pytest

from services.android_installer import AndroidInstallerError, AndroidInstallerService
from services.command_runner import CommandResult


class FakeRunner:
    def __init__(self, event, returncode=0):
        self.event = event
        self.returncode = returncode
        self.commands = []

    def run(self, command, cwd="", timeout=None, env=None):
        self.commands.append(list(command))
        return CommandResult(
            command=list(command),
            cwd=cwd,
            returncode=self.returncode,
            stdout=json.dumps(self.event) + "\n",
            stderr="",
            log_path="",
        )


def _helper(tmp_path):
    helper = tmp_path / "ipod6g-android-installer"
    helper.write_text("test placeholder", encoding="utf-8")
    helper.chmod(0o755)
    return helper


def _ramdiag_bundle(repo_root):
    bundle = repo_root / "rockpod" / "bin" / "ipod6g-android" / "ramdiag"
    bundle.mkdir(parents=True)
    report = {
        "artifact_gate_passed": True,
        "hardware_actions_enabled": False,
        "board": "apple-n25-ipod-classic-6g",
        "profile": "ram-only-no-storage",
        "watchdog_restart_compiled": True,
        "automatic_recovery_seconds": 60,
    }
    files = {
        "n25-ramdiag-initramfs.cpio.gz": b"initramfs",
        "n25-ramdiag.itb": b"fit",
        "n25-ramdiag-uboot.dfu": b"dfu",
        "qualification.json": json.dumps(report).encode(),
    }
    lines = []
    for name, data in files.items():
        (bundle / name).write_bytes(data)
        lines.append(f"{hashlib.sha256(data).hexdigest()}  {name}\n")
    (bundle / "SHA256SUMS").write_text("".join(lines), encoding="utf-8")
    return bundle


def _eclair_native_bundle(repo_root):
    bundle = repo_root / "rockpod" / "bin" / "ipod6g-android" / "eclair-native"
    bundle.mkdir(parents=True)
    report = {
        "artifact_gate_passed": True,
        "android_native_gate_passed": True,
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
        "software_renderer_tested": True,
        "system_server_ready": True,
        "launcher_tested": True,
        "launcher_dex_optimized": True,
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
        "full_system_emulation_gate_passed": True,
        "persistent_storage_available": False,
        "hardware_actions_enabled": False,
        "rockbox_menu_play_boot_packaged": True,
        "rockbox_boot_components_checksum_wrapped": True,
        "rockbox_menu_play_bootloader_volatile_dfu": True,
        "board": "apple-n25-ipod-classic-6g",
        "profile": "eclair-native-ram-only-no-storage",
        "aosp_tag": "android-2.0_r1",
    }
    root_report = {
        "static_gate_passed": True,
        "emulation_gate_passed": True,
        "hardware_actions_enabled": False,
        "persistent_storage_nodes": False,
        "storage_tools": False,
        "root_read_only": True,
        "dalvik_dex_parser_tested": True,
        "dalvik_vm_tested": True,
        "core_library_tested": True,
        "framework_library_tested": True,
        "n25_clickwheel_keylayout_packaged": True,
        "framework_resource_qualified": True,
        "app_process_packaged": True,
        "zygote_native_runtime_packaged": True,
        "policy_library_packaged": True,
        "services_library_packaged": True,
        "system_server_native_runtime_packaged": True,
        "headless_gralloc_packaged": True,
        "software_renderer_packaged": True,
        "installd_packaged": True,
        "settings_provider_packaged": True,
        "launcher_packaged": True,
        "data_cache_metadata": "tmpfs",
    }
    initramfs_sha256 = hashlib.sha256(b"initramfs").hexdigest()
    system_report = {
        "system_boot_gate_passed": True,
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
        "software_renderer_tested": True,
        "system_server_ready": True,
        "launcher_tested": True,
        "launcher_dex_optimized": True,
        "binder_protocol": 7,
        "android_init_stayed_alive": True,
        "persistent_storage_attached": False,
        "network_backend_attached": False,
        "hardware_actions_enabled": False,
        "artifacts": {"initramfs": {"sha256": initramfs_sha256}},
    }
    input_report = {
        "model_gate_passed": True,
        "persistent_storage_attached": False,
        "hardware_actions_enabled": False,
        "cases": {str(index): True for index in range(8)},
    }
    files = {
        "n25-eclair-native-initramfs.cpio.gz": b"initramfs",
        "eclair-root-qualification.json": json.dumps(root_report).encode(),
        "eclair-system-emulation.json": json.dumps(system_report).encode(),
        "n25-input-emulation.json": json.dumps(input_report).encode(),
        "n25-eclair-native.itb": b"fit",
        "n25-eclair-native-uboot.dfu": b"dfu",
        "n25-eclair-kernel.ipod": b"kernel-ipod",
        "n25-eclair-initramfs.ipod": b"initramfs-ipod",
        "n25-eclair-dtb.ipod": b"dtb-ipod",
        "n25-eclair-menu-play-bootloader.ipod": b"bootloader-ipod",
        "n25-eclair-menu-play-bootloader.dfu": b"bootloader-dfu",
        "qualification.json": json.dumps(report).encode(),
    }
    lines = []
    for name, data in files.items():
        (bundle / name).write_bytes(data)
        lines.append(f"{hashlib.sha256(data).hexdigest()}  {name}\n")
    (bundle / "SHA256SUMS").write_text("".join(lines), encoding="utf-8")
    return bundle


def _plan(image):
    digest = "a" * 64
    return {
        "protocol": 1,
        "event": "plan",
        "hardware_writes_enabled": False,
        "image_path": str(image.resolve()),
        "plan_digest": digest,
        "image_identity_sha256": digest,
        "mbr_sector_sha256": digest,
        "fat32_boot_sector_sha256": digest,
        "fat32_fsinfo_sector_sha256": digest,
        "safety": {
            "target_kind": "regular-file-image",
            "regular_file_only": True,
            "hardware_writes_enabled": False,
        },
    }


def test_hello_requires_regular_file_only_feature(tmp_path):
    event = {
        "protocol": 1,
        "event": "hello",
        "helper_version": "0.1.0",
        "features": ["regular-file-only", "dry-run-plan"],
        "hardware_writes_enabled": False,
    }
    service = AndroidInstallerService(
        helper_path=_helper(tmp_path),
        runner=FakeRunner(event),
    )

    assert service.hello()["hardware_writes_enabled"] is False


def test_ramdiag_status_verifies_installed_bundle(tmp_path):
    _ramdiag_bundle(tmp_path)
    status = AndroidInstallerService(repo_root=tmp_path).ramdiag_bundle_status()

    assert status["artifact_gate_passed"] is True
    assert status["hardware_actions_enabled"] is False
    assert status["profile"] == "ram-only-no-storage"
    assert status["watchdog_restart_compiled"] is True
    assert status["automatic_recovery_seconds"] == 60
    assert len(status["artifacts"]) == 4


def test_ramdiag_status_rejects_tampering_and_symlinks(tmp_path):
    bundle = _ramdiag_bundle(tmp_path)
    service = AndroidInstallerService(repo_root=tmp_path)
    (bundle / "n25-ramdiag.itb").write_bytes(b"tampered")

    with pytest.raises(AndroidInstallerError, match="checksum mismatch"):
        service.ramdiag_bundle_status()

    (bundle / "n25-ramdiag.itb").unlink()
    (bundle / "n25-ramdiag.itb").symlink_to(bundle / "n25-ramdiag-uboot.dfu")
    with pytest.raises(AndroidInstallerError, match="must not be a symlink"):
        service.ramdiag_bundle_status()


def test_ramdiag_status_rejects_unmanifested_bundle_file(tmp_path):
    bundle = _ramdiag_bundle(tmp_path)
    (bundle / "unreviewed.bin").write_bytes(b"not in manifest")

    with pytest.raises(AndroidInstallerError, match="does not match its manifest"):
        AndroidInstallerService(repo_root=tmp_path).ramdiag_bundle_status()


def test_eclair_native_status_verifies_ram_only_native_bundle(tmp_path):
    _eclair_native_bundle(tmp_path)

    status = AndroidInstallerService(repo_root=tmp_path).eclair_native_bundle_status()

    assert status["android_native_gate_passed"] is True
    assert status["hardware_actions_enabled"] is False
    assert status["persistent_storage_available"] is False
    assert status["binder_positive_path_tested"] is True
    assert status["dalvik_vm_tested"] is True
    assert status["core_library_tested"] is True
    assert status["framework_library_tested"] is True
    assert status["framework_resource_qualified"] is True
    assert status["app_process_tested"] is True
    assert status["zygote_accept_loop_tested"] is True
    assert status["policy_library_tested"] is True
    assert status["services_library_tested"] is True
    assert status["system_server_tested"] is True
    assert status["surfaceflinger_tested"] is True
    assert status["surfaceflinger_service_published"] is True
    assert status["headless_gralloc_tested"] is True
    assert status["software_renderer_tested"] is True
    assert status["system_server_ready"] is True
    assert status["launcher_tested"] is True
    assert status["launcher_dex_optimized"] is True
    assert status["n25_framebuffer_compiled"] is True
    assert status["n25_framebuffer_handoff_only"] is False
    assert status["n25_panel_init_compiled"] is True
    assert status["n25_pcf50635_display_power_compiled"] is True
    assert status["n25_clickwheel_compiled"] is True
    assert status["n25_hold_switch_compiled"] is True
    assert status["n25_reset_chord_compiled"] is True
    assert status["n25_watchdog_restart_compiled"] is True
    assert status["n25_clickwheel_keylayout_packaged"] is True
    assert status["linux_framebuffer_gralloc_compiled"] is True
    assert status["physical_display_tested"] is False
    assert status["physical_input_tested"] is False
    assert status["physical_reset_tested"] is False
    assert status["aosp_tag"] == "android-2.0_r1"
    assert status["rockbox_menu_play_boot_packaged"] is True
    assert status["rockbox_boot_components_checksum_wrapped"] is True
    assert status["rockbox_menu_play_bootloader_volatile_dfu"] is True
    assert len(status["artifacts"]) == 12


def test_eclair_boot_file_installer_preserves_rockbox_and_database(tmp_path):
    _eclair_native_bundle(tmp_path)
    device = tmp_path / "device"
    rockbox = device / ".rockbox"
    rockbox.mkdir(parents=True)
    (device / "rockbox.ipod").write_bytes(b"existing-rockbox")
    (rockbox / "rockbox.ipod").write_bytes(b"existing-rockbox")
    database = rockbox / "database_0.tcd"
    database.write_bytes(b"keep-database")

    result = AndroidInstallerService(repo_root=tmp_path).install_eclair_boot_files(device)

    assert result["installed"] is True
    assert result["boot_combo"] == "MENU+PLAY"
    assert result["partition_table_written"] is False
    assert result["nor_written"] is False
    assert result["rockbox_firmware_written"] is False
    assert database.read_bytes() == b"keep-database"
    target = rockbox / "android"
    assert {path.name for path in target.iterdir()} == {
        "n25-eclair-kernel.ipod",
        "n25-eclair-initramfs.ipod",
        "n25-eclair-dtb.ipod",
        "SHA256SUMS",
    }


def test_eclair_boot_file_installer_refuses_overwrite(tmp_path):
    _eclair_native_bundle(tmp_path)
    device = tmp_path / "device"
    target = device / ".rockbox" / "android"
    target.mkdir(parents=True)
    (device / "rockbox.ipod").write_bytes(b"existing-rockbox")
    (device / ".rockbox" / "rockbox.ipod").write_bytes(b"existing-rockbox")
    existing = target / "n25-eclair-kernel.ipod"
    existing.write_bytes(b"user-data")

    with pytest.raises(AndroidInstallerError, match="Refusing to overwrite"):
        AndroidInstallerService(repo_root=tmp_path).install_eclair_boot_files(device)

    assert existing.read_bytes() == b"user-data"


def test_eclair_native_status_rejects_storage_or_unproven_root(tmp_path):
    bundle = _eclair_native_bundle(tmp_path)
    report_path = bundle / "qualification.json"
    report = json.loads(report_path.read_text(encoding="utf-8"))
    report["persistent_storage_available"] = True
    report_path.write_text(json.dumps(report), encoding="utf-8")
    service = AndroidInstallerService(repo_root=tmp_path)

    with pytest.raises(AndroidInstallerError, match="persistent_storage_available"):
        service.eclair_native_bundle_status()

    _eclair_native_bundle(tmp_path.parent / "second")
    second = AndroidInstallerService(repo_root=tmp_path.parent / "second")
    second_root = (
        tmp_path.parent
        / "second/rockpod/bin/ipod6g-android/eclair-native/eclair-root-qualification.json"
    )
    root_report = json.loads(second_root.read_text(encoding="utf-8"))
    root_report["emulation_gate_passed"] = False
    second_root.write_text(json.dumps(root_report), encoding="utf-8")
    with pytest.raises(AndroidInstallerError, match="emulation_gate_passed"):
        second.eclair_native_bundle_status()


def test_plan_uses_argv_and_accepts_only_regular_image(tmp_path):
    image = tmp_path / "disk image.img"
    image.write_bytes(b"fixture")
    runner = FakeRunner(_plan(image))
    service = AndroidInstallerService(helper_path=_helper(tmp_path), runner=runner)

    plan = service.plan_image(image, android_size_mib=2048, sector_size=4096)

    assert plan["safety"]["regular_file_only"] is True
    assert runner.commands[0][1:3] == ["dry-run", "--image"]
    assert str(image.resolve()) in runner.commands[0]


def test_plan_rejects_helper_that_enables_hardware_writes(tmp_path):
    image = tmp_path / "ipod.img"
    image.write_bytes(b"fixture")
    event = _plan(image)
    event["hardware_writes_enabled"] = True
    service = AndroidInstallerService(
        helper_path=_helper(tmp_path),
        runner=FakeRunner(event),
    )

    with pytest.raises(AndroidInstallerError, match="hardware-write lock"):
        service.plan_image(image)


def test_plan_rejects_character_device_before_invoking_helper(tmp_path):
    runner = FakeRunner({})
    service = AndroidInstallerService(helper_path=_helper(tmp_path), runner=runner)

    with pytest.raises(AndroidInstallerError, match="regular image files only"):
        service.plan_image("/dev/null")

    assert runner.commands == []


def test_fixture_refuses_to_overwrite_existing_file(tmp_path):
    image = tmp_path / "existing.img"
    image.write_bytes(b"keep")
    runner = FakeRunner({})
    service = AndroidInstallerService(helper_path=_helper(tmp_path), runner=runner)

    with pytest.raises(AndroidInstallerError, match="Refusing to overwrite"):
        service.create_fixture(image)

    assert image.read_bytes() == b"keep"
    assert runner.commands == []


def test_transaction_requires_readback_verification(tmp_path):
    image = tmp_path / "ipod.img"
    image.write_bytes(b"fixture")
    digest = "a" * 64
    event = {
        "protocol": 1,
        "event": "image-layout-committed",
        "hardware_writes_enabled": False,
        "image_path": str(image.resolve()),
        "plan_digest_before": digest,
        "original_mbr_sha256": digest,
        "committed_mbr_sha256": digest,
        "metadata_verified": True,
        "mbr_verified": False,
    }
    service = AndroidInstallerService(
        helper_path=_helper(tmp_path),
        runner=FakeRunner(event),
    )

    with pytest.raises(AndroidInstallerError, match="read-back"):
        service.commit_image_layout(image, digest)
