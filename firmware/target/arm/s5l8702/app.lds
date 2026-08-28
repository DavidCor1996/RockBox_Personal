#define ASM
#include "config.h"
#include "cpu.h"
#ifdef IPOD_6G
#include "hibernate-6g.h"
#endif

ENTRY(start)

OUTPUT_FORMAT(elf32-littlearm)
OUTPUT_ARCH(arm)
STARTUP(target/arm/s5l8702/crt0.o)

#define PLUGINSIZE PLUGIN_BUFFER_SIZE
#define CODECSIZE CODEC_SIZE

#define IRAMORIG 0x0
#define DRAMORIG 0x08000000

/* End of the audio buffer, where the codec buffer starts */
#define ENDAUDIOADDR  (DRAMORIG + DRAMSIZE)

#ifdef IPOD_6G
#define HIBERNATESIZE IPOD6G_HIBERNATE_AREA_SIZE
#else
#define HIBERNATESIZE 0
#endif

#define DRAMSIZE (DRAM_SIZE - PLUGINSIZE - CODECSIZE - TTB_SIZE \
                  - HIBERNATESIZE)
#define CODECORIG (ENDAUDIOADDR)
#define IRAMSIZE (48*1024)  /* 256KB total - 48KB for core, 200KB for codecs */

/* Where the codec buffer ends, and the plugin buffer starts */
#define ENDADDR (ENDAUDIOADDR + CODECSIZE)

MEMORY
{
    IRAM : ORIGIN = IRAMORIG, LENGTH = IRAMSIZE
    DRAM : ORIGIN = DRAMORIG, LENGTH = DRAMSIZE
#ifdef IPOD_6G
    HIBERNATE : ORIGIN = IPOD6G_HIBERNATE_AREA_ADDR,
                LENGTH = IPOD6G_HIBERNATE_AREA_SIZE
#endif
}

SECTIONS
{
    loadaddress = DRAMORIG;

    .intvect : {
        _intvectstart = . ;
        *(.intvect)
        _intvectend = _newstart ;  
    } >IRAM AT> DRAM
    _intvectcopy = LOADADDR(.intvect) ;

#if defined(IPOD_6G) && IPOD6G_HIBERNATE_STAGE2
    .hibernate_payload :
    {
        . = ALIGN(0x20);
        _hibernate_payload_start = .;
        KEEP(*(.hibernate_payload))
        . = ALIGN(0x20);
        _hibernate_payload_end = .;
    } > DRAM

    ASSERT(_hibernate_payload_end - _hibernate_payload_start <=
           IPOD6G_HIBERNATE_PAYLOAD_MAX_SIZE,
           "iPod 6G hibernate payload exceeds retained-code limit")
#endif

    .text :
    {
        _loadaddress = .;
        _textstart = .;
        *(.init.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        . = ALIGN(0x4);
    } > DRAM

    .rodata :
    {
        *(.rodata*)
        . = ALIGN(0x4);
    } > DRAM

    .data :
    {
        *(.data*)
        . = ALIGN(0x4);
    } > DRAM

    /DISCARD/ :
    {
        *(.eh_frame)
    }

    .iram :
    {
        _iramstart = .;
        *(.icode*)
        *(.irodata*)
        *(.idata*)
        . = ALIGN(0x4);
        _iramend = .;
    } > IRAM AT> DRAM
    _iramcopy = LOADADDR(.iram) ;

    .ibss (NOLOAD) :
    {
        _iedata = .;
        *(.qharray)
        *(.ibss*)
        . = ALIGN(0x4);
        _iend = .;
    } > IRAM

    .stack (NOLOAD) :
    {
       *(.stack)
       stackbegin = .;
       _stackbegin = .;
       . += 0x2000;
       stackend = .;
       _stackend = .;
       _irqstackbegin = .;
       . += 0x400;
       _irqstackend = .;
       _fiqstackbegin = .;
       . += 0x400;
       _fiqstackend = .;
    } > IRAM

    .bss (NOLOAD) :
    {
       _edata = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(0x4);
       _end = .;
    } > DRAM

    .audiobuf (NOLOAD) :
    {
        . = ALIGN(4);
        _audiobuffer = .;
        audiobuffer = .;
    } > DRAM

    .audiobufend ENDAUDIOADDR (NOLOAD) :
    {
        audiobufend = .;
        _audiobufend = .;
    } > DRAM

    .codec CODECORIG (NOLOAD) :
    {
        codecbuf = .;
        _codecbuf = .;
    } > DRAM

    .plugin ENDADDR (NOLOAD) :
    {
        _pluginbuf = .;
        pluginbuf = .;
    }

#ifdef IPOD_6G
    .hibernate IPOD6G_HIBERNATE_AREA_ADDR (NOLOAD) :
    {
        hibernatebuffer = .;
        _hibernatebuffer = .;
        . += IPOD6G_HIBERNATE_AREA_SIZE;
        hibernatebufferend = .;
        _hibernatebufferend = .;
    } > HIBERNATE

    ASSERT(ENDADDR + PLUGINSIZE == IPOD6G_HIBERNATE_AREA_ADDR,
           "plugin buffer overlaps iPod 6G hibernate reserve")
    ASSERT(IPOD6G_HIBERNATE_AREA_ADDR + IPOD6G_HIBERNATE_AREA_SIZE ==
           TTB_BASE_ADDR,
           "iPod 6G hibernate reserve must end at the TTB")
#endif
}
