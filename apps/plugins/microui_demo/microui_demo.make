#             __________               __________.__   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__ /\_ \
#                     \/            \/     \/    \/            \/

MICROUIDEMOSRCDIR := $(APPSDIR)/plugins/microui_demo
MICROUIDEMOBUILDDIR := $(BUILDDIR)/apps/plugins/microui_demo

ROCKS += $(MICROUIDEMOBUILDDIR)/microui_demo.rock

MICROUI_DEMO_SRC := $(call preprocess, $(MICROUIDEMOSRCDIR)/SOURCES)
MICROUI_DEMO_OBJ := $(call c2obj, $(MICROUI_DEMO_SRC))

OTHER_SRC += $(MICROUI_DEMO_SRC)

$(MICROUIDEMOBUILDDIR)/microui_demo.rock: $(MICROUI_DEMO_OBJ)
