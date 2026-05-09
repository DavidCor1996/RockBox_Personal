#!/usr/bin/env python3
"""Write targeted Nano 3G WMOUNT replay dumps over DFU/wInd3x.

This avoids using the iPod LCD as a bulk transport. The output directory is a
host-side fixture for tools/nano3g_wmount_replay.py.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import subprocess
from tempfile import TemporaryDirectory
import string

import nano3g_ftl_replay as replay


N3G_BANKS = 4
N3G_PAGES_PER_BLOCK = 128
N3G_PAGES_PER_HYPERBLOCK = N3G_BANKS * N3G_PAGES_PER_BLOCK
N3G_SYSTEM_HYPERBLOCKS = 425
N3G_OOB_LEN = 64
N3G_ENTRY_SPAN = N3G_PAGES_PER_HYPERBLOCK * 4
TARGET_LBA = 0xA07E
NEAR_LO = 0x00009C00
NEAR_HI = 0x0000A200


@dataclass(frozen=True)
class MapTarget:
    name: str
    block: int
    page: int


@dataclass(frozen=True)
class PageTarget:
    name: str
    j: int
    v: int
    po: int
    slot: int = 0xFFFFFFFF
    l0_hint: int = 0xFFFFFFFF


@dataclass(frozen=True)
class DecodedEntry:
    map_name: str
    j: int
    v: int
    l0: int
    entry_type: int


@dataclass(frozen=True)
class MissingTarget:
    name: str
    j: int
    v: int
    po: int
    slot: int
    l0: int


MAP_TARGETS = (
    MapTarget("map_b6916_p0", 6916, 0),
    MapTarget("map_b6916_p1", 6916, 1),
    MapTarget("map_b6916_p2", 6916, 2),
    MapTarget("map_b6917_p0", 6917, 0),
    MapTarget("map_b6917_p1", 6917, 1),
    MapTarget("map_b6917_p2", 6917, 2),
    MapTarget("map_b6166_p0", 6166, 0),
    MapTarget("map_b6166_p1", 6166, 1),
    MapTarget("map_b6166_p2", 6166, 2),
    MapTarget("map_b6167_p0", 6167, 0),
    MapTarget("map_b6167_p1", 6167, 1),
    MapTarget("map_b6167_p2", 6167, 2),
    MapTarget("map_b6912_p0", 6912, 0),
)

PAGE_TARGETS = (
    PageTarget("page_j982_v056B", 982, 0x056B, 0, 0, 0),
    PageTarget("page_j61_v0384", 61, 0x0384, 287),
    PageTarget("page_j61_v0194", 61, 0x0194, 0),
    PageTarget("page_j462_v0383", 462, 0x0383, 287),
)

TEST_LBAS = (0, 1, 0x3F, 0xA07C, 0xA07E, 0xA080)
OOB_SCAN_RANGES = (
    (TARGET_LBA - 0x400, TARGET_LBA + 0x400),
    (TARGET_LBA * 2 - 0x800, TARGET_LBA * 2 + 0x1000),
)


def parse_int(value: str) -> int:
    if value.lower().startswith("0x"):
        return int(value, 16)
    if any(c in "abcdefABCDEF" for c in value):
        return int(value, 16)
    return int(value, 10)


def parse_key_values(tokens: list[str]) -> dict[str, str]:
    fields: dict[str, str] = {}
    for token in tokens:
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        fields[key] = value
    return fields


def safe_name(value: str) -> str:
    allowed = set(string.ascii_letters + string.digits + "._-")
    return "".join(ch if ch in allowed else "_" for ch in value)


def decode_physical_page(vblock: int, page: int) -> tuple[int, int, int, int]:
    vblock &= 0x0FFF
    abspage = (vblock + N3G_SYSTEM_HYPERBLOCKS) * N3G_PAGES_PER_HYPERBLOCK + page
    bank = abspage % N3G_BANKS
    pblock = abspage // (N3G_PAGES_PER_BLOCK * N3G_BANKS)
    pageoff = (abspage // N3G_BANKS) % N3G_PAGES_PER_BLOCK
    physpage = pblock * N3G_PAGES_PER_BLOCK + pageoff
    return bank, physpage, pblock, pageoff


def run_windex_read(
    windex: str, bank: int, out: Path, physpage: int, timeout: float
) -> subprocess.CompletedProcess[str]:
    cmd = [windex, "nand", "read", str(bank), str(out), str(physpage), "1"]
    return subprocess.run(
        cmd,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout,
    )


def run_windex_read_extra(
    windex: str, bank: int, out: Path, physpages: list[int], timeout: float
) -> subprocess.CompletedProcess[str]:
    cmd = [windex, "nand", "readextra-pages", str(bank), str(out), *(str(p) for p in physpages)]
    return subprocess.run(
        cmd,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout,
    )


def read_body_oob(
    args: argparse.Namespace, bank: int, physpage: int, body_out: Path, oob_out: Path
) -> tuple[bytes, bytes, int]:
    rc = 0
    with TemporaryDirectory(prefix="n3g-wmount-dfu-dump-") as tmpdir:
        tmp = Path(tmpdir)
        body_tmp = tmp / "page.bin"
        oob_tmp = tmp / "page.oob"
        try:
            proc = run_windex_read(args.windex, bank, body_tmp, physpage, args.read_timeout)
            if not body_tmp.exists():
                print(
                    "N3GD_DUMP_FAIL reason=body_nofile "
                    f"bank={bank} phy={physpage} "
                    f"stdout={proc.stdout.strip()!r} stderr={proc.stderr.strip()!r}",
                    flush=True,
                )
                body = b""
                rc = 1
            else:
                body = body_tmp.read_bytes()
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
            print(
                f"N3GD_DUMP_FAIL reason=body_{type(exc).__name__} bank={bank} phy={physpage}",
                flush=True,
            )
            body = b""
            rc = 1
        try:
            proc = run_windex_read_extra(args.windex, bank, oob_tmp, [physpage], args.extra_timeout)
            if not oob_tmp.exists():
                print(
                    "N3GD_DUMP_FAIL reason=oob_nofile "
                    f"bank={bank} phy={physpage} "
                    f"stdout={proc.stdout.strip()!r} stderr={proc.stderr.strip()!r}",
                    flush=True,
                )
                oob = b""
                rc = 1
            else:
                oob = oob_tmp.read_bytes()[:N3G_OOB_LEN]
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
            print(
                f"N3GD_DUMP_FAIL reason=oob_{type(exc).__name__} bank={bank} phy={physpage}",
                flush=True,
            )
            oob = b""
            rc = 1

    body_out.write_bytes(body)
    oob_out.write_bytes(oob)
    return body, oob, rc


def oob_type(oob: bytes) -> int:
    return oob[9] if len(oob) > 9 else 0xFF


def read_l0(oob: bytes, fallback: int = 0xFFFFFFFF) -> int:
    return replay.le32(oob, 0) if len(oob) >= 4 else fallback


def read_idx(oob: bytes) -> int:
    return replay.le16(oob, 4) if len(oob) >= 6 else 0xFFFF


def read_usn(oob: bytes) -> int:
    return replay.le32(oob, 0) if len(oob) >= 4 else 0xFFFFFFFF


def dump_map(args: argparse.Namespace, target: MapTarget, manifest: list[str]) -> int:
    physpage = target.block * N3G_PAGES_PER_BLOCK + target.page
    body_file = f"{target.name}.bin"
    oob_file = f"{target.name}.oob"
    body, oob, rc = read_body_oob(
        args, 0, physpage, args.out / body_file, args.out / oob_file
    )
    manifest.append(
        "N3GD_FILE "
        f"target={target.name} kind=map b={target.block} p={target.page} "
        f"j=4294967295 v=ffff po={target.page} sl=4294967295 "
        f"l0=ffffffff bank=0 pb={target.block} pp={target.page} phy={physpage} "
        f"rc={rc} t={oob_type(oob):02X} l={read_l0(oob):08X} "
        f"u={read_usn(oob):08X} ix={read_idx(oob):04X} "
        f"bin={body_file} oob={oob_file}"
    )
    manifest.append(
        f"N3GM_MAP_SEEN b={target.block} p={target.page} "
        f"t={oob_type(oob):02X} u={read_usn(oob):08X} ix={read_idx(oob):04X} rc={rc}"
    )
    print(f"N3GD_WRITE /n3g-dump/{body_file} rc={rc} bytes={len(body)}", flush=True)
    print(f"N3GD_WRITE /n3g-dump/{oob_file} rc={rc} bytes={len(oob)}", flush=True)
    return 2


def capture_map_entry_oob(
    args: argparse.Namespace, target: MapTarget, manifest: list[str]
) -> tuple[list[DecodedEntry], int]:
    body_path = args.out / f"{target.name}.bin"
    if not body_path.exists() or body_path.stat().st_size < 2:
        return [], 0

    body = body_path.read_bytes()
    chunks = [b"\x00" * N3G_OOB_LEN for _ in range(len(body) // 2)]
    grouped: dict[int, list[tuple[int, int, int]]] = {}
    for j in range(len(body) // 2):
        v = replay.le16(body, j * 2)
        if v in (0, 0xFFFF):
            continue
        bank, physpage, _, _ = decode_physical_page(v, 0)
        grouped.setdefault(bank, []).append((j, v, physpage))

    rc = 0
    with TemporaryDirectory(prefix="n3g-map-entry-oob-") as tmpdir:
        tmp = Path(tmpdir)
        for bank, items in sorted(grouped.items()):
            for first in range(0, len(items), args.entry_oob_chunk):
                group = items[first : first + args.entry_oob_chunk]
                out = tmp / f"{target.name}.bank{bank}.{first}.entries.oob"
                try:
                    run_windex_read_extra(
                        args.windex,
                        bank,
                        out,
                        [physpage for _, _, physpage in group],
                        args.extra_timeout,
                    )
                except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
                    rc = 1
                    continue
                if not out.exists():
                    rc = 1
                    continue
                data = out.read_bytes()
                for index, (j, _, _) in enumerate(group):
                    start = index * N3G_OOB_LEN
                    chunks[j] = data[start : start + N3G_OOB_LEN]

    sidecar_name = f"{target.name}.entries.oob"
    sidecar = b"".join(chunks)
    (args.out / sidecar_name).write_bytes(sidecar)

    decoded: list[DecodedEntry] = []
    valid = 0
    for j, oob in enumerate(chunks):
        if len(oob) < N3G_OOB_LEN:
            continue
        v = replay.le16(body, j * 2)
        if v in (0, 0xFFFF):
            continue
        entry_type = oob_type(oob)
        if entry_type not in (0x40, 0x41):
            continue
        l0 = read_l0(oob)
        decoded.append(DecodedEntry(target.name, j, v, l0, entry_type))
        valid += 1
        manifest.append(
            "N3GM_ENTRY "
            f"mapb={target.block} mapp={target.page} j={j} v={v:04X} "
            f"l0={l0:08X} span={N3G_ENTRY_SPAN} t={entry_type:02X} "
            f"po=0 tgt={TARGET_LBA:08X} d=4294967295"
        )

    manifest.append(
        f"N3GM_ENTRIES target={target.name} valid={valid} file={sidecar_name} rc={rc}"
    )
    print(f"N3GD_WRITE /n3g-dump/{sidecar_name} rc={rc} bytes={len(sidecar)}", flush=True)
    return decoded, 0 if rc == 0 else 1


def dump_physical_page(
    args: argparse.Namespace,
    name: str,
    j: int,
    v: int,
    po: int,
    slot: int,
    l0: int,
    manifest: list[str],
) -> int:
    bank, physpage, pblock, pageoff = decode_physical_page(v, po)
    body_file = f"{name}.bin"
    oob_file = f"{name}.oob"
    body, oob, rc = read_body_oob(args, bank, physpage, args.out / body_file, args.out / oob_file)
    manifest.append(
        "N3GD_FILE "
        f"target={name} kind=page j={j} v={v:04X} po={po} sl={slot} "
        f"l0={l0:08X} bank={bank} pb={pblock} pp={pageoff} phy={physpage} "
        f"rc={rc} t={oob_type(oob):02X} l={read_l0(oob):08X} "
        f"bin={body_file} oob={oob_file}"
    )
    print(f"N3GD_WRITE /n3g-dump/{body_file} rc={rc} bytes={len(body)}", flush=True)
    print(f"N3GD_WRITE /n3g-dump/{oob_file} rc={rc} bytes={len(oob)}", flush=True)
    return 1 if rc else 0


def dump_page(args: argparse.Namespace, target: PageTarget, manifest: list[str]) -> int:
    bank, physpage, pblock, pageoff = decode_physical_page(target.v, target.po)
    _, base_physpage, _, _ = decode_physical_page(target.v, 0)
    body_file = f"{target.name}.bin"
    oob_file = f"{target.name}.oob"
    body, oob, rc = read_body_oob(
        args, bank, physpage, args.out / body_file, args.out / oob_file
    )
    base_oob_tmp = args.out / f".{target.name}.base.oob"
    _, base_oob, base_rc = read_body_oob(
        args, bank, base_physpage, args.out / f".{target.name}.base.bin", base_oob_tmp
    )
    try:
        (args.out / f".{target.name}.base.bin").unlink()
        base_oob_tmp.unlink()
    except FileNotFoundError:
        pass
    l0 = target.l0_hint
    if l0 == 0xFFFFFFFF:
        l0 = read_l0(base_oob, read_l0(oob))
    manifest.append(
        "N3GD_FILE "
        f"target={target.name} kind=page j={target.j} v={target.v:04X} "
        f"po={target.po} sl={target.slot} l0={l0:08X} "
        f"bank={bank} pb={pblock} pp={pageoff} phy={physpage} rc={rc} "
        f"t={oob_type(oob):02X} l={read_l0(oob):08X} base_rc={base_rc} "
        f"bin={body_file} oob={oob_file}"
    )
    if oob_type(base_oob) in (0x40, 0x41):
        manifest.append(
            "N3GM_ENTRY "
            f"mapb=0 mapp=0 j={target.j} v={target.v:04X} "
            f"l0={l0:08X} span={N3G_ENTRY_SPAN} t={oob_type(base_oob):02X} "
            f"po=0 tgt=0000A07E d=4294967295"
        )
    print(f"N3GD_WRITE /n3g-dump/{body_file} rc={rc} bytes={len(body)}", flush=True)
    print(f"N3GD_WRITE /n3g-dump/{oob_file} rc={rc} bytes={len(oob)}", flush=True)
    return 2


def should_dump_entry(entry: DecodedEntry) -> bool:
    end = entry.l0 + N3G_ENTRY_SPAN - 1
    return (
        entry.l0 <= TARGET_LBA <= end
        or (entry.l0 <= NEAR_HI and end >= NEAR_LO)
    )


def dump_covering_entries(
    args: argparse.Namespace, entries: list[DecodedEntry], manifest: list[str]
) -> tuple[int, int]:
    seen: set[tuple[int, int, int]] = set()
    failures = 0
    files = 0
    for entry in entries:
        if not should_dump_entry(entry):
            continue
        delta = TARGET_LBA - entry.l0 if entry.l0 <= TARGET_LBA else 0
        po = max(0, min(N3G_PAGES_PER_HYPERBLOCK - 1, delta // 4))
        key = (entry.v, po, entry.j)
        if key in seen:
            continue
        seen.add(key)
        name = f"cand_{entry.map_name}_j{entry.j}_v{entry.v:04X}_po{po}"
        failures += dump_physical_page(
            args,
            name,
            entry.j,
            entry.v,
            po,
            delta & 3 if entry.l0 <= TARGET_LBA else 0xFFFFFFFF,
            entry.l0,
            manifest,
        )
        files += 2
    return files, failures


def write_manifest(args: argparse.Namespace, manifest: list[str]) -> None:
    text = "\n".join(manifest) + "\n"
    path = args.out / "n3g_wmount_manifest.txt"
    path.write_text(text, encoding="utf-8")
    compat = args.out / "manifest.txt"
    compat.write_text(text, encoding="utf-8")
    print(f"N3GD_WRITE /n3g-dump/manifest.txt rc=0 bytes={len(text.encode('utf-8'))}", flush=True)


def parse_missing_targets(path: Path) -> list[MissingTarget]:
    targets: list[MissingTarget] = []
    seen: set[tuple[int, int, int, int]] = set()
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        fields = parse_key_values(line.split())
        try:
            j = int(fields["j"], 0)
            v = int(fields["v"], 16)
            po = int(fields["po"], 0)
            slot = int(fields.get("sl", "4294967295"), 0)
            l0 = int(fields.get("l0", "ffffffff"), 16)
        except (KeyError, ValueError):
            print(f"N3GD_DUMP_FAIL reason=bad_missing_line line={line!r}", flush=True)
            continue
        key = (j, v, po, l0)
        if key in seen:
            continue
        seen.add(key)
        default_name = f"miss_j{j}_v{v:04X}_po{po}"
        name = safe_name(fields.get("target", default_name))
        targets.append(MissingTarget(name, j, v, po, slot, l0))
    return targets


def existing_manifest(args: argparse.Namespace) -> list[str]:
    path = args.out / "n3g_wmount_manifest.txt"
    if not path.exists():
        return [
            "N3GD_START profile=winpod_mbr_fat32 target=0000A07E ppb=512",
            "N3GM_PART p=0 t=0C st=0000A07E sz=000E7F81",
            "N3GM_MBR j=982 v=056B po=0 l0=00000000",
        ]
    return path.read_text(encoding="utf-8", errors="replace").splitlines()


def existing_targets(manifest: list[str]) -> set[str]:
    targets: set[str] = set()
    for raw in manifest:
        line = raw.strip()
        if not line.startswith("N3GD_FILE "):
            continue
        fields = parse_key_values(line.split()[1:])
        target = fields.get("target")
        if target:
            targets.add(target)
    return targets


def dump_missing(args: argparse.Namespace) -> int:
    args.out.mkdir(parents=True, exist_ok=True)
    manifest = existing_manifest(args)
    have = existing_targets(manifest)
    targets = parse_missing_targets(args.add_missing)
    failures = 0
    files = 0
    print("N3GD_DUMP_START", flush=True)
    manifest.append(f"N3GD_ADD_MISSING file={args.add_missing} count={len(targets)}")
    for target in targets:
        if target.name in have and (args.out / f"{target.name}.bin").exists() and (args.out / f"{target.name}.oob").exists():
            print(f"N3GD_WRITE /n3g-dump/{target.name}.bin rc=0 bytes=skip", flush=True)
            continue
        failures += dump_physical_page(
            args,
            target.name,
            target.j,
            target.v,
            target.po,
            target.slot,
            target.l0,
            manifest,
        )
        files += 2
    write_manifest(args, manifest)
    files += 1
    if failures:
        print(f"N3GD_DUMP_FAIL reason=read_failures count={failures}", flush=True)
        return 1
    print(f"N3GD_DUMP_DONE files={files}", flush=True)
    return 0


def oob_scan_hit(args: argparse.Namespace, logical: int) -> bool:
    if args.scan_l_min is not None and args.scan_l_max is not None:
        return args.scan_l_min <= logical <= args.scan_l_max
    return any(lo <= logical <= hi for lo, hi in OOB_SCAN_RANGES)


def scan_vblock_oob(args: argparse.Namespace) -> int:
    args.out.mkdir(parents=True, exist_ok=True)
    manifest = existing_manifest(args)
    failures = 0
    files = 0
    hits = 0
    print("N3GD_DUMP_START", flush=True)
    for raw_v in args.scan_vblock:
        v = int(raw_v, 0) if raw_v.lower().startswith("0x") else int(raw_v, 16)
        manifest.append(f"N3GD_SCAN_VBLOCK v={v:04X}")
        grouped: dict[int, list[tuple[int, int]]] = {}
        for po in range(N3G_PAGES_PER_HYPERBLOCK):
            bank, physpage, _, _ = decode_physical_page(v, po)
            grouped.setdefault(bank, []).append((po, physpage))
        for bank, items in sorted(grouped.items()):
            with TemporaryDirectory(prefix=f"n3g-vblock-oob-{v:04X}-") as tmpdir:
                tmp = Path(tmpdir)
                for first in range(0, len(items), args.entry_oob_chunk):
                    group = items[first : first + args.entry_oob_chunk]
                    out = tmp / f"v{v:04X}.bank{bank}.{first}.oob"
                    try:
                        run_windex_read_extra(
                            args.windex,
                            bank,
                            out,
                            [physpage for _, physpage in group],
                            args.extra_timeout,
                        )
                    except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
                        failures += 1
                        continue
                    if not out.exists():
                        failures += 1
                        continue
                    data = out.read_bytes()
                    for index, (po, _) in enumerate(group):
                        oob = data[index * N3G_OOB_LEN : (index + 1) * N3G_OOB_LEN]
                        if len(oob) < N3G_OOB_LEN:
                            continue
                        t = oob_type(oob)
                        logical = read_l0(oob)
                        if t not in (0x40, 0x41) or not oob_scan_hit(args, logical):
                            continue
                        name = f"scan_v{v:04X}_po{po}_l{logical:08X}"
                        print(
                            f"N3GD_SCAN_HIT v={v:04X} po={po} t={t:02X} l={logical:08X}",
                            flush=True,
                        )
                        failures += dump_physical_page(
                            args,
                            name,
                            0xFFFFFFFF,
                            v,
                            po,
                            0xFFFFFFFF,
                            logical,
                            manifest,
                        )
                        files += 2
                        hits += 1
                        if args.scan_max_hits and hits >= args.scan_max_hits:
                            break
                    if args.scan_max_hits and hits >= args.scan_max_hits:
                        break
                if args.scan_max_hits and hits >= args.scan_max_hits:
                    break
            if args.scan_max_hits and hits >= args.scan_max_hits:
                break
        if args.scan_max_hits and hits >= args.scan_max_hits:
            break
    manifest.append(f"N3GD_SCAN_DONE hits={hits} failures={failures}")
    write_manifest(args, manifest)
    files += 1
    if failures:
        print(f"N3GD_DUMP_FAIL reason=scan_failures count={failures}", flush=True)
        return 1
    print(f"N3GD_DUMP_DONE files={files} hits={hits}", flush=True)
    return 0


def scan_physical_oob(args: argparse.Namespace) -> int:
    args.out.mkdir(parents=True, exist_ok=True)
    manifest = existing_manifest(args)
    failures = 0
    files = 0
    hits = 0
    banks = args.scan_bank if args.scan_bank else list(range(N3G_BANKS))
    print("N3GD_DUMP_START", flush=True)
    manifest.append(
        f"N3GD_SCAN_PHYS pb_min={args.scan_pblock_min} pb_max={args.scan_pblock_max} "
        f"banks={','.join(str(bank) for bank in banks)}"
    )
    for bank in banks:
        if args.scan_page_offset is None:
            page_offsets = range(N3G_PAGES_PER_BLOCK)
        else:
            if not (0 <= args.scan_page_offset < N3G_PAGES_PER_BLOCK):
                raise ValueError("--scan-page-offset must be in 0..127")
            page_offsets = (args.scan_page_offset,)
        pages = [
            pb * N3G_PAGES_PER_BLOCK + pp
            for pb in range(args.scan_pblock_min, args.scan_pblock_max + 1)
            for pp in page_offsets
        ]
        with TemporaryDirectory(prefix=f"n3g-phys-oob-b{bank}-") as tmpdir:
            tmp = Path(tmpdir)
            chunks_done = 0
            for first in range(0, len(pages), args.entry_oob_chunk):
                group = pages[first : first + args.entry_oob_chunk]
                first_pb = group[0] // N3G_PAGES_PER_BLOCK
                first_pp = group[0] % N3G_PAGES_PER_BLOCK
                if args.scan_progress_chunks and (chunks_done % args.scan_progress_chunks) == 0:
                    print(
                        f"N3GD_PSCAN_PROG bank={bank} chunk={chunks_done} pb={first_pb} pp={first_pp}",
                        flush=True,
                    )
                out = tmp / f"bank{bank}.{first}.oob"
                try:
                    run_windex_read_extra(
                        args.windex,
                        bank,
                        out,
                        group,
                        args.extra_timeout,
                    )
                except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
                    failures += 1
                    continue
                if not out.exists():
                    failures += 1
                    continue
                data = out.read_bytes()
                for index, physpage in enumerate(group):
                    oob = data[index * N3G_OOB_LEN : (index + 1) * N3G_OOB_LEN]
                    if len(oob) < N3G_OOB_LEN:
                        continue
                    t = oob_type(oob)
                    logical = read_l0(oob)
                    if t not in (0x40, 0x41) or not oob_scan_hit(args, logical):
                        continue
                    pb = physpage // N3G_PAGES_PER_BLOCK
                    pp = physpage % N3G_PAGES_PER_BLOCK
                    name = f"pscan_bank{bank}_pb{pb}_pp{pp}_l{logical:08X}"
                    print(
                        f"N3GD_PSCAN_HIT bank={bank} pb={pb} pp={pp} t={t:02X} l={logical:08X}",
                        flush=True,
                    )
                    if args.scan_oob_only:
                        manifest.append(
                            "N3GD_PSCAN_OOB "
                            f"bank={bank} pb={pb} pp={pp} phy={physpage} "
                            f"t={t:02X} l={logical:08X}"
                        )
                        hits += 1
                        if args.scan_max_hits and hits >= args.scan_max_hits:
                            break
                        continue
                    body_file = f"{name}.bin"
                    oob_file = f"{name}.oob"
                    body, full_oob, rc = read_body_oob(
                        args, bank, physpage, args.out / body_file, args.out / oob_file
                    )
                    failures += 1 if rc else 0
                    manifest.append(
                        "N3GD_FILE "
                        f"target={name} kind=page j=4294967295 v=ffff po={pp} "
                        f"sl=4294967295 l0={logical:08X} bank={bank} pb={pb} pp={pp} "
                        f"phy={physpage} rc={rc} t={oob_type(full_oob):02X} "
                        f"l={read_l0(full_oob):08X} bin={body_file} oob={oob_file}"
                    )
                    print(f"N3GD_WRITE /n3g-dump/{body_file} rc={rc} bytes={len(body)}", flush=True)
                    print(f"N3GD_WRITE /n3g-dump/{oob_file} rc={rc} bytes={len(full_oob)}", flush=True)
                    files += 2
                    hits += 1
                    if args.scan_max_hits and hits >= args.scan_max_hits:
                        break
                if args.scan_max_hits and hits >= args.scan_max_hits:
                    break
                chunks_done += 1
                if args.scan_max_chunks and chunks_done >= args.scan_max_chunks:
                    print(f"N3GD_PSCAN_STOP bank={bank} chunks={chunks_done}", flush=True)
                    break
        if args.scan_max_chunks and chunks_done >= args.scan_max_chunks:
            break
        if args.scan_max_hits and hits >= args.scan_max_hits:
            break
    manifest.append(f"N3GD_PSCAN_DONE hits={hits} failures={failures}")
    write_manifest(args, manifest)
    files += 1
    if failures:
        print(f"N3GD_DUMP_FAIL reason=pscan_failures count={failures}", flush=True)
        return 1
    print(f"N3GD_DUMP_DONE files={files} hits={hits}", flush=True)
    return 0


def dump(args: argparse.Namespace) -> int:
    args.out.mkdir(parents=True, exist_ok=True)
    failures = 0
    manifest: list[str] = [
        "N3GD_START profile=winpod_mbr_fat32 target=0000A07E ppb=512",
        "N3GM_PART p=0 t=0C st=0000A07E sz=000E7F81",
        "N3GM_MBR j=982 v=056B po=0 l0=00000000",
        "N3GM_SELECTED_MAP b=6916 p=0",
        "N3GM_SELECTED_MAP b=6166 p=0",
        "N3GM_SELECTED_MAP b=6912 p=0",
    ]
    for lba in TEST_LBAS:
        manifest.append(f"N3GM_TEST_LBA lba={lba:08X}")

    print("N3GD_DUMP_START", flush=True)
    files = 0
    decoded_entries: list[DecodedEntry] = []
    for target in MAP_TARGETS:
        before = len(manifest)
        files += dump_map(args, target, manifest)
        if " rc=1" in manifest[before]:
            failures += 1
        entries, rc = capture_map_entry_oob(args, target, manifest)
        decoded_entries.extend(entries)
        files += 1
        failures += rc
    for target in PAGE_TARGETS:
        before = len(manifest)
        files += dump_page(args, target, manifest)
        if " rc=1" in manifest[before]:
            failures += 1
    extra_files, extra_failures = dump_covering_entries(args, decoded_entries, manifest)
    files += extra_files
    failures += extra_failures
    manifest.append(f"N3GD_DONE pages={len(MAP_TARGETS) + len(PAGE_TARGETS)}")
    write_manifest(args, manifest)
    files += 1
    if failures:
        print(f"N3GD_DUMP_FAIL reason=read_failures count={failures}", flush=True)
        return 1
    print(f"N3GD_DUMP_DONE files={files}", flush=True)
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--windex", default="tmp/wInd3x-src/wInd3x")
    parser.add_argument("--out", type=Path, default=Path("tmp/n3g-dump"))
    parser.add_argument("--read-timeout", type=float, default=8.0)
    parser.add_argument("--extra-timeout", type=float, default=30.0)
    parser.add_argument("--entry-oob-chunk", type=int, default=64)
    parser.add_argument("--add-missing", type=Path)
    parser.add_argument("--scan-vblock", action="append", default=[])
    parser.add_argument("--scan-v-min", type=lambda value: int(value, 0))
    parser.add_argument("--scan-v-max", type=lambda value: int(value, 0))
    parser.add_argument("--scan-pblock-min", type=lambda value: int(value, 0))
    parser.add_argument("--scan-pblock-max", type=lambda value: int(value, 0))
    parser.add_argument("--scan-bank", type=int, action="append", default=[])
    parser.add_argument("--scan-l-min", type=lambda value: int(value, 0))
    parser.add_argument("--scan-l-max", type=lambda value: int(value, 0))
    parser.add_argument("--scan-max-hits", type=int, default=0)
    parser.add_argument("--scan-max-chunks", type=int, default=0)
    parser.add_argument("--scan-progress-chunks", type=int, default=16)
    parser.add_argument("--scan-oob-only", action="store_true")
    parser.add_argument("--scan-page-offset", type=int)
    args = parser.parse_args(argv)
    if args.scan_v_min is not None or args.scan_v_max is not None:
        if args.scan_v_min is None or args.scan_v_max is None:
            parser.error("--scan-v-min and --scan-v-max must be used together")
        if args.scan_v_max < args.scan_v_min:
            parser.error("--scan-v-max must be >= --scan-v-min")
        args.scan_vblock.extend(f"{v:04X}" for v in range(args.scan_v_min, args.scan_v_max + 1))
    try:
        if args.add_missing is not None:
            return dump_missing(args)
        if args.scan_pblock_min is not None or args.scan_pblock_max is not None:
            if args.scan_pblock_min is None or args.scan_pblock_max is None:
                parser.error("--scan-pblock-min and --scan-pblock-max must be used together")
            if args.scan_pblock_max < args.scan_pblock_min:
                parser.error("--scan-pblock-max must be >= --scan-pblock-min")
            return scan_physical_oob(args)
        if args.scan_vblock:
            return scan_vblock_oob(args)
        return dump(args)
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
        print(f"N3GD_DUMP_FAIL reason={type(exc).__name__}", flush=True)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
