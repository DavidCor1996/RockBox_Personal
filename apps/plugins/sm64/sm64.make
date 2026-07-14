SM64SRCDIR := $(APPSDIR)/plugins/sm64
SM64UPSTREAM := $(SM64SRCDIR)/upstream
SM64OBJDIR := $(BUILDDIR)/apps/plugins/sm64

SM64_CORE_SRC := \
	$(wildcard $(SM64UPSTREAM)/src/engine/*.c) \
	$(filter-out $(SM64UPSTREAM)/src/game/main.c,$(wildcard $(SM64UPSTREAM)/src/game/*.c)) \
	$(wildcard $(SM64UPSTREAM)/src/audio/*.c) \
	$(wildcard $(SM64UPSTREAM)/src/menu/*.c) \
	$(wildcard $(SM64UPSTREAM)/src/buffers/*.c) \
	$(wildcard $(SM64UPSTREAM)/actors/*.c) \
	$(wildcard $(SM64UPSTREAM)/levels/*.c) \
	$(wildcard $(SM64UPSTREAM)/levels/*/leveldata.c) \
	$(wildcard $(SM64UPSTREAM)/levels/*/script.c) \
	$(wildcard $(SM64UPSTREAM)/levels/*/geo.c) \
	$(wildcard $(SM64UPSTREAM)/bin/*.c) \
	$(wildcard $(SM64UPSTREAM)/data/*.c) \
	$(wildcard $(SM64UPSTREAM)/src/goddard/*.c) \
	$(wildcard $(SM64UPSTREAM)/src/goddard/dynlists/*.c)

SM64_ULTRA_SRC := $(addprefix $(SM64UPSTREAM)/lib/src/, \
	alBnkfNew.c guLookAtRef.c guMtxF2L.c guNormalize.c guOrthoF.c \
	guPerspectiveF.c guRotateF.c guScaleF.c guTranslateF.c)

SM64_PC_SRC := $(addprefix $(SM64UPSTREAM)/src/pc/, \
	mixer.c ultra_reimplementation.c gfx/gfx_cc.c gfx/gfx_pc.c \
	gfx/gfx_soft.c)

SM64_GENERATED_SRC := \
	$(SM64UPSTREAM)/build/us_pc/assets/mario_anim_data.c \
	$(SM64UPSTREAM)/build/us_pc/assets/demo_data.c \
	$(wildcard $(SM64UPSTREAM)/build/us_pc/bin/*_skybox.c) \
	$(SM64UPSTREAM)/sound/sound_data.c

SM64_PLATFORM_SRC := $(addprefix $(SM64SRCDIR)/, \
	sm64_rockbox.c sm64_audio.c sm64_input.c sm64_video.c sm64_math.c)
SM64_SRC := $(SM64_CORE_SRC) $(SM64_ULTRA_SRC) $(SM64_PC_SRC) \
	$(SM64_GENERATED_SRC) $(SM64_PLATFORM_SRC)
SM64_OBJ := $(call c2obj,$(SM64_SRC))

OTHER_SRC += $(SM64_SRC)
OTHER_INC += -I$(SM64SRCDIR) -I$(SM64UPSTREAM) \
	-I$(SM64UPSTREAM)/include -I$(SM64UPSTREAM)/src \
	-iquote $(SM64UPSTREAM)/include \
	-I$(SM64UPSTREAM)/build/us_pc -I$(SM64UPSTREAM)/build/us_pc/include \
	-I$(SM64UPSTREAM)/build/us_rockbox32 \
	-I$(ROOTDIR)/lib/tlsf/src \
	-D_LANGUAGE_C -DVERSION_US \
	-DF3DEX_GBI_2E -DNON_MATCHING -DAVOID_UB -DNO_SEGMENTED_MEMORY \
	-DENABLE_SOFTRAST -DTARGET_ROCKBOX

SM64FLAGS = -I$(SM64SRCDIR) -I$(SM64UPSTREAM) -I$(SM64UPSTREAM)/include \
	-I$(SM64UPSTREAM)/src -I$(SM64UPSTREAM)/build/us_pc \
	-I$(SM64UPSTREAM)/build/us_pc/include \
	-I$(SM64UPSTREAM)/build/us_rockbox32 \
	-I$(ROOTDIR)/lib/tlsf/src $(filter-out -O%,$(PLUGINFLAGS)) -O3 -std=gnu99 \
	-D_LANGUAGE_C -DVERSION_US -DF3DEX_GBI_2E -DNON_MATCHING -DAVOID_UB \
	-DNO_SEGMENTED_MEMORY -DENABLE_SOFTRAST -DTARGET_ROCKBOX -DNDEBUG \
	-fsigned-char -fno-strict-aliasing -fwrapv -ffast-math \
	-fomit-frame-pointer -Wno-unused-parameter -Wno-unused-function \
	-Wno-sign-compare -Wno-missing-prototypes -Wno-strict-prototypes \
	-Wno-old-style-definition -Wno-parentheses -Wno-pointer-sign \
	-Wno-implicit-function-declaration -Wno-int-conversion \
	-Wstack-usage=2048

ifndef APP_TYPE
ROCKS += $(SM64OBJDIR)/sm64.ovl
SM64_OUTLDS := $(SM64OBJDIR)/sm64.link
SM64_OVLFLAGS = -Wl,--gc-sections -Wl,-Map,$(basename $@).map $(GLOBAL_LDOPTS)

$(SM64OBJDIR)/sm64.refmap: $(SM64_OBJ) $(TLSFLIB)

$(SM64_OUTLDS): $(PLUGIN_LDS) $(SM64OBJDIR)/sm64.refmap
	$(call PRINTS,PP $(@F))$(call preprocess2file,$<,$@,-DOVERLAY_OFFSET=$(shell \
		$(TOOLSDIR)/ovl_offset.pl $(SM64OBJDIR)/sm64.refmap))

$(SM64OBJDIR)/sm64.ovl: $(SM64_OBJ) $(TLSFLIB) $(SM64_OUTLDS)
	$(SILENT)$(CC) $(PLUGINFLAGS) -o $(basename $@).elf \
		$(filter %.o,$^) $(filter %.a,$+) -lgcc -T$(SM64_OUTLDS) \
		$(SM64_OVLFLAGS)
	$(call PRINTS,LD $(@F))$(call objcopy_plugin,$(basename $@).elf,$@)
else
ROCKS += $(SM64OBJDIR)/sm64.rock
$(SM64OBJDIR)/sm64.rock: $(SM64_OBJ) $(TLSFLIB)
endif

# The global dependency generator flattens this generated header into the
# build root.  Keep that dependency satisfied while compilation itself uses
# the pinned upstream build directory above.
$(BUILDDIR)/text_menu_strings.h: $(SM64UPSTREAM)/build/us_pc/include/text_menu_strings.h
	$(SILENT)cp $< $@

$(BUILDDIR)/text_strings.h: $(SM64UPSTREAM)/build/us_pc/include/text_strings.h \
		$(BUILDDIR)/text_menu_strings.h
	$(SILENT)cp $< $@

$(BUILDDIR)/level_headers.h: $(SM64UPSTREAM)/build/us_pc/include/level_headers.h
	$(SILENT)cp $< $@

$(SM64OBJDIR)/upstream/%.o: $(SM64UPSTREAM)/%.c \
		$(SM64SRCDIR)/sm64.make $(SM64SRCDIR)/sm64_compat.h
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) $(SM64FLAGS) \
		-include $(SM64SRCDIR)/sm64_compat.h -c $< -o $@

$(SM64OBJDIR)/%.o: $(SM64SRCDIR)/%.c $(SM64SRCDIR)/sm64.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) $(SM64FLAGS) \
		-c $< -o $@
