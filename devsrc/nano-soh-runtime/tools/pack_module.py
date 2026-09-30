#!/usr/bin/env python3
"""Convert a native ARM ET_REL into bounded, checked NSM1 module storage."""
import argparse
import io
import json
from pathlib import Path
import struct
import zlib

from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from build_resources import resource_hash


def align(value, boundary=16):
    return (value + boundary - 1) & -boundary


def branch_addend(data, at):
    hi, lo = struct.unpack_from('<HH', data, at)
    sign = (hi >> 10) & 1
    i1 = 1 ^ ((lo >> 13) & 1) ^ sign
    i2 = 1 ^ ((lo >> 11) & 1) ^ sign
    value = (sign << 24) | (i1 << 23) | (i2 << 22)
    value |= (hi & 1023) << 12 | (lo & 2047) << 1
    return value - (1 << 25) if sign else value


def pack(source, output=None):
    data = Path(source).read_bytes()
    elf = ELFFile(io.BytesIO(data))
    if (elf.elfclass != 32 or not elf.little_endian or
            elf['e_machine'] != 'EM_ARM' or elf['e_type'] != 'ET_REL'):
        raise ValueError('expected little-endian ARM32 relocatable object')
    sections = list(elf.iter_sections())
    allocated = [(i, s) for i, s in enumerate(sections) if s['sh_flags'] & 2]
    offsets, cursor = {}, 0
    # Use this exact ordering in the reference GNU ld script in the tests.
    ordered = sorted(allocated, key=lambda p: p[1]['sh_type'] == 'SHT_NOBITS')
    image = bytearray()
    bss_started = False
    for index, section in ordered:
        if section['sh_flags'] & 0x400 or section['sh_type'] not in ('SHT_PROGBITS', 'SHT_NOBITS'):
            raise ValueError(f'unsupported allocated section {section.name}')
        boundary = max(section['sh_addralign'], 1)
        if boundary > 16 or boundary & (boundary - 1):
            raise ValueError('unsupported section alignment')
        if section['sh_type'] == 'SHT_NOBITS' and not bss_started:
            cursor = align(cursor)
            bss_started = True
        cursor = align(cursor, boundary)
        offsets[index] = cursor
        cursor += section['sh_size']
        if section['sh_type'] != 'SHT_NOBITS':
            image.extend(bytes(offsets[index] - len(image)))
            image.extend(section.data())
    image_bytes = align(len(image))
    image.extend(bytes(image_bytes - len(image)))
    memory_bytes = align(max(cursor, image_bytes))
    symbols = elf.get_section_by_name('.symtab')
    imports, exports, fixups = {}, {}, []
    all_hashes = {}

    def name_hash(name):
        value = resource_hash(name)
        if value in all_hashes and all_hashes[value] != name:
            raise ValueError('symbol hash collision')
        all_hashes[value] = name
        return value

    def location(symbol):
        section = symbol['st_shndx']
        if section not in offsets:
            raise ValueError(f'unsupported symbol section: {symbol.name} {section}')
        thumb = int(symbol['st_info']['type'] == 'STT_FUNC' and symbol['st_value'] & 1)
        return offsets[section] + (symbol['st_value'] & ~thumb), thumb

    for symbol in symbols.iter_symbols():
        if symbol['st_shndx'] in offsets and symbol['st_info']['bind'] in ('STB_GLOBAL', 'STB_WEAK'):
            offset, thumb = location(symbol)
            if symbol.name in exports:
                raise ValueError('duplicate exported symbol')
            exports[symbol.name] = {'hash': name_hash(symbol.name), 'offset': offset, 'thumb': thumb}
    for relocations in sections:
        if not isinstance(relocations, RelocationSection) or relocations['sh_info'] not in offsets:
            continue
        if relocations.is_RELA():
            raise ValueError('RELA not supported')
        target_section = relocations['sh_info']
        for reloc in relocations.iter_relocations():
            kind = reloc['r_info_type']
            if kind == 0:
                continue
            if kind not in (2, 10, 30):
                raise ValueError(f'unsupported ARM relocation {kind}')
            at = offsets[target_section] + reloc['r_offset']
            if reloc['r_offset'] + 4 > sections[target_section]['sh_size'] or at + 4 > len(image):
                raise ValueError('relocation out of bounds')
            symbol = symbols.get_symbol(reloc['r_info_sym'])
            if symbol['st_shndx'] == 'SHN_UNDEF':
                if not symbol.name or symbol['st_info']['bind'] == 'STB_WEAK':
                    raise ValueError('unresolved weak/anonymous symbol')
                imports[symbol.name] = name_hash(symbol.name)
                source_kind, target = 2, symbol.name
            else:
                target, source_kind = location(symbol)
            addend = struct.unpack_from('<i', image, at)[0] if kind == 2 else branch_addend(image, at)
            fixups.append((at, kind, source_kind, target, addend))
    fixups.sort()
    for left, right in zip(fixups, fixups[1:]):
        if left[0] + 4 > right[0]:
            raise ValueError('overlapping relocations')
    import_names = sorted(imports, key=imports.get)
    import_ids = {name: i for i, name in enumerate(import_names)}
    import_data = b''.join(struct.pack('<Q', imports[n]) for n in import_names)
    export_data = b''.join(struct.pack('<QII', e['hash'], e['offset'], e['thumb'])
                           for e in sorted(exports.values(), key=lambda x: x['hash']))
    relocation_data = b''.join(struct.pack('<IIIIi', at, kind, sk,
        import_ids[target] if sk == 2 else target, addend)
        for at, kind, sk, target, addend in fixups)
    payload = import_data + export_data + relocation_data + image
    header = struct.pack('<4s11I', b'NSM1', 1, 48 + len(payload), image_bytes,
                         memory_bytes - image_bytes, 16, len(imports), len(exports),
                         len(fixups), 0, 0, 0)
    result = bytearray(header + payload)
    struct.pack_into('<I', result, 36, zlib.crc32(result))
    metadata = {'image_bytes': image_bytes, 'memory_bytes': memory_bytes,
                'crc32': struct.unpack_from('<I', result, 36)[0],
                'file_bytes': len(result), 'relocations': len(fixups),
                'imports': import_names, 'exports': exports,
                'sections': [{'name': s.name, 'offset': offsets[i],
                              'alignment': max(s['sh_addralign'], 1),
                              'bytes': s['sh_size'], 'bss': s['sh_type'] == 'SHT_NOBITS'}
                             for i, s in ordered]}
    if output:
        Path(output).write_bytes(result)
    return result, metadata


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('object', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    _, report = pack(args.object, args.output)
    args.output.with_suffix('.json').write_text(json.dumps(report, indent=2) + '\n')
