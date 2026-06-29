#include "rockmacros.h"
#include "defs.h"
#include "pcm.h"
#include "profiler.h"
#include "settings.h"

struct pcm pcm IBSS_ATTR;
extern struct options options;

#define N_BUFS 4
#define BUF_SIZE 2048

static unsigned short *buf=0, *hwbuf=0;

static bool newly_started;
static volatile int queued_bufs;
static volatile int read_buf;
static int write_buf;

static int rockboy_pcm_preferred_hz(void)
{
#if defined(HW_HAVE_11) && !defined(TOSHIBA_GIGABEAT_F)
    return SAMPR_11;
#else
    return SAMPR_44;
#endif
}

static void get_more(const void** start, size_t* size)
{
    if (queued_bufs > 0)
    {
        memcpy(hwbuf, &buf[pcm.len * read_buf], pcm.len * sizeof(short));
        read_buf++;
        if (read_buf >= N_BUFS)
            read_buf = 0;
        queued_bufs--;
    }
    else
    {
        memset(hwbuf, 0, pcm.len * sizeof(short));
        rockboy_profile_pcm_underrun();
    }

    *start = hwbuf;
    *size = pcm.len * sizeof(short);
}

void rockboy_pcm_init(void)
{
    if(plugbuf)
        return;

    newly_started = true;
    queued_bufs = 0;
    read_buf = 0;
    write_buf = 0;

    pcm.hz = rockboy_pcm_preferred_hz();
    pcm.stereo = 1;
    pcm.drop_when_full = options.performance_preset != ROCKBOY_PERF_QUALITY;

    pcm.len = BUF_SIZE;
    if(!buf)
    {
        buf = my_malloc(pcm.len * N_BUFS * sizeof(short));
        hwbuf = my_malloc(pcm.len * sizeof(short));

        if (buf && hwbuf)
            memset(buf, 0, pcm.len * N_BUFS * sizeof(short));
    }
    pcm.buf = (buf && hwbuf) ? buf : NULL;
    pcm.pos = 0;

    rb->pcm_play_stop();

#if INPUT_SRC_CAPS != 0
    /* Select playback */
    rb->audio_set_input_source(AUDIO_SRC_PLAYBACK, SRCF_PLAYBACK);
    rb->audio_set_output_source(AUDIO_SRC_PLAYBACK);
#endif
   
    rb->pcm_set_frequency(pcm.hz); /* 44100 22050 11025 */
}

void rockboy_pcm_close(void)
{
    rb->pcm_play_stop();
    memset(&pcm, 0, sizeof pcm);    
    newly_started = true;
    queued_bufs = 0;
    read_buf = 0;
    write_buf = 0;
    rb->pcm_set_frequency(HW_SAMPR_DEFAULT);
#if INPUT_SRC_CAPS != 0
    rb->audio_set_input_source(AUDIO_SRC_PLAYBACK, SRCF_PLAYBACK);
    rb->audio_set_output_source(AUDIO_SRC_PLAYBACK);
#endif
}

int rockboy_pcm_submit(void)
{
    long wait_start;

    if (!pcm.buf) return 0;
    if (pcm.pos < pcm.len) return 1;

    pcm.drop_when_full = options.performance_preset != ROCKBOY_PERF_QUALITY;
    wait_start = *rb->current_tick;
    while (queued_bufs >= N_BUFS - 1 && !pcm.drop_when_full &&
           *rb->current_tick == wait_start)
    {
        rb->yield();
    }
    if (queued_bufs >= N_BUFS - 1)
    {
        rockboy_profile_add(ROCKBOY_TIME_PCM_WAIT,
                            *rb->current_tick - wait_start);
        rockboy_profile_pcm_drop();
        pcm.pos = 0;
        return 1;
    }
    rockboy_profile_add(ROCKBOY_TIME_PCM_WAIT,
                        *rb->current_tick - wait_start);

    rb->pcm_play_lock();
    queued_bufs++;
    rb->pcm_play_unlock();
    write_buf++;
    if (write_buf >= N_BUFS)
        write_buf = 0;
    pcm.buf = &buf[pcm.len * write_buf];
    pcm.pos = 0;

    if(newly_started)
    {
        rb->pcm_play_data(&get_more, NULL, NULL, 0);
        newly_started = false;
    }

    return 1;
}
