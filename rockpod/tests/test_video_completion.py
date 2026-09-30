"""Exercise the actual C completion policy at credits and fallback boundaries."""
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[2]


def test_video_completion_boundaries(tmp_path):
    source = tmp_path / "completion.c"
    source.write_text(r'''
#include <assert.h>
#include "video_completion.h"
int main(void)
{
    /* Credits can begin well before the generic near-end threshold. */
    assert(!video_at_completion(1199, 1320, 1200));
    assert(video_at_completion(1200, 1320, 1200));
    assert(video_at_completion(1250, 1320, 1200));
    /* Keep the existing near-end completion even with a later marker. */
    assert(video_at_completion(950, 1000, 980));
    assert(video_at_completion(980, 1000, 980));
    assert(!video_at_completion(949, 1000, 0));
    assert(video_at_completion(950, 1000, 0));
    assert(video_at_completion(1000, 1000, 0));
    /* Invalid metadata falls back, unknown duration never completes. */
    assert(video_at_completion(950, 1000, 1000));
    assert(video_at_completion(950, 1000, 1100));
    assert(!video_at_completion(1000, 0, 100));
    assert(!video_at_completion(1000, UINT32_MAX, 100));
    assert(!video_at_completion(0, 1, 0));
    assert(!video_at_completion(949999999, 1000000000, 0));
    assert(video_at_completion(950000000, 1000000000, 0));
    return 0;
}
''')
    binary = tmp_path / "completion"
    subprocess.run(
        ["cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
         "-I", str(ROOT / "apps"), str(source), "-o", str(binary)],
        check=True,
    )
    subprocess.run([str(binary)], check=True)


def test_netflix_browser_reads_player_resume_records(tmp_path):
    root = (ROOT / "apps/root_menu.c").read_text()
    functions = root.split("static int video_netflix_mpeg_resume_time(", 1)[1]
    functions = "static int video_netflix_mpeg_resume_time(" + functions.split(
        "#if defined(HAVE_LCD_COLOR) && defined(HAVE_IPODJS_UI)", 1
    )[0]
    # Small host adapters let the real writer and browser reader share files.
    (tmp_path / "crc32.h").write_text(r'''
#include <stdint.h>
#include <stddef.h>
static uint32_t crc_32(const void *data, size_t len, uint32_t crc)
{
    const unsigned char *p = data;
    while (len--) crc = crc * 33u + *p++;
    return crc;
}
''')
    (tmp_path / "dir.h").write_text(
        '#include <sys/stat.h>\n#define mkdir(p) mkdir(p, 0777)\n'
    )
    (tmp_path / "file.h").write_text(
        '#include <fcntl.h>\n#include <unistd.h>\n'
    )
    (tmp_path / "rbpaths.h").write_text(
        '#define PLUGIN_APPS_DATA_DIR "."\n'
    )
    source = tmp_path / "resume.c"
    source.write_text(r'''
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <strings.h>
#define MAX_PATH 256
#include "video_resume.h"
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define IPOD_6G
#define VIDEO_LIST_RVP_RESUME VIDEO_RESUME_FILE
#define VIDEO_LIST_RVP_RESUME_MAGIC VIDEO_RESUME_MAGIC
#define VIDEO_LIST_MPEG_RESUME "./mpegplayer.cfg"
#define VIDEO_LIST_MPEG_TS_SECOND 45000
#define video_rvp_resume_record video_resume_record
struct video_entry {
    bool playable;
    const char *format;
    const char *path;
    unsigned duration_sec;
    unsigned resume_percent;
};
static int read_line(int fd, char *out, size_t size)
{
    size_t len = 0;
    char c;
    while (len + 1 < size && read(fd, &c, 1) == 1) {
        if (c == '\n') break;
        out[len++] = c;
    }
    out[len] = 0;
    return len;
}
static bool settings_parseline(char *line, char **name, char **value)
{
    char *colon = strchr(line, ':');
    if (!colon || *line == '#') return false;
    *colon = 0;
    *name = line;
    *value = colon + 1;
    return true;
}
''' + functions + r'''
int main(void)
{
    struct video_entry entry = {true, "MP4", "/Videos/episode.mp4", 1200, 0};
    const char *formats[] = {"MP4", "M4V", "MOV", "RVP"};
    assert(!video_netflix_resume_progress(&entry));
    video_resume_save(entry.path, 600, 1200);
    assert(video_resume_load(entry.path, 1200) == 600);
    for (unsigned i = 0; i < 4; i++) {
        entry.format = formats[i];
        assert(video_netflix_resume_progress(&entry));
        assert(entry.resume_percent == 50);
    }
    assert(video_resume_load(entry.path, 1300) == 0);
    video_resume_clear(entry.path);
    assert(!video_netflix_resume_progress(&entry));
    video_resume_save(entry.path, 0, 1200);
    assert(!video_netflix_resume_progress(&entry));
    video_resume_save(entry.path, 1140, 1200);
    assert(!video_netflix_resume_progress(&entry));
    entry.format = "MPG";
    entry.path = "/Videos/episode.mpg";
    FILE *file = fopen(VIDEO_LIST_MPEG_RESUME, "w");
    assert(file);
    fprintf(file, "# comment\ninvalid row\n%s: %u\n", entry.path,
            600 * VIDEO_LIST_MPEG_TS_SECOND);
    fclose(file);
    assert(video_netflix_resume_progress(&entry));
    assert(entry.resume_percent == 50);
    entry.path = "/Videos/unplayed.mpg";
    assert(!video_netflix_resume_progress(&entry));
    return 0;
}
''')
    binary = tmp_path / "resume"
    subprocess.run(
        ["cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
         "-I", str(tmp_path), "-I", str(ROOT / "apps"), str(source),
         "-o", str(binary)], check=True,
    )
    subprocess.run([str(binary)], cwd=tmp_path, check=True)


