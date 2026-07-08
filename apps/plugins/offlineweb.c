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
#define OW_PAGES         OW_ROOT "/cache/pages.tsv"
#define OW_HISTORY       OW_ROOT "/cache/history.tsv"
#define OW_FAVORITES     OW_ROOT "/cache/favorites.tsv"

#define OW_MAX_PAGES     768
#define OW_MAX_LIST      128
#define OW_MAX_LINES     420
#define OW_MAX_LINKS     96
#define OW_LINE_LEN      88
#define OW_FIELD_LEN     80
#define OW_QUERY_LEN     48
#define OW_STACK_LEN     32

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

struct ow_render {
    char lines[OW_MAX_LINES][OW_LINE_LEN];
    int line_count;
    struct ow_link links[OW_MAX_LINKS];
    int link_count;
};

static struct ow_page pages[OW_MAX_PAGES];
static int page_count;
static int list_indices[OW_MAX_LIST];
static int list_count;
static struct ow_render render;
static char current_path[MAX_PATH];
static char back_stack[OW_STACK_LEN][MAX_PATH];
static int back_count;
static char metadata_lines[8][OW_LINE_LEN];
static int metadata_count;

static void ow_mkdirs(void)
{
    rb->mkdir(OW_ROOT);
    rb->mkdir(OW_ROOT "/geocities");
    rb->mkdir(OW_ROOT "/angelfire");
    rb->mkdir(OW_ROOT "/tripod");
    rb->mkdir(OW_ROOT "/yahoo");
    rb->mkdir(OW_ROOT "/archive");
    rb->mkdir(OW_ROOT "/images");
    rb->mkdir(OW_ROOT "/gifs");
    rb->mkdir(OW_ROOT "/midi");
    rb->mkdir(OW_ROOT "/cache");
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
        rb->strlcpy(out, href, size);
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
        return;
    }

    ow_parent_dir(base, dir, sizeof(dir));
    rb->snprintf(out, size, "%s/%s", dir, href);
}

static void ow_log_path(const char *file, const char *path)
{
    int page = ow_find_page_by_path(path);
    int fd = rb->open(file, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0)
        return;

    if (page >= 0)
        rb->fdprintf(fd, "%s\t%s\t%s\n", pages[page].title,
                     pages[page].url, pages[page].path);
    else
        rb->fdprintf(fd, "%s\t%s\t%s\n", path, path, path);
    rb->close(fd);
}

static bool ow_contains(const char *a, const char *b)
{
    return b[0] == '\0' || rb->strcasestr(a, b) != NULL;
}

static void ow_add_line(const char *text)
{
    if (render.line_count >= OW_MAX_LINES)
        return;
    rb->strlcpy(render.lines[render.line_count++], text, OW_LINE_LEN);
}

