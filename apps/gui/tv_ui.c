/* SPDX-License-Identifier: GPL-2.0-or-later
 * Independent TV canvas; handheld theme, fonts, WPS and selection are never
 * replaced. Fixed workspace: 204480 canvas, 98304 art/decode and 27264
 * transition-strip bytes.
 * The existing WPS action loop remains the only playback-screen owner. */
#include "config.h"
#ifdef HAVE_COMPOSITE_VIDEO_OUT
#include "tv_ui.h"
#include "tv_guide.h"
#include "videoout.h"
#include "lcd.h"
#include "font.h"
#include "ipodjs_ui.h"
#include "ipodjs_retailos.h"
#include "tv_apple_assets.h"
#include "tv_directv_assets.h"
#include "settings.h"
#include "lang.h"
#include "list.h"
#include "audio.h"
#include "metadata.h"
#include "action.h"
#include "button.h"
#include "kernel.h"
#include "misc.h"
#include "bmp.h"
#include "jpeg_load.h"
#include "albumart.h"
#include "albumlist_art.h"
#include "rbunicode.h"
#include "strlcpy.h"
#include "tagcache.h"
#include <stdio.h>
#include <string.h>

#define TV_MAX_W 426
#define TV_H 240
#define TV_ART_MAX 180
static uint16_t canvas[TV_MAX_W * TV_H];
static union { uint32_t align; unsigned char bytes[98304]; } art_storage;
static struct bitmap art;
static char art_track[MAX_PATH];
static bool art_ready, art_attempted;
static long art_deadline;
static int cw, scale, line_h, art_requested_size;
static int text_size = -1;
static enum tv_section section = TV_MUSIC;
static bool wps_owner;
static bool guide_owner, guide_building, guide_has_picture;
static struct mutex guide_mutex;
static bool guide_mutex_ready;
static bool home_navigation, home_tab_focus;
/* One narrow destination strip, never a playback-owned allocation. */
#define TV_TRANSITION_ROWS 32
static uint16_t transition_strip[TV_MAX_W * TV_TRANSITION_ROWS];
static uint32_t transition_key;
static bool transition_valid;
void tv_ui_set_home(bool active, bool tabs)
{ home_navigation=active; home_tab_focus=active && tabs; }
static const struct bitmap *app_logo;
static unsigned app_color;
void tv_ui_set_brand(const struct bitmap *logo, unsigned color)
{ app_logo=logo; app_color=color; }
bool tv_ui_active(void)
{ return videoout_active() && global_settings.tv_interface; }
bool tv_ui_remote_volume_locked(void)
{ return tv_ui_active() && (home_navigation || guide_owner); }
void tv_ui_set_section(enum tv_section value)
{ if ((unsigned)value < TV_SECTION_COUNT) section = value; }
void tv_ui_release(void)
{
    transition_valid=false;
    videoout_prepare_frame(NULL);
    tv_guide_render(TV_GUIDE_RELEASE,0,0,0,0,0,NULL);
    tv_ui_set_brand(NULL,0);
    tv_ui_set_home(false,false);
    if (!wps_owner) { videoout_ui_owner(false); videoout_ui_batch(false); }
}
static struct videoout_rect safe;
static const unsigned bg = LCD_RGBPACK(0, 0, 0);

static void rect(int x, int y, int w, int h, unsigned color)
{
    int right = MIN(cw, x + w), bottom = MIN(TV_H, y + h);
    for (int j = MAX(0, y); j < bottom; j++)
        for (int i = MAX(0, x); i < right; i++)
            canvas[j*cw+i] = color;
}
static void frame(int x, int y, int w, int h, unsigned color)
{
    rect(x,y,w,2,color); rect(x,y+h-2,w,2,color);
    rect(x,y,2,h,color); rect(x+w-2,y,2,h,color);
}
/* Blend only source pixels: all chrome comes from the existing Apple cache. */
static unsigned blend(unsigned back, unsigned front, unsigned alpha)
{
    unsigned r = (((back >> 11) & 31) * (255-alpha) +
                  ((front >> 11) & 31)*alpha + 127)/255;
    unsigned g = (((back >> 5) & 63) * (255-alpha) +
                  ((front >> 5) & 63)*alpha + 127)/255;
    unsigned b = ((back & 31) * (255-alpha) +
                  (front & 31)*alpha + 127)/255;
    return (r<<11)|(g<<5)|b;
}
static bool transition_allowed(void)
{
    return videoout_active() && button_queue_empty() && !button_hold() &&
           tagcache_is_usable() && !tagcache_commit_active();
}
/* Only the finished strip is touched. All text/art callbacks have returned
 * before this bounded, interruptible presentation loop starts. */
static void present_transition(const char *title, int selection, int kind,
                               int x, int y, int width, int height)
{
    const unsigned char *p=(const unsigned char *)(title ?
        P2STR((const unsigned char *)title) : "");
    uint32_t key=2166136261u;
    while (*p) key=(key^*p++)*16777619u;
    key=(key^(unsigned)selection)*16777619u;
    key=(key^(unsigned)kind)*16777619u;
    key=(key^(unsigned)section)*16777619u;
    key=(key^(unsigned)(home_navigation*2+home_tab_focus))*16777619u;
    key=(key^(unsigned)cw)*16777619u;
    key=(key^(unsigned)(safe.y*TV_H+y))*16777619u;
    key=(key^(unsigned)height)*16777619u;
    key=(key^(unsigned)(x*TV_MAX_W+width))*16777619u;
    bool changed=transition_valid && transition_key!=key;
    transition_key=key;
    transition_valid=true;
    /* Home selection follows each wheel/remote event immediately. A 160 ms
     * decorative fade here delays the next input even with cached icons. */
    if (home_navigation || !changed || x<0 || width<=0 || x+width>cw || y<0 ||
        height<=0 || height>TV_TRANSITION_ROWS ||
        y+height>TV_H || !transition_allowed())
    {
        videoout_present_ui(canvas,cw,TV_H);
        return;
    }
    size_t row_bytes=width*sizeof(*canvas);
    for (int row=0;row<height;row++)
        memcpy(transition_strip+row*width,canvas+(y+row)*cw+x,row_bytes);
    unsigned duration=MAX(1,HZ*160/1000);
    unsigned start=current_tick;
    while (transition_allowed())
    {
        unsigned elapsed=(unsigned)current_tick-start;
        if (elapsed>=duration) break;
        unsigned t=elapsed*256/duration;
        unsigned ease=t*t*(768-2*t)>>16;
        unsigned alpha=179+76*ease/256;
        for (int row=0;row<height;row++)
            for (int col=0;col<width;col++)
                canvas[(y+row)*cw+x+col]=
                    blend(bg,transition_strip[row*width+col],alpha);
        videoout_present_ui(canvas,cw,TV_H);
        elapsed=(unsigned)current_tick-start;
        if (elapsed>=duration) break;
        sleep(MIN((unsigned)MAX(1,HZ/25),duration-elapsed));
    }
    for (int row=0;row<height;row++)
        memcpy(canvas+(y+row)*cw+x,transition_strip+row*width,row_bytes);
    videoout_present_ui(canvas,cw,TV_H);
}
/* RGB565 bilinear sampling also serves the slanted artwork and reflection. */
static unsigned cover_sample(const struct bitmap *bm, unsigned sx, unsigned sy)
{
    const fb_data *pixels=(const fb_data *)bm->data;
    int x=MIN((int)(sx>>8),bm->width-1);
    int y=MIN((int)(sy>>8),bm->height-1);
    int nx=MIN(x+1,bm->width-1), ny=MIN(y+1,bm->height-1);
    unsigned a=blend(pixels[y*bm->width+x],pixels[y*bm->width+nx],sx&255);
    unsigned b=blend(pixels[ny*bm->width+x],pixels[ny*bm->width+nx],sx&255);
    return blend(a,b,sy&255);
}
/* Filter color with coverage, not transparent RGB: Apple's glow padding may
 * contain invisible color. Quantize once at the final RGB565 destination.
 * The largest accumulator is 63 * 255 * 65536, within 32 bits. */
