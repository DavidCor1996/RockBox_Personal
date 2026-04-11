#!/bin/sh
cd "$(dirname "$0")"
TARGET=${1:-ipod6g}
if [ "$TARGET" = "5g" ] || [ "$TARGET" = "ipodvideo" ]; then
  TARGET_IPOD="ipodvideo"
else
  TARGET_IPOD="ipod6g"
fi

if [ ! -d build-sim ]; then
    mkdir build-sim
    cd build-sim
  ../tools/configure --target="$TARGET_IPOD" --type=s
else
    cd build-sim
fi

make -j$(sysctl -n hw.ncpu) && make install

# Install theme/font files from ~/Temp
# for z in ~/Temp/rockbox-fonts-*.zip ~/Temp/SNARTY.zip ~/Temp/adwaitapod_dark_simplified.zip ~/Temp/themify.zip; do
#     [ -f "$z" ] && unzip -o "$z" -d simdisk/
# done
