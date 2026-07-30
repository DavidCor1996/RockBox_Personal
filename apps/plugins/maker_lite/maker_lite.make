MAKERLITESRCDIR := $(APPSDIR)/plugins/maker_lite
MAKERLITEOBJDIR := $(BUILDDIR)/apps/plugins/maker_lite

MAKERLITE_SRC := $(call preprocess, $(MAKERLITESRCDIR)/SOURCES)
MAKERLITE_OBJ := $(call c2obj, $(MAKERLITE_SRC))

OTHER_SRC += $(MAKERLITE_SRC)
ROCKS += $(MAKERLITEOBJDIR)/maker_lite.rock

MAKERLITEFLAGS = $(filter-out -O%,$(PLUGINFLAGS)) -O2 \
	-I$(MAKERLITESRCDIR) -I$(ROOTDIR)/lib/maker_lite

$(MAKERLITEOBJDIR)/maker_lite.rock: $(MAKERLITE_OBJ)

$(MAKERLITEOBJDIR)/%.o: $(MAKERLITESRCDIR)/%.c \
		$(MAKERLITESRCDIR)/maker_lite.make \
		$(MAKERLITESRCDIR)/maker_lite.h \
		$(ROOTDIR)/lib/maker_lite/maker_lite.h \
		$(ROOTDIR)/lib/maker_lite/maker_lite.c
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) \
		$(MAKERLITEFLAGS) -c $< -o $@
