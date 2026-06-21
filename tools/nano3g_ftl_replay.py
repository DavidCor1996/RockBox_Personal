#!/usr/bin/env python3
"""Replay Nano 3G Whimory/FTL mount diagnostics from N3GD dump logs."""

from __future__ import annotations

import argparse
from dataclasses import dataclass, field
import sys
from typing import Iterable


BPB_OK_IGNORED = 0x020


def le16(data: bytes | bytearray, off: int) -> int:
    return data[off] | (data[off + 1] << 8)


def le32(data: bytes | bytearray, off: int) -> int:
    return (
        data[off]
        | (data[off + 1] << 8)
        | (data[off + 2] << 16)
        | (data[off + 3] << 24)
    )


def is_sane_bps(value: int) -> bool:
    return value in {512, 1024, 2048, 4096}


def is_power_of_two(value: int) -> bool:
    return value != 0 and (value & (value - 1)) == 0


def has_fat_string(sec: bytes | bytearray) -> bool:
    return sec[0x36:0x39] == b"FAT" or sec[0x52:0x55] == b"FAT"


@dataclass
class BpbResult:
    reason: int
    sig: int
    bps: int
    spc: int
    reserved: int
    nfats: int
    total: int
    fatsz: int
    fat_string: str

    @property
    def valid(self) -> bool:
        return self.sig == 0xAA55 and (self.reason & ~BPB_OK_IGNORED) == 0


@dataclass
class DumpPage:
    id: int
    kind: str
    meta: dict[str, int | str]
    body: bytearray = field(default_factory=lambda: bytearray(0x800))
    oob: bytearray = field(default_factory=lambda: bytearray(0x40))
    body_seen: set[int] = field(default_factory=set)
    oob_seen: set[int] = field(default_factory=set)

    def write_body(self, off: int, chunk: bytes) -> None:
        end = off + len(chunk)
        if end > len(self.body):
            self.body.extend(b"\x00" * (end - len(self.body)))
        self.body[off:end] = chunk
        self.body_seen.add(off)

    def write_oob(self, off: int, chunk: bytes) -> None:
        end = off + len(chunk)
        if end > len(self.oob):
            self.oob.extend(b"\x00" * (end - len(self.oob)))
        self.oob[off:end] = chunk
        self.oob_seen.add(off)


@dataclass
class BootProbe:
    page: DumpPage
    slice_index: int
    shift: int
    bpb: BpbResult
    score: int


@dataclass(frozen=True)
class CoveringEntry:
    page: DumpPage
    sector_delta: int
    page_offset: int
    slice_index: int


@dataclass(frozen=True)
class ReplayProfile:
    name: str
    partition_scheme: str
    fat_types: frozenset[int]
    host_to_raw_shift: int = 0


WINPOD_MBR_FAT32 = ReplayProfile(
    name="winpod_mbr_fat32",
    partition_scheme="mbr",
    fat_types=frozenset({0x0B, 0x0C}),
    host_to_raw_shift=0,
)


PROFILES = {
    WINPOD_MBR_FAT32.name: WINPOD_MBR_FAT32,
}


def parse_value(key: str, value: str) -> int | str:
    if key in {"kind", "target", "profile", "bin", "oob"}:
        return value
    hex_keys = {"v", "l0", "tgt", "host", "t", "l", "u", "ix", "off"}
    if value in {"ffffffff", "FFFFFFFF"}:
        return 0xFFFFFFFF
    if key in hex_keys:
        return int(value, 16)
    return int(value, 10)


def parse_tokens(tokens: Iterable[str]) -> dict[str, int | str]:
    parsed: dict[str, int | str] = {}
    for token in tokens:
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        parsed[key] = parse_value(key, value)
    return parsed


def parse_dump(lines: Iterable[str]) -> list[DumpPage]:
    pages: dict[int, DumpPage] = {}

    for raw in lines:
        line = raw.strip()
        if not line:
            continue
        if line.startswith("N3GD_BEGIN "):
            meta = parse_tokens(line.split()[1:])
            page_id = int(meta["id"])
            kind = str(meta.get("kind", "unknown"))
            pages[page_id] = DumpPage(page_id, kind, meta)
        elif line.startswith("N3GD_BODY ") or line.startswith("N3GD_OOB "):
            parts = line.split()
            meta = parse_tokens(parts[1:-1])
            page_id = int(meta["id"])
            off = int(meta["off"])
            chunk = bytes.fromhex(parts[-1])
            if page_id not in pages:
                pages[page_id] = DumpPage(page_id, "unknown", {"id": page_id})
            if line.startswith("N3GD_BODY "):
                pages[page_id].write_body(off, chunk)
            else:
                pages[page_id].write_oob(off, chunk)

    return [pages[i] for i in sorted(pages)]


