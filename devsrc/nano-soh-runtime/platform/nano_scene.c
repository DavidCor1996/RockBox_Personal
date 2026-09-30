#include "global.h"
#include "nano_memory.h"
#include "nano_resource_bridge.h"
#include <stdio.h>

#define SCENE_TYPE 0x4f524f4du
extern void Play_InitScene(PlayState *, s32);

static void execute(PlayState *play, struct nano_asset *a, unsigned depth)
{
    if (!a || a->type != SCENE_TYPE || depth >= 8)
        nano_memory_fail(NANO_MEMORY_RESOURCE, depth, 8);
    SceneCmd *commands = a->data;
    unsigned entrances = 0;
    for (unsigned i = 0; i < a->count; ++i) {
        SceneCmd *c = commands + i;
        unsigned code = c->base.code;
        if (code == SCENE_CMD_ID_END)
            return;
        if (code == SCENE_CMD_ID_ALTERNATE_HEADER_LIST) {
            unsigned layer = gSaveContext.sceneLayer;
            char **names = c->altHeaders.segment;
            char *name =
                layer && layer <= c->altHeaders.data1 ? names[layer - 1] : NULL;
            if (!name && layer == 3 && c->altHeaders.data1 >= 2)
                name = names[1];
            if (name) {
                execute(play, nano_resource_named(name, SCENE_TYPE), depth + 1);
                return;
            }
            continue;
        }
        if (code == SCENE_CMD_ID_ENTRANCE_LIST)
            entrances = c->entranceList.data1;
        if (code == SCENE_CMD_ID_SPAWN_LIST &&
            ((unsigned)play->curSpawn >= entrances ||
             !play->setupEntranceList ||
             play->setupEntranceList[play->curSpawn].spawn >=
                 c->spawnList.data1))
            nano_memory_fail(NANO_MEMORY_RESOURCE, play->curSpawn, entrances);
        if (code >= SCENE_CMD_ID_MAX || !gSceneCmdHandlers[code])
            nano_memory_fail(NANO_MEMORY_RESOURCE, code, SCENE_CMD_ID_MAX);
        gSceneCmdHandlers[code](play, c);
        if (code == SCENE_CMD_ID_TRANSITION_ACTOR_LIST) {
            /* The engine negates IDs when a doorway actor has spawned.
             * Restoring them allows a cached room to be entered again. */
            for (int j = 0; j < play->transiActorCtx.numActors; ++j) {
                s16 *id = &play->transiActorCtx.list[j].id;
                if (*id < 0)
                    *id = -*id;
            }
        }
    }
    nano_memory_fail(NANO_MEMORY_RESOURCE, a->count, 0);
}

void nano_scene_execute(PlayState *play, struct nano_asset *a)
{
    execute(play, a, 0);
}

s32 OTRRoom_RequestNewRoom(PlayState *play, RoomContext *ctx, s32 number)
{
    if (ctx->status)
        return 0;
    if (number < 0 || number >= play->numRooms)
        nano_memory_fail(NANO_MEMORY_RESOURCE, number, play->numRooms);
    struct nano_asset *room =
        nano_resource_named(play->roomList[number].fileName, SCENE_TYPE);
    ctx->prevRoom = ctx->curRoom;
    ctx->curRoom.num = number;
    ctx->curRoom.segment = NULL;
    ctx->roomToLoad = room->data;
    ctx->unk_34 = room->data;
    ctx->status = 1;
    ctx->activeBufPage ^= 1;
    return 1;
}

s32 OTRfunc_800973FC(PlayState *play, RoomContext *ctx)
{
    if (ctx->status == 1) {
        struct nano_asset *room =
            nano_resource_by_data(ctx->roomToLoad, SCENE_TYPE);
        ctx->curRoom.segment = room->data;
        gSegments[3] = VIRTUAL_TO_PHYSICAL(room->data);
        nano_scene_execute(play, room);
        Player_SetBootData(play, GET_PLAYER(play));
        Actor_SpawnTransitionActors(play, &play->actorCtx);
        ctx->status = 0;
    }
    return 1;
}

void OTRPlay_SpawnScene(PlayState *play, s32 id, s32 spawn)
{
    if (id < 0 || id >= SCENE_ID_MAX)
        nano_memory_fail(NANO_MEMORY_RESOURCE, id, SCENE_ID_MAX);
    SceneTableEntry *entry = gSceneTable + id;
    int dungeon = (id >= SCENE_DEKU_TREE && id <= SCENE_ICE_CAVERN) ||
                  id == SCENE_GERUDO_TRAINING_GROUND ||
                  id == SCENE_INSIDE_GANONS_CASTLE;
    char name[256];
    int n = snprintf(name, sizeof(name), "scenes/%s/%s/%s",
                     dungeon ? "nonmq" : "shared", entry->sceneFile.fileName,
                     entry->sceneFile.fileName);
    if (n < 0 || (unsigned)n >= sizeof(name))
        nano_memory_fail(NANO_MEMORY_RESOURCE, n, sizeof(name));
    struct nano_asset *scene = nano_resource_named(name, SCENE_TYPE);
    entry->unk_13 = 0;
    play->loadedScene = entry;
    play->sceneNum = id;
    play->sceneConfig = entry->config;
    play->sceneSegment = scene->data;
    gSegments[2] = VIRTUAL_TO_PHYSICAL(scene->data);
    Play_InitScene(play, spawn);
    RoomContext *ctx = &play->roomCtx;
    ctx->activeBufPage = 0;
    ctx->status = 0;
    ctx->bufPtrs[0] = ctx->bufPtrs[1] = NULL;
    if (gSaveContext.respawnFlag > 0 &&
        gSaveContext.respawnFlag > ARRAY_COUNT(gSaveContext.respawn))
        nano_memory_fail(NANO_MEMORY_RESOURCE, gSaveContext.respawnFlag,
                         ARRAY_COUNT(gSaveContext.respawn));
    int room =
        gSaveContext.respawnFlag > 0
            ? gSaveContext.respawn[gSaveContext.respawnFlag - 1].roomIndex
            : play->setupEntranceList[play->curSpawn].room;
    OTRRoom_RequestNewRoom(play, ctx, room);
}
