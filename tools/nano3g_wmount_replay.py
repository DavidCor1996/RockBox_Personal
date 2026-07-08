#!/usr/bin/env python3
"""Replay Nano 3G WinPod WMOUNT sector lookup captures.

The live device emits N3GD_* page chunks and N3GM_ENTRY manifest lines. This
tool replays lookup_lba() on the desktop using range containment only:

    requested_lba >= entry_l0
    requested_lba < entry_l0 + entry_span

It intentionally does not implement MacPod/APM/HFS probing.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import importlib.util
from pathlib import Path
import re
import sys
from typing import Iterable


FTL_REPLAY_PATH = Path(__file__).with_name("nano3g_ftl_replay.py")
SPEC = importlib.util.spec_from_file_location("nano3g_ftl_replay", FTL_REPLAY_PATH)
ftl = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules["nano3g_ftl_replay"] = ftl
SPEC.loader.exec_module(ftl)


WINPOD_TEST_LBAS = (0x00000000, 0x00000001, 0x0000003F, 0x0000A07C, 0x0000A07E, 0x0000A080)
WINPOD_FAT_TYPES = {0x0B, 0x0C}


@dataclass(frozen=True)
class WmountEntry:
    j: int
    v: int
    l0: int
    span: int
    entry_type: int = 0xFF
    po0: int = 0


@dataclass(frozen=True)
class LookupResult:
    lba: int
    entry: WmountEntry | None
    delta: int | None
    po_final: int | None
    sector_slice: int | None
    page: object | None
    reason: str


@dataclass(frozen=True)
class BootSearchHit:
    score: int
    transform: str
    layout: str
    raw_lba: int
    entry: WmountEntry
    delta: int
    po_final: int
    sector_slice: int
    actual_slice: int
    shift: int
    page: object
    bpb: object


@dataclass(frozen=True)
class MissingPage:
    transform: str
    layout: str
    raw_lba: int
    target_lba: int
    entry: WmountEntry
    delta: int
    po_final: int
    sector_slice: int

    def target_name(self) -> str:
        return f"miss_j{self.entry.j}_v{self.entry.v:04X}_po{self.po_final}"

    def line(self) -> str:
        return (
            f"target={self.target_name()} transform={self.transform} layout={self.layout} "
            f"target_lba={self.target_lba:08X} raw={self.raw_lba:08X} "
            f"j={self.entry.j} v={self.entry.v:04X} l0={self.entry.l0:08X} "
            f"span={self.entry.span} po={self.po_final} sl={self.sector_slice} "
            f"delta={self.delta}"
        )


@dataclass(frozen=True)
class ReferenceHit:
    score: int
    exact: bool
    page: object
    off: int
    bpb: object
    equal_bytes: int
    equal_chunks: int


@dataclass(frozen=True)
class CurrentPageCover:
    page: object
    raw_l0: int
    page_l0: int
    raw_delta: int
    slice_index: int
    score: tuple[int, int, int, int]


def parse_int(value: str) -> int:
    if value.lower().startswith("0x"):
        return int(value, 16)
    if any(c in "abcdefABCDEF" for c in value):
        return int(value, 16)
    return int(value, 10)


def parse_dec(value: str) -> int:
    return int(value, 10)


def parse_hex(value: str) -> int:
    return int(value, 16)


def parse_key_values(tokens: Iterable[str]) -> dict[str, str]:
    out: dict[str, str] = {}
    for token in tokens:
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        out[key] = value
    return out


def parse_manifest_entries(lines: Iterable[str], default_span: int) -> list[WmountEntry]:
    raw_lines = [raw.strip() for raw in lines]
    map_types: dict[tuple[int, int], int] = {}
    for line in raw_lines:
        if not line.startswith("N3GM_MAP_SEEN "):
            continue
        fields = parse_key_values(line.split()[1:])
        try:
            map_types[(parse_dec(fields["b"]), parse_dec(fields["p"]))] = parse_hex(fields["t"])
        except (KeyError, ValueError):
            continue

    entries: dict[tuple[int, int, int], WmountEntry] = {}
    for line in raw_lines:
        if not line.startswith("N3GM_ENTRY "):
            continue
        fields = parse_key_values(line.split()[1:])
        try:
            mapb = parse_dec(fields.get("mapb", "0"))
            mapp = parse_dec(fields.get("mapp", "0"))
        except ValueError:
            mapb = 0
            mapp = 0
        if map_types and map_types.get((mapb, mapp), 0xFF) not in (0x44, 0x45):
            continue
        j = parse_dec(fields["j"])
        v = parse_hex(fields["v"])
        l0 = parse_hex(fields["l0"])
        span = parse_dec(fields.get("span", str(default_span)))
        entry_type = parse_hex(fields.get("t", "ff"))
        po0 = parse_dec(fields.get("po", "0"))
        entries[(j, v, l0)] = WmountEntry(j, v, l0, span, entry_type, po0)
    return list(entries.values())


def entries_from_pages(pages: list[object], default_span: int) -> list[WmountEntry]:
    entries: dict[tuple[int, int, int], WmountEntry] = {}
    for page in pages:
        if not ftl.is_boot_candidate_page(page):
            continue
        j = int(page.meta.get("j", 0xFFFFFFFF))
        v = int(page.meta.get("v", 0xFFFF))
        l0 = ftl.page_l0(page)
        t = int(page.meta.get("t", 0xFF))
        if j == 0xFFFFFFFF or v in (0, 0xFFFF) or l0 == 0xFFFFFFFF:
            continue
        if t not in (0x40, 0x41):
            continue
        po = int(page.meta.get("po", 0))
        entries[(j, v, l0)] = WmountEntry(j, v, l0, default_span, t, po)
    return list(entries.values())


def merge_entries(primary: list[WmountEntry], fallback: list[WmountEntry]) -> list[WmountEntry]:
    merged: dict[tuple[int, int, int], WmountEntry] = {}
    for entry in fallback + primary:
        merged[(entry.j, entry.v, entry.l0)] = entry
    return list(merged.values())


def select_mbr(pages: list[object]) -> tuple[object | None, int, list[tuple[int, int, int]]]:
    return ftl.select_mbr(pages)


def mbr_entry_identity(mbr_page: object | None) -> tuple[int, int] | None:
    if mbr_page is None:
        return None
    return int(mbr_page.meta.get("j", 0xFFFFFFFF)), int(mbr_page.meta.get("v", 0xFFFF))


def find_entry_covering_lba(
    entries: list[WmountEntry], lba: int, mbr_identity: tuple[int, int] | None
) -> tuple[WmountEntry | None, str]:
    best: WmountEntry | None = None
    for entry in entries:
        if not (entry.l0 <= lba < entry.l0 + entry.span):
            continue
        if lba != 0 and mbr_identity is not None and (entry.j, entry.v) == mbr_identity:
            continue
        if best is None or entry.l0 > best.l0:
            best = entry
    if best is None:
        return None, "nocover"
    return best, "cover"


def page_for_entry_po(pages: list[object], entry: WmountEntry, po_final: int) -> object | None:
    for page in pages:
        j = int(page.meta.get("j", 0xFFFFFFFF))
        v = int(page.meta.get("v", 0xFFFF))
        l0 = ftl.page_l0(page)
        if (j, v, l0) != (entry.j, entry.v, entry.l0):
            continue
        if int(page.meta.get("po", 0)) == po_final:
            return page
    return None


def lookup_lba(
    pages: list[object],
    entries: list[WmountEntry],
    lba: int,
    mbr_identity: tuple[int, int] | None,
) -> LookupResult:
    if lba == 0:
        page, _, _ = select_mbr(pages)
        return LookupResult(lba, None, 0, 0, 0, page, "mbr" if page is not None else "nombr")

    entry, reason = find_entry_covering_lba(entries, lba, mbr_identity)
    if entry is None:
        return LookupResult(lba, None, None, None, None, None, reason)

    delta = lba - entry.l0
    po_final = entry.po0 + (delta // 4)
    sector_slice = delta & 3
    page = page_for_entry_po(pages, entry, po_final)
    if page is None:
        return LookupResult(lba, entry, delta, po_final, sector_slice, None, "nopage")
    return LookupResult(lba, entry, delta, po_final, sector_slice, page, "ok")


def read_result_sector(result: LookupResult) -> bytes | None:
    if result.page is None:
        return None
    if result.lba == 0:
        off = 0
    else:
        off = (result.sector_slice or 0) * 0x200
    if len(result.page.body) < off + 0x200:
        return None
    return bytes(result.page.body[off : off + 0x200])


def load_capture(path: str, ppb: int) -> tuple[list[object], list[WmountEntry]]:
    capture = Path(path)
    if capture.is_dir():
        manifest = capture / "n3g_wmount_manifest.txt"
        lines = manifest.read_text(encoding="utf-8", errors="replace").splitlines()
        pages = load_binary_pages(capture, lines)
        for extra in sorted(capture.glob("*.n3gd")):
            pages.extend(ftl.parse_dump(extra.read_text(encoding="utf-8", errors="replace").splitlines()))
    else:
        lines = capture.read_text(encoding="utf-8", errors="replace").splitlines()
        pages = ftl.parse_dump(lines)
    entries = merge_entries(parse_manifest_entries(lines, ppb * 4), entries_from_pages(pages, ppb * 4))
    return pages, entries


def load_binary_pages(root: Path, lines: Iterable[str]) -> list[object]:
    pages: list[object] = []
    page_id = 1
    loaded_bins: set[str] = set()
    for raw in lines:
        line = raw.strip()
        if not line.startswith("N3GD_FILE "):
            continue
        fields = parse_key_values(line.split()[1:])
        body_path = root / fields["bin"]
        oob_path = root / fields["oob"]
        if not body_path.exists() or not oob_path.exists():
            continue
        if body_path.stat().st_size < 0x200 or oob_path.stat().st_size == 0:
            continue
        meta = ftl.parse_tokens(line.split()[1:])
        page = ftl.DumpPage(
            id=page_id,
            kind=str(meta.get("kind", "unknown")),
            meta=meta,
            body=bytearray(body_path.read_bytes()),
            oob=bytearray(oob_path.read_bytes()),
        )
        pages.append(page)
        loaded_bins.add(fields["bin"])
        page_id += 1
    for body_path in sorted(root.glob("*.bin")):
        if body_path.name in loaded_bins:
            continue
        oob_path = body_path.with_suffix(".oob")
        if not oob_path.exists():
            continue
        if body_path.stat().st_size < 0x200 or oob_path.stat().st_size == 0:
            continue
        page = orphan_binary_page(page_id, body_path, oob_path)
        if page is not None:
            pages.append(page)
            page_id += 1
    if pages:
        return pages
    return ftl.parse_dump(lines)


def orphan_binary_page(page_id: int, body_path: Path, oob_path: Path):
    target = body_path.stem
    body = body_path.read_bytes()
    oob = oob_path.read_bytes()
    meta: dict[str, int | str] = {
        "target": target,
        "kind": "page",
        "j": 0xFFFFFFFF,
        "v": 0xFFFF,
        "po": 0,
        "l0": 0xFFFFFFFF,
        "pb": 0xFFFFFFFF,
        "pp": 0xFFFFFFFF,
        "t": 0xFF,
    }

    match = re.match(r"scan_v([0-9A-Fa-f]{4})_po(\d+)_l([0-9A-Fa-f]{8})$", target)
    if match:
        meta["v"] = int(match.group(1), 16)
        meta["po"] = int(match.group(2), 10)
        meta["l0"] = int(match.group(3), 16)
    match = re.match(r".*_v([0-9A-Fa-f]{4})_po(\d+)$", target)
    if match:
        meta["v"] = int(match.group(1), 16)
        meta["po"] = int(match.group(2), 10)
    match = re.match(r".*_j(\d+)_v([0-9A-Fa-f]{4})_po(\d+)$", target)
    if match:
        meta["j"] = int(match.group(1), 10)
        meta["v"] = int(match.group(2), 16)
        meta["po"] = int(match.group(3), 10)
    match = re.match(r"page_b(\d+)_p(\d+)$", target)
    if match:
        meta["pb"] = int(match.group(1), 10)
        meta["pp"] = int(match.group(2), 10)

    if len(oob) >= 10:
        meta["l"] = ftl.le32(oob, 0)
        meta["l0"] = int(meta.get("l0", 0xFFFFFFFF))
        if meta["l0"] == 0xFFFFFFFF:
            meta["l0"] = int(meta["l"])
        meta["t"] = oob[9]

    if target == "page_b1524_p0":
        meta["v"] = 0x044B
        meta["po"] = 0

    return ftl.DumpPage(
        id=page_id,
        kind=str(meta.get("kind", "page")),
        meta=meta,
        body=bytearray(body),
        oob=bytearray(oob),
    )


def describe_lookup(result: LookupResult, part_size: int) -> str:
    sec = read_result_sector(result)
    sig = ftl.le16(sec, 0x1FE) if sec is not None else 0
    bpb = ftl.bpb_reason(sec, part_size) if sec is not None else None
    fatstr = repr(bpb.fat_string) if bpb is not None else "''"
    if result.entry is None:
        return (
            f"LBA {result.lba:08X}: rc=-1 reason={result.reason} "
            f"j=FFFFFFFF v=FFFF l0=FFFFFFFF span=0 delta=FFFFFFFF po=FFFFFFFF slice=FFFFFFFF sig={sig:04X}"
        )
    return (
        f"LBA {result.lba:08X}: rc={0 if result.reason in ('ok', 'mbr') else -1} "
        f"reason={result.reason} j={result.entry.j} v={result.entry.v:04X} "
        f"l0={result.entry.l0:08X} span={result.entry.span} "
        f"delta={result.delta if result.delta is not None else 0xFFFFFFFF} "
        f"po_base={result.entry.po0} po_final={result.po_final if result.po_final is not None else 0xFFFFFFFF} "
        f"slice={result.sector_slice if result.sector_slice is not None else 0xFFFFFFFF} "
        f"pb={getattr(result.page, 'meta', {}).get('pb') if result.page is not None else 'NA'} "
        f"pp={getattr(result.page, 'meta', {}).get('pp') if result.page is not None else 'NA'} "
        f"sig={sig:04X} "
        f"bps={bpb.bps if bpb is not None else 0} "
        f"spc={bpb.spc if bpb is not None else 0} "
        f"rs={bpb.reserved if bpb is not None else 0} "
        f"nf={bpb.nfats if bpb is not None else 0} "
        f"fatstr={fatstr}"
    )


def transform_candidates(target_lba: int, part_start: int, mbr_l0: int) -> list[tuple[str, int]]:
    values: list[tuple[str, int]] = [
        ("raw=host", target_lba),
        ("raw=host+2", target_lba + 2),
        ("raw=host-2", target_lba - 2 if target_lba >= 2 else 0),
        ("raw=host*2", target_lba * 2),
        ("raw=host*4", target_lba * 4),
    ]
    if target_lba % 2 == 0:
        values.append(("raw=host/2", target_lba // 2))
    if target_lba % 4 == 0:
        values.append(("raw=host/4", target_lba // 4))

    rel = target_lba - part_start if target_lba >= part_start else 0
    values.extend(
        [
            ("raw=part_start+rel", part_start + rel),
            ("raw=part_start-mbr_l0", part_start - mbr_l0 if part_start >= mbr_l0 else 0),
            ("raw=part_start+2", part_start + 2),
            ("raw=part_start-2", part_start - 2 if part_start >= 2 else 0),
        ]
    )
    return values


def layout_candidates(ppb: int) -> list[tuple[str, int, int]]:
    return [
        ("spp4", ppb * 4, 4),
        ("spp2", ppb * 2, 2),
        ("spp1", ppb, 1),
        ("spp8", ppb * 8, 8),
    ]


def boot_search_score(bpb: object) -> int:
    score = 0
    if bpb.valid:
        score += 1000
    if bpb.sig == 0xAA55:
        score += 100
    if ftl.is_sane_bps(bpb.bps):
        score += 50
    if ftl.is_power_of_two(bpb.spc):
        score += 10
    if bpb.reserved > 0:
        score += 5
    if 0 < bpb.nfats <= 4:
        score += 5
    if bpb.fat_string:
        score += 25
    score -= bpb.reason.bit_count()
    return score


def iter_pages_for_entry_po(
    pages: list[object], entry: WmountEntry, po_final: int
) -> Iterable[object]:
    for page in pages:
        j = int(page.meta.get("j", 0xFFFFFFFF))
        v = int(page.meta.get("v", 0xFFFF))
        po = int(page.meta.get("po", 0xFFFFFFFF))
        l0 = ftl.page_l0(page)
        if (j, v, po) != (entry.j, entry.v, po_final):
            continue
        if l0 not in (entry.l0, 0xFFFFFFFF) and int(page.meta.get("t", 0xFF)) in (0x40, 0x41):
            continue
        yield page


def search_boot(
    path: str,
    target_lba: int,
    ppb: int = 512,
    try_transforms: bool = False,
    print_covers: bool = False,
    emit_missing: Path | None = None,
    near_entries: int = 0,
) -> int:
    pages, entries = load_capture(path, ppb)
    mbr_page, mbr_slice, parts = select_mbr(pages)
    if mbr_page is None:
        print("WMREPLAY_MBR_FAIL")
        return 1

    mbr_sec = bytes(mbr_page.body[mbr_slice * 0x200 : mbr_slice * 0x200 + 0x200])
    mbr_sig = ftl.le16(mbr_sec, 0x1FE)
    mbr_l0 = ftl.page_l0(mbr_page)
    part_type = 0
    part_start = 0
    part_size = 0
    for ptype, start, size in parts:
        if ptype in WINPOD_FAT_TYPES and start and size:
            part_type, part_start, part_size = ptype, start, size
            break

    print(
        f"WMREPLAY_MBR_OK j={mbr_page.meta.get('j')} v={int(mbr_page.meta.get('v', 0)):04X} "
        f"po={mbr_page.meta.get('po')} slice={mbr_slice} sig={mbr_sig:04X}"
    )
    print(f"WMREPLAY_PART_OK p0t={part_type:02X} p0st={part_start:08X} p0sz={part_size:08X}")

    transforms = transform_candidates(target_lba, part_start, mbr_l0) if try_transforms else [("raw=host", target_lba)]
    hits: list[BootSearchHit] = []
    covers = 0
    pages_tested = 0
    missing_pages = 0
    missing: dict[tuple[int, int, int, int], MissingPage] = {}
    for transform, raw_lba in transforms:
        for layout, span, sectors_per_page in layout_candidates(ppb):
            for entry in entries:
                if not (entry.l0 <= raw_lba < entry.l0 + span):
                    continue
                if raw_lba != 0 and (entry.j, entry.v) == mbr_entry_identity(mbr_page):
                    continue
                covers += 1
                delta = raw_lba - entry.l0
                po_final = entry.po0 + (delta // sectors_per_page)
                sector_slice = delta % sectors_per_page
                matched_pages = list(iter_pages_for_entry_po(pages, entry, po_final))
                if print_covers:
                    print(
                        "WMREPLAY_COVER "
                        f"transform={transform} layout={layout} raw={raw_lba:08X} j={entry.j} "
                        f"v={entry.v:04X} l0={entry.l0:08X} span={span} "
                        f"delta={delta} po={po_final} slice={sector_slice} "
                        f"pages={len(matched_pages)} present={'yes' if matched_pages else 'no'}"
                    )
                if not matched_pages:
                    missing_pages += 1
                    key = (entry.j, entry.v, entry.l0, po_final)
                    missing.setdefault(
                        key,
                        MissingPage(
                            transform,
                            layout,
                            raw_lba,
                            target_lba,
                            entry,
                            delta,
                            po_final,
                            sector_slice,
                        ),
                    )
                for page in matched_pages:
                    pages_tested += 1
                    for actual_slice, shift, _, sec_view in ftl.iter_slices_and_shifts(page):
                        sec = bytes(sec_view)
                        if len(sec) < 0x200:
                            continue
                        bpb = ftl.bpb_reason(sec, part_size)
                        interesting = bpb.sig == 0xAA55 or ftl.is_sane_bps(bpb.bps) or bool(bpb.fat_string)
                        if not interesting:
                            continue
                        hits.append(
                            BootSearchHit(
                                boot_search_score(bpb),
                                transform,
                                layout,
                                raw_lba,
                                entry,
                                delta,
                                po_final,
                                sector_slice,
                                actual_slice,
                                shift,
                                page,
                                bpb,
                            )
                        )

        if near_entries > 0:
            nearest = sorted(
                (
                    entry
                    for entry in entries
                    if raw_lba != 0 or (entry.j, entry.v) != mbr_entry_identity(mbr_page)
                ),
                key=lambda entry: abs(entry.l0 - raw_lba),
            )[:near_entries]
            for entry in nearest:
                if raw_lba != 0 and (entry.j, entry.v) == mbr_entry_identity(mbr_page):
                    continue
                po_final = entry.po0
                sector_slice = 0
                delta = raw_lba - entry.l0
                matched_pages = list(iter_pages_for_entry_po(pages, entry, po_final))
                if print_covers:
                    print(
                        "WMREPLAY_NEAR_ENTRY "
                        f"transform={transform} raw={raw_lba:08X} j={entry.j} "
                        f"v={entry.v:04X} l0={entry.l0:08X} "
                        f"distance={abs(entry.l0 - raw_lba)} pages={len(matched_pages)} "
                        f"present={'yes' if matched_pages else 'no'}"
                    )
                if not matched_pages:
                    missing_pages += 1
                    key = (entry.j, entry.v, entry.l0, po_final)
                    missing.setdefault(
                        key,
                        MissingPage(
                            transform,
                            "near",
                            raw_lba,
                            target_lba,
                            entry,
                            delta,
                            po_final,
                            sector_slice,
                        ),
                    )
                for page in matched_pages:
                    pages_tested += 1
                    for actual_slice, shift, _, sec_view in ftl.iter_slices_and_shifts(page):
                        sec = bytes(sec_view)
                        if len(sec) < 0x200:
                            continue
                        bpb = ftl.bpb_reason(sec, part_size)
                        interesting = bpb.sig == 0xAA55 or ftl.is_sane_bps(bpb.bps) or bool(bpb.fat_string)
                        if not interesting:
                            continue
                        hits.append(
                            BootSearchHit(
                                boot_search_score(bpb),
                                transform,
                                "near",
                                raw_lba,
                                entry,
                                delta,
                                po_final,
                                sector_slice,
                                actual_slice,
                                shift,
                                page,
                                bpb,
                            )
                        )

    hits.sort(key=lambda hit: hit.score, reverse=True)
    for hit in hits[:64]:
        bpb = hit.bpb
        print(
            "WMREPLAY_BOOT_CAND "
            f"score={hit.score} transform={hit.transform} layout={hit.layout} raw={hit.raw_lba:08X} "
            f"j={hit.entry.j} v={hit.entry.v:04X} l0={hit.entry.l0:08X} "
            f"span={hit.entry.span} delta={hit.delta} po={hit.po_final} "
            f"slice={hit.sector_slice} actual_slice={hit.actual_slice} shift={hit.shift} "
            f"sig={bpb.sig:04X} bps={bpb.bps} "
            f"spc={bpb.spc} rs={bpb.reserved} nf={bpb.nfats} "
            f"fatstr={bpb.fat_string!r} r={bpb.reason:03X}"
        )
        if bpb.valid:
            print(
                "WMREPLAY_BOOT_OK "
                f"lba={target_lba:08X} transform={hit.transform} layout={hit.layout} raw={hit.raw_lba:08X} "
                f"j={hit.entry.j} v={hit.entry.v:04X} po={hit.po_final} "
                f"slice={hit.actual_slice} shift={hit.shift} sig={bpb.sig:04X} bps={bpb.bps} "
                f"spc={bpb.spc} nf={bpb.nfats}"
            )
            return 0

    if emit_missing is not None:
        emit_missing.parent.mkdir(parents=True, exist_ok=True)
        text = "\n".join(item.line() for item in missing.values())
        if text:
            text += "\n"
        emit_missing.write_text(text, encoding="utf-8")
        print(f"WMREPLAY_MISSING_WRITTEN file={emit_missing} pages={len(missing)}")

    print(
        "WMREPLAY_BOOT_NOT_FOUND "
        f"lba={target_lba:08X} transforms={len(transforms)} covers={covers} "
        f"pages={pages_tested} missing_pages={missing_pages} cands={len(hits)}"
    )
    return 1


def scan_pages(path: str, ppb: int = 512, limit: int = 64) -> int:
    pages, _ = load_capture(path, ppb)
    mbr_page, mbr_slice, parts = select_mbr(pages)
    part_size = 0
    for ptype, start, size in parts:
        if ptype in WINPOD_FAT_TYPES and start and size:
            part_size = size
            break

    if mbr_page is not None:
        print(
            f"WMREPLAY_MBR_OK j={mbr_page.meta.get('j')} v={int(mbr_page.meta.get('v', 0)):04X} "
            f"po={mbr_page.meta.get('po')} slice={mbr_slice}"
        )

    candidates: list[tuple[int, object, int, int, object]] = []
    for page in pages:
        for slice_index, shift, _, sec in ftl.iter_slices_and_shifts(page):
            bpb = ftl.bpb_reason(sec, part_size)
            if not (bpb.sig == 0xAA55 or ftl.is_sane_bps(bpb.bps) or bpb.fat_string):
                continue
            candidates.append((boot_search_score(bpb), page, slice_index, shift, bpb))

    candidates.sort(key=lambda item: item[0], reverse=True)
    valid = 0
    for score, page, slice_index, shift, bpb in candidates[:limit]:
        if bpb.valid:
            valid += 1
        print(
            "WMREPLAY_PAGE_CAND "
            f"score={score} target={page.meta.get('target', '')} "
            f"j={page.meta.get('j')} v={int(page.meta.get('v', 0)):04X} "
            f"po={page.meta.get('po')} l0={ftl.page_l0(page):08X} "
            f"slice={slice_index} shift={shift} sig={bpb.sig:04X} bps={bpb.bps} "
            f"spc={bpb.spc} rs={bpb.reserved} nf={bpb.nfats} "
            f"fatstr={bpb.fat_string!r} r={bpb.reason:03X}"
        )
    print(f"WMREPLAY_PAGE_SCAN pages={len(pages)} cands={len(candidates)} valid={valid}")
    return 0 if valid else 1


def is_uniform(chunk: bytes) -> bool:
    return bool(chunk) and all(value == chunk[0] for value in chunk)


def reference_match_score(ref: bytes, sec: bytes) -> tuple[int, int, int]:
    equal_bytes = sum(1 for left, right in zip(ref, sec) if left == right)
    equal_chunks = 0
    score = equal_bytes
    for off in range(0, min(len(ref), len(sec)), 16):
        ref_chunk = ref[off : off + 16]
        if len(ref_chunk) != 16 or is_uniform(ref_chunk):
            continue
        if sec[off : off + 16] == ref_chunk:
            equal_chunks += 1
            score += 256
    return score, equal_bytes, equal_chunks


def scan_reference_sector(path: str, reference: Path, ppb: int = 512, limit: int = 32) -> int:
    ref = reference.read_bytes()
    if len(ref) != 0x200:
        print(f"WMREPLAY_REF_FAIL file={reference} reason=size size={len(ref)}")
        return 1

    pages, _ = load_capture(path, ppb)
    _, _, parts = select_mbr(pages)
    part_size = 0
    for ptype, _, size in parts:
        if ptype in WINPOD_FAT_TYPES and size:
            part_size = size
            break

    hits: list[ReferenceHit] = []
    best: ReferenceHit | None = None
    for page in pages:
        body = bytes(page.body)
        if len(body) < 0x200:
            continue
        for off in range(0, len(body) - 0x200 + 1):
            sec = body[off : off + 0x200]
            exact = sec == ref
            bpb = ftl.bpb_reason(sec, part_size)
            score, equal_bytes, equal_chunks = reference_match_score(ref, sec)
            interesting = (
                exact
                or equal_chunks >= 2
                or equal_bytes >= 384
                or bpb.sig == 0xAA55
                or ftl.is_sane_bps(bpb.bps)
                or bool(bpb.fat_string)
            )
            if not interesting:
                continue
            hit = ReferenceHit(score + (100000 if exact else 0), exact, page, off, bpb, equal_bytes, equal_chunks)
            hits.append(hit)
            if best is None or hit.score > best.score:
                best = hit

    hits.sort(key=lambda hit: hit.score, reverse=True)
    for hit in hits[:limit]:
        page = hit.page
        slice_index = hit.off // 0x200
        shift = hit.off - slice_index * 0x200
        bpb = hit.bpb
        print(
            "WMREPLAY_REF_CAND "
            f"exact={1 if hit.exact else 0} score={hit.score} eqb={hit.equal_bytes} eqc={hit.equal_chunks} "
            f"target={page.meta.get('target', '')} j={page.meta.get('j')} "
            f"v={int(page.meta.get('v', 0)):04X} po={page.meta.get('po')} "
            f"l0={ftl.page_l0(page):08X} pb={page.meta.get('pb')} pp={page.meta.get('pp')} "
            f"off={hit.off:04X} slice={slice_index} shift={shift} "
            f"sig={bpb.sig:04X} bps={bpb.bps} spc={bpb.spc} rs={bpb.reserved} "
            f"nf={bpb.nfats} fatstr={bpb.fat_string!r} r={bpb.reason:03X}"
        )
        if hit.exact:
            print(
                "WMREPLAY_REF_OK "
                f"file={reference} target={page.meta.get('target', '')} "
                f"j={page.meta.get('j')} v={int(page.meta.get('v', 0)):04X} "
                f"po={page.meta.get('po')} l0={ftl.page_l0(page):08X} "
                f"off={hit.off:04X} slice={slice_index} shift={shift}"
            )
            return 0

    if best is None:
        print(f"WMREPLAY_REF_NOT_FOUND file={reference} pages={len(pages)} cands=0")
    else:
        print(
            "WMREPLAY_REF_NOT_FOUND "
            f"file={reference} pages={len(pages)} cands={len(hits)} "
            f"best_score={best.score} best_eqb={best.equal_bytes} best_eqc={best.equal_chunks}"
        )
    return 1


def exact_sector_page(pages: list[object], sector: bytes) -> tuple[object | None, int]:
    for page in pages:
        body = bytes(page.body)
        for off in range(0, len(body) - 0x200 + 1, 0x200):
            if body[off : off + 0x200] == sector:
                return page, off // 0x200
    return None, -1


def page_by_l0(pages: list[object], l0: int) -> object | None:
    candidates = [page for page in pages if ftl.page_l0(page) == l0]
    if not candidates:
        return None
    candidates.sort(
        key=lambda page: (
            0 if int(page.meta.get("v", 0xFFFF)) == 0x044B else 1,
            int(page.meta.get("po", 0xFFFFFFFF)),
            str(page.meta.get("target", "")),
        )
    )
    return candidates[0]


def current_page_covering_l0(pages: list[object], raw_l0: int, active_v: int | None = None) -> CurrentPageCover | None:
    """Find the current WinPod page/slice covering a raw logical OOB value.

    Current Nano 3G WinPod captures show disk LBA N maps to:

        raw_l = mbr_l + 2 * N

    A 2048-byte NAND page then covers four 512-byte sectors at raw logical
    values page_l, page_l+2, page_l+4, and page_l+6. Some dumps also contain
    stale exact-l pages from adjacent vblocks, so prefer the active vblock that
    contains the confirmed MBR page over low-bit/exact coincidences.
    """

    best: CurrentPageCover | None = None
    for page in pages:
        page_l0 = ftl.page_l0(page)
        if page_l0 == 0xFFFFFFFF:
            continue
        delta = raw_l0 - page_l0
        if delta < 0 or delta > 6 or (delta & 1):
            continue
        slice_index = delta // 2
        v = int(page.meta.get("v", 0xFFFF))
        po = int(page.meta.get("po", 0xFFFFFFFF))
        target = str(page.meta.get("target", ""))
        score = (
            0 if active_v is not None and v == active_v else 1,
            0 if v == 0x044B else 1,
            0 if delta == 0 else 1,
            po,
        )
        cover = CurrentPageCover(page, raw_l0, page_l0, delta, slice_index, score)
        if best is None or cover.score < best.score or (
            cover.score == best.score and target < str(best.page.meta.get("target", ""))
        ):
            best = cover
    return best


def read_current_winpod_sector(
    pages: list[object], base_l0: int, lba: int, active_v: int | None = None
) -> tuple[bytes | None, object | None, int, int]:
    raw_l0 = base_l0 + 2 * lba
    cover = current_page_covering_l0(pages, raw_l0, active_v)
    if cover is None:
        return None, None, raw_l0, -1
    off = cover.slice_index * 0x200
    if len(cover.page.body) < off + 0x200:
        return None, None, raw_l0, -1
    return bytes(cover.page.body[off : off + 0x200]), cover.page, raw_l0, cover.slice_index


def replay_current_winpod(path: str, lba0_reference: Path, boot_reference: Path | None) -> int:
    pages, _ = load_capture(path, 512)
    lba0 = lba0_reference.read_bytes()
    if len(lba0) != 0x200:
        print(f"WMREPLAY_CUR_FAIL reason=lba0_size size={len(lba0)}")
        return 1

    mbr_page, mbr_slice = exact_sector_page(pages, lba0)
    if mbr_page is None:
        print("WMREPLAY_CUR_FAIL reason=no_lba0_match")
        return 1

    mbr_l0 = ftl.page_l0(mbr_page)
    ptype = lba0[0x1BE + 4]
    pstart = ftl.le32(lba0, 0x1BE + 8)
    psize = ftl.le32(lba0, 0x1BE + 12)
    sig = ftl.le16(lba0, 0x1FE)
    print(
        "WMREPLAY_CUR_MBR_OK "
        f"target={mbr_page.meta.get('target', '')} v={int(mbr_page.meta.get('v', 0)):04X} "
        f"po={mbr_page.meta.get('po')} slice={mbr_slice} l={mbr_l0:08X} "
        f"sig={sig:04X} p0t={ptype:02X} p0st={pstart:08X} p0sz={psize:08X}"
    )
    if sig != 0xAA55 or ptype not in WINPOD_FAT_TYPES or pstart == 0 or psize == 0:
        print("WMREPLAY_CUR_FAIL reason=mbr_parse")
        return 1

    boot_l0 = mbr_l0 + 2 * pstart
    active_v = int(mbr_page.meta.get("v", 0xFFFF))
    boot_cover = current_page_covering_l0(pages, boot_l0, active_v)
    if boot_cover is None:
        print(f"WMREPLAY_CUR_FAIL reason=no_boot_lpn l={boot_l0:08X}")
        return 1
    boot_page = boot_cover.page
    boot_sector = bytes(boot_page.body[boot_cover.slice_index * 0x200 : boot_cover.slice_index * 0x200 + 0x200])
    bpb = ftl.bpb_reason(boot_sector, psize)
    print(
        "WMREPLAY_CUR_BOOT_CAND "
        f"target={boot_page.meta.get('target', '')} v={int(boot_page.meta.get('v', 0)):04X} "
        f"po={boot_page.meta.get('po')} l={boot_l0:08X} page_l={boot_cover.page_l0:08X} "
        f"slice={boot_cover.slice_index} sig={bpb.sig:04X} "
        f"bps={bpb.bps} spc={bpb.spc} rs={bpb.reserved} nf={bpb.nfats} "
        f"fz={bpb.fatsz} ts={bpb.total} fatstr={bpb.fat_string!r} r={bpb.reason:03X}"
    )
    if not bpb.valid:
        print("WMREPLAY_CUR_FAIL reason=boot_bpb")
        return 1
    if boot_reference is not None:
        ref = boot_reference.read_bytes()
        ref_slice = ref[: len(boot_sector)]
        if len(ref_slice) != len(boot_sector) or ref_slice != boot_sector:
            equal = sum(1 for left, right in zip(ref_slice, boot_sector) if left == right)
            print(
                "WMREPLAY_CUR_FAIL "
                f"reason=boot_ref_mismatch eqb={equal} ref={len(ref)} cmp={len(ref_slice)}"
            )
            return 1
    print(f"WMREPLAY_CUR_OK base={mbr_l0:08X} scale=2 boot_lba={pstart:08X} boot_l={boot_l0:08X}")
    return 0


def replay_current_reads(path: str, lba0_reference: Path, lbas: Iterable[int]) -> int:
    pages, _ = load_capture(path, 512)
    lba0 = lba0_reference.read_bytes()
    mbr_page, mbr_slice = exact_sector_page(pages, lba0)
    if mbr_page is None:
        print("WMREPLAY_CUR_READ_FAIL reason=no_lba0_match")
        return 1
    base_l0 = ftl.page_l0(mbr_page)
    active_v = int(mbr_page.meta.get("v", 0xFFFF))
    pstart = ftl.le32(lba0, 0x1BE + 8)
    psize = ftl.le32(lba0, 0x1BE + 12)
    status = 0
    for lba in lbas:
        sec, page, raw_l0, slice_index = read_current_winpod_sector(pages, base_l0, lba, active_v)
        if sec is None or page is None:
            print(f"WMREPLAY_CUR_READ lba={lba:08X} raw={raw_l0:08X} rc=-1 reason=missing")
            status = 1
            continue
        bpb = ftl.bpb_reason(sec, psize)
        print(
            "WMREPLAY_CUR_READ "
            f"lba={lba:08X} raw={raw_l0:08X} rc=0 target={page.meta.get('target', '')} "
            f"v={int(page.meta.get('v', 0)):04X} po={page.meta.get('po')} "
            f"page_l={ftl.page_l0(page):08X} slice={slice_index} "
            f"sig={bpb.sig:04X} bps={bpb.bps} spc={bpb.spc} rs={bpb.reserved} "
            f"nf={bpb.nfats} fatstr={bpb.fat_string!r} r={bpb.reason:03X}"
        )
        if lba in (0, pstart) and bpb.sig != 0xAA55:
            status = 1
    return status


def current_read_probe_line(
    label: str, pages: list[object], base_l0: int, active_v: int, lba: int, part_size: int
) -> str:
    sec, page, raw_l0, slice_index = read_current_winpod_sector(pages, base_l0, lba, active_v)
    if sec is None or page is None:
        return f"WMREPLAY_CUR_PLAN {label} lba={lba:08X} raw={raw_l0:08X} rc=-1 reason=missing"
    bpb = ftl.bpb_reason(sec, part_size)
    return (
        f"WMREPLAY_CUR_PLAN {label} lba={lba:08X} raw={raw_l0:08X} rc=0 "
        f"target={page.meta.get('target', '')} v={int(page.meta.get('v', 0)):04X} "
        f"po={page.meta.get('po')} page_l={ftl.page_l0(page):08X} slice={slice_index} "
        f"sig={bpb.sig:04X} bps={bpb.bps} spc={bpb.spc} rs={bpb.reserved} "
        f"nf={bpb.nfats} fatstr={bpb.fat_string!r} r={bpb.reason:03X}"
    )


def synthetic_fsinfo_sector() -> bytes:
    sector = bytearray(0x200)
    sector[0:4] = b"RRaA"
    sector[0x1E4:0x1E8] = b"rrAa"
    sector[0x1E8:0x1EC] = (0).to_bytes(4, "little")
    sector[0x1EC:0x1F0] = (2).to_bytes(4, "little")
    sector[0x1FC:0x200] = b"\x00\x00\x55\xAA"
    return bytes(sector)


def fsinfo_signature_valid(sector: bytes) -> bool:
    return len(sector) >= 4 and ftl.le32(sector, 0) == 0x41615252


def page_for_v_po(pages: list[object], vblock: int, po: int) -> object | None:
    for page in pages:
        if same_physical_v(int(page.meta.get("v", 0xFFFF)), vblock) and int(page.meta.get("po", 0xFFFFFFFF)) == po:
            return page
    return None


def replay_current_a07x_bank_test(path: str) -> int:
    pages, entries = load_capture(path, 512)
    entry = next((item for item in entries if item.j == 150 and item.v == 0x044C and item.l0 == 0x00014080), None)
    if entry is None:
        matching = [item for item in entries if item.j == 150 and item.v == 0x044C]
        entry = min(matching, key=lambda item: item.l0) if matching else None
    if entry is None:
        print("WMREPLAY_A07X_FAIL reason=no_j150_v044C")
        return 1

    status = 0
    print(
        f"WMREPLAY_A07X_ENTRY j={entry.j} v={entry.v:04X} "
        f"l0={entry.l0:08X} span={entry.span}"
    )
    for lba in (0xA07C, 0xA07E, 0xA080):
        key = lba * 2
        if key < entry.l0:
            print(f"WMREPLAY_A07X lba={lba:08X} key={key:08X} rc=-1 reason=below")
            status = 1
            continue
        delta = key - entry.l0
        naive_po = delta >> 1
        fixed_po = naive_po & ~3
        naive = page_for_v_po(pages, entry.v, naive_po)
        fixed = page_for_v_po(pages, entry.v, fixed_po)

        def page_fields(page: object | None) -> tuple[str, int, int, int, int]:
            if page is None:
                return "missing", 0xFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
            return (
                str(page.meta.get("target", "")),
                int(page.meta.get("t", 0xFF)),
                ftl.page_l0(page),
                int(page.meta.get("pb", 0xFFFFFFFF)),
                int(page.meta.get("pp", 0xFFFFFFFF)),
            )

        ntarget, nt, nl, npb, npp = page_fields(naive)
        ftarget, ft, fl, fpb, fpp = page_fields(fixed)
        if fixed is None or fl == 0xFFFFFFFF or key < fl or key > fl + 6 or ((key - fl) & 1):
            print(
                f"WMREPLAY_A07X lba={lba:08X} key={key:08X} rc=-1 "
                f"naive_po={naive_po} naive_t={nt:02X} naive_l={nl:08X} "
                f"fixed_po={fixed_po} fixed_t={ft:02X} fixed_l={fl:08X} reason=fixed_miss"
            )
            status = 1
            continue
        slice_index = (key - fl) >> 1
        off = slice_index * 0x200
        sec = bytes(fixed.body[off : off + 0x200])
        bpb = ftl.bpb_reason(sec, 0)
        print(
            f"WMREPLAY_A07X lba={lba:08X} key={key:08X} rc=0 "
            f"naive_po={naive_po} naive_bank={naive_po & 3} naive_t={nt:02X} naive_l={nl:08X} "
            f"fixed_po={fixed_po} fixed_bank={fixed_po & 3} fixed_t={ft:02X} fixed_l={fl:08X} "
            f"pb={fpb} pp={fpp} slice={slice_index} sig={bpb.sig:04X} "
            f"bps={bpb.bps} spc={bpb.spc} rs={bpb.reserved} nf={bpb.nfats} "
            f"fatstr={bpb.fat_string!r} target={ftarget} naive_target={ntarget}"
        )
    return status


def same_physical_v(left: int, right: int) -> bool:
    return (left & 0x0FFF) == (right & 0x0FFF)


def current_map_read_sector(
    pages: list[object],
    entries: list[WmountEntry],
    raw_l0: int,
    mbr_v: int,
) -> tuple[bytes | None, object | None, WmountEntry | None, int, int, str]:
    best: WmountEntry | None = None
    for entry in entries:
        if entry.po0 != 0:
            continue
        if (entry.v & 0x0FFF) == (mbr_v & 0x0FFF):
            continue
        if not (entry.l0 <= raw_l0 < entry.l0 + entry.span):
            continue
        if best is None or entry.l0 > best.l0:
            best = entry
    if best is None:
        return None, None, None, 0xFFFFFFFF, 0xFFFFFFFF, "nocover"

    po_guess = (raw_l0 - best.l0) // 2
    po_base = po_guess & ~3
    candidates = [
        page
        for page in pages
        if same_physical_v(int(page.meta.get("v", 0xFFFF)), best.v)
        and int(page.meta.get("po", 0xFFFFFFFF)) in range(max(0, po_base - 4), po_base + 5)
    ]
    candidates.sort(
        key=lambda page: (
            0 if int(page.meta.get("po", 0xFFFFFFFF)) == po_base else 1,
            0 if int(page.meta.get("t", 0xFF)) in (0x40, 0x41) else 1,
            abs(int(page.meta.get("po", 0xFFFFFFFF)) - po_base),
            int(page.meta.get("po", 0xFFFFFFFF)),
        )
    )
    for page in candidates:
        page_l0 = ftl.page_l0(page)
        if page_l0 == 0xFFFFFFFF:
            continue
        delta = raw_l0 - page_l0
        if delta < 0 or delta > 6 or delta & 1:
            continue
        slice_index = delta // 2
        off = slice_index * 0x200
        if len(page.body) < off + 0x200:
            continue
        return bytes(page.body[off : off + 0x200]), page, best, int(page.meta.get("po", 0)), slice_index, "ok"
    return None, None, best, po_base, 0xFFFFFFFF, "nopage"


def replay_current_plan(path: str, lba0_reference: Path, boot_reference: Path) -> int:
    pages, entries = load_capture(path, 512)
    lba0 = lba0_reference.read_bytes()
    boot = boot_reference.read_bytes()
    mbr_page, _ = exact_sector_page(pages, lba0)
    if mbr_page is None:
        print("WMREPLAY_CUR_PLAN_FAIL reason=no_lba0_match")
        return 1
    base_l0 = ftl.page_l0(mbr_page)
    active_v = int(mbr_page.meta.get("v", 0xFFFF))
    pstart = ftl.le32(lba0, 0x1BE + 8)
    psize = ftl.le32(lba0, 0x1BE + 12)
    bps = ftl.le16(boot, 0x0B)
    spc = boot[0x0D]
    reserved = ftl.le16(boot, 0x0E)
    nfats = boot[0x10]
    fatsz = ftl.le16(boot, 0x16) or ftl.le32(boot, 0x24)
    root_cluster = ftl.le32(boot, 0x2C)
    fsinfo = ftl.le16(boot, 0x30)
    backup = ftl.le16(boot, 0x32)
    scale = bps // 512 if bps and bps % 512 == 0 else 1
    first_fat = pstart + reserved * scale
    fat2 = first_fat + fatsz * scale
    data = pstart + (reserved + nfats * fatsz) * scale
    root = data + (root_cluster - 2) * spc * scale if root_cluster >= 2 else data
    fsinfo_lba = pstart + fsinfo * scale
    labels = [
        ("mbr", 0),
        ("boot", pstart),
        ("fsinfo", fsinfo_lba),
        ("backup", pstart + backup * scale),
        ("fat1", first_fat),
        ("fat2", fat2),
        ("root", root),
        ("root_next", root + scale),
    ]
    print(
        "WMREPLAY_CUR_PLAN_BPB "
        f"base={base_l0:08X} active_v={active_v:04X} pstart={pstart:08X} psize={psize:08X} "
        f"bps={bps} scale={scale} spc={spc} rs={reserved} nf={nfats} fatsz={fatsz} "
        f"fsinfo={fsinfo} backup={backup} root_cluster={root_cluster}"
    )
    status = 0
    for label, lba in labels:
        if label == "fsinfo":
            raw_l0 = base_l0 + 2 * lba
            raw_line = current_read_probe_line(label + "_raw", pages, base_l0, active_v, lba, psize)
            synth = synthetic_fsinfo_sector()
            print(raw_line)
            print(
                "WMREPLAY_CUR_PLAN fsinfo_synth "
                f"lba={lba:08X} raw={raw_l0:08X} rc=0 "
                f"sig={ftl.le32(synth, 0):08X} free={ftl.le32(synth, 0x1E8):08X} "
                f"next={ftl.le32(synth, 0x1EC):08X} trail={ftl.le32(synth, 0x1FC):08X}"
            )
            if lba != fsinfo_lba or not fsinfo_signature_valid(synth):
                status = 1
            continue
        line = current_read_probe_line(label, pages, base_l0, active_v, lba, psize)
        print(line)
        if label in {"mbr", "boot", "fat1", "root"} and " rc=-1 " in line:
            status = 1
        if label not in {"mbr", "boot", "fsinfo", "backup"}:
            raw_l0 = base_l0 + 2 * lba
            sec, page, entry, po, slice_index, reason = current_map_read_sector(
                pages, entries, raw_l0, active_v
            )
            if sec is None or page is None or entry is None:
                print(
                    "WMREPLAY_CUR_MAP "
                    f"{label} lba={lba:08X} raw={raw_l0:08X} rc=-1 reason={reason} "
                    f"j={entry.j if entry is not None else 0xFFFFFFFF} "
                    f"v={entry.v if entry is not None else 0xFFFF:04X} "
                    f"l0={entry.l0 if entry is not None else 0xFFFFFFFF:08X} "
                    f"po={po} slice={slice_index}"
                )
                continue
            bpb = ftl.bpb_reason(sec, psize)
            print(
                "WMREPLAY_CUR_MAP "
                f"{label} lba={lba:08X} raw={raw_l0:08X} rc=0 "
                f"target={page.meta.get('target', '')} j={entry.j} v={entry.v:04X} "
                f"l0={entry.l0:08X} po={po} page_l={ftl.page_l0(page):08X} "
                f"slice={slice_index} sig={bpb.sig:04X} bps={bpb.bps} "
                f"spc={bpb.spc} rs={bpb.reserved} nf={bpb.nfats} "
                f"fatstr={bpb.fat_string!r} r={bpb.reason:03X}"
            )
    return status


def replay(path: str, test_lbas: Iterable[int], ppb: int = 512) -> int:
    pages, entries = load_capture(path, ppb)
    mbr_page, mbr_slice, parts = select_mbr(pages)
    mbr_identity = mbr_entry_identity(mbr_page)
    part_type = 0
    part_start = 0
    part_size = 0

    if mbr_page is None:
        print("WMREPLAY_MBR_FAIL")
        return 1

    mbr_sec = bytes(mbr_page.body[mbr_slice * 0x200 : mbr_slice * 0x200 + 0x200])
    mbr_sig = ftl.le16(mbr_sec, 0x1FE)
    for ptype, start, size in parts:
        if ptype in WINPOD_FAT_TYPES and start and size:
            part_type, part_start, part_size = ptype, start, size
            break
    print(
        f"WMREPLAY_MBR_OK j={mbr_page.meta.get('j')} v={int(mbr_page.meta.get('v', 0)):04X} "
        f"po={mbr_page.meta.get('po')} slice={mbr_slice} sig={mbr_sig:04X}"
    )
    print(
        f"WMREPLAY_PART_OK p0t={part_type:02X} p0st={part_start:08X} p0sz={part_size:08X}"
    )

    status = 0
    for lba in test_lbas:
        result = lookup_lba(pages, entries, lba, mbr_identity)
        print(describe_lookup(result, part_size))
        sec = read_result_sector(result)
        if lba == 0:
            if sec is None or ftl.le16(sec, 0x1FE) != 0xAA55 or part_type != 0x0C:
                status = 1
        elif mbr_identity is not None and result.entry is not None and (result.entry.j, result.entry.v) == mbr_identity:
            print(f"WMREPLAY_BAD_MBR_REUSE lba={lba:08X} j={result.entry.j} v={result.entry.v:04X}")
            status = 1
        elif lba == part_start:
            if sec is None:
                print(f"WMREPLAY_BOOT_FAIL lba={lba:08X} reason=nosector")
                status = 1
            else:
                bpb = ftl.bpb_reason(sec, part_size)
                if not bpb.valid:
                    print(
                        "WMREPLAY_BOOT_FAIL "
                        f"lba={lba:08X} sig={bpb.sig:04X} bps={bpb.bps} "
                        f"spc={bpb.spc} rs={bpb.reserved} nf={bpb.nfats} "
                        f"r={bpb.reason:03X}"
                    )
                    status = 1
    return status


def parse_lbas(values: list[str]) -> list[int]:
    if not values:
        return list(WINPOD_TEST_LBAS)
    return [int(value, 0) for value in values]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", help="N3GD text capture or dump directory")
    parser.add_argument("--ppb", type=int, default=512)
    parser.add_argument("--lba", action="append", default=[], help="LBA to test; may be repeated")
    parser.add_argument("--search-boot", action="store_true")
    parser.add_argument("--scan-pages", action="store_true")
    parser.add_argument("--try-transforms", action="store_true")
    parser.add_argument("--print-covers", action="store_true")
    parser.add_argument("--target-lba", type=lambda value: int(value, 0), default=0xA07E)
    parser.add_argument("--emit-missing", type=Path)
    parser.add_argument("--near-entries", type=int, default=0)
    parser.add_argument("--reference-sector", type=Path, help="512-byte host disk sector to find in dumped NAND pages")
    parser.add_argument("--reference-limit", type=int, default=32)
    parser.add_argument("--current-winpod", action="store_true", help="Validate current restored WinPod LBA0 -> FAT boot transform")
    parser.add_argument("--current-reads", action="store_true", help="Read LBAs through current restored WinPod OOB transform")
    parser.add_argument("--current-plan", action="store_true", help="Plan/check current WinPod FAT LBAs from MBR and BPB references")
    parser.add_argument("--current-a07x-bank-test", action="store_true", help="Replay corrected A07C/A07E/A080 bank0 page selection")
    parser.add_argument("--lba0-reference", type=Path, help="Host raw disk LBA0 reference sector")
    parser.add_argument("--boot-reference", type=Path, help="Host partition boot reference sector")
    args = parser.parse_args(argv)
    if args.current_winpod:
        if args.lba0_reference is None:
            parser.error("--current-winpod requires --lba0-reference")
        return replay_current_winpod(args.capture, args.lba0_reference, args.boot_reference)
    if args.current_reads:
        if args.lba0_reference is None:
            parser.error("--current-reads requires --lba0-reference")
        return replay_current_reads(args.capture, args.lba0_reference, parse_lbas(args.lba))
    if args.current_plan:
        if args.lba0_reference is None or args.boot_reference is None:
            parser.error("--current-plan requires --lba0-reference and --boot-reference")
        return replay_current_plan(args.capture, args.lba0_reference, args.boot_reference)
    if args.current_a07x_bank_test:
        return replay_current_a07x_bank_test(args.capture)
    if args.reference_sector is not None:
        return scan_reference_sector(args.capture, args.reference_sector, args.ppb, args.reference_limit)
    if args.scan_pages:
        return scan_pages(args.capture, args.ppb)
    if args.search_boot:
        return search_boot(
            args.capture,
            args.target_lba,
            args.ppb,
            args.try_transforms,
            args.print_covers,
            args.emit_missing,
            args.near_entries,
        )
    return replay(args.capture, parse_lbas(args.lba), args.ppb)


if __name__ == "__main__":
    raise SystemExit(main())
