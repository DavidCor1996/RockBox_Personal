import os

from services.linux_payload import DEBIAN_LIVE_XFCE_SHA256, LinuxPayloadService


def _make_file(path, content=b"x"):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    mode = "wb" if isinstance(content, bytes) else "w"
    with open(path, mode) as handle:
        handle.write(content)


def test_linux_payload_bundle_preserves_live_iso_layout(tmp_dir):
    extracted = os.path.join(tmp_dir, "extracted")
    _make_file(os.path.join(extracted, "EFI", "boot", "bootx64.efi"), b"efi")
    _make_file(os.path.join(extracted, "boot", "grub", "grub.cfg"), b"grub")
    _make_file(os.path.join(extracted, "live", "filesystem.squashfs"), b"squash")
    _make_file(os.path.join(extracted, "[BOOT]", "1-Boot-NoEmul.img"), b"skip")

    bundle = LinuxPayloadService().build_bundle_from_extracted(extracted)
    destinations = {asset["destination_rel"] for asset in bundle["assets"]}

    assert "EFI/boot/bootx64.efi" in destinations
    assert "boot/grub/grub.cfg" in destinations
    assert "live/filesystem.squashfs" in destinations
    assert ".rockpod-linux/manifest.json" in destinations
    assert "[BOOT]/1-Boot-NoEmul.img" not in destinations


def test_linux_payload_install_requires_rockbox_marker_and_backs_up(tmp_dir):
    service = LinuxPayloadService()
    extracted = os.path.join(tmp_dir, "extracted")
    device = os.path.join(tmp_dir, "device")
    _make_file(os.path.join(extracted, "EFI", "boot", "bootx64.efi"), b"new")
    _make_file(os.path.join(extracted, "live", "filesystem.squashfs"), b"squash")
    _make_file(os.path.join(device, ".rockbox", "config.cfg"), "volume: -20\n")
    _make_file(os.path.join(device, "EFI", "boot", "bootx64.efi"), b"old")

    bundle = service.build_bundle_from_extracted(extracted)
    profile = {
        "id": "ipod",
        "name": "iPod",
        "device_mount_path": device,
        "source_repo_path": tmp_dir,
        "backup_location": os.path.join(tmp_dir, "backups"),
    }

    result = service.install_bundle(profile, bundle, allocation_gb=16)

    assert result["success"] is True
    with open(os.path.join(device, "EFI", "boot", "bootx64.efi"), "rb") as handle:
        assert handle.read() == b"new"
    assert os.path.isfile(os.path.join(device, ".rockpod-linux", "manifest.json"))
    assert service.installed_status(device)["installed"] is True

    backup_file = os.path.join(result["backup_dir"], "EFI", "boot", "bootx64.efi")
    with open(backup_file, "rb") as handle:
        assert handle.read() == b"old"


def test_linux_payload_install_rejects_non_rockbox_root(tmp_dir):
    service = LinuxPayloadService()
    extracted = os.path.join(tmp_dir, "extracted")
    device = os.path.join(tmp_dir, "device")
    os.makedirs(device, exist_ok=True)
    _make_file(os.path.join(extracted, "live", "filesystem.squashfs"), b"squash")
    bundle = service.build_bundle_from_extracted(extracted)

    try:
        service.install_bundle(
            {
                "id": "ipod",
                "device_mount_path": device,
                "source_repo_path": tmp_dir,
                "backup_location": os.path.join(tmp_dir, "backups"),
            },
            bundle,
        )
    except ValueError as exc:
        assert ".rockbox marker not found" in str(exc)
    else:
        raise AssertionError("Expected non-Rockbox root to be rejected")


def test_linux_payload_allocation_is_capped_at_80gb(tmp_dir):
    service = LinuxPayloadService()
    bundle = {"total_size": 4 * 1024 ** 3}

    assert service.validate_allocation(bundle, 80)["valid"] is True
    too_large = service.validate_allocation(bundle, 81)

    assert too_large["valid"] is False
    assert "capped at 80 GB" in too_large["message"]


