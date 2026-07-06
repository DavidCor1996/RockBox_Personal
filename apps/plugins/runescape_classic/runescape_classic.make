#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/

RUNESCAPE_CLASSIC_SRCDIR := $(APPSDIR)/plugins/runescape_classic
RUNESCAPE_CLASSIC_BUILDDIR := $(BUILDDIR)/apps/plugins/runescape_classic

RUNESCAPE_CLASSIC_SRC := $(call preprocess, $(RUNESCAPE_CLASSIC_SRCDIR)/SOURCES)
RUNESCAPE_CLASSIC_OBJ := $(call c2obj, $(RUNESCAPE_CLASSIC_SRC))

OTHER_SRC += $(RUNESCAPE_CLASSIC_SRC)
ROCKS += $(RUNESCAPE_CLASSIC_BUILDDIR)/runescape_classic.rock

RUNESCAPE_CLASSIC_FLAGS = $(PLUGINFLAGS) \
	-DROCKBOX -DRENDER_SW -DNO_RSA -DNO_ISAAC -DCLIENT_CONFIG_NAME=\"runescape_classic\" \
	-DNDEBUG \
	-I$(RUNESCAPE_CLASSIC_SRCDIR) -I$(RUNESCAPE_CLASSIC_SRCDIR)/rsc-c \
	-I$(RUNESCAPE_CLASSIC_SRCDIR)/rsc-c/lib \
	-include $(RUNESCAPE_CLASSIC_SRCDIR)/rockbox-platform.h \
	-Wno-strict-prototypes -Wno-unused-parameter -Wno-unused-function \
	-Wno-missing-prototypes -Wno-old-style-definition

$(RUNESCAPE_CLASSIC_BUILDDIR)/runescape_classic.rock: $(RUNESCAPE_CLASSIC_OBJ) $(TLSFLIB)
ifdef APP_TYPE
$(RUNESCAPE_CLASSIC_BUILDDIR)/runescape_classic.rock: PLUGINLDFLAGS += -lm
endif

$(RUNESCAPE_CLASSIC_BUILDDIR)/%.o: $(RUNESCAPE_CLASSIC_SRCDIR)/%.c $(RUNESCAPE_CLASSIC_SRCDIR)/runescape_classic.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) $(RUNESCAPE_CLASSIC_FLAGS) -c $< -o $@
