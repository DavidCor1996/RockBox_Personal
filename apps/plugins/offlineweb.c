/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Offline Internet browser for local early-web archives.
 *
 ****************************************************************************/

#include "plugin.h"

#define OW_ROOT          ROCKBOX_DIR "/offlineweb"
#define OW_INDEX         OW_ROOT "/index.html"
#define OW_SHORTCUTS     "rockbox:shortcuts"
#define OW_PAGES         OW_ROOT "/cache/pages.tsv"
#define OW_HISTORY       OW_ROOT "/cache/history.tsv"
#define OW_FAVORITES     OW_ROOT "/cache/favorites.tsv"
#define OW_CURSOR        OW_ROOT "/assets/cursor.bmp"

#define OW_MAX_PAGES     128
#define OW_MAX_LINES     192
#define OW_MAX_LINKS     192
#define OW_LINE_LEN      88
#define OW_FIELD_LEN     80
#define OW_STACK_LEN     32

#define OW_STYLE_TEXT    0x00
#define OW_STYLE_HEADING 0x01
#define OW_STYLE_LINK    0x02
#define OW_STYLE_IMAGE   0x04
#define OW_STYLE_RULE    0x08

#define OW_TOP_H         17
#define OW_BOTTOM_H      13
#define OW_MARGIN_X      3
#define OW_CURSOR_W      18
#define OW_CURSOR_H      18
#define OW_CURSOR_STEP   12
#define OW_IMAGE_MAX_H   (LCD_HEIGHT - OW_TOP_H - OW_BOTTOM_H - 8)
#define OW_BROWSER_CONTINUE -1000

#ifdef HAVE_LCD_COLOR
#define OW_IPODJS_HEADER_TOP       LCD_RGBPACK(252, 253, 253)
#define OW_IPODJS_HEADER_BOTTOM    LCD_RGBPACK(174, 178, 183)
#define OW_IPODJS_HEADER_DARK      LCD_RGBPACK(24, 29, 38)
#define OW_IPODJS_HEADER_DARK_LINE LCD_RGBPACK(54, 60, 70)
#define OW_IPODJS_SCREEN_BG        LCD_RGBPACK(255, 255, 255)
#define OW_IPODJS_DARK_BG          LCD_RGBPACK(18, 20, 24)
#define OW_IPODJS_DARK_PANEL       LCD_RGBPACK(24, 27, 32)
#define OW_IPODJS_TEXT             LCD_RGBPACK(0, 0, 0)
#define OW_IPODJS_DARK_TEXT        LCD_RGBPACK(239, 242, 246)
#define OW_IPODJS_MUTED_TEXT       LCD_RGBPACK(99, 101, 103)
#define OW_IPODJS_DARK_MUTED       LCD_RGBPACK(166, 173, 184)
#define OW_IPODJS_SPLIT            LCD_RGBPACK(210, 210, 210)
#define OW_IPODJS_ACTIVE_TOP       LCD_RGBPACK(107, 200, 254)
#define OW_IPODJS_ACTIVE_BOTTOM    LCD_RGBPACK(0, 92, 192)
#define OW_IPODJS_DARK_ACTIVE      LCD_RGBPACK(38, 146, 226)
#endif

struct ow_page {
    char title[OW_FIELD_LEN];
    char url[OW_FIELD_LEN];
    char path[MAX_PATH];
    char source[32];
    char neighborhood[32];
    char author[48];
    char archived[32];
    char keywords[96];
};

struct ow_link {
    char label[OW_FIELD_LEN];
    char target[MAX_PATH];
};

struct ow_render_line {
    char text[OW_LINE_LEN];
    char image_path[MAX_PATH];
    int image_height;
    int link;
    unsigned char style;
};

struct ow_render {
    struct ow_render_line lines[OW_MAX_LINES];
    int line_count;
    struct ow_link links[OW_MAX_LINKS];
    int link_count;
};

static struct ow_page pages[OW_MAX_PAGES];
static int page_count;
static struct ow_render render;
static char current_path[MAX_PATH];
static char back_stack[OW_STACK_LEN][MAX_PATH];
static int back_count;
static int browser_scroll;
static int browser_selected_link;
static int browser_zoom;
static int mouse_x;
static int mouse_y;
static bool cursor_loaded;
static fb_data image_pixels[LCD_WIDTH * OW_IMAGE_MAX_H];
static fb_data cursor_pixels[OW_CURSOR_W * OW_CURSOR_H];
static struct bitmap cursor_bitmap;

static const char *ow_basename(const char *path);
static int ow_line_height(void);

#ifdef HAVE_LCD_COLOR
static bool ow_dark(void)
{
    return rb->global_settings && rb->global_settings->ui_engine_dark_mode;
}

static unsigned ow_color_screen(void)
{
    return ow_dark() ? OW_IPODJS_DARK_BG : OW_IPODJS_SCREEN_BG;
}

static unsigned ow_color_panel(void)
{
    return ow_dark() ? OW_IPODJS_DARK_PANEL : OW_IPODJS_SCREEN_BG;
}

static unsigned ow_color_header(void)
{
    return ow_dark() ? OW_IPODJS_HEADER_DARK : OW_IPODJS_HEADER_BOTTOM;
}

static unsigned ow_color_split(void)
{
    return ow_dark() ? OW_IPODJS_HEADER_DARK_LINE : OW_IPODJS_SPLIT;
}

static unsigned ow_color_text(void)
{
    return ow_dark() ? OW_IPODJS_DARK_TEXT : OW_IPODJS_TEXT;
}

static unsigned ow_color_muted(void)
{
    return ow_dark() ? OW_IPODJS_DARK_MUTED : OW_IPODJS_MUTED_TEXT;
}

static unsigned ow_color_link(void)
{
    return ow_dark() ? OW_IPODJS_DARK_TEXT : LCD_RGBPACK(0, 70, 190);
}

