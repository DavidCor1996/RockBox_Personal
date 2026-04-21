SHELL := /bin/bash

ifndef GBDK_HOME
ifneq ("$(wildcard /tmp/gbdk-4.5.0/gbdk/bin/lcc)","")
	GBDK_HOME := /tmp/gbdk-4.5.0/gbdk
else
	GBDK_HOME := ../../../
endif
endif

LCC = $(GBDK_HOME)/bin/lcc

PROJECTNAME = dialtone
TARGETS = gb

LCCFLAGS_gb = -Wm-ys -Wl-yt0x1B -Wm-yn"DIALTONE" -autobank
LCCFLAGS += $(LCCFLAGS_$(EXT))
LCCFLAGS += -Wl-j
CFLAGS += -Wf-MMD -Wf-Wp-MP

ifdef GBDK_DEBUG
	LCCFLAGS += -debug -v
endif

SRCDIR = src
RESDIR = res
OBJDIR = obj/$(EXT)
BINDIR = build/$(EXT)
MKDIRS = $(OBJDIR) $(BINDIR)

CSOURCES = main.c audio.c music_menu.c assets.c world_data.c startup_scott_stinks_a.c startup_bimpson_productions_a.c startup_dialtone_title_a.c startup_dialtone_title_b.c luma_lane_district_v2.c luma_lane_apartment.c needle_and_neon_records.c rook_repair_tech.c soma_ramen.c iona_rooftop.c southline_overpass.c
ASMSOURCES = $(foreach dir,$(SRCDIR),$(notdir $(wildcard $(dir)/*.s)))
OBJS = $(CSOURCES:%.c=$(OBJDIR)/%.o) $(ASMSOURCES:%.s=$(OBJDIR)/%.o)
DEPS = $(OBJS:%.o=%.d)

-include $(DEPS)

all: $(TARGETS)

compile.bat: Makefile
	@echo "REM Automatically generated from Makefile" > compile.bat
	@make -sn | sed y/\\//\\\\/ | grep -v make >> compile.bat

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(LCC) $(LCCFLAGS) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: $(RESDIR)/%.c
	$(LCC) $(LCCFLAGS) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: $(SRCDIR)/%.s
	$(LCC) $(LCCFLAGS) $(CFLAGS) -c -o $@ $<

$(BINDIR)/$(PROJECTNAME).$(EXT): $(OBJS)
	$(LCC) $(LCCFLAGS) $(CFLAGS) -o $@ $(OBJS)

clean:
	@echo Cleaning
	@for target in $(TARGETS); do \
		$(MAKE) $$target-clean; \
	done

include Makefile.targets

ifneq ($(strip $(EXT)),)
$(info $(shell mkdir -p $(MKDIRS)))
endif
