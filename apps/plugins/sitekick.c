/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__ /  \
 *                     \/            \/     \/    \/            \/
 *
 * Sitekick offline iPod port
 *
 * Customise an offline Sitekick from chips preserved by the Sitekick
 * Remastered project.  Trading, chip-of-the-week drops and redemption codes
 * are settled host-side by RockPod and arrive through sync/inbox.v1.tsv.
 *
 * Copyright (C) 2026 David Cor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 ****************************************************************************/

#include "plugin.h"
#include "lib/pluginlib_actions.h"
#include "lib/pluginlib_bmp.h"

static const struct button_mapping *plugin_contexts[] = { pla_main_ctx };

enum sk_preset_name_action
{
    SK_ACTION_PRESET_SAVE = LAST_PLUGINLIB_ACTION + 1,
    SK_ACTION_PRESET_CANCEL
};

static const struct button_mapping sk_preset_name_ctx[] =
{
    { SK_ACTION_PRESET_SAVE, BUTTON_PLAY, BUTTON_NONE },
    { SK_ACTION_PRESET_CANCEL, BUTTON_MENU, BUTTON_NONE },
    LAST_ITEM_IN_LIST__NEXTLIST(CONTEXT_PLUGIN),
};

static const struct button_mapping *sk_preset_name_contexts[] =
{
    sk_preset_name_ctx, pla_main_ctx
};

#define SK_ROOT         ROCKBOX_DIR "/sitekick"
#define SK_BASE_DIR     SK_ROOT "/base"
#define SK_CHIP_DIR     SK_ROOT "/chips"
#define SK_ICON_DIR     SK_ROOT "/icons"
#define SK_DATA_DIR     SK_ROOT "/data"
#define SK_SOUND_DIR    SK_ROOT "/sounds"
#define SK_BACKGROUND_DIR SK_ROOT "/backgrounds"
#define SK_SYNC_DIR     SK_ROOT "/sync"
#define SK_PREVIEW_DIR  SK_ROOT "/preview"

#define SK_CHIPS_FILE   SK_DATA_DIR "/chips.v1.tsv"
#define SK_SAVE_FILE    SK_ROOT "/state/save.v1.dat"
#define SK_SAVE_TMP     SK_ROOT "/state/save.v1.tmp"
#define SK_INBOX_FILE   SK_SYNC_DIR "/inbox.v1.tsv"
#define SK_OUTBOX_FILE  SK_SYNC_DIR "/outbox.v1.tsv"
#define SK_PREVIEW_FLOAT SK_PREVIEW_DIR "/current-float.bmp"
#define SK_PREVIEW_FLOAT_TMP SK_PREVIEW_DIR "/current-float.tmp"
#define SK_PREVIEW_PANE SK_PREVIEW_DIR "/pane-background.bmp"
#define SK_PREVIEW_PANE_TMP SK_PREVIEW_DIR "/pane-background.tmp"

#if LCD_WIDTH >= 1920
#define SK_DM_UNDERLAY_FILE \
    PLUGIN_APPS_DATA_DIR "/desktop_mode_sitekick_underlay.raw"
#define SK_DM_UNDERLAY_MAGIC 0x44534b31u /* "DSK1" */
#define SK_DM_WINDOW_FILE \
    PLUGIN_APPS_DATA_DIR \
    "/desktop_mode_snow_leopard/1920x1080/chrome/" \
    "window-plain.897x671x16.bmp"
#define SK_DM_WIN_W 897
#define SK_DM_WIN_H 671
#define SK_DM_WIN_X ((LCD_WIDTH - SK_DM_WIN_W) / 2)
#define SK_DM_WIN_Y 170
#define SK_DM_TITLE_H 24
#define SK_DM_TOOLBAR_H 29
#define SK_DM_STATUS_H 24
#define SK_DM_BODY_X (SK_DM_WIN_X + 1)
#define SK_DM_BODY_Y (SK_DM_WIN_Y + SK_DM_TITLE_H + SK_DM_TOOLBAR_H)
#define SK_DM_BODY_W (SK_DM_WIN_W - 2)
#define SK_DM_BODY_H \
    (SK_DM_WIN_H - SK_DM_TITLE_H - SK_DM_TOOLBAR_H - SK_DM_STATUS_H)
#define SK_DM_HEADER_H 48
#define SK_DM_TABS_H 42
#define SK_DM_CONTENT_Y (SK_DM_BODY_Y + SK_DM_HEADER_H)
#define SK_DM_CONTENT_H (SK_DM_BODY_H - SK_DM_HEADER_H - SK_DM_TABS_H)
#define SK_DM_TAB_Y (SK_DM_CONTENT_Y + SK_DM_CONTENT_H)
#define SK_DM_STAGE_SCALE 2
#define SK_DM_STAGE_W (SK_STAGE_W * SK_DM_STAGE_SCALE)
#define SK_DM_STAGE_H (SK_STAGE_H * SK_DM_STAGE_SCALE)
#endif

/* Screen furniture, sized for the 320x240 iPod panel. */
#define SK_TITLE_H      30
#define SK_STATUS_H     18
#define SK_CONTENT_Y    SK_TITLE_H
#define SK_CONTENT_H    (LCD_HEIGHT - SK_TITLE_H - SK_STATUS_H)
#define SK_LIST_W       LCD_WIDTH
#define SK_ROW_H        32
#define SK_ROWS_MAX     ((SK_CONTENT_H) / SK_ROW_H)

#define SK_STAGE_W      240
#define SK_STAGE_H      192
#define SK_PREVIEW_W    154
#define SK_PREVIEW_H    139
#define SK_PREVIEW_INNER_W 148
#define SK_PREVIEW_INNER_H 129
#define SK_BODY_ORIGIN_X 120
#define SK_BODY_ORIGIN_Y 70
#define SK_ICON_PX      32
#define SK_ICONS_PER_ROW 5
#define SK_ICON_ROWS    10
#define SK_ICONS_PER_PAGE (SK_ICONS_PER_ROW * SK_ICON_ROWS)

#define SK_MAX_CHIPS    1024
#define SK_NAME_MAX     20
#define SK_TEXT_BUF     4096

/* The original CHIPS screen supplied eight equip positions.  A chip may be
 * a whole costume with arms and legs, so these positions are deliberately
 * not body-part categories.  The catalogue's recovered category remains
 * useful for collection browsing, but never restricts what can be worn. */
enum sk_slot
{
    SK_SLOT_AURA = 0,
    SK_SLOT_SHELL,
    SK_SLOT_ARMS,
    SK_SLOT_FACE,
    SK_SLOT_EYES,
    SK_SLOT_HAIR,
    SK_SLOT_ANTENNA,
    SK_SLOT_ACCESSORY,
    SK_SLOT_COUNT,
    SK_SLOT_NONE = SK_SLOT_COUNT
};

enum sk_rarity
{
    SK_RARITY_COMMON = 0,
    SK_RARITY_RARE,
    SK_RARITY_LEGENDARY,
    SK_RARITY_COUNT
};

enum sk_scene
{
    SK_SCENE_WORKSHOP = 0,
    SK_SCENE_SLOT,
    SK_SCENE_COLLECTION,
    SK_SCENE_DUMP,
    SK_SCENE_COLOR,
    SK_SCENE_BACKGROUND,
    SK_SCENE_GAMES,
    SK_SCENE_STATS,
    SK_SCENE_INBOX,
    SK_SCENE_SHOP,
    SK_SCENE_PRESETS
};

#define SK_WORKSHOP_PRESETS      (SK_SLOT_COUNT + 8)
#define SK_WORKSHOP_SAVE_PRESET  (SK_SLOT_COUNT + 9)
#define SK_WORKSHOP_CLEAR         (SK_SLOT_COUNT + 10)
#define SK_WORKSHOP_ITEMS         (SK_SLOT_COUNT + 11)

struct sk_chip
{
    uint16_t id;
    uint8_t slot;
    int8_t z;
    int16_t ax;
    int16_t ay;
    uint16_t w;
    uint16_t h;
    uint8_t page;
    uint8_t index;
    uint8_t rarity;
    uint8_t worn;
    char name[SK_NAME_MAX];
};

static const char * const sk_slot_names[SK_SLOT_COUNT] =
{
    "aura", "shell", "arms", "face", "eyes", "hair", "antenna", "accessory"
};

static const char * const sk_slot_labels[SK_SLOT_COUNT] =
{
    "Equip 1", "Equip 2", "Equip 3", "Equip 4",
    "Equip 5", "Equip 6", "Equip 7", "Equip 8"
};

static const char * const sk_rarity_labels[SK_RARITY_COUNT] =
{
    "Common", "Rare", "Legendary"
};

/* --- asset buffers ------------------------------------------------------
 * Everything decorative is static so the plugin never asks core_alloc() for
 * memory that playback is using.  See docs/ipodjs-ui-memory-animation-
 * steering.md.
 */
#define SK_ICON_BYTES   (160 * 1024)
#define SK_BITMAP_BYTES (160 * 1024)
#define SK_LOGO_BYTES   (8 * 1024)
#define SK_SOUND_BYTES  (128 * 1024)

static unsigned char sk_icon_data[SK_ICON_BYTES] CACHEALIGN_ATTR;
static unsigned char sk_bitmap_data[SK_BITMAP_BYTES] CACHEALIGN_ATTR;
static unsigned char sk_logo_data[SK_LOGO_BYTES] CACHEALIGN_ATTR;
static fb_data sk_stage_data[LCD_NBELEMS(SK_STAGE_W, SK_STAGE_H)]
    CACHEALIGN_ATTR;
static fb_data sk_stage_background_data[
    LCD_NBELEMS(SK_STAGE_W, SK_STAGE_H)] CACHEALIGN_ATTR;
/* 92,160 bytes on the 16-bit iPod LCD. This fixed buffer is the transparent
 * paperdoll layer used by the live workshop animation. It never comes from
 * core_alloc(), so playback memory cannot be shrunk or reclaimed for it. */
static fb_data sk_float_data[LCD_NBELEMS(SK_STAGE_W, SK_STAGE_H)]
    CACHEALIGN_ATTR;
static fb_data sk_preview_data[
    LCD_NBELEMS(SK_PREVIEW_W, SK_PREVIEW_H)] CACHEALIGN_ATTR;

static struct bitmap sk_bitmap;
static bool sk_body_loaded;
static int sk_body_w, sk_body_h;
static bool sk_stage_loaded;
static struct frame_buffer_t sk_stage_fb;
static struct viewport sk_stage_vp;
static struct frame_buffer_t sk_pane_fb;
static struct viewport sk_pane_vp;

static struct bitmap sk_icon_page;
static int sk_icon_page_loaded = -1;
static struct bitmap sk_logo;
static bool sk_logo_loaded;

static struct sk_chip sk_chips[SK_MAX_CHIPS];
static int sk_chip_count;
static bool sk_chips_truncated;

/* --- save state --------------------------------------------------------- */
#define SK_OWNED_WORDS  ((SK_MAX_CHIPS + 31) / 32)
#define SK_SAVE1_MAGIC  "SKS1"
#define SK_SAVE2_MAGIC  "SKS2"
#define SK_SAVE3_MAGIC  "SKS3"
#define SK_SAVE1_BYTES  (16 + SK_OWNED_WORDS * 4 + SK_SLOT_COUNT * 2)
#define SK_SAVE2_BASE_HEADER (16 + SK_SLOT_COUNT * 2)
#define SK_SAVE2_DUMP_HEADER (SK_SAVE2_BASE_HEADER + 8)
#define SK_SAVE2_HEADER (SK_SAVE2_DUMP_HEADER + 8)
#define SK_SHOP_SLOTS   6
#define SK_SAVE2_SHOP_HEADER (SK_SAVE2_HEADER + 4 + SK_SHOP_SLOTS * 2)
#define SK_SAVE2_BYTES  (SK_SAVE2_SHOP_HEADER + SK_MAX_CHIPS * 2)
#define SK_PRESET_COUNT 8
#define SK_PRESET_NAME_MAX 21
#define SK_PRESET_BYTES (3 + SK_PRESET_NAME_MAX + SK_SLOT_COUNT * 2)
#define SK_SAVE3_HEADER (SK_SAVE2_SHOP_HEADER + 2 + \
                         SK_PRESET_COUNT * SK_PRESET_BYTES)
#define SK_SAVE3_BYTES  (SK_SAVE3_HEADER + SK_MAX_CHIPS * 2)
#define SK_NO_CHIP_ID   0xffff
#define SK_SHOP_PERIOD_SEC (2 * 60 * 60)

static uint32_t sk_owned[SK_OWNED_WORDS];
static int16_t sk_equipped[SK_SLOT_COUNT];   /* chip index, -1 = empty */
static uint16_t sk_owned_ids[SK_MAX_CHIPS];
static unsigned char sk_save_data[SK_SAVE3_BYTES];
static uint32_t sk_xp;
static uint32_t sk_coins;
static bool sk_dirty;
static int16_t sk_dump_index = -1;
static uint32_t sk_dump_ready_at;
static uint8_t sk_body_color;
static uint8_t sk_background;
static int16_t sk_shop_stock[SK_SHOP_SLOTS];   /* chip index, -1 = sold */
static uint32_t sk_shop_ready_at;
static const uint32_t sk_shop_prices[SK_RARITY_COUNT] = { 60, 180, 500 };

struct sk_preset
{
    bool active;
    uint8_t body_color;
    uint8_t background;
    uint16_t equipped_ids[SK_SLOT_COUNT];
    char name[SK_PRESET_NAME_MAX];
};

static struct sk_preset sk_presets[SK_PRESET_COUNT];
static int sk_preset_index = -1;

/* --- ui state ----------------------------------------------------------- */
static enum sk_scene sk_scene = SK_SCENE_WORKSHOP;
static int sk_sel;                  /* selection in the active list */
static int sk_top;                  /* first visible row */
static int sk_slot_filter = SK_SLOT_AURA;
static char sk_message[64];
static long sk_message_until;
static char sk_text[SK_TEXT_BUF];
static bool sk_game_usb_connected;
static int sk_game_icons[3] = { -1, -1, -1 };

static bool sk_sounds_on = true;
static bool sk_sounds_over_music = true;

static bool sk_preset_name_input(char *name, size_t size);
static void sk_draw_workshop(void);
static void sk_center_text(int y, const char *text, unsigned color);
static void sk_clear_loadout(void);

#if LCD_WIDTH >= 1920
static bool sk_desktop_mode;
static fb_data *sk_dm_underlay;
static fb_data *sk_dm_window;
static fb_data *sk_dm_stage_background;
static fb_data *sk_dm_stage_float;
static unsigned char *sk_dm_arena;
static size_t sk_dm_arena_left;
static int sk_dm_pointer_x = SK_DM_WIN_X + SK_DM_WIN_W / 2;
static int sk_dm_pointer_y = SK_DM_WIN_Y + SK_DM_WIN_H / 2;
static unsigned int sk_dm_pointer_buttons;
static long sk_dm_pointer_poll_tick;
static bool sk_dm_pointer_active;
static int sk_dm_drag_chip = -1;
static int sk_dm_drag_source_slot = -1;
#endif

/* Period YTV palette: purple chrome, ooze green, hot orange and yellow. */
#define SK_COL_TITLE_TOP    LCD_RGBPACK(0x7d, 0x28, 0x9d)
#define SK_COL_TITLE_BOT    LCD_RGBPACK(0x3b, 0x08, 0x5b)
#define SK_COL_SEL_TOP      LCD_RGBPACK(0xff, 0xd5, 0x24)
#define SK_COL_SEL_BOT      LCD_RGBPACK(0xff, 0x91, 0x16)
#define SK_COL_SEL_TEXT     LCD_RGBPACK(0x20, 0x04, 0x2d)
#define SK_COL_TEXT         LCD_RGBPACK(0x2a, 0x08, 0x37)
#define SK_COL_DIM          LCD_RGBPACK(0x79, 0x62, 0x80)
#define SK_COL_WHITE        LCD_RGBPACK(0xff, 0xff, 0xff)
#define SK_COL_YELLOW       LCD_RGBPACK(0xff, 0xd8, 0x1f)
#define SK_COL_ORANGE       LCD_RGBPACK(0xff, 0x70, 0x0b)
#define SK_COL_BG           LCD_RGBPACK(0xf7, 0xf0, 0xf9)
#define SK_COL_PANE         LCD_RGBPACK(0xb9, 0xdd, 0x42)
#define SK_COL_STATUS_TOP   LCD_RGBPACK(0xb9, 0xdd, 0x42)
#define SK_COL_STATUS_BOT   LCD_RGBPACK(0x79, 0xb8, 0x23)
#define SK_COL_RULE         LCD_RGBPACK(0x4c, 0x0b, 0x64)

#define SK_BODY_COLOR_COUNT 7
#define SK_BACKGROUND_COUNT 19

static const char * const sk_body_color_names[SK_BODY_COLOR_COUNT] =
{
    "Classic", "Slime", "YTV Purple", "Ooze Orange", "Aqua", "Hot Pink",
    "Classic Yellow"
};

static const char * const sk_background_names[SK_BACKGROUND_COUNT] =
{
    "Sitekick Splash", "Butterfly", "Purple Gear",
    "Blue Gear", "Aqua Leaf", "Ooze Grid",
    "Oliver Scrapyard", "Emma Neon Box", "Oliver Alone Crowd",
    "Beatles Crosswalk", "Beatles Pepperland", "Beatles Rooftop",
    "Hasan News Studio", "QTC Spotlight Stage", "Maya Wildlife Perch",
    "Habs Home Ice", "Polaroid Darkroom", "KI Arena Lightning",
    "Cyberpunk Night City"
};

static const unsigned sk_background_colors[SK_BACKGROUND_COUNT] =
{
    LCD_RGBPACK(0xb9, 0xdd, 0x42),
    LCD_RGBPACK(0x7d, 0x28, 0x9d),
    LCD_RGBPACK(0xff, 0x91, 0x16),
    LCD_RGBPACK(0x46, 0xa5, 0xd6),
    LCD_RGBPACK(0x26, 0x1f, 0x36),
    LCD_RGBPACK(0xe1, 0xe8, 0xf0),
    LCD_RGBPACK(0x18, 0xc6, 0xe5),
    LCD_RGBPACK(0x29, 0x16, 0x3e),
    LCD_RGBPACK(0x12, 0x2e, 0x74),
    LCD_RGBPACK(0x54, 0x3e, 0x78),
    LCD_RGBPACK(0x09, 0x83, 0xde),
    LCD_RGBPACK(0xc5, 0x62, 0x28),
    LCD_RGBPACK(0x53, 0x15, 0x27),
    LCD_RGBPACK(0x52, 0x33, 0x4a),
    LCD_RGBPACK(0x70, 0xb0, 0x91),
    LCD_RGBPACK(0xc6, 0xd4, 0xe3),
    LCD_RGBPACK(0x35, 0x1c, 0x17),
    LCD_RGBPACK(0x39, 0x17, 0x21),
    LCD_RGBPACK(0x1c, 0x11, 0x2a),
};

#if LCD_WIDTH >= 1920
static void sk_desktop_refresh_stage(void);
static int sk_slot_chip_count(int slot);
static int sk_slot_chip_at(int slot, int nth);
static void sk_equip(int slot, int chip_index);
static bool sk_handle_select(void);
#endif

/* ---------------------------------------------------------------------- */
/* sound                                                                    */
/* ---------------------------------------------------------------------- */

enum sk_sound
{
    SK_SND_NAVIGATE = 0,
    SK_SND_SELECT,
    SK_SND_BACK,
    SK_SND_REWARD,
    SK_SND_COUNT
};

/* The source clips (original Ooze install button sounds) are mastered
 * hotter than typical music, so unity gain on the BEEP channel made them
 * jump out over whatever the user was listening to. Half amplitude keeps
 * them at a steady, audible-but-polite level whether or not music is
 * playing, instead of spiking above it. */
#define SK_SND_AMPLITUDE (MIX_AMP_UNITY / 2)

#ifndef HAVE_HARDWARE_BEEP
static unsigned char sk_sound_data[SK_SOUND_BYTES] CACHEALIGN_ATTR;
static uint32_t sk_sound_offset[SK_SND_COUNT];
static uint32_t sk_sound_length[SK_SND_COUNT];
static bool sk_sound_loaded;

static uint16_t sk_le16(const unsigned char *data)
{
    return (uint16_t)(data[0] | (data[1] << 8));
}

