#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

export THEME_CAPTURE_THEME="${IPONE_CAPTURE_THEME:-${THEME_CAPTURE_THEME:-iPone}}"
export THEME_CAPTURE_OUT_DIR="${THEME_CAPTURE_OUT_DIR:-${2:-${repo_root}/docs/ipone-regression-shots}}"
export THEME_CAPTURE_KEEP_ROOT="${IPONE_CAPTURE_KEEP_ROOT:-${THEME_CAPTURE_KEEP_ROOT:-0}}"

exec "${repo_root}/tools/theme_regression_capture.sh" "${1:-${repo_root}/build-sim-video-5g}" "${THEME_CAPTURE_OUT_DIR}"
