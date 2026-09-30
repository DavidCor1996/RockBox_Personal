/***************************************************************************
 * MSN Messenger: synced contacts and locally authored offline conversations.
 * Original Microsoft MSN 7.5 resources are loaded from ipodjs/msn.
 * Fixed workspace; no playback buffer, core allocation or playlist ownership.
 ****************************************************************************/
#include "plugin.h"
#include "lib/pluginlib_actions.h"
#include "lib/video_player.h"
#include "msn_font.h"
#include "msn_emoticons.h"
#include "../msn_clock.h"

#define MSN_ROOT ROCKBOX_DIR "/msn"
#define MSN_ART ROCKBOX_DIR "/ipodjs/msn"
#define MSN_CONTACTS 64
#define MSN_EVENTS 1024
#define MSN_LINE 1024
#define MSN_SOUND_BYTES 400000
#define MSN_BLUE LCD_RGBPACK(35, 68, 110)
#define MSN_INK LCD_RGBPACK(35, 38, 45)
#define MSN_GRAY LCD_RGBPACK(100, 110, 125)
#define MSN_WHITE LCD_RGBPACK(255, 255, 255)
#define MSN_SELECT LCD_RGBPACK(221, 235, 253)
#define MSN_FOCUS LCD_RGBPACK(49, 106, 197)
#define MSN_BITMAP_BYTES(w,h) ((w)*(h)*sizeof(fb_data)+(w)*4+16)

enum msn_kind { MSN_MESSAGE, MSN_PHOTO, MSN_GIF, MSN_VIDEO,
                MSN_NUDGE, MSN_STATUS, MSN_REPLY, MSN_WINK };
struct msn_contact
{
    char id[33], name[128], email[96], personal[256], avatar[MAX_PATH];
    int status;
};
struct msn_event
{
    char id[33], text[512], media[MAX_PATH];
    long at;
    short contact;
    unsigned char kind, typing;
    bool read, persisted, outgoing, played;
};
static struct msn_contact contacts[MSN_CONTACTS];
static struct msn_event events[MSN_EVENTS];
static int contact_count, event_count, selected, active = -1, cursor;
static int visible[MSN_EVENTS], visible_count, text_height, font_id;
static char bundle[MAX_PATH], line[MSN_LINE], path[MAX_PATH];
static char reply[512];
static long now, shake_until, opened_tick, last_service;
static bool sound_on = true, sound_owned, redraw = true;
static int pending_sound = -1;
static int shake_x;
static bool toolbar_focus, suppress_reaction;
static unsigned wink_samples,wink_position,wink_phase,wink_rate;
static int16_t wink_output[1024];
static char choice_text[5][512],choice_response[5][512],choice_id[5][33];
static char wink_ids[32][33],wink_names[32][48];
static int wink_count;
static int wink_play(struct msn_event *e);
static int toolbar_selected;
static long last_animation, last_input;
/* One visible image card; fixed pixels plus BMP scaler workspace. */
static unsigned char chat_image_data[MSN_BITMAP_BYTES(128,80)+8192];
static struct bitmap chat_image;
static int chat_image_event=-2;
static struct msn_event history_page[64];
static int history_count;
static unsigned char sound_data[MSN_SOUND_BYTES] CACHEALIGN_ATTR;
static unsigned char header_data[MSN_BITMAP_BYTES(320,70)];
static unsigned char toolbar_data[MSN_BITMAP_BYTES(320,28)];
static unsigned char avatar_data[MSN_BITMAP_BYTES(48,48)];
static unsigned char self_data[MSN_BITMAP_BYTES(48,48)];
static unsigned char logo_data[MSN_BITMAP_BYTES(46,16)];
static unsigned char icon_data[11][MSN_BITMAP_BYTES(20,28)];
static unsigned char emote_data[MSN_BITMAP_BYTES(MSN_EMOTE_ATLAS_W,MSN_EMOTE_ATLAS_H)];
/* Mutually exclusive sign-in atlas, attachment bitmap, and streamed GIF frame. */
static union {
    unsigned char image[MSN_BITMAP_BYTES(75,2001)];
    fb_data frame[160*120];
} media;
static struct bitmap header_bm, toolbar_bm, avatar_bm, self_bm, logo_bm;
static struct bitmap icons[11], emote_bm, media_bm;
static int signin_delay[29];
#if CONFIG_KEYPAD == IPOD_4G_PAD || CONFIG_KEYPAD == IPOD_3G_PAD
static const struct button_mapping msn_ipod_ctx[] =
{
    { PLA_SCROLL_BACK, BUTTON_SCROLL_BACK, BUTTON_NONE },
    { PLA_SCROLL_FWD, BUTTON_SCROLL_FWD, BUTTON_NONE },
    { PLA_SCROLL_BACK_REPEAT, BUTTON_SCROLL_BACK|BUTTON_REPEAT, BUTTON_NONE },
    { PLA_SCROLL_FWD_REPEAT, BUTTON_SCROLL_FWD|BUTTON_REPEAT, BUTTON_NONE },
    { PLA_CANCEL, BUTTON_MENU|BUTTON_REL, BUTTON_MENU },
    { PLA_SELECT_REL, BUTTON_SELECT|BUTTON_REL, BUTTON_SELECT },
    { PLA_SELECT_REPEAT, BUTTON_SELECT|BUTTON_REPEAT, BUTTON_SELECT },
    { PLA_RIGHT, BUTTON_RIGHT|BUTTON_REL, BUTTON_RIGHT },
    { PLA_LEFT, BUTTON_LEFT|BUTTON_REL, BUTTON_LEFT },
    { PLA_DOWN, BUTTON_PLAY|BUTTON_REL, BUTTON_PLAY },
    LAST_ITEM_IN_LIST
};
static const struct button_mapping *contexts[] = { msn_ipod_ctx };
#else
static const struct button_mapping *contexts[] = { pla_main_ctx };
#endif

static int fields(char *text, char **out, int count)
{
    int n = 0;
    out[n++] = text;
    while (*text && n < count)
    {
        if (*text == '\t')
        {
            *text = 0;
            out[n++] = text + 1;
        }
        text++;
    }
    return n;
}

static bool safe_relative(const char *name)
{
    return name[0] && name[0] != '/' && !rb->strstr(name, "..") &&
           !rb->strchr(name, '\\') && !rb->strchr(name, ':');
}

static bool safe_media(const char *name)
{
    return safe_relative(name) ||
        ((!rb->strncmp(name,MSN_ROOT "/bundles/",sizeof(MSN_ROOT "/bundles/")-1) ||
          !rb->strncmp(name,MSN_ART "/gifs/",sizeof(MSN_ART "/gifs/")-1) ||
          !rb->strncmp(name,MSN_ART "/winks/",sizeof(MSN_ART "/winks/")-1)) &&
         !rb->strstr(name,"..") && !rb->strchr(name,'\\'));
}

static void media_path(char *out, size_t size, const char *name)
{
    if (name[0] == '/') rb->strlcpy(out,name,size);
    else rb->snprintf(out,size,"%s/%s",bundle,name);
}

static bool bitmap_load(const char *filename, struct bitmap *bm,
                        void *pixels, size_t capacity, int maxw, int maxh)
{
    bm->data = pixels;
    int result = rb->read_bmp_file(filename, bm, capacity,
                                  FORMAT_NATIVE | FORMAT_DITHER | FORMAT_TRANSPARENT, NULL);
    if (result <= 0 || bm->width > maxw || bm->height > maxh)
    {
        bm->width = 0;
        return false;
    }
    return true;
}

static bool bitmap_fit(const char *filename, struct bitmap *bm,
                       void *pixels, size_t capacity, int width, int height)
{
    bm->data=pixels;bm->width=width;bm->height=height;
    int result=rb->read_bmp_file(filename,bm,capacity,
        FORMAT_NATIVE|FORMAT_RESIZE|FORMAT_KEEP_ASPECT|FORMAT_DITHER,NULL);
    if (result<=0 || bm->width>width || bm->height>height)
    { bm->width=0;return false; }
    return true;
}

static bool is_picture(const struct msn_event *e)
{
    return e->kind==MSN_PHOTO || e->kind==MSN_GIF;
}

static bool picture_preview(const struct msn_event *e, char *out, size_t size)
{
    char origin[MAX_PATH];
    media_path(origin,sizeof(origin),e->media);
    char *tail=rb->strstr(origin,"/media/");
    if (!tail) return false;
    *tail=0;
    rb->snprintf(out,size,"%s/saved/%s.preview.bmp",origin,e->id);
    return true;
}

static void art_load(const char *name, struct bitmap *bm, void *pixels,
                     size_t capacity, int maxw, int maxh)
{
    rb->snprintf(path, sizeof(path), MSN_ART "/%s.bmp", name);
    bitmap_load(path, bm, pixels, capacity, maxw, maxh);
}