def bpb_reason(sec: bytes | bytearray, part_size: int = 0) -> BpbResult:
    if len(sec) < 0x200:
        return BpbResult(
            reason=0x7FF,
            sig=0,
            bps=0,
            spc=0,
            reserved=0,
            nfats=0,
            total=0,
            fatsz=0,
            fat_string="",
        )
    sig = le16(sec, 0x1FE)
    bps = le16(sec, 0x0B)
    spc = sec[0x0D]
    reserved = le16(sec, 0x0E)
    nfats = sec[0x10]
    total16 = le16(sec, 0x13)
    total32 = le32(sec, 0x20)
    total = total16 or total32
    fatsz16 = le16(sec, 0x16)
    fatsz32 = le32(sec, 0x24)
    fatsz = fatsz16 or fatsz32
    rootclus = le32(sec, 0x2C)
    fat = has_fat_string(sec)
    reason = 0

    if sec[0] not in (0xEB, 0xE9):
        reason |= 0x001
    if not is_sane_bps(bps):
        reason |= 0x002
    if not is_power_of_two(spc):
        reason |= 0x004
    if reserved == 0:
        reason |= 0x008
    if nfats == 0 or nfats > 4:
        reason |= 0x010
    if nfats != 2:
        reason |= 0x020
    if total == 0:
        reason |= 0x040
    elif part_size and (total + 0x1000 < part_size or total > part_size + 0x1000):
        reason |= 0x080
    if not fat:
        reason |= 0x100
    if fatsz == 0:
        reason |= 0x200
    if fatsz16 == 0 and rootclus < 2:
        reason |= 0x400

    fat_string = ""
    if sec[0x36:0x39] == b"FAT":
        fat_string = sec[0x36:0x3E].decode("ascii", "replace")
    elif sec[0x52:0x55] == b"FAT":
        fat_string = sec[0x52:0x5A].decode("ascii", "replace")

    return BpbResult(reason, sig, bps, spc, reserved, nfats, total, fatsz, fat_string)


def parse_mbr(sec: bytes | bytearray) -> tuple[int, int, list[tuple[int, int, int]]]:
    sig = le16(sec, 0x1FE)
    parts: list[tuple[int, int, int]] = []
    for p in range(4):
        off = 0x1BE + p * 16
        parts.append((sec[off + 4], le32(sec, off + 8), le32(sec, off + 12)))
    return sig, 0, parts


def iter_slices_and_shifts(page: DumpPage):
    for slice_index in range(4):
        slice_off = slice_index * 0x200
        for shift in range(-32, 33):
            off = slice_off + shift
            if off < 0 or off + 0x200 > len(page.body):
                continue
            yield slice_index, shift, off, page.body[off : off + 0x200]


def select_mbr(pages: list[DumpPage]) -> tuple[DumpPage | None, int, list[tuple[int, int, int]]]:
    candidates = [p for p in pages if p.kind == "mbr"]
    candidates.extend(p for p in pages if p.kind != "mbr" and int(p.meta.get("l0", -1)) == 0)
    seen: set[int] = set()
    for page in candidates:
        if page.id in seen:
            continue
        seen.add(page.id)
        for slice_index in range(4):
            off = slice_index * 0x200
            sig, _, parts = parse_mbr(page.body[off : off + 0x200])
            if sig == 0xAA55:
                return page, slice_index, parts
    return None, -1, []


def page_l0(page: DumpPage) -> int:
    actual_l = int(page.meta.get("l", 0xFFFFFFFF))
    if actual_l not in (0xFFFFFFFF,):
        return actual_l
    l0 = int(page.meta.get("l0", 0xFFFFFFFF))
    return l0


def page_entry_l0(page: DumpPage) -> int:
    return int(page.meta.get("l0", page_l0(page)))


def is_boot_candidate_page(page: DumpPage) -> bool:
    return page.kind != "map" and page_l0(page) != 0xFFFFFFFF


def page_entry_key(page: DumpPage) -> tuple[int, int, int]:
    return (
        int(page.meta.get("j", 0xFFFFFFFF)),
        int(page.meta.get("v", 0xFFFF)),
        page_entry_l0(page),
    )


