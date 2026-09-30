/* Executed as ARM machine code by the host test runner. No device I/O. */
#include "nano_audio_memory.h"
#include "nano_audio_profile.h"
#include "nano_memory.h"
#include <string.h>

static u8 test_heap[512 * 1024] __attribute__((aligned(16)));
static u8 scene_heap[NANO_SYSTEM_BYTES] __attribute__((aligned(16)));
struct nano_audio_plan test_plan;
u32 test_results[16];
u8 test_audio_before[sizeof(AudioContext)];

void *memset(void *p, int v, size_t n) {
    u8 *q = p;
    while (n--) *q++ = v;
    return p;
}
void *memcpy(void *p, const void *s, size_t n) {
    u8 *q = p;
    const u8 *r = s;
    while (n--) *q++ = *r++;
    return p;
}

unsigned test_audio(unsigned id, unsigned rate) {
    struct nano_audio_plan largest;
    if (!nano_audio_plan_all(&largest) || largest.heap_bytes > sizeof(test_heap)) return 5;
    memset(test_heap + largest.heap_bytes, 0xa5, sizeof(test_heap) - largest.heap_bytes);
    gAudioHeap = test_heap;
    osTvType = rate == 50 ? OS_TV_PAL : OS_TV_NTSC;
    /* Repeated initialization is deliberate: stale map sizes must not cause
     * ResetLoadStatus to follow NULL status pointers after context clearing. */
    AudioLoad_Init(NULL, 0);
    if (!nano_audio_plan(id, rate, &test_plan)) return 1;
    gAudioContext.audioResetSpecIdToLoad = id;
    gAudioContext.audioSessionPool.size = test_plan.session_bytes;
    AudioHeap_Init();
    test_results[0] = gAudioContext.notesAndBuffersPool.cur - gAudioContext.notesAndBuffersPool.start;
    test_results[1] = gAudioContext.sampleDmaCount;
    test_results[2] = gAudioContext.audioInitPool.cur - gAudioContext.audioInitPool.start;
    test_results[3] = gAudioContext.audioInitPool.size - test_results[2];
    test_results[4] = sizeof(Note);
    test_results[5] = sizeof(SequenceChannel);
    test_results[6] = sizeof(PlayState);
    test_results[7] = sizeof(GameAllocEntry);
    if (test_results[0] != test_plan.notes_bytes || test_results[1] != test_plan.dma_count ||
        test_results[3] != 0) return 2;
    if (AudioHeap_Alloc(&gAudioContext.notesAndBuffersPool, 1) != NULL) return 3;
    if (AudioHeap_Alloc(&gAudioContext.notesAndBuffersPool, SIZE_MAX) != NULL) return 4;
    for (size_t i = largest.heap_bytes; i < sizeof(test_heap); ++i)
        if (test_heap[i] != 0xa5) return 6;
    return 0;
}

/* Must fail BEFORE altering the current notes, tables or pool cursors. */
void test_audio_short(unsigned id, unsigned rate) {
    gAudioContext.audioResetSpecIdToLoad = id;
    gAudioContext.refreshRate = rate;
    nano_audio_plan(id, rate, &test_plan);
    gAudioContext.audioSessionPool.size = test_plan.session_bytes - 1;
    memcpy(test_audio_before, &gAudioContext, sizeof(gAudioContext));
    AudioHeap_Init();
}

unsigned test_pool(unsigned offset, unsigned size) {
    AudioAllocPool pool;
    AudioHeap_AllocPoolInit(&pool, test_heap + offset, size);
    test_results[0] = (uintptr_t)pool.start;
    test_results[1] = pool.size;
    test_results[2] = (uintptr_t)AudioHeap_Alloc(&pool, size);
    return 0;
}

unsigned test_pool_invalid(void) {
    AudioAllocPool pool;
    for (unsigned i = 0; i < 16; ++i) {
        AudioHeap_AllocPoolInit(&pool, test_heap + i, SIZE_MAX);
        if (pool.start || pool.cur || pool.size || AudioHeap_Alloc(&pool, 0)) return 1;
    }
    AudioHeap_AllocPoolInit(&pool, NULL, 0);
    if (AudioHeap_Alloc(&pool, 0)) return 2;
    AudioHeap_AllocPoolInit(&pool, test_heap, 16);
    pool.size = -1;
    if (AudioHeap_Alloc(&pool, 16)) return 3;
    return 0;
}

unsigned test_scene(void) {
    GameState state = {0};
    PlayState *play;
    SystemArena_Init(scene_heap, sizeof(scene_heap));
    play = SystemArena_Malloc(sizeof(PlayState));
    if (!play) return 1;
    memset(play, 0, sizeof(*play));
    GameAlloc_Init(&state.alloc);
    GameState_InitArena(&state, NANO_SCENE_BYTES);
    void *original = state.tha.bufp;
    GameState_Realloc(&state, NANO_SCENE_BYTES);
    if (state.tha.bufp != original) return 2;
    play->state = state;
    play->sceneNum = SCENE_KOKIRI_FOREST;
    Object_InitBank(play, &play->objectCtx);
    test_results[8] = THA_GetSize(&play->state.tha);
    if (test_results[8] != NANO_SCENE_BYTES - NANO_OBJECT_TOKEN_BYTES) return 3;
    u32 largest, free_bytes, used;
    SystemArena_GetSizes(&largest, &free_bytes, &used);
    test_results[9] = largest;
    test_results[10] = used;
    GameAlloc_Cleanup(&state.alloc);
    SystemArena_Free(play);
    SystemArena_GetSizes(&largest, &free_bytes, &used);
    if (used) return 4;
    return 0;
}

TwoHeadArena test_arena;
void test_arena_init(unsigned size) { THA_Ct(&test_arena, test_heap, size); }
void test_arena_end(unsigned size) { THA_AllocEndAlign16(&test_arena, size); }
void test_arena_start(unsigned size) { THA_AllocStart(&test_arena, size); }
