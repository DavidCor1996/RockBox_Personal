"""Execute native USB control paths with deterministic host events."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def compile_run(tmp_path, source):
    source_path = tmp_path / "usb.c"
    executable = tmp_path / "usb"
    source_path.write_text(source)
    subprocess.run(["cc", "-Wall", "-Wextra", "-Werror", str(source_path),
                    "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)


def test_native_hid_and_raw_events_keep_redrawing(tmp_path):
    src = (ROOT / "apps/gui/usb_screen.c").read_text()
    body = src.split("static int handle_usb_events(void)", 1)[1].split(
        "\n#endif /* SIMULATOR */", 1)[0]
    preamble='''
    #include <stdbool.h>
    #include <stddef.h>
    #include <assert.h>
    #define CONFIG_STORAGE 0
    #define STORAGE_MMC 1
    #define USB_ENABLE_HID
    #define HZ 100
    #define MAX(a,b) ((a)>(b)?(a):(b))
    #define SCREEN_MAIN 0
    #define SYS_USB_DISCONNECTED 100
    #define SYS_CHARGER_DISCONNECTED 101
    #define SYS_TIMEOUT -1
    #define ACTION_USB_HID_MODE_SWITCH_NEXT 200
    #define ACTION_USB_HID_MODE_SWITCH_PREV 201
    #define GUI_EVENT_ACTIONUPDATE 1
    static bool usb_hid, enabled=true;
    static int calls, event, timeout;
    static bool ipodjs_ui_enabled(int screen) { (void)screen; return enabled; }
    static bool ipodjs_ui_usb_animation_active(void) {return enabled;}
    static int get_hid_usb_action(int t) { assert(++calls<3); timeout=t; return event; }
    static int button_get_w_tmo(int t) { assert(++calls<3); timeout=t; return event; }
    static void send_event(int e, void *p) {(void)e; (void)p;}
    static void reset_runtime(void) {}
    '''
    main='''
    int main(void) {
     for (int hid=0;hid<2;hid++) {
      usb_hid=hid;
      for (int i=0;i<100;i++) {
       calls=0; event=hid?0:SYS_TIMEOUT;
       assert(handle_usb_events()==0);
       assert(calls==1 && timeout==HZ/12);
      }
      calls=0; event=42; assert(handle_usb_events()==0);
      calls=0; event=SYS_USB_DISCONNECTED; assert(handle_usb_events()==1);
     }
     return 0;
    }
    '''
    compile_run(tmp_path, preamble + "static int handle_usb_events(void)" + body + main)


def test_safe_eject_requires_flush_all_drives_and_csw(tmp_path):
    src = (ROOT / "firmware/usbstack/usb_storage.c").read_text()
    eject = src.split("        case SCSI_START_STOP_UNIT:", 1)[1].split(
        "        case SCSI_ALLOW_MEDIUM_REMOVAL:", 1)[0]
    completion = src.split("    if (eject_pending && dir == USB_DIR_IN", 1)[1].split(
        "\n    switch(state)", 1)[0]
    preamble = r'''
#include <assert.h>
#include <stdbool.h>
#define HAVE_STORAGE_FLUSH
#define CONFIG_STORAGE 1
#define STORAGE_ATA 1
#define STORAGE_NAND 2
#define HAVE_MULTIDRIVE
#define logf(...) ((void)0)
#define SENSE_MEDIUM_ERROR 3
#define ASC_WRITE_ERROR 4
#define UMS_STATUS_FAIL 1
#define UMS_STATUS_GOOD 0
#define USB_DIR_IN 1
#define WAITING_FOR_CSW_COMPLETION_OR_COMMAND 10
#define WAITING_FOR_CSW_COMPLETION 11
static bool ejected[2], safe_to_disconnect, eject_pending, skip_first;
static int flush_result, csw, flushes;
static struct { int sense_key, asc, ascq; } cur_sense_data;
struct command { unsigned char command_block[16]; };
static int storage_num_drives(void) {return 2;}
static int storage_flush(void) {flushes++; return flush_result;}
static void send_csw(int status) {csw=status;}
static void eject_command(int lun, int command) {
 struct command block = {{0}};
 struct command *cbw = &block;
 cbw->command_block[4] = command;
 switch (1) { case 1:
'''
    middle = '''
 }
}
static void complete(int dir, int status, int state) {
 if (eject_pending && dir == USB_DIR_IN'''
    main = r'''
}
int main(void) {
 /* A failed flush must not mark the drive ejected or show safety. */
 flush_result=-1;
 eject_command(0, 2);
 assert(csw==UMS_STATUS_FAIL && !ejected[0] && !eject_pending);
 complete(1, 0, 10); assert(!safe_to_disconnect);
 /* Ejecting one of two drives is insufficient. */
 flush_result=0;
 eject_command(0, 2);
 assert(ejected[0] && !eject_pending && csw==UMS_STATUS_GOOD);
 complete(1, 0, 10); assert(!safe_to_disconnect);
 eject_command(1, 2);
 assert(eject_pending && !safe_to_disconnect);
 /* Receiving the next command before CSW completion is not success. */
 complete(0, 0, 10); assert(!safe_to_disconnect && eject_pending);
 complete(1, -1, 11); assert(!safe_to_disconnect && !eject_pending);
 eject_command(1, 2);
 complete(1, 0, 11); assert(safe_to_disconnect && !eject_pending);
 /* A hidden first drive must not prevent ejecting exposed storage. */
 safe_to_disconnect=false; ejected[0]=false; ejected[1]=false;
 skip_first=true; eject_command(1, 2);
 complete(1, 0, 10); assert(safe_to_disconnect);
 /* Stop without eject, or an invalid control nibble, is not eject. */
 safe_to_disconnect=false; ejected[1]=false;
 eject_command(1, 0); complete(1, 0, 10);
 assert(!safe_to_disconnect && !ejected[1]);
 eject_command(1, 0x12); complete(1, 0, 10);
 assert(!safe_to_disconnect && !ejected[1]);
 assert(flushes==5);
 return 0;
}
'''
    compile_run(tmp_path, preamble + eject + middle + completion + main)
