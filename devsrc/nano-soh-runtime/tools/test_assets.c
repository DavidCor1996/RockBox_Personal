#include "global.h"
#include "nano_assets.h"
#include "nano_memory.h"
#include "nano_resource_bridge.h"
#include "soh/ResourceManagerHelpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct nano_pack pack;
static struct nano_assets assets;
static struct nano_asset slots[512];
static const char *root;
static size_t allocated;
static unsigned streamed_samples;
static size_t streamed_bytes, largest_font;

static void check_sample(struct nano_asset *a)
{
    uint8_t got[4096], expected[4096];
    SoundFontSample *sample = a->data;
    struct nano_resource resource;
    if (nano_pack_find(&pack, a->hash, &resource) ||
        sample->medium != MEDIUM_CART || sample->size != a->count)
        abort();
    for (size_t offset = 0; offset < a->count + sizeof(got); offset += sizeof(got)) {
        memset(expected, 0, sizeof(expected));
        size_t n = offset < a->count ? a->count - offset : 0;
        if (n > sizeof(expected)) n = sizeof(expected);
        if (n && nano_pack_read(&pack, &resource, 72 + offset, expected, n)) abort();
        if (nano_assets_sample_read(&assets, (uintptr_t)sample->sampleAddr + offset,
                                    got, sizeof(got)) != 1 ||
            memcmp(got, expected, sizeof(got))) abort();
    }
    if (nano_assets_sample_read(&assets, (uintptr_t)sample->sampleAddr + NANO_SAMPLE_STRIDE - 1,
                                got, sizeof(got)) != -1) abort();
    ++streamed_samples;
    streamed_bytes += a->count;
}

static void bad_type(void *ignored)
{
    (void)ignored;
    nano_resource_named("objects/object_link_child/gLinkChildSkel",
                        0x4f544558u);
    abort();
}

static void check_bridge(void)
{
    struct nano_memory_error error;
    if (nano_resources_bind(&assets) ||
        ResourceMgr_GetGameVersion(0) != 0xec7011b7u ||
        ResourceMgr_GetGameRegion(0) != GAME_REGION_NTSC ||
        !ResourceMgr_FileExists("objects/object_link_child/gLinkChildSkel") ||
        ResourceMgr_FileExists("missing/resource"))
        abort();
    SkeletonHeader *skel = ResourceMgr_LoadSkeletonByName(
        "__OTR__objects/object_link_child/gLinkChildSkel", NULL);
    if (!skel || skel->limbCount != 21 || skel->skeletonType != 1)
        abort();
    for (unsigned i = 0; i < skel->limbCount; ++i) {
        LodLimb *limb = skel->segment[i];
        if (!limb || (limb->child != 255 && limb->child >= skel->limbCount) ||
            (limb->sibling != 255 && limb->sibling >= skel->limbCount))
            abort();
        for (unsigned j = 0; j < 2; ++j)
            if (limb->dLists[j] &&
                !ResourceMgr_OTRSigCheck((char *)limb->dLists[j]))
                abort();
    }
    if (nano_memory_guard(bad_type, NULL, &error) == 0 ||
        error.phase != NANO_MEMORY_RESOURCE || assets.error != NANO_ASSET_TYPE)
        abort();
    if (!nano_resource_named("objects/object_link_child/gLinkChildSkel", 0) ||
        assets.error)
        abort();
    nano_assets_clear(&assets);
    if (allocated)
        abort();

    const char *collision =
        "scenes/shared/spot04_scene/spot04_sceneCollisionHeader_008918";
    struct nano_asset *a = nano_resource_named(collision, 0x4f434f4cu);
    size_t exact = assets.used;
    CollisionHeader *h = a->data;
    if (!h->numVertices || !h->numPolygons || !h->vtxList || !h->polyList)
        abort();
    nano_assets_clear(&assets);
    assets.budget = exact - 1;
    if (nano_assets_get(&assets, nano_resource_hash(collision)) ||
        assets.error != NANO_ASSET_BUDGET || allocated || assets.used ||
        assets.count)
        abort();
    assets.budget = exact;
    if (!nano_assets_get(&assets, nano_resource_hash(collision)) ||
        assets.used != exact)
        abort();
    nano_assets_clear(&assets);
    assets.budget = 4 * 1024 * 1024;
    nano_resources_bind(NULL);
    puts("Resource bridge: legal-pack identity, Link skeleton dependencies, "
         "typed failures/recovery and exact-budget boundaries passed.");
}

static int read_file(void *user, const char *name, void *dst, uint32_t cap,
                     uint32_t *got)
{
    char path[1024];
    (void)user;
    if (snprintf(path, sizeof(path), "%s/%s", root, name) >= (int)sizeof(path))
        return -1;
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    *got = fread(dst, 1, cap, f);
    int ok = !ferror(f) && fgetc(f) == EOF;
    fclose(f);
    return ok ? 0 : -1;
}

static void *allocate(void *user, size_t size)
{
    (void)user;
    void *p = calloc(1, size);
    if (p)
        ++allocated;
    return p;
}

static void release(void *user, void *p)
{
    (void)user;
    if (p) {
        --allocated;
        free(p);
    }
}

int main(int argc, char **argv)
{
    if (argc != 3)
        return 2;
    root = argv[1];
    if (nano_pack_open(&pack, read_file, NULL))
        return 3;
    nano_assets_init(&assets, &pack, slots, 512, 4 * 1024 * 1024, allocate,
                     release, NULL);
    FILE *list = fopen(argv[2], "r");
    if (!list)
        return 4;
    char name[512];
    unsigned loaded = 0, failed = 0, legacy = 0;
    size_t peak = 0;
    while (fgets(name, sizeof(name), list)) {
        name[strcspn(name, "\r\n")] = 0;
        struct nano_asset *a =
            nano_assets_get(&assets, nano_resource_hash(name));
        if (!a && assets.error == NANO_ASSET_LEGACY) {
            ++legacy;
        } else if (!a) {
            if (failed < 100)
                fprintf(stderr, "FAILED %u: %s\n", assets.error, name);
            ++failed;
        } else {
            if (!a->data || !a->ready || nano_assets_get(&assets, a->hash) != a)
                abort();
            if (a->type == 0x4f534d50u) check_sample(a);
            if (a->type == 0x4f534654u && assets.used > largest_font) largest_font = assets.used;
            ++loaded;
        }
        if (assets.used > peak)
            peak = assets.used;
        nano_assets_clear(&assets);
        if (allocated || assets.used || assets.count)
            abort();
    }
    fclose(list);
    if (!failed)
        check_bridge();
    printf("Decoded %u, failed %u, unused legacy %u; largest single-resource "
           "dependency closure %zu bytes (host pointers).\n",
           loaded, failed, legacy, peak);
    printf("Verified %u streamed samples (%zu bytes), including padded final blocks; "
           "largest soundfont dependency metadata %zu bytes.\n",
           streamed_samples, streamed_bytes, largest_font);
    return failed != 0;
}
