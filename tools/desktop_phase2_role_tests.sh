#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build=$(mktemp -d /tmp/desktop-role-tests.XXXXXX)
trap 'rm -rf "$build"' EXIT HUP INT TERM
${CC:-cc} -std=c99 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -I"$root/firmware/usbhost" "$root/tools/tests/desktop_phase2_role.c" \
    "$root/firmware/usbhost/role_probe.c" -o "$build/test"
"$build/test"
