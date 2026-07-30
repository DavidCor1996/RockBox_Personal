#!/bin/bash
#
# Application bundle entry point. Finder runs this file when RockPod Setup is
# opened. It only resolves the bundle layout and hands off to the installer.

bundle_macos="$(cd "$(dirname "$0")" && pwd)"
resources="$(cd "${bundle_macos}/../Resources" && pwd)"

# Remove the quarantine flag from the extracted bundle so the helper scripts are
# not blocked after a plain unzip. This affects only this bundle.
/usr/bin/xattr -dr com.apple.quarantine "$(cd "${bundle_macos}/../.." && pwd)" \
    >/dev/null 2>&1 || true

exec /bin/bash "${resources}/setup/main.sh" "$@"
