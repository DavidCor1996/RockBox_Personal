#include "nano_resource_bridge.h"
#include "global.h"
#include "nano_memory.h"
#include "soh/ResourceManagerHelpers.h"
#include <stdlib.h>
#include <string.h>

static struct nano_assets *resources;
static uint32_t game_version;

int nano_resources_bind(struct nano_assets *a)
{
    struct nano_resource file;
    uint8_t bytes[5];
    if (!a) {
        resources = NULL;
        game_version = 0;
        return 0;
    }
    if (nano_pack_find(a->pack, nano_resource_hash("version"), &file) ||
        file.size != sizeof(bytes) ||
        nano_pack_load(a->pack, &file, bytes, sizeof(bytes)))
        return -1;
    uint32_t version = (uint32_t)bytes[1] << 24 | (uint32_t)bytes[2] << 16 |
                       (uint32_t)bytes[3] << 8 | bytes[4];
    if (bytes[0] != 1 || version != 0xec7011b7u)
        return -1;
    resources = a;
    game_version = version;
    return 0;
}

static _Noreturn void fail(void)
{
    nano_memory_fail(NANO_MEMORY_RESOURCE, 0,
                     resources ? resources->budget - resources->used : 0);
}

struct nano_asset *nano_resource_crc(uint64_t crc, uint32_t type)
{
    if (!resources)
        fail();
    struct nano_asset *a = nano_assets_get(resources, crc);
    if (!a)
        fail();
    if (type && a->type != type) {
        resources->error = NANO_ASSET_TYPE;
        resources->failed_hash = crc;
        fail();
    }
    return a;
}

struct nano_asset *nano_resource_named(const char *name, uint32_t type)
{
    if (!name)
        fail();
    return nano_resource_crc(nano_resource_hash(name), type);
}

struct nano_asset *nano_resource_by_data(const void *p, uint32_t type)
{
    if (!resources || !p)
        fail();
    for (unsigned i = 0; i < resources->capacity; ++i) {
        struct nano_asset *a = resources->slots + i;
        if (a->ready && a->data == p && (!type || a->type == type))
            return a;
    }
    fail();
}

int nano_resource_dma_read(uintptr_t address, void *dest, size_t bytes)
{
    if (!resources) fail();
    int result = nano_assets_sample_read(resources, address, dest, bytes);
    if (result < 0) fail();
    return result;
}

SoundFont *ResourceMgr_LoadAudioSoundFontByName(const char *name)
{
    return nano_resource_named(name, 0x4f534654u)->data;
}

SequenceData *ResourceMgr_LoadSeqPtrByName(const char *name)
{
    return nano_resource_named(name, 0x4f534551u)->data;
}

SequenceData ResourceMgr_LoadSeqByName(const char *name)
{
    return *ResourceMgr_LoadSeqPtrByName(name);
}

SoundFontSample *ResourceMgr_LoadAudioSample(const char *name)
{
    return nano_resource_named(name, 0x4f534d50u)->data;
}

uint32_t ResourceMgr_GetNumGameVersions(void)
{
    return resources ? 1 : 0;
}
uint32_t ResourceMgr_GetGameVersion(int i)
{
    if (i != 0 || !resources)
        fail();
    return game_version;
}
uint32_t ResourceMgr_GetGamePlatform(int i)
{
    (void)ResourceMgr_GetGameVersion(i);
    return GAME_PLATFORM_N64;
}
uint32_t ResourceMgr_GetGameRegion(int i)
{
    (void)ResourceMgr_GetGameVersion(i);
    return GAME_REGION_NTSC;
}
uint32_t ResourceMgr_GameHasOriginal(void)
{
    return resources != NULL;
}
uint32_t ResourceMgr_GameHasMasterQuest(void)
{
    return 0;
}
uint32_t ResourceMgr_IsGameMasterQuest(void)
{
    return 0;
}
uint32_t ResourceMgr_IsSceneMasterQuest(s16 scene)
{
    (void)scene;
    return 0;
}
bool ResourceMgr_IsPalLoaded(void)
{
    return false;
}
bool ResourceMgr_IsAltAssetsEnabled(void)
{
    return false;
}
uint8_t ResourceGetIsCustomByName(const char *name)
{
    (void)name;
    return 0;
}
uint8_t ResourceMgr_TexIsRaw(const char *name)
{
    (void)name;
    return 0;
}
uint8_t ResourceMgr_FileExists(const char *name)
{
    struct nano_resource file;
    if (!resources || !name)
        return 0;
    int result =
        nano_pack_find(resources->pack, nano_resource_hash(name), &file);
    if (result != NANO_PACK_OK && result != NANO_PACK_NOT_FOUND) {
        resources->error = NANO_ASSET_IO;
        resources->failed_hash = nano_resource_hash(name);
        fail();
    }
    return result == NANO_PACK_OK;
}