def find_entry_covering_lba(
    pages: list[DumpPage], lba: int, ppb: int
) -> CoveringEntry | None:
    """Return the map entry covering a disk LBA plus the sector delta.

    Nano 3G map entries cover a range. The partition boot sector may be inside
    an entry whose first logical sector is lower than the requested LBA, e.g.
    l0=0xA07C covering LBA 0xA07E at delta=2.
    """

    pages_by_key_po: dict[tuple[tuple[int, int, int], int], DumpPage] = {}
    entries: dict[tuple[int, int, int], DumpPage] = {}

    for page in pages:
        if not is_boot_candidate_page(page):
            continue
        l0 = page_entry_l0(page)
        if l0 == 0:
            continue
        key = page_entry_key(page)
        entries.setdefault(key, page)
        pages_by_key_po[(key, int(page.meta.get("po", 0)))] = page

    best: CoveringEntry | None = None
    for key, entry_page in entries.items():
        l0 = key[2]
        if not (l0 <= lba < l0 + ppb * 4):
            continue
        delta = lba - l0
        page_offset = delta // 4
        slice_index = delta & 3
        page = pages_by_key_po.get((key, page_offset), entry_page)
        candidate = CoveringEntry(page, delta, page_offset, slice_index)
        if best is None or page_l0(candidate.page) > page_l0(best.page):
            best = candidate
    return best


def read_lba_512(pages: list[DumpPage], lba: int, ppb: int) -> bytes | None:
    if lba == 0:
        mbr_page, mbr_slice, _ = select_mbr(pages)
        if mbr_page is None:
            return None
        off = mbr_slice * 0x200
        return bytes(mbr_page.body[off : off + 0x200])

    entry = find_entry_covering_lba(pages, lba, ppb)
    if entry is None:
        return None
    off = entry.slice_index * 0x200
    return bytes(entry.page.body[off : off + 0x200])


def covering_pages_for_lba(pages: list[DumpPage], lba: int, ppb: int) -> list[DumpPage]:
    entry = find_entry_covering_lba(pages, lba, ppb)
    return [] if entry is None else [entry.page]


def nearest_pages_for_lba(
    pages: list[DumpPage], lba: int, limit: int, excluded: set[int] | None = None
) -> list[DumpPage]:
    excluded = excluded or set()
    return sorted(
        (
            p
            for p in pages
            if is_boot_candidate_page(p) and p.id not in excluded and page_l0(p) != 0
        ),
        key=lambda p: abs(page_l0(p) - lba),
    )[:limit]


def candidate_pages_for_lba(pages: list[DumpPage], lba: int, ppb: int) -> list[DumpPage]:
    covering = covering_pages_for_lba(pages, lba, ppb)
    if covering:
        return covering
    return nearest_pages_for_lba(pages, lba, 8)


def boot_probe_score(bpb: BpbResult) -> int:
    score = 0
    if bpb.valid:
        score += 1000
    if bpb.sig == 0xAA55:
        score += 100
    if is_sane_bps(bpb.bps):
        score += 50
    if bpb.fat_string:
        score += 25
    score -= bpb.reason.bit_count()
    return score


def best_boot_probe(page: DumpPage, part_size: int) -> BootProbe | None:
    best: BootProbe | None = None
    for slice_index, shift, _, sec in iter_slices_and_shifts(page):
        bpb = bpb_reason(sec, part_size)
        interesting = bpb.sig == 0xAA55 or is_sane_bps(bpb.bps) or bool(bpb.fat_string)
        if not interesting:
            continue
        probe = BootProbe(page, slice_index, shift, bpb, boot_probe_score(bpb))
        if best is None or probe.score > best.score:
            best = probe
        if bpb.valid:
            return probe
    return best


def print_boot_probe(prefix: str, probe: BootProbe) -> None:
    page = probe.page
    bpb = probe.bpb
    print(
        f"{prefix} "
        f"j={page.meta.get('j')} v={int(page.meta.get('v', 0)):04X} "
        f"po={page.meta.get('po')} l0={page_l0(page):08X} "
        f"slice={probe.slice_index} shift={probe.shift} "
        f"pb={page.meta.get('pb')} pp={page.meta.get('pp')} "
        f"sig={bpb.sig:04X} bps={bpb.bps} spc={bpb.spc} rs={bpb.reserved} "
        f"nf={bpb.nfats} fatsz={bpb.fatsz} fs={bpb.fat_string!r} r={bpb.reason:03X}"
    )


