#!/usr/bin/env bash
# Check USB source frames, re-entry and storage-independent paint paths.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/ipodjs_navigation_sim_regression.sh"
mkdir -p "${out_dir}"
trap cleanup EXIT INT TERM HUP
export IPODJS_NAVIGATION_CAPTURE_DELAY=0.03
for mode in off on; do
    IPODJS_NAVIGATION_DARK_MODE="${mode}" prepare_root
    launch_sim
    wait_for_home
    for connection in 1 2; do
        before=$(trace_last_sequence)
        tap_key F11 1.5
        wait_for_trace_after "USB Connected" "$before"
        for frame in $(seq 0 23); do
            capture "${mode}-${connection}-${frame}"
        done
        sleep 2
        tap_key F12 1
        capture "${mode}-${connection}-returned"
    done
    cp "${runtime_root}/.rockbox/ipodjs-trace.tsv" \
       "${out_dir}/${mode}-trace.tsv"
    cleanup
    runtime_root=""
done
"${repo_root}/rockpod/.venv/bin/python" - "${out_dir}" <<'PY'
import csv
from pathlib import Path
import sys
import numpy as np
from PIL import Image

out = Path(sys.argv[1])
for mode in ("off", "on"):
    rows = list(csv.DictReader((out / f"{mode}-trace.tsv").open(), delimiter="\t"))
    frames = [r for r in rows if r["name"] == "USB Connected"]
    assert {int(r["selected"]) for r in frames} == set(range(18))
    assert all(int(r["items"]) == 18 for r in frames)
    for connection in (1, 2):
        images = [np.array(Image.open(out / f"{mode}-{connection}-{i}.png").convert("RGB"))
                  for i in range(24)]
        baseline = images[0]
        assert baseline[55, 160, 0] > baseline[55, 160, 2]  # original gold badge
        assert len({im[62:138, 122:198].tobytes() for im in images}) > 6
        for im in images[1:]:
            difference = np.any(im != baseline, axis=2)
            difference[62:138, 122:198] = False
            assert not difference.any(), "animation damaged the static screen"
print("PASS: all 18 USB source frames, fixed damage rectangle, two connections in both modes")
PY
