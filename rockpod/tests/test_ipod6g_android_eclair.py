"""Safety and reproducibility tests for the Android 2.0 native milestone."""

from __future__ import annotations

import io
import json
from pathlib import Path
import stat
import sys
import tarfile
import zipfile

import pytest


REPO_ROOT = Path(__file__).resolve().parents[2]
TOOLS = REPO_ROOT / "tools" / "ipod6g_android"
sys.path.insert(0, str(TOOLS))

import package_eclair_native_root as package_root  # noqa: E402
import prepare_eclair_native_source as prepare_source  # noqa: E402
import qualify_eclair_native as qualify_native  # noqa: E402
import qualify_n25_ramdiag as qualify_n25  # noqa: E402
import qualify_n25_eclair_native as qualify_n25_eclair  # noqa: E402
import emulate_eclair_system as emulate_system  # noqa: E402


def parse_newc(data: bytes) -> dict[str, tuple[int, bytes]]:
    offset = 0
    entries = {}
    while True:
        assert data[offset:offset + 6] == b"070701"
        fields = [
            int(data[offset + 6 + index:offset + 14 + index], 16)
            for index in range(0, 104, 8)
        ]
        mode = fields[1]
        size = fields[6]
        name_size = fields[11]
        offset += 110
        name = data[offset:offset + name_size - 1].decode("utf-8")
        assert data[offset + name_size - 1] == 0
        offset = (offset + name_size + 3) & ~3
        body = data[offset:offset + size]
        offset = (offset + size + 3) & ~3
        if name == "TRAILER!!!":
            break
        entries[name] = (mode, body)
    return entries


def test_eclair_source_lock_is_official_and_checksum_pinned():
    lock = json.loads((TOOLS / "eclair/source-lock.json").read_text())

    assert lock["aosp_tag"] == "android-2.0_r1"
    assert lock["platform_version"] == "2.0"
    assert lock["platform_sdk"] == 5
    assert len(lock["archives"]) == 34
    for archive in lock["archives"]:
        assert archive["url"].startswith("https://android.googlesource.com/")
        assert "+archive/android-2.0_r1" in archive["url"]
        assert len(archive["sha256"]) == 64
        int(archive["sha256"], 16)
        assert not Path(archive["destination"]).is_absolute()
        assert ".." not in Path(archive["destination"]).parts


def test_eclair_host_jdk_is_checksum_pinned_java6():
    lock = json.loads((TOOLS / "eclair/host-toolchain-lock.json").read_text())

    assert lock["java_version"] == "1.6.0-119"
    assert lock["archive"]["url"].startswith("https://cdn.azul.com/zulu/bin/")
    assert lock["archive"]["metadata_url"].startswith("https://api.azul.com/")
    assert lock["archive"]["size"] == 64378729
    assert len(lock["archive"]["sha256"]) == 64
    int(lock["archive"]["sha256"], 16)


def test_archive_validator_rejects_traversal_and_links(tmp_path):
    archive_path = tmp_path / "unsafe.tar.gz"
    with tarfile.open(archive_path, "w:gz") as archive:
        traversal = tarfile.TarInfo("../escape")
        traversal.size = 1
        archive.addfile(traversal, io.BytesIO(b"x"))
    with tarfile.open(archive_path, "r:gz") as archive:
        with pytest.raises(prepare_source.PreparationError, match="unsafe archive path"):
            list(prepare_source.checked_members(archive, 0))

    link_path = tmp_path / "link.tar.gz"
    with tarfile.open(link_path, "w:gz") as archive:
        link = tarfile.TarInfo("link")
        link.type = tarfile.SYMTYPE
        link.linkname = "target"
        archive.addfile(link)
    with tarfile.open(link_path, "r:gz") as archive:
        with pytest.raises(prepare_source.PreparationError,
                           match="archive link target"):
            list(prepare_source.checked_members(archive, 0))


def test_archive_validator_materializes_safe_internal_links(tmp_path):
    archive_path = tmp_path / "safe-link.tar.gz"
    with tarfile.open(archive_path, "w:gz") as archive:
        target = tarfile.TarInfo("source/value.c")
        target.size = 4
        archive.addfile(target, io.BytesIO(b"safe"))
        link = tarfile.TarInfo("apps/value.c")
        link.type = tarfile.SYMTYPE
        link.linkname = "../source/value.c"
        archive.addfile(link)

    destination = tmp_path / "output"
    prepare_source.extract_archive(archive_path, destination, 0)

    materialized = destination / "apps/value.c"
    assert materialized.read_bytes() == b"safe"
    assert not materialized.is_symlink()


