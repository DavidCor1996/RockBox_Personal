/* DL-1xx software encoder. GPL-2.0-or-later. */
#ifndef DL1XX_H
#define DL1XX_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
struct dl1xx_mode
{
    uint16_t width, height, hblank, vblank, hfront, hsync, vfront, vsync;
    uint32_t clock_khz;
    bool hpositive, vpositive;
};
/* Base EDID only; checksum, descriptor bounds and timing sanity required.
 * The first valid progressive detailed timing is returned. No guessed mode. */
bool dl1xx_edid(const uint8_t *data, size_t length, struct dl1xx_mode *out);
/* One command, 1..256 pixels, RGB565 host values -> big-endian DL wire.
 * Returns 0 without writing when the buffer is too small/arguments invalid. */
size_t dl1xx_encode(uint8_t *out, size_t capacity, uint32_t address,
                     const uint16_t *pixels, size_t count, bool rle);
#define DL1XX_COMMAND_BYTES 16384
/* Two buffers, explicit ownership. Never reuse a submitted buffer until the
 * transport completion returns it. On error, caller re-damages the region. */
struct dl1xx_commands
{
    uint8_t data[2][DL1XX_COMMAND_BYTES];
    size_t used[2];
    bool busy[2];
};
int dl1xx_command_acquire(struct dl1xx_commands *commands);
bool dl1xx_command_submit(struct dl1xx_commands *commands, unsigned index);
void dl1xx_command_complete(struct dl1xx_commands *commands, unsigned index);
#endif
