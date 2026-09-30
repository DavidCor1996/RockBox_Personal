#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/ipodjs_navigation_sim_regression.sh"
mkdir -p "$out_dir"
trap cleanup EXIT INT TERM HUP
export ROCKPOD_SIM_USB_EJECT_SECONDS=4
prepare_root
launch_sim
wait_for_home
before=$(trace_last_sequence)
tap_key F11 1.5
wait_for_trace_after "USB Connected" "$before"
capture connected
sleep 4
capture safe-eject
sleep 2
capture safe-eject-later
before=$(trace_last_sequence)
tap_key F12 1
wait_for_trace_after "Home" "$before"
capture returned
"${repo_root}/rockpod/.venv/bin/python" - "${repo_root}" "${out_dir}" <<'PYTEST'
from pathlib import Path
import sys
import numpy as np
from PIL import Image

root, out = map(Path, sys.argv[1:])
connected = np.array(Image.open(out / "connected.png").convert("RGB"))
safe = np.array(Image.open(out / "safe-eject.png").convert("RGB"))
later = np.array(Image.open(out / "safe-eject-later.png").convert("RGB"))
assert np.array_equal(safe, later), "ejected screen must remain stationary"
assert not np.array_equal(connected[174:209], safe[174:209])
assert not np.any(safe[191:209].max(axis=2) > 220), "old eject warning remains"
icon = np.array(Image.open(root / "assets/ipodjs/apple/retailos-2.0.4/563-token-0dad0dba-112x112-0064.png").convert("RGBA"))
opaque = icon[:, :, 3] == 255
actual = safe[46:158, 104:216].astype(int)
assert np.max(np.abs(actual[opaque] - icon[:, :, :3][opaque])) <= 8
assert not np.array_equal(safe, np.array(Image.open(out / "returned.png").convert("RGB")))
print("PASS: original disconnect icon, cleared warning, stationary safe-eject screen, unplug exit")
PYTEST
