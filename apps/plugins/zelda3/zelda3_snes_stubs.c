/***************************************************************************
 * Zelda3 uses the SNES DMA register model for native video bookkeeping, but
 * never executes its emulated bus-transfer routines.  Keep those unreachable
 * references inert without linking the SNES CPU, cartridge, or APU emulator.
 ****************************************************************************/
#include "zelda3.h"
#define dsp_init zelda3_dsp_init
#define DIR ZELDA3_DSP_DIR
#include "upstream/snes/snes.h"

uint8_t snes_readBBus(Snes *snes, uint8_t address)
{
    (void)snes;
    (void)address;
    return 0;
}

void snes_writeBBus(Snes *snes, uint8_t address, uint8_t value)
{
    (void)snes;
    (void)address;
    (void)value;
}

uint8_t snes_read(Snes *snes, uint32_t address)
{
    (void)snes;
    (void)address;
    return 0;
}

void snes_write(Snes *snes, uint32_t address, uint8_t value)
{
    (void)snes;
    (void)address;
    (void)value;
}
