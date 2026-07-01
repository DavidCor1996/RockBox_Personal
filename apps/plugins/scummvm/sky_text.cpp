/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ |__   _______  ___
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026 Rockpod contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ****************************************************************************/

#include "lib/plugin_cxx_compat.h"
#include "rbfile.h"
#include "sky_loader.h"
#include "sky_text.h"

#define private public
#include "upstream-1.9.0/engines/sky/text.h"
#undef private

#define SKY_NO_OF_TEXT_SECTIONS 8
#define SKY_MAX_TEXT_SECTIONS 16
#define SKY_CHAR_SET_FILE 60150
#define SKY_CHAR_SET_HEADER 128
#define SKY_MAIN_CHAR_HEIGHT 12
#define SKY_MAX_RENDER_TEXT 512
#define SKY_MAX_RENDER_LINES 10
#define SKY_TEXT_HEADER_SIZE 22

static const struct scummvm_target *text_target;
static uint16_t text_game_version;
static struct scummvm_sky_resource text_sections[SKY_MAX_TEXT_SECTIONS];
static struct scummvm_sky_resource text_charset;

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_be16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

static void write_le16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static bool read_all(int fd, void *dst, size_t size)
{
    uint8_t *out = (uint8_t *)dst;
    size_t done = 0;

    while (done < size) {
        ssize_t got = rb->read(fd, out + done, size - done);
        if (got <= 0)
            return false;
        done += (size_t)got;
    }

    return true;
}

static uint32_t file_size_in_dir(const char *dir, const char *name)
{
    char path[MAX_PATH];
    int fd;
    long size;

    if (!scummvm_make_path(path, sizeof(path), dir, name))
        return 0;

    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return 0;

    size = rb->filesize(fd);
    rb->close(fd);
    return size > 0 ? (uint32_t)size : 0;
}

static bool read_dnr_entries(const struct scummvm_target *target,
                             uint32_t *entries)
{
    char path[MAX_PATH];
    uint8_t buf[4];
    int fd;
    bool ok = false;

    if (!scummvm_make_path(path, sizeof(path), target->path, "sky.dnr"))
        return false;

    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;

    if (read_all(fd, buf, sizeof(buf))) {
        *entries = read_le32(buf);
        ok = true;
    }

    rb->close(fd);
    return ok;
}

static uint16_t detect_game_version(const struct scummvm_target *target)
{
    uint32_t entries;
    uint32_t dsk_size;

    if (!read_dnr_entries(target, &entries))
        return 0;

    dsk_size = file_size_in_dir(target->path, "sky.dsk");

    switch (entries) {
    case 232:
        return 272;
    case 243:
        return 109;
    case 247:
        return 267;
    case 1404:
        return 288;
    case 1413:
        return 303;
    case 1445:
        return dsk_size == 8830435 ? 348 : 331;
    case 1711:
        return 365;
    case 5099:
        return 368;
    case 5097:
        return 372;
    default:
        return 0;
    }
}

static const Sky::HuffTree *text_tree(uint16_t version)
{
    switch (version) {
    case 109:
        return Sky::Text::_huffTree_00109;
    case 272:
    case 267:
        return Sky::Text::_huffTree_00267;
    case 288:
        return Sky::Text::_huffTree_00288;
    case 303:
        return Sky::Text::_huffTree_00303;
    case 331:
        return Sky::Text::_huffTree_00331;
    case 348:
        return Sky::Text::_huffTree_00348;
    case 365:
        return Sky::Text::_huffTree_00365;
    case 368:
        return Sky::Text::_huffTree_00368;
    case 372:
        return Sky::Text::_huffTree_00372;
    default:
        return NULL;
    }
}

static bool get_text_bit(const uint8_t **data, uint32_t *bit_pos)
{
    if (*bit_pos)
        (*bit_pos)--;
    else {
        (*data)++;
        *bit_pos = 7;
    }

    return ((**data) >> *bit_pos) & 1;
}

static char get_text_char(const Sky::HuffTree *tree,
                          const uint8_t **data,
                          uint32_t *bit_pos)
{
    int pos = 0;
    uint32_t guard = 0;

    while (guard++ < 1024) {
        pos = get_text_bit(data, bit_pos) ?
            tree[pos].rChild : tree[pos].lChild;

        if (!tree[pos].lChild && !tree[pos].rChild)
            return (char)tree[pos].value;
    }

    return '\0';
}

static bool patch_message(uint32_t text_num, char *out, size_t out_size)
{
    const struct {
        uint32_t text_num;
        const char *text;
    } patches[] = {
        { 28724, "Text and Speech" },
        { 28707, "Text Only" },
        { 28693, "Speech Only" },
    };
    uint32_t i;

    for (i = 0; i < ARRAYLEN(patches); i++) {
        if (patches[i].text_num == text_num) {
            rb->strlcpy(out, patches[i].text, out_size);
            return true;
        }
    }

    return false;
}