static void art_draw(const struct bitmap *bm, int x, int y)
{
    if (bm->width > 0)
        rb->lcd_bitmap_transparent((fb_data *)bm->data, x + shake_x, y, bm->width, bm->height);
}

static unsigned glyph(ucschar_t ch)
{
    if (ch == 0x2022) ch = '-';
    if (ch == 0x2019 || ch == 0x2018) ch = '\'';
    if (ch == 0x201c || ch == 0x201d) ch = '"';
    return (ch >= 32 && ch <= 255 ? ch : '?') - 32;
}

static int emote_match(const char *p, int *bytes)
{
    for (int i=0; i<MSN_EMOTE_COUNT; i++)
    {
        int n=rb->strlen(msn_emotes[i].shortcut);
        if (n && !rb->strncmp(p,msn_emotes[i].shortcut,n))
        { *bytes=n; return i; }
    }
    return -1;
}

static void emote_draw(int index, int x, int y)
{
    if (!emote_bm.width || index<0 || index>=MSN_EMOTE_COUNT) return;
    const struct msn_emote *e=&msn_emotes[index];
    int elapsed=((*rb->current_tick-opened_tick)*1000/HZ)%MAX(1,e->duration);
    int frame=0;
    while (frame+1<e->frames && elapsed>=msn_emote_delays[e->first+frame])
        elapsed-=msn_emote_delays[e->first+frame++];
    int cell=e->first+frame;
    rb->lcd_bitmap_transparent_part((fb_data *)emote_bm.data,(cell%16)*19,
        (cell/16)*19,MSN_EMOTE_ATLAS_W,x+shake_x,y,19,19);
}

/* Keep Unicode clusters together for the core's resident 4009-glyph emoji
 * atlas. ASCII MSN shortcuts use the original Messenger animation atlas. */
static int text_token(const char *p, char *token, int *width, int *emote)
{
    int bytes=0;
    *emote=emote_match(p,&bytes);
    if (*emote>=0) { *width=20; return bytes; }
    ucschar_t ch;
    const unsigned char *end=rb->utf8decode((const unsigned char *)p,&ch);
    bool joined=false;
    if (ch>255)
    {
        while (*end && end-(const unsigned char *)p<48)
        {
            ucschar_t next;
            const unsigned char *q=rb->utf8decode(end,&next);
            if (next==0xfe0f || next==0xfe0e || next==0x200d ||
                (next>=0x1f3fb && next<=0x1f3ff) || joined ||
                (ch>=0x1f1e6 && ch<=0x1f1ff && next>=0x1f1e6 && next<=0x1f1ff))
            { end=q; joined=next==0x200d; }
            else break;
        }
    }
    bytes=end-(const unsigned char *)p;
    rb->memcpy(token,p,bytes); token[bytes]=0;
    if (ch>255) rb->lcd_getstringsize(token,width,NULL);
    else *width=_sysfont_width[glyph(ch)];
    return bytes;
}

static void text(int x, int y, const char *value, unsigned color)
{
    rb->lcd_set_foreground(color);
    rb->lcd_set_drawmode(DRMODE_FG);
    while (*value)
    {
        char token[64]; int width,emote;
        int bytes=text_token(value,token,&width,&emote);
        if (x+width>LCD_WIDTH-3) break;
        if (emote>=0) emote_draw(emote,x,y-2);
        else if ((unsigned char)*value>=0xc4)
            rb->lcd_putsxy(x+shake_x,y,token);
        else
        {
            ucschar_t ch; rb->utf8decode((const unsigned char *)value,&ch);
            unsigned g=glyph(ch);
            rb->lcd_mono_bitmap(_font_bits+_sysfont_offset[g],x+shake_x,y,width,12);
        }
        x+=width; value+=bytes;
    }
    rb->lcd_set_drawmode(DRMODE_SOLID);
}

static void fill(int x, int y, int w, int h, unsigned color)
{
    rb->lcd_set_foreground(color);
    rb->lcd_fillrect(x + shake_x, y, w, h);
}

static void focus_box(int x, int y, int w, int h)
{
    fill(x,y,w,h,MSN_SELECT);
    rb->lcd_set_foreground(MSN_FOCUS);
    rb->lcd_drawrect(x+shake_x,y,w,h);
    rb->lcd_drawrect(x+shake_x+1,y+1,w-2,h-2);
}

static int wrapped(const char *value, int x, int y, int width,
                   int max_lines, unsigned color)
{
    char part[160];
    int used = 0;
    while (*value && used < max_lines)
    {
        int length = 0, last_space = 0, w = 0;
        while (value[length] && length < (int)sizeof(part) - 5)
        {
            char token[64]; int tw,emote;
            int bytes=text_token(value+length,token,&tw,&emote);
            if (length+bytes >= (int)sizeof(part) || w+tw>width) break;
            unsigned char ch=value[length];
            rb->memcpy(part+length,value+length,bytes); length+=bytes; w+=tw;
            if (ch == ' ') last_space = length;
        }
        if (!length) break;
        if (value[length] && last_space > 0) length = last_space;
        rb->memcpy(part, value, length);
        part[length] = 0;
        text(x, y + used * text_height, part, color);
        value += length;
        while (*value == ' ') value++;
        used++;
    }
    return used;
}

static long rtc_now(void)
{
    struct tm *tm = rb->get_time();
    if (!tm || tm->tm_year < 120) return 0;
    return msn_local_timestamp(tm);
}

static int contact_index(const char *id)
{
    for (int i = 0; i < contact_count; i++)
        if (!rb->strcmp(contacts[i].id, id)) return i;
    return -1;
}

static int kind_index(const char *kind)
{
    static const char *names[] = { "message", "photo", "gif", "video",
                                  "nudge", "status", "reply", "wink" };
    for (int i = 0; i < 8; i++)
        if (!rb->strcmp(kind, names[i])) return i;
    return -1;
}

static bool decode_event(char *row, struct msn_event *e, bool local)
{
    char *f[8];
    if (fields(row,f,local ? 8 : 7)!=(local ? 8 : 7)) return false;
    int c=contact_index(f[2]),kind=kind_index(f[3]);
    const char *media_name=f[local ? 7 : 6];
    if (c<0 || kind<0 || (*media_name && !safe_media(media_name))) return false;
    rb->memset(e,0,sizeof(*e));
    rb->strlcpy(e->id,f[0],sizeof(e->id)); e->at=rb->atoi(f[1]);
    e->contact=c; e->kind=kind; e->typing=MIN(10,MAX(0,rb->atoi(f[4])));
    e->outgoing=local && rb->atoi(f[5]);
    rb->strlcpy(e->text,f[local ? 6 : 5],sizeof(e->text));
    rb->strlcpy(e->media,media_name,sizeof(e->media));
    e->read=e->outgoing;
    return true;
}

static bool event_exists(const char *id)
{
    for (int i=0;i<event_count;i++)
        if (!rb->strcmp(id,events[i].id)) return true;
    return false;
}

static void load_recent_archive(void)
{
    int fd=rb->open(MSN_ROOT "/archive.tsv",O_RDONLY),n=0;
    if (fd<0) return;
    while (rb->read_line(fd,line,sizeof(line))>0)
    {
        struct msn_event candidate;
        if (decode_event(line,&candidate,false) && candidate.at<=rtc_now() && !event_exists(candidate.id))
            history_page[n++%64]=candidate;
    }
    rb->close(fd);
    for (int i=MAX(0,n-64);i<n && event_count<MSN_EVENTS;i++)
        events[event_count++]=history_page[i%64];
}

/* Journals keep all messages on disk. The active window may discard its
 * oldest delivered entry, never a pending delivery, to accept new replies. */
static bool reserve_event(void)
{
    if (event_count<MSN_EVENTS) return true;
    int oldest=-1;long clock=rtc_now();
    for (int i=0;i<event_count;i++)
        if (events[i].at<=clock && (oldest<0 || events[i].at<events[oldest].at)) oldest=i;
    if (oldest<0) return false;
    rb->memmove(events+oldest,events+oldest+1,(event_count-oldest-1)*sizeof(*events));
    event_count--;chat_image_event=-2;return true;
}
static void load_local_events(void)
{
    int fd=rb->open(MSN_ROOT "/local.tsv",O_RDONLY);
    if (fd<0) return;
    struct msn_event candidate;
    while (rb->read_line(fd,line,sizeof(line))>0)
        if (decode_event(line,&candidate,true) && !event_exists(candidate.id) && reserve_event())
            events[event_count++]=candidate;
    rb->close(fd);
}

