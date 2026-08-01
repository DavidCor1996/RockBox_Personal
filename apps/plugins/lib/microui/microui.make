# microUI is compiled into Rockbox's existing plugin library through
# apps/plugins/lib/SOURCES. This fragment records the library source set for
# consumers and tooling without creating a second archive or firmware ABI.
MICROUILIBSRCDIR := $(APPSDIR)/plugins/lib/microui
MICROUILIBSRC := $(MICROUILIBSRCDIR)/microui.c \
                 $(MICROUILIBSRCDIR)/microui_rockbox.c
