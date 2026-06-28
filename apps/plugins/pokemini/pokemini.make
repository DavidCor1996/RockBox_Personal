#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/                \/

POKEMINISRCDIR := $(APPSDIR)/plugins/pokemini
POKEMINIBUILDDIR := $(BUILDDIR)/apps/plugins/pokemini

ROCKS += $(POKEMINIBUILDDIR)/pokemini.rock

POKEMINI_SRC := $(call preprocess, $(POKEMINISRCDIR)/SOURCES)
POKEMINI_OBJ := $(call c2obj, $(POKEMINI_SRC))

OTHER_SRC += $(POKEMINI_SRC)

POKEMINIFLAGS = $(filter-out -O%,$(PLUGINFLAGS)) -O2 \
	-DPERFORMANCE -DNO_RTC \
	-include $(POKEMINISRCDIR)/pokemini_compat.h \
	-I$(POKEMINISRCDIR) \
	-I$(POKEMINISRCDIR)/source \
	-I$(POKEMINISRCDIR)/freebios \
	-I$(POKEMINISRCDIR)/resource \
	-Wno-strict-prototypes -Wno-unused-parameter -Wno-unused-function \
	-Wno-sign-compare -Wno-missing-field-initializers

$(POKEMINIBUILDDIR)/pokemini.rock: $(POKEMINI_OBJ)

$(POKEMINIBUILDDIR)/%.o: $(POKEMINISRCDIR)/%.c $(POKEMINISRCDIR)/pokemini.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) $(POKEMINIFLAGS) -c $< -o $@
