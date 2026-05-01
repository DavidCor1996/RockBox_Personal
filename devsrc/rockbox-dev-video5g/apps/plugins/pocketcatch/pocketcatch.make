#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/
#

#ifdef HAVE_LCD_COLOR
POCKETCATCH_SRCDIR := $(APPSDIR)/plugins/pocketcatch

ROCKS += $(BUILDDIR)/apps/plugins/pocketcatch.rock

POCKETCATCH_SRC := $(call preprocess, $(POCKETCATCH_SRCDIR)/SOURCES)
POCKETCATCH_OBJ := $(call c2obj, $(POCKETCATCH_SRC))

OTHER_SRC += $(POCKETCATCH_SRC)

$(BUILDDIR)/apps/plugins/pocketcatch.rock: $(POCKETCATCH_OBJ)
#endif
