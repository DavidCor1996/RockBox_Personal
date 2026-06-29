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
OTHER_INC += -I$(SCUMMVMSRCDIR) \
	-I$(SCUMMVMSRCDIR)/upstream-1.9.0 \
	-I$(SCUMMVMSRCDIR)/upstream-1.9.0/engines \
	-DHAVE_CONFIG_H -DSCUMM_LITTLE_ENDIAN

SCUMMVMFLAGS = $(filter-out -O%,$(PLUGINFLAGS)) -Os
SCUMMVM_CXXFLAGS = -I$(SCUMMVMSRCDIR)/upstream-1.9.0 \
	-I$(SCUMMVMSRCDIR)/upstream-1.9.0/engines \
	$(PLUGIN_CXXFLAGS) -I$(SCUMMVMSRCDIR) \
	-DHAVE_CONFIG_H \
	-D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS

$(SCUMMVMBUILDDIR)/scummvm.rock: $(SCUMMVM_OBJ)

$(SCUMMVMBUILDDIR)/%.o: $(SCUMMVMSRCDIR)/%.c $(SCUMMVMSRCDIR)/scummvm.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) $(SCUMMVMFLAGS) -c $< -o $@

$(SCUMMVMBUILDDIR)/%.o: $(SCUMMVMSRCDIR)/%.cpp $(SCUMMVMSRCDIR)/scummvm.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CXX $(subst $(ROOTDIR)/,,$<))$(PLUGIN_CXX) -I$(dir $<) $(SCUMMVM_CXXFLAGS) -c $< -o $@
