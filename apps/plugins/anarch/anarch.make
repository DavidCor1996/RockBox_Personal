# Anarch native Rockbox frontend.

ANARCH_SRCDIR := $(APPSDIR)/plugins/anarch
ANARCH_BUILDDIR := $(BUILDDIR)/apps/plugins/anarch

ANARCH_SRC := $(call preprocess, $(ANARCH_SRCDIR)/SOURCES)
ANARCH_OBJ := $(call c2obj, $(ANARCH_SRC))

OTHER_SRC += $(ANARCH_SRC)
ROCKS += $(ANARCH_BUILDDIR)/anarch.rock

$(ANARCH_BUILDDIR)/anarch.rock: $(ANARCH_OBJ)

$(ANARCH_BUILDDIR)/%.o: $(ANARCH_SRCDIR)/%.c \
				 $(ANARCH_SRCDIR)/anarch_platform.h \
				 $(ANARCH_SRCDIR)/anarch.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) \
		$(PLUGINFLAGS) -Wno-strict-prototypes -Wno-undef \
		-Wno-unused-parameter -O2 -c $< -o $@