static unsigned asset_sample(unsigned back,
                  const struct ipodjs_retailos_image *im,
                  unsigned sx, unsigned sy, unsigned opacity)
{
    unsigned x=sx>>8, y=sy>>8;
    unsigned nx=MIN(x+1,(unsigned)im->width-1);
    unsigned ny=MIN(y+1,(unsigned)im->height-1);
    unsigned fx=sx&255, fy=sy&255;
    unsigned weights[4]={(256-fx)*(256-fy),fx*(256-fy),
                         (256-fx)*fy,fx*fy};
    const unsigned char *pixels[4]={
        im->pixels+(y*im->width+x)*3,
        im->pixels+(y*im->width+nx)*3,
        im->pixels+(ny*im->width+x)*3,
        im->pixels+(ny*im->width+nx)*3};
    unsigned red=0, green=0, blue=0, coverage=0;
    const unsigned total=255*65536;
    for (int k=0;k<4;k++)
    {
        unsigned pixel=pixels[k][0]|(pixels[k][1]<<8);
        unsigned weight=weights[k]*((pixels[k][2]*opacity+127)/255);
        coverage+=weight;
        red+=((pixel>>11)&31)*weight;
        green+=((pixel>>5)&63)*weight;
        blue+=(pixel&31)*weight;
    }
    red= (red+((back>>11)&31)*(total-coverage)+total/2)/total;
    green= (green+((back>>5)&63)*(total-coverage)+total/2)/total;
    blue= (blue+(back&31)*(total-coverage)+total/2)/total;
    return (red<<11)|(green<<5)|blue;
}
static void asset_into_alpha(uint16_t *destination, int dw, int dh,
                  const struct ipodjs_retailos_image *im,
                  int x, int y, int w, int h, unsigned opacity)
{
    if (!im || !im->pixels || !im->width || !im->height ||
        w<=0 || h<=0 || !opacity) return;
    /* Preserve native-size glyphs/icons exactly. Compute scaled coordinate
     * increments once, avoiding division in the destination pixel loop. */
    bool native=w==im->width && h==im->height;
    unsigned dx=w>1 ? ((im->width-1)<<16)/(w-1) : 0;
    unsigned dy=h>1 ? ((im->height-1)<<16)/(h-1) : 0;
    for (int j=MAX(0,-y); j<h && y+j<dh; j++)
        for (int i=MAX(0,-x); i<w && x+i<dw; i++)
        {
            int dest=(y+j)*dw+x+i;
            if (native)
            {
                const unsigned char *p=im->pixels+(j*im->width+i)*3;
                destination[dest]=blend(destination[dest],p[0]|(p[1]<<8),
                                         (p[2]*opacity+127)/255);
            }
            else
                destination[dest]=asset_sample(destination[dest],im,
                                                (i*dx)>>8,(j*dy)>>8,
                                                opacity);
        }
}
static void asset_into(uint16_t *destination, int dw, int dh,
                       const struct ipodjs_retailos_image *im,
                       int x, int y, int w, int h)
{ asset_into_alpha(destination,dw,dh,im,x,y,w,h,255); }
/* Native cached pixels only. Fit artwork without changing its aspect. */
static void bitmap_region(const struct bitmap *bm, int x, int y, int dw, int dh,
                          int source_x, int source_y, int sw, int sh)
{
    const fb_data *pixels=(const fb_data *)bm->data;
    for (int j=0;j<dh;j++) for(int i=0;i<dw;i++)
    {
        /* Filter directly from resident source pixels. This is especially
         * useful for the 80px app masters and 384px album covers; no decode,
         * allocation or storage access is allowed in this renderer. */
        unsigned sx = dw > 1 ? i * ((sw-1) << 8) / (dw-1) : 0;
        unsigned sy = dh > 1 ? j * ((sh-1) << 8) / (dh-1) : 0;
        int x0=source_x+(sx>>8), y0=source_y+(sy>>8);
        int x1=MIN(x0+1,bm->width-1), y1=MIN(y0+1,bm->height-1);
        unsigned fx=sx&255, fy=sy&255;
        fb_data p[4]={pixels[y0*bm->width+x0],pixels[y0*bm->width+x1],
                      pixels[y1*bm->width+x0],pixels[y1*bm->width+x1]};
        fb_data nearest=p[(fy>=128?2:0)+(fx>=128?1:0)];
        if (nearest==LCD_RGBPACK(255,0,255)) continue;
        unsigned weights[4]={(256-fx)*(256-fy),fx*(256-fy),(256-fx)*fy,fx*fy};
        unsigned red=0,green=0,blue=0;
        for(int k=0;k<4;k++)
        {
            if(p[k]==LCD_RGBPACK(255,0,255)) p[k]=nearest;
            red+=FB_UNPACK_RED(p[k])*weights[k];
            green+=FB_UNPACK_GREEN(p[k])*weights[k];
            blue+=FB_UNPACK_BLUE(p[k])*weights[k];
        }
        rect(x+i,y+j,1,1,LCD_RGBPACK((red+32768)>>16,
                                    (green+32768)>>16,(blue+32768)>>16));
    }
}
static void bitmap_fit(const struct bitmap *bm, int x, int y, int w, int h)
{
    if (!bm || !bm->data || bm->width<=0 || bm->height<=0 || w<=0 || h<=0) return;
    int dw=w, dh=w*bm->height/bm->width;
    if (dh>h) { dh=h; dw=h*bm->width/bm->height; }
    if (dw<=0 || dh<=0) return;
    bitmap_region(bm,x+(w-dw)/2,y+(h-dh)/2,dw,dh,0,0,bm->width,bm->height);
}
/* Fill the top shelf without stretching the original library artwork. */
static void bitmap_fill(const struct bitmap *bm, int x, int y, int w, int h)
{
    if (!bm || !bm->data || bm->width<=0 || bm->height<=0 || w<=0 || h<=0) return;
    int sw=bm->width, sh=MAX(1,sw*h/w);
    if (sh>bm->height) { sh=bm->height; sw=MAX(1,sh*w/h); }
    bitmap_region(bm,x,y,w,h,(bm->width-sw)/2,(bm->height-sh)/3,sw,sh);
}
/* Retain Apple's end caps and repeat/stretch only the supplied middle. */
static void parts_into(uint16_t *destination, int dw, int dh,
                  const struct ipodjs_retailos_image *im,
                  int x, int y, int w, int h)
{
    if (!im || w<(int)(im[0].width+im[2].width)) return;
    asset_into(destination,dw,dh,&im[0],x,y,im[0].width,h);
    asset_into(destination,dw,dh,&im[1],x+im[0].width,y,w-im[0].width-im[2].width,h);
    asset_into(destination,dw,dh,&im[2],x+w-im[2].width,y,im[2].width,h);
}
static void asset(const struct ipodjs_retailos_image *im,
                  int x, int y, int w, int h)
{ asset_into(canvas,cw,TV_H,im,x,y,w,h); }
static void parts(const struct ipodjs_retailos_image *im,
                  int x, int y, int w, int h)
{ parts_into(canvas,cw,TV_H,im,x,y,w,h); }
static int text_height(void)
{
    return font_get(ipodjs_ui_tv_font(text_size<0?global_settings.tv_text_size:text_size))->height;
}
static void begin(void)
{
    cw = global_settings.tv_screen ? TV_MAX_W : 320;
    scale = 1;
    line_h = text_height()+4;
    struct videoout_geometry g;
    videoout_calc_geometry(cw, TV_H, global_settings.tv_screen ? 16 : 4,
        global_settings.tv_screen ? 9 : 3, cw, TV_H,
        (struct videoout_rect){0,0,cw,TV_H}, global_settings.tv_screen,
        false, global_settings.tv_overscan, true, &g);
    safe = g.safe;
    rect(0,0,cw,TV_H,bg);
    if (!app_color) asset(tv_apple_background,0,0,cw,safe.y+24);
}
static struct font *character_font(ucschar_t ch, int *factor)
{
    (void)ch;
    *factor=1;
    return font_get(ipodjs_ui_tv_font(text_size<0?global_settings.tv_text_size:text_size));
}
static void text_offset(int x, int y, int width, const char *str,
                        unsigned color, int offset)
{
    const unsigned char *p = (const unsigned char *)(str ? P2STR((const unsigned char *)str) : "");
    int left = x, limit = MIN(cw, x + width);
    x -= offset;
    while (*p)
    {
        ucschar_t ch;
        p = utf8decode(p, &ch);
        int factor;
        struct font *f = character_font(ch, &factor);
        int w = font_get_width(f, ch);
        if (x >= limit) break;
        const unsigned char *bits = font_get_bits(f, ch);
        for (unsigned row = 0; row < f->height; row++)
            for (int col = 0; col < w; col++)
            {
                unsigned alpha;
                if (f->depth)
                {
                    unsigned pixel=row*w+col;
                    alpha=255-((bits[pixel/2] >> ((pixel&1)*4))&15)*17;
                }
                else alpha=(bits[(row/8)*w+col] & (1u<<(row%8)))?255:0;
                int px=x+col*factor, py=y+row*factor;
                if (alpha && px>=left && px<limit && px>=0 &&
                    py>=0 && py<TV_H)
                    canvas[py*cw+px]=blend(canvas[py*cw+px],color,alpha);
            }
        x += w*factor;
    }
}
static void text(int x, int y, int width, const char *str, unsigned color)
{
    text_offset(x,y,width,str,color,0);
}
static int text_width(const char *str)
{
    const unsigned char *p=(const unsigned char *)str;
    int pixels=0;
    while (*p)
    {
        ucschar_t ch;
        p=utf8decode(p,&ch);
        int factor;
        struct font *f=character_font(ch,&factor);
        pixels+=font_get_width(f,ch)*factor;
    }
    return pixels;
}
static void scrolling_text_color(int x, int y, int width, const char *str,
                                 unsigned color)
{
    if (!str) return;
    int pixels=text_width(str);
    int offset=0;
    if (pixels>width)
    {
        int phase=(current_tick/(HZ/10))%(pixels+40);
        offset=MAX(0,phase-40);
    }
    text_offset(x,y,width,str,color,offset);
}
static void scrolling_text(int x, int y, int width, const char *str)
{ scrolling_text_color(x,y,width,str,LCD_WHITE); }
/* Home alone has the category bar; app headers start at the same safe edge.
 * Apple's original BackRow
 * graphics are compiled read-only; no file loads, font loads or heap work. */