static bool load_bundle(void)
{
    char generation[32];
    int fd = rb->open(MSN_ROOT "/current.txt", O_RDONLY);
    if (fd < 0) return false;
    int got = rb->read_line(fd, generation, sizeof(generation));
    rb->close(fd);
    if (got <= 0 || rb->strlen(generation) != 16) return false;
    for (int i = 0; i < 16; i++)
        if (!((generation[i] >= '0' && generation[i] <= '9') ||
              (generation[i] >= 'a' && generation[i] <= 'f'))) return false;
    rb->snprintf(bundle, sizeof(bundle), MSN_ROOT "/bundles/%s", generation);
    rb->snprintf(path, sizeof(path), "%s/contacts.tsv", bundle);
    fd = rb->open(path, O_RDONLY);
    if (fd < 0) return false;
    rb->read_line(fd, line, sizeof(line));
    while (contact_count < MSN_CONTACTS && rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *f[5];
        if (fields(line, f, 5) != 5 || !safe_media(f[4])) continue;
        struct msn_contact *c = &contacts[contact_count++];
        rb->strlcpy(c->id, f[0], sizeof(c->id));
        rb->strlcpy(c->name, f[1], sizeof(c->name));
        rb->strlcpy(c->email, f[2], sizeof(c->email));
        rb->strlcpy(c->personal, f[3], sizeof(c->personal));
        rb->strlcpy(c->avatar, f[4], sizeof(c->avatar));
        c->status = 3;
    }
    rb->close(fd);
    fd=rb->open(MSN_ROOT "/archive-contacts.tsv",O_RDONLY);
    if (fd>=0)
    {
        rb->read_line(fd,line,sizeof(line));
        while (contact_count<MSN_CONTACTS && rb->read_line(fd,line,sizeof(line))>0)
        {
            char *f[5];
            if (fields(line,f,5)!=5 || contact_index(f[0])>=0 || !safe_media(f[4])) continue;
            struct msn_contact *c=&contacts[contact_count++];
            rb->strlcpy(c->id,f[0],sizeof(c->id)); rb->strlcpy(c->name,f[1],sizeof(c->name));
            rb->strlcpy(c->email,f[2],sizeof(c->email)); rb->strlcpy(c->personal,f[3],sizeof(c->personal));
            rb->strlcpy(c->avatar,f[4],sizeof(c->avatar)); c->status=3;
        }
        rb->close(fd);
    }
    rb->snprintf(path, sizeof(path), "%s/events.tsv", bundle);
    fd = rb->open(path, O_RDONLY);
    if (fd >= 0)
    {
        rb->read_line(fd, line, sizeof(line));
        while (event_count < MSN_EVENTS && rb->read_line(fd, line, sizeof(line)) > 0)
        {
            char *f[7];
            if (fields(line, f, 7) != 7) continue;
            int c = contact_index(f[2]), kind = kind_index(f[3]);
            if (c < 0 || kind < 0 || (f[6][0] && !safe_media(f[6]))) continue;
            struct msn_event *e = &events[event_count++];
            rb->strlcpy(e->id, f[0], sizeof(e->id));
            e->at = rb->atoi(f[1]); e->contact = c; e->kind = kind;
            e->typing = MIN(10, MAX(0, rb->atoi(f[4])));
            rb->strlcpy(e->text, f[5], sizeof(e->text));
            rb->strlcpy(e->media, f[6], sizeof(e->media));
        }
        rb->close(fd);
    }
    return contact_count > 0;
}

static void read_state(void)
{
    int fd = rb->open(MSN_ROOT "/read.tsv", O_RDONLY);
    if (fd >= 0)
    {
        while (rb->read_line(fd, line, sizeof(line)) > 0)
            for (int i = 0; i < event_count; i++)
                if (!rb->strcmp(line, events[i].id)) events[i].read = events[i].persisted = true;
        rb->close(fd);
    }
    fd = rb->open(MSN_ROOT "/replies.tsv", O_RDONLY);
    if (fd >= 0)
    {
        while (event_count < MSN_EVENTS && rb->read_line(fd, line, sizeof(line)) > 0)
        {
            char *f[3];
            if (fields(line, f, 3) != 3) continue;
            int c = contact_index(f[1]);
            if (c < 0) continue;
            struct msn_event *e = &events[event_count++];
            e->at = rb->atoi(f[0]); e->contact = c;
            e->kind = MSN_REPLY; e->read = true; e->outgoing = true;
            rb->strlcpy(e->text, f[2], sizeof(e->text));
        }
        rb->close(fd);
    }
    fd = rb->open(MSN_ROOT "/selection.txt", O_RDONLY);
    if (fd >= 0)
    {
        if (rb->read_line(fd, line, sizeof(line)) > 0)
            selected = MAX(0, contact_index(line));
        rb->close(fd);
    }
}

static void save_read(void)
{
    int fd = rb->open(MSN_ROOT "/read.tsv", O_WRONLY|O_CREAT|O_APPEND, 0666);
    if (fd < 0) return;
    bool ok = true;
    for (int i = 0; i < event_count && ok; i++)
        if (events[i].read && !events[i].persisted && events[i].id[0])
        {
            int n = rb->snprintf(line, sizeof(line), "%s\n", events[i].id);
            ok = rb->write(fd, line, n) == n;
            if (ok) events[i].persisted = true;
        }
    rb->close(fd);

}

static void sound_stop(void)
{
#ifndef HAVE_HARDWARE_BEEP
    if (sound_owned)
    {
        rb->mixer_channel_stop(PCM_MIXER_CHAN_BEEP);
        rb->mixer_channel_set_buffer_hook(PCM_MIXER_CHAN_BEEP, NULL);
        rb->mixer_channel_set_amplitude(PCM_MIXER_CHAN_BEEP, MIX_AMP_UNITY);
        sound_owned = false;
    }
#endif
}

static void sound_play(int sound)
{
#ifndef HAVE_HARDWARE_BEEP
    static const char *names[] = { "message", "nudge", "online" };
    unsigned rate = rb->mixer_get_frequency();
    if (!sound_on || sound < 0 || sound > 2 || (rate != 44100 && rate != 48000)) return;
    sound_stop();
    rb->snprintf(path, sizeof(path), MSN_ART "/%s-%u.pcm", names[sound], rate);
    int fd = rb->open(path, O_RDONLY);
    if (fd < 0) return;
    off_t length = rb->filesize(fd);
    if (length <= 0 || length > MSN_SOUND_BYTES || length % 4)
    {
        rb->close(fd);
        return;
    }
    int got = rb->read(fd, sound_data, length);
    rb->close(fd);
    if (got != length) return;
    rb->mixer_channel_set_amplitude(PCM_MIXER_CHAN_BEEP, MIX_AMP_UNITY/2);
    sound_owned = true;
    rb->mixer_channel_play_data(PCM_MIXER_CHAN_BEEP, NULL, sound_data, got);
#else
    (void)sound;
#endif
}

static int event_compare(const void *a, const void *b)
{
    const struct msn_event *ea = &events[*(const int *)a];
    const struct msn_event *eb = &events[*(const int *)b];
    return ea->at < eb->at ? -1 : ea->at > eb->at ? 1 : 0;
}

static void filter(void)
{
    visible_count = 0;
    for (int i = 0; i < event_count; i++)
        if (events[i].contact == active && events[i].at <= now)
            visible[visible_count++] = i;
    rb->qsort(visible, visible_count, sizeof(visible[0]), event_compare);
    cursor = MIN(cursor, MAX(0, visible_count-1));
}

static void service(void)
{
    long previous = now;
    now = rtc_now();
    if (!now) return;
    bool changed = false;
    for (int c = 0; c < contact_count; c++) contacts[c].status = 3;
    for (int i = 0; i < event_count; i++)
    {
        struct msn_event *e = &events[i];
        if (e->at <= now)
        {
            if (e->kind == MSN_STATUS)
            {
                static const char *status[] = { "Online", "Away", "Busy", "Offline" };
                for (int s = 0; s < 4; s++)
                    if (!rb->strcmp(e->text, status[s])) contacts[e->contact].status = s;
            }
            else if (now - e->at < 30*60 && !e->outgoing)
                contacts[e->contact].status = 0;
            if (previous && e->at > previous && !e->outgoing)
            {
                changed = true;
                pending_sound = e->kind == MSN_NUDGE ? 1 : e->kind == MSN_STATUS ? 2 : 0;
                if (e->kind == MSN_NUDGE && e->contact == active)
                    shake_until = *rb->current_tick + HZ;
            }
        }
        else if (e->at - now <= e->typing && e->kind == MSN_MESSAGE)
            contacts[e->contact].status = 0;
    }
    if (changed)
    {
        filter();
        cursor = MAX(0, visible_count-1);
        redraw = true;
    }
    if (active >= 0)
    {
        bool save = false;
        for (int i = 0; i < event_count; i++)
            if (events[i].contact == active && events[i].at <= now && !events[i].read)
            {
                events[i].read = true;
                save = true;
            }
        if (save) save_read();
    }
}

