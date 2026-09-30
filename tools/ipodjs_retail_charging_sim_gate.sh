#!/usr/bin/env bash
# Reuse the navigation fixture/launcher; exercise actual source assets.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/ipodjs_navigation_sim_regression.sh"

mkdir -p "${out_dir}"
trap cleanup EXIT INT TERM HUP
for mode in off on; do
    IPODJS_NAVIGATION_DARK_MODE="${mode}" prepare_root
    # Notifications are independent UI owners and can obscure this fixture's
    # status bar immediately after dismissing the charging surface.
    awk '!/^notifications:/ { print } END { print "notifications: off" }' \
        "${runtime_root}/.rockbox/config.cfg" > "${runtime_root}/config.new"
    mv "${runtime_root}/config.new" "${runtime_root}/.rockbox/config.cfg"
    launch_sim
    wait_for_home
    # Discharge enough to capture both charging and charged presentations.
    sleep 12
    capture "${mode}-home"
    tap_key F6 1.2
    capture "${mode}-charging"
    sleep 0.5
    capture "${mode}-charging-next"
    tap_key KP_5 0.5
    capture "${mode}-header-charging"
    sleep 12
    capture "${mode}-header-charged"
    tap_key F6 1
    tap_key F6 5
    capture "${mode}-charged"
    tap_key KP_5 0.5
    tap_key F6 1
    capture "${mode}-unplugged"
    for pulse in 1 2 3 4; do
        tap_key KP_2 0.3
    done
    sleep 1
    capture "${mode}-extras-clock"
    cleanup
    runtime_root=""
done

"${repo_root}/rockpod/.venv/bin/python" - "${out_dir}" <<'PY'
from pathlib import Path
import sys
from PIL import Image

directory = Path(sys.argv[1])
for mode in ("off", "on"):
    for state in ("header-charging", "header-charged"):
        im = Image.open(directory / f"{mode}-{state}.png").convert("RGB")
        # The terminal lies outside both source overlays. A bolt alone has
        # the plain header background here instead of the battery terminal.
        assert im.getpixel((153, 9)) != im.getpixel((157, 9)), (mode, state)
    charging = Image.open(directory / f"{mode}-charging.png").convert("RGB")
    charged = Image.open(directory / f"{mode}-charged.png").convert("RGB")
    # Native masks centered within the reference's 154x75 casing. Source
    # masks are translucent; compare with the body rather than solid black.
    for im, x, y, name in ((charging, 161, 112, "bolt"),
                           (charged, 158, 122, "plug")):
        assert sum(im.getpixel((x, y))) < sum(im.getpixel((x+35, y))) * .75, (mode, name)
print("PASS: source battery casing and centered charging masks in both modes")
PY
