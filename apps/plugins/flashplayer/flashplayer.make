#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/                \/

FLASHPLAYERSRCDIR := $(APPSDIR)/plugins/flashplayer
FLASHPLAYERBUILDDIR := $(BUILDDIR)/apps/plugins/flashplayer

ifeq ($(PLUGIN_CXX_AVAILABLE),yes)
ROCKS += $(FLASHPLAYERBUILDDIR)/flashplayer.rock

FLASHPLAYER_SRC := $(call preprocess, $(FLASHPLAYERSRCDIR)/SOURCES,$(PLUGIN_CXX_DEFINES))
FLASHPLAYER_OBJ := $(FLASHPLAYER_SRC:.c=.o)
FLASHPLAYER_OBJ := $(FLASHPLAYER_OBJ:.cpp=.o)
FLASHPLAYER_OBJ := $(call full_path_subst,$(ROOTDIR)/%,$(BUILDDIR)/%,$(FLASHPLAYER_OBJ))

OTHER_SRC += $(FLASHPLAYER_SRC)
OTHER_INC += -I$(FLASHPLAYERSRCDIR) -I$(APPSDIR)/plugins/imageviewer/png \
	-I$(FLASHPLAYERSRCDIR)/gameswf_compat \
	-I$(FLASHPLAYERSRCDIR)/gameswf \
	-I$(FLASHPLAYERSRCDIR)/gameswf/base \
	-I$(FLASHPLAYERSRCDIR)/gameswf/gameswf

FLASHPLAYER_CFLAGS = $(PLUGINFLAGS) -I$(FLASHPLAYERSRCDIR) \
	-I$(APPSDIR)/plugins/imageviewer/png \
	-Wno-strict-prototypes -Wno-unused-parameter -Wno-unused-function
FLASHPLAYER_CXXFLAGS = $(filter-out -Os,$(PLUGIN_CXXFLAGS)) -O2 -I$(FLASHPLAYERSRCDIR) \
	-I$(APPSDIR)/plugins/imageviewer/png \
	-I$(FLASHPLAYERSRCDIR)/gameswf_compat \
	-I$(FLASHPLAYERSRCDIR)/gameswf \
	-I$(FLASHPLAYERSRCDIR)/gameswf/base \
	-I$(FLASHPLAYERSRCDIR)/gameswf/gameswf \
	-include $(FLASHPLAYERSRCDIR)/gameswf_compat/compatibility_include.h \
	-DNDEBUG -w

FLASHPLAYER_HEADERS := $(wildcard $(FLASHPLAYERSRCDIR)/*.h \
    $(FLASHPLAYERSRCDIR)/gameswf/base/*.h \
    $(FLASHPLAYERSRCDIR)/gameswf/gameswf/*.h \
    $(FLASHPLAYERSRCDIR)/gameswf_compat/*.h)
$(FLASHPLAYER_OBJ): $(FLASHPLAYER_HEADERS)

$(FLASHPLAYERBUILDDIR)/flashplayer.rock: $(FLASHPLAYER_OBJ)

$(FLASHPLAYERBUILDDIR)/%.o: $(FLASHPLAYERSRCDIR)/%.cpp \
                           $(FLASHPLAYERSRCDIR)/flashplayer.make \
                           $(BUILDDIR)/lang_enum.h
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CXX $(subst $(ROOTDIR)/,,$<))$(PLUGIN_CXX) -I$(dir $<) $(FLASHPLAYER_CXXFLAGS) -c $< -o $@

$(FLASHPLAYERBUILDDIR)/%.o: $(FLASHPLAYERSRCDIR)/%.c $(FLASHPLAYERSRCDIR)/flashplayer.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) $(FLASHPLAYER_CFLAGS) -c $< -o $@

$(FLASHPLAYERBUILDDIR)/../imageviewer/png/%.o: $(APPSDIR)/plugins/imageviewer/png/%.c $(FLASHPLAYERSRCDIR)/flashplayer.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) $(FLASHPLAYER_CFLAGS) -c $< -o $@
endif