def test_eclair_port_patch_has_runtime_and_failure_guards():
    patch = (TOOLS / "eclair/patches/aosp-native-only-modern-host.patch").read_text()

    assert "0xffff0fe0" in patch
    assert "if (!bs)" in patch
    assert "ECLAIR_NATIVE_ONLY" in patch
    assert "print(r)" in patch
    assert "IPOD6G_ECLAIR_BINDER:CONTEXT_MANAGER_READY" in patch
    assert "IPOD6G_ECLAIR_HEADLESS_GRALLOC:READY 320x240 RGB565" in patch
    assert "IPOD6G_ECLAIR_SURFACEFLINGER_SERVICE:PUBLISHED" in patch
    assert "IPOD6G_ECLAIR_SYSTEM_SERVER:READY" in patch
    assert "IPOD6G_ECLAIR_BATTERY:STUB_READY" in patch
    assert "IPOD6G_ECLAIR_ACCESSIBILITY:READY" in patch
    assert "usage & ~GRALLOC_USAGE_HW_FB" in patch

    egl_config = (TOOLS / "eclair/probe/egl.cfg").read_text()
    assert egl_config == "0 0 android\n"

    binder_patch = (
        TOOLS / "patches/linux-android-binder-ipc32.patch"
    ).read_text()
    assert "ccflags-$(CONFIG_ARM) += -DBINDER_IPC_32BIT" in binder_patch
    assert "drivers/android/Makefile" in binder_patch
    assert "drivers/android/binder.c" not in binder_patch

    ashmem_fallback = (TOOLS / "eclair/overlay/ashmem-dev.c").read_text()
    assert "O_EXCL" in ashmem_fallback
    assert "unlink" in ashmem_fallback
    assert "mkstemp(" not in ashmem_fallback
    assert '"/dev/ashmem-fallback"' in ashmem_fallback


def test_n25_fit_staging_is_below_payload_destinations(tmp_path):
    assert qualify_n25.FIT_STAGING_LOAD == 0x08800000
    uboot_patch = (
        TOOLS / "patches/uboot-s5l8702-n25-ramdiag.patch"
    ).read_text()
    assert "+CONFIG_SYS_LOAD_ADDR=0x08800000" in uboot_patch
    assert "diagnostic ram 0x08800000 0x01800000" in uboot_patch

    sizes = {
        "fit": 15 * 1024 * 1024,
        "zimage": 2 * 1024 * 1024,
        "image": 6 * 1024 * 1024,
        "initramfs": 13 * 1024 * 1024,
        "dtb": 16 * 1024,
    }
    paths = {}
    for name, size in sizes.items():
        path = tmp_path / name
        path.write_bytes(b"")
        with path.open("r+b") as handle:
            handle.truncate(size)
        paths[name] = path

    qualify_n25.check_memory_layout(
        paths["fit"], paths["zimage"], paths["image"],
        paths["initramfs"], paths["dtb"],
        initramfs_load=0x09C00000, dtb_load=0x0AD00000,
    )
    with paths["fit"].open("r+b") as handle:
        handle.truncate(21 * 1024 * 1024)
    with pytest.raises(qualify_n25.QualificationError, match="RAM overlap"):
        qualify_n25.check_memory_layout(
            paths["fit"], paths["zimage"], paths["image"],
            paths["initramfs"], paths["dtb"],
            initramfs_load=0x09C00000, dtb_load=0x0AD00000,
        )


def test_vfp_parser_ignores_instruction_bytes_but_rejects_real_vfp():
    false_positive = "    8dca:\tf002 fadd \tbl\tb388 <target>"
    assert qualify_native.classify_vfp_instructions(false_positive) == (None, [])

    forbidden = "00008000 <main>:\n    8000:\tec900b21 \tfldmiax\tr0, {d0-d15}"
    line, dormant = qualify_native.classify_vfp_instructions(forbidden)
    assert "fldmiax" in line
    assert dormant == []

    unwind = (
        "00008000 <__gnu_Unwind_Restore_VFP>:\n"
        "    8000:\tec900b21 \tfldmiax\tr0, {d0-d15}"
    )
    assert qualify_native.classify_vfp_instructions(unwind) == (
        None,
        ["__gnu_Unwind_Restore_VFP"],
    )


