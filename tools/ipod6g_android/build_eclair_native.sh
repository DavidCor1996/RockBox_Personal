#!/usr/bin/env bash
# Build the storage-independent Android 2.0 native bring-up subset.

set -euo pipefail

if [[ $# -ne 3 ]]; then
    echo "usage: $0 PREPARED_ECLAIR_SOURCE PINNED_JDK6 PINNED_JDK6_ARCHIVE" >&2
    exit 2
fi

script_dir=$(cd -- "$(dirname -- "$0")" && pwd)
source_dir=$(realpath -- "$1")
jdk_dir=$(realpath -- "$2")
jdk_archive=$(realpath -- "$3")
marker="$source_dir/.ipod6g-eclair-source.json"
compiler="$source_dir/prebuilt/linux-x86/toolchain/arm-eabi-4.4.0/bin/arm-eabi-gcc"
jobs=${JOBS:-4}

if [[ ! "$jobs" =~ ^[1-9][0-9]*$ ]]; then
    echo "JOBS must be a positive integer" >&2
    exit 2
fi
if [[ ! -f "$marker" || -L "$marker" ]]; then
    echo "refusing unverified source tree: run prepare_eclair_native_source.py" >&2
    exit 1
fi
if [[ ! -x "$compiler" || -L "$compiler" ]]; then
    echo "missing pinned ARM EABI 4.4.0 compiler" >&2
    exit 1
fi
if ! grep -q '"aosp_tag": "android-2.0_r1"' "$marker"; then
    echo "source marker does not identify android-2.0_r1" >&2
    exit 1
fi

common=(
    -C "$source_dir"
    -j"$jobs"
    ECLAIR_NATIVE_ONLY=true
    TARGET_PRODUCT=generic
    TARGET_BUILD_VARIANT=eng
    TARGET_PRELINK_MODULE=false
    TARGET_STRIP_MODULE=false
    BUILD_ID=ESD20
    BUILD_NUMBER=eng.ipod6g.20000101.000000
    USER=android
)

python3 "$script_dir/verify_eclair_host_jdk.py" \
    "$jdk_archive" "$jdk_dir" \
    --output "$source_dir/out/target/product/generic/ipod6g-eclair-host-toolchain.json"

make "${common[@]}" \
    'ONE_SHOT_MAKEFILE=build/libs/host/Android.mk build/tools/acp/Android.mk' \
    acp

make "${common[@]}" ONE_SHOT_MAKEFILE=system/core/liblog/Android.mk \
    out/target/product/generic/obj/STATIC_LIBRARIES/liblog_intermediates/liblog.a

make "${common[@]}" ONE_SHOT_MAKEFILE=system/core/libcutils/Android.mk \
    out/target/product/generic/obj/STATIC_LIBRARIES/libcutils_intermediates/libcutils.a

make "${common[@]}" ONE_SHOT_MAKEFILE=bionic/Android.mk \
    out/target/product/generic/obj/lib/crtbegin_dynamic.o \
    linker libc libstdc++ libm

make "${common[@]}" ONE_SHOT_MAKEFILE=system/core/liblog/Android.mk \
    out/target/product/generic/system/lib/liblog.so

make "${common[@]}" ONE_SHOT_MAKEFILE=system/core/libcutils/Android.mk \
    out/target/product/generic/system/lib/libcutils.so

make "${common[@]}" ONE_SHOT_MAKEFILE=system/core/init/Android.mk init

make "${common[@]}" \
    ONE_SHOT_MAKEFILE=frameworks/base/cmds/servicemanager/Android.mk \
    servicemanager

make "${common[@]}" \
    ONE_SHOT_MAKEFILE=frameworks/base/cmds/installd/Android.mk \
    installd

make "${common[@]}" ONE_SHOT_MAKEFILE=ipod6g_probe/Android.mk \
    ipod6g_eclair_probe_dynamic ipod6g_eclair_probe_static \
    ipod6g_eclair_zygote_gate

make "${common[@]}" ONE_SHOT_MAKEFILE=external/zlib/Android.mk \
    out/target/product/generic/system/lib/libz.so

make "${common[@]}" \
    'ONE_SHOT_MAKEFILE=dalvik/libdex/Android.mk dalvik/dexdump/Android.mk' \
    out/target/product/generic/system/xbin/dexdump

make "${common[@]}" \
    'HOST_GLOBAL_CPPFLAGS=-std=gnu++98 -fpermissive -Wno-error' \
    'ONE_SHOT_MAKEFILE=build/libs/host/Android.mk build/tools/acp/Android.mk system/core/liblog/Android.mk system/core/libcutils/Android.mk frameworks/base/libs/utils/Android.mk external/expat/Android.mk external/libpng/Android.mk external/zlib/Android.mk frameworks/base/tools/aapt/Android.mk' \
    aapt

JAVA_HOME="$jdk_dir" PATH="$jdk_dir/bin:$PATH" \
make "${common[@]}" CUSTOM_JAVA_COMPILER=eclipse \
    ONE_SHOT_MAKEFILE=dalvik/dx/Android.mk dx

JAVA_HOME="$jdk_dir" PATH="$jdk_dir/bin:$PATH" \
make "${common[@]}" CUSTOM_JAVA_COMPILER=eclipse \
    ONE_SHOT_MAKEFILE=dalvik/libcore/Android.mk core

make "${common[@]}" \
    'ONE_SHOT_MAKEFILE=frameworks/base/libs/utils/Android.mk build/tools/bin2asm/Android.mk external/zlib/Android.mk external/expat/Android.mk external/fdlibm/Android.mk external/icu4c/Android.mk external/openssl/Android.mk external/sqlite/android/Android.mk external/sqlite/dist/Android.mk dalvik/Android.mk' \
    dalvikvm dexopt

make "${common[@]}" \
    'HOST_GLOBAL_CPPFLAGS=-std=gnu++98 -fpermissive -Wno-error' \
    ONE_SHOT_MAKEFILE=frameworks/base/tools/aidl/Android.mk aidl

JAVA_HOME="$jdk_dir" PATH="$jdk_dir/bin:$PATH" \
make "${common[@]}" CUSTOM_JAVA_COMPILER=eclipse \
    ONE_SHOT_MAKEFILE=build/tools/signapk/Android.mk signapk

make "${common[@]}" ONE_SHOT_MAKEFILE=build/tools/zipalign/Android.mk zipalign

JAVA_HOME="$jdk_dir" PATH="$jdk_dir/bin:$PATH" \
make "${common[@]}" CUSTOM_JAVA_COMPILER=eclipse \
    'ONE_SHOT_MAKEFILE=external/googleclient/Android.mk frameworks/base/core/res/Android.mk frameworks/base/Android.mk' \
    framework-res ext framework

make "${common[@]}" \
    ONE_SHOT_MAKEFILE=frameworks/base/data/fonts/Android.mk \
    out/target/product/generic/system/fonts/DroidSans.ttf \
    out/target/product/generic/system/fonts/DroidSans-Bold.ttf \
    out/target/product/generic/system/fonts/DroidSerif-Regular.ttf \
    out/target/product/generic/system/fonts/DroidSerif-Bold.ttf \
    out/target/product/generic/system/fonts/DroidSerif-Italic.ttf \
    out/target/product/generic/system/fonts/DroidSerif-BoldItalic.ttf \
    out/target/product/generic/system/fonts/DroidSansMono.ttf \
    out/target/product/generic/system/fonts/DroidSansFallback.ttf

JAVA_HOME="$jdk_dir" PATH="$jdk_dir/bin:$PATH" \
make "${common[@]}" CUSTOM_JAVA_COMPILER=eclipse \
    ONE_SHOT_MAKEFILE=frameworks/policies/base/phone/Android.mk \
    android.policy_phone

JAVA_HOME="$jdk_dir" PATH="$jdk_dir/bin:$PATH" \
make "${common[@]}" CUSTOM_JAVA_COMPILER=eclipse \
    ONE_SHOT_MAKEFILE=frameworks/base/services/java/Android.mk \
    services

JAVA_HOME="$jdk_dir" PATH="$jdk_dir/bin:$PATH" \
make "${common[@]}" CUSTOM_JAVA_COMPILER=eclipse \
    ONE_SHOT_MAKEFILE=frameworks/base/packages/SettingsProvider/Android.mk \
    SettingsProvider

JAVA_HOME="$jdk_dir" PATH="$jdk_dir/bin:$PATH" \
make "${common[@]}" CUSTOM_JAVA_COMPILER=eclipse \
    ONE_SHOT_MAKEFILE=ipod6g_launcher/Android.mk \
    RockpodLauncher

# Build the official native framework entry point and its complete Eclair
# dependency closure.  Keep these as ordered one-shot builds: the Eclair build
# system does not discover rules outside ONE_SHOT_MAKEFILE, even when an
# already-declared module links against them.
make "${common[@]}" \
    'ONE_SHOT_MAKEFILE=frameworks/base/libs/utils/Android.mk frameworks/base/libs/binder/Android.mk external/wpa_supplicant/Android.mk system/core/libnetutils/Android.mk hardware/libhardware/Android.mk hardware/libhardware_legacy/Android.mk' \
    libbinder libwpa_client libnetutils libhardware libhardware_legacy

make "${common[@]}" \
    'ONE_SHOT_MAKEFILE=external/sonivox/arm-wt-22k/Android.mk external/speex/Android.mk' \
    libsonivox libspeex

make "${common[@]}" \
    'ONE_SHOT_MAKEFILE=external/freetype/Android.mk external/jpeg/Android.mk external/giflib/Android.mk frameworks/opt/emoji/Android.mk external/libpng/Android.mk external/zlib/Android.mk external/skia/Android.mk' \
    libft2 libjpeg libgif libemoji libskia

make "${common[@]}" \
    'ONE_SHOT_MAKEFILE=frameworks/base/opengl/libs/Android.mk system/core/libpixelflinger/Android.mk' \
    libEGL libGLESv1_CM libpixelflinger

make "${common[@]}" \
    ONE_SHOT_MAKEFILE=frameworks/base/opengl/libagl/Android.mk \
    libGLES_android

make "${common[@]}" ONE_SHOT_MAKEFILE=frameworks/base/libs/ui/Android.mk \
    libui

make "${common[@]}" \
    'ONE_SHOT_MAKEFILE=external/speex/Android.mk frameworks/base/media/libmedia/Android.mk' \
    libmedia

make "${common[@]}" ONE_SHOT_MAKEFILE=external/skia/Android.mk libskiagl

make "${common[@]}" \
    'ONE_SHOT_MAKEFILE=frameworks/base/core/jni/Android.mk frameworks/base/cmds/app_process/Android.mk' \
    libandroid_runtime app_process

make "${common[@]}" \
    ONE_SHOT_MAKEFILE=frameworks/base/libs/surfaceflinger/Android.mk \
    libsurfaceflinger

make "${common[@]}" \
    ONE_SHOT_MAKEFILE=hardware/libhardware/modules/gralloc/Android.mk \
    gralloc.default

make "${common[@]}" \
    'ONE_SHOT_MAKEFILE=frameworks/base/libs/surfaceflinger/Android.mk frameworks/base/cmds/system_server/library/Android.mk' \
    libsystem_server

make "${common[@]}" \
    ONE_SHOT_MAKEFILE=frameworks/base/services/jni/Android.mk \
    libandroid_servers

fixture_root="$source_dir/out/ipod6g-framework-fixture"
fixture_jar="$source_dir/out/target/product/generic/system/framework/ipod6g-framework-fixture.jar"
rm -rf -- "$fixture_root"
mkdir -p -- "$fixture_root/classes" "$fixture_root/stubs"
"$jdk_dir/bin/javac" -source 1.5 -target 1.5 \
    -bootclasspath \
    "$source_dir/out/target/common/obj/JAVA_LIBRARIES/core_intermediates/classes.jar" \
    -d "$fixture_root/stubs" \
    "$script_dir/eclair/fixture/stubs/dalvikExecTest/HelloWorld.java"
"$jdk_dir/bin/javac" -source 1.5 -target 1.5 \
    -bootclasspath \
    "$source_dir/out/target/common/obj/JAVA_LIBRARIES/core_intermediates/classes.jar:$source_dir/out/target/common/obj/JAVA_LIBRARIES/framework_intermediates/classes.jar" \
    -classpath "$fixture_root/stubs" \
    -d "$fixture_root/classes" \
    "$script_dir/eclair/fixture/FrameworkHello.java"
JAVA_HOME="$jdk_dir" PATH="$jdk_dir/bin:$PATH" \
    "$source_dir/out/host/linux-x86/bin/dx" --dex \
    --output="$fixture_jar" "$fixture_root/classes"

python3 "$script_dir/qualify_eclair_native.py" "$source_dir"
