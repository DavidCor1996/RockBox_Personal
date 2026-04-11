#!/bin/sh
cd "$(dirname "$0")/build-sim-video-5g" || exit 1
export SDL_VIDEODRIVER=x11
export SDL_RENDER_DRIVER=software
exec ./rockboxui --zoom 2