static uint32_t sk_le32(const unsigned char *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static bool sk_load_sounds(void)
{
    char path[MAX_PATH];
    unsigned int rate = rb->mixer_get_frequency();
    size_t size = 0;
    int fd, got, index;

    if (rate != 44100 && rate != 48000)
        return false;
    rb->snprintf(path, sizeof(path), SK_SOUND_DIR "/ui-%u.uib", rate);
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;
    got = rb->read(fd, sk_sound_data, sizeof(sk_sound_data));
    rb->close(fd);
    if (got <= 0)
        return false;
    size = (size_t)got;

    if (size < 12 + SK_SND_COUNT * 12 ||
        rb->memcmp(sk_sound_data, "UIB1", 4) ||
        sk_le32(sk_sound_data + 4) != rate ||
        sk_le16(sk_sound_data + 8) != SK_SND_COUNT)
        return false;

    for (index = 0; index < SK_SND_COUNT; ++index)
    {
        size_t entry = 12 + index * 12;
        sk_sound_offset[index] = sk_le32(sk_sound_data + entry + 4);
        sk_sound_length[index] = sk_le32(sk_sound_data + entry + 8);
        if (sk_sound_offset[index] + sk_sound_length[index] > size)
            return false;
    }
    sk_sound_loaded = true;
    rb->mixer_channel_set_amplitude(PCM_MIXER_CHAN_BEEP, SK_SND_AMPLITUDE);
    return true;
}

/* Effects go out on the BEEP channel so they mix over whatever the user is
 * listening to; playback keeps PCM_MIXER_CHAN_PLAYBACK to itself. */
static void sk_play(enum sk_sound sound)
{
    if (!sk_sounds_on || !sk_sound_loaded || sound >= SK_SND_COUNT)
        return;
    if (!sk_sounds_over_music && (rb->audio_status() & AUDIO_STATUS_PLAY))
        return;
    if (!sk_sound_length[sound])
        return;
    rb->mixer_channel_stop(PCM_MIXER_CHAN_BEEP);
    rb->mixer_channel_play_data(PCM_MIXER_CHAN_BEEP, NULL,
                                sk_sound_data + sk_sound_offset[sound],
                                sk_sound_length[sound]);
}

static void sk_sound_shutdown(void)
{
    long deadline = *rb->current_tick + HZ / 4;

    rb->mixer_channel_stop(PCM_MIXER_CHAN_BEEP);
    rb->mixer_channel_set_buffer_hook(PCM_MIXER_CHAN_BEEP, NULL);
    while (rb->mixer_channel_status(PCM_MIXER_CHAN_BEEP) != CHANNEL_STOPPED &&
           TIME_BEFORE(*rb->current_tick, deadline))
        rb->sleep(1);
}
#else
static bool sk_load_sounds(void)
{
    return false;
}

static void sk_play(enum sk_sound sound)
{
    (void)sound;
}

static void sk_sound_shutdown(void)
{
}
#endif

/* ---------------------------------------------------------------------- */
/* helpers                                                                  */
/* ---------------------------------------------------------------------- */

static void sk_set_message(const char *text)
{
    rb->strlcpy(sk_message, text, sizeof(sk_message));
    sk_message_until = *rb->current_tick + 2 * HZ;
}

static bool sk_owned_get(int index)
{
    if (index < 0 || index >= SK_MAX_CHIPS)
        return false;
    return (sk_owned[index >> 5] & (1u << (index & 31))) != 0;
}

static void sk_owned_set(int index, bool value)
{
    if (index < 0 || index >= SK_MAX_CHIPS)
        return;
    if (value)
        sk_owned[index >> 5] |= 1u << (index & 31);
    else
        sk_owned[index >> 5] &= ~(1u << (index & 31));
}

static int sk_owned_count(void)
{
    int index, total = 0;

    for (index = 0; index < sk_chip_count; ++index)
        if (sk_owned_get(index))
            total++;
    return total;
}

static int sk_find_chip(int id)
{
    int index;

    for (index = 0; index < sk_chip_count; ++index)
        if (sk_chips[index].id == id)
            return index;
    return -1;
}

static int sk_slot_from_name(const char *name)
{
    int index;

    for (index = 0; index < SK_SLOT_COUNT; ++index)
        if (!rb->strcmp(name, sk_slot_names[index]))
            return index;
    return SK_SLOT_NONE;
}

static int sk_rarity_from_name(const char *name)
{
    if (!rb->strcmp(name, "legendary"))
        return SK_RARITY_LEGENDARY;
    if (!rb->strcmp(name, "rare"))
        return SK_RARITY_RARE;
    return SK_RARITY_COMMON;
}

static int sk_parse_int(const char *text)
{
    int value = 0;
    int sign = 1;

    if (*text == '-')
    {
        sign = -1;
        text++;
    }
    while (*text >= '0' && *text <= '9')
        value = value * 10 + (*text++ - '0');
    return value * sign;
}

/* Split a tab separated line in place.  Returns the field count. */
static int sk_split(char *line, char **fields, int max_fields)
{
    int count = 0;

    while (count < max_fields)
    {
        fields[count++] = line;
        line = rb->strchr(line, '\t');
        if (!line)
            break;
        *line++ = '\0';
    }
    return count;
}

static int sk_read_text_file(const char *path)
{
    int fd = rb->open(path, O_RDONLY);
    int got;

    if (fd < 0)
        return -1;
    got = rb->read(fd, sk_text, sizeof(sk_text) - 1);
    rb->close(fd);
    if (got < 0)
        return -1;
    sk_text[got] = '\0';
    return got;
}

/* ---------------------------------------------------------------------- */
/* asset loading                                                            */
/* ---------------------------------------------------------------------- */

static bool sk_load_bitmap(const char *path, struct bitmap *bm,
                           unsigned char *data, size_t data_size)
{
    int rc;

    rb->memset(bm, 0, sizeof(*bm));
    bm->data = data;
    rc = rb->read_bmp_file(path, bm, (int)data_size,
                           FORMAT_NATIVE | FORMAT_TRANSPARENT, NULL);
    return rc >= 0;
}

static void sk_load_brand(void)
{
    sk_logo_loaded = sk_load_bitmap(SK_BASE_DIR "/ytv-logo.bmp", &sk_logo,
                                    sk_logo_data,
                                    sizeof(sk_logo_data));
}

/* The chip table is streamed a line at a time so a large catalogue never
 * needs the whole file resident. */
static bool sk_load_chips(void)
{
    int fd = rb->open(SK_CHIPS_FILE, O_RDONLY);
    char line[160];
    int pos = 0;
    char chunk[512];
    int got;

    if (fd < 0)
        return false;
    sk_chip_count = 0;
    sk_chips_truncated = false;

    while ((got = rb->read(fd, chunk, sizeof(chunk))) > 0)
    {
        int i;

        for (i = 0; i < got; ++i)
        {
            char ch = chunk[i];

            if (ch != '\n')
            {
                if (ch != '\r' && pos < (int)sizeof(line) - 1)
                    line[pos++] = ch;
                continue;
            }
            line[pos] = '\0';
            pos = 0;

            if (line[0] && line[0] != '#')
            {
                char *f[12];
                int n = sk_split(line, f, 12);

                if (n >= 12 && sk_chip_count < SK_MAX_CHIPS)
                {
                    struct sk_chip *chip = &sk_chips[sk_chip_count];

                    chip->id = (uint16_t)sk_parse_int(f[0]);
                    chip->slot = (uint8_t)sk_slot_from_name(f[1]);
                    chip->z = (int8_t)sk_parse_int(f[2]);
                    chip->ax = (int16_t)sk_parse_int(f[3]);
                    chip->ay = (int16_t)sk_parse_int(f[4]);
                    chip->w = (uint16_t)sk_parse_int(f[5]);
                    chip->h = (uint16_t)sk_parse_int(f[6]);
                    chip->page = (uint8_t)sk_parse_int(f[7]);
                    chip->index = (uint8_t)sk_parse_int(f[8]);
                    chip->rarity = (uint8_t)sk_rarity_from_name(f[9]);
                    chip->worn = (uint8_t)(sk_parse_int(f[10]) != 0);
                    rb->strlcpy(chip->name, f[11], SK_NAME_MAX);
                    sk_chip_count++;
                }
                else if (n >= 12)
                {
                    sk_chips_truncated = true;
                }
            }
        }
    }
    rb->close(fd);
    if (sk_chips_truncated)
    {
        DEBUGF("sitekick: catalogue exceeds SK_MAX_CHIPS=%d\n",
               SK_MAX_CHIPS);
        rb->splashf(3 * HZ, "Sitekick limited to %d chips",
                    SK_MAX_CHIPS);
    }
    return sk_chip_count > 0;
}

static bool sk_load_icon_page(int page)
{
    char path[MAX_PATH];

    if (page < 0)
        return false;
    if (sk_icon_page_loaded == page)
        return true;
    rb->snprintf(path, sizeof(path), SK_ICON_DIR "/page%d.bmp", page);
    if (!sk_load_bitmap(path, &sk_icon_page, sk_icon_data,
                        sizeof(sk_icon_data)))
    {
        sk_icon_page_loaded = -1;
        return false;
    }
    sk_icon_page_loaded = page;
    return true;
}

static void *sk_stage_address(int x, int y)
{
#if LCD_STRIDEFORMAT == VERTICAL_STRIDE
    size_t element = (size_t)x * LCD_NATIVE_STRIDE(sk_stage_fb.stride) + y;
#else
    size_t element = (size_t)y * LCD_NATIVE_STRIDE(sk_stage_fb.stride) + x;
#endif

    return sk_stage_fb.fb_ptr + element % sk_stage_fb.elems;
}

static void *sk_pane_address(int x, int y)
{
#if LCD_STRIDEFORMAT == VERTICAL_STRIDE
    size_t element = (size_t)x * LCD_NATIVE_STRIDE(sk_pane_fb.stride) + y;
#else
    size_t element = (size_t)y * LCD_NATIVE_STRIDE(sk_pane_fb.stride) + x;
#endif

    return sk_pane_fb.fb_ptr + element % sk_pane_fb.elems;
}

static fb_data *sk_stage_buffer_address(fb_data *buffer, int x, int y)
{
#if LCD_STRIDEFORMAT == VERTICAL_STRIDE
    size_t element =
        (size_t)x * LCD_NATIVE_STRIDE(sk_stage_fb.stride) + y;
#else
    size_t element = (size_t)y * LCD_NATIVE_STRIDE(SK_STAGE_W) + x;
#endif

    return buffer + element % LCD_NBELEMS(SK_STAGE_W, SK_STAGE_H);
}

static void sk_stage_draw_chip(int slot)
{
    char path[MAX_PATH];
    const struct sk_chip *chip;
    int index = sk_equipped[slot];

    if (index < 0 || index >= sk_chip_count)
        return;
    chip = &sk_chips[index];
    if (!chip->worn)
        return;

    rb->snprintf(path, sizeof(path), SK_CHIP_DIR "/%04d.bmp", chip->id);
    if (sk_load_bitmap(path, &sk_bitmap, sk_bitmap_data,
                       sizeof(sk_bitmap_data)))
    {
        rb->lcd_bmp_part(&sk_bitmap, 0, 0,
                         SK_BODY_ORIGIN_X + chip->ax,
                         SK_BODY_ORIGIN_Y + chip->ay,
                         sk_bitmap.width, sk_bitmap.height);
    }
}

/* Compose the complete paperdoll into one small framebuffer when the loadout
 * changes. Draw functions only copy these cached pixels and never touch disk. */
static void sk_reload_stage(void)
{
    struct sk_layer
    {
        int slot;
        int z;
    };
    struct sk_layer layers[SK_SLOT_COUNT];
    struct viewport *old_vp;
    char body_path[MAX_PATH];
    char background_path[MAX_PATH];
    unsigned background = sk_background_colors[
        sk_background < SK_BACKGROUND_COUNT ? sk_background : 0];
    int i, layer_count = 0;
    int x, y;

    for (i = 0; i < SK_SLOT_COUNT; ++i)
    {
        int index = sk_equipped[i];
        int pos;
        struct sk_layer layer;

        if (index < 0 || index >= sk_chip_count || !sk_chips[index].worn)
            continue;
        layer.slot = i;
        layer.z = sk_chips[index].z;
        pos = layer_count;
        while (pos > 0 &&
               (layers[pos - 1].z > layer.z ||
                (layers[pos - 1].z == layer.z &&
                 layers[pos - 1].slot > layer.slot)))
        {
            layers[pos] = layers[pos - 1];
            pos--;
        }
        layers[pos] = layer;
        layer_count++;
    }

    rb->memset(&sk_stage_vp, 0, sizeof(sk_stage_vp));
    sk_stage_vp.width = SK_STAGE_W;
    sk_stage_vp.height = SK_STAGE_H;
    sk_stage_vp.font = FONT_UI;
    sk_stage_vp.drawmode = DRMODE_SOLID;
    sk_stage_vp.fg_pattern = background;
    sk_stage_vp.bg_pattern = background;

    sk_stage_fb.data = sk_stage_data;
    sk_stage_fb.elems = LCD_NBELEMS(SK_STAGE_W, SK_STAGE_H);
    sk_stage_fb.stride = STRIDE_MAIN(SK_STAGE_W, SK_STAGE_H);
    sk_stage_fb.get_address_fn = sk_stage_address;
    rb->viewport_set_buffer(&sk_stage_vp, &sk_stage_fb, SCREEN_MAIN);
    old_vp = rb->lcd_set_viewport(&sk_stage_vp);

    rb->lcd_set_foreground(background);
    rb->lcd_fillrect(0, 0, SK_STAGE_W, SK_STAGE_H);
    rb->snprintf(background_path, sizeof(background_path),
                 SK_BACKGROUND_DIR "/stage-%d.bmp", sk_background);
    if (sk_load_bitmap(background_path, &sk_bitmap, sk_bitmap_data,
                       sizeof(sk_bitmap_data)))
        rb->lcd_bmp_part(&sk_bitmap, 0, 0, 0, 0,
                         MIN(SK_STAGE_W, sk_bitmap.width),
                         MIN(SK_STAGE_H, sk_bitmap.height));
    rb->memcpy(sk_stage_background_data, sk_stage_data,
               sizeof(sk_stage_background_data));
    /* Alpha bitmaps need foreground-only mode to blend with pixels already
     * in the stage. SOLID blends against the viewport background colour,
     * producing the white rectangles seen around equipped chips. */
    rb->lcd_set_drawmode(DRMODE_FG);
    for (i = 0; i < layer_count; ++i)
        if (layers[i].z < 0)
            sk_stage_draw_chip(layers[i].slot);

    if (sk_body_color > 0 && sk_body_color < SK_BODY_COLOR_COUNT)
        rb->snprintf(body_path, sizeof(body_path),
                     SK_BASE_DIR "/sitekick-color-%d.bmp", sk_body_color);
    else
        rb->strlcpy(body_path, SK_BASE_DIR "/sitekick.bmp",
                    sizeof(body_path));
    sk_body_loaded = sk_load_bitmap(body_path, &sk_bitmap, sk_bitmap_data,
                                    sizeof(sk_bitmap_data));
    if (!sk_body_loaded && sk_body_color != 0)
        sk_body_loaded = sk_load_bitmap(
            SK_BASE_DIR "/sitekick.bmp", &sk_bitmap, sk_bitmap_data,
            sizeof(sk_bitmap_data));
    if (sk_body_loaded)
    {
        sk_body_w = sk_bitmap.width;
        sk_body_h = sk_bitmap.height;
        rb->lcd_bmp_part(&sk_bitmap, 0, 0, 0, 0,
                         sk_body_w, sk_body_h);
    }
    for (i = 0; i < layer_count; ++i)
        if (layers[i].z >= 0)
            sk_stage_draw_chip(layers[i].slot);

    rb->lcd_set_viewport(old_vp);
    /* Split the already-composed cached stage into a fixed background and a
     * transparent paperdoll. This bounded pass runs only when appearance
     * state changes, never inside an animation frame. */
    for (y = 0; y < SK_STAGE_H; ++y)
    {
        for (x = 0; x < SK_STAGE_W; ++x)
        {
            fb_data stage = *sk_stage_buffer_address(
                sk_stage_data, x, y);
            fb_data background_pixel = *sk_stage_buffer_address(
                sk_stage_background_data, x, y);

            *sk_stage_buffer_address(sk_float_data, x, y) =
                stage == background_pixel ? TRANSPARENT_COLOR : stage;
        }
    }
    sk_stage_loaded = true;
#if LCD_WIDTH >= 1920
    if (sk_desktop_mode)
        sk_desktop_refresh_stage();
#endif
}

/* Publish the cached paperdoll for the Extras/Applications pane. This runs
 * only when state changes (or when the plugin opens), never from a draw or
 * animation callback. The magenta surround is Rockbox's native transparent
 * colour, so the character can float over a real patterned background. */
static bool sk_publish_character_preview(void)
{
    struct bitmap preview;
    int min_x = SK_STAGE_W, min_y = SK_STAGE_H;
    int max_x = -1, max_y = -1;
    int bound_w, bound_h, draw_w, draw_h;
    int offset_x, offset_y;
    int x, y;

    if (!sk_stage_loaded)
        return false;
    for (y = 0; y < SK_STAGE_H; ++y)
    {
        for (x = 0; x < SK_STAGE_W; ++x)
        {
            fb_data stage = *sk_stage_buffer_address(sk_stage_data, x, y);
            fb_data background =
                *sk_stage_buffer_address(sk_stage_background_data, x, y);

            if (stage == background)
                continue;
            min_x = MIN(min_x, x);
            min_y = MIN(min_y, y);
            max_x = MAX(max_x, x);
            max_y = MAX(max_y, y);
        }
    }
    if (max_x < min_x || max_y < min_y)
        return false;

    bound_w = max_x - min_x + 1;
    bound_h = max_y - min_y + 1;
    if (bound_w * SK_PREVIEW_INNER_H >
        bound_h * SK_PREVIEW_INNER_W)
    {
        draw_w = SK_PREVIEW_INNER_W;
        draw_h = MAX(1, bound_h * draw_w / bound_w);
    }
    else
    {
        draw_h = SK_PREVIEW_INNER_H;
        draw_w = MAX(1, bound_w * draw_h / bound_h);
    }
    offset_x = (SK_PREVIEW_W - draw_w) / 2;
    offset_y = (SK_PREVIEW_H - draw_h) / 2;
    for (y = 0; y < SK_PREVIEW_H; ++y)
        for (x = 0; x < SK_PREVIEW_W; ++x)
            sk_preview_data[y * SK_PREVIEW_W + x] = TRANSPARENT_COLOR;

    for (y = 0; y < draw_h; ++y)
    {
        int source_y = min_y + y * bound_h / draw_h;

        for (x = 0; x < draw_w; ++x)
        {
            int source_x = min_x + x * bound_w / draw_w;
            fb_data stage = *sk_stage_buffer_address(
                sk_stage_data, source_x, source_y);
            fb_data background = *sk_stage_buffer_address(
                sk_stage_background_data, source_x, source_y);

            if (stage != background)
                sk_preview_data[
                    (offset_y + y) * SK_PREVIEW_W + offset_x + x] = stage;
        }
    }

    rb->memset(&preview, 0, sizeof(preview));
    preview.width = SK_PREVIEW_W;
    preview.height = SK_PREVIEW_H;
    preview.format = FORMAT_NATIVE;
    preview.data = (unsigned char *)sk_preview_data;
    rb->mkdir(SK_PREVIEW_DIR);
    if (save_bmp_file((char *)SK_PREVIEW_FLOAT_TMP, &preview) < 0)
        return false;
    rb->remove(SK_PREVIEW_FLOAT);
    return rb->rename(SK_PREVIEW_FLOAT_TMP, SK_PREVIEW_FLOAT) >= 0;
}

static bool sk_publish_pane_background(void)
{
    char source[MAX_PATH];
    char text[40];
    struct viewport *old_vp;

    rb->snprintf(source, sizeof(source),
                 SK_BACKGROUND_DIR "/pane-%d.bmp", sk_background);
    if (!sk_load_bitmap(source, &sk_bitmap, sk_bitmap_data,
                        sizeof(sk_bitmap_data)))
        return false;

    /* The source template owns the card furniture. Draw the current values
     * into that cached bitmap before publishing it; otherwise a native
     * background refresh replaces RockPod's XP/coin text with a blank card. */
    rb->memset(&sk_pane_vp, 0, sizeof(sk_pane_vp));
    sk_pane_vp.width = sk_bitmap.width;
    sk_pane_vp.height = sk_bitmap.height;
    sk_pane_vp.font = FONT_SYSFIXED;
    sk_pane_vp.drawmode = DRMODE_FG;
    sk_pane_vp.fg_pattern = LCD_RGBPACK(42, 8, 55);
    sk_pane_vp.bg_pattern = LCD_RGBPACK(247, 240, 249);
    sk_pane_fb.data = sk_bitmap.data;
    sk_pane_fb.elems = LCD_NBELEMS(sk_bitmap.width, sk_bitmap.height);
    sk_pane_fb.stride = STRIDE_MAIN(sk_bitmap.width, sk_bitmap.height);
    sk_pane_fb.get_address_fn = sk_pane_address;
    rb->viewport_set_buffer(&sk_pane_vp, &sk_pane_fb, SCREEN_MAIN);
    old_vp = rb->lcd_set_viewport(&sk_pane_vp);
    rb->lcd_set_foreground(LCD_RGBPACK(42, 8, 55));
    rb->snprintf(text, sizeof(text), "XP %lu", (unsigned long)sk_xp);
    rb->lcd_putsxy(16, 197, (const unsigned char *)text);
    rb->snprintf(text, sizeof(text), "COINS %lu",
                 (unsigned long)sk_coins);
    rb->lcd_putsxy(16, 213, (const unsigned char *)text);
    rb->lcd_set_viewport(old_vp);

    rb->mkdir(SK_PREVIEW_DIR);
    if (save_bmp_file((char *)SK_PREVIEW_PANE_TMP, &sk_bitmap) < 0)
        return false;
    rb->remove(SK_PREVIEW_PANE);
    return rb->rename(SK_PREVIEW_PANE_TMP, SK_PREVIEW_PANE) >= 0;
}

static void sk_publish_preview(bool pane_changed)
{
    if (pane_changed)
        sk_publish_pane_background();
    sk_publish_character_preview();
}

/* ---------------------------------------------------------------------- */
/* save state                                                               */
/* ---------------------------------------------------------------------- */

static void sk_seed_starter(void)
{
    int i, granted = 0;

    for (i = 0; i < sk_chip_count && granted < 12; ++i)
    {
        if (!sk_chips[i].worn)
            continue;
        sk_owned_set(i, true);
        granted++;
    }
    sk_dirty = true;
}

static bool sk_load_save1(int got)
{
    int slot, i;

    if (got < SK_SAVE1_BYTES ||
        rb->memcmp(sk_save_data, SK_SAVE1_MAGIC, 4))
        return false;

    sk_xp = sk_le32(sk_save_data + 4);
    sk_coins = sk_le32(sk_save_data + 8);
    for (i = 0; i < SK_OWNED_WORDS; ++i)
        sk_owned[i] = sk_le32(sk_save_data + 16 + i * 4);
    for (slot = 0; slot < SK_SLOT_COUNT; ++slot)
    {
        int index = (int16_t)sk_le16(sk_save_data + 16 +
                                     SK_OWNED_WORDS * 4 + slot * 2);

        sk_equipped[slot] =
            (index >= 0 && index < sk_chip_count) ? index : -1;
    }

    /* SKS1 was positional. Preserve what can be mapped against the current
     * catalogue and immediately rewrite it as stable chip IDs. */
    sk_dirty = true;
    return true;
}

static bool sk_load_save2(int got)
{
    int owned_count, header, slot, i;

    if (got < SK_SAVE2_BASE_HEADER ||
        rb->memcmp(sk_save_data, SK_SAVE2_MAGIC, 4))
        return false;

    owned_count = sk_le16(sk_save_data + 12);
    header = sk_le16(sk_save_data + 14);
    if (header == 0)
        header = SK_SAVE2_BASE_HEADER;
    if (owned_count < 0 || owned_count > SK_MAX_CHIPS ||
        (header != SK_SAVE2_BASE_HEADER &&
         header != SK_SAVE2_DUMP_HEADER &&
         header != SK_SAVE2_HEADER &&
         header != SK_SAVE2_SHOP_HEADER) ||
        got < header + owned_count * 2)
        return false;

    sk_xp = sk_le32(sk_save_data + 4);
    sk_coins = sk_le32(sk_save_data + 8);
    if (header >= SK_SAVE2_DUMP_HEADER)
    {
        uint16_t dump_id = sk_le16(sk_save_data + SK_SAVE2_BASE_HEADER);

        sk_dump_index = dump_id == SK_NO_CHIP_ID ?
                        -1 : sk_find_chip(dump_id);
        sk_dump_ready_at = sk_le32(sk_save_data +
                                   SK_SAVE2_BASE_HEADER + 4);
    }
    if (header >= SK_SAVE2_HEADER)
    {
        sk_body_color = sk_save_data[SK_SAVE2_DUMP_HEADER];
        sk_background = sk_save_data[SK_SAVE2_DUMP_HEADER + 1];
        if (sk_body_color >= SK_BODY_COLOR_COUNT)
            sk_body_color = 0;
        if (sk_background >= SK_BACKGROUND_COUNT)
            sk_background = 0;
    }
    if (header >= SK_SAVE2_SHOP_HEADER)
    {
        int shop_slot;

        sk_shop_ready_at = sk_le32(sk_save_data + SK_SAVE2_HEADER);
        for (shop_slot = 0; shop_slot < SK_SHOP_SLOTS; ++shop_slot)
        {
            uint16_t id = sk_le16(sk_save_data + SK_SAVE2_HEADER + 4 +
                                  shop_slot * 2);

            sk_shop_stock[shop_slot] = id == SK_NO_CHIP_ID ?
                                       -1 : sk_find_chip(id);
        }
    }
    for (i = 0; i < owned_count; ++i)
    {
        int index = sk_find_chip(sk_le16(sk_save_data + header +
                                         i * 2));

        if (index >= 0)
            sk_owned_set(index, true);
    }
    for (slot = 0; slot < SK_SLOT_COUNT; ++slot)
    {
        uint16_t id = sk_le16(sk_save_data + 16 + slot * 2);
        int index = id == SK_NO_CHIP_ID ? -1 : sk_find_chip(id);

        if (index >= 0 && sk_chips[index].worn && sk_owned_get(index))
            sk_equipped[slot] = index;
    }
    return true;
}

static bool sk_load_save3(int got)
{
    int owned_count, slot, i;
    int preset_count;

    if (got < SK_SAVE3_HEADER ||
        rb->memcmp(sk_save_data, SK_SAVE3_MAGIC, 4))
        return false;

    owned_count = sk_le16(sk_save_data + 12);
    if (sk_le16(sk_save_data + 14) != SK_SAVE3_HEADER ||
        owned_count < 0 || owned_count > SK_MAX_CHIPS ||
        got < SK_SAVE3_HEADER + owned_count * 2)
        return false;

    sk_xp = sk_le32(sk_save_data + 4);
    sk_coins = sk_le32(sk_save_data + 8);
    {
        uint16_t id = sk_le16(sk_save_data + SK_SAVE2_BASE_HEADER);

        sk_dump_index = id == SK_NO_CHIP_ID ? -1 : sk_find_chip(id);
        sk_dump_ready_at = sk_le32(sk_save_data + SK_SAVE2_BASE_HEADER + 4);
    }
    sk_body_color = sk_save_data[SK_SAVE2_DUMP_HEADER];
    sk_background = sk_save_data[SK_SAVE2_DUMP_HEADER + 1];
    if (sk_body_color >= SK_BODY_COLOR_COUNT)
        sk_body_color = 0;
    if (sk_background >= SK_BACKGROUND_COUNT)
        sk_background = 0;
    sk_shop_ready_at = sk_le32(sk_save_data + SK_SAVE2_HEADER);
    for (slot = 0; slot < SK_SHOP_SLOTS; ++slot)
    {
        uint16_t id = sk_le16(sk_save_data + SK_SAVE2_HEADER + 4 +
                              slot * 2);

        sk_shop_stock[slot] = id == SK_NO_CHIP_ID ? -1 : sk_find_chip(id);
    }
    for (i = 0; i < owned_count; ++i)
    {
        int index = sk_find_chip(sk_le16(sk_save_data + SK_SAVE3_HEADER +
                                         i * 2));

        if (index >= 0)
            sk_owned_set(index, true);
    }
    for (slot = 0; slot < SK_SLOT_COUNT; ++slot)
    {
        uint16_t id = sk_le16(sk_save_data + 16 + slot * 2);
        int index = id == SK_NO_CHIP_ID ? -1 : sk_find_chip(id);

        if (index >= 0 && sk_chips[index].worn && sk_owned_get(index))
            sk_equipped[slot] = index;
    }

    /* Presets contain stable chip IDs, so catalogue reordering is harmless. */
    preset_count = sk_le16(sk_save_data + SK_SAVE2_SHOP_HEADER);
    preset_count = MIN(preset_count, SK_PRESET_COUNT);
    for (i = 0; i < preset_count; ++i)
    {
        unsigned char *data = sk_save_data + SK_SAVE2_SHOP_HEADER + 2 +
                              i * SK_PRESET_BYTES;
        struct sk_preset *preset = &sk_presets[i];

        if (!data[0])
            continue;
        preset->active = true;
        preset->body_color = data[1] < SK_BODY_COLOR_COUNT ? data[1] : 0;
        preset->background = data[2] < SK_BACKGROUND_COUNT ? data[2] : 0;
        rb->memcpy(preset->name, data + 3, SK_PRESET_NAME_MAX);
        preset->name[SK_PRESET_NAME_MAX - 1] = '\0';
        if (!preset->name[0])
            preset->active = false;
        for (slot = 0; slot < SK_SLOT_COUNT; ++slot)
            preset->equipped_ids[slot] = sk_le16(data + 3 +
                SK_PRESET_NAME_MAX + slot * 2);
    }
    return true;
}

static void sk_load_save(void)
{
    int fd, got, slot;

    for (slot = 0; slot < SK_SLOT_COUNT; ++slot)
        sk_equipped[slot] = -1;
    rb->memset(sk_owned, 0, sizeof(sk_owned));
    sk_xp = 0;
    sk_coins = 250;
    sk_dump_index = -1;
    sk_dump_ready_at = 0;
    sk_body_color = 0;
    sk_background = 0;
    for (slot = 0; slot < SK_SHOP_SLOTS; ++slot)
        sk_shop_stock[slot] = -1;
    sk_shop_ready_at = 0;
    rb->memset(sk_presets, 0, sizeof(sk_presets));
    sk_preset_index = -1;

    fd = rb->open(SK_SAVE_FILE, O_RDONLY);
    if (fd < 0)
    {
        /* First run: seed a small starter set so the workshop is usable. */
        sk_seed_starter();
        return;
    }
    got = rb->read(fd, sk_save_data, sizeof(sk_save_data));
    rb->close(fd);

    if (sk_load_save3(got) || sk_load_save2(got) || sk_load_save1(got))
        return;

    sk_seed_starter();
}

static void sk_put32(unsigned char *p, uint32_t v)
{
    p[0] = v & 0xff;
    p[1] = (v >> 8) & 0xff;
    p[2] = (v >> 16) & 0xff;
    p[3] = (v >> 24) & 0xff;
}

static void sk_put16(unsigned char *p, uint16_t v)
{
    p[0] = v & 0xff;
    p[1] = (v >> 8) & 0xff;
}

/* Write to a temp file and rename, so a yanked battery can never leave a
 * half written save behind. */
static bool sk_write_save(void)
{
    int fd, slot, i, owned_count = 0, preset_count = 0;
    size_t bytes;

    for (i = 0; i < sk_chip_count; ++i)
    {
        int pos;
        uint16_t id;

        if (!sk_owned_get(i))
            continue;
        id = sk_chips[i].id;
        pos = owned_count;
        while (pos > 0 && sk_owned_ids[pos - 1] > id)
        {
            sk_owned_ids[pos] = sk_owned_ids[pos - 1];
            pos--;
        }
        sk_owned_ids[pos] = id;
        owned_count++;
    }

    bytes = SK_SAVE3_HEADER + owned_count * 2;
    rb->memset(sk_save_data, 0, bytes);
    rb->memcpy(sk_save_data, SK_SAVE3_MAGIC, 4);
    sk_put32(sk_save_data + 4, sk_xp);
    sk_put32(sk_save_data + 8, sk_coins);
    sk_put16(sk_save_data + 12, (uint16_t)owned_count);
    sk_put16(sk_save_data + 14, SK_SAVE3_HEADER);
    for (slot = 0; slot < SK_SLOT_COUNT; ++slot)
    {
        int index = sk_equipped[slot];
        uint16_t id = index >= 0 && index < sk_chip_count ?
                      sk_chips[index].id : SK_NO_CHIP_ID;

        sk_put16(sk_save_data + 16 + slot * 2, id);
    }
    sk_put16(sk_save_data + SK_SAVE2_BASE_HEADER,
             sk_dump_index >= 0 && sk_dump_index < sk_chip_count ?
             sk_chips[sk_dump_index].id : SK_NO_CHIP_ID);
    sk_put32(sk_save_data + SK_SAVE2_BASE_HEADER + 4, sk_dump_ready_at);
    sk_save_data[SK_SAVE2_DUMP_HEADER] = sk_body_color;
    sk_save_data[SK_SAVE2_DUMP_HEADER + 1] = sk_background;
    sk_put32(sk_save_data + SK_SAVE2_HEADER, sk_shop_ready_at);
    for (slot = 0; slot < SK_SHOP_SLOTS; ++slot)
    {
        int index = sk_shop_stock[slot];
        uint16_t id = index >= 0 && index < sk_chip_count ?
                      sk_chips[index].id : SK_NO_CHIP_ID;

        sk_put16(sk_save_data + SK_SAVE2_HEADER + 4 + slot * 2, id);
    }
    for (i = 0; i < SK_PRESET_COUNT; ++i)
    {
        const struct sk_preset *preset = &sk_presets[i];
        unsigned char *data = sk_save_data + SK_SAVE2_SHOP_HEADER + 2 +
                              i * SK_PRESET_BYTES;

        if (!preset->active)
            continue;
        preset_count++;
        data[0] = 1;
        data[1] = preset->body_color;
        data[2] = preset->background;
        rb->strlcpy((char *)data + 3, preset->name, SK_PRESET_NAME_MAX);
        for (slot = 0; slot < SK_SLOT_COUNT; ++slot)
            sk_put16(data + 3 + SK_PRESET_NAME_MAX + slot * 2,
                     preset->equipped_ids[slot]);
    }
    sk_put16(sk_save_data + SK_SAVE2_SHOP_HEADER, preset_count);
    for (i = 0; i < owned_count; ++i)
        sk_put16(sk_save_data + SK_SAVE3_HEADER + i * 2,
                sk_owned_ids[i]);

    rb->mkdir(SK_ROOT "/state");
    fd = rb->open(SK_SAVE_TMP, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;
    if (rb->write(fd, sk_save_data, bytes) != (ssize_t)bytes)
    {
        rb->close(fd);
        rb->remove(SK_SAVE_TMP);
        return false;
    }
    rb->close(fd);
    rb->remove(SK_SAVE_FILE);
    if (rb->rename(SK_SAVE_TMP, SK_SAVE_FILE) < 0)
        return false;
    sk_dirty = false;
    return true;
}

static int sk_preset_total(void)
{
    int i, count = 0;

    for (i = 0; i < SK_PRESET_COUNT; ++i)
        if (sk_presets[i].active)
            count++;
    return count;
}

static int sk_preset_at(int position)
{
    int i;

    for (i = 0; i < SK_PRESET_COUNT; ++i)
    {
        if (!sk_presets[i].active)
            continue;
        if (position-- == 0)
            return i;
    }
    return -1;
}

static void sk_apply_preset(int index)
{
    struct sk_preset *preset;
    int slot;

    if (index < 0 || index >= SK_PRESET_COUNT || !sk_presets[index].active)
        return;
    preset = &sk_presets[index];
    sk_body_color = preset->body_color;
    sk_background = preset->background;
    for (slot = 0; slot < SK_SLOT_COUNT; ++slot)
    {
        int chip = sk_find_chip(preset->equipped_ids[slot]);

        sk_equipped[slot] = chip >= 0 && sk_chips[chip].worn &&
                            sk_owned_get(chip) ? chip : -1;
    }
    sk_preset_index = index;
    sk_dirty = true;
    sk_reload_stage();
    sk_publish_preview(true);
    sk_set_message(preset->name);
}

static void sk_save_preset(void)
{
    char name[SK_PRESET_NAME_MAX];
    int index = -1;
    int slot;

    rb->snprintf(name, sizeof(name), "Preset %d", sk_preset_total() + 1);
    if (!sk_preset_name_input(name, sizeof(name)) || !name[0])
        return;

    for (slot = 0; slot < SK_PRESET_COUNT; ++slot)
    {
        if (sk_presets[slot].active && !rb->strcmp(sk_presets[slot].name,
                                                    name))
        {
            index = slot;
            break;
        }
        if (index < 0 && !sk_presets[slot].active)
            index = slot;
    }
    if (index < 0)
    {
        sk_set_message("Preset list is full");
        sk_play(SK_SND_BACK);
        return;
    }

    rb->memset(&sk_presets[index], 0, sizeof(sk_presets[index]));
    sk_presets[index].active = true;
    sk_presets[index].body_color = sk_body_color;
    sk_presets[index].background = sk_background;
    rb->strlcpy(sk_presets[index].name, name, SK_PRESET_NAME_MAX);
    for (slot = 0; slot < SK_SLOT_COUNT; ++slot)
    {
        int chip = sk_equipped[slot];

        sk_presets[index].equipped_ids[slot] =
            chip >= 0 && chip < sk_chip_count ? sk_chips[chip].id :
            SK_NO_CHIP_ID;
    }
    sk_preset_index = index;
    sk_dirty = true;
    sk_write_save();
    sk_publish_preview(true);
    sk_set_message(name);
    sk_play(SK_SND_SELECT);
}

static void sk_cycle_preset(int direction)
{
    int count = sk_preset_total();
    int position = 0;
    int index;

    if (!count)
    {
        sk_set_message("No saved presets");
        sk_play(SK_SND_BACK);
        return;
    }
    if (sk_preset_index >= 0)
    {
        for (position = 0; position < count; ++position)
            if (sk_preset_at(position) == sk_preset_index)
                break;
        position = (position + direction + count) % count;
    }
    else if (direction < 0)
        position = count - 1;
    index = sk_preset_at(position);
    sk_apply_preset(index);
    sk_play(SK_SND_NAVIGATE);
}

/* ---------------------------------------------------------------------- */
/* inbox: grants pushed down by RockPod                                     */
/* ---------------------------------------------------------------------- */

static int sk_inbox_lines;
static int sk_inbox_applied;

/* inbox.v1.tsv rows are "grant <chip id> <label>" or "coins <amount> ...".
 * RockPod owns generation; the device only ever applies what it is given. */
static void sk_apply_inbox(void)
{
    char *cursor;
    int got = sk_read_text_file(SK_INBOX_FILE);
    uint32_t old_owned[SK_OWNED_WORDS];
    uint32_t old_xp = sk_xp;
    uint32_t old_coins = sk_coins;
    bool old_dirty = sk_dirty;

    sk_inbox_lines = 0;
    sk_inbox_applied = 0;
    if (got <= 0)
        return;

    rb->memcpy(old_owned, sk_owned, sizeof(old_owned));
    cursor = sk_text;
    while (cursor && *cursor)
    {
        char *line = cursor;
        char *nl = rb->strchr(cursor, '\n');
        char *f[4];
        int n;

        if (nl)
        {
            *nl = '\0';
            cursor = nl + 1;
        }
        else
        {
            cursor = NULL;
        }
        if (!line[0] || line[0] == '#')
            continue;

        sk_inbox_lines++;
        n = sk_split(line, f, 4);
        if (n < 2)
            continue;

        if (!rb->strcmp(f[0], "grant"))
        {
            int index = sk_find_chip(sk_parse_int(f[1]));

            if (index >= 0 && !sk_owned_get(index))
            {
                sk_owned_set(index, true);
                sk_xp += 10;
                sk_inbox_applied++;
                sk_dirty = true;
            }
        }
        else if (!rb->strcmp(f[0], "coins"))
        {
            int amount = sk_parse_int(f[1]);

            if (amount > 0 && (uint32_t)amount <= UINT32_MAX - sk_coins)
            {
                sk_coins += (uint32_t)amount;
                sk_inbox_applied++;
                sk_dirty = true;
            }
        }
    }
    if (sk_inbox_lines > 0)
    {
        if (sk_dirty && !sk_write_save())
        {
            rb->memcpy(sk_owned, old_owned, sizeof(old_owned));
            sk_xp = old_xp;
            sk_coins = old_coins;
            sk_dirty = old_dirty;
            sk_inbox_applied = 0;
            return;
        }
        /* Consumed grants must not reapply on the next launch. */
        rb->remove(SK_INBOX_FILE);
        if (sk_inbox_applied > 0)
            sk_play(SK_SND_REWARD);
    }
}

/* ---------------------------------------------------------------------- */
/* drawing                                                                  */
/* ---------------------------------------------------------------------- */

static void sk_vgradient(int x, int y, int w, int h,
                         unsigned top, unsigned bottom)
{
    int row;
    int r1 = RGB_UNPACK_RED(top), g1 = RGB_UNPACK_GREEN(top);
    int b1 = RGB_UNPACK_BLUE(top);
    int r2 = RGB_UNPACK_RED(bottom), g2 = RGB_UNPACK_GREEN(bottom);
    int b2 = RGB_UNPACK_BLUE(bottom);

    if (h <= 0)
        return;
    for (row = 0; row < h; ++row)
    {
        int r = r1 + (r2 - r1) * row / h;
        int g = g1 + (g2 - g1) * row / h;
        int b = b1 + (b2 - b1) * row / h;

        rb->lcd_set_foreground(LCD_RGBPACK(r, g, b));
        rb->lcd_hline(x, x + w - 1, y + row);
    }
}

/* Text is always painted foreground-only so it cannot stamp a solid
 * background rectangle over gradients or other completed artwork. */
static void sk_putsxy(int x, int y, const unsigned char *text)
{
    int old_mode = rb->lcd_get_drawmode();

    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_putsxy(x, y, text);
    rb->lcd_set_drawmode(old_mode);
}

static void sk_draw_title(const char *title)
{
    int tw, th, x = 5;

    sk_vgradient(0, 0, LCD_WIDTH, SK_TITLE_H,
                 SK_COL_TITLE_TOP, SK_COL_TITLE_BOT);
    if (sk_logo_loaded)
    {
        int old_mode = rb->lcd_get_drawmode();

        rb->lcd_set_drawmode(DRMODE_FG);
        rb->lcd_bmp_part(&sk_logo, 0, 0, 4,
                         (SK_TITLE_H - sk_logo.height) / 2,
                         sk_logo.width, sk_logo.height);
        rb->lcd_set_drawmode(old_mode);
        x = 38;
    }
    rb->lcd_set_foreground(SK_COL_YELLOW);
    sk_putsxy(x, 6, (const unsigned char *)"SITEKICK");

    rb->lcd_getstringsize((const unsigned char *)title, &tw, &th);
    rb->lcd_set_foreground(SK_COL_WHITE);
    sk_putsxy(LCD_WIDTH - tw - 7, (SK_TITLE_H - th) / 2,
              (const unsigned char *)title);
    rb->lcd_set_foreground(SK_COL_ORANGE);
    rb->lcd_hline(0, LCD_WIDTH - 1, SK_TITLE_H - 1);
}

static void sk_draw_status(void)
{
    char buf[64];
    int y = LCD_HEIGHT - SK_STATUS_H;
    int tw, th;

    sk_vgradient(0, y, LCD_WIDTH, SK_STATUS_H,
                 SK_COL_STATUS_TOP, SK_COL_STATUS_BOT);
    rb->lcd_set_foreground(SK_COL_RULE);
    rb->lcd_hline(0, LCD_WIDTH - 1, y);

    rb->lcd_set_foreground(SK_COL_TEXT);
    if (sk_message[0] && TIME_BEFORE(*rb->current_tick, sk_message_until))
    {
        sk_putsxy(6, y + 3, (const unsigned char *)sk_message);
        return;
    }
    sk_message[0] = '\0';

    if (sk_preset_index >= 0 && sk_preset_index < SK_PRESET_COUNT &&
        sk_presets[sk_preset_index].active)
        rb->snprintf(buf, sizeof(buf), "Preset: %s",
                     sk_presets[sk_preset_index].name);
    else
        rb->snprintf(buf, sizeof(buf), "Chips %d/%d", sk_owned_count(),
                     sk_chip_count);
    sk_putsxy(6, y + 3, (const unsigned char *)buf);

    rb->snprintf(buf, sizeof(buf), "XP %lu", (unsigned long)sk_xp);
    rb->lcd_getstringsize((const unsigned char *)buf, &tw, &th);
    sk_putsxy(LCD_WIDTH - tw - 6, y + 3, (const unsigned char *)buf);
}

/* Paint the fixed cached background, then float the transparent cached
 * paperdoll using elapsed ticks. No file access, decoding, allocation, or
 * playback-memory ownership occurs in this frame path. */
static void sk_draw_stage(int x, int y)
{
    static const signed char float_y[24] = {
         0, -1, -2, -3, -4, -5, -5, -4, -3, -2, -1,  0,
         1,  2,  3,  4,  4,  3,  2,  1,  0, -1, -2, -1,
    };
    static const signed char float_x[24] = {
         0,  0,  1,  1,  2,  2,  2,  1,  1,  0,  0, -1,
        -1, -2, -2, -2, -1, -1,  0,  0,  1,  1,  1,  0,
    };
    int phase = (int)(*rb->current_tick / MAX(1, HZ / 12)) %
                (int)ARRAYLEN(float_y);
    int offset_y = float_y[phase];
    int offset_x = float_x[phase];
    int src_y = offset_y < 0 ? -offset_y : 0;
    int dst_y = y + (offset_y > 0 ? offset_y : 0);
    int height = SK_STAGE_H - (offset_y < 0 ? -offset_y : offset_y);
    int shadow_w = 50 - (offset_y < 0 ? -offset_y : offset_y) * 2;
    int shadow_x = x + (SK_STAGE_W - shadow_w) / 2 + offset_x;
    int shadow_y = y + SK_STAGE_H - 21;

    if (sk_stage_loaded)
    {
        rb->lcd_bitmap_part(sk_stage_background_data, 0, 0,
                            STRIDE_MAIN(SK_STAGE_W, SK_STAGE_H),
                            x, y, SK_STAGE_W, SK_STAGE_H);
        /* The stage is restored from the fixed cache every frame, then this
         * small contact shadow and a 24-frame orbit are painted. No frame
         * reads assets or allocates, so active music remains untouched. */
        rb->lcd_set_foreground(SK_COL_RULE);
        rb->lcd_hline(shadow_x + 6, shadow_x + shadow_w - 7, shadow_y);
        rb->lcd_hline(shadow_x + 2, shadow_x + shadow_w - 3, shadow_y + 1);
        rb->lcd_hline(shadow_x, shadow_x + shadow_w - 1, shadow_y + 2);
        rb->lcd_set_foreground(REPLACEWITHFG_COLOR);
        rb->lcd_bitmap_transparent_part(
            sk_float_data, 0, src_y,
            STRIDE_MAIN(SK_STAGE_W, SK_STAGE_H),
            x + offset_x, dst_y, SK_STAGE_W, height);
    }
    rb->lcd_set_foreground(SK_COL_RULE);
    rb->lcd_drawrect(x, y, SK_STAGE_W, SK_STAGE_H);
}

static void sk_clear_loadout(void)
{
    int slot;

    for (slot = 0; slot < SK_SLOT_COUNT; ++slot)
        sk_equipped[slot] = -1;
    sk_body_color = 0;
    sk_background = 0;
    sk_preset_index = -1;
    sk_dirty = true;
    sk_reload_stage();
    sk_write_save();
    sk_set_message("Sitekick cleared");
    sk_play(SK_SND_BACK);
}

/* Match the iPodJS Music Search alphabet carousel instead of opening the
 * generic Rockbox keyboard. The wheel scrolls the selected letter, Select
 * appends it, Left deletes and Right inserts a space. */
static void sk_draw_preset_name_input(const char *name, int character)
{
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const int panel_x = 12;
    const int panel_y = LCD_HEIGHT - SK_STATUS_H - 58;
    const int panel_w = LCD_WIDTH - 24;
    const int center_x = LCD_WIDTH / 2;
    int offset;
    int tw, th;

    sk_draw_title("Name Preset");
    sk_draw_workshop();
    sk_vgradient(panel_x, panel_y, panel_w, 58,
                 SK_COL_TITLE_TOP, SK_COL_TITLE_BOT);
    rb->lcd_set_foreground(SK_COL_YELLOW);
    rb->lcd_drawrect(panel_x, panel_y, panel_w, 58);
    rb->lcd_set_foreground(SK_COL_WHITE);
    rb->lcd_getstringsize((const unsigned char *)name, &tw, &th);
    sk_putsxy(MAX(panel_x + 8, panel_x + panel_w - tw - 8), panel_y + 5,
              (const unsigned char *)name);

    for (offset = -5; offset <= 5; ++offset)
    {
        int index = character + offset;
        int x = center_x + offset * 22;
        char glyph[2];

        while (index < 0)
            index += (int)sizeof(alphabet) - 1;
        glyph[0] = alphabet[index % ((int)sizeof(alphabet) - 1)];
        glyph[1] = '\0';
        if (offset == 0)
        {
            rb->lcd_set_foreground(SK_COL_SEL_TOP);
            rb->lcd_fillrect(x - 9, panel_y + 25, 18, 20);
            rb->lcd_set_foreground(SK_COL_SEL_TEXT);
        }
        else
            rb->lcd_set_foreground(SK_COL_WHITE);
        sk_putsxy(x - 3, panel_y + 29, (const unsigned char *)glyph);
    }
    rb->lcd_set_foreground(SK_COL_TEXT);
    sk_center_text(panel_y + 47, "WHEEL scroll  PLAY saves", SK_COL_TEXT);
    sk_draw_status();
    rb->lcd_update();
}

static bool sk_preset_name_input(char *name, size_t size)
{
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int character = 0;
    bool redraw = true;

    while (true)
    {
        int button;

        if (redraw)
        {
            sk_draw_preset_name_input(name, character);
            redraw = false;
        }
        button = pluginlib_getaction(HZ / 20, sk_preset_name_contexts,
                                     ARRAYLEN(sk_preset_name_contexts));
        switch (button)
        {
        case PLA_UP:
        case PLA_UP_REPEAT:
        case PLA_SCROLL_BACK:
        case PLA_SCROLL_BACK_REPEAT:
            character = character <= 0 ? (int)sizeof(alphabet) - 2 :
                        character - 1;
            sk_play(SK_SND_NAVIGATE);
            redraw = true;
            break;

        case PLA_DOWN:
        case PLA_DOWN_REPEAT:
        case PLA_SCROLL_FWD:
        case PLA_SCROLL_FWD_REPEAT:
            character = (character + 1) % ((int)sizeof(alphabet) - 1);
            sk_play(SK_SND_NAVIGATE);
            redraw = true;
            break;

        case PLA_SELECT:
            if (rb->strlen(name) + 1 < size)
            {
                size_t length = rb->strlen(name);

                name[length] = alphabet[character];
                name[length + 1] = '\0';
                sk_play(SK_SND_SELECT);
                redraw = true;
            }
            break;

        case PLA_LEFT:
            if (name[0])
            {
                name[rb->strlen(name) - 1] = '\0';
                sk_play(SK_SND_BACK);
                redraw = true;
            }
            break;

        case PLA_RIGHT:
            if (rb->strlen(name) + 1 < size)
            {
                size_t length = rb->strlen(name);

                name[length] = ' ';
                name[length + 1] = '\0';
                sk_play(SK_SND_SELECT);
                redraw = true;
            }
            break;

        case SK_ACTION_PRESET_SAVE:
            return name[0];

        case SK_ACTION_PRESET_CANCEL:
        case PLA_CANCEL:
            return false;

        case PLA_EXIT:
            return false;

        default:
            if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
                return false;
            break;
        }
    }
}

static void sk_draw_cached_icon(int chip_index, int x, int y)
{
    const struct sk_chip *chip;
    int old_mode;

    if (chip_index < 0 || chip_index >= sk_chip_count)
        return;
    chip = &sk_chips[chip_index];
    if (sk_icon_page_loaded != chip->page)
        return;
    old_mode = rb->lcd_get_drawmode();
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_bmp_part(&sk_icon_page,
                     (chip->index % SK_ICONS_PER_ROW) * SK_ICON_PX,
                     (chip->index / SK_ICONS_PER_ROW) * SK_ICON_PX,
                     x, y, SK_ICON_PX, SK_ICON_PX);
    rb->lcd_set_drawmode(old_mode);
}

static void sk_draw_icon(int chip_index, int x, int y)
{
    if (chip_index < 0 || chip_index >= sk_chip_count ||
        !sk_load_icon_page(sk_chips[chip_index].page))
        return;
    sk_draw_cached_icon(chip_index, x, y);
}

#if LCD_WIDTH >= 1920
static void *sk_dm_alloc(size_t bytes)
{
    size_t aligned = ALIGN_UP(bytes, 4);
    void *result;

    if (!sk_dm_arena || aligned > sk_dm_arena_left)
        return NULL;
    result = sk_dm_arena;
    sk_dm_arena += aligned;
    sk_dm_arena_left -= aligned;
    return result;
}

static bool sk_dm_read_all(int fd, void *buffer, size_t size)
{
    unsigned char *cursor = buffer;

    while (size > 0)
    {
        ssize_t got = rb->read(fd, cursor, size);

        if (got <= 0)
            return false;
        cursor += got;
        size -= got;
    }
    return true;
}

static bool sk_desktop_init(void)
{
    uint32_t header[3];
    struct bitmap window;
    size_t size;
    size_t screen_bytes =
        (size_t)LCD_WIDTH * LCD_HEIGHT * sizeof(fb_data);
    size_t window_bytes =
        BM_SIZE(SK_DM_WIN_W, SK_DM_WIN_H, FORMAT_NATIVE, false);
    size_t stage_bytes =
        BM_SIZE(SK_DM_STAGE_W, SK_DM_STAGE_H, FORMAT_NATIVE, false);
    int fd;

    sk_dm_arena = rb->plugin_get_buffer(&size);
    sk_dm_arena_left = size;
    sk_dm_underlay = sk_dm_alloc(screen_bytes);
    sk_dm_window = sk_dm_alloc(window_bytes);
    sk_dm_stage_background = sk_dm_alloc(stage_bytes);
    sk_dm_stage_float = sk_dm_alloc(stage_bytes);
    if (!sk_dm_underlay || !sk_dm_window || !sk_dm_stage_background ||
        !sk_dm_stage_float)
        return false;

    fd = rb->open(SK_DM_UNDERLAY_FILE, O_RDONLY);
    if (fd < 0)
        return false;
    if (!sk_dm_read_all(fd, header, sizeof(header)) ||
        header[0] != SK_DM_UNDERLAY_MAGIC ||
        header[1] != LCD_WIDTH || header[2] != LCD_HEIGHT ||
        !sk_dm_read_all(fd, sk_dm_underlay, screen_bytes))
    {
        rb->close(fd);
        return false;
    }
    rb->close(fd);

    rb->memset(&window, 0, sizeof(window));
    window.width = SK_DM_WIN_W;
    window.height = SK_DM_WIN_H;
    window.format = FORMAT_NATIVE;
    window.data = (unsigned char *)sk_dm_window;
    if (rb->read_bmp_file(SK_DM_WINDOW_FILE, &window, window_bytes,
                          FORMAT_NATIVE, NULL) <= 0 ||
        window.width != SK_DM_WIN_W || window.height != SK_DM_WIN_H)
        return false;
    return true;
}

static void sk_desktop_refresh_stage(void)
{
    struct bitmap source;
    struct bitmap destination;

    if (!sk_desktop_mode || !sk_dm_stage_background ||
        !sk_dm_stage_float || !sk_stage_loaded)
        return;

    rb->memset(&source, 0, sizeof(source));
    rb->memset(&destination, 0, sizeof(destination));
    source.width = SK_STAGE_W;
    source.height = SK_STAGE_H;
    source.format = FORMAT_NATIVE;
    destination.width = SK_DM_STAGE_W;
    destination.height = SK_DM_STAGE_H;
    destination.format = FORMAT_NATIVE;

    source.data = (unsigned char *)sk_stage_background_data;
    destination.data = (unsigned char *)sk_dm_stage_background;
    simple_resize_bitmap(&source, &destination);
    source.data = (unsigned char *)sk_float_data;
    destination.data = (unsigned char *)sk_dm_stage_float;
    simple_resize_bitmap(&source, &destination);
}

static void sk_dm_text(int x, int y, const char *text, unsigned color)
{
    rb->lcd_set_foreground(color);
    sk_putsxy(x, y, (const unsigned char *)text);
}

static void sk_dm_center_text(int x, int y, int width, const char *text,
                              unsigned color)
{
    int text_w;
    int text_h;

    rb->lcd_getstringsize((const unsigned char *)text, &text_w, &text_h);
    sk_dm_text(x + MAX(0, (width - text_w) / 2), y, text, color);
}

static void sk_dm_grid(int x, int y, int width, int height)
{
    int position;

    rb->lcd_set_foreground(LCD_RGBPACK(133, 218, 18));
    rb->lcd_fillrect(x, y, width, height);
    rb->lcd_set_foreground(LCD_RGBPACK(100, 193, 20));
    for (position = x; position < x + width; position += 18)
        rb->lcd_vline(position, y, y + height - 1);
    for (position = y; position < y + height; position += 18)
        rb->lcd_hline(x, x + width - 1, position);
}

static void sk_dm_shell(const char *title)
{
    static const char * const tabs[] =
        { "MAIN", "CHIPS", "SK-TV", "TRADE", "HELP" };
    char caption[80];
    char status[80];
    int index;
    int tab_w = SK_DM_BODY_W / (int)ARRAYLEN(tabs);

    rb->lcd_bitmap(sk_dm_underlay, 0, 0, LCD_WIDTH, LCD_HEIGHT);
    rb->lcd_bitmap(sk_dm_window, SK_DM_WIN_X, SK_DM_WIN_Y,
                   SK_DM_WIN_W, SK_DM_WIN_H);

    rb->snprintf(caption, sizeof(caption), "Sitekick - %s", title);
    sk_dm_center_text(SK_DM_WIN_X, SK_DM_WIN_Y + 5, SK_DM_WIN_W,
                      caption, LCD_RGBPACK(60, 60, 60));

    sk_dm_grid(SK_DM_BODY_X, SK_DM_BODY_Y, SK_DM_BODY_W, SK_DM_BODY_H);
    sk_vgradient(SK_DM_BODY_X, SK_DM_BODY_Y, SK_DM_BODY_W, SK_DM_HEADER_H,
                 SK_COL_TITLE_TOP, SK_COL_TITLE_BOT);
    if (sk_logo_loaded)
    {
        int old_mode = rb->lcd_get_drawmode();

        rb->lcd_set_drawmode(DRMODE_FG);
        rb->lcd_bmp_part(&sk_logo, 0, 0, SK_DM_BODY_X + 14,
                         SK_DM_BODY_Y + 11, sk_logo.width, sk_logo.height);
        rb->lcd_set_drawmode(old_mode);
    }
    sk_dm_text(SK_DM_BODY_X + 52, SK_DM_BODY_Y + 14, "SITEKICK",
               SK_COL_YELLOW);
    sk_dm_text(SK_DM_BODY_X + 146, SK_DM_BODY_Y + 14,
               "YTV DESKTOP DOCK", SK_COL_WHITE);
    rb->snprintf(status, sizeof(status),
                 "%d/%d CHIPS    XP %lu    COINS %lu",
                 sk_owned_count(), sk_chip_count,
                 (unsigned long)sk_xp, (unsigned long)sk_coins);
    sk_dm_text(SK_DM_BODY_X + SK_DM_BODY_W - 310,
               SK_DM_BODY_Y + 14, status, SK_COL_WHITE);

    for (index = 0; index < (int)ARRAYLEN(tabs); ++index)
    {
        int x = SK_DM_BODY_X + index * tab_w;
        bool active =
            (index == 0 && sk_scene == SK_SCENE_WORKSHOP) ||
            (index == 1 && (sk_scene == SK_SCENE_SLOT ||
                            sk_scene == SK_SCENE_COLLECTION ||
                            sk_scene == SK_SCENE_COLOR ||
                            sk_scene == SK_SCENE_BACKGROUND)) ||
            (index == 2 && sk_scene == SK_SCENE_STATS) ||
            (index == 3 && (sk_scene == SK_SCENE_INBOX ||
                            sk_scene == SK_SCENE_DUMP)) ||
            (index == 4 && sk_scene == SK_SCENE_GAMES);

        rb->lcd_set_foreground(active ? SK_COL_YELLOW : SK_COL_PANE);
        rb->lcd_fillrect(x, SK_DM_TAB_Y, tab_w - 2, SK_DM_TABS_H);
        rb->lcd_set_foreground(SK_COL_RULE);
        rb->lcd_drawrect(x, SK_DM_TAB_Y, tab_w - 2, SK_DM_TABS_H);
        sk_dm_center_text(x, SK_DM_TAB_Y + 13, tab_w - 2, tabs[index],
                          SK_COL_TEXT);
    }
}

static void sk_dm_footer(void)
{
    const char *message =
        sk_message[0] && TIME_BEFORE(*rb->current_tick, sk_message_until) ?
        sk_message : "Arrow keys choose - Enter opens - Esc goes back";

    sk_dm_center_text(SK_DM_WIN_X, SK_DM_WIN_Y + SK_DM_WIN_H - 18,
                      SK_DM_WIN_W, message, LCD_RGBPACK(70, 70, 70));
}

static void sk_dm_row(int x, int y, int width, const char *label,
                      const char *value, bool selected)
{
    int text_w;
    int text_h;

    if (selected)
        sk_vgradient(x, y, width, 28, SK_COL_SEL_TOP, SK_COL_SEL_BOT);
    else
    {
        rb->lcd_set_foreground(LCD_RGBPACK(247, 240, 249));
        rb->lcd_fillrect(x, y, width, 28);
    }
    sk_dm_text(x + 10, y + 7, label,
               selected ? SK_COL_SEL_TEXT : SK_COL_TEXT);
    if (value)
    {
        rb->lcd_getstringsize((const unsigned char *)value,
                              &text_w, &text_h);
        sk_dm_text(x + width - text_w - 10, y + 7, value,
                   selected ? SK_COL_SEL_TEXT : SK_COL_DIM);
    }
    rb->lcd_set_foreground(SK_COL_RULE);
    rb->lcd_hline(x, x + width - 1, y + 27);
}

static void sk_dm_draw_workshop(void)
{
    static const char * const labels[] =
    {
        "Equip 1", "Equip 2", "Equip 3", "Equip 4",
        "Equip 5", "Equip 6", "Equip 7", "Equip 8",
        "Collection", "Chip Dump", "Trading Post", "Body Color",
        "Background", "Minigames", "Chip Shop", "Stats", "Presets",
        "Save Preset", "+ Clear Sitekick"
    };
    const int sidebar_x = SK_DM_BODY_X + 16;
    const int sidebar_y = SK_DM_CONTENT_Y + 10;
    const int sidebar_w = 248;
    const int stage_x = SK_DM_BODY_X + 350;
    const int stage_y = SK_DM_CONTENT_Y + 20;
    static const signed char bob[8] = { 0, -2, -4, -2, 0, 2, 4, 2 };
    int phase = (int)((*rb->current_tick * 8 / MAX(1, 2 * HZ)) & 7);
    int index;

    rb->lcd_set_foreground(LCD_RGBPACK(76, 11, 100));
    rb->lcd_fillrect(sidebar_x - 4, sidebar_y - 4,
                     sidebar_w + 8, (int)ARRAYLEN(labels) * 28 + 8);
    for (index = 0; index < (int)ARRAYLEN(labels); ++index)
        sk_dm_row(sidebar_x, sidebar_y + index * 28, sidebar_w,
                  labels[index], index < SK_SLOT_COUNT &&
                  sk_equipped[index] >= 0 ?
                  sk_chips[sk_equipped[index]].name : NULL,
                  sk_sel == index);

    if (sk_stage_loaded)
    {
        rb->lcd_bitmap(sk_dm_stage_background, stage_x, stage_y,
                       SK_DM_STAGE_W, SK_DM_STAGE_H);
        rb->lcd_set_foreground(REPLACEWITHFG_COLOR);
        rb->lcd_bitmap_transparent(
            sk_dm_stage_float, stage_x, stage_y + bob[phase],
            SK_DM_STAGE_W, SK_DM_STAGE_H);
    }
    rb->lcd_set_foreground(SK_COL_RULE);
    rb->lcd_drawrect(stage_x, stage_y, SK_DM_STAGE_W, SK_DM_STAGE_H);
    sk_dm_center_text(stage_x, stage_y + SK_DM_STAGE_H + 12,
                      SK_DM_STAGE_W, "YOUR SITEKICK", SK_COL_TEXT);
}

static int sk_dm_slot_item(int item)
{
    return item == 0 ? -1 :
           sk_slot_chip_at(sk_slot_filter, item - 1);
}

static void sk_dm_draw_rows(const char * const *names, int count,
                            int current)
{
    const int rows = 15;
    const int width = SK_DM_BODY_W - 80;
    const int x = SK_DM_BODY_X + 40;
    const int y = SK_DM_CONTENT_Y + 8;
    int top = MAX(0, MIN(sk_sel - rows / 2, MAX(0, count - rows)));
    int row;

    for (row = 0; row < rows && top + row < count; ++row)
        sk_dm_row(x, y + row * 28, width, names[top + row],
                  top + row == current ? "EQUIPPED" : NULL,
                  top + row == sk_sel);
}

static void sk_dm_draw_slot(void)
{
    const int count = sk_slot_chip_count(sk_slot_filter) + 1;
    const int rows = 12;
    const int width = SK_DM_BODY_W - 80;
    const int x = SK_DM_BODY_X + 40;
    const int y = SK_DM_CONTENT_Y + 8;
    int top = MAX(0, MIN(sk_sel - rows / 2, MAX(0, count - rows)));
    int row;

    for (row = 0; row < rows && top + row < count; ++row)
    {
        int item = top + row;
        int chip = sk_dm_slot_item(item);
        const char *label = chip >= 0 ? sk_chips[chip].name : "(None)";
        const char *value = chip >= 0 &&
            sk_equipped[sk_slot_filter] == chip ? "EQUIPPED" : NULL;

        sk_dm_row(x, y + row * 34, width, label, value, item == sk_sel);
        if (chip >= 0)
            sk_draw_icon(chip, x + 4, y + row * 34 + 1);
    }
}

static void sk_dm_draw_collection(void)
{
    const int columns = 10;
    const int rows = 5;
    const int cell_w = 76;
    const int cell_h = 64;
    const int page_size = columns * rows;
    const int first = (sk_sel / page_size) * page_size;
    const int x0 = SK_DM_BODY_X + 66;
    const int y0 = SK_DM_CONTENT_Y + 142;
    char page[40];
    int slot;
    int offset;

    sk_dm_center_text(SK_DM_BODY_X, SK_DM_CONTENT_Y + 4,
                      SK_DM_BODY_W, "DRAG CHIPS INTO AN EQUIP SLOT",
                      SK_COL_TEXT);
    for (slot = 0; slot < SK_SLOT_COUNT; ++slot)
    {
        int x = SK_DM_BODY_X + 92 + slot * 88;
        int chip = sk_equipped[slot];

        rb->lcd_set_foreground(SK_COL_RULE);
        rb->lcd_fillrect(x - 3, SK_DM_CONTENT_Y + 39, 70, 70);
        rb->lcd_set_foreground(LCD_RGBPACK(247, 240, 249));
        rb->lcd_fillrect(x, SK_DM_CONTENT_Y + 42, 64, 64);
        if (chip >= 0)
            sk_draw_icon(chip, x + 16, SK_DM_CONTENT_Y + 58);
        sk_dm_center_text(x - 5, SK_DM_CONTENT_Y + 114, 74,
                          sk_slot_labels[slot], SK_COL_TEXT);
    }

    for (offset = 0; offset < page_size; ++offset)
    {
        int item = first + offset;
        int x = x0 + (offset % columns) * cell_w;
        int y = y0 + (offset / columns) * cell_h;
        bool owned;

        if (item >= sk_chip_count)
            break;
        owned = sk_owned_get(item);
        rb->lcd_set_foreground(item == sk_sel ?
                               SK_COL_YELLOW : SK_COL_RULE);
        rb->lcd_fillrect(x - 3, y - 3, 38, 38);
        rb->lcd_set_foreground(LCD_RGBPACK(247, 240, 249));
        rb->lcd_fillrect(x, y, 32, 32);
        if (owned)
            sk_draw_icon(item, x, y);
        sk_dm_center_text(x - 18, y + 39, 70,
                          owned ? sk_chips[item].name : "LOCKED",
                          owned ? SK_COL_TEXT : SK_COL_DIM);
    }
    rb->snprintf(page, sizeof(page), "CHIPENDIUM  %d-%d OF %d",
                 first + 1, MIN(first + page_size, sk_chip_count),
                 sk_chip_count);
    sk_dm_center_text(SK_DM_BODY_X, SK_DM_CONTENT_Y + 126,
                      SK_DM_BODY_W, page, SK_COL_TEXT);
}

static void sk_dm_draw_dump(void)
{
    char text[96];
    int x = SK_DM_BODY_X + 180;
    int y = SK_DM_CONTENT_Y + 54;
    int width = SK_DM_BODY_W - 360;

    rb->lcd_set_foreground(SK_COL_ORANGE);
    rb->lcd_fillrect(x, y, width, 300);
    rb->lcd_set_foreground(SK_COL_RULE);
    rb->lcd_drawrect(x, y, width, 300);
    sk_dm_center_text(x, y + 22, width, "THE CHIP DUMP", SK_COL_WHITE);
    if (sk_dump_index >= 0 && sk_dump_index < sk_chip_count)
    {
        sk_draw_icon(sk_dump_index, x + 42, y + 94);
        sk_dm_text(x + 98, y + 92, sk_chips[sk_dump_index].name,
                   SK_COL_TEXT);
        rb->snprintf(text, sizeof(text), "%s chip",
                     sk_rarity_labels[sk_chips[sk_dump_index].rarity]);
        sk_dm_text(x + 98, y + 118, text, SK_COL_TEXT);
        sk_dm_center_text(x, y + 210, width,
                          "Press Enter to rescue this chip", SK_COL_TEXT);
    }
    else
    {
        sk_dm_center_text(x, y + 110, width,
                          "Nothing has washed in yet.", SK_COL_TEXT);
        sk_dm_center_text(x, y + 150, width,
                          "COMMON 70%  RARE 25%  LEGENDARY 5%",
                          SK_COL_TEXT);
    }
}

static void sk_dm_draw_information(void)
{
    char lines[8][96];
    int count = 0;
    int index;
    int y = SK_DM_CONTENT_Y + 42;

    if (sk_scene == SK_SCENE_INBOX)
    {
        rb->strlcpy(lines[count++], "TRADING POST", sizeof(lines[0]));
        rb->strlcpy(lines[count++],
                    "Trades and drops are settled when RockPod syncs.",
                    sizeof(lines[0]));
        rb->snprintf(lines[count++], sizeof(lines[0]),
                     "Inbox rows seen: %d", sk_inbox_lines);
        rb->snprintf(lines[count++], sizeof(lines[0]),
                     "Grants applied: %d", sk_inbox_applied);
        rb->strlcpy(lines[count++],
                    "Press Enter to offer equipped chips.",
                    sizeof(lines[0]));
    }
    else if (sk_scene == SK_SCENE_GAMES)
    {
        static const char * const games[] =
            { "Beat Bounce", "Chip Match", "Ooze Catch" };
        static const char * const kinds[] =
            { "TIMING", "MEMORY", "CATCHING" };

        for (index = 0; index < 3; ++index)
            sk_dm_row(SK_DM_BODY_X + 120, y + index * 74,
                      SK_DM_BODY_W - 240, games[index],
                      kinds[index], sk_sel == index);
        return;
    }
    else
    {
        rb->snprintf(lines[count++], sizeof(lines[0]),
                     "Chips owned: %d of %d", sk_owned_count(),
                     sk_chip_count);
        rb->snprintf(lines[count++], sizeof(lines[0]), "XP: %lu",
                     (unsigned long)sk_xp);
        rb->snprintf(lines[count++], sizeof(lines[0]), "Coins: %lu",
                     (unsigned long)sk_coins);
        rb->snprintf(lines[count++], sizeof(lines[0]), "Color: %s",
                     sk_body_color_names[sk_body_color]);
        rb->snprintf(lines[count++], sizeof(lines[0]), "Background: %s",
                     sk_background_names[sk_background]);
    }

    for (index = 0; index < count; ++index)
        sk_dm_center_text(SK_DM_BODY_X, y + index * 44,
                          SK_DM_BODY_W, lines[index],
                          index == 0 ? SK_COL_ORANGE : SK_COL_TEXT);
}

static int sk_dm_hit_equip_slot(int x, int y)
{
    int slot;

    if (sk_scene != SK_SCENE_COLLECTION)
        return -1;
    for (slot = 0; slot < SK_SLOT_COUNT; ++slot)
    {
        int left = SK_DM_BODY_X + 92 + slot * 88;

        if (x >= left - 3 && x < left + 67 &&
            y >= SK_DM_CONTENT_Y + 39 &&
            y < SK_DM_CONTENT_Y + 109)
            return slot;
    }
    return -1;
}

static int sk_dm_hit_collection_chip(int x, int y)
{
    const int columns = 10;
    const int cell_w = 76;
    const int cell_h = 64;
    const int page_size = 50;
    const int x0 = SK_DM_BODY_X + 66;
    const int y0 = SK_DM_CONTENT_Y + 142;
    int column;
    int row;
    int item;

    if (sk_scene != SK_SCENE_COLLECTION || x < x0 || y < y0)
        return -1;
    column = (x - x0) / cell_w;
    row = (y - y0) / cell_h;
    if (column < 0 || column >= columns || row < 0 || row >= 5)
        return -1;
    item = (sk_sel / page_size) * page_size + row * columns + column;
    return item < sk_chip_count ? item : -1;
}

static void sk_dm_activate_pointer_target(int x, int y)
{
    int tab_w = SK_DM_BODY_W / 5;

    if (y >= SK_DM_TAB_Y && y < SK_DM_TAB_Y + SK_DM_TABS_H &&
        x >= SK_DM_BODY_X && x < SK_DM_BODY_X + SK_DM_BODY_W)
    {
        int tab = (x - SK_DM_BODY_X) / tab_w;

        sk_scene = tab == 0 ? SK_SCENE_WORKSHOP :
                   tab == 1 ? SK_SCENE_COLLECTION :
                   tab == 2 ? SK_SCENE_STATS :
                   tab == 3 ? SK_SCENE_INBOX : SK_SCENE_GAMES;
        sk_sel = 0;
        sk_top = 0;
        sk_play(SK_SND_SELECT);
        return;
    }

    if (sk_scene == SK_SCENE_WORKSHOP)
    {
        int top = SK_DM_CONTENT_Y + 10;
        int row = (y - top) / 28;

        if (x >= SK_DM_BODY_X + 16 && x < SK_DM_BODY_X + 264 &&
            y >= top && row >= 0 && row < SK_WORKSHOP_ITEMS)
        {
            sk_sel = row;
            sk_handle_select();
        }
    }
    else if (sk_scene == SK_SCENE_COLLECTION)
    {
        int chip = sk_dm_hit_collection_chip(x, y);

        if (chip >= 0)
            sk_sel = chip;
    }
}

static bool sk_dm_poll_pointer(void)
{
    char record[14];
    unsigned int buttons;
    unsigned int changed;
    long now = *rb->current_tick;
    int x;
    int y;
    int fd;

    if (!sk_desktop_mode ||
        TIME_BEFORE(now, sk_dm_pointer_poll_tick + MAX(1, HZ / 50)))
        return false;
    sk_dm_pointer_poll_tick = now;
    fd = rb->open(ROCKBOX_DIR "/host-pointer", O_RDONLY);
    if (fd < 0)
        return false;
    if (rb->read(fd, record, 13) != 13)
    {
        rb->close(fd);
        return false;
    }
    rb->close(fd);
    if (record[4] != ' ' || record[9] != ' ')
        return false;
    record[4] = '\0';
    record[9] = '\0';
    record[12] = '\0';
    x = MAX(0, MIN(LCD_WIDTH - 1, rb->atoi(record)));
    y = MAX(0, MIN(LCD_HEIGHT - 1, rb->atoi(record + 5)));
    buttons = (unsigned int)rb->atoi(record + 10);
    changed = buttons ^ sk_dm_pointer_buttons;
    sk_dm_pointer_active = true;
    sk_dm_pointer_x = x;
    sk_dm_pointer_y = y;

    if ((changed & 1u) && (buttons & 1u))
    {
        int slot = sk_dm_hit_equip_slot(x, y);
        int chip = sk_dm_hit_collection_chip(x, y);

        sk_dm_drag_chip = -1;
        sk_dm_drag_source_slot = -1;
        if (slot >= 0 && sk_equipped[slot] >= 0)
        {
            sk_dm_drag_chip = sk_equipped[slot];
            sk_dm_drag_source_slot = slot;
        }
        else if (chip >= 0)
        {
            sk_sel = chip;
            if (sk_owned_get(chip) && sk_chips[chip].worn)
                sk_dm_drag_chip = chip;
        }
    }
    else if ((changed & 1u) && !(buttons & 1u))
    {
        if (sk_dm_drag_chip >= 0)
        {
            int target = sk_dm_hit_equip_slot(x, y);

            if (target >= 0)
            {
                sk_equip(target, sk_dm_drag_chip);
                sk_write_save();
                sk_play(SK_SND_SELECT);
            }
            else if (sk_dm_drag_source_slot >= 0 &&
                     sk_dm_hit_collection_chip(x, y) >= 0)
            {
                sk_equip(sk_dm_drag_source_slot, -1);
                sk_write_save();
                sk_play(SK_SND_BACK);
            }
        }
        else
        {
            sk_dm_activate_pointer_target(x, y);
        }
        sk_dm_drag_chip = -1;
        sk_dm_drag_source_slot = -1;
    }
    sk_dm_pointer_buttons = buttons;
    return true;
}

static void sk_dm_draw_pointer(void)
{
    int x;
    int y;
    int row;

    if (!sk_dm_pointer_active)
        return;
    x = sk_dm_pointer_x;
    y = sk_dm_pointer_y;
    if (sk_dm_drag_chip >= 0)
    {
        rb->lcd_set_foreground(SK_COL_YELLOW);
        rb->lcd_fillrect(x - 20, y - 20, 40, 40);
        sk_draw_cached_icon(sk_dm_drag_chip, x - 16, y - 16);
    }

    rb->lcd_set_foreground(LCD_RGBPACK(0, 0, 0));
    for (row = 0; row < 18; ++row)
        rb->lcd_hline(x, x + row / 2 + 1, y + row);
    rb->lcd_set_foreground(SK_COL_WHITE);
    for (row = 1; row < 15; ++row)
        rb->lcd_hline(x + 1, x + row / 2, y + row);
}

static void sk_draw_desktop(const char *title)
{
    sk_dm_shell(title);
    switch (sk_scene)
    {
    case SK_SCENE_SLOT:
        sk_dm_draw_slot();
        break;
    case SK_SCENE_COLLECTION:
        sk_dm_draw_collection();
        break;
    case SK_SCENE_DUMP:
        sk_dm_draw_dump();
        break;
    case SK_SCENE_COLOR:
        sk_dm_draw_rows(sk_body_color_names, SK_BODY_COLOR_COUNT,
                        sk_body_color);
        break;
    case SK_SCENE_BACKGROUND:
        sk_dm_draw_rows(sk_background_names, SK_BACKGROUND_COUNT,
                        sk_background);
        break;
    case SK_SCENE_GAMES:
    case SK_SCENE_STATS:
    case SK_SCENE_INBOX:
        sk_dm_draw_information();
        break;
    default:
        sk_dm_draw_workshop();
        break;
    }
    sk_dm_footer();
    sk_dm_draw_pointer();
    rb->lcd_update();
}
#endif

/* One stock-style list row: blue selection bar, white text, right chevron. */
static void sk_draw_row(int x, int y, int w, const char *text,
                        const char *value, bool selected, bool chevron,
                        bool dim)
{
    int tw, th;

    if (selected)
    {
        sk_vgradient(x, y, w, SK_ROW_H, SK_COL_SEL_TOP, SK_COL_SEL_BOT);
        rb->lcd_set_foreground(SK_COL_SEL_TEXT);
    }
    else
    {
        rb->lcd_set_foreground(dim ? SK_COL_DIM : SK_COL_TEXT);
    }

    rb->lcd_getstringsize((const unsigned char *)text, &tw, &th);
    sk_putsxy(x + 8, y + (SK_ROW_H - th) / 2,
              (const unsigned char *)text);

    if (value)
    {
        int vw;

        rb->lcd_getstringsize((const unsigned char *)value, &vw, &th);
        sk_putsxy(x + w - vw - (chevron ? 16 : 8),
                  y + (SK_ROW_H - th) / 2,
                  (const unsigned char *)value);
    }

    if (chevron)
    {
        int cxx = x + w - 11;
        int cyy = y + SK_ROW_H / 2;
        int i;

        /* Two diagonal strokes meeting at the tip, stock list style. */
        for (i = 0; i < 4; ++i)
        {
            rb->lcd_drawpixel(cxx + i, cyy - 4 + i);
            rb->lcd_drawpixel(cxx + i, cyy + 4 - i);
        }
    }

    if (!selected)
    {
        rb->lcd_set_foreground(SK_COL_RULE);
        rb->lcd_hline(x + 6, x + w - 6, y + SK_ROW_H - 1);
    }
}

static void sk_clear_content(void)
{
    rb->lcd_set_foreground(SK_COL_BG);
    rb->lcd_fillrect(0, SK_CONTENT_Y, LCD_WIDTH, SK_CONTENT_H);
}

static void sk_scroll_into_view(int count, int rows)
{
    if (sk_sel < 0)
        sk_sel = 0;
    if (sk_sel >= count)
        sk_sel = count > 0 ? count - 1 : 0;
    if (sk_sel < sk_top)
        sk_top = sk_sel;
    if (sk_sel >= sk_top + rows)
        sk_top = sk_sel - rows + 1;
    if (sk_top < 0)
        sk_top = 0;
}

/* ---------------------------------------------------------------------- */
/* scenes                                                                   */
/* ---------------------------------------------------------------------- */

static int sk_slot_chip_count(int slot)
{
    int index, total = 0;

    (void)slot;
    for (index = 0; index < sk_chip_count; ++index)
        if (sk_chips[index].worn && sk_owned_get(index))
            total++;
    return total;
}

static int sk_slot_chip_at(int slot, int nth)
{
    int index, seen = 0;

    (void)slot;
    for (index = 0; index < sk_chip_count; ++index)
    {
        if (!sk_chips[index].worn || !sk_owned_get(index))
            continue;
        if (seen == nth)
            return index;
        seen++;
    }
    return -1;
}

static uint32_t sk_now(void)
{
    time_t now = rb->mktime(rb->get_time());

    return now > 0 ? (uint32_t)now : 0;
}

static int sk_random_unowned_chip(int rarity)
{
    int index, count = 0, pick;

    for (index = 0; index < sk_chip_count; ++index)
        if (!sk_owned_get(index) &&
            (rarity < 0 || sk_chips[index].rarity == rarity))
            count++;
    if (count == 0)
        return -1;

    pick = rb->rand() % count;
    for (index = 0; index < sk_chip_count; ++index)
    {
        if (sk_owned_get(index) ||
            (rarity >= 0 && sk_chips[index].rarity != rarity))
            continue;
        if (pick-- == 0)
            return index;
    }
    return -1;
}

/* The Dump selects a rarity first, then a chip within that rarity. This keeps
 * the authored catalogue mix from making legendary chips too common. */
static bool sk_dump_service(void)
{
    uint32_t now = sk_now();
    int roll, rarity;

    if (sk_dump_index >= 0 && sk_dump_index < sk_chip_count &&
        !sk_owned_get(sk_dump_index))
        return false;
    sk_dump_index = -1;
    if (sk_dump_ready_at && now && now < sk_dump_ready_at)
        return false;

    roll = rb->rand() % 100;
    rarity = roll < 70 ? SK_RARITY_COMMON :
             (roll < 95 ? SK_RARITY_RARE : SK_RARITY_LEGENDARY);
    sk_dump_index = sk_random_unowned_chip(rarity);
    if (sk_dump_index < 0)
        sk_dump_index = sk_random_unowned_chip(-1);
    if (sk_dump_index < 0)
        return false;

    sk_dump_ready_at = 0;
    sk_dirty = true;
    sk_write_save();
    return true;
}

static void sk_dump_claim(void)
{
    static const int xp_reward[SK_RARITY_COUNT] = { 5, 15, 50 };
    uint32_t now = sk_now();
    int index = sk_dump_index;

    if (index < 0 || index >= sk_chip_count || sk_owned_get(index))
    {
        sk_set_message("The Dump is empty");
        sk_play(SK_SND_BACK);
        return;
    }

    sk_owned_set(index, true);
    sk_xp += xp_reward[sk_chips[index].rarity];
    sk_dump_index = -1;
    sk_dump_ready_at = now ? now + 30 + (rb->rand() % 91) : 0;
    sk_dirty = true;
    sk_write_save();
    sk_publish_preview(true);
    sk_set_message("Chip rescued from the Dump!");
    sk_play(SK_SND_REWARD);
}

/* The shop rerolls its whole rack together every SK_SHOP_PERIOD_SEC, unlike
 * the Dump's one-at-a-time trickle. Each slot rolls a rarity with the same
 * weights as the Dump, then picks a chip the player doesn't already own and
 * that isn't already sitting in an earlier slot this rotation. */
static bool sk_shop_pick_unique(int rarity, int filled)
{
    int index, count = 0, pick, slot;

    for (index = 0; index < sk_chip_count; ++index)
    {
        if (sk_owned_get(index) ||
            (rarity >= 0 && sk_chips[index].rarity != rarity))
            continue;
        for (slot = 0; slot < filled; ++slot)
            if (sk_shop_stock[slot] == index)
                goto skip;
        count++;
skip:
        ;
    }
    if (count == 0)
        return false;

    pick = rb->rand() % count;
    for (index = 0; index < sk_chip_count; ++index)
    {
        if (sk_owned_get(index) ||
            (rarity >= 0 && sk_chips[index].rarity != rarity))
            continue;
        for (slot = 0; slot < filled; ++slot)
            if (sk_shop_stock[slot] == index)
                goto skip2;
        if (pick-- == 0)
        {
            sk_shop_stock[filled] = index;
            return true;
        }
skip2:
        ;
    }
    return false;
}

static void sk_shop_rotate(bool force)
{
    uint32_t now = sk_now();
    int slot;

    if (!force && sk_shop_ready_at && now && now < sk_shop_ready_at)
        return;

    for (slot = 0; slot < SK_SHOP_SLOTS; ++slot)
    {
        int roll = rb->rand() % 100;
        int rarity = roll < 70 ? SK_RARITY_COMMON :
                     (roll < 95 ? SK_RARITY_RARE : SK_RARITY_LEGENDARY);

        sk_shop_stock[slot] = -1;
        if (!sk_shop_pick_unique(rarity, slot))
            sk_shop_pick_unique(-1, slot);
    }
    sk_shop_ready_at = now ? now + SK_SHOP_PERIOD_SEC : 0;
    sk_dirty = true;
    sk_write_save();
}

static void sk_shop_buy(int slot)
{
    int index;
    uint32_t price;

    if (slot < 0 || slot >= SK_SHOP_SLOTS)
        return;

    index = sk_shop_stock[slot];
    if (index < 0 || index >= sk_chip_count || sk_owned_get(index))
    {
        sk_set_message("Sold out");
        sk_play(SK_SND_BACK);
        return;
    }

    price = sk_shop_prices[sk_chips[index].rarity];
    if (sk_coins < price)
    {
        sk_set_message("Not enough coins");
        sk_play(SK_SND_BACK);
        return;
    }

    sk_coins -= price;
    sk_owned_set(index, true);
    sk_shop_stock[slot] = -1;
    sk_dirty = true;
    sk_write_save();
    sk_publish_preview(true);
    sk_set_message(sk_chips[index].name);
    sk_play(SK_SND_REWARD);
}

static void sk_draw_workshop(void)
{
    unsigned background = sk_background_colors[
        sk_background < SK_BACKGROUND_COUNT ? sk_background : 0];
    int clear_x = LCD_WIDTH - 92;
    int clear_y = LCD_HEIGHT - SK_STATUS_H - 27;
    bool clear_selected = sk_sel == SK_WORKSHOP_CLEAR;

    sk_clear_content();
    rb->lcd_set_foreground(background);
    rb->lcd_fillrect(0, SK_CONTENT_Y, LCD_WIDTH, SK_CONTENT_H);
    sk_scroll_into_view(SK_WORKSHOP_ITEMS, SK_WORKSHOP_ITEMS);
    sk_draw_stage((LCD_WIDTH - SK_STAGE_W) / 2, SK_CONTENT_Y);
    if (clear_selected)
    {
        sk_vgradient(clear_x, clear_y, 84, 20, SK_COL_SEL_TOP,
                    SK_COL_SEL_BOT);
        rb->lcd_set_foreground(SK_COL_SEL_TEXT);
        sk_putsxy(clear_x + 12, clear_y + 5,
                  (const unsigned char *)"+ CLEAR");
    }
}

static void sk_draw_presets(void)
{
    int count = sk_preset_total();
    int rows = SK_ROWS_MAX;
    int i;

    sk_clear_content();
    sk_scroll_into_view(count + (count < SK_PRESET_COUNT), rows);
    for (i = 0; i < rows; ++i)
    {
        int item = sk_top + i;
        int y = SK_CONTENT_Y + i * SK_ROW_H;
        int index;

        if (item < count)
        {
            index = sk_preset_at(item);
            sk_draw_row(0, y, LCD_WIDTH, sk_presets[index].name,
                        index == sk_preset_index ? "*" : NULL,
                        item == sk_sel, false, false);
        }
        else if (item == count && count < SK_PRESET_COUNT)
        {
            sk_draw_row(0, y, LCD_WIDTH, "Save new preset", NULL,
                        item == sk_sel, false, false);
        }
        else
            break;
    }
}

static void sk_draw_appearance_picker(bool background_picker)
{
    const char * const *names = background_picker ?
        sk_background_names : sk_body_color_names;
    int count = background_picker ?
        SK_BACKGROUND_COUNT : SK_BODY_COLOR_COUNT;
    int current = background_picker ? sk_background : sk_body_color;
    int rows = SK_ROWS_MAX;
    int i;

    sk_clear_content();
    sk_scroll_into_view(count, rows);
    for (i = 0; i < rows; ++i)
    {
        int item = sk_top + i;
        int y = SK_CONTENT_Y + i * SK_ROW_H;

        if (item >= count)
            break;
        sk_draw_row(0, y, LCD_WIDTH, names[item],
                    item == current ? "*" : NULL,
                    item == sk_sel, false, false);
        if (background_picker)
        {
            rb->lcd_set_foreground(sk_background_colors[item]);
            rb->lcd_fillrect(LCD_WIDTH - 50, y + 9, 12, 12);
            rb->lcd_set_foreground(SK_COL_RULE);
            rb->lcd_drawrect(LCD_WIDTH - 51, y + 8, 14, 14);
        }
    }
}

static void sk_draw_slot_picker(void)
{
    int count = sk_slot_chip_count(sk_slot_filter);
    int rows = SK_ROWS_MAX;
    int i;

    sk_clear_content();
    sk_scroll_into_view(count + 1, rows);

    for (i = 0; i < rows; ++i)
    {
        int item = sk_top + i;
        int y = SK_CONTENT_Y + i * SK_ROW_H;
        bool sel = (item == sk_sel);

        if (item == 0)
        {
            sk_draw_row(0, y, LCD_WIDTH, "(None)", NULL, sel, false, false);
        }
        else if (item <= count)
        {
            int index = sk_slot_chip_at(sk_slot_filter, item - 1);

            if (index >= 0)
            {
                bool worn = (sk_equipped[sk_slot_filter] == index);

                if (sel)
                    sk_vgradient(0, y, LCD_WIDTH, SK_ROW_H,
                                 SK_COL_SEL_TOP, SK_COL_SEL_BOT);
                sk_draw_icon(index, 4, y + (SK_ROW_H - SK_ICON_PX) / 2);
                rb->lcd_set_foreground(sel ? SK_COL_SEL_TEXT : SK_COL_TEXT);
                sk_putsxy(SK_ICON_PX + 14, y + 10,
                          (const unsigned char *)sk_chips[index].name);
                if (worn)
                {
                    rb->lcd_set_foreground(sel ? SK_COL_SEL_TEXT :
                                           SK_COL_DIM);
                    sk_putsxy(LCD_WIDTH - 22, y + 10,
                              (const unsigned char *)"*");
                }
            }
        }
    }

}

static void sk_draw_collection(void)
{
    int rows = SK_ROWS_MAX;
    int i;

    sk_clear_content();
    sk_scroll_into_view(sk_chip_count, rows);

    for (i = 0; i < rows; ++i)
    {
        int item = sk_top + i;
        int y = SK_CONTENT_Y + i * SK_ROW_H;
        bool sel = (item == sk_sel);
        bool owned;

        if (item >= sk_chip_count)
            break;
        owned = sk_owned_get(item);

        if (sel)
            sk_vgradient(0, y, LCD_WIDTH, SK_ROW_H,
                         SK_COL_SEL_TOP, SK_COL_SEL_BOT);
        if (owned)
            sk_draw_icon(item, 4, y + (SK_ROW_H - SK_ICON_PX) / 2);

        rb->lcd_set_foreground(sel ? SK_COL_SEL_TEXT :
                               (owned ? SK_COL_TEXT : SK_COL_DIM));
        sk_putsxy(SK_ICON_PX + 14, y + 10,
                  (const unsigned char *)sk_chips[item].name);
        sk_putsxy(LCD_WIDTH - 96, y + 10, (const unsigned char *)
                  (owned ? sk_rarity_labels[sk_chips[item].rarity]
                         : "Locked"));
        if (!sel)
        {
            rb->lcd_set_foreground(SK_COL_RULE);
            rb->lcd_hline(6, LCD_WIDTH - 6, y + SK_ROW_H - 1);
        }
    }
}

static void sk_draw_dump(void)
{
    char buf[80];
    int y = SK_CONTENT_Y + 12;

    sk_clear_content();
    rb->lcd_set_foreground(SK_COL_ORANGE);
    sk_putsxy(12, y, (const unsigned char *)"THE CHIP DUMP");
    y += 26;

    if (sk_dump_index >= 0 && sk_dump_index < sk_chip_count)
    {
        const struct sk_chip *chip = &sk_chips[sk_dump_index];

        rb->lcd_set_foreground(SK_COL_PANE);
        rb->lcd_fillrect(12, y, LCD_WIDTH - 24, 64);
        rb->lcd_set_foreground(SK_COL_RULE);
        rb->lcd_drawrect(12, y, LCD_WIDTH - 24, 64);
        sk_draw_icon(sk_dump_index, 24, y + 16);

        rb->lcd_set_foreground(SK_COL_TEXT);
        sk_putsxy(70, y + 12, (const unsigned char *)chip->name);
        rb->snprintf(buf, sizeof(buf), "%s chip",
                     sk_rarity_labels[chip->rarity]);
        sk_putsxy(70, y + 34, (const unsigned char *)buf);
        y += 80;
        rb->lcd_set_foreground(SK_COL_TEXT);
        sk_putsxy(12, y, (const unsigned char *)
                  "Select to rescue this chip");
    }
    else
    {
        uint32_t now = sk_now();
        uint32_t wait = sk_dump_ready_at && now < sk_dump_ready_at ?
                        sk_dump_ready_at - now : 0;

        rb->lcd_set_foreground(SK_COL_DIM);
        sk_putsxy(12, y, (const unsigned char *)
                  "Nothing has washed in yet.");
        y += 24;
        if (wait)
            rb->snprintf(buf, sizeof(buf), "Next scan in %lu sec",
                         (unsigned long)wait);
        else
            rb->strlcpy(buf, "Scanning for loose chips...", sizeof(buf));
        sk_putsxy(12, y, (const unsigned char *)buf);
        y += 36;
        rb->lcd_set_foreground(SK_COL_TEXT);
        sk_putsxy(12, y, (const unsigned char *)
                  "Common 70%   Rare 25%");
        y += 20;
        sk_putsxy(12, y, (const unsigned char *)
                  "Legendary 5%");
    }
}

static void sk_draw_stats(void)
{
    char buf[64];
    int y = SK_CONTENT_Y + 8;
    int slot, legendary = 0, rare = 0, index;

    sk_clear_content();
    for (index = 0; index < sk_chip_count; ++index)
    {
        if (!sk_owned_get(index))
            continue;
        if (sk_chips[index].rarity == SK_RARITY_LEGENDARY)
            legendary++;
        else if (sk_chips[index].rarity == SK_RARITY_RARE)
            rare++;
    }

    rb->lcd_set_foreground(SK_COL_TEXT);
    rb->snprintf(buf, sizeof(buf), "Chips owned: %d of %d", sk_owned_count(),
                 sk_chip_count);
    sk_putsxy(10, y, (const unsigned char *)buf);
    y += 20;
    rb->snprintf(buf, sizeof(buf), "Rare: %d    Legendary: %d", rare,
                 legendary);
    sk_putsxy(10, y, (const unsigned char *)buf);
    y += 20;
    rb->snprintf(buf, sizeof(buf), "XP: %lu", (unsigned long)sk_xp);
    sk_putsxy(10, y, (const unsigned char *)buf);
    y += 20;
    rb->snprintf(buf, sizeof(buf), "Coins: %lu", (unsigned long)sk_coins);
    sk_putsxy(10, y, (const unsigned char *)buf);
    y += 20;
    rb->snprintf(buf, sizeof(buf), "Color: %s",
                 sk_body_color_names[sk_body_color]);
    sk_putsxy(10, y, (const unsigned char *)buf);
    y += 20;
    rb->snprintf(buf, sizeof(buf), "Background: %s",
                 sk_background_names[sk_background]);
    sk_putsxy(10, y, (const unsigned char *)buf);
    y += 24;

    rb->lcd_set_foreground(SK_COL_DIM);
    sk_putsxy(10, y, (const unsigned char *)"Equipped");
    y += 18;
    rb->lcd_set_foreground(SK_COL_TEXT);
    for (slot = 0; slot < SK_SLOT_COUNT; ++slot)
    {
        int index2 = sk_equipped[slot];

        if (index2 < 0)
            continue;
        rb->snprintf(buf, sizeof(buf), "%s: %s", sk_slot_labels[slot],
                     sk_chips[index2].name);
        sk_putsxy(18, y, (const unsigned char *)buf);
        y += 16;
        if (y > SK_CONTENT_Y + SK_CONTENT_H - 16)
            break;
    }
}

static void sk_draw_inbox(void)
{
    char buf[64];
    int y = SK_CONTENT_Y + 8;

    sk_clear_content();
    rb->lcd_set_foreground(SK_COL_TEXT);
    sk_putsxy(10, y, (const unsigned char *)"Trading Post");
    y += 22;
    rb->lcd_set_foreground(SK_COL_DIM);
    sk_putsxy(10, y, (const unsigned char *)
              "Trades and drops are settled by");
    y += 16;
    sk_putsxy(10, y, (const unsigned char *)
              "RockPod when you sync.");
    y += 24;

    rb->lcd_set_foreground(SK_COL_TEXT);
    rb->snprintf(buf, sizeof(buf), "Inbox rows seen: %d", sk_inbox_lines);
    sk_putsxy(10, y, (const unsigned char *)buf);
    y += 18;
    rb->snprintf(buf, sizeof(buf), "Grants applied: %d", sk_inbox_applied);
    sk_putsxy(10, y, (const unsigned char *)buf);
    y += 24;

    rb->lcd_set_foreground(SK_COL_DIM);
    sk_putsxy(10, y, (const unsigned char *)
              "Select: offer equipped chips");
}

static void sk_draw_games(void)
{
    sk_clear_content();
    sk_scroll_into_view(3, 3);
    sk_draw_row(0, SK_CONTENT_Y, LCD_WIDTH, "Beat Bounce",
                "Timing", sk_sel == 0, true, false);
    sk_draw_row(0, SK_CONTENT_Y + SK_ROW_H, LCD_WIDTH, "Chip Match",
                "Memory", sk_sel == 1, true, false);
    sk_draw_row(0, SK_CONTENT_Y + SK_ROW_H * 2, LCD_WIDTH, "Ooze Catch",
                "Catching", sk_sel == 2, true, false);

    rb->lcd_set_foreground(SK_COL_DIM);
    sk_putsxy(10, SK_CONTENT_Y + SK_ROW_H * 3 + 14,
              (const unsigned char *)"Play for XP + coins");
    sk_putsxy(10, SK_CONTENT_Y + SK_ROW_H * 3 + 32,
              (const unsigned char *)"Rewards update the right pane");
}

static void sk_draw_shop(void)
{
    char status[24];
    int rows = MIN(SK_SHOP_SLOTS, SK_ROWS_MAX - 1);
    int i;

    sk_clear_content();
    sk_scroll_into_view(SK_SHOP_SLOTS, rows);

    for (i = 0; i < rows; ++i)
    {
        int slot = sk_top + i;
        int y = SK_CONTENT_Y + i * SK_ROW_H;
        bool sel = (slot == sk_sel);
        int index;

        if (slot >= SK_SHOP_SLOTS)
            break;
        index = sk_shop_stock[slot];

        if (sel)
            sk_vgradient(0, y, LCD_WIDTH, SK_ROW_H,
                         SK_COL_SEL_TOP, SK_COL_SEL_BOT);

        if (index >= 0 && index < sk_chip_count && !sk_owned_get(index))
        {
            sk_draw_icon(index, 4, y + (SK_ROW_H - SK_ICON_PX) / 2);
            rb->lcd_set_foreground(sel ? SK_COL_SEL_TEXT : SK_COL_TEXT);
            sk_putsxy(SK_ICON_PX + 14, y + 10,
                      (const unsigned char *)sk_chips[index].name);
            rb->snprintf(status, sizeof(status), "%lu coins",
                         (unsigned long)sk_shop_prices[
                             sk_chips[index].rarity]);
        }
        else
        {
            rb->lcd_set_foreground(sel ? SK_COL_SEL_TEXT : SK_COL_DIM);
            sk_putsxy(SK_ICON_PX + 14, y + 10,
                      (const unsigned char *)"Sold out");
            status[0] = '\0';
        }
        if (status[0])
        {
            int vw, th;

            rb->lcd_getstringsize((const unsigned char *)status, &vw, &th);
            sk_putsxy(LCD_WIDTH - vw - 8, y + 10,
                      (const unsigned char *)status);
        }
        if (!sel)
        {
            rb->lcd_set_foreground(SK_COL_RULE);
            rb->lcd_hline(6, LCD_WIDTH - 6, y + SK_ROW_H - 1);
        }
    }

    {
        uint32_t now = sk_now();
        uint32_t wait = sk_shop_ready_at && now < sk_shop_ready_at ?
                        sk_shop_ready_at - now : 0;
        char footer[40];

        if (wait)
            rb->snprintf(footer, sizeof(footer), "Restocks in %luh %02lum",
                         (unsigned long)(wait / 3600),
                         (unsigned long)((wait / 60) % 60));
        else
            rb->strlcpy(footer, "Restocking...", sizeof(footer));
        rb->lcd_set_foreground(SK_COL_DIM);
        sk_putsxy(10, SK_CONTENT_Y + rows * SK_ROW_H + 12,
                  (const unsigned char *)footer);
    }
}

static void sk_center_text(int y, const char *text, unsigned color)
{
    int w, h;

    rb->lcd_getstringsize((const unsigned char *)text, &w, &h);
    rb->lcd_set_foreground(color);
    sk_putsxy((LCD_WIDTH - w) / 2, y, (const unsigned char *)text);
}

static void sk_draw_game_footer(const char *text)
{
    int y = LCD_HEIGHT - SK_STATUS_H;

    sk_vgradient(0, y, LCD_WIDTH, SK_STATUS_H,
                 SK_COL_STATUS_TOP, SK_COL_STATUS_BOT);
    rb->lcd_set_foreground(SK_COL_RULE);
    rb->lcd_hline(0, LCD_WIDTH - 1, y);
    sk_center_text(y + 3, text, SK_COL_TEXT);
}

static void sk_award_game(const char *name, uint32_t xp, uint32_t coins)
{
    char message[64];

    sk_xp += xp;
    sk_coins += coins;
    sk_dirty = true;
    sk_write_save();
    /* XP and coin text lives in the pane background, so it must be
     * republished even though the paperdoll itself did not change. */
    sk_publish_preview(true);
    rb->snprintf(message, sizeof(message), "%s: +%lu XP +%lu coins",
                 name, (unsigned long)xp, (unsigned long)coins);
    sk_set_message(message);
    sk_play(SK_SND_REWARD);
}

/* The three matching faces and Beat Bounce marker are preserved original
 * Sitekick chip tokens. Load their shared atlas once before entering a game;
 * every game frame after that is cached drawing only. */
static bool sk_prepare_game_icons(void)
{
    static const uint16_t ids[3] = { 15, 18, 21 };
    int page = -1;
    int i;

    for (i = 0; i < 3; ++i)
    {
        sk_game_icons[i] = sk_find_chip(ids[i]);
        if (sk_game_icons[i] < 0)
            return false;
        if (page < 0)
            page = sk_chips[sk_game_icons[i]].page;
        else if (sk_chips[sk_game_icons[i]].page != page)
            return false;
    }
    return sk_load_icon_page(page);
}

static void sk_draw_beat_bounce(int round, int score, int combo,
                                int marker_x, int target_x, int target_w,
                                bool authentic_icons)
{
    char text[64];
    int rail_x = 30;
    int rail_y = SK_CONTENT_Y + 91;
    int rail_w = LCD_WIDTH - 60;

#if LCD_WIDTH >= 1920
    if (sk_desktop_mode)
    {
        rail_x = SK_DM_BODY_X + 70;
        rail_y = SK_DM_CONTENT_Y + 218;
        rail_w = SK_DM_BODY_W - 140;
        sk_dm_shell("Beat Bounce");
        rb->snprintf(text, sizeof(text), "BEAT %d/10    SCORE %d",
                     round + 1, score);
        sk_dm_center_text(SK_DM_BODY_X, SK_DM_CONTENT_Y + 55,
                          SK_DM_BODY_W, text, SK_COL_TEXT);
        rb->snprintf(text, sizeof(text), "COMBO x%d", combo);
        sk_dm_center_text(SK_DM_BODY_X, SK_DM_CONTENT_Y + 95,
                          SK_DM_BODY_W, text,
                          combo ? SK_COL_ORANGE : SK_COL_DIM);
        rb->lcd_set_foreground(SK_COL_RULE);
        rb->lcd_fillrect(rail_x, rail_y, rail_w, 12);
        rb->lcd_set_foreground(SK_COL_YELLOW);
        rb->lcd_fillrect(target_x, rail_y - 18, target_w, 48);
        rb->lcd_set_foreground(SK_COL_ORANGE);
        rb->lcd_drawrect(target_x, rail_y - 18, target_w, 48);
        if (authentic_icons)
            sk_draw_cached_icon(sk_game_icons[0], marker_x - 12,
                                rail_y - 10);
        else
        {
            rb->lcd_set_foreground(SK_COL_TITLE_TOP);
            rb->lcd_fillrect(marker_x, rail_y - 18, 12, 48);
        }
        sk_dm_center_text(SK_DM_BODY_X, SK_DM_CONTENT_Y + 320,
                          SK_DM_BODY_W,
                          "Hit Enter while the chip is in the ooze zone",
                          SK_COL_TEXT);
        sk_dm_footer();
        rb->lcd_update();
        return;
    }
#endif

    sk_draw_title("Beat Bounce");
    sk_clear_content();

    rb->snprintf(text, sizeof(text), "BEAT %d/10   SCORE %d", round + 1,
                 score);
    sk_center_text(SK_CONTENT_Y + 14, text, SK_COL_TEXT);
    rb->snprintf(text, sizeof(text), "COMBO x%d", combo);
    sk_center_text(SK_CONTENT_Y + 38, text,
                   combo ? SK_COL_ORANGE : SK_COL_DIM);

    /* Ooze rail: a cel-shaded gradient tube with a chunky outline and a
     * highlight line, instead of a flat filled bar. */
    sk_vgradient(rail_x, rail_y, rail_w, 10, SK_COL_STATUS_TOP,
                SK_COL_STATUS_BOT);
    rb->lcd_set_foreground(SK_COL_RULE);
    rb->lcd_drawrect(rail_x, rail_y, rail_w, 10);
    rb->lcd_set_foreground(SK_COL_WHITE);
    rb->lcd_hline(rail_x + 2, rail_x + rail_w - 3, rail_y + 2);

    sk_vgradient(target_x, rail_y - 10, target_w, 28, SK_COL_SEL_TOP,
                SK_COL_SEL_BOT);
    rb->lcd_set_foreground(SK_COL_RULE);
    rb->lcd_drawrect(target_x, rail_y - 10, target_w, 28);

    if (authentic_icons)
        sk_draw_cached_icon(sk_game_icons[0], marker_x - 12, rail_y - 12);
    else
    {
        rb->lcd_set_foreground(SK_COL_TITLE_TOP);
        rb->lcd_fillrect(marker_x - 4, rail_y - 12, 16, 32);
        rb->lcd_set_foreground(SK_COL_RULE);
        rb->lcd_drawrect(marker_x - 4, rail_y - 12, 16, 32);
        rb->lcd_set_foreground(SK_COL_WHITE);
        rb->lcd_fillrect(marker_x - 1, rail_y - 8, 5, 5);
    }

    sk_center_text(SK_CONTENT_Y + 132, "Hit SELECT in the ooze zone",
                   SK_COL_TEXT);
    sk_draw_game_footer("MENU exits - completed games pay");
    rb->lcd_update();
}

static void sk_run_beat_bounce(void)
{
    const int rounds = 10;
    const long period = HZ;
    int rail_x = 30;
    int rail_w = LCD_WIDTH - 60;
    const int marker_w = 9;
    int target_w = 48;
    int target_x;
    int round = 0;
    int score = 0;
    int combo = 0;
    bool authentic_icons = sk_prepare_game_icons();
    long round_start = *rb->current_tick;

#if LCD_WIDTH >= 1920
    if (sk_desktop_mode)
    {
        rail_x = SK_DM_BODY_X + 70;
        rail_w = SK_DM_BODY_W - 140;
        target_w = 86;
    }
#endif
    target_x = rail_x + (rail_w - target_w) / 2;

    while (round < rounds)
    {
        long elapsed = *rb->current_tick - round_start;
        int marker_x;
        int button;

        if (elapsed >= period)
        {
            combo = 0;
            round++;
            round_start = *rb->current_tick;
            sk_play(SK_SND_BACK);
            continue;
        }

        marker_x = rail_x +
            (int)(elapsed * (rail_w - marker_w) / period);
        sk_draw_beat_bounce(round, score, combo, marker_x,
                            target_x, target_w, authentic_icons);
        button = pluginlib_getaction(MAX(1, HZ / 30), plugin_contexts,
                                     ARRAYLEN(plugin_contexts));

        if (button == PLA_SELECT || button == PLA_RIGHT)
        {
            int marker_center = marker_x + marker_w / 2;
            int target_center = target_x + target_w / 2;
            int distance = marker_center - target_center;

            if (distance < 0)
                distance = -distance;
            if (distance <= target_w / 2)
            {
                combo++;
                score += 2 + MIN(combo, 4);
                sk_play(SK_SND_SELECT);
            }
            else
            {
                combo = 0;
                sk_play(SK_SND_BACK);
            }
            round++;
            round_start = *rb->current_tick;
        }
        else if (button == PLA_CANCEL || button == PLA_LEFT ||
                 button == PLA_EXIT)
        {
            sk_set_message("Beat Bounce cancelled");
            sk_play(SK_SND_BACK);
            return;
        }
        else if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
        {
            sk_game_usb_connected = true;
            return;
        }
    }

    sk_award_game("Beat Bounce", 8 + score, 12 + score * 2);
}

static void sk_draw_match_card(int index, int selected, int value,
                               bool visible, bool matched,
                               bool authentic_icons)
{
    static const unsigned face_top[3] =
    {
        SK_COL_SEL_TOP, SK_COL_TITLE_TOP, SK_COL_STATUS_TOP
    };
    static const unsigned face_bot[3] =
    {
        SK_COL_SEL_BOT, SK_COL_TITLE_BOT, SK_COL_STATUS_BOT
    };
    int card_w = 84;
    int card_h = 58;
    int gap = 12;
    int x = 20 + (index % 3) * (card_w + gap);
    int y = SK_CONTENT_Y + 43 + (index / 3) * (card_h + 12);
    int old_mode;

#if LCD_WIDTH >= 1920
    if (sk_desktop_mode)
    {
        card_w = 170;
        card_h = 126;
        gap = 38;
        x = SK_DM_BODY_X + 151 +
            (index % 3) * (card_w + gap);
        y = SK_DM_CONTENT_Y + 98 +
            (index / 3) * (card_h + 32);
    }
#endif

    rb->lcd_set_foreground(selected ? SK_COL_YELLOW : SK_COL_RULE);
    rb->lcd_fillrect(x - 3, y - 3, card_w + 6, card_h + 6);
    if (visible || matched)
    {
        /* Cel-shaded gradient face plus a bright highlight strip, instead
         * of a single flat fill. */
        sk_vgradient(x, y, card_w, card_h, face_top[value], face_bot[value]);
        rb->lcd_set_foreground(SK_COL_RULE);
        rb->lcd_drawrect(x, y, card_w, card_h);
        rb->lcd_set_foreground(SK_COL_WHITE);
        rb->lcd_hline(x + 3, x + card_w - 4, y + 3);
        if (authentic_icons)
            sk_draw_cached_icon(
                sk_game_icons[value],
                x + (card_w - SK_ICON_PX) / 2,
                y + (card_h - SK_ICON_PX) / 2);
    }
    else
    {
        sk_vgradient(x, y, card_w, card_h,
                     SK_COL_TITLE_TOP, SK_COL_TITLE_BOT);
        rb->lcd_set_foreground(SK_COL_RULE);
        rb->lcd_drawrect(x, y, card_w, card_h);
        if (sk_logo_loaded)
        {
            old_mode = rb->lcd_get_drawmode();
            rb->lcd_set_drawmode(DRMODE_FG);
            rb->lcd_bmp_part(
                &sk_logo, 0, 0,
                x + (card_w - sk_logo.width) / 2,
                y + (card_h - sk_logo.height) / 2,
                sk_logo.width, sk_logo.height);
            rb->lcd_set_drawmode(old_mode);
        }
        else
        {
            int cx = x + card_w / 2, cy = y + card_h / 2;

            rb->lcd_set_foreground(SK_COL_PANE);
            rb->lcd_fillrect(cx - 14, cy - 11, 28, 22);
            rb->lcd_set_foreground(SK_COL_RULE);
            rb->lcd_drawrect(cx - 14, cy - 11, 28, 22);
            rb->lcd_set_foreground(SK_COL_YELLOW);
            rb->lcd_fillrect(cx - 4, cy - 4, 8, 8);
        }
    }
}

static void sk_draw_chip_match(int selected, const unsigned char *cards,
                               const bool *visible, const bool *matched,
                               int pairs, int moves, bool authentic_icons)
{
    char text[48];
    int card;

#if LCD_WIDTH >= 1920
    if (sk_desktop_mode)
    {
        sk_dm_shell("Chip Match");
        rb->snprintf(text, sizeof(text), "PAIRS %d/3    TURNS %d",
                     pairs, moves);
        sk_dm_center_text(SK_DM_BODY_X, SK_DM_CONTENT_Y + 38,
                          SK_DM_BODY_W, text, SK_COL_TEXT);
        for (card = 0; card < 6; ++card)
            sk_draw_match_card(card, selected, cards[card],
                               visible[card], matched[card],
                               authentic_icons);
        sk_dm_footer();
        rb->lcd_update();
        return;
    }
#endif

    sk_draw_title("Chip Match");
    sk_clear_content();
    rb->snprintf(text, sizeof(text), "PAIRS %d/3   TURNS %d", pairs, moves);
    sk_center_text(SK_CONTENT_Y + 12, text, SK_COL_TEXT);
    for (card = 0; card < 6; ++card)
        sk_draw_match_card(card, selected, cards[card],
                           visible[card], matched[card], authentic_icons);
    sk_draw_game_footer("WHEEL moves - SELECT flips - MENU exits");
    rb->lcd_update();
}

static void sk_run_chip_match(void)
{
    unsigned char cards[6] = { 0, 0, 1, 1, 2, 2 };
    bool visible[6] = { false, false, false, false, false, false };
    bool matched[6] = { false, false, false, false, false, false };
    int selected = 0;
    int first = -1;
    int second = -1;
    int pairs = 0;
    int moves = 0;
    long resolve_at = 0;
    bool authentic_icons = sk_prepare_game_icons();
    int i;

    for (i = 5; i > 0; --i)
    {
        int other = rb->rand() % (i + 1);
        unsigned char value = cards[i];

        cards[i] = cards[other];
        cards[other] = value;
    }

    while (pairs < 3)
    {
        int button;

        if (resolve_at && TIME_AFTER(*rb->current_tick, resolve_at))
        {
            if (cards[first] == cards[second])
            {
                matched[first] = true;
                matched[second] = true;
                pairs++;
                sk_play(SK_SND_REWARD);
            }
            visible[first] = false;
            visible[second] = false;
            first = -1;
            second = -1;
            resolve_at = 0;
            if (pairs >= 3)
                break;
        }

        sk_draw_chip_match(selected, cards, visible, matched, pairs, moves,
                           authentic_icons);
        button = pluginlib_getaction(MAX(1, HZ / 20), plugin_contexts,
                                     ARRAYLEN(plugin_contexts));

        if (!resolve_at &&
            (button == PLA_UP || button == PLA_UP_REPEAT ||
             button == PLA_SCROLL_BACK || button == PLA_SCROLL_BACK_REPEAT))
        {
            selected = (selected + 5) % 6;
            sk_play(SK_SND_NAVIGATE);
        }
        else if (!resolve_at &&
                 (button == PLA_DOWN || button == PLA_DOWN_REPEAT ||
                  button == PLA_SCROLL_FWD ||
                  button == PLA_SCROLL_FWD_REPEAT))
        {
            selected = (selected + 1) % 6;
            sk_play(SK_SND_NAVIGATE);
        }
        else if (!resolve_at &&
                 (button == PLA_SELECT || button == PLA_RIGHT))
        {
            if (!visible[selected] && !matched[selected])
            {
                visible[selected] = true;
                sk_play(SK_SND_SELECT);
                if (first < 0)
                    first = selected;
                else
                {
                    second = selected;
                    moves++;
                    resolve_at = *rb->current_tick + HZ / 2;
                }
            }
        }
        else if (button == PLA_CANCEL || button == PLA_LEFT ||
                 button == PLA_EXIT)
        {
            sk_set_message("Chip Match cancelled");
            sk_play(SK_SND_BACK);
            return;
        }
        else if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
        {
            sk_game_usb_connected = true;
            return;
        }
    }

    {
        int bonus = MAX(0, 12 - moves);

        sk_award_game("Chip Match", 18 + bonus * 2, 30 + bonus * 3);
    }
}

#define SK_OOZE_LANES   5
#define SK_OOZE_ROUNDS  12

/* A drop kind's fill gradient, catch value and label. Bad ooze costs the
 * player points and combo if caught, unlike a missed common/rare drop
 * which only breaks combo. */
static const unsigned sk_ooze_top[3] =
{
    SK_COL_STATUS_TOP, SK_COL_SEL_TOP, SK_COL_TITLE_TOP
};
static const unsigned sk_ooze_bot[3] =
{
    SK_COL_STATUS_BOT, SK_COL_SEL_BOT, SK_COL_TITLE_BOT
};
static const int sk_ooze_value[3] = { 2, 5, -3 };

static void sk_draw_ooze_catch(int round, int score, int combo,
                               int catcher_lane, int drop_lane, int drop_kind,
                               int drop_y, bool active, bool authentic_icons)
{
    char text[64];
    int margin = 20;
    int lane_w = (LCD_WIDTH - margin * 2) / SK_OOZE_LANES;
    int lane_top = SK_CONTENT_Y + 44;
    int lane_bottom = SK_CONTENT_Y + 150;
    int catcher_w = lane_w - 10;
    int catcher_h = 12;
    int catcher_x = margin + catcher_lane * lane_w + (lane_w - catcher_w) / 2;
    int lane;

    sk_draw_title("Ooze Catch");
    sk_clear_content();

    rb->snprintf(text, sizeof(text), "DROP %d/%d   SCORE %d", round + 1,
                 SK_OOZE_ROUNDS, score);
    sk_center_text(SK_CONTENT_Y + 12, text, SK_COL_TEXT);
    rb->snprintf(text, sizeof(text), "COMBO x%d", combo);
    sk_center_text(SK_CONTENT_Y + 30, text,
                   combo ? SK_COL_ORANGE : SK_COL_DIM);

    rb->lcd_set_foreground(SK_COL_RULE);
    for (lane = 0; lane <= SK_OOZE_LANES; ++lane)
        rb->lcd_vline(margin + lane * lane_w, lane_top - 6,
                      lane_bottom + catcher_h + 4);

    if (active)
    {
        int drop_w = lane_w - 12;
        int drop_x = margin + drop_lane * lane_w + (lane_w - drop_w) / 2;

        sk_vgradient(drop_x, drop_y, drop_w, 20, sk_ooze_top[drop_kind],
                    sk_ooze_bot[drop_kind]);
        rb->lcd_set_foreground(SK_COL_RULE);
        rb->lcd_drawrect(drop_x, drop_y, drop_w, 20);
        if (authentic_icons)
            sk_draw_cached_icon(sk_game_icons[drop_kind],
                                drop_x + (drop_w - SK_ICON_PX) / 2,
                                drop_y + (20 - SK_ICON_PX) / 2);
    }

    sk_vgradient(catcher_x, lane_bottom, catcher_w, catcher_h,
                SK_COL_SEL_TOP, SK_COL_WHITE);
    rb->lcd_set_foreground(SK_COL_RULE);
    rb->lcd_drawrect(catcher_x, lane_bottom, catcher_w, catcher_h);

    sk_draw_game_footer("LEFT/RIGHT move - completed games pay");
    rb->lcd_update();
}

static void sk_run_ooze_catch(void)
{
    const long period = HZ * 9 / 10;
    int lane_top = SK_CONTENT_Y + 44;
    int lane_bottom = SK_CONTENT_Y + 150;
    int catcher_lane = SK_OOZE_LANES / 2;
    int round = 0;
    int score = 0;
    int combo = 0;
    bool authentic_icons = sk_prepare_game_icons();
    int drop_lane = rb->rand() % SK_OOZE_LANES;
    int drop_kind;
    long round_start = *rb->current_tick;

    {
        int roll = rb->rand() % 100;
        drop_kind = roll < 55 ? 0 : (roll < 85 ? 1 : 2);
    }

    while (round < SK_OOZE_ROUNDS)
    {
        long elapsed = *rb->current_tick - round_start;
        int drop_y;
        int button;

        if (elapsed >= period)
        {
            bool caught = (drop_lane == catcher_lane);

            if (caught && drop_kind == 2)
            {
                score += sk_ooze_value[2];
                combo = 0;
                sk_play(SK_SND_BACK);
            }
            else if (caught)
            {
                combo++;
                score += sk_ooze_value[drop_kind] + MIN(combo, 4);
                sk_play(SK_SND_SELECT);
            }
            else if (drop_kind != 2)
            {
                combo = 0;
                sk_play(SK_SND_BACK);
            }

            round++;
            round_start = *rb->current_tick;
            drop_lane = rb->rand() % SK_OOZE_LANES;
            {
                int roll = rb->rand() % 100;
                drop_kind = roll < 55 ? 0 : (roll < 85 ? 1 : 2);
            }
            continue;
        }

        drop_y = lane_top +
            (int)(elapsed * (lane_bottom - lane_top) / period);
        sk_draw_ooze_catch(round, score, combo, catcher_lane, drop_lane,
                           drop_kind, drop_y, true, authentic_icons);
        button = pluginlib_getaction(MAX(1, HZ / 30), plugin_contexts,
                                     ARRAYLEN(plugin_contexts));

        if (button == PLA_LEFT || button == PLA_LEFT_REPEAT)
        {
            if (catcher_lane > 0)
                catcher_lane--;
            sk_play(SK_SND_NAVIGATE);
        }
        else if (button == PLA_RIGHT || button == PLA_RIGHT_REPEAT)
        {
            if (catcher_lane < SK_OOZE_LANES - 1)
                catcher_lane++;
            sk_play(SK_SND_NAVIGATE);
        }
        else if (button == PLA_CANCEL || button == PLA_EXIT)
        {
            sk_set_message("Ooze Catch cancelled");
            sk_play(SK_SND_BACK);
            return;
        }
        else if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
        {
            sk_game_usb_connected = true;
            return;
        }
    }

    {
        int reward = score > 0 ? score : 0;

        sk_award_game("Ooze Catch", 8 + reward, 14 + reward * 2);
    }
}

static const char *sk_scene_title(void)
{
    if (sk_scene == SK_SCENE_WORKSHOP)
    {
        if (sk_sel < SK_SLOT_COUNT)
            return sk_slot_labels[sk_sel];
        if (sk_sel == SK_SLOT_COUNT)
            return "Collection";
        if (sk_sel == SK_SLOT_COUNT + 1)
            return "Chip Dump";
        if (sk_sel == SK_SLOT_COUNT + 2)
            return "Trading Post";
        if (sk_sel == SK_SLOT_COUNT + 3)
            return "Body Color";
        if (sk_sel == SK_SLOT_COUNT + 4)
            return "Background";
        if (sk_sel == SK_SLOT_COUNT + 5)
            return "Minigames";
        if (sk_sel == SK_SLOT_COUNT + 6)
            return "Chip Shop";
        if (sk_sel == SK_WORKSHOP_PRESETS)
            return "Presets";
        if (sk_sel == SK_WORKSHOP_SAVE_PRESET)
            return "Save Preset";
        if (sk_sel == SK_WORKSHOP_CLEAR)
            return "Clear Sitekick";
        return "Stats";
    }

    switch (sk_scene)
    {
    case SK_SCENE_SLOT:
        return sk_slot_labels[sk_slot_filter];
    case SK_SCENE_COLLECTION:
        return "Collection";
    case SK_SCENE_DUMP:
        return "Chip Dump";
    case SK_SCENE_COLOR:
        return "Body Color";
    case SK_SCENE_BACKGROUND:
        return "Background";
    case SK_SCENE_GAMES:
        return "Minigames";
    case SK_SCENE_SHOP:
        return "Chip Shop";
    case SK_SCENE_STATS:
        return "Stats";
    case SK_SCENE_INBOX:
        return "Trading Post";
    case SK_SCENE_PRESETS:
        return "Presets";
    default:
        break;
    }
    return "Workshop";
}

static void sk_draw(void)
{
#if LCD_WIDTH >= 1920
    if (sk_desktop_mode)
    {
        sk_draw_desktop(sk_scene_title());
        return;
    }
#endif
    sk_draw_title(sk_scene_title());
    switch (sk_scene)
    {
    case SK_SCENE_SLOT:
        sk_draw_slot_picker();
        break;
    case SK_SCENE_COLLECTION:
        sk_draw_collection();
        break;
    case SK_SCENE_DUMP:
        sk_draw_dump();
        break;
    case SK_SCENE_COLOR:
        sk_draw_appearance_picker(false);
        break;
    case SK_SCENE_BACKGROUND:
        sk_draw_appearance_picker(true);
        break;
    case SK_SCENE_GAMES:
        sk_draw_games();
        break;
    case SK_SCENE_SHOP:
        sk_draw_shop();
        break;
    case SK_SCENE_STATS:
        sk_draw_stats();
        break;
    case SK_SCENE_INBOX:
        sk_draw_inbox();
        break;
    case SK_SCENE_PRESETS:
        sk_draw_presets();
        break;
    default:
        sk_draw_workshop();
        break;
    }
    sk_draw_status();
    rb->lcd_update();
}

/* ---------------------------------------------------------------------- */
/* trading outbox                                                           */
/* ---------------------------------------------------------------------- */

/* Stage the current loadout as a trade offer.  RockPod picks this up and
 * settles it against a peer; the device never decides a trade itself. */
static void sk_write_offer(void)
{
    char line[96];
    int fd, slot, written = 0;

    rb->mkdir(SK_SYNC_DIR);
    fd = rb->open(SK_OUTBOX_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
    {
        sk_set_message("Could not write offer");
        return;
    }
    rb->write(fd, "# type\tchip\tname\n", 17);
    for (slot = 0; slot < SK_SLOT_COUNT; ++slot)
    {
        int index = sk_equipped[slot];
        int len;

        if (index < 0)
            continue;
        len = rb->snprintf(line, sizeof(line), "offer\t%d\t%s\n",
                           sk_chips[index].id, sk_chips[index].name);
        if (len > 0)
            rb->write(fd, line, len);
        written++;
    }
    rb->close(fd);
    if (written)
        sk_set_message("Offer staged for sync");
    else
        sk_set_message("Equip chips to offer");
}

/* ---------------------------------------------------------------------- */
/* input                                                                    */
/* ---------------------------------------------------------------------- */

static void sk_enter_slot(int slot)
{
    sk_slot_filter = slot;
    sk_scene = SK_SCENE_SLOT;
    sk_sel = 0;
    sk_top = 0;
    if (sk_equipped[slot] >= 0)
    {
        int count = sk_slot_chip_count(slot);
        int i;

        for (i = 0; i < count; ++i)
        {
            if (sk_slot_chip_at(slot, i) == sk_equipped[slot])
            {
                sk_sel = i + 1;
                break;
            }
        }
    }
}

static void sk_equip(int slot, int chip_index)
{
    int other;

    if (sk_equipped[slot] == chip_index)
        return;
    for (other = 0; other < SK_SLOT_COUNT; ++other)
        if (other != slot && sk_equipped[other] == chip_index)
            sk_equipped[other] = -1;
    sk_equipped[slot] = chip_index;
    sk_preset_index = -1;
    sk_dirty = true;
    sk_reload_stage();
    if (chip_index >= 0)
    {
        sk_xp += 1;
        sk_set_message(sk_chips[chip_index].name);
    }
    else
    {
        sk_set_message("Slot cleared");
    }
    /* The Home pane continues to show the last explicitly selected preset.
     * Workshop edits become a new pane only when the user saves a preset. */
}

static bool sk_handle_select(void)
{
    switch (sk_scene)
    {
    case SK_SCENE_WORKSHOP:
        if (sk_sel < SK_SLOT_COUNT)
        {
            if (sk_slot_chip_count(sk_sel) == 0)
            {
                sk_set_message("No wearable chips owned");
                sk_play(SK_SND_BACK);
                return true;
            }
            sk_enter_slot(sk_sel);
        }
        else if (sk_sel == SK_SLOT_COUNT)
        {
            sk_scene = SK_SCENE_COLLECTION;
            sk_sel = 0;
            sk_top = 0;
        }
        else if (sk_sel == SK_SLOT_COUNT + 1)
        {
            sk_scene = SK_SCENE_DUMP;
            sk_dump_service();
        }
        else if (sk_sel == SK_SLOT_COUNT + 2)
        {
            sk_scene = SK_SCENE_INBOX;
        }
        else if (sk_sel == SK_SLOT_COUNT + 3)
        {
            sk_scene = SK_SCENE_COLOR;
            sk_sel = sk_body_color;
            sk_top = 0;
        }
        else if (sk_sel == SK_SLOT_COUNT + 4)
        {
            sk_scene = SK_SCENE_BACKGROUND;
            sk_sel = sk_background;
            sk_top = 0;
        }
        else if (sk_sel == SK_SLOT_COUNT + 5)
        {
            sk_scene = SK_SCENE_GAMES;
            sk_sel = 0;
            sk_top = 0;
        }
        else if (sk_sel == SK_SLOT_COUNT + 6)
        {
            sk_scene = SK_SCENE_SHOP;
            sk_shop_rotate(false);
            sk_sel = 0;
            sk_top = 0;
        }
        else if (sk_sel == SK_WORKSHOP_PRESETS)
        {
            sk_scene = SK_SCENE_PRESETS;
            sk_sel = 0;
            sk_top = 0;
        }
        else if (sk_sel == SK_WORKSHOP_SAVE_PRESET)
        {
            sk_save_preset();
        }
        else if (sk_sel == SK_WORKSHOP_CLEAR)
        {
            sk_clear_loadout();
        }
        else
        {
            sk_scene = SK_SCENE_STATS;
        }
        sk_play(SK_SND_SELECT);
        return true;

    case SK_SCENE_SLOT:
        if (sk_sel == 0)
            sk_equip(sk_slot_filter, -1);
        else
        {
            int index = sk_slot_chip_at(sk_slot_filter, sk_sel - 1);

            if (index >= 0)
                sk_equip(sk_slot_filter, index);
        }
        sk_play(SK_SND_SELECT);
        return true;

    case SK_SCENE_COLLECTION:
        if (sk_sel >= 0 && sk_sel < sk_chip_count)
        {
            const struct sk_chip *chip = &sk_chips[sk_sel];
            int position;

            if (!sk_owned_get(sk_sel))
                sk_set_message("Not collected yet");
            else if (!chip->worn)
                sk_set_message("Collectible only");
            else
            {
                for (position = 0; position < SK_SLOT_COUNT; ++position)
                    if (sk_equipped[position] < 0)
                        break;
                if (position >= SK_SLOT_COUNT)
                    position = 0;
                sk_equip(position, sk_sel);
                sk_play(SK_SND_SELECT);
                return true;
            }
        }
        sk_play(SK_SND_BACK);
        return true;

    case SK_SCENE_DUMP:
        sk_dump_claim();
        return true;

    case SK_SCENE_COLOR:
        if (sk_sel >= 0 && sk_sel < SK_BODY_COLOR_COUNT)
        {
            sk_body_color = sk_sel;
            sk_preset_index = -1;
            sk_dirty = true;
            sk_reload_stage();
            sk_write_save();
            sk_set_message(sk_body_color_names[sk_body_color]);
            sk_play(SK_SND_SELECT);
            return true;
        }
        break;

    case SK_SCENE_BACKGROUND:
        if (sk_sel >= 0 && sk_sel < SK_BACKGROUND_COUNT)
        {
            sk_background = sk_sel;
            sk_preset_index = -1;
            sk_dirty = true;
            sk_reload_stage();
            sk_write_save();
            sk_set_message(sk_background_names[sk_background]);
            sk_play(SK_SND_SELECT);
            return true;
        }
        break;

    case SK_SCENE_INBOX:
        sk_write_offer();
        sk_play(SK_SND_SELECT);
        return true;

    case SK_SCENE_GAMES:
        sk_game_usb_connected = false;
        if (sk_sel == 0)
            sk_run_beat_bounce();
        else if (sk_sel == 1)
            sk_run_chip_match();
        else if (sk_sel == 2)
            sk_run_ooze_catch();
        return true;

    case SK_SCENE_SHOP:
        sk_shop_buy(sk_sel);
        return true;

    case SK_SCENE_PRESETS:
    {
        int count = sk_preset_total();

        if (sk_sel < count)
            sk_apply_preset(sk_preset_at(sk_sel));
        else if (sk_sel == count && count < SK_PRESET_COUNT)
            sk_save_preset();
        sk_play(SK_SND_SELECT);
        return true;
    }

    default:
        break;
    }
    return false;
}

static bool sk_handle_back(void)
{
    if (sk_scene == SK_SCENE_WORKSHOP)
        return false;
    if (sk_scene == SK_SCENE_SLOT)
        sk_sel = sk_slot_filter;
    else if (sk_scene == SK_SCENE_COLLECTION)
        sk_sel = SK_SLOT_COUNT;
    else if (sk_scene == SK_SCENE_DUMP)
        sk_sel = SK_SLOT_COUNT + 1;
    else if (sk_scene == SK_SCENE_INBOX)
        sk_sel = SK_SLOT_COUNT + 2;
    else if (sk_scene == SK_SCENE_COLOR)
        sk_sel = SK_SLOT_COUNT + 3;
    else if (sk_scene == SK_SCENE_BACKGROUND)
        sk_sel = SK_SLOT_COUNT + 4;
    else if (sk_scene == SK_SCENE_GAMES)
        sk_sel = SK_SLOT_COUNT + 5;
    else if (sk_scene == SK_SCENE_SHOP)
        sk_sel = SK_SLOT_COUNT + 6;
    else if (sk_scene == SK_SCENE_PRESETS)
        sk_sel = SK_WORKSHOP_PRESETS;
    else
        sk_sel = SK_SLOT_COUNT + 7;
    sk_scene = SK_SCENE_WORKSHOP;
    sk_top = 0;
    sk_play(SK_SND_BACK);
    return true;
}

static int sk_list_length(void)
{
    switch (sk_scene)
    {
    case SK_SCENE_WORKSHOP:
        return SK_WORKSHOP_ITEMS;
    case SK_SCENE_SLOT:
        return sk_slot_chip_count(sk_slot_filter) + 1;
    case SK_SCENE_COLLECTION:
        return sk_chip_count;
    case SK_SCENE_COLOR:
        return SK_BODY_COLOR_COUNT;
    case SK_SCENE_BACKGROUND:
        return SK_BACKGROUND_COUNT;
    case SK_SCENE_GAMES:
        return 3;
    case SK_SCENE_SHOP:
        return SK_SHOP_SLOTS;
    case SK_SCENE_PRESETS:
        return sk_preset_total() +
               (sk_preset_total() < SK_PRESET_COUNT ? 1 : 0);
    default:
        break;
    }
    return 0;
}

/* ---------------------------------------------------------------------- */
/* entry point                                                              */
/* ---------------------------------------------------------------------- */

static void sk_draw_missing(void)
{
    rb->lcd_clear_display();
    sk_draw_title("Sitekick");
    rb->lcd_set_foreground(SK_COL_TEXT);
    sk_putsxy(10, SK_CONTENT_Y + 20,
              (const unsigned char *)"Asset pack not installed.");
    sk_putsxy(10, SK_CONTENT_Y + 40,
              (const unsigned char *)"Run sitekick_package_assets.py");
    sk_putsxy(10, SK_CONTENT_Y + 56,
              (const unsigned char *)"or sync from RockPod.");
    rb->lcd_update();
    rb->sleep(3 * HZ);
}

#ifdef SIMULATOR
/* Load every packaged bitmap once and report the tally.  This is the gate
 * that proves the packager's 32bpp BMPs are actually acceptable to
 * read_bmp_file() before anything is flashed to hardware. */
static void sk_self_test(void)
{
    struct bitmap probe;
    char path[MAX_PATH];
    int index, slot, ok = 0, failed = 0, pages_ok = 0, pages_failed = 0;
    int max_w = 0, max_h = 0;
    size_t worst = 0;

    DEBUGF("sitekick: chips=%d body=%d (%dx%d)\n", sk_chip_count,
           (int)sk_body_loaded, sk_body_w, sk_body_h);

    for (index = 0; index < sk_chip_count; ++index)
    {
        if (!sk_chips[index].worn)
            continue;
        rb->snprintf(path, sizeof(path), SK_CHIP_DIR "/%04d.bmp",
                     sk_chips[index].id);
        if (sk_load_bitmap(path, &probe, sk_bitmap_data,
                           sizeof(sk_bitmap_data)))
        {
            size_t bytes = (size_t)BM_SIZE(probe.width, probe.height,
                                           FORMAT_NATIVE, false) +
                           (size_t)(ALIGN_UP(probe.width, 2) *
                                    probe.height / 2);

            ok++;
            if (probe.width > max_w)
                max_w = probe.width;
            if (probe.height > max_h)
                max_h = probe.height;
            if (bytes > worst)
                worst = bytes;
        }
        else
        {
            failed++;
            DEBUGF("sitekick: FAILED chip %04d\n", sk_chips[index].id);
        }
    }

    for (index = 0; index < 32; ++index)
    {
        rb->snprintf(path, sizeof(path), SK_ICON_DIR "/page%d.bmp", index);
        if (!rb->file_exists(path))
            break;
        if (sk_load_bitmap(path, &probe, sk_icon_data, sizeof(sk_icon_data)))
            pages_ok++;
        else
        {
            pages_failed++;
            DEBUGF("sitekick: FAILED icon page %d\n", index);
        }
    }

    DEBUGF("sitekick: chip bitmaps ok=%d failed=%d max=%dx%d worst=%u\n",
           ok, failed, max_w, max_h, (unsigned)worst);
    DEBUGF("sitekick: icon pages ok=%d failed=%d\n", pages_ok, pages_failed);
    DEBUGF("sitekick: sounds=%d stage=%u float=%u scratch=%u icon_buf=%u\n",
#ifndef HAVE_HARDWARE_BEEP
           (int)sk_sound_loaded,
#else
           0,
#endif
           (unsigned)sizeof(sk_stage_data), (unsigned)sizeof(sk_float_data),
           (unsigned)SK_BITMAP_BYTES,
           (unsigned)SK_ICON_BYTES);
    for (slot = 0; slot < SK_SLOT_COUNT; ++slot)
        if (sk_equipped[slot] >= 0)
            DEBUGF("sitekick: equipped %s=id:%u z:%d\n",
                   sk_slot_labels[slot],
                   sk_chips[sk_equipped[slot]].id,
                   sk_chips[sk_equipped[slot]].z);
    if (sk_dump_index >= 0)
        DEBUGF("sitekick: dump=id:%u rarity:%s\n",
               sk_chips[sk_dump_index].id,
               sk_rarity_labels[sk_chips[sk_dump_index].rarity]);

    /* The probes above scribbled over the scratch and icon cache. */
    sk_icon_page_loaded = -1;
    sk_reload_stage();
}
#endif

static enum plugin_status sk_finish(enum plugin_status status)
{
    if (sk_dirty)
        sk_write_save();
    sk_sound_shutdown();
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_foreground(LCD_DEFAULT_FG);
    rb->lcd_set_background(LCD_DEFAULT_BG);
    return status;
}

enum plugin_status plugin_start(const void *parameter)
{
    bool quit = false;
#if LCD_WIDTH >= 1920
    sk_desktop_mode = parameter &&
        !rb->strcmp((const char *)parameter, "-desktop");
#endif
#ifdef SIMULATOR
    bool costume_self_test = parameter &&
        !rb->strcmp((const char *)parameter, "costume-self-test");
    bool self_test_only = parameter &&
        (!rb->strcmp((const char *)parameter, "self-test") ||
         costume_self_test);
    bool dump_view = parameter &&
        !rb->strcmp((const char *)parameter, "dump-view");
    bool costume_view = parameter &&
        (!rb->strcmp((const char *)parameter, "costume-view") ||
         costume_self_test);
    bool helmet_view = parameter &&
        !rb->strcmp((const char *)parameter, "helmet-view");
#else
#if LCD_WIDTH < 1920
    (void)parameter;
#endif
#endif

    rb->lcd_setfont(FONT_UI);
    rb->lcd_set_backdrop(NULL);
#if LCD_WIDTH >= 1920
    if (sk_desktop_mode && !sk_desktop_init())
    {
        rb->splash(HZ * 2, "Could not prepare Sitekick Desktop window");
        return PLUGIN_ERROR;
    }
#endif
    rb->srand((unsigned int)(*rb->current_tick ^ sk_now()));
    sk_load_brand();

    if (!sk_load_chips())
    {
        sk_draw_missing();
        return PLUGIN_OK;
    }
    sk_load_save();
    sk_apply_inbox();
    sk_load_sounds();
#ifdef SIMULATOR
    if (costume_view)
    {
        static const uint16_t costume_ids[] = { 317, 318, 319 };
        int position;

        for (position = 0; position < (int)ARRAYLEN(costume_ids); ++position)
        {
            int costume = sk_find_chip(costume_ids[position]);

            if (costume >= 0 && sk_chips[costume].worn)
            {
                sk_owned_set(costume, true);
                sk_equipped[position] = costume;
            }
        }
    }
    else if (helmet_view)
    {
        int helmet = sk_find_chip(42);

        if (helmet >= 0 && sk_chips[helmet].worn)
        {
            sk_owned_set(helmet, true);
            sk_equipped[0] = helmet;
        }
    }
#endif
    sk_reload_stage();
    /* Preserve the Home pane's last selected preset. A first install has no
     * preview yet, so seed one from the current starter loadout. */
    if (!rb->file_exists(SK_PREVIEW_PANE) ||
        !rb->file_exists(SK_PREVIEW_FLOAT))
        sk_publish_preview(true);
#ifdef SIMULATOR
    if (self_test_only || dump_view)
        sk_dump_service();
    sk_self_test();
    if (self_test_only)
        return sk_finish(PLUGIN_OK);
    if (dump_view)
        sk_scene = SK_SCENE_DUMP;
#endif

    sk_draw();

    while (!quit)
    {
        int timeout = sk_scene == SK_SCENE_WORKSHOP ?
                      MAX(1, HZ / 12) : HZ / 2;
#if LCD_WIDTH >= 1920
        if (sk_desktop_mode)
            timeout = MAX(1, HZ / 50);
#endif
        int button = pluginlib_getaction(timeout, plugin_contexts,
                                         ARRAYLEN(plugin_contexts));
        int count = sk_list_length();
        bool redraw = false;

#if LCD_WIDTH >= 1920
        if (sk_desktop_mode && sk_dm_poll_pointer())
            redraw = true;
#endif
        switch (button)
        {
        case PLA_UP:
        case PLA_UP_REPEAT:
        case PLA_SCROLL_BACK:
        case PLA_SCROLL_BACK_REPEAT:
            if (count > 0)
            {
                sk_sel = (sk_sel - 1 + count) % count;
                sk_play(SK_SND_NAVIGATE);
                redraw = true;
            }
            break;

        case PLA_DOWN:
        case PLA_DOWN_REPEAT:
        case PLA_SCROLL_FWD:
        case PLA_SCROLL_FWD_REPEAT:
            if (count > 0)
            {
                sk_sel = (sk_sel + 1) % count;
                sk_play(SK_SND_NAVIGATE);
                redraw = true;
            }
            break;

        case PLA_SELECT:
            redraw = sk_handle_select();
            break;

        case PLA_RIGHT:
        case PLA_RIGHT_REPEAT:
            if (sk_scene == SK_SCENE_WORKSHOP)
            {
                sk_cycle_preset(1);
                redraw = true;
            }
            else
                redraw = sk_handle_select();
            break;

        case PLA_CANCEL:
            if (!sk_handle_back())
                quit = true;
            else
                redraw = true;
            break;

        case PLA_LEFT:
        case PLA_LEFT_REPEAT:
            if (sk_scene == SK_SCENE_WORKSHOP)
            {
                sk_cycle_preset(-1);
                redraw = true;
            }
            else if (!sk_handle_back())
                quit = true;
            else
                redraw = true;
            break;

        case PLA_EXIT:
            quit = true;
            break;

        default:
            if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
                return sk_finish(PLUGIN_USB_CONNECTED);
            if (button == ACTION_NONE && sk_scene == SK_SCENE_WORKSHOP)
                redraw = true;
            if (sk_scene == SK_SCENE_DUMP && sk_dump_service())
                redraw = true;
            /* Let a timed out message clear itself. */
            if (sk_message[0] &&
                TIME_AFTER(*rb->current_tick, sk_message_until))
                redraw = true;
            break;
        }

        if (sk_game_usb_connected)
            return sk_finish(PLUGIN_USB_CONNECTED);
        if (redraw)
            sk_draw();
    }

    return sk_finish(PLUGIN_OK);
}
