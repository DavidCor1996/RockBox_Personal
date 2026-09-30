/* Shared Netflix preferences; callers supply NF_API for core/plugin I/O. */
#ifndef NETFLIX_PREFERENCES_H
#define NETFLIX_PREFERENCES_H
#define NF_PREFS_PATH ROCKBOX_DIR "/videolist/netflix-settings.bin"
#define NF_SUBTITLES 1u
static unsigned nf_preferences(void)
{
    uint32_t record[2];
    int fd = NF_API(open)(NF_PREFS_PATH, O_RDONLY);
    if (fd < 0) return 0;
    int n = NF_API(read)(fd, record, sizeof(record));
    NF_API(close)(fd);
    return n == sizeof(record) && record[0] == 0x3143464e ?
           record[1] & 127u : 0;
}
#endif