static int header_height(void)
{ return (home_navigation ? 28 : 0) +
         (app_logo && !home_navigation ? 44 :
          font_get(ipodjs_ui_tv_font(0))->height + 8); }
static int header(const char *title)
{
    static const char * const names[] = { "Apps", "Videos", "Music", "Settings" };
    text_size=0;
    int navigation=home_navigation ? 28 : 0;
    for (int i=0;home_navigation && i<TV_SECTION_COUNT;i++)
    {
        int x=safe.x+i*safe.w/TV_SECTION_COUNT;
        int right=safe.x+(i+1)*safe.w/TV_SECTION_COUNT;
        int tabw=right-x;
        if (i==(int)section)
        {
            /* One original BackRow glow, without a second list selection
             * laid over it. The four labels keep equal centered columns. */
            asset(home_tab_focus ? tv_apple_tab : tv_apple_tab_inactive,
                  x-6,safe.y-2,tabw+12,28);
        }
        int width=MIN(text_width(names[i]),tabw-4);
        text(x+(tabw-width)/2,safe.y+(22-text_height())/2,
             width,names[i],i==(int)section ? LCD_WHITE :
                  LCD_RGBPACK(173,181,190));
    }
    int title_x=safe.x+6;
    if (app_color)
    {
        int h=header_height()-navigation;
        /* Brand color reaches the TV edge; content retains the safe inset. */
        rect(0,0,cw,safe.y+navigation+h,app_color);
        if (app_logo)
        {
            /* Keep the 91x42 original logo at its native readable size. */
            bitmap_fit(app_logo,safe.x+2,safe.y+navigation+1,91,h-2);
            title_x+=98;
        }
    }
    int content_y=safe.y+navigation+
        (header_height()-navigation-text_height())/2;
    int x=safe.x+safe.w-18;
    if (audio_status() & AUDIO_STATUS_PLAY)
    {
        asset(audio_status()&AUDIO_STATUS_PAUSE?tv_apple_pause:tv_apple_play,
              x,content_y,16,16);
        x-=20;
        if(global_settings.playlist_shuffle)
        { asset(tv_apple_shuffle,x,content_y,16,16); x-=20; }
        if(global_settings.repeat_mode)
        { asset(tv_apple_repeat,x,content_y,16,16); x-=20; }
    }
    text(title_x,content_y,MAX(0,x-title_x),title,
         home_navigation ? LCD_RGBPACK(173,181,190) : LCD_WHITE);
    text_size=-1;
    return safe.y+header_height()+5;
}
/* Suppress intermediate handheld mirror frames while a semantic TV frame is
 * being composed. The LCD itself still updates; no extra TV handoff is added. */
void tv_ui_batch(bool start, bool wps)
{
    videoout_ui_batch(start && videoout_active() &&
        (wps ? global_settings.tv_now_playing : global_settings.tv_interface));
}
void tv_wps_enter(void)
{
    /* Hold ownership across the skin wait, scroll callbacks and dirty LCD
     * refreshes; a short batch must never release it. */
    wps_owner=videoout_active() && global_settings.tv_now_playing;
    videoout_ui_owner(wps_owner);
}
/* Mirror the handheld stock WPS projection, not the cover's lettering.
 * Match its 10px slant and 50px reflection per 136px cover. Sample only
 * resident artwork into the existing canvas; no new buffer or decoder. */
static void album_cover_project(const struct bitmap *cover,
                                int x, int y, int width, int available_h)
{
    if (!cover || !cover->data || cover->width<=0 || cover->height<=0)
        return;
    int w=width, h=w*cover->height/cover->width;
    int max_h=available_h*136/186;
    if (h>max_h) { h=max_h; w=h*cover->width/cover->height; }
    if (w<2 || h<2) return;
    x+=(width-w)/2;
    int depth=MAX(2,h*50/136);
    y+=MAX(0,(available_h-h-depth)/2);
    int slant=h*10/136;
    for (int dx=0;dx<w;dx++)
    {
        unsigned top=(w-1-dx)*slant*256/(w-1);
        unsigned height=h*256-top;
        unsigned sx=dx*((cover->width-1)<<8)/(w-1);
        unsigned step=((unsigned)(cover->height-1)<<16)/
                      MAX(256u,height-256);
        for (int dy=top>>8;dy<h;dy++)
        {
            unsigned position=MAX(0,dy*256-(int)top);
            unsigned sy=(uint64_t)position*step>>8;
            unsigned pixel=cover_sample(cover,sx,sy);
            unsigned coverage=dy==(int)(top>>8) ? 255-(top&255) : 255;
            rect(x+dx,y+dy,1,1,blend(bg,pixel,coverage));
        }
        for (int dy=0;dy<depth;dy++)
        {
            unsigned distance=dy*step;
            unsigned sy=MAX(0,((cover->height-1)<<8)-(int)distance);
            unsigned alpha=112-dy*112/(depth-1);
            rect(x+dx,y+h+dy,1,1,
                 blend(bg,cover_sample(cover,sx,sy),alpha));
        }
        /* A vertical 1:2:1 kernel attenuates single-line detail before the
         * interlaced output. Preserve original neighbours while working
         * in place; labels and focus chrome remain sharp. */
        int px=x+dx;
        if (px<0 || px>=cw) continue;
        int first=MAX(0,y+(int)(top>>8));
        int last=MIN(TV_H,y+h+depth);
        unsigned previous=bg;
        for (int row=first;row<last;row++)
        {
            unsigned current=canvas[row*cw+px];
            unsigned next=row+1<last ? canvas[(row+1)*cw+px] : bg;
            canvas[row*cw+px]=blend(blend(previous,next,128),current,128);
            previous=current;
        }
    }
}
void tv_list_art_draw(struct gui_synclist *list, const struct bitmap *cover)
{
    if (!videoout_active() || !global_settings.tv_interface ||
        !list || !list->callback_get_item_name)
        return;
    begin();
    int top = header(list->title);
    int listw=safe.w;
    bool music_pane=cover || (home_navigation && section == TV_MUSIC);
    if (!cover && music_pane)
        cover=ipodjs_ui_retailos_music_cover();
    /* Album titles share the canvas with a cover; use the resident smaller
     * Apple face instead of letting TV's largest text consume that pane. */
    if (music_pane) text_size=0;
    if (music_pane)
    {
        /* Equal columns keep the cover centered in the right half instead
         * of anchoring its pane to the screen's outer edge. */
        int gutter=16;
        listw=(safe.w-gutter)/2;
        int pane=safe.w-listw-gutter;
        if (cover)
            album_cover_project(cover,safe.x+listw+gutter,top,pane,
                                safe.y+safe.h-top-4);
    }
    /* Keep the original BackRow focus caps clear of adjacent baselines. */
    int row_h = MAX(27, text_height()+8);
    int rows = MAX(1,(safe.y+safe.h-top-4)/row_h);
    videoout_ui_owner(true);
    int start = MAX(0, list->selected_item - rows/2);
    start = MIN(start, MAX(0,list->nb_items-rows));
    for (int row=0; row<rows && start+row<list->nb_items; row++)
    {
        char buf[256];
        int item = start+row, y=top+row*row_h;
        const char *name = list->callback_get_item_name(item, list->data,
                                                        buf, sizeof(buf));
        if (item == list->selected_item && !home_tab_focus)
        {
            if (app_color) rect(safe.x,y,listw,row_h,app_color);
            else parts(tv_apple_selection,safe.x,y,listw,row_h);
        }
        text(safe.x+10,y+(row_h-text_height())/2,listw-20,name,
             home_tab_focus ? LCD_RGBPACK(173,181,190) : LCD_WHITE);
    }
    text_size=-1;
    int focus_y=top+(list->selected_item-start)*row_h;
    present_transition(list->title,list->selected_item,1,safe.x,
        home_tab_focus || row_h>TV_TRANSITION_ROWS ? safe.y : focus_y,
        home_tab_focus ? safe.w : listw,
        home_tab_focus || row_h>TV_TRANSITION_ROWS ? 24 : row_h);
}
void tv_list_draw(struct gui_synclist *list)
{
    const struct bitmap *cover=NULL;
#ifdef HAVE_TAGCACHE
    cover=albumlist_tv_cover_cached(list);
#endif
    tv_list_art_draw(list,cover);
}

