# iPod Hero: five-lane rhythm game for 320x240 click-wheel iPods.

IPODHERO_SRCDIR := $(APPSDIR)/plugins/ipodhero
IPODHERO_BUILDDIR := $(BUILDDIR)/apps/plugins/ipodhero

IPODHERO_SRC := $(call preprocess, $(IPODHERO_SRCDIR)/SOURCES)
IPODHERO_OBJ := $(call c2obj, $(IPODHERO_SRC))

OTHER_SRC += $(IPODHERO_SRC)
ROCKS += $(IPODHERO_BUILDDIR)/ipodhero.rock

$(IPODHERO_BUILDDIR)/ipodhero.rock: $(IPODHERO_OBJ)

$(IPODHERO_BUILDDIR)/%.o: $(IPODHERO_SRCDIR)/%.c \
				 $(IPODHERO_SRCDIR)/ipodhero.h \
				 $(IPODHERO_SRCDIR)/ipodhero.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) \
		$(PLUGINFLAGS) -c $< -o $@
