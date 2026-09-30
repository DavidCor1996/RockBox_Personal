#!/usr/bin/env bash
# Section 28 passive diagnostic; no deployment or host-role probe.
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-$(mktemp -d /tmp/rockbox-usb-capability.XXXXXX)}"
artifact_dir="${2:-${repo_root}/.rockpod-private/usb-capability-20260930}"
if [ -e "${build_dir}/Makefile" ] || [ -e "${artifact_dir}/manifest.json" ]; then
    echo "Use fresh build and artifact directories; preserving earlier results." >&2
    exit 1
fi
mkdir -p "${build_dir}" "${artifact_dir}"
build_dir="$(cd "${build_dir}" && pwd)"
artifact_dir="$(cd "${artifact_dir}" && pwd)"
cd "${build_dir}"
"${repo_root}/tools/configure" --target=ipod6g --type=n
python3 - <<'PY'
from pathlib import Path
p = Path('Makefile')
s = p.read_text()
assert s.count('export EXTRA_DEFINES=') == 1
p.write_text(s.replace('export EXTRA_DEFINES=',
                      'export EXTRA_DEFINES= -DUSB_CAPABILITY_SNAPSHOT_BUILD '))
PY
make -j"${JOBS:-4}" bin
cp rockbox.ipod rockbox.elf rockbox-info.txt "${artifact_dir}/"
python3 - "${repo_root}" "${build_dir}" "${artifact_dir}" <<'PY'
import hashlib
import json
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
repo, build, output = map(Path, sys.argv[1:])
names = [
    'firmware/export/usb_dw_capabilities.h', 'firmware/usbhost/capabilities.c',
    'firmware/drivers/usb-designware.c',
    'firmware/target/arm/s5l8702/usb-s5l8702.c',
    'firmware/export/usb-designware.h', 'firmware/export/config/ipod6g.h',
    'firmware/SOURCES', 'apps/debug_menu.c',
    'tools/tests/usb_dw_capabilities.c',
    'tools/usb_dw_capability_tests.sh',
    'tools/build_usb_capability_snapshot.sh',
    'docs/usb-accessory-capability-checkpoint.md',
]
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
sources = {}
for name in names:
    destination = output / 'source-snapshot' / name
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(repo / name, destination)
    sources[name] = digest(destination)
metadata = output / 'build-metadata'
metadata.mkdir(exist_ok=True)
for name in ['Makefile', 'autoconf.h', 'rockbox.map', 'rbversion.h']:
    shutil.copy2(build / name, metadata / name)
report = {
    'stage': 'Section 28A: passive capabilities; physical evidence pending',
    'target': 'ipod6g', 'build_define': 'USB_CAPABILITY_SNAPSHOT_BUILD',
    'base_commit': subprocess.check_output(
        ['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip(),
    'recorded_utc': datetime.now(timezone.utc).isoformat(),
    'build_directory': str(build), 'sources': sources,
    'artifacts': {name: digest(output / name) for name in
                  ['rockbox.ipod', 'rockbox.elf', 'rockbox-info.txt']},
    'hardware_tested': False, 'deployed': False,
}
(output / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n')
PY
echo "Passive diagnostic ready at ${artifact_dir}; not deployed."
