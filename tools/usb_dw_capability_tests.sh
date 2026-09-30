#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
test_dir="$(mktemp -d /tmp/rockbox-capability-tests.XXXXXX)"
trap 'rm -rf "${test_dir}"' EXIT
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -I "${repo_root}/firmware/export" \
    "${repo_root}/tools/tests/usb_dw_capabilities.c" \
    "${repo_root}/firmware/usbhost/capabilities.c" \
    -o "${test_dir}/capabilities"
# No allocation is performed; LeakSanitizer cannot run under the sandbox tracer.
ASAN_OPTIONS=detect_leaks=0 "${test_dir}/capabilities"
