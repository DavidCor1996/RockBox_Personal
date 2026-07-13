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

# The global dependency scanner does not see POKEMINIFLAGS and therefore
# records this local include at the build root. Provide the same header there
# so clean all-plugin builds retain the dependency instead of dropping it.
$(BUILDDIR)/freebios.h: $(POKEMINISRCDIR)/freebios/freebios.h
	$(call PRINTS,CP $(@F))cp $< $@

$(BUILDDIR)/PokeMini_ColorPal.h: $(POKEMINISRCDIR)/resource/PokeMini_ColorPal.h
	$(call PRINTS,CP $(@F))cp $< $@

$(POKEMINIBUILDDIR)/%.o: $(POKEMINISRCDIR)/%.c \
		$(POKEMINISRCDIR)/pokemini.make $(BUILDDIR)/sysfont.h \
		$(BUILDDIR)/lang_enum.h
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) $(POKEMINIFLAGS) -c $< -o $@
