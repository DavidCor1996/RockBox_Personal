#!/usr/bin/env python3
"""Compile the actual lingo-2 handler against bounded host stubs (ASan/UBSan)."""
import pathlib, re, subprocess, tempfile
root=pathlib.Path(__file__).resolve().parents[2]
source=(root/'apps/iap/iap-lingo2.c').read_text()
source=re.sub(r'^#include .*$','',source,flags=re.M)
buttons=(root/'firmware/target/arm/s5l8702/ipod6g/button-target.h').read_text()
buttons='\n'.join(re.findall(r'^#define BUTTON_RC_.*$',buttons,re.M))
stub=r'''
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#define CONFIG_TUNER 0
#define BUTTON_NONE 0
#define MIN(a,b) ((a)<(b)?(a):(b))
#define BIT_N(n) (1u<<(n))
#define AUDIO_STATUS_PLAY 1
#define AUDIO_STATUS_PAUSE 2
#define IAP_ACK_OK 0
#define IAP_ACK_BAD_PARAM 4
#define IAP_ACK_NO_AUTHEN 5
#define IAP_ACK_CMD_FAILED 6
#define DEVICE_LINGO_SUPPORTED(x) true
#define DEVICE_AUTHENTICATED true
static struct { struct { bool idps; } auth; } device;
static struct { bool playlist_shuffle; } global_settings;
static unsigned long iap_remotebtn;
static int iap_repeatbtn, iap_timeoutbtn;
static bool iap_btnshuffle, iap_btnrepeat, suppressed;
static unsigned tx[16],txlen,acks,shuffles,repeats;
#define IAP_TX_INIT(a,b) do {txlen=0;tx[txlen++]=(a);tx[txlen++]=(b);}while(0)
#define IAP_TX_PUT(x) do {tx[txlen++]=(x);}while(0)
static void iap_send_tx(void){acks++;}
static int audio_status(void){return AUDIO_STATUS_PLAY;}
static bool iap_kokkia_present(void){return false;}
static void iap_note_kokkia_peer_connection(void){}
static bool iap_remote_input_suppressed(void){return suppressed;}
static void iap_shuffle_state(bool b){(void)b;shuffles++;}
static void iap_repeat_next(void){repeats++;}
static void iap_remote_packet(const unsigned char*p,unsigned n,uint32_t b,unsigned k,char e)
{(void)p;(void)n;(void)b;(void)k;(void)e;}
static int disable_irq_save(void){return 0;}
static void restore_irq(int v){(void)v;}
'''
tests=r'''
static void send_bits(uint32_t bits,int tid)
{
    unsigned char p[8]={2,0}; unsigned off=tid?4:2;
    device.auth.idps=tid;
    if(tid){p[2]=0xff;p[3]=0xff;}
    for(unsigned i=0;i<4;i++)p[off+i]=bits>>(i*8);
    iap_handlepkt_mode2(off+4,p);
}
int main(void)
{
    for(int tid=0;tid<2;tid++)
    {
        send_bits(0,tid);iap_repeatbtn=0;
        send_bits(2|(0x40u<<16),tid);
        assert(iap_remotebtn==(BUTTON_RC_VOL_UP|BUTTON_RC_MENU));
        assert(iap_repeatbtn==2 && iap_timeoutbtn==3);
        iap_repeatbtn=0;
        send_bits(2|(0x40u<<16),tid);
        assert(iap_repeatbtn==0); /* held packet must not delay queue again */
        send_bits(0,tid);
        assert(iap_remotebtn==0 && iap_repeatbtn==2);
        send_bits(0x800000u|0x1000000u,tid);
        assert(iap_remotebtn==(BUTTON_RC_SELECT|BUTTON_RC_UP));
        send_bits(1,tid);assert(iap_remotebtn==BUTTON_RC_PLAY);
        send_bits(0,tid);
        for(int i=0;i<25;i++)send_bits(0x8000u|0x10000u,tid);
        assert(shuffles==(unsigned)tid+1 && repeats==(unsigned)tid+1);
        send_bits(0,tid);
    }
    unsigned before=acks;
    device.auth.idps=true;
    for(unsigned n=0;n<5;n++)
    {
        unsigned char *p=malloc(n?n:1);memset(p,0,n);
        if(n)p[0]=2;
        iap_handlepkt_mode2(n,p);free(p);
    }
    assert(acks==before); /* malformed ContextButtonStatus is silent */
    unsigned char audio[]={2,4,0x12,0x34,0x02};
    iap_handlepkt_mode2(sizeof(audio),audio);
    assert(iap_remotebtn==BUTTON_RC_VOL_UP);
    assert(tx[2]==0x12 && tx[3]==0x34 && tx[5]==4);
    suppressed=true;send_bits(1,0);assert(iap_remotebtn==0);
    /* Oversized packets cannot shift beyond 31 or read out of bounds. */
    unsigned char huge[32]={2,0};iap_handlepkt_mode2(sizeof(huge),huge);
    puts("PASS: real lingo-2 legacy/transaction packets, mixed bytes, short release, hold, malformed/auth and quarantine");
}
'''
with tempfile.TemporaryDirectory(prefix='tv-remote-') as tmp:
    p=pathlib.Path(tmp);(p/'test.c').write_text(stub+'\n'+buttons+'\n'+source+'\n'+tests)
    subprocess.run(['cc','-std=c99','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