void tv_grid_draw(struct gui_synclist *list, const struct bitmap * const *icons,
                  int first, int columns)
{
    if (!tv_ui_active() || !list || !list->callback_get_item_name) return;
    columns=MAX(2,MIN(3,columns));
    begin();
    int top=header(list->title);
    int cellw=safe.w/columns, cellh=(safe.y+safe.h-top-18)/2;
    text_size=0;
    for(int i=0;i<columns*2 && first+i<list->nb_items;i++)
    {
        int x=safe.x+(i%columns)*cellw, y=top+(i/columns)*cellh;
        bool selected=first+i==list->selected_item && !home_tab_focus;
        char buf[128];
        const char *name=list->callback_get_item_name(first+i,list->data,buf,sizeof(buf));
        int width=MIN(cellw-18,text_width(name));
        int focusw=MIN(cellw-6,width+18);
        if(selected) parts(tv_apple_selection,x+(cellw-focusw)/2,
                           y+cellh-text_height()-7,focusw,text_height()+6);
        bitmap_fit(icons?icons[i]:NULL,x+8,y+4,
                   cellw-16,cellh-text_height()-13);
        text(x+(cellw-width)/2,y+cellh-text_height()-5,width,name,LCD_WHITE);
    }
    char footer[64];
    snprintf(footer,sizeof(footer),"%d of %d",list->nb_items?list->selected_item+1:0,list->nb_items);
    int fw=MIN(safe.w-8,text_width(footer));
    text(safe.x+(safe.w-fw)/2,safe.y+safe.h-text_height(),fw,footer,
         LCD_RGBPACK(160,166,174));
    text_size=-1;
    videoout_ui_owner(true);
    present_transition(list->title,list->selected_item,2,
        home_tab_focus ? safe.x :
        safe.x+(list->selected_item-first)%columns*cellw,
        home_tab_focus ? safe.y :
        top+((list->selected_item-first)/columns+1)*cellh-24,
        home_tab_focus ? safe.w : cellw,24);
}

/* The Videos top shelf advertises cached library artwork above a single
 * row of apps, following the modern Apple TV Home layout. */
void tv_home_videos_draw(struct gui_synclist *list,
                        const struct bitmap * const *icons,
                        const struct bitmap *banner, const char *title,
                        bool focused, int selected, int count)
{
    if (!tv_ui_active() || !list || !list->callback_get_item_name) return;
    begin();
    int top=header("Featured"), bottom=safe.y+safe.h;
    text_size=0;
    int apph=64, imageh=bottom-top-apph-8;
    if (banner)
    {
        bitmap_fill(banner,safe.x,top,safe.w,imageh);
        /* A soft scrim keeps the title readable over the real banner. */
        int shadeh=MIN(imageh,text_height()+14);
        for (int j=0;j<shadeh;j++)
            for (int x=safe.x;x<safe.x+safe.w;x++)
            {
                int offset=(top+imageh-shadeh+j)*cw+x;
                canvas[offset]=blend(canvas[offset],bg,210*j/shadeh);
            }
        text(safe.x+6,top+imageh-text_height()-3,safe.w-12,title,LCD_WHITE);
    }
    if (focused) frame(safe.x,top,safe.w,imageh,LCD_WHITE);
    if (count>1)
    {
        int step=MIN(8,(safe.w-12)/count);
        int first=safe.x+(safe.w-(count-1)*step)/2;
        for (int i=0;i<count;i++)
        {
            int radius=i==selected ? 2 : 1;
            unsigned color=i==selected ? LCD_WHITE : LCD_RGBPACK(116,120,126);
            for (int dy=-radius;dy<=radius;dy++)
                for (int dx=-radius;dx<=radius;dx++)
                    {
                        int distance=dx*dx+dy*dy;
                        unsigned alpha=radius==1 ? (distance==2 ? 90 : 255) :
                            distance<=2 ? 255 : distance==4 ? 210 : distance==5 ? 65 : 0;
                        int px=first+i*step+dx, py=top+imageh+4+dy;
                        rect(px,py,1,1,blend(canvas[py*cw+px],color,alpha));
                    }
        }
    }
    if (!banner && count>0)
        text(safe.x+6,top+imageh-text_height()-3,safe.w-12,title,LCD_WHITE);
    int cellw=safe.w/4, y=bottom-apph;
    for (int i=0;i<4 && i<list->nb_items;i++)
    {
        int x=safe.x+i*cellw;
        bool selected=i==list->selected_item && !home_tab_focus && !focused;
        char buf[128];
        const char *name=list->callback_get_item_name(i,list->data,buf,sizeof(buf));
        int width=MIN(cellw-4,text_width(name));
        int focusw=MIN(cellw-2,width+10);
        if (selected) parts(tv_apple_selection,x+(cellw-focusw)/2,
            bottom-text_height()-6,focusw,text_height()+6);
        bitmap_fit(icons?icons[i]:NULL,x+5,y+4,
                   cellw-10,apph-text_height()-12);
        text(x+(cellw-width)/2,bottom-text_height()-4,width,name,LCD_WHITE);
    }
    text_size=-1;
    videoout_ui_owner(true);
    present_transition("Featured",focused ? selected : list->selected_item,
        focused ? 4 : 3,
        home_tab_focus || focused ? safe.x : safe.x+list->selected_item*cellw,
        home_tab_focus || focused ? safe.y : bottom-24,
        home_tab_focus || focused ? safe.w : cellw,24);
}

/* Paint a small reflection from the completed canvas, not another art cache. */
static void reflection(int x, int y, int w, int h, int depth)
{
    for (int row=0; row<depth && y+h+2+row<safe.y+safe.h; row++)
        for (int col=MAX(x,safe.x); col<MIN(x+w,safe.x+safe.w); col++)
        {
            int source_y=y+h-1-row;
            if (source_y<0 || source_y>=TV_H) continue;
            canvas[(y+h+2+row)*cw+col]=blend(bg,
                canvas[source_y*cw+col],70*(depth-row)/depth);
        }
}

