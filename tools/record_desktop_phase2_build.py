#!/usr/bin/env python3
"""Record the exact local role-probe build and a bounded source snapshot."""
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path
import shutil
import subprocess
import sys

repo, build, output = map(Path, sys.argv[1:])
files = [
    'firmware/usbhost/role_probe.h', 'firmware/usbhost/role_probe.c',
    'firmware/export/usb_host_probe.h', 'firmware/export/usb.h',
    'firmware/export/config/ipod6g.h', 'firmware/usb.c', 'firmware/SOURCES',
    'firmware/target/arm/s5l8702/usb-host-probe.c',
    'firmware/target/arm/s5l8702/usb-s5l8702.c',
    'firmware/export/usb-designware.h', 'apps/debug_menu.c',
    'tools/tests/desktop_phase2_role.c', 'tools/desktop_phase2_role_tests.sh',
    'tools/build_desktop_phase2_role.sh', 'tools/record_desktop_phase2_build.py',
]
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

sources = {}
for name in files:
    destination = output / 'source-snapshot' / name
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(repo / name, destination)
    sources[name] = digest(destination)
metadata = output / 'build-metadata'
metadata.mkdir(exist_ok=True)
for name in ['Makefile', 'autoconf.h', 'rockbox.map']:
    shutil.copy2(build / name, metadata / name)
report = {
    'stage': 'Phase 2A: role probe only; physical result pending',
    'target': 'ipod6g', 'build_define': 'USB_HOST_ROLE_PROBE_BUILD',
    'recorded_utc': datetime.now(timezone.utc).isoformat(),
    'build_directory': str(build.resolve()),
    'base_commit': subprocess.check_output(['git', '-C', str(repo),
                                          'rev-parse', 'HEAD'], text=True).strip(),
    'working_tree': 'Personal tree contains unrelated changes; no upstream patchset',
    'artifacts': {name: digest(output / name) for name in
                  ['rockbox.ipod', 'rockbox.elf', 'rockbox-info.txt']},
    'sources': sources,
    'hardware_tested': False,
    'deployed': False,
}
(output / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n')
