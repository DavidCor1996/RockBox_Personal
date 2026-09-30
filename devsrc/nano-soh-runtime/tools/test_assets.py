#!/usr/bin/env python3
"""Exercise the native decoder on the user's external resource pack."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pack', type=Path)
    args = parser.parse_args()
    flags = [x.replace('@ROOT@',str(ROOT)) for x in json.loads((ROOT/'platform/compile_flags.json').read_text())]
    host = []
    previous = ''
    for flag in flags:
        if previous == '-include' or flag == '-include' or flag.startswith(('-I', '-D')):
            host.append(flag)
        previous = flag
    supported = {0x4f544558,0x4f444c54,0x4f415252,0x4f414e4d,0x4f50414d,
                 0x4f434f4c,0x4f534b4c,0x4f534c42,0x4f505448,0x4f4d5458,0x4f424c42,0x4f424749,
                 0x4f524f4d,0x4f435654,0x4f534d50,0x4f534654,0x4f534551}
    catalog = json.loads((args.pack/'catalog.json').read_text())
    with tempfile.TemporaryDirectory(prefix='nano-assets-') as work:
        work = Path(work)
        names = [x['name'] for x in catalog if int(x['type'], 16) in supported]
        assert names
        (work/'names').write_text('\n'.join(names)+'\n')
        subprocess.run(['cc','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror',
                        '-fsanitize=address,undefined','-fno-omit-frame-pointer',*host,
                        ROOT/'tools/test_assets.c', ROOT/'platform/nano_assets.c',
                        ROOT/'platform/nano_resource_bridge.c', ROOT/'platform/nano_memory.c',
                        ROOT/'platform/nano_pack.c','-o',work/'test'],check=True)
        subprocess.run([work/'test',args.pack,work/'names'],check=True)


if __name__ == '__main__':
    main()
