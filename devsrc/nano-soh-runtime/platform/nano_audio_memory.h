#ifndef NANO_AUDIO_MEMORY_H
#define NANO_AUDIO_MEMORY_H
#include "global.h"

struct nano_audio_plan {
    size_t notes_bytes;
    size_t cache_bytes;
    size_t session_bytes;
    size_t init_bytes;
    size_t permanent_bytes;
    size_t heap_bytes;
    unsigned updates;
    unsigned dma_count;
};

/* Includes every startup allocation, including all DMA buffers. No reduction
 * in voices, sequence channels, sample rate or reverb settings. */
int nano_audio_plan(unsigned spec_id, unsigned refresh_rate,
                    struct nano_audio_plan *plan);
int nano_audio_plan_all(struct nano_audio_plan *plan);
void nano_audio_require_session(unsigned spec_id, unsigned refresh_rate);

#endif
