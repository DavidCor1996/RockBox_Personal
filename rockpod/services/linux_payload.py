"""PC-side Linux live payload staging for RockPod-managed iPod storage."""

from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import subprocess
import urllib.request
from datetime import datetime

from services.file_safety import atomic_write_json
from services.path_safety import resolve_under_root, validate_device_root


DEBIAN_LIVE_XFCE_ISO = "debian-live-13.5.0-amd64-xfce.iso"
DEBIAN_LIVE_XFCE_SHA256 = "a0a89e62ca0b20f329878ece4bdc612cc5ad747cb5e3318a595e0ed63623d5e6"
DEBIAN_LIVE_XFCE_URL = (
    "https://cdimage.debian.org/debian-cd/current-live/amd64/iso-hybrid/"
    + DEBIAN_LIVE_XFCE_ISO
)

ROCKPOD_LINUX_ICON = "rockpod-linux.svg"
LINUX_MANIFEST_REL = ".rockpod-linux/manifest.json"
VM_ROOT_REL = "Linux/RockPodVM"
VM_MANIFEST_REL = VM_ROOT_REL + "/manifest.json"
VM_README_REL = VM_ROOT_REL + "/README.txt"
VM_ICON_REL = VM_ROOT_REL + "/" + ROCKPOD_LINUX_ICON
VM_LINUX_LAUNCHER_REL = VM_ROOT_REL + "/start-linux-linux.sh"
VM_LINUX_COMMAND_LAUNCHER_REL = VM_ROOT_REL + "/start-linux-linux.command"
VM_LINUX_TERMINAL_LAUNCHER_REL = VM_ROOT_REL + "/Start RockPod Linux.command"
VM_LINUX_DESKTOP_LAUNCHER_REL = VM_ROOT_REL + "/Start RockPod Linux.desktop"
VM_LINUX_AUTOINSTALL_REL = VM_ROOT_REL + "/autoinstall-linux-linux.command"
VM_WINDOWS_LAUNCHER_REL = VM_ROOT_REL + "/start-linux-windows.bat"
VM_MAC_LAUNCHER_REL = VM_ROOT_REL + "/start-linux-macos.command"
VM_LINUX_PROVISION_REL = VM_ROOT_REL + "/provision-rockpod-linux-linux.command"
VM_GUEST_PROVISION_REL = VM_ROOT_REL + "/rockpod-guest-provision.sh"
VM_PRESEED_REL = VM_ROOT_REL + "/preseed.cfg"
DEFAULT_ALLOCATION_GB = 16
MAX_ALLOCATION_GB = 80
DEFAULT_VM_USERNAME = "rockpod"
DEFAULT_VM_PASSWORD = "rockpod"
VM_PROVISION_VERSION = "2"
GIB = 1024 ** 3
VM_DISK_DESCRIPTOR = "rockpod-linux-data.vmdk"
VM_DISK_EXTENT_PREFIX = "rockpod-linux-data-s"
VM_DISK_EXTENT_SUFFIX = ".vmdk"
VM_LEGACY_QCOW2 = "rockpod-linux-data.qcow2"
VM_INSTALLED_FLAG = "rockpod-linux-installed.flag"
VM_INSTALLER_KERNEL = "rockpod-installer-vmlinuz"
VM_INSTALLER_INITRD = "rockpod-installer-initrd.gz"
VM_KNOWN_FILENAMES = {
    DEBIAN_LIVE_XFCE_ISO,
    "start-linux-linux.sh",
    "start-linux-linux.command",
    "Start RockPod Linux.command",
    "Start RockPod Linux.desktop",
    "autoinstall-linux-linux.command",
    "start-linux-windows.bat",
    "start-linux-macos.command",
    "provision-rockpod-linux-linux.command",
    "rockpod-guest-provision.sh",
    "preseed.cfg",
    "README.txt",
    "manifest.json",
    ROCKPOD_LINUX_ICON,
    VM_DISK_DESCRIPTOR,
    VM_LEGACY_QCOW2,
    VM_INSTALLED_FLAG,
    VM_INSTALLER_KERNEL,
    VM_INSTALLER_INITRD,
}

EXCLUDED_EXTRACTED_NAMES = {
    "[BOOT]",
}