def test_storage_free_init_rc_uses_only_ram_for_mutable_state():
    init_rc = (TOOLS / "eclair/root/init.rc").read_text().lower()

    assert init_rc.count("mount tmpfs") == 3
    assert "mount tmpfs tmpfs /data" in init_rc
    assert "mount tmpfs tmpfs /cache" in init_rc
    assert "mount tmpfs tmpfs /metadata" in init_rc
    assert "mount rootfs rootfs / ro remount" in init_rc
    for forbidden in package_root.FORBIDDEN_INIT_RC:
        assert forbidden not in init_rc


def test_n25_eclair_kernel_profile_has_binder_but_no_storage_or_network_devices():
    config = (
        TOOLS
        / "linux-overlay/arch/arm/configs/apple_n25_eclair_native_defconfig"
    ).read_text()

    required = {
        "CONFIG_CPU_ARM926T=y",
        "CONFIG_ARM_THUMB=y",
        "CONFIG_KUSER_HELPERS=y",
        "CONFIG_MULTIUSER=y",
        "CONFIG_FILE_LOCKING=y",
        "CONFIG_COMPAT_32BIT_TIME=y",
        "CONFIG_NET=y",
        "CONFIG_UNIX=y",
        "CONFIG_ANDROID_BINDER_IPC=y",
        'CONFIG_ANDROID_BINDER_DEVICES="binder"',
        "CONFIG_TMPFS=y",
        "CONFIG_FB=y",
        "CONFIG_FB_S5L8702=y",
        "CONFIG_I2C=y",
        "CONFIG_I2C_S5L8702=y",
        "CONFIG_INPUT=y",
        "CONFIG_INPUT_EVDEV=y",
        "CONFIG_INPUT_MISC=y",
        "CONFIG_INPUT_S5L8702_N25_CLICKWHEEL=y",
        "CONFIG_POWER_RESET=y",
        "CONFIG_POWER_RESET_S5L8702=y",
    }
    forbidden = {
        "# CONFIG_BLOCK is not set",
        "# CONFIG_ATA is not set",
        "# CONFIG_SCSI is not set",
        "# CONFIG_MMC is not set",
        "# CONFIG_MTD is not set",
        "# CONFIG_SPI is not set",
        "# CONFIG_INET is not set",
        "# CONFIG_NETDEVICES is not set",
    }
    assert required <= set(config.splitlines())
    assert forbidden <= set(config.splitlines())


def test_full_system_emulator_requires_runtime_abi_and_forbids_devices():
    required = emulate_system.REQUIRED_CONFIG
    forbidden = emulate_system.FORBIDDEN_CONFIG

    assert required["CONFIG_CPU_ARM926T"] == "y"
    assert required["CONFIG_ARM_THUMB"] == "y"
    assert required["CONFIG_KUSER_HELPERS"] == "y"
    assert required["CONFIG_MULTIUSER"] == "y"
    assert required["CONFIG_FILE_LOCKING"] == "y"
    assert required["CONFIG_COMPAT_32BIT_TIME"] == "y"
    assert required["CONFIG_ANDROID_BINDER_IPC"] == "y"
    assert {
        "CONFIG_BLOCK",
        "CONFIG_INET",
        "CONFIG_NETDEVICES",
        "CONFIG_INPUT",
        "CONFIG_FB",
        "CONFIG_SOUND",
    } <= forbidden

    qemu_config = (
        TOOLS / "linux-overlay/arch/arm/configs/eclair_qemu_arm926_defconfig"
    ).read_text().splitlines()
    assert "CONFIG_ARM_THUMB=y" in qemu_config
    assert "CONFIG_KUSER_HELPERS=y" in qemu_config
    assert "CONFIG_MULTIUSER=y" in qemu_config
    assert "CONFIG_FILE_LOCKING=y" in qemu_config
    assert "CONFIG_COMPAT_32BIT_TIME=y" in qemu_config
    assert "# CONFIG_BLOCK is not set" in qemu_config

    source = (TOOLS / "emulate_eclair_system.py").read_text()
    assert '"-nic", "none"' in source
    for drive_option in ('"-drive"', '"-hda"', '"-sd"'):
        assert drive_option not in source
    for marker in (
        "dalvikvm: System server process",
        "SurfaceFlinger: SurfaceFlinger is starting",
        "IPOD6G_ECLAIR_HEADLESS_GRALLOC:READY 320x240 RGB565",
        "libGLES_android.so",
        "Android PixelFlinger 1.1",
        "SystemServer: Entered the Android system server!",
        "IPOD6G_ECLAIR_SURFACEFLINGER_SERVICE:PUBLISHED",
        "IPOD6G_ECLAIR_SYSTEM_SERVER:READY",
        "RockpodLauncher.apk' (success)",
        "IPOD6G_ECLAIR_LAUNCHER:ON_CREATE",
    ):
        assert marker in source

    aosp_patch = (
        TOOLS / "eclair/patches/aosp-native-only-modern-host.patch"
    ).read_text()
    assert "IPOD6G_ECLAIR_GRALLOC:NO_FB_DEVICE_USING_HEADLESS" in aosp_patch
    assert "IPOD6G_ECLAIR_GRALLOC:USING_LINUX_FRAMEBUFFER" in aosp_patch

    assert "system/lib/egl/libGLES_android.so" in package_root.RUNTIME_FILES
    assert "system/bin/installd" in package_root.RUNTIME_FILES
    assert "system/app/RockpodLauncher.apk" in package_root.DATA_FILES
    assert "system/app/SettingsProvider.apk" in package_root.DATA_FILES
    assert package_root.DATA_FILES["system/lib/egl/egl.cfg"] == "ipod6g_probe/egl.cfg"
    assert (
        "system/usr/keylayout/iPod_Classic_Click_Wheel.kl"
        in package_root.LOCAL_FILES
    )


