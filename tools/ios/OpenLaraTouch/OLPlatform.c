#include "OLPlatform.h"

struct ol_icade_pair {
    uint16_t down_character;
    uint16_t up_character;
    uint32_t key;
};

static const struct ol_icade_pair ol_icade_map[] = {
    {'w', 'e', OL_KEY_UP},
    {'x', 'z', OL_KEY_DOWN},
    {'a', 'q', OL_KEY_LEFT},
    {'d', 'c', OL_KEY_RIGHT},
    {'y', 't', OL_KEY_A},
    {'h', 'r', OL_KEY_B},
    {'u', 'f', OL_KEY_C},
    {'j', 'n', OL_KEY_X},
    {'i', 'm', OL_KEY_Y},
    {'k', 'p', OL_KEY_START},
    {'o', 'g', OL_KEY_Z},
    {'l', 'v', OL_KEY_SELECT}
};

uint32_t ol_icade_update(uint32_t state, uint16_t character,
                         int *recognized)
{
    unsigned int i;

    if (recognized)
        *recognized = 0;
    for (i = 0; i < sizeof(ol_icade_map) / sizeof(ol_icade_map[0]); ++i)
    {
        if (character == ol_icade_map[i].down_character)
        {
            if (recognized)
                *recognized = 1;
            return state | ol_icade_map[i].key;
        }
        if (character == ol_icade_map[i].up_character)
        {
            if (recognized)
                *recognized = 1;
            return state & ~ol_icade_map[i].key;
        }
    }
    return state;
}

uint32_t ol_touch_keys(float x, float y, float width, float height)
{
    static const struct {
        float x;
        float bottom_offset;
        uint32_t key;
    } buttons[] = {
        {410.0f, 68.0f, OL_KEY_A},
        {352.0f, 43.0f, OL_KEY_B},
        {442.0f, 126.0f, OL_KEY_C},
        {300.0f, 101.0f, OL_KEY_X},
        {361.0f, 145.0f, OL_KEY_Y},
        {244.0f, 40.0f, OL_KEY_Z}
    };
    unsigned int i;

    if (x < 180.0f && y > height - 185.0f)
    {
        float dx = x - 83.0f;
        float dy = y - (height - 82.0f);
        uint32_t result = 0;

        if (dx < -14.0f) result |= OL_KEY_LEFT;
        if (dx > 14.0f) result |= OL_KEY_RIGHT;
        if (dy < -14.0f) result |= OL_KEY_UP;
        if (dy > 14.0f) result |= OL_KEY_DOWN;
        return result;
    }
    for (i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i)
    {
        float dx = x - buttons[i].x;
        float dy = y - (height - buttons[i].bottom_offset);

        if (dx * dx + dy * dy < 29.0f * 29.0f)
            return buttons[i].key;
    }
    if (x > width - 66.0f && y < 50.0f)
        return OL_KEY_SELECT;
    return 0;
}

void ol_palette_rgba(uint16_t color, uint8_t rgba[4])
{
    rgba[0] = (uint8_t)((color & 31) * 255 / 31);
    rgba[1] = (uint8_t)(((color >> 5) & 31) * 255 / 31);
    rgba[2] = (uint8_t)(((color >> 10) & 31) * 255 / 31);
    rgba[3] = 255;
}

static uint16_t ol_read_u16(const uint8_t *data)
{
    return (uint16_t)(data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t ol_read_u32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

int ol_validate_pkd_header(const uint8_t *header, size_t header_size,
                           size_t file_size)
{
    uint16_t counts[14];
    unsigned int i;

    if (!header || header_size < 172 || file_size < 172 ||
        file_size > 8u * 1024u * 1024u)
        return 0;

    for (i = 0; i < 14; ++i)
        counts[i] = ol_read_u16(header + 4 + i * 2);
    if (!counts[0] || !counts[1] || counts[1] > 139 ||
        counts[8] > 1536 || counts[9] > 180 || counts[10] > 256 ||
        counts[11] > 16)
        return 0;

    for (i = 0; i < 35; ++i)
    {
        if (ol_read_u32(header + 32 + i * 4) >= file_size)
            return 0;
    }
    if ((size_t)counts[1] * 56u >
        file_size - ol_read_u32(header + 32 + 3 * 4))
        return 0;
    return 1;
}