static void prepare_contact(void)
{
    int index = active >= 0 ? active : selected;
    media_path(path,sizeof(path),contacts[index].avatar);
    bitmap_load(path, &avatar_bm, avatar_data, sizeof(avatar_data), 48, 48);
}

static void chrome(const char *title, const char *footer)
{
    rb->lcd_set_viewport(NULL);
    rb->lcd_setfont(font_id);
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_background(MSN_WHITE);
    rb->lcd_clear_display();
    rb->lcd_set_foreground(MSN_WHITE);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT);
    art_draw(&header_bm, 0, 0);
    art_draw(&logo_bm, LCD_WIDTH-52, 4);
    text(8, 5, title, MSN_BLUE);
    art_draw(&toolbar_bm, 0, LCD_HEIGHT-28);
    text(6, LCD_HEIGHT-20, footer, MSN_BLUE);
}

static void draw_contacts(void)
{
    chrome("MSN Messenger", "SELECT Chat                 MENU Exit");
    art_draw(&self_bm, 8, 22);
    text(64, 27, "My Messenger", MSN_BLUE);
    text(64, 46, now ? "Available" : "Set the iPod date and time", MSN_GRAY);
    int first = MAX(0, selected-3);
    for (int row = 0; row < 4 && first+row < contact_count; row++)
    {
        int c = first+row, y = 74+row*33, unread = 0;
        if (c == selected) fill(2,y-1,LCD_WIDTH-4,32,MSN_FOCUS);
        art_draw(&icons[contacts[c].status], 8, y);
        wrapped(contacts[c].name,35,y,240,1,c==selected ? MSN_WHITE : MSN_INK);
        for (int i = 0; i < event_count; i++)
            if (events[i].contact == c && events[i].at <= now && !events[i].read) unread++;
        if (unread)
        {
            char count[12]; rb->snprintf(count,sizeof(count),"(%d)",unread);
            text(278,y,count,c==selected ? MSN_WHITE : MSN_BLUE);
        }
        wrapped(contacts[c].personal,35,y+text_height,270,1,c==selected ? MSN_WHITE : MSN_GRAY);
    }
}

static void draw_action_bar(void)
{
    static const char *labels[]={"Reply","Emoticons","Send a nudge","Send a Wink","Animated emoticons","Conversation history","Messenger sounds"};
    static const int artwork[]={7,4,5,8,4,10,9};
    art_draw(&toolbar_bm,0,185);
    for (int i=0;i<7;i++)
    {
        int x=6+i*44;
        if (i==toolbar_selected && toolbar_focus) focus_box(x,186,42,26);
        art_draw(&icons[artwork[i]],x+10,189);
    }
    fill(0,214,320,26,MSN_WHITE);
    if (toolbar_focus)
    {
        char label[80];
        rb->snprintf(label,sizeof(label),"SELECT  %s",labels[toolbar_selected]);
        text(8,219,label,MSN_BLUE);
    }
    else if (visible_count && is_picture(&events[visible[cursor]]))
        text(8,219,"SELECT Enlarge   HOLD Reply   PLAY Tools",MSN_BLUE);
    else text(8,219,"PLAY Tools   HOLD Reply   SELECT Open",MSN_GRAY);
}

/* One thumbnail load after input settles; draw_chat only uses cached pixels. */
static void prepare_chat_image(void)
{
    if (active<0 || !visible_count) return;
    int index=visible[cursor];
    if (chat_image_event==index) return;
    chat_image_event=index;chat_image.width=0;
    struct msn_event *e=&events[index];
    if (is_picture(e))
    {
        char preview[MAX_PATH];
        bool loaded=picture_preview(e,preview,sizeof(preview)) &&
            bitmap_fit(preview,&chat_image,chat_image_data,
                       sizeof(chat_image_data),128,80);
        if (!loaded && e->kind==MSN_PHOTO)
        {
            media_path(preview,sizeof(preview),e->media);
            bitmap_fit(preview,&chat_image,chat_image_data,
                       sizeof(chat_image_data),128,80);
        }
        else if (!loaded && e->kind==MSN_GIF)
        {
            /* Original sent emoticons have no photo sidecar. Read only their
             * first frame, using the existing attachment workspace. */
            media_path(preview,sizeof(preview),e->media);
            int fd=rb->open(preview,O_RDONLY);
            unsigned char header[12];
            if (fd>=0 && rb->read(fd,header,12)==12 &&
                !rb->memcmp(header,"MGA1",4) && header[4]==160 &&
                header[5]==0 && header[6]==120 && header[7]==0 &&
                rb->read(fd,media.frame,sizeof(media.frame))==sizeof(media.frame))
            {
                fb_data *pixels=(fb_data *)chat_image_data;
                for (int y=0;y<80;y++)
                    for (int x=0;x<106;x++)
                    {
                        fb_data pixel=media.frame[(y*120/80)*160+x*160/106];
#if defined(ROCKBOX_BIG_ENDIAN)
                        pixel=swap16(pixel);
#endif
                        pixels[y*106+x]=pixel;
                    }
                chat_image.data=chat_image_data;
                chat_image.width=106;chat_image.height=80;
            }
            if (fd>=0) rb->close(fd);
        }
    }
    redraw=true;
}
static void draw_chat(void)
{
    static const char *statuses[]={"Online","Away","Busy","Offline"};
    chrome("Conversation","");
    art_draw(&avatar_bm,263,20);
    wrapped(contacts[active].name,8,26,248,1,MSN_BLUE);
    text(8,47,statuses[contacts[active].status],MSN_GRAY);
    bool typing=false;
    for (int i=0;i<event_count;i++)
        if (events[i].contact==active && !events[i].outgoing && events[i].kind==MSN_MESSAGE &&
            events[i].at>now && events[i].at-now<=events[i].typing) typing=true;
    if (typing) text(65,47,"is typing a message...",MSN_GRAY);
    if (!visible_count) wrapped("Your conversation starts here.",12,96,290,2,MSN_GRAY);
    int first=cursor,y=73;
    if (cursor>0 && !is_picture(&events[visible[cursor]]) &&
        !is_picture(&events[visible[cursor-1]])) first--;
    for (int pos=first;pos<=cursor && pos<visible_count;pos++)
    {
        struct msn_event *e=&events[visible[pos]];
        bool picture=is_picture(e);
        if (pos==cursor && !toolbar_focus)
            focus_box(3,y-1,314,picture ? 110 : 54);
        char label[180];
        rb->snprintf(label,sizeof(label),"%02ld:%02ld  %s says:",(e->at/3600)%24,(e->at/60)%60,
                     e->outgoing ? "You" : contacts[active].name);
        wrapped(label,8,y,298,1,MSN_BLUE);
        const char *body=e->kind==MSN_NUDGE ? (e->outgoing ? "You have sent a nudge!" : "You have received a nudge!") : e->text;
        if (!*body) body=e->kind==MSN_PHOTO ? "Photo - select to view" : e->kind==MSN_GIF ? "Animation - select to view" : "Video - select to play";
        if (picture)
        {
            if (chat_image_event==visible[pos] && chat_image.width)
                rb->lcd_bitmap((fb_data *)chat_image.data,
                    10+shake_x+(128-chat_image.width)/2,
                    y+18+(80-chat_image.height)/2,
                    chat_image.width,chat_image.height);
            else wrapped(chat_image_event==visible[pos] ?
                         "Preview unavailable" : "Loading photo...",
                         12,y+30,120,3,MSN_GRAY);
            wrapped(body,148,y+18,156,4,MSN_INK);
            text(148,y+94,e->kind==MSN_GIF ? "SELECT Play / Save" :
                 "SELECT Enlarge",MSN_BLUE);
        }
        else wrapped(body,12,y+18,288,2,MSN_INK);
        y+=picture ? 112 : 56;
    }
    draw_action_bar();
}

static bool local_id_exists(const char *id)
{
    if (event_exists(id)) return true;
    int fd=rb->open(MSN_ROOT "/local.tsv",O_RDONLY);
    if (fd<0) return false;
    char row[MSN_LINE]; bool found=false;
    while (rb->read_line(fd,row,sizeof(row))>0)
    {
        char *tab=rb->strchr(row,'\t'); if (tab) *tab=0;
        if (!rb->strcmp(row,id)) { found=true; break; }
    }
    rb->close(fd); return found;
}