def test_n25_eclair_device_tree_is_ram_only_and_usb_peripheral():
    dts = (
        TOOLS
        / "linux-overlay/arch/arm/boot/dts/samsung/s5l8702-n25-eclair-native.dts"
    ).read_text().lower()

    assert 'rockpod,safety-profile = "eclair-native-ram-only-no-storage"' in dts
    assert 'dr_mode = "peripheral"' in dts
    assert "androidboot.hardware=n25" in dts
    assert "reg = <0x08000000 0x04000000>" in dts
    assert 'compatible = "apple,n25-lcd"' in dts
    assert 'reg-names = "lcd", "panel-strap"' in dts
    assert "<0x3cf000c4 0x4>" in dts
    assert 'compatible = "samsung,s5l8702-i2c"' in dts
    assert dts.count("i2c@3c600000") == 1
    assert "pmu-i2c = <&i2c0>" in dts
    assert 'compatible = "apple,n25-clickwheel"' in dts
    assert 'reg-names = "wheel", "pcon14", "pwrcon1"' in dts
    assert "interrupts = <23>" in dts
    assert 'compatible = "apple,s5l8702-watchdog-reset"' in dts
    assert "apple,test-timeout-seconds = <180>" in dts
    assert "panic=5" in dts
    assert "linux,initrd-start = <0x09c00000>" in dts
    assert "linux,initrd-end = <0x0a933780>" in dts
    for node in ("ata@", "ceata@", "mmc@", "nand@", "spi@", "eeprom@"):
        assert node not in dts


def test_classic_menu_play_direct_boot_is_fixed_and_volatile():
    bootloader = (REPO_ROOT / "bootloader/ipod-s5l87xx.c").read_text()
    builder = (TOOLS / "build_n25_eclair_native_bundle.sh").read_text()

    for marker in (
        "btn == (BUTTON_MENU|BUTTON_PLAY)",
        'BOOTDIR "/android/n25-eclair-kernel.ipod"',
        'BOOTDIR "/android/n25-eclair-initramfs.ipod"',
        'BOOTDIR "/android/n25-eclair-dtb.ipod"',
        "N25_ANDROID_KERNEL_ADDR 0x08008000u",
        "N25_ANDROID_INITRD_ADDR 0x09c00000u",
        "N25_ANDROID_DTB_ADDR    0x0ad00000u",
        "disk_unmount_all();",
        "storage_sleepnow();",
        "n25_android_linux_jump",
    ):
        assert marker in bootloader
    assert 'makedfu --kind n3g' in builder
    assert 'n25-eclair-menu-play-bootloader.dfu' in builder
    assert '--rockbox-bootloader-dfu' in builder