static void ow_add_text(const char *text, int *col)
{
    char tmp[OW_LINE_LEN];
    const char *p = text;

    while (*p)
    {
        int n = 0;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
            p++;
        while (*p && *p != '<' && *p != '\r' && *p != '\n' &&
               n < OW_LINE_LEN - 1)
            tmp[n++] = *p++;
        tmp[n] = '\0';
        if (!n)
            break;
        if (*col + n >= OW_LINE_LEN - 2)
        {
            ow_add_line("");
            *col = 0;
        }
        if (render.line_count == 0 || render.lines[render.line_count - 1][0])
        {
            if (render.line_count == 0)
                ow_add_line("");
        }
        rb->strlcat(render.lines[render.line_count - 1], tmp, OW_LINE_LEN);
        *col += n;
    }
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

static void ow_add_link(const char *base, const char *tag, const char *label)
{
    char href[MAX_PATH];
    char path[MAX_PATH];
    struct ow_link *link;

    if (render.link_count >= OW_MAX_LINKS)
        return;
    ow_attr_value(tag, "href", href, sizeof(href));
    if (!href[0])
        ow_attr_value(tag, "src", href, sizeof(href));
    if (!href[0])
        return;

    ow_join_path(path, sizeof(path), base, href);
    link = &render.links[render.link_count];
    rb->strlcpy(link->target, path, sizeof(link->target));
    if (label && label[0])
        rb->strlcpy(link->label, label, sizeof(link->label));
    else
        rb->strlcpy(link->label, href, sizeof(link->label));
    render.link_count++;
}

static void ow_render_html(const char *path)
{
    int fd;
    char line[512];
    int col = 0;

    rb->memset(&render, 0, sizeof(render));
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
    {
        ow_add_line("Cannot open page");
        return;
    }

    ow_add_line("");
    while (rb->read_line(fd, line, sizeof(line)) > 0 &&
           render.line_count < OW_MAX_LINES - 2)
    {
        char *p = line;
        while (*p)
        {
            char *tag = rb->strchr(p, '<');
            if (!tag)
            {
                ow_add_text(p, &col);
                break;
            }
            *tag = '\0';
            ow_add_text(p, &col);
            p = tag + 1;
            tag = rb->strchr(p, '>');
            if (!tag)
                break;
            *tag = '\0';

            if (!rb->strncasecmp(p, "br", 2) ||
                !rb->strncasecmp(p, "p", 1) ||
                !rb->strncasecmp(p, "/p", 2) ||
                !rb->strncasecmp(p, "tr", 2) ||
                !rb->strncasecmp(p, "/h", 2))
            {
                ow_add_line("");
                col = 0;
            }
            if (!rb->strncasecmp(p, "a ", 2) ||
                !rb->strncasecmp(p, "area ", 5) ||
                !rb->strncasecmp(p, "img ", 4) ||
                !rb->strncasecmp(p, "embed ", 6) ||
                !rb->strncasecmp(p, "bgsound ", 8))
            {
                char label[OW_FIELD_LEN];
                rb->snprintf(label, sizeof(label), "[%d]", render.link_count + 1);
                ow_add_link(path, p, label);
            }
            p = tag + 1;
        }
    }
    rb->close(fd);

    if (render.link_count)
    {
        int i;
        ow_add_line("");
        ow_add_line("Links:");
        for (i = 0; i < render.link_count && render.line_count < OW_MAX_LINES; i++)
        {
            char tmp[OW_LINE_LEN];
            rb->snprintf(tmp, sizeof(tmp), "%d. %s", i + 1,
                         render.links[i].target);
            ow_add_line(tmp);
        }
    }
}

static const char *ow_page_line(int selected, void *data,
                                char *buffer, size_t buffer_len)
{
    (void)data;
    rb->strlcpy(buffer, render.lines[selected], buffer_len);
    return buffer;
}

static int ow_open_path(const char *path, bool push_back);

static const char *ow_metadata_line(int selected, void *data,
                                    char *buffer, size_t buffer_len)
{
    (void)data;
    rb->strlcpy(buffer, metadata_lines[selected], buffer_len);
    return buffer;
}

static int ow_page_action(int action, struct gui_synclist *lists)
{
    if (action == ACTION_STD_OK)
    {
        int sel = rb->gui_synclist_get_sel_pos(lists);
        int link = sel - (render.line_count - render.link_count);
        if (link >= 0 && link < render.link_count)
        {
            ow_open_path(render.links[link].target, true);
            return ACTION_STD_CANCEL;
        }
    }
    return action;
}

static void ow_show_metadata(const char *path)
{
    int page = ow_find_page_by_path(path);
    struct simplelist_info info;
    metadata_count = 0;
    if (page >= 0)
    {
        rb->snprintf(metadata_lines[metadata_count++], OW_LINE_LEN,
                     "Title: %s", pages[page].title);
        rb->snprintf(metadata_lines[metadata_count++], OW_LINE_LEN,
                     "URL: %s", pages[page].url);
        rb->snprintf(metadata_lines[metadata_count++], OW_LINE_LEN,
                     "Archived: %s", pages[page].archived);
        rb->snprintf(metadata_lines[metadata_count++], OW_LINE_LEN,
                     "Source: %s", pages[page].source);
        rb->snprintf(metadata_lines[metadata_count++], OW_LINE_LEN,
                     "Neighborhood: %s", pages[page].neighborhood);
        rb->snprintf(metadata_lines[metadata_count++], OW_LINE_LEN,
                     "Author: %s", pages[page].author);
    }
    else
    {
        rb->snprintf(metadata_lines[metadata_count++], OW_LINE_LEN,
                     "%s", path);
    }
    rb->simplelist_info_init(&info, "Site Metadata", metadata_count, NULL);
    info.get_name = ow_metadata_line;
    rb->simplelist_show_list(&info);
}

static int ow_page_menu(void)
{
    int selected = 0;
    MENUITEM_STRINGLIST(menu, "Offline Internet", NULL,
                        "Back", "Home", "Favorite", "Metadata", "Exit");

    selected = rb->do_menu(&menu, &selected, NULL, false);
    switch (selected)
    {
        case 0:
            if (back_count > 0)
                return ow_open_path(back_stack[--back_count], false);
            rb->splash(HZ, "No history");
            break;
        case 1:
            return ow_open_path(OW_INDEX, true);
        case 2:
            ow_log_path(OW_FAVORITES, current_path);
            rb->splash(HZ, "Saved favorite");
            break;
        case 3:
            ow_show_metadata(current_path);
            break;
        case 4:
            return PLUGIN_OK;
    }
    return PLUGIN_OK;
}

static int ow_show_page(const char *path)
{
    int ret;
    struct simplelist_info info;

    rb->strlcpy(current_path, path, sizeof(current_path));
    ow_log_path(OW_HISTORY, path);
    ow_render_html(path);

    rb->simplelist_info_init(&info, "Netscape Offline", render.line_count,
                             NULL);
    info.get_name = ow_page_line;
    info.action_callback = ow_page_action;
    info.scroll_all = true;
    ret = rb->simplelist_show_list(&info);
    if (ret)
        return PLUGIN_USB_CONNECTED;
    return ow_page_menu();
}

static int ow_open_path(const char *path, bool push_back)
{
    char plugin[MAX_PATH];
    int attr;

    if (push_back && current_path[0] && back_count < OW_STACK_LEN)
        rb->strlcpy(back_stack[back_count++], current_path, MAX_PATH);

    if (ow_has_ext(path, ".gif.png.jpg.jpeg.bmp"))
    {
        attr = rb->filetype_get_attr(path);
        if (rb->filetype_get_plugin(attr, plugin, sizeof(plugin)))
            return rb->plugin_open(plugin, path);
    }

    if (ow_has_ext(path, ".mid.midi.wav.mod.xm.s3m.it"))
    {
        attr = rb->filetype_get_attr(path);
        if (rb->filetype_get_plugin(attr, plugin, sizeof(plugin)))
            return rb->plugin_open(plugin, path);
    }

    if (!rb->file_exists(path))
    {
        rb->splashf(HZ * 2, "Missing local file: %s", path);
        return PLUGIN_OK;
    }

    return ow_show_page(path);
}

static const char *ow_page_name(int selected, void *data,
                                char *buffer, size_t buffer_len)
{
    (void)data;
    rb->snprintf(buffer, buffer_len, "%s", pages[list_indices[selected]].title);
    return buffer;
}

static int ow_choose_page(const char *title)
{
    struct simplelist_info info;
    if (list_count <= 0)
    {
        rb->splash(HZ, "No pages");
        return PLUGIN_OK;
    }
    rb->simplelist_info_init(&info, (char *)title, list_count, NULL);
    info.get_name = ow_page_name;
    info.scroll_all = true;
    if (rb->simplelist_show_list(&info))
        return PLUGIN_USB_CONNECTED;
    if (info.selection >= 0 && info.selection < list_count)
        return ow_open_path(pages[list_indices[info.selection]].path, true);
    return PLUGIN_OK;
}

static int ow_browse_source(const char *source)
{
    int i;
    list_count = 0;
    for (i = 0; i < page_count && list_count < OW_MAX_LIST; i++)
        if (ow_contains(pages[i].source, source))
            list_indices[list_count++] = i;
    return ow_choose_page(source);
}

static int ow_browse_neighborhood(void)
{
    int selected = 0;
    static const char *names[] = {
        "Area51", "Hollywood", "SiliconValley", "Tokyo", "Athens",
        "Heartland", "EnchantedForest", "RainForest"
    };
    MENUITEM_STRINGLIST(menu, "GeoCities", NULL,
                        "Area51", "Hollywood", "SiliconValley", "Tokyo",
                        "Athens", "Heartland", "EnchantedForest",
                        "RainForest");
    selected = rb->do_menu(&menu, &selected, NULL, false);
    if (selected >= 0)
    {
        int i;
        list_count = 0;
        for (i = 0; i < page_count && list_count < OW_MAX_LIST; i++)
            if (ow_contains(pages[i].neighborhood, names[selected]))
                list_indices[list_count++] = i;
        return ow_choose_page(names[selected]);
    }
    return PLUGIN_OK;
}

static int ow_search(void)
{
    char query[OW_QUERY_LEN];
    int i;
    query[0] = '\0';
    if (rb->kbd_input(query, sizeof(query), NULL) < 0)
        return PLUGIN_OK;

    list_count = 0;
    for (i = 0; i < page_count && list_count < OW_MAX_LIST; i++)
    {
        if (ow_contains(pages[i].title, query) ||
            ow_contains(pages[i].url, query) ||
            ow_contains(pages[i].keywords, query) ||
            ow_contains(pages[i].neighborhood, query) ||
            ow_contains(pages[i].author, query))
            list_indices[list_count++] = i;
    }
    rb->splashf(HZ, "%d pages", list_count);
    return ow_choose_page("RockSearch");
}

static int ow_random(void)
{
    if (page_count <= 0)
    {
        rb->splash(HZ, "No pages indexed");
        return PLUGIN_OK;
    }
    rb->srand(*rb->current_tick);
    return ow_open_path(pages[rb->rand() % page_count].path, true);
}

static int ow_recent_file(const char *title, const char *file)
{
    int fd;
    char line[512];
    list_count = 0;
    fd = rb->open(file, O_RDONLY);
    if (fd < 0)
    {
        rb->splash(HZ, "Empty");
        return PLUGIN_OK;
    }
    while (list_count < OW_MAX_LIST &&
           rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *p = line;
        int idx;
        ow_chomp(line);
        (void)ow_field(&p);
        (void)ow_field(&p);
        idx = ow_find_page_by_path(ow_field(&p));
        if (idx >= 0)
            list_indices[list_count++] = idx;
    }
    rb->close(fd);
    return ow_choose_page(title);
}

enum plugin_status plugin_start(const void *parameter)
{
    int selected = 0;
    (void)parameter;

    ow_mkdirs();
    ow_load_pages();

    while (true)
    {
        int ret = PLUGIN_OK;
        MENUITEM_STRINGLIST(menu, "INTERNET", NULL,
                            "Browse GeoCities", "Browse Angelfire",
                            "Browse Tripod", "Browse Yahoo", "Search",
                            "Random Site", "Favorites", "History",
                            "Recently Visited", "Home");

        selected = rb->do_menu(&menu, &selected, NULL, false);
        switch (selected)
        {
            case 0: ret = ow_browse_neighborhood(); break;
            case 1: ret = ow_browse_source("Angelfire"); break;
            case 2: ret = ow_browse_source("Tripod"); break;
            case 3: ret = ow_browse_source("Yahoo"); break;
            case 4: ret = ow_search(); break;
            case 5: ret = ow_random(); break;
            case 6: ret = ow_recent_file("Favorites", OW_FAVORITES); break;
            case 7:
            case 8: ret = ow_recent_file("Recently Visited", OW_HISTORY); break;
            case 9: ret = ow_open_path(OW_INDEX, true); break;
            default: return PLUGIN_OK;
        }
        if (ret == PLUGIN_USB_CONNECTED)
            return PLUGIN_USB_CONNECTED;
    }
}
