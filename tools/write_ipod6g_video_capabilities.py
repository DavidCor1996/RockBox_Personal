#!/usr/bin/env python3
"""Record video capabilities only for a verified deployment of a local 6G build."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / 'rockpod'))
from services.video_capabilities import contract_definition, contract_digest
from services.file_safety import atomic_write_text


def checksum(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_record(build, mount):
    info_text = (build / 'rockbox-info.txt').read_text()
    info = dict(line.split(':', 1) for line in info_text.splitlines() if ':' in line)
    if info.get('Target', '').strip() != 'ipod6g':
        raise RuntimeError('Expected a local iPod 6G build')
    if (mount / '.rockbox/rockbox-info.txt').read_text() != info_text:
        raise RuntimeError('Installed and local build identities differ')
    firmware_hash = checksum(build / 'rockbox.ipod')
    for relative in ('rockbox.ipod', '.rockbox/rockbox.ipod'):
        if checksum(mount / relative) != firmware_hash:
            raise RuntimeError('Installed firmware differs: ' + relative)
    symbols = subprocess.check_output(
        ['arm-elf-eabi-nm', '--defined-only', str(build / 'rockbox.elf')], text=True)
    names = {line.split()[-1] for line in symbols.splitlines() if line.split()}
    if not {'video_h264_play', 'vpu_h264_decode_sample'} <= names:
        raise RuntimeError('Local firmware has no linked hardware H.264 player')
    # Tie the ELF evidence to the exact binary copied to both boot locations.
    binary = subprocess.check_output(
        ['arm-elf-eabi-objcopy', '-O', 'binary', str(build / 'rockbox.elf'), '/dev/stdout'])
    # Rockbox iPod images prepend an eight-byte checksum/model header.
    if (build / 'rockbox.ipod').read_bytes()[8:] != binary:
        raise RuntimeError('Local ELF does not match the deployed firmware image')
    definition = contract_definition()
    record = {
        'schema': 1, 'contract_sha256': contract_digest(),
        'build': info['Version'].strip(), 'target': 'ipod6g',
        'firmware_sha256': firmware_hash,
        'limits': definition['limits'],
        'capabilities': definition['targets']['ipod6g'],
        'qualification': definition['qualification'],
    }
    atomic_write_text(mount / '.rockbox/video-capabilities.json',
                      json.dumps(record, indent=2) + '\n')
    print('video capabilities: recorded verified local 6G build ' + record['build'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('mount', type=Path)
    args = parser.parse_args()
    write_record(args.build, args.mount)


if __name__ == '__main__':
    main()
