CC := arm-elf-eabi-gcc
OBJCOPY := arm-elf-eabi-objcopy

CFLAGS := -mcpu=arm926ej-s -marm -ffreestanding -fno-builtin -nostdlib -nostartfiles
LDFLAGS = -nostdlib -nostartfiles -T observability-n3g.lds -Wl,-Map,$*.map -Wl,--build-id=none

TARGETS := immediate-loop-n3g delayed-loop-n3g ram-mutate-loop-n3g

all: $(TARGETS:%=%.bin) $(TARGETS:%=%.elf) $(TARGETS:%=%.map)

%.elf: %.S observability-n3g.lds
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f $(TARGETS:%=%.bin) $(TARGETS:%=%.elf) $(TARGETS:%=%.map)

.PHONY: all clean
