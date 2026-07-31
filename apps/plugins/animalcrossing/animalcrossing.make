ACSRCDIR := $(APPSDIR)/plugins/animalcrossing
ACOBJDIR := $(BUILDDIR)/apps/plugins/animalcrossing

AC_SRC := $(addprefix $(ACSRCDIR)/, \
	animalcrossing_rockbox.c ac_demake.c ac_demake_render.c ac_save.c)
AC_OBJ := $(call c2obj,$(AC_SRC))

OTHER_SRC += $(AC_SRC)
OTHER_INC += -I$(ACSRCDIR)

ACFLAGS = $(filter-out -O%,$(PLUGINFLAGS)) -O2 \
	-ffunction-sections -fdata-sections -Wstack-usage=2048

ifndef APP_TYPE
ROCKS += $(ACOBJDIR)/animalcrossing.ovl
AC_OUTLDS := $(ACOBJDIR)/animalcrossing.link
AC_OVLFLAGS = -Wl,--gc-sections -Wl,-Map,$(basename $@).map $(GLOBAL_LDOPTS)

$(ACOBJDIR)/animalcrossing.refmap: $(AC_OBJ)

$(AC_OUTLDS): $(PLUGIN_LDS) $(ACOBJDIR)/animalcrossing.refmap
	$(call PRINTS,PP $(@F))$(call preprocess2file,$<,$@,-DOVERLAY_OFFSET=$(shell \
		$(TOOLSDIR)/ovl_offset.pl $(ACOBJDIR)/animalcrossing.refmap))

$(ACOBJDIR)/animalcrossing.ovl: $(AC_OBJ) $(AC_OUTLDS)
	$(SILENT)$(CC) $(PLUGINFLAGS) -o $(basename $@).elf \
		$(filter %.o,$^) $(filter %.a,$+) \
		-lgcc -T$(AC_OUTLDS) $(AC_OVLFLAGS)
	$(call PRINTS,LD $(@F))$(call objcopy_plugin,$(basename $@).elf,$@)
else
ROCKS += $(ACOBJDIR)/animalcrossing.rock
$(ACOBJDIR)/animalcrossing.rock: $(AC_OBJ)
endif

$(ACOBJDIR)/%.o: $(ACSRCDIR)/%.c $(ACSRCDIR)/animalcrossing.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) $(ACFLAGS) \
		-I$(ACSRCDIR) -c $< -o $@
