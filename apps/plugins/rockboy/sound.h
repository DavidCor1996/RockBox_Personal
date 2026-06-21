#ifndef __SOUND_H__
#define __SOUND_H__

#include "defs.h"

struct sndchan
{
    /* S1, S2, S3, S4 */
    int on, len, skip, cont, suppress_enable_clock;
    unsigned int pos;

    /* S1, S2, S4 */
    int enlen, envol, endir, enlenreload;
    
    /* S1, S2 */
    const byte *wave;
    
    /* S1 only */
    int swlen, swlenreload, swsteps, swstep, swdir;
    int swshadow, swenabled, swneg_used;

    /* S3 only */
    int outputlevel;

    /* S4 only */
    int shiftskip, shiftpos, shiftright, shiftleft;
    int nsteps, clock;

};

struct snd
{
    int level1, level2, balance;
    bool gbDigitalSound;
    int rate;
    int quality;
    int length_phase;
    int frame_step;
    int wave_timer;
    int wave_access;
    int wave_index;
    int wave_startup;
    struct sndchan ch[4];
};

extern struct snd snd;

#if defined(ICODE_ATTR) && defined(CPU_ARM)
#undef ICODE_ATTR
#define ICODE_ATTR
#endif

byte sound_read(byte r) ICODE_ATTR;
void sound_write(byte r, byte b) ICODE_ATTR;
void sound_dirty(void);
void sound_reset(void);
void sound_mix(void) ICODE_ATTR;
void sound_tick(int cnt) ICODE_ATTR;

#endif
