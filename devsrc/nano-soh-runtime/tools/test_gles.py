#!/usr/bin/env python3
"""Compare fixed-function GPU pixels with an independent combiner equation."""
import argparse
from collections import Counter
import json
from pathlib import Path
import struct
import subprocess
import tempfile
from zipfile import ZipFile

ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive',type=Path)
    args=parser.parse_args()
    modes=Counter()
    with ZipFile(args.archive) as archive:
        for name in archive.namelist():
            b=archive.read(name)
            if len(b)<72 or struct.unpack_from('<I',b,4)[0]!=0x4f444c54:continue
            p=72
            while p<len(b):
                a,c=struct.unpack_from('<II',b,p);p+=8
                if a>>24==0xfc:modes[a,c]+=1
                if a>>24 in (0x20,0x31,0x32,0x33,0x35,0x36,0x42):p+=8
    flags=[x.replace('@ROOT@',str(ROOT)) for x in json.loads((ROOT/'platform/compile_flags.json').read_text())]
    host=[];previous=''
    for flag in flags:
        if previous=='-include' or flag=='-include' or flag.startswith(('-I','-D')):host.append(flag)
        previous=flag
    with tempfile.TemporaryDirectory(prefix='nano-gles-') as work:
        work=Path(work)
        (work/'modes').write_text(''.join(f'{a:x} {b:x} {n}\n' for (a,b),n in modes.items()))
        subprocess.run(['cc','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-DNANO_RENDER_HOST',
            '-fsanitize=address,undefined','-fno-omit-frame-pointer',*host,
            ROOT/'tools/test_gles.c',ROOT/'platform/nano_gles.c',ROOT/'platform/nano_combiner.c',
            ROOT/'platform/nano_renderer.c','-lm','-lGL','-lEGL','-o',work/'test'],check=True)
        subprocess.run([work/'test',work/'modes'],check=True)
if __name__=='__main__':main()