def test_n25_visible_probe_gates_exact_handoff_and_all_lcd_panels():
    bootloader = (REPO_ROOT / "bootloader/ipod-s5l87xx.c").read_text()
    lcd = (
        REPO_ROOT / "firmware/target/arm/s5l8702/lcd-s5l8702.c"
    ).read_text()
    builder = (TOOLS / "build_n25_visible_probe.sh").read_text()
    emulator = (TOOLS / "emulate_n25_arm_head.py").read_text()
    qualifier = (TOOLS / "qualify_n25_visible_probe.py").read_text()
    visible_patch = (
        TOOLS / "patches/linux-s5l8702-n25-visible-handoff.patch"
    ).read_text()
    stager = (TOOLS / "stage_n25_visible_probe.sh").read_text()

    for marker in (
        'BOOTDIR "/android/diagnostic-trace2/n25-visible-kernel.ipod"',
        "lcd_wait_for_dma();",
        "dmac_open(&s5l8702_dmac0);",
        "n25_android_linux_jump(N25_ANDROID_KERNEL_ADDR, N25_ANDROID_DTB_ADDR)",
    ):
        assert marker in bootloader
    assert "void lcd_wait_for_dma(void)" in lcd
    assert 'target_rel=".rockbox/android/diagnostic-trace2"' in stager
    assert "diagnostic-trace1" not in stager
    assert "diagnostic-lcd1" not in stager
    assert "diagnostic-vic1" not in stager

    for marker in (
        "for strap in 0 1 2 3",
        "--loader-bin",
        "--loader-elf",
        "--verify-n25-lcd",
        "--stop-at s5l_lcd_n25_first_frame_complete",
    ):
        assert marker in builder
    for marker in (
        "N25_LCD_PIXELS = 320 * 240",
        "N25_LCD_BAND_PIXELS = 320 * 48",
        "N25_LCD_AMBER = 0xFD20",
        "rockbox_handoff_executed",
        "unexpected N25 LCD write size",
        "N25 cumulative trace did not fill exactly one panel frame",
    ):
        assert marker in emulator
    for marker in (
        "exactly four N25 LCD strap reports are required",
        "Rockbox handoff was bypassed",
        "LCD DMA is not drained before handoff quiesce",
        "ARM Linux entry invariant missing",
    ):
        assert marker in qualifier
    assert "s5l_lcd_n25_first_frame_complete" in visible_patch


def test_n25_lcd_patch_uses_all_classic_panels_and_allowlisted_pcf50635():
    patch = (
        TOOLS / "patches/linux-s5l8702-n25-lcd-handoff.patch"
    ).read_text()
    builder = (TOOLS / "build_n25_eclair_native_bundle.sh").read_text()

    for marker in (
        '"apple,n25-lcd"',
        "#define LCD_MODE_P16\t0x80100db0",
        "#define LCD_MODE_P18\t0x80000da8",
        "panel-strap",
        "n25_cmdset16",
        "applying Rockbox Classic init",
        "n25_init_seq_0",
        "n25_init_seq_1",
        "s5l_lcd_init_n25_type23",
        "PCF50635_REG_LEDOUT",
        "PCF50635_REG_LDO3ENA",
        "R_WRITE_DATA_TO_GRAM",
    ):
        assert marker in patch
    assert "((strap >> 4) & 0x3) >= 2" in patch
    assert "PCF50635_LEDOUT_DEFAULT\t0x20" in patch
    assert "PCF50635_LDO3_3000MV\t0x15" in patch
    assert "s5l_lcd_n25_display_power_on(dev, pmu)" in patch
    assert "s5l_lcd_n25_backlight_on(dev, pmu)" in patch
    assert "linux_lcd_patch" in builder


def test_n25_clickwheel_and_reset_patch_matches_rockbox_contract():
    patch = (
        TOOLS / "patches/linux-s5l8702-n25-input-reset.patch"
    ).read_text()
    builder = (TOOLS / "build_n25_eclair_native_bundle.sh").read_text()
    keylayout = (
        TOOLS / "eclair/root/iPod_Classic_Click_Wheel.kl"
    ).read_text()

    for marker in (
        "N25_WHEEL_POSITIONS\t96",
        "N25_WHEEL_SENSITIVITY\t4",
        "N25_WHEEL_CLOCK_BIT\tBIT(1)",
        "N25_WHEEL_PCON_ACTIVE\t0x00222200",
        "writel(0x380000, wheel->wheel + N25_WHEEL_CTRL0)",
        "writel(0x20000, wheel->wheel + N25_WHEEL_TIMING)",
        "writel(0x8000023a, wheel->wheel + N25_WHEEL_TX)",
        "PCF50635_REG_GPIOSTAT\t0x87",
        "PCF50635_GPIOSTAT_GPIO2\tBIT(1)",
        "N25_RESET_CHORD_MS\t8000",
        "S5L8702_WATCHDOG_RESET\t0x00100000",
        'compatible = "apple,n25-clickwheel"',
        'compatible = "apple,s5l8702-watchdog-reset"',
    ):
        assert marker in patch
    for mapping in (
        "key 28    DPAD_CENTER",
        "key 103   DPAD_UP",
        "key 108   DPAD_DOWN",
        "key 105   DPAD_LEFT",
        "key 106   DPAD_RIGHT",
        "key 158   BACK",
        "key 164   MEDIA_PLAY_PAUSE",
    ):
        assert mapping in keylayout
    assert "linux_input_reset_patch" in builder