static bool append_local(const char *id,int kind,const char *body,const char *file,bool outgoing,long at)
{
    if (!reserve_event()) return false;
    int fd=rb->open(MSN_ROOT "/local.tsv",O_WRONLY|O_CREAT|O_APPEND,0666);
    if (fd<0) return false;
    int n;
    /* Persist kind names, so future versions can read the journal. */
    static const char *kinds[]={"message","photo","gif","video","nudge","status","reply","wink"};
    n=rb->snprintf(line,sizeof(line),"%s\t%ld\t%s\t%s\t5\t%d\t%s\t%s\n",
                  id,at,contacts[active].id,kinds[kind],outgoing,body,file);
    bool ok=n>0 && n<(int)sizeof(line) && rb->write(fd,line,n)==n;
    rb->close(fd);
    if (!ok) return false;
    struct msn_event *e=&events[event_count++]; rb->memset(e,0,sizeof(*e));
    rb->strlcpy(e->id,id,sizeof(e->id)); e->at=at; e->kind=kind; e->contact=active;
    e->outgoing=outgoing; e->typing=5; e->read=outgoing;
    rb->strlcpy(e->text,body,sizeof(e->text)); rb->strlcpy(e->media,file,sizeof(e->media));
    if (outgoing) now=at;
    filter(); cursor=MAX(0,visible_count-1); return true;
}

static void schedule_reaction(const char *sent)
{
    if (contacts[active].status!=0) return;
    char lower[512],row[MSN_LINE],id[33];
    rb->strlcpy(lower,sent,sizeof(lower));
    for (char *p=lower;*p;p++) if (*p>='A' && *p<='Z') *p+=32;
    /* Specific matches first, then a finite authored general response bank. */
    for (int pass=0;pass<2;pass++)
    {
        int fd=rb->open(MSN_ROOT "/responses.tsv",O_RDONLY);
        if (fd<0) return;
        while (rb->read_line(fd,row,sizeof(row))>0)
        {
            char *f[4]; if (fields(row,f,4)!=4 || rb->strcmp(f[1],contacts[active].id)) continue;
            bool match=!*f[2];
            if ((pass==0 && match) || (pass==1 && !match)) continue;
            char *key=f[2];
            while (*key)
            {
                char *end=rb->strchr(key,'|'); if (end) *end=0;
                if (rb->strstr(lower,key)) match=true;
                if (!end) break;
                key=end+1;
            }
            rb->snprintf(id,sizeof(id),"r-%s",f[0]);
            if (match && !local_id_exists(id))
            {
                append_local(id,MSN_MESSAGE,f[3],"",false,now+8+(*rb->current_tick%15));
                rb->close(fd); return;
            }
        }
        rb->close(fd);
    }
}

static int send_local(int kind,const char *body,const char *file)
{
    char id[33];
    rb->snprintf(id,sizeof(id),"u-%ld-%lx",rtc_now(),(unsigned long)*rb->current_tick);
    if (!append_local(id,kind,body,file,true,rtc_now()))
    { rb->splash(HZ*2,"Message could not be saved"); return PLUGIN_OK; }
    if (!suppress_reaction) schedule_reaction(body);
    toolbar_focus=false; redraw=true;
    return PLUGIN_OK;
}

static int compose(void)
{
    reply[0]=0;
    if (rb->kbd_input(reply,sizeof(reply),NULL)<0 || !reply[0]) return PLUGIN_OK;
    for (char *p=reply;*p;p++) if (*p=='\t' || *p=='\n' || *p=='\r') *p=' ';
    return send_local(MSN_REPLY,reply,"");
}

static int read_animation_frame(int fd, int *delay)
{
    unsigned char bytes[2];
    if (rb->read(fd,bytes,2) != 2 ||
        rb->read(fd,media.frame,sizeof(media.frame)) != sizeof(media.frame)) return -1;
    *delay = MAX(2,(bytes[0] | bytes[1]<<8)*HZ/1000);
#if defined(ROCKBOX_BIG_ENDIAN)
    for (unsigned i = 0; i < ARRAYLEN(media.frame); i++)
        media.frame[i] = swap16(media.frame[i]);
#endif
    return 0;
}

/* Copy outside the synced bundle: a later Messenger sync cannot remove saves.
 * A bounded transfer keeps the currently displayed image and music intact. */
static bool copy_saved_file(const char *source, const char *destination)
{
    static unsigned char transfer[4096];
    char temporary[MAX_PATH];
    rb->snprintf(temporary,sizeof(temporary),"%s.msnpart",destination);
    int input = rb->open(source,O_RDONLY);
    if (input < 0) return false;
    int output = rb->open(temporary,O_WRONLY|O_CREAT|O_TRUNC,0666);
    bool ok = output >= 0;
    ssize_t count = 0;
    while (ok && (count = rb->read(input,transfer,sizeof(transfer))) > 0)
    {
        ok = rb->write(output,transfer,count) == count;
        rb->yield();
    }
    ok = ok && count == 0;
    rb->close(input);
    if (output >= 0) rb->close(output);
    if (ok) ok = rb->rename(temporary,destination) == 0;
    if (!ok) rb->remove(temporary);
    return ok;
}

static void save_to_photos(const struct msn_event *e)
{
    char source[MAX_PATH], destination[MAX_PATH];
    const char *extension = e->kind == MSN_GIF ? "gif" : "jpg";
    /* Manifest IDs become filenames, so reject any path separators. */
    for (const char *p = e->id; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '-' || *p == '_')) return;
    rb->mkdir("/Photos");
    rb->mkdir("/Photos/MSN Messenger");
    rb->snprintf(destination,sizeof(destination),
                 "/Photos/MSN Messenger/%s.%s",e->id,extension);
    if (rb->file_exists(destination))
    {
        rb->splash(HZ,"Already saved to Photos");
        return;
    }
    char origin[MAX_PATH];
    media_path(origin,sizeof(origin),e->media);
    char *media_folder=rb->strstr(origin,"/media/");
    if (media_folder) *media_folder=0; else rb->strlcpy(origin,bundle,sizeof(origin));
    rb->snprintf(source,sizeof(source),"%s/saved/%s.%s",origin,e->id,extension);
    if (e->kind==MSN_GIF && !rb->strncmp(e->media,MSN_ART "/gifs/",sizeof(MSN_ART "/gifs/")-1))
    {
        rb->strlcpy(source,e->media,sizeof(source));
        char *ext=rb->strrchr(source,'.'); if (ext) rb->strlcpy(ext,".gif",5);
    }
    if (!copy_saved_file(source,destination))
    {
        rb->splash(HZ*2,"Could not save photo");
        return;
    }
    static const char *folders[] = { ".photo_thumbs", ".photo_previews" };
    static const char *suffixes[] = { "thumb", "preview" };
    for (int i = 0; i < 2; i++)
    {
        rb->snprintf(destination,sizeof(destination),"/Photos/%s",folders[i]);
        rb->mkdir(destination);
        rb->snprintf(destination,sizeof(destination),"/Photos/%s/MSN Messenger",folders[i]);
        rb->mkdir(destination);
        rb->snprintf(destination,sizeof(destination),
                     "/Photos/%s/MSN Messenger/%s.%s.bmp",folders[i],e->id,extension);
        rb->snprintf(source,sizeof(source),"%s/saved/%s.%s.bmp",origin,e->id,suffixes[i]);
        copy_saved_file(source,destination);
    }
    rb->splash(HZ,"Saved to Photos");
}

static int attachment_event(struct msn_event *e)
{
    if (e->kind==MSN_WINK) return wink_play(e);
    int fd = -1, frames = 0, frame = 0, delay = HZ/10;
    long next = 0;
    if (e->kind == MSN_VIDEO)
    {
        static char launch[MAX_PATH+16];
        sound_stop(); save_read();
        media_path(path,sizeof(path),e->media);
        rb->snprintf(launch,sizeof(launch),"msn-app:%s",path);
        return rb->plugin_open(plugin_video_player_for(e->media),launch);
    }
    media_path(path,sizeof(path),e->media);
    if (e->kind == MSN_PHOTO)
    {
        char preview[MAX_PATH];
        if (!picture_preview(e,preview,sizeof(preview)) ||
            !bitmap_fit(preview,&media_bm,media.image,sizeof(media.image),
                        304,184))
            bitmap_load(path,&media_bm,media.image,sizeof(media.image),300,160);
    }
    if (e->kind == MSN_GIF)
    {
        unsigned char bytes[10];
        fd = rb->open(path,O_RDONLY);
        if (fd >= 0 && rb->read(fd,bytes,10) == 10 &&
            !rb->memcmp(bytes,"MGA1",4) && bytes[4] == 160 && bytes[5] == 0 &&
            bytes[6] == 120 && bytes[7] == 0)
            frames = bytes[8] | bytes[9]<<8;
        if (frames < 1 || frames > 600)
        {
            if (fd >= 0) rb->close(fd);
            fd = -1;
        }
    }
    int scroll = 0;
    shake_x = 0;
    while (true)
    {
        if (fd >= 0 && !TIME_BEFORE(*rb->current_tick,next))
        {
            if (frame == frames) { rb->lseek(fd,10,SEEK_SET); frame = 0; }
            if (read_animation_frame(fd,&delay) < 0) { rb->close(fd); fd = -1; }
            else { next = *rb->current_tick+delay; frame++; }
        }
        bool can_save = e->kind == MSN_PHOTO || e->kind == MSN_GIF;
        chrome(contacts[active].name,can_save ?
               "SELECT Save to Photos   MENU Back" : "MENU Conversation");
        if (e->kind == MSN_PHOTO && media_bm.width)
            rb->lcd_bitmap((fb_data *)media_bm.data,
                           (LCD_WIDTH-media_bm.width)/2,
                           24+(184-media_bm.height)/2,
                           media_bm.width,media_bm.height);
        else if (fd >= 0)
            rb->lcd_bitmap(media.frame,80,64,160,120);
        else
            wrapped(e->text+MIN(scroll,(int)rb->strlen(e->text)),10,48,300,10,MSN_INK);
        rb->lcd_update();
        int action = pluginlib_getaction(fd >= 0 ? MAX(1,MIN(HZ/10,next-*rb->current_tick)) : HZ/5,
                                        contexts,ARRAYLEN(contexts));
        if (action == PLA_CANCEL || action == PLA_EXIT || action == SYS_USB_CONNECTED)
        {
            if (fd >= 0) rb->close(fd);
            redraw = true;
            return action == SYS_USB_CONNECTED ? PLUGIN_USB_CONNECTED : PLUGIN_OK;
        }
        if (action == PLA_SCROLL_FWD && scroll < (int)rb->strlen(e->text))
        {
            scroll = MIN(scroll+30,(int)rb->strlen(e->text));
            while ((e->text[scroll]&0xc0) == 0x80) scroll++;
        }
        if (action == PLA_SCROLL_BACK) scroll = MAX(0,scroll-30);
        if (action == PLA_SELECT_REL && can_save) save_to_photos(e);
    }
}


