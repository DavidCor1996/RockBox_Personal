/***************************************************************************
 * Bounded text cache for native iPodJS Now Playing pages.
 * SPDX-License-Identifier: GPL-2.0-or-later
 ****************************************************************************/
#include "config.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "file.h"
#include "rbunicode.h"
#include "string-extra.h"
#include "system.h"
#include "kernel.h"
#include "font.h"
#include "ipodjs_ui.h"
#include "ipodjs_wps_text.h"

#define TEXT_BYTES 8192
#define RAW_BYTES 4096
#define TEXT_LINES 256
#define VISIBLE_LINES 11

static unsigned char raw[RAW_BYTES + 4];
static char text[TEXT_BYTES];
static uint16_t lines[TEXT_LINES];
static int line_count, top_line;

/* Layout is prepared once, outside painting. Overlong lines wrap at UTF-8
 * boundaries using the already prepared Apple detail font. */
static void layout(void)
{
    char *p = text;
    int font = ipodjs_ui_retailos_font(false);
    line_count = top_line = 0;
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')
        p++;
    while (*p && line_count < TEXT_LINES)
    {
        char *start = p, *space = NULL;
        lines[line_count++] = p - text;
        while (*p && *p != '\n' && *p != '\r')
        {
            if (*p == ' ')
                space = p;
            char *next = p + 1;
            while ((*next & 0xc0) == 0x80)
                next++;
            char saved = *next;
            *next = 0;
            int width = font_getstringsize(start, NULL, NULL, font);
            *next = saved;
            if (width > 296 && p > start)
            {
                /* Prefer a space. A long unbroken word is clipped at a
                 * complete codepoint, never allowed to read past the cache. */
                if (space && space > start)
                    p = space;
                break;
            }
            p = next;
        }
        if (!*p)
            break;
        if (*p == '\r' && p[1] == '\n')
            *p++ = 0;
        /* At a word wrap the separator is discarded. If no separator exists,
         * retain the next glyph in a new line by inserting a NUL when space
         * remains in the bounded cache. */
        if (*p != ' ' && *p != '\r' && *p != '\n')
        {
            size_t tail = strlen(p);
            if ((size_t)(p - text) + tail + 2 >= sizeof(text))
            {
                *p = 0;
                break;
            }
            memmove(p + 1, p, tail + 1);
        }
        *p++ = 0;
    }
}

static unsigned char *decode_utf16(const unsigned char *source,
    size_t bytes, bool le)
{
    unsigned char *out = (unsigned char *)text;
    while (bytes >= 2 && out - (unsigned char *)text < TEXT_BYTES - 5)
    {
        unsigned u = le ? source[0] | (source[1] << 8) :
                          (source[0] << 8) | source[1];
        source += 2;
        bytes -= 2;
        if (u >= 0xd800 && u <= 0xdbff)
        {
            unsigned low = bytes >= 2 ?
                (le ? source[0] | (source[1] << 8) :
                      (source[0] << 8) | source[1]) : 0;
            if (low >= 0xdc00 && low <= 0xdfff)
            {
                u = 0x10000 + ((u - 0xd800) << 10) + low - 0xdc00;
                source += 2;
                bytes -= 2;
            }
            else
                u = 0xfffd;
        }
        else if (u >= 0xdc00 && u <= 0xdfff)
            u = 0xfffd;
        if (!u)
            break;
        out = utf8encode(u, out);
    }
    return out;
}

static void decode(const unsigned char *source, size_t length, int encoding)
{
    unsigned char *end = (unsigned char *)text;
    if (encoding == 0)
        end = iso_decode_ex(source, end, ISO_8859_1, length,
                            sizeof(text) - 1);
    else if (encoding == 1 || encoding == 2)
    {
        bool le = false;
        if (encoding == 1 && length >= 2 && utf16_has_bom(source, &le))
        {
            source += 2;
            length -= 2;
        }
        end = decode_utf16(source, length, le);
    }
    else if (encoding == 3)
    {
        length = MIN(length, sizeof(text) - 1);
        memcpy(text, source, length);
        end += length;
    }
    *end = 0;
}

static uint32_t frame_size(const unsigned char *p, bool syncsafe)
{
    uint32_t size = 0;
    for (int i = 0; i < 4; i++)
    {
        if (syncsafe && (p[i] & 0x80))
            return UINT32_MAX;
        size = (size << (syncsafe ? 7 : 8)) | p[i];
    }
    return size;
}

