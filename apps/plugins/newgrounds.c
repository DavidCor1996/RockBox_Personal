/***************************************************************************
 * Newgrounds library for iPod classic. Original games run in Flash Player.
 * Copyright (C) 2026, GPL-2.0-or-later
 ***************************************************************************/
#include "plugin.h"
#include "lib/ipodjs_retailos.h"
#include "lib/newgrounds_font.h"

#ifdef SIMULATOR
extern char *getenv(const char *);
#endif

#define NG_DIR ROCKBOX_DIR "/ipodjs/newgrounds"
#define SKULL_SWF ROCKBOX_DIR "/flash/skullkid/skullkid.swf"
#define WHITE LCD_RGBPACK(255,255,255)
#define BLACK LCD_RGBPACK(0,0,0)
#define GRAY LCD_RGBPACK(176,176,176)
#define PANEL LCD_RGBPACK(35,35,35)

/* Fixed storage: browsing never takes the music buffer. */
static unsigned char battery_data[26*13*25*3];
static struct ipodjs_retailos_image battery;
static fb_data cover_data[146*82], logo_data[146*28], tank_data[146*82], tank_icon_data[28*28];
static struct bitmap cover, logo, tank, tank_icon;
static int page, selected;

static void load_bitmap(const char *name, struct bitmap *bm, fb_data *data,
                        size_t size, int width, int height)
{
    char path[MAX_PATH];
    rb->snprintf(path,sizeof(path),NG_DIR "/%s.bmp",name);
    rb->memset(bm,0,sizeof(*bm));
    bm->data=(unsigned char *)data;
    if (rb->read_bmp_file(path,bm,size,FORMAT_NATIVE,NULL)<0 ||
        bm->width!=width || bm->height!=height)
        bm->data=NULL;
}

static void prepare(void)
{
    ipodjs_retailos_load_animation_rga(IPODJS_RETAILOS_STATUSBAR_BLACK_BATTERY,
        battery_data,sizeof(battery_data),&battery);
    load_bitmap("skullkid",&cover,cover_data,sizeof(cover_data),146,82);
    load_bitmap("wordmark",&logo,logo_data,sizeof(logo_data),146,28);
    load_bitmap("tank-icon",&tank_icon,tank_icon_data,sizeof(tank_icon_data),28,28);
    load_bitmap("tank",&tank,tank_data,sizeof(tank_data),146,82);
}

static void centered(const char *text,int left,int width,int y)
{
    int w,h;
    ng_measure(text,&w,&h);
    ng_putsxy(left+MAX(0,(width-w)/2),y,text);
}

static void draw_header(const char *title)
{
    for(int y=0;y<32;y++) {
        int c=50-y;
        rb->lcd_set_foreground(LCD_RGBPACK(c,c,c));
        rb->lcd_hline(0,319,y);
    }
    if(page==0 && logo.data) {
        if(tank_icon.data) rb->lcd_bitmap((fb_data *)tank_icon.data,5,2,28,28);
        rb->lcd_bitmap((fb_data *)logo.data,37,2,146,28);
    }
    else {
        rb->lcd_set_foreground(WHITE);
        ng_putsxy(9,9,title);
    }
    rb->lcd_set_foreground(LCD_RGBPACK(255,170,0));
    rb->lcd_hline(0,319,32);
    if(battery.pixels) {
        int level=rb->battery_level();
        int frame=MAX(0,MIN(22,level*22/100));
        ipodjs_retailos_blit_part(rb->screens[SCREEN_MAIN],&battery,
            0,frame*13,286,10,26,13);
    }
}

static void row(const char *text,int index)
{
    int y=144+index*23;
    ng_bold=true;
    if(index==selected) {
        for(int line=0;line<23;line++) {
            rb->lcd_set_foreground(LCD_RGBPACK(255,183-line*2,28));
            rb->lcd_hline(7,312,y+line);
        }
        rb->lcd_set_foreground(BLACK);
    } else {
        rb->lcd_set_foreground(LCD_RGBPACK(48,48,48));
        rb->lcd_fillrect(7,y,306,22);
        rb->lcd_set_foreground(WHITE);
    }
    int w,h;
    ng_measure(text,&w,&h);
    ng_putsxy(15,y+(23-h)/2,text);
    for(int i=0;i<4;i++) rb->lcd_vline(298+i,y+8+i,y+15-i);
    ng_bold=false;
}

static void bitmap(struct bitmap *bm,int x,int y)
{
    if(bm->data) rb->lcd_bitmap((fb_data *)bm->data,x,y,bm->width,bm->height);
}