static unsigned ow_color_selected(void)
{
    return ow_dark() ? OW_IPODJS_DARK_ACTIVE : OW_IPODJS_ACTIVE_BOTTOM;
}
#endif

static void ow_mkdirs(void)
{
    rb->mkdir(OW_ROOT);
    rb->mkdir(OW_ROOT "/geocities");
    rb->mkdir(OW_ROOT "/angelfire");
    rb->mkdir(OW_ROOT "/tripod");
    rb->mkdir(OW_ROOT "/yahoo");
    rb->mkdir(OW_ROOT "/myspace");
    rb->mkdir(OW_ROOT "/archive");
    rb->mkdir(OW_ROOT "/images");
    rb->mkdir(OW_ROOT "/gifs");
    rb->mkdir(OW_ROOT "/midi");
    rb->mkdir(OW_ROOT "/cache");
    rb->mkdir(OW_ROOT "/assets");
}

static void ow_load_cursor(void)
{
    int rc;

    rb->memset(&cursor_bitmap, 0, sizeof(cursor_bitmap));
    cursor_bitmap.width = OW_CURSOR_W;
    cursor_bitmap.height = OW_CURSOR_H;
    cursor_bitmap.data = (unsigned char *)cursor_pixels;
    rc = rb->read_bmp_file(OW_CURSOR, &cursor_bitmap, sizeof(cursor_pixels),
                           FORMAT_NATIVE | FORMAT_TRANSPARENT, NULL);
    cursor_loaded = rc > 0 &&
                    cursor_bitmap.width > 0 &&
                    cursor_bitmap.height > 0;
}

static void ow_chomp(char *s)
{
    int len = rb->strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
        s[--len] = '\0';
}

static char *ow_field(char **line)
{
    char *start = *line;
    char *tab = rb->strchr(start, '\t');
    if (tab)
    {
        *tab = '\0';
        *line = tab + 1;
    }
    else
    {
        *line = start + rb->strlen(start);
    }
    return start;
}

static void ow_load_pages(void)
{
    int fd;
    char line[512];

    page_count = 0;
    fd = rb->open(OW_PAGES, O_RDONLY);
    if (fd < 0)
        return;

    while (page_count < OW_MAX_PAGES &&
           rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *p = line;
        struct ow_page *page;

        ow_chomp(line);
        if (line[0] == '#' || line[0] == '\0')
            continue;

        page = &pages[page_count];
        rb->strlcpy(page->title, ow_field(&p), sizeof(page->title));
        rb->strlcpy(page->url, ow_field(&p), sizeof(page->url));
        rb->strlcpy(page->path, ow_field(&p), sizeof(page->path));
        rb->strlcpy(page->source, ow_field(&p), sizeof(page->source));
        rb->strlcpy(page->neighborhood, ow_field(&p),
                    sizeof(page->neighborhood));
        rb->strlcpy(page->author, ow_field(&p), sizeof(page->author));
        rb->strlcpy(page->archived, ow_field(&p), sizeof(page->archived));
        rb->strlcpy(page->keywords, ow_field(&p), sizeof(page->keywords));
        page_count++;
    }
    rb->close(fd);
}

static int ow_find_page_by_path(const char *path)
{
    int i;
    for (i = 0; i < page_count; i++)
        if (!rb->strcmp(pages[i].path, path))
            return i;
    return -1;
}

static bool ow_is_shortcuts_path(const char *path)
{
    return path && !rb->strcmp(path, OW_SHORTCUTS);
}

static bool ow_has_ext(const char *path, const char *exts)
{
    const char *dot = rb->strrchr(path, '.');
    return dot && rb->strcasestr(exts, dot) != NULL;
}

static void ow_parent_dir(const char *path, char *dir, size_t size)
{
    char *slash;
    rb->strlcpy(dir, path, size);
    slash = rb->strrchr(dir, '/');
    if (slash && slash != dir)
        *slash = '\0';
    else
        rb->strlcpy(dir, "/", size);
}

static int ow_open_candidate(const char *path, char *resolved, size_t size)
{
    int fd;

    if (!path || !path[0])
        return -1;

    if (resolved && size > 0)
        rb->strlcpy(resolved, path, size);

    fd = rb->open_utf8(path, O_RDONLY);
    if (fd >= 0)
        return fd;

    return rb->open(path, O_RDONLY);
}

static int ow_url_to_archive_path(const char *url, char *out, size_t size)
{
    const char *host;
    const char *path;
    const char *host_end;
    const char *path_end;
    const char *mark;
    int scheme_len = 0;
    int host_len;
    int path_len;

    if (!url || !out || size <= 0)
        return -1;

    if (!rb->strncasecmp(url, "http://", 7))
        scheme_len = 7;
    else if (!rb->strncasecmp(url, "https://", 8))
        scheme_len = 8;
    else
        return -1;

    host = url + scheme_len;
    path = rb->strchr(host, '/');
    host_end = path ? path : host + rb->strlen(host);
    if (!path)
        path = "/index.html";

    host_len = host_end - host;
    if (host_len <= 0 || host_len >= 96)
        return -1;

    path_end = path + rb->strlen(path);
    mark = rb->strchr(path, '?');
    if (mark && mark < path_end)
        path_end = mark;
    mark = rb->strchr(path, '#');
    if (mark && mark < path_end)
        path_end = mark;
    path_len = path_end - path;
    if (path_len <= 0 || (path_len == 1 && path[0] == '/'))
    {
        path = "/index.html";
        path_len = rb->strlen(path);
    }

    rb->snprintf(out, size, OW_ROOT "/archive/%.*s%.*s",
                 host_len, host, path_len, path);
    return 0;
}

