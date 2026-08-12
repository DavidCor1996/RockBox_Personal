/* Host conformance harness for the Rockbox Uxn interpreter. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uxn.h"

struct uxn_vm uxn;

static unsigned int passed;
static unsigned int failed;

uint8_t uxn_dei(uint8_t addr)
{
    return uxn.dev[addr];
}

void uxn_deo(uint8_t addr, uint8_t value)
{
    uxn.dev[addr] = value;
    if (addr == 0x18)
    {
        if (value == '1')
            passed++;
        else if (value == '0')
            failed++;
    }
}

int main(int argc, char **argv)
{
    FILE *rom;
    size_t length;
    int result;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s opctest.rom\n", argv[0]);
        return 2;
    }
    uxn.ram = calloc(UXN_RAM_SIZE, 1);
    if (!uxn.ram)
        return 2;
    rom = fopen(argv[1], "rb");
    if (!rom)
        return 2;
    length = fread(uxn.ram + UXN_PAGE_PROGRAM, 1,
                   UXN_RAM_SIZE - UXN_PAGE_PROGRAM, rom);
    fclose(rom);
    if (!length)
        return 2;

    result = uxn_eval(UXN_PAGE_PROGRAM);
    printf("Uxn opcode checks: %u passed, %u failed\n", passed, failed);
    free(uxn.ram);
    if (!result || failed || passed < 250)
        return 1;
    return 0;
}
