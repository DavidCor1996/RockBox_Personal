#!/usr/bin/env python3
"""Bounded OpeniBoot ACM uploader for the qualified N81 RAM-only plan."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import time

from plan_ram_boot import DEFAULT_COMMAND_LINE, PlanError, check_command_line, make_plan
from transport_usb import USBBackendError, ensure_usb_process_environment, load_usb


OPENIBOOT_VENDOR = 0x0525
OPENIBOOT_PRODUCT = 0x1280
OPENIBOOT_PRODUCT_NAME = "Apple Mobile Device (OIB)"
OPENIBOOT_VERSION_MARKER = (
    b"openiboot 0.3 commit 866562f for iPod Touch 4G volatile"
)
N81_STATE_PATTERN = re.compile(
    rb"N81STATE sctlr=([0-9a-fA-F]{8}) ttbr0=([0-9a-fA-F]{8}) "
    rb"vbar=([0-9a-fA-F]{8}) miu=([0-9a-fA-F]{8})"
)
OUT_ENDPOINT = 0x02
IN_ENDPOINT = 0x81
UPLOAD_ADDRESS = 0x4D000000
RAMDIAG_TOKEN = "N81_RAMDIAG_VOLATILE_BOOT"
ECLAIR_TOKEN = "N81_ECLAIR_2_0_VOLATILE_BOOT"


class RamBootError(RuntimeError):
    """Raised when the ACM upload cannot honor the exact RAM-only plan."""


def required_token(profile: str) -> str:
    if profile == "n81-volatile-ramdiag-no-storage":
        return RAMDIAG_TOKEN
    if profile == "n81-eclair-2.0-volatile-no-storage":
        return ECLAIR_TOKEN
    raise RamBootError("RAM boot plan has an unsupported profile")


class OpenIBootUSB:
    """The only exposed operations are bounded file upload and fixed commands."""

    def __init__(self, device):
        self.device = device
        try:
            self.usb_core, self.usb_util, self.backend = load_usb()
        except USBBackendError as error:
            raise RamBootError(str(error)) from error

    @classmethod
    def acquire(cls, timeout: float = 10.0) -> "OpenIBootUSB":
        try:
            usb_core, usb_util, backend = load_usb()
        except USBBackendError as error:
            raise RamBootError(str(error)) from error
        deadline = time.monotonic() + timeout
        while True:
            devices = list(
                usb_core.find(
                    find_all=True,
                    idVendor=OPENIBOOT_VENDOR,
                    idProduct=OPENIBOOT_PRODUCT,
                    backend=backend,
                )
                or []
            )
            if len(devices) > 1:
                raise RamBootError("multiple OpeniBoot USB devices are connected")
            if len(devices) == 1:
                device = devices[0]
                try:
                    product = usb_util.get_string(device, device.iProduct)
                except Exception as error:
                    raise RamBootError(f"cannot read OpeniBoot descriptor: {error}") from error
                if product != OPENIBOOT_PRODUCT_NAME:
                    raise RamBootError("USB device has an unexpected OpeniBoot product string")
                try:
                    device.set_configuration()
                except usb_core.USBError:
                    pass
                return cls(device)
            if time.monotonic() >= deadline:
                raise RamBootError("qualified OpeniBoot USB device was not found")
            time.sleep(0.02)

    def close(self) -> None:
        self.usb_util.dispose_resources(self.device)

    def _write(self, body: bytes) -> None:
        for offset in range(0, len(body), 16 * 1024):
            chunk = body[offset : offset + 16 * 1024]
            written = self.device.write(OUT_ENDPOINT, chunk, timeout=5000)
            if written != len(chunk):
                raise RamBootError("short OpeniBoot bulk write")

    def read_until(self, marker: bytes, timeout: float = 10.0) -> bytes:
        deadline = time.monotonic() + timeout
        received = bytearray()
        while marker not in received:
            if time.monotonic() >= deadline:
                raise RamBootError(f"OpeniBoot did not report {marker!r}")
            try:
                received.extend(bytes(self.device.read(IN_ENDPOINT, 4096, timeout=250)))
            except self.usb_core.USBTimeoutError:
                continue
            if len(received) > 128 * 1024:
                del received[: len(received) - 64 * 1024]
        return bytes(received)

    def _fixed_command(self, command: str) -> bytes:
        if not command or any(character in command for character in "\0\r\n"):
            raise RamBootError("invalid fixed OpeniBoot command")
        self._write(command.encode("ascii") + b"\n")
        return self.read_until(b"ACM: Done:")

    def probe_version(self) -> dict[str, object]:
        """Prove the exact volatile N81 loader without exposing a command API."""
        response = self._fixed_command("version")
        if OPENIBOOT_VERSION_MARKER not in response:
            raise RamBootError("OpeniBoot version response is not the qualified N81 loader")
        return {
            "openiboot_usb_alive": True,
            "product_type": "iPod4,1",
            "hardware_model": "N81AP",
            "volatile_profile": True,
            "version_marker_matched": True,
            "persistent_device_writes_enabled": False,
        }

    def probe_n81_state(self) -> dict[str, int | bool]:
        """Read only the fixed registers needed to design the Linux handoff."""
        response = self._fixed_command("n81state")
        match = N81_STATE_PATTERN.search(response)
        if not match:
            raise RamBootError("OpeniBoot did not return the fixed N81 state report")
        sctlr, ttbr0, vbar, miu = (int(value, 16) for value in match.groups())
        if vbar != 0x84000000:
            raise RamBootError("OpeniBoot exception vectors are not at the in-place loader")
        return {
            "n81_state_reported": True,
            "sctlr": sctlr,
            "ttbr0": ttbr0,
            "vbar": vbar,
            "miu": miu,
            "persistent_device_writes_enabled": False,
        }

    def select_kernel(self, command_line: str) -> None:
        try:
            check_command_line(command_line)
        except PlanError as error:
            raise RamBootError(str(error)) from error
        self._fixed_command(f'kernel "{command_line}"')

    def select_initrd(self) -> None:
        self._fixed_command("initrd")

    def boot(self) -> None:
        self._write(b"boot\n")

    def upload(self, body: bytes) -> None:
        if not body:
            raise RamBootError("cannot upload an empty artifact")
        command = f"sendfile 0x{UPLOAD_ADDRESS:08x} {len(body)}"
        # OpeniBoot deliberately suppresses console output while sendfile is
        # busy. Its OUT endpoint is re-queued only after parsing this command,
        # so the following bulk write blocks until the file receiver is ready.
        self._write(command.encode("ascii") + b"\n")
        self._write(body)
        self.read_until(b"ACM: Received file")


def execute_plan(channel, plan: dict[str, object], kernel: bytes, initrd: bytes) -> None:
    expected_header = {
        "product_type": "iPod4,1",
        "hardware_model": "N81AP",
        "machine_id": 3564,
        "hardware_actions_enabled": False,
        "transport_implemented": True,
        "device_test_ready": False,
    }
    for name, expected in expected_header.items():
        if plan.get(name) != expected:
            raise RamBootError(f"RAM boot plan has unexpected {name}")
    steps = plan.get("steps")
    if not isinstance(steps, list) or len(steps) != 5:
        raise RamBootError("RAM boot plan does not have the fixed five-step sequence")
    expected_uploads = (("kernel", kernel), ("initramfs", initrd))
    for step_index, (kind, body) in zip((0, 2), expected_uploads):
        step = steps[step_index]
        if (
            step.get("kind") != kind
            or step.get("size") != len(body)
            or step.get("sha256") != hashlib.sha256(body).hexdigest()
            or step.get("sendfile_command")
            != f"sendfile 0x{UPLOAD_ADDRESS:08x} {len(body)}"
        ):
            raise RamBootError(f"RAM boot plan has an invalid {kind} upload")
    command_line = plan.get("command_line")
    if not isinstance(command_line, str):
        raise RamBootError("RAM boot plan lacks its command line")
    if steps[1] != {"command": f'kernel "{command_line}"'}:
        raise RamBootError("RAM boot plan has an invalid kernel selection")
    if steps[3] != {"command": "initrd"}:
        raise RamBootError("RAM boot plan has an invalid initramfs selection")
    channel.upload(kernel)
    channel.select_kernel(command_line)
    channel.upload(initrd)
    channel.select_initrd()
    if steps[4] != {"command": "boot", "requires_separate_physical_gate": True}:
        raise RamBootError("RAM boot plan has an invalid final step")
    channel.boot()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("build", type=Path)
    parser.add_argument("kernel", type=Path)
    parser.add_argument("initramfs", type=Path)
    parser.add_argument("--command-line", default=DEFAULT_COMMAND_LINE)
    parser.add_argument(
        "--execute-token",
        help="omit for a non-executing, hash-bound manifest",
    )
    args = parser.parse_args()
    try:
        build = args.build.resolve()
        kernel_path = args.kernel.resolve()
        initrd_path = args.initramfs.resolve()
        plan = make_plan(build, kernel_path, initrd_path, args.command_line)
        kernel = kernel_path.read_bytes()
        initrd = initrd_path.read_bytes()
        token = required_token(str(plan["profile"]))
        if args.execute_token is None:
            print(json.dumps(plan, indent=2, sort_keys=True))
            return 0
        if args.execute_token != token:
            raise RamBootError(f"physical boot requires exact token {token}")
        ensure_usb_process_environment()
        channel = OpenIBootUSB.acquire()
        try:
            execute_plan(channel, plan, kernel, initrd)
        finally:
            channel.close()
        print(json.dumps({"volatile_ram_boot_dispatched": True, "profile": plan["profile"]}))
    except (OSError, PlanError, RamBootError) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