def test_linux_payload_rejects_payload_larger_than_selected_allocation(tmp_dir):
    service = LinuxPayloadService()
    bundle = {"total_size": 17 * 1024 ** 3}
    result = service.validate_allocation(bundle, 16)

    assert result["valid"] is False
    assert "larger than the selected allocation" in result["message"]


def test_linux_vm_bundle_uses_single_root_folder_and_no_efi_boot_tree(tmp_dir):
    service = LinuxPayloadService()
    iso_path = os.path.join(tmp_dir, "debian-live-13.5.0-amd64-xfce.iso")
    _make_file(iso_path, b"iso")
    service.verify_iso = lambda path, expected_sha256=DEBIAN_LIVE_XFCE_SHA256: {
        "success": True,
        "message": "ok",
        "sha256": DEBIAN_LIVE_XFCE_SHA256,
        "path": path,
    }

    bundle = service.build_vm_bundle_from_iso(iso_path, os.path.join(tmp_dir, "stage"), allocation_gb=8)
    destinations = {asset["destination_rel"] for asset in bundle["assets"]}

    assert "Linux/RockPodVM/debian-live-13.5.0-amd64-xfce.iso" in destinations
    assert "Linux/RockPodVM/start-linux-linux.sh" in destinations
    assert "Linux/RockPodVM/start-linux-linux.command" in destinations
    assert "Linux/RockPodVM/Start RockPod Linux.command" in destinations
    assert "Linux/RockPodVM/Start RockPod Linux.desktop" in destinations
    assert "Linux/RockPodVM/autoinstall-linux-linux.command" in destinations
    assert "Linux/RockPodVM/provision-rockpod-linux-linux.command" in destinations
    assert "Linux/RockPodVM/rockpod-guest-provision.sh" in destinations
    assert "Linux/RockPodVM/start-linux-windows.bat" in destinations
    assert "Linux/RockPodVM/start-linux-macos.command" in destinations
    assert "Linux/RockPodVM/preseed.cfg" in destinations
    assert "Linux/RockPodVM/rockpod-linux.svg" in destinations
    assert "Linux/RockPodVM/manifest.json" in destinations
    assert not any(path.startswith(("EFI/", "boot/", "live/", "isolinux/")) for path in destinations)
    assert bundle["manifest"]["mode"] == "portable-vm"
    assert bundle["manifest"]["disk_format"] == "split-vmdk"
    assert bundle["manifest"]["vm_username"] == "rockpod"
    assert bundle["manifest"]["vm_root_login"] is False
    assert "rockpod-linux-data.vmdk" in bundle["manifest"]["files"]
    assert "Start RockPod Linux.command" in bundle["manifest"]["files"]
    assert "Start RockPod Linux.desktop" in bundle["manifest"]["files"]
    assert "autoinstall-linux-linux.command" in bundle["manifest"]["files"]
    assert "provision-rockpod-linux-linux.command" in bundle["manifest"]["files"]
    assert "rockpod-guest-provision.sh" in bundle["manifest"]["files"]
    assert "preseed.cfg" in bundle["manifest"]["files"]
    assert "rockpod-linux-data.qcow2" not in bundle["manifest"]["files"]
    assert "rockpod-linux.svg" in bundle["manifest"]["files"]
    for asset in bundle["assets"]:
        if asset["destination_rel"].endswith("start-linux-linux.command"):
            with open(asset["source_abs"], "r", encoding="utf-8") as handle:
                launcher = handle.read()
            assert "subformat=twoGbMaxExtentSparse" in launcher
            assert "format=vmdk" in launcher
            assert "format=qcow2" not in launcher
            assert "ROCKPOD_QEMU_AUDIO_BACKEND" in launcher
            assert 'grep -qF -- "name \\"$1\\"" <<< "$QEMU_DEVICE_HELP"' in launcher
            assert "-audiodev \"$backend,id=rockpod-audio\"" in launcher
            assert "-device hda-output,audiodev=rockpod-audio" in launcher
            assert "\"${QEMU_AUDIO_ARGS[@]}\"" in launcher
            break
    else:
        raise AssertionError("Linux command launcher missing")
    for asset in bundle["assets"]:
        if asset["destination_rel"].endswith("Start RockPod Linux.command"):
            with open(asset["source_abs"], "r", encoding="utf-8") as handle:
                terminal = handle.read()
            assert "x-terminal-emulator" in terminal
            assert "start-linux-linux.command" in terminal
            break
    else:
        raise AssertionError("Linux terminal launcher missing")
    for asset in bundle["assets"]:
        if asset["destination_rel"].endswith("Start RockPod Linux.desktop"):
            with open(asset["source_abs"], "r", encoding="utf-8") as handle:
                desktop = handle.read()
            assert "Name=Start RockPod Linux" in desktop
            assert "Terminal=true" in desktop
            assert "start-linux-linux.command" in desktop
            break
    else:
        raise AssertionError("Linux desktop launcher missing")
    for asset in bundle["assets"]:
        if asset["destination_rel"].endswith("autoinstall-linux-linux.command"):
            with open(asset["source_abs"], "r", encoding="utf-8") as handle:
                autoinstall = handle.read()
            assert "url=http://10.0.2.2:" in autoinstall
            assert "-drive \"file=$DISK,format=vmdk,if=virtio\"" in autoinstall
            assert "touch \"$INSTALLED_FLAG\"" in autoinstall
            break
    else:
        raise AssertionError("Linux autoinstall launcher missing")
    for asset in bundle["assets"]:
        if asset["destination_rel"].endswith("preseed.cfg"):
            with open(asset["source_abs"], "r", encoding="utf-8") as handle:
                preseed = handle.read()
            assert "d-i partman-auto/disk string /dev/vda" in preseed
            assert "d-i grub-installer/bootdev string /dev/vda" in preseed
            assert "rockpod-guest-provision.sh" in preseed
            assert "mousepad ristretto file-roller" in preseed
            assert "d-i passwd/username string rockpod" in preseed
            assert "d-i passwd/user-password password rockpod" in preseed
            break
    else:
        raise AssertionError("preseed missing")
    for asset in bundle["assets"]:
        if asset["destination_rel"].endswith("provision-rockpod-linux-linux.command"):
            with open(asset["source_abs"], "r", encoding="utf-8") as handle:
                provisioner = handle.read()
            assert "qemu-nbd --connect" in provisioner
            assert "ROCKPOD_TARGET_ROOT" in provisioner
            break
    else:
        raise AssertionError("host provisioner missing")
    for asset in bundle["assets"]:
        if asset["destination_rel"].endswith("rockpod-guest-provision.sh"):
            with open(asset["source_abs"], "r", encoding="utf-8") as handle:
                guest = handle.read()
            assert "rockpod-firstboot-provision.service" in guest
            assert "rockpod-home.desktop" in guest
            assert "wallpaper.svg" in guest
            assert "wintc-taskband" in guest
            assert "Windows XP style (Blue)" in guest
            assert "wine wine32 wine64 winetricks" in guest
            assert "rockpod-run-nibiru" in guest
            break
    else:
        raise AssertionError("guest provisioner missing")


