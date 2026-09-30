#!/usr/bin/env python3
"""Validate native rendering on host; no probe or device installation."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('pack', type=Path)
    p.add_argument('--contains', default='scenes/shared/spot04_scene/')
    args = p.parse_args()
    flags = [x.replace('@ROOT@', str(ROOT)) for x in json.loads((ROOT/'platform/compile_flags.json').read_text())]
    host=[]
    previous=''
    for flag in flags:
        if previous == '-include' or flag == '-include' or flag.startswith(('-I','-D')): host.append(flag)
        previous=flag
    catalog=json.loads((args.pack/'catalog.json').read_text())
    names=[x['name'] for x in catalog if int(x['type'],16)==0x4f444c54 and args.contains in x['name']]
    assert names
    with tempfile.TemporaryDirectory(prefix='nano-renderer-') as work:
        work=Path(work)
        (work/'names').write_text('\n'.join(names)+'\n')
        objects=[]
        for name in ('z_scene_table','z_rcp','graph','z_actor','TwoHeadArena'):
            out=work/(name+'.o')
            subprocess.run(['cc','-std=gnu11','-O1','-g','-w','-Wno-error=int-conversion',
                '-Wno-error=incompatible-pointer-types','-fsanitize=address,undefined',
                '--param=asan-globals=0',
                '-ffunction-sections','-fdata-sections',*host,'-c',
                ROOT/f'Shipwright/soh/src/code/{name}.c','-o',out],check=True)
            objects.append(out)
        subprocess.run(['cc','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                        '-fno-omit-frame-pointer','-ffunction-sections','-fdata-sections','-Wl,--gc-sections',*host,ROOT/'tools/test_renderer.c',
                        ROOT/'platform/nano_renderer.c',ROOT/'platform/nano_assets.c',
                        ROOT/'platform/nano_pack.c',ROOT/'platform/nano_gbi.c',
                        ROOT/'platform/nano_resource_bridge.c',ROOT/'platform/nano_memory.c',
                        *objects,'-lm','-o',work/'test'],check=True)
        subprocess.run([work/'test',args.pack,work/'names'],check=True)

if __name__=='__main__': main()
