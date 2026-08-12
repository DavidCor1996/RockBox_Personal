#include "devices.h"

/*
 * Device semantics are adapted from the official Uxn C emulator at commit
 * 156ef01226c069ad5930c2655a10f22380a387a8. See LICENSE.uxn.
 */

static uint16_t controller_vector;
static uint16_t mouse_vector;
static char console_line[128];
static int console_length;

static void system_expansion(uint16_t address)
{
    uint8_t *command = uxn.ram + address;
    uint16_t length = PEEK2(command + 1);
    unsigned int source_bank = PEEK2(command + 3) * UXN_PAGE_SIZE;
    uint16_t source_addr = PEEK2(command + 5);
    unsigned int i;

    if (source_bank >= UXN_RAM_SIZE)
        return;
    switch (command[0])
    {
    case 0:
        for (i = 0; i < length; i++)
            uxn.ram[source_bank + (uint16_t)(source_addr + i)] = command[7];
        break;
    case 1:
    case 2:
    {
        unsigned int dest_bank = PEEK2(command + 7) * UXN_PAGE_SIZE;
        uint16_t dest_addr = PEEK2(command + 9);

        if (dest_bank >= UXN_RAM_SIZE)
            return;
        if (command[0] == 1)
        {
            for (i = 0; i < length; i++)
                uxn.ram[dest_bank + (uint16_t)(dest_addr + i)] =
                    uxn.ram[source_bank + (uint16_t)(source_addr + i)];
        }
        else
        {
            for (i = length; i > 0; i--)
                uxn.ram[dest_bank + (uint16_t)(dest_addr + i - 1)] =
                    uxn.ram[source_bank + (uint16_t)(source_addr + i - 1)];
        }
        break;
    }
    }
}

uint8_t system_dei(uint8_t addr)
{
    if (addr == 0x04)
        return uxn.wst.ptr;
    if (addr == 0x05)
        return uxn.rst.ptr;
    return uxn.dev[addr];
}

void system_deo(uint8_t addr)
{
    switch (addr)
    {
    case 0x03:
        system_expansion(PEEK2(&uxn.dev[0x02]));
        break;
    case 0x04:
        uxn.wst.ptr = uxn.dev[0x04];
        break;
    case 0x05:
        uxn.rst.ptr = uxn.dev[0x05];
        break;
    case 0x0e:
#ifdef ROCKBOX_HAS_LOGF
        rb->logf("Uxn WST %02x RST %02x", uxn.wst.ptr, uxn.rst.ptr);
#elif defined(DEBUG) || defined(SIMULATOR)
        rb->debugf("Uxn WST %02x RST %02x", uxn.wst.ptr, uxn.rst.ptr);
#endif
        break;
    }
}

static void console_flush(void)
{
    if (!console_length)
        return;
    console_line[console_length] = '\0';
#ifdef ROCKBOX_HAS_LOGF
    rb->logf("Uxn: %s", console_line);
#elif defined(DEBUG) || defined(SIMULATOR)
    rb->debugf("Uxn: %s", console_line);
#endif
    console_length = 0;
}

void console_deo(uint8_t addr)
{
    uint8_t value;

    if (addr != 0x18 && addr != 0x19)
        return;
    value = uxn.dev[addr];
    if (value == '\n' || console_length >= (int)sizeof(console_line) - 1)
        console_flush();
    else if (value >= 0x20 && value < 0x7f)
        console_line[console_length++] = value;
}

void controller_deo(uint8_t addr)
{
    if (addr == 0x81)
        controller_vector = PEEK2(&uxn.dev[0x80]);
}

void controller_down(uint8_t mask)
{
    if (!mask)
        return;
    uxn.dev[0x82] |= mask;
    uxn_eval(controller_vector);
}

void controller_up(uint8_t mask)
{
    if (!mask)
        return;
    uxn.dev[0x82] &= ~mask;
    uxn_eval(controller_vector);
}

void controller_key(uint8_t key)
{
    if (!key)
        return;
    uxn.dev[0x83] = key;
    uxn_eval(controller_vector);
    uxn.dev[0x83] = 0;
}

void mouse_deo(uint8_t addr)
{
    if (addr == 0x91)
        mouse_vector = PEEK2(&uxn.dev[0x90]);
}

void mouse_down(uint8_t mask)
{
    uxn.dev[0x96] |= mask;
    uxn_eval(mouse_vector);
}

void mouse_up(uint8_t mask)
{
    uxn.dev[0x96] &= ~mask;
    uxn_eval(mouse_vector);
}

void mouse_pos(uint16_t x, uint16_t y)
{
    POKE2(&uxn.dev[0x92], x);
    POKE2(&uxn.dev[0x94], y);
    uxn_eval(mouse_vector);
}

void mouse_scroll(int16_t x, int16_t y)
{
    POKE2(&uxn.dev[0x9a], x);
    POKE2(&uxn.dev[0x9c], -y);
    uxn_eval(mouse_vector);
    POKE2(&uxn.dev[0x9a], 0);
    POKE2(&uxn.dev[0x9c], 0);
}

uint8_t datetime_dei(uint8_t addr)
{
    struct tm *time = rb->get_time();

    switch (addr)
    {
    case 0xc0: return (time->tm_year + 1900) >> 8;
    case 0xc1: return time->tm_year + 1900;
    case 0xc2: return time->tm_mon;
    case 0xc3: return time->tm_mday;
    case 0xc4: return time->tm_hour;
    case 0xc5: return time->tm_min;
    case 0xc6: return time->tm_sec;
    case 0xc7: return time->tm_wday;
    case 0xc8: return time->tm_yday >> 8;
    case 0xc9: return time->tm_yday;
    case 0xca: return time->tm_isdst;
    default: return uxn.dev[addr];
    }
}
