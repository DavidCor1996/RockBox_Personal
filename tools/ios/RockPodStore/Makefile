TARGET := iphone:clang:9.3:6.0
ARCHS = armv7
PACKAGE_FORMAT = deb
INSTALL_TARGET_PROCESSES = RockPodStore

include $(THEOS)/makefiles/common.mk

APPLICATION_NAME = RockPodStore
RockPodStore_FILES = main.m RPStoreAppDelegate.m RPStoreClient.m RPStoreViewController.m RPDeviceMusic.m
RockPodStore_FRAMEWORKS = UIKit Foundation Security QuartzCore MediaPlayer
RockPodStore_CFLAGS = -fobjc-arc -Wall -Wextra -Wno-deprecated-declarations
RockPodStore_LDFLAGS = -Wl,-flat_namespace
RockPodStore_RESOURCE_DIRS = Resources

include $(THEOS_MAKE_PATH)/application.mk
