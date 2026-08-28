#include "OLPlatform.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_icade(void)
{
    static const struct {
        uint16_t down_character;
        uint16_t up_character;
        uint32_t key;
    } pairs[] = {
        {'w', 'e', OL_KEY_UP}, {'x', 'z', OL_KEY_DOWN},
        {'a', 'q', OL_KEY_LEFT}, {'d', 'c', OL_KEY_RIGHT},
        {'y', 't', OL_KEY_A}, {'h', 'r', OL_KEY_B},
        {'u', 'f', OL_KEY_C}, {'j', 'n', OL_KEY_X},
        {'i', 'm', OL_KEY_Y}, {'k', 'p', OL_KEY_START},
        {'o', 'g', OL_KEY_Z}, {'l', 'v', OL_KEY_SELECT}
    };
    uint32_t state = 0;
    unsigned int i;
    int recognized;

    for (i = 0; i < sizeof(pairs) / sizeof(pairs[0]); ++i)
    {
        state = ol_icade_update(0, pairs[i].down_character, &recognized);
        assert(recognized && state == pairs[i].key);
        state = ol_icade_update(state, pairs[i].up_character, &recognized);
        assert(recognized && state == 0);
    }

    state = ol_icade_update(0, 'w', &recognized);
    state = ol_icade_update(state, 'y', &recognized);
    assert(state == (OL_KEY_UP | OL_KEY_A));
    state = ol_icade_update(state, 'e', &recognized);
    assert(state == OL_KEY_A);
    state = ol_icade_update(state, '?', &recognized);
    assert(!recognized && state == OL_KEY_A);
}

static void test_touch(void)
{
    const float width = 480.0f;
    const float height = 320.0f;

    assert(ol_touch_keys(83, 238, width, height) == 0);
    assert(ol_touch_keys(50, 238, width, height) == OL_KEY_LEFT);
    assert(ol_touch_keys(116, 238, width, height) == OL_KEY_RIGHT);
    assert(ol_touch_keys(83, 200, width, height) == OL_KEY_UP);
    assert(ol_touch_keys(83, 276, width, height) == OL_KEY_DOWN);
    assert(ol_touch_keys(50, 200, width, height) ==
           (OL_KEY_LEFT | OL_KEY_UP));
    assert(ol_touch_keys(410, 252, width, height) == OL_KEY_A);
    assert(ol_touch_keys(352, 277, width, height) == OL_KEY_B);
    assert(ol_touch_keys(442, 194, width, height) == OL_KEY_C);
    assert(ol_touch_keys(300, 219, width, height) == OL_KEY_X);
    assert(ol_touch_keys(361, 175, width, height) == OL_KEY_Y);
    assert(ol_touch_keys(244, 280, width, height) == OL_KEY_Z);
    assert(ol_touch_keys(450, 25, width, height) == OL_KEY_SELECT);
    assert(ol_touch_keys(240, 100, width, height) == 0);
}

static void test_palette(void)
{
    uint8_t rgba[4];

    ol_palette_rgba(0, rgba);
    assert(rgba[0] == 0 && rgba[1] == 0 && rgba[2] == 0 && rgba[3] == 255);
    ol_palette_rgba(31, rgba);
    assert(rgba[0] == 255 && rgba[1] == 0 && rgba[2] == 0);
    ol_palette_rgba(31 << 5, rgba);
    assert(rgba[0] == 0 && rgba[1] == 255 && rgba[2] == 0);
    ol_palette_rgba(31 << 10, rgba);
    assert(rgba[0] == 0 && rgba[1] == 0 && rgba[2] == 255);
}

static void put_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static void test_pkd_header(void)
{
    uint8_t header[172];
    unsigned int i;

    memset(header, 0, sizeof(header));
    put_u16(header + 4, 1);
    put_u16(header + 6, 1);
    for (i = 0; i < 35; ++i)
        put_u32(header + 32 + i * 4, 172);
    assert(ol_validate_pkd_header(header, sizeof(header), 228));
    assert(!ol_validate_pkd_header(NULL, sizeof(header), 228));
    assert(!ol_validate_pkd_header(header, sizeof(header) - 1, 228));
    assert(!ol_validate_pkd_header(header, sizeof(header), 172));

    put_u16(header + 6, 140);
    assert(!ol_validate_pkd_header(header, sizeof(header), 10000));
    put_u16(header + 6, 1);
    put_u32(header + 32 + 3 * 4, 200);
    assert(!ol_validate_pkd_header(header, sizeof(header), 228));
}

int main(void)
{
    test_icade();
    test_touch();
    test_palette();
    test_pkd_header();
    puts("OpenLara Touch platform tests: PASS");
    return 0;
}
