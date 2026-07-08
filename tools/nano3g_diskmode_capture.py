#!/usr/bin/env python3
"""Capture host-visible Nano 3G WinPod sectors for WMOUNT replay.

This tool reads the iPod as Linux exposes it in Apple disk mode. It does not
write to the device.
"""

from __future__ import annotations

import argparse
from pathlib import Path


def read_at(path: Path, offset: int, size: int) -> bytes:
    with path.open("rb", buffering=0) as handle:
        handle.seek(offset)
        data = handle.read(size)
    if len(data) != size:
        raise OSError(f"short read from {path}: got {len(data)} wanted {size}")
    return data


def le16(data: bytes, off: int) -> int:
    return data[off] | (data[off + 1] << 8)


def le32(data: bytes, off: int) -> int:
    return (
        data[off]
        | (data[off + 1] << 8)
        | (data[off + 2] << 16)
        | (data[off + 3] << 24)
    )


def write_sector(out: Path, name: str, data: bytes, manifest: list[str]) -> None:
    target = out / name
    target.write_bytes(data)
    manifest.append(f"file={name} bytes={len(data)}")
    print(f"N3GDM_WRITE {target} bytes={len(data)}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--disk", required=True, type=Path, help="whole iPod block device, e.g. /dev/sdb")
    parser.add_argument("--part", required=True, type=Path, help="FAT partition block device, e.g. /dev/sdb1")
    parser.add_argument("--out", default=Path("tmp/n3g-diskmode-current"), type=Path)
    parser.add_argument("--logical-sector-size", default=4096, type=int)
    args = parser.parse_args(argv)

    args.out.mkdir(parents=True, exist_ok=True)
    manifest: list[str] = []

    mbr = read_at(args.disk, 0, 512)
    write_sector(args.out, "disk_lba0_512.bin", mbr, manifest)
    sig = le16(mbr, 0x1FE)
    p0 = 0x1BE
    ptype = mbr[p0 + 4]
    pstart = le32(mbr, p0 + 8)
    psize = le32(mbr, p0 + 12)
    print(f"N3GDM_MBR sig={sig:04X} p0t={ptype:02X} p0st={pstart:08X} p0sz={psize:08X}")
    manifest.append(f"mbr_sig={sig:04X} p0t={ptype:02X} p0st={pstart:08X} p0sz={psize:08X}")

    boot = read_at(args.part, 0, args.logical_sector_size)
    write_sector(args.out, "part_lba0_boot.bin", boot, manifest)
    bps = le16(boot, 0x0B)
    spc = boot[0x0D]
    reserved = le16(boot, 0x0E)
    nfats = boot[0x10]
    fatsz = le16(boot, 0x16) or le32(boot, 0x24)
    root_cluster = le32(boot, 0x2C)
    fsinfo = le16(boot, 0x30)
    backup = le16(boot, 0x32)
    print(
        "N3GDM_BPB "
        f"sig={le16(boot, 0x1FE):04X} bps={bps} spc={spc} rs={reserved} "
        f"nf={nfats} fatsz={fatsz} root={root_cluster} fsinfo={fsinfo} backup={backup}"
    )
    manifest.append(
        f"bpb_sig={le16(boot, 0x1FE):04X} bps={bps} spc={spc} rs={reserved} "
        f"nf={nfats} fatsz={fatsz} root={root_cluster} fsinfo={fsinfo} backup={backup}"
    )

    if bps not in (512, 1024, 2048, 4096) or bps != args.logical_sector_size:
        print(f"N3GDM_WARN bps={bps} logical_sector_size={args.logical_sector_size}")

    sector_size = args.logical_sector_size
    first_fat = reserved
    fat2 = reserved + fatsz
    first_data = reserved + nfats * fatsz
    root_lba = first_data + (root_cluster - 2) * spc if root_cluster >= 2 else first_data
    probes = [
        ("part_lba1_fsinfo.bin", fsinfo),
        ("part_lba6_backup.bin", backup),
        ("part_lba_fat1.bin", first_fat),
        ("part_lba_fat2.bin", fat2),
        ("part_lba_root.bin", root_lba),
        ("part_lba_root_next.bin", root_lba + 1),
    ]
    for name, lba in probes:
        data = read_at(args.part, lba * sector_size, sector_size)
        write_sector(args.out, name, data, manifest)
        print(f"N3GDM_PROBE {name} lba={lba:08X} sig={le16(data[:512], 0x1FE):04X}")
        manifest.append(f"probe={name} lba={lba:08X}")

    (args.out / "manifest.txt").write_text("\n".join(manifest) + "\n", encoding="utf-8")
    print(f"N3GDM_DONE out={args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
