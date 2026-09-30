/* SPDX-License-Identifier: GPL-2.0-or-later
 * Bounded, opt-in RAM trace. IRQ-protected publication only; no allocation,
 * formatting, file I/O, or waiting in packet/button producers. */
#include "config.h"
#include "iap-remote-debug.h"
#include "iap-core.h"
#include "iap.h"
#include "system.h"
#include "kernel.h"
#include "lcd.h"
#include "font.h"
#include "action.h"
#include "settings.h"
#include "button.h"
#include "file.h"
#include "misc.h"
#include "rbpaths.h"
#include <string.h>
#include <stdio.h>
extern const char rbversion[];
#define TRACE_COUNT 128
struct remote_entry {
    long tick;
    unsigned len, buttons;
    uint32_t bitmap;
    int context, action;
    unsigned char raw[12];
    short volume;
    unsigned short policy;
    char state;
};
static struct remote_entry trace[TRACE_COUNT], snapshot[TRACE_COUNT];
static unsigned head, count;
static bool enabled;
static const char *export_status = "";
static void record(struct remote_entry *entry)
{
    if (!enabled) return;
    entry->volume = global_status.volume;
    entry->policy = (iap_remote_navigation_active() ? 1 : 0) |
                    (iap_remote_tv_active() ? 2 : 0) |
                    (iap_kokkia_present() ? 4 : 0) |
                    (iap_remote_input_suppressed() ? 8 : 0);
    int level=disable_irq_save();
    trace[head]=*entry;
    head=(head+1)%TRACE_COUNT;
    if (count<TRACE_COUNT) count++;
    restore_irq(level);
}
void iap_remote_packet(const unsigned char *raw, unsigned len,
                       uint32_t bitmap, unsigned buttons, char state)
{
    if (!enabled) return;
    struct remote_entry e={.tick=current_tick,.len=len,.bitmap=bitmap,
        .buttons=buttons,.context=-1,.action=-1,.state=state};
    if(raw) memcpy(e.raw,raw,MIN(len,sizeof(e.raw)));
    record(&e);
}
void iap_remote_action(unsigned button, int context, int action)
{
    if (!(button & BUTTON_REMOTE)) return;
    struct remote_entry e={.tick=current_tick,.buttons=button,
        .context=context,.action=action,.state='A'};
    record(&e);
}
void iap_remote_command(const unsigned char *raw, unsigned len)
{
    if (!enabled || !raw || len < 2)
        return;

    /* Keep all non-button commands, including requests outside known
     * control handlers, so an unrecognized Menu command cannot disappear
     * from the diagnostic. Lingo 2 already records its button packets. */
    unsigned off = device.auth.idps ? 2 : 0;
    if (raw[0] == 3 && raw[1] == 0x0e && len >= 3 + off)
        iap_remote_packet(raw, len, raw[2 + off], 0, 'C');
    else if (raw[0] == 4 && len >= 3 && raw[1] == 0 && raw[2] == 0x29)
        iap_remote_packet(raw, len, 0, 0, 'C');
    else if (raw[0] != 2)
        iap_remote_packet(raw, len, 0, 0, 'Q');
}
void iap_remote_filter(unsigned button, bool filtered, bool wake)
{
    if (!(button & BUTTON_REMOTE)) return;
    struct remote_entry e={.tick=current_tick,.buttons=button,
        .context=-1,.action=-1,.state=filtered?'F':wake?'W':'B'};
    record(&e);
}
static unsigned copy_trace(void)
{
    int level=disable_irq_save();
    unsigned n=count;
    for(unsigned i=0;i<n;i++)
        snapshot[i]=trace[(head+TRACE_COUNT-n+i)%TRACE_COUNT];
    restore_irq(level);
    return n;
}
static void export_trace(void)
{
    unsigned n=copy_trace();
    int fd=open(ROCKBOX_DIR "/iap-remote-trace.txt",O_WRONLY|O_CREAT|O_TRUNC,0666);
    if(fd<0) { export_status="Save failed"; return; }
    fdprintf(fd,"build=%s trace_version=3\n", rbversion);
    fdprintf(fd,"device=%08lx lingoes=%08lx idps=%d auth=%d kokkia=%d mode=%d wake=%d\n",
        (unsigned long)device.idps_deviceid,(unsigned long)device.lingoes,
        device.auth.idps,device.auth.state,iap_kokkia_present(),
        global_settings.dock_remote_mode,global_settings.remote_wake);
    fdprintf(fd,"policy bits: 1=navigation 2=TV 4=Kokkia 8=quarantine\n");
    fdprintf(fd,"ticks state len bitmap buttons context action policy volume raw(first12)\n");
    for(unsigned i=0;i<n;i++)
    {
        struct remote_entry *e=&snapshot[i];
        fdprintf(fd,"%ld %c %u %08lx %08x %d %d %x %d",
                 e->tick,e->state,e->len,(unsigned long)e->bitmap,
                 e->buttons,e->context,e->action,e->policy,e->volume);
        for(unsigned j=0;j<MIN(e->len,sizeof(e->raw));j++)
            fdprintf(fd," %02x",e->raw[j]);
        fdprintf(fd,"\n");
    }
    export_status=close(fd)<0?"Save failed":"Saved /.rockbox/iap-remote-trace.txt";
}
int iap_remote_debug_screen(void)
{
    enabled=true;
    int oldfont=lcd_getfont();
    lcd_set_viewport(NULL);
    lcd_setfont(FONT_SYSFIXED);
    while(true)
    {
        unsigned n=copy_trace();
        lcd_clear_display();
        lcd_putsf(0,0,"iAP trace %s (%u/%u)",enabled?"ON":"OFF",n,TRACE_COUNT);
        lcd_putsf(0,1,"ID %08lx lingoes %08lx",(unsigned long)device.idps_deviceid,
                   (unsigned long)device.lingoes);
        lcd_putsf(0,2,"IDPS %d auth %d tid %04x",device.auth.idps,
                   device.auth.state,device.ipod_trans_id);
        lcd_putsf(0,3,"Nav %d TV %d Kokkia %d vol %d",
                   iap_remote_navigation_active(),iap_remote_tv_active(),
                   iap_kokkia_present(),global_status.volume);
        for(unsigned i=0;i<MIN(n,8u);i++)
        {
            struct remote_entry *e=&snapshot[n-MIN(n,8u)+i];
            lcd_putsf(0,5+i,"%c len%u bits%08lx btn%08x",e->state,e->len,
                      (unsigned long)e->bitmap,e->buttons);
        }
        const struct remote_entry *packet=NULL,*action=NULL;
        for(unsigned i=0;i<n;i++)
        {
            if(snapshot[i].len) packet=&snapshot[i];
            if(snapshot[i].state=='A') action=&snapshot[i];
        }
        if(action) lcd_putsf(0,14,"ctx %d action %d",action->context,action->action);
        if(packet)
        {
            lcd_putsf(0,15,"%02x %02x %02x %02x %02x %02x %02x %02x",
                packet->raw[0],packet->raw[1],packet->raw[2],packet->raw[3],
                packet->raw[4],packet->raw[5],packet->raw[6],packet->raw[7]);
        }
        lcd_puts(0,17,export_status);
        lcd_puts(0,18,"Select: save trace | Right: clear");
        lcd_puts(0,19,"Left: toggle | Menu: save/exit");
        lcd_puts(0,21,"P press R release H held T timeout");
        lcd_puts(0,22,"A action C control V nav Q request");
        lcd_update();
        /* Raw local controls keep remote packets available for observation. */
        int b=button_get_w_tmo(HZ/5);
        if(b==BUTTON_MENU || b==BUTTON_RC_MENU)
        { export_trace(); break; }
        if(b==(BUTTON_SELECT|BUTTON_REL)) export_trace();
        if(b==BUTTON_LEFT) enabled=!enabled;
        if(b==BUTTON_RIGHT)
        {
            int level=disable_irq_save();head=count=0;restore_irq(level);
        }
        if(default_event_handler(b)==SYS_USB_CONNECTED) break;
    }
    lcd_setfont(oldfont);
    return 0;
}
