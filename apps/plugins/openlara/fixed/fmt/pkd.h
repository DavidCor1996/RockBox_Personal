#ifndef H_PKD
#define H_PKD

#include "common.h"
#include "stream.h"

#if defined(SIMULATOR) && (__SIZEOF_POINTER__ > 4)
struct DiskRoomData
{
    uint32 quads, triangles, vertices, sprites;
    uint32 portals, sectors, lights, meshes;
};

struct DiskRoomInfo
{
    int16 x, z, yBottom, yTop;
    uint16 quadsCount, trianglesCount, verticesCount, spritesCount;
    uint8 portalsCount, lightsCount, meshesCount, ambient;
    uint8 xSectors, zSectors, alternateRoom, flags;
    DiskRoomData data;
};

struct DiskTexture { uint32 tile, uv01, uv23; };
struct DiskSprite  { uint32 tile, uwvh; int16 l, t, r, b; };

struct DiskLevel
{
    uint32 version;
    uint16 tilesCount, roomsCount, modelsCount, meshesCount;
    uint16 staticMeshesCount, spriteSequencesCount, soundSourcesCount;
    uint16 boxesCount, texturesCount, spritesCount, itemsCount;
    uint16 camerasCount, cameraFramesCount, soundOffsetsCount;
    uint32 palette, lightmap, tiles, roomsInfo, floors, meshes, meshOffsets;
    uint32 anims, animStates, animRanges, animCommands, nodes, animFrames;
    uint32 models, staticMeshes, textures, sprites, spriteSequences, cameras;
    uint32 soundSources, boxes, overlaps, zones[2][ZONE_MAX];
    uint32 animTexData, itemsInfo, cameraFrames, soundMap, soundsInfo;
    uint32 soundData, soundOffsets;
};

template <typename T>
static X_INLINE T *pkdPtr(const uint8 *data, uint32 offset)
{
    return (T *)(data + offset);
}

static void read_PKD_64(const uint8 *data)
{
    const DiskLevel *d = (const DiskLevel *)data;
    const DiskRoomInfo *diskRooms = pkdPtr<const DiskRoomInfo>(data, d->roomsInfo);

    level.version = d->version;
    level.tilesCount = d->tilesCount;
    level.roomsCount = d->roomsCount;
    level.modelsCount = d->modelsCount;
    level.meshesCount = d->meshesCount;
    level.staticMeshesCount = d->staticMeshesCount;
    level.spriteSequencesCount = d->spriteSequencesCount;
    level.soundSourcesCount = d->soundSourcesCount;
    level.boxesCount = d->boxesCount;
    level.texturesCount = d->texturesCount;
    level.spritesCount = d->spritesCount;
    level.itemsCount = d->itemsCount;
    level.camerasCount = d->camerasCount;
    level.cameraFramesCount = d->cameraFramesCount;
    level.soundOffsetsCount = d->soundOffsetsCount;
    level.palette = pkdPtr<const uint16>(data, d->palette);
    level.lightmap = pkdPtr<const uint8>(data, d->lightmap);
    level.tiles = pkdPtr<const uint8>(data, d->tiles);
    level.roomsInfo = roomsInfoHost;
    level.floors = pkdPtr<const FloorData>(data, d->floors);
    level.meshes = pkdPtr<const Mesh *>(data, d->meshes);
    level.meshOffsets = pkdPtr<const int32>(data, d->meshOffsets);
    level.anims = pkdPtr<const Anim>(data, d->anims);
    level.animStates = pkdPtr<const AnimState>(data, d->animStates);
    level.animRanges = pkdPtr<const AnimRange>(data, d->animRanges);
    level.animCommands = pkdPtr<const int16>(data, d->animCommands);
    level.nodes = pkdPtr<const ModelNode>(data, d->nodes);
    level.animFrames = pkdPtr<const uint16>(data, d->animFrames);
    level.models = pkdPtr<const Model>(data, d->models);
    level.staticMeshes = pkdPtr<const StaticMesh>(data, d->staticMeshes);
    level.spriteSequences = pkdPtr<const SpriteSeq>(data, d->spriteSequences);
    level.cameras = pkdPtr<FixedCamera>(data, d->cameras);
    level.soundSources = pkdPtr<const SoundSource>(data, d->soundSources);
    level.boxes = pkdPtr<Box>(data, d->boxes);
    level.overlaps = pkdPtr<const uint16>(data, d->overlaps);
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < ZONE_MAX; ++j)
            level.zones[i][j] = pkdPtr<const uint16>(data, d->zones[i][j]);
    level.animTexData = pkdPtr<const uint16>(data, d->animTexData);
    level.itemsInfo = pkdPtr<const ItemObjInfo>(data, d->itemsInfo);
    level.cameraFrames = pkdPtr<const CameraFrame>(data, d->cameraFrames);
    level.soundMap = pkdPtr<const uint16>(data, d->soundMap);
    level.soundsInfo = pkdPtr<const SoundInfo>(data, d->soundsInfo);
    level.soundData = pkdPtr<const uint8>(data, d->soundData);
    level.soundOffsets = pkdPtr<const int32>(data, d->soundOffsets);

    for (int i = 0; i < level.roomsCount; ++i)
    {
        const DiskRoomInfo &src = diskRooms[i];
        RoomInfo &dst = roomsInfoHost[i];
        dst.x = src.x; dst.z = src.z;
        dst.yBottom = src.yBottom; dst.yTop = src.yTop;
        dst.quadsCount = src.quadsCount; dst.trianglesCount = src.trianglesCount;
        dst.verticesCount = src.verticesCount; dst.spritesCount = src.spritesCount;
        dst.portalsCount = src.portalsCount; dst.lightsCount = src.lightsCount;
        dst.meshesCount = src.meshesCount; dst.ambient = src.ambient;
        dst.xSectors = src.xSectors; dst.zSectors = src.zSectors;
        dst.alternateRoom = src.alternateRoom; dst.flags = src.flags;
        dst.data.quads = pkdPtr<const RoomQuad>(data, src.data.quads);
        dst.data.triangles = pkdPtr<const RoomTriangle>(data, src.data.triangles);
        dst.data.vertices = pkdPtr<const RoomVertex>(data, src.data.vertices);
        dst.data.sprites = pkdPtr<const RoomSprite>(data, src.data.sprites);
        dst.data.portals = pkdPtr<const Portal>(data, src.data.portals);
        dst.data.sectors = pkdPtr<const Sector>(data, src.data.sectors);
        dst.data.lights = pkdPtr<const Light>(data, src.data.lights);
        dst.data.meshes = pkdPtr<const RoomMesh>(data, src.data.meshes);
    }

    const DiskTexture *diskTextures = pkdPtr<const DiskTexture>(data, d->textures);
    for (int i = 0; i < level.texturesCount; ++i)
    {
        texturesHost[i].tile = (uintptr_t)level.tiles + diskTextures[i].tile;
        texturesHost[i].uv01 = diskTextures[i].uv01;
        texturesHost[i].uv23 = diskTextures[i].uv23;
    }
    level.textures = texturesHost;

    const DiskSprite *diskSprites = pkdPtr<const DiskSprite>(data, d->sprites);
    for (int i = 0; i < level.spritesCount; ++i)
    {
        spritesHost[i].tile = (uintptr_t)level.tiles + diskSprites[i].tile;
        spritesHost[i].uwvh = diskSprites[i].uwvh;
        spritesHost[i].l = diskSprites[i].l; spritesHost[i].t = diskSprites[i].t;
        spritesHost[i].r = diskSprites[i].r; spritesHost[i].b = diskSprites[i].b;
    }
    level.sprites = spritesHost;
}
#endif

