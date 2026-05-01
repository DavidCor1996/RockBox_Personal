#include "rockmacros.h"
#include "defs.h"
#include "regs.h"
#include "hw.h"
#include "cpu-gb.h"
#include "mem.h"
#include "lcd-gb.h"
#include "sound.h"
#include "rtc-gb.h"
#include "pcm.h"
#include "emu.h"
#include "profiler.h"

/*
 * emu_reset is called to initialize the state of the emulated
 * system. It should set cpu registers, hardware registers, etc. to
 * their appropriate values at powerup time.
 */

void emu_reset(void)
{
    hw_reset();
    lcd_reset();
    cpu_reset();
    mbc_reset();
    sound_reset();
}

static void emu_profile_cpu_emulate(int cycles, unsigned long *cpu_ticks)
{
    if (rockboy_profile_is_enabled())
    {
        unsigned long start = *rb->current_tick;
        cpu_emulate(cycles);
        *cpu_ticks += *rb->current_tick - start;
    }
    else
        cpu_emulate(cycles);
}

static void emu_profile_step(unsigned long *cpu_ticks)
{
    emu_profile_cpu_emulate(cpu.lcdc, cpu_ticks);
}

#define ROCKBOY_TARGET_FPS 60

static void emu_update_frame_pacing(unsigned long *pace_start,
                                    unsigned long *pace_frames,
                                    int *stable_frames)
{
    unsigned long deadline;
    unsigned long now;
    long lateness;
    int desired_skip;
    int late_tolerance = HZ / ROCKBOY_TARGET_FPS;

    if (late_tolerance < 1)
        late_tolerance = 1;
    if (options.frameskip > options.maxskip)
        options.frameskip = options.maxskip;
    if (options.frameskip < 0)
        options.frameskip = 0;

    (*pace_frames)++;
    deadline = *pace_start +
        ((*pace_frames * HZ) + (ROCKBOY_TARGET_FPS / 2)) /
        ROCKBOY_TARGET_FPS;

    while ((long)(deadline - *rb->current_tick) > 0)
        rb->yield();

    now = *rb->current_tick;
    lateness = (long)(now - deadline);

    if (lateness > HZ)
    {
        *pace_start = now -
            ((*pace_frames * HZ) + (ROCKBOY_TARGET_FPS / 2)) /
            ROCKBOY_TARGET_FPS;
        lateness = 0;
    }

    if (lateness > late_tolerance)
    {
        desired_skip = (lateness * ROCKBOY_TARGET_FPS) / HZ;
        if (desired_skip < 1)
            desired_skip = 1;
        if (desired_skip > options.maxskip)
            desired_skip = options.maxskip;

        if (desired_skip > options.frameskip)
        {
            options.frameskip = desired_skip;
            *stable_frames = 0;
        }
        else if (desired_skip < options.frameskip)
        {
            if (++(*stable_frames) >= 15)
            {
                options.frameskip--;
                *stable_frames = 0;
            }
        }
        else
            *stable_frames = 0;
    }
    else if (options.frameskip > 0 && ++(*stable_frames) >= 30)
    {
        options.frameskip--;
        *stable_frames = 0;
    }
}

/* This mess needs to be moved to another module; it's just here to
 * make things work in the mean time. */
void emu_run(void)
{
    int frames=0, timehun=*rb->current_tick;
    unsigned long pace_start=*rb->current_tick;
    unsigned long pace_frames=0;
    int stable_frames=0;

    setvidmode();
    vid_begin();
    lcd_begin();
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    rb->cpu_boost(true);
#endif
#ifdef HAVE_LCD_COLOR
    set_pal();
#endif

    while(!shut)
    {
        unsigned long frame_start = *rb->current_tick;
        unsigned long cpu_ticks = 0;

        emu_profile_cpu_emulate(2280, &cpu_ticks);
        while (R_LY > 0 && R_LY < 144)
            emu_profile_step(&cpu_ticks);

        rtc_tick();

        if (options.sound || !plugbuf)
        {
            unsigned long audio_start = *rb->current_tick;
            sound_mix();
            rockboy_profile_add(ROCKBOY_TIME_AUDIO_MIX,
                                *rb->current_tick - audio_start);
            rockboy_pcm_submit();
        }

        doevents();
        vid_begin();

        if (!(R_LCDC & 0x80))
            emu_profile_cpu_emulate(32832, &cpu_ticks);
        
        while (R_LY > 0) /* wait for next frame */
        {
            emu_profile_step(&cpu_ticks);
            rb->yield();
        }

        frames++;
        emu_update_frame_pacing(&pace_start, &pace_frames, &stable_frames);

        if(options.showstats)
            if(*rb->current_tick-timehun>=100)
            {
                options.fps=frames;
                frames=0;
                timehun=*rb->current_tick;
            }

        rockboy_profile_add(ROCKBOY_TIME_FRAME,
                            *rb->current_tick - frame_start);
        rockboy_profile_add(ROCKBOY_TIME_CPU, cpu_ticks);
    }

#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    rb->cpu_boost(false);
#endif
}
