#include "nano_assets.h"
#include "global.h"
#include "z64skin.h"
#include <limits.h>
#include <string.h>

/* O2R binary resources are little endian. Textures retain their original
 * packed pixel byte order, as required by the graphics interpreter. */
#define TYPE_TEX 0x4f544558u
#define TYPE_DL 0x4f444c54u
#define TYPE_ARRAY 0x4f415252u
#define TYPE_ANIM 0x4f414e4du
#define TYPE_PLAYER 0x4f50414du
#define TYPE_COL 0x4f434f4cu
#define TYPE_SKEL 0x4f534b4cu
#define TYPE_LIMB 0x4f534c42u
#define TYPE_PATH 0x4f505448u
#define TYPE_MATRIX 0x4f4d5458u
#define TYPE_BLOB 0x4f424c42u
#define TYPE_BG 0x4f424749u
#define TYPE_SCENE 0x4f524f4du
#define TYPE_CS 0x4f435654u
#define TYPE_SAMPLE 0x4f534d50u
#define TYPE_FONT 0x4f534654u
#define TYPE_SEQUENCE 0x4f534551u

struct decode {
    struct nano_assets *assets;
    struct nano_resource file;
    struct nano_asset *asset;
    uint32_t offset, crc;
    size_t used, capacity;
    uint8_t *output;
    int error;
};