bool read_PKD(DataStream &f)
{
    const uint8* data = f.getPtr();

#if defined(SIMULATOR) && (__SIZEOF_POINTER__ > 4)
    read_PKD_64(data);
#else
    memcpy(&level, data, sizeof(level));

    { // fix level data offsets
        uint32* ptr = (uint32*)&level.palette;
        while (ptr <= (uint32*)&level.soundOffsets)
        {
            *ptr++ += (uint32)data;
        }
    }
#endif

    { // prepare rooms
        for (int32 i = 0; i < level.roomsCount; i++)
        {
            Room* room = rooms + i;
            room->info = level.roomsInfo + i;
            room->data = room->info->data;

#if defined(SIMULATOR) && (__SIZEOF_POINTER__ > 4)
            /* read_PKD_64 already expanded all 32-bit relative pointers. */
#else
            for (uint32 j = 0; j < sizeof(room->data) / 4; j++)
            {
                int32* x = (int32*)&room->data + j;
                *x += (int32)data;
            }
#endif

            room->sectors = room->data.sectors;
            room->firstItem = NULL;
        }
    }

#ifndef MODEHW
    // initialize global pointers
    gBrightness = -128;
    palSet(level.palette, gSettings.video_gamma << 4, gBrightness);
    memcpy(gLightmap, level.lightmap, sizeof(gLightmap));
#endif

#ifdef ROM_READ
    // prepare textures (required by anim tex logic)
    memcpy(textures, level.textures, level.texturesCount * sizeof(Texture));
    level.textures = textures;

    // prepare sprites (TODO preprocess tile address in packer)
    memcpy(sprites, level.sprites, level.spritesCount * sizeof(Sprite));
    level.sprites = sprites;

    // prepare boxes
    memcpy(boxes, level.boxes, level.boxesCount * sizeof(Box));
    level.boxes = boxes;

    // prepare fixed cameras
    memcpy(cameras, level.cameras, level.camerasCount * sizeof(FixedCamera));
    level.cameras = cameras;
#endif

#if defined(SIMULATOR) && (__SIZEOF_POINTER__ > 4)
    /* texture and sprite addresses were expanded by read_PKD_64 */
#elif defined(__3DO__)
    for (int32 i = 0; i < level.texturesCount; i++)
    {
        Texture* tex = level.textures + i;
        tex->data += intptr_t(RAM_TEX);
    }
#else
    // TODO preprocess in packer
    for (int32 i = 0; i < level.texturesCount; i++)
    {
        level.textures[i].tile += (uintptr_t)level.tiles;
    }

    for (int32 i = 0; i < level.spritesCount; i++)
    {
        level.sprites[i].tile += (uintptr_t)level.tiles;
    }
#endif

    return true;
}

#endif
