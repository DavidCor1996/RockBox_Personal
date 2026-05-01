#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
SIMDISK="$ROOT/build-sim-3g/simdisk/.rockbox"

mkdir -p "$SIMDISK/themes" "$SIMDISK/wps"

cp -a "$ROOT/themes/Galaxy.cfg" "$SIMDISK/themes/Galaxy.cfg"
cp -a "$ROOT/wps/Galaxy.wps" "$SIMDISK/wps/Galaxy.wps"
cp -a "$ROOT/wps/Galaxy.sbs" "$SIMDISK/wps/Galaxy.sbs"
cp -a "$ROOT/wps/Galaxy.fms" "$SIMDISK/wps/Galaxy.fms"
rm -rf "$SIMDISK/wps/Galaxy"
cp -a "$ROOT/wps/Galaxy" "$SIMDISK/wps/Galaxy"

python3 - "$SIMDISK/config.cfg" <<'PY'
import sys
from pathlib import Path

path = Path(sys.argv[1])
text = path.read_text(encoding="utf-8", errors="replace").splitlines()
updates = {
    "font": "/.rockbox/fonts/12-Adobe-Helvetica.fnt",
    "wps": "/.rockbox/wps/Galaxy.wps",
    "sbs": "/.rockbox/wps/Galaxy.sbs",
    "fms": "/.rockbox/wps/Galaxy.fms",
    "theme": "/.rockbox/themes/Galaxy.cfg",
    "iconset": "/.rockbox/icons/tango_small_mono.bmp",
    "viewers iconset": "/.rockbox/icons/tango_small_viewers_mono.bmp",
    "backdrop": "/.rockbox/wps/Galaxy/MenuBackdrop.bmp",
    "selector type": "pointer",
    "statusbar": "off",
    "show icons": "off",
    "ui viewport": "12,32,56,84",
    "background color": "0",
    "foreground color": "3",
    "line selector start color": "1",
    "line selector end color": "1",
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

echo "Prepared build-sim-3g for Galaxy preview."
echo "Launch with:"
echo "  cd \"$ROOT/build-sim-3g\" && ./rockboxui --nobackground --root simdisk"
