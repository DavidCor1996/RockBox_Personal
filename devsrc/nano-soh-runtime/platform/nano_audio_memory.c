#include "nano_audio_memory.h"
#include "nano_memory.h"
#include "nano_audio_profile.h"
#include <string.h>

static int add(size_t *total, size_t size, size_t count)
{
    if (size > UINT32_MAX - 15u)
        return 0;
    size = (size + 15u) & ~(size_t)15u;
    if (count && size > (UINT32_MAX - *total) / count)
        return 0;
    *total += size * count;
    return 1;
}

int nano_audio_plan(unsigned id, unsigned refresh, struct nano_audio_plan *out)
{
    struct nano_audio_plan p = {0};
    const AudioSpec *s;
    size_t persistent = 16, temporary = 16;
    unsigned updates, commands, players;
    if (!out || id >= ARRAY_COUNT(gAudioSpecs) ||
        (refresh != 50 && refresh != 60))
        return 0;
    s = &gAudioSpecs[id];
    if (!s->numNotes || s->numNotes > 64 || !s->unk_04 ||
        s->unk_04 > 2 || s->numReverbs > 4 || !s->reverbSettings ||
        s->frequency < 8000 || s->frequency > 48000)
        return 0;
    updates = ((((s->frequency / refresh + 15u) & ~15u) + 16u) / 208u + 1u) * s->unk_04;
    /* SynthesisReverb.items has five entries per frame. */
    if (updates > ARRAY_COUNT(gAudioContext.synthesisReverbs[0].items[0]))
        return 0;
    players = s->numSequencePlayers > 4 ? 4 : s->numSequencePlayers;
    commands = s->numNotes * 16u * updates + s->numReverbs * 24u + 320u;
    p.updates = updates;
    p.dma_count = s->numNotes * (3u * s->unk_04 + 1u);
    if (p.dma_count >= 256u)
        return 0;
#define ADD(dst, bytes, count) do { if (!add(&(dst), (bytes), (count))) return 0; } while (0)
    ADD(p.notes_bytes, s->numNotes * sizeof(Note), 1);
    ADD(p.notes_bytes, 0x1e0, s->numNotes);
    ADD(p.notes_bytes, updates * s->numNotes * sizeof(NoteSubEu), 1);
    ADD(p.notes_bytes, commands * sizeof(u64), 2);
    ADD(p.notes_bytes, 0x100 * sizeof(f32), 1);
    for (unsigned i = 0; i < s->numReverbs; ++i) {
        const ReverbSettings *r = &s->reverbSettings[i];
        if (!r->downsampleRate || r->windowSize <= 0)
            return 0;
        ADD(p.notes_bytes, (r->windowSize * 64u / r->downsampleRate) * sizeof(s16), 2);
        if (r->downsampleRate != 1) {
            ADD(p.notes_bytes, 0x20, 4);
            ADD(p.notes_bytes, 0x340, 2u * updates);
        }
        if (r->lowPassFilterCutoffLeft) {
            ADD(p.notes_bytes, 0x40, 1);
            ADD(p.notes_bytes, 8 * sizeof(s16), 1);
        }
        if (r->lowPassFilterCutoffRight) {
            ADD(p.notes_bytes, 0x40, 1);
            ADD(p.notes_bytes, 8 * sizeof(s16), 1);
        }
    }
    ADD(p.notes_bytes, sizeof(SequenceChannel), 16u * players);
    ADD(p.notes_bytes, s->persistentSampleCacheMem, 1);
    ADD(p.notes_bytes, s->temporarySampleCacheMem, 1);
    ADD(p.notes_bytes, 4u * s->numNotes * sizeof(SampleDma) * s->unk_04, 1);
    ADD(p.notes_bytes, s->sampleDmaBufSize1, 3u * s->numNotes * s->unk_04);
    ADD(p.notes_bytes, s->sampleDmaBufSize2, s->numNotes);
    ADD(persistent, s->persistentSeqMem, 1);
    ADD(persistent, s->persistentFontMem, 1);
    ADD(persistent, s->persistentSampleMem, 1);
    ADD(temporary, s->temporarySeqMem, 1);
    ADD(temporary, s->temporaryFontMem, 1);
    ADD(temporary, s->temporarySampleMem, 1);
    p.cache_bytes = persistent + temporary;
    p.session_bytes = p.notes_bytes + p.cache_bytes + 0x100;
    /* Shipwright permanently copies every font's shallow SoundFont record;
     * decoded instruments/samples remain a separate resource-loader cost. */
    ADD(p.permanent_bytes, sizeof(SoundFont), NANO_AUDIO_FONT_COUNT);
    ADD(p.permanent_bytes, NANO_AUDIO_PERMANENT_SEQUENCE_BYTES, 1);
    p.init_bytes = p.permanent_bytes;
    ADD(p.init_bytes, AIBUF_LEN * sizeof(s16), 3);
    ADD(p.init_bytes, NANO_AUDIO_FONT_COUNT * sizeof(SoundFont), 1);
    ADD(p.init_bytes, NANO_AUDIO_SEQUENCE_COUNT + 15, 1);
    ADD(p.init_bytes, NANO_AUDIO_FONT_COUNT, 1);
    p.heap_bytes = p.init_bytes + p.session_bytes;
#undef ADD
    *out = p;
    return 1;
}

int nano_audio_plan_all(struct nano_audio_plan *out)
{
    struct nano_audio_plan largest = {0}, candidate;
    if (!out)
        return 0;
    for (unsigned rate = 50; rate <= 60; rate += 10)
        for (unsigned id = 0; id < ARRAY_COUNT(gAudioSpecs); ++id) {
            if (!nano_audio_plan(id, rate, &candidate))
                return 0;
            if (candidate.heap_bytes > largest.heap_bytes)
                largest = candidate;
        }
    *out = largest;
    return 1;
}

void nano_audio_require_session(unsigned id, unsigned refresh)
{
    struct nano_audio_plan p;
    if (!nano_audio_plan(id, refresh, &p))
        nano_memory_fail(NANO_MEMORY_AUDIO, id, 0);
    if (!gAudioContext.audioSessionPool.start ||
        gAudioContext.audioSessionPool.size < 0 ||
        (size_t)gAudioContext.audioSessionPool.size < p.session_bytes ||
        gAudioContext.externalPool.start != NULL)
        nano_memory_fail(NANO_MEMORY_AUDIO, p.session_bytes,
                         gAudioContext.audioSessionPool.size);
}