static void draw(void)
{
    rb->lcd_set_viewport(NULL);
    rb->lcd_setfont(FONT_UI);
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_background(PANEL);
    rb->lcd_set_foreground(BLACK);
    rb->lcd_clear_display();
    rb->lcd_set_drawmode(DRMODE_FG);
    draw_header(page==2 ? "Controls" : page==3 ? "About Newgrounds" : "Newgrounds");
    if(page==2) {
        rb->lcd_set_foreground(WHITE);
        const char *const controls[]={"Previous / Next: Walk","Center: Chainsaw / Fire","Play / Pause: Duck","Menu: Return to Newgrounds"};
        for(int i=0;i<4;i++) ng_putsxy(10,52+i*27,controls[i]);
    } else if(page==3) {
        bitmap(&tank,87,43);
        rb->lcd_set_foreground(WHITE);
        centered("Everything, by Everyone.",0,320,135);
        rb->lcd_set_foreground(GRAY);
        centered("The Skull Kid by korded",0,320,162);
        centered("Original release - September 2002",0,320,183);
        centered("Unofficial iPod adaptation",0,320,213);
    } else {
        rb->lcd_set_foreground(LCD_RGBPACK(255,176,30));
        ng_putsxy(9,39,"GAMES");
        rb->lcd_set_foreground(GRAY);
        ng_putsxy(171,39,"Featured Classic");
        rb->lcd_set_foreground(LCD_RGBPACK(0,0,0));
        rb->lcd_drawrect(7,57,148,84);
        bitmap(&cover,8,58);
        rb->lcd_set_foreground(WHITE);
        ng_putsxy(167,64,"Skull Kid");
        rb->lcd_set_foreground(LCD_RGBPACK(255,176,30));
        ng_putsxy(167,85,"by korded");
        rb->lcd_set_foreground(GRAY);
        ng_putsxy(167,107,"2002 - Action");
        row("Play Skull Kid",0);
        row("Controls",1);
        row("About Newgrounds",2);
        rb->lcd_set_foreground(GRAY);
        centered("Everything, by Everyone.",0,320,222);
    }
    rb->lcd_update();
#ifdef SIMULATOR
    if(getenv("NEWGROUNDS_TEST")) {
        char path[MAX_PATH], hdr[48];
        unsigned char line[LCD_WIDTH*3];
        struct viewport *vp=*rb->screens[SCREEN_MAIN]->current_viewport;
        rb->snprintf(path,sizeof(path),NG_DIR "/screen-%d-%d.ppm",page,selected);
        int fd=rb->open(path,O_WRONLY|O_CREAT|O_TRUNC,0666);
        if(fd>=0) {
            int n=rb->snprintf(hdr,sizeof(hdr),"P6\n%d %d\n255\n",LCD_WIDTH,LCD_HEIGHT);
            rb->write(fd,hdr,n);
            for(int y=0;y<LCD_HEIGHT;y++) {
                for(int x=0;x<LCD_WIDTH;x++) {
                    fb_data px=*(fb_data *)vp->buffer->get_address_fn(x,y);
                    line[x*3]=FB_UNPACK_RED(px); line[x*3+1]=FB_UNPACK_GREEN(px); line[x*3+2]=FB_UNPACK_BLUE(px);
                }
                rb->write(fd,line,sizeof(line));
            }
            rb->close(fd);
        }
    }
#endif
}

enum plugin_status plugin_start(const void *parameter)
{
    (void)parameter;
    page=0;
    selected=0;
    rb->lcd_set_backdrop(NULL);
    prepare();
    rb->button_clear_queue();
    draw();
#ifdef SIMULATOR
    const char *test=getenv("NEWGROUNDS_TEST");
#endif
    for(;;) {
        int button;
#ifdef SIMULATOR
        if(test && *test) {
            char key=*test++;
            button=key=='s' ? BUTTON_SELECT : key=='d' ? BUTTON_SCROLL_FWD : key=='u' ? BUTTON_SCROLL_BACK : BUTTON_MENU;
        } else if(test) return PLUGIN_OK;
        else
#endif
        button=rb->button_get(true);
        int bare=button & ~(BUTTON_REPEAT|BUTTON_REL);
        if(button==SYS_USB_CONNECTED) return PLUGIN_USB_CONNECTED;
        if(button & BUTTON_REL) continue;
        if(bare==BUTTON_MENU) {
            if(page==0) return PLUGIN_OK;
            page=0;
        } else if(page==0 && (bare==BUTTON_SCROLL_FWD || bare==BUTTON_SCROLL_BACK)) {
            selected=(selected+(bare==BUTTON_SCROLL_FWD ? 1 : 2))%3;
        } else if(bare==BUTTON_SELECT && !(button & BUTTON_REPEAT) && page==0) {
            if(selected==0) {
                if(!rb->file_exists(SKULL_SWF)) rb->splash(HZ*2,"Skull Kid is not installed");
                else return rb->plugin_open(VIEWERS_DIR "/flashplayer.rock",SKULL_SWF);
            } else page=selected==1 ? 2 : 3;
        } else if(rb->default_event_handler(button)==SYS_USB_CONNECTED) return PLUGIN_USB_CONNECTED;
        draw();
    }
}