static int emote_picker(bool animated)
{
    int choices[MSN_EMOTE_COUNT], count=0, choice=0;
    bool unicode=false;
    static const char *emoji[]={"🌷","😊","🥰","❤️","😉","😘","😂","💕","✨","🌙","☕","🎵","💯","🤱","🔹","👋","💐","🦋","🍀","🌈","💜","😴","😎","👍"};
    for (int i=0;i<MSN_EMOTE_COUNT;i++)
        if (!animated || msn_emotes[i].frames>1) choices[count++]=i;
    while (true)
    {
        chrome(animated ? "Animated emoticons" : unicode ? "Emoji" : "MSN Emoticons",
               animated ? "SELECT Send   MENU Back" : "RIGHT Emoji / MSN   SELECT Send");
        int total=unicode ? (int)ARRAYLEN(emoji) : count, first=(choice/32)*32;
        for (int i=first;i<MIN(first+32,total);i++)
        {
            int x=11+(i%8)*38,y=40+((i-first)/8)*35;
            if (i==choice) focus_box(x-4,y-4,30,28);
            if (unicode) text(x,y,emoji[i],MSN_INK);
            else emote_draw(choices[i],x,y);
        }
        text(10,190,unicode ? emoji[choice] : msn_emotes[choices[choice]].name,MSN_BLUE);
        rb->lcd_update();
        int a=pluginlib_getaction(HZ/10,contexts,ARRAYLEN(contexts));
        if (a==PLA_CANCEL || a==PLA_EXIT) return PLUGIN_OK;
        if (a==SYS_USB_CONNECTED) return PLUGIN_USB_CONNECTED;
        if (a==PLA_RIGHT && !animated) { unicode=!unicode;choice=0; }
        if (a==PLA_SCROLL_FWD || a==PLA_SCROLL_FWD_REPEAT) choice=(choice+1)%total;
        if (a==PLA_SCROLL_BACK || a==PLA_SCROLL_BACK_REPEAT) choice=(choice+total-1)%total;
        if (a==PLA_SELECT_REL)
        {
            if (unicode) return send_local(MSN_REPLY,emoji[choice],"");
            const struct msn_emote *e=&msn_emotes[choices[choice]];
            if (!animated) return send_local(MSN_REPLY,e->shortcut,"");
            char file[MAX_PATH]; rb->snprintf(file,sizeof(file),MSN_ART "/gifs/%s.mga",e->slug);
            return send_local(MSN_GIF,e->name,file);
        }
        if (rb->default_event_handler(a)==SYS_USB_CONNECTED) return PLUGIN_USB_CONNECTED;
    }
}

/* Scan persistent journals only on explicit history navigation. Keep the latest
 * 64 entries before a stable (timestamp,id) boundary; storage history is unbounded. */
static int event_order(const struct msn_event *a,const struct msn_event *b)
{
    if (a->at!=b->at) return a->at<b->at ? -1 : 1;
    return rb->strcmp(a->id,b->id);
}
static void history_load(long before,const char *id)
{
    history_count=0;
    for (int source=0;source<3;source++)
    {
        int fd=rb->open(source==2 ? MSN_ROOT "/replies.tsv" : source ? MSN_ROOT "/local.tsv" : MSN_ROOT "/archive.tsv",O_RDONLY);
        if (fd<0) continue;
        struct msn_event candidate;int legacy_row=0;
        while (rb->read_line(fd,line,sizeof(line))>0)
        {
            bool valid;
            if (source==2)
            {
                char *f[3];legacy_row++;valid=fields(line,f,3)==3;
                if (!valid) continue;
                rb->memset(&candidate,0,sizeof(candidate));candidate.at=rb->atoi(f[0]);
                candidate.contact=contact_index(f[1]);candidate.kind=MSN_REPLY;candidate.outgoing=true;
                rb->strlcpy(candidate.text,f[2],sizeof(candidate.text));
                rb->snprintf(candidate.id,sizeof(candidate.id),"legacy-%d",legacy_row);
            }
            else valid=decode_event(line,&candidate,source);
            if (!valid || candidate.contact!=active || candidate.at>now ||
                candidate.at>before || (candidate.at==before && rb->strcmp(candidate.id,id)>=0)) continue;
            int pos=0;
            while (pos<history_count && event_order(&history_page[pos],&candidate)<0) pos++;
            if (pos<history_count && !event_order(&history_page[pos],&candidate)) continue;
            if (history_count==64)
            {
                if (!pos) continue;
                rb->memmove(history_page,history_page+1,63*sizeof(*history_page));
                history_count--;pos--;
            }
            rb->memmove(history_page+pos+1,history_page+pos,(history_count-pos)*sizeof(*history_page));
            history_page[pos]=candidate;history_count++;
        }
        rb->close(fd);
    }
}
static int history_browser(void)
{
    history_load(0x7fffffff,"");int pos=MAX(0,history_count-1);
    while (true)
    {
        chrome("Conversation history","LEFT Older   RIGHT Latest   MENU Back");
        if (!history_count) text(10,60,"No earlier messages",MSN_GRAY);
        for (int i=MAX(0,pos-2),y=35;i<=pos && i<history_count;i++,y+=56)
        {
            struct msn_event *e=&history_page[i];
            if (i==pos) focus_box(4,y-2,312,56);
            char label[180];struct tm tm,*t=&tm;time_t stamp=e->at;rb->gmtime_r(&stamp,t);
            rb->snprintf(label,sizeof(label),"%02d/%02d %02d:%02d  %s",t->tm_mon+1,t->tm_mday,t->tm_hour,t->tm_min,e->outgoing ? "You" : contacts[active].name);
            text(8,y,label,MSN_BLUE);wrapped(e->text,8,y+18,304,2,MSN_INK);
        }
        rb->lcd_update();int a=pluginlib_getaction(HZ/10,contexts,ARRAYLEN(contexts));
        if (a==PLA_CANCEL || a==PLA_EXIT) return PLUGIN_OK;
        if (a==SYS_USB_CONNECTED) return PLUGIN_USB_CONNECTED;
        if (a==PLA_SCROLL_FWD || a==PLA_SCROLL_FWD_REPEAT) pos=MIN(history_count-1,pos+1);
        if (a==PLA_SCROLL_BACK || a==PLA_SCROLL_BACK_REPEAT) pos=MAX(0,pos-1);
        if (a==PLA_LEFT && history_count)
        {
            long at=history_page[0].at;char id[33];rb->strlcpy(id,history_page[0].id,sizeof(id));
            history_load(at,id);pos=MAX(0,history_count-1);
        }
        if (a==PLA_RIGHT) { history_load(0x7fffffff,"");pos=MAX(0,history_count-1); }
        if (a==PLA_SELECT_REL && history_count)
        { int result=attachment_event(&history_page[pos]);if (result!=PLUGIN_OK) return result; }
        if (rb->default_event_handler(a)==SYS_USB_CONNECTED) return PLUGIN_USB_CONNECTED;
    }
}

