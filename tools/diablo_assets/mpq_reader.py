"""Minimal reader for the original Diablo (1996) MPQ archive format.

This targets format-version-0 MPQs specifically (no listfile, no
BURN/sector-CRC extensions): hash/block table parsing is adapted from the
public-domain-equivalent algorithm documented for the MoPaQ format (the
same generic hash/decrypt scheme is reimplemented in dozens of open-source
MPQ tools; it is not Blizzard asset content). Compressed sector payloads
are decompressed with the PKWare DCL "explode" algorithm via the vendored,
verified blast_cli helper (see blast_build.py), not reimplemented here.

Every hash/decrypt routine below is validated in prepare_assets.py against
real DIABDAT.MPQ data before use (monotonic sector-offset check, and a
full round-trip decode of a known asset) -- see that script's --selftest.
"""
from __future__ import annotations

import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

MPQ_FILE_IMPLODE = 0x00000100
MPQ_FILE_COMPRESS = 0x00000200
MPQ_FILE_ENCRYPTED = 0x00010000
MPQ_FILE_FIX_KEY = 0x00020000
MPQ_FILE_SINGLE_UNIT = 0x01000000
MPQ_FILE_EXISTS = 0x80000000

_HEADER_FMT = "<4sIIHHIIII"
_HEADER_SIZE = struct.calcsize(_HEADER_FMT)


def _build_crypt_table() -> list[int]:
    seed = 0x00100001
    table = [0] * 0x500
    for i in range(256):
        index = i
        for _ in range(5):
            seed = (seed * 125 + 3) % 0x2AAAAB
            temp1 = (seed & 0xFFFF) << 0x10
            seed = (seed * 125 + 3) % 0x2AAAAB
            temp2 = seed & 0xFFFF
            table[index] = temp1 | temp2
            index += 0x100
    return table


_CRYPT_TABLE = _build_crypt_table()
_HASH_TABLE_OFFSET, _HASH_A, _HASH_B, _HASH_FILE_KEY = 0, 1, 2, 3


def mpq_hash(string: str, hash_type: int) -> int:
    seed1 = 0x7FED7FED
    seed2 = 0xEEEEEEEE
    for ch in string.upper():
        value = _CRYPT_TABLE[(hash_type << 8) + ord(ch)]
        seed1 = (value ^ (seed1 + seed2)) & 0xFFFFFFFF
        seed2 = (ord(ch) + seed1 + seed2 + (seed2 << 5) + 3) & 0xFFFFFFFF
    return seed1


def _decrypt_dwords(data: bytes, key: int) -> bytes:
    """Decrypt data whose length is a multiple of 4 bytes."""
    seed1 = key & 0xFFFFFFFF
    seed2 = 0xEEEEEEEE
    out = bytearray(len(data))
    for i in range(0, len(data), 4):
        seed2 = (seed2 + _CRYPT_TABLE[0x400 + (seed1 & 0xFF)]) & 0xFFFFFFFF
        (value,) = struct.unpack_from("<I", data, i)
        value = (value ^ (seed1 + seed2)) & 0xFFFFFFFF
        seed1 = ((~seed1 << 0x15) + 0x11111111 | (seed1 >> 0x0B)) & 0xFFFFFFFF
        seed2 = (value + seed2 + (seed2 << 5) + 3) & 0xFFFFFFFF
        struct.pack_into("<I", out, i, value)
    return bytes(out)


