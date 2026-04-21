#!/usr/bin/env bash
set -u

REPO_ROOT="/home/david/Documents/RockBox_Personal-master/rockpod"
PYTHON_BIN="$REPO_ROOT/.venv/bin/python"
MAIN_PY="$REPO_ROOT/main.py"
LOG_DIR="${HOME}/.rockpod"
LOG_FILE="$LOG_DIR/launcher.log"

LOG_WRITABLE=0
mkdir -p "$LOG_DIR" 2>/dev/null || true
cd "$REPO_ROOT"

if touch "$LOG_FILE" 2>/dev/null; then
  LOG_WRITABLE=1
  {
    echo "===== $(date '+%Y-%m-%d %H:%M:%S') ====="
    echo "Launching RockPod from desktop wrapper"
  } >>"$LOG_FILE" 2>/dev/null || true
fi

if [[ "$LOG_WRITABLE" -eq 1 ]]; then
  exec "$PYTHON_BIN" "$MAIN_PY" >>"$LOG_FILE" 2>&1
fi

exec "$PYTHON_BIN" "$MAIN_PY"
