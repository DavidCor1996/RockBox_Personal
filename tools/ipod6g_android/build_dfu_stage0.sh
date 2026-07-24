#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
uboot_source="${1:-}"
uboot_build="${2:-}"
wind3x_bin="${3:-}"
output_dfu="${4:-}"
build_dir="${5:-${uboot_build}/n25-dfu-stage0}"
cross_compile="${CROSS_COMPILE:-arm-none-eabi-}"
checkpoint="${N25_STAGE0_CHECKPOINT:-boot}"
stage0_python="${N25_UNICORN_PYTHON:-python3}"
source_dir="${repo_root}/tools/ipod6g_android/dfu_stage0"

if [[ -z "${uboot_source}" || -z "${uboot_build}" || \
      -z "${wind3x_bin}" || -z "${output_dfu}" ]]; then
    echo "usage: $0 UBOOT_SOURCE UBOOT_BUILD WIND3X OUTPUT_DFU [BUILD_DIR]" >&2
    exit 2
fi

uboot_source="$(realpath "${uboot_source}")"
uboot_build="$(realpath "${uboot_build}")"
wind3x_bin="$(realpath "${wind3x_bin}")"
output_dfu="$(realpath -m "${output_dfu}")"
build_dir="$(realpath -m "${build_dir}")"

[[ -f "${uboot_build}/u-boot.bin" ]] || {
    echo "missing U-Boot binary: ${uboot_build}/u-boot.bin" >&2
    exit 1
}
[[ -f "${uboot_build}/include/config.h" ]] || {
    echo "missing generated U-Boot config" >&2
    exit 1
}
[[ -f "${uboot_build}/.config" ]] || {
    echo "missing generated U-Boot .config" >&2
    exit 1
}
[[ -x "${wind3x_bin}" ]] || {
    echo "missing wInd3x executable" >&2
    exit 1
}

mkdir -p "${build_dir}" "$(dirname "${output_dfu}")"

python3 - "${uboot_build}/.config" <<'PY'
import sys
from pathlib import Path

config_path = Path(sys.argv[1])
config = {}
for line in config_path.read_text(encoding="utf-8").splitlines():
    if line.startswith("CONFIG_") and "=" in line:
        name, value = line.split("=", 1)
        config[name] = value

required = {
    "CONFIG_ENV_IS_NOWHERE": "y",
    "CONFIG_DFU_RAM": "y",
    "CONFIG_USB_GADGET_VENDOR_NUM": "0x05ac",
    "CONFIG_USB_GADGET_PRODUCT_NUM": "0x8007",
}
for name, expected in required.items():
    if config.get(name) != expected:
        raise SystemExit(
            f"unsafe U-Boot config: {name} must be {expected}, "
            f"got {config.get(name)!r}"
        )

forbidden = {
    "CONFIG_AHCI",
    "CONFIG_ATA",
    "CONFIG_CMD_IDE",
    "CONFIG_CMD_MMC",
    "CONFIG_CMD_MTD",
    "CONFIG_CMD_NVME",
    "CONFIG_CMD_SATA",
    "CONFIG_CMD_SCSI",
    "CONFIG_CMD_USB_MASS_STORAGE",
    "CONFIG_IDE",
    "CONFIG_MMC",
    "CONFIG_MTD",
    "CONFIG_NVME",
    "CONFIG_SATA",
    "CONFIG_SCSI",
    "CONFIG_USB_FUNCTION_MASS_STORAGE",
}
enabled_forbidden = sorted(name for name in forbidden if config.get(name) == "y")
enabled_persistent_env = sorted(
    name
    for name, value in config.items()
    if name.startswith("CONFIG_ENV_IS_IN_") and value == "y"
)
if enabled_forbidden or enabled_persistent_env:
    names = ", ".join(enabled_forbidden + enabled_persistent_env)
    raise SystemExit(f"unsafe U-Boot storage configuration: {names}")
PY

