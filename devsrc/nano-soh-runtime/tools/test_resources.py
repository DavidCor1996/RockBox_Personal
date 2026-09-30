#!/usr/bin/env python3
"""Differential-test the real C reader against every resource in an O2R."""
import argparse
import ctypes as C
import json
from pathlib import Path
import random
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]


class Resource(C.Structure):
    _fields_ = [('hash', C.c_uint64)] + [(n, C.c_uint32) for n in
                ('offset', 'size', 'crc', 'type', 'bucket_bytes')]


def run(archive_path, pack_path):
    with tempfile.TemporaryDirectory(prefix='nano-pack-test-') as scratch:
        scratch = Path(scratch)
        (scratch / 'size.c').write_text(
            '#include "nano_pack.h"\nsize_t workspace_size(void) { return sizeof(struct nano_pack); }\n')
        libpath = scratch / 'pack.so'
        subprocess.run(['cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-fPIC', '-shared', '-I'+str(ROOT/'platform'),
                        str(ROOT/'platform/nano_pack.c'), str(scratch/'size.c'),
                        '-o', str(libpath)], check=True)
        lib = C.CDLL(str(libpath))
        lib.workspace_size.restype = C.c_size_t
        memory = C.create_string_buffer(lib.workspace_size())
        callback_type = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_char_p, C.c_void_p,
                                    C.c_uint32, C.POINTER(C.c_uint32))
        injected = {}
        @callback_type
        def read(_, name, dst, cap, got):
            try:
                rel = name.decode()
                data = injected.get(rel)
                if data is None:
                    data = (pack_path / rel).read_bytes()
                if len(data) > cap:
                    return -1
                C.memmove(dst, data, len(data))
                got[0] = len(data)
                return 0
            except OSError:
                return -1
        lib.nano_resource_hash.argtypes = [C.c_char_p]
        lib.nano_resource_hash.restype = C.c_uint64
        lib.nano_pack_open.argtypes = [C.c_void_p, callback_type, C.c_void_p]
        lib.nano_pack_find.argtypes = [C.c_void_p, C.c_uint64, C.POINTER(Resource)]
        lib.nano_pack_load.argtypes = [C.c_void_p, C.POINTER(Resource), C.c_void_p, C.c_uint32]
        lib.nano_pack_read.argtypes = [C.c_void_p, C.POINTER(Resource), C.c_uint32,
                                      C.c_void_p, C.c_uint32]
        assert lib.nano_pack_open(memory, read, None) == 0
        catalog = json.loads((pack_path/'catalog.json').read_text())
        count = 0
        rng = random.Random(7)
        with zipfile.ZipFile(archive_path) as archive:
            for item in catalog:
                name = item['name'].encode()
                key = lib.nano_resource_hash(name)
                assert key == int(item['hash'], 16)
                assert key == lib.nano_resource_hash(b'__OTR__'+name)
                res = Resource()
                assert lib.nano_pack_find(memory, key, C.byref(res)) == 0, item
                original = archive.read(item['name'])
                assert res.size == len(original)
                data = C.create_string_buffer(res.size)
                assert lib.nano_pack_load(memory, C.byref(res), data, res.size) == 0, item
                assert data.raw == original, item
                if res.size:
                    offset = rng.randrange(res.size)
                    length = rng.randrange(res.size - offset + 1)
                    part = C.create_string_buffer(length)
                    assert lib.nano_pack_read(memory, C.byref(res), offset, part, length) == 0
                    assert part.raw == original[offset:offset+length]
                    assert lib.nano_pack_load(memory, C.byref(res), data, res.size-1) != 0
                assert lib.nano_pack_read(memory, C.byref(res), res.size, data, 1) != 0
                assert lib.nano_pack_read(memory, C.byref(res), 0xffffffff, data, 1) != 0
                count += 1

        # Corrupt every byte in each format's header and a payload byte.
        item = next(i for i in catalog if i['size'] > 100)
        key = int(item['hash'], 16)
        assert lib.nano_pack_open(memory, read, None) == 0
        res = Resource()
        assert lib.nano_pack_find(memory, key, C.byref(res)) == 0
        fixtures = [('pack.nsp', 60), (f'{key >> 56:02x}/index.nsi', 32),
                    (f'{key >> 56:02x}/{res.offset // 65536:06x}.nsd', 28)]
        corruptions = 0
        def attempt():
            error = lib.nano_pack_open(memory, read, None)
            if error:
                return error
            error = lib.nano_pack_find(memory, key, C.byref(res))
            if error:
                return error
            data = C.create_string_buffer(res.size)
            return lib.nano_pack_load(memory, C.byref(res), data, res.size)
        for path, header in fixtures:
            source = (pack_path/path).read_bytes()
            for pos in [*range(header), len(source)-1]:
                damaged = bytearray(source)
                damaged[pos] ^= 1
                injected[path] = bytes(damaged)
                assert attempt() != 0, (path, pos)
                corruptions += 1
            for length in [0, 1, header-1, len(source)-1]:
                injected[path] = source[:length]
                assert attempt() != 0, (path, length)
                corruptions += 1
            injected[path] = source + b'oversize'
            assert attempt() != 0
            injected.clear()
        assert attempt() == 0
        print(f'{count} resources match O2R byte for byte; random ranges and '
              f'{corruptions} corruption/truncation cases passed. '
              f'C workspace: {lib.workspace_size()} bytes.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('pack', type=Path)
    args = parser.parse_args()
    run(args.archive, args.pack)