def test_linux_vm_bundle_accepts_custom_credentials(tmp_dir):
    service = LinuxPayloadService()
    iso_path = os.path.join(tmp_dir, "debian-live-13.5.0-amd64-xfce.iso")
    _make_file(iso_path, b"iso")
    service.verify_iso = lambda path, expected_sha256=DEBIAN_LIVE_XFCE_SHA256: {
        "success": True,
        "message": "ok",
        "sha256": DEBIAN_LIVE_XFCE_SHA256,
        "path": path,
    }

    bundle = service.build_vm_bundle_from_iso(
        iso_path,
        os.path.join(tmp_dir, "stage"),
        allocation_gb=8,
        vm_user={
            "username": "david",
            "password": "musicbox",
            "su_password": "rootbox",
        },
    )

    assert bundle["manifest"]["vm_username"] == "david"
    assert bundle["manifest"]["vm_root_login"] is True
    for asset in bundle["assets"]:
        if asset["destination_rel"].endswith("preseed.cfg"):
            with open(asset["source_abs"], "r", encoding="utf-8") as handle:
                preseed = handle.read()
            assert "d-i passwd/root-login boolean true" in preseed
            assert "d-i passwd/root-password password rootbox" in preseed
            assert "d-i passwd/username string david" in preseed
            assert "d-i passwd/user-password password musicbox" in preseed
            assert "in-target usermod -aG sudo david" in preseed
            break
    else:
        raise AssertionError("preseed missing")