static int ow_open_read_resolved(const char *path, char *resolved, size_t size)
{
    char candidate[MAX_PATH];
    int fd;

    if (ow_url_to_archive_path(path, candidate, sizeof(candidate)) == 0)
    {
        fd = ow_open_candidate(candidate, resolved, size);
        if (fd >= 0)
            return fd;
        if (rb->strrchr(candidate, '.') == NULL)
        {
            rb->strlcat(candidate, ".html", sizeof(candidate));
            fd = ow_open_candidate(candidate, resolved, size);
            if (fd >= 0)
                return fd;
        }
    }

    fd = ow_open_candidate(path, resolved, size);
    if (fd >= 0)
        return fd;

    if (path && path[0] == '/' && path[1] != '\0')
    {
        fd = ow_open_candidate(path + 1, resolved, size);
        if (fd >= 0)
            return fd;
    }
    else if (path && path[0] != '/')
    {
        rb->snprintf(candidate, sizeof(candidate), "/%s", path);
        fd = ow_open_candidate(candidate, resolved, size);
        if (fd >= 0)
            return fd;
    }

    if (resolved && size > 0)
        rb->strlcpy(resolved, path ? path : "", size);
    return -1;
}

static bool ow_archive_host_root(const char *base, char *out, size_t size)
{
    const char *prefix = OW_ROOT "/archive/";
    const char *host;
    const char *slash;
    int host_len;

    if (!base || !out || size <= 0)
        return false;

    host = rb->strstr(base, prefix);
    if (!host)
        return false;
    host += rb->strlen(prefix);
    slash = rb->strchr(host, '/');
    if (!slash)
        return false;

    host_len = slash - host;
    if (host_len <= 0 || host_len >= MAX_PATH)
        return false;

    rb->snprintf(out, size, "%s%.*s", prefix, host_len, host);
    return true;
}

static void ow_strip_query_fragment(char *path)
{
    char *mark;

    if (!path)
        return;

    mark = rb->strchr(path, '#');
    if (mark)
        *mark = '\0';
    mark = rb->strchr(path, '?');
    if (mark)
        *mark = '\0';
}

static void ow_join_path(char *out, size_t size, const char *base,
                         const char *href)
{
    char dir[MAX_PATH];

    if (!href || !href[0])
    {
        out[0] = '\0';
        return;
    }

    if (!rb->strncasecmp(href, "file://", 7))
        href += 7;

    if (href[0] == '/')
    {
        if (ow_archive_host_root(base, dir, sizeof(dir)))
            rb->snprintf(out, size, "%s%s", dir, href);
        else
            rb->strlcpy(out, href, size);
        ow_strip_query_fragment(out);
        return;
    }

    if (!rb->strncasecmp(href, "http://", 7) ||
        !rb->strncasecmp(href, "https://", 8))
    {
        int i;
        for (i = 0; i < page_count; i++)
        {
            if (!rb->strcasecmp(pages[i].url, href))
            {
                rb->strlcpy(out, pages[i].path, size);
                return;
            }
        }
        rb->strlcpy(out, href, size);
        ow_strip_query_fragment(out);
        return;
    }

    ow_parent_dir(base, dir, sizeof(dir));
    rb->snprintf(out, size, "%s/%s", dir, href);
    ow_strip_query_fragment(out);
}

static void ow_log_path(const char *file, const char *path)
{
    int page = ow_find_page_by_path(path);
    int fd;

    if (ow_is_shortcuts_path(path))
        return;

    fd = rb->open(file, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0)
        return;

    if (page >= 0)
        rb->fdprintf(fd, "%s\t%s\t%s\n", pages[page].title,
                     pages[page].url, pages[page].path);
    else
        rb->fdprintf(fd, "%s\t%s\t%s\n", path, path, path);
    rb->close(fd);
}

static int ow_chars_per_line(void)
{
    int w = 6;
    int h;
    int chars;

    rb->lcd_getstringsize("M", &w, &h);
    if (w <= 0)
        w = 6;
    chars = (LCD_WIDTH - OW_MARGIN_X * 2) / w;
    if (browser_zoom > 0)
        chars = chars * 100 / (100 + browser_zoom * 30);
    if (chars < 12)
        chars = 12;
    if (chars >= OW_LINE_LEN)
        chars = OW_LINE_LEN - 1;
    return chars;
}

static void ow_add_line_styled(const char *text, unsigned char style, int link)
{
    struct ow_render_line *line;

    if (render.line_count >= OW_MAX_LINES)
        return;

    line = &render.lines[render.line_count++];
    rb->strlcpy(line->text, text ? text : "", sizeof(line->text));
    line->image_path[0] = '\0';
    line->image_height = 0;
    line->style = style;
    line->link = link;
}

static void ow_add_image(const char *path, const char *label, int link,
                         int height)
{
    struct ow_render_line *line;

    if (render.line_count >= OW_MAX_LINES)
        return;

    line = &render.lines[render.line_count++];
    rb->snprintf(line->text, sizeof(line->text), "%s",
                 label && label[0] ? label : ow_basename(path));
    rb->strlcpy(line->image_path, path ? path : "", sizeof(line->image_path));
    line->image_height = MIN(MAX(height, 36), OW_IMAGE_MAX_H);
    line->style = OW_STYLE_IMAGE;
    line->link = link;
}

static void ow_add_line(const char *text)
{
    ow_add_line_styled(text, OW_STYLE_TEXT, -1);
}

static void ow_add_blank(void)
{
    if (render.line_count > 0 &&
        render.lines[render.line_count - 1].text[0] == '\0')
    {
        return;
    }
    ow_add_line("");
}

static void ow_ensure_text_line(unsigned char style, int link)
{
    if (render.line_count <= 0)
    {
        ow_add_line_styled("", style, link);
        return;
    }

    if (render.lines[render.line_count - 1].style != style ||
        render.lines[render.line_count - 1].link != link)
    {
        if (render.lines[render.line_count - 1].text[0] != '\0')
            ow_add_line_styled("", style, link);
    }
}