def replay(
    path: str,
    target_host_lba: int,
    ppb: int,
    near_count: int = 16,
    host_to_raw_shift: int = 0,
    target_raw_lba: int | None = None,
    profile: ReplayProfile = WINPOD_MBR_FAT32,
) -> int:
    if target_raw_lba is None:
        target_raw_lba = target_host_lba << host_to_raw_shift
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        pages = parse_dump(handle)

    mbr_page, mbr_slice, parts = select_mbr(pages)
    part_start = 0
    part_size = 0
    part_type = 0
    if mbr_page is None:
        print("REPLAY_MBR_FAIL")
    else:
        print(
            "REPLAY_MBR_OK "
            f"j={mbr_page.meta.get('j')} v={int(mbr_page.meta.get('v', 0)):04X} "
            f"po={mbr_page.meta.get('po')} slice={mbr_slice}"
        )
        for index, (ptype, start, size) in enumerate(parts):
            if ptype in profile.fat_types and start and size and not part_start:
                part_type = ptype
                part_start = start
                part_size = size
            print(f"REPLAY_PART idx={index} type={ptype:02X} start={start:08X} size={size:08X}")
        if part_start:
            print(
                f"REPLAY_PART_OK type={part_type:02X} start={part_start:08X} "
                f"size={part_size:08X}"
            )

    if target_host_lba == 0 and mbr_page is not None:
        return 0

    if part_start and target_host_lba == part_start:
        part_size_for_bpb = part_size
    else:
        part_size_for_bpb = 0

    selected: BootProbe | None = None
    covering_entry = find_entry_covering_lba(pages, target_raw_lba, ppb)
    if covering_entry is not None:
        print(
            "REPLAY_COVER "
            f"lba={target_raw_lba:08X} j={covering_entry.page.meta.get('j')} "
            f"v={int(covering_entry.page.meta.get('v', 0)):04X} "
            f"l0={page_l0(covering_entry.page):08X} "
            f"delta={covering_entry.sector_delta} po={covering_entry.page_offset} "
            f"slice={covering_entry.slice_index}"
        )
    covering = [] if covering_entry is None else [covering_entry.page]
    near = nearest_pages_for_lba(pages, target_raw_lba, near_count, {p.id for p in covering})
    printed = 0
    for phase, phase_pages in (("cover", covering), ("near", near)):
        for page in phase_pages:
            probe = best_boot_probe(page, part_size_for_bpb)
            if probe is None:
                continue
            print_boot_probe(f"REPLAY_BOOT_CAND phase={phase}", probe)
            printed += 1
            if probe.bpb.valid:
                selected = probe
                break
            print(
                "REPLAY_BOOT_REJECT "
                f"phase={phase} j={page.meta.get('j')} po={page.meta.get('po')} "
                f"slice={probe.slice_index} shift={probe.shift} reason={probe.bpb.reason:03X}"
            )
        if selected is not None:
            break

    if selected is not None:
        page = selected.page
        bpb = selected.bpb
        print(
            "REPLAY_BOOT_OK "
            f"j={page.meta.get('j')} v={int(page.meta.get('v', 0)):04X} "
            f"po={page.meta.get('po')} slice={selected.slice_index} shift={selected.shift} "
            f"bps={bpb.bps} spc={bpb.spc} rs={bpb.reserved} nf={bpb.nfats}"
        )
        return 0

    if not printed:
        for page in (covering + near)[:8]:
            print(
                "REPLAY_NEAR "
                f"j={page.meta.get('j')} v={int(page.meta.get('v', 0)):04X} "
                f"po={page.meta.get('po')} l0={page_l0(page):08X} "
                f"pb={page.meta.get('pb')} pp={page.meta.get('pp')}"
            )
    print(
        f"REPLAY_NOBOOT host={target_host_lba:08X} raw={target_raw_lba:08X} "
        f"covering={len(covering)} "
        f"near={len(near)} printed={printed}"
    )
    return 1


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dump", help="Text log containing N3GD_* lines")
    parser.add_argument(
        "--target-lba",
        dest="target_host_lba",
        default="0xA07E",
        help="Host 512-byte disk LBA to replay; default is p0 start from the MBR.",
    )
    parser.add_argument("--target-host-lba", dest="target_host_lba")
    parser.add_argument("--target-raw-lba", help="Override raw OOB logical target.")
    parser.add_argument("--host-to-raw-shift", type=int)
    parser.add_argument("--ppb", type=int, default=512)
    parser.add_argument("--near-count", type=int, default=16)
    parser.add_argument(
        "--profile",
        choices=sorted(PROFILES),
        default=WINPOD_MBR_FAT32.name,
        help="Replay profile; default is current Nano 3G WinPod MBR/FAT32-LBA.",
    )
    args = parser.parse_args(argv)
    profile = PROFILES[args.profile]
    host_to_raw_shift = (
        profile.host_to_raw_shift if args.host_to_raw_shift is None else args.host_to_raw_shift
    )
    target_raw_lba = int(args.target_raw_lba, 0) if args.target_raw_lba else None
    return replay(
        args.dump,
        int(args.target_host_lba, 0),
        args.ppb,
        args.near_count,
        host_to_raw_shift,
        target_raw_lba,
        profile,
    )


if __name__ == "__main__":
    raise SystemExit(main())
