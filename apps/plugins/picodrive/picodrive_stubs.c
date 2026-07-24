#include "picodrive.h"
#include "upstream/pico/pico_int.h"
#include "upstream/pico/memory.h"
#include "upstream/pico/cd/megasd.h"
#include <stdarg.h>

unsigned char media_id_header[0x100];
#ifdef EMU_F68K
M68K_CONTEXT PicoCpuFS68k;
#elif defined(EMU_C68K)
struct Cyclone PicoCpuCS68k;
#endif
uptr s68k_read8_map[0x1000000 >> M68K_MEM_SHIFT];
uptr s68k_read16_map[0x1000000 >> M68K_MEM_SHIFT];
uptr s68k_write8_map[0x1000000 >> M68K_MEM_SHIFT];
uptr s68k_write16_map[0x1000000 >> M68K_MEM_SHIFT];
mcd_state *Pico_mcd;
struct megasd Pico_msd;
unsigned int p32x_event_times[5];
unsigned int pcd_event_times[PCD_EVENT_COUNT];
unsigned int SekCycleCntS68k;
unsigned int SekCycleAimS68k;
u32 pcd_base_address;
picohw_state PicoPicohw;

void lprintf(const char *format, ...)
{
    va_list arguments;
    char line[192];

    va_start(arguments, format);
    rb->vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    pd_log("core: %s", line);
}

void PicoInitMCD(void) { }
void PicoExitMCD(void) { }
void PicoPowerMCD(void) { }
int PicoResetMCD(void) { return 0; }
void PicoFrameMCD(void) { }
void PicoMCDPrepare(void) { }
void PicoMemSetupCD(void) { PicoMemSetup(); }
void Pico32xPrepare(void) { }
void PicoPrepareMS(void) { }
void PicoDrawSetOutFormat32x(pdso_t format, int line_mode)
{
    (void)format;
    (void)line_mode;
}
void PicoDrawSetOutBuf32X(void *destination, int increment)
{
    (void)destination;
    (void)increment;
}
int (*PicoScan32xBegin)(unsigned int line);
int (*PicoScan32xEnd)(unsigned int line);
#ifndef _ASM_MEMORY_C
u32 PicoRead8_32x(u32 address) { (void)address; return 0; }
u32 PicoRead16_32x(u32 address) { (void)address; return 0; }
void PicoWrite8_32x(u32 address, u32 value)
{
    (void)address;
    (void)value;
}
void PicoWrite16_32x(u32 address, u32 value)
{
    (void)address;
    (void)value;
}
#endif

void PicoSVPInit(void) { }
void PicoSVPStartup(void) { PicoIn.AHW &= ~PAHW_SVP; }

void vgm_finish(void) { }
void vgm_reset(void) { }
void vgm_frame(void) { }

void pdb_cleanup(void) { }

void PicoInitPico(void) { }
void PicoMemSetupPico(void) { }
void PicoReratePico(void) { }
void PicoPicoPCMUpdate(short *buffer, int length, int stereo)
{
    (void)buffer;
    (void)length;
    (void)stereo;
}
int PicoPicoIrqAck(int level) { (void)level; return 0; }
int PicoPicoPCMSave(void *buffer, int length)
{
    (void)buffer;
    (void)length;
    return 0;
}
void PicoPicoPCMLoad(void *buffer, int length)
{
    (void)buffer;
    (void)length;
}

void PicoDoHighPal555SMS(void) { }
void PicoDrawSetOutputSMS(pdso_t format) { (void)format; }

int gfx_context_save(unsigned char *state) { (void)state; return 0; }
int gfx_context_load(const unsigned char *state) { (void)state; return 0; }
int cdc_context_save(unsigned char *state) { (void)state; return 0; }
int cdc_context_load(unsigned char *state) { (void)state; return 0; }
int cdc_context_load_old(unsigned char *state) { (void)state; return 0; }
int cdd_context_save(unsigned char *state) { (void)state; return 0; }
int cdd_context_load(unsigned char *state) { (void)state; return 0; }
int cdd_context_load_old(unsigned char *state) { (void)state; return 0; }
void pcd_state_loaded(void) { }
void wram_1M_to_2M(unsigned char *memory) { (void)memory; }
void wram_2M_to_1M(unsigned char *memory) { (void)memory; }
void DmaSlowCell(u32 source, u32 address, int length, unsigned char increment)
{
    (void)source;
    (void)address;
    (void)length;
    (void)increment;
}

void pcd_pcm_update(s32 *buffer, int length, int stereo)
{
    (void)buffer;
    (void)length;
    (void)stereo;
}

int mp3_get_bitrate(void *file, int size)
{
    (void)file;
    (void)size;
    return 0;
}
void mp3_start_play(void *file, int position)
{
    (void)file;
    (void)position;
}
void mp3_update(s32 *buffer, int length, int stereo)
{
    (void)buffer;
    (void)length;
    (void)stereo;
}
int ogg_get_length(void *file) { (void)file; return 0; }
void ogg_start_play(void *file, int sample_offset)
{
    (void)file;
    (void)sample_offset;
}
void ogg_stop_play(void) { }
void ogg_update(s32 *buffer, int length, int stereo)
{
    (void)buffer;
    (void)length;
    (void)stereo;
}

void *movie_data;
