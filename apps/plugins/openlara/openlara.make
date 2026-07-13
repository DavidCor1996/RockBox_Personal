# OpenLara fixed-point Rockbox plugin

OPENLARASRCDIR := $(APPSDIR)/plugins/openlara
OPENLARABUILDDIR := $(BUILDDIR)/apps/plugins/openlara

ifeq ($(PLUGIN_CXX_AVAILABLE),yes)
ROCKS += $(OPENLARABUILDDIR)/openlara.rock

OPENLARA_SRC := $(call preprocess,$(OPENLARASRCDIR)/SOURCES,$(PLUGIN_CXX_DEFINES))
OPENLARA_OBJ := $(OPENLARA_SRC:.cpp=.o)
OPENLARA_OBJ := $(call full_path_subst,$(ROOTDIR)/%,$(BUILDDIR)/%,$(OPENLARA_OBJ))

OTHER_SRC += $(OPENLARA_SRC)
OTHER_INC += -I$(OPENLARASRCDIR) -I$(OPENLARASRCDIR)/fixed

OPENLARA_CXXFLAGS = $(filter-out -O%,$(PLUGIN_CXXFLAGS)) -O3 \
	-I$(OPENLARASRCDIR) -I$(OPENLARASRCDIR)/fixed \
	-include $(OPENLARASRCDIR)/openlara_compat.h \
	-D__ROCKBOX__ -DNDEBUG -Wno-unused-parameter -Wno-unused-function

$(OPENLARABUILDDIR)/openlara.rock: $(OPENLARA_OBJ)

$(OPENLARABUILDDIR)/%.o: $(OPENLARASRCDIR)/%.cpp \
			       $(OPENLARASRCDIR)/openlara.make \
			       $(BUILDDIR)/lang_enum.h
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CXX $(subst $(ROOTDIR)/,,$<))$(PLUGIN_CXX) \
		-I$(dir $<) $(OPENLARA_CXXFLAGS) -c $< -o $@
endif
