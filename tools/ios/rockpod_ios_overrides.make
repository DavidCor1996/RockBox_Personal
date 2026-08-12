# Overrides injected into an ordinary iPod 6G SDL simulator Makefile by
# build_rockpod_link.sh. Keep generated build directories out of version
# control; this file is the reproducible source of the iOS cross-build.
ROCKPOD_THEOS ?= /tmp/rockpod-theos
ROCKPOD_SDL_SOURCE ?= /tmp/SDL2-2.32.10
ROCKPOD_SDL_BUILD ?= /tmp/rockpod-sdl-ios
ROCKPOD_IOS_SDK ?= $(ROCKPOD_THEOS)/sdks/iPhoneOS16.5.sdk
ROCKPOD_IOS_TOOLCHAIN := $(ROCKPOD_THEOS)/toolchain/linux/iphone/bin

export ROCKPOD_IOS_EMBED := yes
export EXTRA_DEFINES := -DSIMULATOR -DHAVE_TEST_PLUGINS -DROCKPOD_IOS_EMBED
export ENABLEDPLUGINS := no
export CC := $(ROCKPOD_IOS_TOOLCHAIN)/clang
export CPP := $(CC) -E
export LD := $(CC)
export AR := $(ROCKPOD_IOS_TOOLCHAIN)/llvm-ar
export AS := $(CC)
export OC := $(ROCKPOD_IOS_TOOLCHAIN)/llvm-objcopy
export RANLIB := $(ROCKPOD_IOS_TOOLCHAIN)/llvm-ranlib
export IOS_LIBTOOL := $(ROCKPOD_IOS_TOOLCHAIN)/libtool
export UNAME := Darwin
export GCCOPTS := -W -Wall -Wextra -Wundef -Os -Wstrict-prototypes -pipe \
 -std=gnu99 -fno-delete-null-pointer-checks -fno-strict-overflow \
 -fno-common -fno-builtin -g -Wno-unused-result \
 -I$(ROCKPOD_SDL_SOURCE)/include -D_REENTRANT -I$(SIMDIR) \
 -Wno-pointer-sign -Wno-override-init -Wno-unknown-warning-option \
 -target arm64-apple-ios12.0 -isysroot $(ROCKPOD_IOS_SDK) -fPIC
export SHARED_LDFLAGS := -bundle -Wl,-undefined,dynamic_lookup
export SHARED_CFLAGS := -fPIC -fvisibility=hidden
export LDOPTS := $(ROCKPOD_SDL_BUILD)/libSDL2.a -framework UIKit \
 -framework Foundation -framework CoreGraphics -framework CoreAudio \
 -framework AudioToolbox -framework AVFoundation -framework GameController \
 -framework OpenGLES -framework QuartzCore -framework CoreMotion \
 -framework CoreHaptics -framework CoreBluetooth -framework Metal \
 -framework IOKit -liconv -lm -lpthread
export GLOBAL_LDOPTS :=
