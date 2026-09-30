/* Host-run ARM integration test. Device I/O is supplied by the test runner;
 * scene parsing, object-bank management and collision setup are engine code. */
#include "global.h"
#include "nano_assets.h"
#include "nano_memory.h"
#include "nano_resource_bridge.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/mixer.h"
#include <string.h>

extern void Play_InitScene(PlayState *, s32);
extern s32 OTRRoom_RequestNewRoom(PlayState *, RoomContext *, s32);
extern int test_read_file(void *, const char *, void *, uint32_t, uint32_t *);

static struct nano_pack pack;
static struct nano_assets assets;
static struct nano_asset slots[512];
static u8 assets_heap[1024 * 1024] __attribute__((aligned(16)));
static u8 system_heap[NANO_SYSTEM_BYTES] __attribute__((aligned(16)));
static size_t assets_cursor, live_allocations;
static GameInfo game_info;
static GraphicsContext graphics;
static PlayState *play;
u32 scene_results[24];
char sample_name[256];
s16 sample_decoded[176];

extern s32 AudioLoad_Dma(OSIoMesg *, u32, s32, uintptr_t, uintptr_t,
                        size_t, OSMesgQueue *, s32, const char *);

unsigned test_audio_sample(void)
{
    SoundFontSample *sample = ResourceMgr_LoadAudioSample(sample_name);
    if (!sample || (sample->codec != CODEC_ADPCM && sample->codec != CODEC_SMALL_ADPCM)) return 1;
    OSMesg message, received;
    OSMesgQueue queue;
    OSIoMesg io;
    u8 compressed[128] __attribute__((aligned(16)));
    ADPCM_STATE state = {0};
    osCreateMesgQueue(&queue, &message, 1);
    if (AudioLoad_Dma(&io, 0, OS_READ, (uintptr_t)sample->sampleAddr,
                      (uintptr_t)compressed, sizeof(compressed), &queue,
                      sample->medium, "test") || queue.validCount != 1) return 2;
    if (osRecvMesg(&queue, &received, OS_MESG_NOBLOCK) || queue.validCount) return 3;
    aLoadADPCMImpl(16 * sample->book->order * sample->book->npredictors, sample->book->book);
    aLoadBufferImpl(compressed, 0x3c0, sizeof(compressed));
    aSetBufferImpl(0, 0x3c0, 0x500, 320);
    aADPCMdecImpl(A_INIT | (sample->codec == CODEC_SMALL_ADPCM ? 4 : 0), state);
    aSaveBufferImpl(0x500, sample_decoded, sizeof(sample_decoded));
    return 0;
}

static void *asset_alloc(void *ignored, size_t n)
{
    (void)ignored;
    n = (n + 15u) & ~15u;
    if (n > sizeof(assets_heap) - assets_cursor)
        return NULL;
    void *p = assets_heap + assets_cursor;
    assets_cursor += n;
    ++live_allocations;
    return p;
}

static void asset_free(void *ignored, void *p)
{
    (void)ignored;
    if (p)
        --live_allocations;
}

unsigned test_scene_resource(void)
{
    if (nano_pack_open(&pack, test_read_file, NULL))
        return 1;
    nano_assets_init(&assets, &pack, slots, 512, sizeof(assets_heap),
                     asset_alloc, asset_free, NULL);
    if (nano_resources_bind(&assets))
        return 2;
    gGameInfo = &game_info;
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    gSaveContext.linkAge = LINK_AGE_CHILD;
    gSaveContext.dayTime = 0x8000;
    SystemArena_Init(system_heap, sizeof(system_heap));
    play = SystemArena_Malloc(sizeof(*play));
    if (!play)
        return 3;
    memset(play, 0, sizeof(*play));
    play->state.gfxCtx = &graphics;
    GameAlloc_Init(&play->state.alloc);
    GameState_InitArena(&play->state, NANO_SCENE_BYTES);
    play->sceneNum = SCENE_KOKIRI_FOREST;
    play->sceneSegment =
        nano_resource_named("scenes/shared/spot04_scene/spot04_scene",
                            0x4f524f4du)
            ->data;
    Play_InitScene(play, 0);
    scene_results[0] = play->numRooms;
    scene_results[1] = play->objectCtx.num;
    scene_results[2] = play->colCtx.colHeader->numVertices;
    scene_results[3] = play->colCtx.colHeader->numPolygons;
    scene_results[4] = play->linkActorEntry->pos.x;
    scene_results[5] = play->linkActorEntry->pos.y;
    scene_results[6] = play->linkActorEntry->pos.z;
    scene_results[7] = play->skyboxId;
    scene_results[8] = assets.used;
    scene_results[9] = assets.count;
    scene_results[10] = THA_GetSize(&play->state.tha);
    return !scene_results[0] || !scene_results[2] || !scene_results[3];
}

unsigned test_room_resource(unsigned number)
{
    play->roomCtx.status = 0;
    if (!OTRRoom_RequestNewRoom(play, &play->roomCtx, number))
        return 1;
    struct nano_asset *room =
        nano_resource_by_data(play->roomCtx.roomToLoad, 0x4f524f4du);
    nano_scene_execute(play, room);
    scene_results[11] = play->numSetupActors;
    scene_results[12] = play->objectCtx.num;
    scene_results[13] = play->roomCtx.curRoom.meshHeader->base.type;
    scene_results[14] = assets.used;
    scene_results[15] = assets.count;
    return play->roomCtx.curRoom.num != (s8)number;
}

unsigned test_scene_cleanup(void)
{
    GameAlloc_Cleanup(&play->state.alloc);
    THA_Dt(&play->state.tha);
    SystemArena_Free(play);
    SystemArena_Cleanup();
    nano_assets_clear(&assets);
    nano_resources_bind(NULL);
    nano_pack_close(&pack);
    if (live_allocations)
        return 1;
    assets_cursor = 0;
    return 0;
}
