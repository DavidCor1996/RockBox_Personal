"""Guards that keep RockPod Linux storage invisible to iPod firmware."""

from pathlib import Path


FORBIDDEN_TOKENS = (
    "rockpod-linux",
    ".rockpod-linux",
    "BOOTX64.EFI",
    "rootfs.squashfs",
    "linux_system",
    "Start Linux",
)


def test_rockbox_firmware_does_not_reference_rockpod_linux_payload():
    repo_root = Path(__file__).resolve().parents[2]
    source_roots = (
        repo_root / "apps",
        repo_root / "bootloader",
        repo_root / "firmware",
    )
    suffixes = {".c", ".h", ".S", ".s", ".cpp", ".cc", ".hpp", ".inc"}
    matches = []

    for source_root in source_roots:
        for path in source_root.rglob("*"):
            if not path.is_file() or path.suffix not in suffixes:
                continue
            try:
                text = path.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                continue
            for token in FORBIDDEN_TOKENS:
                if token in text:
                    matches.append(f"{path.relative_to(repo_root)}: {token}")

    assert matches == []
