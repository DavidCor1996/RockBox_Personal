#!/usr/bin/env python3
"""Derive audio pool metadata from an owned O2R; never copy asset payloads."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def profile(archive):
    sequences, fonts = {}, {}
    with zipfile.ZipFile(archive) as z:
        for item in z.infolist():
            kind = ('sequence' if item.filename.startswith('audio/sequences/') else
                    'font' if item.filename.startswith('audio/fonts/') else None)
            if kind is None:
                continue
            b = z.read(item)
            magic = b'QESO' if kind == 'sequence' else b'TFSO'
            if len(b) < 68 or b[:4] != bytes(4) or b[4:8] != magic or b[8:12] != struct.pack('<I', 2):
                raise ValueError('unsupported audio resource: ' + item.filename)
            n, = struct.unpack_from('<I', b, 64)
            if kind == 'font':
                if n in fonts:
                    raise ValueError('duplicate font ID')
                fonts[n] = item.filename
                continue
            if n > len(b) - 75:
                raise ValueError('truncated sequence')
            ident, medium, policy, nf = struct.unpack_from('<BBBI', b, 68 + n)
            if len(b) != 75 + n + nf or nf > 16 or policy > 4 or ident in sequences:
                raise ValueError('invalid sequence metadata')
            sequences[ident] = (n, policy, item.filename)
    if set(fonts) != set(range(38)) or set(sequences) != set(range(110)):
        raise ValueError('this development target requires the 38-font, 110-sequence vanilla pack')
    return {'font_count': len(fonts), 'sequence_count': len(sequences),
            'permanent_sequence_bytes': sum((n + 15) & ~15 for n, p, _ in sequences.values() if p == 0),
            'sequence_names': [sequences[i][2] for i in sorted(sequences)],
            'sequence_policies': [sequences[i][1] for i in sorted(sequences)],
            'font_names': [fonts[i] for i in sorted(fonts)],
            'archive_sha256': hashlib.sha256(Path(archive).read_bytes()).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    args = parser.parse_args()
    p = profile(args.archive)
    header = ('/* Generated resource counts only; no game data. */\n'
              '#ifndef NANO_AUDIO_PROFILE_H\n#define NANO_AUDIO_PROFILE_H\n'
              f'#define NANO_AUDIO_FONT_COUNT {p["font_count"]}u\n'
              f'#define NANO_AUDIO_SEQUENCE_COUNT {p["sequence_count"]}u\n'
              f'#define NANO_AUDIO_PERMANENT_SEQUENCE_BYTES {p["permanent_sequence_bytes"]}u\n'
              '#endif\n')
    (ROOT/'platform/nano_audio_profile.h').write_text(header)
    catalog = ['/* Generated names and policies only; no audio payloads. */']
    for kind in ('sequence', 'font'):
        suffix = ' + 15' if kind == 'sequence' else ''
        catalog.append(f'static char *nano_{kind}_names[NANO_AUDIO_{kind.upper()}_COUNT{suffix}] = {{')
        catalog.extend(json.dumps(name) + ',' for name in p[kind+'_names'])
        catalog.append('};')
    catalog.append('static const u8 nano_sequence_policies[] = {' +
                   ','.join(map(str, p['sequence_policies'])) + '};')
    (ROOT/'platform/nano_audio_catalog.inc').write_text('\n'.join(catalog)+'\n')
    (ROOT/'build/audio-profile.json').write_text(json.dumps(p, indent=2)+'\n')
    print(json.dumps({k: v for k, v in p.items() if not isinstance(v, list)}, indent=2))


if __name__ == '__main__':
    main()
