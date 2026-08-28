ZELDA3SRCDIR := $(APPSDIR)/plugins/zelda3
ZELDA3UPSTREAM := $(ZELDA3SRCDIR)/upstream
ZELDA3UPSTREAMSRC := $(ZELDA3UPSTREAM)/src
ZELDA3UPSTREAMSNES := $(ZELDA3UPSTREAM)/snes
ZELDA3OBJDIR := $(BUILDDIR)/apps/plugins/zelda3

ZELDA3_ENGINE_SRC := \
	$(filter-out $(ZELDA3UPSTREAMSRC)/main.c \
		$(ZELDA3UPSTREAMSRC)/config.c \
		$(ZELDA3UPSTREAMSRC)/zelda_cpu_infra.c \
		$(ZELDA3UPSTREAMSRC)/opengl.c \
		$(ZELDA3UPSTREAMSRC)/glsl_shader.c, \
		$(wildcard $(ZELDA3UPSTREAMSRC)/*.c)) \
	$(ZELDA3UPSTREAMSNES)/ppu.c \
	$(ZELDA3UPSTREAMSNES)/dma.c \
	$(ZELDA3UPSTREAMSNES)/dsp.c
ZELDA3_PLATFORM_SRC := $(addprefix $(ZELDA3SRCDIR)/, \
	zelda3_rockbox.c zelda3_video.c zelda3_input.c zelda3_audio.c \
	zelda3_opus_stubs.c zelda3_snes_stubs.c)
ZELDA3_SRC := $(ZELDA3_ENGINE_SRC) $(ZELDA3_PLATFORM_SRC)
ZELDA3_OBJ := $(call c2obj,$(ZELDA3_SRC))

OTHER_SRC += $(ZELDA3_SRC)

ZELDA3FLAGS = $(filter-out -O%,$(PLUGINFLAGS)) \
	-idirafter$(ZELDA3SRCDIR) -idirafter$(ZELDA3UPSTREAM) \
	-idirafter$(ZELDA3UPSTREAMSRC) -idirafter$(ZELDA3UPSTREAMSNES) \
	-I$(ROOTDIR)/lib/tlsf/src \
	-O3 -std=gnu99 -DNDEBUG \
	-DROCKBOX -DTARGET_ROCKBOX -DSYSTEM_VOLUME_MIXER_AVAILABLE=0 \
	-fsigned-char -fno-strict-aliasing -fwrapv -ffast-math \
	-ffunction-sections -fdata-sections -fomit-frame-pointer \
	-Wno-unused-parameter -Wno-unused-function \
	-Wno-sign-compare -Wno-missing-prototypes -Wno-strict-prototypes \
	-Wno-old-style-definition -Wno-parentheses -Wno-pointer-sign \
	-Wno-format -Wno-return-type -Wstack-usage=4096

ifndef APP_TYPE
ROCKS += $(ZELDA3OBJDIR)/zelda3.ovl
ZELDA3_OUTLDS := $(ZELDA3OBJDIR)/zelda3.link
ZELDA3_OVLFLAGS = -Wl,--gc-sections -Wl,-Map,$(basename $@).map \
	$(GLOBAL_LDOPTS)

$(ZELDA3OBJDIR)/zelda3.refmap: $(ZELDA3_OBJ) $(TLSFLIB)

$(ZELDA3_OUTLDS): $(PLUGIN_LDS) $(ZELDA3OBJDIR)/zelda3.refmap
	$(call PRINTS,PP $(@F))$(call preprocess2file,$<,$@,-DOVERLAY_OFFSET=$(shell \
		$(TOOLSDIR)/ovl_offset.pl $(ZELDA3OBJDIR)/zelda3.refmap))

$(ZELDA3OBJDIR)/zelda3.ovl: $(ZELDA3_OBJ) $(TLSFLIB) $(ZELDA3_OUTLDS)
	$(SILENT)$(CC) $(PLUGINFLAGS) -o $(basename $@).elf \
		$(filter %.o,$^) $(filter %.a,$+) -lgcc -T$(ZELDA3_OUTLDS) \
		$(ZELDA3_OVLFLAGS)
	$(call PRINTS,LD $(@F))$(call objcopy_plugin,$(basename $@).elf,$@)
else
ROCKS += $(ZELDA3OBJDIR)/zelda3.rock
$(ZELDA3OBJDIR)/zelda3.rock: $(ZELDA3_OBJ) $(TLSFLIB)
endif

$(ZELDA3OBJDIR)/upstream/src/%.o: $(ZELDA3UPSTREAMSRC)/%.c \
		$(ZELDA3SRCDIR)/zelda3.make $(ZELDA3SRCDIR)/zelda3_compat.h
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) $(ZELDA3FLAGS) \
		-include $(ZELDA3SRCDIR)/zelda3_compat.h -c $< -o $@

$(ZELDA3OBJDIR)/upstream/snes/%.o: $(ZELDA3UPSTREAMSNES)/%.c \
		$(ZELDA3SRCDIR)/zelda3.make $(ZELDA3SRCDIR)/zelda3_compat.h
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) $(ZELDA3FLAGS) \
		-include $(ZELDA3SRCDIR)/zelda3_compat.h -c $< -o $@

$(ZELDA3OBJDIR)/upstream/third_party/%.o: $(ZELDA3UPSTREAM)/third_party/%.c \
		$(ZELDA3SRCDIR)/zelda3.make $(ZELDA3SRCDIR)/zelda3_compat.h
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) $(ZELDA3FLAGS) \
		-include $(ZELDA3SRCDIR)/zelda3_compat.h -c $< -o $@

$(ZELDA3OBJDIR)/%.o: $(ZELDA3SRCDIR)/%.c \
		$(ZELDA3SRCDIR)/zelda3.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) $(ZELDA3FLAGS) \
		-c $< -o $@
