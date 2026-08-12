TARGET := iphone:clang:9.3:6.0
ARCHS = armv7
PACKAGE_FORMAT = deb
INSTALL_TARGET_PROCESSES = OpenLaraTouch

include $(THEOS)/makefiles/common.mk

APPLICATION_NAME = OpenLaraTouch
OpenLaraTouch_FILES = main.m OLAppDelegate.m OLGameViewController.mm \
	EngineCommon.cpp EngineRender.cpp EngineSound.cpp OLCompat.c OLPlatform.c
OpenLaraTouch_FRAMEWORKS = UIKit Foundation QuartzCore AudioToolbox CoreGraphics
OpenLaraTouch_CFLAGS = -fobjc-arc -Wall -Wextra \
	-Wno-deprecated-declarations -fmodules-cache-path=/tmp/openlara-touch-modules
OpenLaraTouch_CCFLAGS = -std=gnu++11 -O3 -fno-exceptions -fno-rtti \
	-D__IOS__ -DNDEBUG -I../../../apps/plugins/openlara \
	-I../../../apps/plugins/openlara/fixed -Wno-unused-parameter \
	-Wno-unused-function -Wno-sign-compare \
	-Wno-error=missing-field-initializers \
	-Wno-error=dynamic-class-memaccess
OpenLaraTouch_LDFLAGS = -Wl,-flat_namespace
OpenLaraTouch_LIBRARIES = c++ c++abi
OpenLaraTouch_RESOURCE_DIRS = Resources

include $(THEOS_MAKE_PATH)/application.mk
