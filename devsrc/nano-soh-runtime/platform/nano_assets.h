#ifndef NANO_ASSETS_H
#define NANO_ASSETS_H
#include "nano_pack.h"

enum nano_asset_error {
    NANO_ASSET_OK,
    NANO_ASSET_IO,
    NANO_ASSET_FORMAT,
    NANO_ASSET_TYPE,
    NANO_ASSET_BUDGET,
    NANO_ASSET_CYCLE,
    NANO_ASSET_LEGACY
};

struct nano_asset {
    uint64_t hash;
    void *memory, *data;
    uint32_t bytes, type, count, width, height, format;
    uint8_t ready, loading;
};

struct nano_assets {
    struct nano_pack *pack;
    struct nano_asset *slots;
    unsigned capacity, count, depth;
    size_t budget, used, peak;
    void *(*alloc)(void *, size_t);
    void (*free)(void *, void *);
    void *owner;
    enum nano_asset_error error;
    uint64_t failed_hash;
};

/* The session owns returned pointers. They remain stable until clear after
 * all CPU consumers, GPU work and audio callbacks have quiesced. No implicit
 * eviction of native pointers and no fallback to an unbounded desktop heap. */
void nano_assets_init(struct nano_assets *, struct nano_pack *,
                      struct nano_asset *, unsigned, size_t,
                      void *(*)(void *, size_t), void (*)(void *, void *),
                      void *);
struct nano_asset *nano_assets_get(struct nano_assets *, uint64_t);
void nano_assets_clear(struct nano_assets *);

/* Logical cartridge addresses, never CPU pointers. Only compressed samples
 * use this range; the audio DMA adapter resolves them into its bounded cache. */
#define NANO_SAMPLE_BASE ((uintptr_t)0x40000000u)
#define NANO_SAMPLE_STRIDE ((uintptr_t)0x00100000u)
#define NANO_SAMPLE_SLOTS 512u
int nano_assets_sample_read(struct nano_assets *, uintptr_t, void *, size_t);

#endif