static int natural_reply(void)
{
    const char *context="";long latest=0;
    for (int i=0;i<event_count;i++)
        if (events[i].contact==active && !events[i].outgoing && events[i].at<=now && events[i].at>=latest &&
            rb->strncmp(events[i].id,"r-",2) && rb->strncmp(events[i].id,"q-",2))
        { latest=events[i].at;context=events[i].id; }
    if (visible_count && cursor>=0 && cursor<visible_count)
    {
        struct msn_event *e=&events[visible[cursor]];
        if (!e->outgoing && rb->strncmp(e->id,"r-",2) && rb->strncmp(e->id,"q-",2)) context=e->id;
    }
    int fd=rb->open(MSN_ROOT "/choices.tsv",O_RDONLY),count=0;
    /* A choice and its follow-up can each use 511 bytes. */
    char row[1152];
    if (fd>=0)
    {
        while (count<5 && rb->read_line(fd,row,sizeof(row))>0)
        {
            char *f[4];if (fields(row,f,4)!=4 || rb->strcmp(f[0],context) || local_id_exists(f[2])) continue;
            rb->strlcpy(choice_text[count],f[1],512);rb->strlcpy(choice_id[count],f[2],33);
            rb->strlcpy(choice_response[count],f[3],512);count++;
        }
        rb->close(fd);
    }
    int selected=0;
    while (true)
    {
        chrome("Reply","SELECT Send   MENU Back");
        for (int i=(selected/3)*3;i<=count && i<(selected/3+1)*3;i++)
        {
            int y=35+(i%3)*54;
            if (i==selected) fill(4,y,312,52,MSN_FOCUS);
            wrapped(i==count ? "Write your own..." : choice_text[i],12,y+6,296,2,
                    i==selected ? MSN_WHITE : MSN_INK);
        }
        rb->lcd_update();int a=pluginlib_getaction(HZ/5,contexts,ARRAYLEN(contexts));
        if (a==PLA_CANCEL || a==PLA_EXIT) return PLUGIN_OK;
        if (a==SYS_USB_CONNECTED) return PLUGIN_USB_CONNECTED;
        if (a==PLA_SCROLL_FWD || a==PLA_SCROLL_FWD_REPEAT) selected=(selected+1)%(count+1);
        if (a==PLA_SCROLL_BACK || a==PLA_SCROLL_BACK_REPEAT) selected=(selected+count)%(count+1);
        if (a==PLA_SELECT_REL)
        {
            if (selected==count) return compose();
            suppress_reaction=true;send_local(MSN_REPLY,choice_text[selected],"");suppress_reaction=false;
            if (*choice_response[selected]) append_local(choice_id[selected],MSN_MESSAGE,choice_response[selected],"",false,now+7+(*rb->current_tick%12));
            return PLUGIN_OK;
        }
        if (rb->default_event_handler(a)==SYS_USB_CONNECTED) return PLUGIN_USB_CONNECTED;
    }
}
static void wink_more(const void **start,size_t *size)
{
    unsigned frames=0;
    while (frames<512 && wink_position<wink_samples)
    {
        int16_t sample=(int16_t)(sound_data[2*wink_position]|sound_data[2*wink_position+1]<<8);
        wink_output[frames*2]=sample;wink_output[frames*2+1]=sample;frames++;
        wink_phase+=11025;wink_position+=wink_phase/wink_rate;wink_phase%=wink_rate;
    }
    *start=wink_output;*size=frames*4;
}
static void wink_sound(const char *file)
{
#ifndef HAVE_HARDWARE_BEEP
    sound_stop();if (!sound_on) return;
    wink_rate=rb->mixer_get_frequency();if (!wink_rate) return;
    char filename[MAX_PATH];rb->strlcpy(filename,file,sizeof(filename));
    char *dot=rb->strrchr(filename,'.');if (!dot) return;rb->strcpy(dot,".pcm");
    int fd=rb->open(filename,O_RDONLY);if (fd<0) return;
    off_t size=rb->filesize(fd);int got=0;
    if (size>0 && size<=MSN_SOUND_BYTES && !(size%2)) got=rb->read(fd,sound_data,size);
    rb->close(fd);if (got!=size || !got) return;
    wink_samples=got/2;wink_position=wink_phase=0;sound_owned=true;
    rb->mixer_channel_set_amplitude(PCM_MIXER_CHAN_BEEP,MIX_AMP_UNITY/2);
    rb->mixer_channel_play_data(PCM_MIXER_CHAN_BEEP,wink_more,NULL,0);
#else
    (void)file;
#endif
}
static int wink_play(struct msn_event *e)
{
    media_path(path,sizeof(path),e->media);
    int fd=rb->open(path,O_RDONLY),result=PLUGIN_OK;unsigned char head[10];
    if (fd<0) return result;
    if (rb->read(fd,head,10)!=10 || rb->memcmp(head,"MWA2",4) || head[4]!=240 || head[5] || head[6]!=160 || head[7])
    { rb->close(fd);return result; }
    int frames=head[8]|head[9]<<8;if (frames<1 || frames>600) { rb->close(fd);return result; }
    if (!e->played && !e->outgoing)
    {
        int journal=rb->open(MSN_ROOT "/winks-played.tsv",O_WRONLY|O_CREAT|O_APPEND,0666);
        if (journal>=0) { rb->fdprintf(journal,"%s\n",e->id);rb->close(journal); }
        e->played=true;
    }
    wink_sound(path);long next=*rb->current_tick;int frame=0;
    while (frame<frames || TIME_BEFORE(*rb->current_tick,next))
    {
        int action=pluginlib_getaction(1,contexts,ARRAYLEN(contexts));
        if (action==PLA_CANCEL || action==PLA_EXIT) break;
        if (action==SYS_USB_CONNECTED || rb->default_event_handler(action)==SYS_USB_CONNECTED)
        { result=PLUGIN_USB_CONNECTED;break; }
        if (frame<frames && !TIME_BEFORE(*rb->current_tick,next))
        {
            unsigned char delay[4];
            if (rb->read(fd,delay,4)!=4) break;
            int runs=delay[2]|delay[3]<<8;
            if (!runs || runs>240*160) break;
            unsigned char *encoded=media.image+240*160*sizeof(fb_data);
            if (rb->read(fd,encoded,runs*4)!=runs*4) break;
            fb_data *pixels=(fb_data *)media.image;unsigned total=0;bool valid=true;
            for (int i=0;i<runs;i++)
            {
                unsigned n=encoded[i*4]|encoded[i*4+1]<<8;
                fb_data value=encoded[i*4+2]|encoded[i*4+3]<<8;
                if (!n || n>240*160-total) { valid=false;break; }
                while (n--) pixels[total++]=value;
            }
            if (!valid || total!=240*160) break;
            next+=MAX(1,(delay[0]|delay[1]<<8)*HZ/1000);frame++;
            draw_chat();rb->lcd_bitmap_transparent((fb_data *)media.image,40,35,240,160);rb->lcd_update();
        }
    }
    rb->close(fd);sound_stop();redraw=true;return result;
}
static int wink_picker(void)
{
    int selected=0;
    if (!wink_count) { rb->splash(HZ,"Sync original MSN Winks first");return PLUGIN_OK; }
    while (true)
    {
        chrome("MSN Winks","SELECT Send Wink   MENU Back");
        int first=(selected/5)*5;
        for (int i=first,y=33;i<MIN(first+5,wink_count);i++,y+=31)
        {
            if (i==selected) fill(4,y,312,30,MSN_FOCUS);
            text(12,y+5,wink_names[i],i==selected ? MSN_WHITE : MSN_INK);
        }
        rb->lcd_update();int a=pluginlib_getaction(HZ/5,contexts,ARRAYLEN(contexts));
        if (a==PLA_CANCEL || a==PLA_EXIT) return PLUGIN_OK;
        if (a==SYS_USB_CONNECTED) return PLUGIN_USB_CONNECTED;
        if (a==PLA_SCROLL_FWD || a==PLA_SCROLL_FWD_REPEAT) selected=(selected+1)%wink_count;
        if (a==PLA_SCROLL_BACK || a==PLA_SCROLL_BACK_REPEAT) selected=(selected+wink_count-1)%wink_count;
        if (a==PLA_SELECT_REL)
        {
            char file[MAX_PATH];rb->snprintf(file,sizeof(file),MSN_ART "/winks/%s.mwa",wink_ids[selected]);
            send_local(MSN_WINK,wink_names[selected],file);
            struct msn_event preview;rb->memset(&preview,0,sizeof(preview));preview.outgoing=true;
            rb->strlcpy(preview.media,file,sizeof(preview.media));return wink_play(&preview);
        }
        if (rb->default_event_handler(a)==SYS_USB_CONNECTED) return PLUGIN_USB_CONNECTED;
    }
}
static int toolbar_action(void)
{
    switch (toolbar_selected)
    {
        case 0:return natural_reply();
        case 1:return emote_picker(false);
        case 2:
            pending_sound=1;shake_until=*rb->current_tick+HZ;
            return send_local(MSN_NUDGE,"You have just sent a nudge!","");
        case 3:return wink_picker();
        case 4:return emote_picker(true);
        case 5:return history_browser();
        case 6:
            sound_on=!sound_on;if (!sound_on) sound_stop();
            rb->splash(HZ/2,sound_on ? "Messenger sounds on" : "Messenger sounds off");
    }
    return PLUGIN_OK;
}
static int signin(void)
{
    art_load("signin",&media_bm,media.image,sizeof(media.image),75,2001);
    if (!media_bm.width) return PLUGIN_OK;
    int fd = rb->open(MSN_ART "/signin.tsv",O_RDONLY);
    int total = 0;
    for (int i = 0; i < 29; i++)
    {
        signin_delay[i] = HZ/10;
        if (fd >= 0 && rb->read_line(fd,line,sizeof(line)) > 0)
            signin_delay[i] = MAX(1,rb->atoi(line)*HZ/1000);
        total += signin_delay[i];
    }
    if (fd >= 0) rb->close(fd);
    long start = *rb->current_tick;
    while (TIME_BEFORE(*rb->current_tick,start+2*HZ))
    {
        int elapsed = (*rb->current_tick-start)%total, index = 0;
        while (index < 28 && elapsed >= signin_delay[index]) elapsed -= signin_delay[index++];
        chrome("MSN Messenger","SELECT Skip");
        rb->lcd_bitmap_transparent_part((fb_data *)media_bm.data,0,index*69,75,122,83,75,69);
        text(102,166,"Signing in...",MSN_BLUE);
        rb->lcd_update();
        int action = pluginlib_getaction(HZ/15,contexts,ARRAYLEN(contexts));
        if (action!=ACTION_NONE) last_input=*rb->current_tick;
        if (action == SYS_USB_CONNECTED) return PLUGIN_USB_CONNECTED;
        if (action == PLA_SELECT_REL || action == PLA_CANCEL || action == PLA_EXIT) break;
    }
    return PLUGIN_OK;
}

