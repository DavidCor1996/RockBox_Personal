TARGET ?= minimal-n3g
SRC ?= $(TARGET).S
LDSCRIPT ?= minimal-n3g.lds
CPPFLAGS ?=
CC := arm-elf-eabi-gcc
OBJCOPY := arm-elf-eabi-objcopy
OBJDUMP := arm-elf-eabi-objdump
READELF := arm-elf-eabi-readelf
SIZE := arm-elf-eabi-size

CFLAGS := -mcpu=arm926ej-s -marm -ffreestanding -fno-builtin -nostdlib -nostartfiles
LDFLAGS := -nostdlib -nostartfiles -T $(LDSCRIPT) -Wl,-Map,$(TARGET).map -Wl,--build-id=none

all: $(TARGET).bin $(TARGET).elf $(TARGET).map

$(TARGET).elf: $(SRC) $(LDSCRIPT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(SRC)

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f $(TARGET).bin $(TARGET).elf $(TARGET).map

.PHONY: all clean
