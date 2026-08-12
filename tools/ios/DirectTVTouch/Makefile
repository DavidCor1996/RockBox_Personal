TARGET := iphone:clang:9.3:6.0
ARCHS = armv7
PACKAGE_FORMAT = deb
INSTALL_TARGET_PROCESSES = DirectTV

include $(THEOS)/makefiles/common.mk

APPLICATION_NAME = DirectTV
DirectTV_FILES = main.m DTVAppDelegate.m DTVClient.m DTVGuideController.m RPCompat.c
DirectTV_FRAMEWORKS = UIKit Foundation Security QuartzCore AVFoundation
DirectTV_CFLAGS = -fobjc-arc -Wall -Wextra -Wno-deprecated-declarations \
    -fno-builtin-memset \
    -fmodules-cache-path=/tmp/rockpod-directtv-module-cache
DirectTV_LDFLAGS = -Wl,-flat_namespace
DirectTV_RESOURCE_DIRS = Resources

include $(THEOS_MAKE_PATH)/application.mk
