"""Build (once, cached) and invoke the blast_cli PKWare DCL decompressor."""
from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path

_HERE = Path(__file__).resolve().parent
_BIN = _HERE / "blast_cli.bin"


def ensure_built() -> Path:
    sources = [_HERE / "blast_cli.c", _HERE / "blast.c"]
    if _BIN.exists() and _BIN.stat().st_mtime > max(s.stat().st_mtime for s in sources):
        return _BIN
    subprocess.run(
        ["cc", "-O2", "-Wall", "-o", str(_BIN), *[str(s) for s in sources]],
        check=True,
    )
    return _BIN


def explode(data: bytes) -> bytes:
    binary = ensure_built()
    with tempfile.NamedTemporaryFile(delete=False) as fi:
        fi.write(data)
        inpath = Path(fi.name)
    outpath = inpath.with_suffix(".out")
    try:
        result = subprocess.run(
            [str(binary), str(inpath), str(outpath)], capture_output=True
        )
        if result.returncode != 0:
            raise RuntimeError(
                f"blast_cli failed rc={result.returncode}: {result.stderr!r}"
            )
        return outpath.read_bytes()
    finally:
        inpath.unlink(missing_ok=True)
        outpath.unlink(missing_ok=True)


def selftest() -> None:
    """The worked example from blast.c's own header comment."""
    packed = bytes([0x00, 0x04, 0x82, 0x24, 0x25, 0x8F, 0x80, 0x7F])
    result = explode(packed)
    expected = b"AIAIAIAIAIAIA"
    if result != expected:
        raise AssertionError(f"blast selftest failed: {result!r} != {expected!r}")