python3 "${repo_root}/tools/ipod6g_android/pack_dfu_stage0.py" \
    --uboot "${uboot_build}/u-boot.bin" \
    --deflate "${build_dir}/u-boot.deflate" \
    --header "${build_dir}/payload_constants.h" \
    --report "${build_dir}/compression.json"

cflags=(
    -march=armv5te -marm -Os -ffreestanding -fno-builtin
    -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables
    -ffunction-sections -fdata-sections -nostdlib -Wall -Wextra -Werror
)

case "${checkpoint}" in
    boot)
        ;;
    entry)
        cflags+=( -DN25_STAGE0_CHECKPOINT_ENTRY -Wno-error=unused-function )
        ;;
    dram)
        cflags+=( -DN25_STAGE0_CHECKPOINT_DRAM )
        ;;
    decompress)
        cflags+=( -DN25_RELOCATOR_CHECKPOINT_RESET )
        ;;
    *)
        echo "invalid N25_STAGE0_CHECKPOINT: ${checkpoint}" >&2
        exit 2
        ;;
esac

"${cross_compile}gcc" "${cflags[@]}" \
    -I"${uboot_build}/include" -I"${uboot_source}/include" \
    -I"${uboot_source}/arch/arm/include" \
    -I"${uboot_build}/arch/arm/include/generated" \
    -I"${uboot_build}/include/generated" \
    -I"${build_dir}" -c "${source_dir}/relocator.c" \
    -o "${build_dir}/relocator.o"
"${cross_compile}gcc" "${cflags[@]}" \
    -Wno-error=sign-compare \
    -I"${uboot_build}/include" -I"${uboot_source}/include" \
    -I"${uboot_source}/arch/arm/include" \
    -I"${uboot_build}/arch/arm/include/generated" \
    -I"${uboot_build}/include/generated" \
    -c "${uboot_source}/fs/jffs2/mini_inflate.c" \
    -o "${build_dir}/mini_inflate.o"
"${cross_compile}gcc" "${cflags[@]}" -c "${source_dir}/relocator.S" \
    -o "${build_dir}/relocator-start.o"
"${cross_compile}ld" --gc-sections -T "${source_dir}/relocator.lds" \
    -Map "${build_dir}/relocator.map" \
    -o "${build_dir}/relocator.elf" \
    "${build_dir}/relocator-start.o" "${build_dir}/relocator.o" \
    "${build_dir}/mini_inflate.o"
"${cross_compile}objcopy" -O binary "${build_dir}/relocator.elf" \
    "${build_dir}/relocator.bin"

(
    cd "${build_dir}"
    "${cross_compile}gcc" "${cflags[@]}" -c "${source_dir}/data.S" -o data.o
)
"${cross_compile}gcc" "${cflags[@]}" -c "${source_dir}/stage0.S" \
    -o "${build_dir}/stage0-start.o"
"${cross_compile}gcc" "${cflags[@]}" -c "${source_dir}/stage0.c" \
    -o "${build_dir}/stage0.o"
"${cross_compile}ld" --gc-sections -T "${source_dir}/stage0.lds" \
    -Map "${build_dir}/stage0.map" \
    -o "${build_dir}/stage0.elf" \
    "${build_dir}/stage0-start.o" "${build_dir}/stage0.o" \
    "${build_dir}/data.o"
"${cross_compile}objcopy" -O binary "${build_dir}/stage0.elf" \
    "${build_dir}/stage0.bin"

stage0_size="$(stat -c %s "${build_dir}/stage0.bin")"
if (( stage0_size > 114688 )); then
    echo "stage zero exceeds the 112 KiB fail-closed gate: ${stage0_size}" >&2
    exit 1
fi

"${wind3x_bin}" makedfu --kind n3g "${build_dir}/stage0.bin" "${output_dfu}"

emulation_json=""
if [[ "${checkpoint}" == "boot" ]]; then
    "${stage0_python}" -c 'import unicorn' || {
        echo "the boot checkpoint requires Python Unicorn for ARM926 qualification" >&2
        exit 1
    }
    emulation_json="${build_dir}/emulation.json"
    "${stage0_python}" \
        "${repo_root}/tools/ipod6g_android/emulate_n25_dfu_stage0.py" \
        --stage0 "${build_dir}/stage0.bin" \
        --uboot "${uboot_build}/u-boot.bin" \
        --compressed "${build_dir}/u-boot.deflate" \
        --relocator "${build_dir}/relocator.bin" \
        --json-output "${emulation_json}"
