SMSGG_SRCDIR := $(APPSDIR)/plugins/smsgg
SMSGG_OBJDIR := $(BUILDDIR)/apps/plugins/smsgg

SMSGG_SRC := $(call preprocess, $(SMSGG_SRCDIR)/SOURCES)
SMSGG_OBJ := $(call c2obj, $(SMSGG_SRC))

OTHER_SRC += $(SMSGG_SRC)
ROCKS += $(SMSGG_OBJDIR)/smsgg.rock

SMSGGFLAGS = $(filter-out -O%,$(PLUGINFLAGS)) -O2 \
	-I$(SMSGG_SRCDIR) \
	-I$(SMSGG_SRCDIR)/upstream \
	-I$(SMSGG_SRCDIR)/upstream/cpu \
	-I$(SMSGG_SRCDIR)/upstream/sound \
	-ffunction-sections -fdata-sections \
	-Wno-unused-parameter -Wno-unused-function -Wno-sign-compare \
	-Wno-missing-prototypes -Wno-old-style-definition \
	-Wno-strict-prototypes

$(SMSGG_OBJDIR)/smsgg.rock: $(SMSGG_OBJ)

$(SMSGG_OBJDIR)/%.o: $(SMSGG_SRCDIR)/%.c $(SMSGG_SRCDIR)/smsgg.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) $(SMSGGFLAGS) -c $< -o $@