/* Reuse Netflix's cached checkmark at the actual fitted poster corner. */
static void shelf_watched(const struct bitmap *poster,
                          const struct bitmap *badge,
                          int x, int y, int w, int h)
{
    if (!poster || !badge || poster->width<=0 || poster->height<=0) return;
    int dw=w, dh=w*poster->height/poster->width;
    if (dh>h) { dh=h; dw=h*poster->width/poster->height; }
    int size=MIN(16,MIN(dw,dh)-4);
    bitmap_fit(badge,x+(w-dw)/2+dw-size-2,y+(h-dh)/2+2,size,size);
}
void tv_shelf_draw(const char *title, const char *name,
                   const struct bitmap * const *posters,
                   const struct bitmap *banner, const char *footer,
                   int selected, int count, unsigned watched,
                   const struct bitmap *watched_badge)
{
    if (!tv_ui_active()) return;
    begin();
    int top=header(title)+5, bottom=safe.y+safe.h;
    int titleh=text_height();
    int bodyh=bottom-top-titleh-34;
    /* Show each title's synced banner alongside its poster. Without a banner,
     * keep adjacent posters visible; no fabricated replacement artwork. */
    if (banner)
    {
        int posterw=safe.w/4;
        bitmap_fit(banner,safe.x+2,top,safe.w-posterw-12,bodyh);
        bitmap_fit(posters?posters[1]:NULL,safe.x+safe.w-posterw,top,posterw,bodyh);
        if (posters && (watched&2))
            shelf_watched(posters[1],watched_badge,
                          safe.x+safe.w-posterw,top,posterw,bodyh);
    }
    else if(posters)
    {
        int cell=safe.w/3;
        for(int i=0;i<3;i++)
        {
            int h=i==1?bodyh:bodyh*4/5;
            int w=MIN(cell-14,h*2/3);
            /* Fit the real artwork before placing the border/reflection.
             * Landscape thumbnails and square covers must not sit inside
             * an empty portrait frame. Keep a common shelf baseline. */
            const struct bitmap *poster=posters[i];
            if (poster && poster->width>0 && poster->height>0)
            {
                int fitted_h=w*poster->height/poster->width;
                if (fitted_h>h) w=h*poster->width/poster->height;
                else h=fitted_h;
            }
            int x=safe.x+i*cell+(cell-w)/2, y=top+bodyh-h;
            if (i==1 && posters[i]) frame(x-3,y-3,w+6,h+6,LCD_WHITE);
            bitmap_fit(posters[i],x,y,w,h);
            reflection(x,y,w,h,7);
            if (watched&(1u<<i))
                shelf_watched(posters[i],watched_badge,x,y,w,h);
        }
    }
    int width=MIN(safe.w-12,text_width(name?name:""));
    scrolling_text(safe.x+(safe.w-width)/2,top+bodyh+13,width,name);
    text_size=0;
    char position[32];
    snprintf(position,sizeof(position),"%d / %d",count?selected+1:0,count);
    int positionw=text_width(position);
    text(safe.x+4,bottom-text_height(),MAX(0,safe.w-positionw-18),footer,
         LCD_RGBPACK(173,181,190));
    text(safe.x+safe.w-positionw-4,bottom-text_height(),positionw,position,
         LCD_RGBPACK(173,181,190));
    text_size=-1;
    videoout_ui_owner(true);
    present_transition(name,selected,5,safe.x,safe.y,safe.w,24);
}

void tv_detail_draw(const char *title, const struct bitmap *poster,
                    const struct bitmap *banner, const char *metadata, const char *plot, const char *footer, int choice)
{
    if(!tv_ui_active()) return;
    begin();
    int top=header(title), bottom=safe.y+safe.h;
    top+=5;
    int pw=safe.w/3;
    bitmap_fit(poster,safe.x+2,top,pw-6,bottom-top-30);
    int x=safe.x+pw+10, width=safe.w-pw-14;
    text_size=0;
    if (banner)
    {
        int bh=MIN(65,(bottom-top-42)/2);
        bitmap_fit(banner,x,top,width,bh);
        top+=bh+7;
    }
    text(x,top,width,metadata,LCD_RGBPACK(173,181,190));
    const unsigned char *p=(const unsigned char *)(plot?plot:"");
    for(int y=top+text_height()+5;*p && y+text_height()<bottom-22;y+=text_height()+2)
    {
        char line[128]; int n=0,pixels=0,word=-1;
        const unsigned char *start=p;
        while(*p && n<(int)sizeof(line)-5)
        {
            ucschar_t ch; const unsigned char *next=utf8decode(p,&ch);
            int factor; struct font *f=character_font(ch,&factor);
            int w=font_get_width(f,ch);
            if(pixels+w>width) break;
            if(ch==' ') word=n;
            while(p<next) line[n++]=*p++;
            pixels+=w;
        }
        if(*p && word>0) { n=word; p=start+n; }
        if(n==0) break;
        line[n]=0; text(x,y,width,line,LCD_WHITE);
        while(*p==' ') p++;
    }
    text_size=0;
    int actions=choice>=0 ? 2 : 1;
    int buttonw=(safe.w-(actions-1)*8)/actions;
    for (int i=0;i<actions;i++)
    {
        int x=safe.x+i*(buttonw+8);
        bool selected=choice<0 || choice==i;
        const char *label=choice<0 ? footer : i==0 ? "Resume" : "Start Over";
        if (app_color)
            rect(x,bottom-23,buttonw,23,selected ? app_color : LCD_RGBPACK(38,38,38));
        else if (selected) parts(tv_apple_selection,x,bottom-23,buttonw,23);
        int actionw=MIN(buttonw-12,text_width(label));
        text(x+(buttonw-actionw)/2,bottom-20,actionw,label,
             selected ? LCD_WHITE : LCD_RGBPACK(173,181,190));
    }
    text_size=-1;
    videoout_ui_owner(true);
    present_transition(title,choice,6,safe.x+MAX(0,choice)*(buttonw+8),
                       bottom-24,buttonw,24);
}

void tv_dialog_draw(const char *title, const char * const *lines, int count,
                    const char *footer)
{
    if (!videoout_active() || !global_settings.tv_interface) return;
    begin();
    int y=header(title);
    int bottom=safe.y+safe.h-line_h-4;
    for (int i=0;i<count && y+line_h<=bottom;i++)
    {
        const unsigned char *p=(const unsigned char *)P2STR((const unsigned char *)lines[i]);
        int chars=MAX(1,(safe.w-12)/font_get(ipodjs_ui_tv_font(text_size<0?global_settings.tv_text_size:text_size))->maxwidth);
        while (p && *p && y+line_h<=bottom)
        {
            char line[256];
            int bytes=0,n=0;
            while (*p && n<chars && bytes<(int)sizeof(line)-5)
            {
                ucschar_t ch;
                const unsigned char *end=utf8decode(p,&ch);
                while(p<end) line[bytes++]=*p++;
                n++;
            }
            line[bytes]=0;
            text(safe.x+6,y,safe.w-12,line,LCD_WHITE);
            y+=line_h;
        }
    }
    parts(tv_apple_selection,safe.x,safe.y+safe.h-line_h,safe.w,line_h);
    text(safe.x+6,safe.y+safe.h-line_h+3,safe.w-12,footer,LCD_WHITE);
    present_transition(footer,count,7,safe.x,safe.y,safe.w,24);
}

void tv_ui_leave(void)
{
    transition_valid=false;
    tv_ui_set_brand(NULL,0);
    tv_ui_set_home(false,false);
    wps_owner=false;
    videoout_ui_owner(false);
    videoout_ui_batch(false);
    art_requested_size = 0;
    art_ready = art_attempted = false;
    art_track[0] = 0;
}
/* One attempt per track. Work runs after an idle WPS action timeout only.
 * A changed track clears validity BEFORE decode and is checked again after it.
 * Fixed decode space cannot shrink playback's audio buffer. */
