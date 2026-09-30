/* Shared logical-video state. Milliseconds survive codec/rendition changes.
 * The optional .media sidecar binds several renditions to one library item.
 * All writes happen on the UI thread, never an audio or video callback. */
#ifndef VIDEO_LIBRARY_STATE_H
#define VIDEO_LIBRARY_STATE_H
#include <stdint.h>
#include <stdbool.h>
#ifndef VLS_API
#define VLS_API
#endif
#define VLS_MAGIC 0x564c5332u
struct video_library_state
{
    uint32_t magic, identity, position_ms, duration_ms, watched;
};

static inline uint32_t video_library_identity(const char *path)
{
    char sidecar[MAX_PATH], line[40];
    VLS_API strlcpy(sidecar, path, sizeof(sidecar));
    char *dot = VLS_API strrchr(sidecar, '.');
    char *slash = VLS_API strrchr(sidecar, '/');
    if (dot && (!slash || dot > slash))
    {
        VLS_API strlcpy(dot, ".media", sizeof(sidecar) - (dot-sidecar));
        int fd = VLS_API open(sidecar, O_RDONLY);
        if (fd >= 0)
        {
            int size = VLS_API read(fd, line, sizeof(line)-1);
            VLS_API close(fd);
            if (size >= 35 && !VLS_API strncmp(line, "id=", 3))
            {
                bool valid = true;
                for (int i=3; i<35; i++)
                    if (!((line[i]>='0' && line[i]<='9') ||
                          (line[i]>='a' && line[i]<='f'))) valid = false;
                if (valid) return VLS_API crc_32(line+3, 32, 0xffffffff);
            }
        }
    }
    return VLS_API crc_32(path, VLS_API strlen(path), 0xffffffff);
}

static inline void video_library_filename(uint32_t identity, char *name, size_t size)
{
    VLS_API snprintf(name,size,PLUGIN_APPS_DATA_DIR "/video-state-%08lx.dat",
                     (unsigned long)identity);
}

static inline bool video_library_load(const char *path, struct video_library_state *state)
{
    char name[MAX_PATH];
    uint32_t identity = video_library_identity(path);
    video_library_filename(identity, name, sizeof(name));
    int fd = VLS_API open(name,O_RDONLY);
    if (fd < 0) return false;
    int count = VLS_API read(fd,state,sizeof(*state));
    VLS_API close(fd);
    return count == sizeof(*state) && state->magic == VLS_MAGIC &&
        state->identity == identity && state->duration_ms > 0 &&
        state->position_ms <= state->duration_ms;
}

static inline bool video_library_save(const char *path, uint32_t position,
                                      uint32_t duration, bool complete)
{
    if (!duration || duration == UINT32_MAX) return false;
    char name[MAX_PATH], temporary[MAX_PATH+4];
    struct video_library_state state = {VLS_MAGIC,video_library_identity(path),
        position < duration ? position : duration,duration,complete};
    video_library_filename(state.identity,name,sizeof(name));
    VLS_API snprintf(temporary,sizeof(temporary),"%s.tmp",name);
    VLS_API mkdir(PLUGIN_APPS_DATA_DIR);
    int fd = VLS_API open(temporary,O_WRONLY|O_CREAT|O_TRUNC,0666);
    if (fd < 0) return false;
    bool ok = VLS_API write(fd,&state,sizeof(state)) == sizeof(state);
    if (VLS_API close(fd) < 0) ok = false;
    if (ok) ok = VLS_API rename(temporary,name) == 0;
    if (!ok) VLS_API remove(temporary);
    return ok;
}
#endif
