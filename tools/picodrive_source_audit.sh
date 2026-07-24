#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
port="$repo_root/apps/plugins/picodrive"
sources="$port/SOURCES"
makefile="$port/picodrive.make"
buildzip="$repo_root/tools/buildzip.pl"
categories="$repo_root/apps/plugins/CATEGORIES"

test -f "$port/upstream/COPYING"
test -f "$port/upstream/PROVENANCE.md"
test -f "$port/upstream/cpu/cyclone/License-GPLv2.txt"
grep -Fq 'PICODRIVE_NONCOMMERCIAL' "$makefile"
grep -Fq -- '-DNO_32X' "$makefile"
grep -Fq -- '-DNO_SMS' "$makefile"
grep -Fq 'glob_unlink("$temp_dir/rocks/picodrive.rock")' "$buildzip"
if grep -Eq '^picodrive,' "$categories"; then
    echo "PicoDrive audit: personal-use plugin entered distribution categories" >&2
    exit 1
fi

if grep -Eiq '(^|/)(cd|32x|pico/pico|libretro|sdl|zlib|unzip|drc|sh2)(/|$)|media\.c$|mode4\.c$' "$sources"; then
    echo "PicoDrive audit: excluded subsystem entered SOURCES" >&2
    exit 1
fi

if grep -Eq 'mix_arm\.S' "$sources"; then
    echo "PicoDrive audit: duplicate ARM/C mixer selection" >&2
    exit 1
fi

expected='5a22d084e8f51e084fb888835fd39b5c16ac02cdc6317c4165ef4d17730261fc'
actual="$(sha256sum "$port/upstream/cpu/cyclone/Cyclone.s" | cut -d' ' -f1)"
test "$actual" = "$expected"

echo "PicoDrive source audit passed"
