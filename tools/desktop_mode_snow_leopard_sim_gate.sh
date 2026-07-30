#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec "${repo_root}/rockpod/.venv/bin/python" \
    "${repo_root}/tools/desktop_mode_snow_leopard_gate.py" \
    --require-builds "$@"

