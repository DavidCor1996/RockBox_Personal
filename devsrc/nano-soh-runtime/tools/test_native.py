#!/usr/bin/env python3
"""Build host-side platform tests with ASan and UBSan and original test data."""
import json
from pathlib import Path
import subprocess
import tempfile
import zipfile
from build_resources import build

ROOT = Path(__file__).resolve().parents[1]
flags = json.loads((ROOT/'platform/compile_flags.json').read_text())
flags = [f.replace('@ROOT@', str(ROOT)) for f in flags]
host_flags = []
keep_next = False
for flag in flags:
    if keep_next or flag == '-include' or flag.startswith(('-I', '-D')):
        host_flags.append(flag)
    keep_next = flag == '-include'
with tempfile.TemporaryDirectory(prefix='nano-platform-test-') as work:
    work = Path(work)
    with zipfile.ZipFile(work/'test.o2r', 'w') as archive:
        archive.writestr('version', b'original test version')
        archive.writestr('portVersion', b'original test port')
        archive.writestr('test/cross-page', bytes(range(256))*512+bytes(range(17)))
    build(work/'test.o2r', work/'pack')
    sources = [ROOT/'tools/test_native.c'] + [ROOT/'platform'/n for n in
               ('nano_actor_db.c', 'nano_os_mesg.c', 'nano_pack.c')]
    subprocess.run(['cc', '-std=gnu11', '-g', '-O1', '-Wall', '-Wextra', '-Werror', '-fmax-errors=4',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    *host_flags, *map(str, sources), '-o', str(work/'test')], check=True)
    subprocess.run([str(work/'test'), str(work/'pack')], check=True)