class LinuxPayloadService:
    """Install a bootable PC Linux live payload as ordinary iPod files."""

    def payload_profile(self, profile):
        repo_root = os.path.abspath(profile.get("source_repo_path") or os.getcwd())
        backup_root = os.path.join(repo_root, "rockpod", ".backups", "linux", profile.get("id") or "device")
        return {
            "id": f"{profile.get('id') or 'device'}-linux",
            "name": f"{profile.get('name') or 'iPod'} Linux Payload",
            "device_mount_path": os.path.abspath(profile.get("device_mount_path") or ""),
            "source_repo_path": repo_root,
            "backup_location": os.path.abspath(backup_root),
        }

    def validate_mount(self, mount_path):
        root = validate_device_root(mount_path)
        if not os.path.isdir(os.path.join(root, ".rockbox")):
            raise ValueError("Refusing to install Linux payload: .rockbox marker not found")
        return root

    def verify_iso(self, iso_path, expected_sha256=DEBIAN_LIVE_XFCE_SHA256):
        path = os.path.abspath(iso_path or "")
        if not os.path.isfile(path):
            return {"success": False, "message": "ISO file does not exist", "sha256": ""}
        digest = self._sha256(path)
        return {
            "success": digest == expected_sha256,
            "message": "ISO checksum verified" if digest == expected_sha256 else "ISO checksum mismatch",
            "sha256": digest,
            "expected_sha256": expected_sha256,
            "path": path,
        }

    def download_iso(self, cache_dir, progress_callback=None):
        cache_dir = os.path.abspath(cache_dir or "")
        os.makedirs(cache_dir, exist_ok=True)
        target_path = os.path.join(cache_dir, DEBIAN_LIVE_XFCE_ISO)
        if self.verify_iso(target_path)["success"]:
            return {"success": True, "path": target_path, "downloaded": False}

        tmp_path = target_path + ".part"
        try:
            with urllib.request.urlopen(DEBIAN_LIVE_XFCE_URL, timeout=30) as response:
                total = int(response.headers.get("Content-Length") or 0)
                copied = 0
                with open(tmp_path, "wb") as handle:
                    while True:
                        chunk = response.read(1024 * 1024)
                        if not chunk:
                            break
                        handle.write(chunk)
                        copied += len(chunk)
                        if progress_callback:
                            progress_callback(copied, total)
            os.replace(tmp_path, target_path)
        finally:
            if os.path.exists(tmp_path):
                try:
                    os.remove(tmp_path)
                except OSError:
                    pass

        verification = self.verify_iso(target_path)
        return {
            "success": verification["success"],
            "path": target_path,
            "downloaded": True,
            "message": verification["message"],
        }

    def extract_iso(self, iso_path, extract_root):
        iso_path = os.path.abspath(iso_path or "")
        extract_root = os.path.abspath(extract_root or "")
        if not os.path.isfile(iso_path):
            raise ValueError("ISO file does not exist")
        if not shutil.which("7z"):
            raise ValueError("7z is required to extract the Debian live ISO")
        os.makedirs(extract_root, exist_ok=True)
        result = subprocess.run(
            ["7z", "x", "-y", f"-o{extract_root}", iso_path],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if result.returncode != 0:
            raise RuntimeError(result.stderr.strip() or result.stdout.strip() or "7z extraction failed")
        return extract_root

    def build_bundle_from_extracted(self, extracted_root):
        extracted_root = os.path.abspath(extracted_root or "")
        if not os.path.isdir(extracted_root):
            raise ValueError("Extracted Linux payload directory does not exist")
        assets = []
        total_size = 0
        for root, dirs, files in os.walk(extracted_root):
            dirs[:] = [name for name in dirs if name not in EXCLUDED_EXTRACTED_NAMES]
            for name in files:
                source_abs = os.path.join(root, name)
                rel = os.path.relpath(source_abs, extracted_root).replace(os.sep, "/")
                if rel.startswith("[BOOT]/"):
                    continue
                size = os.path.getsize(source_abs)
                total_size += size
                assets.append(
                    {
                        "kind": "linux_payload",
                        "source_rel": rel,
                        "source_abs": source_abs,
                        "destination_rel": rel,
                        "exists": True,
                        "size": size,
                        "preserve_metadata": True,
                    }
                )

        manifest = self._manifest(assets, total_size)
        manifest_path = os.path.join(extracted_root, "rockpod-linux-manifest.json")
        atomic_write_json(manifest_path, manifest)
        assets.append(
            {
                "kind": "linux_manifest",
                "source_rel": "rockpod-linux-manifest.json",
                "source_abs": manifest_path,
                "destination_rel": LINUX_MANIFEST_REL,
                "exists": True,
                "size": os.path.getsize(manifest_path),
                "preserve_metadata": False,
            }
        )
        return {
            "id": "rockpod-linux-debian-live-xfce",
            "name": "RockPod Linux Debian Live Xfce",
            "assets": assets,
            "manifest": manifest,
            "total_size": total_size + os.path.getsize(manifest_path),
        }

    def build_vm_bundle_from_iso(self, iso_path, working_root, allocation_gb=DEFAULT_ALLOCATION_GB, vm_user=None):
        verification = self.verify_iso(iso_path)
        if not verification["success"]:
            raise ValueError(verification["message"])
        credentials = self._vm_credentials(vm_user)
        working_root = os.path.abspath(working_root or "")
        os.makedirs(working_root, exist_ok=True)
        iso_size = os.path.getsize(iso_path)
        allocation = self.validate_allocation({"total_size": iso_size}, allocation_gb)
        if not allocation["valid"]:
            raise ValueError(allocation["message"])
        data_disk_gb = max(1, min(MAX_ALLOCATION_GB, allocation["allocation_gb"]) - ((iso_size + GIB - 1) // GIB))
        manifest = {
            "schema": 1,
            "mode": "portable-vm",
            "name": "RockPod Linux VM",
            "base": "Debian Live Xfce",
            "iso": DEBIAN_LIVE_XFCE_ISO,
            "iso_sha256": DEBIAN_LIVE_XFCE_SHA256,
            "arch": "x86_64",
            "allocation_gb": allocation["allocation_gb"],
            "allocation_bytes": allocation["allocation_bytes"],
            "payload_bytes": iso_size,
            "data_disk_gb": data_disk_gb,
            "vm_username": credentials["username"],
            "vm_root_login": bool(credentials["su_password"]),
            "disk_format": "split-vmdk",
            "disk_extent_pattern": VM_DISK_EXTENT_PREFIX + "###" + VM_DISK_EXTENT_SUFFIX,
            "installed_flag": VM_INSTALLED_FLAG,
            "rockbox_share_tag": "rockbox_root",
            "files": [
                DEBIAN_LIVE_XFCE_ISO,
                "start-linux-linux.sh",
                "start-linux-linux.command",
                "Start RockPod Linux.command",
                "Start RockPod Linux.desktop",
                "autoinstall-linux-linux.command",
                "provision-rockpod-linux-linux.command",
                "rockpod-guest-provision.sh",
                "start-linux-windows.bat",
                "start-linux-macos.command",
                "preseed.cfg",
                "README.txt",
                "manifest.json",
                ROCKPOD_LINUX_ICON,
                VM_DISK_DESCRIPTOR,
                VM_INSTALLED_FLAG,
                VM_INSTALLER_KERNEL,
                VM_INSTALLER_INITRD,
            ],
        }
        launcher = self._linux_vm_launcher(data_disk_gb)
        terminal_launcher = self._linux_terminal_launcher()
        desktop_launcher = self._linux_desktop_launcher()
        autoinstall_launcher = self._linux_autoinstall_launcher(data_disk_gb)
        provision_launcher = self._linux_provision_launcher()
        guest_provision = self._guest_provision_script()
        windows_launcher = self._windows_vm_launcher(data_disk_gb)
        mac_launcher = self._macos_vm_launcher(data_disk_gb)
        preseed = self._preseed(credentials)
        readme = self._vm_readme(data_disk_gb)
        generated = [
            ("start-linux-linux.sh", launcher),
            ("start-linux-linux.command", launcher),
            ("Start RockPod Linux.command", terminal_launcher),
            ("Start RockPod Linux.desktop", desktop_launcher),
            ("autoinstall-linux-linux.command", autoinstall_launcher),
            ("provision-rockpod-linux-linux.command", provision_launcher),
            ("rockpod-guest-provision.sh", guest_provision),
            ("start-linux-windows.bat", windows_launcher),
            ("start-linux-macos.command", mac_launcher),
            ("preseed.cfg", preseed),
            ("README.txt", readme),
        ]
        assets = [
            {
                "kind": "linux_vm_iso",
                "source_rel": os.path.basename(iso_path),
                "source_abs": os.path.abspath(iso_path),
                "destination_rel": f"{VM_ROOT_REL}/{DEBIAN_LIVE_XFCE_ISO}",
                "exists": True,
                "size": iso_size,
                "preserve_metadata": True,
            }
        ]
        total_size = iso_size
        icon_path = self._linux_icon_path()
        if os.path.isfile(icon_path):
            icon_size = os.path.getsize(icon_path)
            total_size += icon_size
            assets.append(
                {
                    "kind": "linux_vm_icon",
                    "source_rel": ROCKPOD_LINUX_ICON,
                    "source_abs": icon_path,
                    "destination_rel": VM_ICON_REL,
                    "exists": True,
                    "size": icon_size,
                    "preserve_metadata": True,
                }
            )
        for filename, text in generated:
            source_abs = os.path.join(working_root, filename)
            with open(source_abs, "w", encoding="utf-8", newline="\n") as handle:
                handle.write(text)
            if filename.endswith((".sh", ".command", ".desktop")):
                os.chmod(source_abs, 0o755)
            total_size += os.path.getsize(source_abs)
            assets.append(
                {
                    "kind": "linux_vm_launcher",
                    "source_rel": filename,
                    "source_abs": source_abs,
                    "destination_rel": f"{VM_ROOT_REL}/{filename}",
                    "exists": True,
                    "size": os.path.getsize(source_abs),
                    "preserve_metadata": False,
                }
            )
        manifest_path = os.path.join(working_root, "manifest.json")
        atomic_write_json(manifest_path, manifest)
        total_size += os.path.getsize(manifest_path)
        assets.append(
            {
                "kind": "linux_vm_manifest",
                "source_rel": "manifest.json",
                "source_abs": manifest_path,
                "destination_rel": VM_MANIFEST_REL,
                "exists": True,
                "size": os.path.getsize(manifest_path),
                "preserve_metadata": False,
            }
        )
        return {
            "id": "rockpod-linux-portable-vm",
            "name": "RockPod Linux Portable VM",
            "assets": assets,
            "manifest": manifest,
            "total_size": total_size,
        }

    def refresh_vm_provision_files(self, mount_path):
        root = self.validate_mount(mount_path)
        vm_root = resolve_under_root(root, VM_ROOT_REL)
        manifest_path = resolve_under_root(root, VM_MANIFEST_REL)
        if not os.path.isdir(vm_root):
            raise ValueError("RockPod Linux VM folder was not found")
        files = {
            "provision-rockpod-linux-linux.command": self._linux_provision_launcher(),
            "rockpod-guest-provision.sh": self._guest_provision_script(),
        }
        written = []
        for filename, text in files.items():
            path = resolve_under_root(vm_root, filename)
            with open(path, "w", encoding="utf-8", newline="\n") as handle:
                handle.write(text)
            os.chmod(path, 0o755)
            written.append(path)
        manifest = self._read_json(manifest_path) if os.path.isfile(manifest_path) else {}
        manifest_files = list(manifest.get("files") or [])
        for filename in files:
            if filename not in manifest_files:
                manifest_files.append(filename)
        if manifest:
            manifest["files"] = manifest_files
            manifest["provision_version"] = VM_PROVISION_VERSION
            atomic_write_json(manifest_path, manifest)
        return {"success": True, "written": written}

    def refresh_vm_launcher_files(self, mount_path):
        root = self.validate_mount(mount_path)
        vm_root = resolve_under_root(root, VM_ROOT_REL)
        manifest_path = resolve_under_root(root, VM_MANIFEST_REL)
        if not os.path.isdir(vm_root):
            raise ValueError("RockPod Linux VM folder was not found")
        manifest = self._read_json(manifest_path) if os.path.isfile(manifest_path) else {}
        data_disk_gb = int(manifest.get("data_disk_gb") or DEFAULT_ALLOCATION_GB)
        files = {
            "start-linux-linux.sh": self._linux_vm_launcher(data_disk_gb),
            "start-linux-linux.command": self._linux_vm_launcher(data_disk_gb),
            "Start RockPod Linux.command": self._linux_terminal_launcher(),
            "Start RockPod Linux.desktop": self._linux_desktop_launcher(),
        }
        written = []
        for filename, text in files.items():
            path = resolve_under_root(vm_root, filename)
            with open(path, "w", encoding="utf-8", newline="\n") as handle:
                handle.write(text)
            os.chmod(path, 0o755)
            written.append(path)
        manifest_files = list(manifest.get("files") or [])
        for filename in files:
            if filename not in manifest_files:
                manifest_files.append(filename)
        if manifest:
            manifest["files"] = manifest_files
            atomic_write_json(manifest_path, manifest)
        return {"success": True, "written": written}

    def validate_allocation(self, bundle, allocation_gb):
        allocation = max(1, int(allocation_gb or DEFAULT_ALLOCATION_GB))
        if allocation > MAX_ALLOCATION_GB:
            return {
                "valid": False,
                "message": f"Linux allocation is capped at {MAX_ALLOCATION_GB} GB",
                "allocation_gb": allocation,
                "allocation_bytes": allocation * GIB,
            }
        allocation_bytes = allocation * GIB
        total_size = int((bundle or {}).get("total_size") or 0)
        if total_size > allocation_bytes:
            return {
                "valid": False,
                "message": "Linux payload is larger than the selected allocation",
                "allocation_gb": allocation,
                "allocation_bytes": allocation_bytes,
                "payload_bytes": total_size,
            }
        return {
            "valid": True,
            "message": "Linux payload fits selected allocation",
            "allocation_gb": allocation,
            "allocation_bytes": allocation_bytes,
            "payload_bytes": total_size,
            "remaining_bytes": max(allocation_bytes - total_size, 0),
        }

    def install_bundle(self, profile, bundle, allocation_gb=DEFAULT_ALLOCATION_GB):
        allocation = self.validate_allocation(bundle, allocation_gb)
        if not allocation["valid"]:
            raise ValueError(allocation["message"])
        mount_path = self.validate_mount(profile.get("device_mount_path") or "")
        usage = shutil.disk_usage(mount_path)
        required = int(bundle.get("total_size") or 0)
        if required > usage.free:
            raise ValueError("Not enough free space on the iPod for the Linux payload")
        backup_root = os.path.abspath(profile.get("backup_location") or "")
        timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        backup_dir = os.path.join(backup_root, timestamp)
        os.makedirs(backup_dir, exist_ok=True)

        manifest = {
            "created_at": datetime.now().isoformat(timespec="seconds"),
            "mount_path": mount_path,
            "bundle_id": bundle.get("id", ""),
            "allocation_gb": allocation["allocation_gb"],
            "allocation_bytes": allocation["allocation_bytes"],
            "payload_bytes": allocation["payload_bytes"],
            "items": [],
        }
        copied = 0
        failures = []
        for asset in bundle.get("assets", []):
            rel = asset.get("destination_rel", "")
            try:
                dest_abs = resolve_under_root(mount_path, rel)
                backup_abs = resolve_under_root(backup_dir, rel)
            except ValueError as exc:
                failures.append(f"{rel}: {exc}")
                continue
            source_abs = asset.get("source_abs", "")
            if not os.path.isfile(source_abs):
                failures.append(f"{rel}: source file missing")
                continue
            existed = os.path.exists(dest_abs)
            try:
                if existed:
                    os.makedirs(os.path.dirname(backup_abs), exist_ok=True)
                    shutil.copy2(dest_abs, backup_abs)
                os.makedirs(os.path.dirname(dest_abs), exist_ok=True)
                shutil.copy2(source_abs, dest_abs)
                if self._sha256(source_abs) != self._sha256(dest_abs):
                    raise OSError("verification mismatch after copy")
                manifest["items"].append(
                    {
                        "destination_rel": rel,
                        "backup_rel": rel,
                        "existed": existed,
                        "size": os.path.getsize(dest_abs),
                    }
                )
                copied += 1
            except OSError as exc:
                failures.append(f"{rel}: {exc}")

        manifest_path = os.path.join(backup_dir, "manifest.json")
        atomic_write_json(manifest_path, manifest)
        return {
            "success": not failures,
            "copied_count": copied,
            "failure_count": len(failures),
            "failures": failures,
            "backup_dir": backup_dir,
            "manifest_path": manifest_path,
            "allocation": allocation,
        }

    def installed_status(self, mount_path):
        root = validate_device_root(mount_path)
        vm_manifest = resolve_under_root(root, VM_MANIFEST_REL)
        vm_iso = resolve_under_root(root, f"{VM_ROOT_REL}/{DEBIAN_LIVE_XFCE_ISO}")
        vm_launcher = resolve_under_root(root, VM_LINUX_LAUNCHER_REL)
        vm_installed_flag = resolve_under_root(root, f"{VM_ROOT_REL}/{VM_INSTALLED_FLAG}")
        vm_root = resolve_under_root(root, VM_ROOT_REL)
        if os.path.isfile(vm_manifest) and os.path.isfile(vm_iso) and os.path.isfile(vm_launcher):
            manifest = self._read_json(vm_manifest)
            return {
                "installed": True,
                "vm_ready": os.path.isfile(vm_installed_flag),
                "installed_flag": vm_installed_flag,
                "mode": "portable-vm",
                "manifest_path": vm_manifest,
                "efi_boot": "",
                "live_root": "",
                "launcher": vm_launcher,
                "vm_root": vm_root,
                "size_bytes": self._tree_size(vm_root),
                "allocation_gb": int(manifest.get("allocation_gb") or 0),
                "data_disk_gb": int(manifest.get("data_disk_gb") or 0),
            }
        manifest_path = resolve_under_root(root, LINUX_MANIFEST_REL)
        boot_candidates = [
            resolve_under_root(root, "EFI/boot/bootx64.efi"),
            resolve_under_root(root, "EFI/BOOT/BOOTX64.EFI"),
        ]
        live_root = resolve_under_root(root, "live")
        return {
            "installed": os.path.isfile(manifest_path) and any(os.path.isfile(path) for path in boot_candidates) and os.path.isdir(live_root),
            "manifest_path": manifest_path,
            "efi_boot": next((path for path in boot_candidates if os.path.isfile(path)), ""),
            "live_root": live_root if os.path.isdir(live_root) else "",
            "size_bytes": self._tree_size(live_root) if os.path.isdir(live_root) else 0,
        }

    def uninstall_vm(self, mount_path):
        root = self.validate_mount(mount_path)
        vm_root = resolve_under_root(root, VM_ROOT_REL)
        manifest_path = resolve_under_root(root, VM_MANIFEST_REL)
        if not os.path.isfile(manifest_path):
            raise ValueError("RockPod Linux VM manifest was not found")
        manifest = self._read_json(manifest_path)
        if manifest.get("mode") != "portable-vm":
            raise ValueError("Installed Linux payload is not a RockPod portable VM")

        removed = []
        failures = []
        allowed = set(str(name or "") for name in manifest.get("files", [])) | VM_KNOWN_FILENAMES
        try:
            for name in os.listdir(vm_root):
                if self._is_vm_disk_extent(name):
                    allowed.add(name)
        except OSError as exc:
            failures.append(str(exc))
        for name in sorted(allowed):
            if not name or "/" in name or name in {".", ".."}:
                continue
            try:
                path = resolve_under_root(vm_root, name)
            except ValueError as exc:
                failures.append(f"{name}: {exc}")
                continue
            if not os.path.exists(path):
                continue
            try:
                os.remove(path)
                removed.append(name)
            except OSError as exc:
                failures.append(f"{name}: {exc}")

        try:
            if os.path.isdir(vm_root) and not os.listdir(vm_root):
                os.rmdir(vm_root)
            linux_root = resolve_under_root(root, "Linux")
            if os.path.isdir(linux_root) and not os.listdir(linux_root):
                os.rmdir(linux_root)
        except OSError as exc:
            failures.append(str(exc))

        return {
            "success": not failures,
            "removed": removed,
            "removed_count": len(removed),
            "failures": failures,
        }

    def _linux_vm_launcher(self, data_disk_gb):
        return f"""#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "${{BASH_SOURCE[0]}}")" && pwd)"
IPOD_ROOT="$(cd "$DIR/../.." && pwd)"
ISO="$DIR/{DEBIAN_LIVE_XFCE_ISO}"
DISK="$DIR/{VM_DISK_DESCRIPTOR}"
INSTALLED_FLAG="$DIR/{VM_INSTALLED_FLAG}"
DATA_DISK_GB={int(data_disk_gb)}
QEMU_DISPLAY="${{ROCKPOD_QEMU_DISPLAY:-auto}}"
QEMU_SPICE_PORT="${{ROCKPOD_QEMU_SPICE_PORT:-5930}}"
QEMU_AUDIO_BACKEND="${{ROCKPOD_QEMU_AUDIO_BACKEND:-auto}}"

if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
    echo "qemu-system-x86_64 is required. Install QEMU on this PC, then run this script again." >&2
    exit 1
fi

QEMU_HELP="$(qemu-system-x86_64 --help 2>&1 || true)"
QEMU_DEVICE_HELP="$(qemu-system-x86_64 -device help 2>&1 || true)"

qemu_supports() {{
    grep -qF -- "$1" <<< "$QEMU_HELP"
}}

qemu_supports_display() {{
    qemu_supports "$1"
}}

qemu_supports_device() {{
    grep -qF -- "name \\\"$1\\\"" <<< "$QEMU_DEVICE_HELP"
}}

build_audio_args() {{
    QEMU_AUDIO_ARGS=()

    if [[ "$QEMU_AUDIO_BACKEND" == off ]] || [[ "$QEMU_AUDIO_BACKEND" == none ]]; then
        echo "Starting without QEMU host audio."
        return
    fi

    local backend=""
    if [[ "$QEMU_AUDIO_BACKEND" == auto ]]; then
        local candidate=""
        for candidate in pipewire pa alsa sdl; do
            if qemu_supports "audiodev $candidate"; then
                backend=$candidate
                break
            fi
        done
    elif qemu_supports "audiodev $QEMU_AUDIO_BACKEND"; then
        backend=$QEMU_AUDIO_BACKEND
    else
        echo "Requested audio backend '$QEMU_AUDIO_BACKEND' is not supported, skipping audio." >&2
        return
    fi

    if [[ -z "$backend" ]]; then
        echo "No supported qemu audio backend found; starting without host audio." >&2
        return
    fi

    QEMU_AUDIO_ARGS=(-audiodev "$backend,id=rockpod-audio")
    if qemu_supports_device intel-hda && qemu_supports_device hda-output; then
        QEMU_AUDIO_ARGS+=(-device intel-hda -device hda-output,audiodev=rockpod-audio)
    elif qemu_supports_device ich9-intel-hda && qemu_supports_device hda-duplex; then
        QEMU_AUDIO_ARGS+=(-device ich9-intel-hda -device hda-duplex,audiodev=rockpod-audio)
    elif qemu_supports_device ES1370; then
        QEMU_AUDIO_ARGS+=(-device ES1370,audiodev=rockpod-audio)
    elif qemu_supports_device AC97; then
        QEMU_AUDIO_ARGS+=(-device AC97,audiodev=rockpod-audio)
    else
        echo "No supported qemu sound card found; starting without guest audio hardware." >&2
        QEMU_AUDIO_ARGS=()
        return
    fi

    echo "Using QEMU audio backend '$backend'."
}}

if [ ! -f "$ISO" ]; then
    echo "Missing ISO: $ISO" >&2
    exit 1
fi

build_audio_args

if [ ! -f "$DISK" ]; then
    if ! command -v qemu-img >/dev/null 2>&1; then
        echo "qemu-img is required to create the capped VM data disk." >&2
        exit 1
    fi
    qemu-img create -f vmdk -o subformat=twoGbMaxExtentSparse "$DISK" "${{DATA_DISK_GB}}G"
fi

KVM_ARGS=()
if [ -r /dev/kvm ] && [ -w /dev/kvm ]; then
    KVM_ARGS=(-enable-kvm)
fi

BOOT_ARGS=(-cdrom "$ISO")
if [ -f "$INSTALLED_FLAG" ]; then
    BOOT_ARGS+=(-boot order=c)
else
    BOOT_ARGS+=(-boot d)
fi

run_qemu_with_display() {{
    local requested_display=$1
    local -a display_args=()

    if [[ "$requested_display" == spice* ]]; then
        if ! qemu_supports_display "spice"; then
            echo "Skipping SPICE backend: this QEMU build lacks SPICE support." >&2
            return 2
        fi
        display_args=(
            -vga qxl
            -spice "addr=127.0.0.1,port=${{QEMU_SPICE_PORT}},disable-ticketing=on,disable-copy-paste=off"
        )
    fi

    if ! qemu_supports_display "$requested_display"; then
        echo "Skipping '$requested_display': not supported by qemu-system-x86_64." >&2
        return 2
    fi

    echo "Starting RockPod VM with -display $requested_display."
    qemu-system-x86_64 \\
        -name "RockPod Linux" \\
        -m 4096 \\
        -smp 2 \\
        "${{KVM_ARGS[@]}}" \\
        "${{BOOT_ARGS[@]}}" \\
        -drive "file=$DISK,format=vmdk,if=virtio" \\
        -virtfs "local,path=$IPOD_ROOT,mount_tag=rockbox_root,security_model=mapped-xattr,id=rockbox_root" \\
        -device virtio-rng-pci \\
        "${{QEMU_AUDIO_ARGS[@]}}" \\
        "${{display_args[@]}}" \\
        -display "$requested_display"
}}

if [[ "$QEMU_DISPLAY" == auto ]]; then
    DISPLAY_TRY=(gtk sdl spice-app)
else
    DISPLAY_TRY=("$QEMU_DISPLAY" gtk sdl)
fi

DISPLAY_TRY_FINAL=()
for display in "${{DISPLAY_TRY[@]}}"; do
    display_exists=0
    for existing in "${{DISPLAY_TRY_FINAL[@]-}}"; do
        if [[ "$existing" == "$display" ]]; then
            display_exists=1
            break
        fi
    done
    if (( !display_exists )); then
        DISPLAY_TRY_FINAL+=("$display")
    fi
done

for display in "${{DISPLAY_TRY_FINAL[@]}}"; do
    set +e
    run_qemu_with_display "$display"
    status=$?
    set -e
    if [[ $status -eq 0 ]]; then
        exit 0
    fi
    if [[ $status -eq 130 || $status -eq 143 ]]; then
        exit "$status"
    fi
    if [[ $status -ne 2 ]]; then
        echo "Display '$display' could not start (exit $status), trying next option." >&2
    fi
done

echo "No supported qemu display backend could launch this VM." >&2
exit 1
"""

    @staticmethod
    def _linux_terminal_launcher():
        return r"""#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CMD="$DIR/start-linux-linux.command"

if [ ! -f "$CMD" ]; then
    echo "Missing VM launcher: $CMD" >&2
    exit 1
fi

run_in_terminal() {
    local title="RockPod Linux VM"
    local qdir
    local xfce_command
    printf -v qdir '%q' "$DIR"
    local command="cd $qdir && bash ./start-linux-linux.command; status=\$?; echo; echo \"RockPod Linux exited with status \$status.\"; echo \"Press Enter to close.\"; read -r _; exit \$status"

    if command -v x-terminal-emulator >/dev/null 2>&1; then
        x-terminal-emulator -T "$title" -e bash -lc "$command" &
        return 0
    fi
    if command -v konsole >/dev/null 2>&1; then
        konsole --new-tab --workdir "$DIR" -p "tabtitle=$title" -e bash -lc "$command" &
        return 0
    fi
    if command -v gnome-terminal >/dev/null 2>&1; then
        gnome-terminal --title="$title" --working-directory="$DIR" -- bash -lc "$command" &
        return 0
    fi
    if command -v xfce4-terminal >/dev/null 2>&1; then
        printf -v xfce_command 'bash -lc %q' "$command"
        xfce4-terminal --title="$title" --working-directory="$DIR" --command "$xfce_command" &
        return 0
    fi
    if command -v xterm >/dev/null 2>&1; then
        xterm -T "$title" -e bash -lc "$command" &
        return 0
    fi
    return 1
}

if ! run_in_terminal; then
    cd "$DIR"
    exec bash ./start-linux-linux.command
fi
"""

    @staticmethod
    def _linux_desktop_launcher():
        return """[Desktop Entry]
Type=Application
Name=Start RockPod Linux
Comment=Start the RockPod Debian VM from this iPod
Exec=bash -lc 'for dir in "/run/media/$USER"/*/Linux/RockPodVM "$HOME"/RockPodVM; do if [ -f "$dir/start-linux-linux.command" ]; then cd "$dir" && exec bash ./start-linux-linux.command; fi; done; echo "RockPod Linux VM folder was not found under /run/media/$USER."; echo "Run bash start-linux-linux.command from the RockPodVM folder."; read -r _'
Icon=utilities-terminal
Terminal=true
Categories=System;Emulator;
StartupNotify=true
"""

    def _linux_autoinstall_launcher(self, data_disk_gb):
        return f"""#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "${{BASH_SOURCE[0]}}")" && pwd)"
ISO="$DIR/{DEBIAN_LIVE_XFCE_ISO}"
DISK="$DIR/{VM_DISK_DESCRIPTOR}"
PRESEED="$DIR/preseed.cfg"
KERNEL="$DIR/{VM_INSTALLER_KERNEL}"
INITRD="$DIR/{VM_INSTALLER_INITRD}"
INSTALLED_FLAG="$DIR/{VM_INSTALLED_FLAG}"
DATA_DISK_GB={int(data_disk_gb)}
PRESEED_PORT="${{ROCKPOD_PRESEED_PORT:-8067}}"

if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
    echo "qemu-system-x86_64 is required. Install QEMU on this PC, then run this script again." >&2
    exit 1
fi

if [ ! -f "$ISO" ]; then
    echo "Missing ISO: $ISO" >&2
    exit 1
fi

if [ ! -f "$PRESEED" ]; then
    echo "Missing preseed file: $PRESEED" >&2
    exit 1
fi

if [ ! -f "$DISK" ]; then
    if ! command -v qemu-img >/dev/null 2>&1; then
        echo "qemu-img is required to create the capped VM data disk." >&2
        exit 1
    fi
    qemu-img create -f vmdk -o subformat=twoGbMaxExtentSparse "$DISK" "${{DATA_DISK_GB}}G"
fi

if [ ! -f "$KERNEL" ] || [ ! -f "$INITRD" ]; then
    EXTRACT_DIR="$DIR/.rockpod-installer-extract"
    rm -rf "$EXTRACT_DIR"
    mkdir -p "$EXTRACT_DIR"
    if command -v 7z >/dev/null 2>&1; then
        7z x -y -o"$EXTRACT_DIR" "$ISO" install/vmlinuz install/initrd.gz >/dev/null
    elif command -v bsdtar >/dev/null 2>&1; then
        (cd "$EXTRACT_DIR" && bsdtar -xf "$ISO" install/vmlinuz install/initrd.gz)
    else
        echo "7z or bsdtar is required to extract Debian Installer from the ISO." >&2
        exit 1
    fi
    mv "$EXTRACT_DIR/install/vmlinuz" "$KERNEL"
    mv "$EXTRACT_DIR/install/initrd.gz" "$INITRD"
    rm -rf "$EXTRACT_DIR"
fi

if ! command -v python3 >/dev/null 2>&1; then
    echo "python3 is required to serve the unattended installer preseed." >&2
    exit 1
fi

python3 -m http.server "$PRESEED_PORT" --bind 127.0.0.1 --directory "$DIR" >/tmp/rockpod-preseed-http.log 2>&1 &
SERVER_PID=$!
cleanup() {{
    kill "$SERVER_PID" >/dev/null 2>&1 || true
}}
trap cleanup EXIT
sleep 1

KVM_ARGS=()
if [ -r /dev/kvm ] && [ -w /dev/kvm ]; then
    KVM_ARGS=(-enable-kvm)
fi

qemu-system-x86_64 \\
    -name "RockPod Linux Installer" \\
    -m 4096 \\
    -smp 2 \\
    "${{KVM_ARGS[@]}}" \\
    -kernel "$KERNEL" \\
    -initrd "$INITRD" \\
    -append "auto=true priority=critical url=http://10.0.2.2:${{PRESEED_PORT}}/preseed.cfg console=ttyS0,115200n8 --- quiet" \\
    -cdrom "$ISO" \\
    -drive "file=$DISK,format=vmdk,if=virtio" \\
    -netdev user,id=net0 \\
    -device virtio-net-pci,netdev=net0 \\
    -device virtio-rng-pci \\
    -display none \\
    -serial mon:stdio

touch "$INSTALLED_FLAG"
echo "RockPod Linux install finished. Run ./start-linux-linux.command to boot the installed desktop."
"""

    @staticmethod
    def _linux_provision_launcher():
        return r"""#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DISK="$DIR/rockpod-linux-data.vmdk"
GUEST_PROVISION="$DIR/rockpod-guest-provision.sh"
MOUNT_DIR="${ROCKPOD_VM_MOUNT_DIR:-/mnt/rockpod-linux-vm}"

if [ ! -f "$DISK" ]; then
    echo "Missing VM disk: $DISK" >&2
    exit 1
fi

if [ ! -f "$GUEST_PROVISION" ]; then
    echo "Missing guest provision script: $GUEST_PROVISION" >&2
    exit 1
fi

if [ "$(id -u)" -ne 0 ]; then
    if command -v pkexec >/dev/null 2>&1; then
        exec pkexec env DISPLAY="${DISPLAY:-}" XAUTHORITY="${XAUTHORITY:-}" bash "$0"
    fi
    echo "Root permission is required to attach and mount the VM disk." >&2
    echo "Run: sudo bash $0" >&2
    exit 1
fi

if ! command -v qemu-nbd >/dev/null 2>&1; then
    echo "qemu-nbd is required to provision the installed VM disk." >&2
    exit 1
fi

if command -v lsof >/dev/null 2>&1 && lsof "$DISK" >/dev/null 2>&1; then
    echo "The VM disk is in use. Shut down RockPod Linux before provisioning." >&2
    exit 1
fi

modprobe nbd max_part=16 >/dev/null 2>&1 || true

NBD=""
for candidate in /dev/nbd{0..15}; do
    name="$(basename "$candidate")"
    size_file="/sys/block/$name/size"
    pid_file="/sys/block/$name/pid"
    if [ -b "$candidate" ] \
        && [ -r "$size_file" ] \
        && [ "$(cat "$size_file")" = "0" ] \
        && [ ! -s "$pid_file" ]; then
        NBD="$candidate"
        break
    fi
done

if [ -z "$NBD" ]; then
    echo "No free /dev/nbd device was found." >&2
    exit 1
fi

cleanup() {
    set +e
    if mountpoint -q "$MOUNT_DIR"; then
        umount "$MOUNT_DIR"
    fi
    qemu-nbd --disconnect "$NBD" >/dev/null 2>&1
}
trap cleanup EXIT

qemu-nbd --connect="$NBD" "$DISK"
partprobe "$NBD" >/dev/null 2>&1 || true
sleep 1

ROOT_PART="$(lsblk -nrpo NAME,FSTYPE "$NBD" | awk '$2 ~ /^ext[234]$/ { print $1; exit }')"
if [ -z "$ROOT_PART" ]; then
    echo "Could not find an ext filesystem in $DISK." >&2
    exit 1
fi

mkdir -p "$MOUNT_DIR"
mount "$ROOT_PART" "$MOUNT_DIR"
ROCKPOD_TARGET_ROOT="$MOUNT_DIR" bash "$GUEST_PROVISION" --install-offline
sync

echo "RockPod Linux apps/theme provisioned. Start the VM once to finish package installation."
"""

    @staticmethod
    def _guest_provision_script():
        return r"""#!/usr/bin/env bash
set -euo pipefail

TARGET_ROOT="${ROCKPOD_TARGET_ROOT:-}"
MODE="${1:---install-offline}"
VERSION="2"
ROCKPOD_APPS="mousepad ristretto file-roller pavucontrol gnome-disk-utility audacious vlc firefox-esr"
ROCKPOD_WINE_APPS="cabextract fonts-wine wine wine32 wine64 winetricks"
ROCKPOD_WINTC_APPS="xcape wintc-cursor-theme-standard-no-shadow wintc-cursor-theme-standard-with-shadow wintc-icon-theme-luna wintc-sound-theme-xp wintc-theme-luna-blue wintc-theme-luna-homestead wintc-theme-luna-metallic wintc-theme-professional wintc-theme-royale wintc-theme-royale-noir wintc-theme-zune"

target_path() {
    printf '%s%s\n' "$TARGET_ROOT" "$1"
}

write_file() {
    local path=$1
    local mode=$2
    local abs
    abs="$(target_path "$path")"
    mkdir -p "$(dirname "$abs")"
    cat > "$abs"
    chmod "$mode" "$abs"
}

install_desktop_file() {
    local base=$1
    local file=$2
    mkdir -p "$base/Desktop"
    cp "$(target_path "/usr/share/applications/$file")" "$base/Desktop/$file"
    chmod 755 "$base/Desktop/$file" || true
}

install_autostart_file() {
    local base=$1
    local file=$2
    mkdir -p "$base/.config/autostart"
    cp "$(target_path "/etc/xdg/autostart/$file")" "$base/.config/autostart/$file"
    chmod 644 "$base/.config/autostart/$file" || true
}

write_file /usr/local/share/rockpod-linux/wallpaper.svg 644 <<'EOF'
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1920 1080">
  <defs>
    <linearGradient id="bg" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="#1d4f73"/>
      <stop offset="1" stop-color="#d8edf5"/>
    </linearGradient>
  </defs>
  <rect width="1920" height="1080" fill="url(#bg)"/>
  <rect x="92" y="84" width="1736" height="912" rx="30" fill="#ffffff" opacity="0.14"/>
  <text x="130" y="185" font-family="sans-serif" font-size="72" fill="#ffffff">RockPod Linux</text>
  <text x="132" y="260" font-family="sans-serif" font-size="34" fill="#eaf6fb">Portable Debian Xfce for your iPod</text>
  <circle cx="1610" cy="790" r="132" fill="#f6f8fa" opacity="0.86"/>
  <circle cx="1610" cy="790" r="52" fill="#b8c3cf"/>
</svg>
EOF

write_file /usr/local/bin/rockpod-mount-ipod 755 <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
sudo mkdir -p /mnt/ipod
if ! mountpoint -q /mnt/ipod; then
    sudo mount -t 9p -o trans=virtio rockbox_root /mnt/ipod || true
fi
for path in /mnt/ipod/RockPod/Home /mnt/ipod/RockPod/Desktop /mnt/ipod/RockPod/Documents /mnt/ipod/RockPod/Downloads; do
    sudo mkdir -p "$path" || true
    sudo chown "$USER:$USER" "$path" >/dev/null 2>&1 || true
done
EOF

write_file /usr/local/bin/rockpod-welcome 755 <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
rockpod-mount-ipod || true
cat <<'MSG'
RockPod Linux

Shared iPod folders:
  /mnt/ipod/RockPod/Home
  /mnt/ipod/RockPod/Desktop
  /mnt/ipod/RockPod/Documents
  /mnt/ipod/RockPod/Downloads
  /mnt/ipod/Music
  /mnt/ipod/Videos
  /mnt/ipod/.rockbox
MSG
EOF

write_file /usr/local/bin/rockpod-start-wintc 755 <<'EOF'
#!/usr/bin/env bash
set -u

if ! command -v wintc-desktop >/dev/null 2>&1 \
    || ! command -v wintc-taskband >/dev/null 2>&1; then
    exit 0
fi

LOCK_DIR="${XDG_RUNTIME_DIR:-/tmp}/rockpod-wintc-start.lock"
if ! mkdir "$LOCK_DIR" >/dev/null 2>&1; then
    exit 0
fi
trap 'rmdir "$LOCK_DIR" >/dev/null 2>&1 || true' EXIT

xfce4-panel --quit >/dev/null 2>&1 || true
xfdesktop --quit >/dev/null 2>&1 || true

if ! pgrep -u "$USER" -x wintc-desktop >/dev/null 2>&1; then
    nohup wintc-desktop >/tmp/rockpod-wintc-desktop.log 2>&1 &
fi

if ! pgrep -u "$USER" -x wintc-taskband >/dev/null 2>&1; then
    nohup wintc-taskband >/tmp/rockpod-wintc-taskband.log 2>&1 &
fi

if command -v xcape >/dev/null 2>&1; then
    pgrep -u "$USER" -x xcape >/dev/null 2>&1 || xcape -e 'Super_L=Alt_L|F1' >/dev/null 2>&1 &
fi
EOF

write_file /usr/local/bin/rockpod-run-nibiru 755 <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

PREFIX="${WINEPREFIX:-$HOME/.wine}"
EXE="$PREFIX/drive_c/Program Files (x86)/The Adventure Company/Nibiru/nibiru.exe"
if [ ! -f "$EXE" ]; then
    EXE="$PREFIX/drive_c/Program Files/The Adventure Company/Nibiru/nibiru.exe"
fi
if [ ! -f "$EXE" ]; then
    echo "NiBiRu is not installed in $PREFIX." >&2
    exit 1
fi

cd "$(dirname "$EXE")"
if command -v wine-stable >/dev/null 2>&1; then
    exec env WINEPREFIX="$PREFIX" wine-stable "$EXE"
fi
exec env WINEPREFIX="$PREFIX" wine "$EXE"
EOF

write_file /usr/local/sbin/rockpod-guest-provision 755 <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
ROCKPOD_APPS="mousepad ristretto file-roller pavucontrol gnome-disk-utility audacious vlc firefox-esr"
ROCKPOD_WINE_APPS="cabextract fonts-wine wine wine32 wine64 winetricks"
ROCKPOD_WINTC_APPS="xcape wintc-cursor-theme-standard-no-shadow wintc-cursor-theme-standard-with-shadow wintc-icon-theme-luna wintc-sound-theme-xp wintc-theme-luna-blue wintc-theme-luna-homestead wintc-theme-luna-metallic wintc-theme-professional wintc-theme-royale wintc-theme-royale-noir wintc-theme-zune"

if [ "${1:-}" = "--firstboot" ]; then
    if [ ! -f /var/lib/rockpod-linux/apps-installed ]; then
        export DEBIAN_FRONTEND=noninteractive
        dpkg --add-architecture i386 || true
        apt-get update || true
        apt-get install -y $ROCKPOD_APPS $ROCKPOD_WINE_APPS || true
        apt-get install -y $ROCKPOD_WINTC_APPS || true
        mkdir -p /var/lib/rockpod-linux
        touch /var/lib/rockpod-linux/apps-installed
    fi
    /usr/local/bin/rockpod-mount-ipod || true
    exit 0
fi

mkdir -p /var/lib/rockpod-linux
touch /var/lib/rockpod-linux/provisioned
EOF

write_file /etc/systemd/system/rockpod-firstboot-provision.service 644 <<'EOF'
[Unit]
Description=RockPod Linux first boot app and mount provisioning
After=network-online.target graphical.target
Wants=network-online.target

[Service]
Type=oneshot
ExecStart=/usr/local/sbin/rockpod-guest-provision --firstboot
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
EOF

mkdir -p "$(target_path /etc/systemd/system/multi-user.target.wants)"
ln -sf /etc/systemd/system/rockpod-firstboot-provision.service \
    "$(target_path /etc/systemd/system/multi-user.target.wants/rockpod-firstboot-provision.service)"

write_file /usr/share/applications/rockpod-home.desktop 644 <<'EOF'
[Desktop Entry]
Type=Application
Name=RockPod Home
Comment=Open shared RockPod files on the iPod
Exec=sh -c 'rockpod-mount-ipod; exo-open --launch FileManager /mnt/ipod/RockPod/Home'
Icon=folder
Terminal=false
Categories=Utility;
EOF

write_file /usr/share/applications/rockpod-music.desktop 644 <<'EOF'
[Desktop Entry]
Type=Application
Name=iPod Music
Comment=Open the shared iPod Music folder
Exec=sh -c 'rockpod-mount-ipod; exo-open --launch FileManager /mnt/ipod/Music'
Icon=multimedia-player
Terminal=false
Categories=Audio;Utility;
EOF

write_file /usr/share/applications/rockpod-rockbox.desktop 644 <<'EOF'
[Desktop Entry]
Type=Application
Name=Rockbox Root
Comment=Open the Rockbox folder on the iPod
Exec=sh -c 'rockpod-mount-ipod; exo-open --launch FileManager /mnt/ipod/.rockbox'
Icon=drive-removable-media
Terminal=false
Categories=Utility;
EOF

write_file /usr/share/applications/rockpod-welcome.desktop 644 <<'EOF'
[Desktop Entry]
Type=Application
Name=RockPod Welcome
Comment=Show RockPod Linux shared folders
Exec=xfce4-terminal --hold -e /usr/local/bin/rockpod-welcome
Icon=help-about
Terminal=false
Categories=Utility;
EOF

write_file /etc/xdg/autostart/rockpod-mount-ipod.desktop 644 <<'EOF'
[Desktop Entry]
Type=Application
Name=Mount iPod share
Exec=/usr/local/bin/rockpod-mount-ipod
X-GNOME-Autostart-enabled=true
EOF

write_file /etc/xdg/autostart/rockpod-wintc.desktop 644 <<'EOF'
[Desktop Entry]
Type=Application
Name=RockPod XP Taskbar
Exec=/usr/local/bin/rockpod-start-wintc
OnlyShowIn=XFCE;
StartupNotify=false
Terminal=false
Hidden=false
EOF

write_file "/etc/xdg/autostart/WinXP Taskbar.desktop" 644 <<'EOF'
[Desktop Entry]
Type=Application
Name=WinXP Taskbar
Exec=/usr/local/bin/rockpod-start-wintc
OnlyShowIn=XFCE;
StartupNotify=false
Terminal=false
Hidden=false
EOF

write_file "/etc/xdg/autostart/WinXP.desktop" 644 <<'EOF'
[Desktop Entry]
Type=Application
Name=WinXP Desktop
Exec=/usr/local/bin/rockpod-start-wintc
OnlyShowIn=XFCE;
StartupNotify=false
Terminal=false
Hidden=false
EOF

write_file "/etc/xdg/autostart/XP Start.desktop" 644 <<'EOF'
[Desktop Entry]
Type=Application
Name=XP Start
Exec=/usr/local/bin/rockpod-start-wintc
OnlyShowIn=XFCE;
StartupNotify=false
Terminal=false
Hidden=false
EOF

write_file "/etc/xdg/autostart/XP Start Button.desktop" 644 <<'EOF'
[Desktop Entry]
Type=Application
Name=XP Start Button
Exec=sh -c "command -v xcape >/dev/null 2>&1 && xcape -e 'Super_L=Alt_L|F1'"
OnlyShowIn=XFCE;
StartupNotify=false
Terminal=false
Hidden=false
EOF

write_file /etc/skel/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-desktop.xml 644 <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<channel name="xfce4-desktop" version="1.0">
  <property name="backdrop" type="empty">
    <property name="screen0" type="empty">
      <property name="monitorVirtual-1" type="empty">
        <property name="workspace0" type="empty">
          <property name="color-style" type="int" value="0"/>
          <property name="image-style" type="int" value="5"/>
          <property name="last-image" type="string" value="/usr/local/share/rockpod-linux/wallpaper.svg"/>
        </property>
      </property>
    </property>
  </property>
</channel>
EOF

write_file /etc/skel/.config/xfce4/xfconf/xfce-perchannel-xml/xsettings.xml 644 <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<channel name="xsettings" version="1.0">
  <property name="Net" type="empty">
    <property name="ThemeName" type="string" value="Windows XP style (Blue)"/>
    <property name="IconThemeName" type="string" value="WinTC-Luna"/>
  </property>
</channel>
EOF

write_file /etc/skel/.config/xfce4/xfconf/xfce-perchannel-xml/xfwm4.xml 644 <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<channel name="xfwm4" version="1.0">
  <property name="general" type="empty">
    <property name="theme" type="string" value="Windows XP style (Blue)"/>
    <property name="button_layout" type="string" value="O|SHMC"/>
    <property name="title_alignment" type="string" value="center"/>
    <property name="title_font" type="string" value="Sans Bold 9"/>
    <property name="show_app_icon" type="bool" value="false"/>
    <property name="use_compositing" type="bool" value="true"/>
  </property>
</channel>
EOF

write_file /usr/share/applications/rockpod-nibiru.desktop 644 <<'EOF'
[Desktop Entry]
Type=Application
Name=NiBiRu
Comment=Run NiBiRu through Wine
Exec=/usr/local/bin/rockpod-run-nibiru
Icon=37FF_nibiru.0
Terminal=false
Categories=Game;
StartupNotify=true
StartupWMClass=nibiru.exe
EOF

SKEL_BASE="$(target_path /etc/skel)"
for base in "$SKEL_BASE" "$(target_path /home)"/*; do
    [ -d "$base" ] || continue
    if [ "$base" != "$SKEL_BASE" ]; then
        mkdir -p "$base/.config/xfce4/xfconf/xfce-perchannel-xml"
        cp "$(target_path /etc/skel/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-desktop.xml)" \
            "$base/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-desktop.xml"
        cp "$(target_path /etc/skel/.config/xfce4/xfconf/xfce-perchannel-xml/xsettings.xml)" \
            "$base/.config/xfce4/xfconf/xfce-perchannel-xml/xsettings.xml"
        cp "$(target_path /etc/skel/.config/xfce4/xfconf/xfce-perchannel-xml/xfwm4.xml)" \
            "$base/.config/xfce4/xfconf/xfce-perchannel-xml/xfwm4.xml"
    fi
    install_desktop_file "$base" rockpod-home.desktop
    install_desktop_file "$base" rockpod-music.desktop
    install_desktop_file "$base" rockpod-rockbox.desktop
    install_desktop_file "$base" rockpod-welcome.desktop
    install_desktop_file "$base" rockpod-nibiru.desktop
    install_autostart_file "$base" rockpod-wintc.desktop
    install_autostart_file "$base" "WinXP Taskbar.desktop"
    install_autostart_file "$base" "WinXP.desktop"
    install_autostart_file "$base" "XP Start.desktop"
    install_autostart_file "$base" "XP Start Button.desktop"
    if [ "$base" != "$SKEL_BASE" ]; then
        user="$(basename "$base")"
        if [ -z "$TARGET_ROOT" ]; then
            chown -R "$user:$user" "$base/Desktop" "$base/.config" >/dev/null 2>&1 || true
        fi
    fi
done

mkdir -p "$(target_path /var/lib/rockpod-linux)"
printf '%s\n' "$VERSION" > "$(target_path /var/lib/rockpod-linux/provision-version)"

if [ "$MODE" = "--firstboot" ] && [ -z "$TARGET_ROOT" ]; then
    /usr/local/sbin/rockpod-guest-provision --firstboot
fi
"""

    def _windows_vm_launcher(self, data_disk_gb):
        return f"""@echo off
setlocal
set DIR=%~dp0
set ISO=%DIR%{DEBIAN_LIVE_XFCE_ISO}
set DISK=%DIR%{VM_DISK_DESCRIPTOR}
set INSTALLED_FLAG=%DIR%{VM_INSTALLED_FLAG}
set DATA_DISK_GB={int(data_disk_gb)}

where qemu-system-x86_64.exe >nul 2>nul
if errorlevel 1 (
  echo qemu-system-x86_64.exe is required. Install QEMU for Windows and add it to PATH.
  exit /b 1
)

if not exist "%ISO%" (
  echo Missing ISO: %ISO%
  exit /b 1
)

if not exist "%DISK%" (
  where qemu-img.exe >nul 2>nul
  if errorlevel 1 (
    echo qemu-img.exe is required to create the capped VM data disk.
    exit /b 1
  )
  qemu-img.exe create -f vmdk -o subformat=twoGbMaxExtentSparse "%DISK%" %DATA_DISK_GB%G
)

if exist "%INSTALLED_FLAG%" (
  set BOOT=-boot order=c
) else (
  set BOOT=-boot d
)

qemu-system-x86_64.exe -name "RockPod Linux" -m 4096 -smp 2 -cdrom "%ISO%" %BOOT% -drive "file=%DISK%,format=vmdk,if=virtio" -display gtk
"""

    def _macos_vm_launcher(self, data_disk_gb):
        return f"""#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "${{BASH_SOURCE[0]}}")" && pwd)"
ISO="$DIR/{DEBIAN_LIVE_XFCE_ISO}"
DISK="$DIR/{VM_DISK_DESCRIPTOR}"
INSTALLED_FLAG="$DIR/{VM_INSTALLED_FLAG}"
DATA_DISK_GB={int(data_disk_gb)}

if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
    echo "qemu-system-x86_64 is required. Install QEMU for macOS first." >&2
    echo "Homebrew example: brew install qemu" >&2
    exit 1
fi

if [ ! -f "$ISO" ]; then
    echo "Missing ISO: $ISO" >&2
    exit 1
fi

if [ ! -f "$DISK" ]; then
    if ! command -v qemu-img >/dev/null 2>&1; then
        echo "qemu-img is required to create the capped VM data disk." >&2
        exit 1
    fi
    qemu-img create -f vmdk -o subformat=twoGbMaxExtentSparse "$DISK" "${{DATA_DISK_GB}}G"
fi

BOOT_ARGS=(-cdrom "$ISO")
if [ -f "$INSTALLED_FLAG" ]; then
    BOOT_ARGS+=(-boot order=c)
else
    BOOT_ARGS+=(-boot d)
fi

# This uses x86_64 emulation so the same Debian live desktop boots on Intel PCs
# and Apple Silicon Macs. On M1/M2/M3 it is compatible but slower than a future
# native arm64 image.
exec qemu-system-x86_64 \\
    -name "RockPod Linux" \\
    -m 4096 \\
    -smp 2 \\
    -accel tcg \\
    "${{BOOT_ARGS[@]}}" \\
    -drive "file=$DISK,format=vmdk,if=virtio" \\
    -device virtio-rng-pci \\
    -display cocoa
"""

    def _vm_readme(self, data_disk_gb):
        return f"""RockPod Linux Portable VM

This folder is managed by RockPod. It is ordinary storage content on the iPod.
Rockbox and the iPod bootloader do not read or boot these files.

Linux allocation cap:
- Debian live ISO plus launch files are copied here.
- The RockPod Linux icon is copied here for launchers and folder previews.
- The VM data disk is split VMDK and capped at {int(data_disk_gb)} GB.
- Split VMDK avoids the FAT 4 GB single-file limit on stock iPod storage.
- Total RockPod Linux allocation is capped at {MAX_ALLOCATION_GB} GB.

Linux host:
1. Install QEMU if it is not already installed.
2. Run ./autoinstall-linux-linux.command once to install Debian to the VM disk.
3. Run ./start-linux-linux.command from this folder.
   If your shell still refuses to execute it, run: bash start-linux-linux.sh
4. Inside Debian, mount the iPod root share with:
   sudo mkdir -p /mnt/ipod
   sudo mount -t 9p -o trans=virtio rockbox_root /mnt/ipod

Windows host:
1. Install QEMU for Windows and add it to PATH.
2. Run start-linux-windows.bat.

M1/M2/M3 Mac:
1. Install QEMU for macOS.
2. Run start-linux-macos.command.
3. This boots the same x86_64 Debian live desktop through emulation. It is
   compatible with Apple Silicon but slower than a future native arm64 image.

Do not delete files in this folder while the VM is running.
"""

    @staticmethod
    def _vm_credentials(vm_user=None):
        vm_user = vm_user or {}
        username = str(vm_user.get("username") or DEFAULT_VM_USERNAME).strip().lower()
        password = str(vm_user.get("password") or DEFAULT_VM_PASSWORD)
        su_password = str(vm_user.get("su_password") or "")
        if not re.match(r"^[a-z][-a-z0-9_]{0,31}$", username):
            raise ValueError("Linux username must start with a lowercase letter and use only lowercase letters, numbers, '-' or '_'")
        for label, value in (("Linux password", password), ("Linux su password", su_password)):
            if "\n" in value or "\r" in value:
                raise ValueError(f"{label} cannot contain line breaks")
        if not password:
            raise ValueError("Linux user password cannot be empty")
        return {
            "username": username,
            "password": password,
            "su_password": su_password,
        }

    @staticmethod
    def _preseed(credentials=None):
        credentials = LinuxPayloadService._vm_credentials(credentials)
        root_lines = "d-i passwd/root-login boolean false"
        if credentials["su_password"]:
            root_lines = "\n".join(
                [
                    "d-i passwd/root-login boolean true",
                    f"d-i passwd/root-password password {credentials['su_password']}",
                    f"d-i passwd/root-password-again password {credentials['su_password']}",
                ]
            )
        return f"""d-i debian-installer/locale string en_US.UTF-8
d-i debian-installer/language string en
d-i debian-installer/country string US
d-i keyboard-configuration/xkb-keymap select us

d-i netcfg/choose_interface select auto
d-i netcfg/get_hostname string rockpod-linux
d-i netcfg/get_domain string local
d-i hw-detect/load_firmware boolean true

d-i mirror/country string manual
d-i mirror/http/hostname string deb.debian.org
d-i mirror/http/directory string /debian
d-i mirror/http/proxy string
d-i apt-setup/non-free-firmware boolean true
d-i apt-setup/use_mirror boolean true

{root_lines}
d-i passwd/user-fullname string {credentials['username']}
d-i passwd/username string {credentials['username']}
d-i passwd/user-password password {credentials['password']}
d-i passwd/user-password-again password {credentials['password']}
d-i user-setup/allow-password-weak boolean true
d-i user-setup/encrypt-home boolean false

d-i clock-setup/utc boolean true
d-i time/zone string America/New_York
d-i clock-setup/ntp boolean true

d-i partman-auto/disk string /dev/vda
d-i partman-auto/method string regular
d-i partman-lvm/device_remove_lvm boolean true
d-i partman-md/device_remove_md boolean true
d-i partman-auto/choose_recipe select atomic
d-i partman-partitioning/confirm_write_new_label boolean true
d-i partman/choose_partition select finish
d-i partman/confirm boolean true
d-i partman/confirm_nooverwrite boolean true

tasksel tasksel/first multiselect xfce-desktop, standard
d-i pkgsel/include string sudo qemu-guest-agent spice-vdagent openssh-client mousepad ristretto file-roller pavucontrol gnome-disk-utility audacious vlc firefox-esr cabextract fonts-wine wine wine64 winetricks xcape gtk2-engines-pixbuf gnome-themes-extra
d-i pkgsel/upgrade select none
popularity-contest popularity-contest/participate boolean false

d-i grub-installer/only_debian boolean true
d-i grub-installer/with_other_os boolean true
d-i grub-installer/bootdev string /dev/vda
d-i finish-install/reboot_in_progress note
d-i cdrom-detect/eject boolean false
d-i debian-installer/exit/poweroff boolean true

d-i preseed/late_command string in-target usermod -aG sudo {credentials['username']}; wget -O /target/usr/local/sbin/rockpod-guest-provision http://10.0.2.2:8067/rockpod-guest-provision.sh || true; chmod 755 /target/usr/local/sbin/rockpod-guest-provision || true; ROCKPOD_TARGET_ROOT=/target /bin/bash /target/usr/local/sbin/rockpod-guest-provision --install-offline || true; in-target systemctl enable qemu-guest-agent || true
"""

    @staticmethod
    def _tree_size(path):
        total = 0
        if not os.path.isdir(path):
            return 0
        for root, _dirs, files in os.walk(path):
            for name in files:
                try:
                    total += os.path.getsize(os.path.join(root, name))
                except OSError:
                    pass
        return total

    @staticmethod
    def _linux_icon_path():
        return os.path.abspath(
            os.path.join(os.path.dirname(__file__), "..", "assets", "icons", ROCKPOD_LINUX_ICON)
        )

    @staticmethod
    def _is_vm_disk_extent(name):
        if not name.startswith(VM_DISK_EXTENT_PREFIX) or not name.endswith(VM_DISK_EXTENT_SUFFIX):
            return False
        number = name[len(VM_DISK_EXTENT_PREFIX) : -len(VM_DISK_EXTENT_SUFFIX)]
        return len(number) == 3 and number.isdigit()

    @staticmethod
    def _read_json(path):
        try:
            with open(path, "r", encoding="utf-8") as handle:
                return json.load(handle)
        except (OSError, json.JSONDecodeError):
            return {}

    def _manifest(self, assets, total_size):
        return {
            "schema": 1,
            "name": "RockPod Linux",
            "base": "Debian Live Xfce",
            "iso": DEBIAN_LIVE_XFCE_ISO,
            "iso_sha256": DEBIAN_LIVE_XFCE_SHA256,
            "arch": "x86_64",
            "boot_modes": ["uefi", "bios"],
            "total_size": total_size,
            "file_count": len(assets),
            "rockbox_mount": "/mnt/ipod",
        }

    @staticmethod
    def _sha256(path):
        digest = hashlib.sha256()
        with open(path, "rb") as handle:
            while True:
                chunk = handle.read(1024 * 1024)
                if not chunk:
                    break
                digest.update(chunk)
        return digest.hexdigest()
