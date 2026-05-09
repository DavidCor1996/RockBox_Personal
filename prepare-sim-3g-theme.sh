#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
SIMDISK="$ROOT/build-sim-3g/simdisk/.rockbox"

mkdir -p "$SIMDISK/themes" "$SIMDISK/wps"

cp -a "$ROOT/themes/CoverPod_3g.cfg" "$SIMDISK/themes/CoverPod_3g.cfg"
cp -a "$ROOT/wps/CoverPod_3g.wps" "$SIMDISK/wps/CoverPod_3g.wps"
cp -a "$ROOT/wps/CoverPod_3g.sbs" "$SIMDISK/wps/CoverPod_3g.sbs"
cp -a "$ROOT/wps/CoverPod_3g.fms" "$SIMDISK/wps/CoverPod_3g.fms"
rm -rf "$SIMDISK/wps/CoverPod_3g"
cp -a "$ROOT/wps/CoverPod_3g" "$SIMDISK/wps/CoverPod_3g"

python3 - "$SIMDISK/config.cfg" <<'PY'
import sys
from pathlib import Path

path = Path(sys.argv[1])
text = path.read_text(encoding="utf-8", errors="replace").splitlines()
updates = {
    "font": "/.rockbox/fonts/12-Adobe-Helvetica.fnt",
    "wps": "/.rockbox/wps/CoverPod_3g.wps",
    "sbs": "/.rockbox/wps/CoverPod_3g.sbs",
    "fms": "/.rockbox/wps/CoverPod_3g.fms",
    "theme": "/.rockbox/themes/CoverPod_3g.cfg",
    "iconset": "/.rockbox/icons/tango_small_mono.bmp",
    "viewers iconset": "/.rockbox/icons/tango_small_viewers_mono.bmp",
    "backdrop": "-",
    "selector type": "bar (inverse)",
    "statusbar": "off",
    "show icons": "off",
    "ui viewport": "8,30,144,88",
    "background color": "3",
    "foreground color": "0",
    "line selector start color": "0",
    "line selector end color": "0",
    "line selector text color": "3",
    "list separator color": "2",
    "backlight on button hold": "on",
    "start in screen": "root",
}

seen = set()
output = []
for raw in text:
    if ":" not in raw:
        output.append(raw)
        continue
    key, _value = raw.split(":", 1)
    key = key.strip()
    if key in updates:
        output.append(f"{key}: {updates[key]}")
        seen.add(key)
    else:
        output.append(raw)

for key, value in updates.items():
    if key not in seen:
        output.append(f"{key}: {value}")

path.write_text("\n".join(output) + "\n", encoding="utf-8")
PY

echo "Prepared build-sim-3g for CoverPod 3G preview."
echo "Launch with:"
echo "  cd \"$ROOT/build-sim-3g\" && ./rockboxui --nobackground --root simdisk"
