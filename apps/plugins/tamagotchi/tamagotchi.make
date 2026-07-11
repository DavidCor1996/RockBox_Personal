TAMAGOTCHI_SRCDIR := $(APPSDIR)/plugins/tamagotchi
TAMAGOTCHI_OBJDIR := $(BUILDDIR)/apps/plugins/tamagotchi

TAMAGOTCHI_SRC := $(call preprocess, $(TAMAGOTCHI_SRCDIR)/SOURCES)
TAMAGOTCHI_OBJ := $(call c2obj, $(TAMAGOTCHI_SRC))

OTHER_SRC += $(TAMAGOTCHI_SRC)
ROCKS += $(TAMAGOTCHI_OBJDIR)/tamagotchi.rock

TAMAGOTCHIFLAGS = $(PLUGINFLAGS) \
	-I$(TAMAGOTCHI_SRCDIR) \
	-I$(TAMAGOTCHI_SRCDIR)/upstream/tamalib \
	-ffunction-sections -fdata-sections \
	-Wno-unused-parameter -Wno-missing-prototypes

$(TAMAGOTCHI_OBJDIR)/tamagotchi.rock: $(TAMAGOTCHI_OBJ)

$(TAMAGOTCHI_OBJDIR)/%.o: $(TAMAGOTCHI_SRCDIR)/%.c $(TAMAGOTCHI_SRCDIR)/tamagotchi.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) $(TAMAGOTCHIFLAGS) -c $< -o $@