enum plugin_status plugin_start(const void *parameter)
{
    int result = PLUGIN_OK, w;
    rb->lcd_set_backdrop(NULL);
    font_id = FONT_UI;
    rb->lcd_setfont(font_id);
    rb->lcd_getstringsize("Ag",&w,&text_height);
    if (text_height > 14)
    {
        font_id = FONT_SYSFIXED; rb->lcd_setfont(font_id);
        rb->lcd_getstringsize("Ag",&w,&text_height);
    }
    text_height = 18;
    if (!load_bundle())
    {
        rb->splash(HZ*3,"Sync contacts from RockPod Messenger");
        return PLUGIN_OK;
    }
    load_recent_archive();
    load_local_events();
    read_state();
    int wink_fd=rb->open(MSN_ROOT "/winks-played.tsv",O_RDONLY);
    if (wink_fd>=0)
    {
        while (rb->read_line(wink_fd,line,sizeof(line))>0)
            for (int i=0;i<event_count;i++) if (!rb->strcmp(line,events[i].id)) events[i].played=true;
        rb->close(wink_fd);
    }
    wink_fd=rb->open(MSN_ART "/winks/catalog.tsv",O_RDONLY);
    if (wink_fd>=0)
    {
        while (wink_count<32 && rb->read_line(wink_fd,line,sizeof(line))>0)
        {
            char *f[2];if (fields(line,f,2)!=2 || !safe_relative(f[0])) continue;
            rb->strlcpy(wink_ids[wink_count],f[0],33);rb->strlcpy(wink_names[wink_count++],f[1],48);
        }
        rb->close(wink_fd);
    }
    art_load("header",&header_bm,header_data,sizeof(header_data),320,70);
    art_load("toolbar",&toolbar_bm,toolbar_data,sizeof(toolbar_data),320,28);
    art_load("default",&self_bm,self_data,sizeof(self_data),48,48);
    art_load("logo",&logo_bm,logo_data,sizeof(logo_data),46,16);
    static const char *names[] = { "online","away","busy","offline","emoticon","nudge","mail","pen","wink","sound","history" };
    for (unsigned i = 0; i < ARRAYLEN(names); i++)
        art_load(names[i],&icons[i],icon_data[i],sizeof(icon_data[i]),20,28);
    art_load("emoticons",&emote_bm,emote_data,sizeof(emote_data),MSN_EMOTE_ATLAS_W,MSN_EMOTE_ATLAS_H);
    opened_tick = *rb->current_tick;
    if (parameter)
    {
        const char *p = parameter;
        if (!rb->strncmp(p,"return:",7)) active = selected;
        else active = contact_index(p);
    }
    else result = signin();
    if (result != PLUGIN_OK) goto done;
    service(); filter(); cursor = MAX(0,visible_count-1); prepare_contact();
    sound_play(2);
    while (true)
    {
        if (TIME_AFTER(*rb->current_tick,last_service+HZ/2))
        {
            service(); last_service = *rb->current_tick; redraw = true;
        }
        if (active>=0 && rb->button_queue_count()==0)
        {
            for (int i=0;i<event_count;i++)
                if (events[i].contact==active && events[i].kind==MSN_WINK && events[i].at<=now && !events[i].outgoing && !events[i].played)
                { result=wink_play(&events[i]);break; }
            if (result!=PLUGIN_OK) break;
        }
        if (rb->button_queue_count()==0 && TIME_AFTER(*rb->current_tick,last_input+HZ/8)) prepare_chat_image();
        if (pending_sound >= 0 && rb->button_queue_count() == 0)
        {
            sound_play(pending_sound); pending_sound = -1;
        }
        if (TIME_AFTER(*rb->current_tick,last_animation+HZ/10))
        { last_animation=*rb->current_tick;redraw=true; }
        if (redraw || TIME_BEFORE(*rb->current_tick,shake_until))
        {
            shake_x = TIME_BEFORE(*rb->current_tick,shake_until) ?
                ((*rb->current_tick/(HZ/15))%2 ? 3 : -3) : 0;
            if (active < 0) draw_contacts(); else draw_chat();
            rb->lcd_update(); redraw = false;
        }
        int action = pluginlib_getaction(HZ/15,contexts,ARRAYLEN(contexts));
        if (action!=ACTION_NONE) last_input=*rb->current_tick;
        if (action == SYS_USB_CONNECTED) { result = PLUGIN_USB_CONNECTED; break; }
        if (action == PLA_EXIT) break;
        if (action == PLA_CANCEL)
        {
            if (active < 0) break;
            if (toolbar_focus) toolbar_focus=false;
            else { active = -1; prepare_contact(); }
            redraw = true;
        }
        else if (action == PLA_SCROLL_FWD || action == PLA_SCROLL_BACK ||
                 action == PLA_SCROLL_FWD_REPEAT || action == PLA_SCROLL_BACK_REPEAT)
        {
            int delta = (action == PLA_SCROLL_FWD || action == PLA_SCROLL_FWD_REPEAT) ? 1 : -1;
            if (active < 0) selected = MAX(0,MIN(contact_count-1,selected+delta));
            else if (toolbar_focus) toolbar_selected=(toolbar_selected+delta+7)%7;
            else cursor = MAX(0,MIN(visible_count-1,cursor+delta));
            redraw = true;
        }
        else if (action == PLA_SELECT_REL)
        {
            if (active < 0)
            {
                active = selected; filter(); cursor = MAX(0,visible_count-1);
                prepare_contact(); service();
            }
            else
            {
                sound_stop(); result = toolbar_focus ? toolbar_action() :
                    visible_count ? attachment_event(&events[visible[cursor]]) : PLUGIN_OK;
                if (result != PLUGIN_OK) break;
            }
            redraw = true;
        }
        else if (action == PLA_SELECT_REPEAT && active >= 0)
        {
            sound_stop(); result = natural_reply();
            if (result != PLUGIN_OK) break;
            redraw = true;
        }
        else if (action == PLA_DOWN && active >= 0)
        { toolbar_focus=!toolbar_focus;redraw=true; }
        else if ((action == PLA_RIGHT || action == PLA_LEFT) && active >= 0)
        {
            toolbar_focus=true;
            toolbar_selected=(toolbar_selected+(action==PLA_RIGHT ? 1 : 6))%7;
            redraw=true;
        }
        else if (action != PLA_SELECT && action != PLA_SELECT_REL)
        {
            if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
            { result = PLUGIN_USB_CONNECTED; break; }
        }
    }
done:
    sound_stop(); save_read();
    int fd = rb->open(MSN_ROOT "/selection.txt",O_WRONLY|O_CREAT|O_TRUNC,0666);
    if (fd >= 0)
    {
        rb->fdprintf(fd,"%s\n",contacts[selected].id); rb->close(fd);
    }
    return result;
}
