#include "nano_pack.h"

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint64_t le64(const uint8_t *p)
{
    return le32(p) | (uint64_t)le32(p + 4) << 32;
}

static uint32_t crc32(const uint8_t *p, uint32_t len, uint32_t skip)
{
    uint32_t crc = UINT32_MAX, i, bit;
    for (i = 0; i < len; i++) {
        crc ^= i >= skip && i - skip < 4 ? 0 : p[i];
        for (bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

uint64_t nano_resource_hash(const char *name)
{
    uint64_t crc = UINT64_MAX;
    unsigned bit;
    /* Same non-reflected ECMA polynomial and seed as pinned libultraship. */
    if (!name) return 0;
    if (name[0] == '_' && name[1] == '_' && name[2] == 'O' &&
        name[3] == 'T' && name[4] == 'R' && name[5] == '_' && name[6] == '_')
        name += 7;
    while (*name) {
        crc ^= (uint64_t)(uint8_t)*name++ << 56;
        for (bit = 0; bit < 8; bit++)
            crc = (crc << 1) ^ (UINT64_C(0x42f0e1eba9ea3693) &
                                (UINT64_C(0) - (crc >> 63)));
    }
    return crc;
}

static void hex(char *out, uint32_t value, unsigned digits)
{
    static const char alphabet[] = "0123456789abcdef";
    while (digits) {
        digits--;
        out[digits] = alphabet[value & 15];
        value >>= 4;
    }
}

static int read_file(struct nano_pack *p, const char *name, void *dst,
                     uint32_t cap, uint32_t *got)
{
    *got = 0;
    if (p->read_file(p->io, name, dst, cap, got) || *got > cap)
        return NANO_PACK_IO;
    p->storage_bytes += *got;
    return NANO_PACK_OK;
}

void nano_pack_close(struct nano_pack *p)
{
    p->ready = p->index_valid = p->page_valid = 0;
    p->read_file = NULL;
    p->io = NULL;
}

int nano_pack_open(struct nano_pack *p, nano_pack_read_file reader, void *io)
{
    uint8_t h[60];
    uint32_t got;
    int error;
    nano_pack_close(p);
    if (!reader) return NANO_PACK_INVALID;
    p->read_file = reader;
    p->io = io;
    p->storage_bytes = p->page_misses = 0;
    error = read_file(p, "pack.nsp", h, sizeof h, &got);
    if (error) return error;
    if (got != sizeof h || le32(h) != 0x3150534e || le32(h + 4) != 1 ||
        le32(h + 8) != NANO_PACK_PAGE || le32(h + 12) != 256 ||
        le32(h + 16) > 100000 || le32(h + 20) > 256u * 1024 * 1024)
        return NANO_PACK_INVALID;
    if (crc32(h, sizeof h, 56) != le32(h + 56)) return NANO_PACK_CRC;
    p->pack_id = le64(h + 24);
    p->resources = le32(h + 16);
    p->expanded_bytes = le32(h + 20);
    p->ready = 1;
    return NANO_PACK_OK;
}

static int index_load(struct nano_pack *p, uint32_t bucket)
{
    char path[] = "00/index.nsi";
    uint32_t got, count, total, next = 0, i;
    uint64_t previous = 0;
    int error;
    if (p->index_valid && p->index_bucket == bucket) return NANO_PACK_OK;
    p->index_valid = 0;
    hex(path, bucket, 2);
    error = read_file(p, path, p->index, sizeof p->index, &got);
    if (error) return error;
    if (got < 32 || le32(p->index) != 0x3149534e ||
        le32(p->index + 4) != 1 || le32(p->index + 8) != bucket ||
        le64(p->index + 24) != p->pack_id) return NANO_PACK_INVALID;
    count = le32(p->index + 12);
    total = le32(p->index + 16);
    if (count > (sizeof p->index - 32) / 24 || got != 32 + count * 24 ||
        total > p->expanded_bytes) return NANO_PACK_INVALID;
    if (crc32(p->index, got, 20) != le32(p->index + 20)) return NANO_PACK_CRC;
    for (i = 0; i < count; i++) {
        const uint8_t *e = p->index + 32 + i * 24;
        uint64_t hash = le64(e);
        uint32_t offset = le32(e + 8), len = le32(e + 12);
        if (hash >> 56 != bucket || (i && hash <= previous) ||
            offset != next || offset > total || len > total - offset)
            return NANO_PACK_INVALID;
        previous = hash;
        next = offset + len;
    }
    if (next != total) return NANO_PACK_INVALID;
    p->index_bucket = bucket;
    p->index_count = count;
    p->bucket_bytes = total;
    p->index_valid = 1;
    return NANO_PACK_OK;
}

int nano_pack_find(struct nano_pack *p, uint64_t hash, struct nano_resource *out)
{
    uint32_t lo = 0, hi;
    int error;
    if (!p->ready || !out) return NANO_PACK_INVALID;
    error = index_load(p, (uint32_t)(hash >> 56));
    if (error) return error;
    hi = p->index_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        const uint8_t *e = p->index + 32 + mid * 24;
        uint64_t current = le64(e);
        if (current < hash) lo = mid + 1;
        else if (current > hash) hi = mid;
        else {
            out->hash = hash;
            out->offset = le32(e + 8);
            out->size = le32(e + 12);
            out->crc = le32(e + 16);
            out->type = le32(e + 20);
            out->bucket_bytes = p->bucket_bytes;
            return NANO_PACK_OK;
        }
    }
    return NANO_PACK_NOT_FOUND;
}

static int page_load(struct nano_pack *p, uint32_t bucket, uint32_t page,
                     uint32_t total)
{
    char path[] = "00/000000.nsd";
    uint32_t got, expected, start;
    int error;
    if (page > UINT32_MAX / NANO_PACK_PAGE) return NANO_PACK_RANGE;
    start = page * NANO_PACK_PAGE;
    if (start >= total) return NANO_PACK_RANGE;
    expected = total - start;
    if (expected > NANO_PACK_PAGE) expected = NANO_PACK_PAGE;
    if (p->page_valid && p->page_bucket == bucket && p->page_number == page &&
        p->page_bytes == expected) return NANO_PACK_OK;
    p->page_valid = 0;
    hex(path, bucket, 2);
    hex(path + 3, page, 6);
    error = read_file(p, path, p->page, sizeof p->page, &got);
    if (error) return error;
    p->page_misses++;
    if (got != 28 + expected || le32(p->page) != 0x3144534e ||
        le32(p->page + 4) != bucket || le32(p->page + 8) != page ||
        le32(p->page + 12) != expected || le64(p->page + 20) != p->pack_id)
        return NANO_PACK_INVALID;
    if (crc32(p->page, got, 16) != le32(p->page + 16)) return NANO_PACK_CRC;
    p->page_bucket = bucket;
    p->page_number = page;
    p->page_bytes = expected;
    p->page_valid = 1;
    return NANO_PACK_OK;
}

int nano_pack_read(struct nano_pack *p, const struct nano_resource *res,
                   uint32_t offset, void *dst, uint32_t len)
{
    uint8_t *out = dst;
    uint32_t position, part, i;
    int error;
    if (!p->ready || !res || (!dst && len)) return NANO_PACK_INVALID;
    if (offset > res->size || len > res->size - offset ||
        res->bucket_bytes > p->expanded_bytes ||
        res->offset > res->bucket_bytes ||
        res->size > res->bucket_bytes - res->offset) return NANO_PACK_RANGE;
    position = res->offset + offset;
    while (len) {
        error = page_load(p, (uint32_t)(res->hash >> 56),
                          position / NANO_PACK_PAGE, res->bucket_bytes);
        if (error) return error;
        part = NANO_PACK_PAGE - position % NANO_PACK_PAGE;
        if (part > len) part = len;
        for (i = 0; i < part; i++)
            out[i] = p->page[28 + position % NANO_PACK_PAGE + i];
        position += part;
        out += part;
        len -= part;
    }
    return NANO_PACK_OK;
}

int nano_pack_load(struct nano_pack *p, const struct nano_resource *res,
                   void *dst, uint32_t capacity)
{
    int error;
    if (!res || capacity < res->size) return NANO_PACK_RANGE;
    error = nano_pack_read(p, res, 0, dst, res->size);
    if (error) return error;
    return crc32(dst, res->size, UINT32_MAX) == res->crc ?
           NANO_PACK_OK : NANO_PACK_CRC;
}
