/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2005 Dave Chapman
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>
#include "platform.h"
#include "metadata.h"
#include "metadata_common.h"

/* Define LOGF_ENABLE to enable logf output in this file */
/*#define LOGF_ENABLE*/
#include "logf.h"

/* --------------------------------------------- */
/* TIDAL_DATA helpers                            */
/* --------------------------------------------- */

static const char *json_skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    return p;
}

static bool json_extract_string_after(const char *start,
                                      const char *key,
                                      char *out,
                                      size_t outsz)
{
    const char *p;
    size_t n = 0;

    if (!start || !key || !out || outsz == 0)
        return false;

    p = strstr(start, key);
    if (!p)
        return false;

    p += strlen(key);
    p = strchr(p, ':');
    if (!p)
        return false;

    p++;
    p = json_skip_ws(p);

    if (*p != '"')
        return false;

    p++;

    while (*p && n + 1 < outsz)
    {
        if (*p == '\\')
        {
            p++;
            if (!*p)
                break;

            switch (*p)
            {
                case '"':
                case '\\':
                case '/':
                    out[n++] = *p;
                    break;
                case 'b':
                    out[n++] = '\b';
                    break;
                case 'f':
                    out[n++] = '\f';
                    break;
                case 'n':
                    out[n++] = '\n';
                    break;
                case 'r':
                    out[n++] = '\r';
                    break;
                case 't':
                    out[n++] = '\t';
                    break;
                default:
                    break;
            }
            p++;
            continue;
        }

        if (*p == '"')
            break;

        out[n++] = *p++;
    }

    out[n] = '\0';
    return n > 0;
}

static bool json_extract_bool_after(const char *start,
                                    const char *key,
                                    bool *out)
{
    const char *p;

    if (!start || !key || !out)
        return false;

    p = strstr(start, key);
    if (!p)
        return false;

    p += strlen(key);
    p = strchr(p, ':');
    if (!p)
        return false;

    p++;
    p = json_skip_ws(p);

    if (!strncmp(p, "true", 4))
    {
        *out = true;
        return true;
    }

    if (!strncmp(p, "false", 5))
    {
        *out = false;
        return true;
    }

    return false;
}

static bool json_extract_int_after(const char *start,
                                   const char *key,
                                   int *out)
{
    const char *p;

    if (!start || !key || !out)
        return false;

    p = strstr(start, key);
    if (!p)
        return false;

    p += strlen(key);
    p = strchr(p, ':');
    if (!p)
        return false;

    p++;
    p = json_skip_ws(p);

    *out = atoi(p);
    return true;
}

static int add_vorbis_tag_from_string(const char *name,
                                      const char *value,
                                      struct mp3entry *id3,
                                      char **buf,
                                      int *buf_remaining)
{
    int used;

    if (!name || !value || !*value || !buf || !*buf || !buf_remaining || *buf_remaining <= 0)
        return 0;

    used = parse_tag((char *)name, (char *)value, id3, *buf, *buf_remaining, TAGTYPE_VORBIS);
    if (used > 0)
    {
        *buf += used;
        *buf_remaining -= used;
    }

    return used;
}

