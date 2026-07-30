DIABLO_SRCDIR := $(APPSDIR)/plugins/diablo
DIABLO_BUILDDIR := $(BUILDDIR)/apps/plugins/diablo

DIABLO_SRC := $(call preprocess, $(DIABLO_SRCDIR)/SOURCES)
DIABLO_OBJ := $(call c2obj, $(DIABLO_SRC))

OTHER_SRC += $(DIABLO_SRC)

ROCKS += $(DIABLO_BUILDDIR)/diablo.rock

$(DIABLO_BUILDDIR)/diablo.rock: $(DIABLO_OBJ)