static uint32_t crc_step(uint32_t crc, const uint8_t *b, size_t n)
{
    while (n--) {
        crc ^= *b++;
        for (unsigned i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return crc;
}

static void read_bytes(struct decode *d, void *out, size_t n)
{
    uint8_t scratch[128];
    if (d->error)
        return;
    if (d->offset > d->file.size || n > d->file.size - d->offset) {
        d->error = NANO_ASSET_FORMAT;
        return;
    }
    while (n) {
        size_t part = n < sizeof(scratch) ? n : sizeof(scratch);
        uint8_t *p = out ? out : scratch;
        if (nano_pack_read(d->assets->pack, &d->file, d->offset, p, part)) {
            d->error = NANO_ASSET_IO;
            return;
        }
        d->crc = crc_step(d->crc, p, part);
        d->offset += part;
        n -= part;
        if (out)
            out = (uint8_t *)out + part;
    }
}

static uint32_t integer(struct decode *d, unsigned bytes)
{
    uint8_t b[4] = {0};
    read_bytes(d, b, bytes);
    return b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 |
           (uint32_t)b[3] << 24;
}

static void *reserve(struct decode *d, size_t count, size_t size)
{
    size_t offset = (d->used + 15u) & ~(size_t)15u;
    if (d->error)
        return NULL;
    if (offset > d->capacity ||
        (count && size > (d->capacity - offset) / count)) {
        d->error = NANO_ASSET_BUDGET;
        return NULL;
    }
    d->used = offset + count * size;
    return d->output ? d->output + offset : NULL;
}

static uint32_t count(struct decode *d, uint32_t max)
{
    uint32_t n = integer(d, 4);
    if (n > max)
        d->error = NANO_ASSET_FORMAT;
    return d->error ? 0 : n;
}

static char *string(struct decode *d, int handle)
{
    uint32_t n = count(d, 255);
    char *s = reserve(d, n + 1 + (handle && n ? 7 : 0), 1);
    if (s && handle && n)
        memcpy(s, "__OTR__", 7);
    read_bytes(d, s ? s + (handle && n ? 7 : 0) : NULL, n);
    if (s) {
        s[n + (handle && n ? 7 : 0)] = 0;
        if (memchr(s + (handle && n ? 7 : 0), 0, n))
            d->error = NANO_ASSET_FORMAT;
    }
    return n ? s : NULL;
}

static void *dependency(struct decode *d, const char *name, uint32_t type)
{
    struct nano_asset *a;
    if (!d->output || !name || d->error)
        return NULL;
    a = nano_assets_get(d->assets, nano_resource_hash(name));
    if (!a) {
        d->error = d->assets->error;
        return NULL;
    }
    if (type && a->type != type) {
        d->error = NANO_ASSET_TYPE;
        return NULL;
    }
    return a->data;
}

#define SET(ptr, field, value)                                                 \
    do {                                                                       \
        __typeof__(value) v_ = (value);                                        \
        if (ptr)                                                               \
            (ptr)->field = v_;                                                 \
    } while (0)

static void *array(struct decode *d)
{
    uint32_t kind = integer(d, 4), n = count(d, 65535);
    d->asset->format = kind;
    if (kind == 25) {
        Vtx *v = reserve(d, n, sizeof(Vtx));
        read_bytes(d, v, n * 16u);
        d->asset->count = n;
        return v;
    }
    if (kind != 16 && kind != 24) {
        d->error = NANO_ASSET_TYPE;
        return NULL;
    }
    /* All scalar/vector resources in the vanilla pack are signed/unsigned
     * 16-bit values; arrays are compact native values, not 8-byte C++ unions.
     */
    uint32_t elements = 0;
    size_t first = (d->used + 15u) & ~(size_t)15u;
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t scalar = integer(d, 4);
        uint32_t lanes = kind == 24 ? count(d, 16) : 1;
        if (scalar != 4 && scalar != 5)
            d->error = NANO_ASSET_TYPE;
        /* Keep successive vectors tightly packed. */
        if (i == 0)
            reserve(d, 0, 1);
        if (lanes > (d->capacity - d->used) / 2u)
            d->error = NANO_ASSET_BUDGET;
        if (d->error)
            return NULL;
        read_bytes(d, d->output ? d->output + d->used : NULL, lanes * 2u);
        d->used += lanes * 2u;
        elements += lanes;
    }
    d->asset->count = elements;
    return d->output ? d->output + first : NULL;
}

static void *collision(struct decode *d)
{
    CollisionHeader *h = reserve(d, 1, sizeof(*h));
    Vec3s bounds[2] = {0};
    read_bytes(d, bounds, sizeof(bounds));
    if (h) {
        h->minBounds = bounds[0];
        h->maxBounds = bounds[1];
    }
    uint32_t nv = count(d, 8192);
    Vec3s *v = reserve(d, nv, sizeof(*v));
    read_bytes(d, v, nv * sizeof(*v));
    uint32_t np = count(d, 65535);
    CollisionPoly *p = reserve(d, np, sizeof(*p));
    read_bytes(d, p, np * sizeof(*p));
    uint32_t ns = count(d, 65535);
    SurfaceType *surface = reserve(d, ns, sizeof(*surface));
    for (uint32_t i = 0; i < ns; ++i) {
        uint32_t b = integer(d, 4), a = integer(d, 4);
        if (surface) {
            surface[i].data[0] = a;
            surface[i].data[1] = b;
        }
    }
    uint32_t nc = count(d, 65535);
    CamData *cams = reserve(d, nc, sizeof(*cams));
    int32_t *indices = reserve(d, nc, sizeof(*indices));
    for (uint32_t i = 0; i < nc; ++i) {
        uint16_t type = integer(d, 2), n = integer(d, 2);
        int32_t index = integer(d, 4);
        if (cams) {
            cams[i].cameraSType = type;
            cams[i].numCameras = n;
            indices[i] = index;
        }
    }
    uint32_t npos = count(d, 65535);
    Vec3s *positions = reserve(d, npos ? npos : 1, sizeof(*positions));
    read_bytes(d, positions, npos * sizeof(*positions));
    uint32_t nw = count(d, 65535);
    WaterBox *water = reserve(d, nw, sizeof(*water));
    for (uint32_t i = 0; i < nw; ++i) {
        int16_t coords[5] = {0};
        read_bytes(d, coords, sizeof(coords));
        uint32_t properties = integer(d, 4);
        if (water) {
            water[i].xMin = coords[0];
            water[i].ySurface = coords[1];
            water[i].zMin = coords[2];
            water[i].xLength = coords[3];
            water[i].zLength = coords[4];
            water[i].properties = properties;
        }
    }
    if (h && !d->error) {
        for (uint32_t i = 0; i < np; ++i)
            if ((p[i].flags_vIA & 8191u) >= nv ||
                (p[i].flags_vIB & 8191u) >= nv || (p[i].vIC & 8191u) >= nv ||
                p[i].type >= ns)
                d->error = NANO_ASSET_FORMAT;
        for (uint32_t i = 0; i < nc; ++i) {
            if (npos && (indices[i] < 0 || (uint32_t)indices[i] >= npos))
                d->error = NANO_ASSET_FORMAT;
            else
                cams[i].camPosData = positions + (npos ? indices[i] : 0);
        }
        h->numVertices = nv;
        h->vtxList = v;
        h->numPolygons = np;
        h->polyList = p;
        h->surfaceTypeList = surface;
        h->cameraDataList = cams;
        h->cameraDataListLen = nc;
        h->numWaterBoxes = nw;
        h->waterBoxes = water;
    }
    return h;
}

static void *animation(struct decode *d)
{
    uint32_t kind = integer(d, 4);
    if (kind == 3) {
        d->error = NANO_ASSET_LEGACY;
        return NULL;
    }
    int16_t frames = integer(d, 2);
    if (kind == 0) {
        AnimationHeader *h = reserve(d, 1, sizeof(*h));
        uint32_t nv = count(d, 1000000);
        int16_t *values = reserve(d, nv, 2);
        read_bytes(d, values, nv * 2u);
        uint32_t nj = count(d, 1024);
        JointIndex *joints = reserve(d, nj, sizeof(*joints));
        read_bytes(d, joints, nj * sizeof(*joints));
        uint16_t max = integer(d, 2);
        if (h) {
            h->common.frameCount = frames;
            h->frameData = values;
            h->jointIndices = joints;
            h->staticIndexMax = max;
        }
        return h;
    }
    if (kind == 1) {
        LinkAnimationHeader *h = reserve(d, 1, sizeof(*h));
        char *name = string(d, 0);
        void *data = dependency(d, name, TYPE_PLAYER);
        if (h) {
            h->common.frameCount = frames;
            h->segment = data;
        }
        return h;
    }
    if (kind == 2) {
        TransformUpdateIndex *h = reserve(d, 1, sizeof(*h));
        uint32_t nr = count(d, 65535);
        uint8_t *ref = reserve(d, nr, 1);
        read_bytes(d, ref, nr);
        uint32_t nt = count(d, 65535);
        TransformData *transforms = reserve(d, nt, sizeof(*transforms));
        read_bytes(d, transforms, nt * sizeof(*transforms));
        uint32_t nc = count(d, 65535);
        int16_t *copy = reserve(d, nc, 2);
        read_bytes(d, copy, nc * 2u);
        if (h) {
            h->refIndex = ref;
            h->transformData = transforms;
            h->copyValues = copy;
        }
        return h;
    }
    d->error = NANO_ASSET_TYPE;
    return NULL;
}

static void *limb(struct decode *d)
{
    uint32_t kind = integer(d, 1), skin = integer(d, 1);
    if (kind == 5) {
        d->error = NANO_ASSET_LEGACY;
        return NULL;
    }
    /* Reserve the largest native limb, preserving pointers on host and ARM. */
    void *result =
        reserve(d, 1,
                sizeof(LodLimb) > sizeof(SkinLimb) ? sizeof(LodLimb)
                                                   : sizeof(SkinLimb));
    char *skin_list = string(d, 1);
    uint16_t vertices = integer(d, 2);
    uint32_t mods = count(d, 1024);
    SkinLimbModif *mod = reserve(d, mods, sizeof(*mod));
    for (uint32_t i = 0; i < mods; ++i) {
        uint16_t unknown = integer(d, 2);
        uint32_t nv = count(d, 65535);
        SkinVertex *v = reserve(d, nv, sizeof(*v));
        read_bytes(d, v, nv * sizeof(*v));
        uint32_t nt = count(d, 65535);
        SkinTransformation *t = reserve(d, nt, sizeof(*t));
        for (uint32_t j = 0; j < nt; ++j) {
            uint8_t index = integer(d, 1);
            int16_t xyz[3] = {0};
            read_bytes(d, xyz, sizeof(xyz));
            uint8_t scale = integer(d, 1);
            if (t) {
                t[j].limbIndex = index;
                t[j].x = xyz[0];
                t[j].y = xyz[1];
                t[j].z = xyz[2];
                t[j].scale = scale;
            }
        }
        if (mod) {
            mod[i].unk_4 = unknown;
            mod[i].vtxCount = nv;
            mod[i].transformCount = nt;
            mod[i].skinVertices = v;
            mod[i].limbTransformations = t;
        }
    }
    char *skin_list2 = string(d, 1);
    read_bytes(d, NULL,
               18); /* Legacy translation/rotation, absent on this target. */
    string(d, 0);
    string(d, 0); /* Legacy sibling/child paths. */
    char *list1 = string(d, 1), *list2 = string(d, 1);
    Vec3s xyz = {0};
    read_bytes(d, &xyz, sizeof(xyz));
    uint8_t child = integer(d, 1), sibling = integer(d, 1);
    if (kind == 1 || kind == 2) {
        LodLimb *l = result;
        if (l) {
            l->jointPos = xyz;
            l->child = child;
            l->sibling = sibling;
            l->dLists[0] = (Gfx *)list1;
            l->dLists[1] = (Gfx *)list2;
        }
    } else if (kind == 4) {
        SkelCurveLimb *l = result;
        if (l) {
            l->firstChildIdx = child;
            l->nextLimbIdx = sibling;
            l->dList[0] = (Gfx *)list1;
            l->dList[1] = (Gfx *)list2;
        }
    } else if (kind == 3) {
        SkinLimb *l = result;
        SkinAnimatedLimbData *a = NULL;
        if (skin == 4) {
            a = reserve(d, 1, sizeof(*a));
            if (a) {
                a->totalVtxCount = vertices;
                a->limbModifCount = mods;
                a->limbModifications = mod;
                a->dlist = (Gfx *)skin_list2;
            }
        } else if (skin != 0 && skin != 5 && skin != 11)
            d->error = NANO_ASSET_TYPE;
        if (l) {
            l->jointPos = xyz;
            l->child = child;
            l->sibling = sibling;
            l->segmentType = skin;
            l->segment = skin == 11 ? (void *)skin_list : a;
        }
    } else
        d->error = NANO_ASSET_TYPE;
    return result;
}

static void *skeleton(struct decode *d)
{
    uint32_t kind = integer(d, 1);
    integer(d, 1);
    uint32_t limbs = count(d, 255), lists = count(d, 255);
    integer(d, 1);
    uint32_t n = count(d, 255);
    if (limbs != n || kind > 2)
        d->error = NANO_ASSET_FORMAT;
    FlexSkeletonHeader *h = reserve(d, 1, sizeof(*h));
    void **parts = reserve(d, n, sizeof(*parts));
    for (uint32_t i = 0; i < n; ++i) {
        char *name = string(d, 0);
        void *p = dependency(d, name, TYPE_LIMB);
        if (parts)
            parts[i] = p;
    }
    if (h) {
        h->sh.segment = parts;
        h->sh.limbCount = limbs;
        h->sh.skeletonType = kind;
        h->dListCount = lists;
    }
    return h;
}

#include "nano_asset_scene.inc"
#include "nano_asset_audio.inc"

static void *decode(struct decode *d)
{
    uint8_t header[64];
    read_bytes(d, header, sizeof(header));
    if (d->error)
        return NULL;
    uint32_t type = (uint32_t)header[4] | (uint32_t)header[5] << 8 |
                    (uint32_t)header[6] << 16 | (uint32_t)header[7] << 24;
    unsigned version = type == TYPE_SAMPLE || type == TYPE_FONT || type == TYPE_SEQUENCE ? 2 : 0;
    if (header[0] || header[1] || header[2] || header[3] || header[8] != version ||
        header[9] || header[10] || header[11] || type != d->file.type) {
        d->error = NANO_ASSET_TYPE;
        return NULL;
    }
    d->asset->type = type;
    switch (type) {
    case TYPE_TEX: {
        uint32_t fmt = integer(d, 4), w = count(d, 2048), h = count(d, 2048),
                 n = count(d, 16 * 1024 * 1024);
        void *p = reserve(d, n, 1);
        read_bytes(d, p, n);
        d->asset->format = fmt;
        d->asset->width = w;
        d->asset->height = h;
        d->asset->count = n;
        return p;
    }
    case TYPE_DL: {
        uint32_t ucode = integer(d, 1);
        read_bytes(d, NULL, 7);
        if (ucode > 5 || (d->file.size - d->offset) % 8)
            d->error = NANO_ASSET_FORMAT;
        size_t n = (d->file.size - d->offset) / 8;
        Gfx *p = reserve(d, n, sizeof(*p));
        for (size_t i = 0; i < n; ++i) {
            uint32_t a = integer(d, 4), b = integer(d, 4);
            if (p) {
                p[i].words.w0 = a;
                p[i].words.w1 = b;
            }
        }
        d->asset->format = ucode;
        d->asset->count = n;
        return p;
    }
    case TYPE_ARRAY:
        return array(d);
    case TYPE_COL:
        return collision(d);
    case TYPE_ANIM:
        return animation(d);
    case TYPE_LIMB:
        return limb(d);
    case TYPE_SKEL:
        return skeleton(d);
    case TYPE_SCENE:
        return scene(d);
    case TYPE_SAMPLE:
        return sample(d);
    case TYPE_FONT:
        return soundfont(d);
    case TYPE_SEQUENCE:
        return sequence(d);
    case TYPE_CS: {
        uint32_t n = count(d, 1024 * 1024);
        /* Binary cutscene exporters already serialize mixed byte/halfword
         * commands in the little-endian layout consumed by the native game. */
        if (n < 3 || n != (d->file.size - d->offset) / 4 ||
            (d->file.size - d->offset) % 4)
            d->error = NANO_ASSET_FORMAT;
        uint32_t *p = reserve(d, n, sizeof(*p));
        read_bytes(d, p, n * 4u);
        d->asset->count = n;
        return p;
    }
    case TYPE_PLAYER:
    case TYPE_BLOB:
    case TYPE_BG: {
        uint32_t n = count(d, 16 * 1024 * 1024),
                 size = type == TYPE_PLAYER ? 2 : 1;
        void *p = reserve(d, n, size);
        read_bytes(d, p, n * size);
        d->asset->count = n;
        return p;
    }
    case TYPE_MATRIX: {
        Mtx *p = reserve(d, 1, sizeof(*p));
        read_bytes(d, p, 64);
        return p;
    }
    case TYPE_PATH: {
        uint32_t n = count(d, 255);
        Path *p = reserve(d, n, sizeof(*p));
        for (uint32_t i = 0; i < n; ++i) {
            uint32_t np = count(d, 255);
            Vec3s *v = reserve(d, np, sizeof(*v));
            read_bytes(d, v, np * sizeof(*v));
            if (p) {
                p[i].count = np;
                p[i].points = v;
            }
        }
        d->asset->count = n;
        return p;
    }
    default:
        d->error = NANO_ASSET_TYPE;
        return NULL;
    }
}

void nano_assets_init(struct nano_assets *a, struct nano_pack *p,
                      struct nano_asset *slots, unsigned capacity,
                      size_t budget, void *(*alloc)(void *, size_t),
                      void (*release)(void *, void *), void *owner)
{
    memset(a, 0, sizeof(*a));
    a->pack = p;
    a->slots = slots;
    a->capacity = slots ? capacity : 0;
    a->budget = budget > UINT32_MAX - 15u ? UINT32_MAX - 15u : budget;
    a->alloc = alloc;
    a->free = release;
    a->owner = owner;
    if (slots)
        memset(slots, 0, capacity * sizeof(*slots));
}

struct nano_asset *nano_assets_get(struct nano_assets *a, uint64_t hash)
{
    struct nano_asset *slot = NULL;
    struct nano_resource file;
    enum nano_asset_error error = NANO_ASSET_OK;
    if (!a->depth) {
        a->error = NANO_ASSET_OK;
        a->failed_hash = 0;
    }
    for (unsigned i = 0; i < a->capacity; ++i) {
        if ((a->slots[i].ready || a->slots[i].loading) &&
            a->slots[i].hash == hash) {
            if (a->slots[i].ready)
                return &a->slots[i];
            error = NANO_ASSET_CYCLE;
            goto fail;
        }
        if (!slot && !a->slots[i].ready && !a->slots[i].loading)
            slot = &a->slots[i];
    }
    if (!slot || a->depth >= 32 || !a->alloc || !a->free) {
        error = NANO_ASSET_BUDGET;
        goto fail;
    }
    if (nano_pack_find(a->pack, hash, &file)) {
        error = NANO_ASSET_IO;
        goto fail;
    }
    memset(slot, 0, sizeof(*slot));
    slot->hash = hash;
    slot->loading = 1;
    ++a->depth;
    struct decode d = {.assets = a,
                       .file = file,
                       .asset = slot,
                       .crc = UINT32_MAX,
                       .capacity = a->budget - a->used};
    decode(&d);
    if (!d.error && (d.offset != file.size || ~d.crc != file.crc))
        d.error = NANO_ASSET_FORMAT;
    if (!d.error) {
        slot->bytes = (d.used + 15u) & ~(size_t)15u;
        if (!slot->bytes)
            slot->bytes = 16;
        if (slot->bytes > a->budget - a->used)
            d.error = NANO_ASSET_BUDGET;
        else if (!(slot->memory = a->alloc(a->owner, slot->bytes)))
            d.error = NANO_ASSET_BUDGET;
        else {
            a->used += slot->bytes;
            if (a->used > a->peak)
                a->peak = a->used;
            memset(slot->memory, 0, slot->bytes);
            d.output = slot->memory;
            d.capacity = slot->bytes;
            d.used = d.offset = 0;
            d.crc = UINT32_MAX;
            slot->data = decode(&d);
            if (!d.error && (d.offset != file.size || ~d.crc != file.crc))
                d.error = NANO_ASSET_FORMAT;
        }
    }
    --a->depth;
    slot->loading = 0;
    if (!d.error) {
        slot->ready = 1;
        ++a->count;
        return slot;
    }
    error = d.error;
    if (slot->memory) {
        a->free(a->owner, slot->memory);
        a->used -= slot->bytes;
    }
    memset(slot, 0, sizeof(*slot));
fail:
    a->error = error;
    a->failed_hash = hash;
    return NULL;
}

void nano_assets_clear(struct nano_assets *a)
{
    if (a->depth)
        return;
    for (unsigned i = 0; i < a->capacity; ++i) {
        if (a->slots[i].memory)
            a->free(a->owner, a->slots[i].memory);
        memset(&a->slots[i], 0, sizeof(a->slots[i]));
    }
    a->used = a->count = 0;
    a->error = NANO_ASSET_OK;
}

int nano_assets_sample_read(struct nano_assets *a, uintptr_t address, void *out,
                           size_t bytes)
{
    if (address < NANO_SAMPLE_BASE ||
        address >= NANO_SAMPLE_BASE + NANO_SAMPLE_STRIDE * NANO_SAMPLE_SLOTS)
        return 0;
    unsigned index = (address - NANO_SAMPLE_BASE) / NANO_SAMPLE_STRIDE;
    size_t offset = (address - NANO_SAMPLE_BASE) % NANO_SAMPLE_STRIDE;
    if (index >= a->capacity || !out || bytes > NANO_SAMPLE_STRIDE - offset)
        return -1;
    struct nano_asset *slot = a->slots + index;
    if (!slot->ready || slot->type != TYPE_SAMPLE) return -1;
    struct nano_resource file;
    if (nano_pack_find(a->pack, slot->hash, &file)) return -1;
    size_t available = offset < slot->count ? slot->count - offset : 0;
    size_t read = bytes < available ? bytes : available;
    if (read && nano_pack_read(a->pack, &file, 72 + offset, out, read)) return -1;
    /* The audio cache requests aligned blocks past a sample's logical end.
     * Supply silence padding rather than exposing adjacent resource metadata. */
    memset((uint8_t *)out + read, 0, bytes - read);
    return 1;
}
