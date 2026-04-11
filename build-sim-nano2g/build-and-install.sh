#!/bin/bash
# Rockbox Sim Build & Install Wrapper (Nano 2G)
# Handles make install + symlink restoration + iPod asset sync

set -e

SCRIPT_DIR="/home/david/Documents/RockBox_Personal-master/build-sim-nano2g"
BUILD_DIR="$SCRIPT_DIR"
IPOD_SOURCE="/Volumes/IPOD 1"

echo "=== Rockbox Nano2G Sim Build & Install ==="

# Run make install
echo "Running make install..."
cd "$BUILD_DIR"
make install

# Re-create symlinks for live plugin development
echo "Setting up symlinks..."
SIMDISK="$BUILD_DIR/simdisk/.rockbox"

# Ensure dirs exist
mkdir -p "$SIMDISK/rocks"

# games/ -> build plugins dir (live testing)
if [ -L "$SIMDISK/rocks/games" ] || [ -e "$SIMDISK/rocks/games" ]; then
    rm -rf "$SIMDISK/rocks/games"
fi

ln -s "$BUILD_DIR/apps/plugins/games" "$SIMDISK/rocks/games"
echo "  symlinked rocks/games"

# Sync iPod assets (WPS, fonts, langs, config)
echo "Syncing iPod assets..."
if [ -d "$IPOD_SOURCE/.rockbox" ]; then
    rsync -av \
        --exclude='doom' \
        --exclude='duke3d' \
        --exclude='quake' \
        --exclude='wolf3d' \
        --exclude='timidity' \
        --exclude='patchset' \
        --exclude='xworld' \
        --exclude='rockbox.ipod-*' \
        "$IPOD_SOURCE/.rockbox/" "$SIMDISK/"

    if [ -f "$IPOD_SOURCE/.rockbox/rockbox.ipod" ]; then
        cp "$IPOD_SOURCE/.rockbox/rockbox.ipod" "$SIMDISK/"
    fi

    echo "  synced from $IPOD_SOURCE"
else
    echo "  WARNING: $IPOD_SOURCE not found, skipping asset sync"
fi

# Rebuild minishcap (needs special handling)
echo "Building minishcap..."
if grep -q "minishcap.c" "$SCRIPT_DIR/apps/plugins/SOURCES" 2>/dev/null; then
    touch "$SCRIPT_DIR/apps/plugins/minishcap.c"
    make rocks 2>&1 | grep -E "minishcap|Error" || true
    echo "  done"
fi

echo "=== Done ==="

# Launch sim (Linux version)
if [ -f "$BUILD_DIR/rockboxui" ]; then
    echo "Launching simulator..."
    "$BUILD_DIR/rockboxui" &
else
    echo "Run manually: $BUILD_DIR/rockboxui"
fi
