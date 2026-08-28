#!/usr/bin/env python3
"""Exact, retry-bounded PyUSB/libusb loader shared by the N81 transports."""

from __future__ import annotations

import os
import json
import sys
import time


PYUSB_VERSION = "1.3.1"
BACKEND_ATTEMPTS = 4
BACKEND_RETRY_SECONDS = 0.02
BACKEND_INITIAL_DELAY_SECONDS = 0.02


class USBBackendError(RuntimeError):
    """Raised when the exact host USB dependency cannot be initialized."""


def ensure_usb_process_environment() -> None:
    """Re-exec once so the CPython 3.14 workaround exists at process start."""
    if os.environ.get("PYUSB_DEBUG") == "critical":
        return
    os.execv(
        "/usr/bin/env",
        ["env", "PYUSB_DEBUG=critical", sys.executable, *sys.argv],
    )


def load_usb():
    # PyUSB 1.3.1's tracing wrapper avoids a CPython 3.14/libusb_init startup
    # failure observed on this host. "critical" enables the wrapper without
    # emitting descriptors, identifiers, or transfer logs.
    if os.environ.get("PYUSB_DEBUG") != "critical":
        raise USBBackendError("USB process was not initialized by the safe launcher")
    try:
        import usb
        import usb.backend.libusb1
        import usb.core
        import usb.util
    except ImportError as error:
        raise USBBackendError(
            f"PyUSB {PYUSB_VERSION} and libusb 1.x are required"
        ) from error
    if getattr(usb, "__version__", None) != PYUSB_VERSION:
        raise USBBackendError(
            f"expected PyUSB {PYUSB_VERSION}, found "
            f"{getattr(usb, '__version__', 'unknown')}"
        )
    # Some Linux hosts transiently return LIBUSB_ERROR_OTHER when libusb_init
    # is called in the same instant the ctypes backend is first loaded.
    time.sleep(BACKEND_INITIAL_DELAY_SECONDS)
    backend = None
    for attempt in range(BACKEND_ATTEMPTS):
        backend = usb.backend.libusb1.get_backend()
        if backend is not None:
            return usb.core, usb.util, backend
        if attempt + 1 < BACKEND_ATTEMPTS:
            time.sleep(BACKEND_RETRY_SECONDS)
    raise USBBackendError("libusb 1.x backend could not be initialized")


def main() -> int:
    ensure_usb_process_environment()
    try:
        _, _, backend = load_usb()
    except USBBackendError as error:
        print(json.dumps({"usb_host_preflight_passed": False, "error": str(error)}))
        return 1
    print(
        json.dumps(
            {
                "usb_host_preflight_passed": True,
                "pyusb_version": PYUSB_VERSION,
                "libusb_backend": type(backend).__name__,
                "device_enumeration_performed": False,
                "device_transfer_performed": False,
            },
            indent=2,
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
