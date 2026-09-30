#ifndef NANO_PACK_H
#define NANO_PACK_H

#include <stddef.h>
#include <stdint.h>

#define NANO_PACK_PAGE 65536u
#define NANO_PACK_INDEX 16384u

enum nano_pack_result {
    NANO_PACK_OK, NANO_PACK_IO, NANO_PACK_INVALID,
    NANO_PACK_CRC, NANO_PACK_NOT_FOUND, NANO_PACK_RANGE
};

/* Read one complete file. Reject a file larger than capacity. No open file
 * or shared OS filesystem scratch may survive this callback. */
typedef int (*nano_pack_read_file)(void *, const char *, void *, uint32_t,
                                   uint32_t *);

struct nano_resource {
    uint64_t hash;
    uint32_t offset, size, crc, type, bucket_bytes;
};

/* Caller-owned, fixed-size workspace. Do not put this on the UI task stack. */
struct nano_pack {
    nano_pack_read_file read_file;
    void *io;
    uint64_t pack_id;
    uint32_t resources, expanded_bytes, storage_bytes, page_misses;
    uint32_t index_bucket, index_count, bucket_bytes;
    uint32_t page_bucket, page_number, page_bytes;
    uint8_t index_valid, page_valid, ready;
    uint8_t index[NANO_PACK_INDEX];
    uint8_t page[NANO_PACK_PAGE + 28];
};

uint64_t nano_resource_hash(const char *name);
int nano_pack_open(struct nano_pack *, nano_pack_read_file, void *);
int nano_pack_find(struct nano_pack *, uint64_t, struct nano_resource *);
int nano_pack_read(struct nano_pack *, const struct nano_resource *, uint32_t,
                   void *, uint32_t);
/* The full read additionally validates the resource's own CRC. */
int nano_pack_load(struct nano_pack *, const struct nano_resource *, void *,
                   uint32_t);
void nano_pack_close(struct nano_pack *);

#endif
