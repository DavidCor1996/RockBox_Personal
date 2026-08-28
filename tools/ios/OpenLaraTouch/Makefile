TARGET := iphone:clang:9.3:6.0
ARCHS = armv7
PACKAGE_FORMAT = deb
INSTALL_TARGET_PROCESSES = OpenLaraTouch

include $(THEOS)/makefiles/common.mk

APPLICATION_NAME = OpenLaraTouch
OpenLaraTouch_FILES = main.m OLAppDelegate.m OLFullGameViewController.mm \
	OLCompat.c OLPlatform.c UpstreamFull/libs/stb_vorbis/stb_vorbis.c \
	UpstreamFull/libs/tinf/tinflate.c
OpenLaraTouch_FRAMEWORKS = UIKit Foundation QuartzCore AudioToolbox \
	CoreGraphics OpenGLES GLKit
OpenLaraTouch_CFLAGS = -fobjc-arc -Wall -Wextra \
	-Wno-deprecated-declarations -Wno-invalid-source-encoding \
	-Wno-unused-parameter \
	-Wno-error=pointer-bool-conversion \
	-Wno-error=tautological-pointer-compare -Wno-error=sign-compare \
	-Wno-error=misleading-indentation \
	-fmodules-cache-path=/tmp/openlara-touch-modules
OpenLaraTouch_CCFLAGS = -std=gnu++11 -O3 -fno-exceptions -fno-rtti \
	-DNDEBUG -DOPENLARA_IPOD_TOUCH4 -D_GAPI_GLES2 -IUpstreamFull \
	-Wno-unused-parameter \
	-Wno-unused-function -Wno-unused-variable
OpenLaraTouch_LDFLAGS = -Wl,-flat_namespace
OpenLaraTouch_LIBRARIES = c++ c++abi
OpenLaraTouch_RESOURCE_DIRS = Resources

include $(THEOS_MAKE_PATH)/application.mk
