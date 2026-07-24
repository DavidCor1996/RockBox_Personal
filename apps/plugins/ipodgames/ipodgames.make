# iPod Click Wheel Games compatibility-layer prototype.

IPODGAMES_SRCDIR := $(APPSDIR)/plugins/ipodgames
IPODGAMES_BUILDDIR := $(BUILDDIR)/apps/plugins/ipodgames

IPODGAMES_SRC := $(call preprocess, $(IPODGAMES_SRCDIR)/SOURCES)
IPODGAMES_OBJ := $(call c2obj, $(IPODGAMES_SRC))

# The ARM compatibility core and software GL renderer are throughput-bound.
# Rockbox's default -Os leaves hot helpers out of line, which makes retail
# games miss their 60 Hz cadence badly on the iPod's ARM926EJ-S.
IPODGAMES_FLAGS = $(filter-out -O%,$(PLUGINFLAGS)) -O3

OTHER_SRC += $(IPODGAMES_SRC)
ROCKS += $(IPODGAMES_BUILDDIR)/ipodgames.rock

$(IPODGAMES_BUILDDIR)/ipodgames.rock: $(IPODGAMES_OBJ)

$(IPODGAMES_BUILDDIR)/%.o: $(IPODGAMES_SRCDIR)/%.c \
                                  $(IPODGAMES_SRCDIR)/ipodgames.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) \
		$(IPODGAMES_FLAGS) -c $< -o $@