static bool load_text_section(uint16_t section_no,
                              struct scummvm_sky_resource **section)
{
    uint16_t file_no;
    char status[96];

    if (section_no >= ARRAYLEN(text_sections) || !text_target)
        return false;

    *section = &text_sections[section_no];
    if ((*section)->data)
        return true;

    file_no = (uint16_t)(60600 + section_no);
    return scummvm_sky_loader_load_resource(text_target,
                                            file_no,
                                            *section,
                                            status,
                                            sizeof(status));
}

static bool load_charset(void)
{
    char status[96];

    if (text_charset.data)
        return true;
    if (!text_target)
        return false;

    return scummvm_sky_loader_load_resource(text_target,
                                            SKY_CHAR_SET_FILE,
                                            &text_charset,
                                            status,
                                            sizeof(status));
}

static uint8_t text_char_index(uint8_t ch)
{
    if (ch < 0x20)
        return 0;
    ch -= 0x20;
    if (ch >= SKY_CHAR_SET_HEADER)
        return 0;
    return ch;
}

static uint16_t text_char_advance(const uint8_t *charset, uint8_t ch)
{
    return (uint16_t)(charset[text_char_index(ch)] + 1);
}

static void render_character(uint8_t ch,
                             const uint8_t *charset,
                             uint8_t **dest,
                             uint8_t color,
                             uint16_t pitch)
{
    uint8_t index = text_char_index(ch);
    uint8_t char_width = (uint8_t)(charset[index] + 1);
    const uint8_t *char_sprite =
        charset + SKY_CHAR_SET_HEADER + SKY_MAIN_CHAR_HEIGHT * 4 * index;
    uint8_t *start = *dest;
    uint8_t *cur = start;
    int row;

    for (row = 0; row < SKY_MAIN_CHAR_HEIGHT; row++) {
        uint8_t *row_start = cur;
        uint16_t data = read_be16(char_sprite);
        uint16_t mask = read_be16(char_sprite + 2);
        uint8_t col;

        char_sprite += 4;
        for (col = 0; col < char_width; col++) {
            bool mask_bit = (mask & 0x8000) != 0;
            bool data_bit = (data & 0x8000) != 0;

            mask <<= 1;
            data <<= 1;
            if (mask_bit)
                *cur = data_bit ? color : 240;
            cur++;
        }
        cur = row_start + pitch;
    }

    *dest = start + char_width;
}

bool scummvm_sky_text_init(const struct scummvm_target *target,
                           char *status,
                           size_t status_size)
{
    scummvm_sky_text_reset();
    text_target = target;
    text_game_version = detect_game_version(target);

    if (!text_tree(text_game_version)) {
        rb->snprintf(status, status_size,
                     "Sky text version unsupported");
        return false;
    }

    rb->snprintf(status, status_size, "Sky text v0.0%u",
                 (unsigned)text_game_version);
    return true;
}

