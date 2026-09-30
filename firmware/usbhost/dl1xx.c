/* DL-1xx software encoder. GPL-2.0-or-later.
 * Independently implemented from public protocol notes. See
 * docs/desktop-mode-phase1.md for source links and license provenance. */
#include "dl1xx.h"

bool dl1xx_edid(const uint8_t *p, size_t n, struct dl1xx_mode *out)
{
    const uint8_t header[] = {0,255,255,255,255,255,255,0};
    uint8_t sum = 0;
    if (!p || !out || n < 128) return false;
    for (unsigned i = 0; i < 8; i++) if (p[i] != header[i]) return false;
    for (unsigned i = 0; i < 128; i++) sum += p[i];
    if (sum || p[18] != 1 || p[19] > 4) return false;
    for (unsigned offset = 54; offset <= 108; offset += 18)
    {
        const uint8_t *d = p + offset;
        struct dl1xx_mode m = {0};
        m.clock_khz = (d[0] | (uint32_t)d[1] << 8) * 10;
        if (!m.clock_khz || (d[17] & 0x80) || (d[17] & 0x18) != 0x18)
            continue;
        m.width = d[2] | (d[4] & 0xf0) << 4;
        m.hblank = d[3] | (d[4] & 15) << 8;
        m.height = d[5] | (d[7] & 0xf0) << 4;
        m.vblank = d[6] | (d[7] & 15) << 8;
        m.hfront = d[8] | (d[11] & 0xc0) << 2;
        m.hsync = d[9] | (d[11] & 0x30) << 4;
        m.vfront = (d[10] >> 4) | (d[11] & 12) << 2;
        m.vsync = (d[10] & 15) | (d[11] & 3) << 4;
        m.hpositive = (d[17] & 2) != 0;
        m.vpositive = (d[17] & 4) != 0;
        if (!m.width || !m.height || !m.hsync || !m.vsync ||
            m.width > 2048 || m.height > 1152 ||
            m.hfront + m.hsync >= m.hblank ||
            m.vfront + m.vsync >= m.vblank) continue;
        *out = m;
        return true;
    }
    return false;
}

/* Measure first, then emit. This makes buffer failure transactional. */
static size_t encode_payload(uint8_t *out, const uint16_t *p, size_t n, bool rle)
{
    size_t pos = 0, used = 0;
    while (pos < n)
    {
        size_t start = pos++;
        while (pos < n && (!rle || p[pos] != p[pos - 1])) pos++;
        size_t literals = pos - start;
        if (out) out[used] = (uint8_t)literals;
        used++;
        for (size_t i = start; i < pos; i++)
        {
            if (out) { out[used] = p[i] >> 8; out[used + 1] = p[i]; }
            used += 2;
        }
        if (pos < n)
        {
            size_t repeat = 0;
            while (pos < n && p[pos] == p[pos - 1]) { repeat++; pos++; }
            if (out) out[used] = repeat;
            used++;
        }
    }
    return used;
}

size_t dl1xx_encode(uint8_t *out, size_t capacity, uint32_t address,
                     const uint16_t *pixels, size_t count, bool rle)
{
    if (!out || !pixels || !count || count > 256 || (address & 1) ||
        address > 0x1000000u - count * 2) return 0;
    size_t size = 6 + encode_payload(NULL, pixels, count, rle);
    if (size > capacity) return 0;
    out[0] = 0xaf; out[1] = 0x6b;
    out[2] = address >> 16; out[3] = address >> 8; out[4] = address;
    out[5] = (uint8_t)count;
    encode_payload(out + 6, pixels, count, rle);
    return size;
}

int dl1xx_command_acquire(struct dl1xx_commands *c)
{
    for (int i = 0; i < 2; i++)
        if (!c->busy[i]) { c->used[i] = 0; return i; }
    return -1;
}
bool dl1xx_command_submit(struct dl1xx_commands *c, unsigned i)
{
    if (i >= 2 || c->busy[i] || !c->used[i] ||
        c->used[i] > DL1XX_COMMAND_BYTES - 2) return false;
    c->data[i][c->used[i]++] = 0xaf;
    c->data[i][c->used[i]++] = 0xa0;
    c->busy[i] = true;
    return true;
}
void dl1xx_command_complete(struct dl1xx_commands *c, unsigned i)
{
    if (i < 2) { c->busy[i] = false; c->used[i] = 0; }
}
