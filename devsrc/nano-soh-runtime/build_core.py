#!/usr/bin/env python3
"""Compile the native C game without linking desktop Shipwright dependencies."""
from concurrent.futures import ThreadPoolExecutor
import json
import hashlib
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
SHIP = ROOT / 'Shipwright'
BUILD = ROOT / 'build/core'


def build_inputs():
    """Shared headers affect every module ABI; reject stale objects later."""
    files = [ROOT/'platform/compile_flags.json']
    for directory in (SHIP/'soh', SHIP/'libultraship/include', ROOT/'platform'):
        files.extend(p for p in directory.rglob('*')
                     if p.is_file() and p.suffix in ('.h', '.inc'))
    records = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
               for p in sorted(set(files))}
    return records


def source_files():
    excluded_dirs = ('src/dmadata/', 'src/elf_message/', 'src/libultra/io/',
                     'src/libultra/libc/', 'src/libultra/os/', 'src/libultra/rmon/')
    excluded_gu = ('cosf', 'lookat', 'lookathil', 'perspective', 'position',
                   'sinf', 'sqrtf', 'us2dex')
    sources = [p for p in (SHIP/'soh/src').rglob('*.c')
               if not str(p.relative_to(SHIP/'soh')).startswith(excluded_dirs)
               and not (p.parent.name == 'gu' and p.stem in excluded_gu)]
    sources += [SHIP/'soh/soh/stubs.c', SHIP/'soh/soh/gu_pc.c', SHIP/'soh/soh/mixer.c']
    sources += [ROOT/'platform'/name for name in
                ('nano_actor_db.c', 'nano_gbi.c', 'nano_os_mesg.c', 'nano_pack.c',
                 'nano_memory.c', 'nano_audio_memory.c', 'nano_assets.c',
                 'nano_resource_bridge.c', 'nano_scene.c', 'nano_renderer.c',
                 'nano_combiner.c', 'nano_gles.c')]
    return sorted(sources)


def compile_one(source):
    relative = source.relative_to(SHIP/'soh') if SHIP in source.parents else source.relative_to(ROOT)
    out = BUILD/relative.with_suffix('.o')
    out.parent.mkdir(parents=True, exist_ok=True)
    log = out.with_suffix('.log')
    flags = [f.replace('@ROOT@',str(ROOT)) for f in
             json.loads((ROOT/'platform/compile_flags.json').read_text())]
    if SHIP not in source.parents:
        flags = [f for f in flags if not f.startswith('-Wno-error=')]
        flags += ['-Wall', '-Wextra', '-Werror']
    with log.open('w') as stream:
        result = subprocess.run(['arm-none-eabi-gcc', *flags, '-fmax-errors=4',
                                 '-c', str(source), '-o', str(out)],
                                stdout=stream, stderr=stream)
    errors = [line for line in log.read_text().splitlines() if 'error:' in line]
    return {'source':str(relative), 'object':str(out.relative_to(ROOT)),
            'ok':result.returncode == 0, 'errors':errors,
            'sha256':hashlib.sha256(source.read_bytes()).hexdigest()}


def main():
    sources = source_files()
    inputs = build_inputs()
    if len(sys.argv) > 1:
        sources = [p for p in sources if any(arg in str(p) for arg in sys.argv[1:])]
    results = []
    with ThreadPoolExecutor(max_workers=8) as executor:
        for result in executor.map(compile_one, sources):
            results.append(result)
    BUILD.mkdir(parents=True,exist_ok=True)
    report = 'compile-filtered.json' if len(sys.argv)>1 else 'compile-report.json'
    (BUILD/report).write_text(json.dumps(results,indent=2)+'\n')
    good = sum(r['ok'] for r in results)
    print(f'Native C objects: {good}/{len(results)} compiled')
    for result in results:
        if not result['ok']:
            print(result['source']+': '+(result['errors'][0] if result['errors'] else 'compiler failed'))
    if good == len(results) and len(sys.argv)==1:
        if inputs != build_inputs():
            raise ValueError('headers or build flags changed during compilation; rebuild')
        (BUILD/'build-inputs.json').write_text(json.dumps(inputs, indent=2)+'\n')
        combined = ROOT/'build/oot-core.rel.o'
        subprocess.run(['arm-none-eabi-ld','-r','-o',str(combined),
                        *[str(ROOT/r['object']) for r in results]],check=True)
        unresolved = subprocess.check_output(['arm-none-eabi-nm','-u',str(combined)],text=True)
        (ROOT/'build/unresolved.txt').write_text(unresolved)
        sizes = subprocess.check_output(['arm-none-eabi-size',str(combined)],text=True)
        (ROOT/'build/size.txt').write_text(sizes)
        subprocess.run(['arm-none-eabi-ar','rcs',str(ROOT/'build/liboot-nano.a'),
                        *[str(ROOT/r['object']) for r in results]],check=True)
        print(sizes.strip())
        print(f'{len(unresolved.splitlines())} unresolved services (including libc/libgcc).')
        print('Relocatable development object only; no runnable engine was linked.')
    return int(good != len(results))


if __name__ == '__main__':
    sys.exit(main())