def test_mpeg_netflix_resume_long_episode(tmp_path):
    source = (ROOT / "apps/plugins/mpegplayer/mpeg_settings.c").read_text()
    body = source.split("int mpeg_start_menu(uint32_t duration)", 1)[1]
    body = body.split("    switch (settings.resume_options)", 1)[0]
    harness = tmp_path / "mpeg_resume.c"
    harness.write_text(r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#define TS_SECOND 45000
#define INVALID_TIMESTAMP UINT32_MAX
#define MPEG_START_RESTART 0
#define MPEG_START_SEEK 1
static struct { int resume_time; } settings;
static bool mpegplayer_netflix_launch = true;
static void mpeg_sysevent_clear(void) {}
static bool stream_can_seek(void) { return true; }
static int mpeg_start_menu(uint32_t duration)
''' + body + r'''
    (void)resume_threshold_low;
    return -1;
}
int main(void)
{
    /* 22-minute episodes overflow the old 32-bit duration * 95 expression. */
    settings.resume_time = 600 * TS_SECOND;
    assert(mpeg_start_menu(1320 * TS_SECOND) == MPEG_START_SEEK);
    assert(settings.resume_time == 600 * TS_SECOND);
    settings.resume_time = 1254 * TS_SECOND;
    assert(mpeg_start_menu(1320 * TS_SECOND) == MPEG_START_RESTART);
    settings.resume_time = 0;
    assert(mpeg_start_menu(1320 * TS_SECOND) == MPEG_START_RESTART);
    settings.resume_time = 3600 * TS_SECOND;
    assert(mpeg_start_menu(7200 * TS_SECOND) == MPEG_START_SEEK);
    return 0;
}
''')
    binary = tmp_path / "mpeg_resume"
    subprocess.run(
        ["cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
         str(harness), "-o", str(binary)], check=True,
    )
    subprocess.run([str(binary)], check=True)
