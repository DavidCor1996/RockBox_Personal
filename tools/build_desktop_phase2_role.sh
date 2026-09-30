#!/usr/bin/env bash
# Build the first USB hardware gate without touching a connected device.
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-$(mktemp -d /tmp/rockbox-role-probe.XXXXXX)}"
artifact_dir="${2:-${repo_root}/.rockpod-private/desktop-phase2}"
if [ -e "${artifact_dir}/manifest.json" ]; then
    echo "Choose a new artifact directory; preserving the recorded build." >&2
    exit 1
fi
mkdir -p "${build_dir}" "${artifact_dir}"
build_dir="$(cd "${build_dir}" && pwd)"
artifact_dir="$(cd "${artifact_dir}" && pwd)"
if [ -e "${build_dir}/Makefile" ]; then
    echo "Use a fresh build directory to avoid mixing diagnostic flags." >&2
    exit 1
fi
cd "${build_dir}"
"${repo_root}/tools/configure" --target=ipod6g --type=n
python3 - <<'PY'
from pathlib import Path
p = Path('Makefile')
s = p.read_text()
assert s.count('export EXTRA_DEFINES=') == 1
p.write_text(s.replace('export EXTRA_DEFINES=',
                      'export EXTRA_DEFINES= -DUSB_HOST_ROLE_PROBE_BUILD '))
PY
make -j"${JOBS:-4}" bin
cp rockbox.ipod rockbox.elf rockbox-info.txt "${artifact_dir}/"
python3 "${repo_root}/tools/record_desktop_phase2_build.py" \
    "${repo_root}" "${build_dir}" "${artifact_dir}"
echo "Test build ready at ${artifact_dir}; not deployed."
