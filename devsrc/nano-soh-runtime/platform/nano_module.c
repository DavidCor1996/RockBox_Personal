#include "nano_module.h"
#include <limits.h>
#include <stddef.h>
#include <string.h>

#define HEADER_BYTES 48u
#define MAGIC 0x314d534eu
#define ABS32 2u
#define THM_CALL 10u
#define THM_JUMP24 30u

static uint32_t u32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint64_t u64(const uint8_t *p)
{
    return u32(p) | (uint64_t)u32(p + 4) << 32;
}

static void put16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
    put16(p, v);
    put16(p + 2, v >> 16);
}

static int header(nano_module_read read, void *io, uint8_t *h,
                  struct nano_module_info *info)
{
    uint32_t file, image, bss, ni, ne, nr;
    uint64_t expected;
    memset(info, 0, sizeof *info);
    if (!read || read(io, 0, h, HEADER_BYTES)) return NANO_MODULE_IO;
    file = u32(h + 8); image = u32(h + 12); bss = u32(h + 16);
    ni = u32(h + 24); ne = u32(h + 28); nr = u32(h + 32);
    expected = HEADER_BYTES + (uint64_t)ni * 8 + (uint64_t)ne * 16 +
               (uint64_t)nr * 20 + image;
    if (u32(h) != MAGIC || u32(h + 4) != 1 || u32(h + 20) != 16 ||
        u32(h + 40) || u32(h + 44) || !image ||
        expected != file || file > 16u * 1024 * 1024 ||
        image > 4u * 1024 * 1024 || bss > 4u * 1024 * 1024 - image ||
        ni > 65535 || ne > 65535 || nr > image / 2)
        return NANO_MODULE_FORMAT;
    info->image_bytes = image;
    info->memory_bytes = image + bss;
    info->file_bytes = file;
    info->exports = ne;
    info->export_offset = HEADER_BYTES + ni * 8;
    info->crc32 = u32(h + 36);
    return NANO_MODULE_OK;
}

int nano_module_inspect(nano_module_read read, void *io,
                        struct nano_module_info *info)
{
    uint8_t h[HEADER_BYTES];
    if (!info) return NANO_MODULE_FORMAT;
    return header(read, io, h, info);
}

static int checksum(nano_module_read read, void *io, uint32_t size,
                    uint32_t expected)
{
    uint8_t bytes[512];
    uint32_t pos = 0, crc = UINT32_MAX, n, i, bit;
    while (pos < size) {
        n = size - pos;
        if (n > sizeof bytes) n = sizeof bytes;
        if (read(io, pos, bytes, n)) return NANO_MODULE_IO;
        for (i = 0; i < n; i++) {
            crc ^= pos + i >= 36 && pos + i < 40 ? 0 : bytes[i];
            for (bit = 0; bit < 8; bit++)
                crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
        }
        pos += n;
    }
    return ~crc == expected ? NANO_MODULE_OK : NANO_MODULE_CHECKSUM;
}

static int relocate(uint8_t *image, uint32_t base, uint32_t off,
                    uint32_t kind, struct nano_module_symbol symbol,
                    int32_t addend)
{
    uint8_t *p = image + off;
    int64_t delta;
    uint32_t bits, sign, i1, i2, j1, j2, lo, hi;
    if (symbol.thumb > 1 || (symbol.thumb && (symbol.address & 1)))
        return NANO_MODULE_RELOCATION;
    if (kind == ABS32) {
        put32(p, (symbol.address + (uint32_t)addend) | symbol.thumb);
        return NANO_MODULE_OK;
    }
    /* The selected modules contain Thumb calls/jumps. ARM interworking and
     * veneers are deliberately rejected, never approximated with a bad jump. */
    if (!symbol.thumb || (kind != THM_CALL && kind != THM_JUMP24))
        return NANO_MODULE_RELOCATION;
    hi = (uint32_t)p[0] | (uint32_t)p[1] << 8;
    lo = (uint32_t)p[2] | (uint32_t)p[3] << 8;
    if ((hi & 0xf800) != 0xf000 ||
        (lo & 0xd000) != (kind == THM_CALL ? 0xd000u : 0x9000u))
        return NANO_MODULE_RELOCATION;
    delta = (int64_t)symbol.address + addend - ((int64_t)base + off);
    if ((delta & 1) || delta < -16777216 || delta > 16777214)
        return NANO_MODULE_RELOCATION;
    bits = (uint32_t)delta;
    sign = (bits >> 24) & 1;
    i1 = (bits >> 23) & 1; i2 = (bits >> 22) & 1;
    j1 = !(i1 ^ sign); j2 = !(i2 ^ sign);
    put16(p, 0xf000 | sign << 10 | ((bits >> 12) & 0x3ff));
    put16(p + 2, (kind == THM_CALL ? 0xd000u : 0x9000u) |
                 j1 << 13 | j2 << 11 | ((bits >> 1) & 0x7ff));
    return NANO_MODULE_OK;
}

