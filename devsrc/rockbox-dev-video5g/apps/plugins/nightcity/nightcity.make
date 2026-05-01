#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/

#ifdef HAVE_LCD_COLOR
NIGHTCITY_SRCDIR := $(APPSDIR)/plugins/nightcity
NIGHTCITY_BUILDDIR := $(BUILDDIR)/apps/plugins/nightcity

ROCKS += $(NIGHTCITY_BUILDDIR)/nightcity.rock

NIGHTCITY_SRC := $(call preprocess, $(NIGHTCITY_SRCDIR)/SOURCES)
NIGHTCITY_OBJ := $(call c2obj, $(NIGHTCITY_SRC))

OTHER_SRC += $(NIGHTCITY_SRC)

$(NIGHTCITY_BUILDDIR)/nightcity.rock: $(NIGHTCITY_OBJ)
#endif
