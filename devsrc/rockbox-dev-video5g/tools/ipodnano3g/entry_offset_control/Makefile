TARGET := entry-offset-control-n3g
CC := arm-elf-eabi-gcc
OBJCOPY := arm-elf-eabi-objcopy

CFLAGS := -mcpu=arm926ej-s -marm -ffreestanding -fno-builtin -nostdlib -nostartfiles
LDFLAGS := -nostdlib -nostartfiles -T entry-offset-control-n3g.lds -Wl,-Map,$(TARGET).map -Wl,--build-id=none

all: $(TARGET).bin $(TARGET).elf $(TARGET).map

$(TARGET).elf: entry-offset-control-n3g.S entry-offset-control-n3g.lds
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ entry-offset-control-n3g.S

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f $(TARGET).bin $(TARGET).elf $(TARGET).map

.PHONY: all clean
