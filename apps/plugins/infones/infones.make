#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/
#

INFONESSRCDIR := $(APPSDIR)/plugins/infones
INFONESBUILDDIR := $(BUILDDIR)/apps/plugins/infones

ROCKS += $(INFONESBUILDDIR)/infones.rock

INFONES_SRC := $(call preprocess, $(INFONESSRCDIR)/SOURCES)
INFONES_OBJ := $(call c2obj, $(INFONES_SRC))

OTHER_SRC += $(INFONES_SRC)

INFONESFLAGS = $(filter-out -O%,$(PLUGINFLAGS)) -O2 \
	-Wno-strict-prototypes -Wno-unused-parameter -Wno-unused-function \
	-Wno-sequence-point -Wno-type-limits -Wno-sign-compare \
	-Wno-array-bounds -Wno-aggressive-loop-optimizations -Wno-parentheses

$(INFONESBUILDDIR)/infones.rock: $(INFONES_OBJ)

$(INFONESBUILDDIR)/%.o: $(INFONESSRCDIR)/%.c $(INFONESSRCDIR)/infones.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) $(INFONESFLAGS) -c $< -o $@