void tv_wps_service(bool idle)
{
    struct mp3entry *id3 = audio_current_track();
    if (!videoout_active() || !global_settings.tv_now_playing || !id3 ||
        !(audio_status() & AUDIO_STATUS_PLAY))
    { tv_ui_leave(); return; }
    struct videoout_geometry layout;
    int virtual_width=global_settings.tv_screen?TV_MAX_W:320;
    videoout_calc_geometry(virtual_width,TV_H,global_settings.tv_screen?16:4,
        global_settings.tv_screen?9:3,virtual_width,TV_H,
        (struct videoout_rect){0,0,virtual_width,TV_H},global_settings.tv_screen,
        false,global_settings.tv_overscan,true,&layout);
    int needed=MIN(180,MIN(layout.safe.h-(24+text_height()+6)-32,
                           (layout.safe.w-24)/2));
    needed=MAX(32,needed);
    if (strcmp(id3->path,art_track) || art_requested_size != needed)
    {
        art_requested_size=needed;
        strlcpy(art_track,id3->path,sizeof(art_track));
        art_ready=art_attempted=false;
        art_deadline=current_tick+HZ;
    }
    if (!idle || !button_queue_empty() || button_hold())
    { art_deadline=current_tick+HZ; return; }
    if (art_attempted || TIME_BEFORE(current_tick,art_deadline) ||
        !tagcache_is_usable() || tagcache_commit_active()) return;
    art_attempted=true;
    char path[MAX_PATH];
    struct dim size = {needed,needed};
    if (!find_albumart(id3,path,sizeof(path),&size)) return;
    memset(&art,0,sizeof(art));
    art.width=art.height=needed;
    art.data=art_storage.bytes;
    int format=FORMAT_NATIVE|FORMAT_RESIZE|FORMAT_KEEP_ASPECT;
    int rc=read_bmp_file(path,&art,sizeof(art_storage.bytes),format,NULL);
#ifdef HAVE_JPEG
    if (rc<0)
    {
        art.width=art.height=needed;
        rc=read_jpeg_file(path,&art,sizeof(art_storage.bytes),format,NULL);
    }
#endif
    id3=audio_current_track();
    art_ready=rc>0 && id3 && !strcmp(id3->path,art_track) &&
        art.width>0 && art.height>0 && art.width<=TV_ART_MAX &&
        art.height<=TV_ART_MAX;
}
void tv_wps_draw(void)
{
    if (!videoout_active() || !global_settings.tv_now_playing) return;
    struct mp3entry *id3=audio_current_track();
    if (!id3 || !(audio_status() & AUDIO_STATUS_PLAY))
    { tv_ui_leave(); return; }
    tv_wps_enter();
    begin();
    section=TV_MUSIC;
    int top=header("Now Playing");
    int box=MIN(180,MIN(safe.y+safe.h-top-46,(safe.w-24)/2));
    box=MAX(32,box);
    int ax=safe.x+4;
    int ay=top;
    if (art_ready && !strcmp(id3->path,art_track))
    {
        int w=box,h=box;
        if (art.width>art.height) h=box*art.height/art.width;
        else w=box*art.width/art.height;
        bitmap_fit(&art,ax,ay,box,box);
        reflection(ax+(box-w)/2,ay+(box-h)/2,w,h,10);
    }
    else
    {
        /* Original Apple TV shelf artwork; compiled and already resident. */
        asset(tv_apple_music,ax,ay,box,box);
        reflection(ax,ay,box,box,10);
    }

    int tx=ax+box+10;
    int title_h=text_height();
    text_size=0;
    int detail_h=text_height()+4;
    int ty=ay+MAX(0,(box-title_h-8-2*detail_h)/2);
    text_size=-1;
    int tw=safe.x+safe.w-tx-4;
    scrolling_text(tx,ty,tw,id3->title?id3->title:id3->path);
    text_size=0;
    scrolling_text_color(tx,ty+title_h+8,tw,id3->artist,
                         LCD_RGBPACK(200,204,210));
    scrolling_text_color(tx,ty+title_h+8+detail_h,tw,id3->album,
                         LCD_RGBPACK(173,181,190));
    text_size=-1;
    int bottom=safe.y+safe.h;
    int bar_y=bottom-28;
    int bx=safe.x+4;
    int bw=safe.w-8;
    parts(tv_apple_progress,bx,bar_y,bw,7);
    if (id3->length)
        parts(tv_apple_fill,bx,bar_y,
            (int)((uint64_t)bw*MIN(id3->elapsed,id3->length)/id3->length),7);
    char status[32];
    unsigned long elapsed=id3->length ? MIN(id3->elapsed,id3->length) :
                                       id3->elapsed;
    unsigned long remaining=id3->length ? id3->length-elapsed : 0;
    snprintf(status,sizeof(status),"%lu:%02lu",
        elapsed/60000,elapsed/1000%60);
    text_size=0;
    text(bx,bottom-text_height(),bw,status,LCD_WHITE);
    if (id3->length)
        snprintf(status,sizeof(status),"-%lu:%02lu",
            remaining/60000,remaining/1000%60);
    else
        strlcpy(status,"--:--",sizeof(status));
    int statusw=MIN(bw,text_width(status));
    text(bx+bw-statusw,bottom-text_height(),statusw,status,
         LCD_RGBPACK(173,181,190));
    text_size=-1;
    videoout_present_ui(canvas,cw,TV_H);
}
/* Keep the supplied receiver header geometry, but give the schedule the
 * remaining safe height for TV readability. Six schedule rows, channel
 * logos, selection and live-video ownership retain their existing contract. */
