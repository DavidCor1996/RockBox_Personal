"""Strict image helper plus bounded iPod 6G Android RAM-payload stager."""

from __future__ import annotations

import hashlib
import json
import os
import re
from pathlib import Path

from services.command_runner import CommandRunner


PROTOCOL_VERSION = 1
HISTORICAL_ANDROID_DIAGNOSTIC_DIRS = frozenset(
    {
        "diagnostic",
        "diagnostic-timerb",
        "diagnostic-vic1",
        "diagnostic-lcd1",
        "diagnostic-trace1",
        "diagnostic-trace2",
    }
)


class AndroidInstallerError(RuntimeError):
    """Raised when the native qualification helper cannot produce a safe result."""


class AndroidInstallerService:
    """Run image-only layout tools and the bounded Rockbox payload stager."""

    def __init__(self, repo_root="", helper_path="", runner=None):
        self.repo_root = Path(repo_root or Path(__file__).resolve().parents[2]).resolve()
        self._explicit_helper = Path(helper_path).resolve() if helper_path else None
        self._runner = runner or CommandRunner()

    def helper_candidates(self):
        candidates = []
        env_path = os.environ.get("ROCKPOD_ANDROID_INSTALLER_HELPER", "").strip()
        if self._explicit_helper:
            candidates.append(self._explicit_helper)
        if env_path:
            candidates.append(Path(env_path).expanduser().resolve())
        native_root = self.repo_root / "rockpod" / "native" / "ipod6g_android_installer"
        candidates.extend(
            [
                self.repo_root
                / "rockpod"
                / "bin"
                / "linux-x86_64"
                / "ipod6g-android-installer",
                native_root / "target" / "release" / "ipod6g-android-installer",
                native_root / "target" / "debug" / "ipod6g-android-installer",
            ]
        )
        return candidates

    def helper_path(self):
        for candidate in self.helper_candidates():
            if candidate.is_file() and os.access(candidate, os.X_OK):
                return candidate
        searched = "\n".join(f"- {path}" for path in self.helper_candidates())
        raise AndroidInstallerError(
            "Android qualification helper is not built. Searched:\n" + searched
        )

    def ramdiag_bundle_status(self):
        """Verify the installed, non-launchable N25 RAM diagnostic packet."""
        bundle = self.repo_root / "rockpod" / "bin" / "ipod6g-android" / "ramdiag"
        qualification = self._regular_bundle_file(
            bundle / "qualification.json", bundle, "RAM diagnostic"
        )
        sums_file = self._regular_bundle_file(
            bundle / "SHA256SUMS", bundle, "RAM diagnostic"
        )
        try:
            report = json.loads(qualification.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise AndroidInstallerError(f"Invalid RAM diagnostic qualification: {error}") from error
        if report.get("artifact_gate_passed") is not True:
            raise AndroidInstallerError("RAM diagnostic artifact gate has not passed")
        if report.get("hardware_actions_enabled") is not False:
            raise AndroidInstallerError("RAM diagnostic bundle does not retain the hardware lock")
        if report.get("board") != "apple-n25-ipod-classic-6g":
            raise AndroidInstallerError("RAM diagnostic bundle targets an unexpected board")
        if report.get("watchdog_restart_compiled") is not True:
            raise AndroidInstallerError("RAM diagnostic reset implementation is not qualified")
        if report.get("automatic_recovery_seconds") != 60:
            raise AndroidInstallerError("RAM diagnostic has no bounded recovery timeout")
        expected = self._parse_sha256sums(sums_file, "RAM diagnostic")
        required = {
            "n25-ramdiag-initramfs.cpio.gz",
            "n25-ramdiag.itb",
            "n25-ramdiag-uboot.dfu",
            "qualification.json",
        }
        if set(expected) != required:
            raise AndroidInstallerError("RAM diagnostic checksum manifest has unexpected entries")
        self._reject_unmanifested_entries(bundle, required, "RAM diagnostic")
        artifacts = {}
        for name in sorted(required):
            path = self._regular_bundle_file(bundle / name, bundle, "RAM diagnostic")
            digest = self._sha256(path)
            if digest != expected[name]:
                raise AndroidInstallerError(f"RAM diagnostic checksum mismatch: {name}")
            artifacts[name] = {"size": path.stat().st_size, "sha256": digest}
        return {
            "artifact_gate_passed": True,
            "hardware_actions_enabled": False,
            "board": report["board"],
            "profile": report.get("profile"),
            "watchdog_restart_compiled": True,
            "automatic_recovery_seconds": 60,
            "bundle_path": str(bundle),
            "artifacts": artifacts,
        }

    def eclair_native_bundle_status(self):
        """Verify the installed Android 2.0 native-core RAM bundle.

        This is deliberately status-only.  It does not expose a launch, DFU, or
        physical-device operation.
        """
        label = "Android 2.0 native-core"
        bundle = self.repo_root / "rockpod" / "bin" / "ipod6g-android" / "eclair-native"
        qualification = self._regular_bundle_file(
            bundle / "qualification.json", bundle, label
        )
        root_qualification = self._regular_bundle_file(
            bundle / "eclair-root-qualification.json", bundle, label
        )
        system_qualification = self._regular_bundle_file(
            bundle / "eclair-system-emulation.json", bundle, label
        )
        input_qualification = self._regular_bundle_file(
            bundle / "n25-input-emulation.json", bundle, label
        )
        boot_chord_qualification = self._regular_bundle_file(
            bundle / "n25-boot-chord-emulation.json", bundle, label
        )
        volatile_force_qualification = self._regular_bundle_file(
            bundle / "n25-forced-volatile-boot-emulation.json", bundle, label
        )
        chain_qualification = self._regular_bundle_file(
            bundle / "n25-select-right-chain-emulation.json", bundle, label
        )
        sums_file = self._regular_bundle_file(bundle / "SHA256SUMS", bundle, label)
        try:
            report = json.loads(qualification.read_text(encoding="utf-8"))
            root_report = json.loads(root_qualification.read_text(encoding="utf-8"))
            system_report = json.loads(system_qualification.read_text(encoding="utf-8"))
            input_report = json.loads(input_qualification.read_text(encoding="utf-8"))
            boot_chord_report = json.loads(
                boot_chord_qualification.read_text(encoding="utf-8")
            )
            volatile_force_report = json.loads(
                volatile_force_qualification.read_text(encoding="utf-8")
            )
            chain_report = json.loads(
                chain_qualification.read_text(encoding="utf-8")
            )
        except (OSError, json.JSONDecodeError) as error:
            raise AndroidInstallerError(f"Invalid {label} qualification: {error}") from error

        required_gates = {
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
            "linux_framebuffer_gralloc_compiled": True,
            "physical_display_tested": False,
            "physical_input_tested": False,
            "physical_reset_tested": False,
            "full_system_emulation_gate_passed": True,
            "persistent_storage_available": False,
            "hardware_actions_enabled": False,
            "rockbox_select_right_boot_packaged": True,
            "rockbox_select_right_boot_binary_emulated": True,
            "rockbox_to_linux_zimage_binary_emulated": True,
            "rockbox_boot_image_sizes_binary_verified": True,
            "rockbox_boot_components_checksum_wrapped": True,
            "rockbox_select_right_bootloader_volatile_dfu": True,
            "rockbox_forced_android_volatile_test_binary_emulated": True,
            "rockbox_select_right_dualboot_nor_installer_packaged": True,
            "rockbox_nor_installer_preserves_original_firmware": True,
            "rockbox_dualboot_nor_uninstaller_packaged": True,
        }
        for name, expected_value in required_gates.items():
            if report.get(name) is not expected_value:
                raise AndroidInstallerError(f"{label} gate rejected: {name}")
        if report.get("board") != "apple-n25-ipod-classic-6g":
            raise AndroidInstallerError(f"{label} bundle targets an unexpected board")
        if report.get("profile") != "eclair-native-ram-only-no-storage":
            raise AndroidInstallerError(f"{label} bundle has an unexpected safety profile")
        if report.get("aosp_tag") != "android-2.0_r1":
            raise AndroidInstallerError(f"{label} bundle is not the pinned AOSP release")
        if report.get("volatile_test_timeout_seconds") != 180:
            raise AndroidInstallerError(f"{label} has no bounded volatile-test timeout")

        root_gates = {
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
        }
        for name, expected_value in root_gates.items():
            if root_report.get(name) is not expected_value:
                raise AndroidInstallerError(f"{label} root gate rejected: {name}")
        if root_report.get("data_cache_metadata") != "tmpfs":
            raise AndroidInstallerError(f"{label} mutable state is not RAM-only")

        system_gates = {
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
            "android_init_stayed_alive": True,
            "persistent_storage_attached": False,
            "network_backend_attached": False,
            "hardware_actions_enabled": False,
        }
        for name, expected_value in system_gates.items():
            if system_report.get(name) is not expected_value:
                raise AndroidInstallerError(f"{label} system gate rejected: {name}")
        if system_report.get("binder_protocol") != 7:
            raise AndroidInstallerError(f"{label} uses an incompatible Binder protocol")
        input_cases = input_report.get("cases", {})
        if (
            input_report.get("model_gate_passed") is not True
            or input_report.get("persistent_storage_attached") is not False
            or input_report.get("hardware_actions_enabled") is not False
            or len(input_cases) != 8
            or not all(input_cases.values())
        ):
            raise AndroidInstallerError(f"{label} input-model gate rejected")
        boot_cases = boot_chord_report.get("cases", {})
        expected_boot_cases = {
            f"0x{buttons:02x}": {
                "android": buttons == 0x09,
                "usb": buttons == 0x42,
            }
            for buttons in range(128)
        }
        boot_chord_gates = {
            "schema": 1,
            "scope": "ipod6g-rockbox-boot-chord-exact-binary-emulation",
            "cpu": "ARM926EJ-S",
            "button_patterns_tested": 128,
            "android_chord": "Select+Right",
            "android_button_bits": "0x09",
            "usb_chord": "Menu+Play",
            "usb_button_bits": "0x42",
            "exact_chords_only": True,
            "main_calls_qualified_functions": True,
            "android_usb_overlap": False,
            "gate_passed": True,
        }
        if any(
            boot_chord_report.get(name) != expected
            for name, expected in boot_chord_gates.items()
        ) or boot_cases != expected_boot_cases:
            raise AndroidInstallerError(f"{label} boot-chord gate rejected")
        forced_cases = volatile_force_report.get("forced_cases", {})
        forced_gates = {
            **boot_chord_gates,
            "forced_android_volatile_test": True,
            "forced_button_patterns_tested": 128,
            "forced_main_calls_qualified_function": True,
        }
        if (
            any(
                volatile_force_report.get(name) != expected
                for name, expected in forced_gates.items()
            )
            or volatile_force_report.get("cases") != expected_boot_cases
            or set(forced_cases) != set(expected_boot_cases)
            or not all(value is True for value in forced_cases.values())
        ):
            raise AndroidInstallerError(
                f"{label} forced volatile boot gate rejected"
            )

        required = {
            "n25-eclair-native-initramfs.cpio.gz",
            "eclair-root-qualification.json",
            "eclair-system-emulation.json",
            "n25-input-emulation.json",
            "n25-boot-chord-emulation.json",
            "n25-forced-volatile-boot-emulation.json",
            "n25-select-right-chain-emulation.json",
            "n25-eclair-native.itb",
            "n25-eclair-native-uboot.dfu",
            "n25-eclair-kernel.ipod",
            "n25-eclair-initramfs.ipod",
            "n25-eclair-dtb.ipod",
            "n25-eclair-select-right-bootloader.ipod",
            "n25-eclair-select-right-bootloader.dfu",
            "n25-eclair-select-right-forced-volatile-test.dfu",
            "n25-eclair-select-right-nor-installer.dfu",
            "n25-eclair-select-right-nor-uninstaller.dfu",
            "qualification.json",
        }
        expected = self._parse_sha256sums(sums_file, label)
        if set(expected) != required:
            raise AndroidInstallerError(f"{label} checksum manifest has unexpected entries")
        self._reject_unmanifested_entries(bundle, required, label)
        artifacts = {}
        for name in sorted(required):
            path = self._regular_bundle_file(bundle / name, bundle, label)
            digest = self._sha256(path)
            if digest != expected[name]:
                raise AndroidInstallerError(f"{label} checksum mismatch: {name}")
            artifacts[name] = {"size": path.stat().st_size, "sha256": digest}
        emulated_initramfs = (
            system_report.get("artifacts", {}).get("initramfs", {}).get("sha256")
        )
        if emulated_initramfs != artifacts[
            "n25-eclair-native-initramfs.cpio.gz"
        ]["sha256"]:
            raise AndroidInstallerError(f"{label} emulated a different initramfs")
        bootloader_wrapper = self._regular_bundle_file(
            bundle / "n25-eclair-select-right-bootloader.ipod", bundle, label
        ).read_bytes()
        if len(bootloader_wrapper) <= 8 or bootloader_wrapper[4:8] != b"ip6g":
            raise AndroidInstallerError(f"{label} bootloader wrapper is invalid")
        bootloader_body = bootloader_wrapper[8:]
        expected_sum = (71 + sum(bootloader_body)) & 0xFFFFFFFF
        if int.from_bytes(bootloader_wrapper[:4], "big") != expected_sum:
            raise AndroidInstallerError(f"{label} bootloader checksum is invalid")
        bootloader_body_sha256 = hashlib.sha256(bootloader_body).hexdigest()
        qualified_bootloader_sha256 = (
            report.get("artifacts", {})
            .get("rockbox_bootloader_bin", {})
            .get("sha256")
        )
        if (
            boot_chord_report.get("bootloader_bin_sha256")
            != bootloader_body_sha256
            or qualified_bootloader_sha256 != bootloader_body_sha256
        ):
            raise AndroidInstallerError(
                f"{label} boot-chord report used a different bootloader"
            )
        forced_bootloader_sha256 = (
            report.get("artifacts", {})
            .get("rockbox_volatile_test_bin", {})
            .get("sha256")
        )
        forced_dfu = self._regular_bundle_file(
            bundle / "n25-eclair-select-right-forced-volatile-test.dfu",
            bundle,
            label,
        ).read_bytes()
        if len(forced_dfu) <= 0x800 or forced_dfu[:8] != b"87021.0\x02":
            raise AndroidInstallerError(
                f"{label} forced volatile DFU wrapper is invalid"
            )
        forced_size = int.from_bytes(forced_dfu[12:16], "little")
        forced_body = forced_dfu[0x800:]
        if (
            forced_size != len(forced_body)
            or forced_size > 0x3C000
            or forced_size % 16
            or volatile_force_report.get("bootloader_bin_sha256")
            != forced_bootloader_sha256
            or not forced_bootloader_sha256
        ):
            raise AndroidInstallerError(
                f"{label} forced volatile DFU qualification is inconsistent"
            )
        # Padding is excluded from the qualified raw payload digest.  Find the
        # unique prefix length recorded by the qualification report and insist
        # that all remaining IMG1 bytes are zero.
        matching_lengths = [
            length
            for length in range(max(1, len(forced_body) - 15), len(forced_body) + 1)
            if hashlib.sha256(forced_body[:length]).hexdigest()
            == forced_bootloader_sha256
            and not any(forced_body[length:])
        ]
        if len(matching_lengths) != 1:
            raise AndroidInstallerError(
                f"{label} forced volatile DFU embeds a different bootloader"
            )
        chain_required = {
            "gate_passed": True,
            "cpu": "ARM926EJ-S",
            "image_format": "zImage",
            "image_sha256": (
                report.get("artifacts", {}).get("zimage", {}).get("sha256")
            ),
            "dtb_sha256": (
                report.get("artifacts", {}).get("linux_dtb", {}).get("sha256")
            ),
            "initramfs_sha256": artifacts[
                "n25-eclair-native-initramfs.cpio.gz"
            ]["sha256"],
            "rockbox_bootloader_sha256": bootloader_body_sha256,
            "stop_at": "s5l_lcd_probe",
            "model_n25_timer": True,
            "verify_n25_irq": True,
            "verify_n25_lcd": False,
            "rockbox_handoff_executed": True,
        }
        if any(
            chain_report.get(name) != expected
            for name, expected in chain_required.items()
        ):
            raise AndroidInstallerError(f"{label} production-chain gate rejected")
        entry = chain_report.get("rockbox_linux_entry_state", {})
        milestones = set(chain_report.get("milestones", ()))
        required_milestones = {
            "stext",
            "__lookup_processor_type",
            "__turn_mmu_on",
            "start_kernel",
            "s5l8702_clkevt_set_periodic",
            "s5l8702_timer_interrupt",
            "of_platform_default_populate_init",
            "s5l_lcd_driver_init",
            "s5l_lcd_probe",
        }
        injections = chain_report.get("timer_irq_injections", 0)
        if (
            entry.get("r0") != 0
            or entry.get("r1") != 0xFFFFFFFF
            or entry.get("r2") != 0x0AD00000
            or entry.get("cpsr", 0) & 0xFF != 0xD3
            or not required_milestones.issubset(milestones)
            or chain_report.get("timer_e_reads", 0) <= 0
            or injections <= 0
            or chain_report.get("timer_irq_acks") != injections
            or chain_report.get("vic_address_reads", 0) < injections * 2
            or chain_report.get("vic_address_completions", 0) < injections * 2
        ):
            raise AndroidInstallerError(f"{label} production-chain details rejected")
        nor_installer = self._regular_bundle_file(
            bundle / "n25-eclair-select-right-nor-installer.dfu", bundle, label
        ).read_bytes()
        info_offset = 0x30C + 5396 - 80
        payload_offset = info_offset + 0x800
        padded_size = (len(bootloader_body) + 15) & ~15
        if (
            len(nor_installer) != payload_offset + padded_size
            or nor_installer[:8] != b"87021.0\x03"
            or nor_installer[info_offset:info_offset + 8] != b"87021.0\x02"
            or int.from_bytes(
                nor_installer[info_offset + 12:info_offset + 16], "little"
            ) != padded_size
            or any(nor_installer[info_offset + 64:info_offset + 80])
            or nor_installer[payload_offset:payload_offset + len(bootloader_body)]
            != bootloader_body
            or any(nor_installer[payload_offset + len(bootloader_body):])
        ):
            raise AndroidInstallerError(
                f"{label} dual-boot NOR installer is invalid or single-boot"
            )
        nor_uninstaller = self._regular_bundle_file(
            bundle / "n25-eclair-select-right-nor-uninstaller.dfu",
            bundle,
            label,
        ).read_bytes()
        if (
            len(nor_uninstaller) != 5856
            or nor_uninstaller[:8] != b"87021.0\x03"
            or not any(nor_uninstaller[0x30C:])
        ):
            raise AndroidInstallerError(
                f"{label} dual-boot NOR uninstaller is invalid"
            )
        return {
            "artifact_gate_passed": True,
            "android_native_gate_passed": True,
            "hardware_actions_enabled": False,
            "persistent_storage_available": False,
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
            "rockbox_select_right_boot_packaged": True,
            "rockbox_select_right_boot_binary_emulated": True,
            "rockbox_to_linux_zimage_binary_emulated": True,
            "rockbox_boot_image_sizes_binary_verified": True,
            "rockbox_boot_components_checksum_wrapped": True,
            "rockbox_select_right_bootloader_volatile_dfu": True,
            "rockbox_forced_android_volatile_test_binary_emulated": True,
            "rockbox_select_right_dualboot_nor_installer_packaged": True,
            "rockbox_nor_installer_preserves_original_firmware": True,
            "rockbox_dualboot_nor_uninstaller_packaged": True,
            "board": report["board"],
            "profile": report["profile"],
            "aosp_tag": report["aosp_tag"],
            "bundle_path": str(bundle),
            "artifacts": artifacts,
        }

    def install_eclair_boot_files(self, device_root):
        """Stage only the checksum-wrapped RAM-boot components on Rockbox.

        This operation never writes a partition table, boot sector, Rockbox
        firmware, database file, or NOR.  It may replace an older set only
        when the existing canonical files exactly match their own checksum
        manifest.  Unrecognized or partially staged content is never replaced.
        """
        status = self.eclair_native_bundle_status()
        try:
            root = Path(device_root).expanduser().resolve(strict=True)
        except OSError as error:
            raise AndroidInstallerError(f"iPod mount is unavailable: {error}") from error
        if not root.is_dir():
            raise AndroidInstallerError("iPod mount must be a directory")

        rockbox = root / ".rockbox"
        if rockbox.is_symlink() or not rockbox.is_dir():
            raise AndroidInstallerError("Mounted volume has no safe .rockbox directory")
        try:
            rockbox.resolve(strict=True).relative_to(root)
        except (OSError, ValueError) as error:
            raise AndroidInstallerError(".rockbox escapes the mounted volume") from error

        root_firmware = self._regular_device_file(root / "rockbox.ipod", root)
        dot_firmware = self._regular_device_file(rockbox / "rockbox.ipod", root)
        if self._sha256(root_firmware) != self._sha256(dot_firmware):
            raise AndroidInstallerError(
                "The two installed Rockbox firmware copies do not match"
            )

        database_before = self._database_snapshot(rockbox, root)
        target = rockbox / "android"
        if target.is_symlink():
            raise AndroidInstallerError("Refusing symlinked .rockbox/android directory")
        try:
            target.mkdir(mode=0o755, exist_ok=True)
        except OSError as error:
            raise AndroidInstallerError(
                f"Cannot create .rockbox/android (is the iPod read-only?): {error}"
            ) from error
        if not target.is_dir():
            raise AndroidInstallerError(".rockbox/android is not a directory")

        names = (
            "n25-eclair-kernel.ipod",
            "n25-eclair-initramfs.ipod",
            "n25-eclair-dtb.ipod",
        )
        allowed = set(names) | {"SHA256SUMS"}
        entries = {entry.name: entry for entry in target.iterdir()}
        unexpected = sorted(
            set(entries) - allowed - HISTORICAL_ANDROID_DIAGNOSTIC_DIRS
        )
        if unexpected:
            raise AndroidInstallerError(
                "Refusing unrecognized .rockbox/android content: "
                + ", ".join(unexpected)
            )
        historical_before = {}
        for name in sorted(set(entries) & HISTORICAL_ANDROID_DIAGNOSTIC_DIRS):
            directory = entries[name]
            if directory.is_symlink() or not directory.is_dir():
                raise AndroidInstallerError(
                    f"Historical Android diagnostic path is unsafe: {name}"
                )
            historical_before[name] = self._tree_snapshot(directory, root)

        existing = set(entries) - HISTORICAL_ANDROID_DIAGNOSTIC_DIRS
        if existing and existing != allowed:
            raise AndroidInstallerError(
                "Refusing partial .rockbox/android content; expected the three "
                "canonical payloads and SHA256SUMS"
            )

        previous = {}
        if existing:
            manifest = self._regular_device_file(target / "SHA256SUMS", root)
            previous = self._parse_sha256sums(manifest, "staged Android")
            if set(previous) != set(names):
                raise AndroidInstallerError(
                    "Refusing Android boot files with an unrecognized manifest"
                )
            for name in names:
                current = self._regular_device_file(target / name, root)
                if self._sha256(current) != previous[name]:
                    raise AndroidInstallerError(
                        f"Refusing modified Android boot file: {name}"
                    )

        bundle = Path(status["bundle_path"])
        installed = []
        sources = {}
        for name in names:
            source = self._regular_bundle_file(bundle / name, bundle, "Android 2.0 native-core")
            digest = self._sha256(source)
            sources[name] = (source, digest)
            installed.append({
                "name": name,
                "size": source.stat().st_size,
                "sha256": digest,
            })

        manifest_body = "".join(
            f"{record['sha256']}  {record['name']}\n" for record in installed
        ).encode("ascii")
        desired = {record["name"]: record["sha256"] for record in installed}
        already_current = (
            previous == desired
            and self._regular_device_file(target / "SHA256SUMS", root).read_bytes()
            == manifest_body
        )
        if not already_current:
            self._replace_android_payload_set(
                target, names, sources, manifest_body, replacing=bool(existing)
            )

        for record in installed:
            destination = self._regular_device_file(target / record["name"], root)
            if (
                destination.stat().st_size != record["size"]
                or self._sha256(destination) != record["sha256"]
            ):
                raise AndroidInstallerError(
                    f"Android boot-file read-back failed: {record['name']}"
                )
        manifest = target / "SHA256SUMS"
        if self._regular_device_file(manifest, root).read_bytes() != manifest_body:
            raise AndroidInstallerError("Android boot manifest read-back failed")

        if self._database_snapshot(rockbox, root) != database_before:
            raise AndroidInstallerError("Rockbox database changed during Android staging")
        historical_after = {
            name: self._tree_snapshot(target / name, root)
            for name in historical_before
        }
        if historical_after != historical_before:
            raise AndroidInstallerError(
                "Historical Android diagnostic files changed during staging"
            )
        self._fsync_directory(target)
        return {
            "installed": True,
            "updated": bool(existing) and not already_current,
            "already_current": already_current,
            "mount_path": str(root),
            "device_directory": ".rockbox/android",
            "boot_combo": "SELECT+RIGHT",
            "partition_table_written": False,
            "nor_written": False,
            "rockbox_firmware_written": False,
            "database_files_preserved": True,
            "historical_diagnostic_directories_preserved": sorted(
                historical_before
            ),
            "files": installed,
        }

    def _replace_android_payload_set(
        self, target, names, sources, manifest_body, replacing
    ):
        """Replace one manifest-owned payload set, rolling back runtime errors."""
        ordered_names = tuple(names) + ("SHA256SUMS",)
        staged = {
            name: target / f".rockpod-android-new-{index}"
            for index, name in enumerate(ordered_names)
        }
        backups = {
            name: target / f".rockpod-android-old-{index}"
            for index, name in enumerate(ordered_names)
        }
        reserved = tuple(staged.values()) + tuple(backups.values())
        if any(path.exists() or path.is_symlink() for path in reserved):
            raise AndroidInstallerError(
                "Refusing stale Android staging transaction files"
            )

        moved = []
        installed = []
        try:
            for name in names:
                self._copy_new_file(sources[name][0], staged[name])
                if self._sha256(staged[name]) != sources[name][1]:
                    raise AndroidInstallerError(
                        f"Android temporary-file read-back failed: {name}"
                    )
            self._write_new_file(manifest_body, staged["SHA256SUMS"])
            self._fsync_directory(target)

            for name in ordered_names:
                destination = target / name
                if replacing:
                    os.replace(destination, backups[name])
                    moved.append(name)
                os.replace(staged[name], destination)
                installed.append(name)
            self._fsync_directory(target)
        except (OSError, AndroidInstallerError) as error:
            rollback_errors = []
            for name in reversed(ordered_names):
                destination = target / name
                backup = backups[name]
                try:
                    if name in moved:
                        if destination.exists() and not destination.is_symlink():
                            destination.unlink()
                        os.replace(backup, destination)
                    elif name in installed and destination.exists():
                        destination.unlink()
                except OSError as rollback_error:
                    rollback_errors.append(str(rollback_error))
            for path in reserved:
                try:
                    if path.exists() and not path.is_symlink():
                        path.unlink()
                except OSError as rollback_error:
                    rollback_errors.append(str(rollback_error))
            detail = f": {error}"
            if rollback_errors:
                detail += "; rollback errors: " + "; ".join(rollback_errors)
            raise AndroidInstallerError(
                "Cannot transactionally stage Android boot files" + detail
            ) from error

        for path in backups.values():
            if path.exists():
                try:
                    path.unlink()
                except OSError as error:
                    raise AndroidInstallerError(
                        f"Android files were updated but backup cleanup failed: {error}"
                    ) from error
        self._fsync_directory(target)

    @staticmethod
    def _regular_device_file(path, root):
        if path.is_symlink():
            raise AndroidInstallerError(f"Device file must not be a symlink: {path.name}")
        try:
            resolved = path.resolve(strict=True)
            resolved.relative_to(root)
        except (OSError, ValueError) as error:
            raise AndroidInstallerError(f"Missing or escaped device file: {path.name}") from error
        if not resolved.is_file():
            raise AndroidInstallerError(f"Device path is not a regular file: {path.name}")
        return resolved

    def _database_snapshot(self, rockbox, root):
        snapshot = {}
        for pattern in ("database*.tcd", "tagcache*.tcd"):
            for path in rockbox.glob(pattern):
                regular = self._regular_device_file(path, root)
                snapshot[path.name] = {
                    "size": regular.stat().st_size,
                    "sha256": self._sha256(regular),
                }
        return snapshot

    def _tree_snapshot(self, directory, root):
        snapshot = {}
        for path in directory.rglob("*"):
            if path.is_symlink():
                raise AndroidInstallerError(
                    f"Historical Android diagnostic symlink is unsafe: {path.name}"
                )
            try:
                resolved = path.resolve(strict=True)
                resolved.relative_to(root)
            except (OSError, ValueError) as error:
                raise AndroidInstallerError(
                    f"Historical Android diagnostic path escapes volume: {path.name}"
                ) from error
            relative = str(resolved.relative_to(directory))
            if resolved.is_dir():
                snapshot[relative + "/"] = {"type": "directory"}
            elif resolved.is_file():
                snapshot[relative] = {
                    "type": "file",
                    "size": resolved.stat().st_size,
                    "sha256": self._sha256(resolved),
                }
            else:
                raise AndroidInstallerError(
                    f"Historical Android diagnostic entry is unsafe: {path.name}"
                )
        return snapshot

    @staticmethod
    def _write_new_file(body, destination):
        flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
        if hasattr(os, "O_NOFOLLOW"):
            flags |= os.O_NOFOLLOW
        descriptor = None
        try:
            descriptor = os.open(destination, flags, 0o644)
            view = memoryview(body)
            while view:
                written = os.write(descriptor, view)
                if written <= 0:
                    raise OSError("short write")
                view = view[written:]
            os.fsync(descriptor)
        except OSError as error:
            if descriptor is not None:
                os.close(descriptor)
                descriptor = None
            try:
                destination.unlink()
            except OSError:
                pass
            raise AndroidInstallerError(f"Cannot stage {destination.name}: {error}") from error
        finally:
            if descriptor is not None:
                os.close(descriptor)

    @classmethod
    def _copy_new_file(cls, source, destination):
        cls._write_new_file(source.read_bytes(), destination)

    @staticmethod
    def _fsync_directory(path):
        flags = os.O_RDONLY
        if hasattr(os, "O_DIRECTORY"):
            flags |= os.O_DIRECTORY
        try:
            descriptor = os.open(path, flags)
            try:
                os.fsync(descriptor)
            finally:
                os.close(descriptor)
        except OSError as error:
            raise AndroidInstallerError(f"Cannot sync Android boot directory: {error}") from error

    @staticmethod
    def _reject_unmanifested_entries(bundle, required, label):
        expected = set(required) | {"SHA256SUMS"}
        try:
            actual = {entry.name for entry in bundle.iterdir()}
        except OSError as error:
            raise AndroidInstallerError(f"Cannot inspect {label} bundle: {error}") from error
        if actual != expected:
            extras = sorted(actual - expected)
            missing = sorted(expected - actual)
            detail = []
            if extras:
                detail.append("unexpected: " + ", ".join(extras))
            if missing:
                detail.append("missing: " + ", ".join(missing))
            raise AndroidInstallerError(
                f"{label} bundle directory does not match its manifest ({'; '.join(detail)})"
            )

    @staticmethod
    def _regular_bundle_file(path, bundle, label="bundle"):
        if path.is_symlink():
            raise AndroidInstallerError(
                f"{label} artifact must not be a symlink: {path.name}"
            )
        try:
            resolved = path.resolve(strict=True)
            resolved.relative_to(bundle.resolve(strict=True))
        except (FileNotFoundError, ValueError) as error:
            raise AndroidInstallerError(
                f"Missing or escaped {label} artifact: {path.name}"
            ) from error
        if not resolved.is_file():
            raise AndroidInstallerError(
                f"{label} artifact is not a regular file: {path.name}"
            )
        return resolved

    @staticmethod
    def _parse_sha256sums(path, label="bundle"):
        values = {}
        for line in path.read_text(encoding="utf-8").splitlines():
            parts = line.split(maxsplit=1)
            if len(parts) != 2 or not re.fullmatch(r"[0-9a-f]{64}", parts[0]):
                raise AndroidInstallerError(f"Malformed {label} checksum manifest")
            name = parts[1].lstrip(" *")
            if not name or "/" in name or name in values:
                raise AndroidInstallerError(f"Unsafe {label} checksum entry")
            values[name] = parts[0]
        return values

    @staticmethod
    def _sha256(path):
        digest = hashlib.sha256()
        with path.open("rb") as handle:
            for chunk in iter(lambda: handle.read(1024 * 1024), b""):
                digest.update(chunk)
        return digest.hexdigest()

    def hello(self):
        event = self._invoke(["hello"], expected_event="hello")
        features = event.get("features")
        if not isinstance(features, list) or "regular-file-only" not in features:
            raise AndroidInstallerError("Helper does not advertise the regular-file-only gate")
        return event

    def plan_image(self, image_path, android_size_mib=1024, sector_size=4096):
        image = self._require_regular_image(image_path)
        event = self._invoke(
            [
                "dry-run",
                "--image",
                str(image),
                "--android-size-mib",
                str(int(android_size_mib)),
                "--sector-size",
                str(int(sector_size)),
            ],
            expected_event="plan",
        )
        self._validate_plan(event, image)
        return event

    def create_fixture(
        self,
        image_path,
        size_mib=4096,
        android_size_mib=1024,
        sector_size=4096,
        pre_shrunk=False,
    ):
        image = Path(image_path).expanduser().resolve()
        if image.exists():
            raise AndroidInstallerError(f"Refusing to overwrite existing fixture: {image}")
        command = [
            "create-fixture",
            "--image",
            str(image),
            "--size-mib",
            str(int(size_mib)),
            "--android-size-mib",
            str(int(android_size_mib)),
            "--sector-size",
            str(int(sector_size)),
        ]
        if pre_shrunk:
            command.append("--pre-shrunk")
        return self._invoke(command, expected_event="fixture-created")

    def commit_image_layout(
        self,
        image_path,
        expected_plan_digest,
        android_size_mib=1024,
        sector_size=4096,
    ):
        image = self._require_regular_image(image_path)
        event = self._invoke(
            [
                "commit-image-layout",
                "--image",
                str(image),
                "--expected-plan-digest",
                str(expected_plan_digest),
                "--android-size-mib",
                str(int(android_size_mib)),
                "--sector-size",
                str(int(sector_size)),
            ],
            expected_event="image-layout-committed",
        )
        self._validate_transaction(event, image)
        return event

    def verify_image_layout(self, image_path, android_size_mib=1024, sector_size=4096):
        image = self._require_regular_image(image_path)
        event = self._invoke(
            [
                "verify-image-layout",
                "--image",
                str(image),
                "--android-size-mib",
                str(int(android_size_mib)),
                "--sector-size",
                str(int(sector_size)),
            ],
            expected_event="image-layout-verified",
        )
        self._validate_transaction(event, image)
        return event

    def rollback_image_layout(self, image_path, android_size_mib=1024, sector_size=4096):
        image = self._require_regular_image(image_path)
        event = self._invoke(
            [
                "rollback-image-layout",
                "--image",
                str(image),
                "--android-size-mib",
                str(int(android_size_mib)),
                "--sector-size",
                str(int(sector_size)),
            ],
            expected_event="image-layout-rolled-back",
        )
        self._validate_transaction(event, image)
        return event

    def _require_regular_image(self, image_path):
        image = Path(image_path).expanduser().resolve()
        if not image.is_file():
            raise AndroidInstallerError(f"Dry runs accept regular image files only: {image}")
        return image

    def _invoke(self, arguments, expected_event):
        helper = self.helper_path()
        result = self._runner.run(
            [str(helper), *arguments],
            cwd=str(self.repo_root),
            timeout=30,
        )
        event = self._decode_event(result.stdout)
        if not result.success:
            message = event.get("message") if event else ""
            if not message:
                message = result.stderr.strip() or result.failure_message()
            raise AndroidInstallerError(message)
        if not event:
            raise AndroidInstallerError("Helper returned no JSON protocol event")
        self._validate_envelope(event, expected_event)
        return event

    @staticmethod
    def _decode_event(stdout):
        lines = [line.strip() for line in (stdout or "").splitlines() if line.strip()]
        if not lines:
            return {}
        try:
            value = json.loads(lines[-1])
        except json.JSONDecodeError as error:
            raise AndroidInstallerError(f"Helper returned malformed JSON: {error}") from error
        if not isinstance(value, dict):
            raise AndroidInstallerError("Helper protocol event must be a JSON object")
        return value

    @staticmethod
    def _validate_envelope(event, expected_event):
        if event.get("protocol") != PROTOCOL_VERSION:
            raise AndroidInstallerError("Helper protocol version mismatch")
        if event.get("event") != expected_event:
            raise AndroidInstallerError(
                f"Expected helper event {expected_event!r}, received {event.get('event')!r}"
            )
        if event.get("hardware_writes_enabled") is not False:
            raise AndroidInstallerError("Refusing helper without an explicit hardware-write lock")

    @staticmethod
    def _validate_plan(plan, image):
        safety = plan.get("safety")
        if not isinstance(safety, dict):
            raise AndroidInstallerError("Plan is missing its safety state")
        if safety.get("target_kind") != "regular-file-image":
            raise AndroidInstallerError("Plan target is not a regular-file image")
        if safety.get("regular_file_only") is not True:
            raise AndroidInstallerError("Plan does not retain the regular-file-only gate")
        if safety.get("hardware_writes_enabled") is not False:
            raise AndroidInstallerError("Plan unexpectedly enables hardware writes")
        if Path(plan.get("image_path", "")).resolve() != image:
            raise AndroidInstallerError("Plan image identity is bound to a different path")
        for digest_name in (
            "plan_digest",
            "image_identity_sha256",
            "mbr_sector_sha256",
            "fat32_boot_sector_sha256",
            "fat32_fsinfo_sector_sha256",
        ):
            value = plan.get(digest_name, "")
            if len(value) != 64 or any(character not in "0123456789abcdef" for character in value):
                raise AndroidInstallerError(f"Plan has invalid {digest_name}")

    @staticmethod
    def _validate_transaction(event, image):
        if Path(event.get("image_path", "")).resolve() != image:
            raise AndroidInstallerError("Transaction result is bound to a different image")
        if event.get("metadata_verified") is not True or event.get("mbr_verified") is not True:
            raise AndroidInstallerError("Transaction did not pass metadata and MBR read-back")
        for digest_name in (
            "plan_digest_before",
            "original_mbr_sha256",
            "committed_mbr_sha256",
        ):
            value = event.get(digest_name, "")
            if len(value) != 64 or any(character not in "0123456789abcdef" for character in value):
                raise AndroidInstallerError(f"Transaction has invalid {digest_name}")
