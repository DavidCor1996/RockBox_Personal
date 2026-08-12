TARGET := iphone:clang:16.5:12.0
ARCHS = arm64
PACKAGE_FORMAT = ipa
INSTALL_TARGET_PROCESSES = RockPodLink

include $(THEOS)/makefiles/common.mk

APPLICATION_NAME = RockPodLink
ROCKBOX_ROOT := $(abspath ../../..)
ROCKPOD_SDL_SOURCE ?= /tmp/rockpod-sdl-ios
ROCKPOD_SDL_LIBRARY ?= /tmp/rockpod-sdl-ios/libSDL2.a
RockPodLink_FILES = main.m RPAppDelegate.m RPRootViewController.m RPRelayService.m RPIPodAudioStreamer.m RPVideoConverter.m RPMPEGEncoder.m RPVideoMetadataService.m RPLibraryBrowserView.m RPAudioTranscoder.m RPGameStoreService.m RPGameArchive.m RPGameBrowserViewController.m RPMusicService.m RPMusicDiscoveryService.m RPMediaServerService.m RPSimulatorHost.m
RockPodLink_FRAMEWORKS = UIKit CoreGraphics WebKit Photos MediaPlayer AVFoundation CoreMedia CoreVideo ImageIO CoreAudio AudioToolbox GameController OpenGLES QuartzCore CoreMotion CoreLocation CoreHaptics CoreBluetooth Metal IOKit Security
RockPodLink_WEAK_FRAMEWORKS = PhotosUI
RockPodLink_CFLAGS = -fobjc-arc -Wall -Wextra -Wno-unguarded-availability-new -fmodules-cache-path=/tmp/rockpod-clang-modules -I$(ROCKPOD_SDL_SOURCE)/include -I$(ROCKPOD_SDL_SOURCE)/include/SDL2 -I$(ROCKPOD_SDL_SOURCE)/include-config-release/SDL2 -IVendor/FFmpeg/include
RockPodLink_LDFLAGS = -Wl,-force_load,$(ROCKBOX_ROOT)/build-ios-ipod6g/librockbox-ios.a -Wl,-force_load,$(ROCKPOD_SDL_LIBRARY) -Wl,-force_load,$(THEOS_PROJECT_DIR)/Vendor/FFmpeg/lib/libavformat.a -Wl,-force_load,$(THEOS_PROJECT_DIR)/Vendor/FFmpeg/lib/libavcodec.a -Wl,-force_load,$(THEOS_PROJECT_DIR)/Vendor/FFmpeg/lib/libavutil.a -liconv
RockPodLink_RESOURCE_DIRS = Resources

include $(THEOS_MAKE_PATH)/application.mk