static int guide_ref_x(int x) { return safe.x+x*safe.w/398; }
static int guide_ref_y(int y) { return safe.y+y*safe.h/300; }
static int guide_x(int x)
{
    int reference=x<68 ? 18+x*58/68 : 76+(x-68)*322/252;
    return guide_ref_x(reference);
}
static int guide_y(int y)
{
    int reference;
    if (y<26) reference=y*58/26;
    else if (y<41) reference=58+(y-26)*22/15;
    else if (y<77) reference=80+(y-41)*50/36;
    else if (y<91) reference=130+(y-77)*19/14;
    else if (y<223) reference=149+(y-91)*138/132;
    else reference=287+(y-223)*13/17;
    return guide_ref_y(reference);
}
static struct videoout_rect guide_picture_rect(void)
{
    return (struct videoout_rect){guide_ref_x(266),guide_ref_y(18),
        guide_ref_x(368)-guide_ref_x(266),
        guide_ref_y(87)-guide_ref_y(18)};
}
static void guide_fill(int x, int y, int w, int h, unsigned color)
{
    bool banner=y>=0 && y+h<=26;
    /* Receiver chrome fills the TV; only content is inset for overscan. */
    int right=x+w>=320 ? cw : guide_x(x+w);
    int bottom=guide_y(y+h);
    if (y>=26 && y<41 && x==0 && w==78)
        right=guide_ref_x(76);
    if (y>=26 && y<41 && x==78 && w==1)
    {
        x=guide_ref_x(76); right=guide_ref_x(77);
    }
    else if (y>=223 && w<=6 && (x==112 || x==158 || x==206))
    {
        int reference=x==112 ? 210 : x==158 ? 258 : 306;
        right=guide_ref_x(reference+8);
        x=guide_ref_x(reference);
    }
    else x=x<=0 && y<77 ? 0 : guide_x(x);
    y=y<=0 ? 0 : guide_y(y);
    /* Preserve the live picture while the guide chrome is redrawn. */
    struct videoout_rect picture=guide_picture_rect();
    int px=picture.x, py=picture.y;
    int pr=px+picture.w, pb=py+picture.h;
    for (int j=MAX(0,y); j<MIN(TV_H,bottom); j++)
        for (int i=MAX(0,x); i<MIN(cw,right); i++)
            if (!guide_has_picture || i<px || i>=pr || j<py || j>=pb)
            {
                if (banner)
                {
                    unsigned sx=MIN(MAX(0,i-safe.x),safe.w-1)*
                        ((tv_directv_header.width-1)<<8)/MAX(1,safe.w-1);
                    unsigned sy=MIN(MAX(0,j-safe.y),guide_y(26)-safe.y-1)*
                        ((tv_directv_header.height-1)<<8)/
                        MAX(1,guide_y(26)-safe.y-1);
                    canvas[j*cw+i]=asset_sample(color,&tv_directv_header,
                                                sx,sy,255);
                }
                else canvas[j*cw+i]=color;
            }
}
static unsigned glyph_alpha(const struct font *f, const unsigned char *bits,
                            int width, unsigned x, unsigned y)
{
    unsigned pixel=y*width+x;
    return f->depth ? 255-((bits[pixel/2]>>((pixel&1)*4))&15)*17 :
        ((bits[(y/8)*width+x]&(1u<<(y%8)))?255:0);
}
static void guide_text(int x, int y, int w, int source_height,
                       const char *str, unsigned color)
{
    struct font *f=font_get(ipodjs_ui_tv_font(0));
    (void)source_height;
    int text_h=16, right=MIN(guide_x(x+w),safe.x+safe.w);
    int dx=guide_x(x), dy=guide_y(y);
    /* Foreground values are sampled from the original receiver screenshot.
     * Keep distinct metadata, programme, selected and channel-label inks. */
    if (y<26) color=TV_DIRECTV_TITLE;
    else if (y<41) color=TV_DIRECTV_METADATA;
    else if (y<77) color=TV_DIRECTV_BODY;
    else if (y>=223 || (y>=91 && x<68)) color=TV_DIRECTV_CHANNEL;
    else if (color==LCD_RGBPACK(16,37,74)) color=TV_DIRECTV_SELECTED;
    else if (color==LCD_WHITE) color=TV_DIRECTV_ROW;
    if (y<26)
    {
        /* Logo and guide wordmark are already in the original header. */
        if (x<70 || (str && !strcmp(str,"guide"))) return;
        dx=guide_ref_x(82); dy=guide_ref_y(36);
        right=guide_ref_x(261); text_h=18;
    }
    else if (y<41)
    {
        text_h=14; dy=guide_ref_y(61);
        if (x<70) { dx=guide_ref_x(18); right=guide_ref_x(74); }
        else if (x<172) { dx=guide_ref_x(84); right=guide_ref_x(186); }
        else { dx=guide_ref_x(190); right=guide_ref_x(261); }
    }
    else if (y<77)
    {
        dx=guide_ref_x(18);
        right=guide_ref_x(guide_has_picture && dy<guide_ref_y(89) ?
                          261 : 396);
    }
    else if (y<91)
    {
        int edge=x<68 ? 66 : 68+((x-68)/84+1)*84-2;
        right=MIN(right,guide_x(edge));
        dy=guide_ref_y(130); text_h=x<68 ? 16 : 19;
    }
    else if (y<223)
    {
        dy=guide_ref_y(150+((y-91)/22)*23);
        text_h=21;
    }
    else
    {
        dy=guide_ref_y(287); text_h=14;
        if (x==121) { dx=guide_ref_x(220); right=guide_ref_x(251); }
        else if (x==167) { dx=guide_ref_x(268); right=guide_ref_x(301); }
        else if (x==215) { dx=guide_ref_x(316); right=guide_ref_x(396); }
    }
    int h=MAX(8,text_h*safe.h/300);
    /* Schedule glyphs use their natural width; the compact receiver header
     * retains its existing metrics and original artwork. */
    int width_percent=y>=77 && y<223 ? 100 : 90;
    x=dx; y=dy;
    char fitted[128];
    const unsigned char *p=(const unsigned char *)str;
    int used=0, pixels=0, last=0;
    int dots=3*MAX(1,font_get_width(f,'.')*h*width_percent/((int)f->height*100));
    while (p && *p)
    {
        ucschar_t ch;
        const unsigned char *next=utf8decode(p,&ch);
        int bytes=next-p, dw=MAX(1,font_get_width(f,ch)*h*width_percent/((int)f->height*100));
        if (pixels+dw>right-x || used+bytes>=(int)sizeof(fitted)-4)
        {
            memcpy(fitted+last,"...",4); used=last+3; break;
        }
        memcpy(fitted+used,p,bytes); used+=bytes; pixels+=dw;
        if (pixels+dots<=right-x) last=used;
        p=next;
    }
    fitted[used]=0;
    p=(const unsigned char *)fitted;
    while (*p && x<right)
    {
        ucschar_t ch; p=utf8decode(p,&ch);
        int sourcew=font_get_width(f,ch), dw=MAX(1,sourcew*h*width_percent/((int)f->height*100));
        const unsigned char *bits=font_get_bits(f,ch);
        if (x+dw>right) break;
        /* Integrate four coverage samples from the resident font instead
         * of dropping rows/columns when fitting the small receiver text.
         * Pixel-center coordinates retain thin stems at 10 and 12 px. */
        for (int row=0;row<h;row++) for (int col=0;col<dw;col++)
        {
            unsigned alpha=0;
            for (int yy=0;yy<2;yy++) for (int xx=0;xx<2;xx++)
            {
                unsigned sx=(4*col+1+2*xx)*sourcew/(4*dw);
                unsigned sy=(4*row+1+2*yy)*f->height/(4*h);
                alpha+=glyph_alpha(f,bits,sourcew,sx,sy);
            }
            alpha=(alpha+2)/4;
            if (y+row>=safe.y && y+row<safe.y+safe.h && x+col>=safe.x)
            {
                int offset=(y+row)*cw+x+col;
                canvas[offset]=blend(canvas[offset],color,alpha);
            }
        }
        x+=dw;
    }
}
bool tv_guide_render(enum tv_guide_command command, int x, int y,
                     int width, int height, unsigned color, const void *data)
{
    if (command==TV_GUIDE_RELEASE)
    {
        if (guide_mutex_ready) mutex_lock(&guide_mutex);
        bool owned=guide_owner;
        if (owned) videoout_ui_owner(false);
        guide_owner=false;
        if (guide_mutex_ready) mutex_unlock(&guide_mutex);
        return owned;
    }
    if (command==TV_GUIDE_PRESENT && guide_building)
    {
        if (guide_has_picture)
        {
            struct videoout_rect picture=guide_picture_rect();
            frame(picture.x-2,picture.y-2,picture.w+4,picture.h+4,LCD_WHITE);
        }
        if (tv_ui_active()) videoout_present_ui(canvas,cw,TV_H);
        guide_building=false;
        mutex_unlock(&guide_mutex);
        return true;
    }
    if (!tv_ui_active()) return false;
    if (command==TV_GUIDE_BEGIN)
    {
        if (!guide_mutex_ready) { mutex_init(&guide_mutex); guide_mutex_ready=true; }
        mutex_lock(&guide_mutex);
        guide_building=true;
        if (!guide_owner)
        {
            begin();
            rect(0,0,cw,TV_H,LCD_RGBPACK(2,111,175));
        }
        guide_has_picture=color!=0;
        guide_owner=true;
        videoout_prepare_frame(NULL);
        videoout_ui_owner(true);
        return true;
    }
    if (!guide_owner) return false;
    if (command==TV_GUIDE_PICTURE)
    {
        /* The decoder never waits for a guide redraw. Its next normal frame
         * refreshes the preview; stream timing and audio are untouched. */
        if (guide_building || !guide_has_picture || !data) return false;
        mutex_lock(&guide_mutex);
        const struct tv_guide_picture *p=data;
        if (guide_owner && p->planes[0] && p->planes[1] && p->planes[2] && p->width>1 && p->height>1 && p->stride>=p->width && !(p->stride&1))
        {
            struct videoout_rect picture=guide_picture_rect();
            int bx=picture.x, by=picture.y, bw=picture.w, bh=picture.h;
            frame(bx-2,by-2,bw+4,bh+4,LCD_WHITE);
            rect(bx,by,bw,bh,bg);
            /* Preserve the source picture's aspect inside the receiver PIG. */
            int dar_n=p->dar_n>0 ? p->dar_n : p->width;
            int dar_d=p->dar_d>0 ? p->dar_d : p->height;
            int dw=MIN(bw,bh*dar_n/dar_d), dh=MIN(bh,dw*dar_d/dar_n);
            bx+=(bw-dw)/2; by+=(bh-dh)/2;
            for (int j=0;j<dh;j++) for(int i=0;i<dw;i++)
            {
                int sx=i*p->width/dw, sy=j*p->height/dh;
                int yy=p->planes[0][sy*p->stride+sx]-16;
                int u=p->planes[1][(sy/2)*(p->stride/2)+sx/2]-128;
                int v=p->planes[2][(sy/2)*(p->stride/2)+sx/2]-128;
                int red=(298*yy+409*v+128)>>8;
                int green=(298*yy-100*u-208*v+128)>>8;
                int blue=(298*yy+516*u+128)>>8;
                canvas[(by+j)*cw+bx+i]=LCD_RGBPACK(MAX(0,MIN(255,red)),
                    MAX(0,MIN(255,green)),MAX(0,MIN(255,blue)));
            }
            videoout_present_ui(canvas,cw,TV_H);
        }
        mutex_unlock(&guide_mutex);
        return true;
    }
    if (!guide_building) return false;
    switch(command)
    {
        case TV_GUIDE_FILL: guide_fill(x,y,width,height,color); break;
        case TV_GUIDE_TEXT: guide_text(x,y,width,height,data,color); break;
        case TV_GUIDE_BITMAP:
            /* The header already carries the exact original logo. */
            if (x==4 && y<26) break;
            if (x<68 && y>=91 && y<223)
            {
                /* Preserve the user's actual channel logos in the narrower
                 * reference column; never substitute call signs or new art. */
                int row=(y-91)/22;
                bitmap_fit(data,guide_ref_x(20),guide_ref_y(150+row*23),
                           guide_ref_x(73)-guide_ref_x(20),
                           guide_ref_y(171+row*23)-guide_ref_y(150+row*23));
            }
            else bitmap_fit(data,guide_x(x),guide_y(y),
                       guide_x(x+width)-guide_x(x),guide_y(y+height)-guide_y(y));
            break;
        case TV_GUIDE_PARAGRAPH:
        {
            const char *p=data;
            char line[128];
            struct font *f=font_get(ipodjs_ui_tv_font(0));
            int rows=3;
            for (int row=0; row<rows && p && *p; row++)
            {
                int edge=guide_has_picture &&
                    guide_y(y+row*11)<guide_ref_y(89) ? 261 : 396;
                int available=guide_ref_x(edge)-guide_ref_x(18);
                int length=0, pixels=0, space=0;
                while (p[length] && length<(int)sizeof(line)-1)
                {
                    ucschar_t ch;
                    const unsigned char *next=utf8decode((const unsigned char *)p+length,&ch);
                    int bytes=next-((const unsigned char *)p+length);
                    pixels+=MAX(1,font_get_width(f,ch)*MAX(8,16*safe.h/300)*90/
                                ((int)f->height*100));
                    if (pixels>available ||
                        length+bytes>=(int)sizeof(line)) break;
                    if (ch==' ') space=length;
                    memcpy(line+length,p+length,bytes); length+=bytes;
                }
                if (p[length] && space>0) length=space;
                if (!length) break;
                line[length]=0;
                guide_text(x,y+row*11,width,8,line,color);
                p+=length; while (*p==' ') p++;
            }
            break;
        }
        default: break;
    }
    return true;
}

