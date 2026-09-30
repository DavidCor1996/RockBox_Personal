/* Shared Photos collection discovery. GPL-2.0-or-later. */
#include "plugin.h"
#include "photo_library.h"
#define PHOTOS_DEFAULT_ROOT "/Photos"
#define PHOTOS_THUMB_DIR_SUFFIX ".photo_thumbs"
#define PHOTOS_PREVIEW_DIR_SUFFIX ".photo_previews"
#define PHOTOS_ROOT_SCAN_MAX_DIRS 64
#define PHOTOS_ROOT_SCAN_MAX_DEPTH 2
struct photo_root_scan_dir { char path[MAX_PATH]; int depth; };
bool photo_library_supported(const char *name)
{
    static const char *const exts[] = {
        ".bmp", ".gif", ".jpg", ".jpe", ".jpeg", ".png",
#ifdef HAVE_LCD_COLOR
        ".ppm",
#endif
    };
    const char *ext = rb->strrchr(name, '.');
    size_t i;

    if (!ext)
        return false;

    for (i = 0; i < ARRAYLEN(exts); i++)
    {
        if (!rb->strcasecmp(ext, exts[i]))
            return true;
    }

    return false;
}

static bool photos_root_has_sidecars(const char *root)
{
    char thumb_root[MAX_PATH];
    char preview_root[MAX_PATH];

    rb->snprintf(thumb_root, sizeof(thumb_root), "%s/%s",
                 root, PHOTOS_THUMB_DIR_SUFFIX);
    rb->snprintf(preview_root, sizeof(preview_root), "%s/%s",
                 root, PHOTOS_PREVIEW_DIR_SUFFIX);

    return rb->dir_exists(thumb_root) || rb->dir_exists(preview_root);
}

static int photos_count_preview_files(const char *path, int depth)
{
    DIR *dir;
    struct dirent *entry;
    struct dirinfo info;
    int count = 0;
    char child[MAX_PATH];
    const char *ext;

    if (!path || !path[0] || depth > 6)
        return 0;

    dir = rb->opendir(path);
    if (!dir)
        return 0;

    while ((entry = rb->readdir(dir)) != NULL)
    {
        if (!rb->strcmp(entry->d_name, ".") || !rb->strcmp(entry->d_name, ".."))
            continue;

        rb->snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        info = rb->dir_get_info(dir, entry);
        if (info.attribute & ATTR_DIRECTORY)
        {
            count += photos_count_preview_files(child, depth + 1);
            continue;
        }

        ext = rb->strrchr(entry->d_name, '.');
        if (ext && !rb->strcasecmp(ext, ".bmp"))
            count++;
    }

    rb->closedir(dir);
    return count;
}

static void photos_scan_better_root(bool *found_root, char *best_root,
                                    size_t best_root_size, int *best_count,
                                    void *scratch, size_t buffer_size)
{
    struct photo_root_scan_dir *scan_dirs;
    int scan_capacity;
    int scan_head = 0;
    int scan_count = 1;
    DIR *dir;
    struct dirent *entry;
    struct dirinfo info;
    char candidate[MAX_PATH];
    char preview_root[MAX_PATH];
    int count;

    scan_dirs = scratch;
    if (!scan_dirs)
        return;

    scan_capacity = (int)(buffer_size / sizeof(*scan_dirs));
    if (scan_capacity > PHOTOS_ROOT_SCAN_MAX_DIRS)
        scan_capacity = PHOTOS_ROOT_SCAN_MAX_DIRS;
    if (scan_capacity < 1)
        return;

    rb->strlcpy(scan_dirs[0].path, "/", sizeof(scan_dirs[0].path));
    scan_dirs[0].depth = 0;

    while (scan_head < scan_count)
    {
        const char *base = scan_dirs[scan_head].path;
        int depth = scan_dirs[scan_head].depth;
        bool is_root = (base[0] == '/' && base[1] == '\0');

        dir = rb->opendir(base);
        if (!dir)
        {
            scan_head++;
            continue;
        }

        while ((entry = rb->readdir(dir)) != NULL)
        {
            if (entry->d_name[0] == '.')
                continue;

            if (is_root)
                rb->snprintf(candidate, sizeof(candidate), "/%s",
                             entry->d_name);
            else
                rb->snprintf(candidate, sizeof(candidate), "%s/%s", base,
                             entry->d_name);

            info = rb->dir_get_info(dir, entry);
            if (!(info.attribute & ATTR_DIRECTORY))
                continue;

            if (photos_root_has_sidecars(candidate))
            {
                rb->snprintf(preview_root, sizeof(preview_root), "%s/%s",
                             candidate, PHOTOS_PREVIEW_DIR_SUFFIX);
                count = photos_count_preview_files(preview_root, 0);
                if (!*found_root || count > *best_count)
                {
                    rb->strlcpy(best_root, candidate, best_root_size);
                    *best_count = count;
                    *found_root = true;
                }
            }

            if (depth + 1 < PHOTOS_ROOT_SCAN_MAX_DEPTH &&
                scan_count < scan_capacity)
            {
                rb->strlcpy(scan_dirs[scan_count].path, candidate,
                            sizeof(scan_dirs[scan_count].path));
                scan_dirs[scan_count].depth = depth + 1;
                scan_count++;
            }
        }

        rb->closedir(dir);
        scan_head++;
    }
}

bool photo_library_root(char *photos_root, size_t root_size,
                        void *scratch, size_t scratch_size)
{
    bool found_best_root = false;
    int best_count = 0;
    char best_root[MAX_PATH];

    /* The normal RockPod layout is authoritative.  Avoid surveying the
     * volume at every launch when /Photos is already present. */
    if (rb->dir_exists(PHOTOS_DEFAULT_ROOT))
    {
        rb->strlcpy(photos_root, PHOTOS_DEFAULT_ROOT, root_size);
        return true;
    }

    photos_scan_better_root(&found_best_root, best_root, sizeof(best_root),
                            &best_count, scratch, scratch_size);

    if (found_best_root)
    {
        rb->strlcpy(photos_root, best_root, root_size);
        return true;
    }

    return false;
}

