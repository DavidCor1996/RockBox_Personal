#!/usr/bin/env python3
"""Losslessly page a user-owned O2R for NanoApps' bounded whole-file reader."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import struct
import tempfile
import zipfile
import zlib

PAGE = 65536
MASK = (1 << 64) - 1
TABLE = []
for i in range(256):
    n = i << 56
    for _ in range(8):
        n = ((n << 1) ^ (0x42f0e1eba9ea3693 if n >> 63 else 0)) & MASK
    TABLE.append(n)


def resource_hash(name):
    if name.startswith('__OTR__'):
        name = name[7:]
    value = MASK
    for byte in name.encode('utf8'):
        value = ((value << 8) ^ TABLE[(value >> 56) ^ byte]) & MASK
    return value


def checked_file(header, payload, crc_offset):
    data = bytearray(header + payload)
    struct.pack_into('<I', data, crc_offset, zlib.crc32(data))
    return data


def build(source, destination):
    source, destination = Path(source).resolve(), Path(destination).absolute()
    if destination.exists():
        raise ValueError('output exists; select a new destination')
    # ROM-derived output must never be written into either source checkout.
    for ancestor in [destination, *destination.parents]:
        git = ancestor / '.git'
        if git.is_file() or (git / 'HEAD').is_file():
            raise ValueError(f'asset output must be outside a source checkout: {ancestor}')
    with source.open('rb') as stream:
        archive_hash = hashlib.file_digest(stream, 'sha256').digest()
    pack_id = int.from_bytes(archive_hash[:8], 'little')
    destination.parent.mkdir(parents=True, exist_ok=True)
    temp = Path(tempfile.mkdtemp(prefix='.nanores-', dir=destination.parent))
    try:
        with zipfile.ZipFile(source) as archive:
            records = [[] for _ in range(256)]
            seen, expanded, duplicates = {}, 0, 0
            for info in archive.infolist():
                if info.is_dir():
                    continue
                path = PurePosixPath(info.filename)
                if (path.is_absolute() or '..' in path.parts or '\\' in info.filename
                        or not info.filename or '\x00' in info.filename):
                    raise ValueError('invalid resource path')
                key = resource_hash(info.filename)
                if key in seen:
                    previous = seen[key]
                    if (previous.filename != info.filename or
                            archive.read(previous) != archive.read(info)):
                        raise ValueError('conflicting resource name or CRC64 collision')
                    duplicates += 1
                    continue
                if info.file_size > 32 * 1024 * 1024:
                    raise ValueError('resource exceeds host conversion bound')
                seen[key] = info
                expanded += info.file_size
                if expanded > 256 * 1024 * 1024 or len(seen) > 100000:
                    raise ValueError('archive exceeds Nano pack bounds')
                records[key >> 56].append((key, info))
            if not {'version', 'portVersion'}.issubset(archive.namelist()):
                raise ValueError('expected a validated Shipwright O2R archive')
            catalog, file_count = [], 1
            for bucket, items in enumerate(records):
                folder = temp / f'{bucket:02x}'
                folder.mkdir()
                entries, pending, offset, page = bytearray(), bytearray(), 0, 0

                def write_page(payload):
                    nonlocal page, file_count
                    header = struct.pack('<4s4IQ', b'NSD1', bucket, page,
                                         len(payload), 0, pack_id)
                    (folder / f'{page:06x}.nsd').write_bytes(
                        checked_file(header, payload, 16))
                    page += 1
                    file_count += 1

                for key, info in sorted(items):
                    # ZipFile.read validates this resource's ZIP CRC.
                    data = archive.read(info)
                    typ = struct.unpack_from('<I', data, 4)[0] if len(data) >= 64 else 0
                    entries += struct.pack('<Q4I', key, offset, len(data),
                                           zlib.crc32(data), typ)
                    catalog.append({'name': info.filename, 'hash': f'{key:016x}',
                                    'size': len(data), 'type': f'{typ:08x}'})
                    offset += len(data)
                    pending += data
                    while len(pending) >= PAGE:
                        write_page(pending[:PAGE])
                        del pending[:PAGE]
                if pending:
                    write_page(pending)
                header = struct.pack('<4s5IQ', b'NSI1', 1, bucket, len(items),
                                     offset, 0, pack_id)
                index = checked_file(header, entries, 20)
                if len(index) > 16384:
                    raise ValueError('resource index shard exceeds device bound')
                (folder / 'index.nsi').write_bytes(index)
                file_count += 1
            header = struct.pack('<4s5I32sI', b'NSP1', 1, PAGE, 256, len(seen),
                                 expanded, archive_hash, 0)
            (temp / 'pack.nsp').write_bytes(checked_file(header, b'', 56))
            manifest = {'format': 'Nano resource pages v1',
                        'source_sha256': archive_hash.hex(), 'resources': len(seen),
                        'expanded_bytes': expanded, 'page_bytes': PAGE,
                        'files': file_count, 'lossless': True,
                        'identical_duplicate_entries_removed': duplicates,
                        'native_structure_conversion': False,
                        'playable_runtime': False}
            (temp / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
            (temp / 'catalog.json').write_text(json.dumps(catalog, indent=2) + '\n')
            checksums = []
            for file in sorted(temp.rglob('*')):
                if file.is_file() and file.suffix in ('.nsp', '.nsi', '.nsd'):
                    digest = hashlib.sha256(file.read_bytes()).hexdigest()
                    checksums.append(f'{digest}  {file.relative_to(temp).as_posix()}\n')
            (temp/'SHA256SUMS').write_text(''.join(checksums))
        temp.rename(destination)
        return manifest
    finally:
        if temp.exists():
            shutil.rmtree(temp)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    print(json.dumps(build(args.archive, args.output), indent=2))
