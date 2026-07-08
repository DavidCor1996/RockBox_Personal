#!/bin/sh
build_root="$(CDPATH= cd -- "$(dirname "$0")/build-sim-ipod6g" && pwd -P)"
cd "$build_root" || exit 1
export SDL_VIDEODRIVER=x11
export SDL_RENDER_DRIVER=software
exec ./rockboxui --zoom 2 --root "$build_root/simdisk"
