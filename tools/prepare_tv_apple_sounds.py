#!/usr/bin/env python3
"""Convert original Apple TV 3.0.1 navigation recordings to resident PCM."""
import argparse, hashlib, json, struct, subprocess
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('source', type=Path)
p.add_argument('header', type=Path)
p.add_argument('manifest', type=Path)
a = p.parse_args()
code = ['/* Original Apple TV 3.0.1 BackRow recordings, copyright Apple.',
        ' * See docs/tv-apple-sounds.json. Converted to stereo signed PCM. */']
records = []
for symbol, filename in [('move', 'SelectionChange.aif'),
                         ('select', 'Selection.aif'), ('back', 'Exit.aif')]:
    path = a.source / filename
    data = subprocess.check_output(['ffmpeg', '-v', 'error', '-i', str(path),
                                   '-ac', '2', '-ar', '44100',
                                   '-f', 's16le', '-'])
    samples = struct.unpack('<' + 'h' * (len(data) // 2), data)
    code.append('static const int16_t tv_apple_sound_' + symbol + '[] = {\n' +
                ',\n'.join(','.join(map(str, samples[i:i+16]))
                            for i in range(0, len(samples), 16)) + '\n};')
    records.append({'file': filename, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                    'frames': len(samples) // 2, 'pcm_sha256': hashlib.sha256(data).hexdigest()})
a.header.write_text('\n'.join(code) + '\n')
a.manifest.write_text(json.dumps({
    'source': 'Apple TV 3.0.1 (2009), OSBoot/System/Library/PrivateFrameworks/BackRow.framework/Versions/A/Resources',
    'download': 'https://mesu.apple.com/data/OS/061-7491.20091107.TVA31/2Z694-6004-003.dmg',
    'firmware_sha256': '26d4cf4fe0abca2068672cc70fcf366eaa0737066c3710519b0a0bcbee82a1bf',
    'conversion': '44100 Hz signed 16-bit stereo PCM; original duration and recording, no synthesized tones',
    'assets': records}, indent=2) + '\n')
print('Converted three original Apple navigation sounds')