static void parse_tidal_data_tag(const char *json,
                                 struct mp3entry *id3,
                                 char **buf,
                                 int *buf_remaining)
{
    char tmp[256];
    char grouping[128];
    const char *album_section;
    const char *track_section;
    const char *artist_section;
    int tracknum;
    int discnum;
    bool explicit_flag;
    int n = 0;

    if (!json || !*json)
        return;

    album_section = strstr(json, "\"album\"");
    track_section = strstr(json, "\"item\"");
    if (!track_section)
        track_section = json;

    if (track_section)
    {
        if (!id3->title &&
            json_extract_string_after(track_section, "\"title\"", tmp, sizeof(tmp)))
        {
            add_vorbis_tag_from_string("TITLE", tmp, id3, buf, buf_remaining);
        }

        artist_section = strstr(track_section, "\"artist\"");
        if (artist_section)
        {
            if (!id3->artist &&
                json_extract_string_after(artist_section, "\"name\"", tmp, sizeof(tmp)))
            {
                add_vorbis_tag_from_string("ARTIST", tmp, id3, buf, buf_remaining);
            }
        }

        if (!id3->track_string &&
            json_extract_int_after(track_section, "\"trackNumber\"", &tracknum))
        {
            snprintf(tmp, sizeof(tmp), "%d", tracknum);
            add_vorbis_tag_from_string("TRACKNUMBER", tmp, id3, buf, buf_remaining);
        }

        if (!id3->disc_string &&
            json_extract_int_after(track_section, "\"volumeNumber\"", &discnum))
        {
            snprintf(tmp, sizeof(tmp), "%d", discnum);
            add_vorbis_tag_from_string("DISCNUMBER", tmp, id3, buf, buf_remaining);
        }

        if (!id3->mb_track_id &&
            json_extract_string_after(track_section, "\"isrc\"", tmp, sizeof(tmp)))
        {
            add_vorbis_tag_from_string("MUSICBRAINZ_TRACKID", tmp, id3, buf, buf_remaining);
        }


        if (!id3->comment &&
            json_extract_string_after(track_section, "\"key\"", tmp, sizeof(tmp)))
        {
            char keybuf[64];
            snprintf(keybuf, sizeof(keybuf), "KEY:%s", tmp);
            add_vorbis_tag_from_string("COMMENT", keybuf, id3, buf, buf_remaining);
        }
    }

    if (album_section)
    {
        if (!id3->album &&
            json_extract_string_after(album_section, "\"title\"", tmp, sizeof(tmp)))
        {
            add_vorbis_tag_from_string("ALBUM", tmp, id3, buf, buf_remaining);
        }

        artist_section = strstr(album_section, "\"artist\"");
        if (artist_section)
        {
            if (!id3->albumartist &&
                json_extract_string_after(artist_section, "\"name\"", tmp, sizeof(tmp)))
            {
                add_vorbis_tag_from_string("ALBUMARTIST", tmp, id3, buf, buf_remaining);
            }

            if (!id3->artist &&
                json_extract_string_after(artist_section, "\"name\"", tmp, sizeof(tmp)))
            {
                add_vorbis_tag_from_string("ARTIST", tmp, id3, buf, buf_remaining);
            }
        }

        if (!id3->year_string &&
            json_extract_string_after(album_section, "\"releaseDate\"", tmp, sizeof(tmp)))
        {
            char year[5];

            if (strlen(tmp) >= 4)
            {
                memcpy(year, tmp, 4);
                year[4] = '\0';
                add_vorbis_tag_from_string("DATE", year, id3, buf, buf_remaining);
            }
        }

        if (!id3->comment &&
            json_extract_string_after(album_section, "\"copyright\"", tmp, sizeof(tmp)))
        {
            add_vorbis_tag_from_string("COMMENT", tmp, id3, buf, buf_remaining);
        }
    }

    grouping[0] = '\0';

    if (json_extract_bool_after(track_section, "\"explicit\"", &explicit_flag))
    {
        n += snprintf(grouping + n, sizeof(grouping) - n,
                      "%s", explicit_flag ? "Explicit" : "Clean");
    }

    if (n > 0 && !id3->grouping)
    {
        add_vorbis_tag_from_string("GROUPING", grouping, id3, buf, buf_remaining);
    }
}

/* --------------------------------------------- */
/* Ogg / Vorbis reader                           */
/* --------------------------------------------- */

/* Read an Ogg page header. file->packet_remaining is set to the size of the
 * first packet on the page; file->packet_ended is set to true if the packet
 * ended on the current page. Returns true if the page header was
 * successfully read.
 */
static bool file_read_page_header(struct ogg_file* file)
{
    unsigned char buffer[64];
    ssize_t table_left;

    if (read(file->fd, buffer, 27) != 27)
    {
        return false;
    }

    if (memcmp("OggS", buffer, 4))
    {
        return false;
    }

    table_left = buffer[26];
    file->packet_remaining = 0;

    do
    {
        ssize_t count = MIN(sizeof(buffer), (size_t)table_left);
        int i;

        if (read(file->fd, buffer, count) < count)
        {
            return false;
        }

        table_left -= count;

        for (i = 0; i < count; i++)
        {
            file->packet_remaining += buffer[i];

            if (buffer[i] < 255)
            {
                file->packet_ended = true;

                if (lseek(file->fd, table_left, SEEK_CUR) < 0)
                {
                    return false;
                }

                table_left = 0;
                break;
            }
        }
    }
    while (table_left > 0);

    return true;
}