/* Separate small overlay canvas: video and main UI can run on different
 * threads. The status band contains no disk/font-cache work. */
static uint16_t video_status[TV_MAX_W * 32];
bool tv_video_prepare(const unsigned char * const planes[3],
    int width, int height, int stride, int dar_n, int dar_d,
    unsigned long elapsed_ms, unsigned long duration_ms,
    bool paused, bool show_status, const unsigned char *caption)
{
    if (!planes)
    { videoout_prepare_frame(NULL); return false; }
    /* Releasing native playback must not release a guide or modal canvas.
     * Only a new full-screen video presentation takes the guide's canvas. */
    tv_guide_render(TV_GUIDE_RELEASE,0,0,0,0,0,NULL);
    if (!videoout_active()) return false;
    int w=global_settings.tv_screen?TV_MAX_W:320;
    int inset=videoout_inset(global_settings.tv_overscan);
    int xmargin=((w*inset/100)&~1)+4;
    memset(video_status,0,sizeof(video_status));
    if(show_status)
    {
        for(int y=0;y<32;y++) for(int x=xmargin;x<w-xmargin;x++)
            video_status[y*w+x]=bg;
        char line[64];
        snprintf(line,sizeof(line),"%s %lu:%02lu / %lu:%02lu",
            paused?"Paused":"Playing",elapsed_ms/60000,elapsed_ms/1000%60,
            duration_ms/60000,duration_ms/1000%60);
        asset_into(video_status,w,32,tv_apple_background,0,0,w,24);
        int x=xmargin+4;
        for(const char *p=line;*p;p++)
        {
            const char *found=strchr(tv_apple_status_chars,*p);
            if (!found) continue;
            const struct ipodjs_retailos_image *glyph=
                &tv_apple_status_font[found-tv_apple_status_chars];
            if (x+glyph->width>w-xmargin) break;
            asset_into(video_status,w,32,glyph,x,3,glyph->width,glyph->height);
            x+=glyph->width;
        }
        int barw=w-2*xmargin;
        parts_into(video_status,w,32,tv_apple_progress,
                   xmargin,24,barw,7);
        int fill=duration_ms?(uint64_t)barw*MIN(elapsed_ms,duration_ms)/duration_ms:0;
        parts_into(video_status,w,32,tv_apple_fill,
                   xmargin,24,fill,7);
    }
    struct videoout_tv_frame f={.planes={planes[0],planes[1],planes[2]},
        .width=width,.height=height,.stride=stride,.dar_n=dar_n,.dar_d=dar_d,
        .strides={stride,stride/2,stride/2},
        .coded_width=stride,.coded_height=height,
        .visible={0,0,width,height},.format=VIDEOOUT_YUV420P,
        .color=VIDEOOUT_SD_LIMITED,.pts_ms=elapsed_ms,.duration_ms=0,
        .caption=caption,.caption_canvas_width=w,.caption_y=240-((240*inset/100)&~1)-44-4,
        .fill=global_settings.tv_fit,.overlay=show_status?video_status:NULL,
        .overlay_width=w,.overlay_y=(240*inset/100)&~1,.overlay_height=32};
    videoout_prepare_frame(&f);
    return true;
}

int tv_test_screen(void)
{
    lcd_set_viewport(NULL);
    lcd_clear_display();
    lcd_puts(0,0,"Composite geometry test");
    lcd_puts(0,2,"TV must match selected aspect.");
    lcd_puts(0,4,"Select: toggle 4:3 / 16:9");
    lcd_puts(0,5,"Menu: exit");
    lcd_update();
    while (true)
    {
        begin();
        frame(0,0,cw,TV_H,LCD_WHITE);
        frame(safe.x,safe.y,safe.w,safe.h,LCD_RGBPACK(0,255,0));
        for (int i=0;i<4;i++)
        {
            int x=i&1?cw-12:2,y=i&2?TV_H-12:2;
            rect(x,y,10,10,LCD_RGBPACK(255,255,0));
        }
        scale=1;
        text(safe.x+8,safe.y+6,safe.w-16,
             global_settings.tv_screen?"16:9 selected | 4:3 reference":
                                       "4:3 selected | 16:9 reference",LCD_WHITE);
        frame(cw/2-95,TV_H/2-35,70,70,LCD_WHITE);
        int cx=cw/2+50,cy=TV_H/2,r=35;
        for(int y=-r;y<=r;y++) for(int x=-r;x<=r;x++)
        {
            int d=x*x+y*y;
            if(d<=r*r && d>=(r-2)*(r-2)) rect(cx+x,cy+y,1,1,LCD_WHITE);
        }
        text(safe.x+8,safe.y+safe.h-48,safe.w-16,"TV-safe text: Aa 0123456789",LCD_WHITE);
        for(int i=0;i<8;i++)
        {
            int v=i*255/7;
            rect(safe.x+8+i*(safe.w-16)/8,safe.y+safe.h-30,
                 (safe.w-16)/8,10,LCD_RGBPACK(v,v,v));
            rect(safe.x+8+i*(safe.w-16)/8,safe.y+safe.h-18,
                 (safe.w-16)/8,10,LCD_RGBPACK(i&1?255:0,i&2?255:0,i&4?255:0));
        }
        videoout_present_ui(canvas,cw,TV_H);
        int a=get_action(CONTEXT_STD,HZ/5);
        if(a==ACTION_STD_CANCEL || a==ACTION_STD_MENU) break;
        if(a==ACTION_STD_OK)
        {
            global_settings.tv_screen=!global_settings.tv_screen;
            videoout_set_preferences(global_settings.tv_screen,
                                     global_settings.tv_overscan);
            settings_save();
        }
        if(default_event_handler(a)==SYS_USB_CONNECTED) break;
    }
    lcd_update();
    return 0;
}
#endif