def decrypt_block(data: bytes, key: int) -> bytes:
    """Decrypt an MPQ block. Any trailing 1-3 bytes (not a full dword) were
    never encrypted by the archiver and are passed through unchanged."""
    aligned = (len(data) // 4) * 4
    return _decrypt_dwords(data[:aligned], key) + data[aligned:]


@dataclass
class HashEntry:
    hash_a: int
    hash_b: int
    locale: int
    platform: int
    block_index: int


@dataclass
class BlockEntry:
    offset: int
    archived_size: int
    size: int
    flags: int


class MpqArchive:
    def __init__(self, path: Path, explode: Callable[[bytes], bytes]):
        self.path = Path(path)
        self._explode = explode
        self._fh = open(self.path, "rb")
        self._read_header()
        self._read_tables()

    def close(self):
        self._fh.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def _read_header(self):
        self._fh.seek(0)
        raw = self._fh.read(_HEADER_SIZE)
        (
            magic,
            header_size,
            archive_size,
            format_version,
            sector_size_shift,
            hash_table_offset,
            block_table_offset,
            hash_table_entries,
            block_table_entries,
        ) = struct.unpack(_HEADER_FMT, raw)
        if magic != b"MPQ\x1a":
            raise ValueError(f"{self.path}: not an MPQ archive (bad magic)")
        self.header_size = header_size
        self.archive_size = archive_size
        self.format_version = format_version
        self.sector_size = 512 << sector_size_shift
        self.hash_table_offset = hash_table_offset
        self.block_table_offset = block_table_offset
        self.hash_table_entries = hash_table_entries
        self.block_table_entries = block_table_entries

    def _read_table(self, offset: int, count: int, key_name: str, fields: int):
        self._fh.seek(offset)
        raw = self._fh.read(count * 16)
        key = mpq_hash(f"({key_name} table)", _HASH_FILE_KEY)
        data = decrypt_block(raw, key)
        rows = []
        for i in range(count):
            rows.append(struct.unpack_from("<" + "I" * fields, data, i * 16))
        return rows

    def _read_tables(self):
        hash_rows = self._read_table(
            self.hash_table_offset, self.hash_table_entries, "hash", 4
        )
        self.hash_table = [
            HashEntry(a, b, locale & 0xFFFF, locale >> 16, block_index)
            for (a, b, locale, block_index) in hash_rows
        ]
        block_rows = self._read_table(
            self.block_table_offset, self.block_table_entries, "block", 4
        )
        self.block_table = [BlockEntry(*row) for row in block_rows]

    def find(self, name: str) -> BlockEntry | None:
        hash_a = mpq_hash(name, _HASH_A)
        hash_b = mpq_hash(name, _HASH_B)
        for entry in self.hash_table:
            if entry.hash_a == hash_a and entry.hash_b == hash_b:
                block = self.block_table[entry.block_index]
                if block.flags & MPQ_FILE_EXISTS:
                    return block
        return None

    def read(self, name: str) -> bytes:
        block = self.find(name)
        if block is None:
            raise KeyError(name)

        self._fh.seek(block.offset)
        raw = self._fh.read(block.archived_size)

        basename = name.rsplit("\\", 1)[-1]
        file_key = mpq_hash(basename, _HASH_FILE_KEY)
        if block.flags & MPQ_FILE_FIX_KEY:
            file_key = (file_key + block.offset) ^ block.size
            file_key &= 0xFFFFFFFF

        def maybe_explode(chunk: bytes, raw_size: int) -> bytes:
            if block.flags & MPQ_FILE_IMPLODE and len(chunk) != raw_size:
                return self._explode(chunk)
            return chunk

        if block.flags & MPQ_FILE_SINGLE_UNIT:
            data = raw
            if block.flags & MPQ_FILE_ENCRYPTED:
                data = decrypt_block(data, file_key)
            return maybe_explode(data, block.size)[: block.size]

        n_sectors = (block.size + self.sector_size - 1) // self.sector_size
        pos_table_len = (n_sectors + 1) * 4
        pos_bytes = raw[:pos_table_len]
        if block.flags & MPQ_FILE_ENCRYPTED:
            pos_bytes = decrypt_block(pos_bytes, file_key - 1)
        positions = struct.unpack("<%dI" % (n_sectors + 1), pos_bytes)
        if positions[0] != pos_table_len:
            raise ValueError(f"{name}: corrupt sector table (bad decrypt key?)")

        out = bytearray()
        remaining = block.size
        for i in range(n_sectors):
            sector = raw[positions[i] : positions[i + 1]]
            raw_size = min(self.sector_size, remaining)
            if block.flags & MPQ_FILE_ENCRYPTED:
                sector = decrypt_block(sector, file_key + i)
            out += maybe_explode(sector, raw_size)
            remaining -= raw_size
        return bytes(out[: block.size])