def test_linux_vm_install_status_detects_portable_vm(tmp_dir):
    service = LinuxPayloadService()
    device = os.path.join(tmp_dir, "device")
    _make_file(os.path.join(device, ".rockbox", "config.cfg"), "volume: -20\n")
    _make_file(os.path.join(device, "Linux", "RockPodVM", "manifest.json"), "{}")
    _make_file(os.path.join(device, "Linux", "RockPodVM", "debian-live-13.5.0-amd64-xfce.iso"), b"iso")
    _make_file(os.path.join(device, "Linux", "RockPodVM", "start-linux-linux.sh"), "#!/bin/sh\n")

    status = service.installed_status(device)

    assert status["installed"] is True
    assert status["vm_ready"] is False
    assert status["mode"] == "portable-vm"
    assert status["efi_boot"] == ""


def test_linux_vm_refresh_provision_files_updates_existing_vm(tmp_dir):
    service = LinuxPayloadService()
    device = os.path.join(tmp_dir, "device")
    vm = os.path.join(device, "Linux", "RockPodVM")
    _make_file(os.path.join(device, ".rockbox", "config.cfg"), "volume: -20\n")
    _make_file(os.path.join(vm, "manifest.json"), '{"mode":"portable-vm","files":["manifest.json"]}')
    os.makedirs(vm, exist_ok=True)

    result = service.refresh_vm_provision_files(device)

    assert result["success"] is True
    assert os.path.isfile(os.path.join(vm, "provision-rockpod-linux-linux.command"))
    assert os.path.isfile(os.path.join(vm, "rockpod-guest-provision.sh"))
    with open(os.path.join(vm, "manifest.json"), "r", encoding="utf-8") as handle:
        manifest = handle.read()
    assert "provision-rockpod-linux-linux.command" in manifest
    assert "rockpod-guest-provision.sh" in manifest


def test_linux_vm_refresh_launcher_files_updates_existing_vm(tmp_dir):
    service = LinuxPayloadService()
    device = os.path.join(tmp_dir, "device")
    vm = os.path.join(device, "Linux", "RockPodVM")
    _make_file(os.path.join(device, ".rockbox", "config.cfg"), "volume: -20\n")
    _make_file(os.path.join(vm, "manifest.json"), '{"mode":"portable-vm","data_disk_gb":42,"files":["manifest.json"]}')
    os.makedirs(vm, exist_ok=True)

    result = service.refresh_vm_launcher_files(device)

    assert result["success"] is True
    assert os.path.isfile(os.path.join(vm, "start-linux-linux.command"))
    assert os.path.isfile(os.path.join(vm, "start-linux-linux.sh"))
    assert os.path.isfile(os.path.join(vm, "Start RockPod Linux.command"))
    assert os.path.isfile(os.path.join(vm, "Start RockPod Linux.desktop"))
    with open(os.path.join(vm, "start-linux-linux.command"), "r", encoding="utf-8") as handle:
        command = handle.read()
    assert "DATA_DISK_GB=42" in command
    with open(os.path.join(vm, "Start RockPod Linux.desktop"), "r", encoding="utf-8") as handle:
        desktop = handle.read()
    assert "Terminal=true" in desktop
    assert "start-linux-linux.command" in desktop
    with open(os.path.join(vm, "Start RockPod Linux.command"), "r", encoding="utf-8") as handle:
        terminal = handle.read()
    assert "x-terminal-emulator" in terminal
    assert "start-linux-linux.command" in terminal
    with open(os.path.join(vm, "manifest.json"), "r", encoding="utf-8") as handle:
        manifest = handle.read()
    assert "Start RockPod Linux.desktop" in manifest


