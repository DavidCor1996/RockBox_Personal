## Automatically generated. http://www.rockbox.org/

export ROOTDIR=/home/david/Documents/RockBox_Personal-master
export FIRMDIR=$(ROOTDIR)/firmware
export APPSDIR=$(ROOTDIR)/apps
export TOOLSDIR=$(ROOTDIR)/tools
export DOCSDIR=$(ROOTDIR)/docs
export MANUALDIR=$(ROOTDIR)/manual
export DEBUG=-DDEBUG

export MODELNAME=ipodvideo
export FLASHFILE=
export TARGET_ID=15
export TARGET=-DIPOD_VIDEO

export SYSFONT=08-Schumacher-Clean
export ARCH=arch_none
export CPU=arm
export MANUFACTURER=ipod

export OBJDIR=$(ROOTDIR)/build-sim-video-5g
export BUILDDIR=$(ROOTDIR)/build-sim-video-5g
export RBCODEC_BLD=$(BUILDDIR)/lib/rbcodec

export MEMORYSIZE=64

export BUILDDATE:=$(shell date -u +'-DYEAR=%Y -DMONTH=%m -DDAY=%d')

export MKFIRMWARE=$(ROOTDIR)/tools/scramble -add=ipvd
export BMP2RB_MONO=$(ROOTDIR)/tools/bmp2rb -f 0
export BMP2RB_NATIVE=$(ROOTDIR)/tools/bmp2rb -f 4

export BINARY=rockboxui
export APPEXTRA=recorder:gui:radio
export ENABLEDPLUGINS=yes
export EXTRA_DEFINES= -DSIMULATOR -DHAVE_TEST_PLUGINS

# ✅ FIXED TOOLCHAIN (Linux)
export HOSTCC=/usr/bin/gcc
export HOSTAR=/usr/bin/ar
export CC=/usr/bin/gcc
export CPP=/usr/bin/cpp
export LD=/usr/bin/ld
export AR=/usr/bin/gcc-ar
export AS=/usr/bin/as
export OC=objcopy
export RANLIB=gcc-ranlib

export PREFIX=

export APP_TYPE=sdl-sim

export SIMDIR=$(ROOTDIR)/uisimulator/sdl

export GCCOPTS=-W -Wall -Wextra -Wundef -Os -Wstrict-prototypes -pipe -std=gnu99 -funit-at-a-time -fno-delete-null-pointer-checks -fno-strict-overflow -fno-common -fno-builtin -g -Wno-unused-result -I/usr/include/SDL2 -D_THREAD_SAFE -I$(SIMDIR) -Wno-pointer-sign -Wno-override-init -Wimplicit-fallthrough=0

export TARGET_INC=-I$(FIRMDIR)/target/arm/ipod/video -I$(FIRMDIR)/target/hosted/sdl -I$(FIRMDIR)/target/hosted -I$(FIRMDIR)/target/arm/ipod -I$(FIRMDIR)/target/arm/pp -I$(FIRMDIR)/target/arm

export SHARED_LDFLAGS=-shared
export SHARED_CFLAGS=-fPIC -fvisibility=hidden

# ✅ FIXED SDL + LINUX LINKING
export LDOPTS= -lm -ldl -lSDL2 -lpthread

export GLOBAL_LDOPTS=
export LDMAP_OPT=-Map

export UNAME=Linux

export MANUALDEV=ipodvideo

export USE_ELF=no
export PLUGIN_USE_ELF=no

export RBDIR=/.rockbox

export SDLCONFIG=/usr/bin/sdl2-config

export TOOLSET=rdf2binary convbdf codepages scramble bmp2rb

CONFIGURE_OPTIONS=  --target=$(MODELNAME) --ram=$(MEMORYSIZE) --rbdir=$(RBDIR) --type=s

include $(TOOLSDIR)/root.make