ssize_t ogg_file_read(struct ogg_file* file, void* buffer, size_t buffer_size)
{
    ssize_t done = 0;
    ssize_t count = -1;

    do
    {
        if (file->packet_remaining <= 0)
        {
            if (file->packet_ended)
            {
                break;
            }

            if (!file_read_page_header(file))
            {
                count = -1;
                break;
            }
        }

        count = MIN(buffer_size, (size_t)file->packet_remaining);

        if (buffer)
        {
            count = read(file->fd, buffer, count);
        }
        else
        {
            if (lseek(file->fd, count, SEEK_CUR) < 0)
            {
                count = -1;
            }
        }

        if (count <= 0)
        {
            break;
        }

        if (buffer)
        {
            buffer += count;
        }

        buffer_size -= count;
        done += count;
        file->packet_remaining -= count;
    }
    while (buffer_size > 0);

    return (count < 0 ? count : done);
}

static bool file_read_int32(struct ogg_file* file, int32_t* value)
{
    char buf[sizeof(int32_t)];

    if (ogg_file_read(file, buf, sizeof(buf)) < (ssize_t)sizeof(buf))
    {
        return false;
    }

    *value = get_long_le(buf);
    return true;
}

static long file_read_string(struct ogg_file* file, char* buffer,
                             long buffer_size, int eos, long size)
{
    long read_bytes = 0;

    while (size > 0)
    {
        char c;

        if (ogg_file_read(file, &c, 1) != 1)
        {
            read_bytes = -1;
            break;
        }

        read_bytes++;
        size--;

        if ((eos != -1) && (eos == (unsigned char)c))
        {
            break;
        }

        if (buffer_size > 1)
        {
            *buffer++ = c;
            buffer_size--;
        }
        else if (eos == -1)
        {
            if (ogg_file_read(file, NULL, size) < 0)
            {
                read_bytes = -1;
            }
            else
            {
                read_bytes += size;
            }

            break;
        }
    }

    *buffer = 0;
    return read_bytes;
}

bool ogg_file_init(struct ogg_file* file, int fd, int type, int remaining)
{
    memset(file, 0, sizeof(*file));
    file->fd = fd;

    if (type == AFMT_OGG_VORBIS || type == AFMT_SPEEX || type == AFMT_OPUS)
    {
        if (!file_read_page_header(file))
        {
            return false;
        }
    }

    if (type == AFMT_OGG_VORBIS)
    {
        char buffer[7];

        if (ogg_file_read(file, buffer, sizeof(buffer)) < (ssize_t)sizeof(buffer))
        {
            return false;
        }

        if (buffer[0] != 3)
        {
            return false;
        }
    }
    else if (type == AFMT_OPUS)
    {
        char buffer[8];

        if (ogg_file_read(file, buffer, sizeof(buffer)) < (ssize_t)sizeof(buffer))
        {
            return false;
        }

        if (memcmp(buffer, "OpusTags", 8) != 0)
        {
            return false;
        }
    }
    else if (type == AFMT_FLAC)
    {
        file->packet_remaining = remaining;
        file->packet_ended = true;
    }

    return true;
}

#define B64_START_CHAR '+'
const signed char b64_codes[] =
{
    62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -2, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
};

size_t base64_decode(const char *in, size_t in_len, unsigned char *out)
{
    size_t i = 0;
    int val = 0;
    size_t len = 0;

    while (i < in_len)
    {
        if (in[i] == '=')
        {
            switch (i & 3)
            {
                case 2:
                    out[len++] = (val >> 4) & 0xFF;
                    break;
                case 3:
                    out[len++] = (val >> 10) & 0xFF;
                    out[len++] = (val >> 2) & 0xFF;
                    break;
            }
            break;
        }

        {
            int index = in[i] - B64_START_CHAR;
            #ifdef SIMULATOR
            if (index < 0 || index >= (int)ARRAYLEN(b64_codes) || b64_codes[index] < 0)
            {
                DEBUGF("Invalid base64 char: '%c', char code: %i.\n", in[i], in[i]);
                break;
            }
            #endif
            val = (val << 6) | b64_codes[index];
        }

        if ((++i & 3) == 0)
        {
            out[len++] = (val >> 16) & 0xFF;
            out[len++] = (val >> 8) & 0xFF;
            out[len++] = val & 0xFF;
        }
    }

    return len;
}

