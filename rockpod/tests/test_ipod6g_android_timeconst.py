"""Qualification tests for the bc-free Linux timeconst generator."""

import importlib.util
import io
from pathlib import Path


MODULE_PATH = (
    Path(__file__).resolve().parents[2]
    / "tools"
    / "ipod6g_android"
    / "generate_timeconst.py"
)
SPEC = importlib.util.spec_from_file_location("generate_timeconst", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def test_matches_known_linux_hz_1000_constants():
    output = MODULE.generate(1000)

    assert "#define HZ_TO_MSEC_MUL32\tU64_C(0x80000000)" in output
    assert "#define HZ_TO_MSEC_SHR32\t31" in output
    assert "#define HZ_TO_USEC_MUL32\tU64_C(0xFA000000)" in output
    assert "#define NSEC_TO_HZ_DEN\t\t1000000" in output


def test_generates_ipod_kernel_hz_100_header():
    output = MODULE.generate(100)

    assert "#if HZ != 100" in output
    assert "#define HZ_TO_MSEC_NUM\t\t10" in output
    assert "#define MSEC_TO_HZ_DEN\t\t10" in output


def test_bc_shim_is_restricted_to_linux_timeconst_invocation():
    shim_path = MODULE_PATH.with_name("timeconst_bc_shim.py")
    spec = importlib.util.spec_from_file_location("timeconst_bc_shim", shim_path)
    shim = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(shim)
    output = io.StringIO()

    assert shim.main(["-q", "kernel/time/timeconst.bc"], io.StringIO("100\n"), output) == 0
    assert "#if HZ != 100" in output.getvalue()
    assert shim.main(["arbitrary.bc"], io.StringIO("100\n"), io.StringIO()) == 2
