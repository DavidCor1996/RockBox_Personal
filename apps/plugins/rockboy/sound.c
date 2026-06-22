/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * The following code is rewrite of the C++ code provided in VisualBoyAdvance.
 * There are also portions of the original GNUboy code.
 * Copyright (C) 2001 the GNUboy development team
 *
 * VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
 * Copyright (C) 1999-2003 Forgotten
 * Copyright (C) 2004 Forgotten and the VBA development team
 *
 * VisualBoyAdvance conversion from C++ to C by Karl Kurbjun July, 2007
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 */

#include "rockmacros.h"
#include "defs.h"
#include "pcm.h"
#include "sound.h"
#include "cpu-gb.h"
#include "hw.h"
#include "regs.h"

static const byte soundWavePattern[4][32] = {
  {0x01,0x01,0x01,0x01,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff},
  {0x01,0x01,0x01,0x01,
   0x01,0x01,0x01,0x01,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff},
  {0x01,0x01,0x01,0x01,
   0x01,0x01,0x01,0x01,
   0x01,0x01,0x01,0x01,
   0x01,0x01,0x01,0x01,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff},
  {0x01,0x01,0x01,0x01,
   0x01,0x01,0x01,0x01,
   0x01,0x01,0x01,0x01,
   0x01,0x01,0x01,0x01,
   0x01,0x01,0x01,0x01,
   0x01,0x01,0x01,0x01,
   0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff}
};

int soundFreqRatio[8] ICONST_ATTR= {
  1048576, // 0
  524288,  // 1
  262144,  // 2
  174763,  // 3
  131072,  // 4
  104858,  // 5
  87381,   // 6
  74898    // 7
};

int soundShiftClock[16] ICONST_ATTR= {
      2, // 0
      4, // 1
      8, // 2
     16, // 3
     32, // 4
     64, // 5
    128, // 6
    256, // 7
    512, // 8
   1024, // 9
   2048, // 10
   4096, // 11
   8192, // 12
  16384, // 13
  1,     // 14
  1      // 15
};

struct snd snd IBSS_ATTR;

#define RATE (snd.rate)
#define WAVE (ram.hi+0x30)
#define S1 (snd.ch[0])
#define S2 (snd.ch[1])
#define S3 (snd.ch[2])
#define S4 (snd.ch[3])

#define SOUND_MAGIC   0x60000000
#define SOUND_MAGIC_2 0x30000000
#define NOISE_MAGIC 5

