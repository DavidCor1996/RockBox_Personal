#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
rockpod_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
runtime="$rockpod_dir/.ofscraper-venv"
requirements="$rockpod_dir/requirements-ofscraper.txt"

if command -v uv >/dev/null 2>&1; then
    uv_bin=$(command -v uv)
else
    bootstrap="$rockpod_dir/.ofscraper-bootstrap"
    if [ ! -x "$bootstrap/bin/uv" ]; then
        python3 -m venv "$bootstrap"
        "$bootstrap/bin/pip" install "uv==0.12.5"
    fi
    uv_bin="$bootstrap/bin/uv"
fi

"$uv_bin" venv --clear --python 3.12 "$runtime"
"$uv_bin" pip install --python "$runtime/bin/python" -r "$requirements"
"$runtime/bin/ofscraper" --version
