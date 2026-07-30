#!/bin/sh
set -eu

volume_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
case "$(/usr/bin/uname -m)" in
    arm64)
        platform=macos-arm64
        ;;
    x86_64)
        platform=macos-x86_64
        ;;
    *)
        /usr/bin/osascript -e \
            'display alert "Desktop Mode" message "This Mac architecture is not included in the iPod portable runtime."'
        exit 1
        ;;
esac

bundle="${volume_root}/.rockbox/desktop-host/${platform}"
runtime="${bundle}/rockboxui"
system_root="${bundle}/system-root"
if [ ! -x "${runtime}" ] || \
   [ ! -f "${system_root}/.rockbox/rocks/apps/desktop_mode.rock" ]; then
    /usr/bin/osascript -e \
        'display alert "Desktop Mode" message "The portable macOS Desktop Mode runtime is missing or incomplete. Reinstall this Rockbox package."'
    exit 1
fi

export RBROOT="${volume_root}"
export ROCKBOX_SIM_SYSTEM_ROOT="${system_root}"
export ROCKBOX_SIM_PLUGIN="/.rockbox/rocks/apps/desktop_mode.rock"
export ROCKBOX_SIM_PLUGIN_EXIT=1
export ROCKPOD_SIM_HOST_POINTER="${volume_root}/.rockbox/host-pointer"
export DYLD_LIBRARY_PATH="${bundle}${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}"

cd "${bundle}"
exec "${runtime}" --fullscreen --nobackground --root "${volume_root}"
