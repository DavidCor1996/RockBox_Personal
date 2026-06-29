#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/                \/
#

MINIVMACSRCDIR := $(APPSDIR)/plugins/minivmac
MINIVMACBUILDDIR := $(BUILDDIR)/apps/plugins/minivmac

ROCKS += $(MINIVMACBUILDDIR)/minivmac.rock

MINIVMAC_SRC := $(call preprocess, $(MINIVMACSRCDIR)/SOURCES)
MINIVMAC_OBJ := $(call c2obj, $(MINIVMAC_SRC))

OTHER_SRC += $(MINIVMAC_SRC)
OTHER_INC += -I$(MINIVMACSRCDIR)/cfg -I$(MINIVMACSRCDIR)/src

MINIVMACFLAGS = $(filter-out -O%,$(PLUGINFLAGS)) -O2 \
	-Wno-unused-parameter -Wno-unused-function -Wno-missing-prototypes \
	-Wno-strict-prototypes -I$(MINIVMACSRCDIR)/cfg \
	-I$(MINIVMACSRCDIR)/src

$(MINIVMACBUILDDIR)/minivmac.rock: $(MINIVMAC_OBJ)

$(MINIVMACBUILDDIR)/%.o: $(MINIVMACSRCDIR)/%.c $(MINIVMACSRCDIR)/minivmac.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) $(MINIVMACFLAGS) -c $< -o $@
