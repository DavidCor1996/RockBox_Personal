#!/usr/bin/env python3
"""Compile focused host probes from the actual firmware functions.

These probes do not simulate controllers, PCM DMA, or storage hardware.
Usage: python3 tools/tests/test_6g_upstream_sync.py [source-root]
"""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]

def function(path, name):
    text = (ROOT / path).read_text()
    import re
    match = re.search(r'^(?:static )?(?:void|bool|int) ' + name + r'\([^;{}]*?\)\s*\{', text, re.M)
    assert match, name
    start = text.index('{', match.start())
    depth = 1
    end = start + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[match.start():end] + '\n'

COMMON = '''#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define logf(...) ((void)0)
/* Rockbox supplies a type-generic abs macro. */
#undef abs
#define abs(x) ((x)<0?-(x):(x))
#define MIN(a,b) ((a)<(b)?(a):(b))
'''

def probe(name, code):
    with tempfile.TemporaryDirectory() as tmp:
        src = Path(tmp) / 'probe.c'
        exe = Path(tmp) / 'probe'
        src.write_text(COMMON + code)
        subprocess.run(['cc', '-std=gnu99', '-Wall', '-Wextra', '-Werror', str(src), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    print(name + ': PASS')

p = 'firmware/usbstack/usb_audio.c'
probe('source packet cadence and frequency validation', '''
static const unsigned long hw_freq_sampr[] = {48000,44100,32000,22050};
static const int source_freq_indices[] = {2,1,0};
#define SOURCE_NUM_FREQ 3u
static int as_source_freq_idx, source_frac_num;
static bool source_streaming, source_freq_set_by_host;
struct usb_ctrlrequest { int wValue, bRequest, wLength; };
#define USB_AS_EP_CS_SAMPLING_FREQ_CTL 1
#define USB_AC_SET_CUR 1
#define USB_AC_GET_CUR 2
#define USB_CONTROL_STALL 1
#define USB_CONTROL_ACK 2
#define USB_CONTROL_RECEIVE 3
static unsigned char usb_buffer[3];
static int response;
static void usb_drv_control_response(int r, void *p, int n) {(void)p;(void)n;response=r;}
static unsigned long decode3(void *p) {unsigned char *b=p;return b[0]|b[1]<<8|b[2]<<16;}
static void encode3(void *p,unsigned long f) {unsigned char *b=p;b[0]=f;b[1]=f>>8;b[2]=f>>16;}
''' + ''.join(function(p, n) for n in ['source_frame_bytes', 'set_source_sampling_frequency', 'usb_audio_source_endpoint_request']) + '''
int main(void) {
 for(int i=0;i<3;i++) {
  as_source_freq_idx=i;source_frac_num=0;int total=0;
  for(int j=0;j<10000;j++) {int n=source_frame_bytes();assert(n<=192);total+=n;}
  assert(total==(int)hw_freq_sampr[i]*40);assert(source_frac_num==0);
 }
 struct usb_ctrlrequest req={256,USB_AC_SET_CUR,3};unsigned char data[3];
 encode3(data,44100);assert(usb_audio_source_endpoint_request(&req,data));assert(response==USB_CONTROL_ACK);
 source_frac_num=700;
 encode3(data,44101);usb_audio_source_endpoint_request(&req,data);
 assert(response==USB_CONTROL_STALL && as_source_freq_idx==1 && source_frac_num==700 && source_freq_set_by_host);
 req.bRequest=USB_AC_GET_CUR;usb_audio_source_endpoint_request(&req,NULL);assert(decode3(usb_buffer)==44100);
 req.bRequest=USB_AC_SET_CUR;req.wLength=2;usb_audio_source_endpoint_request(&req,data);assert(response==USB_CONTROL_STALL);
 set_source_sampling_frequency(22050);assert(as_source_freq_idx==2);
 return 0;
}
''')
p = 'firmware/usbstack/usb_storage.c'
probe('BOT residue and deferred-command state transitions', '''
#define USB_DIR_IN 128
#define USB_DIR_OUT 0
#define CONFIG_RTC 1
static enum {WAITING_FOR_COMMAND,WAITING_FOR_STORAGE,SENDING_RESULT,SENDING_FAILED_RESULT,SENDING_BLOCKS,RECEIVING_BLOCKS,RECEIVING_TIME} state;
static struct {unsigned int data_residue;} cur_cmd;
struct command_block_wrapper {int token;};
static unsigned char cbw_buffer[64];
static bool exclusive;
static int executions;
static bool usb_exclusive_storage(void) {return exclusive;}
static void handle_scsi_ready(struct command_block_wrapper *cbw) {(void)cbw;executions++;state=SENDING_RESULT;}
''' + ''.join(function(p, n) for n in ['handle_scsi', 'usb_storage_notify_event', 'account_data_transfer']) + '''
int main(void) {
 cur_cmd.data_residue=512;state=SENDING_RESULT;
 account_data_transfer(USB_DIR_OUT,0,128);assert(cur_cmd.data_residue==512);
 account_data_transfer(USB_DIR_IN,-1,128);assert(cur_cmd.data_residue==512);
 account_data_transfer(USB_DIR_IN,0,128);assert(cur_cmd.data_residue==384);
 account_data_transfer(USB_DIR_IN,0,999);assert(cur_cmd.data_residue==0);
 state=WAITING_FOR_COMMAND;cur_cmd.data_residue=512;
 account_data_transfer(USB_DIR_OUT,0,31);assert(cur_cmd.data_residue==512);
 handle_scsi((void*)cbw_buffer);assert(state==WAITING_FOR_STORAGE && executions==0);
 usb_storage_notify_event(0);assert(executions==0);
 exclusive=true;usb_storage_notify_event(0);assert(executions==1);
 usb_storage_notify_event(0);assert(executions==1);
 exclusive=false;handle_scsi((void*)cbw_buffer);state=WAITING_FOR_COMMAND;
 exclusive=true;usb_storage_notify_event(0);assert(executions==1);
 return 0;
}
''')
p = 'firmware/target/arm/s5l8702/ipod6g/storage_ata-6g.c'
probe('iFlash model detection', '''
static uint16_t identify_info[256];static bool ceata;
''' + function(p,'ata_is_iflash') + '''
int main(void) {
 const char *model="iFlash-Platform iPod Adapters            ";
 for(int i=0;i<20;i++) identify_info[27+i]=(unsigned char)model[2*i]<<8|(unsigned char)model[2*i+1];
 assert(ata_is_iflash());ceata=true;assert(!ata_is_iflash());ceata=false;
 identify_info[27]='X'<<8|'F';assert(!ata_is_iflash());return 0;
}
''')
p = 'apps/recorder/albumart.c'
s = (ROOT / p).read_text()
start = s.index('static char* strip_filename')
end = s.index('\n#ifndef PLUGIN', start)
probe('artwork fallback and sized lookup precedence', '''
#define MAX_PATH 260
#define USE_JPEG_COVER
struct mp3entry {const char *path,*album,*albumartist,*artist;};
static const char *files[8];static int count;
static bool file_exists(const char *p) {for(int i=0;i<count;i++)if(!strcmp(p,files[i]))return true;return false;}
static void strmemccpy(char *d,const char *s,size_t n) {if(n){size_t l=strlen(s);if(l>=n)l=n-1;memcpy(d,s,l);d[l]=0;}}
static void strip_extension(char *d,size_t n,const char *s) {strmemccpy(d,s,n);char *p=strrchr(d,'.');if(p)*p=0;}
static void fix_path_part(char *p,int a,int n) {(void)p;(void)a;(void)n;}
#define ROCKBOX_DIR "/.rockbox"
''' + s[start:end] + '''
int main(void) {
 struct mp3entry id={"/Artist/Album/song.mp3","album","artist","artist"};char buf[280];
 files[0]="/Artist/folder.jpg";count=1;
 assert(search_albumart_files(&id,"",buf,sizeof(buf)));assert(!strcmp(buf,files[0]));
 assert(!search_albumart_files(&id,".100x100",buf,sizeof(buf)));
 files[1]="/Artist/Album/folder.jpg";count=2;
 assert(search_albumart_files(&id,"",buf,sizeof(buf)));assert(!strcmp(buf,files[1]));
 files[2]="/Artist/Album/cover.jpg";count=3;
 assert(search_albumart_files(&id,"",buf,sizeof(buf)));assert(!strcmp(buf,files[2]));
 files[3]="/Artist/cover.100x100.bmp";count=4;
 assert(search_albumart_files(&id,".100x100",buf,sizeof(buf)));assert(!strcmp(buf,files[3]));
 count=1;files[0]="/folder.jpg";assert(!search_albumart_files(&id,"",buf,sizeof(buf)));
 return 0;
}
''')
p = 'firmware/usb.c'
probe('exclusive ownership repeated requests and acknowledgement epochs', '''
static bool exclusive_storage_requested,exclusive_storage_enabled,usb_host_present=true;
static unsigned int usb_broadcast_seqnum;
static int usb_num_acks_to_expect,broadcasts,acks=3,mounts,unmounts,notifications;
#define SYS_USB_CONNECTED 1
#define SYS_USB_DISCONNECTED 2
#define USB_ENABLE_STORAGE
#define USB_DRIVER_MASS_STORAGE 0
#define DEBUGF(...) ((void)0)
static int queue_broadcast(int e,int n) {(void)e;(void)n;broadcasts++;return acks+1;}
static void usb_slave_mode(bool on) {if(on)unmounts++;else mounts++;}
static void usb_signal_class_notify(int c,int n) {(void)c;(void)n;notifications++;}
''' + function(p,'usb_request_exclusive_storage') + function(p,'usb_release_exclusive_storage') + '''
int main(void) {
 usb_request_exclusive_storage();assert(broadcasts==1 && usb_num_acks_to_expect==3);
 unsigned int epoch=usb_broadcast_seqnum;
 usb_num_acks_to_expect=1;usb_request_exclusive_storage();
 assert(broadcasts==1 && usb_num_acks_to_expect==1 && usb_broadcast_seqnum==epoch);
 usb_release_exclusive_storage();assert(usb_num_acks_to_expect==0 && !exclusive_storage_requested);
 acks=0;usb_request_exclusive_storage();assert(exclusive_storage_enabled && unmounts==1 && notifications==1);
 usb_request_exclusive_storage();assert(unmounts==1);
 usb_release_exclusive_storage();assert(mounts==1 && !exclusive_storage_enabled);
 return 0;
}
''')