def test_n25_framebuffer_link_gate_requires_linked_driver_markers(tmp_path):
    vmlinux = tmp_path / "vmlinux"
    vmlinux.write_bytes(
        b"apple,n25-lcd\0N25 panel strap type\0"
        b"applying Rockbox Classic init\0display PMU write\0"
        b"apple,n25-clickwheel\0Rockbox-derived N25 click wheel ready\0"
        b"PCF50635 Hold read failed\0Menu+Select held for 8 seconds\0"
        b"apple,s5l8702-watchdog-reset\0"
        b"Rockbox-derived S5L8702 restart handler ready\0"
        b"volatile test window expired after\0"
    )
    qualify_n25_eclair.check_n25_framebuffer_linked(vmlinux)

    vmlinux.write_bytes(b"apple,n25-lcd\0")
    with pytest.raises(qualify_n25.QualificationError, match="not linked"):
        qualify_n25_eclair.check_n25_framebuffer_linked(vmlinux)


def test_eclair_bundle_builder_has_no_device_or_mount_commands():
    builder = (TOOLS / "build_n25_eclair_native_bundle.sh").read_text()

    assert "qualify_n25_eclair_native.py" in builder
    assert "hardware actions remain disabled" in builder
    assert "-ffile-prefix-map=${uboot_source}=freemyipod-u-boot" in builder
    for forbidden in ("lsblk", "findmnt", "mount ", "umount", "ipodpatcher", "--single"):
        assert forbidden not in builder.lower()


def test_deterministic_root_archive_has_exact_regular_files(tmp_path):
    root = tmp_path / "root"
    for relative, mode in package_root.DIRECTORIES.items():
        directory = root / relative
        directory.mkdir(parents=True, exist_ok=True)
        directory.chmod(mode)
    (root / "init.rc").write_bytes(b"test-init-rc\n")
    for relative in package_root.RUNTIME_FILES:
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(relative.encode("ascii"))
    for relative in package_root.DATA_FILES:
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(relative.encode("ascii"))

    first = package_root.build_newc(root)
    second = package_root.build_newc(root)
    assert first == second
    entries = parse_newc(first)
    assert set(entries) == {
        *package_root.DIRECTORIES,
        "init.rc",
        *package_root.RUNTIME_FILES,
        *package_root.DATA_FILES,
    }
    for relative in package_root.RUNTIME_FILES:
        mode, body = entries[relative]
        assert stat.S_ISREG(mode)
        assert body == relative.encode("ascii")
    for relative in package_root.DIRECTORIES:
        assert stat.S_ISDIR(entries[relative][0])


def test_deterministic_zip_removes_time_and_entry_order_variance(tmp_path):
    first_source = tmp_path / "first.jar"
    second_source = tmp_path / "second.jar"
    first_output = tmp_path / "first-normalized.jar"
    second_output = tmp_path / "second-normalized.jar"

    with zipfile.ZipFile(first_source, "w") as archive:
        archive.writestr(
            zipfile.ZipInfo("classes.dex", (2026, 7, 18, 17, 20, 0)), b"dex\n"
        )
        archive.writestr(
            zipfile.ZipInfo("META-INF/MANIFEST.MF", (2026, 7, 18, 17, 21, 0)),
            b"Manifest-Version: 1.0\n",
        )
    with zipfile.ZipFile(second_source, "w") as archive:
        archive.writestr(
            zipfile.ZipInfo("META-INF/MANIFEST.MF", (2009, 10, 26, 8, 0, 0)),
            b"Manifest-Version: 1.0\n",
        )
        archive.writestr(
            zipfile.ZipInfo("classes.dex", (2009, 10, 26, 8, 1, 0)), b"dex\n"
        )

    package_root.copy_deterministic_zip(first_source, first_output)
    package_root.copy_deterministic_zip(second_source, second_output)

    assert first_output.read_bytes() == second_output.read_bytes()
    with zipfile.ZipFile(first_output) as archive:
        assert archive.namelist() == ["META-INF/MANIFEST.MF", "classes.dex"]
        assert all(info.date_time == (1980, 1, 1, 0, 0, 0) for info in archive.infolist())
