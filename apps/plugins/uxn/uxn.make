UXNSRCDIR := $(APPSDIR)/plugins/uxn
UXNBUILDDIR := $(BUILDDIR)/apps/plugins/uxn

UXN_SRC := $(call preprocess, $(UXNSRCDIR)/SOURCES)
UXN_OBJ := $(call c2obj, $(UXN_SRC))

OTHER_SRC += $(UXN_SRC)
ROCKS += $(UXNBUILDDIR)/uxn.rock

UXNFLAGS = $(filter-out -O%,$(PLUGINFLAGS)) -O2 \
	-I$(UXNSRCDIR) -ffunction-sections -fdata-sections \
	-Wno-unused-parameter

$(UXNBUILDDIR)/uxn.rock: $(UXN_OBJ)

$(UXNBUILDDIR)/%.o: $(UXNSRCDIR)/%.c $(UXNSRCDIR)/uxn.make \
		$(BUILDDIR)/sysfont.h $(BUILDDIR)/lang_enum.h
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) \
		$(UXNFLAGS) -c $< -o $@