def test_linux_vm_uninstall_removes_only_known_vm_files(tmp_dir):
    service = LinuxPayloadService()
    device = os.path.join(tmp_dir, "device")
    vm = os.path.join(device, "Linux", "RockPodVM")
    _make_file(os.path.join(device, ".rockbox", "config.cfg"), "volume: -20\n")
    _make_file(os.path.join(vm, "manifest.json"), '{"mode":"portable-vm","files":["debian-live-13.5.0-amd64-xfce.iso","start-linux-linux.sh","autoinstall-linux-linux.command","preseed.cfg","manifest.json","rockpod-linux.svg","rockpod-linux-data.vmdk","rockpod-linux-installed.flag","rockpod-installer-vmlinuz","rockpod-installer-initrd.gz"]}')
    _make_file(os.path.join(vm, "debian-live-13.5.0-amd64-xfce.iso"), b"iso")
    _make_file(os.path.join(vm, "start-linux-linux.sh"), "#!/bin/sh\n")
    _make_file(os.path.join(vm, "autoinstall-linux-linux.command"), "#!/bin/sh\n")
    _make_file(os.path.join(vm, "preseed.cfg"), b"preseed")
    _make_file(os.path.join(vm, "rockpod-linux.svg"), b"<svg/>")
    _make_file(os.path.join(vm, "rockpod-linux-data.vmdk"), b"descriptor")
    _make_file(os.path.join(vm, "rockpod-linux-data-s001.vmdk"), b"extent")
    _make_file(os.path.join(vm, "rockpod-linux-data-sabc.vmdk"), b"keep")
    _make_file(os.path.join(vm, "rockpod-linux-data.qcow2"), b"legacy")
    _make_file(os.path.join(vm, "rockpod-linux-installed.flag"), b"installed")
    _make_file(os.path.join(vm, "rockpod-installer-vmlinuz"), b"kernel")
    _make_file(os.path.join(vm, "rockpod-installer-initrd.gz"), b"initrd")
    _make_file(os.path.join(vm, "personal-note.txt"), b"keep")
    _make_file(os.path.join(device, ".rockbox", "keep.cfg"), b"rockbox")

    result = service.uninstall_vm(device)

    assert result["success"] is True
    assert not os.path.exists(os.path.join(vm, "debian-live-13.5.0-amd64-xfce.iso"))
    assert not os.path.exists(os.path.join(vm, "rockpod-linux.svg"))
    assert not os.path.exists(os.path.join(vm, "rockpod-linux-data.vmdk"))
    assert not os.path.exists(os.path.join(vm, "rockpod-linux-data-s001.vmdk"))
    assert not os.path.exists(os.path.join(vm, "rockpod-linux-data.qcow2"))
    assert not os.path.exists(os.path.join(vm, "rockpod-linux-installed.flag"))
    assert not os.path.exists(os.path.join(vm, "rockpod-installer-vmlinuz"))
    assert not os.path.exists(os.path.join(vm, "rockpod-installer-initrd.gz"))
    assert not os.path.exists(os.path.join(vm, "autoinstall-linux-linux.command"))
    assert not os.path.exists(os.path.join(vm, "preseed.cfg"))
    assert os.path.isfile(os.path.join(vm, "rockpod-linux-data-sabc.vmdk"))
    assert os.path.isfile(os.path.join(vm, "personal-note.txt"))
    assert os.path.isfile(os.path.join(device, ".rockbox", "keep.cfg"))
