#!/usr/bin/env python3
"""DFU-side BPB scanner for Nano 3G Whimory map entries.

The scanner uses a captured t=44 map page body plus page-0 OOB/extras for each
map entry, then asks wInd3x to read selected physical pages from DFU. It prints
only sectors with a boot-sector-like signal: AA55, sane bytes-per-sector, or a
FAT/FAT32 string.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory

import nano3g_ftl_replay as replay


N3G_BANKS = 4
N3G_PAGES_PER_BLOCK = 128
N3G_PAGES_PER_HYPERBLOCK = N3G_BANKS * N3G_PAGES_PER_BLOCK
N3G_SYSTEM_HYPERBLOCKS = 425
N3G_OOB_LEN = 64


@dataclass
class MapEntry:
    j: int
    v: int
    l0: int
    oob_type: int

    @property
    def valid(self) -> bool:
        return self.v not in (0, 0xFFFF) and self.oob_type in (0x40, 0x41)


@dataclass
class PhysicalPage:
    j: int
    v: int
    po: int
    l0: int
    bank: int
    physpage: int
    pblock: int
    pageoff: int


@dataclass
class MetaPage:
    block: int
    page: int
    oob_type: int
    idx: int
    usn: int


def decode_physical_page(vblock: int, page: int) -> tuple[int, int, int, int]:
    abspage = (vblock + N3G_SYSTEM_HYPERBLOCKS) * N3G_PAGES_PER_HYPERBLOCK + page
    bank = abspage % N3G_BANKS
    pblock = abspage // (N3G_PAGES_PER_BLOCK * N3G_BANKS)
    pageoff = (abspage // N3G_BANKS) % N3G_PAGES_PER_BLOCK
    physpage = pblock * N3G_PAGES_PER_BLOCK + pageoff
    return bank, physpage, pblock, pageoff


def load_map_entries(map_body: Path, map_extra: Path) -> list[MapEntry]:
    body = map_body.read_bytes()
    extra = map_extra.read_bytes()
    entries: list[MapEntry] = []
    count = min(len(body) // 2, len(extra) // N3G_OOB_LEN)
    for j in range(count):
        v = replay.le16(body, j * 2)
        oob = extra[j * N3G_OOB_LEN : (j + 1) * N3G_OOB_LEN]
        l0 = replay.le32(oob, 0)
        oob_type = oob[9] if len(oob) > 9 else 0
        entries.append(MapEntry(j, v, l0, oob_type))
    return entries


def load_body_map_entries(map_body: Path, logical_base: int) -> list[MapEntry]:
    body = map_body.read_bytes()
    entries: list[MapEntry] = []
    for j in range(len(body) // 2):
        v = replay.le16(body, j * 2)
        if v == 0xFFFF:
            oob_type = 0xFF
        elif v == 0:
            oob_type = 0
        else:
            oob_type = 0x40
        entries.append(MapEntry(j, v, logical_base + j, oob_type))
    return entries


def oob_meta_fields(oob: bytes) -> tuple[int, int, int]:
    # Whimory meta OOB layout observed on Nano 3G: usn at dword 0,
    # 16-bit index at bytes 4..5, type marker at byte 9.
    idx = replay.le16(oob, 4)
    usn = replay.le32(oob, 0)
    oob_type = oob[9] if len(oob) > 9 else 0
    return oob_type, idx, usn


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


def write_dump_chunks(handle, prefix: str, page_id: int, data: bytes) -> None:
    for off in range(0, len(data), 16):
        handle.write(f"{prefix} id={page_id} off={off:03x} {data[off:off + 16].hex()}\n")


def should_dump_page(args: argparse.Namespace, probe: replay.BootProbe | None, mbr_candidate: bool) -> bool:
    if args.dump_all_pages:
        return True
    if mbr_candidate:
        return True
    if probe is None:
        return False
    bpb = probe.bpb
    return bpb.sig == 0xAA55 or replay.is_sane_bps(bpb.bps) or bool(bpb.fat_string)


def dump_page_log(
    args: argparse.Namespace,
    handle,
    page_id: int,
    page: PhysicalPage,
    body: bytes,
    oob: bytes,
    probe: replay.BootProbe | None,
    mbr_candidate: bool,
) -> None:
    oob_type, idx, usn = oob_meta_fields(oob) if len(oob) >= N3G_OOB_LEN else (0, 0xFFFF, 0)
    logical = replay.le32(oob, 0) if len(oob) >= 4 else 0xFFFFFFFF
    page_l0 = page.l0 if page.l0 != 0xFFFFFFFF else logical
    shift = probe.shift if probe is not None else 0xFFFFFFFF
    slice_index = probe.slice_index if probe is not None else 0xFFFFFFFF
    handle.write(
        "N3GD_BEGIN "
        f"id={page_id} kind={'mbr' if mbr_candidate else 'cand'} "
        f"j={page.j} v={page.v:04X} po={page.po} sl={slice_index} "
        f"l0={page_l0:08X} host={args.target_host_lba:08X} "
        f"tgt={args.target_raw_lba:08X} bank={page.bank} "
        f"pb={page.pblock} pp={page.pageoff} phy={page.physpage} "
        f"shift={shift} t={oob_type:02X} ix={idx:04X} u={usn:08X} l={logical:08X}\n"
    )
    write_dump_chunks(handle, "N3GD_BODY", page_id, body)
    write_dump_chunks(handle, "N3GD_OOB", page_id, oob[:N3G_OOB_LEN])
    handle.write(f"N3GD_END id={page_id}\n")


def capture_map_extra(args: argparse.Namespace) -> int:
    body = args.map_body.read_bytes()
    chunks: list[bytes] = [b"\x00" * N3G_OOB_LEN for _ in range(len(body) // 2)]
    grouped: dict[int, list[tuple[int, int]]] = {}
    for j in range(len(body) // 2):
        v = replay.le16(body, j * 2)
        if v in (0, 0xFFFF):
            continue
        bank, physpage, _, _ = decode_physical_page(v, 0)
        grouped.setdefault(bank, []).append((j, physpage))

    with TemporaryDirectory(prefix="n3g-map-extra-") as tmpdir:
        tmp = Path(tmpdir)
        for bank, items in sorted(grouped.items()):
            out = tmp / f"bank{bank}.extra"
            cmd = [
                args.windex,
                "nand",
                "readextra-pages",
                str(bank),
                str(out),
                *(str(physpage) for _, physpage in items),
            ]
            print(
                f"N3G_DFU_EXTRA_READ bank={bank} pages={len(items)} out={args.capture_map_extra}",
                flush=True,
            )
            subprocess.run(
                cmd,
                check=True,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=args.extra_timeout,
            )
            data = out.read_bytes()
            for index, (j, _) in enumerate(items):
                start = index * N3G_OOB_LEN
                chunks[j] = data[start : start + N3G_OOB_LEN]

    args.capture_map_extra.parent.mkdir(parents=True, exist_ok=True)
    args.capture_map_extra.write_bytes(b"".join(chunks))
    print(
        f"N3G_DFU_EXTRA_DONE entries={len(chunks)} file={args.capture_map_extra}",
        flush=True,
    )
    return 0


def capture_meta_pages(args: argparse.Namespace) -> int:
    candidates: list[tuple[int, int, int]] = []
    for block in range(args.meta_start_block, args.meta_end_block + 1):
        for page in range(args.meta_start_page, args.meta_end_page + 1):
            candidates.append((block, page, block * N3G_PAGES_PER_BLOCK + page))

    with TemporaryDirectory(prefix="n3g-meta-") as tmpdir:
        tmp = Path(tmpdir)
        extra_file = tmp / "meta.extra"
        print(
            f"N3G_DFU_META_EXTRA start={args.meta_start_block} end={args.meta_end_block} "
            f"pages={len(candidates)}",
            flush=True,
        )
        run_windex_read_extra(
            args.windex,
            0,
            extra_file,
            [physpage for _, _, physpage in candidates],
            args.extra_timeout,
        )
        extra = extra_file.read_bytes()
        found: list[MetaPage] = []
        for index, (block, page, _) in enumerate(candidates):
            oob = extra[index * N3G_OOB_LEN : (index + 1) * N3G_OOB_LEN]
            oob_type, idx, usn = oob_meta_fields(oob)
            if args.meta_types and oob_type not in args.meta_types:
                continue
            found.append(MetaPage(block, page, oob_type, idx, usn))

        args.meta_out.mkdir(parents=True, exist_ok=True)
        for meta in found:
            physpage = meta.block * N3G_PAGES_PER_BLOCK + meta.page
            stem = f"meta-b{meta.block}-p{meta.page}-t{meta.oob_type:02x}-i{meta.idx:04x}-u{meta.usn:08x}"
            body_path = args.meta_out / f"{stem}.body.bin"
            oob_path = args.meta_out / f"{stem}.oob.bin"
            try:
                run_windex_read(args.windex, 0, body_path, physpage, args.read_timeout)
            except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
                print(
                    f"N3G_DFU_META_BODY_FAIL b={meta.block} p={meta.page} "
                    f"t={meta.oob_type:02X} err={type(exc).__name__}",
                    flush=True,
                )
                continue
            oob = extra[candidates.index((meta.block, meta.page, physpage)) * N3G_OOB_LEN :]
            oob_path.write_bytes(oob[:N3G_OOB_LEN])
            print(
                f"N3G_DFU_META_PAGE b={meta.block} p={meta.page} "
                f"t={meta.oob_type:02X} idx={meta.idx:04X} u={meta.usn:08X} "
                f"body={body_path}",
                flush=True,
            )

    print(f"N3G_DFU_META_DONE count={len(found)} out={args.meta_out}", flush=True)
    return 0


def scan_meta_pages(args: argparse.Namespace) -> int:
    total = 0
    hits = 0
    current: list[tuple[int, int, int]] = []

    def flush_chunk() -> None:
        nonlocal total, hits, current
        if not current:
            return
        with TemporaryDirectory(prefix="n3g-meta-scan-") as tmpdir:
            out = Path(tmpdir) / "chunk.extra"
            try:
                run_windex_read_extra(
                    args.windex,
                    0,
                    out,
                    [physpage for _, _, physpage in current],
                    args.extra_timeout,
                )
            except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
                print(
                    f"N3G_DFU_MSCAN_FAIL first_b={current[0][0]} n={len(current)} "
                    f"err={type(exc).__name__}",
                    flush=True,
                )
                current = []
                return
            data = out.read_bytes()
            for index, (block, page, _) in enumerate(current):
                oob = data[index * N3G_OOB_LEN : (index + 1) * N3G_OOB_LEN]
                if len(oob) < N3G_OOB_LEN:
                    continue
                oob_type, idx, usn = oob_meta_fields(oob)
                total += 1
                if oob_type in args.meta_types:
                    hits += 1
                    print(
                        f"N3G_DFU_MSCAN_HIT b={block} p={page} "
                        f"t={oob_type:02X} idx={idx:04X} u={usn:08X}",
                        flush=True,
                    )
        current = []

    print(
        f"N3G_DFU_MSCAN_START b0={args.meta_start_block} b1={args.meta_end_block} "
        f"p0={args.meta_start_page} p1={args.meta_end_page}",
        flush=True,
    )
    for block in range(args.meta_start_block, args.meta_end_block + 1):
        if block != args.meta_start_block and (block - args.meta_start_block) % args.meta_progress == 0:
            print(f"N3G_DFU_MSCAN_PROGRESS b={block} hits={hits}", flush=True)
        for page in range(args.meta_start_page, args.meta_end_page + 1):
            current.append((block, page, block * N3G_PAGES_PER_BLOCK + page))
            if len(current) >= args.meta_chunk_pages:
                flush_chunk()
    flush_chunk()
    print(f"N3G_DFU_MSCAN_DONE total={total} hits={hits}", flush=True)
    return 0


def scan_oob_target(args: argparse.Namespace) -> int:
    total = 0
    hits = 0
    best_delta = 0xFFFFFFFF
    best_physpage = 0xFFFFFFFF
    best_logical = 0xFFFFFFFF
    failures = 0
    current = args.oob_start_page
    target = args.target_raw_lba
    end = args.oob_end_page
    print(
        "N3G_DFU_OOB_TARGET_START "
        f"bank={args.oob_bank} start={current} end={end} "
        f"host={args.target_host_lba:08X} raw={target:08X} win={args.target_window}",
        flush=True,
    )

    with TemporaryDirectory(prefix="n3g-oob-target-") as tmpdir:
        tmp = Path(tmpdir)
        while current <= end:
            chunk_end = min(end, current + args.oob_chunk_pages - 1)
            physpages = list(range(current, chunk_end + 1, args.oob_stride))
            out = tmp / f"oob-{current}.extra"
            if (
                current == args.oob_start_page
                or current % args.oob_progress < args.oob_chunk_pages
            ):
                print(
                    "N3G_DFU_OOB_TARGET_CHUNK "
                    f"first={current} last={chunk_end} hits={hits} "
                    f"best_phy={best_physpage} best_l={best_logical:08X} "
                    f"best_delta={best_delta}",
                    flush=True,
                )
            try:
                proc = run_windex_read_extra(args.windex, args.oob_bank, out, physpages, args.extra_timeout)
            except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
                print(
                    f"N3G_DFU_OOB_TARGET_FAIL first={current} n={len(physpages)} "
                    f"err={type(exc).__name__}",
                    flush=True,
                )
                failures += 1
                if failures >= args.oob_max_failures:
                    print(
                        f"N3G_DFU_OOB_TARGET_ABORT failures={failures} reason=read_fail",
                        flush=True,
                    )
                    return 1
                current = chunk_end + 1
                continue
            if not out.exists():
                print(
                    "N3G_DFU_OOB_TARGET_NOFILE "
                    f"first={current} n={len(physpages)} "
                    f"stdout={proc.stdout.strip()!r} stderr={proc.stderr.strip()!r}",
                    flush=True,
                )
                failures += 1
                if failures >= args.oob_max_failures:
                    print(
                        f"N3G_DFU_OOB_TARGET_ABORT failures={failures} reason=no_file",
                        flush=True,
                    )
                    return 1
                current = chunk_end + 1
                continue
            data = out.read_bytes()
            for index, physpage in enumerate(physpages):
                oob = data[index * N3G_OOB_LEN : (index + 1) * N3G_OOB_LEN]
                if len(oob) < N3G_OOB_LEN:
                    continue
                total += 1
                oob_type, idx, _ = oob_meta_fields(oob)
                logical = replay.le32(oob, 0)
                if oob_type not in (0x40, 0x41):
                    continue
                delta = abs(logical - target)
                if delta < best_delta:
                    best_delta = delta
                    best_physpage = physpage
                    best_logical = logical
                if delta > args.target_window:
                    continue
                hits += 1
                pblock = physpage // N3G_PAGES_PER_BLOCK
                pageoff = physpage % N3G_PAGES_PER_BLOCK
                print(
                    "N3G_DFU_OOB_TARGET_HIT "
                    f"phy={physpage} pb={pblock} pp={pageoff} t={oob_type:02X} "
                    f"l={logical:08X} delta={delta} ix={idx:04X}",
                    flush=True,
                )
                body_path = tmp / f"hit-{physpage}.bin"
                try:
                    run_windex_read(args.windex, args.oob_bank, body_path, physpage, args.read_timeout)
                    body = body_path.read_bytes()
                except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
                    body = b""
                if len(body) >= 0x800:
                    page = PhysicalPage(-1, 0xFFFF, pageoff, logical, args.oob_bank, physpage, pblock, pageoff)
                    probe = page_best_probe(page, body, args.part_size)
                    if probe is not None:
                        bpb = probe.bpb
                        print(
                            "N3G_DFU_OOB_TARGET_BODY "
                            f"phy={physpage} slice={probe.slice_index} shift={probe.shift} "
                            f"sig={bpb.sig:04X} bps={bpb.bps} spc={bpb.spc} "
                            f"rs={bpb.reserved} nf={bpb.nfats} fs={bpb.fat_string!r} "
                            f"r={bpb.reason:03X}",
                            flush=True,
                        )
                        if bpb.valid:
                            print(f"N3G_DFU_OOB_TARGET_BOOT_OK phy={physpage}", flush=True)
                            return 0
            if total and total % args.oob_progress < len(physpages):
                print(
                    "N3G_DFU_OOB_TARGET_PROGRESS "
                    f"read={total} hits={hits} best_phy={best_physpage} "
                    f"best_l={best_logical:08X} best_delta={best_delta}",
                    flush=True,
                )
            current = chunk_end + 1

    print(
        "N3G_DFU_OOB_TARGET_DONE "
        f"read={total} hits={hits} best_phy={best_physpage} "
        f"best_l={best_logical:08X} best_delta={best_delta}",
        flush=True,
    )
    return 0 if hits else 1


def candidate_offsets(entry: MapEntry, target_lba: int, ppb: int, sweep: bool) -> list[int]:
    offsets: set[int] = set()
    if entry.l0 <= target_lba <= entry.l0 + ppb - 1:
        delta = target_lba - entry.l0
        for po in range(max(0, delta - 4), min(ppb - 1, delta + 4) + 1):
            offsets.add(po)
    if sweep:
        offsets.update((0, 1, 2, 3, 4, 63, 64, 65, 126, 127, 128, 255, 256, 287, 288, 289, 511))
    return sorted(offsets)


def candidate_offsets_for_span(
    entry: MapEntry,
    target_lba: int,
    ppb: int,
    span_pages: int,
    sweep: bool,
    logical_units_per_page: int,
) -> list[int]:
    offsets: set[int] = set()
    if entry.l0 <= target_lba <= entry.l0 + span_pages - 1:
        delta = target_lba - entry.l0
        logical_units_per_page = max(1, logical_units_per_page)
        logical_po = delta // logical_units_per_page
        models = {
            logical_po,
            delta // N3G_BANKS,
            (delta // N3G_BANKS) * N3G_BANKS,
            delta & (ppb - 1),
            (delta // 2) & (ppb - 1),
            (delta // 4) & (ppb - 1),
        }
        for center in models:
            if center < 0:
                continue
            for po in range(max(0, center - 4), min(ppb - 1, center + 4) + 1):
                offsets.add(po)
    if sweep:
        offsets.update((0, 1, 2, 3, 4, 63, 64, 65, 126, 127, 128, 255, 256, 287, 288, 289, 511))
    return sorted(offsets)


def build_scan_pages(
    entries: list[MapEntry],
    target_lba: int,
    window: int,
    max_pages: int,
    ppb: int,
    sweep_offsets: bool,
    span_pages: int,
    logical_units_per_page: int,
) -> list[PhysicalPage]:
    selected: list[PhysicalPage] = []
    if max_pages <= 0:
        return selected
    if span_pages <= 0:
        span_pages = ppb * max(1, logical_units_per_page)
    valid = [entry for entry in entries if entry.valid]
    valid.sort(
        key=lambda e: 0
        if e.l0 <= target_lba <= e.l0 + span_pages - 1
        else abs(e.l0 - target_lba)
    )
    for entry in valid:
        if abs(entry.l0 - target_lba) > window and not (
            entry.l0 <= target_lba <= entry.l0 + span_pages - 1
        ):
            continue
        for po in candidate_offsets_for_span(
            entry,
            target_lba,
            ppb,
            span_pages,
            sweep_offsets,
            logical_units_per_page,
        ):
            bank, physpage, pblock, pageoff = decode_physical_page(entry.v, po)
            selected.append(PhysicalPage(entry.j, entry.v, po, entry.l0, bank, physpage, pblock, pageoff))
            if len(selected) >= max_pages:
                return selected
    return selected


def build_content_search_pages(
    entries: list[MapEntry],
    max_pages: int,
    offsets: list[int],
    start_j: int,
    end_j: int,
    only_j: set[int],
) -> list[PhysicalPage]:
    selected: list[PhysicalPage] = []
    if max_pages <= 0:
        return selected
    for entry in entries:
        if not entry.valid:
            continue
        if only_j and entry.j not in only_j:
            continue
        if entry.j < start_j or entry.j >= end_j:
            continue
        for po in offsets:
            bank, physpage, pblock, pageoff = decode_physical_page(entry.v, po)
            selected.append(PhysicalPage(entry.j, entry.v, po, entry.l0, bank, physpage, pblock, pageoff))
            if len(selected) >= max_pages:
                return selected
    return selected


def build_direct_vblock_pages(vblocks: list[int], max_pages: int, offsets: list[int]) -> list[PhysicalPage]:
    selected: list[PhysicalPage] = []
    if max_pages <= 0:
        return selected
    for vblock in vblocks:
        for po in offsets:
            bank, physpage, pblock, pageoff = decode_physical_page(vblock, po)
            selected.append(PhysicalPage(-1, vblock, po, 0xFFFFFFFF, bank, physpage, pblock, pageoff))
            if len(selected) >= max_pages:
                return selected
    return selected


def nearest_entries(
    entries: list[MapEntry],
    target_lba: int,
    count: int,
    span_pages: int,
) -> list[MapEntry]:
    valid = [entry for entry in entries if entry.valid]

    def distance(entry: MapEntry) -> int:
        end = entry.l0 + max(1, span_pages) - 1
        if entry.l0 <= target_lba <= end:
            return 0
        return min(abs(target_lba - entry.l0), abs(target_lba - end))

    return sorted(valid, key=distance)[:count]


def print_nearest_entries(args: argparse.Namespace, entries: list[MapEntry], span_pages: int) -> None:
    for rank, entry in enumerate(nearest_entries(entries, args.target_raw_lba, args.near_count, span_pages)):
        end = entry.l0 + max(1, span_pages) - 1
        if entry.l0 <= args.target_raw_lba <= end:
            dist = 0
            cover = 1
        elif args.target_raw_lba < entry.l0:
            dist = entry.l0 - args.target_raw_lba
            cover = 0
        else:
            dist = args.target_raw_lba - end
            cover = 0
        print(
            "N3G_DFU_MAP_NEAR "
            f"rank={rank} j={entry.j} v={entry.v:04X} t={entry.oob_type:02X} "
            f"l0={entry.l0:08X} end={end:08X} raw={args.target_raw_lba:08X} "
            f"host={args.target_host_lba:08X} cover={cover} dist={dist}",
            flush=True,
        )


def parse_offsets(value: str) -> list[int]:
    offsets: list[int] = []
    for part in value.split(","):
        part = part.strip()
        if not part:
            continue
        offsets.append(int(part, 0))
    return offsets


def parse_offset_range(value: str) -> list[int]:
    if not value:
        return []
    if ":" not in value:
        return parse_offsets(value)
    start_text, end_text = value.split(":", 1)
    start = int(start_text, 0)
    end = int(end_text, 0)
    if end < start:
        raise argparse.ArgumentTypeError("offset range end must be >= start")
    return list(range(start, end + 1))


def parse_j_list(value: str) -> set[int]:
    if not value:
        return set()
    return {int(part, 0) for part in value.split(",") if part}


def parse_vblock_list(value: str) -> list[int]:
    if not value:
        return []
    return [int(part, 0) for part in value.split(",") if part]


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


def page_best_probe(page: PhysicalPage, body: bytes, part_size: int) -> replay.BootProbe | None:
    dump = replay.DumpPage(
        id=0,
        kind="dfu",
        meta={
            "j": page.j,
            "v": page.v,
            "po": page.po,
            "l0": page.l0,
            "pb": page.pblock,
            "pp": page.pageoff,
        },
        body=bytearray(body),
    )
    return replay.best_boot_probe(dump, part_size)


def scan(args: argparse.Namespace) -> int:
    if args.body_map_only:
        entries = load_body_map_entries(args.map_body, args.logical_base)
    else:
        entries = load_map_entries(args.map_body, args.map_extra)
    effective_span = args.span_pages or args.ppb * max(1, args.logical_units_per_page)
    valid_count = sum(1 for entry in entries if entry.valid)
    pages = build_scan_pages(
        entries,
        args.target_raw_lba,
        args.window,
        args.max_pages,
        args.ppb,
        args.sweep_offsets,
        effective_span,
        args.logical_units_per_page,
    )
    if args.content_search:
        offsets = args.search_offset_range or args.search_offsets
        pages = build_content_search_pages(
            entries, args.max_pages, offsets, args.start_j, args.end_j, args.only_j
        )
    if args.direct_vblocks:
        offsets = args.search_offset_range or args.search_offsets
        pages = build_direct_vblock_pages(args.direct_vblocks, args.max_pages, offsets)
    print(
        f"N3G_DFU_SCAN_START entries={len(entries)} valid={valid_count} "
        f"host={args.target_host_lba:08X} raw={args.target_raw_lba:08X} "
        f"span={effective_span} pages={len(pages)} "
        f"mode={'direct' if args.direct_vblocks else 'content' if args.content_search else 'target'}"
    , flush=True)
    if not args.content_search and not args.direct_vblocks:
        print_nearest_entries(args, entries, effective_span)

    dump_handle = None
    dump_page_id = 0
    if args.dump_pages_log is not None:
        args.dump_pages_log.parent.mkdir(parents=True, exist_ok=True)
        dump_handle = args.dump_pages_log.open("w", encoding="utf-8")
        dump_handle.write(
            f"N3GD_START host={args.target_host_lba:08X} "
            f"target={args.target_raw_lba:08X} ppb={args.ppb} pages={len(pages)}\n"
        )

    hits = 0
    rejects = 0
    aa_only_suppressed = 0
    try:
        with TemporaryDirectory(prefix="n3g-dfu-bpb-") as tmpdir:
            tmp = Path(tmpdir)
            for index, page in enumerate(pages):
                if index and index % args.progress == 0:
                    print(f"N3G_DFU_SCAN_PROGRESS read={index} hits={hits}", flush=True)
                out = tmp / f"p{index}.bin"
                try:
                    proc = run_windex_read(
                        args.windex, page.bank, out, page.physpage, args.read_timeout
                    )
                except subprocess.CalledProcessError as exc:
                    print(
                        f"N3G_DFU_READ_FAIL j={page.j} v={page.v:04X} po={page.po} "
                        f"bank={page.bank} pp={page.physpage} rc={exc.returncode}"
                    , flush=True)
                    continue
                except subprocess.TimeoutExpired:
                    print(
                        f"N3G_DFU_READ_TIMEOUT j={page.j} v={page.v:04X} po={page.po} "
                        f"bank={page.bank} pp={page.physpage} timeout={args.read_timeout:g}"
                    , flush=True)
                    continue
                if not out.exists():
                    print(
                        f"N3G_DFU_READ_NOFILE j={page.j} v={page.v:04X} po={page.po} "
                        f"bank={page.bank} pp={page.physpage} "
                        f"stdout={proc.stdout.strip()!r} stderr={proc.stderr.strip()!r}"
                    , flush=True)
                    continue
                body = out.read_bytes()
                if len(body) < 0x800:
                    print(
                        f"N3G_DFU_READ_SHORT j={page.j} v={page.v:04X} po={page.po} "
                        f"bank={page.bank} pp={page.physpage} bytes={len(body)}",
                        flush=True,
                    )
                    continue
                probe = page_best_probe(page, body, args.part_size)
                mbr_candidate = False
                for slice_index in range(4):
                    off = slice_index * 0x200
                    sig, _, parts = replay.parse_mbr(body[off : off + 0x200])
                    if sig == 0xAA55 and any(
                        ptype in (0x0B, 0x0C) and start and size
                        for ptype, start, size in parts
                    ):
                        mbr_candidate = True
                        part_text = ",".join(
                            f"{ptype:02X}:{start:08X}:{size:08X}"
                            for ptype, start, size in parts
                        )
                        print(
                            "N3G_DFU_MBR_CAND "
                            f"j={page.j} v={page.v:04X} po={page.po} l0={page.l0:08X} "
                            f"bank={page.bank} pb={page.pblock} pp={page.pageoff} "
                            f"slice={slice_index} parts={part_text}",
                            flush=True,
                        )

                if dump_handle is not None and should_dump_page(args, probe, mbr_candidate):
                    extra = tmp / f"p{index}.extra"
                    try:
                        run_windex_read_extra(
                            args.windex, page.bank, extra, [page.physpage], args.extra_timeout
                        )
                        oob = extra.read_bytes()[:N3G_OOB_LEN]
                    except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
                        oob = b"\x00" * N3G_OOB_LEN
                    dump_page_log(args, dump_handle, dump_page_id, page, body, oob, probe, mbr_candidate)
                    dump_page_id += 1

                if probe is None:
                    continue
                bpb = probe.bpb
                aa_only = bpb.sig == 0xAA55 and not replay.is_sane_bps(bpb.bps) and not bpb.fat_string
                if aa_only and not args.print_aa_only:
                    aa_only_suppressed += 1
                    continue
                print(
                    "N3G_DFU_BOOT_CAND "
                    f"j={page.j} v={page.v:04X} po={page.po} l0={page.l0:08X} "
                    f"bank={page.bank} pb={page.pblock} pp={page.pageoff} "
                    f"slice={probe.slice_index} shift={probe.shift} sig={bpb.sig:04X} "
                    f"bps={bpb.bps} spc={bpb.spc} rs={bpb.reserved} nf={bpb.nfats} "
                    f"fatsz={bpb.fatsz} fs={bpb.fat_string!r} r={bpb.reason:03X}"
                , flush=True)
                if bpb.valid:
                    print(
                        "N3G_DFU_BOOT_OK "
                        f"j={page.j} v={page.v:04X} po={page.po} slice={probe.slice_index} "
                        f"shift={probe.shift} l0={page.l0:08X}"
                    , flush=True)
                    return 0
                rejects += 1
                hits += 1
        print(
            f"N3G_DFU_SCAN_DONE read={len(pages)} hits={hits} rejects={rejects} "
            f"aa_only_suppressed={aa_only_suppressed} boot=0",
            flush=True,
        )
        return 1
    finally:
        if dump_handle is not None:
            dump_handle.write(f"N3GD_DONE pages={dump_page_id}\n")
            dump_handle.close()
            print(
                f"N3G_DFU_DUMP_DONE pages={dump_page_id} file={args.dump_pages_log}",
                flush=True,
            )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--windex", default="tmp/wInd3x-src/wInd3x")
    parser.add_argument("--map-body", type=Path, default=Path("tmp/n3g-capture-cv/map-b6166-p0-body.bin"))
    parser.add_argument("--map-extra", type=Path, default=Path("tmp/n3g-capture-cv/map-b6166-entry0-extra.bin"))
    parser.add_argument(
        "--body-map-only",
        action="store_true",
        help="Treat --map-body as a bare 16-bit vblock table without per-entry OOB.",
    )
    parser.add_argument("--logical-base", type=lambda value: int(value, 0), default=0)
    parser.add_argument(
        "--capture-map-extra",
        type=Path,
        help="Capture page-0 OOB/extras for each non-empty entry in --map-body and exit.",
    )
    parser.add_argument(
        "--capture-meta-pages",
        action="store_true",
        help="Capture t=44/t=45 metadata pages in a physical block/page range and exit.",
    )
    parser.add_argument(
        "--scan-meta-pages",
        action="store_true",
        help="Scan metadata page OOB/extras in a physical block/page range and exit.",
    )
    parser.add_argument(
        "--scan-oob-target",
        action="store_true",
        help="Scan bank OOB/extras for user pages whose logical field is near the target raw LBA.",
    )
    parser.add_argument("--meta-start-block", type=int, default=6166)
    parser.add_argument("--meta-end-block", type=int, default=6169)
    parser.add_argument("--meta-start-page", type=int, default=0)
    parser.add_argument("--meta-end-page", type=int, default=7)
    parser.add_argument("--meta-out", type=Path, default=Path("tmp/n3g-capture-cv/meta"))
    parser.add_argument("--meta-chunk-pages", type=int, default=512)
    parser.add_argument("--meta-progress", type=int, default=512)
    parser.add_argument(
        "--meta-types",
        type=lambda value: set()
        if value.lower() == "all"
        else {int(part, 0) for part in value.split(",") if part},
        default={0x44, 0x45},
    )
    parser.add_argument(
        "--target-lba",
        dest="target_host_lba",
        type=lambda value: int(value, 0),
        default=0xA07E,
        help="Host 512-byte disk LBA to search for; default is p0 start from the MBR.",
    )
    parser.add_argument(
        "--target-host-lba",
        dest="target_host_lba",
        type=lambda value: int(value, 0),
        help="Explicit host 512-byte disk LBA to search for.",
    )
    parser.add_argument(
        "--target-raw-lba",
        type=lambda value: int(value, 0),
        help="Override the raw OOB logical target used for map-entry matching.",
    )
    parser.add_argument(
        "--host-to-raw-shift",
        type=int,
        default=1,
        help="Convert host 512-byte LBA to raw OOB logical units by left-shifting this amount.",
    )
    parser.add_argument("--part-size", type=lambda value: int(value, 0), default=0xE7F81)
    parser.add_argument("--window", type=lambda value: int(value, 0), default=0x20000)
    parser.add_argument("--max-pages", type=int, default=512)
    parser.add_argument("--near-count", type=int, default=8)
    parser.add_argument("--ppb", type=int, default=512)
    parser.add_argument(
        "--logical-units-per-page",
        type=int,
        default=2,
        help="Observed user OOB logical increment per physical page offset.",
    )
    parser.add_argument(
        "--span-pages",
        type=lambda value: int(value, 0),
        default=0,
        help="Logical span covered by one map entry for candidate selection.",
    )
    parser.add_argument("--progress", type=int, default=64)
    parser.add_argument("--oob-bank", type=int, default=0)
    parser.add_argument("--oob-start-page", type=int, default=0)
    parser.add_argument("--oob-end-page", type=int, default=(8192 * 128) - 1)
    parser.add_argument("--oob-chunk-pages", type=int, default=128)
    parser.add_argument("--oob-stride", type=int, default=1)
    parser.add_argument("--oob-progress", type=int, default=32768)
    parser.add_argument("--oob-max-failures", type=int, default=4)
    parser.add_argument("--target-window", type=lambda value: int(value, 0), default=0x20)
    parser.add_argument("--read-timeout", type=float, default=8.0)
    parser.add_argument("--extra-timeout", type=float, default=90.0)
    parser.add_argument(
        "--print-aa-only",
        action="store_true",
        help="Print AA55-only candidates even when bytes-per-sector and FAT string are invalid.",
    )
    parser.add_argument(
        "--dump-pages-log",
        type=Path,
        help="Write replayable N3GD raw body/OOB records for interesting pages to this file.",
    )
    parser.add_argument(
        "--dump-all-pages",
        action="store_true",
        help="With --dump-pages-log, dump every scanned page instead of only interesting pages.",
    )
    parser.add_argument(
        "--content-search",
        action="store_true",
        help="Ignore target range and scan selected offsets from valid map entries by content.",
    )
    parser.add_argument(
        "--search-offsets",
        type=parse_offsets,
        default=parse_offsets("0"),
        help="Comma-separated page offsets for --content-search.",
    )
    parser.add_argument(
        "--search-offset-range",
        type=parse_offset_range,
        default=[],
        help="Inclusive START:END page-offset range for --content-search.",
    )
    parser.add_argument(
        "--only-j",
        type=parse_j_list,
        default=set(),
        help="Comma-separated map entry indexes to scan under --content-search.",
    )
    parser.add_argument(
        "--direct-vblocks",
        type=parse_vblock_list,
        default=[],
        help="Comma-separated vblocks to read directly with --search-offsets/range.",
    )
    parser.add_argument("--start-j", type=int, default=0)
    parser.add_argument("--end-j", type=int, default=1 << 30)
    parser.add_argument(
        "--sweep-offsets",
        action="store_true",
        help="Also test common page offsets in every selected map entry.",
    )
    args = parser.parse_args(argv)
    if args.target_raw_lba is None:
        args.target_raw_lba = args.target_host_lba << args.host_to_raw_shift
    if args.capture_map_extra is not None:
        return capture_map_extra(args)
    if args.scan_oob_target:
        return scan_oob_target(args)
    if args.scan_meta_pages:
        return scan_meta_pages(args)
    if args.capture_meta_pages:
        return capture_meta_pages(args)
    return scan(args)


if __name__ == "__main__":
    raise SystemExit(main())
