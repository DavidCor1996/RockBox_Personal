CPS1SRCDIR := $(APPSDIR)/plugins/cps1
CPS1OBJDIR := $(BUILDDIR)/apps/plugins/cps1

ifeq ($(CPS1_NONCOMMERCIAL),1)
ifeq ($(PLUGIN_CXX_AVAILABLE),yes)

CPS1_SRC := $(call preprocess,$(CPS1SRCDIR)/SOURCES,$(PLUGIN_CXX_DEFINES))
CPS1_OBJ := $(call c2obj,$(CPS1_SRC))
CPS1_OBJ := $(CPS1_OBJ:.cpp=.o)

OTHER_SRC += $(CPS1_SRC)
ROCKS += $(CPS1OBJDIR)/cps1.rock

CPS1FLAGS = $(filter-out -O%,$(PLUGINFLAGS)) -O3 \
	-I$(CPS1SRCDIR) -I$(CPS1SRCDIR)/upstream/burn \
	-I$(CPS1SRCDIR)/upstream/burn/capcom \
	-I$(CPS1SRCDIR)/upstream/cpu/z80 \
	-I$(CPS1SRCDIR)/upstream/cpu/cyclone \
	-I$(CPS1SRCDIR)/upstream/cpu/m68k \
	-include $(CPS1SRCDIR)/cps1_core_compat.h \
	-DOOPSWARE_FIX -DNDEBUG -ffunction-sections -fdata-sections \
	-fomit-frame-pointer -fno-strict-aliasing \
	-Wno-unused-parameter -Wno-unused-function -Wno-sign-compare \
	-Wno-write-strings -Wno-pointer-to-int-cast \
	-Wno-int-to-pointer-cast

CPS1_CXXFLAGS = $(PLUGIN_CXXFLAGS) $(CPS1FLAGS)

$(CPS1OBJDIR)/cps1.rock: $(CPS1_OBJ) $(TLSFLIB)

$(CPS1OBJDIR)/%.o: $(CPS1SRCDIR)/%.c $(CPS1SRCDIR)/cps1.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) \
		$(CPS1FLAGS) -c $< -o $@

$(CPS1OBJDIR)/%.o: $(CPS1SRCDIR)/%.cpp $(CPS1SRCDIR)/cps1.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CXX $(subst $(ROOTDIR)/,,$<))$(PLUGIN_CXX) -I$(dir $<) \
		$(CPS1_CXXFLAGS) -c $< -o $@

$(CPS1OBJDIR)/%.o: $(CPS1SRCDIR)/%.s $(CPS1SRCDIR)/cps1.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,AS $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) \
		$(CPS1FLAGS) -c $< -o $@

endif
endif