void *ResourceGetDataByName(const char *name)
{
    return nano_resource_named(name, 0)->data;
}
char *ResourceMgr_GetResourceDataByNameHandlingMQ(const char *name)
{
    return ResourceGetDataByName(name);
}
char *ResourceMgr_LoadTexOrDListByName(const char *name)
{
    return ResourceGetDataByName(name);
}
AnimationHeaderCommon *ResourceMgr_LoadAnimByName(const char *name)
{
    return nano_resource_named(name, 0x4f414e4du)->data;
}
CollisionHeader *ResourceMgr_LoadColByName(const char *name)
{
    return nano_resource_named(name, 0x4f434f4cu)->data;
}
Gfx *ResourceMgr_LoadGfxByName(const char *name)
{
    return nano_resource_named(name, 0x4f444c54u)->data;
}
Gfx *ResourceMgr_LoadGfxByCRC(uint64_t hash)
{
    return nano_resource_crc(hash, 0x4f444c54u)->data;
}
Vtx *ResourceMgr_LoadVtxByCRC(uint64_t hash)
{
    struct nano_asset *a = nano_resource_crc(hash, 0x4f415252u);
    if (a->format != 25)
        fail();
    return a->data;
}
Vtx *ResourceMgr_LoadVtxByName(char *name)
{
    return ResourceMgr_LoadVtxByCRC(nano_resource_hash(name));
}
s32 *ResourceMgr_LoadCSByName(const char *name)
{
    return nano_resource_named(name, 0x4f435654u)->data;
}
char *ResourceMgr_LoadIfDListByName(const char *name)
{
    struct nano_resource file;
    if (!resources || !name)
        fail();
    if (nano_pack_find(resources->pack, nano_resource_hash(name), &file))
        fail();
    return file.type == 0x4f444c54u ? (char *)ResourceMgr_LoadGfxByName(name)
                                    : NULL;
}
char *ResourceMgr_LoadPlayerAnimByName(const char *name)
{
    return nano_resource_named(name, 0x4f50414du)->data;
}
SkeletonHeader *ResourceMgr_LoadSkeletonByName(const char *name,
                                               SkelAnime *skel)
{
    (void)skel;
    return nano_resource_named(name, 0x4f534b4cu)->data;
}
/* These entry points manage upstream's optional alternate-skeleton registry.
 * Native resources are owned by the bound session, not by that registry. */
void ResourceMgr_UnregisterSkeleton(SkelAnime *skel)
{
    (void)skel;
}
void ResourceMgr_ClearSkeletons(void)
{
}
void ResourceMgr_UnloadOriginalWhenAltExists(const char *name)
{
    (void)name;
}

char *ResourceMgr_LoadArrayByNameAsVec3s(const char *name)
{
    struct nano_asset *a = nano_resource_named(name, 0x4f415252u);
    if (a->format == 25 || a->count % 3)
        fail();
    size_t bytes = a->count * sizeof(int16_t);
    void *copy = malloc(bytes ? bytes : 1);
    if (!copy)
        nano_memory_fail(NANO_MEMORY_RESOURCE, bytes, 0);
    memcpy(copy, a->data, bytes);
    return copy;
}

int ResourceMgr_OTRSigCheck(char *p)
{
    /* Engine callers pass readable, aligned data or a resource handle. Test
     * each byte in order, as short ordinary data need not span seven bytes. */
    if (!p || ((uintptr_t)p & 1))
        return 0;
    return p[0] == '_' && p[1] == '_' && p[2] == 'O' && p[3] == 'T' &&
           p[4] == 'R' && p[5] == '_' && p[6] == '_';
}

uint16_t ResourceGetTexWidthByName(const char *name)
{
    return nano_resource_named(name, 0x4f544558u)->width;
}
uint16_t ResourceGetTexHeightByName(const char *name)
{
    return nano_resource_named(name, 0x4f544558u)->height;
}
size_t ResourceGetTexSizeByName(const char *name)
{
    return nano_resource_named(name, 0x4f544558u)->count;
}
uint8_t ResourceMgr_ResourceIsBackground(char *name)
{
    return nano_resource_named(name, 0)->type == 0x4f424749u;
}

size_t ResourceGetSizeByName(const char *name)
{
    struct nano_asset *a = nano_resource_named(name, 0);
    switch (a->type) {
    case 0x4f544558u:
    case 0x4f424c42u:
    case 0x4f424749u:
        return a->count;
    case 0x4f50414du:
        return a->count * sizeof(int16_t);
    case 0x4f444c54u:
        return a->count * sizeof(Gfx);
    case 0x4f415252u:
        return a->count * (a->format == 25 ? sizeof(Vtx) : sizeof(int16_t));
    default:
        fail();
    }
}

void ResourceMgr_PatchGfxByName(const char *name, const char *patch, int index,
                                Gfx instruction)
{
    (void)patch;
    struct nano_asset *a = nano_resource_named(name, 0x4f444c54u);
    if (index < 0 || (uint32_t)index >= a->count)
        fail();
    ((Gfx *)a->data)[index] = instruction;
}

void ResourceUnloadDirectory(const char *name)
{
    /* GameState_Destroy only requests the alternate-asset directory, absent
     * from this admitted pack.
     * Refuse arbitrary eviction while actors or GPU commands borrow pointers.
     */
    if (strcmp(name, "alt/*"))
        fail();
}