static char ow_entity_char(const char **p)
{
    const char *s = *p;

    if (!rb->strncasecmp(s, "&amp;", 5))
    {
        *p += 5;
        return '&';
    }
    if (!rb->strncasecmp(s, "&lt;", 4))
    {
        *p += 4;
        return '<';
    }
    if (!rb->strncasecmp(s, "&gt;", 4))
    {
        *p += 4;
        return '>';
    }
    if (!rb->strncasecmp(s, "&quot;", 6))
    {
        *p += 6;
        return '"';
    }
    if (!rb->strncasecmp(s, "&#39;", 5) ||
        !rb->strncasecmp(s, "&apos;", 6))
    {
        *p += (s[1] == '#') ? 5 : 6;
        return '\'';
    }

    (*p)++;
    return *s;
}

static void ow_append_word(const char *word, int len, int *col,
                           unsigned char style, int link)
{
    int max_cols = ow_chars_per_line();
    char chunk[OW_LINE_LEN];

    if (len <= 0 || render.line_count >= OW_MAX_LINES)
        return;

    while (len > 0)
    {
        int take;
        struct ow_render_line *line;

        ow_ensure_text_line(style, link);
        line = &render.lines[render.line_count - 1];

        if (*col > 0 && *col + len + 1 > max_cols)
        {
            ow_add_line_styled("", style, link);
            *col = 0;
            continue;
        }

        if (*col > 0)
        {
            rb->strlcat(line->text, " ", sizeof(line->text));
            (*col)++;
        }

        take = MIN(len, max_cols - *col);
        if (take <= 0)
        {
            ow_add_line_styled("", style, link);
            *col = 0;
            continue;
        }

        rb->memcpy(chunk, word, take);
        chunk[take] = '\0';
        rb->strlcat(line->text, chunk, sizeof(line->text));
        *col += take;
        word += take;
        len -= take;

        if (len > 0)
        {
            ow_add_line_styled("", style, link);
            *col = 0;
        }
    }
}

static void ow_add_text(const char *text, int *col,
                        unsigned char style, int link)
{
    const char *p = text;
    char word[OW_LINE_LEN];
    int len = 0;

    while (*p)
    {
        char c;

        if (*p == '<')
            break;

        if (*p == '&')
            c = ow_entity_char(&p);
        else
            c = *p++;

        if (c == '\r' || c == '\n' || c == '\t' || c == ' ')
        {
            ow_append_word(word, len, col, style, link);
            len = 0;
            continue;
        }

        if (len < (int)sizeof(word) - 1)
            word[len++] = c;
        else
        {
            ow_append_word(word, len, col, style, link);
            len = 0;
            word[len++] = c;
        }
    }

    ow_append_word(word, len, col, style, link);
}

static void ow_attr_value(const char *tag, const char *attr,
                          char *out, size_t size)
{
    char *p = rb->strcasestr(tag, attr);
    out[0] = '\0';
    if (!p)
        return;
    p += rb->strlen(attr);
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p != '=')
        return;
    p++;
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p == '"' || *p == '\'')
    {
        char q = *p++;
        int n = 0;
        while (*p && *p != q && n < (int)size - 1)
            out[n++] = *p++;
        out[n] = '\0';
    }
    else
    {
        int n = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '>' &&
               n < (int)size - 1)
            out[n++] = *p++;
        out[n] = '\0';
    }
}

static int ow_attr_int(const char *tag, const char *attr, int fallback)
{
    char value[24];
    int out = 0;
    int i = 0;

    ow_attr_value(tag, attr, value, sizeof(value));
    while (value[i] >= '0' && value[i] <= '9')
    {
        out = out * 10 + value[i] - '0';
        i++;
    }
    return out > 0 ? out : fallback;
}

static bool ow_resolve_image_path(const char *path, char *resolved,
                                  size_t size)
{
    int fd;
    char candidate[MAX_PATH];

    if (path && !ow_has_ext(path, ".bmp"))
    {
        rb->snprintf(candidate, sizeof(candidate), "%s.bmp", path);
        fd = ow_open_read_resolved(candidate, resolved, size);
        if (fd >= 0)
        {
            rb->close(fd);
            return true;
        }
    }

    fd = ow_open_read_resolved(path, resolved, size);
    if (fd >= 0)
    {
        rb->close(fd);
        if (ow_has_ext(resolved, ".bmp.jpg.jpeg"))
            return true;
    }

    rb->strlcpy(resolved, path ? path : "", size);
    return false;
}

static int ow_add_link(const char *base, const char *tag, const char *label)
{
    char href[MAX_PATH];
    char path[MAX_PATH];
    struct ow_link *link;

    if (render.link_count >= OW_MAX_LINKS)
        return -1;
    ow_attr_value(tag, "href", href, sizeof(href));
    if (!href[0])
        ow_attr_value(tag, "src", href, sizeof(href));
    if (!href[0])
        return -1;

    ow_join_path(path, sizeof(path), base, href);
    link = &render.links[render.link_count];
    rb->strlcpy(link->target, path, sizeof(link->target));
    if (label && label[0])
        rb->strlcpy(link->label, label, sizeof(link->label));
    else
        rb->strlcpy(link->label, href, sizeof(link->label));
    render.link_count++;
    return render.link_count - 1;
}

static int ow_add_direct_link(const char *target, const char *label)
{
    struct ow_link *link;

    if (render.link_count >= OW_MAX_LINKS)
        return -1;

    link = &render.links[render.link_count];
    rb->strlcpy(link->target, target ? target : "", sizeof(link->target));
    rb->strlcpy(link->label, label && label[0] ? label : link->target,
                sizeof(link->label));
    render.link_count++;
    return render.link_count - 1;
}