static const byte sound_read_mask[0x30] = {
    0x80, 0x3f, 0x00, 0xff, 0xbf, 0xff, 0x3f, 0x00,
    0xff, 0xbf, 0x7f, 0xff, 0x9f, 0xff, 0xbf, 0xff,
    0xff, 0x00, 0x00, 0xbf, 0x00, 0x00, 0x70, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

#define SOUND_LENGTH_UNIT 1
#define SOUND_LENGTH_CLOCK 4096

static void sound_power_off(void)
{
    int r;
    byte nr41 = R_NR41;
    int s1_len = S1.len;
    int s2_len = S2.len;
    int s3_len = S3.len;
    int s4_len = S4.len;

    S1.on = S2.on = S3.on = S4.on = 0;
    S1.cont = S2.cont = S3.cont = S4.cont = 0;
    snd.length_phase = 0;
    snd.frame_step = 0;
    snd.wave_access = 0;
    snd.wave_startup = 0;

    for (r = RI_NR10; r <= RI_NR51; r++)
        ram.hi[r] = 0;

    if (!hw.cgb)
    {
        R_NR41 = nr41;
        S1.len = s1_len;
        S2.len = s2_len;
        S3.len = s3_len;
        S4.len = s4_len;
    }
    else
    {
        S1.len = S2.len = S3.len = S4.len = 0;
        S1.suppress_enable_clock =
            S2.suppress_enable_clock =
            S3.suppress_enable_clock =
            S4.suppress_enable_clock = 0;
    }

    R_NR52 = 0;
}

static bool sound_env_dac_enabled(byte b)
{
    return b & 0xf8;
}

static bool sound_length_extra_clock_phase(void)
{
    return snd.frame_step & 1;
}

static void sound_clock_length_once(struct sndchan *ch, byte clear_mask)
{
    if (ch->len <= 0)
        return;

    ch->len -= SOUND_LENGTH_UNIT;
    if (ch->len <= 0)
    {
        ch->len = 0;
        ch->on = 0;
        R_NR52 &= clear_mask;
    }
}

static void sound_clock_lengths(void)
{
    if (S1.cont)
        sound_clock_length_once(&S1, 0xfe);
    if (S2.cont)
        sound_clock_length_once(&S2, 0xfd);
    if (S3.cont)
        sound_clock_length_once(&S3, 0xfb);
    if (S4.cont)
        sound_clock_length_once(&S4, 0xf7);
}

static void sound_apply_length_enable_write(struct sndchan *ch, byte old,
                                            byte b, byte clear_mask)
{
    if (!(old & 0x40) && (b & 0x40))
    {
        if (!ch->suppress_enable_clock && sound_length_extra_clock_phase()
                && (!(b & 0x80) || ch->len <= SOUND_LENGTH_UNIT))
            sound_clock_length_once(ch, clear_mask);
        ch->suppress_enable_clock = 0;
    }
}

static bool sound_trigger_extra_clock_allowed(byte old, byte b)
{
    return (!(old & 0x40) && (b & 0x40)) || !(old & 0x80);
}

static int sound_wave_period(void)
{
    int freq = 2048 - (((int)(R_NR34 & 7) << 8) | R_NR33);

    return freq;
}

static void sound_wave_restart_timer(void)
{
    snd.wave_timer = sound_wave_period() + (hw.cgb ? 3 : 1);
    snd.wave_access = 0;
    snd.wave_index = 0;
    snd.wave_startup = 1;
}

static int sound_wave_access_offset(void)
{
    if (!snd.wave_access || (snd.wave_startup && snd.wave_index == 1))
        return -1;

    return ((snd.wave_index - 1) >> 1) & 0x0f;
}

static int sound_wave_current_offset_cgb(void)
{
    return (snd.wave_index >> 1) & 0x0f;
}

static int sound_wave_corruption_offset(void)
{
    if (snd.wave_timer != 1 || (snd.wave_startup && snd.wave_index == 1))
        return -1;

    return (snd.wave_index >> 1) & 0x0f;
}

static void sound_wave_retrigger_dmg(void)
{
    int offset;
    int src;

    if (hw.cgb || !S3.on || !(R_NR30 & 0x80))
        return;

    offset = sound_wave_corruption_offset();
    if (offset < 0)
        return;

    if (offset < 4)
    {
        ram.hi[0x30] = ram.hi[0x30 + offset];
        return;
    }

    src = 0x30 + (offset & 0x0c);
    ram.hi[0x30] = ram.hi[src];
    ram.hi[0x31] = ram.hi[src + 1];
    ram.hi[0x32] = ram.hi[src + 2];
    ram.hi[0x33] = ram.hi[src + 3];
}

static void sound_wave_tick(int cnt)
{
    int period;

    if (!S3.on || !(R_NR30 & 0x80))
    {
        snd.wave_access = 0;
        return;
    }

    period = sound_wave_period();
    if (!period)
    {
        snd.wave_access = 0;
        return;
    }

    while (cnt >= snd.wave_timer)
    {
        cnt -= snd.wave_timer;
        snd.wave_timer = period;
        snd.wave_index = (snd.wave_index + 1) & 0x1f;
        if (snd.wave_startup && snd.wave_index != 1)
            snd.wave_startup = 0;
        snd.wave_access = 1;
    }

    snd.wave_timer -= cnt;
    if (snd.wave_access > cnt)
        snd.wave_access -= cnt;
    else
        snd.wave_access = 0;
}

static void sound_sweep_set_freq(int freq)
{
    S1.skip = SOUND_MAGIC / (2048 - freq);
    R_NR13 = freq & 0xff;
    R_NR14 = (R_NR14 & 0xf8) | ((freq >> 8) & 7);
}

static int sound_sweep_calculate(void)
{
    int shift = R_NR10 & 7;
    int delta;

    if (shift)
        delta = S1.swshadow / (int)BIT_N(shift);
    else
        delta = S1.swshadow;
    if (R_NR10 & 0x08)
    {
        S1.swneg_used = 1;
        return S1.swshadow - delta;
    }
    return S1.swshadow + delta;
}

static bool sound_sweep_check_overflow(void)
{
    if (sound_sweep_calculate() <= 2047)
        return false;

    S1.swlen = 0;
    S1.on = 0;
    R_NR52 &= 0xfe;
    return true;
}

static void sound_sweep_clock(void)
{
    int period;
    int newfreq;

    if (!S1.swlen)
        return;

    S1.swlen--;
    if (S1.swlen)
        return;

    period = (R_NR10 >> 4) & 7;
    S1.swlen = period ? period : 8;

    if (!S1.swenabled || !period)
        return;

    newfreq = sound_sweep_calculate();
    if (newfreq > 2047)
    {
        sound_sweep_check_overflow();
        return;
    }

    if (R_NR10 & 7)
    {
        S1.swshadow = newfreq;
        sound_sweep_set_freq(newfreq);
        sound_sweep_check_overflow();
    }
}

static void sound_advance_frame_phase(int quality)
{
    snd.length_phase += quality;
    while (snd.length_phase >= SOUND_LENGTH_CLOCK)
    {
        snd.length_phase -= SOUND_LENGTH_CLOCK;
        if (!(snd.frame_step & 1))
        {
            sound_clock_lengths();
            if (snd.frame_step & 2)
                sound_sweep_clock();
        }
        snd.frame_step = (snd.frame_step + 1) & 7;
    }
}

static void gbSoundChannel1(int *r, int *l, int balance, int quality)
{
    int vol = S1.envol;

    int value = 0;

    if(!S1.on)
        return;

    if(S1.len || !S1.cont)
    {
        S1.pos += quality*S1.skip;
        S1.pos &= 0x1fffffff;

        value = ((signed char)S1.wave[S1.pos>>24]) * vol;
    }

    if (balance & 1) *r += value;
    if (balance & 16) *l += value;

    if(S1.enlen)
    {
        S1.enlen-=quality;

        if(S1.enlen<=0) 
        {
            if(S1.endir)
            {
                if(S1.envol < 15)
                    S1.envol++;
            }
            else 
            {
                if(S1.envol)
                    S1.envol--;
            }

            S1.enlen += S1.enlenreload;
        }
    }

}

static void gbSoundChannel2(int *r, int *l, int balance, int quality)
{
    int vol = S2.envol;
  
    int value = 0;

    if(!S2.on)
        return;
    
    if(S2.len || !S2.cont)
    {
        S2.pos += quality*S2.skip;
        S2.pos &= 0x1fffffff;
    
        value = ((signed char)S2.wave[S2.pos>>24]) * vol;
    }

    if (balance & 2) *r += value;
    if (balance & 32) *l += value;

    if(S2.enlen) {
        S2.enlen-=quality;
      
        if(S2.enlen <= 0) {
            if(S2.endir) {
                if(S2.envol < 15)
                    S2.envol++;
            } else {
                if(S2.envol)
                    S2.envol--;
            }
            S2.enlen += S2.enlenreload;
        }
    }
}

static void gbSoundChannel3(int *r, int *l, int balance, int quality)
{
    int s;

    if(!S3.on)
        return;

    if (S3.len || !S3.cont)
    {
        S3.pos += S3.skip*quality;
        S3.pos &= 0x1fffffff;
        s=ram.hi[0x30 + (S3.pos>>25)];
        if (S3.pos & 0x01000000)
            s &= 0x0f;
        else
            s >>= 4;

        s -= 8;

        switch(S3.outputlevel)
        {
            case 0:
                s=0;
                break;
            case 1:
                break;
            case 2:
                s=s>>1;
                break;
            case 3:
                s=s>>2;
                break;
        }

        if (balance & 4) *r += s;
        if (balance & 64) *l += s;
    }

}

static void gbSoundChannel4(int *r, int *l, int balance, int quality)
{
    int vol = S4.envol;
  
    int value = 0;

    if(!S4.on)
        return;
  
    if(S4.clock <= 0x0c)
    {
        if(S4.len || !S4.cont)
        {
            S4.pos += quality*S4.skip;
            S4.shiftpos += quality*S4.shiftskip;
      
            if(S4.nsteps)
            {
                while(S4.shiftpos > 0x1fffff) {
                    S4.shiftright = (((S4.shiftright << 6) ^
                        (S4.shiftright << 5)) & 0x40) | (S4.shiftright >> 1);
                    S4.shiftpos -= 0x200000;
                }
            } 
            else 
            {
                while(S4.shiftpos > 0x1fffff)
                {
                    S4.shiftright = (((S4.shiftright << 14) ^
                        (S4.shiftright << 13)) & 0x4000) | (S4.shiftright >> 1);
                    S4.shiftpos -= 0x200000;
                }
            }
      
            S4.pos &= 0x1fffff;
            S4.shiftpos &= 0x1fffff;
          
            value = ((S4.shiftright & 1)*2-1) * vol;
        } 
        else
        {
            value = 0;
        }
    }
  
    if (balance & 8) *r += value;
    if (balance & 128) *l += value;
  
    if(S4.enlen) {
        S4.enlen-=quality;
        
        if(S4.enlen <= 0)
        {
            if(S4.endir)
            {
                if(S4.envol < 15)
                    S4.envol++;
            } 
            else 
            {
                if(S4.envol)
                    S4.envol--;
            }
            S4.enlen += S4.enlenreload;
        }
    }
}

void sound_mix(void)
{
    int l, r;
    int balance;
    int quality;
    int left_gain;
    int right_gain;
    int digital_left;
    int digital_right;
    bool digital;

    if (!RATE || cpu.snd < RATE) return;

    balance = snd.balance;
    quality = snd.quality;
    left_gain = snd.level1 * 60;
    right_gain = snd.level2 * 60;
    digital_left = snd.level1 << 8;
    digital_right = snd.level2 << 8;
    digital = snd.gbDigitalSound;

    for (; cpu.snd >= RATE; cpu.snd -= RATE)
    {
        l = r = 0;

        gbSoundChannel1(&r, &l, balance, quality);

        gbSoundChannel2(&r, &l, balance, quality);

        gbSoundChannel3(&r, &l, balance, quality);

        gbSoundChannel4(&r, &l, balance, quality);

        if(digital)
        {
            l = digital_left;
            r = digital_right;
        }
        else
        {
            l *= left_gain;
            r *= right_gain;
        }

        if(l > 32767)
            l = 32767;
        if(l < -32768)
            l = -32768;
            
        if(r > 32767)
            r = 32767;
        if(r < -32768)
            r = -32768;

        if (pcm.buf)
        {
            if (pcm.pos >= pcm.len)
                rockboy_pcm_submit();
            if (pcm.stereo)
            {
                pcm.buf[pcm.pos++] = l;
                pcm.buf[pcm.pos++] = r;
            }
            else pcm.buf[pcm.pos++] = ((l+r)>>1);
        }
    }
    R_NR52 = (R_NR52&0xf0) | S1.on | (S2.on<<1) | (S3.on<<2) | (S4.on<<3);
}

void sound_tick(int cnt)
{
    cpu.snd += cnt;
    if (R_NR52 & 0x80)
        sound_advance_frame_phase(cnt);
    sound_wave_tick(cnt);
}

byte sound_read(byte r)
{
    int offset;

    if(!options.sound) return 0;
    sound_mix();
    /* printf("read %02X: %02X\n", r, REG(r)); */
    if (r >= 0x30 && r <= 0x3f && hw.cgb && S3.on && (R_NR30 & 0x80))
        return ram.hi[0x30 + sound_wave_current_offset_cgb()];
    if (r >= 0x30 && r <= 0x3f && !hw.cgb && S3.on && (R_NR30 & 0x80))
    {
        offset = sound_wave_access_offset();
        if (offset < 0)
            return 0xff;
        return ram.hi[0x30 + offset];
    }
    if (r >= RI_NR10 && r <= 0x3f)
        return REG(r) | sound_read_mask[r - RI_NR10];
    return REG(r);
}

void sound_write(byte r, byte b)
{
    int freq=0;
    byte old;
    bool reloaded_length;

    if(!options.sound)
        return;

    sound_mix();

    if (r >= 0x30 && r <= 0x3f)
    {
        if (hw.cgb && S3.on && (R_NR30 & 0x80))
        {
            ram.hi[0x30 + sound_wave_current_offset_cgb()] = b;
            return;
        }
        if (!hw.cgb && S3.on && (R_NR30 & 0x80))
        {
            int offset = sound_wave_access_offset();
            if (offset >= 0)
                ram.hi[0x30 + offset] = b;
            return;
        }
        ram.hi[r] = b;
        return;
    }

    if (r == RI_NR52)
    {
        if (b & 0x80)
        {
            if (!(R_NR52 & 0x80))
            {
                snd.length_phase = 0;
                snd.frame_step = 0;
                if (hw.cgb)
                {
                    S1.len = S2.len = S3.len = S4.len = 0;
                    S1.suppress_enable_clock =
                        S2.suppress_enable_clock =
                        S3.suppress_enable_clock =
                        S4.suppress_enable_clock = 0;
                }
            }
            R_NR52 = (R_NR52 & 0x0f) | 0x80;
        }
        else
            sound_power_off();
        return;
    }

    if (!(R_NR52 & 0x80))
    {
        if (hw.cgb)
            return;

        switch (r)
        {
        case RI_NR11:
            S1.len = SOUND_LENGTH_UNIT * (64 - (b & 0x3f));
            S1.suppress_enable_clock = 0;
            break;
        case RI_NR21:
            S2.len = SOUND_LENGTH_UNIT * (64 - (b & 0x3f));
            S2.suppress_enable_clock = 0;
            break;
        case RI_NR31:
            S3.len = SOUND_LENGTH_UNIT * (256 - b);
            S3.suppress_enable_clock = 0;
            break;
        case RI_NR41:
            ram.hi[r] = b;
            S4.len = SOUND_LENGTH_UNIT * (64 - (b & 0x3f));
            S4.suppress_enable_clock = 0;
            break;
        }
        return;
    }

    old = REG(r);
    ram.hi[r]=b;

    switch (r)
    {
    case RI_NR10:
        S1.swlenreload = (b >> 4) & 7;
        if (!S1.swlenreload)
            S1.swlenreload = 8;
        S1.swsteps = b & 7;
        S1.swdir = b & 0x08;
        if (S1.swneg_used && (old & 0x08) && !(b & 0x08))
        {
            S1.swlen = 0;
            S1.on = 0;
            R_NR52 &= 0xfe;
        }
        break;
    case RI_NR11:
        S1.len = SOUND_LENGTH_UNIT * (64 - (b & 0x3f));
        S1.suppress_enable_clock = 0;
        S1.wave = soundWavePattern[b >> 6];
        break;
    case RI_NR12:
        S1.envol = b >> 4;
        S1.endir = b & 0x08;
        S1.enlenreload = S1.enlen = 689 * (b & 7);
        if (!sound_env_dac_enabled(b))
        {
            R_NR52 &= 0xfe;
            S1.on = 0;
        }
        break;
    case RI_NR13:
        freq = (((int)(R_NR14 & 7)) << 8) | b;
        freq = 2048 - freq;
        if(freq)
        {
            S1.skip = SOUND_MAGIC / freq;
        }
        else
        {
            S1.skip = 0;
        }
        break;
    case RI_NR14:
        freq = (((int)(b&7) << 8) | R_NR13);
        freq = 2048 - freq;
        S1.cont = b & 0x40;
        sound_apply_length_enable_write(&S1, old, b, 0xfe);
        if(freq) 
        {
            S1.skip = SOUND_MAGIC / freq;
        } 
        else
        {
            S1.skip = 0;
        }
        if(b & 0x80)
        {
            S1.envol = R_NR12 >> 4;
            S1.endir = R_NR12 & 0x08;
            reloaded_length = S1.len <= 0;
            if (S1.len <= 0)
            {
                S1.len = SOUND_LENGTH_UNIT * 64;
                S1.suppress_enable_clock = !(b & 0x40) && !(old & 0x40);
            }
            else
                S1.suppress_enable_clock = 0;
            if (reloaded_length && S1.cont && sound_trigger_extra_clock_allowed(old, b)
                    && sound_length_extra_clock_phase())
                sound_clock_length_once(&S1, 0xfe);
            S1.enlenreload = S1.enlen = 689 * (R_NR12 & 7);
            S1.swlenreload = (R_NR10 >> 4) & 7;
            if (!S1.swlenreload)
                S1.swlenreload = 8;
            S1.swlen = S1.swlenreload;
            S1.swshadow = ((int)(b & 7) << 8) | R_NR13;
            S1.swenabled = ((R_NR10 >> 4) & 7) || (R_NR10 & 7);
            S1.swneg_used = 0;
            S1.swsteps = R_NR10 & 7;
            S1.swdir = R_NR10 & 0x08;
            S1.swstep = 0;
  
            S1.pos = 0;
            if (sound_env_dac_enabled(R_NR12))
            {
                R_NR52 |= 1;
                S1.on = 1;
            }
            else
            {
                R_NR52 &= 0xfe;
                S1.on = 0;
            }

            if (S1.on && (R_NR10 & 7))
                sound_sweep_check_overflow();
        }
        break;
    case RI_NR21:
        S2.wave = soundWavePattern[b >> 6];
        S2.len = SOUND_LENGTH_UNIT * (64 - (b & 0x3f));
        S2.suppress_enable_clock = 0;
        break;
    case RI_NR22:
        S2.envol = b >> 4;
        S2.endir = b & 0x08;
        S2.enlenreload = S2.enlen = 689 * (b & 7);
        if (!sound_env_dac_enabled(b))
        {
            R_NR52 &= 0xfd;
            S2.on = 0;
        }
        break;
    case RI_NR23:
        freq = (((int)(R_NR24 & 7)) << 8) | b;
        freq = 2048 - freq;
        if(freq)
        {
            S2.skip = SOUND_MAGIC / freq;
        } 
        else
        {
            S2.skip = 0;
        }
        break;
    case RI_NR24:
        freq = (((int)(b&7) << 8) | R_NR23);
        freq = 2048 - freq;
        S2.cont = b & 0x40;
        sound_apply_length_enable_write(&S2, old, b, 0xfd);
        if(freq) {
            S2.skip = SOUND_MAGIC / freq;
        } else
            S2.skip = 0;
        if(b & 0x80) {
            S2.envol = R_NR22 >> 4;
            S2.endir = R_NR22 & 0x08;
            reloaded_length = S2.len <= 0;
            if (S2.len <= 0)
            {
                S2.len = SOUND_LENGTH_UNIT * 64;
                S2.suppress_enable_clock = !(b & 0x40) && !(old & 0x40);
            }
            else
                S2.suppress_enable_clock = 0;
            if (reloaded_length && S2.cont && sound_trigger_extra_clock_allowed(old, b)
                    && sound_length_extra_clock_phase())
                sound_clock_length_once(&S2, 0xfd);
            S2.enlenreload = S2.enlen = 689 * (R_NR22 & 7);

            S2.pos = 0;
            if (sound_env_dac_enabled(R_NR22))
            {
                R_NR52 |= 2;
                S2.on = 1;
            }
            else
            {
                R_NR52 &= 0xfd;
                S2.on = 0;
            }
        }
        break;
    case RI_NR30:
        if (!(b & 0x80)){
            R_NR52 &= 0xfb;
            S3.on = 0;
        }
        break;
    case RI_NR31:
        S3.len = (256-R_NR31) * SOUND_LENGTH_UNIT;
        S3.suppress_enable_clock = 0;
        break;
    case RI_NR32:
        S3.outputlevel = (b >> 5) & 3;
        break;
    case RI_NR33:
        freq = 2048 - (((int)(R_NR34&7) << 8) | b);
        if(freq)
            S3.skip = SOUND_MAGIC_2 / freq;
        else
            S3.skip = 0;
        break;
    case RI_NR34:
        freq = 2048 - (((b&7)<<8) | R_NR33);

        if(freq)
            S3.skip = SOUND_MAGIC_2 / freq;
        else
            S3.skip = 0;

        S3.cont=b & 0x40;
        sound_apply_length_enable_write(&S3, old, b, 0xfb);
        if(b & 0x80)
        {
            sound_wave_retrigger_dmg();
            reloaded_length = S3.len <= 0;
            if (S3.len <= 0)
            {
                S3.len = SOUND_LENGTH_UNIT * 256;
                S3.suppress_enable_clock = !(b & 0x40) && !(old & 0x40);
            }
            else
                S3.suppress_enable_clock = 0;
            if (reloaded_length && S3.cont && sound_trigger_extra_clock_allowed(old, b)
                    && sound_length_extra_clock_phase())
                sound_clock_length_once(&S3, 0xfb);
            S3.pos = 0;
            sound_wave_restart_timer();
            if (R_NR30 & 0x80)
            {
                R_NR52 |= 4;
                S3.on = 1;
            }
            else
            {
                R_NR52 &= 0xfb;
                S3.on = 0;
            }
        }
        break;
    case RI_NR41:
        S4.len = SOUND_LENGTH_UNIT * (64 - (b & 0x3f));
        S4.suppress_enable_clock = 0;
        break;
    case RI_NR42:
        S4.envol = b >> 4;
        S4.endir = b & 0x08;
        S4.enlenreload = S4.enlen = 689 * (b & 7);
        if (!sound_env_dac_enabled(b))
        {
            R_NR52 &= 0xf7;
            S4.on = 0;
        }
        break;
    case RI_NR43:
        freq = soundFreqRatio[b & 7];

        S4.nsteps = b & 0x08;
        S4.skip = (freq << 8) / NOISE_MAGIC;
        S4.clock = b >> 4;

        freq = freq / soundShiftClock[S4.clock];
        S4.shiftskip = (freq << 8) / NOISE_MAGIC;
        break;
    case RI_NR44:
        S4.cont = b & 0x40;
        sound_apply_length_enable_write(&S4, old, b, 0xf7);
        if(b & 0x80)
        {
            S4.envol = R_NR42 >> 4;
            S4.endir = R_NR42 & 0x08;
            reloaded_length = S4.len <= 0;
            if (S4.len <= 0)
            {
                S4.len = SOUND_LENGTH_UNIT * 64;
                S4.suppress_enable_clock = !(b & 0x40) && !(old & 0x40);
            }
            else
                S4.suppress_enable_clock = 0;
            if (reloaded_length && S4.cont && sound_trigger_extra_clock_allowed(old, b)
                    && sound_length_extra_clock_phase())
                sound_clock_length_once(&S4, 0xf7);
            S4.enlenreload = S4.enlen = 689 * (R_NR42 & 7);

            S4.on = 1;
            
            S4.pos = 0;
            S4.shiftpos = 0;
            
            freq = soundFreqRatio[R_NR43 & 7];
      
            S4.shiftpos = (freq << 8) / NOISE_MAGIC;

            S4.nsteps = R_NR43 & 0x08;
            
            freq = freq / soundShiftClock[R_NR43 >> 4];
      
            S4.shiftskip = (freq << 8) / NOISE_MAGIC;
            if(S4.nsteps)
            {
                S4.shiftright = 0x7fff;
            }
            else
            {
                S4.shiftright = 0x7f;
            }
            if (sound_env_dac_enabled(R_NR42))
            {
                R_NR52 |= 8;
                S4.on = 1;
            }
            else
            {
                R_NR52 &= 0xf7;
                S4.on = 0;
            }
        }
        break;
    case RI_NR50:
        snd.level1 = b & 7;
        snd.level2 = (b >> 4) & 7;
        break;
    case RI_NR51:
        snd.balance = b;
        break;
    }
    
    snd.gbDigitalSound = true;

    if(S1.on && S1.envol != 0)
        snd.gbDigitalSound = false;
    if(S2.on && S2.envol != 0)
        snd.gbDigitalSound = false;
    if(S3.on && S3.outputlevel != 0)
        snd.gbDigitalSound = false;
    if(S4.on && S4.envol != 0)
        snd.gbDigitalSound = false;
}

void sound_reset(void)
{
    snd.level1 = 7;
    snd.level2 = 7;
    S1.on           = S2.on     = S3.on     = S4.on = 0;
    S1.len          = S2.len    = S3.len    = S4.len = 0;
    S1.skip         = S2.skip   = S3.skip   = S4.skip = 0;
    S1.pos          = S2.pos    = S3.pos    = S4.pos = 0;
    S1.cont         = S2.cont   = S3.cont   = S4.cont = 0;
    S1.suppress_enable_clock =
        S2.suppress_enable_clock =
        S3.suppress_enable_clock =
        S4.suppress_enable_clock = 0;
    S1.envol        = S2.envol              = S4.envol = 0;
    S1.enlen        = S2.enlen              = S4.enlen = 0;
    S1.endir        = S2.endir              = S4.endir = 0;
    S1.enlenreload  = S2.enlenreload        = S4.enlenreload = 0;
    S1.swlen = 0;
    S1.swlenreload = 0;
    S1.swsteps = 0;
    S1.swdir = 0;
    S1.swstep = 0;
    S1.swshadow = 0;
    S1.swenabled = 0;
    S1.swneg_used = 0;
    S1.wave         = S2.wave = soundWavePattern[2];

    S3.outputlevel = 0;

    S4.clock = 0;
    S4.shiftright = 0x7f;
    S4.nsteps = 0;
    snd.length_phase = 0;
    snd.frame_step = 0;
    snd.wave_timer = 0;
    snd.wave_access = 0;
    snd.wave_index = 0;
    snd.wave_startup = 0;

    R_NR52 = 0x80;

    sound_write(0x10, 0x80);
    sound_write(0x11, 0xbf);
    sound_write(0x12, 0xf3);
    sound_write(0x14, 0xbf);
    sound_write(0x16, 0x3f);
    sound_write(0x17, 0x00);
    sound_write(0x19, 0xbf);
  
    sound_write(0x1a, 0x7f);
    sound_write(0x1b, 0xff);
    sound_write(0x1c, 0xbf);
    sound_write(0x1e, 0xbf);
  
    sound_write(0x20, 0xff);
    sound_write(0x21, 0x00);
    sound_write(0x22, 0x00);
    sound_write(0x23, 0xbf);
    sound_write(0x24, 0x77);
    sound_write(0x25, 0xf3);
  
    sound_write(0x26, 0xf0);
  
    S1.on = 0;
    S2.on = 0;
    S3.on = 0;
    S4.on = 0;

    int addr = 0x30;
	while(addr < 0x40) 
    {
        ram.hi[addr++] = 0x00;
        ram.hi[addr++] = 0xff;
    }

	if (pcm.hz)
    {
        snd.rate = (1<<21) / pcm.hz;
        snd.quality=44100 / pcm.hz;
    }
	else snd.rate = 0;
}

void sound_dirty(void)
{
    sound_write(RI_NR10, R_NR10);
    sound_write(RI_NR11, R_NR11);
    sound_write(RI_NR12, R_NR12);
    sound_write(RI_NR13, R_NR13);
    sound_write(RI_NR14, R_NR14);
    
    sound_write(RI_NR21, R_NR21);
    sound_write(RI_NR22, R_NR22);
    sound_write(RI_NR23, R_NR23);
    sound_write(RI_NR24, R_NR24);
    
    sound_write(RI_NR30, R_NR30);
    sound_write(RI_NR31, R_NR31);
    sound_write(RI_NR32, R_NR32);
    sound_write(RI_NR33, R_NR33);
    sound_write(RI_NR34, R_NR34);
    
    sound_write(RI_NR42, R_NR42);
    sound_write(RI_NR43, R_NR43);
    sound_write(RI_NR44, R_NR44);
    
    sound_write(RI_NR50, R_NR50);
    sound_write(RI_NR51, R_NR51);
    sound_write(RI_NR52, R_NR52);
}
