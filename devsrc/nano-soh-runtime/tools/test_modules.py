#!/usr/bin/env python3
"""Compare loaded ARM modules byte-for-byte with GNU ld at two addresses."""
import ctypes as C
import io
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib

from pack_module import pack
from build_resources import resource_hash
from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parents[1]


class Info(C.Structure):
    _fields_ = [(x, C.c_uint32) for x in
                ('image', 'memory', 'file', 'exports', 'export_offset', 'ready', 'crc32')]


class Symbol(C.Structure):
    _fields_ = [('address', C.c_uint32), ('thumb', C.c_uint32)]


READ = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_uint32, C.c_void_p, C.c_uint32)
RESOLVE = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_uint64, C.POINTER(Symbol))
SYNC = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_void_p, C.c_uint32)


def main():
    with tempfile.TemporaryDirectory(prefix='nano-modules-test-') as tmp:
        tmp = Path(tmp)
        shared = tmp/'module.so'
        subprocess.run(['cc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-shared', '-fPIC', str(ROOT/'platform/nano_module.c'),
                        '-o', str(shared)], check=True)
        lib = C.CDLL(str(shared))
        lib.nano_module_load.argtypes = [READ, C.c_void_p, RESOLVE, C.c_void_p,
            SYNC, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint32, C.POINTER(Info)]
        lib.nano_module_export.argtypes = [READ, C.c_void_p, C.POINTER(Info),
                                           C.c_uint32, C.c_uint64, C.POINTER(Symbol)]
        assembly = tmp/'fixture.s'
        assembly.write_text('''
.syntax unified
.thumb
.section .text.probe,"ax",%progbits
.global probe
.type probe,%function
.thumb_func
probe:
 push {lr}
 bl external_fn
 bl helper
 pop {lr}
 b.w external_tail
.size probe,.-probe
.section .text.helper,"ax",%progbits
.global helper
.type helper,%function
.thumb_func
helper:
 bx lr
.size helper,.-helper
.section .data,"aw",%progbits
.balign 4
.global pointer_table
pointer_table:
 .word external_data+4
 .word probe
 .word storage+12
 .word pointer_table-4
.section .bss,"aw",%nobits
.balign 16
.global storage
storage: .space 40
''')
        obj = tmp/'fixture.o'
        subprocess.run(['arm-none-eabi-as', '-mcpu=cortex-a8', str(assembly), '-o', str(obj)], check=True)
        pack(obj, tmp/'fixture.nsm')
        sanitizer = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        subprocess.run(['cc', '-std=c99', '-g', '-O1', '-Wall', '-Wextra', '-Werror',
            *sanitizer, '-I'+str(ROOT/'platform'), str(ROOT/'platform/nano_module.c'),
            str(ROOT/'tools/test_module_safety.c'), '-o', str(tmp/'safety')], check=True)
        subprocess.run([str(tmp/'safety'), str(tmp/'fixture.nsm')], check=True)
        subprocess.run(['cc', '-std=c11', '-g', '-O1', '-Wall', '-Wextra', '-Werror',
            *sanitizer, '-I'+str(ROOT/'platform'), str(ROOT/'platform/nano_module.c'),
            str(ROOT/'platform/nano_modules.c'), str(ROOT/'tools/test_module_ownership.c'),
            '-o', str(tmp/'ownership')], check=True)
        subprocess.run([str(tmp/'ownership'), str(tmp/'fixture.nsm')], check=True)
        subprocess.run(['cc', '-std=c11', '-g', '-O1', '-Wall', '-Wextra', '-Werror',
            *sanitizer, '-I'+str(ROOT/'platform'), str(ROOT/'platform/nano_module_pages.c'),
            str(ROOT/'tools/test_module_pages.c'), '-o', str(tmp/'pages')], check=True)
        subprocess.run([str(tmp/'pages')], check=True)
        flags = [f.replace('@ROOT@', str(ROOT)) for f in
                 json.loads((ROOT/'platform/compile_flags.json').read_text())]
        host_flags = []
        previous = False
        for flag in flags:
            if previous or flag == '-include' or flag.startswith(('-I', '-D')):
                host_flags.append(flag)
            previous = flag == '-include'
        subprocess.run(['cc', '-std=gnu11', '-g', '-O1', '-Wall', '-Wextra', '-Werror',
            *sanitizer, *host_flags, '-DSHIP_NANO_OVERLAYS',
            str(ROOT/'platform/nano_actor_db.c'), str(ROOT/'tools/test_actor_modules.c'),
            '-o', str(tmp/'actors')], check=True)
        subprocess.run([str(tmp/'actors')], check=True)
        subprocess.run(['cc', '-std=gnu11', '-g', '-O1', '-Wall', '-Wextra', '-Werror',
            *sanitizer, *host_flags, '-DSHIP_NANO_OVERLAYS',
            *[str(ROOT/'platform'/name) for name in ('nano_module.c', 'nano_modules.c',
                'nano_actor_db.c', 'nano_actor_loader.c')],
            str(ROOT/'tools/test_actor_loader.c'), '-o', str(tmp/'actor-loader')], check=True)
        subprocess.run([str(tmp/'actor-loader')], check=True)
        paths = [obj]
        manifest = ROOT/'build/modules/manifest.json'
        reports = json.loads(manifest.read_text())['modules']
        paths.extend(ROOT/'build/modules'/(name+'.rel.o') for name in reports)
        total = 0
        for path in paths:
            binary, info = pack(path)
            if path != obj:
                assert binary == path.with_name(path.name.replace('.rel.o', '.nsm')).read_bytes()
            # NSM1 preserves each input section byte-for-byte. Disable GNU
            # string merging in the reference object so ld tests relocation,
            # rather than coalescing/reordering these sections differently.
            reference_object = bytearray(path.read_bytes())
            section_headers = struct.unpack_from('<I', reference_object, 32)[0]
            section_size, section_count = struct.unpack_from('<HH', reference_object, 46)
            for index in range(section_count):
                at = section_headers + index * section_size + 8
                flags = struct.unpack_from('<I', reference_object, at)[0]
                struct.pack_into('<I', reference_object, at, flags & ~0x30)
            (tmp/'reference.o').write_bytes(reference_object)
            # Infer code/data imports from real cross-module exports; missing
            # services are artificial addresses ONLY for relocation comparison.
            # No fixture/import code is ever executed, or shipped to the nano.
            functions = {name for r in reports.values() for name, e in r['exports'].items() if e['thumb']}
            ni, ne, nr = struct.unpack_from('<III', binary, 24)
            hashes = [struct.unpack_from('<Q', binary, 48+i*8)[0] for i in range(ni)]
            call_hashes = set()
            for i in range(nr):
                off, kind, source, target, addend = struct.unpack_from('<IIIIi', binary, 48+ni*8+ne*16+i*20)
                if kind in (10, 30) and source == 2:
                    call_hashes.add(hashes[target])
            for base in (0x09000000, 0x09200000):
                lookups = {resource_hash(name): (0x09100000+i*16,
                            int(name in functions or resource_hash(name) in call_hashes))
                           for i, name in enumerate(info['imports'])}
                current = binary
                fail_resolve = False
                fail_sync = False
                synced = []

                @READ
                def read(_, at, dst, count):
                    if at + count > len(current):
                        return -1
                    C.memmove(dst, bytes(current[at:at+count]), count)
                    return 0

                @RESOLVE
                def resolve(_, key, out):
                    if fail_resolve or key not in lookups:
                        return -1
                    out[0].address, out[0].thumb = lookups[key]
                    return 0

                @SYNC
                def sync(_, memory, size):
                    synced.append(size)
                    return int(fail_sync)

                buf = C.create_string_buffer(info['memory_bytes'] + 32)
                address = (C.addressof(buf) + 15) & ~15
                actual = Info()

                def load(capacity=None, target=base):
                    return lib.nano_module_load(read, None, resolve, None, sync, None,
                        address, info['memory_bytes'] if capacity is None else capacity,
                        target, C.byref(actual))

                rc = load()
                assert rc == 0, (path.name, rc)
                assert actual.ready and synced == [info['memory_bytes']]
                script = ['SECTIONS {']
                for s in info['sections']:
                    script.append(f' {s["name"]} 0x{base+s["offset"]:x} : '
                                  f'SUBALIGN({s["alignment"]}) {{ *({s["name"]}) }}')
                script.append('}')
                import_asm = ['.syntax unified', '.thumb']
                for name in info['imports']:
                    addr, thumb = lookups[resource_hash(name)]
                    import_asm.append(f'.global {name}')
                    if thumb:
                        import_asm.extend([f'.type {name},%function', f'.thumb_set {name},0x{addr:x}'])
                    else:
                        import_asm.extend([f'.type {name},%object', f'.set {name},0x{addr:x}'])
                (tmp/'imports.s').write_text('\n'.join(import_asm)+'\n')
                subprocess.run(['arm-none-eabi-as', '-mcpu=cortex-a8', str(tmp/'imports.s'),
                                '-o', str(tmp/'imports.o')], check=True)
                (tmp/'reference.ld').write_text('\n'.join(script)+'\n')
                # Compare ELF relocations only. GNU's Cortex-A8 page-boundary
                # erratum rewrite creates veneers unrelated to relocation.
                # Hardware CPU/cache/erratum validation remains a launch gate.
                linked = subprocess.run(['arm-none-eabi-ld', '--no-fix-cortex-a8', '-T', str(tmp/'reference.ld'),
                    '-o', str(tmp/'reference.elf'), str(tmp/'reference.o'), str(tmp/'imports.o')], capture_output=True, text=True)
                if linked.returncode:
                    import shutil
                    debug = Path('/tmp/nano-module-link-failure')
                    debug.mkdir(exist_ok=True)
                    for file in tmp.iterdir():
                        if file.is_file(): shutil.copy2(file, debug/file.name)
                    (debug/'object-path.txt').write_text(str(path))
                    raise RuntimeError(linked.stderr + '\n' + '\n'.join(script[:20]))
                oracle = ELFFile(io.BytesIO((tmp/'reference.elf').read_bytes()))
                expected = bytearray(info['memory_bytes'])
                for s in info['sections']:
                    if s['bytes'] and not s['bss']:
                        section = oracle.get_section_by_name(s['name'])
                        assert section['sh_addr'] == base + s['offset']
                        expected[s['offset']:s['offset']+s['bytes']] = section.data()[:s['bytes']]
                oracle_symbols = {s.name: s['st_value'] for s in
                                  oracle.get_section_by_name('.symtab').iter_symbols()}
                for name, entry in info['exports'].items():
                    assert oracle_symbols[name] == ((base+entry['offset']) | entry['thumb'])
                got = C.string_at(address, actual.memory)
                if got != expected:
                    import shutil
                    debug = Path('/tmp/nano-module-byte-failure')
                    debug.mkdir(exist_ok=True)
                    for file in tmp.iterdir():
                        if file.is_file(): shutil.copy2(file, debug/file.name)
                    (debug/'actual.bin').write_bytes(got)
                    (debug/'expected.bin').write_bytes(expected)
                assert got == expected, (path.name, base, next((i for i,(a,b) in enumerate(zip(got,expected)) if a!=b), -1))
                for name, entry in info['exports'].items():
                    symbol = Symbol()
                    assert lib.nano_module_export(read, None, C.byref(actual), base,
                        entry['hash'], C.byref(symbol)) == 0
                    assert (symbol.address, symbol.thumb) == (base+entry['offset'], entry['thumb'])
                total += 1
                if path != obj:
                    continue
                # All truncations and single-byte corruption of original test
                # material must be rejected; no stale ready state may survive.
                for cut in range(len(binary)):
                    current = binary[:cut]
                    assert load() != 0 and not actual.ready, cut
                for index in range(len(binary)):
                    current = bytearray(binary)
                    current[index] ^= 0x80
                    assert load() != 0 and not actual.ready, index
                current = binary
                assert load(info['memory_bytes']-1) == 4 and not actual.ready
                assert load(target=base+1) == 4 and not actual.ready
                assert load(target=0xfffffff0) == 4 and not actual.ready
                fail_resolve = True
                assert load() == 5 and not actual.ready
                assert C.string_at(address, actual.memory) == bytes(actual.memory)
                fail_resolve = False
                fail_sync = True
                assert load() == 7 and not actual.ready
                fail_sync = False
                assert load() == 0 and actual.ready
                # Valid CRC cannot authorize an unknown relocation or ARM jump.
                current = bytearray(binary)
                reloc_at = 48+ni*8+ne*16
                struct.pack_into('<I', current, reloc_at+4, 255)
                struct.pack_into('<I', current, 36, 0)
                struct.pack_into('<I', current, 36, zlib.crc32(current))
                assert load() == 6 and not actual.ready
        print(f'{total} ARM module/address comparisons match GNU ld, including every export and BSS.')
        print('All fixture truncations/corruptions, capacity/import/cache failures and recovery passed.')


if __name__ == '__main__':
    main()