static const char *ow_basename(const char *path)
{
    const char *slash = rb->strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void ow_render_shortcuts(void)
{
    int i;

    rb->memset(&render, 0, sizeof(render));
    ow_add_line_styled("Offline websites", OW_STYLE_HEADING, -1);
    ow_add_blank();

    if (page_count <= 0)
    {
        ow_add_line("No offline websites found.");
        ow_add_line("Use RockPod Website Sync to add pages.");
        return;
    }

    for (i = 0; i < page_count && render.line_count < OW_MAX_LINES - 2; i++)
    {
        int link = ow_add_direct_link(pages[i].path, pages[i].title);

        if (link < 0)
            break;

        ow_add_line_styled(pages[i].title, OW_STYLE_LINK, link);
    }
}

static void ow_render_html(const char *path)
{
    int fd;
    char line[512];
    char resolved[MAX_PATH];
    int col = 0;
    int active_link = -1;
    unsigned char active_style = OW_STYLE_TEXT;
    bool skip_content = false;
    bool skip_head = false;
    bool skip_tag_continuation = false;

    rb->memset(&render, 0, sizeof(render));
    fd = ow_open_read_resolved(path, resolved, sizeof(resolved));
    if (fd < 0)
    {
        ow_add_line("Cannot open page");
        ow_add_line(path);
        return;
    }

    while (rb->read_line(fd, line, sizeof(line)) > 0 &&
           render.line_count < OW_MAX_LINES - 2)
    {
        char *p = line;
        while (*p)
        {
            char *tag = rb->strchr(p, '<');
            if (skip_tag_continuation)
            {
                tag = rb->strchr(p, '>');
                if (!tag)
                    break;
                p = tag + 1;
                skip_tag_continuation = false;
                continue;
            }
            if (!tag)
            {
                if (!skip_content && !skip_head)
                    ow_add_text(p, &col, active_style, active_link);
                break;
            }
            *tag = '\0';
            if (!skip_content && !skip_head)
                ow_add_text(p, &col, active_style, active_link);
            p = tag + 1;
            tag = rb->strchr(p, '>');
            if (!tag)
            {
                skip_tag_continuation = true;
                break;
            }
            *tag = '\0';
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
                p++;

            if (!rb->strncasecmp(p, "script", 6) ||
                !rb->strncasecmp(p, "style", 5))
            {
                skip_content = true;
            }
            else if (!rb->strncasecmp(p, "/script", 7) ||
                     !rb->strncasecmp(p, "/style", 6))
            {
                skip_content = false;
            }
            else if (!rb->strncasecmp(p, "head", 4))
            {
                skip_head = true;
            }
            else if (!rb->strncasecmp(p, "/head", 5))
            {
                skip_head = false;
            }
            else if (!skip_content && !skip_head)
            {
                if (!rb->strncasecmp(p, "br", 2) ||
                    !rb->strncasecmp(p, "p", 1) ||
                    !rb->strncasecmp(p, "/p", 2) ||
                    !rb->strncasecmp(p, "div", 3) ||
                    !rb->strncasecmp(p, "/div", 4) ||
                    !rb->strncasecmp(p, "tr", 2))
                {
                    ow_add_blank();
                    col = 0;
                }
                else if (!rb->strncasecmp(p, "hr", 2))
                {
                    ow_add_line_styled("----------------------------------------",
                                       OW_STYLE_RULE, -1);
                    col = 0;
                }
                else if (!rb->strncasecmp(p, "li", 2))
                {
                    ow_add_blank();
                    col = 0;
                    ow_add_text("*", &col, OW_STYLE_TEXT, -1);
                }
                else if (!rb->strncasecmp(p, "h1", 2) ||
                         !rb->strncasecmp(p, "h2", 2) ||
                         !rb->strncasecmp(p, "h3", 2) ||
                         !rb->strncasecmp(p, "h4", 2) ||
                         !rb->strncasecmp(p, "h5", 2) ||
                         !rb->strncasecmp(p, "h6", 2))
                {
                    ow_add_blank();
                    active_style = OW_STYLE_HEADING;
                    col = 0;
                }
                else if (!rb->strncasecmp(p, "/h", 2))
                {
                    active_style = OW_STYLE_TEXT;
                    ow_add_blank();
                    col = 0;
                }
                else if (!rb->strncasecmp(p, "a ", 2))
                {
                    active_link = ow_add_link(resolved, p, "");
                    col = 0;
                }
                else if (!rb->strncasecmp(p, "/a", 2))
                {
                    active_link = -1;
                    col = 0;
                }
                else if (!rb->strncasecmp(p, "img ", 4) ||
                         !rb->strncasecmp(p, "embed ", 6) ||
                         !rb->strncasecmp(p, "bgsound ", 8))
                {
                    char alt[OW_FIELD_LEN];
                    int width;
                    int height;
                    int link;

                    alt[0] = '\0';
                    ow_attr_value(p, "alt", alt, sizeof(alt));
                    if (!alt[0])
                        ow_attr_value(p, "title", alt, sizeof(alt));
                    width = ow_attr_int(p, "width", LCD_WIDTH - OW_MARGIN_X * 2);
                    height = ow_attr_int(p, "height", 72);
                    if (width > 0 && width > LCD_WIDTH - OW_MARGIN_X * 2)
                        height = height * (LCD_WIDTH - OW_MARGIN_X * 2) / width;
                    link = ow_add_link(resolved, p, alt);
                    if (link >= 0)
                    {
                        ow_add_image(render.links[link].target,
                                     alt[0] ? alt :
                                     ow_basename(render.links[link].target),
                                     link, height);
                        col = 0;
                    }
                }
            }
            p = tag + 1;
        }
    }
    rb->close(fd);

    if (render.line_count <= 0)
        ow_add_line("(blank page)");
}

static void ow_render_path(const char *path)
{
    if (ow_is_shortcuts_path(path))
        ow_render_shortcuts();
    else
        ow_render_html(path);
}

static int ow_open_path(const char *path, bool push_back);

static const char *ow_title_for_path(const char *path)
{
    int page = ow_find_page_by_path(path);

    if (ow_is_shortcuts_path(path))
        return "Offline Websites";
    if (page >= 0 && pages[page].title[0])
        return pages[page].title;
    if (!rb->strcmp(path, OW_INDEX))
        return "RockSearch Offline Internet";
    return ow_basename(path);
}

static int ow_line_height(void)
{
    int w;
    int h;

    rb->lcd_getstringsize("Ag", &w, &h);
    if (h < 8)
        h = 8;
    return h + 2 + browser_zoom * 2;
}

static int ow_visible_rows(void)
{
    return MAX(1, (LCD_HEIGHT - OW_TOP_H - OW_BOTTOM_H) / ow_line_height());
}

static int ow_line_item_height(const struct ow_render_line *line)
{
    if (line && (line->style & OW_STYLE_IMAGE))
        return MIN(MAX(line->image_height, 24), OW_IMAGE_MAX_H);
    return ow_line_height();
}

static void ow_clamp_browser_scroll(void)
{
    int viewport_h = LCD_HEIGHT - OW_TOP_H - OW_BOTTOM_H - 2;
    int content_h = 0;
    int first = render.line_count;
    int max_scroll;
    int i;

    for (i = render.line_count - 1; i >= 0; i--)
    {
        int item_h = ow_line_item_height(&render.lines[i]);

        if (render.lines[i].style & OW_STYLE_IMAGE)
            item_h += 3;
        if (content_h > 0 && content_h + item_h > viewport_h)
            break;
        content_h += item_h;
        first = i;
    }
    max_scroll = MAX(0, first);

    if (browser_scroll < 0)
        browser_scroll = 0;
    if (browser_scroll > max_scroll)
        browser_scroll = max_scroll;
}

static void ow_clamp_mouse(void)
{
    int max_x = LCD_WIDTH - 1;
    int max_y = LCD_HEIGHT - OW_BOTTOM_H - 1;

    if (mouse_x < 0)
        mouse_x = 0;
    if (mouse_y < OW_TOP_H)
        mouse_y = OW_TOP_H;
    if (mouse_x > max_x)
        mouse_x = max_x;
    if (mouse_y > max_y)
        mouse_y = max_y;
}

static int ow_link_at_screen_y(int screen_y)
{
    int i;
    int y = OW_TOP_H + 2;

    for (i = browser_scroll;
         i < render.line_count && y < LCD_HEIGHT - OW_BOTTOM_H; i++)
    {
        int item_h = ow_line_item_height(&render.lines[i]);
        if (screen_y >= y && screen_y < y + item_h)
            return render.lines[i].link;
        y += item_h + ((render.lines[i].style & OW_STYLE_IMAGE) ? 3 : 0);
    }
    return -1;
}

static void ow_update_mouse_link(void)
{
    browser_selected_link = ow_link_at_screen_y(mouse_y);
}

static int ow_first_visible_link(void)
{
    int i;
    int rows = ow_visible_rows();

    for (i = browser_scroll;
         i < render.line_count && i < browser_scroll + rows; i++)
    {
        if (render.lines[i].link >= 0)
            return render.lines[i].link;
    }
    return -1;
}

static void ow_pick_visible_link(void)
{
    int i;
    int rows = ow_visible_rows();
    int mouse_link;

    if (browser_selected_link >= 0)
    {
        for (i = browser_scroll;
             i < render.line_count && i < browser_scroll + rows; i++)
        {
            if (render.lines[i].link == browser_selected_link)
                return;
        }
    }

    mouse_link = ow_link_at_screen_y(mouse_y);
    browser_selected_link = mouse_link >= 0 ? mouse_link : ow_first_visible_link();
}

static int ow_line_for_link(int link)
{
    int i;

    for (i = 0; i < render.line_count; i++)
        if (render.lines[i].link == link)
            return i;
    return -1;
}

static void ow_ensure_link_visible(int link)
{
    int row = ow_line_for_link(link);
    int rows = ow_visible_rows();

    if (row < 0)
        return;

    if (row < browser_scroll)
        browser_scroll = row;
    else if (row >= browser_scroll + rows)
        browser_scroll = row - rows + 1;
    ow_clamp_browser_scroll();
}

static void ow_move_selected_link(int delta)
{
    int link;

    if (render.link_count <= 0)
        return;

    link = browser_selected_link;
    if (link < 0)
        link = (delta >= 0) ? 0 : render.link_count - 1;
    else
        link += delta;

    if (link < 0)
        link = 0;
    if (link >= render.link_count)
        link = render.link_count - 1;

    browser_selected_link = link;
    ow_ensure_link_visible(link);
}

static void ow_draw_bar_text(int x, int y, int width, const char *text)
{
    char buf[OW_LINE_LEN];
    int w;
    int h;
    int len;

    rb->strlcpy(buf, text ? text : "", sizeof(buf));
    len = rb->strlen(buf);
    rb->lcd_getstringsize(buf, &w, &h);
    while (len > 1 && w > width)
    {
        buf[--len] = '\0';
        rb->lcd_getstringsize(buf, &w, &h);
    }
    rb->lcd_putsxy(x, y, buf);
}

static bool ow_draw_image_line(struct ow_render_line *line, int x, int y,
                               int width)
{
    struct bitmap bm;
    char resolved[MAX_PATH];
    int rc = -1;
    int wanted_h = MIN(MAX(line->image_height, 24), OW_IMAGE_MAX_H);
    int wanted_w = MIN(width, LCD_WIDTH);

    if (!line->image_path[0])
        return false;
    if (!ow_resolve_image_path(line->image_path, resolved, sizeof(resolved)))
        return false;

    rb->memset(&bm, 0, sizeof(bm));
    bm.width = wanted_w;
    bm.height = wanted_h;
    bm.data = (unsigned char *)image_pixels;

    if (ow_has_ext(resolved, ".bmp"))
    {
        rc = rb->read_bmp_file(resolved, &bm, sizeof(image_pixels),
                               FORMAT_NATIVE | FORMAT_RESIZE |
                               FORMAT_KEEP_ASPECT, NULL);
    }
#ifdef HAVE_JPEG
    else if (ow_has_ext(resolved, ".jpg.jpeg"))
    {
        rc = rb->read_jpeg_file(resolved, &bm, sizeof(image_pixels),
                                FORMAT_NATIVE | FORMAT_RESIZE |
                                FORMAT_KEEP_ASPECT, NULL);
    }
#endif

    if (rc <= 0 || bm.width <= 0 || bm.height <= 0)
        return false;

    {
        int draw_w = MIN(bm.width, width);
        int draw_h = MIN(bm.height, wanted_h);
        int draw_x = x + MAX(0, (width - draw_w) / 2);

        rb->lcd_bitmap((const fb_data *)bm.data, draw_x, y, draw_w, draw_h);
    }
    return true;
}

static void ow_draw_cursor(void)
{
    int draw_w;
    int draw_h;

    if (!cursor_loaded)
        return;

    draw_w = MIN(cursor_bitmap.width, LCD_WIDTH - mouse_x);
    draw_h = MIN(cursor_bitmap.height, LCD_HEIGHT - OW_BOTTOM_H - mouse_y);
    if (draw_w <= 0 || draw_h <= 0)
        return;

    rb->lcd_bitmap_transparent((const fb_data *)cursor_bitmap.data,
                               mouse_x, mouse_y, draw_w, draw_h);
}

static void ow_draw_browser(const char *path)
{
    int old_fg = rb->lcd_get_foreground();
    int old_bg = rb->lcd_get_background();
    int line_h;
    int i;
    int y;
    char status[OW_LINE_LEN];

    ow_clamp_browser_scroll();
    ow_pick_visible_link();
    line_h = ow_line_height();

#ifdef HAVE_LCD_COLOR
    rb->lcd_set_background(ow_color_screen());
    rb->lcd_set_foreground(ow_color_screen());
#else
    rb->lcd_set_background(LCD_WHITE);
    rb->lcd_set_foreground(LCD_WHITE);
#endif
    rb->lcd_clear_display();
    rb->lcd_set_drawmode(DRMODE_SOLID);

#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(ow_color_header());
#else
    rb->lcd_set_foreground(LCD_BLACK);
#endif
    rb->lcd_fillrect(0, 0, LCD_WIDTH, OW_TOP_H);
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(ow_color_text());
    rb->lcd_set_background(ow_color_header());
#else
    rb->lcd_set_foreground(LCD_BLACK);
#endif
    ow_draw_bar_text(OW_MARGIN_X, 2, LCD_WIDTH - OW_MARGIN_X * 2,
                     ow_title_for_path(path));
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(ow_color_split());
#endif
    rb->lcd_hline(0, LCD_WIDTH - 1, OW_TOP_H - 1);

    y = OW_TOP_H + 2;
    for (i = browser_scroll;
         i < render.line_count && y < LCD_HEIGHT - OW_BOTTOM_H; i++)
    {
        struct ow_render_line *line = &render.lines[i];
        bool selected = line->link >= 0 && line->link == browser_selected_link;
        int item_h = ow_line_item_height(line);

        if (selected && !(line->style & OW_STYLE_IMAGE))
        {
#ifdef HAVE_LCD_COLOR
            rb->lcd_set_foreground(ow_color_selected());
            rb->lcd_fillrect(0, y - 1, LCD_WIDTH, item_h);
#else
            rb->lcd_set_foreground(LCD_BLACK);
            rb->lcd_set_drawmode(DRMODE_COMPLEMENT);
            rb->lcd_fillrect(0, y - 1, LCD_WIDTH, item_h);
#endif
            rb->lcd_set_drawmode(DRMODE_SOLID);
        }

        if (line->style & OW_STYLE_RULE)
        {
#ifdef HAVE_LCD_COLOR
            rb->lcd_set_foreground(ow_color_split());
#else
            rb->lcd_set_foreground(LCD_BLACK);
#endif
            rb->lcd_hline(OW_MARGIN_X, LCD_WIDTH - OW_MARGIN_X, y + line_h / 2);
        }
        else if ((line->style & OW_STYLE_IMAGE) && line->image_path[0])
        {
            bool drew;
#ifdef HAVE_LCD_COLOR
            rb->lcd_set_background(ow_color_panel());
            rb->lcd_set_foreground(ow_color_muted());
#else
            rb->lcd_set_background(LCD_WHITE);
            rb->lcd_set_foreground(LCD_BLACK);
#endif
            drew = ow_draw_image_line(line, OW_MARGIN_X, y,
                                      LCD_WIDTH - OW_MARGIN_X * 2);
            if (!drew)
                rb->lcd_putsxy(OW_MARGIN_X, y, line->text);
        }
        else
        {
#ifdef HAVE_LCD_COLOR
            if (line->style & OW_STYLE_HEADING)
                rb->lcd_set_foreground(ow_color_text());
            else if (line->link >= 0)
                rb->lcd_set_foreground(selected ? LCD_WHITE :
                                       ow_color_link());
            else if (line->style & OW_STYLE_IMAGE)
                rb->lcd_set_foreground(ow_color_muted());
            else
                rb->lcd_set_foreground(ow_color_muted());
#else
            rb->lcd_set_foreground(LCD_BLACK);
#endif
#ifdef HAVE_LCD_COLOR
            rb->lcd_set_background(selected ? ow_color_selected() :
                                   ow_color_panel());
#else
            rb->lcd_set_background(LCD_WHITE);
#endif
            rb->lcd_putsxy(OW_MARGIN_X, y, line->text);
        }
        y += item_h + ((line->style & OW_STYLE_IMAGE) ? 3 : 0);
    }

#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(ow_color_split());
#else
    rb->lcd_set_foreground(LCD_BLACK);
#endif
    rb->lcd_hline(0, LCD_WIDTH - 1, LCD_HEIGHT - OW_BOTTOM_H);
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(ow_color_muted());
    rb->lcd_set_background(ow_color_panel());
#else
    rb->lcd_set_foreground(LCD_BLACK);
#endif
    rb->snprintf(status, sizeof(status), "%d/%d  Z%d  Select open",
                 browser_scroll + 1, MAX(1, render.line_count),
                 browser_zoom + 1);
    ow_draw_bar_text(OW_MARGIN_X, LCD_HEIGHT - OW_BOTTOM_H + 1,
                     LCD_WIDTH - OW_MARGIN_X * 2, status);

    ow_draw_cursor();
    rb->lcd_update();
    rb->lcd_set_foreground(old_fg);
    rb->lcd_set_background(old_bg);
}

static void ow_scroll_browser(int delta)
{
    browser_scroll += delta;
    ow_clamp_browser_scroll();
    ow_update_mouse_link();
}

static void ow_move_mouse(int dx, int dy)
{
    mouse_x += dx;
    mouse_y += dy;
    ow_clamp_mouse();
    ow_update_mouse_link();
}

static int ow_follow_selected_link(void)
{
    int link = browser_selected_link;

    if (link < 0)
        link = ow_first_visible_link();
    if (link < 0 || link >= render.link_count)
    {
        rb->splash(HZ, "No link here");
        return OW_BROWSER_CONTINUE;
    }

    return ow_open_path(render.links[link].target, true);
}

static int ow_show_page(const char *path)
{
    bool redraw = true;
    int button;

    rb->strlcpy(current_path, path, sizeof(current_path));
    ow_log_path(OW_HISTORY, path);
    ow_render_path(path);
    browser_scroll = 0;
    mouse_x = LCD_WIDTH / 2;
    mouse_y = OW_TOP_H + 22;
    ow_clamp_mouse();
    browser_selected_link = ow_link_at_screen_y(mouse_y);
    ow_pick_visible_link();

    while (true)
    {
        if (redraw)
        {
            ow_draw_browser(path);
            redraw = false;
        }

        button = rb->button_get_w_tmo(HZ / 4);
        if ((button & BUTTON_MENU) && (button & BUTTON_SELECT))
            return PLUGIN_OK;

        switch (button)
        {
            case BUTTON_NONE:
                break;

            case BUTTON_SCROLL_FWD:
                if (ow_is_shortcuts_path(path))
                    ow_move_selected_link(1);
                else
                    ow_scroll_browser(1);
                redraw = true;
                break;

            case BUTTON_SCROLL_FWD | BUTTON_REPEAT:
                if (ow_is_shortcuts_path(path))
                    ow_move_selected_link(3);
                else
                    ow_scroll_browser(3);
                redraw = true;
                break;

            case BUTTON_SCROLL_BACK:
                if (ow_is_shortcuts_path(path))
                    ow_move_selected_link(-1);
                else
                    ow_scroll_browser(-1);
                redraw = true;
                break;

            case BUTTON_SCROLL_BACK | BUTTON_REPEAT:
                if (ow_is_shortcuts_path(path))
                    ow_move_selected_link(-3);
                else
                    ow_scroll_browser(-3);
                redraw = true;
                break;

            case BUTTON_LEFT:
                ow_move_mouse(-OW_CURSOR_STEP, 0);
                redraw = true;
                break;

            case BUTTON_LEFT | BUTTON_REPEAT:
                ow_move_mouse(-OW_CURSOR_STEP * 2, 0);
                redraw = true;
                break;

            case BUTTON_RIGHT:
                ow_move_mouse(OW_CURSOR_STEP, 0);
                redraw = true;
                break;

            case BUTTON_RIGHT | BUTTON_REPEAT:
                ow_move_mouse(OW_CURSOR_STEP * 2, 0);
                redraw = true;
                break;

            case BUTTON_MENU:
                ow_move_mouse(0, -OW_CURSOR_STEP);
                redraw = true;
                break;

            case BUTTON_MENU | BUTTON_REPEAT:
                ow_move_mouse(0, -OW_CURSOR_STEP * 2);
                redraw = true;
                break;

            case BUTTON_PLAY:
                ow_move_mouse(0, OW_CURSOR_STEP);
                redraw = true;
                break;

            case BUTTON_PLAY | BUTTON_REPEAT:
                ow_move_mouse(0, OW_CURSOR_STEP * 2);
                redraw = true;
                break;

            case BUTTON_SELECT | BUTTON_REL:
            {
                int ret;

                ow_update_mouse_link();
                ret = ow_follow_selected_link();
                if (ret == OW_BROWSER_CONTINUE)
                {
                    redraw = true;
                    break;
                }
                return ret;
            }

            default:
                if (button == SYS_USB_CONNECTED ||
                    rb->default_event_handler(button) == SYS_USB_CONNECTED)
                    return PLUGIN_USB_CONNECTED;
                break;
        }
    }
}

static int ow_open_path(const char *path, bool push_back)
{
    char plugin[MAX_PATH];
    char path_copy[MAX_PATH];
    int attr;

    rb->strlcpy(path_copy, path ? path : "", sizeof(path_copy));

    if (push_back && current_path[0] && back_count < OW_STACK_LEN)
        rb->strlcpy(back_stack[back_count++], current_path, MAX_PATH);

    if (ow_is_shortcuts_path(path_copy))
        return ow_show_page(path_copy);

    if (ow_has_ext(path_copy, ".gif.png.jpg.jpeg.bmp"))
    {
        attr = rb->filetype_get_attr(path_copy);
        if (rb->filetype_get_plugin(attr, plugin, sizeof(plugin)))
            return rb->plugin_open(plugin, path_copy);
    }

    if (ow_has_ext(path_copy, ".mid.midi.wav.mod.xm.s3m.it"))
    {
        attr = rb->filetype_get_attr(path_copy);
        if (rb->filetype_get_plugin(attr, plugin, sizeof(plugin)))
            return rb->plugin_open(plugin, path_copy);
    }

    return ow_show_page(path_copy);
}

enum plugin_status plugin_start(const void *parameter)
{
    int ret;
    (void)parameter;

    ow_mkdirs();
    ow_load_cursor();
    ow_load_pages();

    ret = ow_open_path(OW_SHORTCUTS, false);
    return ret == PLUGIN_USB_CONNECTED ? PLUGIN_USB_CONNECTED : PLUGIN_OK;
}
