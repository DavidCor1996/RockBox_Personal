#!/usr/bin/env python3
"""Validate Animal Crossing media and build the Rockbox runtime asset pack.

This tool contains no game data. The user must supply a supported GameCube
disc image. The generated pack is also copyrighted game data and must remain
outside source control.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import struct
import sys
import tempfile
from typing import BinaryIO, Iterator
import zlib


CISO_HEADER_SIZE = 0x8000
CISO_MAP_OFFSET = 8
GAMECUBE_MAGIC = b"\xc2\x33\x9f\x3d"
SUPPORTED_GAME_ID = b"GAFE01"
SUPPORTED_REVISION = 0
MAX_LOGICAL_DISC_SIZE = 1_459_978_240
MAX_FST_ENTRIES = 16_384
MAX_FST_SIZE = 16 * 1024 * 1024
MAX_REL_SIZE = 32 * 1024 * 1024
COPY_CHUNK_SIZE = 1024 * 1024

PACK_MAGIC = b"ACIPACK\0"
PACK_VERSION = 1
PACK_HEADER = struct.Struct("<8sIIIIQQ8s16s")
PACK_ENTRY = struct.Struct("<96sQQIIQ")
PACK_ALIGNMENT = 4096

ENTRY_SYSTEM_DOL = 1
ENTRY_SYSTEM_REL = 2
ENTRY_DISC_FILE = 4
ENTRY_YAZ0_DECOMPRESSED = 8


class AssetError(RuntimeError):
    """A malformed or unsupported input prevented safe asset preparation."""


def _be32(data: bytes, offset: int = 0) -> int:
    return int.from_bytes(data[offset : offset + 4], "big")


def _le32(data: bytes, offset: int = 0) -> int:
    return int.from_bytes(data[offset : offset + 4], "little")


def _align(value: int, alignment: int) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


class DiscReader:
    """Bounded logical reader for plain GameCube images and CISO images."""

    def __init__(self, path: Path):
        self.path = path
        self.file: BinaryIO = path.open("rb")
        self.file_size = path.stat().st_size
        self.is_ciso = False
        self.block_size = 0
        self.block_map: list[int] = []
        self.logical_size = self.file_size
        try:
            self._parse_container()
        except Exception:
            self.file.close()
            raise

    def _parse_container(self) -> None:
        header = self.file.read(CISO_HEADER_SIZE)
        if header[:4] != b"CISO":
            self.file.seek(0)
            if self.file_size > MAX_LOGICAL_DISC_SIZE:
                raise AssetError("plain disc image is larger than a GameCube disc")
            return

        if len(header) != CISO_HEADER_SIZE:
            raise AssetError("truncated CISO header")
        block_size = _le32(header, 4)
        if (
            block_size < 0x8000
            or block_size > 16 * 1024 * 1024
            or block_size & (block_size - 1)
        ):
            raise AssetError(f"invalid CISO block size: {block_size}")

        physical = 0
        block_map: list[int] = []
        for present in header[CISO_MAP_OFFSET:]:
            if present:
                block_map.append(physical)
                physical += 1
            else:
                block_map.append(-1)

        expected_minimum = CISO_HEADER_SIZE + physical * block_size
        if expected_minimum > self.file_size:
            raise AssetError("CISO map references data past end of file")

        self.is_ciso = True
        self.block_size = block_size
        self.block_map = block_map
        self.logical_size = min(len(block_map) * block_size,
                                MAX_LOGICAL_DISC_SIZE)

    def close(self) -> None:
        self.file.close()

    def __enter__(self) -> "DiscReader":
        return self

    def __exit__(self, *args: object) -> None:
        self.close()

    def read(self, offset: int, size: int) -> bytes:
        if offset < 0 or size < 0 or offset + size > self.logical_size:
            raise AssetError(
                f"disc read outside logical image: offset={offset:#x}, "
                f"size={size:#x}"
            )
        if size == 0:
            return b""
        if not self.is_ciso:
            self.file.seek(offset)
            data = self.file.read(size)
            if len(data) != size:
                raise AssetError("truncated plain disc image")
            return data

        result = bytearray()
        remaining = size
        while remaining:
            block = offset // self.block_size
            within = offset % self.block_size
            count = min(remaining, self.block_size - within)
            physical = self.block_map[block]
            if physical < 0:
                result.extend(bytes(count))
            else:
                file_offset = (
                    CISO_HEADER_SIZE + physical * self.block_size + within
                )
                self.file.seek(file_offset)
                data = self.file.read(count)
                if len(data) != count:
                    raise AssetError("truncated CISO data block")
                result.extend(data)
            offset += count
            remaining -= count
        return bytes(result)

    def chunks(self, offset: int, size: int) -> Iterator[bytes]:
        while size:
            count = min(size, COPY_CHUNK_SIZE)
            yield self.read(offset, count)
            offset += count
            size -= count


class DiscFile:
    def __init__(self, path: str, offset: int, size: int):
        self.path = path
        self.offset = offset
        self.size = size


class DiscInfo:
    def __init__(
        self,
        game_id: bytes,
        revision: int,
        title: str,
        dol_offset: int,
        dol_size: int,
        fst_offset: int,
        fst_size: int,
        files: list[DiscFile],
    ):
        self.game_id = game_id
        self.revision = revision
        self.title = title
        self.dol_offset = dol_offset
        self.dol_size = dol_size
        self.fst_offset = fst_offset
        self.fst_size = fst_size
        self.files = files


def _safe_disc_path(name: str) -> str:
    path = PurePosixPath(name)
    if (
        not name
        or name.startswith("/")
        or "\\" in name
        or any(part in ("", ".", "..") for part in path.parts)
    ):
        raise AssetError(f"unsafe path in disc filesystem: {name!r}")
    encoded = name.encode("utf-8")
    if len(encoded) > 90:
        raise AssetError(f"disc path is too long for pack index: {name!r}")
    return name


def _decode_name(table: bytes, offset: int) -> str:
    if offset >= len(table):
        raise AssetError("FST name offset is outside string table")
    end = table.find(b"\0", offset)
    if end < 0:
        raise AssetError("unterminated FST name")
    raw = table[offset:end]
    try:
        return raw.decode("ascii")
    except UnicodeDecodeError as error:
        raise AssetError("non-ASCII FST name") from error


def _dol_size(reader: DiscReader, offset: int) -> int:
    header = reader.read(offset, 0xE4)
    maximum = 0
    for index in range(7):
        section_offset = _be32(header, index * 4)
        section_size = _be32(header, 0x90 + index * 4)
        maximum = max(maximum, section_offset + section_size)
    for index in range(11):
        section_offset = _be32(header, 0x1C + index * 4)
        section_size = _be32(header, 0xAC + index * 4)
        maximum = max(maximum, section_offset + section_size)
    if maximum < 0xE4 or offset + maximum > reader.logical_size:
        raise AssetError(f"invalid DOL extent: {maximum:#x}")
    return maximum


def inspect_disc(reader: DiscReader) -> DiscInfo:
    header = reader.read(0, 0x440)
    if header[0x1C:0x20] != GAMECUBE_MAGIC:
        raise AssetError("input does not contain a GameCube disc header")

    game_id = header[:6]
    revision = header[7]
    if game_id != SUPPORTED_GAME_ID or revision != SUPPORTED_REVISION:
        found = game_id.decode("ascii", "replace")
        raise AssetError(
            f"unsupported disc {found}, revision {revision}; "
            "expected GAFE01, revision 0"
        )

    title = header[0x20:0x60].split(b"\0", 1)[0].decode(
        "ascii", "replace"
    )
    dol_offset = _be32(header, 0x420)
    fst_offset = _be32(header, 0x424)
    fst_size = _be32(header, 0x428)
    if (
        fst_offset < 0x440
        or fst_size < 12
        or fst_size > MAX_FST_SIZE
        or fst_offset + fst_size > reader.logical_size
    ):
        raise AssetError(
            f"invalid FST extent: offset={fst_offset:#x}, size={fst_size:#x}"
        )

    root = reader.read(fst_offset, 12)
    if root[0] != 1:
        raise AssetError("FST root is not a directory")
    entry_count = _be32(root, 8)
    if entry_count < 1 or entry_count > MAX_FST_ENTRIES:
        raise AssetError(f"invalid FST entry count: {entry_count}")
    table_size = entry_count * 12
    if table_size > fst_size:
        raise AssetError("FST entry table exceeds declared FST size")

    entries = reader.read(fst_offset, table_size)
    names = reader.read(fst_offset + table_size, fst_size - table_size)
    stack: list[tuple[int, str]] = [(entry_count, "")]
    files: list[DiscFile] = []
    seen: set[str] = set()

    for index in range(1, entry_count):
        while len(stack) > 1 and index >= stack[-1][0]:
            stack.pop()
        entry = entries[index * 12 : (index + 1) * 12]
        name_offset = int.from_bytes(entry[:4], "big") & 0xFFFFFF
        name = _decode_name(names, name_offset)
        full = "/".join([item[1] for item in stack[1:]] + [name])
        full = _safe_disc_path(full)
        if entry[0] == 1:
            next_entry = _be32(entry, 8)
            if next_entry <= index or next_entry > entry_count:
                raise AssetError(f"invalid FST directory extent for {full!r}")
            stack.append((next_entry, name))
            continue

        file_offset = _be32(entry, 4)
        file_size = _be32(entry, 8)
        if file_offset + file_size > reader.logical_size:
            raise AssetError(f"FST file is outside disc image: {full!r}")
        if full in seen:
            raise AssetError(f"duplicate FST path: {full!r}")
        seen.add(full)
        files.append(DiscFile(full, file_offset, file_size))

    return DiscInfo(
        game_id=game_id,
        revision=revision,
        title=title,
        dol_offset=dol_offset,
        dol_size=_dol_size(reader, dol_offset),
        fst_offset=fst_offset,
        fst_size=fst_size,
        files=files,
    )


def decode_yaz0(source: bytes) -> bytes:
    if len(source) < 16 or source[:4] != b"Yaz0":
        raise AssetError("foresta.rel.szs is not Yaz0 data")
    output_size = _be32(source, 4)
    if output_size < 1 or output_size > MAX_REL_SIZE:
        raise AssetError(f"invalid decompressed REL size: {output_size}")

    output = bytearray()
    position = 16
    while len(output) < output_size:
        if position >= len(source):
            raise AssetError("truncated Yaz0 control stream")
        control = source[position]
        position += 1
        for bit in range(7, -1, -1):
            if len(output) >= output_size:
                break
            if control & (1 << bit):
                if position >= len(source):
                    raise AssetError("truncated Yaz0 literal")
                output.append(source[position])
                position += 1
                continue

            if position + 2 > len(source):
                raise AssetError("truncated Yaz0 back-reference")
            first = source[position]
            second = source[position + 1]
            position += 2
            distance = ((first & 0x0F) << 8) | second
            length = first >> 4
            if length == 0:
                if position >= len(source):
                    raise AssetError("truncated Yaz0 long back-reference")
                length = source[position] + 0x12
                position += 1
            else:
                length += 2
            reference = len(output) - distance - 1
            if reference < 0:
                raise AssetError("Yaz0 back-reference precedes output")
            for _ in range(length):
                if len(output) >= output_size:
                    break
                output.append(output[reference])
                reference += 1

    return bytes(output)


class PackSource:
    def __init__(
        self,
        name: str,
        size: int,
        flags: int,
        source_offset: int,
        chunks: Iterator[bytes],
    ):
        encoded = name.encode("utf-8")
        if len(encoded) >= 96:
            raise AssetError(f"pack path is too long: {name!r}")
        self.name = name
        self.size = size
        self.flags = flags
        self.source_offset = source_offset
        self.chunks = chunks


def _bytes_chunks(data: bytes) -> Iterator[bytes]:
    for offset in range(0, len(data), COPY_CHUNK_SIZE):
        yield data[offset : offset + COPY_CHUNK_SIZE]


def _hash_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(COPY_CHUNK_SIZE):
            digest.update(chunk)
    return digest.hexdigest()


def _pack_sources(
    reader: DiscReader, info: DiscInfo
) -> tuple[list[PackSource], int]:
    rel_file = next(
        (item for item in info.files if item.path == "foresta.rel.szs"),
        None,
    )
    if rel_file is None:
        raise AssetError("supported disc is missing foresta.rel.szs")
    compressed_rel = reader.read(rel_file.offset, rel_file.size)
    rel = decode_yaz0(compressed_rel)

    sources = [
        PackSource(
            "sys/main.dol",
            info.dol_size,
            ENTRY_SYSTEM_DOL,
            info.dol_offset,
            reader.chunks(info.dol_offset, info.dol_size),
        ),
        PackSource(
            "sys/foresta.rel",
            len(rel),
            ENTRY_SYSTEM_REL | ENTRY_YAZ0_DECOMPRESSED,
            rel_file.offset,
            _bytes_chunks(rel),
        ),
    ]
    for item in info.files:
        sources.append(
            PackSource(
                f"disc/{item.path}",
                item.size,
                ENTRY_DISC_FILE,
                item.offset,
                reader.chunks(item.offset, item.size),
            )
        )
    return sources, len(rel)


def build_pack(
    source_path: Path,
    output_path: Path,
    *,
    force: bool = False,
) -> dict[str, object]:
    if output_path.exists() and not force:
        raise AssetError(f"output already exists: {output_path}")
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with DiscReader(source_path) as reader:
        info = inspect_disc(reader)
        sources, rel_size = _pack_sources(reader, info)
        index_offset = PACK_HEADER.size
        data_offset = _align(
            index_offset + len(sources) * PACK_ENTRY.size,
            PACK_ALIGNMENT,
        )
        identity = info.game_id + bytes([info.revision, 0])

        temporary_name = ""
        try:
            with tempfile.NamedTemporaryFile(
                mode="w+b",
                prefix=f".{output_path.name}.",
                suffix=".tmp",
                dir=output_path.parent,
                delete=False,
            ) as output:
                temporary_name = output.name
                output.write(
                    PACK_HEADER.pack(
                        PACK_MAGIC,
                        PACK_VERSION,
                        len(sources),
                        PACK_ENTRY.size,
                        0,
                        index_offset,
                        data_offset,
                        identity,
                        bytes(16),
                    )
                )
                output.write(bytes(len(sources) * PACK_ENTRY.size))
                output.write(bytes(data_offset - output.tell()))

                packed_entries = []
                for source in sources:
                    entry_offset = output.tell()
                    checksum = 0
                    written = 0
                    for chunk in source.chunks:
                        output.write(chunk)
                        checksum = zlib.crc32(chunk, checksum)
                        written += len(chunk)
                    if written != source.size:
                        raise AssetError(
                            f"short pack source {source.name!r}: "
                            f"{written} of {source.size}"
                        )
                    packed_entries.append(
                        PACK_ENTRY.pack(
                            source.name.encode("utf-8"),
                            entry_offset,
                            source.size,
                            checksum & 0xFFFFFFFF,
                            source.flags,
                            source.source_offset,
                        )
                    )

                final_size = output.tell()
                output.seek(index_offset)
                for entry in packed_entries:
                    output.write(entry)
                output.flush()
                os.fsync(output.fileno())

            os.replace(temporary_name, output_path)
            temporary_name = ""
        finally:
            if temporary_name:
                Path(temporary_name).unlink(missing_ok=True)

    return {
        "valid": True,
        "game_id": info.game_id.decode("ascii"),
        "revision": info.revision,
        "title": info.title,
        "container": "CISO" if reader.is_ciso else "ISO/GCM",
        "source_sha256": _hash_file(source_path),
        "dol_size": info.dol_size,
        "rel_size": rel_size,
        "fst_files": len(info.files),
        "pack_entries": len(sources),
        "pack_size": final_size,
        "output": str(output_path),
    }


def inspect_path(source_path: Path) -> dict[str, object]:
    with DiscReader(source_path) as reader:
        info = inspect_disc(reader)
        rel_file = next(
            (item for item in info.files if item.path == "foresta.rel.szs"),
            None,
        )
        if rel_file is None:
            raise AssetError("supported disc is missing foresta.rel.szs")
        rel = decode_yaz0(reader.read(rel_file.offset, rel_file.size))
        return {
            "valid": True,
            "game_id": info.game_id.decode("ascii"),
            "revision": info.revision,
            "title": info.title,
            "container": "CISO" if reader.is_ciso else "ISO/GCM",
            "source_sha256": _hash_file(source_path),
            "logical_size": reader.logical_size,
            "dol_offset": info.dol_offset,
            "dol_size": info.dol_size,
            "fst_offset": info.fst_offset,
            "fst_size": info.fst_size,
            "fst_files": len(info.files),
            "compressed_rel_size": rel_file.size,
            "rel_size": len(rel),
        }


def _arguments(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("disc", type=Path, help="user-owned ISO, GCM, or CISO")
    parser.add_argument(
        "--output",
        type=Path,
        help="write the generated runtime .assets.pack",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="replace an existing output after successful preparation",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = _arguments(argv if argv is not None else sys.argv[1:])
    try:
        if args.output:
            report = build_pack(args.disc, args.output, force=args.force)
        else:
            report = inspect_path(args.disc)
    except (AssetError, OSError) as error:
        print(f"animalcrossing assets: {error}", file=sys.stderr)
        return 1
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
