#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build=$(mktemp -d /tmp/desktop-phase1-tests.XXXXXX)
trap 'rm -rf "$build"' EXIT HUP INT TERM
${CC:-cc} -std=c99 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -I"$root/apps/plugins/lib" -I"$root/firmware/usbhost" \
    "$root/tools/tests/desktop_phase1.c" \
    "$root/apps/plugins/lib/desktop_model.c" \
    "$root/apps/plugins/lib/desktop_surface.c" \
    "$root/firmware/usbhost/desktop_usb.c" \
    "$root/firmware/usbhost/dl1xx.c" -o "$build/test"
"$build/test"
