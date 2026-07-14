#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/

SCUMMVMSRCDIR := $(APPSDIR)/plugins/scummvm
SCUMMVMBUILDDIR := $(BUILDDIR)/apps/plugins/scummvm

ROCKS += $(SCUMMVMBUILDDIR)/scummvm.rock

SCUMMVM_SRC := $(call preprocess, $(SCUMMVMSRCDIR)/SOURCES)
SCUMMVM_OBJ := $(call c2obj, $(SCUMMVM_SRC))
SCUMMVM_OBJ := $(SCUMMVM_OBJ:.cpp=.o)

OTHER_SRC += $(SCUMMVM_SRC)

SCUMMVMFLAGS = $(filter-out -O%,$(PLUGINFLAGS)) -Os
SCUMMVM_CXXFLAGS = -iquote $(SCUMMVMSRCDIR)/upstream-1.9.0 \
	-iquote $(SCUMMVMSRCDIR)/upstream-1.9.0/engines \
	$(PLUGIN_CXXFLAGS) -I$(SCUMMVMSRCDIR) \
	-DHAVE_CONFIG_H -DNDEBUG \
	-DROCKBOX_SCUMMVM_EMBEDDED \
	-D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS

ifneq ($(findstring -DSIMULATOR,$(EXTRA_DEFINES)),)
SCUMMVM_CXXFLAGS += -DPOSIX
endif

$(SCUMMVMBUILDDIR)/scummvm.rock: $(SCUMMVM_OBJ)

$(SCUMMVMBUILDDIR)/%.o: $(SCUMMVMSRCDIR)/%.c $(SCUMMVMSRCDIR)/scummvm.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) $(SCUMMVMFLAGS) -c $< -o $@

$(SCUMMVMBUILDDIR)/%.o: $(SCUMMVMSRCDIR)/%.cpp \
                       $(SCUMMVMSRCDIR)/scummvm.make \
                       $(BUILDDIR)/lang_enum.h
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CXX $(subst $(ROOTDIR)/,,$<))$(PLUGIN_CXX) $(SCUMMVM_CXXFLAGS) -c $< -o $@
