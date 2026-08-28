"""Fail-closed tests for the iPod touch 4G volatile Android foundation."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import struct
import sys
from types import SimpleNamespace

import pytest


REPO_ROOT = Path(__file__).resolve().parents[2]
TOOLS = REPO_ROOT / "tools" / "ipodtouch4_android"
sys.path.insert(0, str(TOOLS))


def load_module(name: str, filename: str):
    spec = importlib.util.spec_from_file_location(name, TOOLS / filename)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


probe = load_module("ipodtouch4_device_probe", "device_probe.py")
prepare = load_module("ipodtouch4_prepare", "prepare_openiboot.py")
prepare_kernel = load_module("ipodtouch4_prepare_kernel", "prepare_idroid_kernel.py")
prepare_shatter = load_module(
    "ipodtouch4_prepare_shatter", "prepare_shatter_transport.py"
)
qualify = load_module("ipodtouch4_qualify", "qualify_volatile_openiboot.py")
qualify_kernel = load_module(
    "ipodtouch4_qualify_kernel", "qualify_volatile_kernel.py"
)
qualify_ramdiag = load_module("ipodtouch4_qualify_ramdiag", "qualify_ramdiag.py")
stage_eclair = load_module("ipodtouch4_stage_eclair", "stage_eclair_root_for_n81.py")
flatten = load_module("ipodtouch4_flatten", "flatten_openiboot.py")
plan_boot = load_module("ipodtouch4_plan", "plan_ram_boot.py")
wrap_loader = load_module("ipodtouch4_wrap_loader", "wrap_volatile_openiboot.py")
dfu_transport = load_module("ipodtouch4_dfu", "n81_dfu_transport.py")
ram_transport = load_module("ipodtouch4_ram_transport", "n81_ram_boot_transport.py")
openiboot_probe = load_module(
    "ipodtouch4_openiboot_probe", "n81_openiboot_probe.py"
)


def test_source_lock_pins_exact_n81_openiboot_and_eclair_inputs():
    lock = json.loads((TOOLS / "source-lock.json").read_text(encoding="utf-8"))

    assert lock["target"] == {
        "product_type": "iPod4,1",
        "hardware_model": "N81AP",
        "machine_id": 3564,
        "soc": "Apple A4",
        "cpu": "ARM Cortex-A8",
        "ram_bytes": 256 * 1024 * 1024,
    }
    assert lock["openiboot"]["commit"] == (
        "866562fdb1cfd019bcd77885c80fbf0af65d5c15"
    )
    assert len(lock["openiboot"]["archive_sha256"]) == 64
    int(lock["openiboot"]["archive_sha256"], 16)
    assert lock["openiboot"]["archive_url"].endswith(
        lock["openiboot"]["commit"] + ".tar.gz"
    )
    assert lock["idroid_kernel"]["commit"] == (
        "3f971a676096c37472aec5e139b2720245e509e1"
    )
    assert lock["idroid_kernel"]["version"] == "3.0.8"
    assert len(lock["idroid_kernel"]["archive_sha256"]) == 64
    int(lock["idroid_kernel"]["archive_sha256"], 16)
    assert lock["idroid_kernel"]["archive_url"].endswith(
        lock["idroid_kernel"]["commit"] + ".tar.gz"
    )
    assert lock["dfu_transport"]["commit"] == (
        "0e28932ec6a2a570b10fd77e50bda4216418cd98"
    )
    assert lock["dfu_transport"]["archive_sha256"] == (
        "8f5ca9a9213d9549424e8af20fc1c36bb6f1221d45164b284a368a7bddc906a9"
    )
    assert lock["dfu_transport"]["maximum_image3_bytes"] == 0x2C000
    assert lock["host_tools"]["pyusb_version"] == "1.3.1"
    assert lock["host_tools"]["pyusb_wheel_sha256"] == (
        "bf9b754557af4717fe80c2b07cc2b923a9151f5c08d17bdb5345dac09d6a0430"
    )
    requirements = (TOOLS / "requirements-transport.txt").read_text(
        encoding="utf-8"
    )
    assert "pyusb==1.3.1" in requirements
    assert lock["host_tools"]["pyusb_wheel_sha256"] in requirements
    assert lock["android"]["aosp_tag"] == "android-2.0_r1"
    shared = (TOOLS / lock["android"]["shared_source_lock"]).resolve()
    assert shared.is_relative_to(REPO_ROOT.resolve())
    assert json.loads(shared.read_text(encoding="utf-8"))["aosp_tag"] == (
        "android-2.0_r1"
    )


def test_probe_accepts_only_exact_touch4_and_redacts_unique_ids():
    properties = probe.parse_ideviceinfo(
        "ProductType: iPod4,1\n"
        "HardwareModel: N81AP\n"
        "ProductVersion: 6.1.6\n"
        "UniqueDeviceID: secret\n"
        "SerialNumber: secret\n"
    )
    report = probe.qualify_device(properties)

    assert report["qualified"] is True
    assert report["persistent_device_writes_enabled"] is False
    assert report["device_boot_enabled"] is False
    assert "secret" not in json.dumps(report)
    for product, hardware in (("iPod9,1", "N112AP"), ("iPod4,1", "wrong")):
        with pytest.raises(probe.DeviceProbeError, match="unsupported device"):
            probe.qualify_device(
                {
                    "ProductType": product,
                    "HardwareModel": hardware,
                    "ProductVersion": "1",
                }
            )


def test_probe_continuity_binds_chip_without_retaining_identifier():
    properties = {
        "ProductType": "iPod4,1",
        "HardwareModel": "N81AP",
        "ProductVersion": "6.1.6",
        "UniqueChipID": str(0x1234ABCD),
    }
    bundle = probe.make_continuity_bundle(properties, bytes(range(32)))

    serialized = json.dumps(bundle)
    assert "1234abcd" not in serialized.lower()
    assert str(0x1234ABCD) not in serialized
    dfu_serial = "CPID:8930 ECID:1234ABCD SRTG:[iBoot-574.4]"
    result = dfu_transport.qualify_dfu_serial(
        dfu_serial, bundle, require_pwned=False
    )
    assert result["cpid"] == "8930"
    assert result["ecid_present"] is True
    assert "1234abcd" not in json.dumps(result).lower()

    with pytest.raises(dfu_transport.TransportError, match="continuity-bound"):
        dfu_transport.qualify_dfu_serial(
            dfu_serial.replace("1234ABCD", "1234ABCE"),
            bundle,
            require_pwned=False,
        )


def test_probe_is_read_only_and_build_script_has_no_device_transport():
    probe_text = (TOOLS / "device_probe.py").read_text(encoding="utf-8")
    build_text = (TOOLS / "build_volatile_openiboot.sh").read_text(
        encoding="utf-8"
    )

    assert '["ideviceinfo"]' in probe_text
    combined = (probe_text + build_text).lower()
    for forbidden in (
        "idevicerestore",
        "irecovery",
        "ipwndfu",
        "limera1n",
        "dfu-util",
        "openiboot --install",
    ):
        assert forbidden not in combined


def test_volatile_openiboot_profile_is_usb_first_and_headless():
    patch_text = (
        TOOLS / "patches" / "openiboot-ipodtouch4g-volatile.patch"
    ).read_text(encoding="utf-8")
    volatile_base = patch_text.split("volatile_base_src =", 1)[1].split(
        "Export('volatile_base_src')", 1
    )[0]
    added = "\n".join(
        line[1:] for line in patch_text.splitlines() if line.startswith("+")
    )

    assert "'framebuffer.c'" not in volatile_base
    assert "displaypipe_init()" not in added
    a4_patch = patch_text.split("--- a/plat-a4/a4.c", 1)[1].split(
        "--- a/plat-a4/cdma.c", 1
    )[0]
    volatile_platform = a4_patch.split(
        "+#ifdef CONFIG_VOLATILE_NO_STORAGE", 1
    )[1].split("+#else", 1)[0]
    for required in (
        "tasks_setup();",
        "clock_setup();",
        "interrupt_setup();",
        "event_setup();",
    ):
        assert required in volatile_platform
    for forbidden in (
        "arm_setup();",
        "mmu_setup();",
        "gpio_setup();",
        "timer_setup();",
        "uart_setup();",
    ):
        assert forbidden not in volatile_platform
    assert "'-Ttext=0x84000000'" in added
    assert "#define OpenIBootLoad 0x84000000" in added
    assert "#define GeneralStack 0x8402E000" in added
    assert "#define HeapStart 0x84020000" in added
    assert "#define HeapEnd 0x8402B000" in added
    assert "#define TASK_DEFAULT_STACK_SIZE (8*1024)" in added
    assert "MCR\tp15, 0, R1, c12, c0, 0" in added
    assert 'COMMAND("n81state", "Report fixed volatile handoff state."' in added


def test_prepare_rejects_every_unpinned_archive(tmp_path):
    archive = tmp_path / "not-openiboot.tar.gz"
    archive.write_bytes(b"not the pinned source")

    with pytest.raises(prepare.PreparationError, match="checksum mismatch"):
        prepare.prepare(archive, tmp_path / "destination")
    with pytest.raises(prepare_kernel.PreparationError, match="checksum mismatch"):
        prepare_kernel.prepare(archive, tmp_path / "kernel-destination")
    with pytest.raises(prepare_shatter.PreparationError, match="checksum mismatch"):
        prepare_shatter.prepare(archive, tmp_path / "shatter-destination")


def make_arm_elf(segments, entry=0):
    phoff = flatten.ELF_HEADER.size
    phnum = len(segments)
    payload_offset = phoff + phnum * flatten.PROGRAM_HEADER.size
    payload = bytearray()
    headers = bytearray()
    for vaddr, body, memory_size in segments:
        offset = payload_offset + len(payload)
        payload.extend(body)
        headers.extend(
            flatten.PROGRAM_HEADER.pack(
                flatten.PT_LOAD,
                offset,
                vaddr,
                vaddr,
                len(body),
                memory_size,
                5,
                4,
            )
        )
    ident = b"\x7fELF" + bytes((1, 1, 1)) + b"\0" * 9
    header = flatten.ELF_HEADER.pack(
        ident,
        flatten.ET_EXEC,
        flatten.EM_ARM,
        1,
        entry,
        phoff,
        0,
        0,
        flatten.ELF_HEADER.size,
        flatten.PROGRAM_HEADER.size,
        phnum,
        0,
        0,
        0,
    )
    return bytes(header + headers + payload)


def test_flatten_matches_openiboot_memory_extent_and_zero_fills_bss():
    elf = make_arm_elf(((0, b"TEXT", 8), (12, b"RW", 6)))

    assert flatten.flatten(elf) == b"TEXT" + b"\0" * 8 + b"RW" + b"\0" * 4


def test_flatten_rejects_non_arm_or_oversized_images():
    not_arm = bytearray(make_arm_elf(((0, b"x", 1),)))
    struct.pack_into("<H", not_arm, 18, 3)
    with pytest.raises(flatten.FlattenError, match="ARM"):
        flatten.flatten(bytes(not_arm))

    too_large = make_arm_elf(((0, b"x", flatten.MAX_IMAGE_BYTES + 1),))
    with pytest.raises(flatten.FlattenError, match="1 MiB"):
        flatten.flatten(too_large)


def make_qualified_build(tmp_path):
    build = tmp_path / "build"
    build.mkdir()
    artifacts = {}
    for name, filename, body in (
        ("elf", "openiboot-n81-volatile.elf", b"elf"),
        ("bin", "openiboot-n81-volatile.bin", b"bin"),
    ):
        (build / filename).write_bytes(body)
        artifacts[name] = {
            "size": len(body),
            "sha256": plan_boot.sha256_bytes(body),
        }
    report = {
        "artifact_gate_passed": True,
        "hardware_actions_enabled": False,
        "device_test_ready": False,
        "storage_controller_symbols_present": False,
        "storage_write_commands_present": False,
        "fixed_kernel_initrd_regions": True,
        "load_address": 0x84000000,
        "in_place_shatter_execution": True,
        "in_place_exception_vectors": True,
        "product_type": "iPod4,1",
        "hardware_model": "N81AP",
        "machine_id": 3564,
        "openiboot_commit": "866562fdb1cfd019bcd77885c80fbf0af65d5c15",
        "artifacts": artifacts,
    }
    (build / "qualification.json").write_text(json.dumps(report), encoding="utf-8")
    return build


def write_kernel_and_ramdiag_reports(kernel_path, kernel, initrd_path, initrd):
    kernel_report = {
        "artifact_gate_passed": True,
        "product_type": "iPod4,1",
        "hardware_model": "N81AP",
        "machine_id": 3564,
        "storage_subsystems_configured": False,
        "storage_controller_symbols_present": False,
        "hardware_actions_enabled": False,
        "device_test_ready": False,
        "artifacts": {
            "zimage": {
                "size": len(kernel),
                "sha256": plan_boot.sha256_bytes(kernel),
            }
        },
    }
    (kernel_path.parent / "kernel-qualification.json").write_text(
        json.dumps(kernel_report), encoding="utf-8"
    )
    ramdiag_report = {
        "artifact_gate_passed": True,
        "board": "apple-n81-ipod-touch-4g",
        "profile": "n81-volatile-ramdiag-no-storage",
        "storage_paths_present": False,
        "shell_present": False,
        "automatic_reboot_seconds": 30,
        "hardware_actions_enabled": False,
        "device_test_ready": False,
        "artifacts": {
            "initramfs": {
                "size": len(initrd),
                "sha256": plan_boot.sha256_bytes(initrd),
            }
        },
    }
    (initrd_path.parent / "ramdiag-qualification.json").write_text(
        json.dumps(ramdiag_report), encoding="utf-8"
    )


def test_ram_boot_plan_is_bounded_storage_free_and_non_executing(tmp_path):
    build = make_qualified_build(tmp_path)
    kernel = bytearray(0x40)
    magic_end = plan_boot.ZIMAGE_MAGIC_OFFSET + len(plan_boot.ZIMAGE_MAGIC)
    kernel[plan_boot.ZIMAGE_MAGIC_OFFSET:magic_end] = plan_boot.ZIMAGE_MAGIC
    kernel_path = tmp_path / "zImage"
    initrd_path = tmp_path / "initramfs.cpio.gz"
    kernel_path.write_bytes(kernel)
    initrd_path.write_bytes(b"\x1f\x8btest")
    write_kernel_and_ramdiag_reports(
        kernel_path, bytes(kernel), initrd_path, b"\x1f\x8btest"
    )

    result = plan_boot.make_plan(
        build, kernel_path, initrd_path, plan_boot.DEFAULT_COMMAND_LINE
    )

    assert result["hardware_actions_enabled"] is False
    assert result["transport_implemented"] is True
    assert result["device_test_ready"] is False
    assert result["steps"][0]["sendfile_command"] == "sendfile 0x4d000000 64"
    assert result["steps"][-1] == {
        "command": "boot",
        "requires_separate_physical_gate": True,
    }


def test_image3_wrapper_is_minimal_bounded_and_exact():
    payload = bytes(range(251)) * 11
    image = wrap_loader.make_image3(payload)

    assert len(image) % 64 == 0
    assert len(image) <= wrap_loader.MAX_IMAGE3_BYTES
    assert wrap_loader.payload_from_image3(image) == payload
    magic, total, data, signed, image_type = wrap_loader.ROOT_HEADER.unpack_from(image)
    assert (magic, total, data, signed, image_type) == (
        b"3gmI",
        len(image),
        wrap_loader.TAG_HEADER.size + len(payload),
        wrap_loader.TAG_HEADER.size + len(payload),
        b"ssbi",
    )
    with pytest.raises(wrap_loader.Image3Error, match="padding"):
        wrap_loader.payload_from_image3(image[:-1] + b"x")


def test_shatter_payload_and_transport_expose_no_persistence_commands():
    shellcode = b"safe-shellcode"
    payload = dfu_transport.generate_exploit_payload(shellcode)
    assert payload[:4] == b"3gmI"
    assert payload.endswith(shellcode)
    assert len(shellcode) <= dfu_transport.SHELLCODE_MAX

    combined = "\n".join(
        (TOOLS / name).read_text(encoding="utf-8").lower()
        for name in ("n81_dfu_transport.py", "n81_ram_boot_transport.py")
    )
    for forbidden in (
        "flash_nor",
        "dump-nor",
        "restore",
        "alloc8",
        "24kpwn",
        "mtd_write",
        "saveenv",
        "openiboot --install",
    ):
        assert forbidden not in combined


class FakeOpenIBootChannel:
    def __init__(self):
        self.operations = []

    def upload(self, body):
        self.operations.append(("upload", body))

    def select_kernel(self, command_line):
        self.operations.append(("kernel", command_line))

    def select_initrd(self):
        self.operations.append(("initrd",))

    def boot(self):
        self.operations.append(("boot",))


def test_ram_transport_executes_only_fixed_plan_sequence(tmp_path):
    build = make_qualified_build(tmp_path)
    kernel = bytearray(0x40)
    magic_end = plan_boot.ZIMAGE_MAGIC_OFFSET + len(plan_boot.ZIMAGE_MAGIC)
    kernel[plan_boot.ZIMAGE_MAGIC_OFFSET:magic_end] = plan_boot.ZIMAGE_MAGIC
    kernel_path = tmp_path / "zImage"
    initrd_path = tmp_path / "initramfs.cpio.gz"
    kernel_path.write_bytes(kernel)
    initrd_path.write_bytes(b"\x1f\x8btest")
    write_kernel_and_ramdiag_reports(
        kernel_path, bytes(kernel), initrd_path, b"\x1f\x8btest"
    )
    plan = plan_boot.make_plan(
        build, kernel_path, initrd_path, plan_boot.DEFAULT_COMMAND_LINE
    )
    channel = FakeOpenIBootChannel()

    ram_transport.execute_plan(channel, plan, bytes(kernel), b"\x1f\x8btest")

    assert channel.operations == [
        ("upload", bytes(kernel)),
        ("kernel", plan_boot.DEFAULT_COMMAND_LINE),
        ("upload", b"\x1f\x8btest"),
        ("initrd",),
        ("boot",),
    ]
    assert ram_transport.required_token(plan["profile"]) == (
        "N81_RAMDIAG_VOLATILE_BOOT"
    )


def test_ram_transport_rejects_post_plan_tampering(tmp_path):
    build = make_qualified_build(tmp_path)
    kernel = bytearray(0x40)
    magic_end = plan_boot.ZIMAGE_MAGIC_OFFSET + len(plan_boot.ZIMAGE_MAGIC)
    kernel[plan_boot.ZIMAGE_MAGIC_OFFSET:magic_end] = plan_boot.ZIMAGE_MAGIC
    kernel_path = tmp_path / "zImage"
    initrd_path = tmp_path / "initramfs.cpio.gz"
    kernel_path.write_bytes(kernel)
    initrd_path.write_bytes(b"\x1f\x8btest")
    write_kernel_and_ramdiag_reports(
        kernel_path, bytes(kernel), initrd_path, b"\x1f\x8btest"
    )
    plan = plan_boot.make_plan(
        build, kernel_path, initrd_path, plan_boot.DEFAULT_COMMAND_LINE
    )

    with pytest.raises(ram_transport.RamBootError, match="kernel upload"):
        ram_transport.execute_plan(
            FakeOpenIBootChannel(), plan, bytes(kernel[:-1]) + b"x", b"\x1f\x8btest"
        )
    plan["steps"][1]["command"] = 'kernel "root=/dev/mmcblk0"'
    with pytest.raises(ram_transport.RamBootError, match="kernel selection"):
        ram_transport.execute_plan(
            FakeOpenIBootChannel(), plan, bytes(kernel), b"\x1f\x8btest"
        )


class ScriptedBulkDevice:
    def __init__(self, reads):
        self.reads = list(reads)
        self.writes = []

    def write(self, endpoint, body, timeout):
        self.writes.append((endpoint, bytes(body), timeout))
        return len(body)

    def read(self, endpoint, size, timeout):
        assert endpoint == ram_transport.IN_ENDPOINT
        assert size == 4096
        assert timeout == 250
        return self.reads.pop(0)


def make_openiboot_channel(reads):
    channel = object.__new__(ram_transport.OpenIBootUSB)
    channel.device = ScriptedBulkDevice(reads)
    channel.usb_core = SimpleNamespace(USBTimeoutError=TimeoutError)
    channel.usb_util = None
    channel.backend = None
    return channel


def test_openiboot_usb_upload_uses_only_fixed_sendfile_protocol():
    channel = make_openiboot_channel([b"ACM: Received file"])
    body = b"payload"

    channel.upload(body)

    assert channel.device.writes == [
        (
            ram_transport.OUT_ENDPOINT,
            f"sendfile 0x{ram_transport.UPLOAD_ADDRESS:08x} {len(body)}\n".encode(),
            5000,
        ),
        (ram_transport.OUT_ENDPOINT, body, 5000),
    ]


def test_openiboot_usb_probe_accepts_only_exact_volatile_version():
    response = ram_transport.OPENIBOOT_VERSION_MARKER + b"\r\nACM: Done: version"
    channel = make_openiboot_channel([response])

    report = channel.probe_version()

    assert report["openiboot_usb_alive"] is True
    assert report["version_marker_matched"] is True
    assert report["persistent_device_writes_enabled"] is False
    assert channel.device.writes == [
        (ram_transport.OUT_ENDPOINT, b"version\n", 5000)
    ]
    assert openiboot_probe.PROBE_TOKEN == "N81_OPENIBOOT_VOLATILE_PROBE"

    wrong = make_openiboot_channel([b"openiboot unknown\r\nACM: Done: version"])
    with pytest.raises(ram_transport.RamBootError, match="not the qualified"):
        wrong.probe_version()


def test_openiboot_usb_state_probe_is_fixed_and_requires_in_place_vectors():
    response = (
        b"N81STATE sctlr=00c50078 ttbr0=84020000 "
        b"vbar=84000000 miu=00000001\r\nACM: Done: n81state"
    )
    channel = make_openiboot_channel([response])

    report = channel.probe_n81_state()

    assert report == {
        "n81_state_reported": True,
        "sctlr": 0x00C50078,
        "ttbr0": 0x84020000,
        "vbar": 0x84000000,
        "miu": 1,
        "persistent_device_writes_enabled": False,
    }
    assert channel.device.writes == [
        (ram_transport.OUT_ENDPOINT, b"n81state\n", 5000)
    ]

    wrong_vbar = make_openiboot_channel(
        [b"N81STATE sctlr=00000000 ttbr0=00000000 vbar=00000000 miu=00000000"
         b"\r\nACM: Done: n81state"]
    )
    with pytest.raises(ram_transport.RamBootError, match="exception vectors"):
        wrong_vbar.probe_n81_state()


def test_dfu_exploit_requalifies_continuity_after_every_reenumeration(monkeypatch):
    continuity = {"session": "bound"}
    devices = [SimpleNamespace(serial_number="untrusted") for _ in range(6)]
    devices.append(
        SimpleNamespace(
            serial_number=(
                "CPID:8930 ECID:1234 SRTG:[iBoot-574.4] PWND:[SHAtter]"
            )
        )
    )
    acquisitions = []
    operations = []

    def acquire(bundle, require_pwned=False):
        assert bundle is continuity
        acquisitions.append(require_pwned)
        return devices[len(acquisitions) - 1]

    monkeypatch.setattr(dfu_transport, "acquire_qualified_device", acquire)
    monkeypatch.setattr(
        dfu_transport,
        "qualify_dfu_serial",
        lambda serial, bundle, require_pwned: {"pwned_shatter": require_pwned},
    )
    monkeypatch.setattr(dfu_transport, "release_device", lambda device: None)
    monkeypatch.setattr(
        dfu_transport, "reset_counters", lambda device: operations.append("reset")
    )
    monkeypatch.setattr(
        dfu_transport,
        "get_data",
        lambda device, amount: operations.append(("get", amount)),
    )
    monkeypatch.setattr(
        dfu_transport, "usb_reset", lambda device: operations.append("usb-reset")
    )
    monkeypatch.setattr(
        dfu_transport,
        "request_image_validation",
        lambda device: operations.append("validate"),
    )
    monkeypatch.setattr(
        dfu_transport,
        "send_data",
        lambda device, body: operations.append(("send", len(body))),
    )
    monkeypatch.setattr(dfu_transport.time, "sleep", lambda seconds: None)

    result = dfu_transport.exploit(b"safe", continuity)

    assert acquisitions == [False, False, False, False, False, False, True]
    assert operations == [
        "reset",
        ("get", 0x40),
        "usb-reset",
        "validate",
        ("get", 0x2C000),
        "reset",
        ("get", 0x140),
        "usb-reset",
        "validate",
        ("send", len(dfu_transport.generate_exploit_payload(b"safe"))),
        ("get", 0x2C000),
    ]
    assert result["pwned_shatter"] is True


def test_ram_boot_plan_rejects_storage_roots_and_tampered_loader(tmp_path):
    build = make_qualified_build(tmp_path)
    with pytest.raises(plan_boot.PlanError, match="local storage"):
        plan_boot.check_command_line(
            "root=/dev/mmcblk0 ipodtouch4.storage=disabled"
        )

    (build / "openiboot-n81-volatile.bin").write_bytes(b"tampered")
    with pytest.raises(plan_boot.PlanError, match="does not match"):
        plan_boot.verified_loader(build)


def test_patch_defines_storage_free_target_and_bounded_ram_contract():
    patch = (TOOLS / "patches/openiboot-ipodtouch4g-volatile.patch").read_text(
        encoding="utf-8"
    )
    added = "\n".join(
        line[1:] for line in patch.splitlines() if line.startswith("+")
    )

    assert "CONFIG_VOLATILE_NO_STORAGE" in added
    assert "ipt_4g_volatile_openiboot" in added
    assert "MACH_ID=3564" in added
    assert "kernel = (void*)KERNEL_LOAD" in added
    assert "ramdisk = (void*)INITRD_LOAD" in added
    assert "memcpy((void*)KERNEL_LOAD, (void*)VOLATILE_UPLOAD" in added
    assert "memcpy((void*)INITRD_LOAD, (void*)VOLATILE_UPLOAD" in added
    defines = qualify.parse_defines(added)
    assert {name: defines[name] for name in qualify.EXPECTED_LAYOUT} == (
        qualify.EXPECTED_LAYOUT
    )
    base_sources = qualify.volatile_source_names(added, "volatile_base_src")
    a4_sources = qualify.volatile_source_names(added, "volatile_plat_a4_src")
    sources = base_sources | a4_sources
    assert base_sources == qualify.EXPECTED_VOLATILE_BASE_SOURCES
    assert a4_sources == qualify.EXPECTED_VOLATILE_A4_SOURCES
    assert not sources & qualify.FORBIDDEN_VOLATILE_SOURCES
    assert '"acm"' in added
    assert '"usb-synopsys"' in added
    assert "#ifndef CONFIG_VOLATILE_NO_STORAGE" in added
    assert "displaypipe_init()" not in added


def test_kernel_patch_removes_storage_and_repairs_n81_framebuffer_depth():
    patch = (TOOLS / "patches/idroid-kernel-n81-volatile.patch").read_text(
        encoding="utf-8"
    )
    added = "\n".join(
        line[1:] for line in patch.splitlines() if line.startswith("+")
    )

    assert "s5l8930_register_clcd(&video_mode, 32, &clcd_info)" in added
    assert "dev-h2fmi.o" not in added
    assert "select S3C_DEV_HSMMC" not in added
    assert "s3c_sdhci0_set_platdata" not in added
    assert "__asmeq(\"%1\", \"r2\")" in added


def test_kernel_config_gate_rejects_storage_subsystems():
    safe = "\n".join(
        f"{name}={value}" if value != "n" else f"# {name} is not set"
        for name, value in qualify_kernel.REQUIRED_CONFIG.items()
    )
    parsed = qualify_kernel.parse_config(safe)
    for name, expected in qualify_kernel.REQUIRED_CONFIG.items():
        assert parsed.get(name, "n") == expected

    unsafe = qualify_kernel.parse_config("CONFIG_BLK_DEV_H2FMI=y\n")
    assert any(
        name.startswith(qualify_kernel.FORBIDDEN_CONFIG_PREFIXES)
        for name, value in unsafe.items()
        if value == "y"
    )


def test_ramdiag_newc_parser_rejects_non_archive():
    with pytest.raises(qualify_ramdiag.QualificationError, match="well-formed"):
        qualify_ramdiag.parse_newc(b"not-cpio")

    assembly = (TOOLS / "ramdiag/init.S").read_text(encoding="utf-8").lower()
    assert "frame_bytes,     2457600" in assembly
    assert "mov     r4, #30" in assembly
    for forbidden in ("/dev/mmc", "/dev/mtd", "mount", "pivot_root"):
        assert forbidden not in assembly


def test_eclair_n81_gate_requires_android_runtime_without_storage_paths():
    assert stage_eclair.INITRAMFS_MAX == 20 * 1024 * 1024
    for required in (
        "init",
        "system/bin/app_process",
        "system/bin/servicemanager",
        "system/lib/hw/gralloc.default.so",
        "system/lib/libbinder.so",
        "system/lib/libsurfaceflinger.so",
    ):
        assert required in stage_eclair.REQUIRED_ENTRIES
    markers = " ".join(stage_eclair.FORBIDDEN_PATH_MARKERS)
    for forbidden in ("dev/mmc", "dev/mtd", "system/bin/mount", "system/bin/vold"):
        assert forbidden in markers


def test_eclair_plan_requires_android_kernel_and_n81_command_line(tmp_path):
    loader_build = make_qualified_build(tmp_path)
    kernel_dir = tmp_path / "kernel"
    root_dir = tmp_path / "root"
    kernel_dir.mkdir()
    root_dir.mkdir()
    kernel = bytearray(0x40)
    magic_end = plan_boot.ZIMAGE_MAGIC_OFFSET + len(plan_boot.ZIMAGE_MAGIC)
    kernel[plan_boot.ZIMAGE_MAGIC_OFFSET:magic_end] = plan_boot.ZIMAGE_MAGIC
    kernel_path = kernel_dir / "zImage-n81-volatile"
    kernel_path.write_bytes(kernel)
    (kernel_dir / "kernel-qualification.json").write_text(
        json.dumps(
            {
                "artifact_gate_passed": True,
                "product_type": "iPod4,1",
                "hardware_model": "N81AP",
                "machine_id": 3564,
                "storage_subsystems_configured": False,
                "storage_controller_symbols_present": False,
                "hardware_actions_enabled": False,
                "device_test_ready": False,
                "android_binder_compiled": True,
                "android_logger_compiled": True,
                "android_lowmemorykiller_compiled": True,
                "artifacts": {
                    "zimage": {
                        "size": len(kernel),
                        "sha256": plan_boot.sha256_bytes(kernel),
                    }
                },
            }
        ),
        encoding="utf-8",
    )
    initrd = b"\x1f\x8bqualified-eclair"
    initrd_path = root_dir / "n81-eclair-initramfs.cpio.gz"
    initrd_path.write_bytes(initrd)
    eclair_report = {
        "artifact_gate_passed": True,
        "board": "apple-n81-ipod-touch-4g",
        "profile": "n81-eclair-2.0-volatile-no-storage",
        "storage_paths_present": False,
        "shell_present": False,
        "hardware_actions_enabled": False,
        "device_test_ready": False,
        "host_full_system_gate_passed": True,
        "binder_userspace_packaged": True,
        "zygote_packaged": True,
        "surfaceflinger_packaged": True,
        "software_renderer_packaged": True,
        "linux_framebuffer_gralloc_path_packaged": True,
        "tmpfs_mutable_state": True,
        "root_read_only": True,
        "artifacts": {
            "initramfs": {
                "size": len(initrd),
                "sha256": plan_boot.sha256_bytes(initrd),
            }
        },
    }
    (root_dir / "eclair-n81-qualification.json").write_text(
        json.dumps(eclair_report), encoding="utf-8"
    )
    command_line = (
        "console=tty0 rdinit=/init androidboot.hardware=n81 "
        "ipodtouch4.storage=disabled"
    )

    result = plan_boot.make_plan(
        loader_build, kernel_path, initrd_path, command_line
    )
    assert result["profile"] == "n81-eclair-2.0-volatile-no-storage"

    with pytest.raises(plan_boot.PlanError, match="androidboot.hardware=n81"):
        plan_boot.make_plan(
            loader_build,
            kernel_path,
            initrd_path,
            command_line.replace(" androidboot.hardware=n81", ""),
        )


def test_qualifier_forbids_storage_and_persistent_write_interfaces():
    patterns = "\n".join(qualify.FORBIDDEN_SYMBOL_PATTERNS)
    for marker in ("h2fmi", "nand", "mtd", "vfl", "ftl", "saveenv", "install"):
        assert marker in patterns or marker + ".c" in qualify.FORBIDDEN_VOLATILE_SOURCES

    assert qualify.EXPECTED_LAYOUT == {
        "GeneralStack": 0x8402E000,
        "HeapStart": 0x84020000,
        "HeapEnd": 0x8402B000,
        "KERNEL_LOAD": 0x4A000000,
        "INITRD_LOAD": 0x4B000000,
        "VOLATILE_UPLOAD": 0x4D000000,
        "VOLATILE_KERNEL_MAX": 0x00800000,
        "VOLATILE_INITRD_MAX": 0x01400000,
    }
