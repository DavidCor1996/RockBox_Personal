#!/usr/bin/env python3
"""Create pre-unpacked RuneScape Classic cache archives for Rockbox."""

import argparse
import bz2
from pathlib import Path


def get3(data, offset):
    return (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2]


def put3(value):
    return bytes(((value >> 16) & 0xff, (value >> 8) & 0xff, value & 0xff))


def prepare_file(source, destination, entries_dir=None, deep_entries=False):
    data = source.read_bytes()
    if len(data) < 6:
        raise ValueError(f"{source} is too small")

    size = get3(data, 0)
    compressed_size = get3(data, 3)
    body = data[6 : 6 + compressed_size]

    if len(body) != compressed_size:
        raise ValueError(f"{source} is truncated")

    if size != compressed_size:
        body = bz2.decompress(b"BZh1" + body)
        if len(body) != size:
            raise ValueError(f"{source} decompressed to {len(body)}, expected {size}")

    if entries_dir is not None:
        prepare_archive_entries(body, entries_dir / source.name)

    if deep_entries:
        body = prepare_archive_body(body)

    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(put3(len(body)) + put3(len(body)) + body)


def prepare_archive_entries(body, destination_dir):
    if len(body) < 2:
        return

    count = (body[0] << 8) | body[1]
    table_size = 2 + count * 10
    if count <= 0 or table_size > len(body):
        return

    offset = table_size
    entries = []

    for entry in range(count):
        base = 2 + entry * 10
        file_hash = int.from_bytes(body[base : base + 4], "big")
        file_size = get3(body, base + 4)
        archive_size = get3(body, base + 7)

        if offset + archive_size > len(body):
            return

        data = body[offset : offset + archive_size]
        if file_size != archive_size:
            data = bz2.decompress(b"BZh1" + data)
            if len(data) != file_size:
                raise ValueError(
                    f"entry {entry} decompressed to {len(data)}, expected {file_size}")

        entries.append((file_hash, data))
        offset += archive_size

    destination_dir.mkdir(parents=True, exist_ok=True)
    for file_hash, data in entries:
        (destination_dir / f"{file_hash:08x}").write_bytes(data)


def prepare_archive_body(body):
    if len(body) < 2:
        return body

    count = (body[0] << 8) | body[1]
    table_size = 2 + count * 10
    if count <= 0 or table_size > len(body):
        return body

    offset = table_size
    entries = []

    for entry in range(count):
        base = 2 + entry * 10
        file_hash = body[base : base + 4]
        file_size = get3(body, base + 4)
        archive_size = get3(body, base + 7)

        if offset + archive_size > len(body):
            return body

        data = body[offset : offset + archive_size]
        if file_size != archive_size:
            data = bz2.decompress(b"BZh1" + data)
            if len(data) != file_size:
                raise ValueError(
                    f"entry {entry} decompressed to {len(data)}, expected {file_size}")

        entries.append((file_hash, data))
        offset += archive_size

    rebuilt = bytearray()
    rebuilt.extend(bytes(((count >> 8) & 0xff, count & 0xff)))
    for file_hash, data in entries:
        rebuilt.extend(file_hash)
        rebuilt.extend(put3(len(data)))
        rebuilt.extend(put3(len(data)))
    for _, data in entries:
        rebuilt.extend(data)

    return bytes(rebuilt)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source_dir", type=Path)
    parser.add_argument("ready_dir", type=Path)
    parser.add_argument(
        "--pattern", action="append", default=["*.jag", "*.mem"],
        help="glob pattern to prepare; may be repeated")
    parser.add_argument(
        "--deep-entries", action="store_true",
        help="also unpack compressed files inside each archive")
    parser.add_argument(
        "--no-entry-cache", action="store_true",
        help="skip ready_entries cache generation")
    args = parser.parse_args()

    files = []
    for pattern in args.pattern:
        files.extend(args.source_dir.glob(pattern))

    for source in sorted(set(files)):
        if source.parent == args.ready_dir:
            continue
        entries_dir = None
        if not args.no_entry_cache:
            entries_dir = args.ready_dir.parent / "ready_entries"
        destination = args.ready_dir / source.name
        prepare_file(source, destination, entries_dir, args.deep_entries)
        print(f"{source.name} -> {destination}")


if __name__ == "__main__":
    main()
