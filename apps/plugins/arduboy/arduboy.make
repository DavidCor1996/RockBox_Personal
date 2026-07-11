ARDUBOY_SRCDIR := $(APPSDIR)/plugins/arduboy
ARDUBOY_OBJDIR := $(BUILDDIR)/apps/plugins/arduboy

ARDUBOY_SRC := $(call preprocess, $(ARDUBOY_SRCDIR)/SOURCES)
ARDUBOY_OBJ := $(call c2obj, $(ARDUBOY_SRC))

OTHER_SRC += $(ARDUBOY_SRC)
ROCKS += $(ARDUBOY_OBJDIR)/arduboy.rock

ARDUBOYFLAGS = $(filter-out -O%,$(PLUGINFLAGS)) -O2 \
	-I$(ARDUBOY_SRCDIR) \
	-ffunction-sections -fdata-sections \
	-Wno-unused-parameter

$(ARDUBOY_OBJDIR)/arduboy.rock: $(ARDUBOY_OBJ)

$(ARDUBOY_OBJDIR)/%.o: $(ARDUBOY_SRCDIR)/%.c $(ARDUBOY_SRCDIR)/arduboy.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) $(ARDUBOYFLAGS) -c $< -o $@