fi

python3 - "${build_dir}" "${output_dfu}" "${checkpoint}" \
    "${uboot_build}/u-boot.bin" "${uboot_build}/.config" \
    "${emulation_json}" <<'PY'
import hashlib
import json
import struct
import sys
from pathlib import Path

build = Path(sys.argv[1])
dfu = Path(sys.argv[2])
checkpoint = sys.argv[3]
uboot = Path(sys.argv[4])
uboot_config = Path(sys.argv[5])
emulation_path = Path(sys.argv[6]) if sys.argv[6] else None
data = dfu.read_bytes()
header = struct.unpack("<4s3sBIIIII32sHH16s", data[:80])
body_len = header[4]
stage = (build / "stage0.bin").read_bytes()
if header[:3] != (b"8702", b"1.0", 2) or header[3] != 0:
    raise SystemExit("unexpected IMG1 header")
if body_len != len(data) - 0x800 or data[0x800:0x800 + len(stage)] != stage:
    raise SystemExit("IMG1 body mismatch")
if body_len > 114688:
    raise SystemExit("IMG1 body exceeds Boot ROM workspace gate")

def item(path):
    content = path.read_bytes()
    return {
        "path": str(path.resolve()),
        "size": len(content),
        "sha256": hashlib.sha256(content).hexdigest(),
    }

emulation = None
if emulation_path is not None:
    emulation = json.loads(emulation_path.read_text(encoding="utf-8"))
    required_true = (
        "gate_passed",
        "dram_staging_matches",
        "relocator_copy_matches",
        "reconstructed_uboot_matches",
        "reached_relocated_stage",
        "reached_relocator",
        "reached_uboot_entry",
    )
    if not all(emulation.get(name) is True for name in required_true):
        raise SystemExit("ARM926 stage-zero emulation gate did not pass")
    expected_hashes = {
        "stage0": item(build / "stage0.bin")["sha256"],
        "uboot": item(uboot)["sha256"],
        "compressed": item(build / "u-boot.deflate")["sha256"],
        "relocator": item(build / "relocator.bin")["sha256"],
    }
    if emulation.get("artifacts") != expected_hashes:
        raise SystemExit("ARM926 emulation did not cover the packaged artifacts")

device_test_ready = checkpoint == "boot" and emulation is not None

report = {
    "artifact_gate_passed": True,
    "hardware_qualified": False,
    "device_test_ready": device_test_ready,
    "qualified": False,
    "qualification_note": (
        "Host-qualified for one volatile Stage-A USB enumeration test only. "
        "The exact sub-112-KiB stage zero executed through DRAM setup, "
        "relocation, decompression, integrity checks, and the reconstructed "
        "storage-free U-Boot entry in an ARM926 model. This is not physical "
        "hardware qualification."
        if device_test_ready
        else "Debug checkpoint only; it is not approved for device execution."
    ),
    "checkpoint": checkpoint,
    "load_address": "0x22000000",
    "relocator_address": "0x2203c000",
    "uboot_address": "0x22000000",
    "dram_staging_address": "0x08010000",
    "bootrom_body_limit": 114688,
    "storage_code_present": False,
    "artifacts": {
        "dfu": item(dfu),
        "stage0": item(build / "stage0.bin"),
        "stage0_elf": item(build / "stage0.elf"),
        "relocator": item(build / "relocator.bin"),
        "compressed_uboot": item(build / "u-boot.deflate"),
        "uboot": item(uboot),
        "uboot_config": item(uboot_config),
    },
}
if emulation_path is not None:
    report["artifacts"]["emulation"] = item(emulation_path)
    report["emulation"] = emulation
(build / "qualification.json").write_text(
    json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
)
print(json.dumps(report, indent=2, sort_keys=True))
PY
