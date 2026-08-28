#!/usr/bin/env bash
# Install streamrip into the RockPod venv.
#
# The music store shells out to streamrip for search, album detail, and import,
# so the store is dead without it. Rebuilding the venv from requirements.txt
# alone leaves streamrip out, which is how the store breaks.
#
# streamrip has to come from git. The PyPI release (2.1.0) carries Tidal client
# credentials Tidal has revoked, so token refresh returns HTTP 403 and every
# store call fails. --no-deps keeps streamrip's Pillow<11 and tomlkit<0.8 pins
# from downgrading the copies the rest of the app uses; its runtime
# dependencies are installed from requirements-streamrip.txt instead.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON_BIN="$REPO_ROOT/.venv/bin/python"

if [[ ! -x "$PYTHON_BIN" ]]; then
  echo "RockPod venv not found at $PYTHON_BIN" >&2
  exit 1
fi

"$PYTHON_BIN" -m pip install --no-deps --upgrade --force-reinstall \
  "git+https://github.com/nathom/streamrip.git@main"
"$PYTHON_BIN" -m pip install -r "$REPO_ROOT/requirements-streamrip.txt"

"$PYTHON_BIN" "$REPO_ROOT/scripts/patch_streamrip_tidal_lyrics.py"

echo -n "streamrip installed: "
"$PYTHON_BIN" -c "import streamrip; print(streamrip.__version__)"
