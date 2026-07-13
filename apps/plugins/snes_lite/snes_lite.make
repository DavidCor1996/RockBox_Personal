SNESLITESRCDIR := $(APPSDIR)/plugins/snes_lite
SNESLITEOBJDIR := $(BUILDDIR)/apps/plugins/snes_lite

SNESLITE_SRC := $(call preprocess, $(SNESLITESRCDIR)/SOURCES)
SNESLITE_OBJ := $(call c2obj, $(SNESLITE_SRC))
SNESLITE_FRONTEND_OBJ := $(addprefix $(SNESLITEOBJDIR)/, \
	snes_lite.o snes_lite_frontend.o snes_lite_video.o \
	snes_lite_audio.o snes_lite_input.o snes_lite_config.o \
	snes_lite_saves.o)
SNESLITE_CORE_HEADERS := $(wildcard $(SNESLITESRCDIR)/core/*.h)
SNESLITE_LIBRETRO_HEADERS := \
	$(wildcard $(SNESLITESRCDIR)/libretro/*.h) \
	$(wildcard $(SNESLITESRCDIR)/libretro/libretro-common/include/*.h) \
	$(wildcard $(SNESLITESRCDIR)/libretro/libretro-common/include/streams/*.h)

OTHER_SRC += $(SNESLITE_SRC)
ROCKS += $(SNESLITEOBJDIR)/snes_lite.rock

SNESLITEFLAGS = $(filter-out -O%,$(PLUGINFLAGS)) -O3 \
	-I$(SNESLITESRCDIR) \
	-I$(SNESLITESRCDIR)/core \
	-I$(SNESLITESRCDIR)/libretro \
	-I$(SNESLITESRCDIR)/libretro/libretro-common/include \
	-D__LIBRETRO__ -DHAVE_STDINT_H -DHAVE_INTTYPES_H -DHAVE_STRINGS_H \
	-DLSB_FIRST \
	-D__OLD_RASTER_FX__ -DUSE_SA1 \
	-fsigned-char -fno-strict-aliasing -fomit-frame-pointer -ffast-math \
	-Wno-unused-parameter -Wno-unused-function -Wno-sign-compare \
	-Wno-missing-prototypes -Wno-strict-prototypes -Wno-parentheses \
	-Wno-old-style-definition

ifeq ($(findstring sim,$(APP_TYPE)),)
SNESLITEFLAGS += -flto -fno-unroll-loops -DFAST_ALIGNED_LSB_WORD_ACCESS
$(SNESLITEOBJDIR)/snes_lite.rock: PLUGINFLAGS += -flto
endif

$(SNESLITEOBJDIR)/snes_lite.rock: $(SNESLITE_OBJ)

$(SNESLITE_FRONTEND_OBJ): $(SNESLITESRCDIR)/snes_lite.make

$(SNESLITEOBJDIR)/core/%.o: $(SNESLITESRCDIR)/core/%.c \
		$(SNESLITESRCDIR)/snes_lite.make \
		$(SNESLITESRCDIR)/snes_lite_core_compat.h \
		$(SNESLITE_CORE_HEADERS)
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) $(SNESLITEFLAGS) \
		-include $(SNESLITESRCDIR)/snes_lite_core_compat.h -c $< -o $@

$(SNESLITEOBJDIR)/libretro/%.o: $(SNESLITESRCDIR)/libretro/%.c \
		$(SNESLITESRCDIR)/snes_lite.make \
		$(SNESLITESRCDIR)/snes_lite_core_compat.h \
		$(SNESLITE_CORE_HEADERS) $(SNESLITE_LIBRETRO_HEADERS)
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) $(SNESLITEFLAGS) \
		-include $(SNESLITESRCDIR)/snes_lite_core_compat.h -c $< -o $@
