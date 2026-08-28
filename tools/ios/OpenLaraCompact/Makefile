TARGET := iphone:clang:9.3:6.0
ARCHS = armv7
PACKAGE_FORMAT = deb
INSTALL_TARGET_PROCESSES = OpenLaraCompact

include $(THEOS)/makefiles/common.mk

APPLICATION_NAME = OpenLaraCompact
OpenLaraCompact_FILES = ../OpenLaraTouch/main.m \
	../OpenLaraTouch/OLAppDelegate.m \
	../OpenLaraTouch/OLGameViewController.mm \
	../OpenLaraTouch/OLCompat.c ../OpenLaraTouch/OLPlatform.c \
	../../../apps/plugins/openlara/fixed/common.cpp \
	../../../apps/plugins/openlara/render.cpp \
	../../../apps/plugins/openlara/sound.cpp
OpenLaraCompact_FRAMEWORKS = UIKit Foundation QuartzCore AudioToolbox \
	CoreGraphics GLKit
OpenLaraCompact_CFLAGS = -fobjc-arc -Wall -Wextra \
	-Wno-deprecated-declarations -fmodules-cache-path=/tmp/openlara-compact-modules
OpenLaraCompact_CCFLAGS = -std=gnu++11 -O3 -fno-exceptions -fno-rtti \
	-DNDEBUG -D__IOS__ -I../../../apps/plugins/openlara/fixed \
	-Wno-error=dynamic-class-memaccess \
	-Wno-error=missing-field-initializers -Wno-unused-parameter \
	-Wno-unused-function -Wno-unused-variable
OpenLaraCompact_LDFLAGS = -Wl,-flat_namespace
OpenLaraCompact_LIBRARIES = c++ c++abi
OpenLaraCompact_RESOURCE_DIRS = Resources

include $(THEOS_MAKE_PATH)/application.mk