int nano_module_load(nano_module_read read, void *io,
                     nano_module_resolve resolve, void *imports,
                     nano_module_sync sync, void *sync_ctx,
                     void *memory, uint32_t capacity, uint32_t base,
                     struct nano_module_info *info)
{
    uint8_t h[HEADER_BYTES], entry[20], hash[8];
    uint8_t *image = memory;
    uint32_t ni, nr, image_off, reloc_off, at, n, i, off, previous = 0;
    uint32_t kind, source, target, value;
    uint64_t previous_hash = 0;
    struct nano_module_symbol symbol;
    int rc;
    if (!info) return NANO_MODULE_FORMAT;
    rc = header(read, io, h, info);
    if (rc) return rc;
    if (!memory || capacity < info->memory_bytes || (base & 15) ||
        ((uintptr_t)memory & 15) || base > UINT32_MAX - info->memory_bytes)
        return NANO_MODULE_CAPACITY;
    ni = u32(h + 24); nr = u32(h + 32);
    reloc_off = info->export_offset + info->exports * 16;
    image_off = reloc_off + nr * 20;
    rc = checksum(read, io, info->file_bytes, u32(h + 36));
    if (rc) goto fail;
    /* Validate export metadata before any code can become ready. */
    for (i = 0; i < info->exports; i++) {
        if (read(io, info->export_offset + i * 16, entry, 16)) {
            rc = NANO_MODULE_IO; goto fail;
        }
        value = u32(entry + 8);
        if ((i && u64(entry) <= previous_hash) ||
            value > info->memory_bytes || u32(entry + 12) > 1 ||
            (u32(entry + 12) && ((value & 1) || value >= info->image_bytes))) {
            rc = NANO_MODULE_FORMAT; goto fail;
        }
        previous_hash = u64(entry);
    }
    memset(image, 0, info->memory_bytes);
    for (at = 0; at < info->image_bytes; at += n) {
        n = info->image_bytes - at;
        if (n > 65536) n = 65536;
        if (read(io, image_off + at, image + at, n)) {
            rc = NANO_MODULE_IO; goto fail;
        }
    }
    for (i = 0; i < nr; i++) {
        if (read(io, reloc_off + i * 20, entry, 20)) {
            rc = NANO_MODULE_IO; goto fail;
        }
        off = u32(entry); kind = u32(entry + 4);
        source = u32(entry + 8); target = u32(entry + 12);
        if (info->image_bytes < 4 || off > info->image_bytes - 4 ||
            (i && off < previous + 4) || (off & 1) ||
            (kind == ABS32 && (off & 3)) || source > 2) {
            rc = NANO_MODULE_FORMAT; goto fail;
        }
        previous = off;
        if (source == 2) {
            if (target >= ni || !resolve ||
                read(io, HEADER_BYTES + target * 8, hash, 8)) {
                rc = NANO_MODULE_IMPORT; goto fail;
            }
            symbol.address = symbol.thumb = 0;
            if (resolve(imports, u64(hash), &symbol)) {
                rc = NANO_MODULE_IMPORT; goto fail;
            }
        } else {
            if (target > info->memory_bytes ||
                (source && ((target & 1) || target >= info->image_bytes))) {
                rc = NANO_MODULE_FORMAT; goto fail;
            }
            symbol.address = base + target;
            symbol.thumb = source;
        }
        rc = relocate(image, base, off, kind, symbol, (int32_t)u32(entry + 16));
        if (rc) goto fail;
    }
    if (!sync || sync(sync_ctx, memory, info->memory_bytes)) {
        rc = NANO_MODULE_SYNC; goto fail;
    }
    info->ready = 1;
    return NANO_MODULE_OK;
fail:
    memset(image, 0, info->memory_bytes);
    info->ready = 0;
    return rc;
}

int nano_module_export(nano_module_read read, void *io,
                       const struct nano_module_info *info, uint32_t base,
                       uint64_t hash, struct nano_module_symbol *out)
{
    uint8_t entry[16];
    uint32_t lo = 0, hi, mid;
    uint64_t got;
    if (!info || !info->ready || !out || !read ||
        base > UINT32_MAX - info->memory_bytes) return NANO_MODULE_FORMAT;
    hi = info->exports;
    while (lo < hi) {
        mid = lo + (hi - lo) / 2;
        if (read(io, info->export_offset + mid * 16, entry, 16))
            return NANO_MODULE_IO;
        got = u64(entry);
        if (got < hash) lo = mid + 1;
        else if (got > hash) hi = mid;
        else {
            if (u32(entry + 8) > info->memory_bytes || u32(entry + 12) > 1 ||
                (u32(entry + 12) && ((u32(entry + 8) & 1) ||
                                      u32(entry + 8) >= info->image_bytes)))
                return NANO_MODULE_FORMAT;
            out->address = base + u32(entry + 8);
            out->thumb = u32(entry + 12);
            return NANO_MODULE_OK;
        }
    }
    return NANO_MODULE_IMPORT;
}
