# Pocket Sky: a native, offline sky map for 320x240 click-wheel iPods.

POCKETSKY_SRCDIR := $(APPSDIR)/plugins/pocketsky
POCKETSKY_BUILDDIR := $(BUILDDIR)/apps/plugins/pocketsky

POCKETSKY_SRC := $(call preprocess, $(POCKETSKY_SRCDIR)/SOURCES)
POCKETSKY_OBJ := $(call c2obj, $(POCKETSKY_SRC))
POCKETSKY_UPSTREAM_SRC := $(filter $(POCKETSKY_SRCDIR)/upstream/%, \
	$(POCKETSKY_SRC))
POCKETSKY_UPSTREAM_OBJ := $(call c2obj, $(POCKETSKY_UPSTREAM_SRC))
POCKETSKY_NATIVE_SRC := $(filter-out $(POCKETSKY_UPSTREAM_SRC), \
	$(POCKETSKY_SRC))
POCKETSKY_NATIVE_OBJ := $(call c2obj, $(POCKETSKY_NATIVE_SRC))

OTHER_SRC += $(POCKETSKY_SRC)
ROCKS += $(POCKETSKY_BUILDDIR)/pocketsky.rock

POCKETSKY_FLAGS = -I$(POCKETSKY_SRCDIR) \
	-I$(POCKETSKY_SRCDIR)/upstream/astroterm \
	-I$(POCKETSKY_SRCDIR)/upstream/astronomy_engine \
	-I$(POCKETSKY_SRCDIR)/upstream/musl_math \
	$(PLUGINFLAGS) \
	-DASTRONOMY_ENGINE_NO_CURRENT_TIME \
	-ffunction-sections -fdata-sections

POCKETSKY_UPSTREAM_FLAGS = $(POCKETSKY_FLAGS) \
	-include $(POCKETSKY_SRCDIR)/rb_compat.h \
	-Wno-unused-parameter -Wno-unused-function -Wno-sign-compare \
	-Wno-missing-prototypes -Wno-old-style-definition \
	-Wno-strict-prototypes -Wno-shadow

$(POCKETSKY_BUILDDIR)/pocketsky.rock: $(POCKETSKY_OBJ) $(TLSFLIB)

$(POCKETSKY_NATIVE_OBJ): $(POCKETSKY_BUILDDIR)/%.o: \
		$(POCKETSKY_SRCDIR)/%.c \
		$(POCKETSKY_SRCDIR)/pocketsky.make \
		$(POCKETSKY_SRCDIR)/math.h \
		$(BUILDDIR)/sysfont.h $(BUILDDIR)/lang_enum.h
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) \
		$(POCKETSKY_FLAGS) -c $< -o $@

$(POCKETSKY_UPSTREAM_OBJ): $(POCKETSKY_BUILDDIR)/%.o: \
		$(POCKETSKY_SRCDIR)/%.c \
		$(POCKETSKY_SRCDIR)/pocketsky.make \
		$(POCKETSKY_SRCDIR)/rb_compat.h \
		$(POCKETSKY_SRCDIR)/math.h \
		$(BUILDDIR)/sysfont.h $(BUILDDIR)/lang_enum.h
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) \
		$(POCKETSKY_UPSTREAM_FLAGS) -c $< -o $@
