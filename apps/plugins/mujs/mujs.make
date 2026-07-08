#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/
#

MUJS_SRCDIR := $(APPSDIR)/plugins/mujs
MUJS_BUILDDIR := $(BUILDDIR)/apps/plugins/mujs

MUJS_SRC := $(call preprocess, $(MUJS_SRCDIR)/SOURCES)
MUJS_OBJ := $(call c2obj, $(MUJS_SRC))

OTHER_SRC += $(MUJS_SRC)
ROCKS += $(MUJS_BUILDDIR)/mujs.rock

MUJSFLAGS = $(PLUGINFLAGS) -DROCKBOX_PLUGIN -I$(MUJS_SRCDIR)

$(MUJS_BUILDDIR)/mujs.rock: $(MUJS_OBJ) $(TLSFLIB)

$(MUJS_BUILDDIR)/%.o: $(MUJS_SRCDIR)/%.c $(MUJS_SRCDIR)/mujs.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) $(MUJSFLAGS) -c $< -o $@