bool scummvm_sky_text_render(uint32_t text_num,
                             uint16_t pixel_width,
                             uint8_t color,
                             bool center,
                             struct scummvm_sky_resource *out,
                             uint16_t *text_width)
{
    char text[SKY_MAX_RENDER_TEXT];
    const char *line_start[SKY_MAX_RENDER_LINES];
    const char *line_end[SKY_MAX_RENDER_LINES];
    uint16_t line_width[SKY_MAX_RENDER_LINES];
    const uint8_t *charset;
    const char *pos;
    uint16_t lines = 0;
    uint16_t last_width = 0;
    uint32_t sprite_bytes;
    uint8_t *data;
    uint16_t i;

    if (!out || pixel_width == 0 ||
        !scummvm_sky_text_decode(text_num, text, sizeof(text)) ||
        !load_charset() ||
        text_charset.size < SKY_CHAR_SET_HEADER +
                            SKY_CHAR_SET_HEADER * SKY_MAIN_CHAR_HEIGHT * 4)
        return false;

    charset = text_charset.data;
    pos = text;
    while (*pos && lines < SKY_MAX_RENDER_LINES) {
        const char *start = pos;
        const char *scan = pos;
        const char *last_space = NULL;
        uint16_t width = 0;
        uint16_t width_at_space = 0;

        while (*scan && *scan != '\n') {
            uint16_t next_width =
                (uint16_t)(width +
                           text_char_advance(charset, (uint8_t)*scan));

            if (*scan == ' ') {
                last_space = scan;
                width_at_space = width;
            }
            if (next_width > pixel_width && scan > start) {
                if (last_space && last_space > start) {
                    scan = last_space;
                    width = width_at_space;
                }
                break;
            }
            width = next_width;
            scan++;
        }

        line_start[lines] = start;
        line_end[lines] = scan;
        line_width[lines] = width;
        last_width = width;
        lines++;

        while (*scan == ' ' || *scan == '\n')
            scan++;
        pos = scan;
    }

    if (lines == 0)
        return false;

    sprite_bytes = (uint32_t)pixel_width * SKY_MAIN_CHAR_HEIGHT * lines;
    scummvm_sky_loader_release_resource(out);
    out->data = new uint8_t[SKY_TEXT_HEADER_SIZE + sprite_bytes];
    if (!out->data) {
        out->size = 0;
        out->file_nr = 0;
        return false;
    }

    out->size = SKY_TEXT_HEADER_SIZE + sprite_bytes;
    rb->memset(out->data, 0, out->size);
    write_le16(out->data + 6, pixel_width);
    write_le16(out->data + 8, (uint16_t)(SKY_MAIN_CHAR_HEIGHT * lines));
    write_le16(out->data + 10, (uint16_t)sprite_bytes);
    write_le16(out->data + 12, (uint16_t)sprite_bytes);
    write_le16(out->data + 14, 1);

    data = out->data + SKY_TEXT_HEADER_SIZE;
    for (i = 0; i < lines; i++) {
        const char *ch = line_start[i];
        uint8_t *dest = data + (uint32_t)i * SKY_MAIN_CHAR_HEIGHT *
                               pixel_width;

        if (center && pixel_width > line_width[i])
            dest += (pixel_width - line_width[i]) >> 1;

        while (ch < line_end[i]) {
            render_character((uint8_t)*ch, charset, &dest, color,
                             pixel_width);
            ch++;
        }
    }

    if (text_width)
        *text_width = last_width;
    return true;
}

bool scummvm_sky_text_decode(uint32_t text_num, char *out, size_t out_size)
{
    const Sky::HuffTree *tree;
    struct scummvm_sky_resource *section;
    const uint8_t *text_data;
    const uint8_t *text_ptr;
    uint32_t section_no;
    uint32_t block_nr;
    uint32_t entry_no;
    uint32_t offset = 0;
    uint32_t bit_pos;
    size_t out_pos = 0;
    uint32_t guard = 0;

    if (!out || out_size == 0)
        return false;
    out[0] = '\0';

    if (patch_message(text_num, out, out_size))
        return true;

    tree = text_tree(text_game_version);
    if (!tree)
        return false;

    section_no = (text_num & 0x0f000) >> 12;
    block_nr = text_num & 0x0fe0;
    entry_no = text_num & 0x001f;

    if (!load_text_section((uint16_t)section_no, &section) ||
        section->size < 8)
        return false;

    text_data = section->data;

    if (block_nr) {
        const uint8_t *block_ptr = text_data + 4;
        uint32_t blocks = block_nr >> 5;

        if (4 + blocks * 2 > section->size)
            return false;
        while (blocks--) {
            offset += read_le16(block_ptr);
            block_ptr += 2;
        }
    }

    if (entry_no) {
        const uint8_t *block_ptr;

        if (block_nr + read_le16(text_data) >= section->size)
            return false;

        block_ptr = text_data + block_nr + read_le16(text_data);
        while (entry_no--) {
            uint16_t skip_bytes;

            if (block_ptr >= text_data + section->size)
                return false;
            skip_bytes = *block_ptr++;
            if (skip_bytes & 0x80) {
                skip_bytes &= 0x7f;
                skip_bytes <<= 3;
            }
            offset += skip_bytes;
        }
    }

    bit_pos = offset & 3;
    offset >>= 2;
    offset += read_le16(text_data + 2);
    if (offset >= section->size)
        return false;

    text_ptr = text_data + offset;
    bit_pos ^= 3;
    bit_pos++;
    bit_pos <<= 1;

    while (guard++ < 1000 && out_pos + 1 < out_size) {
        char ch = get_text_char(tree, &text_ptr, &bit_pos);

        if (ch == '\0')
            break;
        if ((uint8_t)ch < 0x20 && ch != '\n')
            ch = ' ';
        if (ch == '`')
            ch = '\'';
        out[out_pos++] = ch;
        if (text_ptr >= text_data + section->size)
            break;
    }

    out[out_pos] = '\0';
    return out_pos > 0;
}

void scummvm_sky_text_reset(void)
{
    uint32_t i;

    for (i = 0; i < ARRAYLEN(text_sections); i++)
        scummvm_sky_loader_release_resource(&text_sections[i]);
    scummvm_sky_loader_release_resource(&text_charset);
    rb->memset(text_sections, 0, sizeof(text_sections));
    text_target = NULL;
    text_game_version = 0;
}