size_t base64_encoded_size(size_t inlen)
{
    size_t ret = inlen;

    if (inlen % 3 != 0)
        ret += 3 - (inlen % 3);

    ret /= 3;
    ret *= 4;

    return ret;
}

long read_vorbis_tags(int fd, struct mp3entry *id3, long tag_remaining)
{
    struct ogg_file file;
    char *buf = id3->id3v2buf;
    int32_t comment_count;
    int32_t len;
    long comment_size = 0;
    int buf_remaining = sizeof(id3->id3v2buf) + sizeof(id3->id3v1buf);
    int i;

    if (!ogg_file_init(&file, fd, id3->codectype, tag_remaining))
    {
        return 0;
    }

    if (!file_read_int32(&file, &len) || (ogg_file_read(&file, NULL, len) < 0))
    {
        return 0;
    }

    if (!file_read_int32(&file, &comment_count))
    {
        return 0;
    }

    comment_size += 4 + len + 4;

    for (i = 0; i < comment_count && file.packet_remaining > 0; i++)
    {
        char name[TAG_NAME_LENGTH];
        int32_t read_len;

        if (!file_read_int32(&file, &len))
        {
            return 0;
        }

        comment_size += 4 + len;
        read_len = file_read_string(&file, name, sizeof(name), '=', len);

        if (read_len < 0)
        {
            return 0;
        }

        len -= read_len;
        #ifdef HAVE_ALBUMART
        {
            int before_block_pos = lseek(fd, 0, SEEK_CUR);
            #endif
            read_len = file_read_string(&file, id3->path, sizeof(id3->path), -1, len);

            if (read_len < 0)
            {
                return 0;
            }

            logf("Vorbis comment %d: %s=%s", i, name, id3->path);

            #ifdef HAVE_ALBUMART
            if (!id3->has_embedded_albumart && !strcasecmp(name, "METADATA_BLOCK_PICTURE"))
            {
                int after_block_pos = lseek(fd, 0, SEEK_CUR);
                char *bufptr = id3->path;
                size_t outlen = base64_decode(bufptr,
                                              MIN(read_len, (int32_t)sizeof(id3->path) - 1),
                                              (unsigned char *)bufptr);

                int picframe_pos;
                parse_flac_album_art((unsigned char *)bufptr, outlen,
                                     &id3->albumart.type, &picframe_pos);
                if (id3->albumart.type != AA_TYPE_UNKNOWN)
                {
                    const int picframe_pos_b64 = base64_encoded_size(picframe_pos + 4);

                    id3->has_embedded_albumart = true;
                    id3->albumart.type |= AA_FLAG_VORBIS_BASE64;
                    id3->albumart.pos = picframe_pos_b64 + before_block_pos;
                    id3->albumart.size = after_block_pos - id3->albumart.pos;
                }
                continue;
            }
            #endif

            if (!strcasecmp(name, "CUESHEET"))
            {
                id3->has_embedded_cuesheet = true;
                id3->embedded_cuesheet.pos = lseek(file.fd, 0, SEEK_CUR) - read_len;
                id3->embedded_cuesheet.size = len;
                id3->embedded_cuesheet.encoding = CHAR_ENC_UTF_8;
                continue;
            }

            if (!strcasecmp(name, "TIDAL_DATA"))
            {
                parse_tidal_data_tag(id3->path, id3, &buf, &buf_remaining);
                continue;
            }

            if (!strcasecmp(name, "TIDAL") ||
                !strcasecmp(name, "JSON") ||
                !strcasecmp(name, "METADATA_JSON"))
            {
                continue;
            }

            len = parse_tag(name, id3->path, id3, buf, buf_remaining, TAGTYPE_VORBIS);
            buf += len;
            buf_remaining -= len;
            #ifdef HAVE_ALBUMART
        }
        #endif
    }

    if (file.packet_remaining)
    {
        if (ogg_file_read(&file, NULL, file.packet_remaining) < 0)
        {
            return 0;
        }
    }

    return comment_size;
}