static bool embedded(int fd)
{
    unsigned char header[10];
    if (read(fd, header, 10) != 10 || memcmp(header, "ID3", 3) ||
        (header[3] != 3 && header[3] != 4) || (header[5] & 0xc0))
        return false;
    int version = header[3];
    uint32_t remaining = frame_size(header + 6, true);
    if (remaining == UINT32_MAX || remaining > 16 * 1024 * 1024 ||
        (off_t)remaining > filesize(fd) - 10)
        return false;
    /* Bound both storage work and malformed frame traversal. Unsupported
     * compression/unsynchronisation remains available in the Lyrics plugin. */
    for (int frame = 0; frame < 128 && remaining >= 10; frame++)
    {
        if (read(fd, header, 10) != 10 || !header[0])
            return false;
        remaining -= 10;
        uint32_t size = frame_size(header + 4, version == 4);
        if (!size || size > remaining)
            return false;
        if (!memcmp(header, "USLT", 4) && !header[9] && size >= 5)
        {
            size_t n = MIN(size, RAW_BYTES);
            if (read(fd, raw, n) != (ssize_t)n)
                return false;
            memset(raw + n, 0, 4);
            int encoding = raw[0];
            size_t pos = 4, step = encoding == 1 || encoding == 2 ? 2 : 1;
            if (encoding > 3)
                return false;
            while (pos + step <= n)
            {
                bool terminator = !raw[pos] && (step == 1 || !raw[pos + 1]);
                pos += step;
                if (terminator)
                {
                    /* UTF-16 lyrics may inherit the descriptor's BOM. */
                    bool le = encoding == 1 && n >= 6 &&
                              raw[4] == 0xff && raw[5] == 0xfe;
                    if (encoding == 1 && !(pos + 2 <= n &&
                        ((raw[pos] == 0xff && raw[pos + 1] == 0xfe) ||
                         (raw[pos] == 0xfe && raw[pos + 1] == 0xff))))
                    {
                        unsigned char *end = decode_utf16(raw + pos,
                            n - pos, le);
                        *end = 0;
                    }
                    else
                        decode(raw + pos, n - pos, encoding);
                    return text[0] != 0;
                }
            }
            return false;
        }
        if (lseek(fd, size, SEEK_CUR) < 0)
            return false;
        remaining -= size;
        yield();
    }
    return false;
}

/* Display LRC as ordinary scrollable lyrics. Timing and synchronized lyrics
 * remain features of the existing long-Center Lyrics plugin. */
static void strip_lrc(void)
{
    char *src = text, *dst = text;
    while (*src)
    {
        while (*src == '[')
        {
            char *end = strchr(src, ']');
            if (!end || end - src > 96)
                break;
            bool timing = src[1] >= '0' && src[1] <= '9' &&
                          memchr(src, ':', end - src);
            bool metadata = !strncmp(src, "[ar:", 4) ||
                !strncmp(src, "[ti:", 4) || !strncmp(src, "[al:", 4) ||
                !strncmp(src, "[by:", 4) || !strncmp(src, "[offset:", 8) ||
                !strncmp(src, "[length:", 8);
            if (!timing && !metadata)
                break;
            src = end + 1;
        }
        while (*src && *src != '\n')
            *dst++ = *src++;
        if (*src)
            *dst++ = *src++;
    }
    *dst = 0;
}

bool ipodjs_wps_text_load(const char *track_path)
{
    char path[MAX_PATH];
    static const char * const extensions[] = { ".lrc", ".txt" };
    text[0] = 0;
    line_count = top_line = 0;
    strmemccpy(path, track_path, sizeof(path));
    char *slash = strrchr(path, '/');
    char *ext = strrchr(path, '.');
    if (!ext || (slash && ext < slash))
        ext = path + strlen(path);
    size_t offset = ext - path;
    if (offset + 5 < sizeof(path))
        for (unsigned i = 0; i < ARRAYLEN(extensions); i++)
        {
            strcpy(path + offset, extensions[i]);
            int fd = open(path, O_RDONLY);
            if (fd < 0)
                continue;
            ssize_t n = read(fd, raw, RAW_BYTES);
            close(fd);
            if (n <= 0)
                continue;
            memset(raw + n, 0, 4);
            size_t start = n >= 3 && !memcmp(raw, "\xef\xbb\xbf", 3) ? 3 : 0;
            int encoding = n >= 2 &&
                ((raw[0] == 0xff && raw[1] == 0xfe) ||
                 (raw[0] == 0xfe && raw[1] == 0xff)) ? 1 : 3;
            decode(raw + start, n - start, encoding);
            if (i == 0)
                strip_lrc();
            layout();
            if (line_count)
                return true;
        }
    int fd = open(track_path, O_RDONLY);
    if (fd < 0)
        return false;
    bool found = embedded(fd);
    close(fd);
    if (found)
        layout();
    return found && line_count > 0;
}

void ipodjs_wps_text_set(const char *value)
{
    strmemccpy(text, value ? value : "", sizeof(text));
    layout();
}

void ipodjs_wps_text_scroll(int delta)
{
    top_line = MAX(0, MIN(MAX(0, line_count - VISIBLE_LINES),
                          top_line + delta));
}

const char *ipodjs_wps_text_line(int row)
{
    row += top_line;
    return row >= 0 && row < line_count ? text + lines[row] : "";
}

int ipodjs_wps_text_count(void) { return line_count; }
int ipodjs_wps_text_top(void) { return top_line; }
