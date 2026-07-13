GWATCH_SRCDIR := $(APPSDIR)/plugins/gwatch
GWATCH_OBJDIR := $(BUILDDIR)/apps/plugins/gwatch

GWATCH_SRC := $(call preprocess, $(GWATCH_SRCDIR)/SOURCES)
GWATCH_OBJ := $(call c2obj, $(GWATCH_SRC))

OTHER_SRC += $(GWATCH_SRC)
ROCKS += $(GWATCH_OBJDIR)/gwatch.rock

GWATCHFLAGS = $(filter-out -O%,$(PLUGINFLAGS)) -O2 \
	-DROCKBOX_PLUGIN -DBZ_NO_STDIO -DLUA_32BITS \
	-include $(GWATCH_SRCDIR)/gwatch_compat.h \
	-I$(GWATCH_SRCDIR) \
	-I$(GWATCH_SRCDIR)/gwrom \
	-I$(GWATCH_SRCDIR)/gwlua \
	-I$(GWATCH_SRCDIR)/bzip2 \
	-I$(GWATCH_SRCDIR)/lua/src \
	-I$(GWATCH_SRCDIR)/retroluxury/src \
	-ffunction-sections -fdata-sections \
	-Wno-unused-parameter -Wno-unused-function -Wno-sign-compare \
	-Wno-missing-prototypes -Wno-old-style-definition \
	-Wno-strict-prototypes -Wno-shadow

$(GWATCH_OBJDIR)/gwatch.rock: $(GWATCH_OBJ) $(TLSFLIB)

$(GWATCH_OBJDIR)/%.o: $(GWATCH_SRCDIR)/%.c $(GWATCH_SRCDIR)/gwatch.make \
		$(GWATCH_SRCDIR)/gwatch_compat.h $(GWATCH_SRCDIR)/locale.h \
		$(GWATCH_SRCDIR)/math.h $(GWATCH_SRCDIR)/stdio.h \
		$(BUILDDIR)/sysfont.h $(BUILDDIR)/lang_enum.h
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) $(GWATCHFLAGS) -c $< -o $@
