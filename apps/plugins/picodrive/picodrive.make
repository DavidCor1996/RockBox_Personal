PICODRIVESRCDIR := $(APPSDIR)/plugins/picodrive
PICODRIVEOBJDIR := $(BUILDDIR)/apps/plugins/picodrive

ifeq ($(PICODRIVE_NONCOMMERCIAL),1)

PICODRIVE_SRC := $(call preprocess, $(PICODRIVESRCDIR)/SOURCES)
PICODRIVE_OBJ := $(call c2obj, $(PICODRIVE_SRC))
PICODRIVE_FRONTEND_OBJ := \
	$(PICODRIVEOBJDIR)/picodrive.o \
	$(PICODRIVEOBJDIR)/picodrive_platform.o \
	$(PICODRIVEOBJDIR)/picodrive_stubs.o \
	$(PICODRIVEOBJDIR)/picodrive_compression_stubs.o

OTHER_SRC += $(PICODRIVE_SRC)
ROCKS += $(PICODRIVEOBJDIR)/picodrive.rock

PICODRIVEFLAGS = $(filter-out -O%,$(PLUGINFLAGS)) -O2 \
	-I$(PICODRIVESRCDIR) \
	-I$(PICODRIVESRCDIR)/upstream \
	-I$(PICODRIVESRCDIR)/upstream/pico \
	-I$(PICODRIVESRCDIR)/upstream/cpu \
	-DPICODRIVE_CART_ONLY -DNO_32X -DNO_SMS \
	-DNDEBUG \
	-ffunction-sections -fdata-sections -fomit-frame-pointer \
	-fno-strict-aliasing -Wno-unused-parameter -Wno-unused-function \
	-Wno-sign-compare -Wno-missing-prototypes -Wno-strict-prototypes \
	-Wno-old-style-definition -Wno-pointer-to-int-cast \
	-Wno-int-to-pointer-cast

PICODRIVEFLAGS += -DEMU_F68K -D_USE_CZ80
ifneq ($(findstring sim,$(APP_TYPE)),sim)
# The initial ARM assembly combination reaches cartridge boot but is not yet
# stable on physical iPod 6G hardware. Keep the portable cores as the safe
# default until each optimized path passes the hardware acceptance matrix.
PICODRIVEFLAGS += -fno-unroll-loops
endif

$(PICODRIVEOBJDIR)/picodrive.rock: $(PICODRIVE_OBJ)

$(PICODRIVE_FRONTEND_OBJ): $(PICODRIVEOBJDIR)/%.o: $(PICODRIVESRCDIR)/%.c \
		$(PICODRIVESRCDIR)/picodrive.make \
		$(PICODRIVESRCDIR)/picodrive_core_compat.h
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) \
		$(PICODRIVEFLAGS) -c $< -o $@

$(PICODRIVEOBJDIR)/upstream/%.o: $(PICODRIVESRCDIR)/upstream/%.c \
		$(PICODRIVESRCDIR)/picodrive.make \
		$(PICODRIVESRCDIR)/picodrive_core_compat.h
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) \
		$(PICODRIVEFLAGS) \
		-include $(PICODRIVESRCDIR)/picodrive_core_compat.h \
		-c $< -o $@

$(PICODRIVEOBJDIR)/%.o: $(PICODRIVESRCDIR)/%.s \
		$(PICODRIVESRCDIR)/picodrive.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,AS $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) \
		$(PICODRIVEFLAGS) -c $< -o $@

$(PICODRIVEOBJDIR)/%.o: $(PICODRIVESRCDIR)/%.S \
		$(PICODRIVESRCDIR)/picodrive.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,AS $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) \
		$(PICODRIVEFLAGS) -c $< -o $@

endif
